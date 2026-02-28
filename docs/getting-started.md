# Getting Started

Rocket 68 can run standalone or inside a host emulator.

## 1. Integration

You can add Rocket 68 source files to your build system.

**Required files:**

- `include/m68k.h` (primary CPU API)
- `src/m68k/*.c` (CPU core implementation)

Compile it with any C11-compliant compiler:

```bash
gcc -std=c11 -O3 -Iinclude -c src/m68k/*.c
```

## 2. Initialization

Before execution, create `M68kCpu` and bind memory.

```c
#include "m68k.h"
#include <stdlib.h>

int main() {
    // 1. Allocate your memory map (e.g., 16 MB of RAM)
    u32 mem_size = 16 * 1024 * 1024;
    u8* ram = malloc(mem_size);
    if (!ram) {
        return 1;
    }

    // 2. Instantiate the CPU state
    M68kCpu cpu;

    // 3. Initialize CPU state
    m68k_init(&cpu, ram, mem_size);

    return 0;
}
```

## 3. Hooking up the I/O Bus

Rocket 68 executes instructions and calls host callbacks for bus and platform behavior.

Each callback takes `M68kCpu* cpu` as its first argument.

```c
// Define bus timing behavior
void my_wait_bus(M68kCpu* cpu, u32 address, M68kSize size) {
    (void)address;
    (void)size;
    // Account for memory access wait states
    cpu->cycles_remaining -= 2; 
}

int main() {
    // ... setup ...

    m68k_set_wait_bus_callback(&cpu, my_wait_bus);
    
    // ... run ...
}
```

## 4. Execution

After initialization, start execution by either:

1. calling `m68k_reset` to load vectors from memory address `0x00000000` and `0x00000004`, or
2. setting registers manually (`m68k_set_pc`, `m68k_set_sr`, and stack state as needed).

```c
// Load initial A7/PC vectors from memory
m68k_reset(&cpu);

// Execute roughly 100,000 cycles
int executed = m68k_execute(&cpu, 100000);
```

## 5. Important Files

Here is a list of the important header files provided by the project in the `include/` directory:

| File         | Description                                                                       |
|--------------|-----------------------------------------------------------------------------------|
| `m68k.h`     | The primary API header for the CPU core execution, callbacks, and management.     |
| `disasm.h`   | The built-in instruction disassembler API for formatting opcodes into text.       |
| `loader.h`   | Utilities for loading raw binaries and Motorola S-record files into memory.       |
| `rocket68.h` | A convenience header that includes all the necessary components for the emulator. |
