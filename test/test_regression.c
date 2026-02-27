#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_regression_postinc_predec_byte() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[0] = 0x100;
    memory[0x100] = 0x42;
    m68k_write_16(&cpu, 0, 0x1018);
    m68k_step(&cpu);

    assert((cpu.d_regs[0] & 0xFF) == 0x42);
    assert(cpu.a_regs[0] == 0x101);

    cpu.a_regs[1] = 0x110;
    memory[0x10F] = 0x55;
    m68k_write_16(&cpu, 2, 0x1221);
    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0x55);
    assert(cpu.a_regs[1] == 0x10F);

    cpu.a_regs[7] = 0x200;
    memory[0x200] = 0xAA;
    m68k_write_16(&cpu, 4, 0x141F);
    m68k_step(&cpu);

    assert((cpu.d_regs[2] & 0xFF) == 0xAA);
    assert(cpu.a_regs[7] == 0x202);

    printf("Regression: Postinc/predec byte test passed!\n");
}

void test_regression_reset_vectors() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x00000400);
    m68k_write_32(&cpu, 0x04, 0x00000100);

    m68k_reset(&cpu);

    assert(cpu.a_regs[7] == 0x400);
    assert(cpu.pc == 0x100);
    assert(cpu.sr == 0x2700);

    printf("Regression: Reset vectors test passed!\n");
}

void test_regression_jmp_dispatch() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[0] = 0x200;
    m68k_write_16(&cpu, 0, 0x4ED0);
    m68k_step(&cpu);

    assert(cpu.pc == 0x200);

    cpu.pc = 0x10;
    m68k_write_16(&cpu, 0x10, 0x4EF8);
    m68k_write_16(&cpu, 0x12, 0x0100);
    m68k_step(&cpu);

    assert(cpu.pc == 0x100);

    printf("Regression: JMP dispatch test passed!\n");
}

void test_regression_div_by_zero() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7] = 0x1000;

    m68k_write_32(&cpu, 5 * 4, 0x300);

    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 100;
    m68k_write_16(&cpu, 0, 0x82C0);
    m68k_step(&cpu);

    assert(cpu.pc == 0x300);

    printf("Regression: Div-by-zero exception test passed!\n");
}

void test_regression_addx_subx_zflag() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.sr |= M68K_SR_Z;
    cpu.sr |= M68K_SR_X;

    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;

    m68k_write_16(&cpu, 0, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 1);
    assert((cpu.sr & M68K_SR_Z) == 0);

    cpu.sr |= M68K_SR_Z;
    cpu.sr &= ~M68K_SR_X;

    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;
    m68k_write_16(&cpu, 2, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 0);
    assert((cpu.sr & M68K_SR_Z) != 0);

    cpu.sr &= ~M68K_SR_Z;
    cpu.sr &= ~M68K_SR_X;
    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;
    m68k_write_16(&cpu, 4, 0xD380);
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) == 0);

    printf("Regression: ADDX/SUBX Z-flag test passed!\n");
}

void test_regression_sbcd() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0] = 0x01;
    cpu.d_regs[1] = 0x10;
    cpu.sr &= ~M68K_SR_X;

    m68k_write_16(&cpu, 0, 0x8300);
    m68k_step(&cpu);
    assert((cpu.d_regs[1] & 0xFF) == 0x09);

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
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0] = 0x00;
    cpu.sr &= ~M68K_SR_X;

    m68k_write_16(&cpu, 0, 0x4800);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x00);
    assert((cpu.sr & M68K_SR_C) == 0);

    cpu.d_regs[0] = 0x01;
    cpu.sr &= ~M68K_SR_X;
    m68k_write_16(&cpu, 2, 0x4800);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0x99);
    assert((cpu.sr & M68K_SR_C) != 0);

    printf("Regression: NBCD test passed!\n");
}

void test_regression_stop_privilege() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 8 * 4, 0x300);

    cpu.sr = 0x0000;
    cpu.pc = 0x100;
    cpu.a_regs[7] = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E72);
    m68k_write_16(&cpu, 0x102, 0x2700);

    m68k_step(&cpu);

    assert(cpu.pc == 0x300);

    printf("Regression: STOP privilege test passed!\n");
}

void test_regression_ext_dispatch() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0] = 0x00000080;

    m68k_write_16(&cpu, 0, 0x4880);
    m68k_step(&cpu);

    assert((cpu.d_regs[0] & 0xFFFF) == 0xFF80);

    cpu.d_regs[1] = 0x00008000;

    m68k_write_16(&cpu, 2, 0x48C1);
    m68k_step(&cpu);

    assert(cpu.d_regs[1] == 0xFFFF8000);

    printf("Regression: EXT dispatch test passed!\n");
}
