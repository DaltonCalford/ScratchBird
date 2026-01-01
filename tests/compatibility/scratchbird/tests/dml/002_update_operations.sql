-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - UPDATE Operations
-- Description: Comprehensive UPDATE statement testing
-- ============================================================================

-- UPDATE DML: Modifying existing data
-- UPDATE with WHERE, SET multiple columns
-- UPDATE with joins, subqueries, CTEs
-- UPDATE RETURNING, conditional updates

-- Create test database
CREATE DATABASE test_update_operations_db;
USE test_update_operations_db;

-- ============================================================================
-- Section 1: Basic UPDATE
-- ============================================================================

CREATE TABLE test_basic_update (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    status VARCHAR(50),
    value INT
);

INSERT INTO test_basic_update (name, status, value) VALUES
    ('Item 1', 'pending', 10),
    ('Item 2', 'active', 20),
    ('Item 3', 'pending', 30);

-- Update single column
UPDATE test_basic_update
SET status = 'completed'
WHERE id = 1;

-- Update multiple columns
UPDATE test_basic_update
SET status = 'active', value = 25
WHERE id = 3;

SELECT * FROM test_basic_update ORDER BY id;

-- ============================================================================
-- Section 2: UPDATE All Rows
-- ============================================================================

CREATE TABLE test_update_all (
    id SERIAL PRIMARY KEY,
    price NUMERIC(10,2),
    discount_applied BOOLEAN DEFAULT FALSE
);

INSERT INTO test_update_all (price) VALUES (100.00), (200.00), (150.00);

-- Update all rows (no WHERE clause)
UPDATE test_update_all
SET price = price * 0.9,
    discount_applied = TRUE;

SELECT * FROM test_update_all ORDER BY id;

-- ============================================================================
-- Section 3: UPDATE with Expressions
-- ============================================================================

CREATE TABLE test_update_expressions (
    id SERIAL PRIMARY KEY,
    quantity INT,
    unit_price NUMERIC(10,2),
    total NUMERIC(10,2)
);

INSERT INTO test_update_expressions (quantity, unit_price) VALUES
    (10, 5.50),
    (20, 3.25),
    (15, 7.00);

-- Update using calculation
UPDATE test_update_expressions
SET total = quantity * unit_price;

SELECT * FROM test_update_expressions ORDER BY id;

-- ============================================================================
-- Section 4: UPDATE with CASE Expression
-- ============================================================================

CREATE TABLE test_update_case (
    id SERIAL PRIMARY KEY,
    score INT,
    grade VARCHAR(2)
);

INSERT INTO test_update_case (score) VALUES (95), (82), (67), (54), (88);

-- Conditional update with CASE
UPDATE test_update_case
SET grade = CASE
    WHEN score >= 90 THEN 'A'
    WHEN score >= 80 THEN 'B'
    WHEN score >= 70 THEN 'C'
    WHEN score >= 60 THEN 'D'
    ELSE 'F'
END;

SELECT * FROM test_update_case ORDER BY score DESC;

-- ============================================================================
-- Section 5: UPDATE from Another Table (JOIN)
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2)
);

CREATE TABLE test_price_updates (
    product_id INT,
    new_price NUMERIC(10,2)
);

INSERT INTO test_products (product_name, price) VALUES
    ('Product A', 100.00),
    ('Product B', 200.00),
    ('Product C', 150.00);

INSERT INTO test_price_updates VALUES (1, 110.00), (3, 160.00);

-- Update from another table
UPDATE test_products p
SET price = pu.new_price
FROM test_price_updates pu
WHERE p.product_id = pu.product_id;

SELECT * FROM test_products ORDER BY product_id;

-- ============================================================================
-- Section 6: UPDATE with Subquery
-- ============================================================================

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200),
    department VARCHAR(100),
    salary NUMERIC(10,2),
    avg_dept_salary NUMERIC(10,2)
);

INSERT INTO test_employees (emp_name, department, salary) VALUES
    ('Alice', 'Engineering', 90000),
    ('Bob', 'Engineering', 95000),
    ('Charlie', 'Sales', 70000),
    ('Diana', 'Sales', 75000);

-- Update with subquery
UPDATE test_employees e
SET avg_dept_salary = (
    SELECT AVG(salary)
    FROM test_employees
    WHERE department = e.department
);

SELECT * FROM test_employees ORDER BY emp_id;

-- ============================================================================
-- Section 7: UPDATE RETURNING
-- ============================================================================

CREATE TABLE test_update_returning (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    value INT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_update_returning (name, value) VALUES
    ('Item 1', 10),
    ('Item 2', 20);

-- Return updated rows
UPDATE test_update_returning
SET value = value * 2,
    updated_at = CURRENT_TIMESTAMP
WHERE id = 1
RETURNING *;

-- Return specific columns
UPDATE test_update_returning
SET value = value + 5
WHERE id = 2
RETURNING id, name, value;

SELECT * FROM test_update_returning ORDER BY id;

-- ============================================================================
-- Section 8: UPDATE with Common Table Expression (CTE)
-- ============================================================================

CREATE TABLE test_cte_update (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT,
    category_total INT
);

INSERT INTO test_cte_update (category, value) VALUES
    ('A', 10),
    ('A', 15),
    ('B', 20),
    ('B', 25),
    ('C', 30);

-- Update using CTE
WITH category_totals AS (
    SELECT category, SUM(value) AS total
    FROM test_cte_update
    GROUP BY category
)
UPDATE test_cte_update t
SET category_total = ct.total
FROM category_totals ct
WHERE t.category = ct.category;

SELECT * FROM test_cte_update ORDER BY id;

-- ============================================================================
-- Section 9: Conditional UPDATE (Only if Changed)
-- ============================================================================

CREATE TABLE test_conditional_update (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    updated_count INT DEFAULT 0
);

INSERT INTO test_conditional_update (status) VALUES ('active'), ('inactive'), ('active');

-- Update only if status changes
UPDATE test_conditional_update
SET status = 'inactive',
    updated_count = updated_count + 1
WHERE status <> 'inactive';

SELECT * FROM test_conditional_update ORDER BY id;

-- ============================================================================
-- Section 10: UPDATE Multiple Rows with Different Values
-- ============================================================================

CREATE TABLE test_multi_value_update (
    id INT PRIMARY KEY,
    value INT
);

INSERT INTO test_multi_value_update VALUES (1, 10), (2, 20), (3, 30);

-- Update different values per row
UPDATE test_multi_value_update
SET value = v.new_value
FROM (VALUES
    (1, 100),
    (2, 200),
    (3, 300)
) AS v(id, new_value)
WHERE test_multi_value_update.id = v.id;

SELECT * FROM test_multi_value_update ORDER BY id;

-- ============================================================================
-- Section 11: UPDATE with Self-Join
-- ============================================================================

CREATE TABLE test_self_join_update (
    id SERIAL PRIMARY KEY,
    parent_id INT,
    value INT,
    parent_value INT
);

INSERT INTO test_self_join_update (id, parent_id, value) VALUES
    (1, NULL, 100),
    (2, 1, 50),
    (3, 1, 75),
    (4, 2, 25);

-- Update with self-join
UPDATE test_self_join_update t1
SET parent_value = t2.value
FROM test_self_join_update t2
WHERE t1.parent_id = t2.id;

SELECT * FROM test_self_join_update ORDER BY id;

-- ============================================================================
-- Section 12: UPDATE with NULL Handling
-- ============================================================================

CREATE TABLE test_null_update (
    id SERIAL PRIMARY KEY,
    value INT,
    previous_value INT
);

INSERT INTO test_null_update (value) VALUES (10), (NULL), (30);

-- Update NULL values
UPDATE test_null_update
SET previous_value = value,
    value = COALESCE(value, 0);

SELECT * FROM test_null_update ORDER BY id;

-- ============================================================================
-- Section 13: UPDATE with String Functions
-- ============================================================================

CREATE TABLE test_string_update (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    domain VARCHAR(100)
);

INSERT INTO test_string_update (email) VALUES
    ('user1@example.com'),
    ('user2@test.org'),
    ('user3@example.com');

-- Extract domain from email
UPDATE test_string_update
SET domain = SPLIT_PART(email, '@', 2);

-- Uppercase domain
UPDATE test_string_update
SET domain = UPPER(domain);

SELECT * FROM test_string_update ORDER BY id;

-- ============================================================================
-- Section 14: UPDATE with Date/Time Functions
-- ============================================================================

CREATE TABLE test_datetime_update (
    id SERIAL PRIMARY KEY,
    created_at TIMESTAMP,
    age_days INT,
    is_old BOOLEAN
);

INSERT INTO test_datetime_update (created_at) VALUES
    (CURRENT_TIMESTAMP - INTERVAL '5 days'),
    (CURRENT_TIMESTAMP - INTERVAL '15 days'),
    (CURRENT_TIMESTAMP - INTERVAL '3 days');

-- Calculate age
UPDATE test_datetime_update
SET age_days = EXTRACT(DAY FROM (CURRENT_TIMESTAMP - created_at)),
    is_old = (created_at < CURRENT_TIMESTAMP - INTERVAL '7 days');

SELECT id, age_days, is_old FROM test_datetime_update ORDER BY id;

-- ============================================================================
-- Section 15: UPDATE with Aggregates
-- ============================================================================

CREATE TABLE test_aggregate_update (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value INT,
    rank_in_category INT
);

INSERT INTO test_aggregate_update (category, value) VALUES
    ('A', 100),
    ('A', 150),
    ('A', 125),
    ('B', 200),
    ('B', 175);

-- Update with window function (requires subquery/CTE)
WITH ranked AS (
    SELECT id, ROW_NUMBER() OVER (PARTITION BY category ORDER BY value DESC) AS rn
    FROM test_aggregate_update
)
UPDATE test_aggregate_update t
SET rank_in_category = r.rn
FROM ranked r
WHERE t.id = r.id;

SELECT * FROM test_aggregate_update ORDER BY category, rank_in_category;

-- ============================================================================
-- Section 16: UPDATE with JSON/JSONB
-- ============================================================================

CREATE TABLE test_json_update (
    id SERIAL PRIMARY KEY,
    data JSONB
);

INSERT INTO test_json_update (data) VALUES
    ('{"name": "Alice", "age": 30}'::JSONB),
    ('{"name": "Bob", "age": 25}'::JSONB);

-- Update JSON field
UPDATE test_json_update
SET data = jsonb_set(data, '{age}', '31'::JSONB)
WHERE data->>'name' = 'Alice';

-- Add new JSON field
UPDATE test_json_update
SET data = data || '{"city": "New York"}'::JSONB
WHERE id = 1;

SELECT * FROM test_json_update;

-- ============================================================================
-- Section 17: UPDATE with Array Operations
-- ============================================================================

CREATE TABLE test_array_update (
    id SERIAL PRIMARY KEY,
    tags TEXT[]
);

INSERT INTO test_array_update (tags) VALUES
    (ARRAY['tag1', 'tag2']),
    (ARRAY['tag3']);

-- Append to array
UPDATE test_array_update
SET tags = array_append(tags, 'new_tag')
WHERE id = 1;

-- Replace array element
UPDATE test_array_update
SET tags = array_replace(tags, 'tag1', 'updated_tag')
WHERE id = 1;

SELECT * FROM test_array_update;

-- ============================================================================
-- Section 18: UPDATE in Partitioned Table
-- ============================================================================

CREATE TABLE test_partitioned_update (
    id SERIAL,
    category VARCHAR(50),
    value INT,
    PRIMARY KEY (id, category)
) PARTITION BY LIST (category);

CREATE TABLE test_part_a PARTITION OF test_partitioned_update
    FOR VALUES IN ('A');

CREATE TABLE test_part_b PARTITION OF test_partitioned_update
    FOR VALUES IN ('B');

INSERT INTO test_partitioned_update (category, value) VALUES
    ('A', 10),
    ('B', 20),
    ('A', 15);

-- Update across partitions
UPDATE test_partitioned_update
SET value = value * 2;

SELECT * FROM test_partitioned_update ORDER BY id;

-- ============================================================================
-- Section 19: UPDATE Performance Considerations
-- ============================================================================

CREATE TABLE test_update_performance (
    id SERIAL PRIMARY KEY,
    value INT,
    updated_at TIMESTAMP
);

INSERT INTO test_update_performance (value)
SELECT i FROM generate_series(1, 10000) i;

-- Efficient update (indexed WHERE clause)
CREATE INDEX idx_perf_value ON test_update_performance(value);

UPDATE test_update_performance
SET updated_at = CURRENT_TIMESTAMP
WHERE value BETWEEN 100 AND 200;

-- Check updated count
SELECT COUNT(*) AS updated_rows
FROM test_update_performance
WHERE updated_at IS NOT NULL;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_update_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_update_best_practices VALUES
    (1, 'Always use WHERE clause (avoid unintended full table updates)'),
    (2, 'Test UPDATE with SELECT first to verify affected rows'),
    (3, 'Use transactions for multi-table updates'),
    (4, 'UPDATE RETURNING for getting modified values'),
    (5, 'Index columns used in WHERE clause'),
    (6, 'Use CASE for conditional updates'),
    (7, 'Join syntax for updating from another table'),
    (8, 'CTEs for complex update logic'),
    (9, 'Monitor update performance (large updates lock rows)'),
    (10, 'Consider batch updates for large datasets'),
    (11, 'Use COALESCE for NULL handling'),
    (12, 'Validate data before update (constraints)'),
    (13, 'Backup before major updates in production'),
    (14, 'Use triggers for automatic timestamp updates'),
    (15, 'UPDATE creates new row version (MVCC bloat)'),
    (16, 'Regular VACUUM after many updates'),
    (17, 'jsonb_set for updating JSON fields'),
    (18, 'Array functions for array column updates'),
    (19, 'Avoid updating partition key (may move row)'),
    (20, 'Use prepared statements for repeated updates');

SELECT id, guideline FROM test_update_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_update_best_practices;
DROP TABLE test_update_performance;
DROP TABLE test_part_b;
DROP TABLE test_part_a;
DROP TABLE test_partitioned_update;
DROP TABLE test_array_update;
DROP TABLE test_json_update;
DROP TABLE test_aggregate_update;
DROP TABLE test_datetime_update;
DROP TABLE test_string_update;
DROP TABLE test_null_update;
DROP TABLE test_self_join_update;
DROP TABLE test_multi_value_update;
DROP TABLE test_conditional_update;
DROP TABLE test_cte_update;
DROP TABLE test_update_returning;
DROP TABLE test_employees;
DROP TABLE test_price_updates;
DROP TABLE test_products;
DROP TABLE test_update_case;
DROP TABLE test_update_expressions;
DROP TABLE test_update_all;
DROP TABLE test_basic_update;

DROP DATABASE test_update_operations_db;

-- End of UPDATE operations tests
