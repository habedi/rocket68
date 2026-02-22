#include "m68k_internal.h"

static u32 get_bit_mask(int bit_num, bool is_register) {
    if (is_register) {
        return 1u << (bit_num & 31);
    } else {
        return 1u << (bit_num & 7);
    }
}

void m68k_exec_btst(M68kCpu* cpu, u16 opcode) {
    bool is_dynamic = (opcode & 0x0100) != 0;
    int bit_num;
    int mode, reg;

    if (is_dynamic) {
        int dn = (opcode >> 9) & 0x7;
        bit_num = cpu->d_regs[dn];
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    } else {
        bit_num = m68k_fetch(cpu) & 0xFF;
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    }

    bool is_register = (mode == 0);
    u32 mask = get_bit_mask(bit_num, is_register);

    u32 data;
    if (is_register) {
        data = cpu->d_regs[reg];
    } else {
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
        data = ea.value & 0xFF;
    }

    if (data & mask)
        cpu->sr &= ~M68K_SR_Z;
    else
        cpu->sr |= M68K_SR_Z;
}

void m68k_exec_bset(M68kCpu* cpu, u16 opcode) {
    bool is_dynamic = (opcode & 0x0100) != 0;
    int bit_num;
    int mode, reg;

    if (is_dynamic) {
        int dn = (opcode >> 9) & 0x7;
        bit_num = cpu->d_regs[dn];
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    } else {
        bit_num = m68k_fetch(cpu) & 0xFF;
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    }

    bool is_register = (mode == 0);
    u32 mask = get_bit_mask(bit_num, is_register);

    if (is_register) {
        u32 data = cpu->d_regs[reg];

        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        cpu->d_regs[reg] = data | mask;
    } else {
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
        u8 data = ea.value & 0xFF;
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        m68k_write_8(cpu, ea.address, data | mask);
    }
}

void m68k_exec_bclr(M68kCpu* cpu, u16 opcode) {
    bool is_dynamic = (opcode & 0x0100) != 0;
    int bit_num;
    int mode, reg;

    if (is_dynamic) {
        int dn = (opcode >> 9) & 0x7;
        bit_num = cpu->d_regs[dn];
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    } else {
        bit_num = m68k_fetch(cpu) & 0xFF;
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    }

    bool is_register = (mode == 0);
    u32 mask = get_bit_mask(bit_num, is_register);

    if (is_register) {
        u32 data = cpu->d_regs[reg];
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        cpu->d_regs[reg] = data & ~mask;
    } else {
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
        u8 data = ea.value & 0xFF;
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        m68k_write_8(cpu, ea.address, data & ~mask);
    }
}

void m68k_exec_bchg(M68kCpu* cpu, u16 opcode) {
    bool is_dynamic = (opcode & 0x0100) != 0;
    int bit_num;
    int mode, reg;

    if (is_dynamic) {
        int dn = (opcode >> 9) & 0x7;
        bit_num = cpu->d_regs[dn];
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    } else {
        bit_num = m68k_fetch(cpu) & 0xFF;
        mode = (opcode >> 3) & 0x7;
        reg = opcode & 0x7;
    }

    bool is_register = (mode == 0);
    u32 mask = get_bit_mask(bit_num, is_register);

    if (is_register) {
        u32 data = cpu->d_regs[reg];
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        cpu->d_regs[reg] = data ^ mask;
    } else {
        M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
        u8 data = ea.value & 0xFF;
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        m68k_write_8(cpu, ea.address, data ^ mask);
    }
}
