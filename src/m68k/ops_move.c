/**
 * @file ops_move.c
 * @brief Data movement opcode implementations.
 */
#include "m68k_internal.h"

void m68k_exec_move(M68kCpu* cpu, u16 opcode) {
    int size_bits = (opcode >> 12) & 0x3;
    M68kSize size;
    switch (size_bits) {
        case 1:
            size = SIZE_BYTE;
            break;
        case 3:
            size = SIZE_WORD;
            break;
        case 2:
            size = SIZE_LONG;
            break;
        default:
            return;
    }

    int dest_reg = (opcode >> 9) & 0x7;
    int dest_mode = (opcode >> 6) & 0x7;
    int src_mode = (opcode >> 3) & 0x7;
    int src_reg = opcode & 0x7;

    /* PC-relative, immediate, and reserved destinations are invalid
     * encodings and trap before any state changes. */
    if (dest_mode == 7 && dest_reg > 1) {
        m68k_exception(cpu, 4);
        return;
    }

    M68kEA src_ea = m68k_calc_ea(cpu, src_mode, src_reg, size);
    u32 pc_after_src = cpu->pc;

    /* Destination (An)+ and -(An) commits interact with write faults:
     * -(An) commits before the write for byte and word only, and (An)+
     * commits only after a successful write, per corpus measurement. A
     * faulted write longjmps out, so ordering encodes the rules. */
    M68kEA dest_ea;
    bool commit_after_write = false;
    u32 commit_value = 0;
    if (dest_mode == 3 || dest_mode == 4) {
        u32 step = (size == SIZE_BYTE && dest_reg == 7) ? 2 : (u32)size;
        cpu->cycles_remaining -= m68k_ea_cycles(dest_mode, dest_reg, size);
        dest_ea = (M68kEA){0};
        if (dest_mode == 3) {
            dest_ea.address = cpu->a_regs[dest_reg].l;
            commit_after_write = true;
            commit_value = dest_ea.address + step;
        } else {
            dest_ea.address = cpu->a_regs[dest_reg].l - step;
            if (size == SIZE_LONG) {
                commit_after_write = true;
                commit_value = dest_ea.address;
            } else {
                cpu->a_regs[dest_reg].l = dest_ea.address;
            }
        }
    } else {
        dest_ea = m68k_calc_ea_addr(cpu, dest_mode, dest_reg, size);
    }

    u16 full_flags = 0;
    if (size == SIZE_BYTE && (s8)src_ea.value < 0)
        full_flags |= M68K_SR_N;
    else if (size == SIZE_WORD && (s16)src_ea.value < 0)
        full_flags |= M68K_SR_N;
    else if (size == SIZE_LONG && (s32)src_ea.value < 0)
        full_flags |= M68K_SR_N;
    if ((size == SIZE_BYTE && (u8)src_ea.value == 0) ||
        (size == SIZE_WORD && (u16)src_ea.value == 0) ||
        (size == SIZE_LONG && src_ea.value == 0)) {
        full_flags |= M68K_SR_Z;
    }

    const u16 ccr_mask = M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C;

    if (dest_ea.is_reg && !dest_ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        u32 current = cpu->d_regs[dest_ea.reg_num].l;
        cpu->d_regs[dest_ea.reg_num].l = (current & ~mask) | (src_ea.value & mask);
        cpu->sr = (cpu->sr & ~ccr_mask) | full_flags;
    } else if (dest_ea.is_reg && dest_ea.is_addr) {
        u32 val = src_ea.value;
        if (size == SIZE_WORD) val = (s32)(s16)val;
        cpu->a_regs[dest_ea.reg_num].l = val;
    } else {
        /* A faulted destination write pushes the PC after the source
         * extension words plus one prefetch advance. An (xxx).l
         * destination with a register-direct source prefetches one more
         * word before the write. */
        bool src_reg_imm = (src_mode <= 1) || (src_mode == 7 && src_reg == 4);
        cpu->fault_pc = pc_after_src + 2;
        if (dest_mode == 7 && dest_reg == 1 && src_mode <= 1) {
            cpu->fault_pc += 2;
        }
        cpu->fault_pc_valid = true;

        /* The condition codes visible in a faulted long write depend on
         * how far the microcode got, measured against the corpus. Word
         * and byte writes always show the fully updated flags, so their
         * flags are final before the write and need no second update. */
        if (size == SIZE_LONG) {
            u16 fault_flags = full_flags;
            u16 old_flags = cpu->sr & ccr_mask;
            bool abs_long_dest = (dest_mode == 7 && dest_reg == 1);
            if (dest_mode == 2 || dest_mode == 3 || (abs_long_dest && !src_reg_imm)) {
                if (src_reg_imm) {
                    fault_flags = old_flags;
                } else {
                    fault_flags = 0;
                    if (src_ea.value & 0x8000) fault_flags |= M68K_SR_N;
                    if ((u16)src_ea.value == 0) fault_flags |= M68K_SR_Z;
                }
            } else if ((dest_mode == 5 || dest_mode == 6) && src_reg_imm) {
                fault_flags = old_flags & (M68K_SR_V | M68K_SR_C);
                if (src_ea.value & 0x80000000u) fault_flags |= M68K_SR_N;
                if ((src_ea.value >> 16) == 0) fault_flags |= M68K_SR_Z;
            }
            cpu->sr = (cpu->sr & ~ccr_mask) | fault_flags;
        } else {
            cpu->sr = (cpu->sr & ~ccr_mask) | full_flags;
        }

        /* A faulted long write has only spent the word share of the EA
         * cost, so the difference is refunded across the write. */
        int fault_refund = (size == SIZE_LONG) ? ((dest_mode == 4) ? 2 : 4) : 0;
        /* An (xxx).l destination with a memory source interleaves the
         * write before the second absolute word is fetched, so that
         * word's cost is not yet spent when the write faults. */
        if (dest_mode == 7 && dest_reg == 1 && !src_reg_imm) {
            fault_refund += 4;
        }
        cpu->cycles_remaining += fault_refund;
        if (dest_mode == 4 && size != SIZE_LONG) {
            /* The predecrement internal cost is spent before the write;
             * the dispatch charge drops to zero to compensate. */
            cpu->cycles_remaining -= 2;
        }

        /* A predecrement destination write overlaps the next prefetch,
         * so the frame IR holds the prefetched word, and a long write
         * goes low word first, faulting on the higher address. */
        if (dest_mode == 4) {
            if (size == SIZE_LONG) {
                m68k_write_16(cpu, dest_ea.address + 2, (u16)src_ea.value);
                m68k_write_16(cpu, dest_ea.address, (u16)(src_ea.value >> 16));
                cpu->cycles_remaining -= fault_refund;
                cpu->a_regs[dest_reg].l = commit_value;
                cpu->sr = (cpu->sr & ~ccr_mask) | full_flags;
                return;
            }
            /* Only faulting writes need the latch, so skip the peek on
             * the aligned fast path. */
            if (dest_ea.address & 1) {
                cpu->fault_bus_word = m68k_peek_word(cpu, pc_after_src);
                cpu->fault_bus_word_valid = true;
            }
        }
        m68k_write_size(cpu, dest_ea.address, src_ea.value, size);
        cpu->cycles_remaining -= fault_refund;
        cpu->fault_bus_word_valid = false;
        if (commit_after_write) {
            cpu->a_regs[dest_reg].l = commit_value;
        }
        if (size == SIZE_LONG) {
            cpu->sr = (cpu->sr & ~ccr_mask) | full_flags;
        }
    }
}

void m68k_exec_moveq(M68kCpu* cpu, u16 opcode) {
    int reg = (opcode >> 9) & 0x7;
    s32 data = (s8)(opcode & 0xFF);

    cpu->d_regs[reg].l = data;

    update_flags_logic(cpu, data, SIZE_LONG);
}

void m68k_exec_lea(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea_ctl(cpu, mode, reg, false);
    cpu->a_regs[reg_idx].l = ea.address;
}

void m68k_exec_pea(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea_ctl(cpu, mode, reg, false);
    m68k_push_32(cpu, ea.address);
}

void m68k_exec_link(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;
    s16 displacement = (s16)m68k_fetch(cpu);

    m68k_push_32(cpu, cpu->a_regs[reg].l);
    cpu->a_regs[reg].l = cpu->a_regs[7].l;
    cpu->a_regs[7].l += displacement;
}

void m68k_exec_unlk(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;

    /* The frame-pointer read happens before the stack pointer moves, so
     * a fault on an odd frame pointer leaves SP and An untouched and the
     * exception frame lands on the original stack. */
    u32 frame_sp = cpu->a_regs[reg].l;
    /* A faulted frame-pointer read pushes the instruction address plus 4,
     * and the pre-read internal cost is already spent. */
    cpu->fault_pc = cpu->pc + 2;
    cpu->fault_pc_valid = true;
    cpu->cycles_remaining -= 4;
    u32 value = m68k_read_32(cpu, frame_sp);
    cpu->a_regs[7].l = frame_sp + 4;
    cpu->a_regs[reg].l = value;
}

void m68k_exec_movem(M68kCpu* cpu, u16 opcode) {
    bool dir_mem_to_reg = (opcode & 0x0400) != 0;
    bool size_long = (opcode & 0x0040) != 0;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    u16 mask = m68k_fetch(cpu);

    M68kSize size = size_long ? SIZE_LONG : SIZE_WORD;
    int step = size_long ? 4 : 2;

    u32 addr = 0;
    M68kEA ea;

    if (dir_mem_to_reg && mode == 4) {
        m68k_exception(cpu, 4);
        return;
    }

    if ((mode == 4 && !dir_mem_to_reg) || mode == 3) {
        /* The postincrement writeback commits only after the transfer
         * loop, so a faulted first read leaves the register unchanged. */
        addr = cpu->a_regs[reg].l;
    } else {
        ea = m68k_calc_ea_addr_nocost(cpu, mode, reg, size);
        addr = ea.address;
    }

    int reg_count = 0;
    for (int i = 0; i < 16; i++) {
        if (mask & (1 << i)) reg_count++;
    }
    (void)reg_count;
    /* The memory-to-register form's trailing extra read is spent after
     * the transfers, so its 4 cycles are charged at the end. */
    cpu->cycles_remaining -= m68k_movem_ea_cycles(mode, reg, dir_mem_to_reg);
    if (dir_mem_to_reg) cpu->cycles_remaining += 4;

    /* A faulted transfer pushes the PC after the consumed words plus one
     * prefetch advance, for both directions. */
    cpu->fault_pc = cpu->pc + 2;
    cpu->fault_pc_valid = true;

    if (dir_mem_to_reg) {
        /* PC-relative transfers read from program space. */
        if (mode == 7 && (reg == 2 || reg == 3)) {
            cpu->operand_program_space = true;
        }
        for (int i = 0; i < 16; i++) {
            if (mask & (1 << i)) {
                u32 val = m68k_read_size(cpu, addr, size);
                cpu->cycles_remaining -= size_long ? 8 : 4;
                if (!size_long) val = (s32)(s16)val;

                if (i < 8) {
                    cpu->d_regs[i].l = val;
                } else {
                    cpu->a_regs[i - 8].l = val;
                }

                addr += step;
            }
        }
        cpu->operand_program_space = false;
        cpu->cycles_remaining -= 4;
        if (mode == 3) cpu->a_regs[reg].l = addr;
    } else {
        if (mode == 4) {
            /* 68000: if the register list includes the base An, the value
             * written is the initial (pre-decrement) value of An. Save it
             * before the loop so the correct value is stored. */
            u32 initial_an = cpu->a_regs[reg].l;
            for (int i = 0; i < 16; i++) {
                if (mask & (1 << i)) {
                    int reg_idx = 15 - i;
                    addr -= step;
                    u32 val;
                    if (reg_idx >= 8 && (reg_idx - 8) == reg) {
                        val = initial_an;
                    } else {
                        val = (reg_idx < 8) ? cpu->d_regs[reg_idx].l : cpu->a_regs[reg_idx - 8].l;
                    }
                    if (size_long) {
                        /* Predecrement long writes go low word first. */
                        m68k_write_16(cpu, addr + 2, (u16)val);
                        m68k_write_16(cpu, addr, (u16)(val >> 16));
                    } else {
                        m68k_write_size(cpu, addr, val & 0xFFFF, size);
                    }
                    cpu->cycles_remaining -= size_long ? 8 : 4;
                }
            }
            cpu->a_regs[reg].l = addr;
        } else {
            for (int i = 0; i < 16; i++) {
                if (mask & (1 << i)) {
                    u32 val = (i < 8) ? cpu->d_regs[i].l : cpu->a_regs[i - 8].l;
                    if (!size_long) val &= 0xFFFF;
                    m68k_write_size(cpu, addr, val, size);
                    cpu->cycles_remaining -= size_long ? 8 : 4;
                    addr += step;
                }
            }
        }
    }
}

void m68k_exec_exg(M68kCpu* cpu, u16 opcode) {
    int rx = (opcode >> 9) & 0x7;
    int ry = opcode & 0x7;
    int opmode = (opcode >> 3) & 0x1F;

    u32 val_rx, val_ry;

    if (opmode == 0x08) {
        val_rx = cpu->d_regs[rx].l;
        val_ry = cpu->d_regs[ry].l;
        cpu->d_regs[rx].l = val_ry;
        cpu->d_regs[ry].l = val_rx;
    } else if (opmode == 0x09) {
        val_rx = cpu->a_regs[rx].l;
        val_ry = cpu->a_regs[ry].l;
        cpu->a_regs[rx].l = val_ry;
        cpu->a_regs[ry].l = val_rx;
    } else if (opmode == 0x11) {
        val_rx = cpu->d_regs[rx].l;
        val_ry = cpu->a_regs[ry].l;
        cpu->d_regs[rx].l = val_ry;
        cpu->a_regs[ry].l = val_rx;
    }
}

void m68k_exec_swap(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;
    u32 val = cpu->d_regs[reg].l;
    u32 result = (val << 16) | (val >> 16);
    cpu->d_regs[reg].l = result;

    update_flags_logic(cpu, result, SIZE_LONG);
}

void m68k_exec_move_sr(M68kCpu* cpu, u16 opcode) {
    if ((opcode & 0xFFC0) == 0x46C0) {
        if (!(cpu->sr & M68K_SR_S)) {
            cpu->pc -= 2;
            m68k_exception(cpu, 8);
            return;
        }
        int mode = (opcode >> 3) & 0x7;
        int reg = opcode & 0x7;
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);
        m68k_set_sr(cpu, ea.value);
        return;
    }

    if ((opcode & 0xFFC0) == 0x40C0) {
        /* MOVE from SR is privileged on the 68010. */
        if (cpu->model >= M68K_MODEL_68010 && !(cpu->sr & M68K_SR_S)) {
            cpu->pc -= 2;
            m68k_exception(cpu, 8);
            return;
        }
        int mode = (opcode >> 3) & 0x7;
        int reg = opcode & 0x7;
        M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, SIZE_WORD);

        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & 0xFFFF0000) | cpu->sr;
        } else {
            /* MOVE from SR reads the destination before writing it, so
             * an odd destination faults as a read. */
            (void)m68k_read_size(cpu, ea.address, SIZE_WORD);
            m68k_write_size(cpu, ea.address, cpu->sr, SIZE_WORD);
        }
        return;
    }

    if ((opcode & 0xFFC0) == 0x42C0) {
        /* MOVE from CCR exists on the 68010 only. */
        int mode = (opcode >> 3) & 0x7;
        int reg = opcode & 0x7;
        M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, SIZE_WORD);
        u16 ccr = cpu->sr & 0x00FF;
        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & 0xFFFF0000) | ccr;
        } else {
            (void)m68k_read_size(cpu, ea.address, SIZE_WORD);
            m68k_write_size(cpu, ea.address, ccr, SIZE_WORD);
        }
    }
}

void m68k_exec_movep(M68kCpu* cpu, u16 opcode) {
    int data_reg = (opcode >> 9) & 0x7;
    int addr_reg = opcode & 0x7;
    int opmode = (opcode >> 6) & 0x7;

    s16 disp = (s16)m68k_fetch(cpu);
    u32 addr = cpu->a_regs[addr_reg].l + disp;

    switch (opmode) {
        case 4:
            cpu->d_regs[data_reg].l = (cpu->d_regs[data_reg].l & 0xFFFF0000) |
                                      ((u32)m68k_read_8(cpu, addr) << 8) |
                                      m68k_read_8(cpu, addr + 2);
            break;
        case 5:
            cpu->d_regs[data_reg].l =
                ((u32)m68k_read_8(cpu, addr) << 24) | ((u32)m68k_read_8(cpu, addr + 2) << 16) |
                ((u32)m68k_read_8(cpu, addr + 4) << 8) | m68k_read_8(cpu, addr + 6);
            break;
        case 6:
            m68k_write_8(cpu, addr, (cpu->d_regs[data_reg].l >> 8) & 0xFF);
            m68k_write_8(cpu, addr + 2, cpu->d_regs[data_reg].l & 0xFF);
            break;
        case 7:
            m68k_write_8(cpu, addr, (cpu->d_regs[data_reg].l >> 24) & 0xFF);
            m68k_write_8(cpu, addr + 2, (cpu->d_regs[data_reg].l >> 16) & 0xFF);
            m68k_write_8(cpu, addr + 4, (cpu->d_regs[data_reg].l >> 8) & 0xFF);
            m68k_write_8(cpu, addr + 6, cpu->d_regs[data_reg].l & 0xFF);
            break;
        default:
            break;
    }
}

void m68k_exec_move_usp(M68kCpu* cpu, u16 opcode) {
    if (!(cpu->sr & M68K_SR_S)) {
        cpu->pc -= 2;
        m68k_exception(cpu, 8);
        return;
    }

    int reg = opcode & 0x7;
    bool to_usp = (opcode & 0x8) == 0;

    if (to_usp) {
        cpu->usp = cpu->a_regs[reg].l;
    } else {
        cpu->a_regs[reg].l = cpu->usp;
    }
}

void m68k_exec_move_ccr(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);
    u16 data = ea.value & 0x1F;
    cpu->sr = (cpu->sr & 0xFF00) | data;
}

void m68k_exec_moves(M68kCpu* cpu, u16 opcode) {
    if (!(cpu->sr & M68K_SR_S)) {
        cpu->pc -= 2;
        m68k_exception(cpu, 8);
        return;
    }

    int size_bits = (opcode >> 6) & 0x3;
    M68kSize size;
    switch (size_bits) {
        case 0:
            size = SIZE_BYTE;
            break;
        case 1:
            size = SIZE_WORD;
            break;
        case 2:
            size = SIZE_LONG;
            break;
        default:
            return;
    }

    u16 ext = m68k_fetch(cpu);
    bool is_addr = (ext >> 15) & 1;
    int reg_num = (ext >> 12) & 0x7;
    bool to_ea = (ext >> 11) & 1;

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea =
        to_ea ? m68k_calc_ea_addr(cpu, mode, reg, size) : m68k_calc_ea(cpu, mode, reg, size);

    if (to_ea) {
        u32 val = is_addr ? cpu->a_regs[reg_num].l : cpu->d_regs[reg_num].l;
        m68k_write_size(cpu, ea.address, val, size);
    } else {
        u32 val = m68k_read_size(cpu, ea.address, size);
        if (is_addr) {
            cpu->a_regs[reg_num].l = val;
        } else {
            u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
            cpu->d_regs[reg_num].l = (cpu->d_regs[reg_num].l & ~mask) | (val & mask);
        }
    }
}
