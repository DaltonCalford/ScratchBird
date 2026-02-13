# Appendix A: SBLR Bytecode Specification

## 1. Scope and Principles
SBLR (ScratchBird Bytecode Language Representation) is the internal bytecode executed by the ScratchBird VM.

Key principles:
- SBLR does not require BLR compatibility. Legacy BLR can be recovered by an external BLR to SQL utility and recompiled to SBLR.
- Stored programmable objects compile at CREATE/ALTER time. Compilation errors fail the DDL.
- Two compilation modes exist:
  - Transportable: SBLR bytecode is stored and executed by the VM.
  - Non-transportable: only native code is stored; SQL/SBLR bodies are not retained.
- If a SHOW command requests a body that is not retained or has been removed, return "Redacted" for the body while input/output signatures and comments remain visible if stored.

## 2. Formats and Selection
SBLR v2 defines two formats:

1) Compact Stream (SBLR stream)
   - Used for transient or dynamic statements.
2) Module Container (SBLR module)
   - Used for stored objects and tooling.
   - Wraps a compact stream as its code section.

The code section MUST include a VERSION opcode and MUST terminate with END even when length is known. This guarantees a sentinel for context-sensitive parsing.

## 3. Common Encoding Conventions
- Byte order: little-endian for fixed-width integers.
- Fixed-width types: uint8, uint16, uint32, uint64, int32.
- UVARINT: unsigned LEB128 (7 bits per byte, MSB continuation).
- Compact stream strings: [len:UVARINT][bytes], UTF-8 NFC, no NUL terminator.
- Module strings: [len:uint32][bytes], UTF-8 NFC, no NUL terminator.
- Booleans: 0x00 false, 0x01 true.
- Compact stream lists: [count:UVARINT] followed by count elements.
- Module lists: [count:uint32] followed by count elements.
- Offsets in module headers are uint32 from the start of the header.

## 4. Compact Stream Format (SBLR Stream v2)
Format:
```
[VERSION opcode][sblr_version byte] ... [bytecode] ... [END opcode]
```

- VERSION opcode = 0x01
- END opcode = 0x00
- sblr_version must be 2
- Extended opcode prefix = 0xFF, followed by a 16-bit little-endian extended opcode.

Errors:
- Any stream with a version other than 2 MUST be rejected.
- Any EXTENDED prefix not followed by two bytes MUST be rejected.
- Missing END sentinel MUST be rejected.

### 4.1 Inline Literal Encoding (compact stream)
Unless an opcode defines a custom payload, inline literal encoding follows these rules:
- int32: 4 bytes, little-endian two's complement.
- int64: 8 bytes, little-endian two's complement.
- float32: IEEE 754, 4 bytes little-endian.
- float64: IEEE 754, 8 bytes little-endian.
- string: compact stream string encoding.
- uuid: 16 bytes, RFC 4122 byte order.
- binary: [len:UVARINT][bytes].

Literal opcodes include `NULL`, integer/floating forms, `STRING`, `CHARSET`,
`COLLATION`, plus typed literals for boolean/date/time/uuid/binary/decimal and
JSON/XML/network families. See `SBLR_OPCODE_REGISTRY.md` for the canonical
opcode list and payload encoding.

## 5. Module Container Format (SBLR Module v2)

The module is a header plus sections. Offsets are relative to the start of the header. Sections may be omitted if their size is zero.

```
typedef struct SBLR_Header_v2 {
    uint32_t magic;          // 0x53424C52 "SBLR"
    uint16_t version_major;  // 2
    uint16_t version_minor;  // 0
    uint32_t header_size;    // bytes including this header
    uint32_t flags;          // SBLR_MODULE_* flags
    uint32_t checksum;       // CRC32 of entire module with checksum set to 0
    uint64_t timestamp_utc;  // compile time (UTC)
    uint32_t source_hash;    // hash of SQL source if retained, else 0

    uint32_t code_offset;
    uint32_t code_size;
    uint32_t const_offset;
    uint32_t const_size;
    uint32_t type_offset;
    uint32_t type_size;
    uint32_t symbol_offset;
    uint32_t symbol_size;
    uint32_t debug_offset;
    uint32_t debug_size;
    uint32_t profile_offset;
    uint32_t profile_size;
    uint32_t exception_offset;
    uint32_t exception_size;
    uint32_t dependency_offset;
    uint32_t dependency_size;
    uint32_t native_offset;
    uint32_t native_size;

    uint32_t stack_size_req;
    uint32_t variable_slots;

    uint32_t main_entry_point;
    uint32_t init_entry_point;
    uint32_t cleanup_entry_point;

    uint32_t reserved[8];
} SBLR_Header_v2;
```

Flags (bitmask):
- SBLR_MODULE_HAS_CODE         0x0001
- SBLR_MODULE_HAS_NATIVE       0x0002
- SBLR_MODULE_NON_TRANSPORT    0x0004
- SBLR_MODULE_BODY_REDACTED    0x0008
- SBLR_MODULE_HAS_CONSTANTS    0x0010
- SBLR_MODULE_HAS_TYPES        0x0020
- SBLR_MODULE_HAS_SYMBOLS      0x0040
- SBLR_MODULE_HAS_DEBUG        0x0080
- SBLR_MODULE_HAS_PROFILE      0x0100
- SBLR_MODULE_HAS_EXCEPTIONS   0x0200
- SBLR_MODULE_HAS_DEPENDENCIES 0x0400

Requirements:
- Transportable objects MUST set HAS_CODE and include a valid compact stream in code section.
- Non-transportable objects MUST set HAS_NATIVE and NON_TRANSPORT, and MUST omit code section.
- Stored objects MUST include a dependency section.
- All code sections MUST include VERSION and END as in the compact stream format.

## 6. Module Sections
Note: Module sections use module encodings (string32 = [len:uint32][bytes], list count uint32).

### 6.1 Code Section
A complete compact stream (Section 4). Entry point offsets are relative to the start of the code section.

### 6.2 Constant Pool
Layout:
```
[count:uint32]
repeat count times:
  [type_id:uint32]
  [value: type-specific encoding]
```

Type-specific encoding uses module encodings (string and binary lengths are uint32). If a constant refers to a complex type, the value payload MUST encode the referenced type index or structured value as defined by the type descriptor.

### 6.3 Type Information
Type IDs are 32-bit values in module sections. When types are referenced in compact streams, they are encoded as UVARINT.

```
typedef struct SBLR_TypeInfo {
    uint32_t base_type;   // SBLR_TYPE_* id
    uint32_t size;        // size in bytes, 0 if variable
    uint16_t precision;   // for decimal
    uint16_t scale;       // for decimal
    uint16_t charset_id;  // for character types
    uint16_t collation_id;// for character types
    uint8_t  flags;       // bit0 nullable, bit1 variable_len, others reserved
} SBLR_TypeInfo;

Note: SBLR type markers and extended type markers are defined in
`SBLR_OPCODE_REGISTRY.md`. Types present in `core/types.h` that lack markers
are not currently encodable in SBLR.
```

Complex types are encoded as extended records in the type section:

Array type:
- base_type = SBLR_TYPE_ARRAY
- payload:
  [element_type_index:uint32]
  [dimensions:uint32]
  repeat dimensions:
    [lower_bound:int32]
    [upper_bound:int32]

Record type:
- base_type = SBLR_TYPE_RECORD
- payload:
  [field_count:uint32]
  repeat field_count:
    [field_name:string32]
    [field_type_index:uint32]

Domain type:
- base_type = SBLR_TYPE_DOMAIN
- payload:
  [domain_uuid:16 bytes]
  [base_type_index:uint32]

### 6.4 Symbol Table (optional)
Layout:
```
[count:uint32]
repeat count times:
  [name:string32]
  [kind:uint8]          // 0=var,1=param,2=routine,3=label,4=cursor
  [index:uint32]        // meaning depends on kind
```

### 6.5 Debug Info (optional)
Layout:
```
[count:uint32]
repeat count times:
  [pc_offset:uint32]
  [source_line:uint32]
  [source_column:uint32]
```

### 6.6 Profile Data (optional)
Implementation-defined. If present, profile data MUST be versioned and self-describing.

### 6.7 Exception Table (optional)
Layout:
```
[count:uint32]
repeat count times:
  [try_start:uint32]
  [try_end:uint32]
  [handler_pc:uint32]
  [stack_depth:uint32]
  [exception_code:uint32] // 0 means catch-all
```

### 6.8 Dependency Section (required for stored objects)
Layout:
```
[count:uint32]
repeat count times:
  [object_uuid:16 bytes]
  [version_kind:uint8]      // 0=none,1=u64,2=uuid,3=hash128
  [version_value]           // kind 1: uint64, kind 2: 16 bytes, kind 3: 16 bytes
```

The dependency list MUST include all referenced objects (tables, domains, routines, roles, etc).
The approval token and dependency hash MUST be computed from this list using the canonical rules in the security specification.

### 6.9 Native Code Section (non-transportable)
Native blob layout:
```
typedef struct SBLR_NativeHeader {
    uint32_t magic;          // 0x53424E56 "SBNV"
    uint16_t version_major;  // 1
    uint16_t version_minor;  // 0
    uint32_t header_size;
    uint32_t flags;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t metadata_offset;
    uint32_t metadata_size;
    uint32_t entry_table_offset;
    uint32_t entry_table_size;
    uint64_t engine_build_id;
    uint64_t engine_feature_mask;
} SBLR_NativeHeader;
```

Metadata is a TLV list (type:uint16, length:uint32, value:bytes) using big-endian for the TLV header.
Required metadata entries:
- 0x0001 TARGET_TRIPLE (string)
- 0x0002 ABI (string)
- 0x0003 CPU_FEATURES (byte string)
- 0x0004 POINTER_SIZE (uint8)
- 0x0005 ENDIANNESS (uint8) // 0=little,1=big

Entry table layout:
```
[count:uint32]
repeat count times:
  [name:string32]
  [offset:uint32]
```

At minimum a "main" entry is required. Entry offsets are relative to code_offset.

Native-only execution MUST hard-fail with a detailed error if any metadata checks fail, including ABI mismatch, missing CPU features, or engine feature incompatibility.

## 7. Opcode Encoding and Ranges

### 7.1 Base Opcodes
- END = 0x00
- VERSION = 0x01
- EXTENDED_PREFIX = 0xFF (must be followed by a 16-bit extended opcode, little-endian)

The opcode registry and allocation policy are defined in `docs/specifications/SBLR_OPCODE_REGISTRY.md`.
If there is a conflict between documentation and `include/scratchbird/sblr/opcodes.h`, the header is authoritative.

### 7.2 Range Grouping (logical)
Base opcode ranges are grouped for readability and debugging. The precise opcode registry is maintained by the engine, but ranges are reserved as follows:

- 0x00-0x0F: stream control and VM control
- 0x10-0x1F: DDL statements and transactional control
- 0x20-0x2F: type markers
- 0x30-0x3F: literal and constant opcodes
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

Any opcode not defined by the registry MUST be rejected by the executor.

### 7.3 Extended Opcodes (16-bit)
Encoding:
```
0xFF <ext_opcode_lo> <ext_opcode_hi> [payload...]
```

Defined extended opcodes:
- EXT_RENAME_OBJECT       = 0x0100
- EXT_MOVE_OBJECT         = 0x0101
- EXT_SET_AUTOCOMMIT      = 0x0102
- EXT_COMMIT_RETAINING    = 0x0103 (deprecated, use COMMIT with RETAINING flag)
- EXT_ROLLBACK_RETAINING  = 0x0104 (deprecated, use ROLLBACK with RETAINING flag)
- EXT_PREPARE_TRANSACTION = 0x0105
- EXT_COMMIT_PREPARED     = 0x0106
- EXT_ROLLBACK_PREPARED   = 0x0107
- EXT_DROP_FUNCTION_STMT  = 0x0033
- EXT_DROP_PROCEDURE_STMT = 0x0034
- EXT_DROP_PACKAGE_STMT   = 0x0035
- EXT_DROP_TRIGGER        = 0x0071
- EXT_DROP_ROLE           = 0x00CE
- EXT_ALTER_DOMAIN        = 0x010E
- EXT_DROP_DOMAIN         = 0x010F
- EXT_CREATE_EXCEPTION_STMT = 0x0318
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
- EXT_DROP_EXCEPTION_STMT = 0x031A

Domain DDL payloads are defined in `docs/specifications/SBLR_DOMAIN_PAYLOADS.md`.

### 7.4 Domain Enforcement Opcodes (v2)
Unless noted, `value_stack_offset` is counted from the top of the execution stack
(0 = top).

- EXT_CHECK_DOMAIN_CONSTRAINT (0x0204)
  ```
  [domain_id:16] [value_stack_offset:uint16]
  ```
  Result: Push bool (all constraints passed)

- EXT_APPLY_DOMAIN_MASKING (0x0205)
  ```
  [domain_id:16] [user_id:16] [value_stack_offset:uint16]
  ```
  Result: Replace stack value with masked value

- EXT_ENCRYPT_DOMAIN_VALUE (0x0206)
  ```
  [domain_id:16] [value_stack_offset:uint16]
  ```
  Result: Replace stack value with encrypted value

- EXT_DECRYPT_DOMAIN_VALUE (0x0207)
  ```
  [domain_id:16] [value_stack_offset:uint16]
  ```
  Result: Replace stack value with decrypted value

- EXT_AUDIT_DOMAIN_ACCESS (0x0208)
  ```
  [domain_id:16] [user_id:16] [table_id:16] [column_id:16]
  ```
  Result: Log audit event

- EXT_CHECK_DOMAIN_PRIVILEGE (0x0209)
  ```
  [domain_id:16] [user_id:16]
  ```
  Result: Push bool (has privilege)

- EXT_NORMALIZE_DOMAIN_VALUE (0x020A)
  ```
  [domain_id:16] [value_stack_offset:uint16]
  ```
  Result: Replace stack value with normalized value

- EXT_VALIDATE_DOMAIN_VALUE (0x020B)
  ```
  [domain_id:16] [value_stack_offset:uint16]
  ```
  Result: Push bool (validation passed)

- EXT_APPLY_QUALITY_PIPELINE (0x020C)
  ```
  [domain_id:16] [value_stack_offset:uint16]
  ```
  Result: Replace stack value with enriched value

- EXT_CHECK_GLOBAL_UNIQUENESS (0x020D)
  ```
  [domain_id:16] [table_id:16] [column_id:16] [row_id:16] [value_stack_offset:uint16]
  ```
  Result: Push bool (is unique)

### 7.5 DML Extended Opcodes (selected)

- EXT_COPY (see `include/scratchbird/sblr/opcodes.h` for value)
  ```
  [table_path:string]          // empty string for COPY (SELECT ...)
  [direction:uint8]            // 1=FROM, 2=TO
  [target:string]              // "STDIN"/"STDOUT" or file path
  [column_count:uint32]
  repeat column_count:
    [column_name:string]
  [format:uint8]               // 1=TEXT,2=CSV,3=BINARY
  [delimiter:string]           // single-character string
  [null_string:string]
  [header:uint8]               // 0/1
  [quote:string]               // single-character string
  [escape:string]              // single-character string
  [encoding:string]
  [batch_size:int32]
  [max_errors:int32]
  [on_error:uint8]             // 0=ABORT,1=SKIP
  if table_path == "":
    [query_len:UVARINT]
    [query_bytes:byte[query_len]]
  ```
  Notes:
  - `query_bytes` is a compact stream statement with no VERSION/END sentinel.
  - The embedded query must begin with SELECT.
  - COPY (SELECT ...) only supports TO and does not allow a column list.
  - `format`/`delimiter`/`null_string`/`header` are honored by the executor; BINARY uses
    length-prefixed row framing (compatible with SBWP DATA_ROW payloads).

### 7.6 Utility Extended Opcodes (selected)

- EXT_COMMENT (0x0094)
  ```
  [object_type:uint8]          // parser::v2::CommentObjectType
  [object_id:uuid]
  [is_null:uint8]              // 1 = remove comment, 0 = set comment
  [comment_text:string]
  ```

## 8. Transaction Opcodes (v2)

ScratchBird is always in a transaction. Transaction opcodes always end by starting a new transaction unless an explicit conflict action says otherwise.

### START_TRANSACTION (0x13)
```
[START_TRANSACTION]
[flags:uint16]
[conflict_action:uint8]               // 0=DEFAULT,1=COMMIT,2=ROLLBACK,3=ERROR,4=KEEP
[conflict_error_code:int32]           // only if flags has CONFLICT_ERROR_CODE
[autocommit_mode:uint8]               // 0=UNCHANGED,1=ON,2=OFF (only if flags has AUTOCOMMIT)
[isolation_level:uint8]               // only if flags has ISOLATION
[read_committed_mode:uint8]           // only if flags has READ_COMMITTED_MODE
[access_mode:uint8]                   // only if flags has ACCESS_MODE (0=RW,1=RO)
[deferrable:uint8]                    // only if flags has DEFERRABLE (0=NOT,1=YES)
[wait_mode:uint8]                     // only if flags has WAIT_MODE (0=NO WAIT,1=WAIT)
[lock_timeout:uint32]                 // only if flags has LOCK_TIMEOUT
[reservations:list]                   // only if flags has RESERVATIONS
```

### SET_TRANSACTION (0x17)
Identical payload to START_TRANSACTION. No in-place modification occurs.

### COMMIT (0x14)
```
[COMMIT]
[flags:uint8] // bit0=AND_CHAIN, bit1=AND_NO_CHAIN, bit2=RETAINING
```

### ROLLBACK (0x15)
```
[ROLLBACK]
[flags:uint8] // bit0=AND_CHAIN, bit1=AND_NO_CHAIN, bit2=RETAINING
```

### Extended Transaction Opcodes
- EXT_SET_AUTOCOMMIT (0x0102)
  ```
  [EXT_SET_AUTOCOMMIT]
  [mode:uint8]            // 0=OFF,1=ON
  [conflict_action:uint8]
  [conflict_error_code:int32]  // only if conflict_action == ERROR
  ```

- EXT_RELEASE_SAVEPOINT (0x0059)
  ```
  [EXT_RELEASE_SAVEPOINT]
  [savepoint_name:string]
  ```
- EXT_PREPARE_TRANSACTION (0x0105) -> [gid:string]
- EXT_COMMIT_PREPARED (0x0106) -> [gid:string]
- EXT_ROLLBACK_PREPARED (0x0107) -> [gid:string]

The gid string uses compact stream string encoding.

### Transaction Flags
- 0x0001 HAS_ISOLATION
- 0x0002 HAS_ACCESS_MODE
- 0x0004 HAS_DEFERRABLE
- 0x0008 HAS_WAIT_MODE
- 0x0010 HAS_LOCK_TIMEOUT
- 0x0020 HAS_RESERVATIONS
- 0x0040 HAS_AUTOCOMMIT
- 0x0080 HAS_CONFLICT_ERROR_CODE
- 0x0100 HAS_READ_COMMITTED_MODE

### Read Committed Mode Values
- 0 = DEFAULT
- 1 = READ CONSISTENCY
- 2 = RECORD VERSION
- 3 = NO RECORD VERSION

## 9. Type System (SBLR_TYPE_*)

SBLR type IDs are 32-bit values in module sections and UVARINT in compact streams. The current registry includes:
- SBLR_TYPE_SHORT (0x07)
- SBLR_TYPE_LONG (0x08)
- SBLR_TYPE_QUAD (0x09)
- SBLR_TYPE_FLOAT (0x0A)
- SBLR_TYPE_DOUBLE (0x1B)
- SBLR_TYPE_TEXT (0x0E)
- SBLR_TYPE_VARYING (0x25)
- SBLR_TYPE_TIMESTAMP (0x23)
- SBLR_TYPE_BLOB (0x105)
- SBLR_TYPE_SQL_DATE (0x0C)
- SBLR_TYPE_SQL_TIME (0x0D)
- SBLR_TYPE_BOOLEAN (0x17)
- SBLR_TYPE_INT8 (0x80)
- SBLR_TYPE_UINT8 (0x81)
- SBLR_TYPE_UINT16 (0x82)
- SBLR_TYPE_UINT32 (0x83)
- SBLR_TYPE_UINT64 (0x84)
- SBLR_TYPE_INT128 (0x85)
- SBLR_TYPE_DECIMAL (0x86)
- SBLR_TYPE_UTF8 (0x90)
- SBLR_TYPE_UUID (0x95)
- SBLR_TYPE_JSON (0xB5)
- SBLR_TYPE_JSONB (0xB6)
- SBLR_TYPE_XML (0xB7)
- SBLR_TYPE_ARRAY (0xB0)
- SBLR_TYPE_RECORD (0xB1)
- SBLR_TYPE_CURSOR (0xB3)
- SBLR_TYPE_DOMAIN (0xB8)
- SBLR_TYPE_VARIANT (0xB9)
- SBLR_TYPE_ANY (0xFF)

The registry may grow; executors MUST reject unknown types unless explicitly configured for forward compatibility.

## 10. Validation and Security Binding
The executor MUST validate SBLR before execution:
- Version must be 2.
- END sentinel must exist.
- All instruction operands must be within bounds.
- Extended opcode prefixes must have full 16-bit operands.
- Jump targets must land on instruction boundaries within the stream.
- Stack underflow or overflow is invalid.
- Constant and type indices must be in range.
- Dependency section must be present for stored objects.

The approval token and dependency hash MUST bind to:
- Declared dependency UUIDs and version stamps.
- The code hash (checksum) for transportable objects.
- The native blob hash and metadata for non-transportable objects.

## 11. Non-Transportable Policy
- Non-transportable compilation is a parser/compiler option.
- A system-wide policy may forbid non-transportable code; in that case the compiler MUST emit transportable SBLR even if requested otherwise.
- Non-transportable objects store only the native blob and metadata; SQL/SBLR bodies are not retained and must be shown as "Redacted".
