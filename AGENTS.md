# AGENTS.md

This file provides guidance to coding agents collaborating on this repository.

## Project Overview

Rocket 68 is a modern, fast, and cycle-accurate emulator core for the Motorola 68000 microprocessor, written entirely in standard C11.
It provides:

- High-performance, cache-aligned execution
- Cycle-accurate pipeline precision
- Full architectural decoupling via instance-based callbacks
- C11 modernizations (`_Static_assert`, `_Generic`, and anonymous unions)

## Project Vision

A portable Motorola 68000 core suitable for modern multi-threaded emulator platforms, embedded systems, and retro game development.

## Project Requirements

- Always use English in code, examples, and comments.
- Features should be implemented concisely, maintainably, and efficiently, prioritizing hot-loop performance.
- Code is not just for execution, but also for readability. Keep the architecture decoupled; avoid global state.
- Only add meaningful comments and tests.

## Architecture

The project is organized as a standard C project:

- `include/` - Public header files (`m68k.h` or `rocket68.h`). This is the decoupled API surface.
- `src/` - Core emulation implementations.
- `src/m68k/` - The CPU execution and opcode logic (`m68k.c`, `ops_*.c`).
- `test/` - Unit and integration tests.
- `benches/` - Performance benchmarks.
- `docs/` - MkDocs project documentation layout.
- `external/` - Third-party libraries or reference emulators (e.g., Musashi) for testing.

## Common Development Commands

### C Development

* Build tests: `make test`
* Run benchmarks: `make bench`
* Clean build artifacts: `make clean`
* The project uses GCC and Clang with the `-std=c11` flag. Make sure compatibility across both compilers.

### Documentation Development

* Build documentation: `make docs`
* Serve documentation locally: `make docs-serve`

## Key Technical Details

1. **Fully Decoupled**: The entire CPU state is encapsulated within the `M68kCpu` struct. All I/O callbacks (memory read/write, interrupts, TAS, etc.)
   are bound to the instance, not the global scope.
2. **C11 Modernized**:
    - `M68kRegister` uses anonymous unions for native endianness handling (`.l` vs `.w`).
    - `_Generic` macros (`m68k_read`, `m68k_write`) abstract data width logic.
    - The `M68kCpu` struct is padded to 64-byte boundaries using `__attribute__((aligned(64)))` to prevent cache thrashing.
3. **Cycle Accuracy**: Timings are carefully managed within the `M68kCpu->cycles_remaining` variable for every opcode simulated.

## Development Notes

- All public APIs should have comprehensive documentation with examples in the `docs/` folder.
- Performance-critical code in `m68k_execute` must remain free of heavy locks or unnecessary branching.
- When expanding features, always verify against the legacy tests (`make test`). Tests should *never* be broken by a commit.

## Development Tips

Code standards:

* Be mindful of memory alignment and cache locality:
    * Do not randomly add fields to `M68kCpu` without considering cache line boundaries.
    * Use standard C data types (`u8`, `u16`, and `u32`) enforced by `<assert.h>`.

Tests:

* When writing unit tests for instructions, ensure you initialize the `M68kCpu` state fully and mock any required memory fetches accurately within the
  decoupled callback handlers.
* Integration tests should verify complete sequence executions (e.g., simulating simple ROM bootloaders or math subroutine execution).

## Review Guidelines

Please note that the attention of contributors and maintainers is the MOST valuable resource.
Less is more: focus on the most important aspects.

- Your review output SHOULD be concise and clear.
- You SHOULD only highlight P0 and P1 level issues, such as severe bugs, cache invalidation risks, pipeline inaccuracy, or decoupling violations (
  e.g., introducing a global variable).
- You MUST not reiterate detailed changes in your review.
- You MUST not repeat aspects of the PR that are already well done.

Please consider the following when reviewing code contributions.

### C API Design

* Design public APIs so they can be evolved easily in the future without breaking changes. Keep the main interaction strictly through the `M68kCpu`
  context pointer.
* When adding new I/O callbacks or hooks, strictly enforce passing the `M68kCpu* cpu` as the first argument to maintain multi-instance capabilities.

### Testing

* Ensure all new opcodes or execution workflows have corresponding tests. **We do not merge code without tests.**
* Run `make bench` on any core loop changes to ensure 68k emulation speeds do not regress.

### Documentation

* New features must include updates to the MkDocs files (`docs/`). Code examples in documentation must compile cleanly against the current C11 API.
