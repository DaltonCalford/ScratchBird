# Specification: Collations

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11067`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:3483`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp`

## Synopsis

This specification defines collation metadata storage, including collation properties, tailoring rules, and collation-to-charset relationships.

## Scope

### In Scope

- Collation definitions (CollationCatalogInfo)
- Collation tailoring kinds
- Collation strength and padding rules
- Default collation per charset
- sb_collation catalog table

### Out of Scope

- Collation comparison algorithms (see charset_manager)
- Collation weight tables (binary data)

## Specification

### Collation Tailoring Kinds

**Source:** `include/scratchbird/core/catalog_manager.h:3483`

```cpp
enum class CollationTailoringKind : uint8_t {
    UCA = 1,                // Unicode Collation Algorithm
    LOCALE = 2,             // Locale-based collation
    VENDOR_MYSQL = 3,       // MySQL-compatible
    VENDOR_FIREBIRD = 4,    // Firebird-compatible
    VENDOR_POSTGRESQL = 5,  // PostgreSQL-compatible
    CUSTOM = 6              // User-defined
};
```

### CollationCatalogInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11067`

```cpp
struct CollationCatalogInfo {
    uint32_t collation_id = 0;      // Unique collation ID
    std::string name;               // e.g., "utf8_general_ci"
    uint16_t charset_id = 0;        // Associated charset
    ID charset_uuid{};              // Charset UUID reference
    
    // Properties
    uint8_t collation_type = 0;     // CollationType enum
    uint8_t strength = 0;           // CollationStrength enum
    uint8_t pad_space = 1;          // 1 = PAD SPACE, 0 = NO PAD
    uint8_t is_default = 0;         // 1 = default for charset
    uint16_t reserved = 0;
    
    // Locale
    char locale[32] = {0};          // e.g., "en_US"
    
    // Metadata
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

### Built-in Collations

| ID | Name | Charset | Strength | Pad | Description |
|----|------|---------|----------|-----|-------------|
| 1 | utf8_bin | UTF8 | Binary | NO PAD | Binary comparison |
| 2 | utf8_general_ci | UTF8 | Primary | PAD SPACE | Case-insensitive |
| 3 | utf8_unicode_ci | UTF8 | Secondary | PAD SPACE | Unicode collation |
| 4 | latin1_bin | LATIN1 | Binary | NO PAD | Binary |
| 5 | latin1_swedish_ci | LATIN1 | Primary | PAD SPACE | Swedish rules |
| 6 | ascii_bin | ASCII | Binary | NO PAD | Binary |
| 7 | ascii_general_ci | ASCII | Primary | PAD SPACE | Case-insensitive |

### Collation Strength Levels

| Level | Name | Comparison |
|-------|------|------------|
| 1 | Primary | Base letters (a = A = á) |
| 2 | Secondary | Accents (a = A ≠ á) |
| 3 | Tertiary | Case (a ≠ A ≠ á) |
| 4 | Quaternary | Punctuation |
| 5 | Identical | Code point |

### Padding Rules

**PAD SPACE:**
- Trailing spaces are ignored in comparison
- `'abc' = 'abc '`

**NO PAD:**
- Trailing spaces are significant
- `'abc' ≠ 'abc '`

### CollationTailoringCatalogInfo

**Source:** `include/scratchbird/core/catalog_manager.h:3528`

```cpp
struct CollationTailoringCatalogInfo {
    ID tailoring_id;
    uint32_t collation_id = 0;
    ID bundle_id;                   // Resource bundle
    CollationTailoringKind tailoring_kind;
    std::optional<std::string> tailoring_json;   // JSON rules
    std::optional<std::string> tailoring_blob;   // Binary weights
    std::string tailoring_hash;     // Integrity hash
    bool is_valid = true;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

### sb_collation Catalog Table

```cpp
struct CollationRecord {
    // Primary key
    uint32_t collation_id;
    
    // Identity
    char name[128];
    uint16_t charset_id;
    ID charset_uuid;
    
    // Properties
    uint8_t collation_type;
    uint8_t strength;
    uint8_t pad_space;
    uint8_t is_default;
    uint16_t reserved;
    
    // Locale
    char locale[32];
    
    // Tailoring reference
    ID tailoring_id;
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Collation SQL Syntax

```sql
-- Create collation
CREATE COLLATION french_ci (
    LOCALE = 'fr_FR',
    STRENGTH = secondary
);

-- Use collation in column definition
CREATE TABLE products (
    name VARCHAR(100) COLLATE utf8_general_ci,
    code VARCHAR(20) COLLATE utf8_bin
);

-- Use collation in query
SELECT * FROM products
WHERE name COLLATE utf8_bin = 'ABC';

-- Set default collation for charset
ALTER CHARACTER SET utf8 
    DEFAULT COLLATION utf8_unicode_ci;
```

## Algorithms

### Algorithm: Resolve Collation

```
Input:  Collation name or ID
Output: CollationCatalogInfo

1. If input is name:
   a. Normalize name
   b. Lookup in collation cache
   c. If not found, query sb_collation
2. If input is ID:
   a. Direct cache lookup
3. If tailoring needed:
   a. Load CollationTailoringCatalogInfo
   b. Apply tailoring rules
4. Return complete CollationCatalogInfo
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `COLL_INV_001` | collation_id unique | Primary key |
| `COLL_INV_002` | charset_id references valid charset | Foreign key |
| `COLL_INV_003` | Only one default per charset | Unique constraint |
| `COLL_INV_004` | Locale valid for tailoring kind | Validation |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `COLLATION_NOT_FOUND` | Unknown collation | Use valid collation |
| `INVALID_COLLATION` | Collation incompatible with charset | Use matching charset |

## Related Specifications

- [character_sets.md](./character_sets.md) - Character sets
- [columns.md](./columns.md) - Column collations

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
