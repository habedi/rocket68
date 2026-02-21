#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_move() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. MOVE.L #0x12345678, D0
    // Opcode: 0010 000 000 111 100 (0x203C)
    // imm: 0x12345678
    m68k_write_16(&cpu, 0, 0x203C);
    m68k_write_32(&cpu, 2, 0x12345678);

    // 2. MOVE.W D0, D1
    // Opcode: 0011 001 000 000 000 (0x3200)
    m68k_write_16(&cpu, 6, 0x3200);

    // 3. MOVE.B #0xAA, (A0)
    // Setup A0 = 0x100
    // Opcode: 0001 010 000 111 100 (0x143C) -> Wait, logic check
    // Dest: (A0) -> Mode 2, Reg 0. Bits: 0 010 (Reg 0, Mode 2) -> 010 000? No.
    // Dest field is <Reg><Mode>. Reg=0(000), Mode=2(010). Field: 000 010.
    // Src: #0xAA -> Mode 7, Reg 4. Field: 111 100.
    // Opcode: 00 01 000 010 111 100 -> 0x10BC
    m68k_set_ar(&cpu, 0, 0x0100);
    m68k_write_16(&cpu, 8, 0x10BC);
    m68k_write_16(&cpu, 10, 0x00AA);  // Immediate byte is low byte of word

    m68k_step(&cpu);  // Exec MOVE.L
    assert(cpu.d_regs[0] == 0x12345678);
    // Flags for MOVE.L: N=0, Z=0 (Assuming value is pos and non-zero)
    // 0x12... is positive.
    assert((cpu.sr & M68K_SR_N) == 0);
    assert((cpu.sr & M68K_SR_Z) == 0);

    m68k_step(&cpu);  // Exec MOVE.W D0, D1
    assert((cpu.d_regs[1] & 0xFFFF) == 0x5678);

    m68k_step(&cpu);  // Exec MOVE.B #AA, (A0)
    assert(m68k_read_8(&cpu, 0x100) == 0xAA);
    // 0xAA is negative as byte (bit 7 is 1)
    assert((cpu.sr & M68K_SR_N) != 0);

    printf("MOVE instruction test passed!\n");
}

void test_movem() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. MOVEM.L D0/D1/A0, -(A7)
    // PreDec: Reg -> Mem.
    // D0=1, D1=2, A0=3.
    // A7 = 100.
    // Registers to push: D0, D1, A0. (Total 3 * 4 = 12 bytes).
    // New A7 = 100 - 12 = 88.
    // Mem[88] = D0, Mem[92] = D1, Mem[96] = A0.

    cpu.d_regs[0] = 1;
    cpu.d_regs[1] = 2;
    cpu.a_regs[0] = 3;
    cpu.a_regs[7] = 100;

    // Opcode: 0100 1000 11 100 111 (0x48E7) -> MOVEM.L Reg->Mem PreDec(A7)
    m68k_write_16(&cpu, 0, 0x48E7);

    // Mask: D0, D1, A0.
    // Reversed for PreDec:
    // A7...A1 A0 D7...D2 D1 D0
    // 0...0   1  0...0   1  1
    // Bits: 15..0
    // Mask: 0000 0000 1000 0011 -> 0x0083?
    // Bit 15: D0 (1). Bit 14: D1 (1). Bit 13: D2 (0)... Bit 8: D7.
    // Bit 7: A0 (1). Bit 6: A1 (0)... Bit 0: A7.
    // So bits set: 15, 14, 7.
    // 1100 0000 1000 0000 -> 0xC080.
    m68k_write_16(&cpu, 2, 0xC080);

    m68k_step(&cpu);

    assert(cpu.a_regs[7] == 88);
    assert(m68k_read_32(&cpu, 88) == 1);  // D0 at Lowest
    assert(m68k_read_32(&cpu, 92) == 2);  // D1
    assert(m68k_read_32(&cpu, 96) == 3);  // A0 at Highest

    // 2. MOVEM.L (A7)+, D0/D1/A0
    // PostInc: Mem -> Reg.
    // A7 = 88.
    // Mem[88]=1, [92]=2, [96]=3.
    // Should load D0=1, D1=2, A0=3.
    // Pop order: Low addr -> D0, ... High -> A7.
    // Mask (Standard): 0=D0, 1=D1, 8=A0.
    // Mask: 0000 0001 0000 0011 -> 0x0103.

    // Clear regs
    cpu.d_regs[0] = 0;
    cpu.d_regs[1] = 0;
    cpu.a_regs[0] = 0;

    // Opcode: 0100 1100 11 011 111 (0x4CDF) -> MOVEM.L Mem->Reg PostInc(A7)
    m68k_write_16(&cpu, 4, 0x4CDF);
    m68k_write_16(&cpu, 6, 0x0103);

    m68k_step(&cpu);

    assert(cpu.d_regs[0] == 1);
    assert(cpu.d_regs[1] == 2);
    assert(cpu.a_regs[0] == 3);
    assert(cpu.a_regs[7] == 100);

    printf("MOVEM instruction test passed!\n");
}

void test_movep() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // MOVEP.W D0, (0,A0) — write word to alternating bytes
    // 0000 000 110 001 000 = 0x0188, displacement = 0
    cpu.d_regs[0] = 0xABCD;
    cpu.a_regs[0] = 0x100;
    m68k_write_16(&cpu, 0, 0x0188);
    m68k_write_16(&cpu, 2, 0x0000);  // displacement
    m68k_step(&cpu);
    assert(memory[0x100] == 0xAB);
    assert(memory[0x102] == 0xCD);

    // MOVEP.W (0,A0), D1 — read word from alternating bytes
    // 0000 001 100 001 000 = 0x0308
    m68k_write_16(&cpu, 4, 0x0308);
    m68k_write_16(&cpu, 6, 0x0000);
    m68k_step(&cpu);
    assert((cpu.d_regs[1] & 0xFFFF) == 0xABCD);

    printf("MOVEP test passed!\n");
}

void test_exg() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. EXG D0, D1
    // Op: 1100 000 1 01000 001 (0xC141) -> D0, D1 exchange
    // Note: Rx=0, Ry=1.
    cpu.d_regs[0] = 0x11111111;
    cpu.d_regs[1] = 0x22222222;
    m68k_write_16(&cpu, 0, 0xC141);
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 0x22222222);
    assert(cpu.d_regs[1] == 0x11111111);

    // 2. EXG A0, A1
    // Op: 1100 000 1 01001 001 (0xC149) -> A0, A1 exchange
    cpu.a_regs[0] = 0xAAAAAAAA;
    cpu.a_regs[1] = 0xBBBBBBBB;
    m68k_write_16(&cpu, 2, 0xC149);
    m68k_step(&cpu);
    assert(cpu.a_regs[0] == 0xBBBBBBBB);
    assert(cpu.a_regs[1] == 0xAAAAAAAA);

    // 3. EXG D0, A0
    // Op: 1100 000 1 10001 000 (0xC188) -> D0, A0 exchange
    // Wait: Rx is D0 (000). Mode 10001. Ry is A0 (000).
    // Swap D0 <-> A0.
    cpu.d_regs[0] = 0xDDDDDDDD;
    cpu.a_regs[0] = 0xAAAAAAAA;  // Changed in prev step, but let's reset or just expect
    // Current A0 is BBBBBBBB from last swap.
    // Reset for clarity
    cpu.a_regs[0] = 0xAAAAAAAA;

    m68k_write_16(&cpu, 4, 0xC188);
    m68k_step(&cpu);

    assert(cpu.d_regs[0] == 0xAAAAAAAA);
    assert(cpu.a_regs[0] == 0xDDDDDDDD);

    printf("EXG instruction test passed!\n");
}

void test_lea_pea() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. LEA (A0), A1
    // A0 = 0x10. A1 should become 0x10.
    // Op: 0100 <A1=1> 111 <Mode=2 (A0)> <Reg=0>
    // 0100 001 111 010 000 -> 0x43D0
    cpu.a_regs[0] = 0x10;
    m68k_write_16(&cpu, 0, 0x43D0);
    m68k_step(&cpu);
    assert(cpu.a_regs[1] == 0x10);

    // 2. PEA (A1)
    // Push 0x10 onto stack.
    // Op: 0100 100 001 <Mode=2> <Reg=1> -> 0x4851
    cpu.a_regs[7] = 0x100;
    m68k_write_16(&cpu, 2, 0x4851);
    m68k_step(&cpu);
    assert(cpu.a_regs[7] == 0x100 - 4);
    assert(m68k_read_32(&cpu, 0xFC) == 0x10);

    printf("LEA/PEA test passed!\n");
}

void test_stack_frame() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7] = 0x200;

    // 1. LINK A6, #-4
    // Push A6, SP=SP-2? No A6 is 4 bytes. SP-4.
    // SP -> A6.
    // SP - 4 -> SP.
    // Op: 0x4E50 + 6 (A6) -> 0x4E56. Disp = 0xFFFC (-4).
    m68k_write_16(&cpu, 0, 0x4E56);
    m68k_write_16(&cpu, 2, 0xFFFC);

    cpu.a_regs[6] = 0xCAFEBABE;

    m68k_step(&cpu);

    assert(cpu.a_regs[6] == 0x200 - 4);                   // New FP = Old SP - 4 (pushed A6)
    assert(cpu.a_regs[7] == 0x200 - 4 - 4);               // New SP = FP - 4
    assert(m68k_read_32(&cpu, 0x200 - 4) == 0xCAFEBABE);  // Pushed A6

    // 2. UNLK A6
    // Op: 0x4E5E (4E58 + 6)
    m68k_write_16(&cpu, 4, 0x4E5E);
    m68k_step(&cpu);

    assert(cpu.a_regs[7] == 0x200);       // Restored SP
    assert(cpu.a_regs[6] == 0xCAFEBABE);  // Restored A6

    printf("Stack Frame (LINK/UNLK) test passed!\n");
}
