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

- [ ] Freeze any new V2 pipeline changes (no new `parser_v2`/`sblr v2` usage).
- [ ] Add CI guard to fail on new `parser::v2` includes under `src/`.
- [ ] Confirm V3 authoritative inventory is locked and referenced by all specs.

---

## 1) V3 SBLR Container + Validation

Specs:
- `SBLR_V3_BYTECODE_CONTAINER.md`
- `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`
- `SBLR_V3_BYTECODE_CANONICALIZATION.md`
- `SBLR_V3_VALIDATION_RULES.md`
- `sblr/SBLR_V3_TEST_VECTORS*.md`

Checklist:
- [ ] Implement V3 container reader/writer with section table.
- [ ] Implement constant pool and symbol table encoding/decoding.
- [ ] Implement canonicalization rules for deterministic output.
- [ ] Implement bytecode verifier (stack depth, ordering, type checks).
- [ ] Add test harness for V3 test vectors (payload + full stream).
- [ ] Remove V2 header parsing from any new V3 entry points.

---

## 2) V3 Opcode Registry + Payloads + Semantics

Specs:
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `SBLR_V3_OPCODE_SEMANTICS.md`
- `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

Checklist:
- [ ] Implement opcode registry and numeric mapping.
- [ ] Implement payload encode/decode for every opcode family.
- [ ] Implement runtime semantics for each opcode.
- [ ] Enforce lock ordering and constraint sequencing.
- [ ] Implement literal opcodes and datatype payload schemas.
- [ ] Add opcode‑level unit tests for serialize/deserialize.

---

## 3) V3 AST + ScratchBird Parser

Specs:
- `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
- `parser/ScratchBird Master Grammar Specification v2.0.md`
- `AST_TYPE_AND_LITERAL_SPEC.md`
- `PARSER_AMBIGUITY_RESOLUTION.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`

Checklist:
- [ ] Implement AST nodes and literal forms per spec.
- [ ] Implement ScratchBird grammar and parser.
- [ ] Implement parse → SBLR emission for DDL/DML/PSQL.
- [ ] Implement deterministic ambiguity resolution rules.
- [ ] Wire parser to V3 SBLR emitter (no SQL parsing in engine).

---

## 4) Emulated Parsers (PostgreSQL / MySQL / Firebird)

Specs:
- `parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `parser/MYSQL_PARSER_SPECIFICATION.md`
- `parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`
- `findings/DIALECT_GAP_EXAMPLES.md`

Checklist:
- [ ] Implement Postgres parser front‑end emitting V3 SBLR.
- [ ] Implement MySQL parser front‑end emitting V3 SBLR.
- [ ] Implement Firebird parser front‑end emitting V3 SBLR.
- [ ] Implement explicit emission rules for all dialect gaps.
- [ ] Remove any shared grammar with ScratchBird parser.

---

## 5) Executor Core (V3)

Specs:
- `EXECUTOR_V3_SBLR.md`
- `EXECUTOR_V3_SQL_ENGINE.md`
- `PSQL_RUNTIME_V3.md`

Checklist:
- [ ] Replace V2 executor pipeline with V3 opcode execution.
- [ ] Implement PSQL runtime semantics (scopes, cursors, exceptions).
- [ ] Implement DDL/DML execution per V3 opcode semantics.
- [ ] Remove V2 semantic analyzer and bytecode generator dependencies.

---

## 6) Catalog + Domains + UUID v7

Specs:
- `catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
- `catalog/UUID_LIFECYCLE_RULES.md`
- `types/VALUE_SPEC_STORAGE_ENCODINGS.md`

Checklist:
- [ ] Replace catalog raw types with SBDB$ domains.
- [ ] Enforce UUID v7 generation for all identifiers.
- [ ] Implement domain validation and default domain bindings.
- [ ] Update system catalog bootstrap pages and layouts.

---

## 7) Storage + Page Layouts

Specs:
- `storage/PAGE_TYPES_AND_LAYOUTS.md`
- `types/BINARY_LAYOUT_ANNEX.md`

Checklist:
- [ ] Implement page header layouts for all page types.
- [ ] Implement size‑dependent formulas and slot arrays.
- [ ] Remove legacy struct‑based layout assumptions.
- [ ] Implement VALUE_SPEC on‑disk encoders/decoders.

---

## 8) Transaction / MGA / Locking

Specs:
- `transaction/TRANSACTION_MGA_CORE.md`
- `transaction/TRANSACTION_MAIN.md`
- `transaction/TRANSACTION_LOCK_MANAGER.md`
- `transaction/TRANSACTION_DISTRIBUTED.md`

Checklist:
- [ ] Implement MGA visibility and version chain rules.
- [ ] Implement TIP and transaction lifecycle.
- [ ] Implement lock manager per Firebird semantics.
- [ ] Align GC/sweep behavior with `FIREBIRD_GC_SWEEP_GLOSSARY.md`.

---

## 9) Index Layer Alignment

Specs:
- `indexes/INDEX_IMPLEMENTATION_SPEC.md`
- `indexes/INDEX_GC_PROTOCOL.md`
- Per‑index specs in `indexes/`

Checklist:
- [ ] Implement shared index entry metadata rules.
- [ ] Implement index GC protocol (heap‑driven).
- [ ] Implement BTREE, HASH, GIN, GIST, SPGIST, BRIN, BITMAP, RTREE, HNSW.
- [ ] Implement IVF, FULLTEXT, ZONEMAP, ZORDER, GEOHASH, LSM, COLUMNSTORE.
- [ ] Implement remaining core index types (FST, SUFFIX, ART, LEARNED, etc.).

---

## 10) Network / IPC / Protocols

Specs:
- `network/ENGINE_PARSER_IPC_CONTRACT.md`
- `network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `network/WIRE_PROTOCOL_SPECIFICATIONS.md`

Checklist:
- [ ] Implement IPC framing and auth passthrough.
- [ ] Implement listener/pool lifecycle and handoff.
- [ ] Implement protocol baselines (PG/MySQL/Firebird).
- [ ] Ensure TDS/MSSQL is rejected at listener level.

---

## 11) Tooling + Conformance Tests

Specs:
- `tools/SB_BUILD_AND_TEST_CLI_SPEC.md`
- `testing/DIALECT_CONFORMANCE_ASSERTIONS.md`
- `sblr/SBLR_V3_TEST_VECTORS*.md`

Checklist:
- [ ] Implement `sb_build` and `sb_test` CLI behavior.
- [ ] Add dialect conformance tests for all baselines.
- [ ] Add SBLR bytecode validation and execution tests.

---

## 12) Removal of V2 Pipeline

Checklist:
- [ ] Remove `parser_v2` headers from `src/`.
- [ ] Remove `QueryCompilerV2` usage from server/executor.
- [ ] Remove `semantic_analyzer_v2.cpp` and V2 AST dependency.
- [ ] Remove V2 SBLR header handling in executor/protocol.
- [ ] Validate all compilation paths are V3 only.

---

## Acceptance Criteria (Global)

- [ ] All SQL paths emit V3 SBLR (no engine SQL parsing).
- [ ] All V3 bytecode examples/test vectors validate and execute.
- [ ] Catalog uses SBDB$ domains and UUID v7 identifiers everywhere.
- [ ] Lock ordering and MGA visibility match V3 specs.
- [ ] No `parser::v2` references remain in `src/`.
