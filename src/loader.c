#include "loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

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

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = 0;
        if (len > 0 && line[len - 1] == '\r') line[--len] = 0;

        if (len < 4) continue;
        if (line[0] != 'S') continue;

        char type = line[1];
        int count = parse_byte(&line[2]);

        if ((int)len < 4 + count * 2) {
            fprintf(stderr, "Line %d: Line too short for count %d\n", line_num, count);
            continue;
        }

        u32 addr = 0;
        int addr_len = 0;
        int data_offset = 4;

        switch (type) {
            case '0':
            case '1':
                addr_len = 2;
                break;
            case '2':
                addr_len = 3;
                break;
            case '3':
                addr_len = 4;
                break;
            case '5':
                continue;
            case '7':
                addr_len = 4;
                break;
            case '8':
                addr_len = 3;
                break;
            case '9':
                addr_len = 2;
                break;
            default:
                fprintf(stderr, "Line %d: Unknown S-Type S%c\n", line_num, type);
                continue;
        }

        for (int i = 0; i < addr_len; i++) {
            addr = (addr << 8) | parse_byte(&line[data_offset]);
            data_offset += 2;
        }

        int data_len = count - addr_len - 1;

        if (type == '1' || type == '2' || type == '3') {
            for (int i = 0; i < data_len; i++) {
                u8 val = parse_byte(&line[data_offset]);
                data_offset += 2;
                m68k_write_8(cpu, addr + i, val);
            }
        } else if (type == '7' || type == '8' || type == '9') {
            cpu->pc = addr;
            printf("Entry point set to %08X\n", addr);

            break;
        }
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
