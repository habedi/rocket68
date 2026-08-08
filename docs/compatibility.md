# Compatibility Notes

This page lists current compatibility notes and scope limits based on the current codebase.

## CPU Model Scope

- The core exposes one execution profile through `M68kCpu`.
- There is no public API to select CPU model variants (for example 68010/68020 mode switches).
- Some later-family instructions exist (`MOVEC`, `MOVES`, `RTD`, `BKPT`), but model behavior is not fully parameterized.

## Address Space and Memory Model

- All memory accesses are against one flat memory buffer (`cpu->memory`, `cpu->memory_size`).
- Addresses are masked to 24-bit (`address & 0x00FFFFFF`) before bounds checks.
- Per-access host memory callbacks (`m68k_set_read8_callback` and related setters) can replace flat-buffer access for each width; see the API reference for details.

## Callback Behavior Notes

- `fc_callback` is emitted for memory reads/writes and instruction fetches.
- The interrupt acknowledge path emits the FC callback with `M68K_FC_INT_ACK` before the vector is resolved, for vectored and autovectored responses alike.
- `pc_changed_callback` is triggered when PC is changed through `m68k_set_pc`.
- Direct PC writes (for example in `m68k_reset` and `m68k_fetch`) do not call `pc_changed_callback`.
- `reset_callback` is tied to execution of the `RESET` instruction, not to `m68k_reset()`.
- `illg_callback` fires before an illegal-instruction exception (vector 4); a nonzero return suppresses the exception. Line-A and line-F opcodes (vectors 10 and 11) do not invoke it.

## Control Registers and Exception Base

- `VBR`, `SFC`, and `DFC` fields exist and are accessible through `MOVEC`.
- Exception vector fetch uses `VBR + vector * 4`; `m68k_reset` clears `VBR` to zero, matching 68010-class reset behavior, so the base is zero unless a program moves it.
- `SFC`/`DFC` values are stored but not used to drive bus access behavior.

## Context Save/Restore Format

- `m68k_get_context` / `m68k_set_context` copy raw `M68kCpu` struct bytes.
- The blob format should be treated as build-dependent (compiler/ABI/version sensitive), not a stable cross-version interchange format.
- `m68k_set_context` preserves destination instance memory binding and installed callbacks.

## Loader and Disassembler Notes

- `m68k_load_srec` and `m68k_load_bin` return `false` only when file open fails.
- `m68k_load_bin` reports the number of bytes written into emulated memory through its optional `size_out` argument; a load that runs past bound memory still returns `true`, and the reported size reveals the truncation.
- `m68k_load_srec` reports malformed lines and continues parsing.
- S-record checksums are validated; a record whose checksum does not match is reported to `stderr` and skipped, and parsing continues with the next record.
- Loaders write directly into bound flat memory; they do not run emulated bus cycles, invoke host memory callbacks, or raise bus errors. When a record reaches an out-of-range address, the first out-of-range byte is reported to `stderr`, the rest of that record is skipped, and parsing continues with the next record.
- S-record entry records (`S7/S8/S9`) set the program counter through `m68k_set_pc`.
- `m68k_disasm` returns instruction bytes consumed; unsupported decode cases may still produce `???` output text.

## JSON Compatibility Harness

- The JSON compatibility runner (`tests/test_json.c`) has a relaxed default for exception-path state checks.
- Tests skipped under the relaxed default are counted and reported per file.
- Strict exception-path checking is available with `ROCKET68_JSON_STRICT=1`.
- Per-test cycle count verification is available with `ROCKET68_JSON_CYCLES=1`.
- This behavior is test-harness policy, not a runtime core API toggle.
