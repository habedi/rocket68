#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

// =============================================================================
// Regression Tests for Bug Fixes
// =============================================================================

void test_regression_postinc_predec_byte() {
    // Bug 1: Postinc/predec double-counted byte ops for non-A7 registers
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // MOVE.B (A0)+, D0 — should increment A0 by 1, not 2
    // Opcode: 0001 000 000 011 000 -> 0x1018
    cpu.a_regs[0] = 0x100;
    memory[0x100] = 0x42;
    m68k_write_16(&cpu, 0, 0x1018);
    m68k_step(&cpu);

    assert((cpu.d_regs[0] & 0xFF) == 0x42);
    assert(cpu.a_regs[0] == 0x101);  // Was 0x102 before fix

    // MOVE.B -(A1), D1 — should decrement A1 by 1, not 2
    // Opcode: 0001 001 000 100 001 -> 0x1221
    cpu.a_regs[1] = 0x110;
    memory[0x10F] = 0x55;
    m68k_write_16(&cpu, 2, 0x1221);
    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0x55);
    assert(cpu.a_regs[1] == 0x10F);  // Was 0x10E before fix

    // A7 should still increment by 2 for byte (stack alignment)
    // MOVE.B (A7)+, D2 -> 0x141F
    cpu.a_regs[7] = 0x200;
    memory[0x200] = 0xAA;
    m68k_write_16(&cpu, 4, 0x141F);
    m68k_step(&cpu);

    assert((cpu.d_regs[2] & 0xFF) == 0xAA);
    assert(cpu.a_regs[7] == 0x202);  // A7 byte = +2

    printf("Regression: Postinc/predec byte test passed!\n");
}

void test_regression_reset_vectors() {
    // Bug 2: m68k_reset should load SSP from vector 0 and PC from vector 1
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // Set up vector table
    m68k_write_32(&cpu, 0x00, 0x00000400);  // SSP
    m68k_write_32(&cpu, 0x04, 0x00000100);  // PC

    m68k_reset(&cpu);

    assert(cpu.a_regs[7] == 0x400);
    assert(cpu.pc == 0x100);
    assert(cpu.sr == 0x2700);

    printf("Regression: Reset vectors test passed!\n");
}

void test_regression_jmp_dispatch() {
    // Bug 3: JMP opcodes (0x4EC0-0x4EFF) were never dispatched
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // JMP (A0) — absolute jump using address register indirect
    // Mode=2, Reg=0. Opcode: 0100 1110 11 010 000 -> 0x4ED0
    cpu.a_regs[0] = 0x200;
    m68k_write_16(&cpu, 0, 0x4ED0);
    m68k_step(&cpu);

    assert(cpu.pc == 0x200);

    // JMP $0x100 absolute short
    // Mode=7, Reg=0. Opcode: 0100 1110 11 111 000 -> 0x4EF8
    cpu.pc = 0x10;
    m68k_write_16(&cpu, 0x10, 0x4EF8);
    m68k_write_16(&cpu, 0x12, 0x0100);
    m68k_step(&cpu);

    assert(cpu.pc == 0x100);

    printf("Regression: JMP dispatch test passed!\n");
}

void test_regression_div_by_zero() {
    // Bug 4: Division by zero should trigger exception (vector 5)
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7] = 0x1000;

    // Set vector 5 to point to 0x300
    m68k_write_32(&cpu, 5 * 4, 0x300);

    // DIVU D0, D1 — D0 = 0 (divide by zero)
    // Op: 1000 001 011 000 000 -> 0x82C0
    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 100;
    m68k_write_16(&cpu, 0, 0x82C0);
    m68k_step(&cpu);

    assert(cpu.pc == 0x300);  // Should have jumped to exception handler

    printf("Regression: Div-by-zero exception test passed!\n");
}

void test_regression_addx_subx_zflag() {
    // Bug 5: ADDX/SUBX Z-flag should be "cleared if non-zero, unchanged if zero"
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // Set Z flag and X flag
    cpu.sr |= M68K_SR_Z;
    cpu.sr |= M68K_SR_X;

    // ADDX.L D0, D1: D0=0, D1=0, X=1 -> result = 1 (non-zero)
    // Z should be CLEARED because result is non-zero
    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;
    // Op: 1101 001 1 10 00 0 000 -> 0xD380
    m68k_write_16(&cpu, 0, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 1);
    assert((cpu.sr & M68K_SR_Z) == 0);  // Z cleared (result non-zero)

    // Now set Z again and test with zero result
    cpu.sr |= M68K_SR_Z;
    cpu.sr &= ~M68K_SR_X;  // X=0

    // ADDX.L D0, D1: D0=0, D1=0, X=0 -> result = 0
    // Z should be UNCHANGED (still set)
    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;
    m68k_write_16(&cpu, 2, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 0);
    assert((cpu.sr & M68K_SR_Z) != 0);  // Z preserved (result is zero)

    // Clear Z, then ADDX with zero result -> Z should remain clear
    cpu.sr &= ~M68K_SR_Z;
    cpu.sr &= ~M68K_SR_X;
    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;
    m68k_write_16(&cpu, 4, 0xD380);
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) == 0);  // Z stays clear

    printf("Regression: ADDX/SUBX Z-flag test passed!\n");
}

void test_regression_sbcd() {
    // Bug 6: SBCD borrow detection was broken for underflows
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // SBCD D0, D1: D1=0x10, D0=0x01, X=0 -> should be 0x09
    cpu.d_regs[0] = 0x01;
    cpu.d_regs[1] = 0x10;
    cpu.sr &= ~M68K_SR_X;

    // SBCD D0, D1: 1000 001 100 00 0 000 -> 0x8300
    m68k_write_16(&cpu, 0, 0x8300);
    m68k_step(&cpu);
    assert((cpu.d_regs[1] & 0xFF) == 0x09);

    // SBCD: D1=0x00, D0=0x01, X=0 -> borrow: 00 - 01 = 99 with carry
    cpu.d_regs[0] = 0x01;
    cpu.d_regs[1] = 0x00;
    cpu.sr &= ~M68K_SR_X;
    m68k_write_16(&cpu, 2, 0x8300);
    m68k_step(&cpu);
    assert((cpu.d_regs[1] & 0xFF) == 0x99);
    assert((cpu.sr & M68K_SR_C) != 0);
    assert((cpu.sr & M68K_SR_X) != 0);

    printf("Regression: SBCD borrow test passed!\n");
}

void test_regression_nbcd() {
    // Bug 7: NBCD result was incorrect
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // NBCD D0: D0 = 0x00, X=0 -> result = 0 (no borrow)
    cpu.d_regs[0] = 0x00;
    cpu.sr &= ~M68K_SR_X;

    // NBCD D0: 0100 100 000 000 000 -> 0x4800
    m68k_write_16(&cpu, 0, 0x4800);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x00);
    assert((cpu.sr & M68K_SR_C) == 0);

    // NBCD D0: D0 = 0x01, X=0 -> 100-1 = 99, carry set
    cpu.d_regs[0] = 0x01;
    cpu.sr &= ~M68K_SR_X;
    m68k_write_16(&cpu, 2, 0x4800);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x99);
    assert((cpu.sr & M68K_SR_C) != 0);

    printf("Regression: NBCD test passed!\n");
}

void test_regression_stop_privilege() {
    // Bug 8: STOP should check privilege before fetching immediate
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // Set up privilege violation vector (vector 8) to 0x300
    m68k_write_32(&cpu, 8 * 4, 0x300);

    cpu.sr = 0x0000;  // User mode
    cpu.pc = 0x100;
    cpu.a_regs[7] = 0x1000;

    // STOP #$2700 at 0x100
    m68k_write_16(&cpu, 0x100, 0x4E72);
    m68k_write_16(&cpu, 0x102, 0x2700);

    m68k_step(&cpu);

    // Should trigger privilege violation, not consume the immediate
    assert(cpu.pc == 0x300);
    // PC should NOT have advanced past the STOP immediate word

    printf("Regression: STOP privilege test passed!\n");
}

void test_regression_ext_dispatch() {
    // Bug 9: EXT.W dispatch was shadowed by MOVEM block
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // EXT.W D0: Byte -> Word sign extend
    // D0 = 0x80 (-128 as byte, should become 0xFF80)
    cpu.d_regs[0] = 0x00000080;

    // EXT.W D0: 0100 1000 1000 0000 -> 0x4880
    m68k_write_16(&cpu, 0, 0x4880);
    m68k_step(&cpu);

    assert((cpu.d_regs[0] & 0xFFFF) == 0xFF80);

    // EXT.L D1: Word -> Long sign extend
    // D1 = 0x8000 (-32768 as word, should become 0xFFFF8000)
    cpu.d_regs[1] = 0x00008000;

    // EXT.L D1: 0100 1000 1100 0001 -> 0x48C1
    m68k_write_16(&cpu, 2, 0x48C1);
    m68k_step(&cpu);

    assert(cpu.d_regs[1] == 0xFFFF8000);

    printf("Regression: EXT dispatch test passed!\n");
}
