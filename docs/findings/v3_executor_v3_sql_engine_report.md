# V3 Executor SQL Engine Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/EXECUTOR_V3_SQL_ENGINE.md`
Date: 2026-02-09
Status: Partially implemented

## Summary
The V3 executor provides direct SBLR3 execution for core statements (notably SELECT/INSERT/UPDATE/DELETE and transactional opcodes) but the spec’s full SQL engine pipeline and semantic guarantees are only partially present. The compiler/emitter path appears to produce SBLR directly without an explicit logical/physical plan stage, and window semantics for V3 opcodes are not implemented in the V3 executor path. Many items in this spec are high-level; only concrete, code-backed items were checked.

## Findings by Spec Item

### 2. Execution Pipeline (AST -> bind -> logical/physical plan -> SBLR -> VM)
- [~] Parse SQL to AST and emit SBLR V3.
  - V3 parser emits SBLR3; V3 executor runs container bytecode. See `src/parser/v3_emitter.cpp` and `src/sblr/executor.cpp:39817`.
- [ ] Name/type resolution and catalog ID binding stage is not clearly separated.
  - No dedicated V3 binder/plan stage was found; emission appears to occur directly from parser/AST structures.
- [ ] Logical/physical plan construction not evident for V3.
  - Optimizer libraries exist, but V3 integration into a plan pipeline is not visible in the V3 parser/emitter path.

### 3. Query Semantics (SELECT/INSERT/UPDATE/DELETE/MERGE)
- [~] SELECT handled by V3 executor.
  - `SBLR3_SELECT` is implemented in `Executor::executeV3` with flags, CTEs, ORDER BY/OFFSET/LIMIT handling. See `src/sblr/executor.cpp:44316` and following.
- [~] INSERT/UPDATE/DELETE/MERGE have V3 cases, but full semantic parity not verified.
  - `SBLR3_INSERT`, `SBLR3_UPDATE`, `SBLR3_DELETE`, `SBLR3_MERGE_START` cases exist; detailed semantic checks (defaults, cascades, MERGE branch semantics) are not fully verified.

### 4. Joins
- [ ] Full join type coverage (INNER/LEFT/RIGHT/FULL/CROSS/SEMI/ANTI) not verified for V3 SBLR.
  - V2 join handling exists; V3 join semantics need explicit verification in V3 path.

### 5. Aggregation
- [~] Aggregate opcodes are referenced in executor paths.
  - V3 opcode names include `SBLR3_AGG_*` and are recognized for aggregate handling, but ordered/DISTINCT aggregates are not verified. See `src/sblr/executor.cpp:45368-45720`.

### 6. Window Functions
- [ ] V3 window opcodes are not executed by V3 executor.
  - V2 WINDOW opcode handling exists; no `SBLR3_WIN_*` execution path was found. See `src/sblr/v3_opcodes.generated.cpp` for V3 opcodes and absence of handling in `src/sblr/executor.cpp` V3 path.

### 7. Transaction Semantics
- [~] Transaction opcodes exist and are handled in V3 executor.
  - `SBLR3_START_TRANSACTION`, `SBLR3_COMMIT`, `SBLR3_ROLLBACK`, savepoints, etc. are implemented. See `src/sblr/executor.cpp:51995-52434`.
- [ ] Default MVCC snapshot isolation not verified.
  - Isolation defaults and MVCC semantics are managed in connection/transaction layers but were not traced for V3-specific defaults.

### 8. Domain Enforcement in SQL
- [ ] Domain constraint order (domain before table constraints) not verified in V3 DML.
  - No explicit enforcement ordering checks were found in the V3 execution path.

### 9. Catalog Integration
- [ ] UUID v7 catalog IDs at compile time not verified.
  - Executor and parser use UUIDs but V7 enforcement was not found.

### 10. Utility and Session Statements
- [~] Utility statements exist in V3 executor (COPY/SET/SHOW/transactional/PSQL), but full coverage not verified.
  - V3 opcode cases for COPY and session/transaction control exist, but feature parity was not exhaustively checked.

### 11. Required Tests
- [ ] Required test suites are not present in this spec review.
  - No dedicated end-to-end V3 SQL engine tests were located/verified here.

## Notes
- This spec is authoritative; gaps should be reconciled with more concrete, opcode-level specs (e.g., `SBLR_V3_OPCODE_SPEC.md`, `SBLR_V3_OPCODE_SEMANTICS.md`) and parser emission rules.
