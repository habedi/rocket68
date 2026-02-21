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
        cpu->sr |= M68K_SR_Z;  // Preserve Z if result is zero and Z was set
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

    u32 divisor_raw = ea.value & 0xFFFF;
    u32 dividend = cpu->d_regs[reg_idx];

    if (divisor_raw == 0) {
        m68k_exception(cpu, 5);  // Zero Divide exception
        return;
    }

    if (is_signed) {
        s16 s_divisor = (s16)divisor_raw;

        // Special case: 0x80000000 / -1 (C undefined behavior)
        if (dividend == 0x80000000 && s_divisor == -1) {
            cpu->sr &= ~(M68K_SR_N | M68K_SR_V | M68K_SR_C);
            cpu->sr |= M68K_SR_Z;
            cpu->d_regs[reg_idx] = 0;
            return;
        }

        s32 s_dividend = (s32)dividend;
        s32 s_quot = s_dividend / s_divisor;
        s32 s_rem = s_dividend % s_divisor;

        if (s_quot != (s32)(s16)s_quot) {
            // Overflow: unconditionally N=1, V=1, Z=0, C=0 (matches MAME/hardware tests)
            cpu->sr &= ~(M68K_SR_Z | M68K_SR_C);
            cpu->sr |= (M68K_SR_N | M68K_SR_V);
            return;
        }

        u32 quotient = (u32)s_quot & 0xFFFF;
        u32 remainder = (u32)s_rem & 0xFFFF;
        cpu->d_regs[reg_idx] = (remainder << 16) | quotient;

        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
        if (quotient & 0x8000) cpu->sr |= M68K_SR_N;
        if (quotient == 0) cpu->sr |= M68K_SR_Z;
    } else {
        u32 quotient = dividend / divisor_raw;
        u32 remainder = dividend % divisor_raw;

        if (quotient > 0xFFFF) {
            // Overflow: unconditionally N=1, V=1, Z=0, C=0 (matches MAME/hardware tests)
            cpu->sr &= ~(M68K_SR_Z | M68K_SR_C);
            cpu->sr |= (M68K_SR_N | M68K_SR_V);
            return;
        }

        cpu->d_regs[reg_idx] = (remainder << 16) | (quotient & 0xFFFF);

        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
        if (quotient & 0x8000) cpu->sr |= M68K_SR_N;
        if (quotient == 0) cpu->sr |= M68K_SR_Z;
    }
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

        int step = (size == SIZE_BYTE) ? 1 : (size == SIZE_WORD) ? 2 : 4;

        u32 src_addr = cpu->a_regs[reg];
        u32 src_val = m68k_read_size(cpu, src_addr, size);
        cpu->a_regs[reg] += (reg == 7 && size == SIZE_BYTE) ? 2 : step;

        u32 dest_addr = cpu->a_regs[reg_idx];
        u32 dest_val = m68k_read_size(cpu, dest_addr, size);
        cpu->a_regs[reg_idx] += (reg_idx == 7 && size == SIZE_BYTE) ? 2 : step;

        u32 result = dest_val - src_val;
        update_flags_cmp(cpu, src_val, dest_val, result, size);
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
        update_flags_cmp(cpu, src, dest, result, SIZE_LONG);
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
        update_flags_cmp(cpu, src, dest, result, size);
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
    update_flags_cmp(cpu, src, dest, result, size);
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

    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 src_total = (src & 0xFF) + x;
    u32 raw_res = (dest & 0xFF) + src_total;
    u32 res = (src & 0x0F) + (dest & 0x0F) + x;

    if (res > 9) res += 6;
    res += (src & 0xF0) + (dest & 0xF0);

    bool c_flag = (res > 0x99);
    if (c_flag) res -= 0xA0;

    u8 result = (u8)res;
    bool v_flag = ((~((dest & 0xFF) ^ src_total) & ((dest & 0xFF) ^ raw_res)) & 0x80) != 0;

    cpu->sr &= ~(M68K_SR_N | M68K_SR_V | M68K_SR_C | M68K_SR_X);
    if (result & 0x80) cpu->sr |= M68K_SR_N;
    if (v_flag) cpu->sr |= M68K_SR_V;
    if (c_flag) cpu->sr |= M68K_SR_C | M68K_SR_X;

    if (result != 0) cpu->sr &= ~M68K_SR_Z;

    if (rm) {
        m68k_write_8(cpu, cpu->a_regs[rx], result);
    } else {
        cpu->d_regs[rx] = (cpu->d_regs[rx] & 0xFFFFFF00) | result;
    }
}

void m68k_exec_sbcd(M68kCpu* cpu, u16 opcode) {
    int rx = (opcode >> 9) & 0x7;
    int rm = (opcode >> 3) & 0x1;
    int ry = opcode & 0x7;

    u32 src, dst;

    if (rm) {  // -(An)
        cpu->a_regs[ry]--;
        if (ry == 7) cpu->a_regs[ry]--;
        src = m68k_read_8(cpu, cpu->a_regs[ry]);

        cpu->a_regs[rx]--;
        if (rx == 7) cpu->a_regs[rx]--;
        dst = m68k_read_8(cpu, cpu->a_regs[rx]);
    } else {  // Dn
        src = cpu->d_regs[ry] & 0xFF;
        dst = cpu->d_regs[rx] & 0xFF;
    }

    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 src_total = (src & 0xFF) + x;
    u32 raw_res = (dst & 0xFF) - src_total;
    u32 res = (dst & 0x0F) - (src & 0x0F) - x;

    if (res > 9) res -= 6;
    res += (dst & 0xF0) - (src & 0xF0);

    bool c_flag = (res > 0x99);
    if (c_flag) res += 0xA0;

    u8 result = (u8)res;
    bool v_flag = ((((dst & 0xFF) ^ src_total) & ((dst & 0xFF) ^ raw_res)) & 0x80) != 0;

    cpu->sr &= ~(M68K_SR_N | M68K_SR_V | M68K_SR_C | M68K_SR_X);
    if (result & 0x80) cpu->sr |= M68K_SR_N;
    if (v_flag) cpu->sr |= M68K_SR_V;
    if (c_flag) cpu->sr |= M68K_SR_C | M68K_SR_X;

    if (result != 0) cpu->sr &= ~M68K_SR_Z;

    if (rm) {
        m68k_write_8(cpu, cpu->a_regs[rx], result);
    } else {
        cpu->d_regs[rx] = (cpu->d_regs[rx] & 0xFFFFFF00) | result;
    }
}

void m68k_exec_nbcd(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
    u32 dest = ea.value & 0xFF;
    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;

    u32 zResult = 0 - dest - x;
    u32 res = zResult;

    if ((0) - (dest & 0x0F) - x > 9) res -= 6;
    if (res > 0x99) res -= 0x60;

    cpu->sr &= ~(M68K_SR_N | M68K_SR_V | M68K_SR_C | M68K_SR_X);
    if (res & 0x80) cpu->sr |= M68K_SR_N;
    if ((zResult & ~res) & 0x80) cpu->sr |= M68K_SR_V;
    if (res > 0xFF) cpu->sr |= M68K_SR_C | M68K_SR_X;

    u8 result = res & 0xFF;
    if (result != 0) cpu->sr &= ~M68K_SR_Z;

    if (ea.is_reg && !ea.is_addr) {
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & 0xFFFFFF00) | result;
    } else {
        m68k_write_8(cpu, ea.address, result);
    }
}

// -----------------------------------------------------------------------------
// Immediate Arithmetic (ADDI, SUBI)
// -----------------------------------------------------------------------------

void m68k_exec_addi(M68kCpu* cpu, u16 opcode) {
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

    // Fetch immediate value
    u32 imm;
    if (size == SIZE_BYTE) {
        imm = m68k_fetch(cpu) & 0xFF;
    } else if (size == SIZE_WORD) {
        imm = m68k_fetch(cpu);
    } else {
        imm = ((u32)m68k_fetch(cpu) << 16) | m68k_fetch(cpu);
    }

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 dest = ea.value;
    u32 result = dest + imm;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_add(cpu, imm, dest, result, size);
}

void m68k_exec_subi(M68kCpu* cpu, u16 opcode) {
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

    u32 imm;
    if (size == SIZE_BYTE) {
        imm = m68k_fetch(cpu) & 0xFF;
    } else if (size == SIZE_WORD) {
        imm = m68k_fetch(cpu);
    } else {
        imm = ((u32)m68k_fetch(cpu) << 16) | m68k_fetch(cpu);
    }

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 dest = ea.value;
    u32 result = dest - imm;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_sub(cpu, imm, dest, result, size);
}

// -----------------------------------------------------------------------------
// NEGX — Negate with Extend
// -----------------------------------------------------------------------------

void m68k_exec_negx(M68kCpu* cpu, u16 opcode) {
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

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src = ea.value;
    u32 x = (cpu->sr & M68K_SR_X) ? 1 : 0;
    u32 result = 0 - src - x;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    // Flags: same as SUB but Z is "cleared if non-zero, unchanged otherwise"
    bool old_z = (cpu->sr & M68K_SR_Z) != 0;
    update_flags_sub(cpu, src, 0, result, size);

    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    if ((result & value_mask) != 0) {
        cpu->sr &= ~M68K_SR_Z;
    } else if (old_z) {
        cpu->sr |= M68K_SR_Z;
    } else {
        cpu->sr &= ~M68K_SR_Z;
    }
}

// -----------------------------------------------------------------------------
// EXTB — Extend Byte to Long (68020+)
// -----------------------------------------------------------------------------

void m68k_exec_extb(M68kCpu* cpu, u16 opcode) {
    int reg = opcode & 0x7;
    s8 byte_val = (s8)(cpu->d_regs[reg] & 0xFF);
    cpu->d_regs[reg] = (u32)(s32)byte_val;
    update_flags_logic(cpu, cpu->d_regs[reg], SIZE_LONG);
}
