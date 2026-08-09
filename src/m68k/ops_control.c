/**
 * @file ops_control.c
 * @brief Control-flow and privileged opcode implementations.
 */
#include "m68k_internal.h"

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
    bool is_word = (opcode & 0xFF) == 0;

    if (is_word) {
        disp = (s16)m68k_fetch(cpu);
    }

    if (cond == 1) {
        /* BSR */
        u32 target = is_word ? (cpu->pc - 2) + disp : cpu->pc + disp;
        m68k_push_32(cpu, cpu->pc);
        /* BSR pushes the return address before the target prefetch
         * faults, and the pushed frame PC is the odd target itself. */
        if (target & 1) {
            m68k_raise_odd_target_fault(cpu, target, target);
            cpu->cycles_remaining -= 18;
            return;
        }
        m68k_set_pc(cpu, target);
        cpu->cycles_remaining -= 18;
    } else if (m68k_check_condition(cpu, cond)) {
        /* Bcc taken */
        if (is_word)
            m68k_set_pc(cpu, (cpu->pc - 2) + disp);
        else
            m68k_set_pc(cpu, cpu->pc + disp);
        cpu->cycles_remaining -= 10;
    } else {
        /* Bcc not taken */
        cpu->cycles_remaining -= is_word ? 12 : 8;
    }
}

void m68k_exec_dbcc(M68kCpu* cpu, u16 opcode) {
    int condition = (opcode >> 8) & 0xF;
    int reg = opcode & 0x7;
    s16 displacement = (s16)m68k_fetch(cpu);

    if (m68k_check_condition(cpu, condition)) {
        cpu->cycles_remaining -= 12;  // Condition True
        return;
    }

    u16 val = cpu->d_regs[reg].l & 0xFFFF;
    val--;

    if (val != 0xFFFF) {
        u32 target = (cpu->pc - 2) + displacement;
        /* A faulted loop branch suppresses the counter writeback and
         * pushes the instruction address plus 4. */
        if (target & 1) {
            cpu->cycles_remaining -= 10;
            m68k_raise_odd_target_fault(cpu, target, cpu->pc);
            return;
        }
        cpu->d_regs[reg].l = (cpu->d_regs[reg].l & 0xFFFF0000) | val;
        cpu->cycles_remaining -= 10;  // Loop branched
        m68k_set_pc(cpu, target);
    } else {
        cpu->d_regs[reg].l = (cpu->d_regs[reg].l & 0xFFFF0000) | val;
        cpu->cycles_remaining -= 14;  // Loop expired
    }
}

void m68k_exec_scc(M68kCpu* cpu, u16 opcode) {
    int condition = (opcode >> 8) & 0xF;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    bool result = m68k_check_condition(cpu, condition);
    u8 val = result ? 0xFF : 0x00;

    M68kEA ea = m68k_calc_ea_addr(cpu, mode, reg, SIZE_BYTE);
    if (ea.is_reg && !ea.is_addr) {
        cpu->d_regs[ea.reg_num].l = (cpu->d_regs[ea.reg_num].l & 0xFFFFFF00) | val;
        cpu->cycles_remaining -= result ? 6 : 4;
    } else {
        m68k_write_8(cpu, ea.address, val);
        cpu->cycles_remaining -= 8;
    }
}

void m68k_exec_jmp(M68kCpu* cpu, u16 opcode) {
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    bool is_jsr = ((opcode >> 6) & 1) == 0;

    M68kEA ea = m68k_calc_ea_ctl(cpu, mode, reg, true);

    if (is_jsr) {
        /* JSR faults on the odd target before pushing the return
         * address, and the pushed frame PC is the PC after EA
         * resolution. */
        if (ea.address & 1) {
            m68k_raise_odd_target_fault(cpu, ea.address, cpu->pc);
            return;
        }
        m68k_push_32(cpu, cpu->pc);
    }

    m68k_set_pc(cpu, ea.address);
}

void m68k_exec_rts(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    m68k_set_pc(cpu, m68k_pop_32(cpu));
}

void m68k_exec_trap(M68kCpu* cpu, u16 opcode) {
    int vector = 32 + (opcode & 0xF);
    m68k_exception(cpu, vector);
}

void m68k_exec_rte(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (!(cpu->sr & M68K_SR_S)) {
        cpu->pc -= 2;
        m68k_exception(cpu, 8);
        return;
    }

    /* The 68010 validates the frame format word before committing; a
     * nonzero format nibble raises a format error with the frame kept
     * intact. */
    if (cpu->model >= M68K_MODEL_68010) {
        u32 sp = cpu->a_regs[7].l;
        u16 fmt = m68k_read_16(cpu, sp + 6);
        if ((fmt & 0xF000) != 0) {
            cpu->pc -= 2;
            m68k_exception(cpu, 14);
            return;
        }
        u16 new_sr = m68k_read_16(cpu, sp);
        u32 new_pc = m68k_read_32(cpu, sp + 2);
        cpu->a_regs[7].l = sp + 8;
        m68k_set_sr(cpu, new_sr);
        m68k_set_pc(cpu, new_pc);
        return;
    }

    u16 new_sr = m68k_pop_16(cpu);
    u32 new_pc = m68k_pop_32(cpu);
    m68k_set_sr(cpu, new_sr);
    m68k_set_pc(cpu, new_pc);
}

void m68k_exec_chk(M68kCpu* cpu, u16 opcode) {
    int reg_idx = (opcode >> 9) & 0x7;
    int mode = (opcode >> 3) & 0x7;
    int reg = opcode & 0x7;

    M68kEA ea = m68k_calc_ea(cpu, mode, reg, SIZE_WORD);
    s16 src = (s16)(cpu->d_regs[reg_idx].l & 0xFFFF);
    s16 bound = (s16)ea.value;

    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);
    if (src < 0) {
        cpu->sr |= M68K_SR_N;
    }

    if (src >= 0 && src <= bound) {
        return;
    }

    /* The greater-than-bound path costs 38 cycles. The negative path
     * costs 40, except that the microcode re-checks the raw N flag of
     * bound minus value without overflow correction and takes the
     * 38-cycle path when it is set. The dispatch charge is suppressed. */
    u16 rdiff = (u16)((u16)bound - (u16)src);
    int trap_cost = (src > bound) ? 38 : ((rdiff & 0x8000) ? 38 : 40);
    cpu->cycles_remaining -= trap_cost;
    m68k_exception(cpu, 6);
}

void m68k_exec_trapv(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (cpu->sr & M68K_SR_V) {
        m68k_exception(cpu, 7);
    }
}

void m68k_exec_rtr(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    u16 ccr = m68k_pop_16(cpu);
    u32 return_pc = m68k_pop_32(cpu);

    cpu->sr = (cpu->sr & 0xFF00) | (ccr & 0x1F);
    m68k_set_pc(cpu, return_pc);
}

void m68k_exec_stop(M68kCpu* cpu, u16 opcode) {
    (void)opcode;

    if (!(cpu->sr & M68K_SR_S)) {
        cpu->pc -= 2;
        m68k_exception(cpu, 8);
        return;
    }

    u16 imm = m68k_fetch(cpu);
    m68k_set_sr(cpu, imm);
    cpu->stopped = true;
}

void m68k_exec_reset(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (!(cpu->sr & M68K_SR_S)) {
        cpu->pc -= 2;
        m68k_exception(cpu, 8);
        return;
    }

    if (cpu->reset_cb) {
        cpu->reset_cb(cpu);
    }
}

void m68k_exec_movec(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    if (!(cpu->sr & M68K_SR_S)) {
        cpu->pc -= 2;
        m68k_exception(cpu, 8);
        return;
    }

    u16 ext = m68k_fetch(cpu);
    bool is_addr = (ext >> 15) & 1;
    int reg_num = (ext >> 12) & 0x7;
    int ctrl_reg = ext & 0xFFF;

    u32* gpr = is_addr ? &cpu->a_regs[reg_num].l : &cpu->d_regs[reg_num].l;

    bool to_ctrl = (opcode & 1) != 0;

    /* An undefined control register raises an illegal instruction
     * exception on real hardware. */
    if (ctrl_reg != 0x000 && ctrl_reg != 0x001 && ctrl_reg != 0x800 && ctrl_reg != 0x801) {
        m68k_exception(cpu, 4);
        return;
    }

    if (to_ctrl) {
        switch (ctrl_reg) {
            case 0x000:
                cpu->sfc = *gpr;
                break;
            case 0x001:
                cpu->dfc = *gpr;
                break;
            case 0x800:
                cpu->usp = *gpr;
                break;
            default:
                cpu->vbr = *gpr;
                break;
        }
    } else {
        switch (ctrl_reg) {
            case 0x000:
                *gpr = cpu->sfc;
                break;
            case 0x001:
                *gpr = cpu->dfc;
                break;
            case 0x800:
                *gpr = cpu->usp;
                break;
            default:
                *gpr = cpu->vbr;
                break;
        }
    }
}

void m68k_exec_rtd(M68kCpu* cpu, u16 opcode) {
    (void)opcode;
    u32 ret_addr = m68k_pop_32(cpu);
    s16 disp = (s16)m68k_fetch(cpu);
    cpu->a_regs[7].l += disp;
    m68k_set_pc(cpu, ret_addr);
}

void m68k_exec_bkpt(M68kCpu* cpu, u16 opcode) {
    (void)opcode;

    m68k_exception(cpu, 4);
}
