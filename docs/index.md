# Rocket 68

<p align="center">
  <img src="https://raw.githubusercontent.com/habedi/rocket68/main/logo.svg" alt="Project Logo" width="200" />
</p>

Rocket 68 is a Motorola 68000 CPU emulator in C11.
It provides a decoupled CPU core with per-instance callbacks for memory and platform behavior.
The project includes tests and benchmarks for behavior and timing validation.

### Features

- C11 API centered on `M68kCpu`
- Per-instance callback hooks for bus timing and interrupt acknowledge
- Exception and interrupt handling paths
- Cycle accounting through `m68k_execute` and timeslice APIs
- Built-in disassembler and binary/S-record loaders
