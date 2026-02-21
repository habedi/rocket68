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

    // Test: MOVE.B (d8, A0, D0.W), D1
    // Base A0 = 0x100
    // Index D0 = 0x02
    // Disp = 0x01
    // Addr = 0x100 + 0x02 + 0x01 = 0x103
    // content at 0x103 = 0x55

    cpu.a_regs[0] = 0x100;
    cpu.d_regs[0] = 0x02;
    memory[0x103] = 0x55;

    // Opcode: MOVE.B (ea), D1 -> 0001 001 000 <Mode 6> <Reg 0>
    // 0001 001 000 110 000 -> 0x1230
    m68k_write_16(&cpu, 0, 0x1230);

    // Extension Word: D/A RRR W/L 000 DISP
    // D0.W -> D/A=0, Reg=0, W/L=0.
    // Disp=1.
    // 0 000 0 000 00000001 -> 0x0001
    m68k_write_16(&cpu, 2, 0x0001);

    m68k_step(&cpu);

    assert((cpu.d_regs[1] & 0xFF) == 0x55);

    printf("Addressing Mode 6 test passed!\n");
}

void test_pcrel() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Test: MOVE.B (d16,PC), D0
    // PC at instruction start 0x100.
    // Opcode at 0x100. Ext word at 0x102.
    // Target is at 0x100 + 2 + 0xA = 0x10C.
    // Value at 0x10C = 0x55.

    cpu.pc = 0x100;
    memory[0x10C] = 0x55;

    // Opcode: MOVE.B (d16,PC), D0
    // Src: Mode 7 (111), Reg 2 (010). PC-Rel Disp.
    // Dest: D0 (Mode 0, Reg 0).
    // MOVE.B -> 0001 (10). Src(111 010) Dest(Mode0 Reg0)
    // Dest field 000 000. Src 111 010.
    // 0001 000 000 111 010 -> 0x103A
    m68k_write_16(&cpu, 0x100, 0x103A);
    m68k_write_16(&cpu, 0x102, 0x000A);  // Disp 10

    m68k_step(&cpu);

    assert((cpu.d_regs[0] & 0xFF) == 0x55);

    printf("PC-Relative Addressing test passed!\n");
}
