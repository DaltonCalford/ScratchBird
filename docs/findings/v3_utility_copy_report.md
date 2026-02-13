# UTILITY_COPY.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/UTILITY_COPY.md` (Authoritative, updated 2026-02-08)

Summary:
- Parser/emitter supports COPY but does not emit `SBLR3_UTILITY_COPY`; it emits `SBLR3_COPY` with `SCHEMA_COPY` payload.

Key findings:

## Emission Rules
[ ] Spec requires `SBLR3_UTILITY_COPY` with `UTILITY_COPY` payload; emitter uses `SBLR3_COPY` with `SCHEMA_COPY` (`src/parser/v3_emitter.cpp:643`).

## Errors
[ ] Spec-defined error codes (`ERR_COPY_MISSING_TARGET`, `ERR_COPY_INVALID_FORMAT`, `ERR_COPY_UNSUPPORTED_OPTION`) are not surfaced as such.

