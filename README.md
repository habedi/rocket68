## M68k CPU Emulator

<div align="center">
  <picture>
    <img alt="C Logo" src="logo.svg" height="25%" width="25%">
  </picture>
</div>
<br>

[![Tests](https://img.shields.io/github/actions/workflow/status/habedi/m68k-emul/tests.yml?label=tests&style=flat&labelColor=282c34&logo=github)](https://github.com/habedi/m68k-emul/actions/workflows/tests.yml)
[![Lints](https://img.shields.io/github/actions/workflow/status/habedi/m68k-emul/lints.yml?label=lints&style=flat&labelColor=282c34&logo=github)](https://github.com/habedi/m68k-emul/actions/workflows/lints.yml)
[![Code Coverage](https://img.shields.io/codecov/c/github/habedi/m68k-emul?label=coverage&style=flat&labelColor=282c34&logo=codecov)](https://codecov.io/gh/habedi/m68k-emul)
[![CodeFactor](https://img.shields.io/codefactor/grade/github/habedi/m68k-emul?label=code%20quality&style=flat&labelColor=282c34&logo=codefactor)](https://www.codefactor.io/repository/github/habedi/m68k-emul)
[![Docs](https://img.shields.io/badge/docs-latest-007ec6?label=docs&style=flat&labelColor=282c34&logo=readthedocs)](docs)
[![License](https://img.shields.io/badge/license-MIT-007ec6?label=license&style=flat&labelColor=282c34&logo=open-source-initiative)](https://github.com/habedi/m68k-emul)
[![Release](https://img.shields.io/github/release/habedi/m68k-emul.svg?label=release&style=flat&labelColor=282c34&logo=github)](https://github.com/habedi/m68k-emul/releases/latest)

This is a Motorola 68000 CPU emulator written in C11.

### Features

- Minimalistic project structure
- Pre-configured GitHub Actions for linting and testing
- Makefile for managing the development workflow and tasks like code formatting, testing, linting, etc.
- Example configuration files for popular tools like `clang-format`, `clang-tidy`, `Doxygen`, and `valgrind`.
- GitHub badges for tests, code quality and coverage, documentation, etc.
- [Code of Conduct](CODE_OF_CONDUCT.md) and [Contributing Guidelines](CONTRIBUTING.md)

### Getting Started

Check out the [Makefile](Makefile) for available commands to manage the development workflow of the project.

```shell
# Install system and development dependencies (for Debian-based systems)
sudo apt-get install make
make install-deps
```

```shell
# See all available commands and their descriptions
make help
```

### Platform Compatibility

This template should work on most Unix-like environments (like GNU/Linux distributions, BSDs, and macOS),
albeit with some minor modifications.
Windows users might need a Unix-like environment (such as WSL, MSYS2, or Cygwin) to use this template.

### Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for details on how to make a contribution.

### License

This project is licensed under the MIT License ([LICENSE](LICENSE)).
