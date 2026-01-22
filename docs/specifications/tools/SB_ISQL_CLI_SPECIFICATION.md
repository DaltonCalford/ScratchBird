# sb_isql CLI Specification (Native + Emulated)

Version: 1.1  
Status: Draft (Alpha networking)  
Last Updated: January 2026

## Purpose

Define the command-line interface for `sb_isql` and the dialect-specific
wrappers (`sb_pg_isql`, `sb_my_isql`, `sb_fb_isql`) with listener/pool
network connectivity.

## Scope

- Network connection options and defaults
- Dialect selection and parser routing
- Batch/script execution and output modes
- Meta/SET command surface (summary only)
- Non-goals: SQL language semantics, wire protocol internals

## Binaries

- sb_isql (ScratchBird native)
- sb_pg_isql (PostgreSQL emulation)
- sb_my_isql (MySQL emulation)
- sb_fb_isql (Firebird emulation)

These wrappers are intended for **scripted CLI testing** against the
emulated listeners. Each wrapper connects to its protocol’s port and behaves
like a native client for that dialect, allowing large input/output scripts to
be executed in the target SQL dialect.

## Usage

```
sb_isql <database> [options]
sb_pg_isql <database> [options]
sb_my_isql <database> [options]
sb_fb_isql <database> [options]
```

## Connection Defaults

Listener defaults are taken from server configuration. If the server uses
non-default ports, the client must pass `-p`.

- sb_isql (native): 3092
- sb_pg_isql: 5432
- sb_my_isql: 3306
- sb_fb_isql: 3050

## Common Options (expected)

```
-H, --host <host>         Server hostname (default: localhost)
-p, --port <port>         Server port (default: protocol default)
-U, --user <user>         Username
-P, --password <pass>     Password (prompted if omitted)
-c, --command <sql>       Execute single command and exit
-f, --file <file>         Execute commands from file and exit
-i, --input <file>        Alias for -f (Firebird compatible)
-o, --output <file>       Write output to file
-t, --tuples-only         Print tuples only (no headers/footers)
-A, --no-align            Unaligned output mode
-F, --field-separator <s> Field separator (default: |)
-q, --quiet               Quiet mode (no welcome message)
-e, --echo                Echo commands before execution
-b, --bail                Stop on first error
-v, --verbose             Verbose mode
--schema-tree             Print schema tree and exit
-a, --extract-all         Extract DDL for all objects
-x, --extract             Extract DDL (no data)
-ex, --extract-db         Extract DDL with CREATE DATABASE
-s, --dialect <n>         SQL dialect (1, 2, or 3)
-par, --parser <name>     Parser (scratchbird, firebird, postgresql, mysql)
--sslmode <mode>          disable|prefer|required
--timeout <seconds>       Connection timeout
--version                 Show version
-h, --help                Show help
```

## Database Targeting

- In **network mode**, `<database>` is a ScratchBird database name/alias.
- In **local/IPC mode** (if enabled), `<database>` may be a filesystem path.

## Listener Requirements

- Tools connect to protocol listeners (native/pg/mysql/firebird) managed by the
  server’s listener/pool architecture.
- If a listener is disabled, the client exits with a protocol-specific error.
- Shared listener mode (single port) is supported when enabled in server config.

## Emulation Testing Behavior

- `sb_pg_isql`, `sb_my_isql`, and `sb_fb_isql` connect to their respective
  emulated ports and behave as native clients for those dialects.
- These wrappers are designed for running scripted tests and verifying
  protocol-level input/output against expected native engine behavior.

## Dialect Selection

- `sb_isql` defaults to native ScratchBird parser.
- `sb_*_isql` wrappers select their dialect automatically.
- `--parser` is allowed for `sb_isql` to explicitly set a parser.

## Meta/SET Commands (summary)

`sb_isql` supports Firebird-style `SET` commands and `\` meta-commands,
including:

- Output formatting (aligned/unaligned, field separator, width, list)
- Execution control (bail, timing, plan/explain)
- Script control (`-f`, `\i`)
- Connection control (`\c`)

See the user documentation or `sb_isql -h` for the full command list.

## Error Handling

- Authentication errors map to protocol-specific error formats.
- Network timeouts report connection failures.
- `-b/--bail` forces a non-zero exit on the first error.

## Related Specs

- `docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `docs/specifications/wire_protocols/*.md`
- `docs/specifications/tools/SB_TOOLING_NETWORK_SPEC.md`
