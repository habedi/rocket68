#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include "m68k.h"

uint8_t memory[0x10000];

unsigned int m68k_read_memory_8(unsigned int address) { return memory[address & 0xFFFF]; }
unsigned int m68k_read_memory_16(unsigned int address) { return (memory[address & 0xFFFF] << 8) | memory[(address + 1) & 0xFFFF]; }
unsigned int m68k_read_memory_32(unsigned int address) { return (m68k_read_memory_16(address) << 16) | m68k_read_memory_16(address + 2); }

unsigned int m68k_read_disassembler_8(unsigned int address) { return m68k_read_memory_8(address); }
unsigned int m68k_read_disassembler_16(unsigned int address) { return m68k_read_memory_16(address); }
unsigned int m68k_read_disassembler_32(unsigned int address) { return m68k_read_memory_32(address); }

void m68k_write_memory_8(unsigned int address, unsigned int value) { memory[address & 0xFFFF] = value & 0xFF; }
void m68k_write_memory_16(unsigned int address, unsigned int value) { memory[address & 0xFFFF] = (value >> 8) & 0xFF; memory[(address + 1) & 0xFFFF] = value & 0xFF; }
void m68k_write_memory_32(unsigned int address, unsigned int value) { m68k_write_memory_16(address, value >> 16); m68k_write_memory_16(address + 2, value & 0xFFFF); }

int main() {
    memset(memory, 0, sizeof(memory));

    uint16_t program[] = {
        0x203c, 0x0098, 0x9680, // MOVE.L #10000000, D0
        0xd481,                 // ADD.L D1, D2
        0xb581,                 // EOR.L D2, D1
        0x5283,                 // ADDQ.L #1, D3
        0x5380,                 // SUBQ.L #1, D0
        0x66f6,                 // BNE.S -10
        0x4e72, 0x2700          // STOP #$2700
    };

    for (size_t i = 0; i < sizeof(program)/sizeof(program[0]); i++) {
        memory[i*2] = program[i] >> 8;
        memory[i*2+1] = program[i] & 0xFF;
    }

    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);

    m68k_set_reg(M68K_REG_PC, 0);

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);


    while (m68k_get_reg(NULL, M68K_REG_PC) < sizeof(program) * 2) {
        m68k_execute(100000);


        if (m68k_get_reg(NULL, M68K_REG_PC) >= 16) break;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Musashi  Elapsed: %.4f seconds\n", elapsed);
    return 0;
}
