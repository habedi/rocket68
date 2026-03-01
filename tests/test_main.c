#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m68k.h"
#include "test_m68k.h"

int main() {
    test_initialization();
    test_register_access();
    test_memory_access();
    test_fetch();
    test_move();
    test_arithmetic();
    test_control_flow();
    test_logic();
    test_data_manipulation();
    test_shift_rotate();
    test_exceptions();
    test_cmp_tst();
    test_lea_pea();
    test_stack_frame();
    test_dbcc_scc();
    test_addressing_mode_6();
    test_exg();
    test_movem();
    test_adda_suba();
    test_addx_subx();
    test_pcrel();

    test_bcd();
    test_misc_control();
    test_interrupts();
    test_int_ack();
    test_fc();
    test_hooks();
    test_timeslice();
    test_serialization();

    test_regression_postinc_predec_byte();
    test_regression_reset_vectors();
    test_regression_jmp_dispatch();
    test_regression_div_by_zero();
    test_regression_addx_subx_zflag();
    test_regression_sbcd();
    test_regression_nbcd();
    test_regression_stop_privilege();
    test_regression_ext_dispatch();
    test_regression_no_spurious_ea_read_on_write();
    test_regression_stop_rte_resume_pc();
    test_regression_execute_wakes_stopped_cpu_on_irq();
    test_regression_exception_guards_are_per_instance();
    test_regression_masked_irq_does_not_exit_stop();
    test_regression_interrupt_stacks_original_sr();
    test_regression_clr_illegal_mode_traps();

    test_bit_manipulation();
    test_immediate_alu();
    test_negx();
    test_new_tas();
    test_movep();
    test_usp_switching();
    test_address_error();
    test_trace_mode();
    test_stopped_state();

    test_fibonacci();
    test_string_copy();
    test_exception_handler();

    test_nop_bsr_rtr();
    test_logic_ccr_sr();
    test_logic_not();

    test_movec();
    test_trapv();
    test_rtd();
    test_bkpt();
    test_move_sr();
    test_move_ccr();
    test_move_usp();
    test_moves();
    test_andi_ori_eori();

    test_regression_set_context_preserves_callbacks();
    test_regression_ori_ccr_preserves_upper_bits();
    test_regression_eori_ccr_preserves_upper_bits();
    test_regression_divu_overflow_n_flag();
    test_regression_divs_overflow_n_flag();
    test_regression_chk_only_modifies_n();
    test_regression_write_oob_triggers_bus_error();

    extern void run_loader_tests();
    run_loader_tests();

    return 0;
}
