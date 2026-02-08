# V3 Parser: MERGE (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the authoritative MERGE statement forms for the ScratchBird V3 parser and
how each form MUST emit SBLR V3 opcodes.

## Parsing Rules (Authoritative)

1. Enter DML parse mode.
2. Parse target table and alias.
3. Parse source table or subquery and alias.
4. Parse join predicate (ON clause).
5. Parse WHEN MATCHED / WHEN NOT MATCHED actions in order.
6. Emit `SBLR3_DML_MERGE` with typed payload and ordered actions.

## Emission Rules (Authoritative)

Emit:
- `SBLR3_DML_MERGE` with payload `DML_MERGE`
- Child nodes:
  - `TABLE_REF` (target)
  - `TABLE_REF` or subquery (source)
  - `EXPR` (join predicate)
  - `MERGE_ACTION` list in order

## Statement Forms

```
MERGE INTO <target> [AS t]
USING <source> [AS s]
ON <expr>
WHEN MATCHED [AND <expr>] THEN
  { UPDATE SET <assignments> [WHERE <expr>] | DELETE [WHERE <expr>] }
WHEN NOT MATCHED [AND <expr>] THEN
  INSERT [(<column_list>)] VALUES (<expr_list>)
```

Multiple WHEN clauses are allowed; they are evaluated in order.

## Errors

- Missing ON predicate: `ERR_PARSE_EXPECTED_ON`.
- Missing WHEN clause: `ERR_PARSE_EXPECTED_WHEN`.
- Unsupported action in dialect: `ERR_FEATURE_NOT_SUPPORTED`.

## Related Specs

- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
