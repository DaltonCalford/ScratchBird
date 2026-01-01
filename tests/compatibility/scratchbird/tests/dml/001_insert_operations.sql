-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - INSERT Operations
-- Description: Comprehensive INSERT statement testing
-- ============================================================================

-- INSERT DML: Adding data to tables
-- INSERT VALUES, INSERT SELECT, INSERT RETURNING
-- Multi-row inserts, default values, conflicts
-- Bulk operations, performance patterns

-- Create test database
CREATE DATABASE test_insert_operations_db;
USE test_insert_operations_db;

-- ============================================================================
-- Section 1: Basic INSERT with VALUES
-- ============================================================================

CREATE TABLE test_basic_insert (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Single row insert
INSERT INTO test_basic_insert (name) VALUES ('Row 1');

-- Explicit column list
INSERT INTO test_basic_insert (id, name) VALUES (100, 'Row 100');

-- All columns (including defaults)
INSERT INTO test_basic_insert (id, name, created_at)
VALUES (200, 'Row 200', CURRENT_TIMESTAMP);

SELECT * FROM test_basic_insert ORDER BY id;

-- ============================================================================
-- Section 2: Multi-row INSERT
-- ============================================================================

CREATE TABLE test_multi_insert (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT
);

-- Multiple rows in single INSERT
INSERT INTO test_multi_insert (category, value) VALUES
    ('A', 10),
    ('B', 20),
    ('A', 15),
    ('C', 30),
    ('B', 25);

SELECT * FROM test_multi_insert ORDER BY id;

-- ============================================================================
-- Section 3: INSERT with DEFAULT VALUES
-- ============================================================================

CREATE TABLE test_default_insert (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50) DEFAULT 'pending',
    priority INT DEFAULT 1,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert with all defaults
INSERT INTO test_default_insert DEFAULT VALUES;

-- Insert with some defaults
INSERT INTO test_default_insert (status) VALUES ('active');

-- Explicit DEFAULT keyword
INSERT INTO test_default_insert (status, priority)
VALUES ('completed', DEFAULT);

SELECT * FROM test_default_insert ORDER BY id;

-- ============================================================================
-- Section 4: INSERT with NULL values
-- ============================================================================

CREATE TABLE test_null_insert (
    id SERIAL PRIMARY KEY,
    nullable_field VARCHAR(100),
    not_null_field VARCHAR(100) NOT NULL
);

-- Insert with NULL
INSERT INTO test_null_insert (nullable_field, not_null_field)
VALUES (NULL, 'Required value');

-- Insert with explicit NULL
INSERT INTO test_null_insert (nullable_field, not_null_field)
VALUES (NULL, 'Another required value');

SELECT * FROM test_null_insert ORDER BY id;

-- ============================================================================
-- Section 5: INSERT SELECT (from another table)
-- ============================================================================

CREATE TABLE test_source_data (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    amount NUMERIC(10,2)
);

INSERT INTO test_source_data (category, amount) VALUES
    ('Sales', 1000.00),
    ('Marketing', 500.00),
    ('Sales', 1500.00),
    ('Operations', 750.00);

CREATE TABLE test_target_data (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    total NUMERIC(10,2)
);

-- INSERT SELECT with aggregation
INSERT INTO test_target_data (category, total)
SELECT category, SUM(amount)
FROM test_source_data
GROUP BY category;

SELECT * FROM test_target_data ORDER BY category;

-- ============================================================================
-- Section 6: INSERT with Subquery
-- ============================================================================

CREATE TABLE test_subquery_insert (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    max_amount NUMERIC(10,2)
);

-- INSERT with subquery in VALUES
INSERT INTO test_subquery_insert (category, max_amount)
SELECT category,
       (SELECT MAX(amount) FROM test_source_data WHERE category = sd.category)
FROM test_source_data sd
GROUP BY category;

SELECT * FROM test_subquery_insert ORDER BY category;

-- ============================================================================
-- Section 7: INSERT RETURNING
-- ============================================================================

CREATE TABLE test_returning_insert (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Return generated ID
INSERT INTO test_returning_insert (name)
VALUES ('New record')
RETURNING id;

-- Return multiple columns
INSERT INTO test_returning_insert (name)
VALUES ('Another record')
RETURNING id, name, created_at;

-- Return all columns
INSERT INTO test_returning_insert (name)
VALUES ('Third record')
RETURNING *;

-- Return computed values
INSERT INTO test_returning_insert (name)
VALUES ('Fourth record')
RETURNING id, UPPER(name) AS uppercase_name, created_at;

-- ============================================================================
-- Section 8: INSERT ON CONFLICT DO NOTHING
-- ============================================================================

CREATE TABLE test_conflict_nothing (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    value INT
);

-- Initial insert
INSERT INTO test_conflict_nothing (code, value)
VALUES ('CODE1', 100);

-- Conflict - do nothing
INSERT INTO test_conflict_nothing (code, value)
VALUES ('CODE1', 200)
ON CONFLICT (code) DO NOTHING;

-- Verify original value unchanged
SELECT * FROM test_conflict_nothing;

-- ============================================================================
-- Section 9: INSERT ON CONFLICT DO UPDATE (UPSERT)
-- ============================================================================

CREATE TABLE test_upsert (
    id SERIAL PRIMARY KEY,
    username VARCHAR(100) UNIQUE,
    login_count INT DEFAULT 0,
    last_login TIMESTAMP
);

-- Initial insert
INSERT INTO test_upsert (username, login_count, last_login)
VALUES ('user1', 1, CURRENT_TIMESTAMP);

-- Upsert - increment counter on conflict
INSERT INTO test_upsert (username, login_count, last_login)
VALUES ('user1', 1, CURRENT_TIMESTAMP)
ON CONFLICT (username) DO UPDATE
SET login_count = test_upsert.login_count + 1,
    last_login = EXCLUDED.last_login;

-- Another upsert
INSERT INTO test_upsert (username, login_count, last_login)
VALUES ('user1', 1, CURRENT_TIMESTAMP)
ON CONFLICT (username) DO UPDATE
SET login_count = test_upsert.login_count + 1,
    last_login = EXCLUDED.last_login;

SELECT * FROM test_upsert;

-- ============================================================================
-- Section 10: INSERT ON CONFLICT with WHERE clause
-- ============================================================================

CREATE TABLE test_conflict_where (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200) UNIQUE,
    status VARCHAR(50),
    updated_at TIMESTAMP
);

INSERT INTO test_conflict_where (email, status, updated_at)
VALUES ('user@example.com', 'active', CURRENT_TIMESTAMP);

-- Update only if status is 'active'
INSERT INTO test_conflict_where (email, status, updated_at)
VALUES ('user@example.com', 'inactive', CURRENT_TIMESTAMP)
ON CONFLICT (email) DO UPDATE
SET status = EXCLUDED.status,
    updated_at = EXCLUDED.updated_at
WHERE test_conflict_where.status = 'active';

SELECT * FROM test_conflict_where;

-- ============================================================================
-- Section 11: Bulk INSERT with generate_series
-- ============================================================================

CREATE TABLE test_bulk_insert (
    id SERIAL PRIMARY KEY,
    sequence_num INT,
    data TEXT
);

-- Generate and insert 1000 rows
INSERT INTO test_bulk_insert (sequence_num, data)
SELECT i, 'Data ' || i
FROM generate_series(1, 1000) i;

SELECT COUNT(*) AS total_rows FROM test_bulk_insert;
SELECT * FROM test_bulk_insert ORDER BY id LIMIT 10;

-- ============================================================================
-- Section 12: INSERT with Common Table Expression (CTE)
-- ============================================================================

CREATE TABLE test_cte_insert (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT
);

-- INSERT with CTE
WITH category_data AS (
    SELECT 'A' AS cat, 10 AS val
    UNION ALL
    SELECT 'B', 20
    UNION ALL
    SELECT 'C', 30
)
INSERT INTO test_cte_insert (category, value)
SELECT cat, val FROM category_data;

SELECT * FROM test_cte_insert ORDER BY id;

-- ============================================================================
-- Section 13: INSERT with CASE expression
-- ============================================================================

CREATE TABLE test_case_insert (
    id SERIAL PRIMARY KEY,
    value INT,
    category VARCHAR(50)
);

-- INSERT with CASE
INSERT INTO test_case_insert (value, category)
SELECT
    i,
    CASE
        WHEN i < 10 THEN 'Low'
        WHEN i < 20 THEN 'Medium'
        ELSE 'High'
    END
FROM generate_series(1, 30) i;

SELECT category, COUNT(*) FROM test_case_insert GROUP BY category ORDER BY category;

-- ============================================================================
-- Section 14: INSERT with JSON/JSONB
-- ============================================================================

CREATE TABLE test_json_insert (
    id SERIAL PRIMARY KEY,
    data JSONB
);

-- Insert JSON data
INSERT INTO test_json_insert (data) VALUES
    ('{"name": "Alice", "age": 30, "city": "New York"}'::JSONB),
    ('{"name": "Bob", "age": 25, "city": "Los Angeles"}'::JSONB),
    ('{"name": "Charlie", "age": 35, "city": "Chicago", "tags": ["developer", "manager"]}'::JSONB);

SELECT * FROM test_json_insert;

-- Query JSON fields
SELECT id, data->>'name' AS name, data->>'city' AS city
FROM test_json_insert;

-- ============================================================================
-- Section 15: INSERT with Array Values
-- ============================================================================

CREATE TABLE test_array_insert (
    id SERIAL PRIMARY KEY,
    tags TEXT[],
    numbers INT[]
);

-- Insert array values
INSERT INTO test_array_insert (tags, numbers) VALUES
    (ARRAY['tag1', 'tag2', 'tag3'], ARRAY[1, 2, 3]),
    ('{tag4,tag5}', '{4,5,6}'),
    (ARRAY['tag6'], ARRAY[7]);

SELECT * FROM test_array_insert;

-- ============================================================================
-- Section 16: INSERT with Foreign Key Validation
-- ============================================================================

CREATE TABLE test_fk_parent (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(100)
);

CREATE TABLE test_fk_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_fk_parent(parent_id),
    child_name VARCHAR(100)
);

-- Insert parent records
INSERT INTO test_fk_parent (parent_name) VALUES ('Parent 1'), ('Parent 2');

-- Insert child records (FK validated)
INSERT INTO test_fk_child (parent_id, child_name)
VALUES (1, 'Child 1'), (2, 'Child 2');

-- Verify relationships
SELECT
    p.parent_name,
    c.child_name
FROM test_fk_parent p
JOIN test_fk_child c ON p.parent_id = c.parent_id
ORDER BY p.parent_id;

-- ============================================================================
-- Section 17: INSERT with Sequences and nextval
-- ============================================================================

CREATE SEQUENCE test_custom_sequence START WITH 1000;

CREATE TABLE test_sequence_insert (
    id INT PRIMARY KEY,
    data TEXT
);

-- Insert with explicit sequence value
INSERT INTO test_sequence_insert (id, data)
VALUES (nextval('test_custom_sequence'), 'Row 1');

INSERT INTO test_sequence_insert (id, data)
VALUES (nextval('test_custom_sequence'), 'Row 2');

SELECT * FROM test_sequence_insert ORDER BY id;

-- ============================================================================
-- Section 18: INSERT into Partitioned Table
-- ============================================================================

CREATE TABLE test_partitioned_insert (
    id SERIAL,
    category VARCHAR(50),
    value INT,
    created_date DATE DEFAULT CURRENT_DATE,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_a PARTITION OF test_partitioned_insert
    FOR VALUES IN ('A');

CREATE TABLE test_part_b PARTITION OF test_partitioned_insert
    FOR VALUES IN ('B');

CREATE TABLE test_part_c PARTITION OF test_partitioned_insert
    FOR VALUES IN ('C');

-- Insert data (automatically routed to partitions)
INSERT INTO test_partitioned_insert (category, value) VALUES
    ('A', 10),
    ('B', 20),
    ('C', 30),
    ('A', 15),
    ('B', 25);

-- Query from parent (shows all data)
SELECT * FROM test_partitioned_insert ORDER BY id;

-- Query specific partition
SELECT * FROM test_part_a;

-- ============================================================================
-- Section 19: INSERT Performance Patterns
-- ============================================================================

CREATE TABLE test_insert_performance (
    id SERIAL PRIMARY KEY,
    data TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Single transaction for bulk insert (fast)
BEGIN;
INSERT INTO test_insert_performance (data)
SELECT 'Bulk data ' || i FROM generate_series(1, 10000) i;
COMMIT;

SELECT COUNT(*) AS rows_inserted FROM test_insert_performance;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_insert_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_insert_best_practices VALUES
    (1, 'Use multi-row INSERT for multiple records (single statement)'),
    (2, 'Batch inserts in transactions for better performance'),
    (3, 'Use INSERT RETURNING to get generated IDs'),
    (4, 'INSERT ON CONFLICT for upsert operations'),
    (5, 'Use INSERT SELECT for copying/transforming data'),
    (6, 'Specify column list explicitly (maintainability)'),
    (7, 'Use DEFAULT keyword for default values'),
    (8, 'Handle NULL values appropriately'),
    (9, 'Validate foreign keys before insert (or handle errors)'),
    (10, 'Use COPY for very large bulk loads (faster than INSERT)'),
    (11, 'Disable indexes/triggers for massive bulk loads'),
    (12, 'Use CTEs for complex INSERT logic'),
    (13, 'JSONB for semi-structured data'),
    (14, 'Arrays for multi-valued attributes'),
    (15, 'Partitioned tables route inserts automatically'),
    (16, 'Monitor insert performance (pg_stat_user_tables)'),
    (17, 'Use sequences for unique ID generation'),
    (18, 'Test constraint violations and handle errors'),
    (19, 'Consider INSERT...ON CONFLICT over explicit checking'),
    (20, 'Use prepared statements for repeated inserts');

SELECT id, guideline FROM test_insert_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_insert_best_practices;
DROP TABLE test_insert_performance;
DROP TABLE test_part_c;
DROP TABLE test_part_b;
DROP TABLE test_part_a;
DROP TABLE test_partitioned_insert;
DROP SEQUENCE test_custom_sequence;
DROP TABLE test_sequence_insert;
DROP TABLE test_fk_child;
DROP TABLE test_fk_parent;
DROP TABLE test_array_insert;
DROP TABLE test_json_insert;
DROP TABLE test_case_insert;
DROP TABLE test_cte_insert;
DROP TABLE test_bulk_insert;
DROP TABLE test_conflict_where;
DROP TABLE test_upsert;
DROP TABLE test_conflict_nothing;
DROP TABLE test_returning_insert;
DROP TABLE test_subquery_insert;
DROP TABLE test_target_data;
DROP TABLE test_source_data;
DROP TABLE test_null_insert;
DROP TABLE test_default_insert;
DROP TABLE test_multi_insert;
DROP TABLE test_basic_insert;

DROP DATABASE test_insert_operations_db;

-- End of INSERT operations tests
