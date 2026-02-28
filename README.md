# ScratchBird

ScratchBird is a database engine with a canonical v3 SQL parser/runtime surface and protocol emulation adapters.

## Public Beta 1 Scope

ScratchBird is at **public beta 1** for **code and tests**.

- Canonical parser/runtime: `v3` (primary implementation surface)
- Emulation protocol surfaces: PostgreSQL, MySQL, Firebird
- Native execution model: SBLR-backed engine execution
- Security surface in test gates: row-level, column-level, and domain-level enforcement

This is not a GA/production declaration. It is a beta engineering baseline.

## Parser Model

- `v3` is the core parser and semantic model.
- PostgreSQL/MySQL/Firebird parser paths are emulation surfaces for compatibility testing and protocol parity.

## Project Documentation

- [docs/README.md](docs/README.md)
- [docs/documentation/README.md](docs/documentation/README.md)
- [docs/PROJECT_STATUS.md](docs/PROJECT_STATUS.md)
- [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md)
- [docs/BUILD.md](docs/BUILD.md)
- [docs/TEST.md](docs/TEST.md)

## Quick Start

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

## License

Initial Developer's Public License Version 1.0 (IDPL 1.0).
