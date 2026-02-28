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

    M68kEA src_ea = m68k_calc_ea(cpu, src_mode, src_reg, size);
    M68kEA dest_ea = m68k_calc_ea_addr(cpu, dest_mode, dest_reg, size);

    if (dest_ea.is_reg && !dest_ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        u32 current = cpu->d_regs[dest_ea.reg_num].l;
        cpu->d_regs[dest_ea.reg_num].l = (current & ~mask) | (src_ea.value & mask);
    } else if (dest_ea.is_reg && dest_ea.is_addr) {
        u32 val = src_ea.value;
        if (size == SIZE_WORD) val = (s32)(s16)val;
        cpu->a_regs[dest_ea.reg_num].l = val;
    } else {
        m68k_write_size(cpu, dest_ea.address, src_ea.value, size);
    }

    if (!(dest_ea.is_reg && dest_ea.is_addr)) {
        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
        if (size == SIZE_BYTE && (s8)src_ea.value < 0)
            cpu->sr |= M68K_SR_N;
        else if (size == SIZE_WORD && (s16)src_ea.value < 0)
            cpu->sr |= M68K_SR_N;
        else if (size == SIZE_LONG && (s32)src_ea.value < 0)
            cpu->sr |= M68K_SR_N;

        if ((size == SIZE_BYTE && (u8)src_ea.value == 0) ||
            (size == SIZE_WORD && (u16)src_ea.value == 0) ||
            (size == SIZE_LONG && src_ea.value == 0)) {
            cpu->sr |= M68K_SR_Z;
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

    M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, SIZE_LONG);
    cpu->a_regs[reg_idx].l = ea.address;
}

void m68k_exec_pea(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, SIZE_LONG);
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

    cpu->a_regs[7].l = cpu->a_regs[reg].l;
    cpu->a_regs[reg].l = m68k_pop_32(cpu);
}

void m68k_exec_movem(M68kCpu* cpu, u16 opcode) {
    bool dir_mem_to_reg = (opcode & 0x0400) != 0;
    bool size_long = (opcode & 0x0040) != 0;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    u16 mask = m68k_fetch(cpu);

    M68kSize size = size_long ? SIZE_LONG : SIZE_WORD;
    int step = size_long ? 4 : 2;

    M68kEA ea;
    u32 addr = 0;
    int count = 0;

    if (mode == 4) {
        for (int i = 0; i < 16; i++)
            if (mask & (1 << i)) count++;
        cpu->a_regs[reg].l -= count * step;
        addr = cpu->a_regs[reg].l;
    } else {
        ea = dir_mem_to_reg ? m68k_calc_ea(cpu, mode, reg, size)
                            : m68k_calc_ea_addr(cpu, mode, reg, size);
        addr = ea.address;
    }

    if (dir_mem_to_reg) {
        for (int i = 0; i < 16; i++) {
            if (mask & (1 << i)) {
                u32 val = m68k_read_size(cpu, addr, size);
                if (!size_long) val = (s32)(s16)val;

                if (i < 8) {
                    cpu->d_regs[i].l = val;
                } else {
                    cpu->a_regs[i - 8].l = val;
                }

                addr += step;
            }
        }
        if (mode == 3) cpu->a_regs[reg].l = addr;
    } else {
        if (mode == 4) {
            for (int i = 15; i >= 0; i--) {
                if (mask & (1 << i)) {
                    int reg_idx = 15 - i;
                    u32 val;
                    if (reg_idx < 8) {
                        val = cpu->d_regs[reg_idx].l;
                    } else if (reg_idx - 8 == reg) {
                        val = cpu->a_regs[reg].l + count * step;
                    } else {
                        val = cpu->a_regs[reg_idx - 8].l;
                    }
                    if (!size_long) val &= 0xFFFF;
                    m68k_write_size(cpu, addr, val, size);
                    addr += step;
                }
            }
        } else {
            for (int i = 0; i < 16; i++) {
                if (mask & (1 << i)) {
                    u32 val = (i < 8) ? cpu->d_regs[i].l : cpu->a_regs[i - 8].l;
                    if (!size_long) val &= 0xFFFF;
                    m68k_write_size(cpu, addr, val, size);
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
        int mode = (opcode >> 3) & 0x7;
        int reg = opcode & 0x7;
        M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, SIZE_WORD);

        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & 0xFFFF0000) | cpu->sr;
        } else {
            m68k_write_size(cpu, ea.address, cpu->sr, SIZE_WORD);
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
