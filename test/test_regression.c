#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

static u32 watched_bus_address = 0;
static int watched_bus_hits = 0;
static M68kCpu* nested_exception_cpu = NULL;

static void regression_wait_bus_watch(M68kCpu* cpu, u32 address, M68kSize size) {
    (void)cpu;
    (void)size;
    if (address == watched_bus_address) {
        watched_bus_hits++;
    }
}

static void regression_nested_exception_cb(M68kCpu* cpu, u32 new_pc) {
    (void)cpu;
    (void)new_pc;
    if (nested_exception_cpu != NULL) {
        m68k_read_16(nested_exception_cpu, 0x1000);
    }
}

void test_regression_postinc_predec_byte() {
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

void test_regression_reset_vectors() {
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

void test_regression_jmp_dispatch() {
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

void test_regression_div_by_zero() {
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

void test_regression_addx_subx_zflag() {
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

void test_regression_sbcd() {
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

void test_regression_nbcd() {
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

void test_regression_stop_privilege() {
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

void test_regression_ext_dispatch() {
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

void test_regression_no_spurious_ea_read_on_write() {
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

void test_regression_stop_rte_resume_pc() {
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

void test_regression_execute_wakes_stopped_cpu_on_irq() {
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

void test_regression_exception_guards_are_per_instance() {
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
