# V3 Parser INSERT Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/INSERT.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
V3 parser supports INSERT forms (VALUES/SELECT/DEFAULT, ON CONFLICT, RETURNING). Emission uses `SBLR3_INSERT` instead of required `SBLR3_DML_INSERT`. Error mapping to spec error codes is not present. Executor supports `SBLR3_INSERT`, including ON CONFLICT handling and RETURNING, but spec-aligned error codes and opcode naming are mismatched.

## Findings by Spec Item

### Parsing Rules
- [~] Enter DML parse mode.
  - `ParseMode::DML_INSERT` is set in `parseInsert`. See `src/parser/parser_v3.cpp:6851`.
- [~] Parse target table reference and optional column list.
  - `parseInsert` reads schema path, optional alias, column list. See `src/parser/parser_v3.cpp:6857-6899`.
- [~] Parse VALUES, SELECT, or DEFAULT VALUES source.
  - `VALUES`, `SELECT`, and `DEFAULT VALUES` are supported. See `src/parser/parser_v3.cpp:6905-6917`.
- [~] Parse ON CONFLICT / UPSERT clause if present.
  - `parseOnConflict` parses conflict target, DO NOTHING/UPDATE, optional WHERE. See `src/parser/parser_v3.cpp:6920-6996`.
- [~] Parse RETURNING clause if present.
  - `parseReturningClause` is used. See `src/parser/parser_v3.cpp:6998-7008`.
- [ ] Emit `SBLR3_DML_INSERT` with typed payload and child expressions.
  - Emitter uses `SBLR3_INSERT` with `source`, `values`/`select`, `on_conflict`, `returning`. See `src/parser/v3_emitter.cpp:443-503`.

### Emission Rules
- [ ] Emit `SBLR3_DML_INSERT` opcode with `DML_INSERT` payload.
  - Emission is `SBLR3_INSERT` (no `SBLR3_DML_INSERT` opcode found). See `src/parser/v3_emitter.cpp:443`.
- [~] Child nodes present (TABLE_REF/columns/VALUES or SELECT/ON_CONFLICT/RETURNING).
  - Payload includes `target`, `columns`, `values` or `select`, `on_conflict`, `returning`.

### Errors
- [ ] Missing target table: `ERR_PARSE_EXPECTED_TABLE`.
  - Parser/executor report generic errors like `V3 INSERT missing target` or parsing errors without spec error codes. See `src/sblr/executor.cpp:46650`.
- [ ] Column count mismatch: `ERR_COLUMN_COUNT_MISMATCH`.
  - Executor reports generic mismatch errors (e.g., `V3 INSERT values/columns length mismatch`). No spec error codes mapped.
- [ ] ON CONFLICT clause not supported in dialect: `ERR_FEATURE_NOT_SUPPORTED`.
  - V3 parser accepts ON CONFLICT unconditionally; no dialect gating or error code mapping found.

## Notes
- Executor implements `SBLR3_INSERT` and appears to support ON CONFLICT and RETURNING; spec requires `SBLR3_DML_INSERT` with specific error code mapping.
