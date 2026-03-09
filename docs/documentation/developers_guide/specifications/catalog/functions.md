# Specification: Functions and Procedures

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11655`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11683`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:11635`

## Synopsis

This specification defines stored function and procedure metadata, including parameter modes, return types, SQL security modes, and SBLR bytecode storage.

## Scope

### In Scope

- Function metadata (FunctionInfo)
- Procedure metadata (ProcedureInfo)
- Parameter definitions (ParameterInfo)
- SQL security modes (DEFINER/INVOKER)
- SBLR bytecode storage
- Function/procedure dependencies

### Out of Scope

- SBLR bytecode format (see SBLR specs)
- PSQL parser (see parser specs)
- Function execution engine (see executor specs)

## Specification

### Parameter Modes

**Source:** `include/scratchbird/core/catalog_manager.h:11635`

```cpp
enum class ParameterMode : uint8_t {
    IN = 0,     // Input only
    OUT = 1,    // Output only
    INOUT = 2   // Both input and output
};
```

**Parameter Mode Characteristics:**

| Mode | Can Read | Can Write | Use Case |
|------|----------|-----------|----------|
| IN | Yes | No | Input values |
| OUT | No | Yes | Return values |
| INOUT | Yes | Yes | Pass by reference |

### SQL Security Modes

**Source:** `include/scratchbird/core/catalog_manager.h:11658`

```cpp
enum class SqlSecurity : uint8_t {
    DEFINER = 0,  // Execute with owner's privileges
    INVOKER = 1   // Execute with caller's privileges (default)
};
```

### ParameterInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11644`

```cpp
struct ParameterInfo {
    std::string name;               // Parameter name
    DataType type;                  // Data type
    uint32_t type_precision = 0;    // For VARCHAR, DECIMAL, etc.
    uint32_t type_scale = 0;        // For DECIMAL
    ParameterMode mode = ParameterMode::IN;
    bool has_default = false;       // Has DEFAULT value
    std::string default_value;      // Serialized default expression
};
```

### FunctionInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11655`

```cpp
struct FunctionInfo {
    // Identity
    ID function_id;                 // UUIDv7 function identifier
    ID schema_id;                   // Owning schema
    std::string name;               // Function name
    bool name_is_delimited = false; // Quoted identifier flag
    ID owner_id;                    // Owner UUID (Phase 3.1)
    
    // Parameters
    std::vector<ParameterInfo> parameters;
    
    // Return type
    DataType return_type = DataType::INT32;
    uint32_t return_type_precision = 0;
    uint32_t return_type_scale = 0;
    
    // Characteristics
    bool or_replace = false;        // CREATE OR REPLACE
    bool deterministic = false;     // DETERMINISTIC flag
    SqlSecurity sql_security = SqlSecurity::INVOKER;
    
    // Implementation
    std::vector<uint8_t> bytecode;  // Compiled SBLR bytecode
    std::string source_text;        // Original PSQL source
    
    // Dependencies
    std::vector<std::pair<ID, ObjectType>> referenced_objects;
    
    // Metadata
    uint64_t created_time = 0;
    uint64_t modified_time = 0;
};
```

### ProcedureInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:11683`

```cpp
struct ProcedureInfo {
    // Identity
    ID procedure_id;                // UUIDv7 procedure identifier
    ID schema_id;                   // Owning schema
    std::string name;               // Procedure name
    bool name_is_delimited = false; // Quoted identifier flag
    ID owner_id;                    // Owner UUID (Phase 3.1)
    
    // Parameters
    std::vector<ParameterInfo> parameters;
    
    // Characteristics
    bool or_replace = false;
    SqlSecurity sql_security = SqlSecurity::INVOKER;
    
    // Implementation
    std::vector<uint8_t> bytecode;  // Compiled SBLR bytecode
    std::string source_text;        // Original PSQL source
    
    // Dependencies
    std::vector<std::pair<ID, ObjectType>> referenced_objects;
    
    // Metadata
    uint64_t created_time = 0;
    uint64_t modified_time = 0;
};
```

### Function/Procedure SQL Syntax

```sql
-- Simple function
CREATE FUNCTION add_numbers(a INTEGER, b INTEGER)
RETURNS INTEGER
AS
BEGIN
    RETURN a + b;
END;

-- Function with defaults
CREATE FUNCTION get_customer_name(
    cust_id INTEGER,
    include_title BOOLEAN DEFAULT false
)
RETURNS VARCHAR(100)
AS
BEGIN
    -- Function body
END;

-- Procedure with OUT parameters
CREATE PROCEDURE get_order_stats(
    IN order_id INTEGER,
    OUT total_amount DECIMAL(10,2),
    OUT item_count INTEGER
)
AS
BEGIN
    -- Procedure body
END;

-- SQL SECURITY DEFINER
CREATE FUNCTION get_sensitive_data()
RETURNS TABLE (...)
SQL SECURITY DEFINER
AS
BEGIN
    -- Runs with definer's privileges
END;

-- DETERMINISTIC function
CREATE FUNCTION calculate_tax(amount DECIMAL(10,2))
RETURNS DECIMAL(10,2)
DETERMINISTIC
AS
BEGIN
    RETURN amount * 0.08;
END;

-- Drop
DROP FUNCTION add_numbers;
DROP FUNCTION IF EXISTS add_numbers;
DROP PROCEDURE get_order_stats;
```

### sb_functions Catalog Table

```cpp
struct FunctionRecord {
    // Primary key
    ID function_id;
    
    // Identity
    ID schema_id;
    char name[512];
    ID owner_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    
    // Return type
    uint16_t return_type;           // DataType
    uint32_t return_type_precision;
    uint32_t return_type_scale;
    
    // Characteristics
    uint8_t deterministic;
    uint8_t sql_security;           // SqlSecurity
    uint8_t reserved2[6];
    
    // Parameters (stored as JSON in TOAST)
    ID parameters_oid;              // TOAST reference
    uint16_t parameter_count;
    uint8_t reserved3[6];
    
    // Implementation
    ID source_text_oid;             // TOAST for PSQL source
    ID bytecode_oid;                // TOAST for SBLR bytecode
    
    // Dependencies
    ID dependencies_oid;            // TOAST for dependency list
    
    // Metadata
    uint64_t created_time;
    uint64_t modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### sb_procedures Catalog Table

```cpp
struct ProcedureRecord {
    // Primary key
    ID procedure_id;
    
    // Identity
    ID schema_id;
    char name[512];
    ID owner_id;
    uint8_t name_is_delimited;
    uint8_t reserved[7];
    
    // Characteristics
    uint8_t sql_security;           // SqlSecurity
    uint8_t reserved2[7];
    
    // Parameters
    ID parameters_oid;
    uint16_t parameter_count;
    uint8_t reserved3[6];
    
    // Implementation
    ID source_text_oid;
    ID bytecode_oid;
    
    // Dependencies
    ID dependencies_oid;
    
    // Metadata
    uint64_t created_time;
    uint64_t modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Function Overloading

```sql
-- Function overloading by parameter types
CREATE FUNCTION process_data(input INTEGER) ...
CREATE FUNCTION process_data(input VARCHAR) ...
CREATE FUNCTION process_data(input INTEGER, flag BOOLEAN) ...

-- Resolution based on argument types
SELECT process_data(123);       -- Calls INTEGER version
SELECT process_data('hello');   -- Calls VARCHAR version
SELECT process_data(123, true); -- Calls 2-param version
```

**Overload Resolution:**
1. Match by name
2. Match by parameter count
3. Match by parameter types (exact > implicit cast)
4. If multiple matches: ambiguous call error

## Algorithms

### Algorithm: Create Function

```
Input:  Schema ID, function name, parameters, return type,
        characteristics, source text
Output: Function ID

1. Validate function name unique in schema
   (or handle overloading if supported)
2. Parse PSQL source text
3. Compile to SBLR bytecode
4. Identify referenced objects (tables, functions, etc.)
5. Generate UUIDv7 for function_id
6. If source > 2KB: store in TOAST
7. If bytecode > 2KB: store in TOAST
8. Create FunctionRecord
9. Create dependency records
10. Commit transaction
```

### Algorithm: Execute Function

```
Input:  Function ID, arguments, execution context
Output: Return value

1. Look up FunctionInfo
2. Verify argument count matches parameter count
3. For each parameter:
   a. If IN or INOUT: bind argument value
   b. If OUT: allocate output slot
4. Determine execution privileges:
   a. If sql_security = DEFINER:
      - Use owner_id privileges
   b. If sql_security = INVOKER:
      - Use current user privileges
5. Load SBLR bytecode
6. Execute bytecode in VM
7. Collect OUT parameters
8. Return result
```

### Algorithm: Replace Function

```
Input:  Existing function ID, new source text
Output: Success/Failure

1. Verify OR REPLACE specified or function exists
2. Parse new PSQL source
3. Compile new SBLR bytecode
4. Compare signature (parameters + return type):
   a. If changed: update catalog
   b. If compatible: allow
   c. If incompatible: require explicit DROP first
5. Identify new referenced objects
6. Update dependencies
7. Invalidate dependent objects
8. Commit transaction
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `FUNC_INV_001` | function_id is valid UUIDv7 | isUuidV7Local() check |
| `FUNC_INV_002` | schema_id references valid schema | Foreign key |
| `FUNC_INV_003` | Signature unique in schema | Unique index |
| `FUNC_INV_004` | All referenced objects exist | Dependency check |
| `FUNC_INV_005` | Bytecode valid or source valid | Validation |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `FUNCTION_EXISTS` | Name+signature conflict | Use OR REPLACE or DROP |
| `INVALID_SIGNATURE` | Parameter/return mismatch | Correct signature |
| `COMPILATION_ERROR` | PSQL parse/compile error | Fix source code |
| `PRIVILEGE_ERROR` | Insufficient privileges | Grant access |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_functions.cpp` | Function CRUD |
| `tests/unit/test_procedures.cpp` | Procedure CRUD |
| `tests/unit/test_function_execution.cpp` | Execution |
| `tests/unit/test_function_overloading.cpp` | Overloading |

## Related Specifications

- [triggers.md](./triggers.md) - Trigger procedures
- [dependency_tracking.md](./dependency_tracking.md) - Function dependencies

## Appendix

### Function Record Size

| Component | Size |
|-----------|------|
| Header | 48 bytes |
| Identity | 544 bytes |
| Return type | 12 bytes |
| Characteristics | 8 bytes |
| References | 48 bytes |
| Metadata | 16 bytes |
| **Total** | **~676 bytes** |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
