## Project Roadmap

This document outlines the implemented features and the future goals for the project.

> [!IMPORTANT]
> This roadmap is a work in progress and is subject to change.

### Core Features

- [x] CPU state structure (`M68kCpu`)
- [x] Memory access (8/16/32-bit and Big Endian)
- [x] Instruction fetch
- [x] Instruction decode
- [x] Addressing modes (basic)
- [x] Basic instructions (`MOVE`, `ADD`, and `SUB`)
- [x] Logic operations (`AND`, `OR`, and `EOR`)
- [x] Program control (`JMP`, `BRA`, `Bcc`, `JSR`, and `RTS`)
- [x] Status register (`CCR`) management
- [x] Stack pointer (`SSP`) handling
- [x] Data manipulation (`CLR`, `NEG`, `NOT`, `EXT`, and `SWAP`)
- [x] Shift and rotate (`ASL`, `ASR`, `LSL`, `LSR`, `ROL`, `ROR`, `ROXL`, and `ROXR`)
- [x] Extended arithmetic (`MULU`, `MULS`, `DIVU`, and `DIVS`)
- [x] Compare and Test (`CMP`, `CMPA`, `CMPI`, `CMPM`, and `TST`)
- [x] Address manipulation (`LEA` and `PEA`)
- [x] Stack and frame (`MOVEM`, `LINK`, and `UNLK`)
- [x] Loop utilities (`DBcc` and `Scc`)
- [x] Address arithmetic (`ADDA` and `SUBA`)
- [x] Extended arithmetic (`ADDX` and `SUBX`)
- [x] Register exchange (`EXG`)
- [ ] BCD arithmetic (`ABCD`, `SBCD`, and `NBCD`)
- [ ] Misc (`CHK`, `TRAPV`, `RTR`, `STOP`, and `RESET`)

### Extra Features

- [x] System control and exceptions (`TRAP`, `RTE`, `MOVE SR`, and exception processing)
- [x] System integration and Tooling (loader, disassembler, and I/O)
- [ ] Supervisor and user mode (full support)
- [ ] Interrupt handling (hardware IRQ)
- [ ] Program loader (S-record and binary)
- [ ] Disassembler (instruction decoding to text)

### Development and Testing

- [x] Basic unit tests
- [ ] Unit tests for instructions
- [ ] Integration tests
