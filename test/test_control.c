#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_control_flow() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_16(&cpu, 0, 0x6004);
    m68k_write_16(&cpu, 2, 0x103C);
    m68k_write_16(&cpu, 4, 0x0001);
    m68k_write_16(&cpu, 6, 0x103C);
    m68k_write_16(&cpu, 8, 0x0002);

    m68k_step(&cpu);
    assert(cpu.pc == 6);

    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 2);

    memset(memory, 0, 8);
    m68k_reset(&cpu);

    m68k_set_ar(&cpu, 7, 1000);

    m68k_write_16(&cpu, 0, 0x4EB8);
    m68k_write_16(&cpu, 2, 0x0100);

    m68k_write_16(&cpu, 4, 0x103C);
    m68k_write_16(&cpu, 6, 0x0005);

    m68k_write_16(&cpu, 0x100, 0x103C);
    m68k_write_16(&cpu, 0x102, 0x000A);
    m68k_write_16(&cpu, 0x104, 0x4E75);

    m68k_step(&cpu);
    assert(cpu.pc == 0x100);

    assert(m68k_read_32(&cpu, cpu.a_regs[7]) == 4);

    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 10);

    m68k_step(&cpu);
    assert(cpu.pc == 4);

    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 5);

    printf("Control Flow test passed!\n");
}

void test_dbcc_scc() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_dr(&cpu, 0, 0);

    m68k_write_16(&cpu, 0, 0x4A00);

    m68k_write_16(&cpu, 2, 0x57C1);

    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) != 0);
    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0xFF);

    m68k_write_16(&cpu, 4, 0x7002);

    m68k_write_16(&cpu, 6, 0x5241);

    m68k_write_16(&cpu, 8, 0x51C8);
    m68k_write_16(&cpu, 10, 0xFFFC);

    cpu.pc = 4;
    m68k_set_dr(&cpu, 1, 0);

    m68k_step(&cpu);

    printf("Loop 1 start\n");
    fflush(stdout);
    m68k_step(&cpu);
    m68k_step(&cpu);
    assert(cpu.pc == 6);
    assert((cpu.d_regs[0] & 0xFFFF) == 1);

    printf("Loop 2 start\n");
    fflush(stdout);
    m68k_step(&cpu);
    m68k_step(&cpu);
    assert(cpu.pc == 6);
    assert((cpu.d_regs[0] & 0xFFFF) == 0);

    m68k_step(&cpu);
    m68k_step(&cpu);
    assert(cpu.pc == 12);
    assert((cpu.d_regs[0] & 0xFFFF) == 0xFFFF);

    printf("Loop (DBcc/Scc) test passed!\n");
}

void test_misc_control() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0] = 50;
    cpu.d_regs[1] = 100;

    m68k_write_32(&cpu, 6 * 4, 0x400);

    m68k_write_16(&cpu, 0, 0x4380);
    m68k_step(&cpu);

    assert(cpu.pc == 0x400);

    printf("Misc Control (CHK) test passed!\n");
}

void test_exceptions() {
    M68kCpu cpu;
    u8 memory[4096];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7] = 0x1000;
    cpu.ssp = 0x1000;
    cpu.pc = 0x200;
    cpu.sr = 0;

    memory[0x80] = 0x00;
    memory[0x81] = 0x00;
    memory[0x82] = 0x04;
    memory[0x83] = 0x00;

    m68k_write_16(&cpu, 0x200, 0x4E40);
    m68k_step(&cpu);

    assert(cpu.pc == 0x400);

    assert((cpu.sr & M68K_SR_S) != 0);

    assert(cpu.a_regs[7] == 0x1000 - 6);

    u16 saved_sr = (memory[cpu.a_regs[7]] << 8) | memory[cpu.a_regs[7] + 1];
    assert((saved_sr & M68K_SR_S) == 0);
    u32 saved_pc = (memory[cpu.a_regs[7] + 2] << 24) | (memory[cpu.a_regs[7] + 3] << 16) |
                   (memory[cpu.a_regs[7] + 4] << 8) | memory[cpu.a_regs[7] + 5];
    assert(saved_pc == 0x202);

    m68k_write_16(&cpu, 0x400, 0x4E73);
    m68k_step(&cpu);

    assert(cpu.pc == 0x202);
    assert(cpu.a_regs[7] == 0x1000);
    assert((cpu.sr & M68K_SR_S) == 0);

    printf("Exception test passed!\n");
}

void test_nop_bsr_rtr() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_16(&cpu, 0, 0x4E71);
    m68k_step(&cpu);
    assert(cpu.pc == 2);

    cpu.pc = 2;
    cpu.a_regs[7] = 0x100;
    m68k_write_16(&cpu, 2, 0x6108);
    m68k_step(&cpu);
    assert(cpu.pc == 4 + 8);
    assert(cpu.a_regs[7] == 0xFC);
    assert(m68k_read_32(&cpu, 0xFC) == 4);

    cpu.pc = 12;

    m68k_write_32(&cpu, cpu.a_regs[7] - 4, 0x20);
    m68k_write_16(&cpu, cpu.a_regs[7] - 6, 0x1F);
    cpu.a_regs[7] -= 6;
    m68k_write_16(&cpu, 12, 0x4E77);
    m68k_step(&cpu);
    assert(cpu.pc == 0x20);
    assert(cpu.a_regs[7] == 0xFC);
    assert((cpu.sr & 0xFF) == 0x1F);

    printf("NOP/BSR/RTR test passed!\n");
}
