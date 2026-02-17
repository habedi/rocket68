#include "loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to convert hex char to int
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

// Helper to parse 2 hex chars as a byte
static u8 parse_byte(const char* ptr) { return (hex_val(ptr[0]) << 4) | hex_val(ptr[1]); }

bool m68k_load_srec(M68kCpu* cpu, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("Failed to open file");
        return false;
    }

    char line[512];
    int line_num = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;

        // Strip newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = 0;
        if (len > 0 && line[len - 1] == '\r') line[--len] = 0;

        if (len < 4) continue;         // Too short
        if (line[0] != 'S') continue;  // Not S-Record

        char type = line[1];
        int count = parse_byte(&line[2]);  // Byte count (Addr + Data + Checksum)

        // Validation: Length should be 2 (S+Type) + 2 (Count) + Count*2
        if ((int)len < 4 + count * 2) {
            fprintf(stderr, "Line %d: Line too short for count %d\n", line_num, count);
            continue;
        }

        u32 addr = 0;
        int addr_len = 0;
        int data_offset = 4;  // Skip S+Type + Count

        // Determine Address Length
        switch (type) {
            case '0':  // Valid, Header (ignore data usually, or print it)
            case '1':
                addr_len = 2;
                break;  // 16-bit
            case '2':
                addr_len = 3;
                break;  // 24-bit
            case '3':
                addr_len = 4;
                break;  // 32-bit
            case '5':   // Count record (optional)
                continue;
            case '7':
                addr_len = 4;
                break;  // 32-bit Entry
            case '8':
                addr_len = 3;
                break;  // 24-bit Entry
            case '9':
                addr_len = 2;
                break;  // 16-bit Entry
            default:
                fprintf(stderr, "Line %d: Unknown S-Type S%c\n", line_num, type);
                continue;
        }

        // Parse Address
        for (int i = 0; i < addr_len; i++) {
            addr = (addr << 8) | parse_byte(&line[data_offset]);
            data_offset += 2;
        }

        int data_len = count - addr_len - 1;  // Subtract Address length and Checksum (1 byte)

        // Action based on Type
        if (type == '1' || type == '2' || type == '3') {
            // Data Record: Load into memory
            for (int i = 0; i < data_len; i++) {
                u8 val = parse_byte(&line[data_offset]);
                data_offset += 2;
                m68k_write_8(cpu, addr + i, val);
            }
        } else if (type == '7' || type == '8' || type == '9') {
            // Termination Record: Entry Point
            cpu->pc = addr;
            printf("Entry point set to %08X\n", addr);
            // We could break here, but some files might have trailing data? S-Rec spec says this is
            // EOF.
            break;
        }

        // (Checksum validation skipped for brevity, but recommended in robust impl)
    }

    fclose(f);
    return true;
}

bool m68k_load_bin(M68kCpu* cpu, const char* filename, u32 address) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        perror("Failed to open file");
        return false;
    }

    // Read directly into memory?
    // We can't access cpu->memory directly safely if we want to respect bounds/mapping (if
    // implemented). But m68k_write_8 is suitable. For speed, let's buffer.

    u8 buffer[1024];
    size_t bytes;
    u32 current_addr = address;

    while ((bytes = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        for (size_t i = 0; i < bytes; i++) {
            m68k_write_8(cpu, current_addr++, buffer[i]);
        }
    }

    fclose(f);
    return true;
}
