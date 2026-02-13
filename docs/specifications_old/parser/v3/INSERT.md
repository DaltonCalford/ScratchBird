# V3 Parser: INSERT (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative INSERT statement forms for the ScratchBird V3 parser and
how each form MUST emit SBLR V3 opcodes.

## Parsing Rules (Authoritative)

1. Enter DML parse mode.
2. Parse target table reference and optional column list.
3. Parse VALUES, SELECT, or DEFAULT VALUES source.
4. Parse ON CONFLICT / UPSERT clause if present.
5. Parse RETURNING clause if present.
6. Emit `SBLR3_DML_INSERT` with typed payload and child expressions.

## Emission Rules (Authoritative)

Emit:
- `SBLR3_DML_INSERT` with payload `DML_INSERT`
- Child nodes:
  - `TABLE_REF` (target)
  - column list (if provided)
  - `VALUES` rows or `SELECT` subquery
  - `ON_CONFLICT_SPEC` (if present)
  - `RETURNING` list

## Statement Forms

```
INSERT INTO <table_ref> [(<column_list>)]
  VALUES (<expr_list>)[, ...]
  [ON CONFLICT <conflict_target> DO { NOTHING | UPDATE SET <assignments> [WHERE <expr>] }]
  [RETURNING <expr_list>]

INSERT INTO <table_ref> [(<column_list>)]
  SELECT <select_query>
  [ON CONFLICT ...]
  [RETURNING <expr_list>]

INSERT INTO <table_ref> DEFAULT VALUES
  [RETURNING <expr_list>]
```

## Errors

- Missing target table: `ERR_PARSE_EXPECTED_TABLE`.
- Column count mismatch: `ERR_COLUMN_COUNT_MISMATCH`.
- ON CONFLICT clause not supported in dialect: `ERR_FEATURE_NOT_SUPPORTED`.

## Related Specs

- `SELECT_AND_QUERY.md`
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
