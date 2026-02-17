#include "m68k_internal.h"

// -----------------------------------------------------------------------------
// Control Flow Instructions
// -----------------------------------------------------------------------------

bool m68k_check_condition(M68kCpu* cpu, int condition) {
    bool c = (cpu->sr & M68K_SR_C) != 0;
    bool v = (cpu->sr & M68K_SR_V) != 0;
    bool z = (cpu->sr & M68K_SR_Z) != 0;
    bool n = (cpu->sr & M68K_SR_N) != 0;

    switch (condition) {
        case 0:
            return true;
        case 1:
            return false;
        case 2:
            return !c && !z;
        case 3:
            return c || z;
        case 4:
            return !c;
        case 5:
            return c;
        case 6:
            return !z;
        case 7:
            return z;
        case 8:
            return !v;
        case 9:
            return v;
        case 10:
            return !n;
        case 11:
            return n;
        case 12:
            return (n && v) || (!n && !v);
        case 13:
            return (n && !v) || (!n && v);
        case 14:
            return (n && v && !z) || (!n && !v && !z);
        case 15:
            return z || (n && !v) || (!n && v);
        default:
            return false;
    }
}

void m68k_exec_bcc(M68kCpu* cpu, u16 opcode) {
    int cond = (opcode >> 8) & 0xF;
    s32 disp = (s8)(opcode & 0xFF);

    if ((opcode & 0xFF) == 0) {
        disp = (s16)m68k_fetch(cpu);
    }

    if (cond == 1) {  // BSR
        m68k_push_32(cpu, cpu->pc);
        if ((opcode & 0xFF) == 0)
            cpu->pc = (cpu->pc - 2) + disp;
        else
            cpu->pc = cpu->pc + disp;
    } else if (m68k_check_condition(cpu, cond)) {
        if ((opcode & 0xFF) == 0)
            cpu->pc = (cpu->pc - 2) + disp;
        else
            cpu->pc = cpu->pc + disp;
    }
}

void m68k_exec_dbcc(M68kCpu* cpu, u16 opcode) {
    int condition = (opcode >> 8) & 0xF;
    int reg = opcode & 0x7;
    s16 displacement = (s16)m68k_fetch(cpu);

    if (m68k_check_condition(cpu, condition)) {
        return;
    }

    u16 val = cpu->d_regs[reg] & 0xFFFF;
    val--;
    cpu->d_regs[reg] = (cpu->d_regs[reg] & 0xFFFF0000) | val;

    if (val != 0xFFFF) {
        cpu->pc = (cpu->pc - 2) + displacement;
    }
}

void m68k_exec_scc(M68kCpu* cpu, u16 opcode) {
    int condition = (opcode >> 8) & 0xF;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    bool result = m68k_check_condition(cpu, condition);
    u8 val = result ? 0xFF : 0x00;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_BYTE);
    if (ea.is_reg && !ea.is_addr) {
        cpu->d_regs[ea.reg_num] = (cpu->d_regs[ea.reg_num] & 0xFFFFFF00) | val;
    } else {
        m68k_write_8(cpu, ea.address, val);
    }
}

void m68k_exec_jmp(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    bool is_jsr = ((opcode >> 6) & 1) == 0;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_LONG);

    if (is_jsr) {
        m68k_push_32(cpu, cpu->pc);
    }

    cpu->pc = ea.address;
}

void m68k_exec_rts(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    cpu->pc = m68k_pop_32(cpu);
}

void m68k_exec_trap(M68kCpu* cpu, u16 opcode) {
    int vector = 32 + (opcode & 0xF);
    m68k_exception(cpu, vector);
}

void m68k_exec_rte(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (!(cpu->sr & M68K_SR_S)) {
        // Privilege Violation! (Not implemented efficiently yet, just return)
        return;
    }

    cpu->sr = m68k_pop_16(cpu);
    cpu->pc = m68k_pop_32(cpu);
}

void m68k_exec_chk(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);
    s16 src = (s16)ea.value;
    s16 dest = (s16)cpu->d_regs[reg_idx];

    if (dest < 0) {
        cpu->sr |= M68K_SR_N;
        m68k_exception(cpu, 6);  // CHK instruction vector
    } else if (dest > src) {
        cpu->sr &= ~M68K_SR_N;
        m68k_exception(cpu, 6);
    }
}

void m68k_exec_trapv(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (cpu->sr & M68K_SR_V) {
        m68k_exception(cpu, 7);  // TRAPV instruction vector
    }
}

void m68k_exec_rtr(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    u16 ccr = m68k_pop_16(cpu);
    u32 return_pc = m68k_pop_32(cpu);

    // Restore only CCR (low byte of SR), preserve high byte
    cpu->sr = (cpu->sr & 0xFF00) | (ccr & 0xFF);
    cpu->pc = return_pc;
}

void m68k_exec_stop(M68kCpu* cpu, u16 opcode) {
    (void)opcode;

    if (!(cpu->sr & M68K_SR_S)) {
        // Privilege Violation — don't consume the immediate word
        m68k_exception(cpu, 8);
        return;
    }

    u16 imm = m68k_fetch(cpu);
    cpu->sr = imm;
    // CPU should stop until interrupt/reset.
    // Emulation of "stopped" state is not fully implemented in this loop model yet.
    // For now, it just sets SR. A real run loop would check a 'stopped' flag.
}

void m68k_exec_reset(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (!(cpu->sr & M68K_SR_S)) {
        // Privilege Violation
        return;
    }

    // Asserts RESET line. In emulation, usually resets external devices.
    // No-op for now unless we have devices.
}
