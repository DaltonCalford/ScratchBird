# V3 Single‑Path Implementation Guide
Last Updated: 2026-02-08
Status: Authoritative (V3)

Purpose: provide a deterministic, end‑to‑end path from SQL text → AST → SBLR →
Executor → Storage for each statement family. This is the primary guide for
implementers and low‑reasoning AI.

## 1. Global Invariants (Firebird MGA Alignment)
- All visibility and GC rules follow Firebird MGA semantics:
  `transaction/TRANSACTION_MGA_CORE.md`, `storage/MGA_IMPLEMENTATION.md`.
- Lock acquisition order is defined in `EXECUTOR_V3_SBLR.md`.
- SBLR emission is required for all statements (`SBLR_V3_OPCODE_SPEC.md`).

## 2. DDL Path (CREATE/ALTER/DROP)

### 2.1 Parse → AST
- Grammar: `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- AST nodes: `AST_TYPE_AND_LITERAL_SPEC.md`

### 2.2 AST → SBLR
- Emit DDL opcodes from `SBLR_V3_OPCODE_SPEC.md`.
- Encode payloads per `SBLR_V3_OPCODE_PAYLOADS.md`.

### 2.3 SBLR → Executor
- DDL opcodes update catalog tables (`catalog/SYSTEM_CATALOG_STRUCTURE.md`).
- Domain/type/constraint enforcement follows `ddl/` specs.

### 2.4 Executor → Storage
- Persist catalog changes using canonical encoding (`types/VALUE_SPEC_STORAGE_ENCODINGS.md`).
- Catalog writes obey MGA + lock ordering.

## 3. DML Path (SELECT/INSERT/UPDATE/DELETE/MERGE)

### 3.1 Parse → AST
- Grammar: `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- Query AST nodes and expression forms.

### 3.2 AST → SBLR
- Emit query structure opcodes (CTE/SELECT/FROM/WHERE/etc.).
- Emit expression opcodes for predicates and projections.

### 3.3 SBLR → Executor
- Build plan per `query/QUERY_OPTIMIZER_SPEC.md`.
- Execute using MGA visibility rules.
- Apply constraint enforcement order (`EXECUTOR_V3_SBLR.md`).

### 3.4 Executor → Storage
- Insert/update/delete write new record versions, never overwrite.
- Index updates follow per‑index specs in `indexes/`.

## 4. Transaction Path (BEGIN/COMMIT/ROLLBACK)

### 4.1 Parse → AST
- Grammar: `TRANSACTION_CONTROL.md` + BNF.

### 4.2 AST → SBLR
- Emit TXN opcodes from `SBLR_V3_OPCODE_SPEC.md`.

### 4.3 Executor → Storage
- Create transaction context, update TIP, manage locks.
- Commit/rollback update transaction inventory and GC horizons.

## 5. DCL Path (GRANT/REVOKE/ROLE/USER)

### 5.1 Parse → AST
- Grammar: `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`.

### 5.2 AST → SBLR
- Emit DCL opcodes; payloads include role/user IDs.

### 5.3 Executor → Storage
- Update security catalogs under `sys.sec.*`.

## 6. PSQL Path (Procedures/Triggers/Blocks)

### 6.1 Parse → AST
- Grammar: `parser/05_PSQL_PROCEDURAL_LANGUAGE.md`.
- AST: `AST_TYPE_AND_LITERAL_SPEC.md`.

### 6.2 AST → SBLR
- Emit PSQL opcodes and control flow blocks.

### 6.3 Executor
- Execute in PSQL runtime per `PSQL_RUNTIME_V3.md`.
- Maintain frame stack and exception propagation.

## 7. Session/Utility Path (SET/SHOW/EXPLAIN/ANALYZE)

- Emit SESSION opcodes.
- Executor applies session changes and may touch catalog state for persistent settings.

## 8. Index Maintenance Path

- Index build and update follow per‑index specs in `indexes/`.
- Index GC follows `INDEX_GC_PROTOCOL.md`.
- MGA visibility is mandatory for any index scan.

## 9. Reference Map

- SBLR registry: `SBLR_V3_OPCODE_SPEC.md`
- Payload schemas: `SBLR_V3_OPCODE_PAYLOADS.md`
- Opcode semantics: `SBLR_V3_OPCODE_SEMANTICS.md`
- Executor model: `EXECUTOR_V3_SBLR.md`
- Storage format: `storage/PAGE_TYPES_AND_LAYOUTS.md`

---

## 10. Opcode Bindings (Required)

Implementers MUST map each statement family to the canonical opcode groups:

- DDL → DDL group (`SBLR3_CREATE_*`, `SBLR3_ALTER_*`, `SBLR3_DROP_*`)
- DML → DML group (`SBLR3_SELECT`, `SBLR3_INSERT`, `SBLR3_UPDATE`, `SBLR3_DELETE`, `SBLR3_MERGE_*`)
- TXN → TXN group (`SBLR3_START_TRANSACTION`, `SBLR3_COMMIT`, `SBLR3_ROLLBACK`, `SBLR3_SAVEPOINT`)
- DCL → DCL group (`SBLR3_GRANT*`, `SBLR3_REVOKE*`, `SBLR3_SET_ROLE`)
- SESSION/UTILITY → SESSION group (`SBLR3_SET_*`, `SBLR3_SHOW_*`, `SBLR3_EXPLAIN_PLAN`, `SBLR3_ANALYZE`)
- PSQL → PSQL group (`SBLR3_BLOCK`, `SBLR3_IF`, `SBLR3_FOR_*`, `SBLR3_RAISE`)

All payloads MUST follow `SBLR_V3_OPCODE_PAYLOADS.md`.
