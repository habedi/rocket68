#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

void test_control_flow(void) {
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
    assert(cpu.d_regs[0].l == 2);

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

    assert(m68k_read_32(&cpu, cpu.a_regs[7].l) == 4);

    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 10);

    m68k_step(&cpu);
    assert(cpu.pc == 4);

    m68k_step(&cpu);
    assert(cpu.d_regs[0].l == 5);

    printf("Control Flow test passed!\n");
}

void test_dbcc_scc(void) {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_dr(&cpu, 0, 0);

    m68k_write_16(&cpu, 0, 0x4A00);

    m68k_write_16(&cpu, 2, 0x57C1);

    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) != 0);
    m68k_step(&cpu);

    assert((cpu.d_regs[1].l & 0xFF) == 0xFF);

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
    assert((cpu.d_regs[0].l & 0xFFFF) == 1);

    printf("Loop 2 start\n");
    fflush(stdout);
    m68k_step(&cpu);
    m68k_step(&cpu);
    assert(cpu.pc == 6);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0);

    m68k_step(&cpu);
    m68k_step(&cpu);
    assert(cpu.pc == 12);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0xFFFF);

    printf("Loop (DBcc/Scc) test passed!\n");
}

void test_misc_control(void) {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0].l = 50;
    cpu.d_regs[1].l = 100;

    m68k_write_32(&cpu, 6 * 4, 0x400);

    m68k_write_16(&cpu, 0, 0x4380);
    m68k_step(&cpu);

    assert(cpu.pc == 0x400);

    printf("Misc Control (CHK) test passed!\n");
}

void test_exceptions(void) {
    M68kCpu cpu;
    u8 memory[4096];
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7].l = 0x1000;
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

    assert(cpu.a_regs[7].l == 0x1000 - 6);

    u16 saved_sr = (memory[cpu.a_regs[7].l] << 8) | memory[cpu.a_regs[7].l + 1];
    assert((saved_sr & M68K_SR_S) == 0);
    u32 saved_pc = (memory[cpu.a_regs[7].l + 2] << 24) | (memory[cpu.a_regs[7].l + 3] << 16) |
                   (memory[cpu.a_regs[7].l + 4] << 8) | memory[cpu.a_regs[7].l + 5];
    assert(saved_pc == 0x202);

    m68k_write_16(&cpu, 0x400, 0x4E73);
    m68k_step(&cpu);

    assert(cpu.pc == 0x202);
    assert(cpu.a_regs[7].l == 0x1000);
    assert((cpu.sr & M68K_SR_S) == 0);

    printf("Exception test passed!\n");
}

void test_vbr_exception_base(void) {
    M68kCpu cpu;
    u8 memory[8192];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.sr = 0x2700; /* supervisor, so MOVEC is allowed */
    m68k_set_model(&cpu, M68K_MODEL_68010);

    /* Relocate the vector table to 0x800 with MOVEC D0, VBR. */
    cpu.d_regs[0].l = 0x800;
    m68k_write_16(&cpu, 0x200, 0x4E7B);
    m68k_write_16(&cpu, 0x202, 0x0801);
    cpu.pc = 0x200;
    m68k_step(&cpu);
    assert(cpu.vbr == 0x800);

    /* TRAP #0 (vector 32) must fetch its handler from VBR + 0x80. */
    m68k_write_32(&cpu, 0x080, 0x400); /* old table: wrong handler */
    m68k_write_32(&cpu, 0x880, 0x600); /* relocated table: right handler */
    m68k_write_16(&cpu, 0x204, 0x4E40);
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    /* After m68k_reset the base must return to zero. */
    m68k_reset(&cpu);
    assert(cpu.vbr == 0);

    printf("VBR exception base test passed!\n");
}

void test_address_error_frame_pc(void) {
    M68kCpu cpu;
    u8 memory[8192];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* TST.w (d16, A5) with an odd effective address. Hardware pushes the
     * instruction address plus 2 in the address-error frame, per the
     * SingleStepTests corpus. */
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[5].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600); /* address error vector */
    m68k_write_16(&cpu, 0x100, 0x4A6D); /* TST.w (d16, A5) */
    m68k_write_16(&cpu, 0x102, 0x0010); /* d16 = 0x10, EA = 0x511 (odd) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[7].l == 0x1000 - 14); /* 14-byte group 0 frame */
    u32 frame_pc = ((u32)memory[0xFFC] << 24) | ((u32)memory[0xFFD] << 16) |
                   ((u32)memory[0xFFE] << 8) | memory[0xFFF];
    assert(frame_pc == 0x102);

    /* TST.w (A5)+ with an odd address: the postincrement is committed
     * before the fault, and the pushed PC is the instruction address
     * plus 2. */
    m68k_init(&cpu, memory, sizeof(memory));
    memset(memory, 0, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[5].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4A5D); /* TST.w (A5)+ */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[5].l == 0x503);
    frame_pc = ((u32)memory[0xFFC] << 24) | ((u32)memory[0xFFD] << 16) |
               ((u32)memory[0xFFE] << 8) | memory[0xFFF];
    assert(frame_pc == 0x102);

    /* TST.w -(A5) with an odd address: the predecrement is committed and
     * the pushed PC is the instruction address plus 4. */
    m68k_init(&cpu, memory, sizeof(memory));
    memset(memory, 0, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[5].l = 0x503;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4A65); /* TST.w -(A5) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[5].l == 0x501);
    frame_pc = ((u32)memory[0xFFC] << 24) | ((u32)memory[0xFFD] << 16) |
               ((u32)memory[0xFFE] << 8) | memory[0xFFF];
    assert(frame_pc == 0x104);

    printf("Address error frame PC test passed!\n");
}

void test_move_write_fault(void) {
    M68kCpu cpu;
    u8 memory[8192];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* MOVE.w D0, (A1) with an odd destination. The pushed PC is the
     * instruction address plus 4, and the pushed SR holds the updated
     * condition codes. */
    cpu.sr = 0x2700 | M68K_SR_C;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.d_regs[0].l = 0x8000; /* negative word: N set, C cleared */
    cpu.a_regs[1].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x3280); /* MOVE.w D0, (A1) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    u32 frame = 0x1000 - 14;
    u32 frame_pc = ((u32)memory[frame + 10] << 24) | ((u32)memory[frame + 11] << 16) |
                   ((u32)memory[frame + 12] << 8) | memory[frame + 13];
    assert(frame_pc == 0x104);
    u16 frame_sr = (u16)((memory[frame + 8] << 8) | memory[frame + 9]);
    assert((frame_sr & 0x1F) == M68K_SR_N);
    assert((cpu.sr & 0x1F) == M68K_SR_N);

    /* MOVE.l D0, (A1) with an odd destination leaves the condition codes
     * unchanged on the fault path. */
    m68k_init(&cpu, memory, sizeof(memory));
    memset(memory, 0, sizeof(memory));
    cpu.sr = 0x2700 | M68K_SR_C;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.d_regs[0].l = 0x80000000;
    cpu.a_regs[1].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x2280); /* MOVE.l D0, (A1) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert((cpu.sr & 0x1F) == M68K_SR_C);

    /* MOVE.w D0, -(A1): the frame IR holds the next prefetch word, not
     * the opcode, and the decrement commits. */
    m68k_init(&cpu, memory, sizeof(memory));
    memset(memory, 0, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.d_regs[0].l = 0x1234;
    cpu.a_regs[1].l = 0x503;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x3300); /* MOVE.w D0, -(A1) */
    m68k_write_16(&cpu, 0x102, 0xBEEF); /* next prefetch word */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[1].l == 0x501);
    u16 frame_ir = (u16)((memory[frame + 6] << 8) | memory[frame + 7]);
    assert(frame_ir == 0xBEEF);

    printf("MOVE write fault test passed!\n");
}

void test_rmw_fault_commits(void) {
    M68kCpu cpu;
    u8 memory[8192];

    /* ADDX.l -(A1), -(A2) with an odd source: the long read goes low
     * word first, the decrement does not commit, and the pushed PC is
     * the instruction address plus 4. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[1].l = 0x505;
    cpu.a_regs[2].l = 0x600;
    m68k_write_32(&cpu, 3 * 4, 0x700);
    m68k_write_16(&cpu, 0x100, 0xD589); /* ADDX.l -(A1), -(A2) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x700);
    assert(cpu.a_regs[1].l == 0x505);
    assert(cpu.a_regs[2].l == 0x600);
    u32 frame = 0x1000 - 14;
    u32 frame_pc = ((u32)memory[frame + 10] << 24) | ((u32)memory[frame + 11] << 16) |
                   ((u32)memory[frame + 12] << 8) | memory[frame + 13];
    assert(frame_pc == 0x104);
    /* The low word is read first, so the fault address is A1 - 2. */
    u32 frame_fa = ((u32)memory[frame + 2] << 24) | ((u32)memory[frame + 3] << 16) |
                   ((u32)memory[frame + 4] << 8) | memory[frame + 5];
    assert(frame_fa == 0x503);

    /* CMPM.l (A1)+, (A2)+ with an odd source commits half the
     * increment before the fault. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[1].l = 0x501;
    cpu.a_regs[2].l = 0x600;
    m68k_write_32(&cpu, 3 * 4, 0x700);
    m68k_write_16(&cpu, 0x100, 0xB589); /* CMPM.l (A1)+, (A2)+ */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x700);
    assert(cpu.a_regs[1].l == 0x503);
    assert(cpu.a_regs[2].l == 0x600);

    printf("RMW fault commit test passed!\n");
}

void test_odd_target_fault(void) {
    M68kCpu cpu;
    u8 memory[8192];

    /* JMP (A1) to an odd target: the fault address is the target, the
     * pushed PC is the instruction address plus 2, and the access is a
     * program-space read (SSW FC = 6 in supervisor mode). */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[1].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4ED1); /* JMP (A1) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    u32 frame = 0x1000 - 14;
    u32 frame_fa = ((u32)memory[frame + 2] << 24) | ((u32)memory[frame + 3] << 16) |
                   ((u32)memory[frame + 4] << 8) | memory[frame + 5];
    u32 frame_pc = ((u32)memory[frame + 10] << 24) | ((u32)memory[frame + 11] << 16) |
                   ((u32)memory[frame + 12] << 8) | memory[frame + 13];
    u16 frame_ssw = (u16)((memory[frame] << 8) | memory[frame + 1]);
    assert(frame_fa == 0x501);
    assert(frame_pc == 0x102);
    assert((frame_ssw & 0x1F) == 0x16); /* read, supervisor program */

    /* JSR (A1) to an odd target does not push the return address. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[1].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4E91); /* JSR (A1) */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[7].l == 0x1000 - 14); /* only the fault frame */

    /* DBF D0, <odd> suppresses the counter writeback on the fault. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.d_regs[0].l = 5;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x51C8); /* DBF D0, ... */
    m68k_write_16(&cpu, 0x102, 0x000B); /* target 0x10D, odd */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.d_regs[0].l == 5);

    /* UNLK A1 with an odd frame pointer leaves SP and A1 untouched. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.a_regs[1].l = 0x501;
    m68k_write_32(&cpu, 3 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4E59); /* UNLK A1 */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[1].l == 0x501);
    assert(cpu.a_regs[7].l == 0x1000 - 14);

    printf("Odd target fault test passed!\n");
}

void test_model_gating(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* The default model is the 68000, where MOVEC is an illegal
     * instruction and takes vector 4. */
    assert(m68k_get_model(&cpu) == M68K_MODEL_68000);
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    cpu.d_regs[0].l = 0x800;
    m68k_write_32(&cpu, 4 * 4, 0x600); /* illegal instruction vector */
    m68k_write_16(&cpu, 0x100, 0x4E7B); /* MOVEC D0, VBR */
    m68k_write_16(&cpu, 0x102, 0x0801);
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.vbr == 0);

    /* RTD is also illegal on the 68000. */
    cpu.a_regs[7].l = 0x1000;
    m68k_write_16(&cpu, 0x104, 0x4E74); /* RTD */
    m68k_write_16(&cpu, 0x106, 0x0004);
    cpu.pc = 0x104;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    /* On the 68010, MOVEC executes. */
    m68k_set_model(&cpu, M68K_MODEL_68010);
    assert(m68k_get_model(&cpu) == M68K_MODEL_68010);
    cpu.a_regs[7].l = 0x1000;
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.vbr == 0x800);

    printf("Model gating test passed!\n");
}

void test_68010_frames(void) {
    M68kCpu cpu;
    u8 memory[8192];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);

    /* TRAP #0 on the 68010 pushes an 8-byte format 0 frame: SR, PC, and
     * the format/vector word (vector offset 0x80). */
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 32 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4E40); /* TRAP #0 */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.a_regs[7].l == 0x1000 - 8);
    u16 fmt = (u16)((memory[0xFFE] << 8) | memory[0xFFF]);
    assert(fmt == 0x0080);

    /* RTE pops the full frame and returns. */
    m68k_write_16(&cpu, 0x600, 0x4E73);
    m68k_step(&cpu);
    assert(cpu.pc == 0x102);
    assert(cpu.a_regs[7].l == 0x1000);

    /* RTE with a nonzero format nibble takes the format error vector. */
    cpu.a_regs[7].l = 0x1000 - 8;
    m68k_write_16(&cpu, 0xFF8, 0x2700);
    m68k_write_32(&cpu, 0xFFA, 0x200);
    m68k_write_16(&cpu, 0xFFE, 0x8080); /* format 8 */
    m68k_write_32(&cpu, 14 * 4, 0x700); /* format error vector */
    m68k_write_16(&cpu, 0x300, 0x4E73);
    cpu.pc = 0x300;
    m68k_step(&cpu);
    assert(cpu.pc == 0x700);

    printf("68010 frame test passed!\n");
}

void test_68010_sr_ccr(void) {
    M68kCpu cpu;
    u8 memory[8192];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);

    /* MOVE from SR is privileged on the 68010. */
    cpu.sr = 0x0700; /* user mode */
    cpu.a_regs[7].l = 0x2000;
    cpu.usp = 0x2000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 8 * 4, 0x600); /* privilege violation vector */
    m68k_write_16(&cpu, 0x100, 0x40C0); /* MOVE SR, D0 */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    /* MOVE from CCR works on the 68010, in user mode too. */
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);
    cpu.sr = 0x0015; /* user mode, X Z C set */
    cpu.a_regs[7].l = 0x2000;
    cpu.usp = 0x2000;
    cpu.d_regs[0].l = 0xFFFFFFFF;
    m68k_write_16(&cpu, 0x100, 0x42C0); /* MOVE CCR, D0 */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x102);
    assert(cpu.d_regs[0].l == 0xFFFF0015);

    /* MOVE from CCR is illegal on the 68000. */
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 4 * 4, 0x700);
    m68k_write_16(&cpu, 0x100, 0x42C0);
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x700);

    /* MOVE from SR stays unprivileged on the 68000. */
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x0015;
    cpu.a_regs[7].l = 0x2000;
    cpu.usp = 0x2000;
    m68k_write_16(&cpu, 0x100, 0x40C0);
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x102);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0x0015);

    printf("68010 SR/CCR test passed!\n");
}

void test_decode_strictness(void) {
    M68kCpu cpu;
    u8 memory[4096];

    /* ORI.b with EA mode 7, register 5 is not a valid encoding and takes
     * the illegal instruction vector. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 4 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x003D); /* ORI.b #, <invalid> */
    m68k_write_16(&cpu, 0x102, 0x0001);
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    /* MOVE.w D0, <mode 7, register 2> is an invalid destination. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 4 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x35C0); /* MOVE.w D0, (d16,PC) dest */
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.d_regs[0].l == 0);

    /* MOVEC with an unknown control register is illegal on the 68010. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 4 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x4E7B); /* MOVEC D0, <invalid> */
    m68k_write_16(&cpu, 0x102, 0x0002);
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    /* MOVES with size code 3 is an invalid encoding. */
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;
    m68k_write_32(&cpu, 4 * 4, 0x600);
    m68k_write_16(&cpu, 0x100, 0x0EC0);
    m68k_write_16(&cpu, 0x102, 0x0000);
    cpu.pc = 0x100;
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    printf("Decode strictness test passed!\n");
}

void test_nop_bsr_rtr(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_16(&cpu, 0, 0x4E71);
    m68k_step(&cpu);
    assert(cpu.pc == 2);

    cpu.pc = 2;
    cpu.a_regs[7].l = 0x100;
    m68k_write_16(&cpu, 2, 0x6108);
    m68k_step(&cpu);
    assert(cpu.pc == 4 + 8);
    assert(cpu.a_regs[7].l == 0xFC);
    assert(m68k_read_32(&cpu, 0xFC) == 4);

    cpu.pc = 12;

    m68k_write_32(&cpu, cpu.a_regs[7].l - 4, 0x20);
    m68k_write_16(&cpu, cpu.a_regs[7].l - 6, 0x1F);
    cpu.a_regs[7].l -= 6;
    m68k_write_16(&cpu, 12, 0x4E77);
    m68k_step(&cpu);
    assert(cpu.pc == 0x20);
    assert(cpu.a_regs[7].l == 0xFC);
    assert((cpu.sr & 0xFF) == 0x1F);

    printf("NOP/BSR/RTR test passed!\n");
}

void test_movec(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);

    cpu.sr = M68K_SR_S;  // Supervisor mode

    // MOVEC D0, VBR (0x4E7B)
    cpu.d_regs[0].l = 0x800;
    m68k_write_16(&cpu, 0, 0x4E7B);
    m68k_write_16(&cpu, 2, 0x0801);  // D0 -> VBR (0x801)
    cpu.pc = 0;
    m68k_step(&cpu);
    assert(cpu.vbr == 0x800);

    // MOVEC VBR, D1 (0x4E7A)
    m68k_write_16(&cpu, 4, 0x4E7A);
    m68k_write_16(&cpu, 6, 0x1801);  // D1 <- VBR
    m68k_step(&cpu);
    assert(cpu.d_regs[1].l == 0x800);

    // MOVEC A0, SFC (0x000)
    cpu.a_regs[0].l = 0x123;
    m68k_write_16(&cpu, 8, 0x4E7B);
    m68k_write_16(&cpu, 10, 0x8000);  // A0 -> SFC (A=1, Reg=0, 0x000)
    m68k_step(&cpu);
    assert(cpu.sfc == 0x123);

    // MOVEC DFC, A1
    cpu.dfc = 0x456;
    m68k_write_16(&cpu, 12, 0x4E7A);
    m68k_write_16(&cpu, 14, 0x9001);  // A1 <- DFC (A=1, Reg=1, 0x001)
    m68k_step(&cpu);
    assert(cpu.a_regs[1].l == 0x456);

    // Privilege test (User mode)
    cpu.sr = 0;
    m68k_write_16(&cpu, 16, 0x4E7A);
    m68k_write_16(&cpu, 18, 0x1801);
    // Privilege violation vector (8) is fetched relative to VBR (0x800 here).
    m68k_write_32(&cpu, 0x820, 0x100);
    cpu.a_regs[7].l = 0x400;
    m68k_step(&cpu);
    assert(cpu.pc == 0x100);

    printf("MOVEC test passed!\n");
}

void test_trapv(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    // TRAPV -> 0x4E76
    cpu.sr = 0;  // V clear
    m68k_write_16(&cpu, 0, 0x4E76);
    cpu.pc = 0;
    m68k_step(&cpu);
    assert(cpu.pc == 2);  // No trap

    cpu.sr = M68K_SR_V;  // V set
    m68k_write_16(&cpu, 2, 0x4E76);
    m68k_write_32(&cpu, 0x1C, 0x100);  // Vector 7 (TRAPV)
    cpu.a_regs[7].l = 0x400;           // Setup stack
    cpu.vbr = 0;
    cpu.pc = 2;
    m68k_step(&cpu);
    assert(cpu.pc == 0x100);

    printf("TRAPV test passed!\n");
}

void test_rtd(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);

    // RTD #4 -> 0x4E74, 0x0004
    m68k_write_16(&cpu, 0, 0x4E74);
    m68k_write_16(&cpu, 2, 0x0004);

    cpu.a_regs[7].l = 0x100;
    m68k_write_32(&cpu, 0x100, 0x200);  // Return PC

    cpu.pc = 0;
    m68k_step(&cpu);

    assert(cpu.pc == 0x200);
    assert(cpu.a_regs[7].l == 0x108);

    printf("RTD test passed!\n");
}

void test_bkpt(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_model(&cpu, M68K_MODEL_68010);

    // BKPT #7 -> 0x484F
    m68k_write_16(&cpu, 0, 0x484F);
    m68k_write_32(&cpu, 0x10, 0x100);  // Vector 4 (Illegal Instruction)
    cpu.sr = M68K_SR_S;                // Start in supervisor mode
    cpu.a_regs[7].l = 0x400;           // Provide valid SSP
    cpu.pc = 0;

    m68k_step(&cpu);

    if (cpu.pc != 0x100) {
        printf("FAILED BKPT: pc is 0x%04X, expected 0x100\n", cpu.pc);
    }
    assert(cpu.pc == 0x100);

    printf("BKPT test passed!\n");
}
