### DDL: Indexes

**What it is**

Indexes are database structures that improve query performance by providing fast access paths to table data. ScratchBird supports various index types including B-tree, hash, bitmap, GIN, and R-tree indexes, along with advanced features like partial indexes, expression indexes, and unique constraints. Indexes can dramatically speed up SELECT queries while adding overhead to INSERT, UPDATE, and DELETE operations.

**Why it matters**

- **Query Performance**: Indexes can reduce query time from hours to milliseconds on large tables
- **Uniqueness Enforcement**: UNIQUE indexes guarantee data integrity at the database level
- **Join Optimization**: Indexes on join columns enable efficient merge and hash joins
- **Sorting**: Indexes eliminate expensive sort operations for ORDER BY queries
- **Covering Indexes**: Can satisfy queries without accessing the table data

**How to use it**

Create indexes on columns frequently used in WHERE, JOIN, and ORDER BY clauses. Balance query performance gains against write overhead. Use EXPLAIN to verify index usage and consider partial or expression indexes for specialized access patterns.

## Index Types and Methods

ScratchBird supports multiple index methods (`src/engine/parser_ddl.cpp`):

### B-Tree Indexes (Default)

B-tree indexes maintain sorted data and support:
- Equality comparisons: `=`, `IN`
- Range queries: `<`, `<=`, `>`, `>=`, `BETWEEN`
- Pattern matching: `LIKE 'prefix%'`
- Sorting: `ORDER BY`
- NULL searches: `IS NULL`, `IS NOT NULL`

```sql
-- Default B-tree index
CREATE INDEX idx_users_email ON users(email);

-- Multi-column B-tree
CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date DESC);

-- B-tree with custom ordering
CREATE INDEX idx_products_category_price ON products(category, price DESC NULLS LAST);
```

### Hash Indexes

Hash indexes provide O(1) lookups for equality comparisons only:

```sql
-- Hash index for exact matches
CREATE INDEX idx_users_username_hash ON users USING HASH (username);

-- Useful for joins on equality
CREATE INDEX idx_orders_customer_hash ON orders USING HASH (customer_id);
```

### Bitmap Indexes

Bitmap indexes are efficient for low-cardinality columns:

```sql
-- Bitmap for status columns
CREATE INDEX idx_orders_status_bitmap ON orders USING BITMAP (status);

-- Multiple bitmap indexes can be combined
CREATE INDEX idx_users_active_bitmap ON users USING BITMAP (is_active);
CREATE INDEX idx_users_verified_bitmap ON users USING BITMAP (is_verified);
```

### GIN (Generalized Inverted) Indexes

GIN indexes support complex data types and containment queries:

```sql
-- GIN for array columns
CREATE INDEX idx_products_tags_gin ON products USING GIN (tags);

-- GIN for full-text search
CREATE INDEX idx_documents_search_gin ON documents USING GIN (to_tsvector('english', content));

-- GIN for JSONB
CREATE INDEX idx_events_data_gin ON events USING GIN (json_data);
```

### R-Tree Indexes

R-tree indexes optimize spatial queries:

```sql
-- R-tree for geometric data
CREATE INDEX idx_locations_coords_rtree ON locations USING RTREE (coordinates);

-- R-tree for range types
CREATE INDEX idx_bookings_period_rtree ON bookings USING RTREE (booking_period);
```

## CREATE INDEX

### Basic Syntax

```sql
CREATE [UNIQUE] INDEX [IF NOT EXISTS] index_name
ON table_name [USING method]
(column_expression [ASC|DESC] [NULLS {FIRST|LAST}] [, ...])
[WHERE predicate]
[TABLESPACE tablespace_name];
```

### Simple Indexes

```sql
-- Single column index
CREATE INDEX idx_users_created_at ON users(created_at);

-- Composite index (column order matters!)
CREATE INDEX idx_orders_customer_status ON orders(customer_id, status);

-- Descending index for reverse sorting
CREATE INDEX idx_logs_timestamp_desc ON logs(timestamp DESC);

-- NULL handling
CREATE INDEX idx_products_discount ON products(discount_rate DESC NULLS LAST);
```

### Unique Indexes

Unique indexes enforce uniqueness and provide fast lookups:

```sql
-- Simple unique index
CREATE UNIQUE INDEX idx_users_email_unique ON users(email);

-- Composite unique constraint
CREATE UNIQUE INDEX idx_accounts_branch_number ON accounts(branch_id, account_number);

-- Unique with NULL handling (multiple NULLs allowed)
CREATE UNIQUE INDEX idx_products_sku ON products(sku) WHERE sku IS NOT NULL;

-- Case-insensitive unique
CREATE UNIQUE INDEX idx_users_username_ci ON users(LOWER(username));
```

### Expression Indexes

Index computed values rather than raw columns:

```sql
-- Case-insensitive search
CREATE INDEX idx_users_email_lower ON users(LOWER(email));

-- Date extraction
CREATE INDEX idx_orders_year_month ON orders(
    EXTRACT(YEAR FROM order_date),
    EXTRACT(MONTH FROM order_date)
);

-- Computed values
CREATE INDEX idx_products_profit_margin ON products((price - cost) / price);

-- JSON field extraction
CREATE INDEX idx_events_user_id ON events((json_data->>'user_id')::INTEGER);

-- Complex expression
CREATE INDEX idx_logs_severity_hour ON logs(
    severity,
    DATE_TRUNC('hour', timestamp)
) WHERE severity IN ('ERROR', 'CRITICAL');
```

### Partial Indexes

Index only rows matching a predicate:

```sql
-- Index only active records
CREATE INDEX idx_users_email_active ON users(email) 
WHERE is_active = TRUE;

-- Index recent data
CREATE INDEX idx_orders_recent ON orders(order_date, customer_id)
WHERE order_date >= DATE '2024-01-01';

-- Exclude NULLs
CREATE INDEX idx_products_discount_nonnull ON products(discount_rate)
WHERE discount_rate IS NOT NULL;

-- Complex conditions
CREATE INDEX idx_high_value_orders ON orders(customer_id, total)
WHERE total > 1000 AND status != 'cancelled';
```

### Covering Indexes (INCLUDE)

Include additional columns for index-only scans:

```sql
-- Include non-key columns
CREATE INDEX idx_orders_customer_covering ON orders(customer_id)
INCLUDE (order_date, total, status);

-- Covering index for common query
CREATE INDEX idx_products_category_covering ON products(category_id)
INCLUDE (name, price, in_stock)
WHERE in_stock = TRUE;
```

### Collation in Indexes

Specify collation for text comparisons:

```sql
-- Case-insensitive collation
CREATE INDEX idx_users_name_ci ON users(name COLLATE "unicode_ci");

-- Language-specific collation
CREATE INDEX idx_products_name_de ON products(name COLLATE "de_DE");

-- Binary collation for exact matches
CREATE INDEX idx_codes_exact ON items(code COLLATE "C");
```

## ALTER INDEX

Modify existing indexes:

### REBUILD

Rebuild an index to reclaim space and improve performance:

```sql
-- Rebuild fragmented index
ALTER INDEX idx_orders_date REBUILD;

-- Rebuild with new tablespace
ALTER INDEX idx_large_table REBUILD TABLESPACE fast_ssd;

-- Rebuild concurrently (if supported)
ALTER INDEX idx_users_email REBUILD CONCURRENTLY;
```

### SET STATISTICS

Adjust statistics collection:

```sql
-- Increase statistics target for better planning
ALTER INDEX idx_orders_customer SET STATISTICS 1000;

-- Decrease for faster ANALYZE
ALTER INDEX idx_logs_timestamp SET STATISTICS 100;
```

### RENAME

```sql
-- Rename index
ALTER INDEX old_index_name RENAME TO new_index_name;
```

### SET TABLESPACE

```sql
-- Move index to different storage
ALTER INDEX idx_large_table SET TABLESPACE fast_storage;
```

## DROP INDEX

Remove indexes:

```sql
-- Basic drop
DROP INDEX idx_temp;

-- Drop if exists
DROP INDEX IF EXISTS idx_old_column;

-- Drop multiple indexes
DROP INDEX idx_one, idx_two, idx_three;

-- Force drop (cascade dependencies)
DROP INDEX idx_parent CASCADE;
```

## REINDEX

Rebuild indexes for maintenance:

```sql
-- Reindex specific index
REINDEX INDEX idx_users_email;

-- Reindex all indexes on table
REINDEX TABLE users;

-- Reindex entire schema
REINDEX SCHEMA public;

-- Reindex database
REINDEX DATABASE mydb;
```

## Index Selection Strategies

### When to Create Indexes

1. **Primary Keys**: Automatically indexed
2. **Foreign Keys**: Index foreign key columns for joins
3. **WHERE Clauses**: Columns in frequent filters
4. **JOIN Conditions**: Both sides of joins
5. **ORDER BY**: Sort columns
6. **GROUP BY**: Grouping columns

### Index Design Patterns

#### Composite Index Column Order

```sql
-- Order matters! Most selective first
-- Good: high cardinality -> low cardinality
CREATE INDEX idx_orders_customer_status ON orders(customer_id, status);

-- Bad: low cardinality -> high cardinality
-- CREATE INDEX idx_orders_status_customer ON orders(status, customer_id);

-- Query uses index:
SELECT * FROM orders WHERE customer_id = 123 AND status = 'pending';
SELECT * FROM orders WHERE customer_id = 123;  -- Also uses index!

-- Query doesn't use index:
-- SELECT * FROM orders WHERE status = 'pending';  -- Can't use idx_orders_customer_status
```

#### Index for Sorting

```sql
-- Match ORDER BY exactly
CREATE INDEX idx_posts_user_date ON posts(user_id, created_at DESC);

-- Efficient for:
SELECT * FROM posts 
WHERE user_id = 123 
ORDER BY created_at DESC;
```

#### Covering Index Pattern

```sql
-- Include all queried columns
CREATE INDEX idx_users_lookup ON users(username)
INCLUDE (email, first_name, last_name, avatar_url);

-- Index-only scan for:
SELECT email, first_name, last_name, avatar_url
FROM users
WHERE username = 'johndoe';
```

#### Partial Index Pattern

```sql
-- Index hot data only
CREATE INDEX idx_orders_recent_active ON orders(customer_id, order_date)
WHERE status IN ('pending', 'processing')
  AND order_date >= CURRENT_DATE - INTERVAL '30 days';

-- Smaller index, faster updates
```

## Performance Considerations

### Index Overhead

```sql
-- Monitor index size
SELECT 
    schemaname,
    tablename,
    indexname,
    pg_size_pretty(pg_relation_size(indexrelid)) AS index_size
FROM pg_stat_user_indexes
ORDER BY pg_relation_size(indexrelid) DESC;

-- Find unused indexes
SELECT 
    schemaname,
    tablename,
    indexname,
    idx_scan,
    idx_tup_read,
    idx_tup_fetch
FROM pg_stat_user_indexes
WHERE idx_scan = 0
ORDER BY schemaname, tablename;
```

### Index Maintenance

```sql
-- Regular maintenance script
DO $$
BEGIN
    -- Update statistics
    ANALYZE;
    
    -- Reindex fragmented indexes
    REINDEX TABLE users;
    REINDEX TABLE orders;
    
    -- Rebuild bloated indexes
    ALTER INDEX idx_large_table REBUILD;
END $$;
```

## Advanced Index Examples

### Multi-Column Patterns

```sql
-- E-commerce search optimization
CREATE INDEX idx_products_search ON products(
    category_id,
    is_active,
    price,
    rating DESC
) WHERE is_active = TRUE;

-- Time-series optimization
CREATE INDEX idx_metrics_device_time ON metrics(
    device_id,
    DATE_TRUNC('hour', timestamp),
    metric_type
) WHERE timestamp >= CURRENT_DATE - INTERVAL '7 days';
```

### Text Search Indexes

```sql
-- Full-text search with weights
CREATE INDEX idx_articles_fts ON articles
USING GIN (
    setweight(to_tsvector('english', title), 'A') ||
    setweight(to_tsvector('english', abstract), 'B') ||
    setweight(to_tsvector('english', content), 'C')
);

-- Trigram similarity search
CREATE EXTENSION IF NOT EXISTS pg_trgm;
CREATE INDEX idx_users_name_trgm ON users
USING GIN (name gin_trgm_ops);
```

### JSON Indexes

```sql
-- Index specific JSON fields
CREATE INDEX idx_events_type ON events((data->>'event_type'));
CREATE INDEX idx_events_user ON events((data->'user'->>'id'));

-- Index JSON paths
CREATE INDEX idx_settings_paths ON user_settings
USING GIN (settings jsonb_path_ops);
```

## Diagnostics and Validation

### VALIDATE INDEX

Verify index integrity:

```sql
-- Validate index structure
VALIDATE INDEX idx_users_email;

-- Validate with detailed output
VALIDATE INDEX idx_orders_customer VERBOSE;
```

### Index Usage Analysis

```sql
-- Check if index is used
EXPLAIN (ANALYZE, BUFFERS) 
SELECT * FROM users WHERE email = 'user@example.com';

-- Force index usage for testing
SET enable_seqscan = OFF;
EXPLAIN SELECT * FROM users WHERE email = 'user@example.com';
SET enable_seqscan = ON;
```

## Implementation Details

**Parser Structure** (`src/engine/parser_ddl.cpp::parse_ddl_index`):
- Handles CREATE, ALTER, DROP, REINDEX, VALIDATE operations
- Parses UNIQUE flag and USING method clause
- Extracts column lists with ASC/DESC and COLLATE
- Captures WHERE clause for partial indexes
- Supports COMPUTED BY for expression indexes

**AST Representation** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlIndexAst {
    std::string name;
    std::string on_table;
    bool unique{false};
    std::string action;  // CREATE|ALTER|DROP|REINDEX|VALIDATE
    std::string method;   // BTREE|HASH|BITMAP|GIN|RTREE
    std::string columns_raw;  // Column list or expression
    std::string where_clause;  // Partial index predicate
    std::string tablespace;
    // ALTER specific
    bool rebuild{false};
    int statistics{-1};
};
```

**Code Anchors**:
- Index parser: `src/engine/parser_ddl.cpp` (parse_ddl_index)
- Index types: Method parsing for GIN, bitmap, rtree
- Expression handling: COMPUTED BY clause parsing
- Partial indexes: WHERE clause extraction

## See also

- [Tables](./ddl-tables.md) - Table structure and constraints
- [SELECT Queries](./sql-select.md) - Query optimization with indexes
- [EXPLAIN/ANALYZE](./explain-analyze.md) - Analyzing index usage
- [Tablespaces](./ddl-tablespaces.md) - Index storage management
- [Performance](./missing-and-future.md) - Performance tuning