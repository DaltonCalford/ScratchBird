# V3 Parser MERGE Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/MERGE.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
MERGE parsing is implemented, including WHEN MATCHED/NOT MATCHED and SQL Server extensions (NOT MATCHED BY SOURCE). Emission uses `SBLR3_MERGE_START` rather than required `SBLR3_DML_MERGE`. Error mappings to spec codes are not implemented; errors are generic. Executor has V3 MERGE handling for `SBLR3_MERGE_START`.

## Findings by Spec Item

### Parsing Rules
- [ ] Enter DML parse mode.
  - `parseMerge` does not set `ParseMode::DML_MERGE` (no guard present). See `src/parser/parser_v3.cpp:10874`.
- [~] Parse target table and alias.
  - Target schema path and optional alias parsed. See `src/parser/parser_v3.cpp:10881-10889`.
- [~] Parse source table or subquery and alias.
  - Supports subquery in parentheses or table path; source alias parsed. See `src/parser/parser_v3.cpp:10892-10912`.
- [~] Parse join predicate (ON clause).
  - `ON` required and parsed. See `src/parser/parser_v3.cpp:10914-10916`.
- [~] Parse WHEN MATCHED / WHEN NOT MATCHED actions in order.
  - Supported, plus NOT MATCHED BY SOURCE. See `src/parser/parser_v3.cpp:10918-11092`.
- [ ] Emit `SBLR3_DML_MERGE` with typed payload and ordered actions.
  - Emission uses `SBLR3_MERGE_START`. See `src/parser/v3_emitter.cpp:562-639`.

### Emission Rules
- [ ] Emit `SBLR3_DML_MERGE` opcode with `DML_MERGE` payload.
  - No `SBLR3_DML_MERGE` opcode found; emitter uses `SBLR3_MERGE_START`.
- [~] Child nodes emitted (target/source refs, ON expr, action lists).
  - Payload includes `target`, `source_query`/`source_table`, `on`, and action lists.

### Errors
- [ ] Missing ON predicate: `ERR_PARSE_EXPECTED_ON`.
  - Parser uses generic `Expected ON` error without spec code mapping.
- [ ] Missing WHEN clause: `ERR_PARSE_EXPECTED_WHEN`.
  - Parser allows zero WHEN clauses; no explicit check.
- [ ] Unsupported action in dialect: `ERR_FEATURE_NOT_SUPPORTED`.
  - No dialect gating or spec error mapping found.

## Notes
- V3 executor implements `SBLR3_MERGE_START` handling (`src/sblr/executor.cpp:49709+`) but not the spec’s `SBLR3_DML_MERGE` opcode.
