# Rocket 68

<p align="center">
  <img src="https://raw.githubusercontent.com/habedi/rocket68/main/logo.svg" alt="Project Logo" width="200" />
</p>

**Rocket 68** is a modern, fast, and cycle-accurate emulator core for the Motorola 68000 microprocessor, written entirely in standard C11. 

Designed for both embedded systems and advanced multi-threaded emulation environments, Rocket 68 prioritizes **cache locality**, **safety**, and **architectural decoupling** to serve as the perfect powerhouse for your next retro console or homebrew project.

---

## Features

- **Strict C11 Modernization:** Built using standard `<assert.h>` sizing checks, anonymous register unions for safe endianness abstraction, and type-safe `_Generic` macros for clean memory access.
- **Architecturally Decoupled:** Unlike older emulators that rely on messy global states, Rocket 68 explicitly passes an `M68kCpu*` context instance to every single cycle and I/O callback. You can securely run a dozen virtual machines in the same process on different threads without cross-contamination.
- **Cache Aligned:** The core `M68kCpu` struct is enforced with 64-byte `__attribute__((aligned(64)))` boundary padding to prevent cache-line thrashing and hardware penalties during the hot emulation loop.
- **Cycle-Accurate Precision:** Carefully tested timings and execution sequences mirroring the exact pipeline delays of real M68k silicon.
- **Save State Ready:** Native context block serialization for rapid game freeze and resume capabilities.

## Why C11?

Rocket 68 was heavily refactored from legacy C99 definitions into standardized C11 functionality. This allows developers to construct concurrent emulation environments using `<stdatomic.h>` and `_Thread_local` without worrying about legacy compiler undefined behaviors or clashing global execution vectors.

---

## Next Steps
Head over to the [Getting Started](getting-started.md) guide to learn how to hook the emulator up to your own memory maps and I/O bus!
