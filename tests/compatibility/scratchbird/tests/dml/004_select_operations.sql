-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - SELECT Operations
-- Description: Comprehensive SELECT statement testing
-- ============================================================================

-- SELECT DML: Querying and retrieving data
-- SELECT with WHERE, ORDER BY, LIMIT, OFFSET
-- Joins, subqueries, CTEs, window functions
-- Aggregates, grouping, set operations

-- Create test database
CREATE DATABASE test_select_operations_db;
USE test_select_operations_db;

-- ============================================================================
-- Section 1: Basic SELECT
-- ============================================================================

CREATE TABLE test_basic_select (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    category VARCHAR(50),
    price NUMERIC(10,2),
    quantity INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_basic_select (name, category, price, quantity) VALUES
    ('Product A', 'Electronics', 299.99, 10),
    ('Product B', 'Books', 19.99, 50),
    ('Product C', 'Electronics', 499.99, 5),
    ('Product D', 'Clothing', 29.99, 100),
    ('Product E', 'Books', 24.99, 30);

-- Select all columns
SELECT * FROM test_basic_select;

-- Select specific columns
SELECT id, name, price FROM test_basic_select;

-- Select with expressions
SELECT id, name, price, quantity, (price * quantity) AS total_value
FROM test_basic_select;

-- ============================================================================
-- Section 2: SELECT with WHERE Clause
-- ============================================================================

-- Simple WHERE
SELECT * FROM test_basic_select WHERE category = 'Electronics';

-- WHERE with comparison operators
SELECT * FROM test_basic_select WHERE price > 50.00;
SELECT * FROM test_basic_select WHERE quantity >= 30;

-- WHERE with AND/OR
SELECT * FROM test_basic_select
WHERE category = 'Electronics' AND price < 400.00;

SELECT * FROM test_basic_select
WHERE category = 'Books' OR quantity > 50;

-- WHERE with BETWEEN
SELECT * FROM test_basic_select
WHERE price BETWEEN 20.00 AND 100.00;

-- WHERE with IN
SELECT * FROM test_basic_select
WHERE category IN ('Books', 'Clothing');

-- WHERE with LIKE
SELECT * FROM test_basic_select
WHERE name LIKE 'Product%';

-- ============================================================================
-- Section 3: SELECT with ORDER BY
-- ============================================================================

-- Order ascending
SELECT * FROM test_basic_select ORDER BY price ASC;

-- Order descending
SELECT * FROM test_basic_select ORDER BY quantity DESC;

-- Order by multiple columns
SELECT * FROM test_basic_select
ORDER BY category ASC, price DESC;

-- Order by expression
SELECT id, name, price, quantity, (price * quantity) AS total_value
FROM test_basic_select
ORDER BY (price * quantity) DESC;

-- Order with NULLS FIRST/LAST
CREATE TABLE test_order_nulls (
    id INT,
    value INT
);

INSERT INTO test_order_nulls VALUES (1, 10), (2, NULL), (3, 20), (4, NULL), (5, 5);

SELECT * FROM test_order_nulls ORDER BY value NULLS FIRST;
SELECT * FROM test_order_nulls ORDER BY value NULLS LAST;

-- ============================================================================
-- Section 4: SELECT with LIMIT and OFFSET
-- ============================================================================

-- Limit results
SELECT * FROM test_basic_select ORDER BY id LIMIT 3;

-- Limit with offset (pagination)
SELECT * FROM test_basic_select ORDER BY id LIMIT 3 OFFSET 2;

-- Alternative: OFFSET without LIMIT
SELECT * FROM test_basic_select ORDER BY id OFFSET 2;

-- Pagination example
SELECT id, name, price
FROM test_basic_select
ORDER BY id
LIMIT 2 OFFSET 0;  -- Page 1

SELECT id, name, price
FROM test_basic_select
ORDER BY id
LIMIT 2 OFFSET 2;  -- Page 2

-- ============================================================================
-- Section 5: SELECT DISTINCT
-- ============================================================================

-- Distinct values
SELECT DISTINCT category FROM test_basic_select ORDER BY category;

-- Distinct on multiple columns
SELECT DISTINCT category, price
FROM test_basic_select
ORDER BY category, price;

-- Distinct with aggregates
SELECT COUNT(DISTINCT category) AS unique_categories
FROM test_basic_select;

-- DISTINCT ON (PostgreSQL specific)
SELECT DISTINCT ON (category) category, name, price
FROM test_basic_select
ORDER BY category, price DESC;

-- ============================================================================
-- Section 6: SELECT with Aggregates
-- ============================================================================

-- Count
SELECT COUNT(*) AS total_products FROM test_basic_select;
SELECT COUNT(DISTINCT category) AS unique_categories FROM test_basic_select;

-- Sum
SELECT SUM(quantity) AS total_quantity FROM test_basic_select;
SELECT SUM(price * quantity) AS total_inventory_value FROM test_basic_select;

-- Average
SELECT AVG(price) AS average_price FROM test_basic_select;

-- Min and Max
SELECT MIN(price) AS min_price, MAX(price) AS max_price
FROM test_basic_select;

-- Multiple aggregates
SELECT
    COUNT(*) AS product_count,
    SUM(quantity) AS total_quantity,
    AVG(price) AS avg_price,
    MIN(price) AS min_price,
    MAX(price) AS max_price
FROM test_basic_select;

-- ============================================================================
-- Section 7: SELECT with GROUP BY
-- ============================================================================

-- Group by single column
SELECT category, COUNT(*) AS product_count
FROM test_basic_select
GROUP BY category
ORDER BY category;

-- Group by with multiple aggregates
SELECT
    category,
    COUNT(*) AS product_count,
    SUM(quantity) AS total_quantity,
    AVG(price) AS avg_price,
    MIN(price) AS min_price,
    MAX(price) AS max_price
FROM test_basic_select
GROUP BY category
ORDER BY category;

-- Group by multiple columns
CREATE TABLE test_sales (
    id SERIAL PRIMARY KEY,
    region VARCHAR(50),
    product VARCHAR(100),
    amount NUMERIC(10,2)
);

INSERT INTO test_sales (region, product, amount) VALUES
    ('North', 'Widget', 100.00),
    ('North', 'Widget', 150.00),
    ('North', 'Gadget', 200.00),
    ('South', 'Widget', 120.00),
    ('South', 'Gadget', 180.00);

SELECT region, product, SUM(amount) AS total_sales
FROM test_sales
GROUP BY region, product
ORDER BY region, product;

-- ============================================================================
-- Section 8: SELECT with HAVING
-- ============================================================================

-- HAVING filters aggregated results
SELECT category, COUNT(*) AS product_count
FROM test_basic_select
GROUP BY category
HAVING COUNT(*) >= 2
ORDER BY category;

-- HAVING with multiple conditions
SELECT category, AVG(price) AS avg_price
FROM test_basic_select
GROUP BY category
HAVING AVG(price) > 30.00
ORDER BY avg_price DESC;

-- WHERE and HAVING together
SELECT category, COUNT(*) AS product_count, AVG(price) AS avg_price
FROM test_basic_select
WHERE quantity > 10
GROUP BY category
HAVING COUNT(*) >= 1
ORDER BY category;

-- ============================================================================
-- Section 9: INNER JOIN
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    city VARCHAR(100)
);

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    total NUMERIC(10,2)
);

INSERT INTO test_customers (customer_name, city) VALUES
    ('Alice', 'New York'),
    ('Bob', 'Los Angeles'),
    ('Charlie', 'Chicago'),
    ('Diana', 'Houston');

INSERT INTO test_orders (customer_id, order_date, total) VALUES
    (1, '2024-01-15', 150.00),
    (1, '2024-02-10', 200.00),
    (2, '2024-01-20', 100.00),
    (3, '2024-03-05', 300.00);

-- Inner join
SELECT
    c.customer_name,
    c.city,
    o.order_date,
    o.total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_name, o.order_date;

-- Join with aggregates
SELECT
    c.customer_name,
    COUNT(o.order_id) AS order_count,
    SUM(o.total) AS total_spent
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name
ORDER BY total_spent DESC;

-- ============================================================================
-- Section 10: LEFT/RIGHT/FULL OUTER JOIN
-- ============================================================================

-- LEFT JOIN (includes all customers, even without orders)
SELECT
    c.customer_name,
    c.city,
    o.order_id,
    o.total
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_name;

-- Find customers with no orders
SELECT
    c.customer_name,
    c.city
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
WHERE o.order_id IS NULL
ORDER BY c.customer_name;

-- RIGHT JOIN
SELECT
    c.customer_name,
    o.order_id,
    o.total
FROM test_customers c
RIGHT JOIN test_orders o ON c.customer_id = o.customer_id
ORDER BY o.order_id;

-- FULL OUTER JOIN
SELECT
    c.customer_name,
    o.order_id,
    o.total
FROM test_customers c
FULL OUTER JOIN test_orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_name NULLS LAST, o.order_id;

-- ============================================================================
-- Section 11: CROSS JOIN
-- ============================================================================

CREATE TABLE test_colors (
    color_id INT PRIMARY KEY,
    color_name VARCHAR(50)
);

CREATE TABLE test_sizes (
    size_id INT PRIMARY KEY,
    size_name VARCHAR(50)
);

INSERT INTO test_colors VALUES (1, 'Red'), (2, 'Blue'), (3, 'Green');
INSERT INTO test_sizes VALUES (1, 'Small'), (2, 'Medium'), (3, 'Large');

-- Cross join (Cartesian product)
SELECT
    c.color_name,
    s.size_name
FROM test_colors c
CROSS JOIN test_sizes s
ORDER BY c.color_name, s.size_name;

-- Cross join with WHERE (equivalent to inner join)
SELECT
    c.color_name,
    s.size_name
FROM test_colors c
CROSS JOIN test_sizes s
WHERE c.color_id = s.size_id;

-- ============================================================================
-- Section 12: Self-Join
-- ============================================================================

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200),
    manager_id INT
);

INSERT INTO test_employees (emp_name, manager_id) VALUES
    ('Alice', NULL),
    ('Bob', 1),
    ('Charlie', 1),
    ('Diana', 2),
    ('Eve', 2);

-- Self-join to find employee and their manager
SELECT
    e.emp_name AS employee,
    m.emp_name AS manager
FROM test_employees e
LEFT JOIN test_employees m ON e.manager_id = m.emp_id
ORDER BY e.emp_name;

-- Find employees with same manager
SELECT
    e1.emp_name AS employee1,
    e2.emp_name AS employee2,
    m.emp_name AS common_manager
FROM test_employees e1
INNER JOIN test_employees e2 ON e1.manager_id = e2.manager_id AND e1.emp_id < e2.emp_id
INNER JOIN test_employees m ON e1.manager_id = m.emp_id
ORDER BY m.emp_name, e1.emp_name;

-- ============================================================================
-- Section 13: Subqueries (Scalar, Row, Table)
-- ============================================================================

-- Scalar subquery (returns single value)
SELECT name, price,
    (SELECT AVG(price) FROM test_basic_select) AS avg_price
FROM test_basic_select
ORDER BY price DESC;

-- WHERE with subquery
SELECT name, price
FROM test_basic_select
WHERE price > (SELECT AVG(price) FROM test_basic_select)
ORDER BY price;

-- IN subquery
SELECT customer_name
FROM test_customers
WHERE customer_id IN (SELECT DISTINCT customer_id FROM test_orders)
ORDER BY customer_name;

-- Table subquery (FROM clause)
SELECT category, avg_price
FROM (
    SELECT category, AVG(price) AS avg_price
    FROM test_basic_select
    GROUP BY category
) AS category_averages
WHERE avg_price > 100.00
ORDER BY avg_price DESC;

-- ============================================================================
-- Section 14: Correlated Subqueries
-- ============================================================================

-- Correlated subquery (references outer query)
SELECT
    c.customer_name,
    (
        SELECT COUNT(*)
        FROM test_orders o
        WHERE o.customer_id = c.customer_id
    ) AS order_count
FROM test_customers c
ORDER BY c.customer_name;

-- Correlated subquery in WHERE
SELECT name, price, category
FROM test_basic_select bs1
WHERE price = (
    SELECT MAX(price)
    FROM test_basic_select bs2
    WHERE bs2.category = bs1.category
)
ORDER BY category;

-- ============================================================================
-- Section 15: EXISTS and NOT EXISTS
-- ============================================================================

-- EXISTS (check if subquery returns any rows)
SELECT customer_name
FROM test_customers c
WHERE EXISTS (
    SELECT 1
    FROM test_orders o
    WHERE o.customer_id = c.customer_id
)
ORDER BY customer_name;

-- NOT EXISTS (customers without orders)
SELECT customer_name
FROM test_customers c
WHERE NOT EXISTS (
    SELECT 1
    FROM test_orders o
    WHERE o.customer_id = c.customer_id
)
ORDER BY customer_name;

-- ============================================================================
-- Section 16: Common Table Expressions (CTEs)
-- ============================================================================

-- Simple CTE
WITH high_price_products AS (
    SELECT name, category, price
    FROM test_basic_select
    WHERE price > 100.00
)
SELECT * FROM high_price_products
ORDER BY price DESC;

-- Multiple CTEs
WITH
category_stats AS (
    SELECT
        category,
        COUNT(*) AS product_count,
        AVG(price) AS avg_price
    FROM test_basic_select
    GROUP BY category
),
overall_stats AS (
    SELECT
        AVG(price) AS overall_avg_price
    FROM test_basic_select
)
SELECT
    cs.category,
    cs.product_count,
    cs.avg_price,
    os.overall_avg_price,
    (cs.avg_price - os.overall_avg_price) AS price_diff
FROM category_stats cs
CROSS JOIN overall_stats os
ORDER BY cs.category;

-- CTE with INSERT/UPDATE/DELETE (data-modifying CTE)
WITH deleted_rows AS (
    DELETE FROM test_order_nulls
    WHERE value IS NULL
    RETURNING *
)
SELECT COUNT(*) AS deleted_count FROM deleted_rows;

-- ============================================================================
-- Section 17: Recursive CTEs
-- ============================================================================

-- Recursive CTE (generate series)
WITH RECURSIVE number_series AS (
    SELECT 1 AS n
    UNION ALL
    SELECT n + 1
    FROM number_series
    WHERE n < 10
)
SELECT * FROM number_series;

-- Recursive CTE (employee hierarchy)
WITH RECURSIVE emp_hierarchy AS (
    -- Base case: top-level employees (no manager)
    SELECT emp_id, emp_name, manager_id, 0 AS level
    FROM test_employees
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive case: employees with managers
    SELECT e.emp_id, e.emp_name, e.manager_id, eh.level + 1
    FROM test_employees e
    INNER JOIN emp_hierarchy eh ON e.manager_id = eh.emp_id
)
SELECT
    REPEAT('  ', level) || emp_name AS employee_hierarchy,
    level
FROM emp_hierarchy
ORDER BY level, emp_name;

-- ============================================================================
-- Section 18: Window Functions
-- ============================================================================

CREATE TABLE test_window_functions (
    id SERIAL PRIMARY KEY,
    department VARCHAR(50),
    employee VARCHAR(100),
    salary NUMERIC(10,2)
);

INSERT INTO test_window_functions (department, employee, salary) VALUES
    ('Sales', 'Alice', 60000),
    ('Sales', 'Bob', 65000),
    ('Sales', 'Charlie', 70000),
    ('Engineering', 'Diana', 80000),
    ('Engineering', 'Eve', 85000),
    ('Engineering', 'Frank', 90000),
    ('HR', 'Grace', 55000),
    ('HR', 'Henry', 58000);

-- ROW_NUMBER
SELECT
    department,
    employee,
    salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) AS row_num
FROM test_window_functions
ORDER BY department, row_num;

-- RANK and DENSE_RANK
SELECT
    department,
    employee,
    salary,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS rank,
    DENSE_RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dense_rank
FROM test_window_functions
ORDER BY department, salary DESC;

-- NTILE (divide into quartiles)
SELECT
    employee,
    salary,
    NTILE(4) OVER (ORDER BY salary) AS quartile
FROM test_window_functions
ORDER BY salary;

-- LAG and LEAD
SELECT
    employee,
    salary,
    LAG(salary) OVER (ORDER BY salary) AS prev_salary,
    LEAD(salary) OVER (ORDER BY salary) AS next_salary,
    salary - LAG(salary) OVER (ORDER BY salary) AS diff_from_prev
FROM test_window_functions
ORDER BY salary;

-- FIRST_VALUE and LAST_VALUE
SELECT
    department,
    employee,
    salary,
    FIRST_VALUE(employee) OVER (PARTITION BY department ORDER BY salary DESC) AS highest_paid,
    LAST_VALUE(employee) OVER (
        PARTITION BY department
        ORDER BY salary DESC
        RANGE BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING
    ) AS lowest_paid
FROM test_window_functions
ORDER BY department, salary DESC;

-- Running totals with SUM
SELECT
    employee,
    salary,
    SUM(salary) OVER (ORDER BY id) AS running_total
FROM test_window_functions
ORDER BY id;

-- ============================================================================
-- Section 19: Set Operations (UNION, INTERSECT, EXCEPT)
-- ============================================================================

CREATE TABLE test_set_a (
    id INT,
    value VARCHAR(50)
);

CREATE TABLE test_set_b (
    id INT,
    value VARCHAR(50)
);

INSERT INTO test_set_a VALUES (1, 'A'), (2, 'B'), (3, 'C'), (4, 'D');
INSERT INTO test_set_b VALUES (3, 'C'), (4, 'D'), (5, 'E'), (6, 'F');

-- UNION (removes duplicates)
SELECT value FROM test_set_a
UNION
SELECT value FROM test_set_b
ORDER BY value;

-- UNION ALL (keeps duplicates)
SELECT value FROM test_set_a
UNION ALL
SELECT value FROM test_set_b
ORDER BY value;

-- INTERSECT (common values)
SELECT value FROM test_set_a
INTERSECT
SELECT value FROM test_set_b
ORDER BY value;

-- EXCEPT (values in A but not in B)
SELECT value FROM test_set_a
EXCEPT
SELECT value FROM test_set_b
ORDER BY value;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_select_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_select_best_practices VALUES
    (1, 'Select only needed columns (avoid SELECT *)'),
    (2, 'Use WHERE to filter rows early (reduce data processed)'),
    (3, 'Index columns used in WHERE, JOIN, ORDER BY'),
    (4, 'Use LIMIT for pagination and large result sets'),
    (5, 'DISTINCT can be expensive (ensure necessary)'),
    (6, 'Use appropriate join type (INNER vs OUTER)'),
    (7, 'Avoid Cartesian products (always use join conditions)'),
    (8, 'Use CTEs for complex queries (improves readability)'),
    (9, 'Window functions avoid self-joins'),
    (10, 'Use EXISTS instead of IN for large subqueries'),
    (11, 'Correlated subqueries can be slow (consider joins)'),
    (12, 'UNION ALL faster than UNION (no deduplication)'),
    (13, 'Use EXPLAIN to analyze query performance'),
    (14, 'Avoid functions on indexed columns in WHERE'),
    (15, 'Use covering indexes for frequently queried columns'),
    (16, 'GROUP BY requires aggregates or grouped columns only'),
    (17, 'HAVING filters aggregated results (after GROUP BY)'),
    (18, 'ORDER BY can be expensive on large datasets'),
    (19, 'Use prepared statements for repeated queries'),
    (20, 'Monitor slow queries (pg_stat_statements extension)');

SELECT id, guideline FROM test_select_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_select_best_practices;
DROP TABLE test_set_b;
DROP TABLE test_set_a;
DROP TABLE test_window_functions;
DROP TABLE test_employees;
DROP TABLE test_sizes;
DROP TABLE test_colors;
DROP TABLE test_orders;
DROP TABLE test_customers;
DROP TABLE test_sales;
DROP TABLE test_order_nulls;
DROP TABLE test_basic_select;

DROP DATABASE test_select_operations_db;

-- End of SELECT operations tests
