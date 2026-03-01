## Project Roadmap

This document outlines the implemented features and the future goals for the project.

> [!IMPORTANT]
> This roadmap is a work in progress and is subject to change.

### Core Features

- [x] CPU state structure (`M68kCpu`)
- [x] Memory access (8/16/32-bit and big endian)
- [x] Instruction fetch
- [x] Instruction decode
- [x] Addressing modes (all 12 modes)
- [x] Basic instructions (`MOVE`, `MOVEA`, `MOVEQ`, `MOVEM`, `MOVEP`)
- [x] Logic operations (`AND`, `OR`, `EOR`, `NOT`)
- [x] Immediate logic (`ANDI`, `ORI`, `EORI` — including to CCR/SR)
- [x] Program control (`JMP`, `BRA`, `Bcc`, `BSR`, `JSR`, and `RTS`)
- [x] Status register (`CCR`/`SR`) management
- [x] Stack pointer (`SSP`/`USP`) handling with dual stack support
- [x] Data manipulation (`CLR`, `NEG`, `NEGX`, `NOT`, `EXT`, `SWAP`)
- [x] Shift and rotate (`ASL`, `ASR`, `LSL`, `LSR`, `ROL`, `ROR`, `ROXL`, `ROXR`)
- [x] Extended arithmetic (`MULU`, `MULS`, `DIVU`, `DIVS`)
- [x] Immediate arithmetic (`ADDI`, `SUBI`)
- [x] Compare and test (`CMP`, `CMPA`, `CMPI`, `CMPM`, `TST`)
- [x] Address manipulation (`LEA` and `PEA`)
- [x] Stack and frame (`LINK` and `UNLK`)
- [x] Loop utilities (`DBcc` and `Scc`)
- [x] Address arithmetic (`ADDA` and `SUBA`)
- [x] Extended arithmetic (`ADDX` and `SUBX`)
- [x] Register exchange (`EXG`)
- [x] BCD arithmetic (`ABCD`, `SBCD`, and `NBCD`)
- [x] Bit manipulation (`BTST`, `BSET`, `BCLR`, `BCHG`)
- [x] Misc (`CHK`, `TRAPV`, `RTR`, `STOP`, `RESET`, `TAS`, `NOP`)
- [x] Move to and from (`MOVE SR`, `MOVE CCR`, `MOVE USP`)

### Extra Features

- [x] System control and exceptions (`TRAP`, `RTE`, `MOVE SR`, exception processing)
- [x] System integration and tooling (loader, disassembler, and I/O)
- [x] Supervisor and user mode (USP/SSP dual stack switching)
- [x] Interrupt handling (hardware IRQ with auto-vectoring and IACK callback)
- [x] Address error exceptions (odd-address word/long access traps)
- [x] Trace mode (single-step exception after each instruction)
- [x] Stopped state (CPU halts on STOP, resumes on interrupt)
- [x] Program loader (S-record and binary)
- [x] Disassembler (instruction decoding to text)
- [x] Baseline instruction/EA cycle accounting
- [x] Memory wait state hooks (`M68kWaitBusCallback`)
- [x] Interrupt acknowledge (IACK) dynamic vector callback
- [x] Function Code (FC0-FC2) callbacks for MMU and memory access typing
- [x] Execution debug hooks (instruction hook, PC change, RESET/TAS callbacks)
- [x] Advanced timeslice management (voluntary yielding and dynamic cycle modification)
- [x] CPU state serialization and save state support

### Compatibility and Advanced CPU Features

- [x] Partial post-68000 opcode coverage (`MOVEC`, `MOVES`, `RTD`, and `BKPT`)
- [ ] Instruction support for 68010, 68020, 68030, and 68040 CPUs
- [ ] CPU model selection API and per-model behavior gates (`68000`/`68010`/`68020`/`68030`/`68040`)
- [ ] Full 68010+ exception vector base handling (`VBR` applied to exception/interrupt vector fetch)
- [ ] 68010+ stack model completeness (ISP/MSP where applicable and model-specific exception stack frames)
- [ ] 68020+ integer/control opcode coverage (bitfield family, `CAS/CAS2`, `CHK2/CMP2`, `CALLM/RTM`, `PACK/UNPK`, etc.)
- [ ] PMMU emulation (68851/68030/68040 translation and PMMU instruction behavior)
- [ ] FPU emulation (68881/68882/68040 FPU instruction set and state)
- [ ] Additional `MOVEC` control register coverage (`CACR`, `CAAR`, and model-specific control registers)
- [ ] Optional prefetch/bus-cycle quirks for high-compatibility emulation (e.g., documented predecrement long-write ordering)
- [ ] Optional separate immediate and PC-relative memory access callbacks for systems that distinguish program/data fetch paths

### Development and Testing

- [x] Basic unit tests
- [x] Unit tests for instructions
- [x] Integration tests
- [x] Benchmarks (against Musashi emulator)
