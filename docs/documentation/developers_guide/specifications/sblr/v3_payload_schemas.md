# Specification: SBLR v3 Payload Schemas

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird SBLR v3 |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_schema.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_payloads.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_types.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_payload_codec.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_schema.cpp:1`

## Synopsis

This specification defines the payload schemas for SBLR v3 instructions. Each opcode has an associated schema that describes the structure of its payload. Schemas use a type system of primitive and compound field types, supporting nested structures, lists, and optional fields.

## Scope

### In Scope

- Field type system definitions
- Schema definition format
- Schema registry lookup
- Payload encoding/decoding algorithms
- Expression and statement schema patterns

### Out of Scope

- Generated schema definitions (see generated schema files)
- Specific SQL statement payloads (see SQL-specific specs)
- Optimization hints in payloads

## Background

SBLR v3 uses a schema-driven approach to payload encoding. This provides:

1. **Type Safety**: Schemas validate payload structure
2. **Extensibility**: New fields can be added to schemas
3. **Efficiency**: Compact binary encoding
4. **Compatibility**: Schema versioning support

## Specification

### Data Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_schema.h:11
enum class FieldType : uint8_t {
    U8,          // Unsigned 8-bit integer
    U16,         // Unsigned 16-bit integer
    U32,         // Unsigned 32-bit integer
    U64,         // Unsigned 64-bit integer
    I8,          // Signed 8-bit integer
    I16,         // Signed 16-bit integer
    I32,         // Signed 32-bit integer
    I64,         // Signed 64-bit integer
    U128,        // 128-bit unsigned (16 bytes)
    UUID,        // 16-byte UUID
    F32,         // 32-bit float
    F64,         // 64-bit float
    BOOL,        // Boolean (1 byte)
    VARUINT,     // Variable-length unsigned (LEB128)
    STRING,      // Length-prefixed UTF-8 string
    IDENT,       // Identifier (string subtype)
    BYTES,       // Length-prefixed byte array
    SCHEMA_PATH, // Schema path (list of strings)
    TYPE_SPEC,   // Type specification (opcode + payload)
    EXPR,        // Expression (nested instruction)
    STMT,        // Statement (nested instruction)
    EXPR_LIST,   // List of expressions
    STMT_LIST,   // List of statements
    LIST,        // Generic list (element type in ref)
    OPT,         // Optional field (presence byte + value)
    SCHEMA       // Nested schema (name in ref)
};
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_schema.h:11-38`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_schema.h:40
struct FieldDef {
    std::string name;      // Field name
    FieldType type;        // Field type
    std::string ref;       // Subtype or schema reference
};

struct SchemaDef {
    std::string name;      // Schema name
    std::vector<FieldDef> fields;
};
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_schema.h:46-49`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_types.h:14
struct TypeSpec {
    uint16_t type_opcode = 0;
    std::vector<uint8_t> type_payload;
};

struct Value {
    using Bytes = std::vector<uint8_t>;
    using List = std::vector<Value>;
    using Object = std::map<std::string, Value>;
    using InstrPtr = std::shared_ptr<Instruction>;
    
    using Variant = std::variant<std::monostate, bool, int64_t, uint64_t, double,
                                 std::string, Bytes, List, Object, InstrPtr, TypeSpec>;
    Variant data;
    
    bool isNull() const { return std::holds_alternative<std::monostate>(data); }
};
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_types.h:14-44`

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_types.h:46
struct Instruction {
    uint16_t opcode = 0;
    uint16_t flags = 0;
    Value payload;  // Typically an Object keyed by field name
};
```

### Schema Registry

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_schema.h:51
const SchemaDef* lookupSchema(std::string_view name);
```

Schema lookup by name returns a pointer to the schema definition or nullptr if not found.

### Opcode-to-Schema Mapping

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_payloads.h:14
const SchemaDef* schemaForOpcode(uint16_t opcode);
```

Maps an opcode to its corresponding schema. Special cases:

| Opcode Pattern | Schema Resolution |
|----------------|-------------------|
| SBLR3_VERSION, SBLR3_END, SBLR3_EXTENDED_OPCODE | nullptr (no payload) |
| SBLR3_LITERAL_* | SCHEMA_LITERAL_<suffix> |
| SBLR3_FUNC_*, SBLR3_EXPR_FUNCTION_CALL | SCHEMA_FUNC_CALL |
| SBLR3_AGG_* | SCHEMA_AGG_CALL |
| SBLR3_WIN_* | SCHEMA_WINDOW_CALL |
| kExprUnary entries | SCHEMA_EXPR_UNARY |
| kExprBinary entries | SCHEMA_EXPR_BINARY |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:162-273`

### Field Type Encodings

| Field Type | Binary Encoding |
|------------|-----------------|
| U8 | [u8] |
| U16 | [u16] little-endian |
| U32 | [u32] little-endian |
| U64 | [u64] little-endian |
| I8 | [i8] signed byte |
| I16 | [i16] little-endian |
| I32 | [i32] little-endian |
| I64 | [i64] little-endian |
| U128 | [16 bytes] |
| UUID | [16 bytes] |
| F32 | [u32] IEEE 754 bits |
| F64 | [u64] IEEE 754 bits |
| BOOL | [u8] 0 or 1 |
| VARUINT | [varuint] LEB128 |
| STRING | [varuint] length + UTF-8 bytes |
| IDENT | [varuint] length + UTF-8 bytes |
| BYTES | [varuint] length + raw bytes |
| SCHEMA_PATH | [varuint] count + [string]... |
| TYPE_SPEC | [u16] opcode + [bytes] payload |
| EXPR | [instruction] nested |
| STMT | [instruction] nested |
| EXPR_LIST | [varuint] count + [instruction]... |
| STMT_LIST | [varuint] count + [instruction]... |
| LIST | [varuint] count + [element]... |
| OPT | [u8] present + [value] if present |
| SCHEMA | Per referenced schema |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:426-653`

### Expression Schemas

#### Unary Expression (SCHEMA_EXPR_UNARY)

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEMA_EXPR_UNARY                          │
├───────────────────────────────────────────────────────────────┤
│ [expr]    operand    // Nested expression instruction         │
└─────────────────────────────────────────────────────────────┘
```

Applies to: SBLR3_EXPR_NOT, SBLR3_BIT_NOT, SBLR3_EXPR_IS_NULL

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:119`

#### Binary Expression (SCHEMA_EXPR_BINARY)

```
┌─────────────────────────────────────────────────────────────┐
│                   SCHEMA_EXPR_BINARY                          │
├───────────────────────────────────────────────────────────────┤
│ [expr]    left       // Left operand                          │
│ [expr]    right      // Right operand                         │
└─────────────────────────────────────────────────────────────┘
```

Applies to: SBLR3_EXPR_ADD, SBLR3_EXPR_SUBTRACT, SBLR3_EXPR_MULTIPLY, etc.

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:125`

#### Function Call (SCHEMA_FUNC_CALL)

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEMA_FUNC_CALL                           │
├───────────────────────────────────────────────────────────────┤
│ [ident]   function_name                                       │
│ [expr_list] args                                              │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:176-183`

#### Aggregate Call (SCHEMA_AGG_CALL)

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEMA_AGG_CALL                            │
├───────────────────────────────────────────────────────────────┤
│ [ident]   function_name                                       │
│ [expr_list] args                                              │
│ [bool]    distinct                                            │
│ [opt<expr>] filter                                            │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:185-190`

### Common Schema Patterns

#### Column Reference (SCHEMA_COLUMN_REF)

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEMA_COLUMN_REF                          │
├───────────────────────────────────────────────────────────────┤
│ [ident]   table_name   // Optional                            │
│ [ident]   column_name                                         │
│ [u16]     column_index // For positional refs                 │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:213`

#### Table Reference (SCHEMA_TABLE_REF)

```
┌─────────────────────────────────────────────────────────────┐
│                    SCHEMA_TABLE_REF                           │
├───────────────────────────────────────────────────────────────┤
│ [schema_path] schema   // Schema path list                    │
│ [ident]       name                                            │
│ [ident]       alias    // Optional alias                      │
│ [opt<uuid>]   table_id // Resolved table UUID                 │
└─────────────────────────────────────────────────────────────┘
```

#### Type Specification (TYPE_SPEC)

```
┌─────────────────────────────────────────────────────────────┐
│                     TYPE_SPEC ENCODING                        │
├───────────────────────────────────────────────────────────────┤
│ [u16]   type_opcode    // SBLR3_TYPE_* value                  │
│ [bytes] type_payload   // Type-specific parameters            │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:534-539`

### Interface Contracts

#### encodePayloadBySchema()

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_codec.h:34
bool encodePayloadBySchema(
    const SchemaDef& schema,  // Schema definition
    const Value& payload,     // Payload value object
    Buffer& out,              // Output buffer
    DecodeError& err          // Error output
);
```

**Preconditions:**
- payload is an Object (map) for most schemas
- Special handling for OPTION_KV schema

**Postconditions:**
- out contains encoded payload bytes
- Fields encoded in schema field order

**Error Handling:**
- Returns false on type mismatch
- Returns false on missing required fields

#### decodePayloadBySchema()

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_codec.h:35
bool decodePayloadBySchema(
    const SchemaDef& schema,  // Schema definition
    const uint8_t* data,      // Input buffer
    size_t size,              // Buffer size
    size_t& offset,           // Current position (updated)
    Value& out,               // Output value
    DecodeError& err          // Error output
);
```

**Preconditions:**
- offset within buffer bounds
- Schema is valid

**Postconditions:**
- out contains decoded Object
- offset advanced past decoded data

### Algorithms

#### Schema-Based Encoding

```
Input:  Schema definition, payload value
Output: Encoded bytes

1. If schema.name == "OPTION_KV":
   a. Encode key-value pairs
   b. Return
2. Verify payload is Object
3. For each field in schema.fields:
   a. Get value from payload object (or default)
   b. Encode based on field.type
   c. Append to output
4. Return success
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:270-376`

#### Schema-Based Decoding

```
Input:  Schema definition, byte buffer
Output: Value object

1. If schema.name == "OPTION_KV":
   a. Decode count
   b. Decode key-value pairs
   c. Return list
2. Create new Object
3. For each field in schema.fields:
   a. Decode based on field.type
   b. Store in object with field name
4. Return object
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:378-413`

#### Instruction Encoding with Schema

```
Input:  Instruction
Output: Encoded bytes

1. Lookup schema for opcode
2. If schema found:
   a. Encode payload using schema
3. Else:
   a. Use raw bytes from payload
4. Write header: opcode (u16) + flags (u16) + len (u32)
5. Append payload bytes
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:275-308`

#### Instruction Decoding with Schema

```
Input:  Byte buffer
Output: Instruction

1. Read header: opcode (u16) + flags (u16) + len (u32)
2. Lookup schema for opcode
3. If schema found:
   a. Decode payload using schema
4. Else:
   b. Store raw bytes as payload
5. Return instruction
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_payloads.cpp:310-338`

## Invariants

1. **Schema Existence**: schemaForOpcode returns valid schema for known opcodes
   - Verification: Opcode registry contains mapping

2. **Field Order**: Fields encoded/decoded in schema definition order
   - Verification: Iteration order in encode/decode functions

3. **Type Safety**: Field values match declared FieldType
   - Verification: Runtime checks in encodeValue/decodeValue

4. **Null Handling**: OPT fields have presence byte; null values use std::monostate
   - Verification: encode/decode for OPT type

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| "payload is not object" | Non-object for struct schema | Return encoding error |
| "expected u8/u16/..." | Type mismatch during encoding | Return encoding error |
| "payload length mismatch" | Decoded size != declared size | Return decoding error |
| "unknown schema" | Schema ref not in registry | Return encoding error |
| "invalid string length" | Varuint decode failure | Return decoding error |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_sblr_v3_payload_codec.cpp` | Encode/decode with schema |
| `test_sblr_v3_schema.cpp` | Schema lookup and validation |
| `test_sblr_v3_container.cpp` | Full container roundtrip |

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Schema | Structure definition for opcode payload |
| FieldDef | Single field with name, type, and reference |
| Value | Variant type holding any payload data |
| Object | Map<string, Value> for structured payloads |
| InstrPtr | Shared pointer to nested instruction |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | SBLR Team |
