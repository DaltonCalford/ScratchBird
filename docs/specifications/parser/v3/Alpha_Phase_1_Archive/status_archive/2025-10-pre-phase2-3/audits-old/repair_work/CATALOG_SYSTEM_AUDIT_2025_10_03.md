# System Catalog Audit Report

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

**Date**: 2025-10-03
**Auditor**: Claude Code
**Purpose**: Comprehensive audit of system catalog structures and metadata management

---

## Executive Summary

The ScratchBird catalog system manages database metadata through a fixed-page structure with in-memory caching. This audit identifies critical issues with the current implementation, particularly regarding new data types and missing metadata fields.

**Critical Findings**:
- ✗ Column metadata does not support VECTOR type dimensions
- ✗ Column metadata missing precision/scale fields for DECIMAL types
- ✗ No support for composite types, arrays, domains
- ✗ Missing constraint metadata (PRIMARY KEY, FOREIGN KEY, CHECK, UNIQUE)
- ✗ No index type specification (B-tree, Hash, GIN, VECTOR, etc.)

---

## 1. Current System Catalog Structure

### 1.1 Catalog Pages Layout

The catalog uses **fixed page IDs** for system tables:

```
Page 3: CATALOG_ROOT_PAGE    - Root catalog metadata
Page 4: SCHEMAS_TABLE_PAGE   - Schema definitions
Page 5: TABLES_TABLE_PAGE    - Table definitions
Page 6: COLUMNS_TABLE_PAGE   - Column definitions
Page 7: INDEXES_TABLE_PAGE   - Index definitions
```

**Location**: `catalog_manager.h:169-173`

### 1.2 Catalog Root Page Structure

```cpp
struct CatalogRootPage {
    PageHeader header;           // 16KB page header
    uint32_t schema_count;       // Total schemas in database
    uint32_t table_count;        // Total tables in database
    uint32_t schemas_page;       // Page ID for schemas table
    uint32_t tables_page;        // Page ID for tables table
    uint32_t columns_page;       // Page ID for columns table
    uint32_t indexes_page;       // Page ID for indexes table
    uint8_t reserved[4064];      // Padding to 16KB
};
```

**Location**: `catalog_manager.cpp:18-28`

---

## 2. Schema Metadata

### 2.1 In-Memory Structure (C++)

```cpp
struct SchemaInfo {
    ID schema_id;              // UUIDv7 (16 bytes)
    std::string schema_name;   // Schema name (dynamic)
    std::string owner;         // Owner name (dynamic)
    uint64_t created_time;     // Creation timestamp
};
```

**Location**: `catalog_manager.h:49-55`

### 2.2 On-Disk Structure

```cpp
struct SchemaRecord {
    ID schema_id;              // 16 bytes
    char schema_name[128];     // 128 bytes - SQL standard compliant
    char owner[128];           // 128 bytes - SQL standard compliant
    uint64_t created_time;     // 8 bytes
    uint32_t is_valid;         // 4 bytes (deletion marker)
};
// Total: 16 + 128 + 128 + 8 + 4 = 284 bytes per schema
```

**Location**: `catalog_manager.cpp:31-38`

### 2.3 Analysis

**✓ Strengths**:
- UUIDv7 provides distributed-safe unique IDs with timestamp ordering
- Soft delete support via `is_valid` flag
- Simple, fixed-size structure

**✓ Improvements Made**:
- ✓ Schema names now 128 characters (SQL standard compliant)
- ✓ Owner names now 128 characters (SQL standard compliant)

**✗ Remaining Issues**:
- No support for schema-level permissions
- Missing schema attributes (default tablespace, search path)
- No support for temporary schemas

**Recommendations**:
1. Add `permissions` field (bitfield or serialized ACL)
2. Add `default_tablespace_id` field
3. Consider increasing name length or using TOAST for long names

---

## 3. Table Metadata

### 3.1 In-Memory Structure

```cpp
struct TableInfo {
    ID table_id;               // UUIDv7
    ID schema_id;              // Parent schema
    std::string table_name;    // Table name
    uint32_t root_page;        // Root page of table data
    uint32_t column_count;     // Number of columns
    uint64_t row_count;        // Estimated row count
    uint64_t created_time;     // Creation timestamp
};
```

**Location**: `catalog_manager.h:58-67`

### 3.2 On-Disk Structure

```cpp
struct TableRecord {
    ID table_id;               // 16 bytes
    ID schema_id;              // 16 bytes
    char table_name[128];      // 128 bytes - SQL standard compliant
    uint32_t root_page;        // 4 bytes
    uint32_t column_count;     // 4 bytes
    uint64_t row_count;        // 8 bytes
    uint64_t created_time;     // 8 bytes
    uint32_t is_valid;         // 4 bytes
};
// Total: 16 + 16 + 128 + 4 + 4 + 8 + 8 + 4 = 188 bytes per table
```

**Location**: `catalog_manager.cpp:41-51`

### 3.3 Analysis

**✓ Strengths**:
- Direct linkage to heap page via `root_page`
- Row count for optimizer statistics

**✗ Critical Issues**:
1. **Missing table type**: No distinction between:
   - Regular heap tables
   - Temporary tables
   - Foreign tables
   - Materialized views
   - Partitioned tables

2. **Missing storage parameters**:
   - Fill factor
   - Compression method
   - Encryption settings
   - Tablespace ID

3. **Missing table attributes**:
   - `relkind` (table, view, index, sequence, etc.)
   - `relam` (access method for indexes)
   - `relpersistence` (permanent, temporary, unlogged)
   - `relnatts` (should match column_count but useful for validation)

4. **Missing statistics**:
   - `relpages` (number of pages)
   - `reltuples` (accurate tuple count)
   - `relallvisible` (pages with all-visible tuples)
   - Last analyze time
   - Last vacuum time

**Recommendations**:
1. Add `table_type` enum field (heap, temp, foreign, materialized_view, partitioned)
2. Add `storage_params` blob for flexible parameters
3. Add `tablespace_id` for tablespace support
4. Add statistics fields for query optimizer

---

## 4. Column Metadata - **CRITICAL ISSUES**

### 4.1 In-Memory Structure

```cpp
struct ColumnInfo {
    ID table_id;                  // Parent table
    ID column_id;                 // Column UUID
    std::string column_name;      // Column name
    uint16_t data_type;           // Type code
    uint32_t max_length;          // For variable length types
    bool nullable;                // NULL allowed
    bool has_default;             // Has default value
    std::string default_value;    // Serialized default
};
```

**Location**: `catalog_manager.h:70-80`

### 4.2 On-Disk Structure

```cpp
struct ColumnRecord {
    ID table_id;               // 16 bytes
    ID column_id;              // 16 bytes
    char column_name[128];     // 128 bytes - SQL standard compliant
    uint16_t data_type;        // 2 bytes
    uint32_t max_length;       // 4 bytes
    uint8_t nullable;          // 1 byte
    uint8_t has_default;       // 1 byte
    char default_value[128];   // 128 bytes
    uint32_t is_valid;         // 4 bytes
};
// Total: 16 + 16 + 128 + 2 + 4 + 1 + 1 + 128 + 4 = 300 bytes per column
```

**Location**: `catalog_manager.cpp:54-65`

### 4.3 Critical Problems

**✗ BLOCKING ISSUE #1: Missing Type Precision/Scale**

The current structure uses:
- `uint16_t data_type` - Type enumeration
- `uint32_t max_length` - Used for VARCHAR/CHAR length

**Problem**: This is insufficient for the new types we just added!

Required fields missing:
1. **VECTOR(n)**: Needs to store dimension count (1-65535)
   - Currently: No way to store that `VECTOR(1536)` has 1536 dimensions
   - Impact: Cannot reconstruct column type from catalog

2. **DECIMAL(p,s)**: Needs precision AND scale
   - Currently: Only `max_length` available, can't store both
   - Impact: `DECIMAL(10,2)` cannot be distinguished from `DECIMAL(8,4)`

3. **CHAR(n)** vs **VARCHAR(n)**:
   - Currently: Uses `max_length` field
   - Problem: No clear separation between different meanings of length

**✗ BLOCKING ISSUE #2: Missing Column Ordinal**

No `column_position` or `ordinal` field:
- Cannot guarantee column order in table
- SELECT * returns columns in unpredictable order
- Critical for SQL compliance

**✗ BLOCKING ISSUE #3: No Constraint Metadata**

Missing fields:
- `is_primary_key` - Is this column part of primary key?
- `is_foreign_key` - Is this column part of foreign key?
- `is_unique` - Does this column have unique constraint?
- `check_constraint` - CHECK expression for column

These are currently stored nowhere in the catalog!

**✗ BLOCKING ISSUE #4: No Array/Composite Support**

For `ARRAY` types:
- No `element_type_id` to store array element type
- No `array_dimensions` to store dimension count

For `COMPOSITE` types:
- No way to link to component columns
- No nested type support

**✗ BLOCKING ISSUE #5: Insufficient Default Value Storage**

`char default_value[128]` is:
- Too small for complex defaults (expressions, UUIDs, JSON)
- Fixed size wastes space for simple defaults (NULL, 0, TRUE)
- Should use TOAST for large defaults

### 4.4 Required Changes to ColumnRecord

**Proposed new structure**:

```cpp
struct ColumnRecord {
    // Identity
    ID table_id;                // 16 bytes - Parent table
    ID column_id;               // 16 bytes - Column UUID
    char column_name[64];       // 64 bytes - Column name
    uint16_t ordinal;           // 2 bytes - Column position (1-based)

    // Type information
    uint16_t data_type;         // 2 bytes - Base type (DataType enum)
    uint32_t type_precision;    // 4 bytes - Precision (DECIMAL, CHAR, VARCHAR, VECTOR)
    uint32_t type_scale;        // 4 bytes - Scale (DECIMAL only)
    uint16_t element_type;      // 2 bytes - For ARRAY types
    uint16_t array_dimensions;  // 2 bytes - For ARRAY types (0 = not array)

    // Constraints and attributes
    uint8_t nullable;           // 1 byte - NULL allowed
    uint8_t has_default;        // 1 byte - Has default value
    uint8_t is_primary_key;     // 1 byte - Part of primary key
    uint8_t is_unique;          // 1 byte - Has unique constraint
    uint8_t is_foreign_key;     // 1 byte - Part of foreign key
    uint8_t is_generated;       // 1 byte - Computed/generated column
    uint8_t storage_type;       // 1 byte - PLAIN, EXTERNAL, EXTENDED, MAIN (TOAST)
    uint8_t reserved_flags;     // 1 byte - Future use

    // Default and check constraints
    uint32_t default_value_oid; // 4 bytes - OID to TOAST table for default
    uint32_t check_expr_oid;    // 4 bytes - OID to TOAST table for CHECK

    // Statistics (for optimizer)
    uint32_t avg_width;         // 4 bytes - Average column width in bytes
    float4 null_frac;           // 4 bytes - Fraction of NULL values
    int32_t n_distinct;         // 4 bytes - Number of distinct values

    // Metadata
    uint64_t created_time;      // 8 bytes - Column creation time
    uint32_t is_valid;          // 4 bytes - Deletion marker
    uint32_t padding;           // 4 bytes - Alignment
};
// Total: ~160 bytes per column (more efficient, more capable)
```

---

## 5. Index Metadata

### 5.1 In-Memory Structure

```cpp
struct IndexInfo {
    ID index_id;                    // Index UUID
    ID table_id;                    // Parent table
    std::string index_name;         // Index name
    uint32_t root_page;             // Root B-tree page
    bool is_unique;                 // Unique index
    std::vector<ID> column_ids;     // Indexed columns
    uint64_t created_time;          // Creation timestamp
};
```

**Location**: `catalog_manager.h:83-92`

### 5.2 On-Disk Structure

```cpp
struct IndexRecord {
    ID index_id;               // 16 bytes
    ID table_id;               // 16 bytes
    char index_name[128];      // 128 bytes - SQL standard compliant
    uint32_t root_page;        // 4 bytes
    uint8_t is_unique;         // 1 byte
    uint16_t column_count;     // 2 bytes
    ID column_ids[16];         // 256 bytes (max 16 columns)
    uint64_t created_time;     // 8 bytes
    uint32_t is_valid;         // 4 bytes
};
// Total: 16 + 16 + 128 + 4 + 1 + 2 + 256 + 8 + 4 = 435 bytes per index
```

**Location**: `catalog_manager.cpp:68-79`

### 5.3 Critical Problems

**✗ BLOCKING ISSUE #1: No Index Type Field**

Missing `index_type` field means no way to distinguish:
- B-tree indexes (default)
- Hash indexes
- GIN (Generalized Inverted Index) for full-text, JSON
- BRIN (Block Range Index) for large tables
- **VECTOR indexes** (HNSW, IVFFlat) for similarity search
- Bitmap indexes
- Spatial indexes (R-tree, GiST)

**Impact**:
- Cannot implement vector similarity search
- Cannot choose appropriate index scan method
- No index-specific optimizations possible

**✗ BLOCKING ISSUE #2: Missing Index Parameters**

Different index types need different parameters:

For **VECTOR indexes**:
- Distance function (L2, cosine, inner product)
- Index construction parameters (M, efConstruction for HNSW)
- Number of lists (for IVFFlat)

For **B-tree indexes**:
- Fill factor
- Sort direction per column (ASC/DESC)
- NULLS FIRST/LAST per column

For **GIN indexes**:
- Fast update mode
- Pending list size

For **Hash indexes**:
- Bucket count

**✗ BLOCKING ISSUE #3: No Partial Index Support**

No `where_clause` field for partial indexes:
```sql
CREATE INDEX idx_active_users ON users(email) WHERE active = true;
```

**✗ BLOCKING ISSUE #4: No Expression Index Support**

Cannot store expression for indexes like:
```sql
CREATE INDEX idx_lower_email ON users(LOWER(email));
```

**✗ BLOCKING ISSUE #5: Limited to 16 Columns**

`ID column_ids[16]` hard limit is insufficient for:
- Covering indexes with many columns
- Composite indexes on wide tables
- Should use dynamic array or TOAST

### 5.4 Required Changes to IndexRecord

**Proposed new structure**:

```cpp
struct IndexRecord {
    // Identity
    ID index_id;               // 16 bytes - Index UUID
    ID table_id;               // 16 bytes - Parent table
    char index_name[64];       // 64 bytes - Index name

    // Index type and properties
    uint16_t index_type;       // 2 bytes - IndexType enum (BTREE, HASH, GIN, VECTOR, etc.)
    uint8_t is_unique;         // 1 byte - Unique constraint
    uint8_t is_primary;        // 1 byte - Primary key index
    uint8_t is_clustered;      // 1 byte - Clustered index (table ordered by this)
    uint8_t is_partial;        // 1 byte - Has WHERE clause
    uint8_t is_expression;     // 1 byte - Expression index
    uint8_t reserved_flags;    // 1 byte - Future use

    // Physical storage
    uint32_t root_page;        // 4 bytes - Root page of index structure
    uint32_t index_size;       // 4 bytes - Total pages in index

    // Indexed columns
    uint16_t column_count;     // 2 bytes - Number of indexed columns/expressions
    uint16_t include_count;    // 2 bytes - Number of INCLUDE columns (covering index)
    uint32_t column_array_oid; // 4 bytes - TOAST OID for column ID array

    // Index-specific parameters
    uint32_t params_oid;       // 4 bytes - TOAST OID for index parameters JSON/blob
    uint32_t where_clause_oid; // 4 bytes - TOAST OID for partial index WHERE clause
    uint32_t expr_list_oid;    // 4 bytes - TOAST OID for expression list

    // Statistics
    uint64_t num_tuples;       // 8 bytes - Number of index entries
    uint64_t num_pages;        // 8 bytes - Number of pages used

    // Metadata
    uint64_t created_time;     // 8 bytes - Index creation time
    uint64_t last_rebuild;     // 8 bytes - Last rebuild time
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~180 bytes + TOAST references
```

---

## 6. Missing Catalog Tables

The current catalog is missing several critical system tables:

### 6.1 Missing: sys_constraints Table

**Purpose**: Store PRIMARY KEY, FOREIGN KEY, CHECK, UNIQUE constraints

```cpp
struct ConstraintRecord {
    ID constraint_id;          // 16 bytes - Constraint UUID
    ID table_id;               // 16 bytes - Parent table
    char constraint_name[64];  // 64 bytes - Constraint name
    uint8_t constraint_type;   // 1 byte - PK, FK, CHECK, UNIQUE, NOT NULL
    uint8_t is_deferrable;     // 1 byte - Deferrable constraint
    uint8_t initially_deferred;// 1 byte - Initially deferred
    uint8_t reserved;          // 1 byte - Alignment

    // For FK constraints
    ID referenced_table_id;    // 16 bytes - Referenced table (FK only)
    uint8_t on_delete;         // 1 byte - CASCADE, SET NULL, RESTRICT, etc.
    uint8_t on_update;         // 1 byte - CASCADE, SET NULL, RESTRICT, etc.
    uint16_t match_type;       // 2 bytes - MATCH FULL, PARTIAL, SIMPLE

    // Column lists
    uint32_t columns_oid;      // 4 bytes - TOAST OID for column list
    uint32_t ref_columns_oid;  // 4 bytes - TOAST OID for referenced columns (FK)

    // CHECK constraint
    uint32_t check_expr_oid;   // 4 bytes - TOAST OID for CHECK expression

    // Metadata
    uint64_t created_time;     // 8 bytes - Creation time
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~156 bytes per constraint
```

### 6.2 Missing: sys_sequences Table

**Purpose**: Store SEQUENCE metadata for auto-increment

```cpp
struct SequenceRecord {
    ID sequence_id;            // 16 bytes - Sequence UUID
    ID schema_id;              // 16 bytes - Parent schema
    char sequence_name[64];    // 64 bytes - Sequence name

    int64_t current_value;     // 8 bytes - Current value
    int64_t increment_by;      // 8 bytes - Increment step
    int64_t min_value;         // 8 bytes - Minimum value
    int64_t max_value;         // 8 bytes - Maximum value
    int64_t cache_size;        // 8 bytes - Cache size

    uint8_t cycle;             // 1 byte - Cycle on overflow
    uint8_t reserved[7];       // 7 bytes - Alignment

    uint64_t created_time;     // 8 bytes - Creation time
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~176 bytes per sequence
```

### 6.3 Missing: sys_views Table

**Purpose**: Store view definitions

```cpp
struct ViewRecord {
    ID view_id;                // 16 bytes - View UUID (same as table_id)
    ID schema_id;              // 16 bytes - Parent schema
    char view_name[64];        // 64 bytes - View name

    uint32_t definition_oid;   // 4 bytes - TOAST OID for SELECT query
    uint8_t is_materialized;   // 1 byte - Materialized view
    uint8_t is_updatable;      // 1 byte - Updatable view (WITH CHECK OPTION)
    uint8_t check_option;      // 1 byte - LOCAL or CASCADED
    uint8_t reserved;          // 1 byte - Alignment

    uint64_t created_time;     // 8 bytes - Creation time
    uint64_t last_refresh;     // 8 bytes - Last refresh (materialized views)
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~140 bytes per view
```

### 6.4 Missing: sys_triggers Table

**Purpose**: Store trigger definitions

```cpp
struct TriggerRecord {
    ID trigger_id;             // 16 bytes - Trigger UUID
    ID table_id;               // 16 bytes - Parent table
    char trigger_name[64];     // 64 bytes - Trigger name

    uint8_t trigger_type;      // 1 byte - BEFORE, AFTER, INSTEAD OF
    uint8_t trigger_events;    // 1 byte - INSERT, UPDATE, DELETE (bitfield)
    uint8_t for_each;          // 1 byte - ROW or STATEMENT
    uint8_t enabled;           // 1 byte - Trigger enabled

    uint32_t when_clause_oid;  // 4 bytes - TOAST OID for WHEN condition
    uint32_t action_oid;       // 4 bytes - TOAST OID for trigger action

    int16_t execution_order;   // 2 bytes - Execution order (multiple triggers)
    uint16_t reserved;         // 2 bytes - Alignment

    uint64_t created_time;     // 8 bytes - Creation time
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~140 bytes per trigger
```

### 6.5 Missing: sys_permissions Table

**Purpose**: Store table/column level permissions

```cpp
struct PermissionRecord {
    ID permission_id;          // 16 bytes - Permission UUID
    ID object_id;              // 16 bytes - Table/View/Schema ID
    ID grantee_id;             // 16 bytes - User/Role UUID
    ID grantor_id;             // 16 bytes - Who granted this permission

    uint8_t object_type;       // 1 byte - TABLE, VIEW, SCHEMA, etc.
    uint8_t permission_type;   // 1 byte - SELECT, INSERT, UPDATE, DELETE, etc.
    uint8_t is_grantable;      // 1 byte - WITH GRANT OPTION
    uint8_t reserved;          // 1 byte - Alignment

    uint32_t column_id;        // 4 bytes - Column-level permission (0 = table-level)

    uint64_t granted_time;     // 8 bytes - When granted
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~108 bytes per permission
```

### 6.6 Missing: sys_statistics Table

**Purpose**: Store column statistics for query optimizer

```cpp
struct StatisticsRecord {
    ID stats_id;               // 16 bytes - Statistics UUID
    ID table_id;               // 16 bytes - Parent table
    ID column_id;              // 16 bytes - Column (or 0 for multi-column stats)

    uint32_t stats_kind;       // 4 bytes - Type of statistics
    float4 null_frac;          // 4 bytes - Fraction of NULLs
    float4 avg_width;          // 4 bytes - Average column width
    int32_t n_distinct;        // 4 bytes - Number of distinct values

    uint32_t histogram_oid;    // 4 bytes - TOAST OID for histogram
    uint32_t mcv_oid;          // 4 bytes - TOAST OID for most common values
    uint32_t correlation;      // 4 bytes - Correlation coefficient

    uint64_t last_analyze;     // 8 bytes - Last ANALYZE time
    uint32_t is_valid;         // 4 bytes - Deletion marker
    uint32_t padding;          // 4 bytes - Alignment
};
// Total: ~108 bytes per statistics entry
```

---

## 7. Recommendations Summary

### 7.1 Immediate Critical Fixes (Blocking)

**Priority 1 - Type Metadata**:
1. ✗ Add `type_precision` and `type_scale` to ColumnRecord
2. ✗ Add `column_position` (ordinal) to ColumnRecord
3. ✗ Add `index_type` enum to IndexRecord
4. ✗ Add `index_params` TOAST reference to IndexRecord

**Priority 2 - Constraint Support**:
1. ✗ Create `sys_constraints` table
2. ✗ Add constraint fields to ColumnRecord (is_primary_key, is_unique)
3. ✗ Link constraints to indexes

**Priority 3 - Missing System Tables**:
1. ✗ Create `sys_sequences` table
2. ✗ Create `sys_views` table
3. ✗ Create `sys_triggers` table
4. ✗ Create `sys_permissions` table
5. ✗ Create `sys_statistics` table

### 7.2 Medium Priority Enhancements

1. Add table-level metadata:
   - Table type (heap, temp, foreign, materialized)
   - Storage parameters
   - Tablespace support

2. Improve schema metadata:
   - Schema-level permissions
   - Default tablespace
   - Search path support

3. Add audit fields:
   - Last modified time
   - Modified by user
   - Version number

### 7.3 Long-term Architectural Changes

1. **TOAST Integration**:
   - Move large fields (defaults, expressions) to TOAST
   - Reduce fixed record sizes
   - Support unlimited-length metadata

2. **System Catalog Versioning**:
   - Add catalog version number
   - Support online catalog upgrades
   - Migration scripts for schema changes

3. **Distributed Catalog**:
   - Prepare for distributed deployments
   - Catalog replication
   - Cross-node schema synchronization

4. **Catalog Performance**:
   - Add B-tree indexes on system tables
   - Cache frequently accessed metadata
   - Lazy loading for large catalogs

---

## 8. Impact on Vector Index Implementation

**Blocking Issue**: Without fixing IndexRecord, we **cannot implement vector indexes**.

Required for vector index:
1. `index_type = INDEX_TYPE_VECTOR` (e.g., HNSW or IVFFlat)
2. `index_params` storing:
   - Distance function (L2, cosine, inner product)
   - HNSW parameters (M, efConstruction, efSearch)
   - IVFFlat parameters (n_lists)
3. Link to VECTOR(n) column with correct dimensions

**Without these**:
- Cannot distinguish vector index from B-tree index
- Cannot store construction parameters
- Cannot validate dimension compatibility
- Cannot choose correct scan algorithm

**Recommendation**: Fix ColumnRecord and IndexRecord **before** implementing vector index.

---

## 9. Compliance Issues

### 9.1 SQL Standard Compliance

The SQL:2023 standard requires:
- ✗ INFORMATION_SCHEMA views (we have none)
- ✗ CHECK constraints (no catalog table)
- ✗ FOREIGN KEY constraints (no catalog table)
- ✗ UNIQUE constraints (no catalog table)
- ✗ Column defaults with precision (DECIMAL, CHAR, VARCHAR)
- ✗ Sequence objects (no catalog table)

### 9.2 PostgreSQL Compatibility

PostgreSQL's system catalog includes:
- `pg_class` - Tables, indexes, sequences, views (we have partial)
- `pg_attribute` - Columns (we have, but missing fields)
- `pg_constraint` - Constraints (we don't have)
- `pg_index` - Indexes (we have, but missing fields)
- `pg_statistic` - Column statistics (we don't have)
- `pg_trigger` - Triggers (we don't have)
- `pg_depend` - Dependencies (we don't have)

**Compatibility gap**: Significant. Tools expecting PostgreSQL catalog will fail.

---

## 10. Conclusion

The current ScratchBird system catalog is a **minimal viable implementation** suitable for basic table creation and simple queries. However, it has **critical gaps** that block:

1. ✗ Full support for new type system (VECTOR, DECIMAL precision/scale)
2. ✗ Constraint enforcement (PK, FK, CHECK, UNIQUE)
3. ✗ Advanced index types (vector indexes, partial indexes, expression indexes)
4. ✗ Query optimization (no statistics, no column ordering)
5. ✗ SQL standard compliance (missing INFORMATION_SCHEMA)

**Immediate Action Required**:
- Extend ColumnRecord with type_precision, type_scale, ordinal
- Extend IndexRecord with index_type, index_params
- Create sys_constraints table

**Estimated Work**:
- Critical fixes: 2-3 days
- All missing tables: 1-2 weeks
- INFORMATION_SCHEMA views: 1 week
- Full PostgreSQL compatibility: 3-4 weeks

---

**End of Audit Report**
