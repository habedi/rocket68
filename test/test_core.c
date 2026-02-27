#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

u16 m68k_fetch(M68kCpu* cpu);

void test_initialization() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));

    m68k_init(&cpu, memory, sizeof(memory));
    m68k_reset(&cpu);

    // After reset with zero vectors, PC and SP should be 0
    assert(cpu.pc == 0);
    assert(cpu.sr == 0x2700);
    for (int i = 0; i < 8; i++) {
        assert(cpu.d_regs[i].l == 0);
        assert(cpu.a_regs[i].l == 0);
    }

    printf("Initialization test passed!\n");
}

void test_register_access() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_dr(&cpu, 0, 0xDEADBEEF);
    assert(m68k_get_dr(&cpu, 0) == 0xDEADBEEF);

    m68k_set_ar(&cpu, 7, 0x1000);
    assert(m68k_get_ar(&cpu, 7) == 0x1000);

    printf("Register access test passed!\n");
}

void test_memory_access() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Test 8-bit
    m68k_write_8(&cpu, 10, 0xAB);
    assert(m68k_read_8(&cpu, 10) == 0xAB);

    // Test 16-bit (Big Endian)
    m68k_write_16(&cpu, 20, 0x1234);
    assert(m68k_read_16(&cpu, 20) == 0x1234);
    assert(memory[20] == 0x12);
    assert(memory[21] == 0x34);

    // Test 32-bit (Big Endian)
    m68k_write_32(&cpu, 30, 0xDEADBEEF);
    assert(m68k_read_32(&cpu, 30) == 0xDEADBEEF);
    assert(memory[30] == 0xDE);
    assert(memory[31] == 0xAD);
    assert(memory[32] == 0xBE);
    assert(memory[33] == 0xEF);

    printf("Memory access test passed!\n");
}

void test_fetch() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Write NOPs
    m68k_write_16(&cpu, 0, 0x4E71);
    m68k_write_16(&cpu, 2, 0x4E71);

    m68k_step(&cpu);
    assert(cpu.pc == 2);

    m68k_step(&cpu);
    assert(cpu.pc == 4);

    printf("Fetch test passed!\n");
}

void test_usp_switching() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.sr = 0x2700;           // Supervisor mode
    cpu.a_regs[7].l = 0x1000;  // SSP

    // MOVE A0, USP: 0100 1110 0110 0 000 = 0x4E60
    cpu.a_regs[0].l = 0x800;
    m68k_write_16(&cpu, 0, 0x4E60);
    m68k_step(&cpu);
    assert(cpu.usp == 0x800);

    // MOVE USP, A1: 0100 1110 0110 1 001 = 0x4E69
    m68k_write_16(&cpu, 2, 0x4E69);
    m68k_step(&cpu);
    assert(cpu.a_regs[1].l == 0x800);

    printf("USP switching test passed!\n");
}

void test_trace_mode() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7].l = 0x1000;
    cpu.sr = 0xA700;  // Supervisor + Trace bit (bit 15)

    // Set trace vector (vector 9) to 0x500
    m68k_write_32(&cpu, 9 * 4, 0x500);

    // Execute NOP at 0x0 — after NOP, trace should fire
    m68k_write_16(&cpu, 0, 0x4E71);  // NOP
    m68k_step(&cpu);
    assert(cpu.pc == 0x500);  // Should have jumped to trace handler

    printf("Trace mode test passed!\n");
}

void test_stopped_state() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.sr = 0x2700;  // Supervisor mode
    cpu.a_regs[7].l = 0x1000;

    // STOP #$2700
    m68k_write_16(&cpu, 0, 0x4E72);
    m68k_write_16(&cpu, 2, 0x2700);
    m68k_step(&cpu);
    assert(cpu.stopped == true);

    // Stepping again while stopped should return idle cycles
    u32 saved_pc = cpu.pc;
    m68k_step(&cpu);
    assert(cpu.pc == saved_pc);  // PC shouldn't advance
    assert(cpu.stopped == true);

    // Setting an IRQ should resume
    m68k_set_irq(&cpu, 7);  // NMI
    // Set auto-vector for level 7 (vector 31)
    m68k_write_32(&cpu, 31 * 4, 0x600);
    m68k_step(&cpu);
    assert(cpu.stopped == false);

    printf("Stopped state test passed!\n");
}

void test_address_error() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7].l = 0x1000;
    cpu.sr = 0x2700;

    // Set address error vector (vector 3) to 0x400
    m68k_write_32(&cpu, 3 * 4, 0x400);

    // Attempt to read from odd address should trigger exception
    m68k_read_16(&cpu, 0x101);
    assert(cpu.pc == 0x400);

    printf("Address error test passed!\n");
}

void test_interrupts() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. Setup
    cpu.pc = 0x100;
    cpu.a_regs[7].l = 0x400;  // SP
    cpu.ssp = 0x400;          // SSP must match since IRQ switches to supervisor mode
    cpu.sr = 0x0000;          // User mode, IRQ mask 0 (Enable all)

    // 2. Setup Auto-Vector for Level 4
    // Vector 24 + 4 = 28. Address 28*4 = 112 (0x70).
    // Handler at 0x500.
    m68k_write_32(&cpu, 28 * 4, 0x500);

    // 3. Trigger IRQ Level 4
    m68k_set_irq(&cpu, 4);

    // 4. Step (Should Ack, Save State, Jump)
    // We need to execute at least one instruction cycle for the check to happen.
    // Let's write a NOP at 0x100.
    m68k_write_16(&cpu, 0x100, 0x4E71);

    m68k_step(&cpu);

    // Check Jump
    assert(cpu.pc == 0x500);

    // Check Supervisor Mode Set
    assert((cpu.sr & M68K_SR_S) != 0);

    // Check Mask Updated to 4
    assert(((cpu.sr >> 8) & 0x7) == 4);

    // Check Stack (Format: PC (4), SR (2))
    // SP was 0x400. Pushed 6 bytes -> 0x3FA.
    assert(cpu.a_regs[7].l == 0x3FA);

    // Verify saved PC (0x100 - before NOP execution since IRQ is pre-fetch)
    u32 saved_pc = m68k_read_32(&cpu, cpu.a_regs[7].l + 2);
    assert(saved_pc == 0x100);

    printf("Interrupt test passed!\n");
}

static int mock_iack_vector = 0;
static int mock_int_ack_cb(int level) {
    (void)level;
    return mock_iack_vector;
}

void test_int_ack() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_int_ack_callback(mock_int_ack_cb);

    // 1. Setup
    cpu.pc = 0x100;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.sr = 0x0000;

    // 2. Test Spurious Interrupt (M68K_INT_ACK_SPURIOUS -> vector 24)
    mock_iack_vector = M68K_INT_ACK_SPURIOUS;
    m68k_write_32(&cpu, 24 * 4, 0x500);  // Spurious handler
    m68k_set_irq(&cpu, 4);
    m68k_write_16(&cpu, cpu.pc, 0x4E71);
    m68k_step_ex(&cpu, true);
    assert(cpu.exception_thrown == 24);
    assert(cpu.pc == 0x500);

    // Reset
    m68k_reset(&cpu);
    cpu.pc = 0x200;
    cpu.ssp = 0x1000;
    cpu.a_regs[7].l = 0x1000;
    cpu.sr = 0x0000;

    // 3. Test Custom Vector (e.g. 0x40 -> vector 64)
    mock_iack_vector = 0x40;
    m68k_write_32(&cpu, 0x40 * 4, 0x600);  // Custom handler
    m68k_set_irq(&cpu, 5);
    m68k_write_16(&cpu, cpu.pc, 0x4E71);
    m68k_step_ex(&cpu, true);
    assert(cpu.exception_thrown == 0x40);
    assert(cpu.pc == 0x600);

    // Reset
    m68k_reset(&cpu);
    cpu.pc = 0x100;
    cpu.ssp = 0x1000;
    cpu.a_regs[7].l = 0x1000;
    cpu.sr = 0x0000;

    // 4. Test Autovector (M68K_INT_ACK_AUTOVECTOR) via IRQ 6 -> vector 30
    mock_iack_vector = M68K_INT_ACK_AUTOVECTOR;
    m68k_write_32(&cpu, 30 * 4, 0x700);  // Autovector handler
    m68k_set_irq(&cpu, 6);
    m68k_write_16(&cpu, cpu.pc, 0x4E71);
    m68k_step_ex(&cpu, true);
    assert(cpu.exception_thrown == 30);
    assert(cpu.pc == 0x700);

    m68k_set_int_ack_callback(NULL);
    printf("IACK Callback test passed!\n");
}

static unsigned int mock_fc = 0;
static void mock_fc_cb(unsigned int fc) { mock_fc = fc; }

void test_fc() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_fc_callback(mock_fc_cb);

    // 1. User Data Access
    cpu.sr = 0x0000;  // User mode
    mock_fc = 0;
    m68k_write_8(&cpu, 10, 0);  // Implicit data write
    assert(mock_fc == M68K_FC_USER_DATA);

    mock_fc = 0;
    m68k_read_16(&cpu, 10);
    assert(mock_fc == M68K_FC_USER_DATA);

    // 2. Supervisor Data Access
    cpu.sr = 0x2000;  // Supervisor mode (bit 13)
    mock_fc = 0;
    m68k_write_32(&cpu, 10, 0);
    assert(mock_fc == M68K_FC_SUPV_DATA);

    // 3. User Program Access
    // Emulated via a fetch
    cpu.sr = 0x0000;
    cpu.pc = 100;
    mock_fc = 0;
    m68k_fetch(&cpu);
    assert(mock_fc == M68K_FC_USER_PROG);

    // 4. Supervisor Program Access
    cpu.sr = 0x2000;
    cpu.pc = 100;
    mock_fc = 0;
    m68k_fetch(&cpu);
    assert(mock_fc == M68K_FC_SUPV_PROG);

    m68k_set_fc_callback(NULL);
    printf("Function Code (FC) Callback test passed!\n");
}

static u32 mock_instr_pc = 0;
static u32 mock_pc_changed = 0;
static bool mock_reset_called = false;
static int mock_tas_result = 1;

static void handle_instr_hook(u32 pc) { mock_instr_pc = pc; }
static void handle_pc_changed(u32 pc) { mock_pc_changed = pc; }
static void handle_reset(void) { mock_reset_called = true; }
static int handle_tas(void) { return mock_tas_result; }

void test_hooks() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_instr_hook_callback(handle_instr_hook);
    m68k_set_pc_changed_callback(handle_pc_changed);
    m68k_set_reset_callback(handle_reset);
    m68k_set_tas_callback(handle_tas);

    // 1. Instruction Hook & PC Changed
    cpu.pc = 0x100;
    // JMP 0x200: 0100 1110 11 111 000 = 0x4EF8
    // Word 2: 0x0200
    m68k_write_16(&cpu, 0x100, 0x4EF8);
    m68k_write_16(&cpu, 0x102, 0x0200);
    m68k_step_ex(&cpu, true);

    // Instruction hook should have fired BEFORE the JMP execution at PC 0x100
    assert(mock_instr_pc == 0x100);
    // PC should have changed to 0x200
    assert(mock_pc_changed == 0x200);

    // 2. RESET instruction hook
    cpu.pc = 0x200;
    cpu.sr = 0x2700;  // Supervisor mode required for RESET
    mock_reset_called = false;
    // RESET: 0100 1110 0111 0000 = 0x4E70
    m68k_write_16(&cpu, 0x200, 0x4E70);
    m68k_step_ex(&cpu, true);

    assert(mock_reset_called == true);

    // 3. TAS instruction hook
    // TAS D0: 0100 1010 11 000 000 = 0x4AC0
    cpu.pc = 0x300;
    cpu.d_regs[0].l = 0;
    m68k_write_16(&cpu, 0x300, 0x4AC0);

    // First try with write allowed
    mock_tas_result = 1;
    m68k_step_ex(&cpu, true);
    assert((cpu.d_regs[0].l & 0x80) != 0);  // Writeback succeeded

    // Try with write denied
    cpu.pc = 0x302;
    cpu.d_regs[0].l = 0;
    m68k_write_16(&cpu, 0x302, 0x4AC0);
    mock_tas_result = 0;
    m68k_step_ex(&cpu, true);
    assert((cpu.d_regs[0].l & 0x80) == 0);  // Writeback denied

    m68k_set_instr_hook_callback(NULL);
    m68k_set_pc_changed_callback(NULL);
    m68k_set_reset_callback(NULL);
    m68k_set_tas_callback(NULL);

    printf("Execution Hooks test passed!\n");
}

static M68kCpu* timeslice_test_cpu = NULL;
static void timeslice_hook(u32 pc) {
    (void)pc;
    m68k_end_timeslice(timeslice_test_cpu);
}

void test_timeslice() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // NOP is 4 cycles
    m68k_write_16(&cpu, 0x100, 0x4E71);
    m68k_write_16(&cpu, 0x102, 0x4E71);
    cpu.pc = 0x100;

    int cycles_used = m68k_execute(&cpu, 8);
    assert(cycles_used >= 8);
    assert(cpu.pc == 0x104);

    // Test early yield
    cpu.pc = 0x100;
    timeslice_test_cpu = &cpu;
    m68k_set_instr_hook_callback(timeslice_hook);
    cycles_used = m68k_execute(&cpu, 8);
    assert(cycles_used == 4);  // one instruction ran, then timeslice ended
    assert(cpu.pc == 0x102);
    m68k_set_instr_hook_callback(NULL);

    printf("Advanced Timeslice logic test passed!\n");
}

void test_serialization() {
    M68kCpu cpu1;
    M68kCpu cpu2;
    u8 memory1[1024];
    u8 memory2[1024];

    memset(memory1, 0, sizeof(memory1));
    memset(memory2, 0, sizeof(memory2));

    m68k_init(&cpu1, memory1, sizeof(memory1));
    m68k_init(&cpu2, memory2, sizeof(memory2));

    cpu1.d_regs[0].l = 0xDEADBEEF;
    cpu1.a_regs[0].l = 0xCAFEBABE;
    cpu1.pc = 0x100;
    cpu1.sr = 0x2700;

    unsigned int ctx_size = m68k_context_size();
    u8* ctx_buffer = malloc(ctx_size);

    m68k_get_context(&cpu1, ctx_buffer);
    m68k_set_context(&cpu2, ctx_buffer);
    free(ctx_buffer);

    assert(cpu2.d_regs[0].l == 0xDEADBEEF);
    assert(cpu2.a_regs[0].l == 0xCAFEBABE);
    assert(cpu2.pc == 0x100);
    assert(cpu2.sr == 0x2700);

    // CPU2 should still point to memory2!
    assert(cpu2.memory == memory2);

    printf("CPU Context Serialization test passed!\n");
}
