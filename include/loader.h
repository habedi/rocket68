#ifndef M68K_LOADER_H
#define M68K_LOADER_H

#include <stdbool.h>

#include "m68k.h"

// Load an S-Record file into the CPU memory.
// Returns true on success, false on failure.
bool m68k_load_srec(M68kCpu* cpu, const char* filename);

// Load a flat binary file into the CPU memory at a specific address.
// Returns true on success, false on failure.
bool m68k_load_bin(M68kCpu* cpu, const char* filename, u32 address);

#endif  // M68K_LOADER_H
