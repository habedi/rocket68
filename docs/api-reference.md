# API Reference

The primary interaction with Rocket 68 uses `M68kCpu` and API functions from `include/m68k.h`.
Most API functions take `M68kCpu* cpu`. The exception is `m68k_context_size(void)`.

---

## Core Structures

### `M68kCpu`

`M68kCpu` is a 64-byte aligned structure that stores one emulated CPU instance.

```c
typedef struct M68kCpu {
    M68kRegister d_regs[8]; // Data registers D0-D7
    M68kRegister a_regs[8]; // Address registers A0-A7
    u32 pc;                 // Program Counter
    u32 ppc;                // Previous Program Counter
    u16 sr;                 // Status Register
    u16 ir;                 // Current instruction register
    // ... Internal execution state
} __attribute__((aligned(64))) M68kCpu;
```

### `M68kRegister`

`M68kRegister` stores one register value with 32-bit and 16-bit access.
The `w` field maps to the low 16 bits.

```c
typedef union {
    u32 l;
    struct {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        u16 high;
        u16 w;
#else
        u16 w;    // low 16 bits
        u16 high; // high 16 bits
#endif
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

Initializes CPU state and binds the CPU instance to a flat memory buffer.

### `m68k_reset`

```c
void m68k_reset(M68kCpu* cpu);
```

Executes a reset sequence.
Loads `A7` (supervisor stack pointer) from `0x00000000` and `PC` from `0x00000004`.

### `m68k_execute`

```c
int m68k_execute(M68kCpu* cpu, int cycles);
```

Executes instructions until the current timeslice is exhausted.
Returns the number of cycles consumed by this call.

### Other frequently used APIs

```c
void m68k_set_pc(M68kCpu* cpu, u32 pc);
u32 m68k_get_pc(M68kCpu* cpu);
void m68k_set_sr(M68kCpu* cpu, u16 new_sr);
void m68k_set_irq(M68kCpu* cpu, int level);
int m68k_cycles_run(M68kCpu* cpu);
int m68k_cycles_remaining(M68kCpu* cpu);
```

---

## I/O Callbacks

Callbacks are stored per CPU instance. They are not global.

```c
// Emulate wait states on the bus
void m68k_set_wait_bus_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, u32 addr, M68kSize sz));

// Respond to Interrupt Acknowledge cycles
void m68k_set_int_ack_callback(M68kCpu* cpu, int (*fn)(M68kCpu* c, int level));

// Respond to Function Code (FC) changes
void m68k_set_fc_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, unsigned int fc));

// Trace PC changes
void m68k_set_pc_changed_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, u32 pc));

// Other callbacks
void m68k_set_instr_hook_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c, u32 pc));
void m68k_set_reset_callback(M68kCpu* cpu, void (*fn)(M68kCpu* c));
void m68k_set_tas_callback(M68kCpu* cpu, int (*fn)(M68kCpu* c));
void m68k_set_illg_callback(M68kCpu* cpu, int (*fn)(M68kCpu* c, int opcode));
```
