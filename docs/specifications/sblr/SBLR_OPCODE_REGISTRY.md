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
