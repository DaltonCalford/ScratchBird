# V3 DDL Databases Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_DATABASES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 code currently implements **emulated database** creation/alter/drop flows, but **native CREATE DATABASE options** (page size, charset/collate, encrypted, owner, sweep interval) are not parsed into AST nor emitted to SBLR.
- V3 emitter/executor wiring for CREATE DATABASE appears incomplete for emulated metadata (path/source/options/aliases are not emitted).

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE DATABASE (native)
[ ] Parser support for PAGE_SIZE, DEFAULT CHARACTER SET, DEFAULT COLLATE, ENCRYPTED, OWNER.
    - `parseCreateDatabase` only accepts IF NOT EXISTS + schema path; no native option parsing.
    - AST `CreateDatabaseStmt` has no fields for page size/charset/collation/encryption/owner/tablespace.
[ ] Emitter support for CREATE DATABASE options.
    - `v3_emitter.cpp` only emits `name`, `encrypted=false`, empty `options` placeholder.
[ ] Executor support for native CREATE DATABASE (non-emulated).
    - `executeCreateDatabase` expects emulated path semantics; uses `extractEmulatedDatabaseComponents`.

### CREATE DATABASE EMULATED
[~] Parser supports EMULATED dialect, ON SERVER, ALIAS, WITH OPTIONS, and source_spec parsing.
    - Implemented in `parseCreateDatabase` (EMULATED branch).
[ ] Emitter includes emulated path/source/options/aliases.
    - Emitter currently emits only `name` and placeholder options; path/source/aliases are not serialized.
[~] Executor implements emulated database creation.
    - `executeCreateDatabase` creates emulation schema, emulation type/server/db records, views, aliases.

### ALTER DATABASE
[ ] Parser support for SET DEFAULT CHARACTER SET, SET DEFAULT COLLATE, SET SWEEP INTERVAL.
    - `parseAlterDatabase` only supports RENAME, OWNER, ALIAS ADD/DROP.
[ ] Emitter support for ALTER DATABASE options.
    - Emits options from AST only; AST lacks these fields.
[~] Executor supports emulated ALTER actions (rename, owner, add/drop alias, set options).
    - No sweep interval or charset/collation handling in V3 path.

### DROP DATABASE
[~] Parser accepts IF EXISTS and CASCADE/RESTRICT; CASCADE maps to `force`.
[~] Executor supports DROP with active-session checks and FORCE (superuser only).
    - Spec says RESTRICT/CASCADE; code uses FORCE semantics for CASCADE.

## Notes
- If native CREATE DATABASE is not intended for V3 (engine single DB), the spec should be clarified and/or moved out of authoritative DDL.
- If EMULATED is the intended scope, emitter must serialize emulated metadata (path/source/options/aliases) so executor receives required data.
