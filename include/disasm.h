#ifndef M68K_DISASM_H
#define M68K_DISASM_H

#include "m68k.h"

// Disassemble instruction at PC.
// Prints mnemonic and operands to buffer.
// Returns instruction length in bytes.
int m68k_disasm(M68kCpu* cpu, u32 pc, char* buffer, int buf_size);

#endif  // M68K_DISASM_H
