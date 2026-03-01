/**
 * @file disasm.h
 * @brief Public disassembler API for Rocket 68.
 */
#ifndef M68K_DISASM_H
#define M68K_DISASM_H

#include "m68k.h"

/**
 * @brief Disassemble one instruction at a program counter.
 *
 * @param cpu CPU instance used for instruction fetches.
 * @param pc Program counter to disassemble from.
 * @param buffer Destination text buffer.
 * @param buf_size Size of @p buffer in bytes.
 * @return Number of bytes consumed by the instruction.
 */
int m68k_disasm(M68kCpu* cpu, u32 pc, char* buffer, int buf_size);

#endif
