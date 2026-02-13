# V3 DDL Functions Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_FUNCTIONS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 implements a subset: CREATE FUNCTION with params, RETURNS, optional DETERMINISTIC and SQL SECURITY; function body captured as text.
- Spec features like LANGUAGE, volatility classes, parallel, cost/rows, RETURNS SETOF/TABLE, RETURNS CURSOR, and ALTER FUNCTION variants are not implemented in V3 parser/emitter/executor.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE FUNCTION
[~] Parser supports params (IN/OUT/INOUT), RETURNS type, DETERMINISTIC, SQL SECURITY, body.
    - No LANGUAGE keyword, volatility (IMMUTABLE/STABLE/VOLATILE), PARALLEL, COST, ROWS.
    - Returns type parsed as TypeName only (no SETOF/TABLE/CURSOR semantics).
[~] AST fields: `deterministic`, `sql_security`, params, return_type, body.
[~] Emitter writes params, return_type, language hardcoded to "SQL", body bytes.
[~] Executor registers function with deterministic + sql_security; no language/volatility/parallel metadata.

### ALTER FUNCTION
[ ] Parser-level ALTER FUNCTION per spec not implemented (only generic RENAME/MOVE via ALTER + RENAME/SET SCHEMA).
[~] Executor supports ALTER FUNCTION flags for sql_security/deterministic/comment but no parser emitter wiring for those flags in V3 DDL.

### DROP FUNCTION
[~] Parser supports DROP FUNCTION; does not enforce signature (parameter types).
[~] Executor drops by name (resolved object), not by signature, and does not handle CASCADE/RESTRICT semantics.

### Spec Features Missing
[ ] LANGUAGE selection (sql/plpgsql/python/etc).
[ ] Volatility classes (IMMUTABLE/STABLE/VOLATILE).
[ ] SECURITY DEFINER/INVOKER syntax (parser uses SQL SECURITY only).
[ ] PARALLEL safety.
[ ] COST/ROWS hints.
[ ] RETURNS SETOF / RETURNS TABLE / RETURNS CURSOR semantics.
[ ] CREATE FUNCTION options for sys.remote_query style declarations.

## Notes
- If function overloading is required, DROP/ALTER should resolve by signature, not name only.
