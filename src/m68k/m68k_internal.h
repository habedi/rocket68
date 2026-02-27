#ifndef M68K_INTERNAL_H
#define M68K_INTERNAL_H

#include "../../include/m68k.h"
#include <stdbool.h>


typedef struct
{
    u32 value;
    u32 address;
    bool is_reg;
    int reg_num;
    bool is_addr;
} M68kEA;


u8 m68k_read_8(M68kCpu* cpu, u32 address);
u16 m68k_read_16(M68kCpu* cpu, u32 address);
u32 m68k_read_32(M68kCpu* cpu, u32 address);

void m68k_write_8(M68kCpu* cpu, u32 address, u8 value);
void m68k_write_16(M68kCpu* cpu, u32 address, u16 value);
void m68k_write_32(M68kCpu* cpu, u32 address, u32 value);

u32 m68k_read_size(M68kCpu* cpu, u32 address, M68kSize size);
void m68k_write_size(M68kCpu* cpu, u32 address, u32 value, M68kSize size);

int m68k_ea_cycles(int mode, int reg, M68kSize size);


u16 m68k_fetch(M68kCpu* cpu);
u32 fetch_extension(M68kCpu* cpu);
M68kEA m68k_calc_ea(M68kCpu* cpu, int mode, int reg, M68kSize size);

void m68k_push_32(M68kCpu* cpu, u32 value);
u32 m68k_pop_32(M68kCpu* cpu);
void m68k_push_16(M68kCpu* cpu, u16 value);
u16 m68k_pop_16(M68kCpu* cpu);

bool m68k_check_condition(M68kCpu* cpu, int condition);


void m68k_exception(M68kCpu* cpu, int vector);


void update_flags_logic(M68kCpu* cpu, u32 result, M68kSize size);
void update_flags_add(M68kCpu* cpu, u32 src, u32 dest, u32 result, M68kSize size);
void update_flags_sub(M68kCpu* cpu, u32 src, u32 dest, u32 result, M68kSize size);
void update_flags_cmp(M68kCpu* cpu, u32 src, u32 dest, u32 result, M68kSize size);


void m68k_exec_add(M68kCpu* cpu, u16 opcode);
void m68k_exec_addq(M68kCpu* cpu, u16 opcode);
void m68k_exec_addx(M68kCpu* cpu, u16 opcode);
void m68k_exec_addi(M68kCpu* cpu, u16 opcode);
void m68k_exec_sub(M68kCpu* cpu, u16 opcode);
void m68k_exec_subq(M68kCpu* cpu, u16 opcode);
void m68k_exec_subx(M68kCpu* cpu, u16 opcode);
void m68k_exec_subi(M68kCpu* cpu, u16 opcode);
void m68k_exec_mul(M68kCpu* cpu, u16 opcode);
void m68k_exec_div(M68kCpu* cpu, u16 opcode);
void m68k_exec_cmp(M68kCpu* cpu, u16 opcode);
void m68k_exec_cmpi(M68kCpu* cpu, u16 opcode);
void m68k_exec_neg(M68kCpu* cpu, u16 opcode);
void m68k_exec_negx(M68kCpu* cpu, u16 opcode);
void m68k_exec_ext(M68kCpu* cpu, u16 opcode);
void m68k_exec_extb(M68kCpu* cpu, u16 opcode);
void m68k_exec_abcd(M68kCpu* cpu, u16 opcode);
void m68k_exec_sbcd(M68kCpu* cpu, u16 opcode);
void m68k_exec_nbcd(M68kCpu* cpu, u16 opcode);


void m68k_exec_move(M68kCpu* cpu, u16 opcode);
void m68k_exec_moveq(M68kCpu* cpu, u16 opcode);
void m68k_exec_movem(M68kCpu* cpu, u16 opcode);
void m68k_exec_movep(M68kCpu* cpu, u16 opcode);
void m68k_exec_lea(M68kCpu* cpu, u16 opcode);
void m68k_exec_pea(M68kCpu* cpu, u16 opcode);
void m68k_exec_exg(M68kCpu* cpu, u16 opcode);
void m68k_exec_swap(M68kCpu* cpu, u16 opcode);
void m68k_exec_link(M68kCpu* cpu, u16 opcode);
void m68k_exec_unlk(M68kCpu* cpu, u16 opcode);
void m68k_exec_move_sr(M68kCpu* cpu, u16 opcode);
void m68k_exec_move_usp(M68kCpu* cpu, u16 opcode);
void m68k_exec_move_ccr(M68kCpu* cpu, u16 opcode);
void m68k_exec_moves(M68kCpu* cpu, u16 opcode);


void m68k_exec_and(M68kCpu* cpu, u16 opcode);
void m68k_exec_or(M68kCpu* cpu, u16 opcode);
void m68k_exec_eor(M68kCpu* cpu, u16 opcode);
void m68k_exec_not(M68kCpu* cpu, u16 opcode);
void m68k_exec_clr(M68kCpu* cpu, u16 opcode);
void m68k_exec_tst(M68kCpu* cpu, u16 opcode);
void m68k_exec_shift(M68kCpu* cpu, u16 opcode);
void m68k_exec_andi(M68kCpu* cpu, u16 opcode);
void m68k_exec_ori(M68kCpu* cpu, u16 opcode);
void m68k_exec_eori(M68kCpu* cpu, u16 opcode);
void m68k_exec_tas(M68kCpu* cpu, u16 opcode);


void m68k_exec_btst(M68kCpu* cpu, u16 opcode);
void m68k_exec_bset(M68kCpu* cpu, u16 opcode);
void m68k_exec_bclr(M68kCpu* cpu, u16 opcode);
void m68k_exec_bchg(M68kCpu* cpu, u16 opcode);


void m68k_exec_bcc(M68kCpu* cpu, u16 opcode);
void m68k_exec_dbcc(M68kCpu* cpu, u16 opcode);
void m68k_exec_scc(M68kCpu* cpu, u16 opcode);
void m68k_exec_jmp(M68kCpu* cpu, u16 opcode);
void m68k_exec_rts(M68kCpu* cpu, u16 opcode);
void m68k_exec_trap(M68kCpu* cpu, u16 opcode);
void m68k_exec_rte(M68kCpu* cpu, u16 opcode);
void m68k_exec_chk(M68kCpu* cpu, u16 opcode);
void m68k_exec_trapv(M68kCpu* cpu, u16 opcode);
void m68k_exec_rtr(M68kCpu* cpu, u16 opcode);
void m68k_exec_stop(M68kCpu* cpu, u16 opcode);
void m68k_exec_reset(M68kCpu* cpu, u16 opcode);
void m68k_exec_movec(M68kCpu* cpu, u16 opcode);
void m68k_exec_rtd(M68kCpu* cpu, u16 opcode);
void m68k_exec_bkpt(M68kCpu* cpu, u16 opcode);


void m68k_swap_sp(M68kCpu* cpu, bool new_supervisor);

#endif
