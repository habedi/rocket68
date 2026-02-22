#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/m68k/m68k_internal.h"

#define DEFAULT_BCD_TABLE_PATH "external/bcd-test-rom/data/bcd-table.bin"
#define FLAG_MASK (M68K_SR_X | M68K_SR_N | M68K_SR_Z | M68K_SR_V | M68K_SR_C)

static bool read_u8(FILE* file, uint8_t* value) {
    int c = fgetc(file);
    if (c == EOF) {
        return false;
    }
    *value = (uint8_t)c;
    return true;
}

static void set_input_flags(M68kCpu* cpu, int carry_in, int zero_in) {
    cpu->sr = 0;
    if (carry_in) {
        cpu->sr |= M68K_SR_X | M68K_SR_C;
    }
    if (zero_in) {
        cpu->sr |= M68K_SR_Z;
    }
}

static void report_mismatch(const char* op, int ii, int jj, int carry_in, int zero_in,
                            uint8_t expected_flags, uint8_t expected_result, uint8_t got_flags,
                            uint8_t got_result, int* reported) {
    if (*reported >= 10) {
        return;
    }

    if (jj >= 0) {
        fprintf(stderr,
                "%s mismatch ii=%02X jj=%02X X=%d Z=%d | exp flags=%02X res=%02X got flags=%02X "
                "res=%02X\n",
                op, ii, jj, carry_in, zero_in, expected_flags, expected_result, got_flags,
                got_result);
    } else {
        fprintf(stderr,
                "%s mismatch ii=%02X X=%d Z=%d | exp flags=%02X res=%02X got flags=%02X res=%02X\n",
                op, ii, carry_in, zero_in, expected_flags, expected_result, got_flags, got_result);
    }

    (*reported)++;
}

static bool verify_abcd(FILE* table, M68kCpu* cpu, int* mismatches, int* reported) {
    const uint16_t opcode = (0 << 9) | (0 << 3) | 1;  // D0 + D1

    for (int ii = 0; ii < 256; ii++) {
        for (int jj = 0; jj < 256; jj++) {
            for (int carry_in = 0; carry_in < 2; carry_in++) {
                for (int zero_in = 0; zero_in < 2; zero_in++) {
                    uint8_t expected_flags = 0;
                    uint8_t expected_result = 0;
                    if (!read_u8(table, &expected_flags) || !read_u8(table, &expected_result)) {
                        fprintf(stderr, "Unexpected end of BCD table while reading ABCD data.\n");
                        return false;
                    }

                    cpu->d_regs[0] = (uint8_t)jj;
                    cpu->d_regs[1] = (uint8_t)ii;
                    set_input_flags(cpu, carry_in, zero_in);

                    m68k_exec_abcd(cpu, opcode);

                    uint8_t got_result = (uint8_t)(cpu->d_regs[0] & 0xFF);
                    uint8_t got_flags = (uint8_t)(cpu->sr & FLAG_MASK);
                    uint8_t expected_masked = expected_flags & 0x1F;

                    if (got_result != expected_result || got_flags != expected_masked) {
                        (*mismatches)++;
                        report_mismatch("ABCD", ii, jj, carry_in, zero_in, expected_masked,
                                        expected_result, got_flags, got_result, reported);
                    }
                }
            }
        }
    }

    return true;
}

static bool verify_sbcd(FILE* table, M68kCpu* cpu, int* mismatches, int* reported) {
    const uint16_t opcode = (0 << 9) | (0 << 3) | 1;  // D0 - D1

    for (int ii = 0; ii < 256; ii++) {
        for (int jj = 0; jj < 256; jj++) {
            for (int carry_in = 0; carry_in < 2; carry_in++) {
                for (int zero_in = 0; zero_in < 2; zero_in++) {
                    uint8_t expected_flags = 0;
                    uint8_t expected_result = 0;
                    if (!read_u8(table, &expected_flags) || !read_u8(table, &expected_result)) {
                        fprintf(stderr, "Unexpected end of BCD table while reading SBCD data.\n");
                        return false;
                    }

                    cpu->d_regs[0] = (uint8_t)jj;
                    cpu->d_regs[1] = (uint8_t)ii;
                    set_input_flags(cpu, carry_in, zero_in);

                    m68k_exec_sbcd(cpu, opcode);

                    uint8_t got_result = (uint8_t)(cpu->d_regs[0] & 0xFF);
                    uint8_t got_flags = (uint8_t)(cpu->sr & FLAG_MASK);
                    uint8_t expected_masked = expected_flags & 0x1F;

                    if (got_result != expected_result || got_flags != expected_masked) {
                        (*mismatches)++;
                        report_mismatch("SBCD", ii, jj, carry_in, zero_in, expected_masked,
                                        expected_result, got_flags, got_result, reported);
                    }
                }
            }
        }
    }

    return true;
}

static bool verify_nbcd(FILE* table, M68kCpu* cpu, int* mismatches, int* reported) {
    const uint16_t opcode = 0;  // NBCD D0

    for (int ii = 0; ii < 256; ii++) {
        for (int carry_in = 0; carry_in < 2; carry_in++) {
            for (int zero_in = 0; zero_in < 2; zero_in++) {
                uint8_t expected_flags = 0;
                uint8_t expected_result = 0;
                if (!read_u8(table, &expected_flags) || !read_u8(table, &expected_result)) {
                    fprintf(stderr, "Unexpected end of BCD table while reading NBCD data.\n");
                    return false;
                }

                cpu->d_regs[0] = (uint8_t)ii;
                set_input_flags(cpu, carry_in, zero_in);

                m68k_exec_nbcd(cpu, opcode);

                uint8_t got_result = (uint8_t)(cpu->d_regs[0] & 0xFF);
                uint8_t got_flags = (uint8_t)(cpu->sr & FLAG_MASK);
                uint8_t expected_masked = expected_flags & 0x1F;

                if (got_result != expected_result || got_flags != expected_masked) {
                    (*mismatches)++;
                    report_mismatch("NBCD", ii, -1, carry_in, zero_in, expected_masked,
                                    expected_result, got_flags, got_result, reported);
                }
            }
        }
    }

    return true;
}

int main(int argc, char** argv) {
    const char* table_path = getenv("BCD_TABLE_PATH");
    if (table_path == NULL || table_path[0] == '\0') {
        table_path = DEFAULT_BCD_TABLE_PATH;
    }

    FILE* table = fopen(table_path, "rb");
    if (!table) {
        fprintf(stderr, "Failed to open BCD table: %s\n", table_path);
        return 1;
    }

    const size_t memory_size = 64 * 1024;
    uint8_t* memory = calloc(1, memory_size);
    if (!memory) {
        fprintf(stderr, "Failed to allocate emulator memory.\n");
        fclose(table);
        return 1;
    }

    M68kCpu cpu;
    m68k_init(&cpu, memory, (uint32_t)memory_size);

    int mismatches = 0;
    int reported = 0;

    if (!verify_abcd(table, &cpu, &mismatches, &reported) ||
        !verify_sbcd(table, &cpu, &mismatches, &reported) ||
        !verify_nbcd(table, &cpu, &mismatches, &reported)) {
        free(memory);
        fclose(table);
        return 1;
    }

    fclose(table);
    free(memory);

    if (mismatches == 0) {
        printf("BCD verifier: all cases matched.\n");
        return 0;
    }

    fprintf(stderr, "BCD verifier: %d mismatches found.\n", mismatches);
    return 2;
}
