# TRANSACTION_CONTROL.md - Implementation Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/TRANSACTION_CONTROL.md` (Authoritative, updated 2026-02-08)

Summary:
- V3 parser supports transaction control statements and transaction characteristics.
- Emitted opcodes differ from spec names (`SBLR3_START_TRANSACTION`/`SBLR3_COMMIT`/`SBLR3_ROLLBACK`/`SBLR3_SAVEPOINT`) rather than `SBLR3_TXN_*` opcodes.

Key findings:

## Emission Rules
[ ] Spec requires `SBLR3_TXN_BEGIN` / `SBLR3_TXN_COMMIT` / `SBLR3_TXN_ROLLBACK` / `SBLR3_TXN_SAVEPOINT` / `SBLR3_TXN_ROLLBACK_TO_SAVEPOINT` / `SBLR3_TXN_RELEASE_SAVEPOINT` / `SBLR3_TXN_SET_OPTIONS` opcodes. None of these opcodes exist in the registry; emitter uses `SBLR3_START_TRANSACTION`, `SBLR3_COMMIT`, `SBLR3_ROLLBACK`, `SBLR3_SAVEPOINT`, `SBLR3_ROLLBACK_TO_SAVEPOINT`, `SBLR3_RELEASE_SAVEPOINT`, and `SBLR3_SET_TRANSACTION`. (`src/parser/v3_emitter.cpp`)

## Parsing Rules
[~] Parser supports BEGIN/START TRANSACTION, COMMIT, ROLLBACK, SAVEPOINT, RELEASE, and SET TRANSACTION with characteristics (`src/parser/parser_v3.cpp`).

## Errors
[ ] Spec-defined errors (`ERR_TXN_OPTION_INVALID`, `ERR_FEATURE_NOT_SUPPORTED`) are not surfaced as such; parser raises generic parse errors.

