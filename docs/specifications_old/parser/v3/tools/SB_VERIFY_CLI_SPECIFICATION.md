# sb_verify CLI Specification

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Version: 1.0  
Status: Authoritative (V3)
Last Updated: January 2026

## Purpose

Define the command-line interface for `sb_verify` with network-capable
database integrity verification.

## Scope

- Verification modes and options
- Remote vs local execution modes
- Output and error behavior

## Usage

```
sb_verify <database> [options]
```

## Options

```
--all                 Run all checks (alias: --full)
--full                Full verification
--quick               Quick verification (header + checksums)
--pages               Verify page structures
--indexes             Verify index integrity
--constraints         Verify constraints (FK, CHECK, UNIQUE)
--checksums           Verify page checksums
--orphans             Find orphaned records
--repair              Attempt automatic repair (dangerous)
--verbose             Detailed output
--quiet               Errors only
-o, --output <file>   Write report to file
```

## Connection Options

```
-H, --host <host>       Server hostname (default: localhost)
-p, --port <port>       Server port (default: 3092)
-U, --user <user>       Username
-P, --password <pass>   Password (prompted if omitted)
--sslmode <mode>        disable|prefer|required
--timeout <seconds>     Connection timeout
--mode <remote|local|auto>
```

## Remote vs Local Semantics

- **Remote mode:** `sb_verify` connects to the native listener and requests
  verification through the server; results are streamed back to the client.
- **Local mode:** verification runs directly against the local database file.
- **Auto mode:** prefer remote unless the database is local and config allows.

## Exit Codes

- `0`: no issues
- `1`: warnings
- `2`: errors
- `3`: corruption detected
- `4`: usage error

## Related Specs

- `/docs/specifications/parser/v3/tools/SB_TOOLING_NETWORK_SPEC.md`
- `/docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md`
- `/docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`
