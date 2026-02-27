#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_arithmetic() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. ADD.B D0, D1
    // D0 = 5, D1 = 10 -> Result = 15
    // ADD Opcode: 1101 <Reg D1=1> <OpMode Byte/Dn=000> <Mode Dn=0> <Reg D0=0>
    // 1101 001 000 000 000 -> 0xD200
    m68k_set_dr(&cpu, 0, 5);
    m68k_set_dr(&cpu, 1, 10);
    m68k_write_16(&cpu, 0, 0xD200);

    m68k_step(&cpu);
    assert(m68k_get_dr(&cpu, 1) == 15);
    assert((cpu.sr & M68K_SR_Z) == 0);

    // 2. SUB.W D0, D1
    // D0 = 5, D1 = 15 -> Result = 10
    // SUB Opcode: 1001 <Reg D1=1> <OpMode Word/Dn=001> <Mode Dn=0> <Reg D0=0>
    // 1001 001 001 000 000 -> 0x9240
    m68k_write_16(&cpu, 2, 0x9240);

    m68k_step(&cpu);
    assert(m68k_get_dr(&cpu, 1) == 10);

    printf("Arithmetic instruction test passed!\n");
}

void test_negx() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // NEGX.L D0: D0=1, X=0 -> 0-1-0 = -1 = 0xFFFFFFFF
    cpu.d_regs[0] = 1;
    cpu.sr &= ~M68K_SR_X;
    // NEGX.L D0: 0100 0000 10 000 000 = 0x4080
    m68k_write_16(&cpu, 0, 0x4080);
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 0xFFFFFFFF);
    assert((cpu.sr & M68K_SR_C) != 0);
    assert((cpu.sr & M68K_SR_X) != 0);

    // NEGX.L D0: D0=0, X=1 -> 0-0-1 = -1 = 0xFFFFFFFF
    cpu.d_regs[0] = 0;
    // X is already 1 from above
    m68k_write_16(&cpu, 2, 0x4080);
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 0xFFFFFFFF);

    printf("NEGX test passed!\n");
}

void test_adda_suba() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. ADDA.L D0, A0
    // A0 = 0x1000. D0 = 0x10.
    // Result: 0x1010.
    // Op: 1101 <Reg A0=0> 111 <Mode 0> <Reg D0=0> (0xD1C0)
    cpu.a_regs[0] = 0x1000;
    cpu.d_regs[0] = 0x10;
    m68k_write_16(&cpu, 0, 0xD1C0);

    // 2. SUBA.W D0, A0
    // A0 = 0x1010. D0 = 0x10.
    // Result: 0x1000.
    // OP: 1001 <Reg A0=0> 011 <Mode 0> <Reg D0=0> (0x90C0)
    m68k_write_16(&cpu, 2, 0x90C0);

    m68k_step(&cpu);  // ADDA
    assert(cpu.a_regs[0] == 0x1010);

    m68k_step(&cpu);  // SUBA
    assert(cpu.a_regs[0] == 0x1000);

    printf("ADDA/SUBA instruction test passed!\n");
}

void test_addx_subx() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. ADDX.L D0, D1
    // D0 = 1, D1 = 2. X = 1 (Set beforehand).
    // Result = 1 + 2 + 1 = 4.
    // Op: 1101 <D1=1> 1 10 00 0 <D0=0> -> 1101 001 1 10 00 0 000 -> 0xD380
    cpu.d_regs[0] = 1;
    cpu.d_regs[1] = 2;
    cpu.sr |= M68K_SR_X;

    m68k_write_16(&cpu, 0, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 4);

    // 2. SUBX.L D0, D1
    // D0 = 1, D1 = 4. X = 0 (Cleared by ADDX result?).
    // ADDX result was 4 (non-zero), so Z cleared. C/X cleared (no carry).
    // Set X=1 manually again.
    // D1 - D0 - X = 4 - 1 - 1 = 2.
    // Op: 1001 <D1=1> 1 10 00 0 <D0=0> -> 0x9380
    cpu.sr |= M68K_SR_X;
    m68k_write_16(&cpu, 2, 0x9380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 2);

    printf("ADDX/SUBX instruction test passed!\n");
}

void test_bcd() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. ABCD D0, D1
    // D0 = 0x09, D1 = 0x01. X=0.
    // Result = 0x10 (BCD).
    cpu.d_regs[0] = 0x09;
    cpu.d_regs[1] = 0x01;
    cpu.sr &= ~M68K_SR_X;

    // ABCD D0, D1: 1100 001 100 00 0 000 -> 0xC300
    m68k_write_16(&cpu, 0, 0xC300);
    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0x10);
    assert((cpu.sr & M68K_SR_X) == 0);  // No carry from 10

    // 2. ABCD D0, D1 (with Carry)
    // D0 = 0x50, D1 = 0x50. Result = 0x00, C=1, X=1.
    cpu.d_regs[0] = 0x50;
    cpu.d_regs[1] = 0x50;
    m68k_write_16(&cpu, 2, 0xC300);
    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0x00);
    assert((cpu.sr & M68K_SR_X) != 0);
    assert((cpu.sr & M68K_SR_C) != 0);

    // 3. SBCD D0, D1 (Subtract)
    // D1 = 0x15, D0 = 0x06. X=0. Result = 0x09.
    cpu.d_regs[0] = 0x06;
    cpu.d_regs[1] = 0x15;
    cpu.sr &= ~M68K_SR_X;

    // SBCD D0, D1: 1000 001 100 00 0 000 -> 0x8300
    m68k_write_16(&cpu, 4, 0x8300);
    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0x09);

    // 4. NBCD D0
    // D0 = 0x01. Result = 0x99 (100 - 1). C=1.
    // NBCD D0: 0100 100 000 000 000 -> 0x4800
    cpu.d_regs[0] = 0x01;
    cpu.sr &= ~M68K_SR_X;
    m68k_write_16(&cpu, 6, 0x4800);
    m68k_step(&cpu);

    // 100 - 1 = 99 (0x99). Carry set.
    assert((cpu.d_regs[0] & 0xFF) == 0x99);
    assert((cpu.sr & M68K_SR_C) != 0);

    printf("BCD instruction test passed!\n");
}

void test_immediate_alu() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // ADDI.L #$100, D0 — add immediate long
    // 0x0680 followed by 0x0000, 0x0100
    cpu.d_regs[0] = 0x50;
    m68k_write_16(&cpu, 0, 0x0680);  // ADDI.L D0
    m68k_write_16(&cpu, 2, 0x0000);
    m68k_write_16(&cpu, 4, 0x0100);
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 0x150);

    // SUBI.W #$10, D1
    // 0x0441 followed by 0x0010
    cpu.d_regs[1] = 0x30;
    m68k_write_16(&cpu, 6, 0x0441);
    m68k_write_16(&cpu, 8, 0x0010);
    m68k_step(&cpu);
    assert((cpu.d_regs[1] & 0xFFFF) == 0x20);

    // ANDI.B #$0F, D0
    // 0x0200 followed by 0x000F
    cpu.d_regs[0] = 0xFF;
    m68k_write_16(&cpu, 10, 0x0200);
    m68k_write_16(&cpu, 12, 0x000F);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x0F);

    // ORI.B #$80, D0
    // 0x0000 followed by 0x0080
    cpu.d_regs[0] = 0x01;
    m68k_write_16(&cpu, 14, 0x0000);
    m68k_write_16(&cpu, 16, 0x0080);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x81);

    // EORI.B #$FF, D0
    // 0x0A00 followed by 0x00FF
    cpu.d_regs[0] = 0xAA;
    m68k_write_16(&cpu, 18, 0x0A00);
    m68k_write_16(&cpu, 20, 0x00FF);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x55);

    // ANDI to CCR: #$10 (clear all flags except X)
    // 0x023C followed by 0x0010
    cpu.sr = 0x001F;  // All CCR flags set
    m68k_write_16(&cpu, 22, 0x023C);
    m68k_write_16(&cpu, 24, 0x0010);
    m68k_step(&cpu);
    assert((cpu.sr & 0xFF) == 0x10);  // Only X remains

    printf("Immediate ALU test passed!\n");
}
