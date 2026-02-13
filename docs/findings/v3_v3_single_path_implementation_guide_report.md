# V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md - Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md` (Authoritative, updated 2026-02-08)

Summary:
- This is a high-level guidance document. Referenced parser BNF and PSQL grammar files exist under `docs/specifications/parser/v3/parser/`.
- Several opcode-group guidance statements conflict with other authoritative specs already reviewed (e.g., `SBLR3_QUERY_*`, `SBLR3_DML_*`, `SBLR3_TXN_*` vs current registry/emitter).

Key findings:

## Reference Integrity
[*] `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md` exists under `docs/specifications/parser/v3/parser/`.
[*] `parser/05_PSQL_PROCEDURAL_LANGUAGE.md` exists under `docs/specifications/parser/v3/parser/`.

## Consistency with Implementation
[~] Guidance aligns with opcode group naming in principle, but previously identified opcode mismatches (QUERY, DML, TXN variants) remain unresolved in current implementation.

