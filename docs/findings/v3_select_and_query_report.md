# SELECT_AND_QUERY.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/SELECT_AND_QUERY.md` (Authoritative, updated 2026-02-08)

Summary:
- The spec requires V3 to emit `SBLR3_QUERY_*` opcodes for SELECT/SETOP/VALUES/CTE/ORDER/LIMIT/OFFSET/FETCH/LOCK; the current V3 emitter does not emit any `SBLR3_QUERY_*` opcodes.
- The implementation instead emits a single `SBLR3_SELECT` instruction with a `SCHEMA_SELECT` payload containing set-op, with/CTE, order/limit/fetch/lock details.

Key findings:

## Emission Rules
[ ] `SBLR3_QUERY_SELECT`, `SBLR3_QUERY_SETOP`, `SBLR3_QUERY_VALUES`, `SBLR3_QUERY_ORDER_BY`, `SBLR3_QUERY_LIMIT`, `SBLR3_QUERY_OFFSET`, `SBLR3_QUERY_FETCH`, `SBLR3_QUERY_LOCK`, and `SBLR3_QUERY_CTE` are not emitted. No `SBLR3_QUERY_*` opcodes exist in the registry or codebase. (`src/parser/v3_emitter.cpp`, `include/scratchbird/sblr/v3_opcodes.generated.h`)
[~] SELECT/CTE/ORDER/LIMIT/FETCH are represented in a single `SBLR3_SELECT` payload (`SCHEMA_SELECT`) with nested structures. (`src/parser/v3_emitter.cpp`, `src/sblr/v3_schema.generated.cpp`)

## Statement Forms
[~] Core SELECT syntax is supported in the parser, but emitted via `SBLR3_SELECT` payload fields rather than discrete query opcodes.
[~] Set operations are represented via `SCHEMA_SELECT.set_op` and `SET_OP` payload, not via `SBLR3_QUERY_SETOP` opcodes.
[~] VALUES-only queries are parsed and handled, but emitted as `SBLR3_SELECT` with `source=VALUES` or `SBLR3_INSERT` payloads depending on context; no `SBLR3_QUERY_VALUES` opcode exists.

## Errors
[ ] Spec-defined errors (`ERR_PARSE_EXPECTED_SELECT_LIST`, `ERR_FEATURE_NOT_SUPPORTED`, `ERR_INVALID_ORDER_BY`) are not surfaced as such in parser/emitter error paths.

