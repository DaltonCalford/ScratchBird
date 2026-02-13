# UPDATE.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/UPDATE.md` (Authoritative, updated 2026-02-08)

Summary:
- Parser/emitter supports UPDATE but does not emit `SBLR3_DML_UPDATE` (spec) and instead uses `SBLR3_UPDATE` with `SCHEMA_UPDATE` payload.

Key findings:

## Emission Rules
[ ] Spec requires `SBLR3_DML_UPDATE` with `DML_UPDATE` payload; emitter uses `SBLR3_UPDATE` with `SCHEMA_UPDATE` (`src/parser/v3_emitter.cpp:506`, `src/sblr/v3_schema.generated.cpp`).

## Errors
[ ] Spec-defined error codes (`ERR_PARSE_EXPECTED_ASSIGNMENT`, `ERR_FEATURE_NOT_SUPPORTED`, `ERR_COLUMN_NOT_FOUND`) are not surfaced as such.

