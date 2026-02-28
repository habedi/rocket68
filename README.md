<div align="center">
  <picture>
    <img alt="Project Logo" src="logo.svg" height="20%" width="20%">
  </picture>
<br>

<h2>Rocket 68</h2>

[![Tests](https://img.shields.io/github/actions/workflow/status/habedi/rocket68/tests.yml?label=tests&style=flat&labelColor=282c34&logo=github)](https://github.com/habedi/rocket68/actions/workflows/tests.yml)
[![Benchmarks](https://img.shields.io/github/actions/workflow/status/habedi/rocket68/benchmarks.yml?label=benchmarks&style=flat&labelColor=282c34&logo=github)](https://github.com/habedi/rocket68/actions/workflows/benchmarks.yml)
[![Code Coverage](https://img.shields.io/codecov/c/github/habedi/rocket68?label=coverage&style=flat&labelColor=282c34&logo=codecov)](https://codecov.io/gh/habedi/rocket68)
[![Docs](https://img.shields.io/badge/docs-latest-007ec6?label=docs&style=flat&labelColor=282c34&logo=readthedocs)](https://habedi.github.io/rocket68/)
[![License](https://img.shields.io/badge/license-MIT-007ec6?label=license&style=flat&labelColor=282c34&logo=open-source-initiative)](LICENSE)
[![Release](https://img.shields.io/github/release/habedi/rocket68.svg?label=release&style=flat&labelColor=282c34&logo=github)](https://github.com/habedi/rocket68/releases/latest)

A Motorola 68000 CPU emulator in C

</div>

---

Rocket 68 is a Motorola 68000 CPU emulator written in pure C11.
It supports all the instructions and addressing modes of the 68000 CPU, plus system control features like supervisor mode, interrupts, and exceptions.
It is also runs as a similar clock cycle to the real 68000 CPU with very good accuracy.

### Features

- Have a simple API and easy to integrate into other projects
- Supports all Motorola 68000 instructions and different addressing modes
- Near-exact cycle timing (which means emulation speed is very close to the real 68000 CPU clock speed)
- Full hardware interrupt support (with auto-vectoring, address error traps, trace mode, and halted states)
- Built-in instruction disassembler and support for loading binary and S-record programs

See [ROADMAP.md](ROADMAP.md) for the list of implemented and planned features.

> [!IMPORTANT]
> This project is in early development, so bugs and breaking changes are expected.
> Please use the [issues page](https://github.com/habedi/rocket68/issues) to report bugs or request features.

---

### Quickstart

1. Clone the repository

```bash
git https://github.com/habedi/rocket68
cd rocket68
```

> [!NOTE]
> If you want to run the tests, benchmarks, etc., and not only use Rocket 68, you need to clone with `--recursive` to get the submodules:
>
> ```bash
> git clone --recursive https://github.com/habedi/rocket68
> ```

2. Build Rocket 68

```bash
BUILD_TYPE=release make all
```

3. Include the header file (from `include` directory) in your project and link with the files in the `lib` directory:

```
# Example compilation command on Linux:
gcc -o main main.c -Iinclude lib/librocket68.a
```

```c
// main.c
#include <rocket68.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    u32 mem_size = 1024 * 1024; // 1 MB
    u8* ram = calloc(mem_size, 1);
    if (!ram) return 1;

    M68kCpu cpu;
    m68k_init(&cpu, ram, mem_size); 

    // A tiny M68k program (machine code)
    u8 program[] = {
        // --- 1. System Reset Vectors ---
        // Address 0x0000: Initial SSP (Stack Pointer: 0x00100000)
        0x00, 0x10, 0x00, 0x00, 
        // Address 0x0004: Initial PC (Program Counter: 0x00000008)
        0x00, 0x00, 0x00, 0x08, 

        // --- 2. Code starts at 0x08 ---
        // MOVEA.L #$E00000, A0 -> Put our terminal port into A0
        0x20, 0x7C, 0x00, 0xE0, 0x00, 0x00,

        // MOVE.B #'H', (A0) -> Write 'H' to terminal
        0x10, 0xBC, 0x00, 'H',
        // MOVE.B #'i', (A0) -> Write 'i' to terminal
        0x10, 0xBC, 0x00, 'i',
        // MOVE.B #'\n', (A0) -> Write newline to terminal
        0x10, 0xBC, 0x00, '\n',
        
        // STOP #$2700 -> Halt the CPU
        0x4E, 0x72, 0x27, 0x00
    };

    // Load the program into the beginning of RAM
    memcpy(ram, program, sizeof(program));

    // Reset CPU (this causes it to read the SSP and PC from address 0)
    m68k_reset(&cpu);

    printf("Executing M68k code...\n");
    // Execute up to 1000 cycles (or until STOP instruction)
    int cycles_executed = m68k_execute(&cpu, 1000);

    // Another way to "show something": print the internal CPU state!
    printf("Finished in %d cycles. CPU halted at PC: 0x%08X\n", cycles_executed, (unsigned int)cpu.pc);

    free(ram);
    return 0;
}
```

4. Check out the Makefile (optional)

You can run the `make help` command to see the available targets and options including the targets for running different tests suites.

---

### Documentation

The project documentation is available [here](https://habedi.github.io/rocket68/).
The detailed API documentation (generated with Doxygen) is available [here](https://habedi.github.io/rocket68/doxygen/index.html).

### Header Files

This project includes the following header files (available in the [include](include) directory):

| File | Description |
| --- | --- |
| `m68k.h` | This is the main API header for the CPU core execution, callbacks, and management. |
| `disasm.h` | The built-in instruction disassembler API for formatting opcodes into text. |
| `loader.h` | Utilities for loading raw binaries and Motorola S-record files into memory for execution. |
| `rocket68.h` | A header that includes all the components (CPU, disassembler, and loader) in one file. |

> [!NOTE]
> It's recommended to use the `rocket68.h` header file instead of including the individual header files for most use cases.
> This header file includes the core CPU emulator, and additional components like the disassembler and loader.

### Benchmarks

The [benches](benches) directory contains benchmarks for comparing Rocket 68 with other emulators.

---

### Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to make a contribution.

### License

This project is licensed under the MIT License (see [LICENSE](LICENSE)).

### Acknowledgements

- The logo is from [SVG Repo](https://www.svgrepo.com/svg/142843/cpu) with some modifications.
- Tests (code and data) from the following projects were used to verify the correctness of Rocket 68:
    - [Musashi](https://github.com/kstenerud/Musashi)
    - [68k-bcd-verifier](https://github.com/flamewing/68k-bcd-verifier)
    - [m68000](https://github.com/SingleStepTests/m68000)

### References

- [M68000 Family Programmer's Reference Manual](https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf)
- [Motorola 68000 Opcodes](http://goldencrystal.free.fr/M68kOpcodes.pdf)
