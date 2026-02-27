#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_logic() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. AND.B D0, D1
    // D0 = 0x0F, D1 = 0xAA (1010 1010) -> Result = 0x0A (0000 1010)
    // AND Opcode: 1100 <Reg D1=1> <OpMode 0> <Mode 0> <Reg D0=0>
    // 1100 001 000 000 000 -> 0xC200
    m68k_set_dr(&cpu, 0, 0x0F);
    m68k_set_dr(&cpu, 1, 0xAA);
    m68k_write_16(&cpu, 0, 0xC200);

    m68k_step(&cpu);
    assert((m68k_get_dr(&cpu, 1) & 0xFF) == 0x0A);
    assert((cpu.sr & M68K_SR_Z) == 0);

    // 2. OR.W D0, D1
    // D0 = 0x00F0, D1 = 0x0A0A -> Result = 0x0AFA
    // OR Opcode: 1000 <Reg D1=1> <OpMode 1> <Mode 0> <Reg D0=0>
    // 1000 001 001 000 000 -> 0x8240
    m68k_set_dr(&cpu, 0, 0x00F0);
    m68k_set_dr(&cpu, 1, 0x0A0A);
    m68k_write_16(&cpu, 2, 0x8240);

    m68k_step(&cpu);
    assert((m68k_get_dr(&cpu, 1) & 0xFFFF) == 0x0AFA);

    // 3. EOR.L D0, D1
    // D0 = 0xFFFFFFFF, D1 = 0x12345678 -> Result = ~D1
    // EOR Opcode: 1011 <Reg D0=0> 1 <Size 10=L> <Mode 0> <Reg D1=1>
    // 1011 000 1 10 000 001 -> 0xB181
    m68k_set_dr(&cpu, 0, 0xFFFFFFFF);
    m68k_set_dr(&cpu, 1, 0x12345678);
    m68k_write_16(&cpu, 4, 0xB181);

    m68k_step(&cpu);
    assert(m68k_get_dr(&cpu, 1) == (u32)(~0x12345678));

    printf("Logic instruction test passed!\n");
}

void test_data_manipulation() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. CLR.L D0
    // 0x4280 (CLR D0)
    m68k_set_dr(&cpu, 0, 0xFFFFFFFF);
    m68k_write_16(&cpu, 0, 0x4280);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0);
    assert((cpu.sr & M68K_SR_Z) != 0);

    // 2. NEG.B D0
    // D0 = 1. -> -1 (0xFF)
    // 0x4400 (NEG.B D0)
    m68k_set_dr(&cpu, 0, 1);
    m68k_write_16(&cpu, 2, 0x4400);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFF) == 0xFF);

    // 3. EXT.W D0 (Byte -> Word)
    // D0 = 0xFF (from prev). Sign extend to 0xFFFF.
    // 0x4880 (EXT.W D0)
    m68k_write_16(&cpu, 4, 0x4880);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0xFFFF);

    // 4. SWAP D0
    // D0 = 0x12345678. -> 0x56781234
    // 0x4840 (SWAP D0)
    m68k_set_dr(&cpu, 0, 0x12345678);
    m68k_write_16(&cpu, 6, 0x4840);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0x56781234);

    // 5. MULU.W D0, D1
    // D0 = 10, D1 = 20. -> Result 200.
    // Op: 1100 <D1=1> 011 <Dn Mode=0> <D0=0> -> 0xC2C0
    m68k_set_dr(&cpu, 0, 10);
    m68k_set_dr(&cpu, 1, 20);
    m68k_write_16(&cpu, 8, 0xC2C0);
    m68k_step(&cpu);
    assert(cpu.d_regs[1].l == 200);

    // 6. DIVU.W D0, D1
    // D1 = 200 (from prev). D0 = 10. -> Result 20.
    // Op: 1000 <D1=1> 011 <Dn Mode=0> <D0=0> -> 0x82C0
    m68k_write_16(&cpu, 10, 0x82C0);
    m68k_step(&cpu);
    // Result packed: Remainder | Quotient. 0 | 20.
    assert((cpu.d_regs[1].l & 0xFFFF) == 20);
    assert((cpu.d_regs[1].l >> 16) == 0);

    printf("Data Manipulation test passed!\n");
}

// =============================================================================
// Tests for Phase 1-3 New Features
// =============================================================================

void test_bit_manipulation() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // BTST #3, D0 — test bit 3 of D0 (should be zero, Z=1)
    // Static BTST: 0000 1000 00 000 000 = 0x0800, followed by 0x0003
    cpu.d_regs[0].l = 0x00;
    m68k_write_16(&cpu, 0, 0x0800);
    m68k_write_16(&cpu, 2, 0x0003);
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) != 0);  // Bit 3 is 0

    // BTST #3, D0 — bit 3 set
    cpu.d_regs[0].l = 0x08;
    cpu.pc = 4;
    m68k_write_16(&cpu, 4, 0x0800);
    m68k_write_16(&cpu, 6, 0x0003);
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) == 0);  // Bit 3 is 1

    // BSET #5, D0 — set bit 5
    // Static BSET: 0000 1000 11 000 000 = 0x08C0, followed by 0x0005
    cpu.d_regs[0].l = 0x00;
    cpu.pc = 8;
    m68k_write_16(&cpu, 8, 0x08C0);
    m68k_write_16(&cpu, 10, 0x0005);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0x20);    // Bit 5 set
    assert((cpu.sr & M68K_SR_Z) != 0);  // Was zero before

    // BCLR #5, D0 — clear bit 5
    // Static BCLR: 0000 1000 10 000 000 = 0x0880, followed by 0x0005
    cpu.pc = 12;
    m68k_write_16(&cpu, 12, 0x0880);
    m68k_write_16(&cpu, 14, 0x0005);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0x00);
    assert((cpu.sr & M68K_SR_Z) == 0);  // Was set before

    // BCHG D1, D0 — toggle bit specified by D1
    cpu.d_regs[0].l = 0x00;
    cpu.d_regs[1].l = 4;  // Bit 4
    cpu.pc = 16;
    // Dynamic BCHG: 0000 001 1 01 000 000 = 0x0340
    m68k_write_16(&cpu, 16, 0x0340);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0x10);

    printf("Bit manipulation test passed!\n");
}

void test_shift_rotate() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. LSR.W #1, D0
    // D0 = 2. Result = 1.
    // 1110 001 0 01 0 01 000 (0xE248). Count=1, R, Word, Imm, LS, D0.
    m68k_set_dr(&cpu, 0, 2);
    m68k_write_16(&cpu, 0, 0xE248);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 1);
    assert((cpu.sr & M68K_SR_C) == 0);

    // 2. LSL.B #1, D0
    // D0 = 0x80. Result = 0. C = 1.
    // 1110 001 1 00 0 01 000 (0xE308?). d=1(L), sz=0(B).
    // D0 = 129 (0x81). << 1 = 0x02. C = 1 (0x80 out).
    m68k_set_dr(&cpu, 0, 0x81);
    m68k_write_16(&cpu, 2, 0xE308);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFF) == 2);
    assert((cpu.sr & M68K_SR_C) != 0);
    assert((cpu.sr & M68K_SR_X) != 0);

    // 3. ASR.W #1, D0
    // D0 = 0x8000 (-32768). >> 1 = 0xC000 (-16384). Sign extend.
    // 1110 001 0 01 0 00 000 (0xE240). d=0(R), sz=1(W), type=0(AS)
    m68k_set_dr(&cpu, 0, 0x8000);
    m68k_write_16(&cpu, 4, 0xE240);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0xC000);

    // 4. ROL.L #1, D0
    // D0 = 0x80000000. -> 0x00000001. C = 1.
    // 1110 001 1 10 0 11 000 (0xE398). d=1, sz=2(L), type=3(RO)
    m68k_set_dr(&cpu, 0, 0x80000000);
    m68k_write_16(&cpu, 6, 0xE398);
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 1);
    assert((cpu.sr & M68K_SR_C) != 0);

    printf("Shift/Rotate test passed!\n");
}

void test_cmp_tst() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. CMP.B D0, D1 (D1 - D0)
    // D0 = 0x05, D1 = 0x05 -> Result 0. Z=1.
    m68k_set_dr(&cpu, 0, 0x05);
    m68k_set_dr(&cpu, 1, 0x05);
    // CMP.B D0, D1: 1011 001 0 00 000 000 (0xB200)
    m68k_write_16(&cpu, 0, 0xB200);
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) != 0);

    // 2. CMP.B D0, D1
    // D0 = 0x06, D1 = 0x05 -> Result -1 (0xFF). N=1, C=1.
    m68k_set_dr(&cpu, 0, 0x06);
    m68k_set_dr(&cpu, 1, 0x05);
    m68k_write_16(&cpu, 2, 0xB200);
    cpu.pc = 2;  // Reset PC
    cpu.sr = 0;  // Clear flags
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_N) != 0);
    assert((cpu.sr & M68K_SR_C) != 0);

    // 3. CMPI.W #$1234, D0
    // D0 = 0x1234. Result 0. Z=1.
    // CMPI: 0000 1100 01 000 000 (0x0C40) + Imm
    m68k_set_dr(&cpu, 0, 0x1234);
    m68k_write_16(&cpu, 4, 0x0C40);
    m68k_write_16(&cpu, 6, 0x1234);
    cpu.pc = 4;
    cpu.sr = 0;  // Clear flags
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) != 0);

    // 4. TST.W D0
    // D0=0. Z=1. V=0 C=0.
    m68k_set_dr(&cpu, 0, 0);
    // TST.W D0: 0100 1010 01 000 000 (0x4A40)
    m68k_write_16(&cpu, 8, 0x4A40);
    cpu.pc = 8;
    cpu.sr = 0xFFFF;  // Set all flags to check clearing
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) != 0);
    assert((cpu.sr & M68K_SR_V) == 0);
    assert((cpu.sr & M68K_SR_C) == 0);

    printf("CMP/TST test passed!\n");
}

void test_new_tas() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // TAS D0: D0=0x00 -> flags based on 0x00, then set bit 7 -> 0x80
    cpu.d_regs[0].l = 0x00;
    // TAS D0: 0100 1010 11 000 000 = 0x4AC0
    m68k_write_16(&cpu, 0, 0x4AC0);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFF) == 0x80);
    assert((cpu.sr & M68K_SR_Z) != 0);  // Was zero

    // TAS D0: D0=0x80 -> flags based on 0x80, set bit 7 -> 0x80 (already set)
    cpu.d_regs[0].l = 0x80;
    m68k_write_16(&cpu, 2, 0x4AC0);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFF) == 0x80);
    assert((cpu.sr & M68K_SR_Z) == 0);
    assert((cpu.sr & M68K_SR_N) != 0);

    printf("TAS test passed!\n");
}

void test_logic_not() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // NOT.W D0
    // 0100 0110 01 000 000 = 0x4640
    cpu.d_regs[0].l = 0x5555;
    m68k_write_16(&cpu, 0, 0x4640);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0xAAAA);
    assert((cpu.sr & M68K_SR_N) != 0);

    // NOT.L D1
    // 0100 0110 10 000 001 = 0x4681
    cpu.d_regs[1].l = 0x12345678;
    m68k_write_16(&cpu, 2, 0x4681);
    m68k_step(&cpu);
    assert(cpu.d_regs[1].l == (u32)~0x12345678);

    printf("NOT test passed!\n");
}

void test_logic_ccr_sr() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // ANDI to CCR
    // 0000 0010 0011 1100 = 0x023C, then word 0x00XX
    cpu.sr = 0x271F;  // Supervisor (S=1, T=0, I=7), all CCR flags set
    m68k_write_16(&cpu, 0, 0x023C);
    m68k_write_16(&cpu, 2, 0x000F);  // Keep only 0x0F
    m68k_step(&cpu);
    assert(cpu.sr == 0x270F);  // Upper byte untouched, lower byte masked

    // ORI to CCR
    // 0000 0000 0011 1100 = 0x003C
    cpu.sr = 0x0000;
    m68k_write_16(&cpu, 4, 0x003C);
    m68k_write_16(&cpu, 6, 0x0015);
    m68k_step(&cpu);
    assert(cpu.sr == 0x0015);

    // EORI to CCR
    // 0000 1010 0011 1100 = 0x0A3C
    cpu.sr = 0x0015;
    m68k_write_16(&cpu, 8, 0x0A3C);
    m68k_write_16(&cpu, 10, 0x0011);
    m68k_step(&cpu);
    assert(cpu.sr == 0x0004);  // 0x15 ^ 0x11 = 0x04

    // EORI to SR (privilege)
    // 0000 1010 0111 1100 = 0x0A7C
    cpu.sr = 0x2000;  // Supervisor mode
    m68k_write_16(&cpu, 12, 0x0A7C);
    m68k_write_16(&cpu, 14, 0x001F);
    m68k_step(&cpu);
    assert(cpu.sr == 0x201F);

    printf("CCR/SR Logic test passed!\n");
}
