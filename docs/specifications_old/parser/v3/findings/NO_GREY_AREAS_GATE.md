# No-Grey-Areas Gate Checklist (V3)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: enumerate every remaining missing/ambiguous specification element
that could prevent a low-context AI from implementing ScratchBird correctly.
Each item is marked Open/Closed and must be closed with exact, deterministic
rules, byte-level schemas, and explicit error conditions.

Maintenance rule: any change that opens or closes an item MUST update this
checklist in the same change set.

Status Legend:
- `OPEN` = ambiguity remains
- `CLOSED` = fully specified with authoritative rules + examples

## 1) Core Bytecode + Encoding
- `CLOSED` VALUE_SPEC canonical encoding (literal payloads use nested literal opcode nodes).
- `CLOSED` Per-opcode payload bytes for *all* complex DDL/DML variants (full catalog of examples) in `sblr/SBLR_V3_BYTECODE_EXAMPLES.md`.
- `CLOSED` Full stream-level bytecode vectors with headers in `sblr/SBLR_V3_TEST_VECTORS_FULL.md`.
- `CLOSED` Complete constant-pool/symbol table determinism rules (including hashing order and canonical sort) in `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.
- `CLOSED` End-to-end bytecode canonicalization rules in `SBLR_V3_BYTECODE_CANONICALIZATION.md`.

## 2) Parser → SBLR Emission
- `CLOSED` Full parse-to-SBLR emission rules for every DDL/DML/PSQL command variant (edge cases) in `PARSER_TO_SBLR_EMISSION_RULES.md`.
- `CLOSED` Exhaustive dialect gap examples (MySQL/PostgreSQL) for every item in gap matrices in `findings/DIALECT_GAP_EXAMPLES.md`.
- `CLOSED` Deterministic rule for resolving ambiguous grammar cases (conflict precedence table) in `PARSER_AMBIGUITY_RESOLUTION.md`.

## 3) Executor Semantics
- `CLOSED` Full constraint enforcement matrix per opcode in `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.
- `CLOSED` Lock ordering table per opcode (including escalation + deadlock resolution) in `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.
- `CLOSED` Error code map per semantic violation (opcode-specific) in `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.

## 4) Storage + Catalog Invariants
- `CLOSED` Canonical storage encoding for all types in `VALUE_SPEC` in `types/VALUE_SPEC_STORAGE_ENCODINGS.md`.
- `CLOSED` Consolidated binary layout annex in `types/BINARY_LAYOUT_ANNEX.md`.
- `CLOSED` Catalog system domain table: every system column mapped to a domain (SBDB$*) in `catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`.
- `CLOSED` Canonical ID/UUID lifecycle rules (generation, persistence, and replication semantics) in `catalog/UUID_LIFECYCLE_RULES.md`.

## 5) Monitoring/Performance/Optimization
- `CLOSED` Mandatory sys.performance counter list with exact units and reset semantics in `operations/MONITORING_SQL_VIEWS.md`.
- `CLOSED` Required monitoring view row-level visibility rules for all dialects in `operations/MONITORING_SQL_VIEWS.md`.
- `CLOSED` Query optimizer determinism rules (tie-breakers for plan choices) in `query/QUERY_OPTIMIZER_SPEC.md`.

## 6) Validation + Verification
- `CLOSED` Verifier error codes and failure conditions (see `SBLR_V3_VALIDATION_RULES.md`).
- `CLOSED` Formal bytecode canonicalization verifier (ordering, optional field constraints) in `SBLR_V3_VALIDATION_RULES.md` + `SBLR_V3_BYTECODE_CANONICALIZATION.md`.
- `CLOSED` Test-vector corpus (bytecode + expected results) for every opcode family in `sblr/SBLR_V3_TEST_VECTORS.md`.

## 7) Tooling + Build
- `CLOSED` CLI tool contract for build/test runner (inputs, outputs, exit codes) in `tools/SB_BUILD_AND_TEST_CLI_SPEC.md`.
- `CLOSED` Conformance harness required assertions for each dialect in `testing/DIALECT_CONFORMANCE_ASSERTIONS.md`.

## 8) Scope + Protocol Boundaries
- `CLOSED` Authoritative scope rule (only the V3 document list is normative) in `README.md`.
- `CLOSED` MSSQL/TDS protocol rejection rules across network specs in `network/`.
- `CLOSED` WAL is forbidden in V3 across transaction, backup, and catalog specs.

## 9) Authoritative Set Lock + Dialect Baselines
- `CLOSED` Authoritative set locked by `AUTHORITATIVE_SPEC_INVENTORY.md`; non-inventory files are explicitly non-authoritative.
- `CLOSED` Dialect baselines normalized to PostgreSQL 16+, MySQL 8.x, Firebird 5.x in emulation specs.

## Closed Items
- VALUE_SPEC canonical encoding: `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- Verifier error codes: `/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`
- V3 authoritative scope rule: `/docs/specifications/parser/v3/README.md`
- V3 authoritative inventory: `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`
