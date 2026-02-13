# Emulated Database Parser Specification (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Compatibility Targets (V3)

- PostgreSQL 16+ behavior (per `POSTGRESQL_PARSER_SPECIFICATION.md`)
- MySQL 8.x behavior (per `MYSQL_PARSER_SPECIFICATION.md`)
- Firebird 5.x behavior (per Firebird emulation specs)
- TDS/MSSQL is not supported and MUST be rejected

## Absolute Rules

1. Emulated parsers are completely separate from the ScratchBird parser.
2. Emulated parsers MUST NOT share grammar or code with the ScratchBird parser.
3. Emulated parsers MUST emit SBLR directly; the engine executes SBLR only.
4. If an emulated dialect is disabled, the parser MUST reject all statements
   with `ERR_FEATURE_DISABLED` and MUST NOT fallback to ScratchBird.
5. Tablespace DDL and TABLESPACE clauses are rejected in emulated parsers.

## Architecture Overview

```
ScratchBird SQL (native)  -> ScratchBird Parser -> SBLR -> Executor
PostgreSQL SQL (emulated) -> PostgreSQL Parser  -> SBLR -> Executor
MySQL SQL (emulated)      -> MySQL Parser       -> SBLR -> Executor
Firebird SQL (emulated)   -> Firebird Parser    -> SBLR -> Executor
```

## Emulated Schema Model

Emulated databases are implemented as schemas within ScratchBird, not separate
physical databases. See `catalog/SCHEMA_PATH_RESOLUTION.md` for path rules.

Example layout:
```
/remote/emulation/<dialect>/<server>/<database>/
```

## Related Specs

- `docs/specifications/parser/v3/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/parser/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/v3/wire_protocols/POSTGRESQL_EMULATION_BEHAVIOR.md`
- `docs/specifications/parser/v3/wire_protocols/MYSQL_EMULATION_BEHAVIOR.md`
- `docs/specifications/parser/v3/wire_protocols/FIREBIRD_EMULATION_BEHAVIOR.md`
