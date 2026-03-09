# Specification: Domains

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:3379`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:3408`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4855`

## Synopsis

This specification defines domain metadata, including user-defined data types with constraints, domain hierarchies, and domain-specific attributes for complex type emulation.

## Scope

### In Scope

- Domain types and constraints
- Domain constraint kinds (NOT NULL, DEFAULT, CHECK)
- Domain parameters and values
- Domain hierarchies (domains based on domains)
- Domain catalog tables

### Out of Scope

- Complex type system (see type specs)
- Domain validation during INSERT/UPDATE (see executor specs)

## Specification

### Domain Constraint Kinds

**Source:** `include/scratchbird/core/catalog_manager.h:3379`

```cpp
enum class DomainConstraintKind : uint8_t {
    NOT_NULL = 1,   // NOT NULL constraint
    DEFAULT = 2,    // DEFAULT value
    CHECK = 3       // CHECK constraint
};
```

### Domain Parameter Types

**Source:** `include/scratchbird/core/catalog_manager.h:3364`

```cpp
enum class DomainParamType : uint8_t {
    U32 = 1,
    I32 = 2,
    U64 = 3,
    I64 = 4,
    U8 = 5,
    BOOL = 6,
    STRING = 7,
    UUID = 8,
    ENUM_VALUE = 9,
    F32 = 10,
    F64 = 11
};
```

### DomainParameterCatalogInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:3418`

```cpp
struct DomainParameterCatalogInfo {
    ID domain_id;                   // Parent domain
    uint16_t param_key_id = 0;      // Parameter key reference
    DomainParamType param_type = DomainParamType::STRING;
    
    // One of these is set based on param_type
    std::optional<uint32_t> val_u32;
    std::optional<int32_t> val_i32;
    std::optional<uint64_t> val_u64;
    std::optional<int64_t> val_i64;
    std::optional<uint8_t> val_u8;
    std::optional<bool> val_bool;
    std::optional<std::string> val_string;
    ID val_uuid;
    std::optional<int32_t> val_enum;
    std::optional<float> val_f32;
    std::optional<double> val_f64;
    
    bool is_valid = true;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

### DomainConstraintCatalogInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:3439`

```cpp
struct DomainConstraintCatalogInfo {
    ID constraint_id;
    ID domain_id;
    DomainConstraintKind constraint_kind = DomainConstraintKind::CHECK;
    std::string constraint_expr_sblr;   // SBLR expression
    bool is_valid = true;
    uint64_t created_time = 0;
    uint64_t last_modified_time = 0;
};
```

### Domain SQL Syntax

```sql
-- Simple domain
CREATE DOMAIN email AS VARCHAR(255)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

-- Domain with default
CREATE DOMAIN status_code AS INTEGER
    DEFAULT 0
    CHECK (VALUE >= 0 AND VALUE <= 999);

-- Domain based on domain
CREATE DOMAIN verified_email AS email
    CHECK (VALUE ~ '@verified\.com$');

-- Domain with NOT NULL
CREATE DOMAIN non_empty_string AS VARCHAR
    NOT NULL
    CHECK (LENGTH(VALUE) > 0);

-- Drop domain
DROP DOMAIN email;
DROP DOMAIN IF EXISTS email CASCADE;  -- Drop dependent columns
```

### Domain Hierarchy

```
Base Types:
├── INTEGER
│   └── DOMAIN: positive_int (CHECK > 0)
│       └── DOMAIN: age (CHECK BETWEEN 0 AND 150)
│
├── VARCHAR
│   └── DOMAIN: email (CHECK pattern)
│       └── DOMAIN: verified_email (CHECK domain)
│
└── DECIMAL
    └── DOMAIN: money (precision=10, scale=2)
        └── DOMAIN: price (CHECK >= 0)
```

### sb_domains Catalog Table

```cpp
struct DomainRecord {
    // Primary key
    ID domain_id;
    
    // Identity
    ID schema_id;
    char domain_name[512];
    ID owner_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    
    // Base type
    uint16_t base_type_id;          // Underlying DataType
    ID parent_domain_id;            // NULL if based on base type
    
    // Type modifiers
    uint32_t precision;
    uint32_t scale;
    uint32_t max_length;
    
    // Nullability
    uint8_t not_null;               // Domain-level NOT NULL
    
    // Default value
    ID default_value_oid;           // TOAST reference
    
    // Collation
    uint32_t collation_id;
    
    // Metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Domain Validation

When a value is assigned to a domain-based column:

```
1. Validate against base type
2. Apply domain CHECK constraints (in order)
3. Apply column CHECK constraints
4. If all pass: value accepted
```

## Algorithms

### Algorithm: Create Domain

```
Input:  Schema ID, domain name, base type, constraints
Output: Domain ID

1. Validate domain name unique in schema
2. Resolve base type or parent domain
3. Validate constraints compatible with base type
4. Generate UUIDv7 for domain_id
5. For each constraint:
   a. Create DomainConstraintRecord
6. Create DomainRecord
7. Commit transaction
```

### Algorithm: Validate Domain Value

```
Input:  Domain ID, value
Output: Valid/Invalid with error details

1. Look up domain info
2. If parent_domain_id set:
   a. Recursively validate against parent domain
3. Validate against base type
4. For each constraint:
   a. Evaluate constraint with value
   b. If fails: return error
5. Return success
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `DOM_INV_001` | domain_id is valid UUIDv7 | isUuidV7Local() check |
| `DOM_INV_002` | No circular domain hierarchy | Cycle detection |
| `DOM_INV_003` | Base type exists | Type validation |
| `DOM_INV_004` | Constraints compatible with base type | Validation |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `DOMAIN_EXISTS` | Name conflict | Choose different name |
| `INVALID_BASE_TYPE` | Base type doesn't exist | Use valid type |
| `CIRCULAR_DOMAIN` | Domain hierarchy cycle | Fix parent reference |
| `DOMAIN_IN_USE` | Column uses this domain | Use CASCADE |

## Related Specifications

- [columns.md](./columns.md) - Domain-based columns
- [types.md](./types.md) - Type system

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
