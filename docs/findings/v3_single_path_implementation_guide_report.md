# Findings: V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md

Spec file: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md`

## Summary
This is a high-level authoritative guide that cross-references many other specs. The implementation is partially aligned in some areas (DDL/DML/TXN/PSQL/session opcodes and executor scaffolding exist), but major mismatches already documented in specific specs (notably DCL/GRANT-REVOKE and utility opcode naming) indicate the “single-path” invariant is not fully met. This guide is not directly verifiable without checking each referenced spec item-by-item; those checks should be handled in the corresponding spec reports.

## Implemented (Partial)
- DDL/DML/TXN opcodes exist and are emitted in the V3 pipeline for many statements.
- PSQL runtime exists in executor (`PSQL_RUNTIME_V3` is partially implemented).
- Session/utility opcodes (SET/SHOW/EXPLAIN/etc.) exist in V3 executor.

## Gaps / Discrepancies (Representative)
- DCL path is inconsistent with `ACCESS_CONTROL.md` (role grants, DCL opcode emission, and V3 executor handling are incomplete).
- Utility/COPY opcode naming diverges (`SBLR3_COPY` vs spec’s `SBLR3_UTILITY_COPY`).
- Several referenced specs appear non-authoritative or missing in V3 inventory (e.g., storage/MGA_IMPLEMENTATION.md is referenced but not in the V3 authoritative inventory).

## Notes
- This guide is best treated as a dependency map. Conformance should be evaluated in the underlying authoritative specs rather than this summary.

## Suggested Next Steps
- Keep this guide as a map; track conformance in each referenced authoritative spec’s report.
- If needed, add a checklist that points to the authoritative spec status (pass/partial/fail) for each section.
