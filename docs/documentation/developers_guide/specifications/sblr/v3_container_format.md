# Specification: SBLR v3 Container Format

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_container.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_codec.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_container.cpp:1`

## Synopsis

This specification defines the binary container format for SBLR v3 bytecode modules. A container encapsulates all information needed to execute a compiled SQL statement or procedure: metadata, symbol tables, constant pool, and the instruction stream.

## Scope

### In Scope

- Container header format and magic bytes
- Section directory structure
- Required and optional sections
- Module metadata encoding
- Symbol table encoding
- Constant pool encoding
- Bytecode stream layout
- Serialization and deserialization algorithms

### Out of Scope

- Individual instruction payload formats (see v3_payload_schemas.md)
- Container signing and encryption
- Network transport framing

## Background

The SBLR v3 container format provides a portable, versioned, and extensible format for compiled SQL modules. It is designed for:

1. **Parser-to-Engine Communication**: Serialized form of compiled statements
2. **Plan Caching**: Persistent storage of compiled execution plans
3. **UDR Artifacts**: Compiled user-defined routines

All multi-byte integers are little-endian. Section offsets are 8-byte aligned.

## Specification

### Data Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:11
struct ContainerHeader {
    char magic[4];              // "SBL3"
    uint16_t version_major;     // 3
    uint16_t version_minor;     // 0
    uint16_t version_patch;     // 0
    uint16_t flags;             // Container flags
    uint16_t section_count;     // Number of sections
    uint16_t header_size;       // Total header bytes
    uint64_t container_size;    // Total container bytes
    uint64_t timestamp_utc;     // Creation timestamp
    uint8_t module_id[16];      // UUID
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:24
struct SectionEntry {
    uint16_t section_id;        // Section type identifier
    uint16_t section_flags;     // Section-specific flags
    uint64_t offset;            // Offset from container start
    uint64_t length;            // Section length in bytes
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:31
enum SectionId : uint16_t {
    SECTION_MODULE_METADATA = 0x0001,
    SECTION_SYMBOL_TABLE = 0x0002,
    SECTION_CONSTANT_POOL = 0x0003,
    SECTION_BYTECODE_STREAM = 0x0004,
    SECTION_DEPENDENCIES = 0x0005,
    SECTION_DEBUG_INFO = 0x0006,
    SECTION_INTEGRITY = 0x0007
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:41
struct ModuleMetadata {
    std::string module_name;
    std::string module_version;
    uint16_t dialect_id = 0;
    uint16_t target_platform = 0;
    std::string build_id;
    std::vector<uint8_t> source_hash;
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:50
struct ConstantPoolEntry {
    uint8_t tag = 0;
    Value value;
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:55
struct Container {
    ContainerHeader header{};
    std::vector<SectionEntry> sections;
    ModuleMetadata metadata;
    std::vector<std::string> symbols;
    std::vector<ConstantPoolEntry> constants;
    std::vector<uint8_t> bytecode_stream;
    std::vector<uint8_t> dependencies;
    std::vector<uint8_t> debug_info;
    std::vector<uint8_t> integrity;
};
```

### Binary Layout

```
┌─────────────────────────────────────────────────────────────┐
│                        CONTAINER HEADER                       │
├─────────────┬─────────┬───────────────────────────────────────┤
│ Offset      │ Size    │ Field                                 │
├─────────────┼─────────┼───────────────────────────────────────┤
│ 0x00        │ 4       │ magic[4] = "SBL3"                     │
│ 0x04        │ 2       │ version_major (3)                     │
│ 0x06        │ 2       │ version_minor (0)                     │
│ 0x08        │ 2       │ version_patch (0)                     │
│ 0x0A        │ 2       │ flags                                 │
│ 0x0C        │ 2       │ section_count                         │
│ 0x0E        │ 2       │ header_size                           │
│ 0x10        │ 8       │ container_size                        │
│ 0x18        │ 8       │ timestamp_utc                         │
│ 0x20        │ 16      │ module_id[16] (UUID)                  │
├─────────────┴─────────┴───────────────────────────────────────┤
│                    SECTION DIRECTORY (variable)               │
├─────────────┬─────────┬───────────────────────────────────────┤
│ 0x30        │ 20*n    │ SectionEntry[section_count]           │
│             │         │   - section_id: u16                   │
│             │         │   - section_flags: u16                │
│             │         │   - offset: u64                       │
│             │         │   - length: u64                       │
├─────────────┴─────────┴───────────────────────────────────────┤
│                      PADDING TO 8-BYTE ALIGN                  │
├───────────────────────────────────────────────────────────────┤
│                        SECTION DATA                           │
│              (8-byte aligned, order by section_id)            │
└─────────────────────────────────────────────────────────────┘
```

### Container Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0x0001 | HAS_DEPENDENCIES | Container has DEPENDENCIES section |
| 0x0002 | HAS_DEBUG_INFO | Container has DEBUG_INFO section |
| 0x0004 | HAS_INTEGRITY | Container has INTEGRITY section |

### Section Encodings

#### MODULE_METADATA Section (0x0001)

Required. Contains module identification and compilation metadata.

```
┌─────────────────────────────────────────────────────────────┐
│                    MODULE_METADATA ENCODING                   │
├───────────────────────────────────────────────────────────────┤
│ [varuint] module_name length                                  │
│ [bytes]   module_name (UTF-8)                                 │
│ [varuint] module_version length                               │
│ [bytes]   module_version (UTF-8)                              │
│ [u16]     dialect_id                                          │
│ [u16]     target_platform                                     │
│ [varuint] build_id length                                     │
│ [bytes]   build_id (UTF-8)                                    │
│ [varuint] source_hash length                                  │
│ [bytes]   source_hash                                         │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_container.cpp:44-64`

#### SYMBOL_TABLE Section (0x0002)

Required. Contains string symbols referenced by the bytecode.

```
┌─────────────────────────────────────────────────────────────┐
│                    SYMBOL_TABLE ENCODING                      │
├───────────────────────────────────────────────────────────────┤
│ [varuint] symbol_count                                        │
│ Repeat symbol_count times:                                    │
│   [varuint] string length                                     │
│   [bytes]   string (UTF-8)                                    │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_container.cpp:66-86`

#### CONSTANT_POOL Section (0x0003)

Required. Contains literal values used by the bytecode.

```
┌─────────────────────────────────────────────────────────────┐
│                    CONSTANT_POOL ENCODING                     │
├───────────────────────────────────────────────────────────────┤
│ [varuint] constant_count                                      │
│ Repeat constant_count times:                                  │
│   [u8]      tag                                               │
│   [bytes]   payload (tag-dependent)                           │
└─────────────────────────────────────────────────────────────┘
```

Constant Tags:

| Tag | Type | Payload Encoding |
|-----|------|------------------|
| 0x01 | INT64 | [u64] little-endian signed |
| 0x02 | UINT64 | [u64] little-endian unsigned |
| 0x03 | FLOAT64 | [u64] IEEE 754 double bits |
| 0x04 | STRING_REF | [varuint] symbol table index |
| 0x05 | BYTES | [varuint] length + raw bytes |
| 0x06 | UUID | [16 bytes] raw UUID |
| 0x07 | DECIMAL | [i32] scale + [varuint] BCD length + BCD bytes |
| 0x08 | BOOLEAN | [u8] 0 or 1 |
| 0x09 | TYPED_NULL | [varuint] type opcode |

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_container.cpp:88-189`

#### BYTECODE_STREAM Section (0x0004)

Required. Contains the instruction stream.

```
┌─────────────────────────────────────────────────────────────┐
│                   BYTECODE_STREAM LAYOUT                      │
├───────────────────────────────────────────────────────────────┤
│ [instruction]...                                              │
│ Each instruction:                                             │
│   [u16]     opcode                                            │
│   [u16]     flags                                             │
│   [u32]     payload_length                                    │
│   [bytes]   payload (payload_length bytes)                    │
└─────────────────────────────────────────────────────────────┘
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_codec.cpp:184-223`

### Algorithms

#### Container Encoding

```
Input:  Container structure
Output: Byte vector

1. Encode module metadata → module_data
2. Encode symbol table → symbols_data
3. Encode constant pool → constants_data
4. Copy bytecode stream → bytecode_data
5. Build section table with offsets
6. Write header with magic "SBL3"
7. Write section directory
8. Write section data in order
9. Patch header_size and container_size
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_container.cpp:191-288`

#### Container Decoding

```
Input:  Byte buffer
Output: Container structure or error

1. Verify magic bytes "SBL3"
2. Parse header fields
3. Read section_count SectionEntry structs
4. For each required section:
   - Verify offset + length ≤ container_size
   - Read and decode section data
5. Return populated Container
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/v3_container.cpp:290-355`

### Interface Contracts

#### encodeContainer()

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:67
bool encodeContainer(
    const Container& container,  // Input container structure
    std::vector<uint8_t>& out,   // Output byte vector (cleared)
    std::string& err             // Error message on failure
);
```

**Preconditions:**
- Container header magic must be "SBL3"
- version_major must be 3

**Postconditions:**
- out contains valid container bytes
- header_size and container_size are patched
- sections are 8-byte aligned

**Error Handling:**
- Returns false on encoding error with message in err

#### decodeContainer()

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_container.h:68
bool decodeContainer(
    const uint8_t* data,      // Input buffer
    size_t size,              // Buffer size
    Container& out,           // Output container (populated)
    std::string& err          // Error message on failure
);
```

**Preconditions:**
- data points to at least 4 bytes for magic check

**Postconditions:**
- out populated with all decoded sections
- Raw section bytes available for inspection

**Error Handling:**
- Returns false if magic invalid
- Returns false if section bounds exceed buffer
- Returns false if required sections missing

## Invariants

1. **Magic Validation**: First 4 bytes must be "SBL3"
   - Verification: decodeContainer checks memcmp(data, "SBL3", 4)

2. **Version Compatibility**: version_major must be 3
   - Verification: Container validator checks version

3. **Section Alignment**: All section offsets are 8-byte aligned
   - Verification: padTo8() called during encoding

4. **Required Sections**: MODULE_METADATA, SYMBOL_TABLE, CONSTANT_POOL, BYTECODE_STREAM must exist
   - Verification: decodeContainer returns error if missing

5. **Bounds Checking**: section.offset + section.length ≤ container_size
   - Verification: decodeContainer validates before reading

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| "container too small" | size < 4 | Reject input |
| "bad magic" | Magic != "SBL3" | Reject input |
| "header parse" | Header fields incomplete | Reject input |
| "section bounds" | Section extends past container | Reject input |
| "missing required section" | Required section absent | Reject input |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_sblr_v3_container.cpp` | Encode/decode roundtrip, validation |

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Container | Complete compiled module with all sections |
| Section | Logical grouping of related data |
| Varuint | Variable-length unsigned integer (LEB128) |
| UUID | 16-byte universally unique identifier |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | SBLR Team |
