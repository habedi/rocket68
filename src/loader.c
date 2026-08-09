/**
 * @file loader.c
 * @brief S-record and binary loader implementation.
 */
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

/* Loading is a host-side operation: write directly into bound flat memory
 * instead of running emulated bus cycles, so out-of-range records cannot
 * trigger the CPU bus-error machinery. */
static bool loader_store(M68kCpu* cpu, u32 address, u8 value) {
    address &= 0x00FFFFFFu;
    if (cpu->memory && address < cpu->memory_size) {
        cpu->memory[address] = value;
        return true;
    }
    return false;
}

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

        /* The count byte, address, data, and checksum must sum to 0xFF
         * modulo 256 (the checksum is the ones' complement of the rest). */
        unsigned int sum = 0;
        for (int i = 0; i <= count; i++) {
            sum += parse_byte(&line[2 + i * 2]);
        }
        if ((sum & 0xFF) != 0xFF) {
            fprintf(stderr, "Line %d: Checksum mismatch\n", line_num);
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
                if (!loader_store(cpu, addr + i, val)) {
                    fprintf(stderr, "Line %d: Address %06X is outside bound memory\n", line_num,
                            (addr + i) & 0x00FFFFFFu);
                    break;
                }
            }
        } else if (type == '7' || type == '8' || type == '9') {
            m68k_set_pc(cpu, addr);
            printf("Entry point set to %08X\n", addr);

            break;
        }
    }

    fclose(f);
    return true;
}

bool m68k_load_bin(M68kCpu* cpu, const char* filename, u32 address, u32* size_out) {
    if (size_out) *size_out = 0;

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
            if (!loader_store(cpu, current_addr, buffer[i])) {
                fprintf(stderr, "Address %06X is outside bound memory\n",
                        current_addr & 0x00FFFFFFu);
                fclose(f);
                if (size_out) *size_out = current_addr - address;
                return true;
            }
            current_addr++;
        }
    }

    fclose(f);
    if (size_out) *size_out = current_addr - address;
    return true;
}

bool m68k_load_ihex(M68kCpu* cpu, const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        perror("Failed to open file");
        return false;
    }

    char line[600];
    int line_num = 0;
    u32 base = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;

        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = 0;

        if (len == 0) continue;
        if (line[0] != ':') {
            fprintf(stderr, "Line %d: Missing record mark\n", line_num);
            continue;
        }

        if (len < 11) {
            fprintf(stderr, "Line %d: Record too short\n", line_num);
            continue;
        }

        int count = parse_byte(&line[1]);
        u32 offset = ((u32)parse_byte(&line[3]) << 8) | parse_byte(&line[5]);
        int type = parse_byte(&line[7]);

        if ((int)len < 11 + count * 2) {
            fprintf(stderr, "Line %d: Record too short for count %d\n", line_num, count);
            continue;
        }

        /* All bytes including the checksum must sum to zero modulo 256. */
        unsigned int sum = 0;
        for (int i = 0; i <= count + 4; i++) {
            sum += parse_byte(&line[1 + i * 2]);
        }
        if ((sum & 0xFF) != 0) {
            fprintf(stderr, "Line %d: Checksum mismatch\n", line_num);
            continue;
        }

        const char* data = &line[9];
        switch (type) {
            case 0x00: /* data */
                for (int i = 0; i < count; i++) {
                    u32 address = base + offset + (u32)i;
                    if (!loader_store(cpu, address, parse_byte(&data[i * 2]))) {
                        fprintf(stderr, "Line %d: Address %06X is outside bound memory\n",
                                line_num, address & 0x00FFFFFFu);
                        break;
                    }
                }
                break;
            case 0x01: /* end of file */
                fclose(f);
                return true;
            case 0x02: /* extended segment address */
                if (count == 2) {
                    base = (((u32)parse_byte(&data[0]) << 8) | parse_byte(&data[2])) << 4;
                } else {
                    fprintf(stderr, "Line %d: Bad extended segment record\n", line_num);
                }
                break;
            case 0x04: /* extended linear address */
                if (count == 2) {
                    base = (((u32)parse_byte(&data[0]) << 8) | parse_byte(&data[2])) << 16;
                } else {
                    fprintf(stderr, "Line %d: Bad extended linear record\n", line_num);
                }
                break;
            case 0x03: /* start segment address: CS:IP */
                if (count == 4) {
                    u32 cs = ((u32)parse_byte(&data[0]) << 8) | parse_byte(&data[2]);
                    u32 ip = ((u32)parse_byte(&data[4]) << 8) | parse_byte(&data[6]);
                    m68k_set_pc(cpu, (cs << 4) + ip);
                } else {
                    fprintf(stderr, "Line %d: Bad start segment record\n", line_num);
                }
                break;
            case 0x05: /* start linear address */
                if (count == 4) {
                    u32 addr = ((u32)parse_byte(&data[0]) << 24) |
                               ((u32)parse_byte(&data[2]) << 16) |
                               ((u32)parse_byte(&data[4]) << 8) | parse_byte(&data[6]);
                    m68k_set_pc(cpu, addr);
                } else {
                    fprintf(stderr, "Line %d: Bad start linear record\n", line_num);
                }
                break;
            default:
                fprintf(stderr, "Line %d: Unknown record type %02X\n", line_num, type);
                break;
        }
    }

    fclose(f);
    return true;
}
