# V3 Parser: COPY (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define parsing and SBLR emission for COPY statements. The engine never parses
SQL; parsers emit SBLR only.

## Statement Forms

```
COPY (SELECT ...) TO { STDOUT | 'file' } [WITH (<options>) | <format>]
COPY <table> [(<columns>)] FROM { STDIN | 'file' } [WITH (<options>) | <format>]
COPY <table> [(<columns>)] TO { STDOUT | 'file' } [WITH (<options>) | <format>]
```

## Supported Options

- `FORMAT` = `CSV` | `TEXT` | `BINARY`
- `DELIMITER`
- `NULL`
- `HEADER`
- `QUOTE`
- `ESCAPE`
- `ENCODING`
- `BATCH_SIZE`
- `MAX_ERRORS`
- `ON_ERROR` = `ABORT` | `SKIP`

## Parsing Rules (Authoritative)

1. Detect SELECT form vs table form.
2. Parse direction (`FROM` or `TO`).
3. Parse source/target (`STDIN`/`STDOUT` or file literal).
4. Parse column list (table form only).
5. Parse options, normalizing option names to uppercase.
6. Emit `SBLR3_UTILITY_COPY` with payload `UTILITY_COPY`.

## Emission Rules

- Select form sets `source_kind = SELECT` and embeds the SELECT subquery.
- Table form sets `source_kind = TABLE` with table reference + columns.
- Options are emitted as `OPTION_KV` list with string or numeric literals.

## Errors

- Missing source/target: `ERR_COPY_MISSING_TARGET`.
- Invalid format: `ERR_COPY_INVALID_FORMAT`.
- Unsupported option: `ERR_COPY_UNSUPPORTED_OPTION`.

## Related Specs

- `SESSION_AND_UTILITY.md`
- `SBLR_V3_OPCODE_SPEC.md`
- `SBLR_V3_OPCODE_PAYLOADS.md`
- `PARSER_TO_SBLR_EMISSION_RULES.md`
