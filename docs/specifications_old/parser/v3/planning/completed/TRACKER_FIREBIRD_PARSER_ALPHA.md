# Firebird Parser Alpha Tracker

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Source audit:** `docs/findings/EMULATED_PARSER_FULL_AUDIT_2026-02-02.md`

## Alpha Blockers
- [x] Complete ALTER/DROP/RECREATE coverage (remove generic "not yet implemented" errors). (`src/parser/firebird/firebird_parser.cpp:1982, 2068, 2223`).

## High Priority (Alpha)
- [x] RDB$GET_CONTEXT / RDB$SET_CONTEXT treated as non-reserved keywords (function calls parse). (`/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md:78-140`).
- [x] UPDATE OR INSERT syntax verification and bytecode emission. (`/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md:329-357`).
- [x] MERGE executor compatibility review for EXT_MERGE_* opcodes (engine gap). (`/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md:343-360`).

## Progress Notes
- 2026-02-02: Added RECREATE support for SEQUENCE/PROCEDURE/FUNCTION/TRIGGER/PACKAGE/EXCEPTION in `src/parser/firebird/firebird_parser.cpp:2205-2250`.
- 2026-02-02: Added explicit ALTER handling for SEQUENCE/GENERATOR and ROLE to avoid generic errors in `src/parser/firebird/firebird_parser.cpp:1968-2045`.
- 2026-02-02: Added explicit ALTER/DROP/RECREATE errors for USER/MAPPING/SHADOW to avoid generic fallback paths in `src/parser/firebird/firebird_parser.cpp:2000-2085, 2240-2290`.
- 2026-01-28: Allowed RDB$GET_CONTEXT/RDB$SET_CONTEXT as non-reserved identifiers and mapped UPDATE OR INSERT to ON CONFLICT UPDATE in `src/parser/firebird/firebird_parser.cpp`.
- 2026-01-28: Confirmed executor support for EXT_MERGE_* bytecode from Firebird MERGE via shared v2 compiler (`src/sblr/executor.cpp:17296-17580`).

## optional extension
- [ ] PSQL test suite + remaining control-structure parity. (`/docs/specifications/parser/v3/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md:378-1009`).
