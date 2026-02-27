#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "m68k.h"

int main(void) {
    M68kCpu cpu;
    uint8_t memory[0x10000];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

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

    cpu.pc = 0;

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (!cpu.stopped && cpu.pc < sizeof(program) * 2) {
        m68k_step(&cpu);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Rocket68 Elapsed: %.4f seconds\n", elapsed);
    return 0;
}
