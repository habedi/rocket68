# Rocket 68

Rocket 68 is a Motorola 68000 emulator written in C11.
It is designed to be embedded in host emulators and tools.

## What This Project Provides

- A CPU core centered on one struct: `M68kCpu`
- Flat-memory emulation with 8/16/32-bit big-endian access helpers
- Instruction stepping (`m68k_step`) and cycle-budget execution (`m68k_execute`)
- Per-instance callbacks for bus wait states, interrupt acknowledge, function code, and debug hooks
- Exception handling (including bus/address-error frames)
- Optional loader utilities (`m68k_load_srec`, `m68k_load_bin`)
- Optional disassembler utility (`m68k_disasm`)

## Important Scope Notes

- The public API is in `include/m68k.h`.
- Address accesses are masked to 24-bit internally (`0x00FFFFFF`) before bounds checks.
- The project currently includes some post-68000 opcodes (`MOVEC`, `MOVES`, `RTD`, and `BKPT`), but does not expose a full CPU-model selection API yet.

## Project Layout

- `include/`: public headers (`m68k.h`, `loader.h`, `disasm.h`, `rocket68.h`)
- `src/m68k/`: CPU core and opcode handlers
- `src/loader.c`: S-record and raw binary loaders
- `src/disasm.c`: disassembler implementation
- `tests/`: unit, regression, and JSON compatibility tests
- `benches/`: Rocket68 vs Musashi benchmarks

## Documentation Map

- [Getting Started](getting-started.md): build + first runnable program
- [Examples](examples.md): focused usage patterns
- [API Reference](api-reference.md): function groups and behavior notes
- [Known Limitations](limitations.md): current scope limits and integration caveats

## Version

- `ROCKET68_VERSION_STR`: see `include/rocket68.h`
