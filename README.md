# ScratchBird

**Why ScratchBird Exists**  
I built ScratchBird to create the database I always wanted: one where developers and teams can organize data like code, move objects while they’re live, and upgrade without downtime — without limits or complicated workarounds. Everything else — clustering, recursive schemas, versioned compute, deterministic replay — exists just to make those goals real. Git support came later, because developers asked for it. ScratchBird is still a work-in-progress passion project, and I welcome curious testers, questions, and feedback.

If you have taken the time to pull it down, let me know in the message boards and I can let you know what is and is not ready for testing.

## Docker Development Environment

- Use the unified environment scripts under `scripts/dev-unified/` to bootstrap and run a full Linux SSH container for building and testing all ScratchBird-related repos.
- Scripts are versioned with this repo and are intended to stay in sync with the source: [scripts/dev-unified/README.md](scripts/dev-unified/README.md)
- Main helpers:
  - `bootstrap-workspace.sh` — one-command workflow to download/refresh repos, install local mutable build scripts, and optionally start the container.
  - `sync-repos.sh` — clone/refresh source repos into your chosen working directory.
  - `install-workspace-build-scripts.sh` — install/update mutable `build-*.sh` and `test-*.sh` scripts directly in your workspace root.
  - `run.sh` — build and start the SSH-ready container bound to your chosen workspace path.
  - `start-scratchbird-environment.sh` — generated into your workspace root; starts ScratchBird and configured emulation listeners inside the container.

Latest docker-ready release bundle: [v0.5.1](https://github.com/DaltonCalford/ScratchBird/releases/tag/v0.5.1)

Typical workflow:

```bash
cd ScratchBird/scripts/dev-unified
./bootstrap-workspace.sh --workspace /home/<user>/CliWork --all --start --ip 127.0.0.5 --port 2222
```

Inside SSH, from your mounted workspace (for example `~/CliWork`), run local build/test scripts such as `./build-scratchbird.sh`, `./build-scratchbird-ai.sh`, `./build-all.sh`, and `./test-all.sh`.

Thanks Again,

Dalton

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
