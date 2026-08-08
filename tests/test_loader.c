#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disasm.h"
#include "loader.h"
#include "m68k.h"

void run_loader_tests(void);

void test_load_srec(void) {
    M68kCpu cpu;
    u8 memory[65536];
    m68k_init(&cpu, memory, sizeof(memory));

    // Create a temporary S-Record file
    const char* filename = "test.srec";
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("Failed to create test S-Record file");
        return;
    }
    // S0 Header
    fprintf(f, "S00600004844521B\n");
    // S1 Data: Addr 0x1000, Data 12 34 56 78 (4 bytes). Count = 2 (Addr) + 4 (Data) + 1 (Check) = 7
    // Checksum: 0xFF - ((07 + 10 + 00 + 12 + 34 + 56 + 78) & 0xFF) = 0xD4
    fprintf(f, "S107100012345678D4\n");
    // S9 Termination: Entry 0x1000
    // Count = 2 (Addr) + 1 (Check) = 3
    fprintf(f, "S9031000EC\n");
    fclose(f);

    bool success = m68k_load_srec(&cpu, filename);
    assert(success);

    // Check Memory
    assert(memory[0x1000] == 0x12);
    assert(memory[0x1001] == 0x34);
    assert(memory[0x1002] == 0x56);
    assert(memory[0x1003] == 0x78);

    // Check Entry Point
    assert(cpu.pc == 0x1000);

    // Cleanup
    remove(filename);

    printf("S-Record Loader test passed!\n");
}

void test_load_bin(void) {
    M68kCpu cpu;
    u8 memory[65536];
    m68k_init(&cpu, memory, sizeof(memory));

    const char* filename = "test.bin";
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to create test binary file");
        return;
    }
    u8 data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    u32 size = 0xDEADBEEF;
    bool success = m68k_load_bin(&cpu, filename, 0x2000, &size);
    assert(success);
    assert(size == 4);

    assert(memory[0x2000] == 0xAA);
    assert(memory[0x2001] == 0xBB);
    assert(memory[0x2002] == 0xCC);
    assert(memory[0x2003] == 0xDD);

    /* A NULL size pointer is allowed. */
    success = m68k_load_bin(&cpu, filename, 0x3000, NULL);
    assert(success);
    assert(memory[0x3000] == 0xAA);
    assert(memory[0x3003] == 0xDD);

    remove(filename);
    printf("Binary Loader test passed!\n");
}

void test_load_bin_size_reporting(void) {
    M68kCpu cpu;
    u8 memory[256];
    m68k_init(&cpu, memory, sizeof(memory));

    /* An empty file loads successfully with a size of zero. */
    const char* filename = "test_empty.bin";
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to create test binary file");
        return;
    }
    fclose(f);

    u32 size = 0xDEADBEEF;
    bool success = m68k_load_bin(&cpu, filename, 0x10, &size);
    remove(filename);
    assert(success);
    assert(size == 0);

    /* An open failure returns false and reports a size of zero. */
    size = 0xDEADBEEF;
    success = m68k_load_bin(&cpu, "no_such_file.bin", 0x10, &size);
    assert(!success);
    assert(size == 0);

    /* A load that runs past bound memory reports the bytes written. */
    filename = "test_trunc.bin";
    f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to create test binary file");
        return;
    }
    u8 data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    size = 0xDEADBEEF;
    success = m68k_load_bin(&cpu, filename, 0xFC, &size); /* runs past the 256-byte end */
    remove(filename);
    assert(success);
    assert(size == 4);
    assert(memory[0xFC] == 1);
    assert(memory[0xFF] == 4);

    printf("Binary Loader size reporting test passed!\n");
}

void test_load_bin_boundaries(void) {
    M68kCpu cpu;
    u8 memory[256];
    m68k_init(&cpu, memory, sizeof(memory));

    const char* filename = "test_bounds.bin";
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to create test binary file");
        return;
    }
    u8 data[4] = {0x11, 0x22, 0x33, 0x44};
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    /* An exact fit against the end of bound memory is not a truncation. */
    u32 size = 0xDEADBEEF;
    bool success = m68k_load_bin(&cpu, filename, 0xFC, &size);
    assert(success);
    assert(size == 4);
    assert(memory[0xFC] == 0x11);
    assert(memory[0xFF] == 0x44);

    /* A start address already outside bound memory writes nothing. */
    size = 0xDEADBEEF;
    success = m68k_load_bin(&cpu, filename, 0x100, &size);
    assert(success);
    assert(size == 0);

    /* Addresses are masked to the 24-bit bus, so the high byte is ignored. */
    memset(memory, 0, sizeof(memory));
    size = 0xDEADBEEF;
    success = m68k_load_bin(&cpu, filename, 0x01000010, &size);
    assert(success);
    assert(size == 4);
    assert(memory[0x10] == 0x11);
    assert(memory[0x13] == 0x44);

    remove(filename);
    printf("Binary Loader boundary test passed!\n");
}

void test_load_bin_large_file(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* A file larger than the loader's internal read buffer exercises size
     * accumulation across multiple read chunks. */
    enum { LARGE_SIZE = 5000 };
    const char* filename = "test_large.bin";
    FILE* f = fopen(filename, "wb");
    if (!f) {
        perror("Failed to create test binary file");
        return;
    }
    for (int i = 0; i < LARGE_SIZE; i++) fputc(i & 0xFF, f);
    fclose(f);

    u32 size = 0xDEADBEEF;
    bool success = m68k_load_bin(&cpu, filename, 0x100, &size);
    remove(filename);
    assert(success);
    assert(size == LARGE_SIZE);
    assert(memory[0x100] == 0x00);
    assert(memory[0x100 + 1024] == (1024 & 0xFF));
    assert(memory[0x100 + LARGE_SIZE - 1] == ((LARGE_SIZE - 1) & 0xFF));
    assert(memory[0x100 + LARGE_SIZE] == 0x00);

    printf("Binary Loader large-file test passed!\n");
}

void test_load_srec_robustness(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    u32 old_pc = cpu.pc;

    const char* filename = "test_robust.srec";
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("Failed to create test S-Record file");
        return;
    }
    /* A count field larger than the line is reported and skipped. */
    fprintf(f, "S1FF100012\n");
    /* An unknown record type is reported and skipped. The checksum is
     * valid so the unknown-type path is the one exercised. */
    fprintf(f, "S407100012345678D4\n");
    /* A non-record line is ignored. */
    fprintf(f, "not an s-record\n");
    /* An S5 record-count record is ignored. */
    fprintf(f, "S5030001FB\n");
    /* Lowercase hex digits are accepted; one byte 0xAB at 0x20fe.
     * Checksum: 0xFF - ((04 + 20 + FE + AB) & 0xFF) = 0x32. */
    fprintf(f, "S10420feab32\n");
    /* No terminator record, so the PC stays untouched. */
    fclose(f);

    bool success = m68k_load_srec(&cpu, filename);
    remove(filename);

    assert(success);
    assert(memory[0x20FE] == 0xAB);
    /* The malformed and unknown records must not have written anything. */
    assert(memory[0x1000] == 0x00);
    assert(cpu.pc == old_pc);

    /* An open failure returns false. */
    assert(!m68k_load_srec(&cpu, "no_such_file.srec"));

    printf("S-Record Loader robustness test passed!\n");
}

void test_load_srec_checksum(void) {
    M68kCpu cpu;
    u8 memory[65536];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    const char* filename = "test_checksum.srec";
    FILE* f = fopen(filename, "w");
    if (!f) {
        perror("Failed to create test S-Record file");
        return;
    }
    /* Valid record: one byte 0xAB at 0x0080. Sum 04+00+80+AB = 0x2F, so
     * the checksum is 0xFF - 0x2F = 0xD0. */
    fprintf(f, "S1040080ABD0\n");
    /* Corrupted checksum: the correct value for this record is 0xC0.
     * The record must be reported and skipped. */
    fprintf(f, "S1040090ABCF\n");
    fclose(f);

    bool success = m68k_load_srec(&cpu, filename);
    remove(filename);

    assert(success);
    assert(memory[0x80] == 0xAB);
    assert(memory[0x90] == 0x00);

    printf("S-Record checksum test passed!\n");
}

void test_disasm(void) {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Write some opcodes
    m68k_write_16(&cpu, 0, 0xD040);  // ADD.W D0, D1 ? (ADD 1101 000 001 000 001 ? )
    // ADD D0, D1: 1101 001 001 000 000 -> D240?
    // D040 -> 1101 0000 0100 0000. D0, opmode 0(byte), ea mode 2(A0)?

    m68k_write_16(&cpu, 2, 0x4E75);

    char buf[64];
    int len = m68k_disasm(&cpu, 0, buf, sizeof(buf));
    (void)len;
    printf("Disasm 0: %s\n", buf);
    assert(strstr(buf, "ADD"));

    len = m68k_disasm(&cpu, 2, buf, sizeof(buf));
    printf("Disasm 2: %s\n", buf);
    assert(strstr(buf, "RTS"));
}

void test_disasm_full(void) {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));
    char buf[64];

    // 1. MOVE.L #$12345678, D0
    m68k_write_16(&cpu, 0, 0x203C);
    m68k_write_32(&cpu, 2, 0x12345678);
    m68k_disasm(&cpu, 0, buf, sizeof(buf));
    printf("Disasm MOVE.L: %s\n", buf);
    assert(strstr(buf, "MOVE.L"));
    assert(strstr(buf, "#$12345678"));
    assert(strstr(buf, "D0"));

    // 2. MOVE.W (A0), (A1)+
    m68k_write_16(&cpu, 6, 0x32D0);  // 00 11 001 011 010 000
    m68k_disasm(&cpu, 6, buf, sizeof(buf));
    printf("Disasm MOVE.W: %s\n", buf);
    assert(strstr(buf, "MOVE.W"));
    assert(strstr(buf, "(A0)"));
    assert(strstr(buf, "(A1)+"));

    // 3. BRA $10 (Offset 14 -> +16 = $10 ? No. PC+2+Disp)
    // PC=8. Next=10. Target=20 ($14). Disp=10 ($A).
    m68k_write_16(&cpu, 8, 0x600A);
    m68k_disasm(&cpu, 8, buf, sizeof(buf));
    printf("Disasm BRA: %s\n", buf);
    assert(strstr(buf, "BRA"));
    // Target = 8 + 2 + 10 = 20 = $14
    assert(strstr(buf, "$14"));

    // 4. ADDQ.L #1, D0
    // 0101 001 0 10 000 000 -> 5280
    m68k_write_16(&cpu, 10, 0x5280);
    m68k_disasm(&cpu, 10, buf, sizeof(buf));
    printf("Disasm ADDQ: %s\n", buf);
    assert(strstr(buf, "ADDQ"));
    assert(strstr(buf, "#1"));

    // 5. ABCD D0, D1
    m68k_write_16(&cpu, 12, 0xC300);
    m68k_disasm(&cpu, 12, buf, sizeof(buf));
    printf("Disasm ABCD: %s\n", buf);
    assert(strstr(buf, "ABCD"));

    // 6. CHK (A0), D1
    // 0100 001 110 010 000 -> 4390
    m68k_write_16(&cpu, 14, 0x4390);
    m68k_disasm(&cpu, 14, buf, sizeof(buf));
    printf("Disasm CHK: %s\n", buf);
    assert(strstr(buf, "CHK"));
}

void test_io(void) {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    printf("Testing I/O (Expect 'A'): ");
    m68k_write_8(&cpu, 0xE00000, 'A');
    printf("\n");
}

// Expose for the main test runner
void run_loader_tests(void) {
    test_load_srec();
    test_load_bin();
    test_load_bin_size_reporting();
    test_load_bin_boundaries();
    test_load_bin_large_file();
    test_load_srec_robustness();
    test_load_srec_checksum();
    test_disasm();
    test_disasm_full();
    test_io();
}
