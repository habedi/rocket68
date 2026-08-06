/**
 * @file test_disasm_bounds.c
 * @brief Bounds regression tests for the disassembler's internal scratch buffers.
 *
 * The disassembler formats each line from three fixed-size scratch buffers, namely
 * an effective address, a mnemonic, and an argument list. Their sizes are defined in
 * `src/disasm.c` and mirrored here. These tests sweep the whole opcode space against
 * adversarial extension words and program counters, then check that the formatted
 * fields stay inside those sizes. A format string that grows past the declared maxima
 * fails here, so the margin that lets the compiler prove the formatting calls cannot
 * truncate stays intact.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "disasm.h"
#include "m68k.h"
#include "test_m68k.h"

/* These must match the corresponding definitions in `src/disasm.c`. */
#define DISASM_EA_SIZE 24
#define DISASM_OP_SIZE 32
#define DISASM_ARGS_SIZE 64

/* The longest effective address any addressing mode can render is "$FFFFFFFF(PC)". */
#define DISASM_MAX_EA_LEN 13

_Static_assert(2 * DISASM_EA_SIZE + sizeof(", ") <= DISASM_ARGS_SIZE,
               "args buffer must hold two effective addresses plus a separator");
_Static_assert(DISASM_MAX_EA_LEN < DISASM_EA_SIZE,
               "effective address buffer must hold the longest rendered address");

/* The line layout produced by m68k_disasm is "%04X  %-8s %s". */
#define DISASM_OPCODE_FIELD 6
#define DISASM_OP_FIELD_MIN 8

static u32 probe_pc = 0;
static u16 probe_opcode = 0;
static u16 probe_extension = 0;

static u16 disasm_bounds_read16(M68kCpu* cpu, u32 address) {
    (void)cpu;
    return (address == (probe_pc & 0x00FFFFFFu)) ? probe_opcode : probe_extension;
}

/* Split a formatted line into its mnemonic and argument lengths. */
static void split_line(const char* line, size_t* op_len, size_t* args_len) {
    size_t line_len = strlen(line);
    assert(line_len > DISASM_OPCODE_FIELD);

    const char* op = line + DISASM_OPCODE_FIELD;
    const char* op_end = strchr(op, ' ');
    assert(op_end != NULL);
    *op_len = (size_t)(op_end - op);

    size_t field = (*op_len > DISASM_OP_FIELD_MIN) ? *op_len : DISASM_OP_FIELD_MIN;
    size_t args_start = DISASM_OPCODE_FIELD + field + 1;
    *args_len = (args_start < line_len) ? line_len - args_start : 0;
}

void test_disasm_buffer_bounds(void) {
    /*
     * These extension-word patterns maximize each rendered field. All zeros and all
     * ones give the numeric extremes, and the remaining patterns select long-form
     * index registers and the widest signed displacements.
     */
    static const u16 patterns[] = {0x0000, 0xFFFF, 0x8080, 0x7FFF, 0xF8FF, 0x0080};
    /*
     * These program counters maximize PC-relative output. The 68000 masks
     * addresses to 24 bits on access, but the disassembler prints the unmasked sum
     * of the base PC and the displacement, so a high PC yields the widest string.
     */
    static const u32 start_pcs[] = {0x00000000u, 0x00001000u, 0x00FFFF00u, 0xFFFFFF00u};

    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));
    m68k_set_read16_callback(&cpu, disasm_bounds_read16);

    size_t max_op = 0;
    size_t max_args = 0;
    size_t max_line = 0;
    char worst_line[256] = "";

    for (size_t p = 0; p < sizeof(patterns) / sizeof(patterns[0]); p++) {
        probe_extension = patterns[p];
        for (size_t s = 0; s < sizeof(start_pcs) / sizeof(start_pcs[0]); s++) {
            probe_pc = start_pcs[s];
            for (u32 opcode = 0; opcode <= 0xFFFF; opcode++) {
                probe_opcode = (u16)opcode;

                /* This buffer is oversized so that truncation cannot mask an overrun. */
                char line[256];
                memset(line, 0, sizeof(line));
                m68k_disasm(&cpu, probe_pc, line, (int)sizeof(line));

                size_t op_len = 0;
                size_t args_len = 0;
                split_line(line, &op_len, &args_len);

                assert(op_len < DISASM_OP_SIZE);
                assert(args_len < DISASM_ARGS_SIZE);
                /* The widest argument list is two effective addresses joined by ", ". */
                assert(args_len <= 2 * DISASM_MAX_EA_LEN + 2);

                if (op_len > max_op) max_op = op_len;
                if (args_len > max_args) max_args = args_len;
                if (strlen(line) > max_line) {
                    max_line = strlen(line);
                    snprintf(worst_line, sizeof(worst_line), "%s", line);
                }
            }
        }
    }

    printf("Disasm bounds: op %zu/%d, args %zu/%d, line %zu (\"%s\")\n", max_op, DISASM_OP_SIZE,
           max_args, DISASM_ARGS_SIZE, max_line, worst_line);
    printf("Disasm buffer bounds test passed!\n");
}

void test_disasm_truncates_small_buffers(void) {
    M68kCpu cpu;
    u8 memory[1024];
    memset(memory, 0, sizeof(memory));
    m68k_init(&cpu, memory, sizeof(memory));

    /* This encodes MOVE.B -128(A0,A0.W), -128(A0,A0.W), the longest line produced. */
    m68k_write_16(&cpu, 0x100, 0x11B0);
    m68k_write_16(&cpu, 0x102, 0x8080);
    m68k_write_16(&cpu, 0x104, 0x8080);

    char full[256];
    memset(full, 0, sizeof(full));
    m68k_disasm(&cpu, 0x100, full, (int)sizeof(full));
    assert(strstr(full, "MOVE.B") != NULL);

    /* A short destination must stay NUL-terminated and leave the guard bytes intact. */
    for (int size = 1; size <= 32; size++) {
        char guarded[64];
        memset(guarded, '#', sizeof(guarded));
        m68k_disasm(&cpu, 0x100, guarded, size);

        assert(memchr(guarded, '\0', (size_t)size) != NULL);
        assert(strncmp(guarded, full, strlen(guarded)) == 0);
        for (size_t i = (size_t)size; i < sizeof(guarded); i++) {
            assert(guarded[i] == '#');
        }
    }

    printf("Disasm small buffer truncation test passed!\n");
}
