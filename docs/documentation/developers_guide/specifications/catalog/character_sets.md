# Specification: Character Sets

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11040`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`

## Synopsis

This specification defines character set metadata storage, including character set properties, default collations, and charset-to-collation relationships.

## Scope

### In Scope

- Character set definitions (CharsetInfo)
- Character set properties (min/max bytes, variable width)
- Default collation per charset
- Character set aliases
- sb_charset catalog table

### Out of Scope

- Collation definitions (see `collations.md`)
- Character encoding conversion algorithms
- Character classification (isalpha, isdigit, etc.)

## Specification

### CharsetInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11040`

```cpp
struct CharsetInfo {
    uint16_t charset_id = 0;        // Character set ID
    ID charset_uuid{};              // UUID reference
    std::string name;               // e.g., "utf8", "latin1"
    std::string description;        // Human-readable description
    
    // Properties
    uint8_t min_bytes = 1;          // Minimum bytes per character
    uint8_t max_bytes = 1;          // Maximum bytes per character
    uint8_t variable_width = 0;     // 1 = variable width, 0 = fixed
    uint8_t reserved = 0;
    
    // Default collation
    uint32_t default_collation_id = 0;
    
    // Metadata
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

### Built-in Character Sets

| ID | Name | Min Bytes | Max Bytes | Variable | Default Collation |
|----|------|-----------|-----------|----------|-------------------|
| 0 | ASCII | 1 | 1 | No | ascii_general_ci |
| 1 | LATIN1 | 1 | 1 | No | latin1_swedish_ci |
| 2 | UTF8 | 1 | 3 | Yes | utf8_general_ci |
| 3 | UTF16 | 2 | 4 | Yes | utf16_general_ci |
| 4 | UTF32 | 4 | 4 | No | utf32_general_ci |
| 5 | UTF8MB4 | 1 | 4 | Yes | utf8mb4_general_ci |

### Character Set Properties

**ASCII:**
- 7-bit character set
- Direct mapping to first 128 Unicode code points
- Fixed 1 byte per character

**LATIN1 (ISO-8859-1):**
- Western European character set
- Fixed 1 byte per character
- Direct mapping to Unicode U+0000 to U+00FF

**UTF8:**
- Variable-width encoding
- 1-3 bytes per character (BMP only)
- Backward compatible with ASCII
- Max code point: U+FFFF

**UTF8MB4:**
- Full UTF-8 support
- 1-4 bytes per character
- Supports supplementary characters (emoji, etc.)
- Max code point: U+10FFFF

### sb_charset Catalog Table

```cpp
struct CharsetRecord {
    // Primary key
    uint16_t charset_id;
    
    // Identity
    ID charset_uuid;
    char name[64];
    char description[256];
    
    // Properties
    uint8_t min_bytes;
    uint8_t max_bytes;
    uint8_t variable_width;
    uint8_t reserved;
    
    // Default collation
    uint32_t default_collation_id;
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### CharsetAliasCatalogInfo

**Source:** `include/scratchbird/core/catalog_manager.h:3516`

```cpp
struct CharsetAliasCatalogInfo {
    ID alias_id;
    ID charset_id;                  // Referenced charset
    ID bundle_id;                   // Resource bundle
    std::string alias_name;         // e.g., "UTF-8" for "utf8"
    std::string normalized_name;    // Normalized form
    bool is_valid = true;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

### Character Set Resolution

```cpp
// Resolve charset by name
Status resolveCharsetId(const std::string& name, uint16_t& id_out) {
    1. Normalize name (lowercase, remove hyphens)
    2. Check built-in charsets:
       - "ascii" -> 0
       - "latin1", "iso88591" -> 1
       - "utf8" -> 2
       - "utf16" -> 3
       - "utf32" -> 4
       - "utf8mb4" -> 5
    3. If not built-in, query sb_charset_aliases
    4. Return NOT_FOUND if unknown
}
```

## Algorithms

### Algorithm: Get Character Set Info

```
Input:  Charset ID or name
Output: CharsetInfo

1. If input is name:
   a. Resolve to charset_id
2. Lookup in charset cache
3. If not cached:
   a. Query sb_charset
   b. Build CharsetInfo
   c. Add to cache
4. Return CharsetInfo
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `CHARSET_INV_001` | charset_id unique | Primary key |
| `CHARSET_INV_002` | min_bytes <= max_bytes | Validation |
| `CHARSET_INV_003` | Default collation exists | Foreign key |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `CHARSET_NOT_FOUND` | Unknown charset name | Use valid charset |
| `INVALID_CHARSET` | Charset ID out of range | Check charset_id |

## Related Specifications

- [collations.md](./collations.md) - Collation definitions

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
