# sb_isql CLI Specification (Network Modes)

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define the command-line interface for sb_isql and dialect-specific
clients (sb_pg_isql, sb_my_isql, sb_fb_isql) with network support.

## Scope

- Connection options (host/port/user/password/role)
- TLS options
- Dialect selection for emulation clients
- Non-goals: SQL language, parser behavior

## Binaries

- sb_isql (ScratchBird native)
- sb_pg_isql (PostgreSQL emulation)
- sb_my_isql (MySQL emulation)
- sb_fb_isql (Firebird emulation)

## Common Options

```
-h, --host <host>       Server hostname (default: localhost)
-p, --port <port>       Server port (default: protocol default)
-U, --user <user>       Username
-W, --password          Prompt for password
-d, --database <db>     Database name
-r, --role <role>       Role name
--sslmode <mode>        disable|prefer|required
--timeout <seconds>     Connection timeout
--json                  Output in JSON format (where supported)
```

## Protocol Defaults

- sb_isql (native): 3092
- sb_pg_isql: 5432
- sb_my_isql: 3306
- sb_fb_isql: 3050

## Connection Rules

- All clients connect over TCP/IP to listeners.
- Emulated clients assume a ScratchBird database already exists.
- CREATE DATABASE in emulated clients creates schema branches only.

## Error Handling

- Authentication errors map to protocol-specific error formats.
- Network timeouts are reported as connection failures.

## Related Specs

- docs/specifications/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md
- docs/specifications/wire_protocols/*.md
