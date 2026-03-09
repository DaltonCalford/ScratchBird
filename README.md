# ScratchBird

**Why ScratchBird Exists**  
I built ScratchBird to create the database I always wanted: one where developers and teams can organize data like code, move objects while they’re live, and upgrade without downtime — without limits or complicated workarounds. Everything else — clustering, recursive schemas, versioned compute, deterministic replay — exists just to make those goals real. Git support came later, because developers asked for it. ScratchBird is still a work-in-progress passion project, and I welcome curious testers, questions, and feedback.

If you have taken the time to pull it down, let me know in the message boards and I can let you know what is and is not ready for testing.

**What is ScratchBird**

It started as a database project, it became a virtual machine.  I knew from the beginning that I wanted to be able to consolidate all the various database engines spread across the office into a single easy to manage environment.  So I separated the parser from the engine and had a series of parsers, one for each database I wanted to emulate, sharing a backend engine.  Each Parser uses the emulated engines wireprotocol, authentication and sql dialect.  The parsers job was to convert the users queries to the language supported by the engine - a bytecode language called SBLR.

Once that was working, to ensure that I have full compatibility, I ran the original engines own regression suites using the original engines tools and code - only after all functionality was supported and all tests pass, did I release those parsers - right now the engine supports, FireBirdSQL, MySQL and PostgreSQL.

I also knew that I needed to be able to migrate the existing data from the legacy databases, so I made a pseudo proxy - point your client to SB(scratchbird) and have SB connect to your legacy server, and SB, using pass-through queries, responds with what the legacy server generated.  Then, SB recreates the remote engine inside itself and starts to replicate the data inside the new SB database.  Once data parity is met, all queries are ran against the legacy and the new emulated database.  The results are compared and any differences reported.  Once you are confident the new is working exactly like the old, you can turn off the legacy server.

Having been in IT for decades, the concept of a full server migration without any code changes and no downtime, was a dream that was now possible.

During the migration, the legacy procedure and function code, is compiled into SBLR inside the engine, which also has a SBLR->SQL process, so you automatically have all your legacy stored procedures become native SB code.  

The way I make this work is via recursive schema - think of namespaces or file directories - each database being emulated is in its own schema with emulated system files so the clients see exactly what they expect.   This also means that we can separate tables, indexes by job or application.  I even included a relative resolution system.
I incorporated LLVM as a JIT or pre-compiler so that you can specify which functions and procedures should be natively compiled.

So what is scratchbird, it is a way to stop all the headaches I have dealt with over the years. 

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
