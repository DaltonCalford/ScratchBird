# Specification: Object Identity and Resolution Rules

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:115`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:292`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4855`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_catalog_parentage_and_name_uniqueness.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_catalog_utf8_identifiers.cpp:1`

## Synopsis

This specification defines how database objects are identified, named, and resolved within the ScratchBird catalog system. It covers SQL identifier comparison rules (Firebird-style), name canonicalization, object path resolution, and the relationship between object UUIDs and their human-readable names.

## Scope

### In Scope

- SQL identifier comparison and canonicalization rules
- Object name uniqueness constraints
- Schema path resolution
- Qualified vs unqualified name lookup
- Delimited (quoted) identifier handling
- Name-to-UUID resolution algorithm
- Object parentage rules

### Out of Scope

- UUID generation (see `uuid_mapping.md`)
- Bootstrap sequence (see `bootstrap_sequence.md`)
- Catalog table storage layouts (see `catalog_table_layouts.md`)

## Background

ScratchBird uses Firebird-style SQL identifier rules:
- **Unquoted identifiers**: Case-insensitive, stored as uppercase
- **Quoted identifiers**: Case-sensitive, stored as-is

This differs from PostgreSQL (always case-folds to lowercase) and aligns with Firebird/Oracle traditions. The catalog stores both the original name and a canonical form for efficient lookup.

## Specification

### Identifier Storage Limits

**Source:** `include/scratchbird/core/catalog_manager.h:97`

```cpp
namespace CatalogConstants {
    constexpr size_t MAX_IDENTIFIER_CHARS = 128;   // SQL standard: 128 characters
    constexpr size_t MAX_IDENTIFIER_BYTES = 512;   // Storage: 128 chars × 4 bytes/char (max UTF-8)
    constexpr size_t MAX_IDENTIFIER_STORAGE = 512; // Including null terminator
}
```

### Identifier Comparison Rules

**Source:** `include/scratchbird/core/catalog_manager.h:115`

```cpp
namespace IdentifierUtils {
    // Convert string to uppercase (ASCII-only)
    inline std::string toUpper(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            if (c >= 'a' && c <= 'z') {
                c = static_cast<char>(c - 32);
            }
        }
        return result;
    }
}
```

### Name Conflict Detection

**Source:** `include/scratchbird/core/catalog_manager.h:134`

```cpp
// Returns true if names conflict (would be treated as same object)
inline bool namesConflict(const std::string& name1, bool delimited1,
                          const std::string& name2, bool delimited2) {
    if (delimited1 && delimited2) {
        // Both are case-sensitive: exact match required for conflict
        return name1 == name2;
    }
    // At least one is case-insensitive: compare UPPER to UPPER
    return toUpper(name1) == toUpper(name2);
}
```

### Name Lookup Matching

**Source:** `include/scratchbird/core/catalog_manager.h:150`

```cpp
// Returns true if names match for lookup purposes
inline bool namesMatch(const std::string& search_name, bool search_delimited,
                       const std::string& stored_name, bool stored_delimited) {
    if (stored_delimited) {
        // Stored is case-sensitive: must match exactly
        return search_name == stored_name;
    }
    // Stored is case-insensitive: compare UPPER to UPPER
    return toUpper(search_name) == toUpper(stored_name);
}
```

### Canonical Identifier for Lookup

**Source:** `src/core/catalog_manager.cpp:292`

```cpp
std::string canonicalIdentifierForLookup(const std::string& value) {
    return IdentifierUtils::toUpper(value);
}
```

### Object Name Record Structure

**Source:** `src/core/catalog_manager.cpp:4833`

```cpp
struct ObjectNameRecord {
    ID name_id;                    // UUID for this name entry
    ID object_id;                  // Reference to actual object
    uint8_t object_type;           // ObjectType enum
    uint8_t reserved_1[3];
    ID parent_object_id;           // Parent for hierarchical naming
    char schema_path[512];         // Full schema path
    char language_code[32];        // Language for i18n names
    char name_text[512];           // Original name text
    char canonical_name_text[512]; // Canonical form (uppercase)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Schema Path Resolution

**Source:** `include/scratchbird/core/catalog_manager.h:163`

```cpp
enum class PathType : uint8_t {
    UNQUALIFIED = 0,   // No schema specified
    CURRENT = 1,       // Current schema only
    PARENT = 2,        // Parent schema reference
    ABSOLUTE = 3       // Fully qualified path
};

struct ObjectPath {
    PathType type = PathType::UNQUALIFIED;
    bool no_search_path = false;  // True when !: disables search path
    std::vector<std::string> components;
};
```

### Schema Record Structure

**Source:** `src/core/catalog_manager.cpp:4851`

```cpp
struct SchemaRecord {
    ID schema_id;
    ID parent_schema_id;            // Parent schema UUID (zero for top-level)
    char schema_name[512];          // Schema name
    ID owner_id;                    // Owner UUID reference
    ID default_tablespace_id;       // Default tablespace UUID
    uint32_t permissions;
    ID default_charset_id;          // Default charset UUID
    uint8_t name_is_delimited;      // 1 if quoted identifier
    uint8_t reserved[7];
    uint32_t default_collation_id;
    ID acl_oid;                     // TOAST reference for ACL
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Object Record Structure

**Source:** `src/core/catalog_manager.cpp:4818`

```cpp
struct ObjectRecord {
    ID object_id;
    uint8_t object_type;
    uint8_t reserved_1[3];
    ID schema_id;
    ID parent_object_id;
    ID owner_id;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

## Algorithms

### Algorithm: Resolve Object by Name

```
Input:  Schema path, object name, object type
Output: Object UUID or NOT_FOUND

1. Parse schema path:
   a. If unqualified: use current schema + search path
   b. If qualified: use specified schema only
   
2. Canonicalize object name:
   a. If delimited (quoted): use as-is
   b. If not delimited: convert to uppercase
   
3. Build lookup key: (schema_path, object_type, canonical_name)
   
4. Query object_name table unique index
   
5. If found:
   a. Return object_id from record
   b. Cache result in session cache
   
6. If not found:
   a. If using search path, try next schema in path
   b. Return NOT_FOUND if all schemas exhausted
```

**Complexity:**
- Time: O(log n) with index, O(k log n) with search path (k = path length)
- Space: O(1)

### Algorithm: Check Name Uniqueness

```
Input:  Schema ID, proposed name, object type, is_delimited flag
Output: UNIQUE or CONFLICT with existing object

1. Canonicalize proposed name:
   a. If is_delimited: canonical = name
   b. Else: canonical = toUpper(name)
   
2. Query existing names in schema:
   SELECT * FROM object_name 
   WHERE schema_id = ? AND object_type = ?
   
3. For each existing name:
   a. If existing.is_delimited AND is_delimited:
      - Conflict if name == existing.name
   b. Else:
      - Conflict if canonical == existing.canonical_name
      
4. Return CONFLICT if any match, else UNIQUE
```

**Complexity:**
- Time: O(n) where n = objects in schema of type
- Space: O(1)

### Algorithm: Rename Object

```
Input:  Object UUID, new name, new_delimited flag
Output: SUCCESS or ERROR

1. Verify new name uniqueness (see Algorithm: Check Name Uniqueness)
   
2. Begin transaction
   
3. Update object_name record:
   - Set name_text = new_name
   - Set canonical_name_text = canonicalize(new_name, new_delimited)
   - Set name_is_delimited = new_delimited
   - Update last_modified_time
   
4. If table: update TableRecord.table_name
   If schema: update SchemaRecord.schema_name
   (Object UUID remains unchanged)
   
5. Invalidate session caches
   
6. Commit transaction
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `IDENT_INV_001` | (schema_path, object_type, canonical_name) is unique | Unique index on object_name |
| `IDENT_INV_002` | Object UUID is immutable | Primary key constraint |
| `IDENT_INV_003` | Name conflicts detected per Firebird rules | namesConflict() validation |
| `IDENT_INV_004` | Schema parentage forms a tree (no cycles) | Closure check at creation |
| `IDENT_INV_005` | Owner UUID always references valid principal | Foreign key constraint |

## Decision Trees

### Name Resolution Decision Tree

```
Is name qualified (contains '.')?
├── No → Unqualified lookup:
│   └── Try current schema first
│       ├── Found → Return object
│       └── Not found → Try search path in order
│           ├── Found → Return object
│           └── Exhausted → ERROR: object not found
└── Yes → Qualified lookup:
    └── Parse schema path
        ├── Absolute path (starts with root)
        │   └── Lookup in exact schema
        ├── Relative path
        │   └── Resolve relative to current schema
        └── Not found → ERROR: schema does not exist
```

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `NAME_CONFLICT` | Name already exists in schema | Choose different name |
| `INVALID_IDENTIFIER` | Exceeds length or invalid chars | Truncate or sanitize |
| `SCHEMA_NOT_FOUND` | Schema path does not exist | Create schema or correct path |
| `AMBIGUOUS_NAME` | Name found in multiple search path schemas | Use qualified name |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_parentage_and_name_uniqueness.cpp` | Parentage and uniqueness |
| `tests/integration/test_catalog_utf8_identifiers.cpp` | UTF-8 identifier handling |
| `tests/unit/test_catalog_rename_move.cpp` | Rename operations |

## Migration Notes

- Name canonicalization changed from lowercase (PostgreSQL-style) to uppercase (Firebird-style) in v0.1.0
- Migration tool must rename existing non-delimited identifiers to uppercase
- Delimited identifiers are unaffected

## Related Specifications

- `uuid_mapping.md` - UUID-based object identification
- `bootstrap_sequence.md` - Schema creation during bootstrap
- `catalog_table_layouts.md` - Storage of object metadata

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Delimited Identifier | Quoted identifier (e.g., "MyTable") - case-sensitive |
| Non-delimited | Unquoted identifier (e.g., mytable) - case-insensitive |
| Canonical Name | Uppercase form used for comparison |
| Schema Path | Dot-separated path (e.g., root.sys.security) |
| Search Path | Ordered list of schemas for unqualified lookups |

### References

- `local_work/docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_OBJECT_PARENTAGE_AND_NAME_UNIQUENESS.md`
- Firebird SQL Reference: Identifiers
- SQL:2016 Standard §5.2

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
