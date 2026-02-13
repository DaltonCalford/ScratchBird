# V3 Parser Joins Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/JOINS.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
V3 parser supports FROM clause table refs, join types, and ON/USING, but the AST does not expose all fields required by the spec (NATURAL, lateral). Emission does not use the normative join opcodes (`SBLR3_TABLE_REF`, `SBLR3_JOIN_TYPE`, `SBLR3_JOIN_CONDITION`, `SBLR3_JOIN_USING`) and instead encodes joins in `SBLR3_SELECT` payload. V3 executor does not implement V3 join opcodes. NATURAL/USING semantics and SQLSTATE error mappings are not implemented in V3 execution.

## Findings by Spec Item

### 1) Grammar
- [~] FROM clause + join types supported, including comma joins -> CROSS.
  - `parseFromClause` handles JOIN keywords and comma joins; comma joins set `JoinType::CROSS`. See `src/parser/parser_v3.cpp:6407-6436`.
- [~] JOIN types INNER/LEFT/RIGHT/FULL/CROSS supported.
  - `parseJoinType` maps these; NATURAL variants are parsed into specialized join types. See `src/parser/parser_v3.cpp:6527-6553`.
- [~] ON/USING parsed for non-CROSS/NATURAL joins.
  - `parseJoin` parses `ON` or `USING` with columns. See `src/parser/parser_v3.cpp:6555-6594`.
- [ ] LATERAL subquery handling.
  - `parseTableRef` accepts `LATERAL` but marks it as `SUBQUERY` (no distinct LATERAL type), so lateral semantics are not represented. See `src/parser/parser_v3.cpp:6453-6471`.

### 2) AST Schema
- [ ] TableRef kind lacks LATERAL_SUBQUERY; no `alias_columns` for subquery output beyond `column_aliases`, and no `func_args` in TableRef.
  - `TableRefNode::Type` only has TABLE/SUBQUERY/FUNCTION/JOIN; no LATERAL. See `include/scratchbird/parser/ast_v3.h:3120-3149`.
- [ ] JoinNode lacks `is_natural` and does not model USING vs NATURAL semantics explicitly.
  - JoinNode has `join_type`, `on_condition`, `using_columns`, `has_using`. NATURAL is encoded into join_type enum, not `is_natural`. See `include/scratchbird/parser/ast_v3.h:3152-3174`.

### 3) SBLR Emission (Normative)
- [ ] No emission of `SBLR3_TABLE_REF`, `SBLR3_JOIN_TYPE`, `SBLR3_JOIN_CONDITION`, `SBLR3_JOIN_USING`.
  - V3 emitter encodes joins as JSON-like payload in `SBLR3_SELECT` via `toJoins`/`toTableRef`. See `src/parser/v3_emitter.cpp:4030-4085`.
- [ ] No join algorithm opcodes emitted (`SBLR3_HASH_JOIN`, `SBLR3_NESTED_LOOP_JOIN`).
  - V3 executor does not reference these opcodes; only v2 executor has HASH_JOIN/NESTED_LOOP_JOIN. See `src/sblr/executor.cpp` and `src/sblr/v3_opcodes.generated.cpp`.

### 4) Executor Semantics (Normative)
- [ ] V3 join semantics (INNER/LEFT/RIGHT/FULL/CROSS) not implemented via V3 opcodes.
  - V3 execution path does not handle `SBLR3_JOIN_*` opcodes; join logic in v2 path uses legacy opcodes.
- [ ] NATURAL/USING expansion semantics not implemented in V3 execution.
- [ ] LATERAL semantics not implemented in V3 execution.
- [ ] Locking semantics for joins (PK order) not verified in V3 path.

### 5) Error Codes / SQLSTATE
- [ ] SQLSTATE mappings (42601/42703/42883/42P01) not implemented in V3 join parsing/execution paths.

## Notes
- The V3 parser currently treats NATURAL joins as distinct join types, but there is no executor logic to translate NATURAL/USING into equality predicates for V3.
