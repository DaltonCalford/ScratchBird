# Phase 27: Universal Type System

## Objective
Implement a type system that supports all major database types with automatic translation.

## Prerequisites
- Phase 26 complete (UUID schema system)

## Tasks

### 27.1 Universal Type Registry
```cpp
enum BaseType {
    // Numeric
    INT8, INT16, INT32, INT64,
    UINT8, UINT16, UINT32, UINT64,
    FLOAT32, FLOAT64,
    DECIMAL,  // Arbitrary precision
    
    // String
    CHAR, VARCHAR, TEXT, BLOB,
    NCHAR, NVARCHAR,  // Unicode variants
    
    // Temporal
    DATE, TIME, TIMESTAMP,
    INTERVAL,
    
    // Complex
    UUID_TYPE, JSON, JSONB, XML,
    ARRAY, COMPOSITE,
    
    // Special
    BOOLEAN, BIT, BYTEA
};

struct UniversalType {
    BaseType base;
    int32_t length;        // For CHAR/VARCHAR
    int32_t precision;     // For DECIMAL
    int32_t scale;         // For DECIMAL
    BaseType element_type; // For ARRAY
    
    // Convert to database-specific type
    string to_mysql_type();
    string to_pg_type();
    string to_mssql_type();
};
```

### 27.2 Type Mapping Tables
```cpp
class TypeMapper {
    // MySQL types -> Universal
    UniversalType from_mysql(string mysql_type) {
        if (mysql_type == "TINYINT") return {INT8};
        if (mysql_type == "BIGINT") return {INT64};
        if (mysql_type == "VARCHAR(255)") return {VARCHAR, 255};
        if (mysql_type == "DATETIME") return {TIMESTAMP};
        // ... complete mapping
    }
    
    // PostgreSQL types -> Universal
    UniversalType from_pg(string pg_type) {
        if (pg_type == "smallint") return {INT16};
        if (pg_type == "bigint") return {INT64};
        if (pg_type == "varchar(255)") return {VARCHAR, 255};
        if (pg_type == "timestamp") return {TIMESTAMP};
        if (pg_type == "uuid") return {UUID_TYPE};
        if (pg_type == "jsonb") return {JSONB};
        // ... complete mapping
    }
};
```

### 27.3 Type Coercion Rules
```cpp
class TypeCoercion {
    bool can_coerce(UniversalType from, UniversalType to) {
        // INT8 -> INT16 -> INT32 -> INT64 (safe)
        // INT64 -> INT32 (requires check)
        // VARCHAR -> TEXT (safe)
        // TIMESTAMP -> DATE (loses time)
    }
    
    Value coerce(Value val, UniversalType from, UniversalType to) {
        // Perform conversion with overflow checking
    }
};
```

### 27.4 Database-Specific Features
```cpp
// MySQL AUTO_INCREMENT
struct AutoIncrement {
    UUID column_id;
    int64_t current_value;
    int64_t increment;
};

// PostgreSQL SERIAL
struct Serial {
    UUID sequence_id;
    UUID column_id;
};

// MSSQL IDENTITY
struct Identity {
    UUID column_id;
    int64_t seed;
    int64_t increment;
};

// Unified interface
class AutoNumbering {
    int64_t next_value(UUID column_id) {
        // Works for all auto-numbering types
    }
};
```

### 27.5 Special Type Handlers
```cpp
// JSON support across databases
class JSONHandler {
    Value parse_json(string json_text);
    string extract_path(Value json, string path);  // ->>, #>
    Value json_agg(vector<Value> rows);  // JSON_ARRAYAGG, json_agg
};

// Array support (PostgreSQL-style)
class ArrayHandler {
    Value create_array(vector<Value> elements);
    bool contains(Value array, Value element);  // @>
    Value unnest(Value array);
};

// UUID support
class UUIDHandler {
    UUID generate_v4();
    string to_string(UUID id);
    UUID from_string(string str);
};
```

## Files to Create
- `include/scratchbird/types/universal_type.h`
- `src/types/type_mapper.cpp`
- `src/types/type_coercion.cpp`
- `src/types/special_handlers.cpp`

## Validation Tests
```cpp
// MySQL client creates table
mysql_execute("CREATE TABLE users (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(255),
    created DATETIME
)");

// PostgreSQL client sees it correctly
pg_result = pg_execute("\\d users");
assert(pg_result.contains("id | bigint"));
assert(pg_result.contains("name | character varying(255)"));
assert(pg_result.contains("created | timestamp"));

// MSSQL client sees it correctly
mssql_result = mssql_execute("sp_columns users");
assert(mssql_result.contains("id | bigint | IDENTITY"));

// Type coercion works
execute("INSERT INTO users (name) VALUES ('test')");
auto id = execute("SELECT id FROM users").get<int32_t>();  // Coerced from INT64

// JSON works across all clients
mysql_execute("CREATE TABLE json_test (data JSON)");
pg_execute("INSERT INTO json_test VALUES ('{\"key\": \"value\"}'::jsonb)");
mssql_execute("SELECT JSON_VALUE(data, '$.key') FROM json_test");

// Arrays work (PostgreSQL feature on all databases)
execute("CREATE TABLE array_test (tags TEXT[])");
execute("INSERT INTO array_test VALUES (ARRAY['tag1', 'tag2'])");
execute("SELECT * FROM array_test WHERE tags @> ARRAY['tag1']");
```

## Exit Criteria
- All major database types supported
- Automatic type mapping between databases
- Type coercion works safely
- Special features (JSON, Arrays, UUID) work
- Each client sees native types