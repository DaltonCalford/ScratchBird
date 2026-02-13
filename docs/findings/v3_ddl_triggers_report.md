# V3 DDL Triggers Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_TRIGGERS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 has **minimal CREATE TRIGGER parsing** for table-level DML triggers only; most spec features are missing.
- V3 emitter produces `SBLR3_CREATE_TRIGGER`, but **executor has no V3 trigger handling**, so triggers are not created at runtime.
- `ALTER TRIGGER` and `DROP TRIGGER ... ON <table>` semantics are **not implemented** in V3.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE TRIGGER
[~] Parser supports table-level triggers with BEFORE/AFTER/INSTEAD OF and INSERT/UPDATE/DELETE event masks; `FOR EACH ROW/STATEMENT` supported.
[ ] Database-level triggers (ON CONNECT/DISCONNECT/TRANSACTION, AFTER CREATE/ALTER/DROP) not parsed.
[ ] `UPDATE OF <column_list>`, `REFERENCING OLD/NEW`, `WHEN (condition)`, `POSITION`, and function/procedure invocation are not parsed.
[ ] Trigger body is captured as raw SQL text after `EXECUTE`/`AS`; no structured call target (procedure/function name + args).
[ ] V3 emitter does not populate `when` expression field; no AST support.
[ ] V3 executor has no handler for `SBLR3_CREATE_TRIGGER` (only legacy EXT_CREATE_TRIGGER exists).

### ALTER TRIGGER
[ ] No AST or parser support for `ALTER TRIGGER ... ENABLE/DISABLE/RENAME`.

### DROP TRIGGER
[~] Parser supports `DROP TRIGGER` with schema path, but does not require `ON <table>` or database-level qualifiers.
[ ] Emitter uses only first trigger path; IF EXISTS/CASCADE/RESTRICT not serialized.
[ ] V3 executor has no handler for `SBLR3_DROP_TRIGGER`.

## Key References
- Trigger AST shape: `include/scratchbird/parser/ast_v3.h:893-920`
- Parser `CREATE TRIGGER`: `src/parser/parser_v3.cpp:3833-3960`
- V3 emitter `CREATE TRIGGER`: `src/parser/v3_emitter.cpp:954-981`
- V3 schema includes `when` field: `src/sblr/v3_schema.generated.cpp:338-346`
- No V3 executor handling for create/drop trigger; only legacy EXT_CREATE_TRIGGER: `src/sblr/executor.cpp:2655`
