# SBLR Type/Opcode Coverage Gaps (V3)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


Date: 2026-02-07

This audit verifies that every DataType in the V3 type system has:
1) A V3 **type marker** opcode, and
2) A **literal encoding** path (either a dedicated literal opcode or an explicit, normative construction rule).

## Sources Checked

- `/docs/specifications/parser/v3/types/SBLR_TYPE_MAP.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `/docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`
- `/docs/specifications/parser/v3/types/README.md`
- `/docs/specifications/parser/v3/types/03_TYPES_AND_DOMAINS.md`

## Summary

- **Type marker coverage:** **Complete** for all types listed in `SBLR_TYPE_MAP.md`.
- **Literal coverage:** **Closed** in V3. Missing literal opcodes were added to the `SBLR_V3_OPCODE_SPEC.md` and payloads defined in `SBLR_V3_OPCODE_PAYLOADS.md`.

## Type Marker Coverage (Complete)

All of the following DataTypes have V3 type markers in `SBLR_V3_OPCODE_SPEC.md` and are mapped in `SBLR_TYPE_MAP.md`:

- Integer family: INT8, INT16, INT32, INT64, INT128, UINT8, UINT16, UINT32, UINT64, UINT128, MEDIUMINT
- Floating/decimal: FLOAT32, FLOAT64, DECIMAL/NUMERIC, MONEY, DECFLOAT16, DECFLOAT34
- Text/binary: CHAR, VARCHAR, TEXT, BINARY, VARBINARY, BYTEA, BLOB, BLOB_TEXT
- Temporal: DATE, TIME, TIMESTAMP, TIME_TZ, TIMESTAMP_TZ, INTERVAL, DATETIME, YEAR
- Boolean/bit: BOOLEAN, BIT
- Structured/user: ARRAY, COMPOSITE, ROW, DOMAIN, ENUM, SET, VARIANT
- JSON/XML: JSON, JSONB, JSONPATH, XML
- UUID/network: UUID, INET, CIDR, MACADDR, MACADDR8
- Spatial: GEOMETRY, GEOMETRYCOLLECTION, POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON
- Ranges: INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE, DATERANGE
- Full-text: TSVECTOR, TSQUERY
- NULL_TYPE

## Literal Coverage Map (Current)

Legend:
- **Dedicated literal opcode:** explicit `SBLR3_LITERAL_*` opcode exists.
- **Constructed literal:** no dedicated literal opcode; must be constructed via specific opcodes and rules.

### Types With Dedicated Literal Opcodes

- BOOLEAN → `SBLR3_LITERAL_BOOLEAN`
- BIT → `SBLR3_LITERAL_BIT`
- INT32 → `SBLR3_LITERAL_INT32`
- INT64 → `SBLR3_LITERAL_INT64`
- MEDIUMINT → `SBLR3_LITERAL_MEDIUMINT`
- DATE → `SBLR3_LITERAL_DATE`
- TIME → `SBLR3_LITERAL_TIME`
- TIMESTAMP → `SBLR3_LITERAL_TIMESTAMP`
- DATETIME → `SBLR3_LITERAL_DATETIME`
- YEAR → `SBLR3_LITERAL_YEAR`
- UUID → `SBLR3_LITERAL_UUID`
- DECIMAL/NUMERIC → `SBLR3_LITERAL_DECIMAL`
- DOUBLE (FLOAT64) → `SBLR3_LITERAL_DOUBLE`
- MONEY → `SBLR3_LITERAL_MONEY`
- JSON → `SBLR3_LITERAL_JSON`
- JSONB → `SBLR3_LITERAL_JSONB`
- JSONPATH → `SBLR3_LITERAL_JSONPATH`
- XML → `SBLR3_LITERAL_XML`
- STRING/TEXT/CHAR/VARCHAR → `SBLR3_LITERAL_STRING`
- BINARY/BYTEA/VARBINARY → `SBLR3_LITERAL_BINARY`
- INET → `SBLR3_LITERAL_INET`
- CIDR → `SBLR3_LITERAL_CIDR`
- MACADDR → `SBLR3_LITERAL_MACADDR`
- MACADDR8 → `SBLR3_LITERAL_MACADDR8`
- INTERVAL → `SBLR3_LITERAL_INTERVAL`
- GEOMETRY (generic) → `SBLR3_LITERAL_GEOMETRY`
- ENUM → `SBLR3_LITERAL_ENUM`
- SET → `SBLR3_LITERAL_SET`
- ROW → `SBLR3_LITERAL_ROW`
- COMPOSITE → `SBLR3_LITERAL_COMPOSITE`
- DOMAIN → `SBLR3_LITERAL_DOMAIN`
- NULL → `SBLR3_LITERAL_NULL`

### Closed Literal Gaps (Added Opcodes)

The following literal opcodes and payloads were added to close coverage:
- `SBLR3_LITERAL_INT8`, `SBLR3_LITERAL_INT16`
- `SBLR3_LITERAL_UINT8`, `SBLR3_LITERAL_UINT16`, `SBLR3_LITERAL_UINT32`, `SBLR3_LITERAL_UINT64`, `SBLR3_LITERAL_UINT128`
- `SBLR3_LITERAL_INT128`
- `SBLR3_LITERAL_FLOAT32`
- `SBLR3_LITERAL_TIME_TZ`, `SBLR3_LITERAL_TIMESTAMP_TZ`
- `SBLR3_LITERAL_RANGE`
- `SBLR3_LITERAL_ARRAY`
- `SBLR3_LITERAL_VARIANT`
- `SBLR3_LITERAL_TSVECTOR`, `SBLR3_LITERAL_TSQUERY`
- `SBLR3_LITERAL_BLOB_LOCATOR`

## Required Actions (Follow-Up)

1. **Update `AST_TYPE_AND_LITERAL_SPEC.md`** with any newly added literal forms if not already specified.
2. **Update `SBLR_V3_VALIDATION_RULES.md`** to include range checks for new numeric literals and range literal validation rules.

## Notes

- This audit only checks **type marker coverage** and **literal encoding coverage**. It does not verify opcode semantics or executor implementations.
- Until gaps are closed, any AI implementation must treat the types above as **spec-incomplete** for literal handling.
