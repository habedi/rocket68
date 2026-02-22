#ifndef M68K_DISASM_H
#define M68K_DISASM_H

#include "m68k.h"

int m68k_disasm(M68kCpu* cpu, u32 pc, char* buffer, int buf_size);

#endif
