# Specification: Column Metadata

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:607`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:599`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:4917`

## Synopsis

This specification defines column metadata storage, including the ColumnInfo structure, generated columns, IDENTITY columns, default values, and CHECK constraints at column level.

## Scope

### In Scope

- Column metadata structures (ColumnInfo)
- Generated columns (STORED/VIRTUAL)
- IDENTITY columns
- Default values and expressions
- Column-level CHECK constraints
- Array columns
- Domain-based columns

### Out of Scope

- Table-level metadata (see `tables.md`)
- Data type system (see type specs)
- Physical column storage (see storage specs)

## Specification

### Generated Column Types

**Source:** `include/scratchbird/core/catalog_manager.h:599`

```cpp
enum class GeneratedColumnType : uint8_t {
    NOT_GENERATED = 0,  // Regular column
    STORED = 1,         // GENERATED ALWAYS AS ... STORED
    VIRTUAL = 2         // GENERATED ALWAYS AS ... VIRTUAL
};
```

**Generated Column Characteristics:**

| Type | Storage | Computation | Use Case |
|------|---------|-------------|----------|
| NOT_GENERATED | User provided | N/A | Standard columns |
| STORED | Computed on write | On INSERT/UPDATE | Cached computed values |
| VIRTUAL | Computed on read | On SELECT | Space-efficient computed values |

### ColumnInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:607`

```cpp
struct ColumnInfo {
    // Identity
    ID table_id;                    // Parent table
    ID column_id;                   // UUIDv7 column identifier
    std::string column_name;        // Column name (up to 128 chars)
    bool name_is_delimited = false; // Quoted identifier flag
    uint16_t ordinal = 0;           // Position in table (1-based)
    
    // Data type
    uint16_t data_type = 0;         // DataType enum
    uint32_t type_precision = 0;    // Precision/length
    uint32_t type_scale = 0;        // Scale for DECIMAL
    uint32_t max_length = 0;        // Legacy field
    
    // Nullability and defaults
    bool nullable = true;
    bool has_default = false;
    std::string default_value;      // Simple literal default
    std::string default_expr;       // DEFAULT expression (bytecode)
    ID default_value_oid{};         // TOAST reference for large defaults
    
    // Constraints
    bool is_primary_key = false;
    bool is_unique = false;
    bool is_foreign_key = false;
    std::string check_expr;         // CHECK constraint expression
    ID check_expr_oid{};            // TOAST reference for check expr
    
    // Generated columns
    bool is_generated = false;
    GeneratedColumnType generated_type = GeneratedColumnType::NOT_GENERATED;
    std::string generation_expression;  // SQL expression or bytecode
    ID generation_expr_oid{};       // TOAST reference for large expressions
    std::vector<uint16_t> dependent_columns;  // Columns this depends on
    
    // IDENTITY columns
    bool is_identity = false;       // Is this an IDENTITY column?
    bool identity_always = true;    // ALWAYS vs BY DEFAULT
    ID identity_sequence_id;        // Associated sequence ID
    
    // Storage
    uint8_t storage_type = 0;       // TOAST storage strategy
    // 0 = PLAIN (inline, uncompressed)
    // 1 = EXTENDED (inline or TOAST, compressed)
    // 2 = EXTERNAL (TOAST, uncompressed)
    // 3 = MAIN (inline, compress if too large)
    
    // Character and time properties
    bool with_timezone = false;     // TIMESTAMP WITH TIME ZONE
    uint16_t charset = 0;           // Character set (0 = inherit)
    ID charset_uuid{};              // Charset UUID reference
    uint16_t timezone_hint = 0;     // Timezone ID for display
    ID timezone_uuid{};             // Timezone UUID reference
    uint32_t collation_id = 0;      // Collation (0 = inherit)
    
    // Domain and array
    ID domain_id;                   // Domain ID (zero if not domain-based)
    bool is_array = false;          // Array column
    uint32_t array_size = 0;        // Fixed array size (0 = variable)
    
    // Metadata
    uint64_t created_time = 0;
};
```

**Field Descriptions:**

| Field | Type | Description |
|-------|------|-------------|
| table_id | ID | Parent table UUID |
| column_id | ID | Unique column UUID |
| column_name | string | Column identifier |
| ordinal | uint16 | 1-based position in table |
| data_type | uint16 | Type code from DataType enum |
| type_precision | uint32 | VARCHAR(n), DECIMAL(p,s) |
| type_scale | uint32 | DECIMAL(p,s) scale |
| nullable | bool | NULL allowed |
| has_default | bool | Has DEFAULT clause |
| is_primary_key | bool | Part of PRIMARY KEY |
| is_generated | bool | Computed column |
| storage_type | uint8 | TOAST strategy |
| domain_id | ID | Base domain (if domain-based) |
| is_array | bool | Array type |

### TOAST Storage Strategies

| Strategy | Code | Behavior |
|----------|------|----------|
| PLAIN | 0 | Inline only, no compression |
| EXTENDED | 1 | Inline or TOAST, compress |
| EXTERNAL | 2 | TOAST only, no compress |
| MAIN | 3 | Inline, compress if large |

**Default by Type:**
- INTEGER, BIGINT: PLAIN
- VARCHAR, TEXT: EXTENDED
| BYTEA: EXTENDED
- JSON, XML: EXTENDED

### IDENTITY Columns

```sql
-- IDENTITY ALWAYS: User cannot override
CREATE TABLE users (
    id INTEGER GENERATED ALWAYS AS IDENTITY,
    name VARCHAR(100)
);

-- IDENTITY BY DEFAULT: User can specify value
CREATE TABLE orders (
    order_id INTEGER GENERATED BY DEFAULT AS IDENTITY,
    customer_id INTEGER
);
```

**Implementation:**

```cpp
struct IdentityColumnInfo {
    ID column_id;              // Reference to column
    ID sequence_id;            // Associated sequence
    bool always;               // ALWAYS vs BY DEFAULT
    int64_t start_value;       // START WITH
    int64_t increment;         // INCREMENT BY
    int64_t min_value;         // MINVALUE
    int64_t max_value;         // MAXVALUE
    bool cycle;                // CYCLE vs NO CYCLE
    int64_t cache_size;        // CACHE
};
```

### sb_columns Catalog Table

**Source:** `src/core/catalog_manager.cpp:4917`

```cpp
struct ColumnRecord {
    // Primary key: (table_id, column_id)
    ID table_id;
    ID column_id;
    
    // Identity
    char column_name[512];
    uint16_t ordinal;
    uint8_t name_is_delimited;
    uint8_t reserved_1;
    
    // Type information
    uint16_t data_type;
    uint32_t type_precision;
    uint32_t type_scale;
    uint32_t max_length;
    ID domain_id;
    
    // Flags (packed)
    uint8_t is_array;
    uint8_t array_size_is_fixed;
    uint32_t array_size;
    uint8_t nullable;
    uint8_t has_default;
    uint8_t is_primary_key;
    uint8_t is_unique;
    uint8_t is_foreign_key;
    uint8_t is_generated;
    uint8_t generated_type;     // GeneratedColumnType
    uint8_t storage_type;
    uint8_t with_timezone;
    uint8_t is_identity;
    uint8_t identity_always;
    
    // Character/time properties
    ID charset_id;
    ID timezone_id;
    uint32_t collation_id;
    
    // Storage references
    char default_value[128];    // Inline small defaults
    ID default_value_oid;       // TOAST for large defaults
    ID check_expr_oid;          // TOAST for CHECK expression
    ID generation_expr_oid;     // TOAST for generation expr
    ID identity_sequence_id;    // Associated sequence
    
    // Generation dependencies
    uint16_t dependent_column_count;
    uint16_t dependent_columns[16];  // Ordinal positions
    
    // Metadata
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Default Value Handling

**Storage Tiers:**

| Size | Storage | Location |
|------|---------|----------|
| ≤ 128 bytes | Inline | ColumnRecord.default_value |
| > 128 bytes | TOAST | ColumnRecord.default_value_oid |

**Default Types:**

```cpp
enum class DefaultValueType : uint8_t {
    NULL_LITERAL = 0,      // DEFAULT NULL
    CONSTANT = 1,          // DEFAULT 'string' or 123
    EXPRESSION = 2,        // DEFAULT (expr) - bytecode
    SEQUENCE_NEXTVAL = 3,  // DEFAULT NEXTVAL('seq')
    CURRENT_TIMESTAMP = 4, // DEFAULT CURRENT_TIMESTAMP
    USER = 5,              // DEFAULT USER
    CURRENT_USER = 6,      // DEFAULT CURRENT_USER
    SESSION_USER = 7       // DEFAULT SESSION_USER
};
```

### Generated Column Dependencies

```
Column: total_price (GENERATED ALWAYS AS (qty * unit_price) STORED)

Dependencies:
├── dependent_columns = [2, 3]  // ordinals of qty, unit_price
├── generated_type = STORED
└── generation_expression = "qty * unit_price"

Validation Rules:
1. Dependent columns must exist in table
2. No circular dependencies allowed
3. Generated column cannot depend on another generated column
4. Type of expression must match declared column type
```

## Algorithms

### Algorithm: Add Column

```
Input:  Table ID, column definition
Output: Column ID

1. Validate column name uniqueness in table
2. Validate data type exists
3. If domain-based:
   a. Resolve domain_id
   b. Inherit domain constraints
4. If generated:
   a. Parse generation expression
   b. Identify dependent columns
   c. Check for circular dependencies
5. Generate UUIDv7 for column_id
6. Assign ordinal (max existing + 1)
7. If has DEFAULT:
   a. If expression > 128 bytes, store in TOAST
   b. Set default_value_oid or default_value
8. Create ColumnRecord
9. If IDENTITY:
   a. Create sequence
   b. Set identity_sequence_id
10. Update table column_count
11. Commit transaction
```

### Algorithm: Compute Generated Column

```
Input:  Table ID, column ordinal, row data
Output: Computed value

1. Look up ColumnInfo for column
2. Verify is_generated = true
3. Retrieve generation expression (from TOAST if needed)
4. Build evaluation context:
   a. For each dependent_column:
      - Get column value from row
      - Bind to expression parameter
5. Execute SBLR bytecode
6. Return result

For STORED:
- Store result in row buffer

For VIRTUAL:
- Return result without storing
```

### Algorithm: Validate IDENTITY Insert

```
Input:  Column info, proposed value
Output: Allow/Reject

1. If column.identity_always = true:
   a. If user provided value:
      - Reject with error: "cannot insert into identity column"
   b. Generate next sequence value
2. If column.identity_always = false:
   a. If user provided value:
      - Validate within domain
      - Allow insert
   b. Else:
      - Generate next sequence value
3. Return allowed value
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|--------------|
| `COL_INV_001` | column_id is valid UUIDv7 | isUuidV7Local() check |
| `COL_INV_002` | table_id references valid table | Foreign key check |
| `COL_INV_003` | ordinal is unique within table | Unique index |
| `COL_INV_004` | (table_id, column_name) is unique | Unique index |
| `COL_INV_005` | Generated columns have valid dependencies | Dependency check |
| `COL_INV_006` | IDENTITY columns have valid sequence | Sequence reference check |
| `COL_INV_007` | Domain-based columns reference valid domain | Foreign key check |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `COLUMN_EXISTS` | Name conflict in table | Choose different name |
| `INVALID_DATA_TYPE` | Unknown type code | Use valid type |
| `CIRCULAR_DEPENDENCY` | Generated column depends on itself | Fix expression |
| `IDENTITY_CONFLICT` | Multiple IDENTITY columns | Remove extra IDENTITY |
| `INVALID_DEFAULT` | Default value incompatible with type | Correct default |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_catalog_columns.cpp` | Column CRUD |
| `tests/unit/test_generated_columns.cpp` | GENERATED columns |
| `tests/unit/test_identity_columns.cpp` | IDENTITY columns |
| `tests/unit/test_column_defaults.cpp` | DEFAULT values |
| `tests/unit/test_domain_columns.cpp` | Domain-based columns |

## Related Specifications

- [tables.md](./tables.md) - Parent table metadata
- [domains.md](./domains.md) - Domain definitions
- [sequences.md](./sequences.md) - IDENTITY sequences
- [constraints.md](./constraints.md) - Column constraints

## Appendix

### Column Record Size

| Component | Size |
|-----------|------|
| Header | 48 bytes |
| Identity fields | 544 bytes |
| Type info | 16 bytes |
| Flags | 16 bytes |
| References | 112 bytes |
| Dependencies | 34 bytes |
| Metadata | 16 bytes |
| **Total** | **~786 bytes** |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
