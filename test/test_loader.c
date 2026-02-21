#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disasm.h"
#include "loader.h"
#include "m68k.h"

void run_loader_tests();

void test_load_srec() {
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
    // Checksum: FF - (03 (count? no) + 10 + 00 + 12 + 34 + 56 + 78) & FF ...
    // Let's just write valid data and ignore checksum in our simple loader for now, but format
    // correctly. Count field is bytes remaining. 0x1000 -> 12 34 56 78 Count = 3 (Addr 2 + Check 1)
    // + 4 = 7. S1 07 10 00 12 34 56 78 ??
    fprintf(f, "S10710001234567800\n");
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

void test_load_bin() {
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

    bool success = m68k_load_bin(&cpu, filename, 0x2000);
    assert(success);

    assert(memory[0x2000] == 0xAA);
    assert(memory[0x2001] == 0xBB);
    assert(memory[0x2002] == 0xCC);
    assert(memory[0x2003] == 0xDD);

    remove(filename);
    printf("Binary Loader test passed!\n");
}

void test_disasm() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    // Write some opcodes
    m68k_write_16(&cpu, 0, 0xD040);  // ADD.W D0, D1 ? (ADD 1101 000 001 000 001 ? )
    // ADD D0, D1: 1101 001 001 000 000 -> D240?
    // D040 -> 1101 0000 0100 0000. D0, opmode 0(byte), ea mode 2(A0)?

    m68k_write_16(&cpu, 2, 0x4E75);  // RTS

    char buf[64];
    int len = m68k_disasm(&cpu, 0, buf, sizeof(buf));
    (void)len;
    printf("Disasm 0: %s\n", buf);
    assert(strstr(buf, "ADD"));

    len = m68k_disasm(&cpu, 2, buf, sizeof(buf));
    printf("Disasm 2: %s\n", buf);
    assert(strstr(buf, "RTS"));
}

// ... (Previous tests)

void test_disasm_full() {
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

void test_io() {
    M68kCpu cpu;
    u8 memory[1024];
    m68k_init(&cpu, memory, sizeof(memory));

    printf("Testing I/O (Expect 'A'): ");
    m68k_write_8(&cpu, 0xE00000, 'A');
    printf("\n");
}

// Expose for the main test runner
void run_loader_tests() {
    test_load_srec();
    test_load_bin();
    test_disasm();
    test_disasm_full();
    test_io();
}
