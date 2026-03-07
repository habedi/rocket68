/**
 * @file m68k.c
 * @brief Core CPU state, memory access, execution, and exception handling.
 */
#include <stdio.h>
#include <string.h>

#include "m68k_internal.h"

void m68k_init(M68kCpu* cpu, u8* memory, u32 memory_size) {
    memset(cpu, 0, sizeof(M68kCpu));
    cpu->memory = memory;
    cpu->memory_size = memory_size;
    cpu->irq_level = 0;
    cpu->usp = 0;
    cpu->stopped = false;
    cpu->trace_pending = false;
    cpu->in_address_error = false;
    cpu->in_bus_error = false;
    cpu->fault_program_access = false;
    cpu->fault_valid = false;
    cpu->fault_trap_active = false;
}

void m68k_reset(M68kCpu* cpu) {
    for (int i = 0; i < 8; i++) {
        cpu->d_regs[i].l = 0;
        cpu->a_regs[i].l = 0;
    }
    cpu->sr = 0x2700;
    cpu->usp = 0;
    cpu->stopped = false;
    cpu->trace_pending = false;
    cpu->exception_thrown = 0;
    cpu->irq_level = 0;
    cpu->in_address_error = false;
    cpu->in_bus_error = false;
    cpu->fault_program_access = false;
    cpu->fault_valid = false;
    cpu->fault_trap_active = false;
    cpu->ppc = 0;
    cpu->ir = 0;

    cpu->a_regs[7].l = m68k_read_32(cpu, 0x00000000);
    cpu->pc = m68k_read_32(cpu, 0x00000004);
}

void m68k_set_pc(M68kCpu* cpu, u32 pc) {
    cpu->pc = pc;
    if (cpu->pc_changed_cb) cpu->pc_changed_cb(cpu, pc);
}

u32 m68k_get_pc(M68kCpu* cpu) { return cpu->pc; }

void m68k_set_dr(M68kCpu* cpu, int reg, u32 value) {
    if (reg >= 0 && reg < 8) {
        cpu->d_regs[reg].l = value;
    }
}

u32 m68k_get_dr(M68kCpu* cpu, int reg) {
    if (reg >= 0 && reg < 8) {
        return cpu->d_regs[reg].l;
    }
    return 0;
}

void m68k_set_ar(M68kCpu* cpu, int reg, u32 value) {
    if (reg >= 0 && reg < 8) {
        cpu->a_regs[reg].l = value;
    }
}

u32 m68k_get_ar(M68kCpu* cpu, int reg) {
    if (reg >= 0 && reg < 8) {
        return cpu->a_regs[reg].l;
    }
    return 0;
}

static inline u32 mask_address_24(u32 address) { return address & 0x00FFFFFFu; }

static bool is_valid_address(M68kCpu* cpu, u32 address) { return address < cpu->memory_size; }

void m68k_set_wait_bus_callback(M68kCpu* cpu, M68kWaitBusCallback callback) {
    cpu->wait_bus = callback;
}

void m68k_set_int_ack_callback(M68kCpu* cpu, M68kIntAckCallback callback) {
    cpu->int_ack = callback;
}

void m68k_set_fc_callback(M68kCpu* cpu, M68kFcCallback callback) { cpu->fc_cb = callback; }

void m68k_set_instr_hook_callback(M68kCpu* cpu, M68kInstrHookCallback callback) {
    cpu->instr_hook_cb = callback;
}

void m68k_set_pc_changed_callback(M68kCpu* cpu, M68kPcChangedCallback callback) {
    cpu->pc_changed_cb = callback;
}

void m68k_set_reset_callback(M68kCpu* cpu, M68kResetCallback callback) { cpu->reset_cb = callback; }

void m68k_set_tas_callback(M68kCpu* cpu, M68kTasCallback callback) { cpu->tas_cb = callback; }

void m68k_set_illg_callback(M68kCpu* cpu, M68kIllgCallback callback) { cpu->illg_cb = callback; }
void m68k_set_read8_callback(M68kCpu* cpu, M68kRead8Callback callback) { cpu->read8_cb = callback; }
void m68k_set_read16_callback(M68kCpu* cpu, M68kRead16Callback callback) {
    cpu->read16_cb = callback;
}
void m68k_set_read32_callback(M68kCpu* cpu, M68kRead32Callback callback) {
    cpu->read32_cb = callback;
}
void m68k_set_write8_callback(M68kCpu* cpu, M68kWrite8Callback callback) {
    cpu->write8_cb = callback;
}
void m68k_set_write16_callback(M68kCpu* cpu, M68kWrite16Callback callback) {
    cpu->write16_cb = callback;
}
void m68k_set_write32_callback(M68kCpu* cpu, M68kWrite32Callback callback) {
    cpu->write32_cb = callback;
}

static inline unsigned int current_fc(const M68kCpu* cpu, bool is_program) {
    bool is_supervisor = (cpu->sr & M68K_SR_S) != 0;
    if (is_supervisor) {
        return is_program ? M68K_FC_SUPV_PROG : M68K_FC_SUPV_DATA;
    }
    return is_program ? M68K_FC_USER_PROG : M68K_FC_USER_DATA;
}

static inline void set_fc(M68kCpu* cpu, bool is_program) {
    if (cpu->fc_cb) cpu->fc_cb(cpu, current_fc(cpu, is_program));
}

#define AERR_INSTRUCTION_NO 0x08u
#define AERR_MODE_READ 0x10u

static inline void capture_access_fault(M68kCpu* cpu, u32 address, bool is_write, bool is_program) {
    u16 fc = (u16)(current_fc(cpu, is_program) & 0x7u);
    u16 rw = (u16)(is_write ? 0u : AERR_MODE_READ);

    cpu->fault_address = address;
    cpu->fault_ir = cpu->ir;
    cpu->fault_ssw = (u16)((cpu->fault_ir & 0xFFE0u) | rw | fc);
    cpu->fault_program_access = is_program;
    cpu->fault_valid = true;
}

static inline void abort_faulted_instruction(M68kCpu* cpu) {
    if (cpu->fault_trap_active && (cpu->exception_thrown == 2 || cpu->exception_thrown == 3)) {
        longjmp(cpu->fault_trap, 1);
    }
}

static const int ea_cycles[12][2] = {
    {0, 0},   {0, 0},  {4, 8},   {4, 8},  {6, 10},  {8, 12},
    {10, 14}, {8, 12}, {12, 16}, {8, 12}, {10, 14}, {4, 8},
};

int m68k_ea_cycles(int mode, int reg, M68kSize size) {
    int idx;
    if (mode <= 6) {
        idx = mode;
    } else {
        switch (reg) {
            case 0:
                idx = 7;
                break;
            case 1:
                idx = 8;
                break;
            case 2:
                idx = 9;
                break;
            case 3:
                idx = 10;
                break;
            case 4:
                idx = 11;
                break;
            default:
                return 0;
        }
    }
    return ea_cycles[idx][size == SIZE_LONG ? 1 : 0];
}

u8 m68k_read_8(M68kCpu* cpu, u32 address) {
    u32 raw_address = address;
    address = mask_address_24(address);
    if (cpu->read8_cb) {
        return cpu->read8_cb(cpu, address);
    }
    set_fc(cpu, false);
    if (cpu->wait_bus) cpu->wait_bus(cpu, address, SIZE_BYTE);
    if (is_valid_address(cpu, address)) {
        return cpu->memory[address];
    }
    if (!cpu->in_bus_error) {
        capture_access_fault(cpu, raw_address, false, false);
        cpu->in_bus_error = true;
        m68k_exception(cpu, 2);
        cpu->in_bus_error = false;
        abort_faulted_instruction(cpu);
    }
    return 0;
}

u16 m68k_read_16(M68kCpu* cpu, u32 address) {
    u32 raw_address = address;
    address = mask_address_24(address);
    if (cpu->read16_cb) {
        return cpu->read16_cb(cpu, address);
    }
    set_fc(cpu, false);
    if (cpu->wait_bus) cpu->wait_bus(cpu, address, SIZE_WORD);
    if ((address & 1) && !cpu->in_address_error) {
        capture_access_fault(cpu, raw_address, false, false);
        cpu->in_address_error = true;
        m68k_exception(cpu, 3);
        cpu->in_address_error = false;
        abort_faulted_instruction(cpu);
        return 0;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 1)) {
        return (u16)((cpu->memory[address] << 8) | cpu->memory[address + 1]);
    }
    if (!cpu->in_bus_error) {
        capture_access_fault(cpu, raw_address, false, false);
        cpu->in_bus_error = true;
        m68k_exception(cpu, 2);
        cpu->in_bus_error = false;
        abort_faulted_instruction(cpu);
    }
    return 0;
}

u32 m68k_read_32(M68kCpu* cpu, u32 address) {
    u32 raw_address = address;
    address = mask_address_24(address);
    if (cpu->read32_cb) {
        return cpu->read32_cb(cpu, address);
    }
    set_fc(cpu, false);
    if (cpu->wait_bus) cpu->wait_bus(cpu, address, SIZE_LONG);
    if ((address & 1) && !cpu->in_address_error) {
        capture_access_fault(cpu, raw_address, false, false);
        cpu->in_address_error = true;
        m68k_exception(cpu, 3);
        cpu->in_address_error = false;
        abort_faulted_instruction(cpu);
        return 0;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 3)) {
        return ((u32)cpu->memory[address] << 24) | ((u32)cpu->memory[address + 1] << 16) |
               ((u32)cpu->memory[address + 2] << 8) | (u32)cpu->memory[address + 3];
    }
    if (!cpu->in_bus_error) {
        capture_access_fault(cpu, raw_address, false, false);
        cpu->in_bus_error = true;
        m68k_exception(cpu, 2);
        cpu->in_bus_error = false;
        abort_faulted_instruction(cpu);
    }
    return 0;
}

void m68k_write_8(M68kCpu* cpu, u32 address, u8 value) {
    u32 raw_address = address;
    address = mask_address_24(address);
    if (cpu->write8_cb) {
        cpu->write8_cb(cpu, address, value);
        return;
    }
    set_fc(cpu, false);
    if (cpu->wait_bus) cpu->wait_bus(cpu, address, SIZE_BYTE);
    if (address < cpu->memory_size) {
        cpu->memory[address] = value;
    } else if (!cpu->in_bus_error) {
        capture_access_fault(cpu, raw_address, true, false);
        cpu->in_bus_error = true;
        m68k_exception(cpu, 2);
        cpu->in_bus_error = false;
        abort_faulted_instruction(cpu);
    }
}

void m68k_write_16(M68kCpu* cpu, u32 address, u16 value) {
    u32 raw_address = address;
    address = mask_address_24(address);
    if (cpu->write16_cb) {
        cpu->write16_cb(cpu, address, value);
        return;
    }
    set_fc(cpu, false);
    if (cpu->wait_bus) cpu->wait_bus(cpu, address, SIZE_WORD);
    if ((address & 1) && !cpu->in_address_error) {
        capture_access_fault(cpu, raw_address, true, false);
        cpu->in_address_error = true;
        m68k_exception(cpu, 3);
        cpu->in_address_error = false;
        abort_faulted_instruction(cpu);
        return;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 1)) {
        cpu->memory[address] = (value >> 8) & 0xFF;
        cpu->memory[address + 1] = value & 0xFF;
    } else if (!cpu->in_bus_error) {
        capture_access_fault(cpu, raw_address, true, false);
        cpu->in_bus_error = true;
        m68k_exception(cpu, 2);
        cpu->in_bus_error = false;
        abort_faulted_instruction(cpu);
    }
}

void m68k_write_32(M68kCpu* cpu, u32 address, u32 value) {
    u32 raw_address = address;
    address = mask_address_24(address);
    if (cpu->write32_cb) {
        cpu->write32_cb(cpu, address, value);
        return;
    }
    set_fc(cpu, false);
    if (cpu->wait_bus) cpu->wait_bus(cpu, address, SIZE_LONG);
    if ((address & 1) && !cpu->in_address_error) {
        capture_access_fault(cpu, raw_address, true, false);
        cpu->in_address_error = true;
        m68k_exception(cpu, 3);
        cpu->in_address_error = false;
        abort_faulted_instruction(cpu);
        return;
    }
    if (is_valid_address(cpu, address) && is_valid_address(cpu, address + 3)) {
        cpu->memory[address] = (value >> 24) & 0xFF;
        cpu->memory[address + 1] = (value >> 16) & 0xFF;
        cpu->memory[address + 2] = (value >> 8) & 0xFF;
        cpu->memory[address + 3] = value & 0xFF;
    } else if (!cpu->in_bus_error) {
        capture_access_fault(cpu, raw_address, true, false);
        cpu->in_bus_error = true;
        m68k_exception(cpu, 2);
        cpu->in_bus_error = false;
        abort_faulted_instruction(cpu);
    }
}

u16 m68k_fetch(M68kCpu* cpu) {
    set_fc(cpu, true);
    u32 raw_fetch_addr = cpu->pc;
    u32 fetch_addr = mask_address_24(raw_fetch_addr);
    if (cpu->wait_bus) cpu->wait_bus(cpu, fetch_addr, SIZE_WORD);
    u16 opcode = 0;

    if ((fetch_addr & 1) && !cpu->in_address_error) {
        capture_access_fault(cpu, raw_fetch_addr, false, true);
        cpu->in_address_error = true;
        m68k_exception(cpu, 3);
        cpu->in_address_error = false;
        abort_faulted_instruction(cpu);
        return 0;
    }
    if (cpu->read16_cb) {
        opcode = cpu->read16_cb(cpu, raw_fetch_addr);
    } else {
        if (is_valid_address(cpu, fetch_addr) && is_valid_address(cpu, fetch_addr + 1)) {
            opcode = (u16)((cpu->memory[fetch_addr] << 8) | cpu->memory[fetch_addr + 1]);
        } else if (!cpu->in_bus_error) {
            capture_access_fault(cpu, raw_fetch_addr, false, true);
            cpu->in_bus_error = true;
            m68k_exception(cpu, 2);
            cpu->in_bus_error = false;
            abort_faulted_instruction(cpu);
            return 0;
        }
    }

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

M68kEA m68k_calc_ea_ex(M68kCpu* cpu, int mode, int reg, M68kSize size, bool fetch_value) {
    M68kEA ea = {0};

    cpu->cycles_remaining -= m68k_ea_cycles(mode, reg, size);

    switch (mode) {
        case 0:
            ea.is_reg = true;
            ea.reg_num = reg;
            ea.is_addr = false;
            ea.value = cpu->d_regs[reg].l;
            break;
        case 1:
            ea.is_reg = true;
            ea.reg_num = reg;
            ea.is_addr = true;
            ea.value = cpu->a_regs[reg].l;
            break;
        case 2:
            ea.address = cpu->a_regs[reg].l;
            if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
            break;
        case 3:
            ea.address = cpu->a_regs[reg].l;
            if (fetch_value) {
                int prev_exception = cpu->exception_thrown;
                ea.value = m68k_read_size(cpu, ea.address, size);
                // On a faulted read, postincrement side effects should not be committed.
                if (cpu->exception_thrown == prev_exception) {
                    cpu->a_regs[reg].l += (size == SIZE_BYTE && reg == 7) ? 2 : size;
                }
            } else {
                cpu->a_regs[reg].l += (size == SIZE_BYTE && reg == 7) ? 2 : size;
            }
            break;
        case 4:

            cpu->a_regs[reg].l -= (size == SIZE_BYTE && reg == 7) ? 2 : size;
            ea.address = cpu->a_regs[reg].l;
            if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
            break;
        case 5: {
            s16 disp = (s16)fetch_extension(cpu);
            ea.address = cpu->a_regs[reg].l + disp;
            if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
        } break;
        case 7:
            switch (reg) {
                case 0:
                    ea.address = (s32)(s16)fetch_extension(cpu);
                    if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
                    break;
                case 1: {
                    u32 high = fetch_extension(cpu);
                    u32 low = fetch_extension(cpu);
                    ea.address = (high << 16) | low;
                    if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
                } break;
                case 4:
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
                case 2: {
                    s16 disp = (s16)fetch_extension(cpu);
                    ea.address = (cpu->pc - 2) + disp;
                    if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
                } break;
                case 3: {
                    u16 ext = fetch_extension(cpu);
                    u32 pc_base = (cpu->pc - 2);
                    s8 disp8 = (s8)(ext & 0xFF);
                    int xn_reg_num = (ext >> 12) & 0x7;
                    int xn_is_addr = (ext >> 15) & 0x1;
                    int xn_is_long = (ext >> 11) & 0x1;

                    u32 xn_val = xn_is_addr ? cpu->a_regs[xn_reg_num].l : cpu->d_regs[xn_reg_num].l;
                    if (!xn_is_long) {
                        xn_val = (s32)(s16)(xn_val & 0xFFFF);
                    }

                    ea.address = pc_base + xn_val + disp8;
                    if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
                } break;
                default:
                    break;
            }
            break;
        case 6: {
            u16 ext = fetch_extension(cpu);
            s8 disp8 = (s8)(ext & 0xFF);
            int xn_reg_num = (ext >> 12) & 0x7;
            int xn_is_addr = (ext >> 15) & 0x1;
            int xn_is_long = (ext >> 11) & 0x1;

            u32 xn_val = xn_is_addr ? cpu->a_regs[xn_reg_num].l : cpu->d_regs[xn_reg_num].l;
            if (!xn_is_long) {
                xn_val = (s32)(s16)(xn_val & 0xFFFF);
            }

            ea.address = cpu->a_regs[reg].l + xn_val + disp8;
            if (fetch_value) ea.value = m68k_read_size(cpu, ea.address, size);
        } break;
    }
    return ea;
}

M68kEA m68k_calc_ea(M68kCpu* cpu, int mode, int reg, M68kSize size) {
    return m68k_calc_ea_ex(cpu, mode, reg, size, true);
}

M68kEA m68k_calc_ea_addr(M68kCpu* cpu, int mode, int reg, M68kSize size) {
    return m68k_calc_ea_ex(cpu, mode, reg, size, false);
}

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

void update_flags_cmp(M68kCpu* cpu, u32 src, u32 dest, u32 result, M68kSize size) {
    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);

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
    }
}

void update_flags_logic(M68kCpu* cpu, u32 result, M68kSize size) {
    cpu->sr &= ~(M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C);

    u32 msb_mask = (size == SIZE_BYTE) ? 0x80 : (size == SIZE_WORD) ? 0x8000 : 0x80000000;
    u32 value_mask = (size == SIZE_BYTE) ? 0xFF : (size == SIZE_WORD) ? 0xFFFF : 0xFFFFFFFF;

    if (result & msb_mask) cpu->sr |= M68K_SR_N;
    if ((result & value_mask) == 0) cpu->sr |= M68K_SR_Z;
}

void m68k_push_32(M68kCpu* cpu, u32 value) {
    cpu->a_regs[7].l -= 4;
    m68k_write_32(cpu, cpu->a_regs[7].l, value);
}

u32 m68k_pop_32(M68kCpu* cpu) {
    u32 value = m68k_read_32(cpu, cpu->a_regs[7].l);
    cpu->a_regs[7].l += 4;
    return value;
}

void m68k_push_16(M68kCpu* cpu, u16 value) {
    cpu->a_regs[7].l -= 2;
    m68k_write_16(cpu, cpu->a_regs[7].l, value);
}

u16 m68k_pop_16(M68kCpu* cpu) {
    u16 value = m68k_read_16(cpu, cpu->a_regs[7].l);
    cpu->a_regs[7].l += 2;
    return value;
}

void m68k_set_sr(M68kCpu* cpu, u16 new_sr) {
    new_sr &= 0xA71F;
    bool old_s = (cpu->sr & M68K_SR_S) != 0;
    bool new_s = (new_sr & M68K_SR_S) != 0;

    if (old_s != new_s) {
        if (new_s) {
            cpu->usp = cpu->a_regs[7].l;
            cpu->a_regs[7].l = cpu->ssp;
        } else {
            cpu->ssp = cpu->a_regs[7].l;
            cpu->a_regs[7].l = cpu->usp;
        }
    }
    cpu->sr = new_sr;
}

void m68k_exception(M68kCpu* cpu, int vector) {
    if (cpu->exception_thrown == 0) {
        cpu->exception_thrown = vector;
    }
    u16 old_sr = cpu->sr;
    u32 old_pc = cpu->pc;
    if (vector == 4 || vector == 8 || vector == 10 || vector == 11) {
        old_pc = cpu->ppc;
    }

    m68k_set_sr(cpu, (cpu->sr | M68K_SR_S) & ~0x8000);

    if (vector == 2 || vector == 3) {
        // 68000 bus/address errors use a 14-byte stack frame.
        const u16 ir = cpu->fault_valid ? cpu->fault_ir : cpu->ir;
        const u32 fault_address = cpu->fault_valid ? cpu->fault_address : 0;
        const u16 ssw = cpu->fault_valid ? cpu->fault_ssw : 0;

        // Mirror Musashi 68000 frame write sequence.
        m68k_push_32(cpu, old_pc);
        m68k_push_16(cpu, old_sr);
        m68k_push_16(cpu, ir);
        m68k_push_32(cpu, fault_address);
        m68k_push_16(cpu, ssw);
    } else {
        m68k_push_32(cpu, old_pc);
        m68k_push_16(cpu, old_sr);
    }
    cpu->fault_program_access = false;
    cpu->fault_valid = false;

    u32 vector_addr = m68k_read_32(cpu, vector * 4);

    m68k_set_pc(cpu, vector_addr);
}

void m68k_set_irq(M68kCpu* cpu, int level) { cpu->irq_level = level; }

static bool check_interrupts(M68kCpu* cpu) {
    if (cpu->irq_level == 0) return false;

    int current_level = (cpu->sr >> 8) & 0x7;

    if (cpu->irq_level > current_level || cpu->irq_level == 7) {
        int vector;

        if (cpu->int_ack) {
            int ack = cpu->int_ack(cpu, cpu->irq_level);
            if (ack == (int)M68K_INT_ACK_AUTOVECTOR) {
                vector = 24 + cpu->irq_level;
            } else if (ack == (int)M68K_INT_ACK_SPURIOUS) {
                vector = 24;
            } else {
                vector = ack & 0xFF;
            }
        } else {
            vector = 24 + cpu->irq_level;
        }

        m68k_exception(cpu, vector);
        cpu->sr &= ~0x0700;
        cpu->sr |= (cpu->irq_level << 8);
        cpu->irq_level = 0;
        cpu->stopped = false;

        return true;
    }
    return false;
}

void m68k_step_ex(M68kCpu* cpu, bool check_exceptions) {
    // These are re-entrancy guards for access helpers, not architectural state.
    cpu->in_address_error = false;
    cpu->in_bus_error = false;
    cpu->fault_program_access = false;
    cpu->fault_valid = false;
    cpu->fault_trap_active = false;

    if (cpu->stopped) {
        if (check_exceptions && check_interrupts(cpu)) {
            cpu->cycles_remaining -= 44;
            return;
        }
        cpu->cycles_remaining -= 4;
        return;
    }

    if (check_exceptions && check_interrupts(cpu)) {
        cpu->cycles_remaining -= 44;
        return;
    }

    bool trace_active = (cpu->sr & 0x8000) != 0;

    if (cpu->instr_hook_cb) {
        cpu->instr_hook_cb(cpu, cpu->pc);
    }

    if (setjmp(cpu->fault_trap) != 0) {
        cpu->fault_trap_active = false;
        cpu->cycles_remaining -= 34;
        return;
    }
    cpu->fault_trap_active = true;

    cpu->ppc = cpu->pc;
    int prev_exception = cpu->exception_thrown;
    u16 opcode = m68k_fetch(cpu);
    if (cpu->exception_thrown != prev_exception) {
        cpu->fault_trap_active = false;
        cpu->cycles_remaining -= 34;
        return;
    }
    cpu->ir = opcode;
    int cycles = 4;

    if ((opcode & 0xF000) == 0x0000) {
        int top4 = (opcode >> 8) & 0xF;

        if (top4 == 0x0E) {
            m68k_exec_moves(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if (top4 == 0x00) {
            m68k_exec_ori(cpu, opcode);
            cycles = 8;
            goto done;
        }

        if (top4 == 0x02) {
            m68k_exec_andi(cpu, opcode);
            cycles = 8;
            goto done;
        }

        if (top4 == 0x04) {
            m68k_exec_subi(cpu, opcode);
            cycles = 8;
            goto done;
        }

        if (top4 == 0x06) {
            m68k_exec_addi(cpu, opcode);
            cycles = 8;
            goto done;
        }

        if (top4 == 0x0A) {
            m68k_exec_eori(cpu, opcode);
            cycles = 8;
            goto done;
        }

        if (top4 == 0x0C) {
            m68k_exec_cmpi(cpu, opcode);
            cycles = 8;
            goto done;
        }

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

        if (opcode & 0x0100) {
            int mode = (opcode >> 3) & 0x7;

            if (mode == 1) {
                m68k_exec_movep(cpu, opcode);
                cycles = 16;
                goto done;
            }

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

    if ((opcode & 0xC000) == 0x0000) {
        int size_bits = (opcode >> 12) & 0x3;
        if (size_bits != 0) {
            m68k_exec_move(cpu, opcode);
            cycles = 4;
            goto done;
        }
    }

    if ((opcode & 0xF000) == 0x4000) {
        if (opcode == 0x4E70) {
            m68k_exec_reset(cpu, opcode);
            cycles = 132;
            goto done;
        }
        if (opcode == 0x4E71) {
            cycles = 4;
            goto done;
        }
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

        if (opcode == 0x4E74) {
            m68k_exec_rtd(cpu, opcode);
            cycles = 16;
            goto done;
        }
        if (opcode == 0x4E7A || opcode == 0x4E7B) {
            m68k_exec_movec(cpu, opcode);
            cycles = 12;
            goto done;
        }

        if ((opcode & 0xFFF8) == 0x4848) {
            m68k_exec_bkpt(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFFF0) == 0x4E40) {
            m68k_exec_trap(cpu, opcode);
            cycles = 34;
            goto done;
        }

        if ((opcode & 0xFFF0) == 0x4E60) {
            m68k_exec_move_usp(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x4E80) {
            m68k_exec_jmp(cpu, opcode);
            cycles = 16;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x4EC0) {
            m68k_exec_jmp(cpu, opcode);
            cycles = 8;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x40C0) {
            m68k_exec_move_sr(cpu, opcode);
            cycles = 6;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x44C0) {
            m68k_exec_move_ccr(cpu, opcode);
            cycles = 12;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x46C0) {
            m68k_exec_move_sr(cpu, opcode);
            cycles = 12;
            goto done;
        }

        if ((opcode & 0xF1C0) == 0x4180) {
            m68k_exec_chk(cpu, opcode);
            cycles = 10;
            goto done;
        }

        if ((opcode & 0xF1C0) == 0x41C0) {
            m68k_exec_lea(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFFF8) == 0x49C0) {
            m68k_exec_extb(cpu, opcode);
            cycles = 4;
            goto done;
        }

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

        if ((opcode & 0xFFF8) == 0x4E50) {
            m68k_exec_link(cpu, opcode);
            cycles = 16;
            goto done;
        }

        if ((opcode & 0xFFF8) == 0x4E58) {
            m68k_exec_unlk(cpu, opcode);
            cycles = 12;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x4AC0) {
            m68k_exec_tas(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFFC0) == 0x4800) {
            m68k_exec_nbcd(cpu, opcode);
            cycles = 8;
            goto done;
        }

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

        if ((opcode & 0xFF00) == 0x4A00) {
            m68k_exec_tst(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFF00) == 0x4000) {
            m68k_exec_negx(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFF00) == 0x4200) {
            m68k_exec_clr(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFF00) == 0x4400) {
            m68k_exec_neg(cpu, opcode);
            cycles = 4;
            goto done;
        }

        if ((opcode & 0xFF00) == 0x4600) {
            m68k_exec_not(cpu, opcode);
            cycles = 4;
            goto done;
        }
    }

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

    if ((opcode & 0xF000) == 0x6000) {
        m68k_exec_bcc(cpu, opcode);
        cycles = 10;
        goto done;
    }

    if ((opcode & 0xF100) == 0x7000) {
        m68k_exec_moveq(cpu, opcode);
        cycles = 4;
        goto done;
    }

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

    if ((opcode & 0xF000) == 0xE000) {
        m68k_exec_shift(cpu, opcode);
        cycles = 6;
        goto done;
    }

    if ((opcode & 0xF000) == 0xA000) {
        m68k_exception(cpu, 10);
    } else if ((opcode & 0xF000) == 0xF000) {
        m68k_exception(cpu, 11);
    } else {
        m68k_exception(cpu, 4);
    }
    cycles = 34;

done:
    cpu->fault_trap_active = false;

    if ((cpu->pc & 1) && !cpu->in_address_error) {
        cpu->in_address_error = true;
        m68k_exception(cpu, 3);
        cpu->in_address_error = false;
    }

    if (check_exceptions && trace_active && !cpu->stopped) {
        m68k_exception(cpu, 9);
    }
    cpu->cycles_remaining -= cycles;
}

void m68k_step(M68kCpu* cpu) { m68k_step_ex(cpu, true); }

int m68k_execute(M68kCpu* cpu, int cycles_requested) {
    cpu->cycles_remaining += cycles_requested;
    cpu->target_cycles += cycles_requested;

    int start_cycles_run = m68k_cycles_run(cpu);

    while (cpu->cycles_remaining > 0) {
        m68k_step(cpu);
    }

    return m68k_cycles_run(cpu) - start_cycles_run;
}

int m68k_cycles_run(M68kCpu* cpu) { return cpu->target_cycles - cpu->cycles_remaining; }

int m68k_cycles_remaining(M68kCpu* cpu) { return cpu->cycles_remaining; }

void m68k_modify_timeslice(M68kCpu* cpu, int cycles) {
    cpu->cycles_remaining += cycles;
    cpu->target_cycles += cycles;
}

void m68k_end_timeslice(M68kCpu* cpu) {
    cpu->target_cycles = cpu->target_cycles - cpu->cycles_remaining;
    cpu->cycles_remaining = 0;
}

unsigned int m68k_context_size(void) { return sizeof(M68kCpu); }

void m68k_get_context(M68kCpu* cpu, void* dst) {
    if (dst) {
        memcpy(dst, cpu, sizeof(M68kCpu));
    }
}

void m68k_set_context(M68kCpu* cpu, const void* src) {
    if (src) {
        u8* old_memory = cpu->memory;
        u32 old_memory_size = cpu->memory_size;
        jmp_buf old_fault_trap;
        memcpy(old_fault_trap, cpu->fault_trap, sizeof(jmp_buf));
        bool old_fault_trap_active = cpu->fault_trap_active;
        M68kWaitBusCallback old_wait_bus = cpu->wait_bus;
        M68kIntAckCallback old_int_ack = cpu->int_ack;
        M68kFcCallback old_fc_cb = cpu->fc_cb;
        M68kInstrHookCallback old_instr_hook_cb = cpu->instr_hook_cb;
        M68kPcChangedCallback old_pc_changed_cb = cpu->pc_changed_cb;
        M68kResetCallback old_reset_cb = cpu->reset_cb;
        M68kTasCallback old_tas_cb = cpu->tas_cb;
        M68kIllgCallback old_illg_cb = cpu->illg_cb;

        memcpy(cpu, src, sizeof(M68kCpu));

        cpu->memory = old_memory;
        cpu->memory_size = old_memory_size;
        memcpy(cpu->fault_trap, old_fault_trap, sizeof(jmp_buf));
        cpu->fault_trap_active = old_fault_trap_active;
        cpu->wait_bus = old_wait_bus;
        cpu->int_ack = old_int_ack;
        cpu->fc_cb = old_fc_cb;
        cpu->instr_hook_cb = old_instr_hook_cb;
        cpu->pc_changed_cb = old_pc_changed_cb;
        cpu->reset_cb = old_reset_cb;
        cpu->tas_cb = old_tas_cb;
        cpu->illg_cb = old_illg_cb;
    }
}
