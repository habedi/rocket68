/**
 * @file ops_logic.c
 * @brief Logical and bitwise opcode implementations.
 */
#include "m68k_internal.h"

void m68k_exec_and(M68kCpu* cpu, u16 opcode) {
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
        default:
            return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src, dest, result;

    if (dir_reg_to_ea) {
        src = cpu->d_regs[reg_idx].l;
        dest = ea.value;
        result = dest & src;

        if (ea.is_reg && !ea.is_addr) {
            u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
            cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
        } else {
            m68k_write_size(cpu, ea.address, result, size);
        }
    } else {
        src = ea.value;
        dest = cpu->d_regs[reg_idx].l;
        result = dest & src;

        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[reg_idx].l = (dest & ~mask) | (result & mask);
    }

    update_flags_logic(cpu, result, size);
}

void m68k_exec_or(M68kCpu* cpu, u16 opcode) {
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
            return;
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
            return;
        default:
            return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src, dest, result;

    if (dir_reg_to_ea) {
        src = cpu->d_regs[reg_idx].l;
        dest = ea.value;
        result = dest | src;

        if (ea.is_reg && !ea.is_addr) {
            u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
            cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
        } else {
            m68k_write_size(cpu, ea.address, result, size);
        }
    } else {
        src = ea.value;
        dest = cpu->d_regs[reg_idx].l;
        result = dest | src;

        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[reg_idx].l = (dest & ~mask) | (result & mask);
    }

    update_flags_logic(cpu, result, size);
}

void m68k_exec_eor(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int opmode = (opcode >> 6) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kSize size;

    switch (opmode) {
        case 4:
            size = SIZE_BYTE;
            break;
        case 5:
            size = SIZE_WORD;
            break;
        case 6:
            size = SIZE_LONG;
            break;
        default:
            return;
    }

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 src, dest, result;

    src = cpu->d_regs[reg_idx].l;
    dest = ea.value;
    result = dest ^ src;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_logic(cpu, result, size);
}

void m68k_exec_not(M68kCpu* cpu, u16 opcode) {
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
    u32 result = ~ea.value;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }

    update_flags_logic(cpu, result, size);
}

void m68k_exec_clr(M68kCpu* cpu, u16 opcode) {
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

    if (mode == 1 || (mode == 7 && reg > 1)) {
        m68k_exception(cpu, 4);
        return;
    }

    M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, size);

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num].l &= ~mask;
    } else {
        m68k_write_size(cpu, ea.address, 0, size);
    }

    cpu->sr &= ~(M68K_SR_N | M68K_SR_V | M68K_SR_C | M68K_SR_Z);
    cpu->sr |= M68K_SR_Z;
}

void m68k_exec_tst(M68kCpu* cpu, u16 opcode) {
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

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 result = ea.value;

    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);

    u32 msb_mask = (size == SIZE_BYTE) ? 0x80 : (size == SIZE_WORD) ? 0x8000 : 0x80000000;
    if (result & msb_mask) cpu->sr |= M68K_SR_N;

    u32 val_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    if ((result & val_mask) == 0) cpu->sr |= M68K_SR_Z;
}

void m68k_exec_shift(M68kCpu* cpu, u16 opcode) {
    bool is_mem = ((opcode >> 6) & 0x3) == 3;

    int type, dr, count, reg;
    M68kSize size;
    u32 value;
    M68kEA ea = {0};

    if (is_mem) {
        type = (opcode >> 9) & 0x3;
        dr = (opcode >> 8) & 0x1;
        size = SIZE_WORD;
        count = 1;
        int mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;

        ea = m68k_calc_ea(cpu, mode, reg, size);
        value = ea.value & 0xFFFF;
    } else {
        count = (opcode >> 9) & 0x7;
        dr = (opcode >> 8) & 0x1;
        int sz_bits = (opcode >> 6) & 0x3;
        size = (sz_bits == 0) ? SIZE_BYTE : (sz_bits == 1) ? SIZE_WORD : SIZE_LONG;

        bool ir = (opcode >> 5) & 0x1;
        type = (opcode >> 3) & 0x3;
        reg = opcode & 0x7;

        if (ir) {
            count = cpu->d_regs[count].l & 63;
        } else {
            if (count == 0) count = 8;
        }

        value = cpu->d_regs[reg].l;
    }

    u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    u32 msb = (size == SIZE_BYTE) ? 0x80 : (size == SIZE_WORD) ? 0x8000 : 0x80000000;
    value &= mask;

    u32 result = value;
    bool carry = false;
    bool overflow = false;
    bool extend = (cpu->sr & M68K_SR_X) != 0;

    if (count == 0 && !is_mem) {
        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
        if (result & msb) cpu->sr |= M68K_SR_N;
        if (result == 0) cpu->sr |= M68K_SR_Z;

        if (type == 2 && extend) cpu->sr |= M68K_SR_C;
        return;
    }

    if (type == 0) {
        if (dr == 0) {
            for (int i = 0; i < count; i++) {
                carry = (result & 1) != 0;
                u32 sign = result & msb;
                result = (result >> 1) | sign;
            }
            extend = carry;
            overflow = false;
        } else {
            for (int i = 0; i < count; i++) {
                bool old_msb = (result & msb) != 0;
                carry = old_msb;
                result = (result << 1) & mask;
                bool new_msb = (result & msb) != 0;
                if (old_msb != new_msb) overflow = true;
            }
            extend = carry;
        }

        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C | M68K_SR_X);
        if (carry) {
            cpu->sr |= M68K_SR_C;
            cpu->sr |= M68K_SR_X;
        }
        if (overflow) cpu->sr |= M68K_SR_V;
    }

    else if (type == 1) {
        if (dr == 0) {
            for (int i = 0; i < count; i++) {
                carry = (result & 1) != 0;
                result = (result >> 1);
            }
            extend = carry;
        } else {
            for (int i = 0; i < count; i++) {
                carry = (result & msb) != 0;
                result = (result << 1) & mask;
            }
            extend = carry;
        }

        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C | M68K_SR_X);
        if (carry) {
            cpu->sr |= M68K_SR_C;
            cpu->sr |= M68K_SR_X;
        }
    }

    else if (type == 2) {
        if (dr == 0) {
            for (int i = 0; i < count; i++) {
                bool out = (result & 1) != 0;
                result = result >> 1;
                if (extend) result |= msb;
                extend = out;
                carry = out;
            }
        } else {
            for (int i = 0; i < count; i++) {
                bool out = (result & msb) != 0;
                result = (result << 1) & mask;
                if (extend) result |= 1;
                extend = out;
                carry = out;
            }
        }

        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C | M68K_SR_X);
        if (carry) cpu->sr |= M68K_SR_C;
        if (extend) cpu->sr |= M68K_SR_X;
    }

    else if (type == 3) {
        if (dr == 0) {
            for (int i = 0; i < count; i++) {
                carry = (result & 1) != 0;
                result = (result >> 1);
                if (carry) result |= msb;
            }
        } else {
            for (int i = 0; i < count; i++) {
                carry = (result & msb) != 0;
                result = (result << 1) & mask;
                if (carry) result |= 1;
            }
        }

        cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
        if (carry) cpu->sr |= M68K_SR_C;
    }

    if (result & msb) cpu->sr |= M68K_SR_N;
    if (result == 0) cpu->sr |= M68K_SR_Z;

    if (is_mem) {
        m68k_write_size(cpu, ea.address, result, size);
    } else {
        u32 reg_val = cpu->d_regs[opcode & 0x7].l;
        cpu->d_regs[opcode & 0x7].l = (reg_val & ~mask) | (result & mask);
    }
}

void m68k_exec_andi(M68kCpu* cpu, u16 opcode) {
    int size_bits = (opcode >> 6) & 0x3;

    if (opcode == 0x023C) {
        u16 imm = m68k_fetch(cpu) & 0xFF;
        cpu->sr = (cpu->sr & 0xFF00) | ((cpu->sr & 0x00FF) & imm);
        return;
    }

    if (opcode == 0x027C) {
        if (!(cpu->sr & M68K_SR_S)) {
            cpu->pc -= 2;
            m68k_exception(cpu, 8);
            return;
        }
        u16 imm = m68k_fetch(cpu);
        m68k_set_sr(cpu, cpu->sr & imm);
        return;
    }

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
    if (size == SIZE_BYTE)
        imm = m68k_fetch(cpu) & 0xFF;
    else if (size == SIZE_WORD)
        imm = m68k_fetch(cpu);
    else
        imm = ((u32)m68k_fetch(cpu) << 16) | m68k_fetch(cpu);

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 result = ea.value & imm;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }
    update_flags_logic(cpu, result, size);
}

void m68k_exec_ori(M68kCpu* cpu, u16 opcode) {
    int size_bits = (opcode >> 6) & 0x3;

    if (opcode == 0x003C) {
        u16 imm = m68k_fetch(cpu) & 0xFF;
        cpu->sr = (cpu->sr & 0xFF00) | ((cpu->sr & 0x00FF) | imm);
        return;
    }

    if (opcode == 0x007C) {
        if (!(cpu->sr & M68K_SR_S)) {
            cpu->pc -= 2;
            m68k_exception(cpu, 8);
            return;
        }
        u16 imm = m68k_fetch(cpu);
        m68k_set_sr(cpu, cpu->sr | imm);
        return;
    }

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
    if (size == SIZE_BYTE)
        imm = m68k_fetch(cpu) & 0xFF;
    else if (size == SIZE_WORD)
        imm = m68k_fetch(cpu);
    else
        imm = ((u32)m68k_fetch(cpu) << 16) | m68k_fetch(cpu);

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 result = ea.value | imm;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }
    update_flags_logic(cpu, result, size);
}

void m68k_exec_eori(M68kCpu* cpu, u16 opcode) {
    int size_bits = (opcode >> 6) & 0x3;

    if (opcode == 0x0A3C) {
        u16 imm = m68k_fetch(cpu) & 0xFF;
        cpu->sr = (cpu->sr & 0xFF00) | ((cpu->sr & 0x00FF) ^ imm);
        return;
    }

    if (opcode == 0x0A7C) {
        if (!(cpu->sr & M68K_SR_S)) {
            cpu->pc -= 2;
            m68k_exception(cpu, 8);
            return;
        }
        u16 imm = m68k_fetch(cpu);
        m68k_set_sr(cpu, cpu->sr ^ imm);
        return;
    }

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
    if (size == SIZE_BYTE)
        imm = m68k_fetch(cpu) & 0xFF;
    else if (size == SIZE_WORD)
        imm = m68k_fetch(cpu);
    else
        imm = ((u32)m68k_fetch(cpu) << 16) | m68k_fetch(cpu);

    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;
    M68kEA ea = m68k_calc_ea(cpu, mode, reg, size);
    u32 result = ea.value ^ imm;

    if (ea.is_reg && !ea.is_addr) {
        u32 mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
        cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & ~mask) | (result & mask);
    } else {
        m68k_write_size(cpu, ea.address, result, size);
    }
    update_flags_logic(cpu, result, size);
}
