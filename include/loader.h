/**
 * @file loader.h
 * @brief Program loading helpers for Rocket 68 memory.
 */
#ifndef M68K_LOADER_H
#define M68K_LOADER_H

#include <stdbool.h>

#include "m68k.h"

/**
 * @brief Load Motorola S-record data into CPU memory.
 *
 * @param cpu CPU instance with bound memory.
 * @param filename Path to an S-record file.
 * @return false only if the input file cannot be opened; otherwise true.
 *
 * Parsing errors in individual records are reported to stderr and skipped.
 */
bool m68k_load_srec(M68kCpu* cpu, const char* filename);

/**
 * @brief Load a raw binary file into CPU memory.
 *
 * @param cpu CPU instance with bound memory.
 * @param filename Path to a binary file.
 * @param address Start address in emulated memory.
 * @return false only if the input file cannot be opened; otherwise true.
 */
bool m68k_load_bin(M68kCpu* cpu, const char* filename, u32 address);

#endif
