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

## Change Process (Policy)
- Add or modify opcodes in `include/scratchbird/sblr/opcodes.h` first.
- Update this registry document to reflect the change.
- Do not reuse retired opcode values.
- Keep opcode comments in the header short and unambiguous.

## Reference
- SBLR bytecode format and transaction payloads: `docs/specifications/Appendix_A_SBLR_BYTECODE.md`
