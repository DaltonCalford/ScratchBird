# Specification: Name Resolution

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Synopsis

This specification defines SQL name resolution rules, including qualified vs unqualified names, search path processing, and schema path resolution.

## Scope

### In Scope

- Path types (UNQUALIFIED, CURRENT, PARENT, ABSOLUTE)
- ObjectPath structure
- Search path processing
- Schema resolution order
- Name canonicalization

### Out of Scope

- Identifier comparison rules (see `object_identity_rules.md`)
- UUID mapping (see `uuid_mapping.md`)

## Specification

### Path Types

**Source:** `include/scratchbird/core/catalog_manager.h:163`

```cpp
enum class PathType : uint8_t {
    UNQUALIFIED = 0,   // No schema specified (use search path)
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

### Name Resolution Patterns

| Pattern | PathType | Example |
|---------|----------|---------|
| `tablename` | UNQUALIFIED | `SELECT * FROM employees` |
| `schema.table` | ABSOLUTE | `SELECT * FROM hr.employees` |
| `!table` | UNQUALIFIED + no_search_path | `SELECT * FROM !employees` |
| `./table` | CURRENT | `SELECT * FROM ./employees` |
| `../table` | PARENT | `SELECT * FROM ../employees` |
| `root.sys.table` | ABSOLUTE | `SELECT * FROM root.sys.users` |

### Search Path

```cpp
// Default search path: current_schema, public
std::vector<ID> default_search_path = {
    current_schema_id,
    public_schema_id
};

// Custom search path
SET search_path = 'sales', 'marketing', 'public';
```

**Search Path Resolution:**

```
For unqualified name "employees":

1. For each schema in search_path:
   a. Look for "employees" in schema
   b. If found: return object
   
2. If not found in any schema:
   a. ERROR: "relation 'employees' does not exist"
   
3. If found in multiple schemas:
   a. Use first match
   b. WARNING: "ambiguous reference"
```

### Name Canonicalization

```cpp
// Firebird-style: unquoted = uppercase
std::string canonicalizeName(const std::string& name, bool is_delimited) {
    if (is_delimited) {
        return name;  // Case-sensitive, as-is
    }
    return toUpper(name);  // Case-insensitive, uppercase
}

// Examples:
// "MyTable" (delimited) -> "MyTable"
// MyTable (not delimited) -> "MYTABLE"
// mytable (not delimited) -> "MYTABLE"
```

## Algorithms

### Algorithm: Resolve Object Name

```
Input:  Name string, object type, current context
Output: Object ID or NOT_FOUND

1. Parse name into ObjectPath:
   a. Split by '.'
   b. Identify PathType
   c. Extract components

2. Switch on PathType:

   case UNQUALIFIED:
     a. If no_search_path:
        - Look in current schema only
     b. Else:
        - For each schema in search_path:
          * Look for name in schema
          * If found: return object
     c. Return NOT_FOUND

   case ABSOLUTE:
     a. Resolve full path:
        - root.schema.object
        - schema.object
     b. If schema not found: return NOT_FOUND
     c. Look for object in schema
     d. Return object or NOT_FOUND

   case CURRENT:
     a. Look in current schema
     b. Return object or NOT_FOUND

   case PARENT:
     a. Get parent of current schema
     b. Look for object in parent
     c. Return object or NOT_FOUND
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `RES_INV_001` | Search path never empty | Validation |
| `RES_INV_002` | Current schema always valid | Session check |
| `RES_INV_003` | Absolute paths resolve deterministically | Path parsing |

## Related Specifications

- [object_identity_rules.md](./object_identity_rules.md) - Identity rules
- [uuid_mapping.md](./uuid_mapping.md) - UUID resolution

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
