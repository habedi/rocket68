# Rocket 68

<p align="center">
  <img src="https://raw.githubusercontent.com/habedi/rocket68/main/logo.svg" alt="Project Logo" width="200" />
</p>

Rocket 68 is a Motorola 68000 CPU emulator written in pure C11.
It supports all the instructions and addressing modes of the 68000 CPU, plus system control features like supervisor mode, interrupts, and exceptions.
It is also runs as a similar clock cycle to the real 68000 CPU with very good accuracy.

### Features

- Have a simple API and easy to integrate into other projects
- Supports all Motorola 68000 instructions and different addressing modes
- Near-exact cycle timing (which means emulation speed is very close to the real 68000 CPU clock speed)
- Full hardware interrupt support (with auto-vectoring, address error traps, trace mode, and halted states)
- Built-in instruction disassembler and support for loading binary and S-record programs
