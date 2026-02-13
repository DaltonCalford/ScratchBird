# V3 Implementation Safety Summary (Low-Context AI)

Status: Authoritative (V3)
Last Updated: 2026-02-08

Purpose: single‑page checklist for an AI with no database background to
implement ScratchBird deterministically without gray areas.

---

## 1) Bytecode Canonicalization
- Use `SBLR_V3_OPCODE_SPEC.md` for opcode registry.
- Encode payloads per `SBLR_V3_OPCODE_PAYLOADS.md`.
- Follow bytecode canonicalization rules in `SBLR_V3_BYTECODE_CANONICALIZATION.md`.
- Use constant/symbol determinism rules in `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.

## 2) Test Vectors
- Payload examples: `sblr/SBLR_V3_BYTECODE_EXAMPLES.md`
- Full streams: `sblr/SBLR_V3_TEST_VECTORS_FULL.md`
- Verifier expectations: `sblr/SBLR_V3_TEST_VECTORS.md`

## 3) Parser → SBLR Emission
- Primary rules: `PARSER_TO_SBLR_EMISSION_RULES.md`
- Ambiguity resolution: `PARSER_AMBIGUITY_RESOLUTION.md`
- Dialect gaps with concrete examples: `findings/DIALECT_GAP_EXAMPLES.md`

## 4) Executor Semantics
- Core executor: `EXECUTOR_V3_SBLR.md`
- Lock/GC/constraint matrix: `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`

## 5) Storage Encodings
- Canonical type storage: `types/VALUE_SPEC_STORAGE_ENCODINGS.md`
- Binary layout annex: `types/BINARY_LAYOUT_ANNEX.md`
- On‑disk layout + page types: `storage/PAGE_TYPES_AND_LAYOUTS.md`

## 6) Catalog Rules
- System catalog DDL: `catalog/SYSTEM_CATALOG_DDL_SBDB.md`
- Domain map: `catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
- UUID lifecycle: `catalog/UUID_LIFECYCLE_RULES.md`

## 7) Monitoring/Optimization
- Monitoring views + metrics: `operations/MONITORING_SQL_VIEWS.md`
- Optimizer determinism: `query/QUERY_OPTIMIZER_SPEC.md`

## 8) Build/Conformance
- Build/test CLI: `tools/SB_BUILD_AND_TEST_CLI_SPEC.md`
- Dialect assertions: `testing/DIALECT_CONFORMANCE_ASSERTIONS.md`

---

If any spec conflicts, the V3 tree is authoritative. When uncertain, check:
- `V3_SERVER_SPEC_INDEX.md`
- `findings/NO_GREY_AREAS_GATE.md`
