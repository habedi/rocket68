# AGENTS.md

This file provides guidance to coding agents collaborating on this repository.

## Mission

Rocket68 is a modern, fast, cycle-accurate Motorola 68000 core in C11.
Priorities, in order:

1. Correctness and cycle accuracy.
2. Decoupled, thread-safe, and multi-instance architecture.
3. Hot-loop performance.
4. Clear, maintainable code.

## Core Rules

- Use English for code, comments, docs, and tests.
- Keep all CPU state inside `M68kCpu`; do not introduce global mutable state.
- Keep public API interactions centered on `M68kCpu* cpu`.
- Prefer small, focused changes over large refactoring.
- Add comments only when they clarify non-obvious behavior.

Quick examples:

- Good: add a per-instance callback field in `M68kCpu` and configure it via `m68k_set_*`.
- Bad: add a static/global callback pointer shared by all CPU instances.

## Repository Layout

- `include/`: public headers (`m68k.h`, `rocket68.h`, `loader.h`, and `disasm.h`).
- `src/`: core implementation.
- `src/m68k/`: CPU execution engine and opcode handlers.
- `test/`: unit and integration tests.
- `benches/`: performance benchmarks.
- `docs/`: MkDocs documentation.
- `external/`: third-party reference code (for comparison/validation).

## Architecture Constraints

- `M68kCpu` is the single source of CPU runtime state.
- Callbacks (memory, IRQ/INT ACK, TAS, hooks, etc.) are instance-bound.
- New callbacks or hooks must take `M68kCpu* cpu` as the first argument.
- Be careful with `M68kCpu` layout: consider alignment and cache locality.
- Avoid adding branches or synchronization in execution hot paths without clear need.

## C11 and Type Conventions

- Keep C11 compatibility (`-std=c11`) across GCC and Clang.
- Use project types consistently (`u8`, `u16`, `u32`).
- Keep `_Generic`, `_Static_assert`, and anonymous union usage coherent with existing API style.

## Required Validation

Run these checks for any non-trivial core/API change:

1. `make test` (this includes JSON tests that can take time to run)
2. `make bench` (required for core loop/opcode timing changes)
3. `make docs` (required when public API or docs/examples change)

Recommended for risky or low-level changes:

1. `make test-asan`
2. `make test-ubsan`
3. `make test-memory`

## First Contribution Flow

Use this sequence for your first change:

1. Read `include/m68k.h` and the touched opcode/core files.
2. Implement the smallest possible code change.
3. Add or update tests in `test/` that fail before and pass after.
4. Run `make test`.
5. Run `make bench` if execution logic, opcodes, or timing changed.
6. Update `docs/` if public API behavior or examples changed.

Example scopes that are good first tasks:

- add tests for an existing opcode edge case;
- fix a callback wiring bug without changing API shape;
- improve docs/examples to match current API behavior.

## Testing Expectations

- No opcode or execution workflow change is complete without tests.
- Unit tests should fully initialize `M68kCpu` and required callbacks/memory behavior.
- Integration tests should verify realistic instruction sequences and state transitions.
- Do not merge code that breaks existing tests.

Minimal unit-test checklist:

1. Initialize CPU and memory explicitly.
2. Set PC/SR/register state needed by the instruction under test.
3. Execute enough cycles/instructions for deterministic completion.
4. Assert result value(s), status flags, and observable side effects.

Example test skeleton:

```c
M68kCpu cpu;
u8 mem[MEM_SIZE] = {0};
m68k_init(&cpu, mem, MEM_SIZE);
m68k_set_pc(&cpu, START_PC);
/* write opcode/data bytes into mem[] */
int used = m68k_execute(&cpu, CYCLES);
/* assert registers/flags/memory and optionally cycles */
```

## Documentation Expectations

- Public API changes must be reflected in `docs/`.
- Examples must compile against current headers.
- Prefer `rocket68.h` in user-facing examples unless a focused header is intended.

Example:

- If you add or change a callback setter, update both API reference and at least one usage example.

## Change Design Checklist

Before coding:

1. Confirm whether the change touches cycle timing, API shape, or struct layout.
2. Identify affected tests and benchmarks.
3. Keep decoupling guarantees explicit.

Before submitting:

1. Verify required validation commands succeeded.
2. Ensure docs/tests were updated where relevant.
3. Confirm no global-state coupling was introduced.

## Review Guidelines (P0/P1 Focus)

Review output should be concise and only include critical issues.

- `P0`: must-fix defects (incorrect emulation behavior, severe regression, architecture breakage).
- `P1`: high-priority defects (likely functional/timing bug, decoupling risk, major perf hazard).

Do not include:

- style-only nitpicks,
- praise/summary of what is already good,
- exhaustive restatement of the patch.

Use this review format:

1. `Severity` (`P0`/`P1`)
2. `File:line`
3. `Issue`
4. `Why it matters`
5. `Minimal fix direction`

## Practical Notes for Agents

- Prefer targeted edits over broad mechanical rewrites.
- If you detect contradictory repository conventions, follow existing code and update docs accordingly.
- When uncertain about timing correctness, add/extend tests first, then optimize.

## Commit and PR Hygiene

- Keep commits scoped to one logical change.
- PR descriptions should include:
  1. behavioral change summary,
  2. tests added/updated,
  3. benchmark impact (or "no measurable impact"),
  4. docs updated (yes/no).

Suggested PR checklist:

- [ ] Tests added/updated for behavior changes
- [ ] `make test` passes
- [ ] `make bench` run (if a core/timing path changed)
- [ ] Docs/examples updated (if API behavior changed)
