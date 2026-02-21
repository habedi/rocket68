#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

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
        assert(cpu.d_regs[i] == 0);
        assert(cpu.a_regs[i] == 0);
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

    cpu.sr = 0x2700;         // Supervisor mode
    cpu.a_regs[7] = 0x1000;  // SSP

    // MOVE A0, USP: 0100 1110 0110 0 000 = 0x4E60
    cpu.a_regs[0] = 0x800;
    m68k_write_16(&cpu, 0, 0x4E60);
    m68k_step(&cpu);
    assert(cpu.usp == 0x800);

    // MOVE USP, A1: 0100 1110 0110 1 001 = 0x4E69
    m68k_write_16(&cpu, 2, 0x4E69);
    m68k_step(&cpu);
    assert(cpu.a_regs[1] == 0x800);

    printf("USP switching test passed!\n");
}

void test_trace_mode() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7] = 0x1000;
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
    cpu.a_regs[7] = 0x1000;

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

    cpu.a_regs[7] = 0x1000;
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
    cpu.a_regs[7] = 0x400;  // SP
    cpu.ssp = 0x400;        // SSP must match since IRQ switches to supervisor mode
    cpu.sr = 0x0000;        // User mode, IRQ mask 0 (Enable all)

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
    assert(cpu.a_regs[7] == 0x3FA);

    // Verify saved PC (0x100 - before NOP execution since IRQ is pre-fetch)
    u32 saved_pc = m68k_read_32(&cpu, cpu.a_regs[7] + 2);
    assert(saved_pc == 0x100);

    printf("Interrupt test passed!\n");
}
