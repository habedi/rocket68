# Real-World Scenario

Below is an example of setting up a tiny, self-contained run cycle for `rocket68`. In a real emulator like an Amiga or Sega Genesis, your bus and
interrupt callbacks would coordinate access to video RAM, cartridges, or sound chips instead of using a flat RAM allocation.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "m68k.h"

// 1) Define a simple interrupt system
int my_int_ack(M68kCpu* cpu, int level) {
    printf("Acknowledged CPU interrupt at level %d!\n", level);
    return M68K_INT_ACK_AUTOVECTOR;
}

// 2) Define illegal instruction traps
int my_illg_cb(M68kCpu* cpu, int opcode) {
    printf("FATAL: CPU halted on opcode 0x%04X at PC:%08X\n", opcode, cpu->pc);
    return 1; // Halt
}

int main() {
    u32 memory_size = 64 * 1024; // 64 KB
    u8* ram = calloc(memory_size, 1);

    // Mock an instruction at address 0x1000:
    // 0x1000: NOP (0x4e71)
    // 0x1002: STOP #$2000 (0x4e72, 0x2000)
    ram[0x1000] = 0x4e; ram[0x1001] = 0x71; 
    ram[0x1002] = 0x4e; ram[0x1003] = 0x72; 
    ram[0x1004] = 0x20; ram[0x1005] = 0x00;

    M68kCpu core;
    m68k_init(&core, ram, memory_size);

    // Attach callbacks
    m68k_set_int_ack_callback(&core, my_int_ack);
    m68k_set_illg_callback(&core, my_illg_cb);

    // Simulate bootloader dropping us at 0x1000
    m68k_set_pc(&core, 0x1000);
    m68k_set_sr(&core, 0x2700); // Supervisor, interrupts masked

    int cycles_burned = m68k_execute(&core, 100);

    printf("Executed %d cycles. CPU PC is now 0x%08X\n", cycles_burned, core.pc);
    free(ram);
    return 0;
}
```
