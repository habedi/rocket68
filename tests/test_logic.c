#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_logic(void) {
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

    /* AND/OR/EOR across remaining sizes and Dn -> <ea> memory paths */
    cpu.a_regs[0].l = 0x200;
    m68k_write_32(&cpu, 0x200, 0xAABBCCDD);
    cpu.d_regs[0].l = 0x0F0F0F0F;
    m68k_write_16(&cpu, 6, 0xC050);  /* AND.W (A0), D0 */
    m68k_write_16(&cpu, 8, 0xC090);  /* AND.L (A0), D0 */
    m68k_write_16(&cpu, 10, 0xC110); /* AND.B D0, (A0) */
    m68k_write_16(&cpu, 12, 0xC150); /* AND.W D0, (A0) */
    m68k_write_16(&cpu, 14, 0xC190); /* AND.L D0, (A0) */
    m68k_write_16(&cpu, 16, 0x8010); /* OR.B (A0), D0 */
    m68k_write_16(&cpu, 18, 0x8090); /* OR.L (A0), D0 */
    m68k_write_16(&cpu, 20, 0x8110); /* OR.B D0, (A0) */
    m68k_write_16(&cpu, 22, 0x8150); /* OR.W D0, (A0) */
    m68k_write_16(&cpu, 24, 0x8190); /* OR.L D0, (A0) */
    m68k_write_16(&cpu, 26, 0xB101); /* EOR.B D0, D1 */
    m68k_write_16(&cpu, 28, 0xB141); /* EOR.W D0, D1 */
    m68k_write_16(&cpu, 30, 0xB110); /* EOR.B D0, (A0) */
    m68k_write_16(&cpu, 32, 0xB150); /* EOR.W D0, (A0) */
    m68k_write_16(&cpu, 34, 0xB190); /* EOR.L D0, (A0) */
    for (int i = 0; i < 15; i++) m68k_step(&cpu);

    printf("Logic instruction test passed!\n");
}

void test_data_manipulation(void) {
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

    /* MULS.W D0, D1 (0xC3C0) and DIVS.W D0, D1 (0x83C0) */
    cpu.d_regs[0].l = 0xFFFF; /* -1 */
    cpu.d_regs[1].l = 20;
    m68k_write_16(&cpu, 12, 0xC3C0);
    m68k_step(&cpu);
    assert((s32)cpu.d_regs[1].l == -20);

    cpu.d_regs[0].l = 5;
    m68k_write_16(&cpu, 14, 0x83C0);
    m68k_step(&cpu);
    assert((s16)(cpu.d_regs[1].l & 0xFFFF) == -4);

    /* EXT.L D0 (0x48C0), NEG.W (A0) (0x4450), NEG.L (A0) (0x4490), NOT.B (A0) (0x4610) */
    cpu.d_regs[0].l = 0x8000;
    cpu.a_regs[0].l = 0x200;
    m68k_write_32(&cpu, 0x200, 0x00010002);
    m68k_write_16(&cpu, 16, 0x48C0);
    m68k_write_16(&cpu, 18, 0x4450);
    m68k_write_16(&cpu, 20, 0x4490);
    m68k_write_16(&cpu, 22, 0x4610);
    for (int i = 0; i < 4; i++) m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0xFFFF8000u);

    printf("Data Manipulation test passed!\n");
}

// =============================================================================
// Tests for Phase 1-3 New Features
// =============================================================================

void test_bit_manipulation(void) {
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

    /* Dynamic and memory variants of BTST, BSET, BCLR, and BCHG */
    cpu.a_regs[0].l = 0x200;
    memory[0x200] = 0x05;
    cpu.d_regs[1].l = 2;
    m68k_write_16(&cpu, 18, 0x0300); /* BTST D1, D0 */
    m68k_write_16(&cpu, 20, 0x0310); /* BTST D1, (A0) */
    m68k_write_16(&cpu, 22, 0x03C0); /* BSET D1, D0 */
    m68k_write_16(&cpu, 24, 0x03D0); /* BSET D1, (A0) */
    m68k_write_16(&cpu, 26, 0x0380); /* BCLR D1, D0 */
    m68k_write_16(&cpu, 28, 0x0390); /* BCLR D1, (A0) */
    m68k_write_16(&cpu, 30, 0x0840); /* BCHG #2, D0 */
    m68k_write_16(&cpu, 32, 0x0002);
    m68k_write_16(&cpu, 34, 0x0850); /* BCHG #2, (A0) */
    m68k_write_16(&cpu, 36, 0x0002);
    m68k_write_16(&cpu, 38, 0x0850); /* BCHG #2, (A0) again to toggle both states */
    m68k_write_16(&cpu, 40, 0x0002);
    for (int i = 0; i < 9; i++) m68k_step(&cpu);
    assert(memory[0x200] == 0x01);

    printf("Bit manipulation test passed!\n");
}

void test_shift_rotate(void) {
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
    // D0 = 0x81. Result = 0x02, and C = 1 because bit 7 shifts out.
    // 1110 001 1 00 0 01 000 (0xE308). d=1(L), sz=0(B).
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

    /* ROXL/ROXR/ROR/ASL across sizes and memory shift/rotate variants */
    cpu.a_regs[0].l = 0x200;
    m68k_write_16(&cpu, 0x200, 0x8001);
    m68k_write_16(&cpu, 8, 0xE310);  /* ROXL.B #1, D0 */
    m68k_write_16(&cpu, 10, 0xE250); /* ROXR.W #1, D0 */
    m68k_write_16(&cpu, 12, 0xE290); /* ROXR.L #1, D0 */
    m68k_write_16(&cpu, 14, 0xE390); /* ROXL.L #1, D0 */
    m68k_write_16(&cpu, 16, 0xE218); /* ROR.B #1, D0 */
    m68k_write_16(&cpu, 18, 0xE258); /* ROR.W #1, D0 */
    m68k_write_16(&cpu, 20, 0xE298); /* ROR.L #1, D0 */
    m68k_write_16(&cpu, 22, 0xE300); /* ASL.B #1, D0 */
    m68k_write_16(&cpu, 24, 0xE380); /* ASL.L #1, D0 */
    m68k_write_16(&cpu, 26, 0xE0D0); /* ASR.W (A0) */
    m68k_write_16(&cpu, 28, 0xE1D0); /* ASL.W (A0) */
    m68k_write_16(&cpu, 30, 0xE2D0); /* LSR.W (A0) */
    m68k_write_16(&cpu, 32, 0xE3D0); /* LSL.W (A0) */
    m68k_write_16(&cpu, 34, 0xE4D0); /* ROXR.W (A0) */
    m68k_write_16(&cpu, 36, 0xE5D0); /* ROXL.W (A0) */
    m68k_write_16(&cpu, 38, 0xE6D0); /* ROR.W (A0) */
    m68k_write_16(&cpu, 40, 0xE7D0); /* ROL.W (A0) */
    for (int i = 0; i < 17; i++) m68k_step(&cpu);

    printf("Shift/Rotate test passed!\n");
}

void test_cmp_tst(void) {
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

void test_new_tas(void) {
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

void test_logic_not(void) {
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

void test_logic_ccr_sr(void) {
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

void test_andi_ori_eori(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // ANDI.L #0x000000FF, D0 -> 0x0280 0000 00FF
    cpu.d_regs[0].l = 0x12345678;
    m68k_write_16(&cpu, 0, 0x0280);
    m68k_write_32(&cpu, 2, 0x000000FF);
    cpu.pc = 0;
    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 0x00000078);
    assert((cpu.sr & M68K_SR_Z) == 0);

    // ORI.W #0x0F0F, (A0) -> 0x0050 0F0F
    cpu.a_regs[0].l = 0x100;
    m68k_write_16(&cpu, 6, 0x0050);
    m68k_write_16(&cpu, 8, 0x0F0F);
    m68k_write_16(&cpu, 0x100, 0xF0F0);
    m68k_step(&cpu);
    assert(m68k_read_16(&cpu, 0x100) == 0xFFFF);
    assert((cpu.sr & M68K_SR_N) != 0);

    // EORI.B #0x55, D1 -> 0x0A01 0055
    cpu.d_regs[1].l = 0xAA;
    m68k_write_16(&cpu, 10, 0x0A01);
    m68k_write_16(&cpu, 12, 0x0055);
    m68k_step(&cpu);
    assert((cpu.d_regs[1].l & 0xFF) == 0xFF);

    /* Remaining ANDI/ORI/EORI sizes and memory writes, plus ANDI/ORI to SR */
    m68k_write_32(&cpu, 0x100, 0x12345678);
    m68k_write_16(&cpu, 14, 0x0250); /* ANDI.W #$0F0F, (A0) */
    m68k_write_16(&cpu, 16, 0x0F0F);
    m68k_write_16(&cpu, 18, 0x0090); /* ORI.L #$F0000000, (A0) */
    m68k_write_32(&cpu, 20, 0xF0000000u);
    m68k_write_16(&cpu, 24, 0x0A50); /* EORI.W #$FFFF, (A0) */
    m68k_write_16(&cpu, 26, 0xFFFF);
    m68k_write_16(&cpu, 28, 0x0A90); /* EORI.L #$FFFFFFFF, (A0) */
    m68k_write_32(&cpu, 30, 0xFFFFFFFFu);
    for (int i = 0; i < 4; i++) m68k_step(&cpu);
    cpu.sr = 0x271F;
    m68k_write_16(&cpu, 34, 0x027C); /* ANDI #$2703, SR */
    m68k_write_16(&cpu, 36, 0x2703);
    m68k_write_16(&cpu, 38, 0x007C); /* ORI #$0010, SR */
    m68k_write_16(&cpu, 40, 0x0010);
    for (int i = 0; i < 2; i++) m68k_step(&cpu);
    assert(cpu.sr == 0x2713);

    printf("ANDI/ORI/EORI test passed!\n");
}
