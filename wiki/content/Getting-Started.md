# Getting Started

- Version: `0.1.0`
- Baseline date: `2026-02-19`

## Build and Verify

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Expected baseline: `3355` tests, all passing.

## Start Core Server

```bash
./build/src/sb_server --config ./sb_config.ini.example
```

## Listener/Parser Binaries

- `sb_listener_native`, `sb_parser_native`
- `sb_listener_pg`, `sb_parser_pg`
- `sb_listener_mysql`, `sb_parser_mysql`
- `sb_listener_fb`, `sb_parser_fb`

## First Steps

1. Create/open a database.
2. Start listener(s) for required protocol(s).
3. Connect with protocol-appropriate client.
4. Run first DDL/DML and validate responses.

## Read Next

- Full developer guide: `developer-guide/Developer-Guide.md`
- Native parser reference: `language-guides/native/Language-Reference.md`
- User docs index: `../../docs/user-documentation/index.md`
