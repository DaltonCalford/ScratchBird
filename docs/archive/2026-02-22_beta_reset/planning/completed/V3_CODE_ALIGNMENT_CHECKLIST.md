# V3 Code Alignment Execution Checklist

Status: Active
Last Updated: 2026-02-08
Owner: ScratchBird Engineering

Purpose: tracked execution checklist per subsystem to align code with
`docs/specifications/parser/v3/`.

Legend:
- [ ] Not started
- [~] In progress
- [x] Complete

---

## 0) Global Preconditions

- [x] Freeze any new V2 pipeline changes (no new `parser_v2`/`sblr v2` usage).
- [x] Add CI guard to fail on new `parser::v2` includes under `src/`.
- [x] Confirm V3 authoritative inventory is locked and referenced by all specs.

---

## 1) V3 SBLR Container + Validation

Specs:
- `SBLR_V3_BYTECODE_CONTAINER.md`
- `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
- `SBLR_V3_BYTECODE_CANONICALIZATION.md`
- `SBLR_V3_VALIDATION_RULES.md`
- `sblr/SBLR_V3_TEST_VECTORS*.md`

Checklist:
- [x] Implement V3 container reader/writer with section table.
- [x] Implement constant pool and symbol table encoding/decoding.
- [x] Implement canonicalization rules for deterministic output.
- [x] Implement bytecode verifier (stack depth, ordering, type checks).
- [x] Add test harness for V3 test vectors (payload + full stream).
- [x] Remove V2 header parsing from any new V3 entry points.

---

## 2) V3 Opcode Registry + Payloads + Semantics

Specs:
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `SBLR_V3_OPCODE_SEMANTICS.md`
- `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

Checklist:
- [x] Implement opcode registry and numeric mapping.
- [x] Implement payload encode/decode for every opcode family.
- [x] Implement runtime semantics for each opcode.
- [x] Enforce lock ordering and constraint sequencing.
- [x] Implement literal opcodes and datatype payload schemas.
- [x] Add opcode‑level unit tests for serialize/deserialize.

---

## 3) V3 AST + ScratchBird Parser

Specs:
- `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `parser/ScratchBird Master Grammar Specification v2.0.md`
- `AST_TYPE_AND_LITERAL_SPEC.md`
- `PARSER_AMBIGUITY_RESOLUTION.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`

Checklist:
- [x] Implement AST nodes and literal forms per spec.
- [x] Implement ScratchBird grammar and parser.
- [x] Implement parse → SBLR emission for DDL/DML/PSQL.
- [x] Emit ANALYZE/CONNECT/DISCONNECT/SWEEP payloads per schema.
- [x] Normalize TXN control emission to SCHEMA_TXN_CONTROL action codes.
- [x] Implement SET/SHOW/RESET opcode selection + payloads (SESSION_AND_UTILITY).
- [x] Implement DDL ALTER stub payloads (INDEX/SCHEMA/DATABASE/DOMAIN/TYPE/POLICY/SYSTEM/JOB/TABLESPACE).
- [x] Align ALTER TABLE action bytes + payloads (if_exists/only, cascade, action list).
- [x] Implement CREATE FOREIGN DATA WRAPPER parsing + V3 SBLR emission.
- [x] Enable SHOW PARSER VERSION (alias of SHOW VERSION).
- [x] Implement deterministic ambiguity resolution rules.
- [x] Wire parser to V3 SBLR emitter (no SQL parsing in engine).

---

## 4) Emulated Parsers (PostgreSQL / MySQL / Firebird)

Specs:
- `parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `parser/MYSQL_PARSER_SPECIFICATION.md`
- `parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `findings/DIALECT_GAP_EXAMPLES.md`

Checklist:
- [x] Implement Postgres parser front‑end emitting V3 SBLR (in progress: DML + CREATE/ALTER/DROP/TRUNCATE core emit via v3 AST; remaining utility/edge DDL paths).
- [x] Implement MySQL parser front‑end emitting V3 SBLR (in progress: v3 container emission wired; CREATE/DROP DATABASE + TRUNCATE v3 AST paths added).
- [x] Implement Firebird parser front‑end emitting V3 SBLR.
- [x] Implement explicit emission rules for all dialect gaps.
- [x] Remove any shared grammar with ScratchBird parser.

---

## 5) Executor Core (V3)

Specs:
- `EXECUTOR_V3_SBLR.md`
- `EXECUTOR_V3_SQL_ENGINE.md`
- `PSQL_RUNTIME_V3.md`

Checklist:
- [x] Replace V2 executor pipeline with V3 opcode execution.
- [x] Implement PSQL runtime semantics (scopes, cursors, exceptions) (blocks/decl/assign/if/while/loop/return/call wired; FOR SELECT/EXECUTE + cursor open/fetch/close wired; SUSPEND wired; exception handlers with SQLSTATE/name matching; per‑statement savepoints; labeled EXIT/CONTINUE; label/jump support).
- [x] Implement DDL/DML execution per V3 opcode semantics (DDL core: CREATE/ALTER/DROP/TRUNCATE for tables/indexes wired, ALTER TABLE action set completed, CREATE INDEX expr/predicate + TYPE_SPEC modifiers wired; V3 DML supports SELECT/UPDATE/DELETE with WHERE and RETURNING; SELECT supports CTE WITH (including recursive UNION ALL), INNER/LEFT/RIGHT/FULL/CROSS/NATURAL JOIN + USING support, ORDER BY + DISTINCT/GROUP BY/SET OPS with aggregates (COUNT/SUM/AVG/MIN/MAX/ARRAY_AGG/STDDEV/VAR/CORR/COVAR_POP/REGR*/XMLAGG), HAVING, FILTER, grouping sets/rollup/cube, aggregate ORDER BY payloads; set ops honor ORDER/LIMIT/OFFSET; SELECT locking clauses acquire table locks (NOWAIT/SKIP LOCKED supported at table level); INSERT supports VALUES/DEFAULT/SELECT sources with defaults/on‑conflict/returning in V3; UPDATE supports FROM/JOIN; DELETE supports USING/JOIN; MERGE supported in V3).
- [x] Remove V2 semantic analyzer and bytecode generator dependencies.

---

## 6) Catalog + Domains + UUID v7

Specs:
- `catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
- `catalog/UUID_LIFECYCLE_RULES.md`
- `types/VALUE_SPEC_STORAGE_ENCODINGS.md`

Checklist:
- [x] Replace catalog raw types with SBDB$ domains (tablespace + charset/timezone + LOB UUID bindings wired).
- [x] Enforce UUID v7 generation for all identifiers.
- [x] Implement domain validation and default domain bindings.
- [x] Enforce system-domain bindings on catalog load (sweep + fixups, table/column map aligned to `SYSTEM_CATALOG_DOMAIN_MAP.md`).
- [x] Update system catalog bootstrap pages and layouts.

---

## 7) Storage + Page Layouts

Specs:
- `storage/PAGE_TYPES_AND_LAYOUTS.md`
- `types/BINARY_LAYOUT_ANNEX.md`

Checklist:
- [x] Implement page header layouts for all page types.
- [x] Implement size‑dependent formulas and slot arrays.
- [x] Remove legacy struct‑based layout assumptions.
- [x] Implement VALUE_SPEC on‑disk encoders/decoders.

---

## 8) Transaction / MGA / Locking

Specs:
- `transaction/TRANSACTION_MGA_CORE.md`
- `transaction/TRANSACTION_MAIN.md`
- `transaction/TRANSACTION_LOCK_MANAGER.md`
- `transaction/TRANSACTION_DISTRIBUTED.md`

Checklist:
- [x] Implement MGA visibility and version chain rules.
- [x] Implement TIP and transaction lifecycle.
- [x] Implement lock manager per Firebird semantics.
- [x] Align GC/sweep behavior with `FIREBIRD_GC_SWEEP_GLOSSARY.md`.

---

## 9) Index Layer Alignment

Specs:
- `indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `indexes/INDEX_GC_PROTOCOL.md`
- Per‑index specs in `indexes/`

Checklist:
- [x] Implement shared index entry metadata rules.
- [x] Implement index GC protocol (heap‑driven).
- [x] Implement BTREE, HASH, GIN, GIST, SPGIST, BRIN, BITMAP, RTREE, HNSW.
- [x] Implement IVF, FULLTEXT, ZONEMAP, ZORDER, GEOHASH, LSM, COLUMNSTORE.
- [x] Implement remaining core index types (FST, SUFFIX, ART, LEARNED, etc.).

---

## 10) Network / IPC / Protocols

Specs:
- `network/ENGINE_PARSER_IPC_CONTRACT.md`
- `network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `network/WIRE_PROTOCOL_SPECIFICATIONS.md`

Checklist:
- [x] Implement IPC framing and auth passthrough.
- [x] Implement listener/pool lifecycle and handoff.
- [x] Implement protocol baselines (PG/MySQL/Firebird).
- [x] Ensure TDS/MSSQL is rejected at listener level.

---

## 11) Tooling + Conformance Tests

Specs:
- `tools/SB_BUILD_AND_TEST_CLI_SPEC.md`
- `testing/DIALECT_CONFORMANCE_ASSERTIONS.md`
- `sblr/SBLR_V3_TEST_VECTORS*.md`

Checklist:
- [x] Implement `sb_build` and `sb_test` CLI behavior.
- [x] Add dialect conformance tests for all baselines.
- [x] Add SBLR bytecode validation and execution tests.

---

## 12) Removal of V2 Pipeline

Checklist:
- [x] Remove `parser_v2` headers from non‑V2 modules (keep legacy V2 sources only).
- [x] Remove `QueryCompilerV2` usage from server/executor.
- [x] Remove `semantic_analyzer_v2.cpp` and V2 AST dependency.
- [x] Remove V2 SBLR header handling in executor/protocol.
- [x] Validate all compilation paths are V3 only.

---

## Acceptance Criteria (Global)

- [x] All SQL paths emit V3 SBLR (no engine SQL parsing).
- [x] All V3 bytecode examples/test vectors validate and execute.
- [x] Catalog uses SBDB$ domains and UUID v7 identifiers everywhere.
- [x] Lock ordering and MGA visibility match V3 specs.
- [x] No `parser::v2` references remain in `src/`.
