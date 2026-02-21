#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

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

void test_exceptions() {
    M68kCpu cpu;
    u8 memory[4096];  // Larger memory for vectors
    m68k_init(&cpu, memory, sizeof(memory));

    // 1. Initial State
    // Set Stack Pointer slightly high
    cpu.a_regs[7] = 0x1000;
    cpu.ssp = 0x1000;  // SSP must match since TRAP switches to supervisor mode
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
