#ifndef M68K_LOADER_H
#define M68K_LOADER_H

#include <stdbool.h>

#include "m68k.h"

bool m68k_load_srec(M68kCpu* cpu, const char* filename);
bool m68k_load_bin(M68kCpu* cpu, const char* filename, u32 address);

#endif
