# Binary Language Representation (BLR) Specification

## Overview

BLR (Binary Language Representation) is ScratchBird's intermediate representation for all database operations. It serves as a universal, pre-compiled format that:
- Eliminates repeated SQL parsing
- Enables cross-dialect compatibility
- Provides optimization opportunities
- Uses UUIDs for object references

## BLR Structure

### Version Header
```c
struct BLR_Header {
    uint8_t  blr_version;      // BLR version (5 for ScratchBird)
    uint8_t  blr_flags;         // Flags (debug, optimized, etc.)
    uint16_t blr_length;        // Total BLR length
    uint8_t  blr_checksum[4];   // CRC32 checksum
};
```

## Instruction Set

### Control Flow Instructions

```c
// Basic Structure
#define blr_version         5       // BLR version number
#define blr_begin           2       // Begin BLR stream
#define blr_end             255     // End BLR stream
#define blr_message         4       // Message declaration

// Conditionals
#define blr_if              6       // If statement
#define blr_then            7       // Then clause
#define blr_else            8       // Else clause
#define blr_endif           9       // End if

// Loops
#define blr_loop            10      // Loop start
#define blr_leave           11      // Exit loop
#define blr_continue        12      // Continue loop
#define blr_endloop         13      // End loop

// Labels and Jumps
#define blr_label           14      // Label definition
#define blr_goto            15      // Unconditional jump
#define blr_call            16      // Procedure call
#define blr_return          17      // Return from procedure
```

### Data Manipulation Instructions

```c
// DML Operations
#define blr_select          20      // SELECT statement
#define blr_insert          21      // INSERT statement
#define blr_update          22      // UPDATE statement
#define blr_delete          23      // DELETE statement
#define blr_merge           24      // MERGE statement

// Cursor Operations
#define blr_cursor          25      // Declare cursor
#define blr_open            26      // Open cursor
#define blr_fetch           27      // Fetch from cursor
#define blr_close           28      // Close cursor

// Transaction Control
#define blr_start_trans     30      // Start transaction
#define blr_commit          31      // Commit transaction
#define blr_rollback        32      // Rollback transaction
#define blr_savepoint       33      // Set savepoint
#define blr_release         34      // Release savepoint
```

### Expression Instructions

```c
// Arithmetic
#define blr_add             40      // Addition
#define blr_subtract        41      // Subtraction
#define blr_multiply        42      // Multiplication
#define blr_divide          43      // Division
#define blr_modulo          44      // Modulo
#define blr_negate          45      // Negation
#define blr_abs             46      // Absolute value

// Comparison
#define blr_eql             50      // Equal
#define blr_neq             51      // Not equal
#define blr_gtr             52      // Greater than
#define blr_geq             53      // Greater or equal
#define blr_lss             54      // Less than
#define blr_leq             55      // Less or equal
#define blr_between         56      // Between
#define blr_like            57      // Like pattern match
#define blr_containing      58      // Contains substring
#define blr_starting        59      // Starts with

// Logical
#define blr_and             60      // Logical AND
#define blr_or              61      // Logical OR
#define blr_not             62      // Logical NOT
#define blr_any             63      // ANY predicate
#define blr_all             64      // ALL predicate
#define blr_exists          65      // EXISTS predicate

// Null Handling
#define blr_missing         70      // IS NULL
#define blr_not_missing     71      // IS NOT NULL
#define blr_coalesce        72      // COALESCE
#define blr_nullif          73      // NULLIF
```

### Type Instructions

```c
// Type Definitions
#define blr_text            80      // Text type
#define blr_short           81      // Small integer (2 bytes)
#define blr_long            82      // Integer (4 bytes)
#define blr_int64           83      // Big integer (8 bytes)
#define blr_int128          84      // 128-bit integer
#define blr_float           85      // Float (4 bytes)
#define blr_double          86      // Double (8 bytes)
#define blr_decimal         87      // Decimal/Numeric
#define blr_date            88      // Date
#define blr_time            89      // Time
#define blr_timestamp       90      // Timestamp
#define blr_blob            91      // BLOB
#define blr_array           92      // Array
#define blr_uuid            93      // UUID
#define blr_boolean         94      // Boolean
#define blr_json            95      // JSON
#define blr_xml             96      // XML

// Type Modifiers
#define blr_varying         100     // Varying length
#define blr_nullable        101     // Nullable
#define blr_not_null        102     // Not null
#define blr_charset         103     // Character set
#define blr_collate         104     // Collation
```

### Object References

```c
// Object Types (using UUIDs)
#define blr_relation        110     // Table reference
#define blr_field           111     // Column reference
#define blr_index           112     // Index reference
#define blr_procedure       113     // Procedure reference
#define blr_function        114     // Function reference
#define blr_trigger         115     // Trigger reference
#define blr_sequence        116     // Sequence reference
#define blr_domain          117     // Domain reference
#define blr_schema          118     // Schema reference

// UUID Reference Format
struct BLR_UUID_Ref {
    uint8_t  ref_type;          // Object type (blr_relation, etc.)
    uint8_t  uuid[16];          // UUID v7 of object
    uint16_t alias_id;          // Alias ID for this query
};
```

### Aggregate Functions

```c
#define blr_count           120     // COUNT
#define blr_sum             121     // SUM
#define blr_avg             122     // AVG
#define blr_min             123     // MIN
#define blr_max             124     // MAX
#define blr_stddev          125     // Standard deviation
#define blr_variance        126     // Variance
#define blr_list            127     // LIST (Firebird-style)
#define blr_string_agg      128     // String aggregation
```

### Window Functions

```c
#define blr_window          130     // Window definition
#define blr_partition       131     // PARTITION BY
#define blr_order           132     // ORDER BY
#define blr_range           133     // Range frame
#define blr_rows            134     // Rows frame
#define blr_row_number      135     // ROW_NUMBER()
#define blr_rank            136     // RANK()
#define blr_dense_rank      137     // DENSE_RANK()
#define blr_lag             138     // LAG()
#define blr_lead            139     // LEAD()
#define blr_first_value     140     // FIRST_VALUE()
#define blr_last_value      141     // LAST_VALUE()
```

## Encoding Rules

### Basic Types

```c
// Integer encoding
void encode_integer(uint8_t* buffer, int64_t value, uint8_t type) {
    *buffer++ = type;
    switch(type) {
        case blr_short:
            encode_int16(buffer, value);
            break;
        case blr_long:
            encode_int32(buffer, value);
            break;
        case blr_int64:
            encode_int64(buffer, value);
            break;
        case blr_int128:
            encode_int128(buffer, value);
            break;
    }
}

// String encoding
void encode_string(uint8_t* buffer, const char* str, size_t len) {
    *buffer++ = blr_text;
    *buffer++ = blr_varying;
    encode_uint16(buffer, len);
    memcpy(buffer + 2, str, len);
}

// UUID encoding
void encode_uuid_ref(uint8_t* buffer, uint8_t type, const uint8_t uuid[16]) {
    *buffer++ = type;
    memcpy(buffer, uuid, 16);
}
```

### Complex Expressions

```c
// Binary operation: A + B
uint8_t expr[] = {
    blr_add,
    blr_field,
    // UUID of field A (16 bytes)
    blr_field,
    // UUID of field B (16 bytes)
};

// Conditional: IF A > 10 THEN B ELSE C
uint8_t cond[] = {
    blr_if,
        blr_gtr,
            blr_field,
            // UUID of field A
            blr_long,
            0x00, 0x00, 0x00, 0x0A,  // 10
    blr_then,
        blr_field,
        // UUID of field B
    blr_else,
        blr_field,
        // UUID of field C
    blr_endif
};
```

## Statement Examples

### SELECT Statement

```sql
-- SQL: SELECT id, name FROM users WHERE age > 18
```

```c
uint8_t select_blr[] = {
    blr_version, 5,
    blr_begin,
        blr_message, 0, 2,  // Output message with 2 fields
            blr_int64,       // id
            blr_text, blr_varying, 100,  // name
        
        blr_select,
            blr_relation,
            // UUID of 'users' table (16 bytes)
            
            // Field list
            2,  // Field count
            blr_field,
            // UUID of 'id' field
            blr_field,
            // UUID of 'name' field
            
            // WHERE clause
            blr_gtr,
                blr_field,
                // UUID of 'age' field
                blr_short,
                0x00, 0x12,  // 18
    blr_end
};
```

### INSERT Statement

```sql
-- SQL: INSERT INTO users (id, name, age) VALUES (?, ?, ?)
```

```c
uint8_t insert_blr[] = {
    blr_version, 5,
    blr_begin,
        blr_message, 1, 3,  // Input message with 3 parameters
            blr_int64,       // id parameter
            blr_text, blr_varying, 100,  // name parameter
            blr_short,       // age parameter
        
        blr_insert,
            blr_relation,
            // UUID of 'users' table
            
            // Field list
            3,  // Field count
            blr_field,
            // UUID of 'id' field
            blr_field,
            // UUID of 'name' field
            blr_field,
            // UUID of 'age' field
            
            // Values (from message 1)
            blr_message, 1,
    blr_end
};
```

### Stored Procedure

```sql
-- SQL: CREATE PROCEDURE get_user_count() RETURNS INTEGER
--      AS
--      BEGIN
--        RETURN (SELECT COUNT(*) FROM users);
--      END
```

```c
uint8_t proc_blr[] = {
    blr_version, 5,
    blr_begin,
        blr_message, 0, 1,  // Output message
            blr_long,        // Return value
        
        blr_procedure,
            // UUID of procedure
            
            blr_return,
                blr_select,
                    blr_relation,
                    // UUID of 'users' table
                    
                    blr_count,
                        blr_field,
                        // UUID of any field (for COUNT(*))
    blr_end
};
```

## Optimization Hints

```c
// Hints can be embedded in BLR
#define blr_hint            200     // Optimization hint
#define blr_hint_index      201     // Use specific index
#define blr_hint_no_index   202     // Don't use index
#define blr_hint_parallel   203     // Enable parallel execution
#define blr_hint_materialize 204    // Materialize subquery
#define blr_hint_no_cache   205     // Don't cache result

// Example with hint
uint8_t optimized_blr[] = {
    blr_select,
        blr_hint, blr_hint_index,
        // UUID of index to use
        
        blr_relation,
        // Rest of query...
};
```

## Execution Context

```c
struct BLR_Context {
    // Transaction context
    uint64_t transaction_id;
    uint8_t  isolation_level;
    
    // Schema context
    uint8_t  current_schema[16];  // UUID of current schema
    uint8_t  search_path[10][16]; // Schema search path
    
    // Security context
    uint8_t  user_id[16];         // UUID of current user
    uint8_t  role_id[16];         // UUID of active role
    
    // Execution parameters
    uint32_t max_rows;             // Row limit
    uint32_t timeout_ms;           // Query timeout
    uint8_t  flags;                // Execution flags
};
```

## Validation Rules

1. **Version Check**: First byte must be valid BLR version
2. **Structure Balance**: All begin/end pairs must match
3. **Type Consistency**: Operations must have compatible types
4. **UUID Validity**: All UUID references must exist
5. **Message Alignment**: Message definitions must match usage
6. **Checksum Verification**: Optional CRC32 validation

## Performance Considerations

1. **Caching**: BLR can be cached after generation
2. **Validation**: Validate once, execute many times
3. **Optimization**: Apply optimizations at BLR level
4. **Compilation**: BLR can be JIT compiled to native code
5. **Vectorization**: BLR operations can be vectorized

## Future Extensions

- Custom operators (blr_user_op)
- Machine learning operations
- Graph traversal operations
- Temporal operations
- Spatial operations
- Full-text search operations