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

typedef void (*M68kWaitBusCallback)(u32 address, M68kSize size);
extern M68kWaitBusCallback wait_bus;

#define M68K_INT_ACK_AUTOVECTOR 0xffffffff
#define M68K_INT_ACK_SPURIOUS 0xfffffffe

typedef int (*M68kIntAckCallback)(int level);
extern M68kIntAckCallback int_ack;

#define M68K_FC_USER_DATA 1
#define M68K_FC_USER_PROG 2
#define M68K_FC_SUPV_DATA 5
#define M68K_FC_SUPV_PROG 6
#define M68K_FC_INT_ACK 7

typedef void (*M68kFcCallback)(unsigned int new_fc);
extern M68kFcCallback fc_cb;

typedef void (*M68kInstrHookCallback)(u32 pc);
extern M68kInstrHookCallback instr_hook_cb;

typedef void (*M68kPcChangedCallback)(u32 new_pc);
extern M68kPcChangedCallback pc_changed_cb;

typedef void (*M68kResetCallback)(void);
extern M68kResetCallback reset_cb;

typedef int (*M68kTasCallback)(void);
extern M68kTasCallback tas_cb;

typedef int (*M68kIllgCallback)(int opcode);
extern M68kIllgCallback illg_cb;

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

    int target_cycles;
    int cycles_remaining;
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

void m68k_step_ex(M68kCpu* cpu, bool check_exceptions);
void m68k_step(M68kCpu* cpu);
int m68k_execute(M68kCpu* cpu, int cycles);

int m68k_cycles_run(M68kCpu* cpu);
int m68k_cycles_remaining(M68kCpu* cpu);
void m68k_modify_timeslice(M68kCpu* cpu, int cycles);
void m68k_end_timeslice(M68kCpu* cpu);

void m68k_set_sr(M68kCpu* cpu, u16 new_sr);

void m68k_set_irq(M68kCpu* cpu, int level);

u8 m68k_read_8(M68kCpu* cpu, u32 address);
u16 m68k_read_16(M68kCpu* cpu, u32 address);
u32 m68k_read_32(M68kCpu* cpu, u32 address);

void m68k_write_8(M68kCpu* cpu, u32 address, u8 value);
void m68k_write_16(M68kCpu* cpu, u32 address, u16 value);
void m68k_write_32(M68kCpu* cpu, u32 address, u32 value);

void m68k_set_wait_bus_callback(M68kWaitBusCallback callback);
void m68k_set_int_ack_callback(M68kIntAckCallback callback);
void m68k_set_fc_callback(M68kFcCallback callback);
void m68k_set_instr_hook_callback(M68kInstrHookCallback callback);
void m68k_set_pc_changed_callback(M68kPcChangedCallback callback);
void m68k_set_reset_callback(M68kResetCallback callback);
void m68k_set_tas_callback(M68kTasCallback callback);
void m68k_set_illg_callback(M68kIllgCallback callback);

unsigned int m68k_context_size(void);
void m68k_get_context(M68kCpu* cpu, void* dst);
void m68k_set_context(M68kCpu* cpu, const void* src);

#endif
