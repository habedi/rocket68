#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"

void test_initialization() {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));

    m68k_init(&cpu, memory, sizeof(memory));
    m68k_reset(&cpu);

    // After reset with zero vectors, PC and SP should be 0
    assert(cpu.pc == 0);
    assert(cpu.sr == 0x2700);
    for (int i = 0; i < 8; i++) {
        assert(cpu.d_regs[i] == 0);
        assert(cpu.a_regs[i] == 0);
    }

    printf("Initialization test passed!\n");
}

void test_register_access() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_dr(&cpu, 0, 0xDEADBEEF);
    assert(m68k_get_dr(&cpu, 0) == 0xDEADBEEF);

    m68k_set_ar(&cpu, 7, 0x1000);
    assert(m68k_get_ar(&cpu, 7) == 0x1000);

    printf("Register access test passed!\n");
}

void test_memory_access() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Test 8-bit
    m68k_write_8(&cpu, 10, 0xAB);
    assert(m68k_read_8(&cpu, 10) == 0xAB);

    // Test 16-bit (Big Endian)
    m68k_write_16(&cpu, 20, 0x1234);
    assert(m68k_read_16(&cpu, 20) == 0x1234);
    assert(memory[20] == 0x12);
    assert(memory[21] == 0x34);

    // Test 32-bit (Big Endian)
    m68k_write_32(&cpu, 30, 0xDEADBEEF);
    assert(m68k_read_32(&cpu, 30) == 0xDEADBEEF);
    assert(memory[30] == 0xDE);
    assert(memory[31] == 0xAD);
    assert(memory[32] == 0xBE);
    assert(memory[33] == 0xEF);

    printf("Memory access test passed!\n");
}

void test_fetch() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Write NOPs
    m68k_write_16(&cpu, 0, 0x4E71);
    m68k_write_16(&cpu, 2, 0x4E71);

    m68k_step(&cpu);
    assert(cpu.pc == 2);

    m68k_step(&cpu);
    assert(cpu.pc == 4);

    printf("Fetch test passed!\n");
}

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

// ... (Existing tests)

void test_control_flow() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. BRA to skip an instruction
    // 0: BRA.S +4 (Skip next 2 bytes) -> 0x6004
    // 2: MOVE.B #1, D0 (Should be skipped) -> 0x103C 0001
    // 4: MOVE.B #2, D0 (Should be executed) -> 0x103C 0002
    m68k_write_16(&cpu, 0, 0x6004);
    m68k_write_16(&cpu, 2, 0x103C);
    m68k_write_16(&cpu, 4, 0x0001);
    m68k_write_16(&cpu, 6, 0x103C);
    m68k_write_16(&cpu, 8, 0x0002);

    // Step 1: Execute BRA
    m68k_step(&cpu);
    assert(cpu.pc == 6);  // 0+2 (inst info) + 4 (disp)? No.
    // BRA logic: "cpu->pc = cpu->pc + disp".
    // 0x6004 at 0. Fetch reads at 0, PC becomes 2.
    // disp is 4.
    // Target = 2 + 4 = 6. Correct.

    // Step 2: Execute MOVE #2
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 2);

    // 2. BNE (Branch Not Equal)
    // 0x6600 <disp> (16-bit)
    // Set Z flag first.
    // MOVE.B #0, D1 -> Sets Z=1.
    // BNE +4 -> Should NOT branch.
    // MOVE.B #1, D1 -> Sets Z=0.
    // BNE -2 -> Should branch back.

    // Reset CPU for clean state
    // Zero vector area so m68k_reset loads PC=0 and SSP=0
    memset(memory, 0, 8);
    m68k_reset(&cpu);
    // Initial SP
    m68k_set_ar(&cpu, 7, 1000);

    // JSR / RTS Test
    // 0: JSR 0x100
    // 4: MOVE.B #5, D0 (Return address)
    // ...
    // 0x100: MOVE.B #10, D0
    // 0x104: RTS

    // JSR Mode=7 (111), Reg=0 -> Abs Short. 0x4E80 | (7<<3) | 0 = 0x4EB8
    m68k_write_16(&cpu, 0, 0x4EB8);
    m68k_write_16(&cpu, 2, 0x0100);

    // Return point
    m68k_write_16(&cpu, 4, 0x103C);  // MOVE.B #5, D0
    m68k_write_16(&cpu, 6, 0x0005);

    // Subroutine
    m68k_write_16(&cpu, 0x100, 0x103C);  // MOVE.B #10, D0
    m68k_write_16(&cpu, 0x102, 0x000A);
    m68k_write_16(&cpu, 0x104, 0x4E75);  // RTS

    m68k_step(&cpu);  // JSR
    assert(cpu.pc == 0x100);
    // Stack should have return address 4
    assert(m68k_read_32(&cpu, cpu.a_regs[7]) == 4);

    m68k_step(&cpu);  // MOVE #10
    assert(cpu.d_regs[0] == 10);

    m68k_step(&cpu);  // RTS
    assert(cpu.pc == 4);

    m68k_step(&cpu);  // MOVE #5
    assert(cpu.d_regs[0] == 5);

    printf("Control Flow test passed!\n");
}

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
    assert(cpu.d_regs[0] == 0);
    assert((cpu.sr & M68K_SR_Z) != 0);

    // 2. NEG.B D0
    // D0 = 1. -> -1 (0xFF)
    // 0x4400 (NEG.B D0)
    m68k_set_dr(&cpu, 0, 1);
    m68k_write_16(&cpu, 2, 0x4400);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 0xFF);

    // 3. EXT.W D0 (Byte -> Word)
    // D0 = 0xFF (from prev). Sign extend to 0xFFFF.
    // 0x4880 (EXT.W D0)
    m68k_write_16(&cpu, 4, 0x4880);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFFFF) == 0xFFFF);

    // 4. SWAP D0
    // D0 = 0x12345678. -> 0x56781234
    // 0x4840 (SWAP D0)
    m68k_set_dr(&cpu, 0, 0x12345678);
    m68k_write_16(&cpu, 6, 0x4840);
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 0x56781234);

    // 5. MULU.W D0, D1
    // D0 = 10, D1 = 20. -> Result 200.
    // Op: 1100 <D1=1> 011 <Dn Mode=0> <D0=0> -> 0xC2C0
    m68k_set_dr(&cpu, 0, 10);
    m68k_set_dr(&cpu, 1, 20);
    m68k_write_16(&cpu, 8, 0xC2C0);
    m68k_step(&cpu);
    assert(cpu.d_regs[1] == 200);

    // 6. DIVU.W D0, D1
    // D1 = 200 (from prev). D0 = 10. -> Result 20.
    // Op: 1000 <D1=1> 011 <Dn Mode=0> <D0=0> -> 0x82C0
    m68k_write_16(&cpu, 10, 0x82C0);
    m68k_step(&cpu);
    // Result packed: Remainder | Quotient. 0 | 20.
    assert((cpu.d_regs[1] & 0xFFFF) == 20);
    assert((cpu.d_regs[1] >> 16) == 0);

    printf("Data Manipulation test passed!\n");
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
    assert(cpu.d_regs[0] == 1);
    assert((cpu.sr & M68K_SR_C) == 0);

    // 2. LSL.B #1, D0
    // D0 = 0x80. Result = 0. C = 1.
    // 1110 001 1 00 0 01 000 (0xE308?). d=1(L), sz=0(B).
    // D0 = 129 (0x81). << 1 = 0x02. C = 1 (0x80 out).
    m68k_set_dr(&cpu, 0, 0x81);
    m68k_write_16(&cpu, 2, 0xE308);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFF) == 2);
    assert((cpu.sr & M68K_SR_C) != 0);
    assert((cpu.sr & M68K_SR_X) != 0);

    // 3. ASR.W #1, D0
    // D0 = 0x8000 (-32768). >> 1 = 0xC000 (-16384). Sign extend.
    // 1110 001 0 01 0 00 000 (0xE240). d=0(R), sz=1(W), type=0(AS)
    m68k_set_dr(&cpu, 0, 0x8000);
    m68k_write_16(&cpu, 4, 0xE240);
    m68k_step(&cpu);
    assert((cpu.d_regs[0] & 0xFFFF) == 0xC000);

    // 4. ROL.L #1, D0
    // D0 = 0x80000000. -> 0x00000001. C = 1.
    // 1110 001 1 10 0 11 000 (0xE398). d=1, sz=2(L), type=3(RO)
    m68k_set_dr(&cpu, 0, 0x80000000);
    m68k_write_16(&cpu, 6, 0xE398);
    m68k_step(&cpu);
    assert(cpu.d_regs[0] == 1);
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

void test_exceptions() {
    M68kCpu cpu;
    u8 memory[4096];  // Larger memory for vectors
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. Initial State
    // Set Stack Pointer slightly high
    cpu.a_regs[7] = 0x1000;
    cpu.pc = 0x200;
    cpu.sr = 0;  // User mode

    // 2. Setup Vector 32 (TRAP #0)
    // Vector 32 address is at 32*4 = 128 (0x80)
    // We want it to point to 0x400 (Handler)
    // Memory is byte array. Big Endian.
    memory[0x80] = 0x00;
    memory[0x81] = 0x00;
    memory[0x82] = 0x04;
    memory[0x83] = 0x00;  // 0x00000400

    // 3. Exec TRAP #0
    // Op: 0x4E40
    m68k_write_16(&cpu, 0x200, 0x4E40);
    m68k_step(&cpu);

    // Check:
    // PC should be 0x400
    assert(cpu.pc == 0x400);
    // SR should have S-bit set (Supervisor)
    assert((cpu.sr & M68K_SR_S) != 0);
    // Stack (A7) should have decremented by 6 (4 for PC + 2 for SR)
    assert(cpu.a_regs[7] == 0x1000 - 6);

    // Verify Stack content
    // Old SR at SP ? No, SR is pushed first?
    // Push PC (4 bytes), Push SR (2 bytes).
    // SP points to SR.
    u16 saved_sr = (memory[cpu.a_regs[7]] << 8) | memory[cpu.a_regs[7] + 1];
    assert((saved_sr & M68K_SR_S) == 0);  // Saved SR was 0
    u32 saved_pc = (memory[cpu.a_regs[7] + 2] << 24) | (memory[cpu.a_regs[7] + 3] << 16) |
                   (memory[cpu.a_regs[7] + 4] << 8) | memory[cpu.a_regs[7] + 5];
    assert(saved_pc == 0x202);  // Next instruction after TRAP

    // 4. Test RTE
    // Write RTE at 0x400
    m68k_write_16(&cpu, 0x400, 0x4E73);
    m68k_step(&cpu);

    // Check Restore
    assert(cpu.pc == 0x202);
    assert(cpu.a_regs[7] == 0x1000);
    assert((cpu.sr & M68K_SR_S) == 0);  // Back to User mode

    printf("Exception test passed!\n");
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

void test_dbcc_scc() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. Scc (SEQ)
    // D0 = 0. TST D0 -> Z=1.
    // SEQ D1. Should set D1.B to 0xFF.
    m68k_set_dr(&cpu, 0, 0);
    // TST.B D0: 0100 1010 00 000 000 (0x4A00)
    m68k_write_16(&cpu, 0, 0x4A00);

    // SEQ D1: 0101 0111 11 000 001 (0x57C1)
    // Cond 0111 (EQ=7). Mode 0 Reg 1.
    m68k_write_16(&cpu, 2, 0x57C1);

    m68k_step(&cpu);                    // TST
    assert((cpu.sr & M68K_SR_Z) != 0);  // Check Z is set
    m68k_step(&cpu);                    // SEQ

    assert((cpu.d_regs[1] & 0xFF) == 0xFF);

    // 2. DBcc (DBF - Always False - Decrement and Branch)
    // Loop 3 times.
    // MOVEQ #2, D0
    // L1: ...
    // DBF D0, L1

    // MOVEQ #2, D0: 0111 000 0 00000010 (0x7002)
    m68k_write_16(&cpu, 4, 0x7002);

    // Label L1 at PC=6.
    // ADDQ #1, D1: 0101 001 0 01 000 001 (0x5241). Add 1 to D1.
    m68k_write_16(&cpu, 6, 0x5241);

    // DBF D0, L1: 0101 0001 11 001 000 (0x51C8 + Disp)
    // Disp: Target (6) - (PC_of_DBF + 2).
    // PC_of_DBF is 8.
    // Target 6. 6 - (8+2) = -4 (0xFFFC).
    m68k_write_16(&cpu, 8, 0x51C8);
    m68k_write_16(&cpu, 10, 0xFFFC);

    cpu.pc = 4;
    m68k_set_dr(&cpu, 1, 0);

    // Exec MOVEQ (D0=2)
    m68k_step(&cpu);

    // Loop 1 (D0=2 -> 1)
    printf("Loop 1 start\n");
    fflush(stdout);
    m68k_step(&cpu);  // ADDQ (D1=1)
    m68k_step(&cpu);  // DBF (Branch)
    assert(cpu.pc == 6);
    assert((cpu.d_regs[0] & 0xFFFF) == 1);

    // Loop 2 (D0=1 -> 0)
    printf("Loop 2 start\n");
    fflush(stdout);
    m68k_step(&cpu);  // ADDQ (D1=2)
    m68k_step(&cpu);  // DBF (Branch)
    assert(cpu.pc == 6);
    assert((cpu.d_regs[0] & 0xFFFF) == 0);

    // Loop 3 (D0=0 -> -1)
    m68k_step(&cpu);  // ADDQ (D1=3)
    m68k_step(&cpu);  // DBF (No Branch, Fallthrough)
    assert(cpu.pc == 12);
    assert((cpu.d_regs[0] & 0xFFFF) == 0xFFFF);

    printf("Loop (DBcc/Scc) test passed!\n");
}

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
    cpu.sr |= M68K_SR_X;  // Set X

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

void test_misc_control() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. CHK.W D0, D1
    // D1 = 100. D0 = 50. 100 > 50 -> Exception.
    // CHK D0, D1: 0100 001 110 00 0 000 -> 0x4380
    cpu.d_regs[0] = 50;
    cpu.d_regs[1] = 100;

    // Set Vector 6 (CHK) to 0x400
    m68k_write_32(&cpu, 6 * 4, 0x400);

    m68k_write_16(&cpu, 0, 0x4380);
    m68k_step(&cpu);

    // Should have jumped to exception handler
    assert(cpu.pc == 0x400);

    printf("Misc Control (CHK) test passed!\n");
}

void test_interrupts() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. Setup
    cpu.pc = 0x100;
    cpu.a_regs[7] = 0x400;  // SP
    cpu.sr = 0x0000;        // User mode, IRQ mask 0 (Enable all)

    // 2. Setup Auto-Vector for Level 4
    // Vector 24 + 4 = 28. Address 28*4 = 112 (0x70).
    // Handler at 0x500.
    m68k_write_32(&cpu, 28 * 4, 0x500);

    // 3. Trigger IRQ Level 4
    m68k_set_irq(&cpu, 4);

    // 4. Step (Should Ack, Save State, Jump)
    // We need to execute at least one instruction cycle for the check to happen.
    // Let's write a NOP at 0x100.
    m68k_write_16(&cpu, 0x100, 0x4E71);

    m68k_step(&cpu);

    // Check Jump
    assert(cpu.pc == 0x500);

    // Check Supervisor Mode Set
    assert((cpu.sr & M68K_SR_S) != 0);

    // Check Mask Updated to 4
    assert(((cpu.sr >> 8) & 0x7) == 4);

    // Check Stack (Format: PC (4), SR (2))
    // SP was 0x400. Pushed 6 bytes -> 0x3FA.
    assert(cpu.a_regs[7] == 0x3FA);

    // Verify saved PC (0x100 - before NOP execution since IRQ is pre-fetch)
    u32 saved_pc = m68k_read_32(&cpu, cpu.a_regs[7] + 2);
    assert(saved_pc == 0x100);

    printf("Interrupt test passed!\n");
}
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

int main() {
    test_initialization();
    test_register_access();
    test_memory_access();
    test_fetch();
    test_move();
    test_arithmetic();
    test_control_flow();
    test_logic();
    test_data_manipulation();  // CLR, NEG, EXT, SWAP, MUL/DIV
    test_shift_rotate();
    test_exceptions();  // TRAP, RTE
    test_cmp_tst();
    test_lea_pea();
    test_stack_frame();        // LINK, UNLK
    test_dbcc_scc();           // DBcc, Scc
    test_addressing_mode_6();  // Index addressing
    test_exg();
    test_movem();  // MOVEM
    test_adda_suba();
    test_addx_subx();
    test_pcrel();

    // New Tests
    test_bcd();
    test_misc_control();
    test_interrupts();

    // Regression Tests for Bug Fixes
    test_regression_postinc_predec_byte();
    test_regression_reset_vectors();
    test_regression_jmp_dispatch();
    test_regression_div_by_zero();
    test_regression_addx_subx_zflag();
    test_regression_sbcd();
    test_regression_nbcd();
    test_regression_stop_privilege();
    test_regression_ext_dispatch();

    // Loader Tests
    extern void run_loader_tests();
    run_loader_tests();

    return 0;
}
