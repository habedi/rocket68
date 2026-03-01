#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_fibonacci() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;

    u32 pc = 0x100;

    m68k_write_16(&cpu, pc, 0x7000);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x7201);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x7408);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x2601);
    pc += 2;

    m68k_write_16(&cpu, pc, 0xD280);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x2003);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x51CA);
    pc += 2;
    m68k_write_16(&cpu, pc, 0xFFF8);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x4E71);

    cpu.pc = 0x100;
    int max_steps = 100;
    while (max_steps-- > 0) {
        if (cpu.pc == pc) break;
        m68k_step(&cpu);
    }
    assert(max_steps > 0);
    assert(cpu.d_regs[1].l == 55);

    printf("Integration: Fibonacci test passed!\n");
}

void test_string_copy() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;

    const char* hello = "Hello";
    for (int i = 0; i <= 5; i++) memory[0x200 + i] = hello[i];

    u32 pc = 0x100;

    m68k_write_16(&cpu, pc, 0x41F8);
    pc += 2;
    m68k_write_16(&cpu, pc, 0x0200);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x43F8);
    pc += 2;
    m68k_write_16(&cpu, pc, 0x0300);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x12D8);
    pc += 2;

    m68k_write_16(&cpu, pc, 0x66FC);
    pc += 2;

    u32 end_pc = pc;
    m68k_write_16(&cpu, pc, 0x4E71);

    cpu.pc = 0x100;
    int max_steps = 100;
    while (max_steps-- > 0) {
        if (cpu.pc == end_pc) break;
        m68k_step(&cpu);
    }
    assert(max_steps > 0);
    assert(memcmp(&memory[0x300], "Hello", 6) == 0);

    printf("Integration: String copy test passed!\n");
}

void test_exception_handler() {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;

    m68k_write_32(&cpu, 32 * 4, 0x400);

    m68k_write_16(&cpu, 0x400, 0x7E2A);

    m68k_write_16(&cpu, 0x402, 0x4E73);

    m68k_write_16(&cpu, 0x100, 0x7E00);

    m68k_write_16(&cpu, 0x102, 0x4E40);

    m68k_write_16(&cpu, 0x104, 0x4E71);

    cpu.pc = 0x100;
    int max_steps = 20;
    while (max_steps-- > 0) {
        if (cpu.pc == 0x104) break;
        m68k_step(&cpu);
    }
    assert(max_steps > 0);
    assert(cpu.d_regs[7].l == 42);
    assert(cpu.pc == 0x104);

    printf("Integration: Exception handler test passed!\n");
}
