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
// Example compilation command on Linux:
gcc -o main main.c -Iinclude lib/librocket68.a
```

```c
// main.c
#include <rocket68.h>
#include <stdlib.h>

int main(void) {
    u32 mem_size = 1024 * 1024; // 1 MB
    u8* ram = calloc(mem_size, 1);
    if (!ram) return 1;

    M68kCpu cpu; // Create a new CPU instance
    m68k_init(&cpu, ram, mem_size); // Initialize the CPU

    // Reset vector layout:
    //   0x00000000: initial SSP (32-bit)
    //   0x00000004: initial PC  (32-bit)
    m68k_reset(&cpu);

    // Execute a time slice of 1000 cycles
    int cycles_executed = m68k_execute(&cpu, 1000);
    (void)cycles_executed;

    free(ram);
    return 0;
}
```

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
