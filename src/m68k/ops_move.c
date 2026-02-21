#include "m68k_internal.h"

// -----------------------------------------------------------------------------
// Data Transfer Instructions
// -----------------------------------------------------------------------------

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
    M68kEA dest_ea = m68k_calc_ea(cpu, dest_mode, dest_reg, size);

    if (dest_ea.is_reg && !dest_ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        u32 current = cpu->d_regs[dest_ea.reg_num];
        cpu->d_regs[dest_ea.reg_num] = (current & ~mask) | (src_ea.value & mask);
    } else if (dest_ea.is_reg && dest_ea.is_addr) {
        u32 val = src_ea.value;
        if (size == SIZE_WORD) val = (s32)(s16)val;
        cpu->a_regs[dest_ea.reg_num] = val;
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

    cpu->d_regs[reg] = data;

    update_flags_logic(cpu, data, SIZE_LONG);
}

void m68k_exec_lea(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_LONG);
    cpu->a_regs[reg_idx] = ea.address;
}

void m68k_exec_pea(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_LONG);
    m68k_push_32(cpu, ea.address);
}

void m68k_exec_link(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;
    s16 displacement = (s16)m68k_fetch(cpu);

    m68k_push_32(cpu, cpu->a_regs[reg]);
    cpu->a_regs[reg] = cpu->a_regs[7];
    cpu->a_regs[7] += displacement;
}

void m68k_exec_unlk(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;

    cpu->a_regs[7] = cpu->a_regs[reg];
    cpu->a_regs[reg] = m68k_pop_32(cpu);
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

    if (mode == 4) {  // -(An)
        int count = 0;
        for (int i = 0; i < 16; i++)
            if (mask & (1 << i)) count++;
        cpu->a_regs[reg] -= count * step;
        addr = cpu->a_regs[reg];
    } else {
        ea = m68k_calc_ea(cpu, mode, reg, size);
        addr = ea.address;
    }

    if (dir_mem_to_reg) {
        for (int i = 0; i < 16; i++) {
            if (mask & (1 << i)) {
                u32 val = m68k_read_size(cpu, addr, size);
                if (!size_long) val = (s32)(s16)val;

                if (i < 8)
                    cpu->d_regs[i] = val;
                else
                    cpu->a_regs[i - 8] = val;

                addr += step;
            }
        }
        if (mode == 3) cpu->a_regs[reg] = addr;

    } else {
        if (mode == 4) {  // -(An)
            for (int i = 15; i >= 0; i--) {
                if (mask & (1 << i)) {
                    int reg_idx = 15 - i;
                    u32 val = (reg_idx < 8) ? cpu->d_regs[reg_idx] : cpu->a_regs[reg_idx - 8];
                    if (!size_long) val &= 0xFFFF;
                    m68k_write_size(cpu, addr, val, size);
                    addr += step;
                }
            }
        } else {
            for (int i = 0; i < 16; i++) {
                if (mask & (1 << i)) {
                    u32 val = (i < 8) ? cpu->d_regs[i] : cpu->a_regs[i - 8];
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

    if (opmode == 0x08) {  // Dx, Dy
        val_rx = cpu->d_regs[rx];
        val_ry = cpu->d_regs[ry];
        cpu->d_regs[rx] = val_ry;
        cpu->d_regs[ry] = val_rx;
    } else if (opmode == 0x09) {  // Ax, Ay
        val_rx = cpu->a_regs[rx];
        val_ry = cpu->a_regs[ry];
        cpu->a_regs[rx] = val_ry;
        cpu->a_regs[ry] = val_rx;
    } else if (opmode == 0x11) {  // Dx, Ay
        val_rx = cpu->d_regs[rx];
        val_ry = cpu->a_regs[ry];
        cpu->d_regs[rx] = val_ry;
        cpu->a_regs[ry] = val_rx;
    }
}

void m68k_exec_swap(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;
    u32 val = cpu->d_regs[reg];
    u32 result = (val << 16) | (val >> 16);
    cpu->d_regs[reg] = result;

    update_flags_logic(cpu, result, SIZE_LONG);
}

void m68k_exec_move_sr(M68kCpu* cpu, u16 opcode) {
    // MOVE <ea>, SR: 0100 0110 11 ... (0x46C0)
    if ((opcode & 0xFFC0) == 0x46C0) {
        if (!(cpu->sr & M68K_SR_S)) {
            return;
        }
        int mode = (opcode >> 3) & 0x7;
        int reg = opcode & 0x7;
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);
        cpu->sr = ea.value;
        return;
    }

    // MOVE SR, <ea>: 0100 0000 11 ... (0x40C0)
    if ((opcode & 0xFFC0) == 0x40C0) {
        int mode = (opcode >> 3) & 0x7;
        int reg = opcode & 0x7;
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);

        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & 0xFFFF0000) | cpu->sr;
        } else {
            m68k_write_size(cpu, ea.address, cpu->sr, SIZE_WORD);
        }
    }
}

// -----------------------------------------------------------------------------
// MOVEP — Move Peripheral Data (alternating bytes in memory)
// -----------------------------------------------------------------------------

void m68k_exec_movep(M68kCpu* cpu, u16 opcode) {
    int data_reg = (opcode >> 9) & 0x7;
    int addr_reg = opcode & 0x7;
    int opmode = (opcode >> 6) & 0x7;

    s16 disp = (s16)m68k_fetch(cpu);
    u32 addr = cpu->a_regs[addr_reg] + disp;

    switch (opmode) {
        case 4:  // MOVEP.W (d16,An), Dn — memory to register, word
            cpu->d_regs[data_reg] = (cpu->d_regs[data_reg] & 0xFFFF0000) |
                                    ((u32)m68k_read_8(cpu, addr) << 8) | m68k_read_8(cpu, addr + 2);
            break;
        case 5:  // MOVEP.L (d16,An), Dn — memory to register, long
            cpu->d_regs[data_reg] =
                ((u32)m68k_read_8(cpu, addr) << 24) | ((u32)m68k_read_8(cpu, addr + 2) << 16) |
                ((u32)m68k_read_8(cpu, addr + 4) << 8) | m68k_read_8(cpu, addr + 6);
            break;
        case 6:  // MOVEP.W Dn, (d16,An) — register to memory, word
            m68k_write_8(cpu, addr, (cpu->d_regs[data_reg] >> 8) & 0xFF);
            m68k_write_8(cpu, addr + 2, cpu->d_regs[data_reg] & 0xFF);
            break;
        case 7:  // MOVEP.L Dn, (d16,An) — register to memory, long
            m68k_write_8(cpu, addr, (cpu->d_regs[data_reg] >> 24) & 0xFF);
            m68k_write_8(cpu, addr + 2, (cpu->d_regs[data_reg] >> 16) & 0xFF);
            m68k_write_8(cpu, addr + 4, (cpu->d_regs[data_reg] >> 8) & 0xFF);
            m68k_write_8(cpu, addr + 6, cpu->d_regs[data_reg] & 0xFF);
            break;
        default:
            break;
    }
}

// -----------------------------------------------------------------------------
// MOVE USP — Move to/from User Stack Pointer (supervisor only)
// -----------------------------------------------------------------------------

void m68k_exec_move_usp(M68kCpu* cpu, u16 opcode) {
    if (!(cpu->sr & M68K_SR_S)) {
        m68k_exception(cpu, 8);  // Privilege violation
        return;
    }

    int reg = opcode & 0x7;
    bool to_usp = (opcode & 0x8) == 0;  // Bit 3: 0 = An->USP, 1 = USP->An

    if (to_usp) {
        cpu->usp = cpu->a_regs[reg];
    } else {
        cpu->a_regs[reg] = cpu->usp;
    }
}

// -----------------------------------------------------------------------------
// MOVE to CCR — MOVE <ea>, CCR (writes lower byte of SR only)
// -----------------------------------------------------------------------------

void m68k_exec_move_ccr(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);
    u16 data = ea.value & 0x1F;  // Only CCR bits (lower 5 bits)
    cpu->sr = (cpu->sr & 0xFF00) | data;
}

// -----------------------------------------------------------------------------
// MOVES — Move to/from Address Space (68010+, supervisor only)
// -----------------------------------------------------------------------------

void m68k_exec_moves(M68kCpu* cpu, u16 opcode) {
    if (!(cpu->sr & M68K_SR_S)) {
        m68k_exception(cpu, 8);  // Privilege violation
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
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);

    if (to_ea) {
        u32 val = is_addr ? cpu->a_regs[reg_num] : cpu->d_regs[reg_num];
        m68k_write_size(cpu, ea.address, val, size);
    } else {
        u32 val = m68k_read_size(cpu, ea.address, size);
        if (is_addr) {
            cpu->a_regs[reg_num] = val;
        } else {
            u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
            cpu->d_regs[reg_num] = (cpu->d_regs[reg_num] & ~mask) | (val & mask);
        }
    }
}
