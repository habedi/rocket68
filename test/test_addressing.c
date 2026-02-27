#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_addressing_mode_6() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[0].l = 0x100;
    cpu.d_regs[0].l = 0x02;
    memory[0x103] = 0x55;

    m68k_write_16(&cpu, 0, 0x1230);

    m68k_write_16(&cpu, 2, 0x0001);

    m68k_step(&cpu);

    assert((cpu.d_regs[1].l & 0xFF) == 0x55);

    printf("Addressing Mode 6 test passed!\n");
}

void test_pcrel() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x100;
    memory[0x10C] = 0x55;

    m68k_write_16(&cpu, 0x100, 0x103A);
    m68k_write_16(&cpu, 0x102, 0x000A);

    m68k_step(&cpu);

    assert((cpu.d_regs[0].l & 0xFF) == 0x55);

    printf("PC-Relative Addressing test passed!\n");
}
