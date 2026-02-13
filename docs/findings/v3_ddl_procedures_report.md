# V3 DDL Procedures Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_PROCEDURES.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 implements a reduced CREATE PROCEDURE: params + SQL SECURITY + body; no LANGUAGE or SET options.
- ALTER PROCEDURE per spec not implemented in parser/emitter; DROP PROCEDURE resolves by name only (no signature, no CASCADE/RESTRICT).

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE PROCEDURE
[~] Parser supports params (IN/OUT/INOUT), SQL SECURITY, body.
    - No LANGUAGE keyword or SET configuration parameters.
[~] AST fields: params, sql_security, body.
[~] Emitter writes params/body; language hardcoded to "SQL".
[~] Executor registers procedure with sql_security; no language/config metadata.

### ALTER PROCEDURE
[ ] Spec ALTER PROCEDURE not parsed (only generic RENAME/MOVE via ALTER RENAME/SET SCHEMA).
[~] Executor supports ALTER PROCEDURE flags for sql_security/comment, but no V3 DDL wiring.

### DROP PROCEDURE
[~] Parser supports DROP PROCEDURE with IF EXISTS; no signature enforcement.
[~] Executor drops by name only; no CASCADE/RESTRICT semantics.

### Spec Features Missing
[ ] LANGUAGE support.
[ ] SECURITY DEFINER/INVOKER syntax (parser uses SQL SECURITY only).
[ ] SET configuration parameters.
[ ] Signature-based resolution for overloads.

## Notes
- If overloads are required, DROP/ALTER should resolve by signature, not name only.
