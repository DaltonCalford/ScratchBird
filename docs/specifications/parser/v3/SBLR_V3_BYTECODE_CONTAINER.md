# SBLR V3 Bytecode Container Format
Status: Authoritative (V3)
Last Updated: 2026-02-08

This document defines the V3 bytecode container format used to serialize SBLR
modules. It replaces the V2 `SBLR_Header_v2` with a sectioned container that
supports module metadata, symbol tables, constant pools, debug info, and
dependency lists.

## Goals
- Single, deterministic container layout for all V3 modules.
- Explicit section table with offsets and lengths.
- Stable identifiers for symbols and constants.
- optional debug and integrity sections without breaking compatibility.

## Encoding (Normative)
- All multi-byte integers are little-endian.
- All offsets are absolute from the start of the file.
- All sections are 8-byte aligned. Padding bytes are `0x00`.
- Strings are UTF-8 with `[len:varuint][bytes]`.
- `varuint` is unsigned LEB128 (as in `SBLR_V3_OPCODE_SPEC.md`).

## Container Layout
```
[SBLR3_ContainerHeader]
[SectionTable]
[SectionData...]
```

### SBLR3_ContainerHeader (Fixed)
```
struct SBLR3_ContainerHeader {
  magic[4]            // "SBL3"
  version_major:u16   // currently 3
  version_minor:u16   // currently 0
  version_patch:u16   // currently 0
  flags:u16           // container flags (see below)
  section_count:u16
  header_size:u16     // bytes from start of header to end of SectionTable
  container_size:u64  // total file length in bytes
  timestamp_utc:u64   // unix epoch seconds, 0 if unknown
  module_id[16]       // UUID v7 (binary), 0 if unknown
}
```

### Container Flags
- `0x0001` = has_debug
- `0x0002` = has_integrity
- `0x0004` = has_dependencies
All other bits are reserved and must be zero.

### SectionTable Entry
```
struct SBLR3_SectionEntry {
  section_id:u16
  section_flags:u16
  offset:u64
  length:u64
}
```

`section_flags` are section-specific. Unknown flags must be ignored for
forward-compatibility.

## Section IDs (Normative)
| ID | Section | Required |
| --- | --- | --- |
| 0x0001 | MODULE_METADATA | yes |
| 0x0002 | SYMBOL_TABLE | yes |
| 0x0003 | CONSTANT_POOL | yes |
| 0x0004 | BYTECODE_STREAM | yes |
| 0x0005 | DEPENDENCIES | optional |
| 0x0006 | DEBUG_INFO | optional |
| 0x0007 | INTEGRITY | optional |

## Sections

### MODULE_METADATA
Stores module-wide information used by loaders and validators.
```
struct MODULE_METADATA {
  module_name:string
  module_version:string
  dialect_id:u16         // SQL dialect or engine compatibility level
  target_platform:u16    // reserved; 0 = generic
  build_id:string        // optional build label
  source_hash:bytes      // optional hash of source (0 length = absent)
}
```

### SYMBOL_TABLE
Symbol table provides stable `string_id` values used throughout the module.
```
struct SYMBOL_TABLE {
  symbol_count:varuint
  symbols[symbol_count]: string
}
```

`string_id` is the 0-based index into `symbols`.

### CONSTANT_POOL
Holds canonical constants shared across bytecode.
```
struct CONSTANT_POOL {
  pool_count:varuint
  entries[pool_count]:
    [tag:u8][payload...]
}
```

Constant pooling rules and deterministic ordering are defined in
`/docs/specifications/parser/v3/SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.

Tags:
- `0x01` = int64    `[value:i64]`
- `0x02` = uint64   `[value:u64]`
- `0x03` = float64  `[value:f64]`
- `0x04` = string   `[string_id:varuint]`
- `0x05` = bytes    `[len:varuint][bytes...]`
- `0x06` = uuid     `[uuid16]`
- `0x07` = decimal  `[scale:i32][bytes_len:varuint][bcd_bytes...]`
- `0x08` = boolean  `[value:bool]`
- `0x09` = null     `[type_id:varuint]`  // typed null

### BYTECODE_STREAM
Raw SBLR instruction stream:
```
[SBLR3_VERSION instruction]
[instruction...]
[SBLR3_END]
```
The stream is validated against `SBLR_V3_OPCODE_SPEC.md` and
`SBLR_V3_OPCODE_SEMANTICS.md`.

### DEPENDENCIES (optional)
Lists external modules and catalog dependencies.
```
struct DEPENDENCIES {
  dep_count:varuint
  deps[dep_count]:
    [kind:u8][name:string][version:string][checksum:bytes]
}
```
`kind`:
- `0x01` = module
- `0x02` = schema/object path
- `0x03` = external library/udr

### DEBUG_INFO (optional)
Maps bytecode offsets to source positions and symbols.
```
struct DEBUG_INFO {
  file_count:varuint
  files[file_count]: string
  map_count:varuint
  map[map_count]:
    [bytecode_offset:u64][file_id:varuint][line:u32][column:u16]
  symbol_map_count:varuint
  symbol_map[symbol_map_count]:
    [symbol_id:varuint][bytecode_offset:u64]
}
```

### INTEGRITY (optional)
Provides container integrity and optional signatures.
```
struct INTEGRITY {
  hash_alg:u8         // 1=SHA256, 2=SHA512
  hash_len:u16
  hash_bytes[hash_len]
  sig_alg:u8          // 0=none, 1=Ed25519, 2=RSA-PSS
  sig_len:u16
  sig_bytes[sig_len]
}
```
The hash covers the entire container except the INTEGRITY section itself.

## Validation Rules (Normative)
- `magic` must be `SBL3`.
- `version_major` must be 3.
- `container_size` must match file length.
- Section offsets must be aligned to 8 bytes and non-overlapping.
- Required sections must be present.
- `BYTECODE_STREAM` must begin with `SBLR3_VERSION` and end with `SBLR3_END`.
- If `has_integrity` is set, the INTEGRITY section must exist and validate.
- Unknown sections must be ignored but preserved if rewriting.

## Forward Compatibility
Loaders must ignore unknown section IDs and unknown section flags. New optional
sections can be added without changing the container header.

## Container Validation Checklist
- `magic` is `SBL3` and `version_major` is 3.
- `container_size` matches actual file length.
- Section table entries are 8-byte aligned, ordered by offset, and non-overlapping.
- Required sections (MODULE_METADATA, SYMBOL_TABLE, CONSTANT_POOL, BYTECODE_STREAM) are present.
- BYTECODE_STREAM begins with `SBLR3_VERSION` and ends with `SBLR3_END`.
- If `has_integrity` is set, INTEGRITY exists and verifies hash/signature.
- Full bytecode validation passes per `/docs/specifications/parser/v3/SBLR_V3_VALIDATION_RULES.md`.
