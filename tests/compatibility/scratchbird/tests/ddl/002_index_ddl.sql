-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DDL - Index Operations
-- Description: Comprehensive CREATE, ALTER, DROP INDEX testing
-- ============================================================================

-- Index DDL: Creating, modifying, and removing indexes
-- CREATE INDEX: Create new indexes
-- DROP INDEX: Remove indexes
-- ALTER INDEX: Modify indexes
-- REINDEX: Rebuild indexes
-- Various index types and options

-- Create test database
CREATE DATABASE test_index_ddl_db;
USE test_index_ddl_db;

-- ============================================================================
-- Section 1: Basic CREATE INDEX
-- ============================================================================

CREATE TABLE test_basic_index (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    email VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create simple B-tree index
CREATE INDEX idx_basic_name ON test_basic_index(name);

-- Create index on multiple columns
CREATE INDEX idx_basic_name_email ON test_basic_index(name, email);

-- Verify indexes created
SELECT
    indexname,
    tablename,
    indexdef
FROM pg_indexes
WHERE tablename = 'test_basic_index'
ORDER BY indexname;

-- ============================================================================
-- Section 2: CREATE UNIQUE INDEX
-- ============================================================================

CREATE TABLE test_unique_index (
    id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200)
);

-- Unique index
CREATE UNIQUE INDEX idx_unique_username ON test_unique_index(username);

-- Unique index on multiple columns
CREATE UNIQUE INDEX idx_unique_user_email ON test_unique_index(username, email);

-- Verify uniqueness enforced
INSERT INTO test_unique_index (username, email) VALUES ('user1', 'user1@example.com');

DO $$
BEGIN
    INSERT INTO test_unique_index (username, email) VALUES ('user1', 'user2@example.com');
EXCEPTION
    WHEN unique_violation THEN
        RAISE NOTICE 'Unique index violation detected';
END $$;

SELECT * FROM test_unique_index;

-- ============================================================================
-- Section 3: CREATE INDEX CONCURRENTLY
-- ============================================================================

CREATE TABLE test_concurrent_index (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_concurrent_index (value)
SELECT i FROM generate_series(1, 10000) i;

-- Create index without blocking writes
CREATE INDEX CONCURRENTLY idx_concurrent_value ON test_concurrent_index(value);

-- Verify index created
SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_concurrent_index';

-- ============================================================================
-- Section 4: Partial Index (WHERE clause)
-- ============================================================================

CREATE TABLE test_partial_index (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    processed_at TIMESTAMP
);

-- Index only active records
CREATE INDEX idx_partial_active ON test_partial_index(id)
WHERE status = 'active';

-- Index only unprocessed records
CREATE INDEX idx_partial_unprocessed ON test_partial_index(status)
WHERE processed_at IS NULL;

-- Query index definitions
SELECT
    indexname,
    indexdef
FROM pg_indexes
WHERE tablename = 'test_partial_index'
ORDER BY indexname;

-- ============================================================================
-- Section 5: Expression Index
-- ============================================================================

CREATE TABLE test_expression_index (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Index on expression (lowercase email)
CREATE INDEX idx_expr_email_lower ON test_expression_index(LOWER(email));

-- Index on date part
CREATE INDEX idx_expr_created_date ON test_expression_index((created_at::DATE));

-- Index on expression with function
CREATE INDEX idx_expr_email_domain ON test_expression_index((SPLIT_PART(email, '@', 2)));

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_expression_index'
ORDER BY indexname;

-- ============================================================================
-- Section 6: DROP INDEX
-- ============================================================================

CREATE TABLE test_drop_index (
    id SERIAL PRIMARY KEY,
    col1 INT,
    col2 TEXT
);

CREATE INDEX idx_drop_col1 ON test_drop_index(col1);
CREATE INDEX idx_drop_col2 ON test_drop_index(col2);

-- Drop single index
DROP INDEX idx_drop_col1;

-- Verify dropped
SELECT indexname
FROM pg_indexes
WHERE tablename = 'test_drop_index';

-- Drop if exists (no error if doesn't exist)
DROP INDEX IF EXISTS idx_drop_nonexistent;

-- ============================================================================
-- Section 7: DROP INDEX CONCURRENTLY
-- ============================================================================

CREATE TABLE test_drop_concurrent (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE INDEX idx_drop_concurrent_value ON test_drop_concurrent(value);

-- Drop index without blocking
DROP INDEX CONCURRENTLY idx_drop_concurrent_value;

-- Verify dropped
SELECT COUNT(*) AS index_count
FROM pg_indexes
WHERE tablename = 'test_drop_concurrent'
  AND indexname = 'idx_drop_concurrent_value';

-- ============================================================================
-- Section 8: ALTER INDEX RENAME
-- ============================================================================

CREATE TABLE test_rename_index (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE INDEX idx_old_name ON test_rename_index(data);

-- Rename index
ALTER INDEX idx_old_name RENAME TO idx_new_name;

-- Verify rename
SELECT indexname
FROM pg_indexes
WHERE tablename = 'test_rename_index';

-- ============================================================================
-- Section 9: REINDEX TABLE
-- ============================================================================

CREATE TABLE test_reindex (
    id SERIAL PRIMARY KEY,
    col1 INT,
    col2 VARCHAR(100)
);

CREATE INDEX idx_reindex_col1 ON test_reindex(col1);
CREATE INDEX idx_reindex_col2 ON test_reindex(col2);

-- Rebuild all indexes on table
REINDEX TABLE test_reindex;

-- Verify indexes still exist
SELECT indexname
FROM pg_indexes
WHERE tablename = 'test_reindex'
ORDER BY indexname;

-- ============================================================================
-- Section 10: REINDEX INDEX
-- ============================================================================

-- Rebuild specific index
REINDEX INDEX idx_reindex_col1;

-- Rebuild concurrently (PostgreSQL 12+)
-- REINDEX INDEX CONCURRENTLY idx_reindex_col2;

-- ============================================================================
-- Section 11: Hash Index
-- ============================================================================

CREATE TABLE test_hash_index (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50)
);

-- Hash index (equality comparisons only)
CREATE INDEX idx_hash_code ON test_hash_index USING hash(code);

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_hash_index';

-- ============================================================================
-- Section 12: GiST Index
-- ============================================================================

CREATE TABLE test_gist_index (
    id SERIAL PRIMARY KEY,
    location POINT,
    range INT4RANGE
);

-- GiST index for geometric types
CREATE INDEX idx_gist_location ON test_gist_index USING gist(location);

-- GiST index for range types
CREATE INDEX idx_gist_range ON test_gist_index USING gist(range);

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_gist_index'
ORDER BY indexname;

-- ============================================================================
-- Section 13: GIN Index
-- ============================================================================

CREATE TABLE test_gin_index (
    id SERIAL PRIMARY KEY,
    tags TEXT[],
    data JSONB
);

-- GIN index for arrays
CREATE INDEX idx_gin_tags ON test_gin_index USING gin(tags);

-- GIN index for JSONB
CREATE INDEX idx_gin_data ON test_gin_index USING gin(data);

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_gin_index'
ORDER BY indexname;

-- ============================================================================
-- Section 14: BRIN Index
-- ============================================================================

CREATE TABLE test_brin_index (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    value INT
);

INSERT INTO test_brin_index (value)
SELECT i FROM generate_series(1, 100000) i;

-- BRIN index (Block Range Index) for large sequential tables
CREATE INDEX idx_brin_created ON test_brin_index USING brin(created_at);
CREATE INDEX idx_brin_value ON test_brin_index USING brin(value);

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_brin_index'
ORDER BY indexname;

-- ============================================================================
-- Section 15: SP-GiST Index
-- ============================================================================

CREATE TABLE test_spgist_index (
    id SERIAL PRIMARY KEY,
    ip_address INET,
    text_value TEXT
);

-- SP-GiST index for IP addresses
CREATE INDEX idx_spgist_ip ON test_spgist_index USING spgist(ip_address);

-- SP-GiST index for text (prefix searches)
CREATE INDEX idx_spgist_text ON test_spgist_index USING spgist(text_value);

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_spgist_index'
ORDER BY indexname;

-- ============================================================================
-- Section 16: Index with INCLUDE (Covering Index)
-- ============================================================================

CREATE TABLE test_covering_index (
    id SERIAL PRIMARY KEY,
    user_id INT,
    name VARCHAR(200),
    email VARCHAR(200),
    created_at TIMESTAMP
);

-- Covering index (includes non-key columns)
CREATE INDEX idx_covering_user ON test_covering_index(user_id)
INCLUDE (name, email);

-- Index-only scan possible for this query
EXPLAIN SELECT user_id, name, email FROM test_covering_index WHERE user_id = 1;

SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'test_covering_index';

-- ============================================================================
-- Section 17: Index Storage Parameters
-- ============================================================================

CREATE TABLE test_index_storage (
    id SERIAL PRIMARY KEY,
    data TEXT
);

-- Index with storage parameters
CREATE INDEX idx_storage_data ON test_index_storage(data)
WITH (fillfactor = 70);

-- Query index options
SELECT
    indexrelid::regclass AS index_name,
    reloptions
FROM pg_class
WHERE relname = 'idx_storage_data';

-- Alter index storage parameters
ALTER INDEX idx_storage_data SET (fillfactor = 80);

-- ============================================================================
-- Section 18: Index on Partitioned Table
-- ============================================================================

CREATE TABLE test_partitioned_index (
    id SERIAL,
    category VARCHAR(50),
    value INT,
    created_date DATE,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_a PARTITION OF test_partitioned_index
    FOR VALUES IN ('A');

CREATE TABLE test_part_b PARTITION OF test_partitioned_index
    FOR VALUES IN ('B');

-- Create index on partitioned table (creates indexes on all partitions)
CREATE INDEX idx_part_value ON test_partitioned_index(value);

-- Verify indexes on partitions
SELECT
    tablename,
    indexname
FROM pg_indexes
WHERE tablename LIKE 'test_part%'
ORDER BY tablename, indexname;

-- ============================================================================
-- Section 19: Index Size and Statistics
-- ============================================================================

CREATE TABLE test_index_stats (
    id SERIAL PRIMARY KEY,
    value INT,
    data TEXT
);

INSERT INTO test_index_stats (value, data)
SELECT i, 'Data ' || i FROM generate_series(1, 10000) i;

CREATE INDEX idx_stats_value ON test_index_stats(value);

-- Analyze to update statistics
ANALYZE test_index_stats;

-- Query index size
SELECT
    indexrelname AS index_name,
    pg_size_pretty(pg_relation_size(indexrelid)) AS index_size,
    idx_scan AS index_scans,
    idx_tup_read AS tuples_read,
    idx_tup_fetch AS tuples_fetched
FROM pg_stat_user_indexes
WHERE relname = 'test_index_stats';

-- Query table and index sizes
SELECT
    pg_size_pretty(pg_total_relation_size('test_index_stats')) AS total_size,
    pg_size_pretty(pg_relation_size('test_index_stats')) AS table_size,
    pg_size_pretty(pg_indexes_size('test_index_stats')) AS indexes_size;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_index_ddl_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_index_ddl_best_practices VALUES
    (1, 'Create indexes on columns used in WHERE, JOIN, ORDER BY clauses'),
    (2, 'Use UNIQUE indexes to enforce uniqueness'),
    (3, 'Use CREATE INDEX CONCURRENTLY to avoid blocking writes'),
    (4, 'Partial indexes reduce size and improve performance for filtered queries'),
    (5, 'Expression indexes support queries on function results'),
    (6, 'B-tree is default and works for most cases'),
    (7, 'Hash indexes only for equality comparisons'),
    (8, 'GiST for geometric, full-text, and range queries'),
    (9, 'GIN for full-text search, JSONB, and arrays'),
    (10, 'BRIN for very large, naturally ordered tables'),
    (11, 'Use INCLUDE for covering indexes (index-only scans)'),
    (12, 'DROP unused indexes to reduce write overhead'),
    (13, 'REINDEX to rebuild corrupted or bloated indexes'),
    (14, 'Monitor index usage with pg_stat_user_indexes'),
    (15, 'Consider index size vs benefit trade-off'),
    (16, 'Composite indexes: order columns by selectivity'),
    (17, 'Too many indexes slow down INSERT/UPDATE/DELETE'),
    (18, 'Test query performance before and after index creation'),
    (19, 'Use EXPLAIN to verify index usage'),
    (20, 'Indexes on partitioned tables automatically create partition indexes');

SELECT id, guideline FROM test_index_ddl_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_index_ddl_best_practices;
DROP TABLE test_index_stats;
DROP TABLE test_part_b;
DROP TABLE test_part_a;
DROP TABLE test_partitioned_index;
DROP TABLE test_index_storage;
DROP TABLE test_covering_index;
DROP TABLE test_spgist_index;
DROP TABLE test_brin_index;
DROP TABLE test_gin_index;
DROP TABLE test_gist_index;
DROP TABLE test_hash_index;
DROP TABLE test_reindex;
DROP TABLE test_rename_index;
DROP TABLE test_drop_concurrent;
DROP TABLE test_drop_index;
DROP TABLE test_expression_index;
DROP TABLE test_partial_index;
DROP TABLE test_concurrent_index;
DROP TABLE test_unique_index;
DROP TABLE test_basic_index;

DROP DATABASE test_index_ddl_db;

-- End of index DDL tests
