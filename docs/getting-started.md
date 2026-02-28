# Getting Started

Rocket 68 is designed to be embedded directly into your C or C++ projects. 

## 1. Integration

The simplest way to use Rocket 68 is to just drop the source files into your build system.

**Required files:**
- `include/m68k.h` (The primary API header)
- `src/m68k/*.c` (The core execution units and opcodes)

Compile it with any C11-compliant compiler:
```bash
gcc -std=c11 -O3 -c src/m68k/*.c
```

## 2. Initialization

Before the emulator can run, you must instantiate the `M68kCpu` context object and provide it with memory.

```c
#include "m68k.h"
#include <stdlib.h>

int main() {
    // 1. Allocate your memory map (e.g., 16 MB of RAM)
    u32 mem_size = 16 * 1024 * 1024;
    u8* ram = malloc(mem_size);

    // 2. Instantiate the CPU state
    M68kCpu cpu;

    // 3. Initialize the emulator internals
    m68k_init(&cpu, ram, mem_size);

    return 0;
}
```

## 3. Hooking up the I/O Bus

Rocket 68 knows how to execute instructions, but it relies on your host application to handle physical hardware mapping. You must wire up the callbacks to interface with the outside world.

Because Rocket 68 is fully decoupled, every callback function takes the `cpu` instance as its first argument.

```c
// Define your custom bus logic
void my_wait_bus(M68kCpu* cpu, u32 address, M68kSize size) {
    // Determine how many cycles were burned accessing this memory
    cpu->cycles_remaining -= 2; 
}

int main() {
    // ... setup ...

    m68k_set_wait_bus_callback(&cpu, my_wait_bus);
    
    // ... run ...
}
```

## 4. Execution

Once initialized, the emulator simply fetches and executes instructions in a loop.
A "Reset" must be issued first to boot the CPU from its reset vectors (SSP at `0x00`, PC at `0x04`).

```c
// Force a hardware reset to load PC and SSP
m68k_reset(&cpu);

// Execute roughly 100,000 cycles
int executed = m68k_execute(&cpu, 100000);
```
