#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disasm.h"
#include "loader.h"
#include "m68k.h"
#include "test_m68k.h"

static u32 watched_bus_address = 0;
static int watched_bus_hits = 0;
static M68kCpu* nested_exception_cpu = NULL;

static int regression_wait_bus_watch(M68kCpu* cpu, u32 address, M68kSize size, bool is_write) {
    (void)cpu;
    (void)size;
    (void)is_write;
    if (address == watched_bus_address) {
        watched_bus_hits++;
    }
    return 0;
}

static void regression_nested_exception_cb(M68kCpu* cpu, u32 new_pc) {
    (void)cpu;
    (void)new_pc;
    if (nested_exception_cpu != NULL) {
        m68k_read_16(nested_exception_cpu, 0x1000);
    }
}

void test_regression_postinc_predec_byte(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[0].l = 0x100;
    memory[0x100] = 0x42;
    m68k_write_16(&cpu, 0, 0x1018);
    m68k_step(&cpu);

    assert((cpu.d_regs[0].l & 0xFF) == 0x42);
    assert(cpu.a_regs[0].l == 0x101);

    cpu.a_regs[1].l = 0x110;
    memory[0x10F] = 0x55;
    m68k_write_16(&cpu, 2, 0x1221);
    m68k_step(&cpu);

    assert((cpu.d_regs[1].l & 0xFF) == 0x55);
    assert(cpu.a_regs[1].l == 0x10F);

    cpu.a_regs[7].l = 0x200;
    memory[0x200] = 0xAA;
    m68k_write_16(&cpu, 4, 0x141F);
    m68k_step(&cpu);

    assert((cpu.d_regs[2].l & 0xFF) == 0xAA);
    assert(cpu.a_regs[7].l == 0x202);

    printf("Regression: Postinc/predec byte test passed!\n");
}

void test_regression_reset_vectors(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x00000400);
    m68k_write_32(&cpu, 0x04, 0x00000100);

    m68k_reset(&cpu);

    assert(cpu.a_regs[7].l == 0x400);
    assert(cpu.pc == 0x100);
    assert(cpu.sr == 0x2700);

    printf("Regression: Reset vectors test passed!\n");
}

void test_regression_jmp_dispatch(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[0].l = 0x200;
    m68k_write_16(&cpu, 0, 0x4ED0);
    m68k_step(&cpu);

    assert(cpu.pc == 0x200);

    cpu.pc = 0x10;
    m68k_write_16(&cpu, 0x10, 0x4EF8);
    m68k_write_16(&cpu, 0x12, 0x0100);
    m68k_step(&cpu);

    assert(cpu.pc == 0x100);

    printf("Regression: JMP dispatch test passed!\n");
}

void test_regression_div_by_zero(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.a_regs[7].l = 0x1000;

    m68k_write_32(&cpu, 5 * 4, 0x300);

    cpu.d_regs[0].l = 0;
    cpu.d_regs[1].l = 100;
    m68k_write_16(&cpu, 0, 0x82C0);
    m68k_step(&cpu);

    assert(cpu.pc == 0x300);

    printf("Regression: Div-by-zero exception test passed!\n");
}

void test_regression_addx_subx_zflag(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.sr |= M68K_SR_Z;
    cpu.sr |= M68K_SR_X;

    cpu.d_regs[0].l = 0;
    cpu.d_regs[1].l = 0;

    m68k_write_16(&cpu, 0, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1].l == 1);
    assert((cpu.sr & M68K_SR_Z) == 0);

    cpu.sr |= M68K_SR_Z;
    cpu.sr &= ~M68K_SR_X;

    cpu.d_regs[0].l = 0;
    cpu.d_regs[1].l = 0;
    m68k_write_16(&cpu, 2, 0xD380);
    m68k_step(&cpu);
    assert(cpu.d_regs[1].l == 0);
    assert((cpu.sr & M68K_SR_Z) != 0);

    cpu.sr &= ~M68K_SR_Z;
    cpu.sr &= ~M68K_SR_X;
    cpu.d_regs[0].l = 0;
    cpu.d_regs[1].l = 0;
    m68k_write_16(&cpu, 4, 0xD380);
    m68k_step(&cpu);
    assert((cpu.sr & M68K_SR_Z) == 0);

    printf("Regression: ADDX/SUBX Z-flag test passed!\n");
}

void test_regression_sbcd(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0].l = 0x01;
    cpu.d_regs[1].l = 0x10;
    cpu.sr &= ~M68K_SR_X;

    m68k_write_16(&cpu, 0, 0x8300);
    m68k_step(&cpu);
    assert((cpu.d_regs[1].l & 0xFF) == 0x09);

    cpu.d_regs[0].l = 0x01;
    cpu.d_regs[1].l = 0x00;
    cpu.sr &= ~M68K_SR_X;
    m68k_write_16(&cpu, 2, 0x8300);
    m68k_step(&cpu);
    assert((cpu.d_regs[1].l & 0xFF) == 0x99);
    assert((cpu.sr & M68K_SR_C) != 0);
    assert((cpu.sr & M68K_SR_X) != 0);

    printf("Regression: SBCD borrow test passed!\n");
}

void test_regression_nbcd(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0].l = 0x00;
    cpu.sr &= ~M68K_SR_X;

    m68k_write_16(&cpu, 0, 0x4800);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFF) == 0x00);
    assert((cpu.sr & M68K_SR_C) == 0);

    cpu.d_regs[0].l = 0x01;
    cpu.sr &= ~M68K_SR_X;
    m68k_write_16(&cpu, 2, 0x4800);
    m68k_step(&cpu);
    assert((cpu.d_regs[0].l & 0xFF) == 0x99);
    assert((cpu.sr & M68K_SR_C) != 0);

    printf("Regression: NBCD test passed!\n");
}

void test_regression_stop_privilege(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 8 * 4, 0x300);

    cpu.sr = 0x0000;
    cpu.pc = 0x100;
    cpu.a_regs[7].l = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E72);
    m68k_write_16(&cpu, 0x102, 0x2700);

    m68k_step(&cpu);

    assert(cpu.pc == 0x300);

    printf("Regression: STOP privilege test passed!\n");
}

void test_regression_ext_dispatch(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.d_regs[0].l = 0x00000080;

    m68k_write_16(&cpu, 0, 0x4880);
    m68k_step(&cpu);

    assert((cpu.d_regs[0].l & 0xFFFF) == 0xFF80);

    cpu.d_regs[1].l = 0x00008000;

    m68k_write_16(&cpu, 2, 0x48C1);
    m68k_step(&cpu);

    assert(cpu.d_regs[1].l == 0xFFFF8000);

    printf("Regression: EXT dispatch test passed!\n");
}

void test_regression_no_spurious_ea_read_on_write(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_wait_bus_callback(&cpu, regression_wait_bus_watch);

    cpu.a_regs[0].l = 0x200;
    watched_bus_address = 0x200;
    watched_bus_hits = 0;

    // MOVE.B #$12,(A0)
    m68k_write_16(&cpu, 0x0000, 0x10BC);
    m68k_write_16(&cpu, 0x0002, 0x0012);
    m68k_step(&cpu);

    assert(cpu.exception_thrown == 0);
    assert(memory[0x200] == 0x12);
    assert(watched_bus_hits == 1);

    m68k_set_wait_bus_callback(&cpu, NULL);
    printf("Regression: No spurious EA read on write test passed!\n");
}

void test_regression_stop_rte_resume_pc(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x100;
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    // STOP #$2700
    m68k_write_16(&cpu, 0x100, 0x4E72);
    m68k_write_16(&cpu, 0x102, 0x2700);
    // IRQ7 autovector handler: RTE
    m68k_write_32(&cpu, 31 * 4, 0x300);
    m68k_write_16(&cpu, 0x300, 0x4E73);

    m68k_step(&cpu);
    assert(cpu.stopped == true);
    assert(cpu.pc == 0x104);

    m68k_set_irq(&cpu, 7);
    m68k_step(&cpu);
    assert(cpu.stopped == false);
    assert(cpu.pc == 0x300);

    m68k_set_irq(&cpu, 0);
    m68k_step(&cpu);
    assert(cpu.pc == 0x104);

    printf("Regression: STOP/RTE resume PC test passed!\n");
}

void test_regression_execute_wakes_stopped_cpu_on_irq(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x100;
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    // STOP #$2700
    m68k_write_16(&cpu, 0x100, 0x4E72);
    m68k_write_16(&cpu, 0x102, 0x2700);
    m68k_write_32(&cpu, 31 * 4, 0x300);

    m68k_step(&cpu);
    assert(cpu.stopped == true);

    // Normalize cycle accounting before execute().
    m68k_end_timeslice(&cpu);

    m68k_set_irq(&cpu, 7);
    int cycles_used = m68k_execute(&cpu, 44);

    assert(cycles_used > 0);
    assert(cpu.stopped == false);
    assert(cpu.pc == 0x300);

    printf("Regression: execute() wakes stopped CPU on IRQ test passed!\n");
}

void test_regression_exception_guards_are_per_instance(void) {
    M68kCpu cpu1;
    M68kCpu cpu2;
    u8 memory1[256];
    u8 memory2[256];
    memset(memory1, 0, sizeof(memory1));
    memset(memory2, 0, sizeof(memory2));
    m68k_init(&cpu1, memory1, sizeof(memory1));
    m68k_init(&cpu2, memory2, sizeof(memory2));

    cpu1.sr = 0x2700;
    cpu2.sr = 0x2700;
    cpu1.a_regs[7].l = 0x80;
    cpu2.a_regs[7].l = 0x80;
    cpu1.ssp = 0x80;
    cpu2.ssp = 0x80;

    m68k_write_32(&cpu1, 2 * 4, 0x40);
    m68k_write_32(&cpu2, 2 * 4, 0x40);

    nested_exception_cpu = &cpu2;
    m68k_set_pc_changed_callback(&cpu1, regression_nested_exception_cb);
    m68k_read_16(&cpu1, 0x1000);

    m68k_set_pc_changed_callback(&cpu1, NULL);
    nested_exception_cpu = NULL;

    assert(cpu1.exception_thrown == 2);
    assert(cpu2.exception_thrown == 2);

    printf("Regression: Exception guards are per-instance test passed!\n");
}

void test_regression_masked_irq_does_not_exit_stop(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x100;
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E72);
    m68k_write_16(&cpu, 0x102, 0x2700);
    m68k_write_16(&cpu, 0x104, 0x4E71);

    m68k_step(&cpu);
    assert(cpu.stopped == true);
    assert(cpu.pc == 0x104);

    m68k_set_irq(&cpu, 1);
    m68k_step(&cpu);

    assert(cpu.stopped == true);
    assert(cpu.pc == 0x104);
    assert(cpu.exception_thrown == 0);

    printf("Regression: Masked IRQ does not exit STOP test passed!\n");
}

void test_regression_interrupt_stacks_original_sr(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x100;
    cpu.sr = 0x2000;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E71);
    m68k_write_32(&cpu, (24 + 4) * 4, 0x500);

    m68k_set_irq(&cpu, 4);
    m68k_step(&cpu);

    assert(cpu.pc == 0x500);
    assert(cpu.a_regs[7].l == 0x0FFA);

    u16 saved_sr = ((u16)memory[cpu.a_regs[7].l] << 8) | memory[cpu.a_regs[7].l + 1];
    assert(saved_sr == 0x2000);
    assert((cpu.sr & 0x2700) == 0x2400);

    printf("Regression: Interrupt stacks original SR test passed!\n");
}

void test_regression_clr_illegal_mode_traps(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x0000;
    cpu.sr = 0x2700;
    cpu.a_regs[0].l = 0x2222;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    m68k_write_32(&cpu, 4 * 4, 0x300);
    m68k_write_16(&cpu, 0x0000, 0x4248);

    m68k_step(&cpu);

    assert(cpu.exception_thrown == 4);
    assert(cpu.pc == 0x300);
    assert(m68k_read_16(&cpu, 0x0000) == 0x4248);

    printf("Regression: CLR illegal mode traps test passed!\n");
}

static void dummy_reset_cb(M68kCpu* cpu) { (void)cpu; }

void test_regression_set_context_preserves_callbacks(void) {
    M68kCpu cpu1;
    M68kCpu cpu2;
    u8 memory1[1024];
    u8 memory2[1024];
    memset(memory1, 0, sizeof(memory1));
    memset(memory2, 0, sizeof(memory2));
    m68k_init(&cpu1, memory1, sizeof(memory1));
    m68k_init(&cpu2, memory2, sizeof(memory2));

    m68k_set_reset_callback(&cpu2, dummy_reset_cb);

    cpu1.d_regs[0].l = 0xCAFE;
    cpu1.pc = 0x200;
    cpu1.sr = 0x2700;

    unsigned int ctx_size = m68k_context_size();
    u8* buf = malloc(ctx_size);
    m68k_get_context(&cpu1, buf);
    m68k_set_context(&cpu2, buf);
    free(buf);

    assert(cpu2.d_regs[0].l == 0xCAFE);
    assert(cpu2.pc == 0x200);
    assert(cpu2.memory == memory2);
    assert(cpu2.reset_cb == dummy_reset_cb);

    printf("Regression: set_context preserves callbacks test passed!\n");
}

void test_regression_ori_ccr_preserves_upper_bits(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.sr = 0x0000;
    // ORI to CCR: bits outside CCR (5-7) are ignored.
    m68k_write_16(&cpu, 0, 0x003C);
    m68k_write_16(&cpu, 2, 0x00E0);
    m68k_step(&cpu);
    assert((cpu.sr & 0x00FF) == 0x00);

    printf("Regression: ORI to CCR preserves upper bits test passed!\n");
}

void test_regression_eori_ccr_preserves_upper_bits(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.sr = 0x00FF;
    // EORI to CCR: bits outside CCR (5-7) are ignored.
    m68k_write_16(&cpu, 0, 0x0A3C);
    m68k_write_16(&cpu, 2, 0x00E0);
    m68k_step(&cpu);
    assert((cpu.sr & 0x00FF) == 0xFF);

    printf("Regression: EORI to CCR preserves upper bits test passed!\n");
}

void test_regression_divu_overflow_n_flag(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.a_regs[7].l = 0x1000;

    m68k_write_32(&cpu, 5 * 4, 0x300);

    // DIVU.W D0, D1: D1 / D0
    // D1 = 0x00020000 (131072), D0 = 1 => quotient = 131072 > 0xFFFF -> overflow
    cpu.d_regs[0].l = 1;
    cpu.d_regs[1].l = 0x00020000;
    cpu.sr = 0;
    // DIVU D0, D1: 1000 001 011 000 000 = 0x82C0
    m68k_write_16(&cpu, 0, 0x82C0);
    m68k_step(&cpu);

    assert((cpu.sr & M68K_SR_V) != 0);
    // 68000 sets N alongside V on divide overflow.
    assert((cpu.sr & M68K_SR_N) != 0);

    printf("Regression: DIVU overflow N flag test passed!\n");
}

void test_regression_divs_overflow_n_flag(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.a_regs[7].l = 0x1000;

    m68k_write_32(&cpu, 5 * 4, 0x300);

    // DIVS.W D0, D1: D1 / D0
    // D1 = 0x00020000 (131072 signed), D0 = 1 => quotient = 131072, overflows s16
    cpu.d_regs[0].l = 1;
    cpu.d_regs[1].l = 0x00020000;
    cpu.sr = 0;
    // DIVS D0, D1: 1000 001 111 000 000 = 0x83C0
    m68k_write_16(&cpu, 0, 0x83C0);
    m68k_step(&cpu);

    assert((cpu.sr & M68K_SR_V) != 0);
    // 68000 sets N alongside V on divide overflow.
    assert((cpu.sr & M68K_SR_N) != 0);

    printf("Regression: DIVS overflow N flag test passed!\n");
}

void test_regression_chk_only_modifies_n(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.a_regs[7].l = 0x1000;
    cpu.sr = 0x2700;

    m68k_write_32(&cpu, 6 * 4, 0x500);

    // CHK D0, D1: 0100 001 110 000 000 = 0x4380
    // D1 = 5 (src), D0 = 10 (bound) => 5 >= 0 && 5 <= 10 => no exception
    cpu.d_regs[0].l = 10;
    cpu.d_regs[1].l = 5;
    cpu.sr = 0x2700 | M68K_SR_Z | M68K_SR_V | M68K_SR_C;
    m68k_write_16(&cpu, 0, 0x4380);
    m68k_step(&cpu);

    // CHK updates N and clears Z/V/C while preserving X.
    assert((cpu.sr & M68K_SR_Z) == 0);
    assert((cpu.sr & M68K_SR_V) == 0);
    assert((cpu.sr & M68K_SR_C) == 0);
    assert((cpu.sr & M68K_SR_N) == 0);

    printf("Regression: CHK only modifies N test passed!\n");
}

void test_regression_dbcc_cycle_count(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* DBRA D0, loop back to start with D0=1 => two iterations */
    cpu.d_regs[0].l = 1;
    /* 51C8 FFFE = DBF D0, displacement -2 from ext word => target 0x0000 */
    m68k_write_16(&cpu, 0, 0x51C8);
    m68k_write_16(&cpu, 2, 0xFFFE);
    m68k_end_timeslice(&cpu);

    /* First iteration: branch taken (counter 1->0, not expired) = 10 cycles */
    int cycles1 = m68k_execute(&cpu, 10);
    assert(cycles1 == 10);
    assert(cpu.pc == 0x0000);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0);

    /* Second iteration: counter 0->FFFF, expired = 14 cycles */
    int cycles2 = m68k_execute(&cpu, 14);
    assert(cycles2 == 14);
    assert(cpu.pc == 0x0004);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0xFFFF);

    printf("Regression: DBcc cycle count test passed!\n");
}

void test_regression_bcc_not_taken_cycles(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* BEQ.B $+4 when Z=0 (not taken) should cost 8 cycles */
    cpu.sr = 0x2700 & ~M68K_SR_Z; /* Z clear */
    /* 6702 = BEQ.B $+4 (byte displacement 2) */
    m68k_write_16(&cpu, 0, 0x6702);
    m68k_write_16(&cpu, 2, 0x4E71); /* NOP as fallthrough */
    m68k_end_timeslice(&cpu);

    int cycles = m68k_execute(&cpu, 8);
    assert(cycles == 8);
    assert(cpu.pc == 0x0002); /* fell through, not branched */

    printf("Regression: Bcc not-taken cycles test passed!\n");
}

void test_regression_bsr_cycles(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    /* BSR.B $+8 (byte displacement 6) should cost 18 cycles */
    /* 6106 = BSR.B displacement=6 => target = PC+2+6 = 8 */
    m68k_write_16(&cpu, 0, 0x6106);
    m68k_write_16(&cpu, 8, 0x4E71); /* NOP at target */
    m68k_end_timeslice(&cpu);

    int cycles = m68k_execute(&cpu, 18);
    assert(cycles == 18);
    assert(cpu.pc == 0x0008); /* branched to target */

    printf("Regression: BSR cycles test passed!\n");
}

void test_regression_scc_register_cycles(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* ST D0 (condition true = always) should cost 6 cycles */
    /* 50C0 = ST D0 */
    m68k_write_16(&cpu, 0, 0x50C0);
    m68k_end_timeslice(&cpu);
    int cycles_true = m68k_execute(&cpu, 6);
    assert(cycles_true == 6);
    assert((cpu.d_regs[0].l & 0xFF) == 0xFF);

    /* SF D1 (condition false = never) should cost 4 cycles */
    /* 51C1 = SF D1 */
    m68k_write_16(&cpu, 2, 0x51C1);
    m68k_end_timeslice(&cpu);
    int cycles_false = m68k_execute(&cpu, 4);
    assert(cycles_false == 4);
    assert((cpu.d_regs[1].l & 0xFF) == 0x00);

    printf("Regression: Scc register cycles test passed!\n");
}

void test_regression_addq_long_dn_cycles(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* ADDQ.L #1,D0 should cost 8 cycles (long to Dn) */
    /* 5280 = ADDQ.L #1,D0 */
    m68k_write_16(&cpu, 0, 0x5280);
    m68k_end_timeslice(&cpu);
    int cycles = m68k_execute(&cpu, 8);
    assert(cycles == 8);
    assert(cpu.d_regs[0].l == 1);

    printf("Regression: ADDQ.L Dn cycles test passed!\n");
}

void test_regression_subq_an_cycles(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.a_regs[0].l = 0x100;

    /* SUBQ.W #1,A0 should cost 8 cycles (any size to An) */
    /* 5348 = SUBQ.W #1,A0 */
    m68k_write_16(&cpu, 0, 0x5348);
    m68k_end_timeslice(&cpu);
    int cycles = m68k_execute(&cpu, 8);
    assert(cycles == 8);
    assert(cpu.a_regs[0].l == 0xFF);

    printf("Regression: SUBQ An cycles test passed!\n");
}

void test_regression_shift_register_cycles(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.d_regs[0].l = 0xFF;

    /* ASL.W #4,D0 should cost 6 + 2*4 = 14 cycles */
    /* E940 = ASL.W #4,D0 */
    m68k_write_16(&cpu, 0, 0xE940);
    m68k_end_timeslice(&cpu);
    int cycles = m68k_execute(&cpu, 14);
    assert(cycles == 14);
    assert((cpu.d_regs[0].l & 0xFFFF) == 0x0FF0);

    printf("Regression: Shift register cycles test passed!\n");
}

static int clr_read_hit = 0;
static u8 clr_read_value = 0;

static u8 clr_test_read8(M68kCpu* cpu, u32 address) {
    (void)cpu;
    if (address == 0x200) {
        clr_read_hit++;
        return clr_read_value;
    }
    return 0;
}

static void clr_test_write8(M68kCpu* cpu, u32 address, u8 value) {
    (void)cpu;
    (void)address;
    (void)value;
}

static u16 clr_test_read16(M68kCpu* cpu, u32 address) {
    (void)cpu;
    if (address == 0x200) {
        clr_read_hit++;
        return clr_read_value;
    }
    /* Instruction fetches from ROM area */
    if (address < 0x100) {
        return (u16)(cpu->memory[address] << 8) | cpu->memory[address + 1];
    }
    return 0;
}

static void clr_test_write16(M68kCpu* cpu, u32 address, u16 value) {
    (void)cpu;
    (void)address;
    (void)value;
}

void test_regression_clr_memory_reads_before_write(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_read8_callback(&cpu, clr_test_read8);
    m68k_set_write8_callback(&cpu, clr_test_write8);
    m68k_set_read16_callback(&cpu, clr_test_read16);
    m68k_set_write16_callback(&cpu, clr_test_write16);

    /* CLR.B $200 should read address $200 before writing 0 (68000 behavior) */
    /* 4239 0000 0200 = CLR.B $00000200.L */
    cpu.memory[0] = 0x42;
    cpu.memory[1] = 0x39;
    cpu.memory[2] = 0x00;
    cpu.memory[3] = 0x00;
    cpu.memory[4] = 0x02;
    cpu.memory[5] = 0x00;
    clr_read_hit = 0;
    clr_read_value = 0x42;

    m68k_step(&cpu);

    /* The read callback should have been hit for the read-before-write */
    assert(clr_read_hit >= 1);

    m68k_set_read8_callback(&cpu, NULL);
    m68k_set_write8_callback(&cpu, NULL);
    m68k_set_read16_callback(&cpu, NULL);
    m68k_set_write16_callback(&cpu, NULL);

    printf("Regression: CLR memory reads before write test passed!\n");
}

void test_regression_movem_predec_writes_initial_an(void) {
    M68kCpu cpu;
    u8 memory[4096];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* MOVEM.L A0,-(A0): should write the INITIAL value of A0, not decremented */
    cpu.a_regs[0].l = 0x200;

    /* 48E0 0100 = MOVEM.L A0,-(A0) — register mask 0x0100 = bit 8 = A0 reversed */
    /* In predecrement mode the mask is reversed: bit 0=A7 ... bit 7=A0 bit 8=D7 ... bit 15=D0 */
    /* A0 in reversed mask = bit 7 = 0x0080 */
    m68k_write_16(&cpu, 0, 0x48E0);
    m68k_write_16(&cpu, 2, 0x0080);
    m68k_step(&cpu);

    /* A0 should now be 0x1FC (decremented by 4 for one long register) */
    assert(cpu.a_regs[0].l == 0x1FC);

    /* The value written to memory at 0x1FC should be the INITIAL 0x200, not 0x1FC */
    u32 stored = ((u32)memory[0x1FC] << 24) | ((u32)memory[0x1FD] << 16) |
                 ((u32)memory[0x1FE] << 8) | (u32)memory[0x1FF];
    assert(stored == 0x200);

    printf("Regression: MOVEM predec writes initial An test passed!\n");
}

void test_regression_move_predec_dest_cycles(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    int before, after, cycles;

    /* MOVE.W D0,-(A0): 8 M68K cycles per M68000 manual.
     * The -(An) destination overlaps the predecrement with the write,
     * saving 2 cycles vs the generic EA table cost. */
    cpu.d_regs[0].l = 0x1234;
    cpu.a_regs[0].l = 0x200;

    /* 3100 = MOVE.W D0,-(A0) */
    m68k_write_16(&cpu, 0, 0x3100);

    before = m68k_cycles_run(&cpu);
    m68k_step(&cpu);
    after = m68k_cycles_run(&cpu);
    cycles = after - before;
    assert(cycles == 8);
    assert(cpu.a_regs[0].l == 0x1FE);
    assert(m68k_read_16(&cpu, 0x1FE) == 0x1234);

    /* MOVE.L D0,-(A0): 12 M68K cycles per M68000 manual. */
    cpu.pc = 0;
    cpu.a_regs[0].l = 0x200;
    m68k_write_16(&cpu, 0, 0x2100);

    before = m68k_cycles_run(&cpu);
    m68k_step(&cpu);
    cycles = m68k_cycles_run(&cpu) - before;
    assert(cycles == 12);

    /* MOVE.W (A1),-(A0): 12 M68K cycles per M68000 manual.
     * Opcode: 00 11 000 100 010 001 = 0x3111 */
    cpu.pc = 0;
    cpu.a_regs[0].l = 0x200;
    cpu.a_regs[1].l = 0x100;
    m68k_write_16(&cpu, 0x100, 0x5678);
    m68k_write_16(&cpu, 0, 0x3111);

    before = m68k_cycles_run(&cpu);
    m68k_step(&cpu);
    cycles = m68k_cycles_run(&cpu) - before;
    assert(cycles == 12);

    printf("Regression: MOVE predecrement destination cycle count test passed!\n");
}

void test_regression_write_oob_triggers_bus_error(void) {
    M68kCpu cpu;
    u8 memory[256];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x80;

    m68k_write_32(&cpu, 2 * 4, 0x40);

    // Write to address beyond memory_size should trigger bus error
    m68k_write_8(&cpu, 0x200, 0x42);
    assert(cpu.exception_thrown == 2);

    printf("Regression: Write OOB triggers bus error test passed!\n");
}

static u32 captured_fetch_addr = 0;

static u16 regression_capture_fetch_cb(M68kCpu* cpu, u32 address) {
    (void)cpu;
    captured_fetch_addr = address;
    return 0x4E71; /* NOP */
}

static u16 regression_flat_read16_cb(M68kCpu* cpu, u32 address) {
    if (address + 1 < cpu->memory_size) {
        return (u16)((cpu->memory[address] << 8) | cpu->memory[address + 1]);
    }
    return 0;
}

static int regression_wait_two_cycles(M68kCpu* cpu, u32 address, M68kSize size, bool is_write) {
    (void)cpu;
    (void)address;
    (void)size;
    (void)is_write;
    return 2;
}

void test_regression_group0_abort_after_prior_exception(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x8000);   /* initial SSP */
    m68k_write_32(&cpu, 0x04, 0x1000);   /* initial PC */
    m68k_write_32(&cpu, 3 * 4, 0x4000);  /* address error handler */
    m68k_write_32(&cpu, 32 * 4, 0x2000); /* TRAP #0 handler */
    m68k_reset(&cpu);

    m68k_write_16(&cpu, 0x1000, 0x4E40); /* TRAP #0 */
    m68k_write_16(&cpu, 0x2000, 0x3010); /* MOVE.W (A0),D0 with odd A0 */
    cpu.a_regs[0].l = 0x2001;
    cpu.d_regs[0].l = 0xDEADBEEF;

    m68k_step(&cpu);
    assert(cpu.pc == 0x2000);

    /* The address error must abort the MOVE even though an earlier
     * exception was taken in a previous step. */
    m68k_step(&cpu);
    assert(cpu.pc == 0x4000);
    assert(cpu.d_regs[0].l == 0xDEADBEEF);

    printf("Regression: group-0 abort after prior exception test passed!\n");
}

void test_regression_illegal_4afc_traps(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x8000);
    m68k_write_32(&cpu, 0x04, 0x1000);
    m68k_write_32(&cpu, 4 * 4, 0x3000); /* illegal instruction handler */
    m68k_reset(&cpu);

    m68k_write_16(&cpu, 0x1000, 0x4AFC); /* ILLEGAL */
    m68k_write_16(&cpu, 0x1002, 0x00AB);
    cpu.d_regs[0].l = 0x11111111;
    m68k_step(&cpu);

    assert(cpu.pc == 0x3000);
    assert(cpu.d_regs[0].l == 0x11111111);
    /* The stacked return address is the ILLEGAL instruction itself. */
    assert(m68k_read_32(&cpu, cpu.a_regs[7].l + 2) == 0x1000);

    printf("Regression: ILLEGAL 0x4AFC traps test passed!\n");
}

void test_regression_tas_invalid_ea_traps(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x8000);
    m68k_write_32(&cpu, 0x04, 0x1000);
    m68k_write_32(&cpu, 4 * 4, 0x3000);
    m68k_reset(&cpu);

    m68k_write_16(&cpu, 0x1000, 0x4AC8); /* TAS A0 (invalid EA) */
    cpu.a_regs[0].l = 0x12345678;
    u8 before = memory[0];
    m68k_step(&cpu);

    assert(cpu.pc == 0x3000);
    assert(memory[0] == before); /* no stray write to address 0 */

    printf("Regression: TAS invalid EA traps test passed!\n");
}

void test_regression_set_context_preserves_memory_callbacks(void) {
    M68kCpu cpu1;
    M68kCpu cpu2;
    u8 memory1[1024];
    u8 memory2[1024];
    memset(memory1, 0, sizeof(memory1));
    memset(memory2, 0, sizeof(memory2));
    m68k_init(&cpu1, memory1, sizeof(memory1));
    m68k_init(&cpu2, memory2, sizeof(memory2));

    m68k_set_read16_callback(&cpu2, regression_flat_read16_cb);
    m68k_set_write8_callback(&cpu2, NULL);

    unsigned int ctx_size = m68k_context_size();
    u8* buf = malloc(ctx_size);
    m68k_get_context(&cpu1, buf); /* cpu1 has no callbacks installed */
    m68k_set_context(&cpu2, buf);
    free(buf);

    assert(cpu2.read16_cb == regression_flat_read16_cb);

    printf("Regression: set_context preserves memory callbacks test passed!\n");
}

void test_regression_fetch_callback_gets_masked_address(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_set_read16_callback(&cpu, regression_capture_fetch_cb);
    cpu.pc = 0xFF000100; /* upper byte must be masked off the bus */
    captured_fetch_addr = 0xFFFFFFFF;
    m68k_step(&cpu);

    assert(captured_fetch_addr == 0x00000100);

    printf("Regression: fetch callback gets masked address test passed!\n");
}

void test_regression_movem_mem_to_reg_no_spurious_read(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_16(&cpu, 0x1000, 0x4CD0); /* MOVEM.L (A0),D0-D1 */
    m68k_write_16(&cpu, 0x1002, 0x0003);
    m68k_write_32(&cpu, 0x2000, 0xAAAAAAAA);
    m68k_write_32(&cpu, 0x2004, 0xBBBBBBBB);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;

    watched_bus_address = 0x2000;
    watched_bus_hits = 0;
    m68k_set_wait_bus_callback(&cpu, regression_wait_bus_watch);
    m68k_step(&cpu);
    m68k_set_wait_bus_callback(&cpu, NULL);

    assert(cpu.d_regs[0].l == 0xAAAAAAAA);
    assert(cpu.d_regs[1].l == 0xBBBBBBBB);
    assert(cpu.a_regs[0].l == 0x2000);
    /* The first element must be read exactly once. */
    assert(watched_bus_hits == 1);

    printf("Regression: MOVEM mem-to-reg no spurious read test passed!\n");
}

void test_regression_odd_data_access_with_callback_raises_address_error(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x8000);
    m68k_write_32(&cpu, 0x04, 0x1000);
    m68k_write_32(&cpu, 3 * 4, 0x4000); /* address error handler */
    m68k_reset(&cpu);

    m68k_write_16(&cpu, 0x1000, 0x3010); /* MOVE.W (A0),D0 with odd A0 */
    cpu.a_regs[0].l = 0x2001;
    cpu.d_regs[0].l = 0xDEADBEEF;

    m68k_set_read16_callback(&cpu, regression_flat_read16_cb);
    m68k_step(&cpu);
    m68k_set_read16_callback(&cpu, NULL);

    assert(cpu.pc == 0x4000);
    assert(cpu.d_regs[0].l == 0xDEADBEEF);

    printf("Regression: odd data access with callback raises address error test passed!\n");
}

void test_regression_alu_long_abs_source_cycles(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* ADD.L $2000.W,D0: 6 base + 12 EA = 18 cycles on hardware. */
    m68k_write_16(&cpu, 0x1000, 0xD0B8);
    m68k_write_16(&cpu, 0x1002, 0x2000);
    cpu.pc = 0x1000;
    m68k_end_timeslice(&cpu);

    int cycles = m68k_execute(&cpu, 18);
    assert(cpu.pc == 0x1004);
    assert(cycles == 18);

    printf("Regression: ADD.L absolute source cycles test passed!\n");
}

void test_regression_control_flow_ea_cycles(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* LEA (A0),A1 = 4 cycles */
    m68k_write_16(&cpu, 0x1000, 0x43D0);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;
    m68k_end_timeslice(&cpu);
    int cycles = m68k_execute(&cpu, 4);
    assert(cpu.a_regs[1].l == 0x2000);
    assert(cycles == 4);

    /* JMP 256(A0) = 10 cycles */
    memset(memory, 0, sizeof(memory));
    m68k_write_16(&cpu, 0x1000, 0x4EE8);
    m68k_write_16(&cpu, 0x1002, 0x0100);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;
    m68k_end_timeslice(&cpu);
    cycles = m68k_execute(&cpu, 10);
    assert(cpu.pc == 0x2100);
    assert(cycles == 10);

    /* JSR (A0) = 16 cycles */
    memset(memory, 0, sizeof(memory));
    m68k_write_16(&cpu, 0x1000, 0x4E90);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;
    cpu.a_regs[7].l = 0x8000;
    m68k_end_timeslice(&cpu);
    cycles = m68k_execute(&cpu, 16);
    assert(cpu.pc == 0x2000);
    assert(cycles == 16);

    /* PEA (A0) = 12 cycles */
    memset(memory, 0, sizeof(memory));
    m68k_write_16(&cpu, 0x1000, 0x4850);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;
    cpu.a_regs[7].l = 0x8000;
    m68k_end_timeslice(&cpu);
    cycles = m68k_execute(&cpu, 12);
    assert(m68k_read_32(&cpu, 0x7FFC) == 0x2000);
    assert(cycles == 12);

    printf("Regression: control flow EA cycles test passed!\n");
}

void test_regression_disasm_cmpi_eori(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_16(&cpu, 0x100, 0x0C40); /* CMPI.W #$1234,D0 */
    m68k_write_16(&cpu, 0x102, 0x1234);
    m68k_write_16(&cpu, 0x104, 0x0A40); /* EORI.W #$1234,D0 */
    m68k_write_16(&cpu, 0x106, 0x1234);

    char buf[128];
    m68k_disasm(&cpu, 0x100, buf, sizeof(buf));
    assert(strstr(buf, "CMPI") != NULL);
    m68k_disasm(&cpu, 0x104, buf, sizeof(buf));
    assert(strstr(buf, "EORI") != NULL);

    printf("Regression: disasm CMPI/EORI test passed!\n");
}

void test_regression_disasm_is_side_effect_free(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_16(&cpu, 0x100, 0x4E71); /* NOP */
    m68k_set_wait_bus_callback(&cpu, regression_wait_two_cycles);

    char buf[128];
    int cycles_before = m68k_cycles_remaining(&cpu);
    m68k_disasm(&cpu, 0x100, buf, sizeof(buf));
    assert(m68k_cycles_remaining(&cpu) == cycles_before);
    m68k_set_wait_bus_callback(&cpu, NULL);

    /* Disassembling past the end of memory must not fault the CPU. */
    u32 sp_before = cpu.a_regs[7].l;
    u32 pc_before = cpu.pc;
    m68k_disasm(&cpu, 0x20000, buf, sizeof(buf));
    assert(cpu.a_regs[7].l == sp_before);
    assert(cpu.pc == pc_before);
    assert(cpu.exception_thrown == 0);

    printf("Regression: disasm is side-effect free test passed!\n");
}

void test_regression_shift_memory_invalid_mode_traps(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    m68k_write_32(&cpu, 0x00, 0x8000);
    m68k_write_32(&cpu, 0x04, 0x1000);
    m68k_write_32(&cpu, 4 * 4, 0x3000);
    m68k_reset(&cpu);

    m68k_write_16(&cpu, 0x1000, 0xE0C0); /* memory-form ASR with mode 0 (invalid) */
    cpu.d_regs[0].l = 0xFFFF;
    u8 before0 = memory[0];
    u8 before1 = memory[1];
    m68k_step(&cpu);

    assert(cpu.pc == 0x3000);
    assert(memory[0] == before0); /* no stray write to address 0 */
    assert(memory[1] == before1);

    printf("Regression: shift memory invalid mode traps test passed!\n");
}

static u32 captured_pc_change = 0;

static void regression_pc_changed_cb(M68kCpu* cpu, u32 new_pc) {
    (void)cpu;
    captured_pc_change = new_pc;
}

static int regression_autovector_ack_cb(M68kCpu* cpu, int level) {
    (void)cpu;
    (void)level;
    return (int)M68K_INT_ACK_AUTOVECTOR;
}

void test_regression_loader_oob_is_harmless(void) {
    M68kCpu cpu;
    u8 memory[256];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    cpu.sr = 0x2700;
    cpu.a_regs[7].l = 0x80;
    m68k_set_pc_changed_callback(&cpu, regression_pc_changed_cb);
    captured_pc_change = 0;

    const char* filename = "test_oob.srec";
    FILE* f = fopen(filename, "w");
    assert(f != NULL);
    fprintf(f, "S1040080AB00\n");   /* one byte 0xAB at 0x80 (in range) */
    fprintf(f, "S10502001234FF\n"); /* two bytes at 0x200 (out of range) */
    fprintf(f, "S903001000\n");     /* entry point 0x10 */
    fclose(f);

    bool success = m68k_load_srec(&cpu, filename);
    remove(filename);

    assert(success);
    assert(memory[0x80] == 0xAB);
    /* Out-of-range records must not run the CPU bus-error machinery. */
    assert(cpu.exception_thrown == 0);
    assert(cpu.a_regs[7].l == 0x80);
    /* The entry point goes through m68k_set_pc, so the hook fires. */
    assert(cpu.pc == 0x10);
    assert(captured_pc_change == 0x10);

    const char* binname = "test_oob.bin";
    f = fopen(binname, "wb");
    assert(f != NULL);
    u8 data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    success = m68k_load_bin(&cpu, binname, 0xFC); /* runs past the 256-byte end */
    remove(binname);

    assert(success);
    assert(memory[0xFC] == 1);
    assert(memory[0xFF] == 4);
    assert(cpu.exception_thrown == 0);
    assert(cpu.a_regs[7].l == 0x80);

    printf("Regression: loader out-of-range is harmless test passed!\n");
}

void test_regression_irq_level_triggered_with_ack_callback(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_int_ack_callback(&cpu, regression_autovector_ack_cb);

    cpu.pc = 0x100;
    cpu.sr = 0x2000; /* supervisor, mask 0 */
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E71); /* NOP */
    m68k_write_16(&cpu, 0x102, 0x4E71); /* NOP */
    m68k_write_32(&cpu, 28 * 4, 0x500); /* level 4 autovector */
    m68k_write_16(&cpu, 0x500, 0x4E73); /* RTE */

    m68k_set_irq(&cpu, 4);
    m68k_step(&cpu);
    assert(cpu.pc == 0x500);
    /* With an INT ACK callback the line is level-held until the host
     * clears it. */
    assert(cpu.irq_level == 4);

    m68k_step(&cpu); /* RTE, mask drops back to 0 */
    assert(cpu.pc == 0x100);

    m68k_step(&cpu); /* line still asserted: interrupt taken again */
    assert(cpu.pc == 0x500);

    m68k_set_irq(&cpu, 0);
    m68k_step(&cpu); /* RTE */
    m68k_step(&cpu); /* NOP runs, no further interrupt */
    assert(cpu.pc == 0x102);

    printf("Regression: level-triggered IRQ with ACK callback test passed!\n");
}

void test_regression_nmi_is_edge_triggered(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_int_ack_callback(&cpu, regression_autovector_ack_cb);

    cpu.pc = 0x100;
    cpu.sr = 0x2000; /* supervisor, mask 0 */
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E71); /* NOP */
    m68k_write_16(&cpu, 0x102, 0x4E71); /* NOP */
    m68k_write_32(&cpu, 31 * 4, 0x600); /* level 7 autovector */
    m68k_write_16(&cpu, 0x600, 0x4E73); /* RTE */

    m68k_set_irq(&cpu, 7);
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);
    assert(cpu.irq_level == 7); /* line still held */

    m68k_step(&cpu); /* RTE, mask drops back to 0 */
    assert(cpu.pc == 0x100);

    /* A held level 7 line must not retrigger without a new edge. */
    m68k_step(&cpu);
    assert(cpu.pc == 0x102);

    /* A new 0 -> 7 transition triggers again. */
    m68k_set_irq(&cpu, 0);
    m68k_set_irq(&cpu, 7);
    m68k_step(&cpu);
    assert(cpu.pc == 0x600);

    printf("Regression: NMI is edge triggered test passed!\n");
}

void test_regression_irq_autoclear_without_ack_callback(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    cpu.pc = 0x100;
    cpu.sr = 0x2000;
    cpu.a_regs[7].l = 0x1000;
    cpu.ssp = 0x1000;

    m68k_write_16(&cpu, 0x100, 0x4E71);
    m68k_write_32(&cpu, 28 * 4, 0x500);

    m68k_set_irq(&cpu, 4);
    m68k_step(&cpu);
    assert(cpu.pc == 0x500);
    /* Without an INT ACK callback the request clears automatically. */
    assert(cpu.irq_level == 0);

    printf("Regression: IRQ auto-clear without ACK callback test passed!\n");
}

void test_regression_movem_cycles(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* MOVEM.L (A0),D0-D1 = 12 + 8 * 2 = 28 cycles */
    m68k_write_16(&cpu, 0x1000, 0x4CD0);
    m68k_write_16(&cpu, 0x1002, 0x0003);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;
    m68k_end_timeslice(&cpu);
    int cycles = m68k_execute(&cpu, 28);
    assert(cycles == 28);

    /* MOVEM.W D0-D3,-(A0) = 8 + 4 * 4 = 24 cycles */
    memset(memory, 0, sizeof(memory));
    m68k_write_16(&cpu, 0x1000, 0x48A0);
    m68k_write_16(&cpu, 0x1002, 0xF000);
    cpu.pc = 0x1000;
    cpu.a_regs[0].l = 0x2000;
    m68k_end_timeslice(&cpu);
    cycles = m68k_execute(&cpu, 24);
    assert(cpu.a_regs[0].l == 0x2000 - 8);
    assert(cycles == 24);

    /* MOVEM.W $2000.W,D0 = 16 + 4 = 20 cycles */
    memset(memory, 0, sizeof(memory));
    m68k_write_16(&cpu, 0x1000, 0x4CB8);
    m68k_write_16(&cpu, 0x1002, 0x0001);
    m68k_write_16(&cpu, 0x1004, 0x2000);
    cpu.pc = 0x1000;
    m68k_end_timeslice(&cpu);
    cycles = m68k_execute(&cpu, 20);
    assert(cpu.pc == 0x1006);
    assert(cycles == 20);

    printf("Regression: MOVEM cycles test passed!\n");
}
