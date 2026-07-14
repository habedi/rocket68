/* A minimal setjmp.h for WASM builds on Zig 0.16 and later, shadowing
 * musl's header. musl's setjmp.h requires the wasm exception-handling
 * feature, and Zig 0.16.0's bundled clang crashes while compiling musl's
 * EH-based setjmp runtime (libc-top-half/musl/src/setjmp/wasm32/rt.c).
 * The implementations are the stubs in src/wasm_compat/setjmp_stubs.c. */
#ifndef _SETJMP_H
#define _SETJMP_H

typedef long jmp_buf[8];

int setjmp(jmp_buf env);
_Noreturn void longjmp(jmp_buf env, int val);

#endif
