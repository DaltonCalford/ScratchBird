# Getting Started

- Version: `0.1.0`
- Baseline date: `2026-02-19`

## Quick Path

1. Build or install from beta package.
2. Start `sb_server`.
3. Connect using native or emulated protocol listeners.
4. Run first SQL statements and verify output.

## Build from Source

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

## Start Server

```bash
./build/src/sb_server --config ./sb_config.ini.example
```

## Connect

Use whichever listener is enabled in configuration:

- Native: `sb_listener_native` (default port `3092`)
- PostgreSQL emulation: `sb_listener_pg` (default port `5432`)
- MySQL emulation: `sb_listener_mysql` (default port `3306`)
- Firebird emulation: `sb_listener_fb` (default port `3050`)

## Continue

- First connection details: `first-connection.md`
- Basic SQL walkthrough: `basic-sql.md`
- Native parser language reference:
  `../language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md`
