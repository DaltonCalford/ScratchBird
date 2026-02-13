# V3 DDL Exceptions Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_EXCEPTIONS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 implements a **simplified** CREATE EXCEPTION: name + message only. No parameters, templates, builder, or options (severity/SQLSTATE/hint/detail).
- DROP EXCEPTION and OR REPLACE are implemented in executor.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE EXCEPTION
[~] Parser supports `CREATE EXCEPTION <name> <message>` only.
    - `parseCreateException` requires message token (string or identifier); no parameter list or WITH clauses.
[~] AST supports only `exception_path` and `message`.
[~] Emitter sends `name` + `message` to `SBLR3_CREATE_EXCEPTION_STMT`.
[~] Executor creates exception with message and supports OR REPLACE.

### Spec Features Missing in V3 Path
[ ] Parameter list for exceptions.
[ ] WITH MESSAGE TEMPLATE.
[ ] WITH MESSAGE BUILDER.
[ ] WITH OPTIONS (SEVERITY/SQLSTATE/HINT/DETAIL).
[ ] Explicit “ALTER EXCEPTION not supported” behavior (parser currently supports only CREATE/DROP; no explicit error for ALTER).

### DROP EXCEPTION
[*] Implemented with IF EXISTS and catalog drop in executor.

## Notes
- If advanced exception features are required in V3, AST/emitter/executor and catalog schema must be extended.
