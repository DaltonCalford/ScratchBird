# Parser → SBLR Emission Rules Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/PARSER_TO_SBLR_EMISSION_RULES.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
Several edge-case emission rules are not implemented or diverge in the V3 parser/emitter. Notably, identifier canonicalization (lowercasing and length enforcement) is missing, SELECT * is not expanded, DISTINCT+ALL mutual exclusion is not enforced, and MERGE uses `SBLR3_MERGE_START` rather than the required `SBLR3_MERGE`. Some INSERT default rules are implemented.

## Findings by Spec Item

### 1) Identifier Canonicalization
- [ ] Unquoted identifiers folded to lowercase before emitting.
  - Lexer interns identifiers as-is; `toIdent` returns the stored string with no folding. See `src/parser/lexer_v3.cpp:446-490` and `src/parser/v3_emitter.cpp:3953-3959`.
- [ ] Identifier length > 128 bytes rejected with `V3E-0091`.
  - No length check found in lexer or parser.
- [~] Quoted identifiers preserve bytes.
  - `scanQuotedIdentifier` preserves content; no length check. See `src/parser/lexer_v3.cpp:680-719`.

### 2) CREATE TABLE Edge Cases
- [ ] Default self-reference should emit `SBLR3_COLUMN_REF` with empty table path.
  - Not verified here; emitter uses normal expression emission.
- [ ] IDENTITY vs DEFAULT precedence and explicit ignore recording not verified.
- [ ] Generated columns deterministic-only enforcement not verified.
- [ ] TEMPORARY/UNLOGGED flags emission not verified.

### 3) CREATE INDEX Edge Cases
- [ ] Expression index emission rules not verified.
- [ ] INCLUDE clause order preservation not verified.
- [ ] Predicate emission and invalid column references not verified.

### 4) ALTER TABLE Edge Cases
- [ ] CHANGE COLUMN (rename + type) should emit two statements; not verified.
- [ ] USING action `29` ordering not verified.
- [ ] SET/DROP DEFAULT/NOT NULL action codes and validation not verified.

### 5) SELECT Edge Cases
- [ ] DISTINCT and ALL are mutually exclusive; must reject if both present.
  - Parser sets only one flag; no explicit rejection of `SELECT DISTINCT ALL ...`. See `src/parser/parser_v3.cpp:6188-6204`.
- [ ] SELECT * expansion to explicit columns (including JOIN USING coalesce).
  - Emitter keeps `SBLR3_SELECT_STAR`/`SBLR3_SELECT_TABLE_STAR`; no expansion. See `src/parser/v3_emitter.cpp:3690-3725`.
- [~] ORDER BY numeric positions resolved (parser-time).
  - Implemented in `parseSelect`, though spec calls for emit-time resolution. See `src/parser/parser_v3.cpp:6233-6259`.
- [ ] ORDER BY alias resolution to select-item expression not implemented.
- [~] LIMIT/OFFSET/FETCH normalized into select fields (partial).
  - `LIMIT`/`OFFSET` and `FETCH FIRST/NEXT` set `limit`/`offset` expressions. No explicit `FETCH_SPEC.kind` mapping observed.

### 6) INSERT Edge Cases
- [~] DEFAULT VALUES emits source=DEFAULT (implemented).
  - `parseInsert` sets source DEFAULT; emitter sets `source=3`. See `src/parser/parser_v3.cpp:6882-6886` and `src/parser/v3_emitter.cpp:458-463`.
- [*] DEFAULT in VALUES emits `SBLR3_DEFAULT_VALUE`.
  - `LiteralType::DEFAULT` emitted as `SBLR3_DEFAULT_VALUE`. See `src/parser/parser_v3.cpp:6919-6923` and `src/parser/v3_emitter.cpp:3557-3560`.
- [~] ON CONFLICT DO NOTHING/UPDATE emitted with `on_conflict` payload (implemented), but opcode is `SBLR3_INSERT` not spec `SBLR3_DML_INSERT`.

### 7) UPDATE Edge Cases
- [ ] UPDATE FROM emission rules not verified.
- [~] Target alias emitted as payload `alias` (present in emitter).

### 8) DELETE Edge Cases
- [~] DELETE USING emitted as `using` and `using_joins` in payload.
  - See `src/parser/v3_emitter.cpp:523-541`.

### 9) MERGE Edge Cases
- [ ] Must emit `SBLR3_MERGE` only; EXT_MERGE_* forbidden.
  - V3 emitter uses `SBLR3_MERGE_START`. See `src/parser/v3_emitter.cpp:562`.

### 10) DDL Constraints
- [ ] CHECK constraint emission as TABLE_CONSTRAINT entries not verified.
- [ ] DEFERRABLE flag preservation not verified.

### 11) PSQL Edge Cases
- [ ] Variable shadowing and scope-specific symbol tables not verified.
- [ ] WHEN ... DO ordering last in block not verified.

### 12) Dialect Separation
- [ ] ScratchBird parser must not accept PG/MySQL syntax (not verified).

