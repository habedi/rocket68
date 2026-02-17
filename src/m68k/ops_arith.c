#include "m68k_internal.h"

// -----------------------------------------------------------------------------
// Arithmetic Instructions
// -----------------------------------------------------------------------------

void m68k_exec_add(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kSize size;
    bool dir_reg_to_ea = false;

    switch (opmode) {
        case 0:
            size = SIZE_BYTE;
            break;
        case 1:
            size = SIZE_WORD;
            break;
        case 2:
            size = SIZE_LONG;
            break;
        case 3:
            size = SIZE_WORD;
            break;
        case 4:
            size = SIZE_BYTE;
            dir_reg_to_ea = true;
            break;
        case 5:
            size = SIZE_WORD;
            dir_reg_to_ea = true;
            break;
        case 6:
            size = SIZE_LONG;
            dir_reg_to_ea = true;
            break;
        case 7:
            size = SIZE_LONG;
            break;
        default:
            return;
    }

    if (opmode == 3 || opmode == 7) {
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
        u32 src = ea.value;
        if (size == SIZE_WORD) src = (s32)(s16)src;

        cpu->a_regs[reg_idx] += src;
        return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src, dest, result;

    if (dir_reg_to_ea) {
        src = cpu->d_regs[reg_idx];
        dest = ea.value;
        result = dest + src;

        if (ea.is_reg && !ea.is_addr) {
            u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
            cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
        } else if (ea.is_reg && ea.is_addr) {
            // N/A
        } else {
            m68k_write_size(cpu, ea.address, result, size);
        }
    } else {
        src = ea.value;
        dest = cpu->d_regs[reg_idx];
        result = dest + src;

        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[reg_idx] = (dest & ~mask) | (result & mask);
    }

    update_flags_add(cpu, src, dest, result, size);
}

void m68k_exec_sub(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kSize size;
    bool dir_reg_to_ea = false;

    switch (opmode) {
        case 0:
            size = SIZE_BYTE;
            break;
        case 1:
            size = SIZE_WORD;
            break;
        case 2:
            size = SIZE_LONG;
            break;
        case 3:
            size = SIZE_WORD;
            break;
        case 4:
            size = SIZE_BYTE;
            dir_reg_to_ea = true;
            break;
        case 5:
            size = SIZE_WORD;
            dir_reg_to_ea = true;
            break;
        case 6:
            size = SIZE_LONG;
            dir_reg_to_ea = true;
            break;
        case 7:
            size = SIZE_LONG;
            break;
        default:
            return;
    }

    if (opmode == 3 || opmode == 7) {
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
        u32 src = ea.value;
        if (size == SIZE_WORD) src = (s32)(s16)src;

        cpu->a_regs[reg_idx] -= src;
        return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src, dest, result;

    if (dir_reg_to_ea) {
        src = cpu->d_regs[reg_idx];
        dest = ea.value;
        result = dest - src;

        if (ea.is_reg && !ea.is_addr) {
            u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
            cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
        } else {
            m68k_write_size(cpu, ea.address, result, size);
        }
    } else {
        src = ea.value;
        dest = cpu->d_regs[reg_idx];
        result = dest - src;

        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[reg_idx] = (dest & ~mask) | (result & mask);
    }

    update_flags_sub(cpu, src, dest, result, size);
}

void m68k_exec_addq(M68kCpu* cpu, u16 opcode) {
    int data = (opcode >> 9) & 0x7;
    if (data == 0) data = 8;

    int size_bits = (opcode >> 6) & 0x3;
    M68kSize size;
    if (size_bits == 0)
        size = SIZE_BYTE;
    else if (size_bits == 1)
        size = SIZE_WORD;
    else if (size_bits == 2)
        size = SIZE_LONG;
    else
        return;

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    if (mode == 1) {
        if (size == SIZE_BYTE) return;

        u32 val = cpu->a_regs[reg];
        cpu->a_regs[reg] = val + data;
        return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src = data;
    u32 dest = ea.value;
    u32 result = dest + src;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_add(cpu, src, dest, result, size);
}

void m68k_exec_subq(M68kCpu* cpu, u16 opcode) {
    int data = (opcode >> 9) & 0x7;
    if (data == 0) data = 8;

    int size_bits = (opcode >> 6) & 0x3;
    M68kSize size;
    if (size_bits == 0)
        size = SIZE_BYTE;
    else if (size_bits == 1)
        size = SIZE_WORD;
    else if (size_bits == 2)
        size = SIZE_LONG;
    else
        return;

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    if (mode == 1) {
        if (size == SIZE_BYTE) return;

        u32 val = cpu->a_regs[reg];
        cpu->a_regs[reg] = val - data;
        return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src = data;
    u32 dest = ea.value;
    u32 result = dest - src;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_sub(cpu, src, dest, result, size);
}

void m68k_exec_addx(M68kCpu* cpu, u16 opcode) {
    int rx = (opcode >> 9) & 0x7;
    int size_bits = (opcode >> 6) & 0x3;
    int rm = (opcode >> 3) & 0x1;
    int ry = opcode & 0x7;

    M68kSize size;
    if (size_bits == 0)
        size = SIZE_BYTE;
    else if (size_bits == 1)
        size = SIZE_WORD;
    else if (size_bits == 2)
        size = SIZE_LONG;
    else
        return;

    u32 src, dest;

    if (rm) {
        int step = (size == SIZE_BYTE) ? 1 : (size == SIZE_WORD) ? 2 : 4;

        cpu->a_regs[ry] -= step;
        if (size == SIZE_BYTE && ry == 7) cpu->a_regs[ry]--;
        src = m68k_read_size(cpu, cpu->a_regs[ry], size);

        cpu->a_regs[rx] -= step;
        if (size == SIZE_BYTE && rx == 7) cpu->a_regs[rx]--;
        dest = m68k_read_size(cpu, cpu->a_regs[rx], size);

    } else {
        src = cpu->d_regs[ry];
        dest = cpu->d_regs[rx];
    }

    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 result = dest + src + x;

    if (rm) {
        m68k_write_size(cpu, cpu->a_regs[rx], result, size);
    } else {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[rx] = (cpu->d_regs[rx] & ~mask) | (result & mask);
    }

    // ADDX Z-flag: cleared if result is non-zero, unchanged otherwise
    bool old_z = (cpu->sr & M68K_SR_Z) != 0;
    update_flags_add(cpu, src, dest, result, size);

    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    if ((result & value_mask) != 0) {
        cpu->sr &= ~M68K_SR_Z;  // Clear Z if result is non-zero
    } else if (old_z) {
        cpu->sr |= M68K_SR_Z;   // Preserve Z if result is zero and Z was set
    } else {
        cpu->sr &= ~M68K_SR_Z;  // Z was clear before, keep it clear
    }
}

void m68k_exec_subx(M68kCpu* cpu, u16 opcode) {
    int rx = (opcode >> 9) & 0x7;
    int size_bits = (opcode >> 6) & 0x3;
    int rm = (opcode >> 3) & 0x1;
    int ry = opcode & 0x7;

    M68kSize size;
    if (size_bits == 0)
        size = SIZE_BYTE;
    else if (size_bits == 1)
        size = SIZE_WORD;
    else if (size_bits == 2)
        size = SIZE_LONG;
    else
        return;

    u32 src, dest;

    if (rm) {
        int step = (size == SIZE_BYTE) ? 1 : (size == SIZE_WORD) ? 2 : 4;

        cpu->a_regs[ry] -= step;
        if (size == SIZE_BYTE && ry == 7) cpu->a_regs[ry]--;
        src = m68k_read_size(cpu, cpu->a_regs[ry], size);

        cpu->a_regs[rx] -= step;
        if (size == SIZE_BYTE && rx == 7) cpu->a_regs[rx]--;
        dest = m68k_read_size(cpu, cpu->a_regs[rx], size);

    } else {
        src = cpu->d_regs[ry];
        dest = cpu->d_regs[rx];
    }

    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 result = dest - src - x;

    if (rm) {
        m68k_write_size(cpu, cpu->a_regs[rx], result, size);
    } else {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[rx] = (cpu->d_regs[rx] & ~mask) | (result & mask);
    }

    // SUBX Z-flag: cleared if result is non-zero, unchanged otherwise
    bool old_z = (cpu->sr & M68K_SR_Z) != 0;
    update_flags_sub(cpu, src, dest, result, size);

    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    if ((result & value_mask) != 0) {
        cpu->sr &= ~M68K_SR_Z;
    } else if (old_z) {
        cpu->sr |= M68K_SR_Z;
    } else {
        cpu->sr &= ~M68K_SR_Z;
    }
}

void m68k_exec_mul(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    bool is_signed = (opmode == 7);

    M68kSize size = SIZE_WORD;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);

    u32 op1 = cpu->d_regs[reg_idx] & 0xFFFF;
    u32 op2 = ea.value & 0xFFFF;
    u32 result;

    if (is_signed) {
        s32 s1 = (s32)(s16)op1;
        s32 s2 = (s32)(s16)op2;
        result = (u32)(s1 * s2);
    } else {
        result = op1 * op2;
    }

    cpu->d_regs[reg_idx] = result;

    update_flags_logic(cpu, result, SIZE_LONG);
}

void m68k_exec_div(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    bool is_signed = (opmode == 7);

    M68kSize size = SIZE_WORD;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);

    u32 divisor = ea.value & 0xFFFF;
    u32 dividend = cpu->d_regs[reg_idx];

    if (divisor == 0) {
        m68k_exception(cpu, 5);  // Zero Divide exception
        return;
    }

    u32 quotient, remainder;

    if (is_signed) {
        s32 s_dividend = (s32)dividend;
        s16 s_divisor = (s16)divisor;
        s32 s_quot = s_dividend / s_divisor;
        s32 s_rem = s_dividend % s_divisor;

        if (s_quot > 32767 || s_quot < -32768) {
            cpu->sr |= M68K_SR_V;
            return;
        }

        quotient = s_quot & 0xFFFF;
        remainder = s_rem & 0xFFFF;
    } else {
        quotient = dividend / divisor;
        remainder = dividend % divisor;

        if (quotient > 0xFFFF) {
            cpu->sr |= M68K_SR_V;
            return;
        }
    }

    cpu->d_regs[reg_idx] = (remainder << 16) | quotient;

    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
    if (is_signed) {
        if ((s16)quotient < 0) cpu->sr |= M68K_SR_N;
    } else {
        if ((quotient & 0x8000)) cpu->sr |= M68K_SR_N;
    }
    if (quotient == 0) cpu->sr |= M68K_SR_Z;
}

void m68k_exec_cmp(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kSize size;
    if (opmode == 0)
        size = SIZE_BYTE;
    else if (opmode == 1)
        size = SIZE_WORD;
    else if (opmode == 2)
        size = SIZE_LONG;
    else if (opmode == 3)
        size = SIZE_WORD;
    else if (opmode == 7)
        size = SIZE_LONG;
    else if (opmode >= 4 && opmode <= 6 && mode == 1) {
        // CMPM
        if (opmode == 4)
            size = SIZE_BYTE;
        else if (opmode == 5)
            size = SIZE_WORD;
        else
            size = SIZE_LONG;

        u32 src_addr = cpu->a_regs[reg];
        u32 dest_addr = cpu->a_regs[reg_idx];

        u32 src_val = m68k_read_size(cpu, src_addr, size);
        u32 dest_val = m68k_read_size(cpu, dest_addr, size);

        int step = (size == SIZE_BYTE) ? 1 : (size == SIZE_WORD) ? 2 : 4;
        if (reg == 7 && size == SIZE_BYTE)
            cpu->a_regs[reg] += 2;
        else
            cpu->a_regs[reg] += step;
        if (reg_idx == 7 && size == SIZE_BYTE)
            cpu->a_regs[reg_idx] += 2;
        else
            cpu->a_regs[reg_idx] += step;

        u32 result = dest_val - src_val;
        update_flags_sub(cpu, src_val, dest_val, result, size);
        return;
    } else {
        return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src = ea.value;
    u32 dest;

    if (opmode == 3 || opmode == 7) {
        // CMPA
        dest = cpu->a_regs[reg_idx];
        if (size == SIZE_WORD) src = (s32)(s16)src;
        u32 result = dest - src;
        update_flags_sub(cpu, src, dest, result, SIZE_LONG);
    } else {
        // CMP
        dest = cpu->d_regs[reg_idx];
        if (size == SIZE_BYTE) {
            src &= 0xFF;
            dest &= 0xFF;
        } else if (size == SIZE_WORD) {
            src &= 0xFFFF;
            dest &= 0xFFFF;
        }

        u32 result = dest - src;
        update_flags_sub(cpu, src, dest, result, size);
    }
}

void m68k_exec_cmpi(M68kCpu* cpu, u16 opcode) {
    int size_bits = (opcode >> 6) & 0x3;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kSize size;

    if (size_bits == 0)
        size = SIZE_BYTE;
    else if (size_bits == 1)
        size = SIZE_WORD;
    else if (size_bits == 2)
        size = SIZE_LONG;
    else
        return;

    u32 src;
    if (size == SIZE_LONG) {
        u32 high = m68k_fetch(cpu);
        u32 low = m68k_fetch(cpu);
        src = (high << 16) | low;
    } else {
        u32 val = m68k_fetch(cpu);
        if (size == SIZE_BYTE) val &= 0xFF;
        src = val;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 dest = ea.value;

    u32 result = dest - src;
    update_flags_sub(cpu, src, dest, result, size);
}

void m68k_exec_neg(M68kCpu* cpu, u16 opcode) {
    int size_bits = (opcode >> 6) & 0x3;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
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

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src = ea.value;
    u32 result = 0 - src;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_sub(cpu, src, 0, result, size);
}

void m68k_exec_ext(M68kCpu* cpu, u16 opcode) {
    int opmode = (opcode >> 6) & 0x7;
    int reg = opcode & 0x7;
    M68kSize size, result_size;

    if (opmode == 2) {
        size = SIZE_BYTE;
        result_size = SIZE_WORD;
    } else if (opmode == 3) {
        size = SIZE_WORD;
        result_size = SIZE_LONG;
    } else {
        return;
    }

    u32 val = cpu->d_regs[reg];
    u32 result;

    if (size == SIZE_BYTE) {
        result = (s32)(s8)(val & 0xFF);
    } else {
        result = (s32)(s16)(val & 0xFFFF);
    }

    if (result_size == SIZE_WORD) {
        cpu->d_regs[reg] = (cpu->d_regs[reg] & 0xFFFF0000) | (result & 0xFFFF);
        update_flags_logic(cpu, result & 0xFFFF, SIZE_WORD);
    } else {
        cpu->d_regs[reg] = result;
        update_flags_logic(cpu, result, SIZE_LONG);
    }
}

// -----------------------------------------------------------------------------
// BCD Arithmetic
// -----------------------------------------------------------------------------

void m68k_exec_abcd(M68kCpu* cpu, u16 opcode) {
    int rx = (opcode >> 9) & 0x7;
    int rm = (opcode >> 3) & 0x1;
    int ry = opcode & 0x7;

    u32 src, dest;

    if (rm) {  // -(An)
        cpu->a_regs[ry]--;
        if (ry == 7) cpu->a_regs[ry]--;
        src = m68k_read_8(cpu, cpu->a_regs[ry]);

        cpu->a_regs[rx]--;
        if (rx == 7) cpu->a_regs[rx]--;
        dest = m68k_read_8(cpu, cpu->a_regs[rx]);
    } else {  // Dn
        src = cpu->d_regs[ry] & 0xFF;
        dest = cpu->d_regs[rx] & 0xFF;
    }

    // Musashi-verified ABCD algorithm
    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 res = (dest & 0x0F) + (src & 0x0F) + x;

    if (res > 9) res += 6;
    res += (dest & 0xF0) + (src & 0xF0);

    bool c = (res > 0x99);
    if (c) res -= 0xA0;

    u8 result = res & 0xFF;

    if (rm) {
        m68k_write_8(cpu, cpu->a_regs[rx], result);
    } else {
        cpu->d_regs[rx] = (cpu->d_regs[rx] & 0xFFFFFF00) | result;
    }

    // Flags: C/X set if carry, Z cleared if non-zero (unchanged otherwise)
    cpu->sr &= ~(M68K_SR_C | M68K_SR_X);
    if (result != 0) cpu->sr &= ~M68K_SR_Z;

    if (c) {
        cpu->sr |= M68K_SR_C;
        cpu->sr |= M68K_SR_X;
    }
    // N and V are undefined
}

void m68k_exec_sbcd(M68kCpu* cpu, u16 opcode) {
    int rx = (opcode >> 9) & 0x7;
    int rm = (opcode >> 3) & 0x1;
    int ry = opcode & 0x7;

    u32 src, dest;

    if (rm) {  // -(An)
        cpu->a_regs[ry]--;
        if (ry == 7) cpu->a_regs[ry]--;
        src = m68k_read_8(cpu, cpu->a_regs[ry]);

        cpu->a_regs[rx]--;
        if (rx == 7) cpu->a_regs[rx]--;
        dest = m68k_read_8(cpu, cpu->a_regs[rx]);
    } else {  // Dn
        src = cpu->d_regs[ry] & 0xFF;
        dest = cpu->d_regs[rx] & 0xFF;
    }

    // Musashi-verified SBCD algorithm
    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 res = (dest & 0x0F) - (src & 0x0F) - x;

    if (res > 9) res -= 6;
    res += (dest & 0xF0) - (src & 0xF0);

    bool c = (res > 0x99);
    if (c) res += 0xA0;

    u8 result = res & 0xFF;

    if (rm) {
        m68k_write_8(cpu, cpu->a_regs[rx], result);
    } else {
        cpu->d_regs[rx] = (cpu->d_regs[rx] & 0xFFFFFF00) | result;
    }

    cpu->sr &= ~(M68K_SR_C | M68K_SR_X);
    if (result != 0) cpu->sr &= ~M68K_SR_Z;

    if (c) {
        cpu->sr |= M68K_SR_C;
        cpu->sr |= M68K_SR_X;
    }
}

void m68k_exec_nbcd(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
    u32 dest = ea.value & 0xFF;
    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;

    // Musashi-verified NBCD algorithm: 0x9A - dest - X
    u32 res = (0x9A - dest - x) & 0xFF;

    if (res != 0x9A) {
        // Adjust low nibble
        if ((res & 0x0F) == 0x0A) {
            res = (res & 0xF0) + 0x10;
        }
        res &= 0xFF;

        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & 0xFFFFFF00) | res;
        } else {
            m68k_write_8(cpu, ea.address, res);
        }

        // Z cleared if non-zero, unchanged otherwise
        if (res != 0) cpu->sr &= ~M68K_SR_Z;

        cpu->sr |= M68K_SR_C;
        cpu->sr |= M68K_SR_X;
    } else {
        cpu->sr &= ~(M68K_SR_C | M68K_SR_X);
        // Result is zero (0x9A masked to byte would have been written as dest)
        // Don't change Z or the destination (result of 0 - 0 - 0 = 0)
        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & 0xFFFFFF00);
        } else {
            m68k_write_8(cpu, ea.address, 0);
        }
    }
}
