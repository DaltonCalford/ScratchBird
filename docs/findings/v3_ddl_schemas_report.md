# V3 DDL Schemas Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_SCHEMAS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 implements CREATE/ALTER/DROP SCHEMA with IF NOT EXISTS, AUTHORIZATION owner, RENAME/OWNER/SET PATH, and CASCADE/RESTRICT flags.
- The spec mentions PATH in CREATE; parser uses schema_path and AUTHORIZATION but does not parse an explicit PATH clause.

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE SCHEMA
[~] Parser supports IF NOT EXISTS, schema name, AUTHORIZATION owner.
[ ] CREATE SCHEMA PATH clause not parsed (no explicit PATH handling).
[~] Emitter sends path and owner.
[~] Executor creates schema path and records object.

### ALTER SCHEMA
[*] Parser supports RENAME, OWNER, SET PATH.
[*] Emitter + executor support these actions.

### DROP SCHEMA
[*] Parser supports IF EXISTS and CASCADE/RESTRICT.
[*] Executor uses cascade flag when dropping.

## Notes
- If PATH clause is required in CREATE SCHEMA, add parser/AST/emitter handling.
