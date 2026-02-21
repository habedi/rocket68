#include <stdio.h>
#include <string.h>

#include "m68k_internal.h"

// -----------------------------------------------------------------------------
// Core / Memory / Fetch
// -----------------------------------------------------------------------------

void m68k_init(M68kCpu* cpu, u8* memory, u32 memory_size) {
    memset(cpu, 0, sizeof(M68kCpu));
    cpu->memory = memory;
    cpu->memory_size = memory_size;
    cpu->irq_level = 0;
    cpu->usp = 0;
    cpu->stopped = false;
    cpu->trace_pending = false;
}

void m68k_reset(M68kCpu* cpu) {
    for (int i = 0; i < 8; i++) {
        cpu->d_regs[i] = 0;
        cpu->a_regs[i] = 0;
    }
    cpu->sr = 0x2700;
    cpu->usp = 0;
    cpu->stopped = false;
    cpu->trace_pending = false;
    // Load initial SSP and PC from exception vectors (68000 hardware behavior)
    cpu->a_regs[7] = m68k_read_32(cpu, 0x00000000);  // Vector 0: Initial SSP
    cpu->pc = m68k_read_32(cpu, 0x00000004);         // Vector 1: Initial PC
}

void m68k_set_pc(M68kCpu* cpu, u32 pc) { cpu->pc = pc; }

u32 m68k_get_pc(M68kCpu* cpu) { return cpu->pc; }

void m68k_set_dr(M68kCpu* cpu, int reg, u32 value) {
    if (reg >= 0 && reg < 8) {
        cpu->d_regs[reg] = value;
    }
}

u32 m68k_get_dr(M68kCpu* cpu, int reg) {
    if (reg >= 0 && reg < 8) {
        return cpu->d_regs[reg];
    }
    return 0;
}

void m68k_set_ar(M68kCpu* cpu, int reg, u32 value) {
    if (reg >= 0 && reg < 8) {
        cpu->a_regs[reg] = value;
    }
}

u32 m68k_get_ar(M68kCpu* cpu, int reg) {
    if (reg >= 0 && reg < 8) {
        return cpu->a_regs[reg];
    }
    return 0;
}

static bool is_valid_address(M68kCpu* cpu, u32 address) { return address < cpu->memory_size; }

// Address error guard to prevent recursion during exception processing
static bool in_address_error = false;
static bool in_bus_error = false;

// EA cycle table: [mode][0=byte/word, 1=long]
// Based on 68000 Programmer's Reference Manual
static const int ea_cycles[12][2] = {
    {0, 0},    // Dn
    {0, 0},    // An
    {4, 8},    // (An)
    {4, 8},    // (An)+
    {6, 10},   // -(An)
    {8, 12},   // d16(An)
    {10, 14},  // d8(An,Xn)
    {8, 12},   // xxx.W
    {12, 16},  // xxx.L
    {8, 12},   // d16(PC)
    {10, 14},  // d8(PC,Xn)
    {4, 8},    // #imm
};

int m68k_ea_cycles(int mode, int reg, M68kSize size) {
    int idx;
    if (mode <= 6) {
        idx = mode;
    } else {
        // Mode 7 submodes
        switch (reg) {
            case 0:
                idx = 7;
                break;  // xxx.W
            case 1:
                idx = 8;
                break;  // xxx.L
            case 2:
                idx = 9;
                break;  // d16(PC)
            case 3:
                idx = 10;
                break;  // d8(PC,Xn)
            case 4:
                idx = 11;
                break;  // #imm
            default:
                return 0;
        }
    }
    return ea_cycles[idx][size == SIZE_LONG ? 1 : 0];
}

u8 m68k_read_8(M68kCpu* cpu, u32 address) {
    if (is_valid_address(cpu, address)) {
        return cpu->memory[address];
    }
    if (!in_bus_error) {
        in_bus_error = true;
        m68k_exception(cpu, 2);
        in_bus_error = false;
    }
    return 0;
}

u16 m68k_read_16(M68kCpu* cpu, u32 address) {
    if ((address & 1) && !in_address_error) {
        in_address_error = true;
        m68k_exception(cpu, 3);  // Address error
        in_address_error = false;
        return 0;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 1)) {
        return (cpu->memory[address] << 8) | cpu->memory[address + 1];
    }
    return 0;
}

u32 m68k_read_32(M68kCpu* cpu, u32 address) {
    if ((address & 1) && !in_address_error) {
        in_address_error = true;
        m68k_exception(cpu, 3);
        in_address_error = false;
        return 0;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 3)) {
        return (cpu->memory[address] << 24) | (cpu->memory[address + 1] << 16) |
               (cpu->memory[address + 2] << 8) | cpu->memory[address + 3];
    }
    return 0;
}

void m68k_write_8(M68kCpu* cpu, u32 address, u8 value) {
    if (address < cpu->memory_size) {
        cpu->memory[address] = value;
    } else if (address == 0xE00000) {
        putchar(value);
        fflush(stdout);
    } else if (!in_bus_error) {
        in_bus_error = true;
        m68k_exception(cpu, 2);
        in_bus_error = false;
    }
}

void m68k_write_16(M68kCpu* cpu, u32 address, u16 value) {
    if ((address & 1) && !in_address_error) {
        in_address_error = true;
        m68k_exception(cpu, 3);
        in_address_error = false;
        return;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 1)) {
        cpu->memory[address] = (value >> 8) & 0xFF;
        cpu->memory[address + 1] = value & 0xFF;
    }
}

void m68k_write_32(M68kCpu* cpu, u32 address, u32 value) {
    if ((address & 1) && !in_address_error) {
        in_address_error = true;
        m68k_exception(cpu, 3);
        in_address_error = false;
        return;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 3)) {
        cpu->memory[address] = (value >> 24) & 0xFF;
        cpu->memory[address + 1] = (value >> 16) & 0xFF;
        cpu->memory[address + 2] = (value >> 8) & 0xFF;
        cpu->memory[address + 3] = value & 0xFF;
    }
}

// Fetch 16-bit instruction word
u16 m68k_fetch(M68kCpu* cpu) {
    u16 opcode = m68k_read_16(cpu, cpu->pc);
    cpu->pc += 2;
    return opcode;
}

u32 fetch_extension(M68kCpu* cpu) { return m68k_fetch(cpu); }

u32 m68k_read_size(M68kCpu* cpu, u32 address, M68kSize size) {
    if (size == SIZE_BYTE) return m68k_read_8(cpu, address);
    if (size == SIZE_WORD) return m68k_read_16(cpu, address);
    if (size == SIZE_LONG) return m68k_read_32(cpu, address);
    return 0;
}

void m68k_write_size(M68kCpu* cpu, u32 address, u32 value, M68kSize size) {
    if (size == SIZE_BYTE)
        m68k_write_8(cpu, address, (u8)value);
    else if (size == SIZE_WORD)
        m68k_write_16(cpu, address, (u16)value);
    else if (size == SIZE_LONG)
        m68k_write_32(cpu, address, value);
}

// Calculate Effective Address
M68kEA m68k_calc_ea(M68kCpu* cpu, int mode, int reg, M68kSize size) {
    M68kEA ea = {0};

    switch (mode) {
        case 0:  // Data Register Direct
            ea.is_reg = true;
            ea.reg_num = reg;
            ea.is_addr = false;
            ea.value = cpu->d_regs[reg];
            break;
        case 1:  // Address Register Direct
            ea.is_reg = true;
            ea.reg_num = reg;
            ea.is_addr = true;
            ea.value = cpu->a_regs[reg];
            break;
        case 2:  // Address Register Indirect
            ea.address = cpu->a_regs[reg];
            ea.value = m68k_read_size(cpu, ea.address, size);
            break;
        case 3:  // Address Register Indirect with Postincrement
            ea.address = cpu->a_regs[reg];
            ea.value = m68k_read_size(cpu, ea.address, size);
            // Increment register (A7 always stays word-aligned)
            cpu->a_regs[reg] += (size == SIZE_BYTE && reg == 7) ? 2 : size;
            break;
        case 4:  // Address Register Indirect with Predecrement
                 // Decrement first (A7 always stays word-aligned)
            cpu->a_regs[reg] -= (size == SIZE_BYTE && reg == 7) ? 2 : size;
            ea.address = cpu->a_regs[reg];
            ea.value = m68k_read_size(cpu, ea.address, size);
            break;
        case 5:  // Address Register Indirect with Displacement
        {
            s16 disp = (s16)fetch_extension(cpu);
            ea.address = cpu->a_regs[reg] + disp;
            ea.value = m68k_read_size(cpu, ea.address, size);
        } break;
        case 7:  // Other modes
            switch (reg) {
                case 0:  // Absolute Short
                    ea.address = (s32)(s16)fetch_extension(cpu);
                    ea.value = m68k_read_size(cpu, ea.address, size);
                    break;
                case 1:  // Absolute Long
                {
                    u32 high = fetch_extension(cpu);
                    u32 low = fetch_extension(cpu);
                    ea.address = (high << 16) | low;
                    ea.value = m68k_read_size(cpu, ea.address, size);
                } break;
                case 4:  // Immediate
                    if (size == SIZE_LONG) {
                        u32 high = fetch_extension(cpu);
                        u32 low = fetch_extension(cpu);
                        ea.value = (high << 16) | low;
                    } else {
                        ea.value = fetch_extension(cpu);
                        if (size == SIZE_BYTE) ea.value &= 0xFF;
                    }
                    ea.is_reg = true;
                    break;
                case 2:  // PC with Displacement (d16,PC)
                {
                    s16 disp = (s16)fetch_extension(cpu);
                    ea.address = (cpu->pc - 2) + disp;
                    ea.value = m68k_read_size(cpu, ea.address, size);
                } break;
                case 3:  // PC with Index (d8,PC,Xn)
                {
                    u16 ext = fetch_extension(cpu);
                    u32 pc_base = (cpu->pc - 2);
                    s8 disp8 = (s8)(ext & 0xFF);
                    int xn_reg_num = (ext >> 12) & 0x7;
                    int xn_is_addr = (ext >> 15) & 0x1;
                    int xn_is_long = (ext >> 11) & 0x1;

                    u32 xn_val = xn_is_addr ? cpu->a_regs[xn_reg_num] : cpu->d_regs[xn_reg_num];
                    if (!xn_is_long) {
                        xn_val = (s32)(s16)(xn_val & 0xFFFF);
                    }

                    ea.address = pc_base + xn_val + disp8;
                    ea.value = m68k_read_size(cpu, ea.address, size);
                } break;
                default:
                    break;
            }
            break;
        case 6:  // Address Register Indirect with Index (d8,An,Xn)
        {
            u16 ext = fetch_extension(cpu);
            s8 disp8 = (s8)(ext & 0xFF);
            int xn_reg_num = (ext >> 12) & 0x7;
            int xn_is_addr = (ext >> 15) & 0x1;
            int xn_is_long = (ext >> 11) & 0x1;

            u32 xn_val = xn_is_addr ? cpu->a_regs[xn_reg_num] : cpu->d_regs[xn_reg_num];
            if (!xn_is_long) {
                xn_val = (s32)(s16)(xn_val & 0xFFFF);
            }

            ea.address = cpu->a_regs[reg] + xn_val + disp8;
            ea.value = m68k_read_size(cpu, ea.address, size);
        } break;
    }
    return ea;
}

// -----------------------------------------------------------------------------
// Flags Helper Implementations
// -----------------------------------------------------------------------------

void update_flags_add(M68kCpu* cpu, u32 src, u32 dest, u32 result, M68kSize size) {
    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C | M68K_SR_X);

    u32 msb_mask = (size == SIZE_BYTE) ? 0x80 : (size == SIZE_WORD) ? 0x8000 : 0x80000000;

    if (result & msb_mask) cpu->sr |= M68K_SR_N;

    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;
    if ((result & value_mask) == 0) cpu->sr |= M68K_SR_Z;

    bool sm = (src & msb_mask) != 0;
    bool dm = (dest & msb_mask) != 0;
    bool rm = (result & msb_mask) != 0;

    if ((sm && dm && !rm) || (!sm && !dm && rm)) cpu->sr |= M68K_SR_V;

    if ((sm && dm) || (!rm && dm) || (sm && !rm)) {
        cpu->sr |= M68K_SR_C;
        cpu->sr |= M68K_SR_X;
    }
}

void update_flags_sub(M68kCpu* cpu, u32 src, u32 dest, u32 result, M68kSize size) {
    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C | M68K_SR_X);

    u32 msb_mask = (size == SIZE_BYTE) ? 0x80 : (size == SIZE_WORD) ? 0x8000 : 0x80000000;
    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;

    if (result & msb_mask) cpu->sr |= M68K_SR_N;

    if ((result & value_mask) == 0) cpu->sr |= M68K_SR_Z;

    bool sm = (src & msb_mask) != 0;
    bool dm = (dest & msb_mask) != 0;
    bool rm = (result & msb_mask) != 0;

    if ((!sm && dm && !rm) || (sm && !dm && rm)) cpu->sr |= M68K_SR_V;

    if ((sm && !dm) || (rm && !dm) || (sm && rm)) {
        cpu->sr |= M68K_SR_C;
        cpu->sr |= M68K_SR_X;
    }
}

void update_flags_logic(M68kCpu* cpu, u32 result, M68kSize size) {
    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);

    u32 msb_mask = (size == SIZE_BYTE) ? 0x80 : (size == SIZE_WORD) ? 0x8000 : 0x80000000;
    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;

    if (result & msb_mask) cpu->sr |= M68K_SR_N;
    if ((result & value_mask) == 0) cpu->sr |= M68K_SR_Z;
}

// -----------------------------------------------------------------------------
// Stack Helpers
// -----------------------------------------------------------------------------

void m68k_push_32(M68kCpu* cpu, u32 value) {
    cpu->a_regs[7] -= 4;
    m68k_write_32(cpu, cpu->a_regs[7], value);
}

u32 m68k_pop_32(M68kCpu* cpu) {
    u32 value = m68k_read_32(cpu, cpu->a_regs[7]);
    cpu->a_regs[7] += 4;
    return value;
}

void m68k_push_16(M68kCpu* cpu, u16 value) {
    cpu->a_regs[7] -= 2;
    m68k_write_16(cpu, cpu->a_regs[7], value);
}

u16 m68k_pop_16(M68kCpu* cpu) {
    u16 value = m68k_read_16(cpu, cpu->a_regs[7]);
    cpu->a_regs[7] += 2;
    return value;
}

// -----------------------------------------------------------------------------
// Exception Handling
// -----------------------------------------------------------------------------

void m68k_exception(M68kCpu* cpu, int vector) {
    u16 old_sr = cpu->sr;

    // 1. Make temporary copy of SR
    // 2. Set S-bit (Supervisor Mode)
    // 3. Clear T-bit (Trace) - Not implemented yet
    cpu->sr |= M68K_SR_S;

    // 4. Push PC
    m68k_push_32(cpu, cpu->pc);

    // 5. Push SR
    m68k_push_16(cpu, old_sr);

    // 6. Fetch Vector
    u32 vector_addr = m68k_read_32(cpu, vector * 4);

    // 7. Jump
    cpu->pc = vector_addr;
}

// -----------------------------------------------------------------------------
// Interrupts
// -----------------------------------------------------------------------------

void m68k_set_irq(M68kCpu* cpu, int level) { cpu->irq_level = level; }

static bool check_interrupts(M68kCpu* cpu) {
    if (cpu->irq_level == 0) return false;

    int current_level = (cpu->sr >> 8) & 0x7;
    // Level 7 is NMI (Non-Maskable Interrupt), always accepted
    if (cpu->irq_level > current_level || cpu->irq_level == 7) {
        // Acknowledge Interrupt (Autovector for now)
        int vector = 24 + cpu->irq_level;  // Vectors 25-31

        // Update SR mask to new level
        cpu->sr &= ~0x0700;
        cpu->sr |= (cpu->irq_level << 8);

        m68k_exception(cpu, vector);

        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// Main Dispatch Loop
// -----------------------------------------------------------------------------

int m68k_step(M68kCpu* cpu) {
    // Stopped state: return immediately until an interrupt arrives
    if (cpu->stopped) {
        if (cpu->irq_level > 0) {
            cpu->stopped = false;
        } else {
            return 4;  // Idle cycle
        }
    }

    // Check for interrupts before fetching
    if (check_interrupts(cpu)) {
        return 44;
    }

    // Save trace flag state before execution (trace fires AFTER instruction)
    bool trace_active = (cpu->sr & 0x8000) != 0;  // T bit = bit 15

    u16 opcode = m68k_fetch(cpu);
    int cycles = 4;  // Default

    // =========================================================================
    // 0000 xxxx — Bit manipulation, MOVEP, immediate operations
    // =========================================================================
    if ((opcode & 0xF000) == 0x0000) {
        int top4 = (opcode >> 8) & 0xF;

        // MOVES (68010+): 0000 1110 ss ...
        if (top4 == 0x0E) {
            m68k_exec_moves(cpu, opcode);
            cycles = 4;
            goto done;
        }

        // ORI / ORI to CCR / ORI to SR: 0000 0000
        if (top4 == 0x00) {
            m68k_exec_ori(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // ANDI / ANDI to CCR / ANDI to SR: 0000 0010
        if (top4 == 0x02) {
            m68k_exec_andi(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // SUBI: 0000 0100
        if (top4 == 0x04) {
            m68k_exec_subi(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // ADDI: 0000 0110
        if (top4 == 0x06) {
            m68k_exec_addi(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // EORI / EORI to CCR / EORI to SR: 0000 1010
        if (top4 == 0x0A) {
            m68k_exec_eori(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // CMPI: 0000 1100
        if (top4 == 0x0C) {
            m68k_exec_cmpi(cpu, opcode);
            cycles = 8;
            goto done;
        }

        // Static bit operations (immediate bit#): 0000 1000 xx
        if (top4 == 0x08) {
            int subop = (opcode >> 6) & 0x3;
            switch (subop) {
                case 0:
                    m68k_exec_btst(cpu, opcode);
                    cycles = 8;
                    goto done;
                case 1:
                    m68k_exec_bchg(cpu, opcode);
                    cycles = 8;
                    goto done;
                case 2:
                    m68k_exec_bclr(cpu, opcode);
                    cycles = 10;
                    goto done;
                case 3:
                    m68k_exec_bset(cpu, opcode);
                    cycles = 8;
                    goto done;
            }
        }

        // Dynamic bit ops or MOVEP: 0000 xxx1 xxxx xxxx
        if (opcode & 0x0100) {
            int mode = (opcode >> 3) & 0x7;
            // MOVEP: mode == 1 (address register direct encoding)
            if (mode == 1) {
                m68k_exec_movep(cpu, opcode);
                cycles = 16;
                goto done;
            }
            // Dynamic bit operations
            int subop = (opcode >> 6) & 0x3;
            switch (subop) {
                case 0:
                    m68k_exec_btst(cpu, opcode);
                    cycles = 6;
                    goto done;
                case 1:
                    m68k_exec_bchg(cpu, opcode);
                    cycles = 8;
                    goto done;
                case 2:
                    m68k_exec_bclr(cpu, opcode);
                    cycles = 8;
                    goto done;
                case 3:
                    m68k_exec_bset(cpu, opcode);
                    cycles = 8;
                    goto done;
            }
        }
    }

    // =========================================================================
    // 00xx xxxx — MOVE (size != 0)
    // =========================================================================
    if ((opcode & 0xC000) == 0x0000) {
        int size_bits = (opcode >> 12) & 0x3;
        if (size_bits != 0) {
            m68k_exec_move(cpu, opcode);
            cycles = 4;
            goto done;
        }
    }

    // =========================================================================
    // 0100 xxxx — Miscellaneous (one-word group)
    // =========================================================================
    if ((opcode & 0xF000) == 0x4000) {
        // Exact-match opcodes first
        if (opcode == 0x4E70) {
            m68k_exec_reset(cpu, opcode);
            cycles = 132;
            goto done;
        }
        if (opcode == 0x4E71) {
            cycles = 4;
            goto done;
        }  // NOP
        if (opcode == 0x4E72) {
            m68k_exec_stop(cpu, opcode);
            cycles = 4;
            goto done;
        }
        if (opcode == 0x4E73) {
            m68k_exec_rte(cpu, opcode);
            cycles = 20;
            goto done;
        }
        if (opcode == 0x4E75) {
            m68k_exec_rts(cpu, opcode);
            cycles = 16;
            goto done;
        }
        if (opcode == 0x4E76) {
            m68k_exec_trapv(cpu, opcode);
            cycles = 4;
            goto done;
        }
        if (opcode == 0x4E77) {
            m68k_exec_rtr(cpu, opcode);
            cycles = 20;
            goto done;
        }
        // 68010+ exact matches
        if (opcode == 0x4E74) {
            m68k_exec_rtd(cpu, opcode);
            cycles = 16;
            goto done;
        }  // RTD
        if (opcode == 0x4E7A || opcode == 0x4E7B) {
            m68k_exec_movec(cpu, opcode);
            cycles = 12;
            goto done;
        }  // MOVEC

        // BKPT: 0100 1110 0100 1xxx
        if ((opcode & 0xFFF8) == 0x4E48) {
            m68k_exec_bkpt(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // TRAP: 0100 1110 0100 xxxx
        if ((opcode & 0xFFF0) == 0x4E40) {
            m68k_exec_trap(cpu, opcode);
            cycles = 34;
            goto done;
        }
        // MOVE USP: 0100 1110 0110 xyyy
        if ((opcode & 0xFFF0) == 0x4E60) {
            m68k_exec_move_usp(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // JSR: 0100 1110 10xx xxxx
        if ((opcode & 0xFFC0) == 0x4E80) {
            m68k_exec_jmp(cpu, opcode);
            cycles = 16;
            goto done;
        }
        // JMP: 0100 1110 11xx xxxx
        if ((opcode & 0xFFC0) == 0x4EC0) {
            m68k_exec_jmp(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // MOVE from SR: 0100 0000 11xx xxxx
        if ((opcode & 0xFFC0) == 0x40C0) {
            m68k_exec_move_sr(cpu, opcode);
            cycles = 6;
            goto done;
        }
        // MOVE to CCR: 0100 0100 11xx xxxx
        if ((opcode & 0xFFC0) == 0x44C0) {
            m68k_exec_move_ccr(cpu, opcode);
            cycles = 12;
            goto done;
        }
        // MOVE to SR: 0100 0110 11xx xxxx
        if ((opcode & 0xFFC0) == 0x46C0) {
            m68k_exec_move_sr(cpu, opcode);
            cycles = 12;
            goto done;
        }
        // CHK: 0100 xxx1 10xx xxxx
        if ((opcode & 0xF1C0) == 0x4180) {
            m68k_exec_chk(cpu, opcode);
            cycles = 10;
            goto done;
        }
        // LEA: 0100 xxx1 11xx xxxx
        if ((opcode & 0xF1C0) == 0x41C0) {
            m68k_exec_lea(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // EXTB (68020+): 0100 1001 11 000 xxx
        if ((opcode & 0xFFF8) == 0x49C0) {
            m68k_exec_extb(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // MOVEM / EXT: 0100 1x00 1xxx xxxx
        if ((opcode & 0xFB80) == 0x4880) {
            int mode = (opcode >> 3) & 0x7;
            if (mode == 0) {
                m68k_exec_ext(cpu, opcode);
                cycles = 4;
                goto done;
            }
            m68k_exec_movem(cpu, opcode);
            cycles = 12;
            goto done;
        }
        // LINK: 0100 1110 0101 0xxx
        if ((opcode & 0xFFF8) == 0x4E50) {
            m68k_exec_link(cpu, opcode);
            cycles = 16;
            goto done;
        }
        // UNLK: 0100 1110 0101 1xxx
        if ((opcode & 0xFFF8) == 0x4E58) {
            m68k_exec_unlk(cpu, opcode);
            cycles = 12;
            goto done;
        }
        // TAS: 0100 1010 11xx xxxx
        if ((opcode & 0xFFC0) == 0x4AC0) {
            m68k_exec_tas(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // NBCD: 0100 1000 00xx xxxx
        if ((opcode & 0xFFC0) == 0x4800) {
            m68k_exec_nbcd(cpu, opcode);
            cycles = 8;
            goto done;
        }
        // SWAP / PEA: 0100 1000 01xx xxxx
        if ((opcode & 0xFFC0) == 0x4840) {
            int mode = (opcode >> 3) & 0x7;
            if (mode == 0) {
                m68k_exec_swap(cpu, opcode);
            } else {
                m68k_exec_pea(cpu, opcode);
            }
            cycles = 4;
            goto done;
        }
        // TST: 0100 1010 ssxx xxxx (but not TAS which is 0x4AC0)
        if ((opcode & 0xFF00) == 0x4A00) {
            m68k_exec_tst(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // NEGX: 0100 0000 ssxx xxxx (but not MOVE from SR which is 0x40C0)
        if ((opcode & 0xFF00) == 0x4000) {
            m68k_exec_negx(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // CLR: 0100 0010 ssxx xxxx
        if ((opcode & 0xFF00) == 0x4200) {
            m68k_exec_clr(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // NEG: 0100 0100 ssxx xxxx (but not MOVE to CCR which is 0x44C0)
        if ((opcode & 0xFF00) == 0x4400) {
            m68k_exec_neg(cpu, opcode);
            cycles = 4;
            goto done;
        }
        // NOT: 0100 0110 ssxx xxxx (but not MOVE to SR which is 0x46C0)
        if ((opcode & 0xFF00) == 0x4600) {
            m68k_exec_not(cpu, opcode);
            cycles = 4;
            goto done;
        }
    }

    // =========================================================================
    // 0101 xxxx — ADDQ / SUBQ / Scc / DBcc
    // =========================================================================
    if ((opcode & 0xF000) == 0x5000) {
        if ((opcode & 0x00C0) == 0x00C0) {
            int mode = (opcode >> 3) & 0x7;
            if (mode == 1) {
                m68k_exec_dbcc(cpu, opcode);
                cycles = 10;
                goto done;
            } else {
                m68k_exec_scc(cpu, opcode);
                cycles = 8;
                goto done;
            }
        }
        bool is_sub = (opcode & 0x0100) != 0;
        if (is_sub)
            m68k_exec_subq(cpu, opcode);
        else
            m68k_exec_addq(cpu, opcode);
        cycles = 4;
        goto done;
    }

    // =========================================================================
    // 0110 xxxx — Bcc / BRA / BSR
    // =========================================================================
    if ((opcode & 0xF000) == 0x6000) {
        m68k_exec_bcc(cpu, opcode);
        cycles = 10;
        goto done;
    }

    // =========================================================================
    // 0111 xxxx — MOVEQ
    // =========================================================================
    if ((opcode & 0xF100) == 0x7000) {
        m68k_exec_moveq(cpu, opcode);
        cycles = 4;
        goto done;
    }

    // =========================================================================
    // 1000 xxxx — OR / DIV / SBCD
    // =========================================================================
    if ((opcode & 0xF000) == 0x8000) {
        int opmode = (opcode >> 6) & 0x7;
        int mode = (opcode >> 3) & 0x7;
        if (opmode == 3 || opmode == 7) {
            m68k_exec_div(cpu, opcode);
            cycles = 140;
            goto done;
        } else if (opmode == 4 && (mode == 0 || mode == 1)) {
            m68k_exec_sbcd(cpu, opcode);
            cycles = 6;
            goto done;
        } else {
            m68k_exec_or(cpu, opcode);
            cycles = 4;
            goto done;
        }
    }

    // =========================================================================
    // 1001 xxxx — SUB / SUBA / SUBX
    // =========================================================================
    if ((opcode & 0xF000) == 0x9000) {
        int opmode = (opcode >> 6) & 0x7;
        int mode = (opcode >> 3) & 0x7;
        if ((opmode >= 4 && opmode <= 6) && (mode == 0 || mode == 1)) {
            m68k_exec_subx(cpu, opcode);
            cycles = 4;
            goto done;
        }
        m68k_exec_sub(cpu, opcode);
        cycles = 4;
        goto done;
    }

    // =========================================================================
    // 1011 xxxx — CMP / CMPA / CMPM / EOR
    // =========================================================================
    if ((opcode & 0xF000) == 0xB000) {
        int opmode = (opcode >> 6) & 0x7;
        int mode = (opcode >> 3) & 0x7;
        bool is_cmpm = (opmode >= 4 && opmode <= 6 && mode == 1);
        bool is_eor = (opmode >= 4 && opmode <= 6 && !is_cmpm);
        if (is_eor) {
            m68k_exec_eor(cpu, opcode);
        } else {
            m68k_exec_cmp(cpu, opcode);
        }
        cycles = 4;
        goto done;
    }

    // =========================================================================
    // 1100 xxxx — AND / MUL / ABCD / EXG
    // =========================================================================
    if ((opcode & 0xF000) == 0xC000) {
        int opmode = (opcode >> 6) & 0x7;
        int mode = (opcode >> 3) & 0x7;
        if (opmode == 3 || opmode == 7) {
            m68k_exec_mul(cpu, opcode);
            cycles = 70;
            goto done;
        } else if (opmode == 4 && (mode == 0 || mode == 1)) {
            m68k_exec_abcd(cpu, opcode);
            cycles = 6;
            goto done;
        } else if ((opmode == 5 && ((opcode >> 3) & 0x1F) == 0x08) ||
                   (opmode == 5 && ((opcode >> 3) & 0x1F) == 0x09) ||
                   (opmode == 6 && ((opcode >> 3) & 0x1F) == 0x11)) {
            m68k_exec_exg(cpu, opcode);
            cycles = 6;
            goto done;
        } else {
            m68k_exec_and(cpu, opcode);
            cycles = 4;
            goto done;
        }
    }

    // =========================================================================
    // 1101 xxxx — ADD / ADDA / ADDX
    // =========================================================================
    if ((opcode & 0xF000) == 0xD000) {
        int opmode = (opcode >> 6) & 0x7;
        int mode = (opcode >> 3) & 0x7;
        if ((opmode >= 4 && opmode <= 6) && (mode == 0 || mode == 1)) {
            m68k_exec_addx(cpu, opcode);
            cycles = 4;
            goto done;
        }
        m68k_exec_add(cpu, opcode);
        cycles = 4;
        goto done;
    }

    // =========================================================================
    // 1110 xxxx — Shift / Rotate
    // =========================================================================
    if ((opcode & 0xF000) == 0xE000) {
        m68k_exec_shift(cpu, opcode);
        cycles = 6;
        goto done;
    }

    // Unrecognized opcode: illegal instruction exception
    m68k_exception(cpu, 4);
    cycles = 34;

done:
    // Trace mode: if T bit was set before execution, fire trace exception
    if (trace_active) {
        m68k_exception(cpu, 9);  // Trace vector
        cycles += 34;
    }

    return cycles;
}
