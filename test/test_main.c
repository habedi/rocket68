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
    test_data_manipulation();  // CLR, NEG, EXT, SWAP, MUL/DIV
    test_shift_rotate();
    test_exceptions();  // TRAP, RTE
    test_cmp_tst();
    test_lea_pea();
    test_stack_frame();        // LINK, UNLK
    test_dbcc_scc();           // DBcc, Scc
    test_addressing_mode_6();  // Index addressing
    test_exg();
    test_movem();  // MOVEM
    test_adda_suba();
    test_addx_subx();
    test_pcrel();

    // New Tests
    test_bcd();
    test_misc_control();
    test_interrupts();

    // Regression Tests for Bug Fixes
    test_regression_postinc_predec_byte();
    test_regression_reset_vectors();
    test_regression_jmp_dispatch();
    test_regression_div_by_zero();
    test_regression_addx_subx_zflag();
    test_regression_sbcd();
    test_regression_nbcd();
    test_regression_stop_privilege();
    test_regression_ext_dispatch();

    // New Instruction & Infrastructure Tests
    test_bit_manipulation();
    test_immediate_alu();
    test_negx();
    test_new_tas();
    test_movep();
    test_usp_switching();
    test_address_error();
    test_trace_mode();
    test_stopped_state();

    // Integration Tests
    test_fibonacci();
    test_string_copy();
    test_exception_handler();

    test_nop_bsr_rtr();
    test_logic_ccr_sr();
    test_logic_not();

    // Loader Tests
    extern void run_loader_tests();
    run_loader_tests();

    return 0;
}
