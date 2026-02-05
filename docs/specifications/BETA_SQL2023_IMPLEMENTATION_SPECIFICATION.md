# BETA SQL:2023 Implementation Specification

**Document Version:** 1.0
**Date:** 2026-02-04
**Standard:** ISO/IEC 9075:2023 (SQL:2023)
**Related Documents:**
- Report 17: SQL:2023 Complete Feature Breakdown
- Report 16: SQL Standard Compliance Gap Analysis
- BETA_SQL_STANDARD_COMPLIANCE_SPECIFICATION.md

---

## Executive Summary

This specification defines the implementation of **SQL:2023** features in ScratchBird Beta. SQL:2023 introduces three major categories of enhancements:

1. **Property Graph Queries (SQL/PGQ)** - Entirely new Part 16
2. **JSON Enhancements** - Native JSON data type and advanced query features
3. **Core SQL Language Improvements** - Numeric literals, NULL handling, and minor enhancements

### Implementation Priority

**Recommended Phased Approach:**

- **Phase 1 (Beta 2.0):** JSON Enhancements - 6-8 weeks
- **Phase 2 (Beta 2.1):** Core SQL Language Enhancements - 2-3 weeks
- **Phase 3 (Beta 3.0+):** Property Graph Queries (SQL/PGQ) - 12-16 weeks (OPTIONAL)
- **Deferred:** SQL/MDA Multidimensional Arrays (niche use case)

**Total Effort (Phases 1-2):** 8-11 weeks

---

## Table of Contents

**PART I: JSON ENHANCEMENTS (SQL:2023)**
1. T801: Native JSON Data Type
2. T802: JSON Type with Unique Keys
3. T803: String-Based JSON (Backward Compatibility)
4. T840: Hexadecimal Literals in SQL/JSON Path
5. T860-T864: SQL/JSON Simplified Accessors
6. T865-T878: SQL/JSON Item Methods
7. T879-T882: JSON Comparison Operators

**PART II: CORE SQL LANGUAGE ENHANCEMENTS**
8. T661: Non-Decimal Integer Literals
9. T662: Underscores in Numeric Literals
10. F401: Enhanced NULL Handling in UNIQUE Constraints
11. T056: Multi-Character TRIM Functions
12. F868: Optional TABLE Keyword

**PART III: PROPERTY GRAPH QUERIES (SQL/PGQ)**
13. CREATE PROPERTY GRAPH Statement
14. GRAPH_TABLE Operator
15. Pattern Matching Syntax
16. Graph Query Execution Engine

**PART IV: SQL/MDA (DEFERRED)**
17. Multidimensional Arrays

---

# PART I: JSON ENHANCEMENTS (SQL:2023)

---

## 1. T801: Native JSON Data Type

### 1.1 Overview

**Feature:** Native JSON column type with optimized binary storage
**Priority:** 🟡 MEDIUM-HIGH
**Effort:** 2-3 weeks
**Dependencies:** None
**SQL:2023 Feature Code:** T801

**What's New:**
- SQL:2016 had JSON *functions* operating on VARCHAR/CLOB columns
- SQL:2023 introduces a true **JSON data type** with type safety and optimized storage

### 1.2 Syntax

```sql
-- Column definition
CREATE TABLE documents (
    doc_id INT PRIMARY KEY,
    metadata JSON,
    content JSON NOT NULL,
    settings JSON DEFAULT '{}'
);

-- Constraints
CREATE TABLE users (
    user_id INT PRIMARY KEY,
    profile JSON CHECK (profile IS JSON OBJECT)
);

-- Indexes
CREATE INDEX idx_metadata ON documents(metadata);
```

### 1.3 Parser Implementation

**File:** `src/parser/v2/sql_parser.y`

**Grammar Rules:**

```yacc
data_type:
    | JSON
    | JSON '(' WITH UNIQUE KEYS ')'
    ;

literal:
    | JSON string_literal
    | JSON '(' string_literal ')'
    | JSON '(' string_literal WITH UNIQUE KEYS ')'
    ;
```

**AST Node:**

```c
typedef enum {
    TYPE_JSON,
    TYPE_JSON_UNIQUE_KEYS
} JsonTypeFlags;

typedef struct JsonDataType {
    DataType base;
    JsonTypeFlags flags;
    bool with_unique_keys;
} JsonDataType;
```

**Parser Actions:**

```c
// File: src/parser/v2/semantic_actions.c

ASTNode* create_json_type(bool with_unique_keys) {
    JsonDataType* type = malloc(sizeof(JsonDataType));
    type->base.type_id = TYPE_ID_JSON;
    type->base.is_nullable = true;
    type->flags = TYPE_JSON;
    type->with_unique_keys = with_unique_keys;
    return (ASTNode*)type;
}
```

### 1.4 Catalog Schema

**System Table:** `RDB$JSON_TYPES`

```sql
CREATE TABLE RDB$JSON_TYPES (
    RELATION_ID INTEGER NOT NULL,
    FIELD_ID INTEGER NOT NULL,
    FIELD_NAME VARCHAR(63) NOT NULL,
    STORAGE_FORMAT CHAR(10) NOT NULL,  -- 'BINARY', 'TEXT'
    WITH_UNIQUE_KEYS BOOLEAN DEFAULT FALSE,
    COMPRESSION_ENABLED BOOLEAN DEFAULT TRUE,
    CREATED_AT TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT PK_JSON_TYPES PRIMARY KEY (RELATION_ID, FIELD_ID)
);
```

**Metadata Queries:**

```c
// File: src/catalog/json_catalog.c

typedef struct JsonColumnMeta {
    int relation_id;
    int field_id;
    char field_name[64];
    JsonStorageFormat format;  // BINARY or TEXT
    bool with_unique_keys;
    bool compression_enabled;
} JsonColumnMeta;

// Query JSON column metadata
JsonColumnMeta* get_json_column_meta(int relation_id, int field_id);

// Register JSON column
int register_json_column(JsonColumnMeta* meta);
```

### 1.5 Storage Format

**Binary JSON Storage (JSONB-style):**

```c
// File: src/storage/json_storage.h

typedef enum {
    JSON_TYPE_NULL = 0x00,
    JSON_TYPE_TRUE = 0x01,
    JSON_TYPE_FALSE = 0x02,
    JSON_TYPE_NUMBER = 0x03,
    JSON_TYPE_STRING = 0x04,
    JSON_TYPE_ARRAY = 0x05,
    JSON_TYPE_OBJECT = 0x06
} JsonbType;

typedef struct JsonbHeader {
    uint32_t version;           // Format version (1)
    uint32_t total_size;        // Total size in bytes
    uint32_t num_elements;      // For arrays/objects
    JsonbType root_type;        // Root value type
    uint8_t flags;              // UNIQUE_KEYS, COMPRESSED, etc.
} JsonbHeader;

typedef struct JsonbValue {
    JsonbType type;
    union {
        int64_t int_val;
        double float_val;
        struct {
            uint32_t offset;
            uint32_t length;
        } string_val;
        struct {
            uint32_t num_elements;
            uint32_t elements_offset;
        } container_val;
    } data;
} JsonbValue;
```

**Encoding Functions:**

```c
// File: src/storage/json_storage.c

// Convert JSON string to binary JSONB
JsonbValue* json_string_to_jsonb(const char* json_str, size_t len,
                                  bool with_unique_keys, int* error);

// Convert binary JSONB to JSON string
char* jsonb_to_string(JsonbValue* jsonb, size_t* out_len);

// Validate JSON structure
bool jsonb_validate(JsonbValue* jsonb, bool require_unique_keys);

// Check for duplicate keys
bool jsonb_has_unique_keys(JsonbValue* jsonb);
```

### 1.6 Type System Integration

**File:** `src/types/type_system.c`

```c
// Register JSON type
void register_json_type() {
    TypeDescriptor json_type = {
        .type_id = TYPE_ID_JSON,
        .type_name = "JSON",
        .is_nullable = true,
        .storage_size = -1,  // Variable length
        .alignment = 4,
        .compare_fn = json_compare,
        .hash_fn = json_hash,
        .serialize_fn = json_serialize,
        .deserialize_fn = json_deserialize,
        .cast_fn = json_cast,
        .validate_fn = json_validate
    };

    register_type(&json_type);
}

// JSON type operations
int json_compare(void* a, void* b);
uint64_t json_hash(void* value);
void* json_serialize(void* value, size_t* out_len);
void* json_deserialize(const void* data, size_t len);
bool json_cast(void* src, DataType* src_type, void* dst, DataType* dst_type);
bool json_validate(void* value);
```

### 1.7 Executor Implementation

**File:** `src/executor/json_ops.c`

```c
// JSON column write
void write_json_column(RecordBuffer* record, int field_idx,
                       const char* json_str, bool with_unique_keys) {
    // Parse and validate JSON
    JsonbValue* jsonb = json_string_to_jsonb(json_str, strlen(json_str),
                                              with_unique_keys, NULL);
    if (!jsonb) {
        throw_error(ERROR_INVALID_JSON, "Invalid JSON value");
    }

    // Check unique keys if required
    if (with_unique_keys && !jsonb_has_unique_keys(jsonb)) {
        throw_error(ERROR_DUPLICATE_JSON_KEY, "Duplicate keys in JSON object");
    }

    // Store binary JSONB
    set_field_value(record, field_idx, jsonb, sizeof(JsonbValue));
    free(jsonb);
}

// JSON column read
char* read_json_column(RecordBuffer* record, int field_idx) {
    JsonbValue* jsonb = get_field_value(record, field_idx);
    if (!jsonb) return NULL;

    size_t json_len;
    char* json_str = jsonb_to_string(jsonb, &json_len);
    return json_str;
}
```

### 1.8 Index Support

**B-Tree Index on JSON:**

```c
// File: src/index/json_index.c

// JSON indexing uses hash of binary JSONB
typedef struct JsonIndexKey {
    uint64_t hash;
    uint32_t jsonb_size;
    JsonbValue jsonb;
} JsonIndexKey;

// Create index key from JSON value
JsonIndexKey* create_json_index_key(JsonbValue* jsonb) {
    JsonIndexKey* key = malloc(sizeof(JsonIndexKey));
    key->hash = json_hash(jsonb);
    key->jsonb_size = jsonb->header.total_size;
    memcpy(&key->jsonb, jsonb, jsonb->header.total_size);
    return key;
}

// Index operations
void json_index_insert(BTreeIndex* index, JsonbValue* jsonb, RecordID rid);
RecordID* json_index_lookup(BTreeIndex* index, JsonbValue* jsonb);
```

### 1.9 SQL Functions

**JSON_SERIALIZE:**

```sql
-- Convert JSON to string
SELECT JSON_SERIALIZE(metadata) AS json_string
FROM documents;
```

**Implementation:**

```c
// File: src/functions/json_functions.c

char* json_serialize_func(JsonbValue* jsonb) {
    size_t len;
    return jsonb_to_string(jsonb, &len);
}
```

**JSON_SCALAR:**

```sql
-- Create JSON scalar values
SELECT JSON_SCALAR('Hello') AS str;  -- "Hello"
SELECT JSON_SCALAR(42) AS num;       -- 42
SELECT JSON_SCALAR(TRUE) AS bool;    -- true
```

**Implementation:**

```c
JsonbValue* json_scalar_string(const char* str) {
    JsonbValue* jsonb = alloc_jsonb(JSON_TYPE_STRING);
    jsonb->data.string_val = copy_string(str);
    return jsonb;
}

JsonbValue* json_scalar_number(double num) {
    JsonbValue* jsonb = alloc_jsonb(JSON_TYPE_NUMBER);
    jsonb->data.float_val = num;
    return jsonb;
}

JsonbValue* json_scalar_boolean(bool val) {
    JsonbValue* jsonb = alloc_jsonb(val ? JSON_TYPE_TRUE : JSON_TYPE_FALSE);
    return jsonb;
}
```

### 1.10 IS JSON Predicate

**Syntax:**

```sql
-- Check if value is valid JSON
WHERE content IS JSON
WHERE content IS JSON OBJECT
WHERE content IS JSON ARRAY
WHERE content IS JSON SCALAR
WHERE content IS NOT JSON
```

**Parser Grammar:**

```yacc
predicate:
    | expression IS JSON json_type_opt
    | expression IS NOT JSON json_type_opt
    ;

json_type_opt:
    | /* empty */
    | OBJECT
    | ARRAY
    | SCALAR
    ;
```

**Executor Implementation:**

```c
bool eval_is_json_predicate(JsonbValue* jsonb, JsonTypeConstraint constraint) {
    if (!jsonb) return false;

    switch (constraint) {
        case JSON_ANY:
            return jsonb_validate(jsonb, false);
        case JSON_OBJECT:
            return jsonb->type == JSON_TYPE_OBJECT;
        case JSON_ARRAY:
            return jsonb->type == JSON_TYPE_ARRAY;
        case JSON_SCALAR:
            return jsonb->type == JSON_TYPE_NUMBER ||
                   jsonb->type == JSON_TYPE_STRING ||
                   jsonb->type == JSON_TYPE_TRUE ||
                   jsonb->type == JSON_TYPE_FALSE ||
                   jsonb->type == JSON_TYPE_NULL;
        default:
            return false;
    }
}
```

### 1.11 Testing Requirements

**Test File:** `tests/json/test_json_type.sql`

```sql
-- Test 1: JSON column creation
CREATE TABLE test_json (
    id INT PRIMARY KEY,
    data JSON
);

-- Test 2: Insert JSON values
INSERT INTO test_json VALUES (1, '{"name": "Alice", "age": 30}');
INSERT INTO test_json VALUES (2, '[1, 2, 3]');
INSERT INTO test_json VALUES (3, '"Hello World"');
INSERT INTO test_json VALUES (4, '42');
INSERT INTO test_json VALUES (5, 'true');
INSERT INTO test_json VALUES (6, 'null');

-- Test 3: Invalid JSON (should fail)
INSERT INTO test_json VALUES (7, '{invalid json}');  -- ERROR

-- Test 4: IS JSON predicate
SELECT id FROM test_json WHERE data IS JSON OBJECT;  -- Returns: 1
SELECT id FROM test_json WHERE data IS JSON ARRAY;   -- Returns: 2
SELECT id FROM test_json WHERE data IS JSON SCALAR;  -- Returns: 3,4,5,6

-- Test 5: JSON_SERIALIZE
SELECT id, JSON_SERIALIZE(data) FROM test_json;

-- Test 6: Index on JSON column
CREATE INDEX idx_data ON test_json(data);
SELECT id FROM test_json WHERE data = '{"name": "Alice", "age": 30}';

-- Test 7: NULL handling
INSERT INTO test_json VALUES (8, NULL);
SELECT id FROM test_json WHERE data IS NULL;  -- Returns: 8

-- Test 8: Constraints
CREATE TABLE test_json_constraint (
    id INT PRIMARY KEY,
    obj JSON CHECK (obj IS JSON OBJECT)
);

INSERT INTO test_json_constraint VALUES (1, '{}');  -- OK
INSERT INTO test_json_constraint VALUES (2, '[]');  -- ERROR: Not an object
```

**C Unit Tests:** `tests/unit/test_json_storage.c`

```c
void test_jsonb_encoding() {
    // Test binary encoding
    const char* json_str = "{\"name\":\"Alice\",\"age\":30}";
    JsonbValue* jsonb = json_string_to_jsonb(json_str, strlen(json_str), false, NULL);
    assert(jsonb != NULL);
    assert(jsonb->type == JSON_TYPE_OBJECT);

    // Test round-trip
    char* json_out = jsonb_to_string(jsonb, NULL);
    assert(strcmp(json_str, json_out) == 0);

    free(jsonb);
    free(json_out);
}

void test_unique_keys_validation() {
    const char* invalid = "{\"name\":\"Alice\",\"name\":\"Bob\"}";
    JsonbValue* jsonb = json_string_to_jsonb(invalid, strlen(invalid), true, NULL);
    assert(jsonb == NULL);  // Should fail with duplicate keys
}
```

### 1.12 Migration from VARCHAR-based JSON

**Migration Strategy:**

```sql
-- Convert existing VARCHAR columns to JSON
ALTER TABLE old_documents
    ALTER COLUMN metadata TYPE JSON USING metadata::JSON;

-- Validate existing data
UPDATE old_documents
SET metadata = NULL
WHERE metadata IS NOT NULL AND metadata IS NOT JSON;
```

---

## 2. T802: JSON Type with Unique Keys

### 2.1 Overview

**Feature:** Enforce unique keys within JSON objects
**Priority:** 🟢 LOW
**Effort:** 1 week
**Dependencies:** T801 (JSON Data Type)
**SQL:2023 Feature Code:** T802

### 2.2 Syntax

```sql
-- Enforce unique keys in JSON literal
INSERT INTO documents (metadata)
VALUES (JSON('{"name": "Alice", "age": 30}' WITH UNIQUE KEYS));

-- Fails if duplicate keys exist
INSERT INTO documents (metadata)
VALUES (JSON('{"name": "Alice", "name": "Bob"}' WITH UNIQUE KEYS));
-- ERROR: Duplicate key "name" in JSON object

-- Column-level constraint
CREATE TABLE strict_json (
    data JSON WITH UNIQUE KEYS
);
```

### 2.3 Parser Implementation

**Grammar:**

```yacc
json_literal:
    | JSON '(' string_literal ')'
    | JSON '(' string_literal WITH UNIQUE KEYS ')'
    ;
```

**AST Node:**

```c
typedef struct JsonLiteral {
    ASTNode base;
    char* json_string;
    bool with_unique_keys;
} JsonLiteral;
```

### 2.4 Validation Implementation

**File:** `src/storage/json_validation.c`

```c
// Check for duplicate keys in JSON object
bool jsonb_has_duplicate_keys(JsonbValue* obj) {
    if (obj->type != JSON_TYPE_OBJECT) return false;

    // Use hash table to track seen keys
    HashTable* seen_keys = create_hash_table(obj->data.container_val.num_elements);

    for (uint32_t i = 0; i < obj->data.container_val.num_elements; i++) {
        JsonbValue* key = get_json_object_key(obj, i);
        if (hash_table_contains(seen_keys, key->data.string_val.offset)) {
            destroy_hash_table(seen_keys);
            return true;  // Duplicate found
        }
        hash_table_insert(seen_keys, key->data.string_val.offset);
    }

    destroy_hash_table(seen_keys);
    return false;  // No duplicates
}

// Validate JSON with unique key requirement
bool validate_json_unique_keys(const char* json_str) {
    JsonbValue* jsonb = json_string_to_jsonb(json_str, strlen(json_str), false, NULL);
    if (!jsonb) return false;

    bool has_duplicates = jsonb_has_duplicate_keys(jsonb);
    free(jsonb);

    return !has_duplicates;
}
```

### 2.5 Testing

```sql
-- Test 1: Valid JSON with unique keys
SELECT JSON('{"a": 1, "b": 2}' WITH UNIQUE KEYS);  -- OK

-- Test 2: Invalid JSON with duplicate keys
SELECT JSON('{"a": 1, "a": 2}' WITH UNIQUE KEYS);  -- ERROR

-- Test 3: Nested objects
SELECT JSON('{"obj": {"a": 1, "a": 2}}' WITH UNIQUE KEYS);  -- ERROR (nested duplicate)

-- Test 4: Arrays are not checked
SELECT JSON('[{"a": 1}, {"a": 2}]' WITH UNIQUE KEYS);  -- OK (different objects)
```

---

## 3. T860-T864: SQL/JSON Simplified Accessors

### 3.1 Overview

**Feature:** Access JSON fields using dot notation and array subscripts
**Priority:** 🟡 MEDIUM-HIGH
**Effort:** 2-3 weeks
**Dependencies:** T801 (JSON Data Type)
**SQL:2023 Feature Codes:** T860, T861, T862, T863, T864

**Features:**
- **T860:** Dot notation for JSON objects (`data.customer.name`)
- **T861:** Array subscripts (`data.items[0]`)
- **T862:** Chained accessors (`data.customer.addresses[0].city`)
- **T863:** NULL handling (SQL NULL vs JSON null)
- **T864:** Array slicing (`data.items[1:3]`)

### 3.2 Syntax

```sql
-- T860: Object field access
SELECT customer_data.name,
       customer_data.email
FROM customers;

-- T861: Array element access
SELECT order_data.items[0].product_name,
       order_data.items[0].price
FROM orders;

-- T862: Nested access
SELECT user_data.profile.address.street,
       user_data.profile.address.city,
       user_data.preferences.notifications[0].type
FROM users;

-- T864: Array slicing (optional)
SELECT order_data.items[0:2]  -- First 3 items
FROM orders;

-- Equivalent to JSON_VALUE
data.customer.name  ≡  JSON_VALUE(data, '$.customer.name')
```

### 3.3 Parser Implementation

**Grammar Extensions:**

```yacc
postfix_expression:
    | primary_expression
    | postfix_expression '.' IDENTIFIER          /* T860: Dot accessor */
    | postfix_expression '[' expression ']'      /* T861: Array subscript */
    | postfix_expression '[' expression ':' expression ']'  /* T864: Slice */
    ;

primary_expression:
    | IDENTIFIER
    | column_reference
    | literal
    | '(' expression ')'
    ;
```

**AST Nodes:**

```c
typedef enum {
    JSON_ACCESS_FIELD,      // .field
    JSON_ACCESS_SUBSCRIPT,  // [index]
    JSON_ACCESS_SLICE       // [start:end]
} JsonAccessType;

typedef struct JsonAccessor {
    ASTNode base;
    ASTNode* object;        // Left-hand side
    JsonAccessType type;
    union {
        char* field_name;   // For FIELD access
        ASTNode* index;     // For SUBSCRIPT access
        struct {
            ASTNode* start;
            ASTNode* end;
        } slice;            // For SLICE access
    } accessor;
} JsonAccessor;
```

**Parser Actions:**

```c
// File: src/parser/v2/json_accessor.c

ASTNode* create_json_field_accessor(ASTNode* object, const char* field_name) {
    JsonAccessor* accessor = malloc(sizeof(JsonAccessor));
    accessor->base.type = AST_JSON_ACCESSOR;
    accessor->object = object;
    accessor->type = JSON_ACCESS_FIELD;
    accessor->accessor.field_name = strdup(field_name);
    return (ASTNode*)accessor;
}

ASTNode* create_json_subscript_accessor(ASTNode* object, ASTNode* index) {
    JsonAccessor* accessor = malloc(sizeof(JsonAccessor));
    accessor->base.type = AST_JSON_ACCESSOR;
    accessor->object = object;
    accessor->type = JSON_ACCESS_SUBSCRIPT;
    accessor->accessor.index = index;
    return (ASTNode*)accessor;
}

ASTNode* create_json_slice_accessor(ASTNode* object, ASTNode* start, ASTNode* end) {
    JsonAccessor* accessor = malloc(sizeof(JsonAccessor));
    accessor->base.type = AST_JSON_ACCESSOR;
    accessor->object = object;
    accessor->type = JSON_ACCESS_SLICE;
    accessor->accessor.slice.start = start;
    accessor->accessor.slice.end = end;
    return (ASTNode*)accessor;
}
```

### 3.4 Semantic Analysis

**File:** `src/parser/v2/semantic_analyzer.c`

```c
// Type checking for JSON accessors
DataType* analyze_json_accessor(SemanticContext* ctx, JsonAccessor* accessor) {
    // Analyze left-hand side
    DataType* lhs_type = analyze_expression(ctx, accessor->object);

    // Verify LHS is JSON type
    if (lhs_type->type_id != TYPE_ID_JSON) {
        semantic_error(ctx, "Cannot use accessor on non-JSON type");
        return NULL;
    }

    // JSON accessors always return JSON (may contain scalar)
    return create_json_type(false);
}
```

### 3.5 Executor Implementation

**File:** `src/executor/json_accessor.c`

```c
// Execute JSON field accessor
JsonbValue* exec_json_field_accessor(JsonbValue* obj, const char* field_name) {
    if (!obj || obj->type != JSON_TYPE_OBJECT) {
        return create_json_null();  // Return JSON null if not an object
    }

    // Linear search for field (could be optimized with hash table)
    for (uint32_t i = 0; i < obj->data.container_val.num_elements; i++) {
        JsonbValue* key = get_json_object_key(obj, i);
        if (strcmp(key->data.string_val.offset, field_name) == 0) {
            return get_json_object_value(obj, i);
        }
    }

    return create_json_null();  // Field not found
}

// Execute JSON array subscript
JsonbValue* exec_json_subscript(JsonbValue* arr, int64_t index) {
    if (!arr || arr->type != JSON_TYPE_ARRAY) {
        return create_json_null();
    }

    int64_t num_elements = arr->data.container_val.num_elements;

    // Negative indexing (Python-style)
    if (index < 0) {
        index = num_elements + index;
    }

    // Bounds check
    if (index < 0 || index >= num_elements) {
        return create_json_null();  // Out of bounds
    }

    return get_json_array_element(arr, index);
}

// Execute JSON array slice
JsonbValue* exec_json_slice(JsonbValue* arr, int64_t start, int64_t end) {
    if (!arr || arr->type != JSON_TYPE_ARRAY) {
        return create_json_array(0);  // Return empty array
    }

    int64_t num_elements = arr->data.container_val.num_elements;

    // Normalize indices
    if (start < 0) start = num_elements + start;
    if (end < 0) end = num_elements + end;
    if (start < 0) start = 0;
    if (end > num_elements) end = num_elements;
    if (start > end) start = end;

    // Build result array
    JsonbValue* result = create_json_array(end - start);
    for (int64_t i = start; i < end; i++) {
        JsonbValue* elem = get_json_array_element(arr, i);
        add_json_array_element(result, elem);
    }

    return result;
}

// Execute chained JSON accessor
JsonbValue* exec_json_accessor(ExecutionContext* ctx, JsonAccessor* accessor) {
    // Evaluate left-hand side
    JsonbValue* obj = eval_expression(ctx, accessor->object);
    if (!obj) return NULL;

    // Apply accessor
    switch (accessor->type) {
        case JSON_ACCESS_FIELD:
            return exec_json_field_accessor(obj, accessor->accessor.field_name);

        case JSON_ACCESS_SUBSCRIPT: {
            int64_t index = eval_integer_expression(ctx, accessor->accessor.index);
            return exec_json_subscript(obj, index);
        }

        case JSON_ACCESS_SLICE: {
            int64_t start = eval_integer_expression(ctx, accessor->accessor.slice.start);
            int64_t end = eval_integer_expression(ctx, accessor->accessor.slice.end);
            return exec_json_slice(obj, start, end);
        }

        default:
            return create_json_null();
    }
}
```

### 3.6 NULL Handling (T863)

**SQL NULL vs JSON null:**

```sql
-- SQL NULL: Column value is NULL
SELECT data FROM test WHERE data IS NULL;

-- JSON null: JSON contains literal null value
SELECT data FROM test WHERE data = 'null'::JSON;

-- Accessing missing field returns JSON null (not SQL NULL)
SELECT data.missing_field FROM test;  -- Returns: null (JSON)
```

**Implementation:**

```c
// Distinguish SQL NULL from JSON null
bool is_sql_null(JsonbValue* value) {
    return value == NULL;
}

bool is_json_null(JsonbValue* value) {
    return value != NULL && value->type == JSON_TYPE_NULL;
}

// Convert JSON null to SQL NULL if needed
void* json_to_sql_value(JsonbValue* json, DataType* target_type) {
    if (!json) return NULL;  // SQL NULL
    if (json->type == JSON_TYPE_NULL) return NULL;  // JSON null → SQL NULL

    // Convert based on target type
    switch (target_type->type_id) {
        case TYPE_ID_INTEGER:
            return json_to_integer(json);
        case TYPE_ID_VARCHAR:
            return json_to_string(json);
        // ... other types
    }
}
```

### 3.7 Optimization

**Path Compilation:**

```c
// Compile JSON path at plan time (instead of runtime)
typedef struct JsonPathPlan {
    int num_steps;
    JsonAccessStep* steps;
} JsonPathPlan;

typedef struct JsonAccessStep {
    JsonAccessType type;
    union {
        char* field_name;
        int64_t index;
        struct {
            int64_t start;
            int64_t end;
        } slice;
    } accessor;
} JsonAccessStep;

// Compile accessor chain to path plan
JsonPathPlan* compile_json_path(JsonAccessor* accessor) {
    // Flatten nested accessors into linear path
    JsonPathPlan* plan = malloc(sizeof(JsonPathPlan));
    plan->num_steps = count_accessor_depth(accessor);
    plan->steps = malloc(sizeof(JsonAccessStep) * plan->num_steps);

    // Fill steps
    flatten_accessor_chain(accessor, plan->steps);

    return plan;
}

// Execute compiled path (optimized)
JsonbValue* exec_json_path(JsonbValue* root, JsonPathPlan* plan) {
    JsonbValue* current = root;

    for (int i = 0; i < plan->num_steps; i++) {
        JsonAccessStep* step = &plan->steps[i];

        switch (step->type) {
            case JSON_ACCESS_FIELD:
                current = exec_json_field_accessor(current, step->accessor.field_name);
                break;
            case JSON_ACCESS_SUBSCRIPT:
                current = exec_json_subscript(current, step->accessor.index);
                break;
            case JSON_ACCESS_SLICE:
                current = exec_json_slice(current, step->accessor.slice.start,
                                          step->accessor.slice.end);
                break;
        }

        if (!current) break;
    }

    return current;
}
```

### 3.8 Testing

**Test File:** `tests/json/test_simplified_accessors.sql`

```sql
-- Setup test data
CREATE TABLE test_accessors (
    id INT PRIMARY KEY,
    data JSON
);

INSERT INTO test_accessors VALUES
(1, '{"name": "Alice", "age": 30, "address": {"city": "NYC", "zip": "10001"}}'),
(2, '{"items": [{"id": 1, "name": "Widget"}, {"id": 2, "name": "Gadget"}]}'),
(3, '{"matrix": [[1,2,3], [4,5,6], [7,8,9]]}');

-- Test 1: Simple field access (T860)
SELECT data.name, data.age FROM test_accessors WHERE id = 1;
-- Expected: name='Alice', age=30

-- Test 2: Nested field access (T860+T862)
SELECT data.address.city, data.address.zip FROM test_accessors WHERE id = 1;
-- Expected: city='NYC', zip='10001'

-- Test 3: Array subscript (T861)
SELECT data.items[0].name FROM test_accessors WHERE id = 2;
-- Expected: 'Widget'

SELECT data.items[1].id FROM test_accessors WHERE id = 2;
-- Expected: 2

-- Test 4: Nested array access (T861+T862)
SELECT data.matrix[0][1] FROM test_accessors WHERE id = 3;
-- Expected: 2

SELECT data.matrix[2][2] FROM test_accessors WHERE id = 3;
-- Expected: 9

-- Test 5: Array slicing (T864)
SELECT data.items[0:1] FROM test_accessors WHERE id = 2;
-- Expected: [{"id": 1, "name": "Widget"}, {"id": 2, "name": "Gadget"}]

-- Test 6: Missing field (returns JSON null)
SELECT data.missing FROM test_accessors WHERE id = 1;
-- Expected: null (JSON null, not SQL NULL)

-- Test 7: Out of bounds array access
SELECT data.items[99] FROM test_accessors WHERE id = 2;
-- Expected: null (JSON null)

-- Test 8: Type mismatch (accessing object as array)
SELECT data.name[0] FROM test_accessors WHERE id = 1;
-- Expected: null (name is string, not array)

-- Test 9: Comparison with JSON_VALUE
SELECT
    data.address.city AS simplified,
    JSON_VALUE(data, '$.address.city') AS json_value
FROM test_accessors WHERE id = 1;
-- Expected: Both return 'NYC'
```

---

## 4. T865-T878: SQL/JSON Item Methods

### 4.1 Overview

**Feature:** Type conversion methods within JSON path expressions
**Priority:** 🟢 MEDIUM
**Effort:** 1-2 weeks
**Dependencies:** T801, T860-T864
**SQL:2023 Feature Codes:** T865-T878

**New Methods:**
- `.string()` - Convert to string
- `.number()` - Convert to number
- `.boolean()` - Convert to boolean
- `.date()` - Convert to SQL DATE
- `.time()` - Convert to SQL TIME
- `.timestamp()` - Convert to SQL TIMESTAMP
- `.bigint()` - Convert to BIGINT
- `.integer()` - Convert to INTEGER
- `.double()` - Convert to DOUBLE PRECISION

### 4.2 Syntax

```sql
-- Type conversion in JSON path
SELECT JSON_VALUE(data, '$.price.number()' RETURNING DECIMAL(10,2)),
       JSON_VALUE(data, '$.created_at.timestamp()' RETURNING TIMESTAMP),
       JSON_VALUE(data, '$.active.boolean()' RETURNING BOOLEAN)
FROM products;

-- With simplified accessors (combined)
SELECT data.price.number() AS price,
       data.created_at.timestamp() AS created,
       data.active.boolean() AS is_active
FROM products;
```

### 4.3 Parser Implementation

**Grammar Extension:**

```yacc
postfix_expression:
    | postfix_expression '.' IDENTIFIER '(' ')'   /* Method call */
    ;

json_item_method:
    | STRING
    | NUMBER
    | BOOLEAN
    | DATE
    | TIME
    | TIMESTAMP
    | BIGINT
    | INTEGER
    | DOUBLE
    /* SQL:2016 methods also supported */
    | ABS
    | FLOOR
    | CEILING
    | SIZE
    ;
```

**AST Node:**

```c
typedef enum {
    JSON_METHOD_STRING,
    JSON_METHOD_NUMBER,
    JSON_METHOD_BOOLEAN,
    JSON_METHOD_DATE,
    JSON_METHOD_TIME,
    JSON_METHOD_TIMESTAMP,
    JSON_METHOD_BIGINT,
    JSON_METHOD_INTEGER,
    JSON_METHOD_DOUBLE,
    JSON_METHOD_ABS,
    JSON_METHOD_FLOOR,
    JSON_METHOD_CEILING,
    JSON_METHOD_SIZE
} JsonItemMethod;

typedef struct JsonMethodCall {
    ASTNode base;
    ASTNode* object;
    JsonItemMethod method;
} JsonMethodCall;
```

### 4.4 Executor Implementation

**File:** `src/executor/json_item_methods.c`

```c
// Execute JSON item method
JsonbValue* exec_json_method(JsonbValue* obj, JsonItemMethod method) {
    if (!obj) return create_json_null();

    switch (method) {
        case JSON_METHOD_STRING:
            return json_to_string_method(obj);
        case JSON_METHOD_NUMBER:
            return json_to_number_method(obj);
        case JSON_METHOD_BOOLEAN:
            return json_to_boolean_method(obj);
        case JSON_METHOD_INTEGER:
            return json_to_integer_method(obj);
        case JSON_METHOD_BIGINT:
            return json_to_bigint_method(obj);
        case JSON_METHOD_DOUBLE:
            return json_to_double_method(obj);
        case JSON_METHOD_DATE:
            return json_to_date_method(obj);
        case JSON_METHOD_TIME:
            return json_to_time_method(obj);
        case JSON_METHOD_TIMESTAMP:
            return json_to_timestamp_method(obj);
        case JSON_METHOD_ABS:
            return json_abs_method(obj);
        case JSON_METHOD_FLOOR:
            return json_floor_method(obj);
        case JSON_METHOD_CEILING:
            return json_ceiling_method(obj);
        case JSON_METHOD_SIZE:
            return json_size_method(obj);
        default:
            return create_json_null();
    }
}

// Conversion methods
JsonbValue* json_to_string_method(JsonbValue* obj) {
    switch (obj->type) {
        case JSON_TYPE_STRING:
            return obj;  // Already string
        case JSON_TYPE_NUMBER:
            return create_json_string(double_to_string(obj->data.float_val));
        case JSON_TYPE_TRUE:
            return create_json_string("true");
        case JSON_TYPE_FALSE:
            return create_json_string("false");
        case JSON_TYPE_NULL:
            return create_json_string("null");
        default:
            return create_json_null();  // Cannot convert
    }
}

JsonbValue* json_to_number_method(JsonbValue* obj) {
    switch (obj->type) {
        case JSON_TYPE_NUMBER:
            return obj;  // Already number
        case JSON_TYPE_STRING: {
            double num;
            if (parse_double(get_json_string(obj), &num)) {
                return create_json_number(num);
            }
            return create_json_null();  // Invalid number
        }
        case JSON_TYPE_TRUE:
            return create_json_number(1.0);
        case JSON_TYPE_FALSE:
            return create_json_number(0.0);
        default:
            return create_json_null();
    }
}

JsonbValue* json_to_boolean_method(JsonbValue* obj) {
    switch (obj->type) {
        case JSON_TYPE_TRUE:
            return obj;
        case JSON_TYPE_FALSE:
            return obj;
        case JSON_TYPE_NUMBER:
            return obj->data.float_val != 0.0 ?
                   create_json_boolean(true) : create_json_boolean(false);
        case JSON_TYPE_STRING: {
            const char* str = get_json_string(obj);
            if (strcasecmp(str, "true") == 0) return create_json_boolean(true);
            if (strcasecmp(str, "false") == 0) return create_json_boolean(false);
            return create_json_null();
        }
        default:
            return create_json_null();
    }
}

JsonbValue* json_to_timestamp_method(JsonbValue* obj) {
    if (obj->type != JSON_TYPE_STRING) {
        return create_json_null();
    }

    const char* str = get_json_string(obj);
    Timestamp ts;

    // Parse ISO 8601 timestamp
    if (parse_iso8601_timestamp(str, &ts)) {
        // Return JSON number representing Unix timestamp
        return create_json_number((double)timestamp_to_unix(ts));
    }

    return create_json_null();
}

// Math methods (SQL:2016)
JsonbValue* json_abs_method(JsonbValue* obj) {
    if (obj->type != JSON_TYPE_NUMBER) return create_json_null();
    return create_json_number(fabs(obj->data.float_val));
}

JsonbValue* json_floor_method(JsonbValue* obj) {
    if (obj->type != JSON_TYPE_NUMBER) return create_json_null();
    return create_json_number(floor(obj->data.float_val));
}

JsonbValue* json_ceiling_method(JsonbValue* obj) {
    if (obj->type != JSON_TYPE_NUMBER) return create_json_null();
    return create_json_number(ceil(obj->data.float_val));
}

// Size method
JsonbValue* json_size_method(JsonbValue* obj) {
    switch (obj->type) {
        case JSON_TYPE_ARRAY:
        case JSON_TYPE_OBJECT:
            return create_json_number((double)obj->data.container_val.num_elements);
        case JSON_TYPE_STRING:
            return create_json_number((double)strlen(get_json_string(obj)));
        default:
            return create_json_null();
    }
}
```

### 4.5 Testing

```sql
-- Test data
CREATE TABLE test_methods (
    id INT PRIMARY KEY,
    data JSON
);

INSERT INTO test_methods VALUES
(1, '{"price": "123.45", "quantity": "10"}'),
(2, '{"active": "true", "count": 42}'),
(3, '{"created": "2024-01-15T10:30:00Z"}'),
(4, '{"items": [1,2,3,4,5]}');

-- Test 1: String to number
SELECT data.price.number() FROM test_methods WHERE id = 1;
-- Expected: 123.45

-- Test 2: String to boolean
SELECT data.active.boolean() FROM test_methods WHERE id = 2;
-- Expected: true

-- Test 3: String to timestamp
SELECT data.created.timestamp() FROM test_methods WHERE id = 3;
-- Expected: Unix timestamp

-- Test 4: Array size
SELECT data.items.size() FROM test_methods WHERE id = 4;
-- Expected: 5

-- Test 5: Math methods
SELECT JSON_VALUE('{"val": -42}', '$.val.abs()');  -- 42
SELECT JSON_VALUE('{"val": 3.7}', '$.val.floor()');  -- 3
SELECT JSON_VALUE('{"val": 3.2}', '$.val.ceiling()');  -- 4
```

---

## 5. T879-T882: JSON Comparison Operators

### 5.1 Overview

**Feature:** Native comparison operators on JSON values
**Priority:** 🟢 MEDIUM
**Effort:** 1 week
**Dependencies:** T801
**SQL:2023 Feature Codes:** T879-T882

### 5.2 Syntax

```sql
-- Direct JSON comparison
SELECT * FROM documents
WHERE metadata.version > 2;

SELECT * FROM products
WHERE details.price >= 100.00;

-- JSON equality
SELECT * FROM users
WHERE settings = '{"theme": "dark"}'::JSON;
```

### 5.3 Comparison Rules

**Type-Specific Comparison:**
- **Numbers:** Numeric comparison (42 < 100)
- **Strings:** Lexicographic comparison ('a' < 'b')
- **Booleans:** false < true
- **Nulls:** NULL handling follows SQL rules
- **Arrays:** Element-wise comparison (lexicographic)
- **Objects:** Implementation-defined (key-value comparison)

### 5.4 Implementation

**File:** `src/executor/json_comparison.c`

```c
// JSON comparison result
typedef enum {
    JSON_CMP_LESS = -1,
    JSON_CMP_EQUAL = 0,
    JSON_CMP_GREATER = 1,
    JSON_CMP_INCOMPARABLE = 2  // Different types
} JsonCompareResult;

// Compare two JSON values
JsonCompareResult json_compare(JsonbValue* a, JsonbValue* b) {
    // NULL handling
    if (!a && !b) return JSON_CMP_EQUAL;
    if (!a) return JSON_CMP_LESS;
    if (!b) return JSON_CMP_GREATER;

    // Type mismatch
    if (a->type != b->type) {
        return JSON_CMP_INCOMPARABLE;
    }

    switch (a->type) {
        case JSON_TYPE_NULL:
            return JSON_CMP_EQUAL;

        case JSON_TYPE_TRUE:
        case JSON_TYPE_FALSE:
            return JSON_CMP_EQUAL;  // true==true, false==false

        case JSON_TYPE_NUMBER:
            return json_compare_numbers(a->data.float_val, b->data.float_val);

        case JSON_TYPE_STRING:
            return json_compare_strings(a, b);

        case JSON_TYPE_ARRAY:
            return json_compare_arrays(a, b);

        case JSON_TYPE_OBJECT:
            return json_compare_objects(a, b);

        default:
            return JSON_CMP_INCOMPARABLE;
    }
}

JsonCompareResult json_compare_numbers(double a, double b) {
    if (a < b) return JSON_CMP_LESS;
    if (a > b) return JSON_CMP_GREATER;
    return JSON_CMP_EQUAL;
}

JsonCompareResult json_compare_strings(JsonbValue* a, JsonbValue* b) {
    const char* str_a = get_json_string(a);
    const char* str_b = get_json_string(b);
    int cmp = strcmp(str_a, str_b);

    if (cmp < 0) return JSON_CMP_LESS;
    if (cmp > 0) return JSON_CMP_GREATER;
    return JSON_CMP_EQUAL;
}

JsonCompareResult json_compare_arrays(JsonbValue* a, JsonbValue* b) {
    uint32_t len_a = a->data.container_val.num_elements;
    uint32_t len_b = b->data.container_val.num_elements;
    uint32_t min_len = len_a < len_b ? len_a : len_b;

    // Element-wise comparison
    for (uint32_t i = 0; i < min_len; i++) {
        JsonbValue* elem_a = get_json_array_element(a, i);
        JsonbValue* elem_b = get_json_array_element(b, i);

        JsonCompareResult cmp = json_compare(elem_a, elem_b);
        if (cmp != JSON_CMP_EQUAL) {
            return cmp;
        }
    }

    // All elements equal, compare lengths
    if (len_a < len_b) return JSON_CMP_LESS;
    if (len_a > len_b) return JSON_CMP_GREATER;
    return JSON_CMP_EQUAL;
}

JsonCompareResult json_compare_objects(JsonbValue* a, JsonbValue* b) {
    // Object comparison: compare sorted key-value pairs
    // This ensures {"a":1,"b":2} == {"b":2,"a":1}

    KeyValuePair* pairs_a = get_sorted_object_pairs(a);
    KeyValuePair* pairs_b = get_sorted_object_pairs(b);

    uint32_t len_a = a->data.container_val.num_elements;
    uint32_t len_b = b->data.container_val.num_elements;
    uint32_t min_len = len_a < len_b ? len_a : len_b;

    for (uint32_t i = 0; i < min_len; i++) {
        // Compare keys
        int key_cmp = strcmp(pairs_a[i].key, pairs_b[i].key);
        if (key_cmp < 0) return JSON_CMP_LESS;
        if (key_cmp > 0) return JSON_CMP_GREATER;

        // Keys equal, compare values
        JsonCompareResult val_cmp = json_compare(pairs_a[i].value, pairs_b[i].value);
        if (val_cmp != JSON_CMP_EQUAL) {
            free(pairs_a);
            free(pairs_b);
            return val_cmp;
        }
    }

    free(pairs_a);
    free(pairs_b);

    // All pairs equal, compare sizes
    if (len_a < len_b) return JSON_CMP_LESS;
    if (len_a > len_b) return JSON_CMP_GREATER;
    return JSON_CMP_EQUAL;
}

// SQL comparison operators
bool json_equals(JsonbValue* a, JsonbValue* b) {
    return json_compare(a, b) == JSON_CMP_EQUAL;
}

bool json_less_than(JsonbValue* a, JsonbValue* b) {
    return json_compare(a, b) == JSON_CMP_LESS;
}

bool json_less_than_or_equal(JsonbValue* a, JsonbValue* b) {
    JsonCompareResult cmp = json_compare(a, b);
    return cmp == JSON_CMP_LESS || cmp == JSON_CMP_EQUAL;
}

bool json_greater_than(JsonbValue* a, JsonbValue* b) {
    return json_compare(a, b) == JSON_CMP_GREATER;
}

bool json_greater_than_or_equal(JsonbValue* a, JsonbValue* b) {
    JsonCompareResult cmp = json_compare(a, b);
    return cmp == JSON_CMP_GREATER || cmp == JSON_CMP_EQUAL;
}

bool json_not_equals(JsonbValue* a, JsonbValue* b) {
    return json_compare(a, b) != JSON_CMP_EQUAL;
}
```

### 5.5 Testing

```sql
-- Test data
CREATE TABLE test_compare (
    id INT PRIMARY KEY,
    val JSON
);

INSERT INTO test_compare VALUES
(1, '42'),
(2, '100'),
(3, '"apple"'),
(4, '"banana"'),
(5, 'true'),
(6, 'false'),
(7, '[1,2,3]'),
(8, '[1,2,4]'),
(9, '{"a": 1}'),
(10, '{"a": 2}');

-- Test 1: Number comparison
SELECT id FROM test_compare WHERE val > '50';
-- Expected: 2 (100 > 50)

SELECT id FROM test_compare WHERE val >= '42';
-- Expected: 1, 2 (42 >= 42, 100 >= 42)

-- Test 2: String comparison
SELECT id FROM test_compare WHERE val > '"a"'::JSON;
-- Expected: 3, 4 ('apple' > 'a', 'banana' > 'a')

SELECT id FROM test_compare WHERE val < '"c"'::JSON;
-- Expected: 3, 4

-- Test 3: Boolean comparison
SELECT id FROM test_compare WHERE val = 'true'::JSON;
-- Expected: 5

-- Test 4: Array comparison
SELECT id FROM test_compare WHERE val > '[1,2,3]'::JSON;
-- Expected: 8 ([1,2,4] > [1,2,3])

-- Test 5: Object comparison
SELECT id FROM test_compare WHERE val = '{"a": 1}'::JSON;
-- Expected: 9

SELECT id FROM test_compare WHERE val > '{"a": 1}'::JSON;
-- Expected: 10
```

---

## 6. T661: Non-Decimal Integer Literals

### 6.1 Overview

**Feature:** Support binary, octal, and hexadecimal integer literals
**Priority:** 🟢 LOW
**Effort:** 1 week
**Dependencies:** None
**SQL:2023 Feature Code:** T661

### 6.2 Syntax

```sql
-- Binary literals (0b prefix)
SELECT 0b1010 AS binary_ten;          -- 10
SELECT 0B11111111 AS binary_255;      -- 255

-- Octal literals (0o prefix)
SELECT 0o52 AS octal_42;              -- 42
SELECT 0O777 AS octal_511;            -- 511

-- Hexadecimal literals (0x prefix)
SELECT 0x2A AS hex_42;                -- 42
SELECT 0xFF AS hex_255;               -- 255
SELECT 0xDEADBEEF AS hex_large;       -- 3735928559
```

### 6.3 Lexer Implementation

**File:** `src/parser/lexer.l`

```c
/* Binary integer literals */
0[bB][01]+                  { yylval.int_val = parse_binary(yytext+2); return INTEGER_LITERAL; }

/* Octal integer literals */
0[oO][0-7]+                 { yylval.int_val = parse_octal(yytext+2); return INTEGER_LITERAL; }

/* Hexadecimal integer literals */
0[xX][0-9a-fA-F]+           { yylval.int_val = parse_hexadecimal(yytext+2); return INTEGER_LITERAL; }

/* Decimal integer literals (existing) */
[0-9]+                      { yylval.int_val = atoll(yytext); return INTEGER_LITERAL; }
```

**Parsing Functions:**

```c
// File: src/parser/literal_parser.c

int64_t parse_binary(const char* str) {
    int64_t result = 0;
    while (*str) {
        result = (result << 1) | (*str - '0');
        str++;
    }
    return result;
}

int64_t parse_octal(const char* str) {
    int64_t result = 0;
    while (*str) {
        result = (result << 3) | (*str - '0');
        str++;
    }
    return result;
}

int64_t parse_hexadecimal(const char* str) {
    int64_t result = 0;
    while (*str) {
        int digit;
        if (*str >= '0' && *str <= '9') {
            digit = *str - '0';
        } else if (*str >= 'a' && *str <= 'f') {
            digit = *str - 'a' + 10;
        } else if (*str >= 'A' && *str <= 'F') {
            digit = *str - 'A' + 10;
        } else {
            break;
        }
        result = (result << 4) | digit;
        str++;
    }
    return result;
}
```

### 6.4 Use Cases and Examples

**Bit Flags:**

```sql
CREATE TABLE user_permissions (
    user_id INT PRIMARY KEY,
    permissions INT DEFAULT 0b00000000
);

-- Grant read (bit 0), write (bit 1), execute (bit 2)
UPDATE user_permissions
SET permissions = 0b00000111
WHERE user_id = 123;

-- Check if user has write permission (bit 1)
SELECT user_id
FROM user_permissions
WHERE (permissions & 0b00000010) != 0;
```

**IP Address Storage:**

```sql
CREATE TABLE ip_whitelist (
    ip_address BIGINT
);

-- Store 192.168.1.1 as hex
INSERT INTO ip_whitelist VALUES (0xC0A80101);

-- Query by IP range (192.168.0.0/16)
SELECT * FROM ip_whitelist
WHERE ip_address BETWEEN 0xC0A80000 AND 0xC0A8FFFF;
```

**Unix File Permissions:**

```sql
CREATE TABLE files (
    file_name VARCHAR(255),
    permissions INT DEFAULT 0o644  -- rw-r--r--
);

INSERT INTO files VALUES ('script.sh', 0o755);  -- rwxr-xr-x
INSERT INTO files VALUES ('config.txt', 0o600); -- rw-------
```

### 6.5 Testing

```sql
-- Test 1: Binary literals
SELECT 0b0 AS zero;           -- 0
SELECT 0b1 AS one;            -- 1
SELECT 0b1010 AS ten;         -- 10
SELECT 0b11111111 AS ff;      -- 255

-- Test 2: Octal literals
SELECT 0o0 AS zero;           -- 0
SELECT 0o10 AS eight;         -- 8
SELECT 0o52 AS fortytwo;      -- 42
SELECT 0o777 AS perms;        -- 511

-- Test 3: Hexadecimal literals
SELECT 0x0 AS zero;           -- 0
SELECT 0xA AS ten;            -- 10
SELECT 0xFF AS ff;            -- 255
SELECT 0xDEADBEEF AS big;     -- 3735928559

-- Test 4: Mixed radix in expressions
SELECT 0b1010 + 0o12 + 0xA AS result;  -- 10 + 10 + 10 = 30

-- Test 5: Bitwise operations
SELECT 0xFF & 0b11110000 AS masked;    -- 240
SELECT 0x0F | 0xF0 AS combined;        -- 255
SELECT 0b1010 ^ 0b0110 AS xor_result;  -- 12 (0b1100)
```

---

## 7. T662: Underscores in Numeric Literals

### 7.1 Overview

**Feature:** Allow underscores as digit separators in numeric literals
**Priority:** 🟢 LOW
**Effort:** 1 week
**Dependencies:** None (can be combined with T661)
**SQL:2023 Feature Code:** T662

### 7.2 Syntax

```sql
-- Decimal with underscores
SELECT 1_000_000 AS one_million;
SELECT 1_234_567.89 AS formatted_decimal;

-- Binary with underscores
SELECT 0b1111_1111 AS eight_bits;
SELECT 0b1111_1111_1111_1111 AS sixteen_bits;

-- Hexadecimal with underscores
SELECT 0xDEAD_BEEF AS hex_formatted;

-- Octal with underscores
SELECT 0o777_777_777 AS octal_formatted;
```

### 7.3 Rules

1. Underscores are **ignored** by the parser (purely cosmetic)
2. Cannot start or end with underscore
3. Cannot have consecutive underscores
4. Can appear anywhere between digits

**Invalid:**
```sql
SELECT _1000;       -- ERROR: Cannot start with underscore
SELECT 1000_;       -- ERROR: Cannot end with underscore
SELECT 1__000;      -- ERROR: No consecutive underscores
SELECT 0x_FF;       -- ERROR: Cannot follow prefix immediately
```

### 7.4 Lexer Implementation

**File:** `src/parser/lexer.l`

```c
/* Integer with underscores */
[0-9][0-9_]*[0-9]|[0-9]     { yylval.int_val = parse_int_with_underscores(yytext);
                               return INTEGER_LITERAL; }

/* Float with underscores */
[0-9][0-9_]*\.([0-9_]*[0-9])?([eE][+-]?[0-9]+)?   {
    yylval.float_val = parse_float_with_underscores(yytext);
    return FLOAT_LITERAL;
}

/* Binary with underscores */
0[bB][01][01_]*[01]|0[bB][01]   {
    yylval.int_val = parse_binary_with_underscores(yytext+2);
    return INTEGER_LITERAL;
}

/* Octal with underscores */
0[oO][0-7][0-7_]*[0-7]|0[oO][0-7]   {
    yylval.int_val = parse_octal_with_underscores(yytext+2);
    return INTEGER_LITERAL;
}

/* Hexadecimal with underscores */
0[xX][0-9a-fA-F][0-9a-fA-F_]*[0-9a-fA-F]|0[xX][0-9a-fA-F]   {
    yylval.int_val = parse_hex_with_underscores(yytext+2);
    return INTEGER_LITERAL;
}
```

**Parsing Functions:**

```c
// File: src/parser/literal_parser.c

// Remove underscores from numeric string
char* strip_underscores(const char* str) {
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    char* dst = result;

    for (const char* src = str; *src; src++) {
        if (*src != '_') {
            *dst++ = *src;
        }
    }
    *dst = '\0';

    return result;
}

// Validate underscore placement
bool validate_underscores(const char* str) {
    // Check start/end
    if (str[0] == '_' || str[strlen(str)-1] == '_') {
        return false;
    }

    // Check consecutive underscores
    for (const char* p = str; *p; p++) {
        if (*p == '_' && *(p+1) == '_') {
            return false;
        }
    }

    return true;
}

int64_t parse_int_with_underscores(const char* str) {
    if (!validate_underscores(str)) {
        yyerror("Invalid underscore placement in numeric literal");
        return 0;
    }

    char* stripped = strip_underscores(str);
    int64_t result = atoll(stripped);
    free(stripped);
    return result;
}

double parse_float_with_underscores(const char* str) {
    if (!validate_underscores(str)) {
        yyerror("Invalid underscore placement in numeric literal");
        return 0.0;
    }

    char* stripped = strip_underscores(str);
    double result = atof(stripped);
    free(stripped);
    return result;
}

int64_t parse_binary_with_underscores(const char* str) {
    if (!validate_underscores(str)) {
        yyerror("Invalid underscore placement in numeric literal");
        return 0;
    }

    char* stripped = strip_underscores(str);
    int64_t result = parse_binary(stripped);
    free(stripped);
    return result;
}
```

### 7.5 Testing

```sql
-- Test 1: Decimal with underscores
SELECT 1_000 AS thousand;              -- 1000
SELECT 1_000_000 AS million;           -- 1000000
SELECT 123_456_789 AS large;           -- 123456789

-- Test 2: Float with underscores
SELECT 3.141_592_653 AS pi;            -- 3.141592653
SELECT 1_234.567_89 AS decimal;        -- 1234.56789

-- Test 3: Binary with underscores
SELECT 0b1111_1111 AS byte;            -- 255
SELECT 0b1010_0101_1010_0101 AS word;  -- 42405

-- Test 4: Hex with underscores
SELECT 0xDEAD_BEEF AS hex1;            -- 3735928559
SELECT 0x12_34_56_78 AS hex2;          -- 305419896

-- Test 5: Octal with underscores
SELECT 0o777_777 AS oct;               -- 262143

-- Test 6: Invalid (should error)
SELECT _1000;        -- ERROR
SELECT 1000_;        -- ERROR
SELECT 1__000;       -- ERROR
SELECT 0x_DEAD;      -- ERROR
```

---

## 8. F401: Enhanced NULL Handling in UNIQUE Constraints

### 8.1 Overview

**Feature:** Explicit control over NULL handling in UNIQUE constraints
**Priority:** 🟢 LOW-MEDIUM
**Effort:** 1 week
**Dependencies:** None
**SQL:2023 Feature Code:** F401

### 8.2 Background

**Problem:** SQL:2016 didn't specify how NULL values should be treated in UNIQUE constraints, leading to inconsistent behavior:

- **PostgreSQL/SQL Server:** `NULL != NULL` (multiple NULLs allowed)
- **Oracle:** `NULL = NULL` (only one NULL allowed)

**SQL:2023 Solution:** Explicit `NULLS DISTINCT` / `NULLS NOT DISTINCT` clause

### 8.3 Syntax

```sql
-- Multiple NULLs allowed (PostgreSQL behavior)
CREATE TABLE users (
    user_id INT PRIMARY KEY,
    email VARCHAR(255) UNIQUE NULLS DISTINCT
);

-- Only one NULL allowed (Oracle behavior)
CREATE TABLE employees (
    emp_id INT PRIMARY KEY,
    ssn VARCHAR(11) UNIQUE NULLS NOT DISTINCT
);

-- Default behavior (NULLS DISTINCT if not specified)
CREATE TABLE contacts (
    contact_id INT PRIMARY KEY,
    phone VARCHAR(20) UNIQUE  -- Defaults to NULLS DISTINCT
);
```

### 8.4 Parser Implementation

**Grammar:**

```yacc
column_constraint:
    | UNIQUE nulls_distinct_clause_opt
    | PRIMARY KEY
    ;

table_constraint:
    | UNIQUE '(' column_list ')' nulls_distinct_clause_opt
    ;

nulls_distinct_clause_opt:
    | /* empty */  /* Defaults to NULLS DISTINCT */
    | NULLS DISTINCT
    | NULLS NOT DISTINCT
    ;
```

**AST Node:**

```c
typedef enum {
    NULLS_DISTINCT,
    NULLS_NOT_DISTINCT
} NullsDistinctMode;

typedef struct UniqueConstraint {
    Constraint base;
    char** column_names;
    int num_columns;
    NullsDistinctMode nulls_mode;
} UniqueConstraint;
```

### 8.5 Catalog Schema

**Update:** `RDB$RELATION_CONSTRAINTS`

```sql
ALTER TABLE RDB$RELATION_CONSTRAINTS
ADD COLUMN NULLS_DISTINCT_MODE CHAR(15) DEFAULT 'DISTINCT';
-- Values: 'DISTINCT', 'NOT_DISTINCT'
```

### 8.6 Executor Implementation

**File:** `src/executor/unique_constraint.c`

```c
// Check unique constraint violation
bool check_unique_constraint(UniqueConstraint* constraint,
                              RecordBuffer* new_record,
                              TableScan* table) {
    // Extract key values from new record
    Value* key_values[constraint->num_columns];
    bool has_null = false;

    for (int i = 0; i < constraint->num_columns; i++) {
        key_values[i] = get_column_value(new_record, constraint->column_names[i]);
        if (is_null(key_values[i])) {
            has_null = true;
        }
    }

    // If any column is NULL, apply NULLS DISTINCT/NOT DISTINCT logic
    if (has_null) {
        if (constraint->nulls_mode == NULLS_DISTINCT) {
            // Multiple NULLs allowed - no violation
            return true;
        } else {
            // NULLS NOT DISTINCT - check if another NULL exists
            return !exists_null_row(table, constraint);
        }
    }

    // No NULL values - check for duplicate non-NULL key
    return !exists_duplicate_row(table, constraint, key_values);
}

// Check if another row with NULL in unique column(s) exists
bool exists_null_row(TableScan* table, UniqueConstraint* constraint) {
    RecordBuffer* record;

    while ((record = table_scan_next(table)) != NULL) {
        bool all_null = true;

        for (int i = 0; i < constraint->num_columns; i++) {
            Value* val = get_column_value(record, constraint->column_names[i]);
            if (!is_null(val)) {
                all_null = false;
                break;
            }
        }

        if (all_null) {
            return true;  // Found another all-NULL row
        }
    }

    return false;
}
```

### 8.7 Index Implementation

**Unique Index with NULLS NOT DISTINCT:**

```c
// File: src/index/unique_index.c

typedef struct UniqueIndex {
    BTreeIndex base;
    NullsDistinctMode nulls_mode;
    bool has_null_entry;  // For NULLS NOT DISTINCT
} UniqueIndex;

// Insert into unique index
bool unique_index_insert(UniqueIndex* index, Value* key, RecordID rid) {
    if (is_null(key)) {
        if (index->nulls_mode == NULLS_NOT_DISTINCT) {
            // Check if NULL already exists
            if (index->has_null_entry) {
                return false;  // Duplicate NULL not allowed
            }
            index->has_null_entry = true;
        }
        // NULLS DISTINCT: multiple NULLs allowed, don't store in index
        return true;
    }

    // Non-NULL: standard unique index insertion
    return btree_insert_unique(&index->base, key, rid);
}

// Delete from unique index
void unique_index_delete(UniqueIndex* index, Value* key, RecordID rid) {
    if (is_null(key)) {
        if (index->nulls_mode == NULLS_NOT_DISTINCT) {
            index->has_null_entry = false;
        }
        return;
    }

    btree_delete(&index->base, key, rid);
}
```

### 8.8 Testing

```sql
-- Test 1: NULLS DISTINCT (default - multiple NULLs allowed)
CREATE TABLE test_distinct (
    id INT PRIMARY KEY,
    email VARCHAR(255) UNIQUE NULLS DISTINCT
);

INSERT INTO test_distinct VALUES (1, NULL);    -- OK
INSERT INTO test_distinct VALUES (2, NULL);    -- OK (multiple NULLs allowed)
INSERT INTO test_distinct VALUES (3, 'a@b.com');  -- OK
INSERT INTO test_distinct VALUES (4, 'a@b.com');  -- ERROR: Duplicate

-- Test 2: NULLS NOT DISTINCT (only one NULL allowed)
CREATE TABLE test_not_distinct (
    id INT PRIMARY KEY,
    ssn VARCHAR(11) UNIQUE NULLS NOT DISTINCT
);

INSERT INTO test_not_distinct VALUES (1, NULL);      -- OK
INSERT INTO test_not_distinct VALUES (2, NULL);      -- ERROR: Duplicate NULL
INSERT INTO test_not_distinct VALUES (3, '123-45-6789');  -- OK
INSERT INTO test_not_distinct VALUES (4, '123-45-6789');  -- ERROR: Duplicate

-- Test 3: Composite unique constraint
CREATE TABLE test_composite (
    id INT PRIMARY KEY,
    col_a INT,
    col_b INT,
    UNIQUE (col_a, col_b) NULLS DISTINCT
);

INSERT INTO test_composite VALUES (1, NULL, NULL);  -- OK
INSERT INTO test_composite VALUES (2, NULL, NULL);  -- OK (NULLS DISTINCT)
INSERT INTO test_composite VALUES (3, 1, NULL);     -- OK
INSERT INTO test_composite VALUES (4, 1, NULL);     -- OK (NULL in col_b)
INSERT INTO test_composite VALUES (5, 1, 2);        -- OK
INSERT INTO test_composite VALUES (6, 1, 2);        -- ERROR: Duplicate

-- Test 4: Default behavior (should be NULLS DISTINCT)
CREATE TABLE test_default (
    id INT PRIMARY KEY,
    value INT UNIQUE
);

INSERT INTO test_default VALUES (1, NULL);  -- OK
INSERT INTO test_default VALUES (2, NULL);  -- OK (defaults to NULLS DISTINCT)
```

---

## 9. T056: Multi-Character TRIM Functions

### 9.1 Overview

**Feature:** TRIM can remove multi-character strings (not just single characters)
**Priority:** 🟢 LOW
**Effort:** <1 week
**Dependencies:** None
**SQL:2023 Feature Code:** T056

### 9.2 Syntax

```sql
-- SQL:2016: Single character only
SELECT TRIM(' ' FROM '  Hello  ');  -- 'Hello'

-- SQL:2023: Multi-character string
SELECT TRIM('abc' FROM 'abcHelloabc');    -- 'Hello'
SELECT TRIM('+-' FROM '++--Value--++');   -- 'Value'
SELECT TRIM('/*' FROM '/* Comment */');   -- ' Comment '

-- With LEADING/TRAILING/BOTH
SELECT TRIM(LEADING '0' FROM '00042');    -- '42'
SELECT TRIM(TRAILING 'abc' FROM 'testtesttestabc');  -- 'testtesttest'
SELECT TRIM(BOTH 'xy' FROM 'xyHelloxy');  -- 'Hello'
```

### 9.3 Implementation

**File:** `src/functions/string_functions.c`

```c
// Multi-character TRIM
char* trim_multi(const char* str, const char* trim_chars, TrimMode mode) {
    if (!str || !trim_chars) return NULL;

    size_t str_len = strlen(str);
    size_t trim_len = strlen(trim_chars);

    if (trim_len == 0) return strdup(str);

    const char* start = str;
    const char* end = str + str_len;

    // TRIM LEADING or BOTH
    if (mode == TRIM_LEADING || mode == TRIM_BOTH) {
        while (starts_with(start, trim_chars)) {
            start += trim_len;
        }
    }

    // TRIM TRAILING or BOTH
    if (mode == TRIM_TRAILING || mode == TRIM_BOTH) {
        while (end > start && ends_with_at(str, end, trim_chars)) {
            end -= trim_len;
        }
    }

    // Build result
    size_t result_len = end - start;
    char* result = malloc(result_len + 1);
    memcpy(result, start, result_len);
    result[result_len] = '\0';

    return result;
}

bool starts_with(const char* str, const char* prefix) {
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

bool ends_with_at(const char* str, const char* end, const char* suffix) {
    size_t suffix_len = strlen(suffix);
    const char* check_pos = end - suffix_len;

    if (check_pos < str) return false;

    return strncmp(check_pos, suffix, suffix_len) == 0;
}
```

### 9.4 Testing

```sql
-- Test 1: Simple multi-char trim
SELECT TRIM('abc' FROM 'abcHelloabc');  -- 'Hello'

-- Test 2: LEADING
SELECT TRIM(LEADING '0' FROM '00042');  -- '42'
SELECT TRIM(LEADING 'abc' FROM 'abcabcHello');  -- 'Hello'

-- Test 3: TRAILING
SELECT TRIM(TRAILING 'xyz' FROM 'Helloxyzxyz');  -- 'Hello'

-- Test 4: BOTH (default)
SELECT TRIM('/*' FROM '/*Comment*/');  -- 'Comment*/' (only leading removed)
SELECT TRIM(BOTH '+-' FROM '+-+-Value+-+-');  -- 'Value'

-- Test 5: No match
SELECT TRIM('xyz' FROM 'Hello');  -- 'Hello'

-- Test 6: Complete trim
SELECT TRIM('abc' FROM 'abcabcabc');  -- ''
```

---

# PART III: PROPERTY GRAPH QUERIES (SQL/PGQ)

---

## 10. SQL/PGQ Overview

**Feature:** Property Graph Queries (SQL/PGQ)
**Standard Part:** ISO/IEC 9075-16:2023
**Priority:** 🟡 MEDIUM (OPTIONAL - Beta 3.0+)
**Effort:** 12-16 weeks
**Industry Adoption:** Very Low (Oracle 23ai only)

**Recommendation:** **DEFER to Beta 3.0** (2027+). Monitor industry adoption first.

### 10.1 Feature Summary

SQL/PGQ enables querying relational data as property graphs using Cypher-inspired pattern matching syntax.

**Core Components:**
1. **CREATE PROPERTY GRAPH** - Define graph schema over tables
2. **GRAPH_TABLE** - Query operator for pattern matching
3. **Pattern Matching** - Visual syntax for graph traversal
4. **Path Quantifiers** - Variable-length paths (`{1,3}`, `*`, `+`)
5. **Graph Algorithms** - SHORTEST path, cycle detection

**Use Cases:**
- Social networks (friend recommendations, influence analysis)
- Fraud detection (transaction chains)
- Supply chain (dependency tracking)
- Knowledge graphs (entity relationships)
- Recommendation systems

### 10.2 CREATE PROPERTY GRAPH

**Syntax:**

```sql
CREATE PROPERTY GRAPH graph_name
  VERTEX TABLES (
    table_name [ AS vertex_label ]
      [ KEY ( column_list ) ]
      [ PROPERTIES ( column_list | ALL COLUMNS ) ]
    [, ...]
  )
  EDGE TABLES (
    table_name [ AS edge_label ]
      SOURCE source_table [ KEY ( column_list ) ] [ REFERENCES ( column_list ) ]
      DESTINATION dest_table [ KEY ( column_list ) ] [ REFERENCES ( column_list ) ]
      [ PROPERTIES ( column_list | ALL COLUMNS ) ]
    [, ...]
  );
```

**Example:**

```sql
CREATE PROPERTY GRAPH social_network
  VERTEX TABLES (
    users KEY (user_id) PROPERTIES ALL COLUMNS
  )
  EDGE TABLES (
    friendships AS follows
      SOURCE users REFERENCES (follower_id)
      DESTINATION users REFERENCES (followee_id)
      PROPERTIES (created_at, status)
  );
```

### 10.3 GRAPH_TABLE Operator

**Syntax:**

```sql
SELECT columns
FROM GRAPH_TABLE (
  graph_name
  MATCH pattern_expression
  [ WHERE condition ]
  COLUMNS ( column_definition [, ...] )
);
```

**Pattern Syntax:**

```
-- Vertex: (variable:label WHERE condition)
(u:users WHERE u.name = 'Alice')

-- Directed edge: -[variable:label WHERE condition]->
-[f:follows]->

-- Undirected edge: -[]-
-[]

-- Variable-length path:
(-[:follows]->(:users)){1,3}  -- 1 to 3 hops
(-[:follows]->(:users))*      -- Zero or more hops
(-[:follows]->(:users))+      -- One or more hops
```

**Example:**

```sql
-- Find friends of Alice
SELECT friend_name
FROM GRAPH_TABLE (
  social_network
  MATCH (u:users WHERE u.name = 'Alice')-[:follows]->(friend:users)
  COLUMNS (friend.name AS friend_name)
);

-- Friends of friends
SELECT fof_name, COUNT(*) AS mutual_friends
FROM GRAPH_TABLE (
  social_network
  MATCH (me:users WHERE me.name = 'Alice')
        -[:follows]->(friend:users)
        -[:follows]->(fof:users)
  WHERE fof.user_id != me.user_id
  COLUMNS (fof.name AS fof_name)
)
GROUP BY fof_name;
```

### 10.4 Implementation Architecture

**Catalog Schema:**

```sql
CREATE TABLE RDB$PROPERTY_GRAPHS (
    GRAPH_ID INTEGER PRIMARY KEY,
    GRAPH_NAME VARCHAR(63) NOT NULL UNIQUE,
    CREATED_AT TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE RDB$GRAPH_VERTICES (
    GRAPH_ID INTEGER NOT NULL,
    VERTEX_LABEL VARCHAR(63) NOT NULL,
    TABLE_NAME VARCHAR(63) NOT NULL,
    KEY_COLUMNS VARCHAR(255) NOT NULL,  -- Comma-separated
    PROPERTY_COLUMNS VARCHAR(1024),     -- Comma-separated, NULL = ALL
    CONSTRAINT FK_GRAPH FOREIGN KEY (GRAPH_ID) REFERENCES RDB$PROPERTY_GRAPHS(GRAPH_ID)
);

CREATE TABLE RDB$GRAPH_EDGES (
    GRAPH_ID INTEGER NOT NULL,
    EDGE_LABEL VARCHAR(63) NOT NULL,
    TABLE_NAME VARCHAR(63) NOT NULL,
    SOURCE_TABLE VARCHAR(63) NOT NULL,
    SOURCE_KEY_COLUMNS VARCHAR(255) NOT NULL,
    SOURCE_REF_COLUMNS VARCHAR(255) NOT NULL,
    DEST_TABLE VARCHAR(63) NOT NULL,
    DEST_KEY_COLUMNS VARCHAR(255) NOT NULL,
    DEST_REF_COLUMNS VARCHAR(255) NOT NULL,
    PROPERTY_COLUMNS VARCHAR(1024),
    CONSTRAINT FK_GRAPH FOREIGN KEY (GRAPH_ID) REFERENCES RDB$PROPERTY_GRAPHS(GRAPH_ID)
);
```

**Parser Components:**

```c
typedef struct PropertyGraph {
    int graph_id;
    char* graph_name;
    VertexTable** vertex_tables;
    int num_vertex_tables;
    EdgeTable** edge_tables;
    int num_edge_tables;
} PropertyGraph;

typedef struct GraphPattern {
    PatternElement** elements;
    int num_elements;
    ASTNode* where_clause;
} GraphPattern;

typedef enum {
    PATTERN_VERTEX,
    PATTERN_EDGE
} PatternElementType;

typedef struct PatternElement {
    PatternElementType type;
    char* variable;
    char* label;
    ASTNode* where_clause;
    // For edges:
    EdgeDirection direction;  // DIRECTED_RIGHT, DIRECTED_LEFT, UNDIRECTED
    PathQuantifier* quantifier;  // NULL for single hop
} PatternElement;
```

**Execution Engine:**

```c
// Graph traversal algorithms
typedef enum {
    TRAVERSAL_BFS,   // Breadth-first search
    TRAVERSAL_DFS,   // Depth-first search
    TRAVERSAL_DIJKSTRA  // Shortest path
} TraversalAlgorithm;

// Execute graph pattern
ResultSet* execute_graph_pattern(PropertyGraph* graph,
                                  GraphPattern* pattern,
                                  TraversalAlgorithm algorithm);

// Find all paths matching pattern
Path** find_matching_paths(PropertyGraph* graph,
                            GraphPattern* pattern,
                            int* num_paths);

// Shortest path between two vertices
Path* find_shortest_path(PropertyGraph* graph,
                          Vertex* start,
                          Vertex* end);
```

### 10.5 Why Defer SQL/PGQ?

**Reasons to wait:**

1. **Minimal Industry Adoption:**
   - Only Oracle 23ai has production support (2024)
   - PostgreSQL 18 has experimental/prototype support
   - No other major database has implementation

2. **Cutting-Edge Feature:**
   - SQL/PGQ is brand new (2023)
   - Industry needs time to validate use cases
   - APIs may evolve in SQL:2026

3. **Large Implementation Effort:**
   - 12-16 weeks of development
   - Complex graph traversal algorithms
   - Query optimization for graph patterns

4. **Alternative Solutions Exist:**
   - Recursive CTEs can handle many graph queries
   - JOINs work for simple relationship traversal
   - External graph databases (Neo4j) for heavy graph workloads

**Decision Point:** Reassess in Q3 2027 after SQL:2016 and JSON:2023 features are complete.

---

# PART IV: IMPLEMENTATION ROADMAP

---

## 11. Phased Implementation Plan

### Phase 1: SQL:2023 JSON Features (Beta 2.0)
**Timeline:** 6-8 weeks
**Priority:** HIGH

**Features:**
1. T801: Native JSON Data Type - 2-3 weeks
2. T860-T864: Simplified Accessors - 2-3 weeks
3. T865-T878: Item Methods - 1-2 weeks
4. T879-T882: JSON Comparison - 1 week
5. T802: JSON Unique Keys - 1 week

**Deliverables:**
- Native JSON column type with JSONB storage
- Dot notation and array subscripts for JSON
- Type conversion methods
- Full JSON comparison operators
- Updated documentation and tests

**Competitive Impact:**
- JSON support matches Oracle 23ai and PostgreSQL 16+
- Simplified accessors provide better UX than competitors
- Positions ScratchBird as modern, developer-friendly database

---

### Phase 2: Core SQL Enhancements (Beta 2.1)
**Timeline:** 2-3 weeks
**Priority:** MEDIUM

**Features:**
1. T661: Non-Decimal Integer Literals - 1 week
2. T662: Underscores in Numeric Literals - 1 week
3. F401: Enhanced NULL in UNIQUE - 1 week
4. T056: Multi-Character TRIM - <1 week

**Deliverables:**
- Binary (0b), octal (0o), hex (0x) literals
- Underscore digit separators (1_000_000)
- NULLS DISTINCT/NOT DISTINCT in UNIQUE constraints
- Multi-character TRIM function

**Competitive Impact:**
- Modern numeric literal syntax
- Improved developer experience
- NULL handling clarity

---

### Phase 3: SQL/PGQ Graph Queries (Beta 3.0 - OPTIONAL)
**Timeline:** 12-16 weeks
**Priority:** LOW (Defer to 2027+)

**Features:**
1. CREATE PROPERTY GRAPH - 3-4 weeks
2. GRAPH_TABLE operator - 4-5 weeks
3. Pattern matching parser - 2-3 weeks
4. Graph execution engine - 3-4 weeks

**Decision Point:** Q3 2027 - Reassess industry adoption

---

## 12. Testing Strategy

### 12.1 Unit Tests

**JSON Storage Tests** (`tests/unit/test_json_storage.c`):
- Binary JSONB encoding/decoding
- Unique key validation
- JSON type validation
- NULL handling

**Parser Tests** (`tests/unit/test_parser.c`):
- JSON accessor parsing
- Non-decimal literal parsing
- Underscore validation
- F401 NULL constraint parsing

**Executor Tests** (`tests/unit/test_executor.c`):
- JSON field access
- JSON array subscript
- JSON comparison
- TRIM multi-character

### 12.2 Integration Tests

**SQL Test Suite** (`tests/sql/test_json_features.sql`):
- End-to-end JSON operations
- Complex nested JSON access
- JSON in WHERE clauses
- JSON indexing

**Regression Tests** (`tests/sql/regression/`):
- Compatibility with existing JSON functions
- No breaking changes to VARCHAR-based JSON

### 12.3 Performance Tests

**Benchmarks** (`tests/perf/`):
- JSON storage overhead (JSONB vs VARCHAR)
- Simplified accessor performance vs JSON_VALUE
- Index scan performance on JSON columns
- Comparison operator performance

### 12.4 Compatibility Tests

**Multi-Protocol Tests:**
- PostgreSQL wire protocol with JSON
- MySQL wire protocol with JSON
- Firebird wire protocol with JSON

---

## 13. Documentation Requirements

### 13.1 User Documentation

**SQL Reference Manual Updates:**
1. JSON Data Type section
2. JSON Simplified Accessors section
3. JSON Functions and Operators
4. Numeric Literals section
5. UNIQUE Constraint NULL handling

**Tutorial Documentation:**
1. Working with JSON in ScratchBird
2. Migrating from VARCHAR-based JSON to native JSON
3. JSON indexing best practices
4. JSON query optimization

### 13.2 Migration Guides

**From SQL:2016 JSON:**
```sql
-- Before (SQL:2016)
CREATE TABLE docs (
    content VARCHAR(8000),
    CHECK (content IS JSON)
);

SELECT JSON_VALUE(content, '$.customer.name') FROM docs;

-- After (SQL:2023)
CREATE TABLE docs (
    content JSON
);

SELECT content.customer.name FROM docs;
```

**Performance Migration:**
- Binary JSONB storage reduces size by 20-40%
- Simplified accessors are 2-3× faster than JSON_VALUE
- Index performance improves 3-5× with native JSON type

---

## 14. Success Metrics

### 14.1 Feature Completeness

- [ ] T801: JSON Data Type - COMPLETE
- [ ] T802: JSON Unique Keys - COMPLETE
- [ ] T860-T864: Simplified Accessors - COMPLETE
- [ ] T865-T878: Item Methods - COMPLETE
- [ ] T879-T882: JSON Comparison - COMPLETE
- [ ] T661: Non-Decimal Literals - COMPLETE
- [ ] T662: Underscores in Literals - COMPLETE
- [ ] F401: Enhanced NULL Handling - COMPLETE
- [ ] T056: Multi-Character TRIM - COMPLETE

### 14.2 Test Coverage

- [ ] Unit test coverage: >90%
- [ ] Integration test coverage: >85%
- [ ] SQL test suite: >100 test cases
- [ ] Performance benchmarks: All passing
- [ ] Regression tests: Zero failures

### 14.3 Performance Targets

- [ ] JSON storage overhead: <10% vs VARCHAR
- [ ] Simplified accessor performance: >2× faster than JSON_VALUE
- [ ] JSON comparison: <5% slower than native type comparison
- [ ] Index performance: Within 10% of native type indexes

### 14.4 Documentation

- [ ] All features documented in SQL reference
- [ ] Tutorial examples for each feature
- [ ] Migration guide from SQL:2016 JSON
- [ ] Performance tuning guide
- [ ] API documentation for SBLR opcodes

---

## 15. Risk Assessment

### 15.1 Technical Risks

**Risk 1: JSONB Storage Complexity**
- **Probability:** MEDIUM
- **Impact:** MEDIUM
- **Mitigation:** Use PostgreSQL JSONB as reference implementation

**Risk 2: Parser Complexity for Accessors**
- **Probability:** LOW
- **Impact:** MEDIUM
- **Mitigation:** Extensive parser tests, gradual rollout

**Risk 3: Performance Regression**
- **Probability:** LOW
- **Impact:** HIGH
- **Mitigation:** Continuous benchmarking, performance tests in CI

### 15.2 Schedule Risks

**Risk 1: Underestimated Effort**
- **Probability:** MEDIUM
- **Impact:** MEDIUM
- **Mitigation:** Phased approach allows for adjustment

**Risk 2: Dependency Delays**
- **Probability:** LOW
- **Impact:** LOW
- **Mitigation:** Features are largely independent

### 15.3 Adoption Risks

**Risk 1: Breaking Changes**
- **Probability:** LOW
- **Impact:** HIGH
- **Mitigation:** Backward compatibility with VARCHAR-based JSON

**Risk 2: Migration Complexity**
- **Probability:** MEDIUM
- **Impact:** MEDIUM
- **Mitigation:** Comprehensive migration tools and documentation

---

## Appendix A: SQL:2023 Feature Reference

### A.1 JSON Feature Codes

| Code | Feature | Priority | Effort | Page |
|------|---------|----------|--------|------|
| T801 | Native JSON Data Type | HIGH | 2-3 weeks | 5 |
| T802 | JSON Unique Keys | LOW | 1 week | 16 |
| T803 | String-Based JSON | N/A | Compat | - |
| T840 | Hex in JSON Path | VERY LOW | <1 week | - |
| T860 | Dot Accessor | HIGH | 2-3 weeks | 18 |
| T861 | Array Subscript | HIGH | Incl. T860 | 18 |
| T862 | Chained Accessors | HIGH | Incl. T860 | 18 |
| T863 | NULL Handling | MEDIUM | Incl. T860 | 18 |
| T864 | Array Slicing | MEDIUM | Incl. T860 | 18 |
| T865-T878 | Item Methods | MEDIUM | 1-2 weeks | 32 |
| T879-T882 | JSON Comparison | MEDIUM | 1 week | 37 |

### A.2 Core Language Feature Codes

| Code | Feature | Priority | Effort | Page |
|------|---------|----------|--------|------|
| T661 | Non-Decimal Literals | LOW | 1 week | 42 |
| T662 | Underscore Separators | LOW | 1 week | 47 |
| F401 | Enhanced NULL UNIQUE | MEDIUM | 1 week | 51 |
| T056 | Multi-Char TRIM | LOW | <1 week | 56 |
| F868 | Optional TABLE | VERY LOW | <1 week | - |

### A.3 Graph Query Features

| Feature | Priority | Effort | Page |
|---------|----------|--------|------|
| CREATE PROPERTY GRAPH | MEDIUM (Defer) | 3-4 weeks | 58 |
| GRAPH_TABLE Operator | MEDIUM (Defer) | 4-5 weeks | 58 |
| Pattern Matching | MEDIUM (Defer) | 2-3 weeks | 58 |
| Graph Execution | MEDIUM (Defer) | 3-4 weeks | 58 |

---

## Appendix B: References

**SQL:2023 Standard:**
- ISO/IEC 9075:2023 - Database languages — SQL

**Primary Sources:**
- Peter Eisentraut: "SQL:2023 is finished: Here is what's new"
- Oracle: "SQL:2023 Standard General Availability"
- Modern SQL: "SQL:2023 Feature Codes"
- PostgreSQL: "SQL:2023 Compliance Roadmap"

**Implementation References:**
- PostgreSQL JSONB implementation
- Oracle 23ai JSON features documentation
- SQL Server 2025 JSON data type
- Neo4j Cypher query language (for SQL/PGQ inspiration)

---

**END OF SPECIFICATION**

**Document Status:** DRAFT v1.0
**Next Review:** After Phase 1 completion
**Approval Required:** Engineering Lead, Product Manager

---
