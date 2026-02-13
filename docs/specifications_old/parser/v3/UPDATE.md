# V3 Parser: UPDATE (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative UPDATE statement forms for the ScratchBird V3 parser and
how each form MUST emit SBLR V3 opcodes.

## Parsing Rules (Authoritative)

1. Enter DML parse mode.
2. Parse target table reference and optional alias.
3. Parse SET assignment list.
4. Parse optional FROM clause (if dialect permits).
5. Parse optional WHERE clause.
6. Parse optional RETURNING clause.
7. Emit `SBLR3_DML_UPDATE` with typed payload and child expressions.

## Emission Rules (Authoritative)

Emit:
- `SBLR3_DML_UPDATE` with payload `DML_UPDATE`
- Child nodes:
  - `TABLE_REF` (target)
  - `ASSIGNMENT` list (SET)
  - `TABLE_REF` list (FROM, if present)
  - `EXPR` (WHERE)
  - `RETURNING` list

## Statement Forms

```
UPDATE <table_ref> [AS alias]
SET <assignment_list>
[FROM <table_ref_list>]
[WHERE <expr>]
[RETURNING <expr_list>]
```

## Errors

- Missing assignment list: `ERR_PARSE_EXPECTED_ASSIGNMENT`.
- RETURNING on unsupported dialect: `ERR_FEATURE_NOT_SUPPORTED`.
- SET with unknown column: `ERR_COLUMN_NOT_FOUND`.

## Related Specs

- `SELECT_AND_QUERY.md`
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
