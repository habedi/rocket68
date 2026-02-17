#include <stdio.h>
#include <unistd.h>

#include "m68k.h"

int main(void) {
    M68kCpu cpu;
    u8 memory[1024];

    printf("M68k Emulator Starting...\n");

    m68k_init(&cpu, memory, sizeof(memory));
    m68k_reset(&cpu);

    // Simple loop
    for (int i = 0; i < 5; i++) {
        m68k_step(&cpu);
        printf("Step %d - PC: %08X\n", i, m68k_get_pc(&cpu));
    }

    return 0;
}
