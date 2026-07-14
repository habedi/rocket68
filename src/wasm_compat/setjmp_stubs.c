/* setjmp/longjmp stubs for WASM builds on Zig 0.16 and later.
 *
 * The core uses setjmp/longjmp to abort the current instruction when a
 * group-0 fault (bus or address error) is raised during execution. With
 * these stubs, setjmp always returns 0 and longjmp traps the module, so
 * such a fault halts the module instead of aborting the instruction. */
#include <setjmp.h>

int setjmp(jmp_buf env) {
    (void)env;
    return 0;
}

_Noreturn void longjmp(jmp_buf env, int val) {
    (void)env;
    (void)val;
    __builtin_trap();
}
