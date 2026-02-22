#ifndef M68K_H
#define M68K_H

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;

typedef enum { SIZE_BYTE = 1, SIZE_WORD = 2, SIZE_LONG = 4 } M68kSize;

typedef enum {
    ADDR_MODE_DATA_REG_DIRECT,
    ADDR_MODE_ADDR_REG_DIRECT,
    ADDR_MODE_ADDR_REG_INDIRECT,
    ADDR_MODE_ARI_POSTINC,
    ADDR_MODE_ARI_PREDEC,
    ADDR_MODE_ARI_DISP,
    ADDR_MODE_ARI_INDEX,
    ADDR_MODE_ABS_SHORT,
    ADDR_MODE_ABS_LONG,
    ADDR_MODE_PC_DISP,
    ADDR_MODE_PC_INDEX,
    ADDR_MODE_IMMEDIATE
} M68kAddrMode;

typedef struct {
    u32 d_regs[8];
    u32 a_regs[8];
    u32 pc;
    u16 sr;

    u8* memory;
    u32 memory_size;

    int irq_level;
    u32 usp;
    u32 ssp;
    bool stopped;
    bool trace_pending;
    int exception_thrown;

    u32 vbr;
    u32 sfc;
    u32 dfc;
} M68kCpu;

#define M68K_SR_C (1 << 0)
#define M68K_SR_V (1 << 1)
#define M68K_SR_Z (1 << 2)
#define M68K_SR_N (1 << 3)
#define M68K_SR_X 0x0010
#define M68K_SR_S 0x2000

void m68k_init(M68kCpu* cpu, u8* memory, u32 memory_size);
void m68k_reset(M68kCpu* cpu);
void m68k_set_pc(M68kCpu* cpu, u32 pc);
u32 m68k_get_pc(M68kCpu* cpu);
void m68k_set_dr(M68kCpu* cpu, int reg, u32 value);
u32 m68k_get_dr(M68kCpu* cpu, int reg);
void m68k_set_ar(M68kCpu* cpu, int reg, u32 value);
u32 m68k_get_ar(M68kCpu* cpu, int reg);

int m68k_step_ex(M68kCpu* cpu, bool check_exceptions);
int m68k_step(M68kCpu* cpu);

void m68k_set_sr(M68kCpu* cpu, u16 new_sr);

void m68k_set_irq(M68kCpu* cpu, int level);

u8 m68k_read_8(M68kCpu* cpu, u32 address);
u16 m68k_read_16(M68kCpu* cpu, u32 address);
u32 m68k_read_32(M68kCpu* cpu, u32 address);

void m68k_write_8(M68kCpu* cpu, u32 address, u8 value);
void m68k_write_16(M68kCpu* cpu, u32 address, u16 value);
void m68k_write_32(M68kCpu* cpu, u32 address, u32 value);

#endif
