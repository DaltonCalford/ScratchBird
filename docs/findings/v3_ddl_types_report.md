# V3 DDL Types Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TYPES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 parser supports rich `CREATE TYPE` and `ALTER TYPE` syntax (ENUM/RECORD/RANGE/BASE/SHELL + options), but **emitter/executor support is largely missing**.
- `CREATE TYPE` emitter does not serialize type definitions; `ALTER TYPE` uses the wrong opcode; `DROP TYPE` maps to domain drop; and **no V3 executor handlers exist** for these opcodes.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE TYPE
[~] Parser supports ENUM/RECORD/AS (composite), RANGE, BASE, and SHELL definitions with options, dialect/compat tags, and COMMENT.
[ ] V3 emitter ignores parsed type details and emits `SBLR3_CREATE_TYPE` with empty `type` spec and placeholder options.
[ ] No V3 executor handling for `SBLR3_CREATE_TYPE`.

### ALTER TYPE
[~] Parser supports RENAME/SET SCHEMA, ADD VALUE, RENAME VALUE, SET (range/base options), FINALIZE.
[ ] V3 emitter incorrectly uses opcode `SBLR3_ALTER_DOMAIN` instead of `SBLR3_ALTER_TYPE`.
[ ] No V3 executor handling for type alteration.

### DROP TYPE
[~] Parser supports IF EXISTS and CASCADE/RESTRICT (in AST).
[ ] V3 emitter maps DROP TYPE to `SBLR3_DROP_DOMAIN` with object_type=8 and drops only the first type.
[ ] No V3 executor handling for drop type; IF EXISTS/CASCADE/RESTRICT not enforced.

### SHOW / EXTRACT
[ ] `SHOW CREATE TYPE` and `EXTRACT TYPE` not parsed/emitted.

### Security & Errors
[ ] No V3 permission checks or structured SQLSTATE error mapping for type operations.

## Key References
- Parser `CREATE TYPE`/`ALTER TYPE`: `src/parser/parser_v3.cpp:4008-4745`
- AST `CreateTypeStmt`/`AlterTypeStmt`/`DropTypeStmt`: `include/scratchbird/parser/ast_v3.h:1522-1605`
- V3 emitter `CREATE TYPE` placeholder: `src/parser/v3_emitter.cpp:1223-1243`
- V3 emitter `ALTER TYPE` opcode mismatch: `src/parser/v3_emitter.cpp:1797-1838`
- V3 emitter `DROP TYPE` mapping: `src/parser/v3_emitter.cpp:2031-2033`
- V3 schema supports `SCHEMA_DDL_CREATE_TYPE` / `SCHEMA_DDL_ALTER_TYPE`: `src/sblr/v3_schema.generated.cpp:311-318`, `src/sblr/v3_schema.generated.cpp:514-524`
