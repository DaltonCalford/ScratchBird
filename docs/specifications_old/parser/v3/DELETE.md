# V3 Parser: DELETE (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative DELETE statement forms for the ScratchBird V3 parser and
how each form MUST emit SBLR V3 opcodes.

## Parsing Rules (Authoritative)

1. Enter DML parse mode.
2. Parse target table reference and optional alias.
3. Parse USING clause (if present).
4. Parse WHERE clause (if present).
5. Parse RETURNING clause (if present).
6. Emit `SBLR3_DML_DELETE` with typed payload and child expressions.

## Emission Rules (Authoritative)

Emit:
- `SBLR3_DML_DELETE` with payload `DML_DELETE`
- Child nodes:
  - `TABLE_REF` (target)
  - `TABLE_REF` list (USING)
  - `EXPR` (WHERE)
  - `RETURNING` list

## Statement Forms

```
DELETE FROM <table_ref>
  [USING <table_ref_list>]
  [WHERE <expr>]
  [RETURNING <expr_list>]
```

## Errors

- Missing target table: `ERR_PARSE_EXPECTED_TABLE`.
- RETURNING on unsupported dialect: `ERR_FEATURE_NOT_SUPPORTED`.
- Attempt to delete from system table without privilege: `ERR_PERMISSION_DENIED`.

## Related Specs

- `SELECT_AND_QUERY.md`
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
