# ScratchBird INDEX - Complete Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

Indexes in ScratchBird provide high-performance data access paths for queries, dramatically improving search and retrieval operations. ScratchBird extends traditional B-Tree indexing with multiple specialized index types optimized for different data patterns and query workloads.

### Key Features and Capabilities

- **Multiple Index Types**: B-Tree, Hash, GIN, Bitmap, Spatial (R-Tree), and Partial Hash indexes
- **Hierarchical Schema Support**: Full support for 3-level qualified names (`schema.subschema.index`)
- **Advanced Optimization**: Query optimizer integration with cost-based index selection
- **Partial Indexes**: Conditional indexing with WHERE clause support
- **Composite Indexes**: Multi-column indexes with expression support
- **Spatial Data Support**: Full geometric/geographic indexing capabilities

### ScratchBird-Specific Enhancements

1. **Partial Hash Indexes**: O(1) lookup performance with WHERE clause filtering
2. **GIN Indexes**: Full-text search and array data indexing
3. **Bitmap Indexes**: Optimized for low-cardinality data with compression
4. **Spatial Indexes**: R-Tree implementation for geometric/geographic data
5. **Enhanced Index Options**: Fine-grained tuning for each index type
6. **Schema-Aware Indexing**: Integration with hierarchical schema system

---

## DDL Syntax Reference

### CREATE INDEX

Creates a new index to accelerate data access for specified columns or expressions.

#### **Basic Syntax**
```sql
CREATE [UNIQUE] [ASC | DESC] INDEX [IF NOT EXISTS] [schema_path.]index_name
    ON [schema_path.]table_name (column_list)
    [USING index_type]
    [WITH (index_options)]
    [WHERE condition]
```

#### **Complete Syntax**
```sql
CREATE [UNIQUE] [ASC | DESC] [index_type] INDEX [IF NOT EXISTS] qualified_index_name
    [ACTIVE | INACTIVE]
    ON qualified_table_name (column_specification [, column_specification] ...)
    [USING {BTREE | HASH | GIN | BITMAP | RTREE}]
    [WITH (option_name = value [, option_name = value] ...)]
    [WHERE condition]
```

#### **Parameter Descriptions**

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `UNIQUE` | keyword | Enforce unique values | FALSE |
| `ASC/DESC` | keyword | Sort direction | ASC |
| `index_type` | keyword | Specialized index type prefix | B-Tree |
| `IF NOT EXISTS` | keyword | Skip creation if index exists | FALSE |
| `qualified_index_name` | identifier | Hierarchical index name (up to 3 levels) | Required |
| `ACTIVE/INACTIVE` | keyword | Index state | ACTIVE |
| `qualified_table_name` | identifier | Target table name | Required |
| `column_specification` | list | Columns or expressions to index | Required |
| `USING` | clause | Explicit index type specification | BTREE |
| `WITH` | clause | Index-specific options | None |
| `WHERE` | clause | Partial index condition | None |

#### **Index Types**

##### **1. B-Tree Index (Default)**
Traditional balanced tree index, optimal for range queries and sorting.

```sql
-- Basic B-Tree index
CREATE INDEX idx_customer_name 
    ON customers (last_name, first_name);

-- Explicit B-Tree specification
CREATE INDEX idx_order_date 
    ON sales.orders.order_headers (order_date)
    USING BTREE;

-- Descending B-Tree index for recent data queries
CREATE DESC INDEX idx_recent_orders
    ON orders (order_date);
```

##### **2. Hash Index**
O(1) average lookup time for equality comparisons.

```sql
-- Hash index for exact lookups
CREATE INDEX idx_customer_id_hash
    ON customers (customer_id)
    USING HASH;

-- Hash index with custom bucket configuration
CREATE INDEX idx_product_code_hash
    ON inventory.products.catalog (product_code)
    USING HASH
    WITH (BUCKETS = 2048, LOAD_FACTOR = 75);
```

##### **3. Partial Hash Index**
Hash index with WHERE clause for selective indexing.

```sql
-- Partial hash index for active customers only
CREATE UNIQUE PARTIAL HASH INDEX idx_active_customer_email
    ON crm.customers.profiles (email)
    WHERE is_active = TRUE;

-- Partial hash index for recent orders
CREATE PARTIAL HASH INDEX idx_recent_order_status
    ON sales.orders.headers (status)  
    WHERE order_date >= CURRENT_DATE - 30;
```

##### **4. GIN Index (Generalized Inverted Index)**
Optimized for full-text search and array data.

```sql
-- GIN index for full-text search
CREATE INDEX idx_product_description_gin
    ON products (description)
    USING GIN
    WITH (FASTUPDATE = ON, PARSER = 'standard');

-- GIN index for array data
CREATE INDEX idx_product_tags_gin
    ON products (tags)
    USING GIN;

-- GIN index with stop words and stemming
CREATE INDEX idx_document_content_gin
    ON documents (content)
    USING GIN
    WITH (STOP_WORDS = ON, STEMMING = ON, CASE_SENSITIVE = OFF);
```

##### **5. Bitmap Index**
Highly compressed indexes for low-cardinality data.

```sql
-- Bitmap index for categorical data
CREATE INDEX idx_customer_status_bitmap
    ON customers (status)
    USING BITMAP
    WITH (COMPRESSION = ZLIB, CHUNK_SIZE = 1024);

-- Bitmap index for boolean flags
CREATE INDEX idx_employee_active_bitmap
    ON hr.employees.profiles (is_active)
    USING BITMAP;
```

##### **6. Spatial Index (R-Tree)**
Optimized for geometric and geographic data.

```sql
-- Spatial index for geographic coordinates
CREATE INDEX idx_location_spatial
    ON locations (coordinates)
    USING RTREE
    WITH (SRID = 4326, SPLIT_STRATEGY = 'RSTAR');

-- Spatial index for geometric shapes
CREATE INDEX idx_polygon_spatial
    ON geographic.regions.boundaries (polygon_data)
    USING RTREE
    WITH (SRID = 3857);
```

#### **Advanced Examples**

##### **Multi-Column Composite Index**
```sql
-- Composite index for complex queries
CREATE INDEX idx_order_customer_date
    ON sales.orders.headers (customer_id, order_date, status)
    WITH (FILL_FACTOR = 85);
```

##### **Expression-Based Index**
```sql
-- Index on computed expression
CREATE INDEX idx_customer_full_name
    ON customers (UPPER(last_name || ', ' || first_name));

-- Index on date extraction
CREATE INDEX idx_order_month_year  
    ON orders (EXTRACT(YEAR FROM order_date), EXTRACT(MONTH FROM order_date));
```

##### **Conditional Index with Complex WHERE**
```sql
-- Partial index for complex business rules
CREATE INDEX idx_high_value_recent_orders
    ON orders (customer_id, total_amount)
    WHERE total_amount > 1000 
      AND order_date >= CURRENT_DATE - 90
      AND status NOT IN ('CANCELLED', 'REFUNDED');
```

##### **Schema-Qualified Index Creation**
```sql
-- Index in hierarchical schema
CREATE UNIQUE INDEX finance.reporting.idx_monthly_summary_period
    ON finance.reporting.monthly_summaries (summary_year, summary_month);

-- Cross-schema index reference
CREATE INDEX crm.analytics.idx_customer_orders
    ON sales.orders.headers (customer_id)
    WHERE EXISTS (
        SELECT 1 FROM crm.customers.profiles c 
        WHERE c.customer_id = sales.orders.headers.customer_id
          AND c.tier = 'PREMIUM'
    );
```

### ALTER INDEX

Modifies the state of an existing index (primarily activation/deactivation).

#### **Syntax**
```sql
ALTER INDEX qualified_index_name {ACTIVE | INACTIVE}
```

#### **Examples**
```sql
-- Activate an inactive index
ALTER INDEX idx_customer_name ACTIVE;

-- Deactivate an index for maintenance
ALTER INDEX finance.reporting.idx_monthly_summary INACTIVE;

-- Deactivate index before bulk data loading
ALTER INDEX idx_large_table_key INACTIVE;
-- ... perform bulk insert ...
ALTER INDEX idx_large_table_key ACTIVE;
```

### DROP INDEX

Permanently removes an index from the database.

#### **Syntax**
```sql
DROP INDEX [IF EXISTS] qualified_index_name
```

#### **Examples**
```sql
-- Standard drop
DROP INDEX idx_customer_name;

-- Safe drop with hierarchical schema
DROP INDEX IF EXISTS finance.accounting.idx_temp_calculations;

-- Drop multiple indexes
DROP INDEX IF EXISTS old_customer_index;
DROP INDEX IF EXISTS old_order_index;
DROP INDEX IF EXISTS old_product_index;
```

---

## Index Type Specifications

### B-Tree Index Options

```sql
CREATE INDEX idx_standard_btree
    ON table_name (column_name)
    USING BTREE
    WITH (
        FILL_FACTOR = 85,           -- Page fill percentage (50-100)
        SPLIT_POINT = 50,          -- Split position percentage
        UNIQUE_NULLS = OFF         -- Allow multiple NULL values
    );
```

### Hash Index Options

```sql
CREATE INDEX idx_hash_optimized
    ON table_name (column_name)
    USING HASH
    WITH (
        BUCKETS = 1024,            -- Number of hash buckets (64-65536)
        LOAD_FACTOR = 75,          -- Maximum load factor percentage
        ALGORITHM = 'MURMUR3',     -- Hash algorithm: CRC32, MURMUR3
        EXPANDABLE = ON            -- Allow dynamic expansion
    );
```

### GIN Index Options

```sql
CREATE INDEX idx_gin_fulltext
    ON table_name (text_column)
    USING GIN
    WITH (
        FASTUPDATE = ON,           -- Enable fast update mode
        STOP_WORDS = ON,           -- Filter common stop words
        STEMMING = ON,             -- Enable word stemming
        CASE_SENSITIVE = OFF,      -- Case-insensitive tokenization
        MIN_TOKEN_LENGTH = 3,      -- Minimum token length
        MAX_TOKEN_LENGTH = 255,    -- Maximum token length
        PARSER = 'standard'        -- Text parser to use
    );
```

### Bitmap Index Options

```sql
CREATE INDEX idx_bitmap_compressed
    ON table_name (category_column)
    USING BITMAP
    WITH (
        COMPRESSION = 'ZLIB',      -- Compression: NONE, ZLIB, LZ4
        CHUNK_SIZE = 1024,         -- Compression chunk size
        NULL_BITMAP = ON           -- Separate NULL value bitmap
    );
```

### Spatial Index Options

```sql
CREATE INDEX idx_spatial_geographic
    ON table_name (geometry_column)
    USING RTREE
    WITH (
        SRID = 4326,               -- Spatial Reference System ID
        SPLIT_STRATEGY = 'RSTAR',  -- Split strategy: QUADRATIC, LINEAR, RSTAR
        MAX_ENTRIES = 50,          -- Maximum entries per node
        MIN_ENTRIES = 20           -- Minimum entries per node
    );
```

---

## Usage Examples

### Performance-Optimized Indexing

#### **E-commerce Query Optimization**
```sql
-- Customer lookup optimization
CREATE UNIQUE INDEX idx_customer_email_hash
    ON crm.customers.profiles (email)
    USING HASH
    WITH (BUCKETS = 4096, LOAD_FACTOR = 70);

-- Order search optimization
CREATE INDEX idx_order_customer_date_status
    ON sales.orders.headers (customer_id, order_date DESC, status)
    WHERE status IN ('PENDING', 'CONFIRMED', 'SHIPPED');

-- Product catalog search
CREATE INDEX idx_product_search_gin
    ON inventory.products.catalog (product_name, description)
    USING GIN
    WITH (FASTUPDATE = ON, STOP_WORDS = ON);
```

#### **Financial Data Indexing**
```sql
-- Transaction processing
CREATE INDEX idx_transaction_account_date
    ON finance.transactions.log (account_number, transaction_date DESC)
    WHERE amount > 0;

-- Account balance calculation
CREATE INDEX idx_balance_calculation
    ON finance.transactions.log (account_number, transaction_date)
    INCLUDE (amount, transaction_type);

-- Audit trail search
CREATE INDEX idx_audit_user_date_gin
    ON finance.audit.log (user_id, (operation_details || ' ' || table_name))
    USING GIN;
```

### Analytics and Reporting Indexes

#### **Business Intelligence Optimization**
```sql
-- Time-series data analysis
CREATE INDEX idx_sales_time_series
    ON sales.fact.daily_sales (
        sale_date DESC,
        product_category,
        region_code
    )
    WHERE sale_date >= '2020-01-01';

-- Customer segmentation
CREATE INDEX idx_customer_segmentation
    ON crm.analytics.customer_metrics (
        total_orders,
        lifetime_value DESC,
        last_order_date
    )
    WHERE is_active = TRUE;

-- Geographic analysis
CREATE INDEX idx_store_locations_spatial
    ON retail.stores.locations (store_coordinates)
    USING RTREE
    WITH (SRID = 4326, SPLIT_STRATEGY = 'RSTAR');
```

### Specialized Use Cases

#### **Full-Text Search Implementation**
```sql
-- Document search system
CREATE INDEX idx_document_fulltext
    ON documents.content.articles (title, body, tags)
    USING GIN
    WITH (
        PARSER = 'standard',
        STEMMING = ON,
        STOP_WORDS = ON,
        MIN_TOKEN_LENGTH = 2
    );

-- Search by document metadata
CREATE INDEX idx_document_metadata
    ON documents.content.articles (
        author_id,
        publication_date DESC,
        category
    )
    WHERE status = 'PUBLISHED';
```

#### **IoT and Sensor Data**
```sql
-- Sensor data time-series
CREATE INDEX idx_sensor_readings_time
    ON iot.sensors.readings (
        sensor_id,
        timestamp DESC,
        reading_type
    )
    WHERE timestamp >= CURRENT_TIMESTAMP - INTERVAL '1 year';

-- Anomaly detection
CREATE INDEX idx_sensor_anomalies
    ON iot.sensors.readings (sensor_id, reading_value)
    WHERE reading_value NOT BETWEEN normal_min AND normal_max;

-- Geographic sensor distribution
CREATE INDEX idx_sensor_locations_spatial
    ON iot.sensors.metadata (installation_coordinates)
    USING RTREE
    WITH (SRID = 4326);
```

### Index Maintenance and Optimization

#### **Bulk Data Loading**
```sql
-- Prepare for bulk insert
ALTER INDEX idx_large_table_primary INACTIVE;
ALTER INDEX idx_large_table_secondary INACTIVE;

-- Perform bulk data loading
INSERT INTO large_table SELECT * FROM staging_table;

-- Reactivate indexes
ALTER INDEX idx_large_table_primary ACTIVE;
ALTER INDEX idx_large_table_secondary ACTIVE;
```

#### **Index Health Monitoring**
```sql
-- Monitor index usage statistics
SELECT 
    i.RDB$INDEX_NAME,
    i.RDB$RELATION_NAME,
    i.RDB$UNIQUE_FLAG,
    i.RDB$INDEX_TYPE,
    s.RDB$STATISTICS
FROM RDB$INDICES i
LEFT JOIN RDB$INDEX_SEGMENTS s ON i.RDB$INDEX_NAME = s.RDB$INDEX_NAME
WHERE i.RDB$SYSTEM_FLAG = 0
ORDER BY i.RDB$RELATION_NAME, i.RDB$INDEX_NAME;

-- Check spatial index effectiveness
SELECT 
    idx.RDB$INDEX_NAME,
    idx.RDB$RELATION_NAME,
    spatial_stats.node_count,
    spatial_stats.avg_entries_per_node,
    spatial_stats.tree_depth
FROM RDB$INDICES idx
JOIN SPATIAL_INDEX_STATISTICS spatial_stats 
    ON idx.RDB$INDEX_NAME = spatial_stats.index_name
WHERE idx.RDB$INDEX_TYPE = 4; -- RTREE indexes
```

---

## Implementation Details

### Primary Implementation Files

| File | Purpose | Key Components |
|------|---------|----------------|
| `src/dsql/parse.y` | SQL grammar for index DDL | `create_index`, `alter_index`, `drop_index` rules |
| `src/dsql/DdlNodes.h` | DDL node definitions | `CreateIndexNode`, `AlterIndexNode`, `DropIndexNode` |
| `src/dsql/DdlNodes.epp` | DDL node implementations | Index creation/modification logic |
| `src/jrd/idx.cpp` | Index management | Physical index operations |
| `src/jrd/IndexTypes/` | Index type implementations | B-Tree, Hash, GIN, Bitmap, Spatial |
| `src/jrd/constants.h` | Index constants | Index type definitions and limits |

### Core Classes and Functions

#### **DDL Node Classes**
```cpp
// Index creation and modification
class CreateIndexNode : public DdlNode {
    struct Definition {
        QualifiedName relation;                    // Target table
        ScratchBird::ObjectsArray<MetaName> columns;  // Index columns
        ScratchBird::TriState unique;              // Uniqueness constraint
        ScratchBird::TriState descending;          // Sort direction
        MetaName indexTypeName;                    // Index type
        // Type-specific options
        ScratchBird::ObjectsArray<ScratchBird::Pair<MetaName, ScratchBird::string>> ginOptions;
        ScratchBird::ObjectsArray<ScratchBird::Pair<MetaName, ScratchBird::string>> bitmapOptions;
        ScratchBird::ObjectsArray<ScratchBird::Pair<MetaName, ScratchBird::string>> spatialOptions;
    };
    
    void execute(thread_db* tdbb) override;
    void processGinIndexOptions(thread_db* tdbb, jrd_tra* transaction, Definition& definition, ValueListNode* options);
    void processBitmapIndexOptions(thread_db* tdbb, jrd_tra* transaction, Definition& definition, ValueListNode* options);
    void processSpatialIndexOptions(thread_db* tdbb, jrd_tra* transaction, Definition& definition, ValueListNode* options);
};

// Index state modification
class AlterIndexNode : public DdlNode {
    QualifiedName name;     // Index name
    bool active;            // Target state
    
    void execute(thread_db* tdbb) override;
};

// Index deletion
class DropIndexNode : public DdlNode {
    QualifiedName name;     // Index name
    bool silent;            // IF EXISTS flag
    
    void execute(thread_db* tdbb) override;
    static bool deleteSegmentRecords(thread_db* tdbb, jrd_tra* transaction, const QualifiedName& name);
};
```

#### **Key Functions**
- `IDX_create_index()` - Create physical index structure
- `IDX_delete_index()` - Remove index from storage
- `IDX_modify_index()` - Change index state
- `validateIndexSuitability()` - Check column compatibility with index type
- `optimizeIndexParameters()` - Auto-tune index options

### System Catalog Entries

#### **RDB$INDICES** (Index metadata)
```sql
CREATE TABLE RDB$INDICES (
    RDB$INDEX_NAME VARCHAR(63) NOT NULL,
    RDB$RELATION_NAME VARCHAR(63) NOT NULL,
    RDB$INDEX_ID SMALLINT,
    RDB$UNIQUE_FLAG SMALLINT,
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,
    RDB$SEGMENT_COUNT SMALLINT,
    RDB$INDEX_INACTIVE SMALLINT,
    RDB$INDEX_TYPE SMALLINT,        -- ScratchBird: Index type (0=BTREE, 1=HASH, 2=GIN, 3=BITMAP, 4=RTREE, 6=PARTIAL_HASH)
    RDB$FOREIGN_KEY_NAME VARCHAR(63),
    RDB$SYSTEM_FLAG SMALLINT,
    RDB$EXPRESSION_BLR BLOB SUB_TYPE BLR,
    RDB$EXPRESSION_SOURCE BLOB SUB_TYPE TEXT,
    RDB$STATISTICS DOUBLE PRECISION,
    RDB$CONDITION_BLR BLOB SUB_TYPE BLR,    -- ScratchBird: Partial index condition
    RDB$CONDITION_SOURCE BLOB SUB_TYPE TEXT, -- ScratchBird: Partial index condition source
    PRIMARY KEY (RDB$INDEX_NAME)
);
```

#### **RDB$INDEX_SEGMENTS** (Index column definitions)
```sql
CREATE TABLE RDB$INDEX_SEGMENTS (
    RDB$INDEX_NAME VARCHAR(63) NOT NULL,
    RDB$FIELD_NAME VARCHAR(63) NOT NULL,
    RDB$FIELD_POSITION SMALLINT,
    RDB$STATISTICS DOUBLE PRECISION,
    PRIMARY KEY (RDB$INDEX_NAME, RDB$FIELD_POSITION)
);
```

#### **RDB$INDEX_OPTIONS** (Index-specific options)
```sql
-- ScratchBird enhancement for storing index-specific options
CREATE TABLE RDB$INDEX_OPTIONS (
    RDB$INDEX_NAME VARCHAR(63) NOT NULL,
    RDB$OPTION_NAME VARCHAR(63) NOT NULL,
    RDB$OPTION_VALUE VARCHAR(255),
    RDB$OPTION_TYPE SMALLINT,        -- 0=STRING, 1=INTEGER, 2=BOOLEAN
    PRIMARY KEY (RDB$INDEX_NAME, RDB$OPTION_NAME)
);
```

### Index Type Constants

From `src/jrd/constants.h`:
```cpp
// Index type identifiers for pluggable index system
inline constexpr int IDX_TYPE_BTREE = 0;           // B-Tree index (default)
inline constexpr int IDX_TYPE_HASH = 1;            // Hash index for equality
inline constexpr int IDX_TYPE_GIN = 2;             // Generalized Inverted Index
inline constexpr int IDX_TYPE_BITMAP = 3;          // Bitmap index
inline constexpr int IDX_TYPE_RTREE = 4;           // R-Tree spatial index
inline constexpr int IDX_TYPE_BRIN = 5;            // Block Range Index
inline constexpr int IDX_TYPE_PARTIAL_HASH = 6;    // Partial hash index

// Hash index constants
inline constexpr USHORT HASH_DEFAULT_BUCKETS = 1024;
inline constexpr UCHAR HASH_MAX_LOAD_FACTOR = 75;
inline constexpr UCHAR HASH_TARGET_LOAD_FACTOR = 50;

// GIN index constants
inline constexpr USHORT GIN_DEFAULT_MIN_TOKEN_LENGTH = 3;
inline constexpr USHORT GIN_DEFAULT_MAX_TOKEN_LENGTH = 255;
inline constexpr ULONG GIN_MAX_POSTING_LIST_SIZE = 1048576;
```

### Index Processing Logic

#### **Index Creation Process**
1. **Parse Index Definition**: Extract columns, type, and options
2. **Validate Target Table**: Ensure table exists and permissions
3. **Schema Resolution**: Resolve hierarchical schema references
4. **Type Validation**: Check column compatibility with index type
5. **Option Processing**: Validate and store type-specific options
6. **Physical Creation**: Build index structure in storage
7. **Metadata Storage**: Update system catalog
8. **Statistics Initialization**: Generate initial optimizer statistics

#### **Index Type Selection Logic**
```cpp
// Index type selection based on query patterns
class IndexTypeSelector {
    IndexType selectOptimalType(const ColumnInfo& columns, const QueryPattern& patterns) {
        // Equality-only queries -> Hash or Partial Hash
        if (patterns.hasOnlyEquality()) {
            return patterns.hasSelectivity() ? IDX_TYPE_PARTIAL_HASH : IDX_TYPE_HASH;
        }
        
        // Text search queries -> GIN
        if (patterns.hasTextSearch() || columns.hasTextColumns()) {
            return IDX_TYPE_GIN;
        }
        
        // Low cardinality data -> Bitmap
        if (columns.hasLowCardinality()) {
            return IDX_TYPE_BITMAP;
        }
        
        // Spatial/geometric data -> R-Tree
        if (columns.hasSpatialData()) {
            return IDX_TYPE_RTREE;
        }
        
        // Default to B-Tree for range queries and sorting
        return IDX_TYPE_BTREE;
    }
};
```

---

## Administrative Notes

### Backup/Restore Considerations

#### **Index Backup**
- **Definition Storage**: Index definitions stored in system catalog
- **Type-Specific Options**: All index options preserved
- **Statistics Backup**: Optimizer statistics included in backup
- **Dependency Tracking**: Index-table relationships maintained

#### **Restore Process**
```sql
-- Indexes restored after tables in dependency order
-- 1. Tables restored first
-- 2. Primary key indexes created automatically
-- 3. User-defined indexes created based on type
-- 4. Statistics regenerated during restore
-- 5. Spatial indexes rebuilt with SRID validation
```

### Security Implications

#### **Index-Level Security**
- **Table Permissions**: Index operations require table-level permissions
- **Schema Integration**: Leverage hierarchical schema security
- **System Index Protection**: System indexes protected from modification
- **Cross-Schema References**: Security enforced for schema boundaries

### Performance Monitoring

#### **Index Performance Metrics**
- **Usage Statistics**: Query optimizer access patterns
- **Selectivity Analysis**: Index effectiveness measurements
- **Storage Utilization**: Page usage and fragmentation
- **Type-Specific Metrics**: Hash collision rates, GIN token statistics, etc.

#### **Index Performance Analysis**
```sql
-- Comprehensive index analysis
SELECT 
    idx.RDB$INDEX_NAME,
    idx.RDB$RELATION_NAME,
    idx.RDB$INDEX_TYPE,
    CASE idx.RDB$INDEX_TYPE
        WHEN 0 THEN 'BTREE'
        WHEN 1 THEN 'HASH'
        WHEN 2 THEN 'GIN'
        WHEN 3 THEN 'BITMAP'
        WHEN 4 THEN 'RTREE'
        WHEN 6 THEN 'PARTIAL_HASH'
        ELSE 'UNKNOWN'
    END as index_type_name,
    idx.RDB$STATISTICS,
    idx.RDB$INDEX_INACTIVE,
    seg.segment_count,
    COALESCE(opt.option_count, 0) as custom_options
FROM RDB$INDICES idx
LEFT JOIN (
    SELECT 
        RDB$INDEX_NAME, 
        COUNT(*) as segment_count
    FROM RDB$INDEX_SEGMENTS 
    GROUP BY RDB$INDEX_NAME
) seg ON idx.RDB$INDEX_NAME = seg.RDB$INDEX_NAME
LEFT JOIN (
    SELECT 
        RDB$INDEX_NAME, 
        COUNT(*) as option_count
    FROM RDB$INDEX_OPTIONS 
    GROUP BY RDB$INDEX_NAME
) opt ON idx.RDB$INDEX_NAME = opt.RDB$INDEX_NAME
WHERE idx.RDB$SYSTEM_FLAG = 0
ORDER BY idx.RDB$RELATION_NAME, idx.RDB$INDEX_NAME;
```

### Troubleshooting Tips

#### **Common Issues**

**1. Index Type Mismatch**
```sql
-- Problem: Creating hash index on text column with range queries
CREATE INDEX idx_description_hash 
    ON products (description) 
    USING HASH;
-- Error: Hash indexes don't support range queries

-- Solution: Use appropriate index type
CREATE INDEX idx_description_gin
    ON products (description)
    USING GIN;
```

**2. Partial Index Condition Issues**
```sql
-- Problem: Partial index not used by query
CREATE INDEX idx_active_customers 
    ON customers (name) 
    WHERE is_active = TRUE;

SELECT * FROM customers WHERE name = 'Smith';
-- Index not used because query doesn't match condition

-- Solution: Include condition in query
SELECT * FROM customers WHERE name = 'Smith' AND is_active = TRUE;
```

**3. Spatial Index SRID Mismatch**
```sql
-- Problem: Query using different SRID than index
CREATE INDEX idx_locations_spatial
    ON locations (coordinates)
    USING RTREE
    WITH (SRID = 4326);

-- Query with different SRID won't use index
SELECT * FROM locations 
WHERE ST_WITHIN(ST_Transform(coordinates, 3857), @search_area);

-- Solution: Use consistent SRID or transform data
SELECT * FROM locations 
WHERE ST_WITHIN(coordinates, ST_Transform(@search_area, 4326));
```

**4. GIN Index Token Length Issues**
```sql
-- Problem: Search terms too short for GIN index
CREATE INDEX idx_product_search
    ON products (description)
    USING GIN
    WITH (MIN_TOKEN_LENGTH = 4);

-- Search for "car" won't use index (length < 4)
-- Solution: Adjust token length or use different approach
CREATE INDEX idx_product_search_flexible
    ON products (description)
    USING GIN
    WITH (MIN_TOKEN_LENGTH = 2);
```

**5. Index Maintenance During Bulk Operations**
```sql
-- Problem: Slow bulk insert due to index maintenance
INSERT INTO large_table SELECT * FROM source_table; -- Very slow

-- Solution: Disable indexes during bulk operations
ALTER INDEX idx_large_table_key INACTIVE;
INSERT INTO large_table SELECT * FROM source_table;
ALTER INDEX idx_large_table_key ACTIVE;
```

