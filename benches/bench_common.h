#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#define _POSIX_C_SOURCE 199309L
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * Shared benchmark types and utilities
 * -------------------------------------------------------------------------- */

typedef struct
{
    const char* name;
    const uint16_t* code;
    size_t code_words;
    uint32_t expected_iterations;
} BenchWorkload;

/* Guardrails to prevent benchmark hangs when a workload fails to terminate. */
#define BENCH_SLICE_CYCLES 100000
#define BENCH_MAX_SLICES 2000000u
#define BENCH_MAX_SECONDS 60.0

static inline double time_diff(const struct timespec* start, const struct timespec* end)
{
    return (end->tv_sec - start->tv_sec) + (end->tv_nsec - start->tv_nsec) / 1e9;
}

static inline void print_result(const char* name, double elapsed, uint32_t iterations)
{
    printf("  %-20s  %8.4f s", name, elapsed);
    if (elapsed > 0.0)
    {
        double mhz = (iterations * 1.0) / elapsed / 1e6;
        printf("  (~%.1fM iter/s)", mhz);
    }
    printf("\n");
}

static inline unsigned int find_stop_end_pc(const BenchWorkload* w)
{
    for (size_t i = 0; i + 1 < w->code_words; i++)
    {
        if (w->code[i] == 0x4E72)
        {
            return (unsigned int)((i + 2) * 2);
        }
    }
    return (unsigned int)(w->code_words * 2);
}

static inline int bench_guard_exceeded(const char* core,
                                       const char* workload,
                                       size_t slices,
                                       const struct timespec* start)
{
    if (slices > BENCH_MAX_SLICES)
    {
        fprintf(stderr,
                "%s benchmark guard tripped in \"%s\": exceeded %u slices\n",
                core,
                workload,
                (unsigned)BENCH_MAX_SLICES);
        return 1;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if (time_diff(start, &now) > BENCH_MAX_SECONDS)
    {
        fprintf(stderr,
                "%s benchmark guard tripped in \"%s\": exceeded %.1f seconds\n",
                core,
                workload,
                BENCH_MAX_SECONDS);
        return 1;
    }

    return 0;
}

/* --------------------------------------------------------------------------
 * Workloads — identical programs for both Rocket68 and Musashi.
 * All use register-register or short (An) indirect addressing.
 *
 * Byte layout and displacement calculations are annotated inline.
 * -------------------------------------------------------------------------- */

/*  Tight ALU loop (10M iterations)
 *  Byte  Words  Instruction
 *    0:  3      MOVE.L  #10000000, D0
 *    6:  1      ADD.L   D1, D2
 *    8:  1      EOR.L   D2, D1
 *   10:  1      ADDQ.L  #1, D3
 *   12:  1      SUBQ.L  #1, D0
 *   14:  1      BNE.S   byte 6   (disp = 6 - 16 = -10 = 0xF6)
 *   16:  2      STOP    #$2700                                      */
static const uint16_t PROG_ALU[] = {
    0x203C, 0x0098, 0x9680,
    0xD481,
    0xB581,
    0x5283,
    0x5380,
    0x66F6,
    0x4E72, 0x2700
};

/*  Memory read/write via (A0) (5M iterations)
 *  A0 must be pre-set to a safe RAM address (e.g. 0x8000).
 *  Byte  Words  Instruction
 *    0:  3      MOVE.L  #5000000, D0
 *    6:  1      MOVE.L  D1, (A0)
 *    8:  1      MOVE.L  (A0), D2
 *   10:  1      ADDQ.L  #1, D1
 *   12:  1      SUBQ.L  #1, D0
 *   14:  1      BNE.S   byte 6   (disp = 6 - 16 = -10 = 0xF6)
 *   16:  2      STOP    #$2700                                      */
static const uint16_t PROG_MEM[] = {
    0x203C, 0x004C, 0x4B40,
    0x2081,
    0x2410,
    0x5281,
    0x5380,
    0x66F6,
    0x4E72, 0x2700
};

/*  Subroutine calls via BSR/RTS (2M iterations)
 *  Byte  Words  Instruction
 *    0:  3      MOVE.L  #2000000, D0
 *    6:  1      BSR.S   byte 16  (disp = 16 - 8 = +8 = 0x08)
 *    8:  1      SUBQ.L  #1, D0
 *   10:  1      BNE.S   byte 6   (disp = 6 - 12 = -6 = 0xFA)
 *   12:  2      STOP    #$2700
 *   16:  1      ADDQ.L  #1, D1    <- subroutine
 *   18:  1      RTS                                                  */
static const uint16_t PROG_CALL[] = {
    0x203C, 0x001E, 0x8480,
    0x6108,
    0x5380,
    0x66FA,
    0x4E72, 0x2700,
    0x5281,
    0x4E75
};

/*  Shift/Rotate heavy (5M iterations)
 *  Byte  Words  Instruction
 *    0:  3      MOVE.L  #5000000, D0
 *    6:  3      MOVE.L  #$DEADBEEF, D1
 *   12:  1      ROL.L   #1, D1
 *   14:  1      LSR.L   #8, D1
 *   16:  1      LSL.L   #4, D1
 *   18:  1      ASR.L   #8, D1
 *   20:  1      SUBQ.L  #1, D0
 *   22:  1      BNE.S   byte 12  (disp = 12 - 24 = -12 = 0xF4)
 *   24:  2      STOP    #$2700                                      */
static const uint16_t PROG_SHIFT[] = {
    0x203C, 0x004C, 0x4B40,
    0x223C, 0xDEAD, 0xBEEF,
    0xE399,
    0xE089,
    0xE981,
    0xE0A1,
    0x5380,
    0x66F4,
    0x4E72, 0x2700
};

#define BENCH_WORKLOADS                                                         \
    {                                                                           \
        {"ALU (10M iter)",      PROG_ALU,   sizeof(PROG_ALU)   / 2, 10000000}, \
        {"Memory (5M iter)",    PROG_MEM,   sizeof(PROG_MEM)   / 2,  5000000}, \
        {"Calls (2M iter)",     PROG_CALL,  sizeof(PROG_CALL)  / 2,  2000000}, \
        {"Shift/Rot (5M iter)", PROG_SHIFT, sizeof(PROG_SHIFT) / 2,  5000000}, \
    }

static inline void load_program(uint8_t* memory, const uint16_t* code, size_t words)
{
    for (size_t i = 0; i < words; i++)
    {
        memory[i * 2] = code[i] >> 8;
        memory[i * 2 + 1] = code[i] & 0xFF;
    }
}

#endif
