# Strict Implementation Workpack: Native SQL Language Pass

## Scope
Implement native parser language families in strict order:
1. Admin
2. DDL
3. DML
4. PSQL and TSQL alias compatibility

## Mandatory Rules
1. Execute tickets in numeric order (`LD-001` to `LD-012`).
2. Do not skip tickets.
3. Do not merge tickets.
4. Do not advance when current ticket evidence is incomplete.
5. Every ticket must produce `RUN_MANIFEST.json`, `SPEC_TRACEABILITY.csv`, `TEST_RESULTS.md`, `IMPLEMENTATION_NOTES.md`, `CHECKSUMS.sha256`.

## Ticket Sequence
- `LD-001` lexical and token contracts
- `LD-002` grammar production registration
- `LD-003` admin grammar and capability keys
- `LD-004` DDL grammar and deterministic rejection
- `LD-005` DML grammar and result-shape contracts
- `LD-006` PSQL block grammar and cursor/exception semantics
- `LD-007` normalization and clause-order matrix
- `LD-008` parameter binding and type/coercion integration
- `LD-009` TSQL alias rewrite and reject modes
- `LD-010` UUID bind and feature-key/result-shape audit
- `LD-011` SQL->SBLR request envelope and diagnostics trace
- `LD-012` end-to-end language gate run

## Gate Mapping
- `P21-LANG-GATE-01` = `LD-001..LD-003`
- `P21-LANG-GATE-02` = `LD-004..LD-007`
- `P21-LANG-GATE-03` = `LD-008..LD-011`
- `P21-LANG-GATE-04` = `LD-012`
