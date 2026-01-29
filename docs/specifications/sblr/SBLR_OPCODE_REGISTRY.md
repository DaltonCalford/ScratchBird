# SBLR Opcode Registry

## Authority and Scope
This document defines the opcode registry policy and encoding rules for SBLR.
If there is any mismatch between this document and `include/scratchbird/sblr/opcodes.h`,
the header file is authoritative.

## Encoding Rules (Normative)
- Base opcodes are 1 byte.
- END = 0x00
- VERSION = 0x01
- EXTENDED_PREFIX = 0xFF
- Extended opcodes are encoded as:
  ```
  0xFF <ext_opcode_lo> <ext_opcode_hi> [payload...]
  ```
  where the extended opcode is a 16-bit little-endian value.
- Unknown opcodes MUST be rejected by the executor.

## Range Grouping Policy (Normative)
Opcode values should be grouped to keep the codebase readable and debuggable.
The following ranges are reserved as a guideline for future allocations:
- 0x00-0x0F: stream and VM control
- 0x10-0x1F: DDL and transaction control
- 0x20-0x2F: type markers
- 0x30-0x3F: literals and constants
- 0x40-0x4F: references and assignments
- 0x50-0x5F: arithmetic
- 0x60-0x6F: comparisons
- 0x70-0x7F: boolean logic
- 0x80-0x8F: built-in functions
- 0x90-0x9F: constraints and modifiers
- 0xA0-0xAF: query structure
- 0xB0-0xBF: extended data types
- 0xC0-0xCF: optimizer hints and joins
- 0xD0-0xDF: sorting, limits, windows
- 0xE0-0xEF: stack and control flow
- 0xF0-0xFE: reserved for base opcodes
- 0xFF: extended prefix

Existing assignments that do not fit these ranges remain valid; new assignments should conform.

## Reserved Extended Opcodes (Normative)
These extended opcodes are reserved and MUST keep their values:
- EXT_RENAME_OBJECT       = 0x0100
- EXT_MOVE_OBJECT         = 0x0101
- EXT_SET_AUTOCOMMIT      = 0x0102
- EXT_COMMIT_RETAINING    = 0x0103
- EXT_ROLLBACK_RETAINING  = 0x0104
- EXT_PREPARE_TRANSACTION = 0x0105
- EXT_COMMIT_PREPARED     = 0x0106
- EXT_ROLLBACK_PREPARED   = 0x0107
- EXT_ALTER_DOMAIN        = 0x010E
- EXT_DROP_DOMAIN         = 0x010F
- EXT_NULL_SAFE_EQ        = 0x0200
- EXT_LIKE_ESCAPE         = 0x0201
- EXT_ILIKE_ESCAPE        = 0x0202
- EXT_PLACEHOLDER         = 0x0203
- EXT_CHECK_DOMAIN_CONSTRAINT = 0x0204
- EXT_APPLY_DOMAIN_MASKING    = 0x0205
- EXT_ENCRYPT_DOMAIN_VALUE    = 0x0206
- EXT_DECRYPT_DOMAIN_VALUE    = 0x0207
- EXT_AUDIT_DOMAIN_ACCESS     = 0x0208
- EXT_CHECK_DOMAIN_PRIVILEGE  = 0x0209
- EXT_NORMALIZE_DOMAIN_VALUE  = 0x020A
- EXT_VALIDATE_DOMAIN_VALUE   = 0x020B
- EXT_APPLY_QUALITY_PIPELINE  = 0x020C
- EXT_CHECK_GLOBAL_UNIQUENESS = 0x020D
- EXT_EXPR_NOT            = 0x0210
- EXT_EXPR_IS_NULL        = 0x0211
- EXT_SAVEPOINT_BEGIN     = 0x0212
- EXT_SAVEPOINT_END       = 0x0213

## Type Marker Coverage (Current)
SBLR type markers are split between base opcodes and extended opcodes. The
header file remains authoritative for numeric values.

### Base Type Markers
| Opcode | Marker | DataType |
| --- | --- | --- |
| 0x20 | TYPE_INTEGER | INT32 |
| 0x21 | TYPE_BIGINT | INT64 |
| 0x22 | TYPE_DOUBLE | FLOAT64 |
| 0x23 | TYPE_VARCHAR | VARCHAR |
| 0x24 | TYPE_BOOLEAN | BOOLEAN |
| 0x25 | TYPE_INT8 | INT8 |
| 0x26 | TYPE_INT16 | INT16 |
| 0x27 | TYPE_FLOAT32 | FLOAT32 |
| 0x28 | TYPE_DATE | DATE |
| 0x29 | TYPE_TIME | TIME |
| 0x2A | TYPE_TIMESTAMP | TIMESTAMP |
| 0x2B | TYPE_UUID | UUID |
| 0x2C | TYPE_DECIMAL | DECIMAL/NUMERIC |
| 0x2D | TYPE_CHAR | CHAR |
| 0x2E | TYPE_TEXT | TEXT |
| 0x2F | TYPE_BINARY | BINARY |
| 0xB0 | TYPE_VARBINARY | VARBINARY |
| 0xB1 | TYPE_BLOB | BLOB |
| 0xB2 | TYPE_BYTEA | BYTEA |
| 0xB3 | TYPE_JSON | JSON |
| 0xB8 | TYPE_DOMAIN | DOMAIN (domain_id payload) |
| 0xB9 | TYPE_ARRAY | ARRAY (element type + size) |

### Extended Type Markers
| Extended Opcode | Marker | DataType |
| --- | --- | --- |
| 0x0400 | EXT_TYPE_INT128 | INT128 |
| 0x0401 | EXT_TYPE_UINT128 | UINT128 |
| 0x0402 | EXT_TYPE_VECTOR | VECTOR |
| 0x0050 | EXT_TYPE_POINT | POINT |
| 0x0051 | EXT_TYPE_LINESTRING | LINESTRING |
| 0x0052 | EXT_TYPE_POLYGON | POLYGON |
| 0x00AB | EXT_TYPE_TSVECTOR | TSVECTOR |
| 0x00AC | EXT_TYPE_TSQUERY | TSQUERY |
| 0x00B1 | EXT_TYPE_INT4RANGE | INT4RANGE |
| 0x00B2 | EXT_TYPE_INT8RANGE | INT8RANGE |
| 0x00B3 | EXT_TYPE_NUMRANGE | NUMRANGE |
| 0x00B4 | EXT_TYPE_DATERANGE | DATERANGE |
| 0x00B5 | EXT_TYPE_TSRANGE | TSRANGE |
| 0x00B6 | EXT_TYPE_TSTZRANGE | TSTZRANGE |

## Type Marker Coverage
All DataTypes defined in `core/types.h` now have assigned SBLR type markers,
including emulated-engine parity additions such as `DECFLOAT(16/34)`.

## Literal Opcode Coverage
Typed literal opcodes are now assigned for boolean/date/time/uuid/binary/decimal
and JSON/XML/network families. See the base and extended literal opcode tables
below for the canonical assignments.

## Opcode Assignments (Canonical)
The assignments below are authoritative and match `include/scratchbird/sblr/opcodes.h`.

### Extended Type Markers
| Extended Opcode | Marker | DataType |
| --- | --- | --- |
| 0x0410 | EXT_TYPE_UINT8 | UINT8 |
| 0x0411 | EXT_TYPE_UINT16 | UINT16 |
| 0x0412 | EXT_TYPE_UINT32 | UINT32 |
| 0x0413 | EXT_TYPE_UINT64 | UINT64 |
| 0x0414 | EXT_TYPE_MONEY | MONEY |
| 0x0415 | EXT_TYPE_INTERVAL | INTERVAL |
| 0x0416 | EXT_TYPE_JSONB | JSONB |
| 0x0417 | EXT_TYPE_XML | XML |
| 0x0418 | EXT_TYPE_MULTIPOINT | MULTIPOINT |
| 0x0419 | EXT_TYPE_MULTILINESTRING | MULTILINESTRING |
| 0x041A | EXT_TYPE_MULTIPOLYGON | MULTIPOLYGON |
| 0x041B | EXT_TYPE_GEOMETRYCOLLECTION | GEOMETRYCOLLECTION |
| 0x041C | EXT_TYPE_COMPOSITE | COMPOSITE |
| 0x041D | EXT_TYPE_VARIANT | VARIANT |
| 0x041E | EXT_TYPE_INET | INET |
| 0x041F | EXT_TYPE_CIDR | CIDR |
| 0x0420 | EXT_TYPE_MACADDR | MACADDR |
| 0x0421 | EXT_TYPE_MACADDR8 | MACADDR8 |
| 0x0422 | EXT_TYPE_TIME_TZ | TIME WITH TIME ZONE |
| 0x0423 | EXT_TYPE_TIMESTAMP_TZ | TIMESTAMP WITH TIME ZONE |
| 0x0424 | EXT_TYPE_DECFLOAT16 | DECFLOAT(16) |
| 0x0425 | EXT_TYPE_DECFLOAT34 | DECFLOAT(34) |

### Base Literal Opcodes (0x37-0x3F)
| Opcode | Marker | Payload |
| --- | --- | --- |
| 0x37 | LITERAL_BOOLEAN | uint8 (0 or 1) |
| 0x38 | LITERAL_UUID | 16 bytes RFC 4122 |
| 0x39 | LITERAL_DATE | int32 (MJD days) |
| 0x3A | LITERAL_TIME | int64 (microseconds since midnight) |
| 0x3B | LITERAL_TIMESTAMP | int64 (microseconds since epoch) |
| 0x3C | LITERAL_BINARY | [len:UVARINT][bytes] |
| 0x3D | LITERAL_DECIMAL | compact string |
| 0x3E | LITERAL_JSON | compact string |
| 0x3F | LITERAL_XML | compact string |

### Extended Literal Opcodes
| Extended Opcode | Marker | Payload |
| --- | --- | --- |
| 0x0600 | EXT_LITERAL_JSONB | compact string |
| 0x0601 | EXT_LITERAL_INTERVAL | compact string |
| 0x0602 | EXT_LITERAL_MONEY | compact string |
| 0x0603 | EXT_LITERAL_INET | compact string |
| 0x0604 | EXT_LITERAL_CIDR | compact string |
| 0x0605 | EXT_LITERAL_MACADDR | compact string |
| 0x0606 | EXT_LITERAL_MACADDR8 | compact string |

Notes:
- TIME/TIMESTAMP literals may include optional timezone offsets in the parser.
  The type marker (TIME_TZ/TIMESTAMP_TZ) determines whether offset is retained.
- Decimal and money literals use compact string payloads to preserve precision
  before casting to the target type.

## Planned Alpha Additions (Reserved, Pending Header Update)
These values are reserved for the Alpha parity additions below. They are not yet in
`include/scratchbird/sblr/opcodes.h` and MUST be added there before implementation.

Operators and predicates:
- EXT_EXPR_DIV_INT         = 0x0215  (DIV integer division operator)
- EXT_PRED_STARTING_WITH   = 0x0216  (STARTING WITH predicate)
- EXT_PRED_CONTAINING      = 0x0217  (CONTAINING predicate)

Functions:
- EXT_FUNC_REPLACE         = 0x0320  (REPLACE(str, search, replacement))
- EXT_FUNC_ENDS_WITH       = 0x0321  (ENDS_WITH(str, suffix))
- EXT_FUNC_ARRAY_POSITION  = 0x0322  (ARRAY_POSITION(array, value))
- EXT_ARRAY_SLICE          = 0x0323  (ARRAY_SLICE(array, lower, upper))
- EXT_FUNC_JSON_EXISTS     = 0x0324  (JSON_EXISTS(json, path))
- EXT_FUNC_JSON_HAS_KEY    = 0x0325  (JSON_HAS_KEY(json, key))
- EXT_FUNC_TO_CHAR         = 0x0326  (TO_CHAR(value, format))
- EXT_FUNC_TO_DATE         = 0x0327  (TO_DATE(text, format))
- EXT_FUNC_TO_TIMESTAMP    = 0x0328  (TO_TIMESTAMP(text, format))
- EXT_FUNC_LEAST           = 0x0329  (LEAST(a, b, ...))
- EXT_FUNC_GREATEST        = 0x032A  (GREATEST(a, b, ...))

## Array Extended Opcodes (Low Range)
- EXT_ARRAY_SUBSCRIPT = 0x0025

## Change Process (Policy)
- Add or modify opcodes in `include/scratchbird/sblr/opcodes.h` first.
- Update this registry document to reflect the change.
- Do not reuse retired opcode values.
- Keep opcode comments in the header short and unambiguous.

## Reference
- SBLR bytecode format and transaction payloads: `docs/specifications/Appendix_A_SBLR_BYTECODE.md`
