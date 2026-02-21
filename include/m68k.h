#ifndef M68K_H
#define M68K_H

#include <stdbool.h>
#include <stdint.h>

// Fixed-size types for 68000
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

// Operation Size
typedef enum { SIZE_BYTE = 1, SIZE_WORD = 2, SIZE_LONG = 4 } M68kSize;

// Addressing Modes
typedef enum {
    ADDR_MODE_DATA_REG_DIRECT,    // Dn
    ADDR_MODE_ADDR_REG_DIRECT,    // An
    ADDR_MODE_ADDR_REG_INDIRECT,  // (An)
    ADDR_MODE_ARI_POSTINC,        // (An)+
    ADDR_MODE_ARI_PREDEC,         // -(An)
    ADDR_MODE_ARI_DISP,           // (d16,An)
    ADDR_MODE_ARI_INDEX,          // (d8,An,Xn)
    ADDR_MODE_ABS_SHORT,          // (xxx).W
    ADDR_MODE_ABS_LONG,           // (xxx).L
    ADDR_MODE_PC_DISP,            // (d16,PC)
    ADDR_MODE_PC_INDEX,           // (d8,PC,Xn)
    ADDR_MODE_IMMEDIATE           // #<data>
} M68kAddrMode;

// Processor State
typedef struct {
    u32 d_regs[8];  // Data registers D0-D7
    u32 a_regs[8];  // Address registers A0-A7 (A7 is SP)
    u32 pc;         // Program Counter
    u16 sr;         // Status Register

    // Memory interface
    u8* memory;
    u32 memory_size;

    // Internal state
    int irq_level;         // Current pending IRQ level (0-7)
    u32 usp;               // User Stack Pointer
    u32 ssp;               // Supervisor Stack Pointer
    bool stopped;          // CPU halted by STOP instruction
    bool trace_pending;    // Trace exception pending after current instruction
    int exception_thrown;  // Tracks if an exception occurred during execution

    // 68010+ control registers
    u32 vbr;  // Vector Base Register (68010+)
    u32 sfc;  // Source Function Code (68010+)
    u32 dfc;  // Destination Function Code (68010+)
} M68kCpu;

// Status Register Flags
#define M68K_SR_C (1 << 0)  // Carry
#define M68K_SR_V (1 << 1)  // Overflow
#define M68K_SR_Z (1 << 2)  // Zero
#define M68K_SR_N (1 << 3)  // Negative
#define M68K_SR_X 0x0010    // Extend
#define M68K_SR_S 0x2000    // Supervisor Bit (Bit 13)

// Function Prototypes
void m68k_init(M68kCpu* cpu, u8* memory, u32 memory_size);
void m68k_reset(M68kCpu* cpu);
void m68k_set_pc(M68kCpu* cpu, u32 pc);
u32 m68k_get_pc(M68kCpu* cpu);
void m68k_set_dr(M68kCpu* cpu, int reg, u32 value);  // Set Data Register
u32 m68k_get_dr(M68kCpu* cpu, int reg);              // Get Data Register
void m68k_set_ar(M68kCpu* cpu, int reg, u32 value);  // Set Address Register
u32 m68k_get_ar(M68kCpu* cpu, int reg);              // Get Address Register

// Execution
// Executes one instruction. Returns cycles consumed (stub for now)
int m68k_step_ex(M68kCpu* cpu, bool check_exceptions);
int m68k_step(M68kCpu* cpu);

// Status Register
void m68k_set_sr(M68kCpu* cpu, u16 new_sr);

// Interrupts
// Sets the current interrupt level (0-7). 0 means no interrupt.
void m68k_set_irq(M68kCpu* cpu, int level);

// Memory Access (Big Endian)
u8 m68k_read_8(M68kCpu* cpu, u32 address);
u16 m68k_read_16(M68kCpu* cpu, u32 address);
u32 m68k_read_32(M68kCpu* cpu, u32 address);

void m68k_write_8(M68kCpu* cpu, u32 address, u8 value);
void m68k_write_16(M68kCpu* cpu, u32 address, u16 value);
void m68k_write_32(M68kCpu* cpu, u32 address, u32 value);

#endif  // M68K_H
