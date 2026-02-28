# API Reference

The primary interaction with Rocket 68 revolves around the `M68kCpu` struct and its management functions. Since Rocket is purely decoupled, all API
signatures require a pointer to the context being emulated.

---

## Core Structures

### `M68kCpu`

The `M68kCpu` is a 64-byte aligned data structure representing the entire state of a single Motorola 68000 microchip.

```c
typedef struct M68kCpu {
    M68kRegister d_regs[8]; // Data registers D0-D7
    M68kRegister a_regs[8]; // Address registers A0-A7
    u32 pc;                 // Program Counter
    u16 sr;                 // Status Register
    // ... Internal execution state
} __attribute__((aligned(64))) M68kCpu;
```

### `M68kRegister`

A standard C11 anonymous union used to quickly extract 16-bit or 32-bit components natively based on platform endianness.

```c
typedef union {
    u32 l;
    struct {
        u16 w;
    };
} M68kRegister;
```

Usage:

- `cpu->d_regs[0].l = 0xFFFF; // Write 32 bits`
- `cpu->d_regs[0].w = 0x00FF; // Write lower 16 bits`

---

## Functions

### `m68k_init`

```c
void m68k_init(M68kCpu* cpu, u8* memory, u32 memory_size);
```

Initializes the CPU struct and binds it to a standard flat byte array representing physical RAM boundaries.

### `m68k_reset`

```c
void m68k_reset(M68kCpu* cpu);
```

Executes a hard reset. Populates the `pc` and `ssp` registers by fetching 32-bit vectors from address `0x00` and `0x04` respectively.
Executes a hard reset. Populates the `ssp` and `pc` registers by fetching 32-bit vectors from address `0x00` and `0x04` respectively.

### `m68k_execute`

```c
int m68k_execute(M68kCpu* cpu, int cycles);
```

Burns CPU cycles by executing instructions. The emulator will run until `cycles` drops to or below zero. Returns the total number of cycles actually
consumed.

---

## I/O Callbacks

Rocket 68 achieves architectural decoupling through callbacks installed directly on the CPU structure. None of these are globally bound.

```c
// Emulate wait states on the bus
void m68k_set_wait_bus_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, u32 addr, M68kSize sz));

// Respond to Interrupt Acknowledge cycles
void m68k_set_int_ack_callback(M68kCpu* cpu, int (*fn)(M68kCpu* c, int level));

// Respond to Function Code (FC) changes
void m68k_set_fc_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, unsigned int fc));

// Trace PC changes
void m68k_set_pc_changed_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, u32 pc));
```
