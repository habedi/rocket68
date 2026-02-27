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
        bit_num = cpu->d_regs[dn].l;
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
        data = cpu->d_regs[reg].l;
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
        bit_num = cpu->d_regs[dn].l;
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
        u32 data = cpu->d_regs[reg].l;

        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        cpu->d_regs[reg].l = data | mask;
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
        bit_num = cpu->d_regs[dn].l;
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
        u32 data = cpu->d_regs[reg].l;
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        cpu->d_regs[reg].l = data & ~mask;
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
        bit_num = cpu->d_regs[dn].l;
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
        u32 data = cpu->d_regs[reg].l;
        if (data & mask)
            cpu->sr &= ~M68K_SR_Z;
        else
            cpu->sr |= M68K_SR_Z;
        cpu->d_regs[reg].l = data ^ mask;
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

void m68k_exec_tas(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
    u8 data;

    if (ea.is_reg && !ea.is_addr) {
        data = cpu->d_regs[ea.reg_num].l & 0xFF;
    } else {
        data = m68k_read_8(cpu, ea.address);
    }

    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
    if (data == 0) cpu->sr |= M68K_SR_Z;
    if (data & 0x80) cpu->sr |= M68K_SR_N;

    data |= 0x80;

    bool allow_write = true;
    if (cpu->tas_cb) {
        allow_write = (cpu->tas_cb(cpu) != 0);
    }

    if (allow_write) {
        if (ea.is_reg && !ea.is_addr) {
            cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & 0xFFFFFF00) | data;
        } else {
            m68k_write_8(cpu, ea.address, data);
        }
    }
}
