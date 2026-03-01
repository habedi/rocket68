/**
 * @file m68k.h
 * @brief Public CPU core API for Rocket 68.
 */
#ifndef M68K_H
#define M68K_H

#include <assert.h>
#include <setjmp.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

static_assert(sizeof(uint8_t) == 1, "uint8_t must be exactly 1 byte");
static_assert(sizeof(uint16_t) == 2, "uint16_t must be exactly 2 bytes");
static_assert(sizeof(uint32_t) == 4, "uint32_t must be exactly 4 bytes");

/** @brief Unsigned 8-bit integer type. */
typedef uint8_t u8;
/** @brief Unsigned 16-bit integer type. */
typedef uint16_t u16;
/** @brief Unsigned 32-bit integer type. */
typedef uint32_t u32;
/** @brief Signed 8-bit integer type. */
typedef int8_t s8;
/** @brief Signed 16-bit integer type. */
typedef int16_t s16;
/** @brief Signed 32-bit integer type. */
typedef int32_t s32;

/** @brief Operand size in bytes. */
typedef enum { SIZE_BYTE = 1, SIZE_WORD = 2, SIZE_LONG = 4 } M68kSize;

/** @brief Forward declaration of the CPU context. */
typedef struct M68kCpu M68kCpu;

/**
 * @brief Bus wait callback.
 *
 * Called on memory accesses to model wait states.
 */
typedef void (*M68kWaitBusCallback)(M68kCpu* cpu, u32 address, M68kSize size);

/** @brief INT ACK callback return value for autovector. */
#define M68K_INT_ACK_AUTOVECTOR 0xffffffff
/** @brief INT ACK callback return value for spurious interrupt. */
#define M68K_INT_ACK_SPURIOUS 0xfffffffe

/**
 * @brief Interrupt acknowledge callback.
 *
 * Return a vector number, @ref M68K_INT_ACK_AUTOVECTOR, or @ref M68K_INT_ACK_SPURIOUS.
 */
typedef int (*M68kIntAckCallback)(M68kCpu* cpu, int level);

/** @brief Function code for user data access. */
#define M68K_FC_USER_DATA 1
/** @brief Function code for user program access. */
#define M68K_FC_USER_PROG 2
/** @brief Function code for supervisor data access. */
#define M68K_FC_SUPV_DATA 5
/** @brief Function code for supervisor program access. */
#define M68K_FC_SUPV_PROG 6
/** @brief Function code for interrupt acknowledge cycle. */
#define M68K_FC_INT_ACK 7

/** @brief Function code change callback. */
typedef void (*M68kFcCallback)(M68kCpu* cpu, unsigned int new_fc);
/** @brief Instruction hook callback executed before instruction decode/execute. */
typedef void (*M68kInstrHookCallback)(M68kCpu* cpu, u32 pc);
/** @brief Program counter changed callback. */
typedef void (*M68kPcChangedCallback)(M68kCpu* cpu, u32 new_pc);
/** @brief Reset callback. */
typedef void (*M68kResetCallback)(M68kCpu* cpu);
/** @brief TAS callback. Return non-zero to request TAS read-modify-write behavior override. */
typedef int (*M68kTasCallback)(M68kCpu* cpu);
/** @brief Illegal instruction callback. Return non-zero to signal handled/halt behavior in host
 * policy. */
typedef int (*M68kIllgCallback)(M68kCpu* cpu, int opcode);

/** @brief Effective address mode encoding used by internal helpers and opcode decoding. */
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

/**
 * @brief 68000 general register view.
 *
 * The `w` field maps to the low 16 bits of `l`.
 */
typedef union {
    u32 l;

    struct {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        u16 high;
        u16 w;
#else
        u16 w;
        u16 high;
#endif
    };
} M68kRegister;

/**
 * @brief Full CPU context for one emulated 68000 instance.
 */
typedef struct M68kCpu {
    _Alignas(64) M68kRegister d_regs[8]; /**< Data registers D0-D7. */
    M68kRegister a_regs[8]; /**< Address registers A0-A7. */
    u32 pc;                 /**< Program counter. */
    u32 ppc;                /**< Previous program counter. */
    u16 sr;                 /**< Status register. */
    u16 ir;                 /**< Current instruction register. */

    u8* memory;      /**< Bound flat memory pointer. */
    u32 memory_size; /**< Size of bound memory in bytes. */

    int irq_level;             /**< Pending interrupt level (0-7). */
    u32 usp;                   /**< User stack pointer. */
    u32 ssp;                   /**< Supervisor stack pointer shadow. */
    bool stopped;              /**< STOP state latch. */
    bool trace_pending;        /**< Trace pending latch. */
    int exception_thrown;      /**< Exception state marker. */
    bool in_address_error;     /**< Address error re-entry guard. */
    bool in_bus_error;         /**< Bus error re-entry guard. */
    u32 fault_address;         /**< Fault address latched for exception frames. */
    u16 fault_ir;              /**< Fault IR latched for exception frames. */
    u16 fault_ssw;             /**< Fault status word for exception frames. */
    bool fault_program_access; /**< Fault access type marker. */
    bool fault_valid;          /**< Fault information validity flag. */
    bool fault_trap_active;    /**< Group-0 fault trap active for the current step. */
    jmp_buf fault_trap;        /**< Non-local escape target for group-0 fault aborts. */

    u32 vbr; /**< Vector base register (model extension path). */
    u32 sfc; /**< Source function code register (model extension path). */
    u32 dfc; /**< Destination function code register (model extension path). */

    int target_cycles;    /**< Timeslice target cycles accumulator. */
    int cycles_remaining; /**< Remaining cycles in current timeslice. */

    M68kWaitBusCallback wait_bus;        /**< Wait/bus callback. */
    M68kIntAckCallback int_ack;          /**< Interrupt acknowledge callback. */
    M68kFcCallback fc_cb;                /**< Function code callback. */
    M68kInstrHookCallback instr_hook_cb; /**< Instruction hook callback. */
    M68kPcChangedCallback pc_changed_cb; /**< PC changed callback. */
    M68kResetCallback reset_cb;          /**< Reset callback. */
    M68kTasCallback tas_cb;              /**< TAS callback. */
    M68kIllgCallback illg_cb;            /**< Illegal opcode callback. */
} M68kCpu;

/** @brief Carry flag bit mask in SR. */
#define M68K_SR_C (1 << 0)
/** @brief Overflow flag bit mask in SR. */
#define M68K_SR_V (1 << 1)
/** @brief Zero flag bit mask in SR. */
#define M68K_SR_Z (1 << 2)
/** @brief Negative flag bit mask in SR. */
#define M68K_SR_N (1 << 3)
/** @brief Extend flag bit mask in SR. */
#define M68K_SR_X 0x0010
/** @brief Supervisor state bit mask in SR. */
#define M68K_SR_S 0x2000

/**
 * @brief Initialize a CPU context and bind memory.
 * @param cpu CPU context to initialize.
 * @param memory Pointer to emulated memory.
 * @param memory_size Size of @p memory in bytes.
 */
void m68k_init(M68kCpu* cpu, u8* memory, u32 memory_size);

/**
 * @brief Reset CPU state and load initial vectors from memory.
 * @param cpu CPU context.
 */
void m68k_reset(M68kCpu* cpu);

/**
 * @brief Set the program counter.
 * @param cpu CPU context.
 * @param pc New program counter value.
 */
void m68k_set_pc(M68kCpu* cpu, u32 pc);

/**
 * @brief Get the current program counter.
 * @param cpu CPU context.
 * @return Current PC.
 */
u32 m68k_get_pc(M68kCpu* cpu);

/**
 * @brief Set data register value.
 * @param cpu CPU context.
 * @param reg Data register index [0,7].
 * @param value New 32-bit value.
 */
void m68k_set_dr(M68kCpu* cpu, int reg, u32 value);

/**
 * @brief Get data register value.
 * @param cpu CPU context.
 * @param reg Data register index [0,7].
 * @return Register value, or 0 for invalid index.
 */
u32 m68k_get_dr(M68kCpu* cpu, int reg);

/**
 * @brief Set address register value.
 * @param cpu CPU context.
 * @param reg Address register index [0,7].
 * @param value New 32-bit value.
 */
void m68k_set_ar(M68kCpu* cpu, int reg, u32 value);

/**
 * @brief Get address register value.
 * @param cpu CPU context.
 * @param reg Address register index [0,7].
 * @return Register value, or 0 for invalid index.
 */
u32 m68k_get_ar(M68kCpu* cpu, int reg);

/**
 * @brief Execute one instruction with optional trace/exception processing toggle.
 * @param cpu CPU context.
 * @param check_exceptions If true, perform exception checks for this step.
 */
void m68k_step_ex(M68kCpu* cpu, bool check_exceptions);

/**
 * @brief Execute one instruction step.
 * @param cpu CPU context.
 */
void m68k_step(M68kCpu* cpu);

/**
 * @brief Execute instructions for a cycle budget.
 * @param cpu CPU context.
 * @param cycles Requested number of cycles to add to the current timeslice.
 * @return Number of cycles consumed by this call.
 */
int m68k_execute(M68kCpu* cpu, int cycles);

/**
 * @brief Get cycles consumed in the active timeslice accounting model.
 * @param cpu CPU context.
 * @return Consumed cycle count.
 */
int m68k_cycles_run(M68kCpu* cpu);

/**
 * @brief Get remaining cycles in the current timeslice.
 * @param cpu CPU context.
 * @return Remaining cycles.
 */
int m68k_cycles_remaining(M68kCpu* cpu);

/**
 * @brief Add cycles to current timeslice.
 * @param cpu CPU context.
 * @param cycles Number of cycles to add.
 */
void m68k_modify_timeslice(M68kCpu* cpu, int cycles);

/**
 * @brief End current timeslice immediately.
 * @param cpu CPU context.
 */
void m68k_end_timeslice(M68kCpu* cpu);

/**
 * @brief Set the status register.
 * @param cpu CPU context.
 * @param new_sr New status register value.
 */
void m68k_set_sr(M68kCpu* cpu, u16 new_sr);

/**
 * @brief Set pending interrupt level.
 * @param cpu CPU context.
 * @param level Interrupt level in range [0,7].
 */
void m68k_set_irq(M68kCpu* cpu, int level);

/**
 * @brief Read one byte from emulated memory.
 * @param cpu CPU context.
 * @param address Emulated address.
 * @return Read value.
 */
u8 m68k_read_8(M68kCpu* cpu, u32 address);

/**
 * @brief Read one word from emulated memory.
 * @param cpu CPU context.
 * @param address Emulated address.
 * @return Read value.
 */
u16 m68k_read_16(M68kCpu* cpu, u32 address);

/**
 * @brief Read one longword from emulated memory.
 * @param cpu CPU context.
 * @param address Emulated address.
 * @return Read value.
 */
u32 m68k_read_32(M68kCpu* cpu, u32 address);

/**
 * @brief Write one byte to emulated memory.
 * @param cpu CPU context.
 * @param address Emulated address.
 * @param value Value to write.
 */
void m68k_write_8(M68kCpu* cpu, u32 address, u8 value);

/**
 * @brief Write one word to emulated memory.
 * @param cpu CPU context.
 * @param address Emulated address.
 * @param value Value to write.
 */
void m68k_write_16(M68kCpu* cpu, u32 address, u16 value);

/**
 * @brief Write one longword to emulated memory.
 * @param cpu CPU context.
 * @param address Emulated address.
 * @param value Value to write.
 */
void m68k_write_32(M68kCpu* cpu, u32 address, u32 value);

/**
 * @brief Install wait bus callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_wait_bus_callback(M68kCpu* cpu, M68kWaitBusCallback callback);

/**
 * @brief Install interrupt acknowledge callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_int_ack_callback(M68kCpu* cpu, M68kIntAckCallback callback);

/**
 * @brief Install function code callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_fc_callback(M68kCpu* cpu, M68kFcCallback callback);

/**
 * @brief Install instruction hook callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_instr_hook_callback(M68kCpu* cpu, M68kInstrHookCallback callback);

/**
 * @brief Install PC-changed callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_pc_changed_callback(M68kCpu* cpu, M68kPcChangedCallback callback);

/**
 * @brief Install reset callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_reset_callback(M68kCpu* cpu, M68kResetCallback callback);

/**
 * @brief Install TAS callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_tas_callback(M68kCpu* cpu, M68kTasCallback callback);

/**
 * @brief Install illegal opcode callback.
 * @param cpu CPU context.
 * @param callback Callback function pointer or NULL.
 */
void m68k_set_illg_callback(M68kCpu* cpu, M68kIllgCallback callback);

/**
 * @brief Get serialized context size.
 * @return Number of bytes required for context copy buffer.
 */
unsigned int m68k_context_size(void);

/**
 * @brief Copy CPU context into an external buffer.
 *
 * Memory pointer and size are copied as part of the context payload.
 *
 * @param cpu CPU context.
 * @param dst Destination buffer with at least @ref m68k_context_size bytes.
 */
void m68k_get_context(M68kCpu* cpu, void* dst);

/**
 * @brief Restore CPU context from an external buffer.
 *
 * The bound memory pointer and memory size from the destination context are preserved.
 *
 * @param cpu CPU context.
 * @param src Source buffer with at least @ref m68k_context_size bytes.
 */
void m68k_set_context(M68kCpu* cpu, const void* src);

#endif
