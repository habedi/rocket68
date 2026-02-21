## Project Roadmap

This document outlines the implemented features and the future goals for the project.

> [!IMPORTANT]
> This roadmap is a work in progress and is subject to change.

### Core Features

- [x] CPU state structure (`M68kCpu`)
- [x] Memory access (8/16/32-bit and Big Endian)
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
- [x] Compare and Test (`CMP`, `CMPA`, `CMPI`, `CMPM`, `TST`)
- [x] Address manipulation (`LEA` and `PEA`)
- [x] Stack and frame (`MOVEM`, `LINK`, and `UNLK`)
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
- [x] System integration and Tooling (loader, disassembler, and I/O)
- [x] Supervisor and user mode (USP/SSP dual stack switching)
- [x] Interrupt handling (hardware IRQ with auto-vectoring)
- [x] Address error exceptions (odd-address word/long access traps)
- [x] Trace mode (single-step exception after each instruction)
- [x] Stopped state (CPU halts on STOP, resumes on interrupt)
- [x] Program loader (S-record and binary)
- [x] Disassembler (instruction decoding to text)

### Development and Testing

- [x] Basic unit tests
- [x] Unit tests for instructions (45 tests)
- [ ] Integration tests
