#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

// =============================================================================
// Integration Tests — Multi-instruction programs
// =============================================================================

void test_fibonacci() {
    // Program: compute fib(10) = 55
    // Uses a loop with MOVEQ, ADD, DBRA
    //
    // ASM:
    //   MOVEQ #0, D0       ; fib(n-2) = 0
    //   MOVEQ #1, D1       ; fib(n-1) = 1
    //   MOVEQ #8, D2       ; loop counter (10-2 iterations)
    // loop:
    //   MOVE.L D0, D3      ; temp = prev
    //   ADD.L D1, D0       ; prev = prev + curr (but we swap)
    //   MOVE.L D3, D1      ; curr = temp ... wait, better approach:
    //   ; Actually: D0 = a, D1 = b. We want a,b = b, a+b
    //   MOVE.L D1, D3      ; D3 = b (temp)
    //   ADD.L D0, D1       ; D1 = a + b (new b)
    //   MOVE.L D3, D0      ; D0 = old b (new a)
    //   DBRA D2, loop
    //   RTS

    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7] = 0x1000;

    u32 pc = 0x100;
    // MOVEQ #0, D0: 0x7000
    m68k_write_16(&cpu, pc, 0x7000);
    pc += 2;
    // MOVEQ #1, D1: 0x7201
    m68k_write_16(&cpu, pc, 0x7201);
    pc += 2;
    // MOVEQ #8, D2: 0x7408
    m68k_write_16(&cpu, pc, 0x7408);
    pc += 2;

    // loop (at pc=0x106):
    // MOVE.L D1, D3: 2601
    m68k_write_16(&cpu, pc, 0x2601);
    pc += 2;
    // ADD.L D0, D1: D280
    m68k_write_16(&cpu, pc, 0xD280);
    pc += 2;
    // MOVE.L D3, D0: 2003
    m68k_write_16(&cpu, pc, 0x2003);
    pc += 2;
    // DBRA D2, loop: 51CA FFF8
    // DBRA at 0x10C, disp at 0x10E. Target = (0x10E) + disp = 0x106
    // disp = 0x106 - 0x10E = -8 = 0xFFF8
    m68k_write_16(&cpu, pc, 0x51CA);
    pc += 2;
    m68k_write_16(&cpu, pc, 0xFFF8);
    pc += 2;
    // NOP (end marker)
    m68k_write_16(&cpu, pc, 0x4E71);

    cpu.pc = 0x100;
    int max_steps = 100;
    while (max_steps-- > 0) {
        if (cpu.pc == pc) break;  // Hit the NOP after DBRA falls through
        m68k_step(&cpu);
    }
    assert(max_steps > 0);        // Didn't time out
    assert(cpu.d_regs[1] == 55);  // fib(10) = 55 (D1 holds the new 'curr' value)

    printf("Integration: Fibonacci test passed!\n");
}

void test_string_copy() {
    // Program: copy "Hello" from 0x200 to 0x300 using (An)+ addressing
    //
    // ASM:
    //   LEA $200, A0
    //   LEA $300, A1
    // loop:
    //   MOVE.B (A0)+, (A1)+
    //   TST.B -1(A1)          ; test the byte we just wrote
    //   BNE loop
    //   NOP

    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7] = 0x1000;

    // Put "Hello\0" at address 0x200
    const char* hello = "Hello";
    for (int i = 0; i <= 5; i++) memory[0x200 + i] = hello[i];

    u32 pc = 0x100;
    // LEA $200.W, A0: 41F8 0200
    m68k_write_16(&cpu, pc, 0x41F8);
    pc += 2;
    m68k_write_16(&cpu, pc, 0x0200);
    pc += 2;
    // LEA $300.W, A1: 43F8 0300
    m68k_write_16(&cpu, pc, 0x43F8);
    pc += 2;
    m68k_write_16(&cpu, pc, 0x0300);
    pc += 2;

    // loop (at 0x108):
    // MOVE.B (A0)+, (A1)+: 12D8
    m68k_write_16(&cpu, pc, 0x12D8);
    pc += 2;
    // TST.B (A1) uses the byte we just wrote... but A1 already incremented.
    // Better approach: CMPI.B #0, -1(A1) or just test D0 after moving through D0.
    // Simpler: MOVE.B -1(A1), D0 to check; but let's use BNE on flags from MOVE.
    // Actually MOVE.B (A0)+,(A1)+ sets Z if byte was 0. So:
    // BNE loop: 66FC (loop is at 0x108, current PC = 0x10C, disp = -4 = 0xFC)
    m68k_write_16(&cpu, pc, 0x66FC);
    pc += 2;
    // NOP (end marker)
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
    // Program: set up TRAP #0 handler, call TRAP, verify handler ran
    //
    // Handler at 0x400:
    //   MOVEQ #42, D7       ; set a marker
    //   RTE
    //
    // Main at 0x100:
    //   MOVEQ #0, D7
    //   TRAP #0
    //   NOP                  ; should reach here after RTE

    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7] = 0x1000;

    // Set TRAP #0 vector (vector 32) to 0x400
    m68k_write_32(&cpu, 32 * 4, 0x400);

    // Handler at 0x400
    // MOVEQ #42, D7: 7E2A
    m68k_write_16(&cpu, 0x400, 0x7E2A);
    // RTE: 4E73
    m68k_write_16(&cpu, 0x402, 0x4E73);

    // Main program at 0x100
    // MOVEQ #0, D7: 7E00
    m68k_write_16(&cpu, 0x100, 0x7E00);
    // TRAP #0: 4E40
    m68k_write_16(&cpu, 0x102, 0x4E40);
    // NOP (end marker): 4E71
    m68k_write_16(&cpu, 0x104, 0x4E71);

    cpu.pc = 0x100;
    int max_steps = 20;
    while (max_steps-- > 0) {
        if (cpu.pc == 0x104) break;  // Reached NOP after RTE
        m68k_step(&cpu);
    }
    assert(max_steps > 0);
    assert(cpu.d_regs[7] == 42);  // Handler set D7 = 42
    assert(cpu.pc == 0x104);      // RTE returned to correct address

    printf("Integration: Exception handler test passed!\n");
}
