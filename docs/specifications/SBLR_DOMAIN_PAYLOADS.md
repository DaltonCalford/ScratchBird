# SBLR Domain Payloads (v2)

This document defines the current SBLR payloads for domain DDL opcodes.
Unless stated otherwise, all strings use the compact stream string encoding
defined in `docs/specifications/Appendix_A_SBLR_BYTECODE.md`.

**Status:** BASIC domain type only. Advanced domain kinds (RECORD/ENUM/SET/VARIANT)
will require payload extensions once those syntaxes are implemented.

---

## Common Conventions

- `string` = compact stream string encoding.
- `bool` = `uint8` (0 = false, 1 = true).
- `schema_path` strings use `schemaPathToString()` (dot-separated).
- Data type encoding uses the same opcode+payload format as column definitions:
  - `TYPE_VARCHAR` / `TYPE_CHAR` => `[type:uint8][length:uint32]`
  - `TYPE_DECIMAL` => `[type:uint8][precision:uint32][scale:uint32]`
  - other base types => `[type:uint8]`

---

## EXT_CREATE_DOMAIN (0x005C)

Payload layout:
```
[flags:uint8]
[domain_path:string]
[base_type:TYPE_* encoding]
[nullable:uint8]                    // 1 = nullable, 0 = NOT NULL
[default_value:string]              // raw expression text or empty string
[constraint_count:uint32]
repeat constraint_count:
  [constraint_type:uint8]           // 0=NOT_NULL,1=NULL_ALLOWED,2=DEFAULT,3=CHECK
  [constraint_name:string]          // empty if unnamed
  [constraint_expression:string]

if flags has INTEGRITY:
  [uniqueness:uint8]
  [normalization_enabled:uint8]
  [normalization_function:string]

if flags has SECURITY:
  [security_flags:uint8]
  [masking:string]                  // if security_flags & 0x01
  [mask_pattern:string]             // if security_flags & 0x02
  [encryption:string]               // if security_flags & 0x04
  [audit_access:uint8]              // if security_flags & 0x08
  [required_privilege:string]       // if security_flags & 0x10

if flags has VALIDATION:
  [validation_flags:uint8]
  [validation_function:string]      // if validation_flags & 0x01
  [error_message:string]            // if validation_flags & 0x02

if flags has QUALITY:
  [quality_flags:uint8]
  [parse_function:string]           // if quality_flags & 0x01
  [standardize_function:string]     // if quality_flags & 0x02
  [enrich_function:string]          // if quality_flags & 0x04
```

Flags:
- `0x01` IF NOT EXISTS
- `0x02` WITH INTEGRITY
- `0x04` WITH SECURITY
- `0x08` WITH VALIDATION
- `0x10` WITH QUALITY

Security flags:
- `0x01` MASKING present
- `0x02` MASK_PATTERN present
- `0x04` ENCRYPTION present
- `0x08` AUDIT_ACCESS present
- `0x10` REQUIRE_PRIVILEGE present

Validation flags:
- `0x01` FUNCTION present
- `0x02` ERROR_MESSAGE present

Quality flags:
- `0x01` PARSE_FUNCTION present
- `0x02` STANDARDIZE_FUNCTION present
- `0x04` ENRICH_FUNCTION present

Example (BASIC domain with integrity):
```
flags = 0x02
domain_path = "email"
base_type = TYPE_TEXT
nullable = 1
default_value = ""
constraint_count = 0
integrity = { uniqueness=1, normalization_enabled=0, normalization_function="" }
```

---

## EXT_ALTER_DOMAIN (0x010E)

Payload layout:
```
[action:uint8]
[domain_path:string]
[action_payload]
```

Action values:
- `0` SET_DEFAULT
- `1` DROP_DEFAULT
- `2` ADD_CHECK
- `3` DROP_CONSTRAINT
- `4` RENAME
- `5` SET_COMPAT
- `6` DROP_COMPAT

Action payloads:
- `SET_DEFAULT` => `[default_value:string]`
- `ADD_CHECK` => `[check_expression:string]`
- `SET_COMPAT` => `[compat_name:string]`
- `DROP_CONSTRAINT` => `[constraint_name:string]`
- `RENAME` => `[new_name:string]`
- `DROP_DEFAULT` / `DROP_COMPAT` => no payload

Example (SET DEFAULT):
```
action = 0
domain_path = "email"
default_value = "''"
```

---

## EXT_DROP_DOMAIN (0x010F)

Payload layout:
```
[flags:uint8]
[domain_path:string]
```

Flags:
- `0x01` IF EXISTS
- `0x02` RESTRICT

The generator emits one EXT_DROP_DOMAIN per domain name when multiple
domains are listed in a single statement.

Example:
```
flags = 0x03  // IF EXISTS + RESTRICT
domain_path = "email"
```

