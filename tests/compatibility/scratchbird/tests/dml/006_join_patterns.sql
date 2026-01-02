-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - Advanced JOIN Patterns
-- Description: Comprehensive JOIN techniques and optimization
-- ============================================================================

-- Advanced JOINs: Complex join patterns and use cases
-- Multiple tables, lateral joins, cross apply
-- Join optimization, anti-joins, semi-joins
-- Real-world join scenarios

-- Create test database
CREATE DATABASE test_join_patterns_db;
USE test_join_patterns_db;

-- ============================================================================
-- Section 1: Multi-Table Joins
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
    status VARCHAR(50)
);

CREATE TABLE test_order_items (
    item_id SERIAL PRIMARY KEY,
    order_id INT,
    product_name VARCHAR(200),
    quantity INT,
    price NUMERIC(10,2)
);

INSERT INTO test_customers (customer_name, city) VALUES
    ('Alice', 'New York'),
    ('Bob', 'Los Angeles'),
    ('Charlie', 'Chicago');

INSERT INTO test_orders (customer_id, order_date, status) VALUES
    (1, '2024-01-15', 'shipped'),
    (1, '2024-02-20', 'pending'),
    (2, '2024-01-10', 'delivered'),
    (3, '2024-03-01', 'shipped');

INSERT INTO test_order_items (order_id, product_name, quantity, price) VALUES
    (1, 'Widget A', 2, 10.00),
    (1, 'Widget B', 1, 15.00),
    (2, 'Gadget A', 3, 25.00),
    (3, 'Widget A', 1, 10.00),
    (4, 'Gadget B', 2, 30.00);

-- Join three tables
SELECT
    c.customer_name,
    o.order_id,
    o.order_date,
    o.status,
    i.product_name,
    i.quantity,
    i.price,
    (i.quantity * i.price) AS line_total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
INNER JOIN test_order_items i ON o.order_id = i.order_id
ORDER BY c.customer_name, o.order_date, i.item_id;

-- Aggregated multi-table join
SELECT
    c.customer_name,
    COUNT(DISTINCT o.order_id) AS order_count,
    COUNT(i.item_id) AS item_count,
    SUM(i.quantity * i.price) AS total_spent
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
INNER JOIN test_order_items i ON o.order_id = i.order_id
GROUP BY c.customer_id, c.customer_name
ORDER BY total_spent DESC;

-- ============================================================================
-- Section 2: Lateral Joins (LATERAL)
-- ============================================================================

-- LATERAL join (correlated subquery in FROM clause)
SELECT
    c.customer_name,
    recent.order_id,
    recent.order_date,
    recent.status
FROM test_customers c
LEFT JOIN LATERAL (
    SELECT order_id, order_date, status
    FROM test_orders
    WHERE customer_id = c.customer_id
    ORDER BY order_date DESC
    LIMIT 2
) recent ON true
ORDER BY c.customer_name, recent.order_date DESC;

-- LATERAL with aggregates
SELECT
    c.customer_name,
    stats.order_count,
    stats.total_spent,
    stats.first_order_date,
    stats.last_order_date
FROM test_customers c
LEFT JOIN LATERAL (
    SELECT
        COUNT(o.order_id) AS order_count,
        SUM(i.quantity * i.price) AS total_spent,
        MIN(o.order_date) AS first_order_date,
        MAX(o.order_date) AS last_order_date
    FROM test_orders o
    LEFT JOIN test_order_items i ON o.order_id = i.order_id
    WHERE o.customer_id = c.customer_id
) stats ON true
ORDER BY c.customer_name;

-- ============================================================================
-- Section 3: Anti-Join Patterns (NOT EXISTS, NOT IN, LEFT JOIN...IS NULL)
-- ============================================================================

-- Find customers with no orders (NOT EXISTS)
SELECT
    c.customer_id,
    c.customer_name,
    c.city
FROM test_customers c
WHERE NOT EXISTS (
    SELECT 1
    FROM test_orders o
    WHERE o.customer_id = c.customer_id
);

-- Find customers with no orders (LEFT JOIN...IS NULL)
SELECT
    c.customer_id,
    c.customer_name,
    c.city
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
WHERE o.order_id IS NULL;

-- Find orders with no items (anti-join)
SELECT
    o.order_id,
    o.customer_id,
    o.order_date,
    o.status
FROM test_orders o
WHERE NOT EXISTS (
    SELECT 1
    FROM test_order_items i
    WHERE i.order_id = o.order_id
)
ORDER BY o.order_id;

-- ============================================================================
-- Section 4: Semi-Join Patterns (EXISTS, IN)
-- ============================================================================

-- Customers who have placed orders (EXISTS)
SELECT
    c.customer_id,
    c.customer_name,
    c.city
FROM test_customers c
WHERE EXISTS (
    SELECT 1
    FROM test_orders o
    WHERE o.customer_id = c.customer_id
)
ORDER BY c.customer_name;

-- Customers who ordered specific product (IN with subquery)
SELECT
    c.customer_id,
    c.customer_name
FROM test_customers c
WHERE c.customer_id IN (
    SELECT DISTINCT o.customer_id
    FROM test_orders o
    INNER JOIN test_order_items i ON o.order_id = i.order_id
    WHERE i.product_name = 'Widget A'
)
ORDER BY c.customer_name;

-- ============================================================================
-- Section 5: Inequality Joins
-- ============================================================================

CREATE TABLE test_price_ranges (
    range_id INT PRIMARY KEY,
    range_name VARCHAR(50),
    min_price NUMERIC(10,2),
    max_price NUMERIC(10,2)
);

INSERT INTO test_price_ranges VALUES
    (1, 'Budget', 0.00, 20.00),
    (2, 'Standard', 20.01, 50.00),
    (3, 'Premium', 50.01, 9999.99);

-- Inequality join (classify items by price range)
SELECT
    i.product_name,
    i.price,
    pr.range_name
FROM test_order_items i
LEFT JOIN test_price_ranges pr
    ON i.price BETWEEN pr.min_price AND pr.max_price
ORDER BY i.price, pr.range_id;

-- ============================================================================
-- Section 6: Date Range Joins
-- ============================================================================

CREATE TABLE test_promotions (
    promo_id INT PRIMARY KEY,
    promo_name VARCHAR(100),
    start_date DATE,
    end_date DATE,
    discount_pct NUMERIC(5,2)
);

INSERT INTO test_promotions VALUES
    (1, 'New Year Sale', '2024-01-01', '2024-01-31', 10.00),
    (2, 'Spring Sale', '2024-03-01', '2024-03-31', 15.00),
    (3, 'Summer Sale', '2024-06-01', '2024-08-31', 20.00);

-- Join orders with active promotions
SELECT
    o.order_id,
    o.order_date,
    p.promo_name,
    p.discount_pct
FROM test_orders o
LEFT JOIN test_promotions p
    ON o.order_date BETWEEN p.start_date AND p.end_date
ORDER BY o.order_date;

-- ============================================================================
-- Section 7: Self-Join with Hierarchy
-- ============================================================================

CREATE TABLE test_org_structure (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(100),
    manager_id INT,
    department VARCHAR(50),
    salary NUMERIC(10,2)
);

INSERT INTO test_org_structure VALUES
    (1, 'Alice CEO', NULL, 'Executive', 200000),
    (2, 'Bob VP Sales', 1, 'Sales', 150000),
    (3, 'Charlie VP Eng', 1, 'Engineering', 160000),
    (4, 'Diana Sales Mgr', 2, 'Sales', 100000),
    (5, 'Eve Engineer', 3, 'Engineering', 90000),
    (6, 'Frank Engineer', 3, 'Engineering', 95000),
    (7, 'Grace Sales Rep', 4, 'Sales', 60000);

-- Employee to manager join
SELECT
    e.emp_name AS employee,
    e.department,
    e.salary,
    m.emp_name AS manager,
    m.salary AS manager_salary
FROM test_org_structure e
LEFT JOIN test_org_structure m ON e.manager_id = m.emp_id
ORDER BY e.emp_id;

-- Find employees earning more than their manager
SELECT
    e.emp_name AS employee,
    e.salary AS emp_salary,
    m.emp_name AS manager,
    m.salary AS mgr_salary,
    (e.salary - m.salary) AS salary_diff
FROM test_org_structure e
INNER JOIN test_org_structure m ON e.manager_id = m.emp_id
WHERE e.salary > m.salary
ORDER BY salary_diff DESC;

-- ============================================================================
-- Section 8: Recursive Join (Organization Hierarchy)
-- ============================================================================

-- Recursive CTE to build full hierarchy
WITH RECURSIVE org_hierarchy AS (
    -- Base case: top-level employees
    SELECT
        emp_id,
        emp_name,
        manager_id,
        department,
        salary,
        0 AS level,
        ARRAY[emp_id] AS path,
        emp_name AS hierarchy_path
    FROM test_org_structure
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive case: employees with managers
    SELECT
        e.emp_id,
        e.emp_name,
        e.manager_id,
        e.department,
        e.salary,
        oh.level + 1,
        oh.path || e.emp_id,
        oh.hierarchy_path || ' > ' || e.emp_name
    FROM test_org_structure e
    INNER JOIN org_hierarchy oh ON e.manager_id = oh.emp_id
)
SELECT
    REPEAT('  ', level) || emp_name AS org_chart,
    department,
    salary,
    level,
    hierarchy_path
FROM org_hierarchy
ORDER BY path;

-- ============================================================================
-- Section 9: Many-to-Many Joins
-- ============================================================================

CREATE TABLE test_students (
    student_id INT PRIMARY KEY,
    student_name VARCHAR(100)
);

CREATE TABLE test_courses (
    course_id INT PRIMARY KEY,
    course_name VARCHAR(100),
    credits INT
);

CREATE TABLE test_enrollments (
    enrollment_id SERIAL PRIMARY KEY,
    student_id INT,
    course_id INT,
    grade VARCHAR(2)
);

INSERT INTO test_students VALUES
    (1, 'Alice'),
    (2, 'Bob'),
    (3, 'Charlie');

INSERT INTO test_courses VALUES
    (1, 'Mathematics', 4),
    (2, 'Physics', 4),
    (3, 'Chemistry', 3),
    (4, 'English', 3);

INSERT INTO test_enrollments (student_id, course_id, grade) VALUES
    (1, 1, 'A'),
    (1, 2, 'B'),
    (1, 3, 'A'),
    (2, 1, 'B'),
    (2, 4, 'A'),
    (3, 2, 'C'),
    (3, 3, 'B'),
    (3, 4, 'A');

-- Many-to-many join through junction table
SELECT
    s.student_name,
    c.course_name,
    c.credits,
    e.grade
FROM test_students s
INNER JOIN test_enrollments e ON s.student_id = e.student_id
INNER JOIN test_courses c ON e.course_id = c.course_id
ORDER BY s.student_name, c.course_name;

-- Students with their total credits
SELECT
    s.student_name,
    COUNT(e.enrollment_id) AS course_count,
    SUM(c.credits) AS total_credits
FROM test_students s
INNER JOIN test_enrollments e ON s.student_id = e.student_id
INNER JOIN test_courses c ON e.course_id = c.course_id
GROUP BY s.student_id, s.student_name
ORDER BY total_credits DESC;

-- Courses not taken by any student
SELECT
    c.course_id,
    c.course_name,
    c.credits
FROM test_courses c
WHERE NOT EXISTS (
    SELECT 1
    FROM test_enrollments e
    WHERE e.course_id = c.course_id
)
ORDER BY c.course_name;

-- ============================================================================
-- Section 10: Join with Aggregates in Subqueries
-- ============================================================================

-- Customer with order statistics
SELECT
    c.customer_name,
    c.city,
    COALESCE(o.order_count, 0) AS order_count,
    COALESCE(o.total_spent, 0) AS total_spent,
    COALESCE(o.avg_order_value, 0) AS avg_order_value
FROM test_customers c
LEFT JOIN (
    SELECT
        customer_id,
        COUNT(*) AS order_count,
        SUM(i.quantity * i.price) AS total_spent,
        AVG(i.quantity * i.price) AS avg_order_value
    FROM test_orders o
    LEFT JOIN test_order_items i ON o.order_id = i.order_id
    GROUP BY customer_id
) o ON c.customer_id = o.customer_id
ORDER BY total_spent DESC;

-- ============================================================================
-- Section 11: Join with Window Functions
-- ============================================================================

-- Orders with running total per customer
SELECT
    c.customer_name,
    o.order_id,
    o.order_date,
    i.total_amount,
    SUM(i.total_amount) OVER (
        PARTITION BY c.customer_id
        ORDER BY o.order_date
    ) AS running_total
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
INNER JOIN (
    SELECT
        order_id,
        SUM(quantity * price) AS total_amount
    FROM test_order_items
    GROUP BY order_id
) i ON o.order_id = i.order_id
ORDER BY c.customer_name, o.order_date;

-- ============================================================================
-- Section 12: Conditional Joins (Join with CASE)
-- ============================================================================

CREATE TABLE test_products (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(100),
    category VARCHAR(50),
    price NUMERIC(10,2)
);

CREATE TABLE test_category_managers (
    manager_id INT PRIMARY KEY,
    manager_name VARCHAR(100),
    category VARCHAR(50)
);

INSERT INTO test_products VALUES
    (1, 'Widget A', 'Widgets', 10.00),
    (2, 'Widget B', 'Widgets', 15.00),
    (3, 'Gadget A', 'Gadgets', 25.00),
    (4, 'Tool A', 'Tools', 30.00);

INSERT INTO test_category_managers VALUES
    (1, 'Manager Widgets', 'Widgets'),
    (2, 'Manager Gadgets', 'Gadgets');

-- Join with conditional logic
SELECT
    p.product_name,
    p.category,
    p.price,
    COALESCE(cm.manager_name, 'No Manager') AS responsible_manager,
    CASE
        WHEN cm.manager_id IS NOT NULL THEN 'Managed'
        ELSE 'Unmanaged'
    END AS management_status
FROM test_products p
LEFT JOIN test_category_managers cm ON p.category = cm.category
ORDER BY p.category, p.product_name;

-- ============================================================================
-- Section 13: Join with DISTINCT ON
-- ============================================================================

-- Latest order per customer (PostgreSQL DISTINCT ON)
SELECT DISTINCT ON (c.customer_id)
    c.customer_name,
    o.order_id,
    o.order_date,
    o.status
FROM test_customers c
INNER JOIN test_orders o ON c.customer_id = o.customer_id
ORDER BY c.customer_id, o.order_date DESC;

-- Alternative: using window function
SELECT
    customer_name,
    order_id,
    order_date,
    status
FROM (
    SELECT
        c.customer_name,
        o.order_id,
        o.order_date,
        o.status,
        ROW_NUMBER() OVER (PARTITION BY c.customer_id ORDER BY o.order_date DESC) AS rn
    FROM test_customers c
    INNER JOIN test_orders o ON c.customer_id = o.customer_id
) ranked
WHERE rn = 1
ORDER BY customer_name;

-- ============================================================================
-- Section 14: Join Optimization with Indexes
-- ============================================================================

CREATE TABLE test_large_table_a (
    id SERIAL PRIMARY KEY,
    ref_id INT,
    data VARCHAR(100)
);

CREATE TABLE test_large_table_b (
    id SERIAL PRIMARY KEY,
    data VARCHAR(100)
);

-- Populate with test data
INSERT INTO test_large_table_a (ref_id, data)
SELECT
    (random() * 1000)::INT,
    'Data A ' || i
FROM generate_series(1, 10000) i;

INSERT INTO test_large_table_b (data)
SELECT 'Data B ' || i
FROM generate_series(1, 1000) i;

-- Create index for join optimization
CREATE INDEX idx_large_a_ref_id ON test_large_table_a(ref_id);

-- Join with index
SELECT
    a.id,
    a.data AS data_a,
    b.data AS data_b
FROM test_large_table_a a
INNER JOIN test_large_table_b b ON a.ref_id = b.id
WHERE b.id BETWEEN 100 AND 110
ORDER BY a.id
LIMIT 20;

-- ============================================================================
-- Section 15: Partitioned Table Joins
-- ============================================================================

CREATE TABLE test_part_orders (
    order_id SERIAL,
    order_date DATE,
    customer_id INT,
    total NUMERIC(10,2),
    PRIMARY KEY (order_id, order_date)
) PARTITION BY RANGE (order_date);

CREATE TABLE test_part_orders_2024_q1 PARTITION OF test_part_orders
    FOR VALUES FROM ('2024-01-01') TO ('2024-04-01');

CREATE TABLE test_part_orders_2024_q2 PARTITION OF test_part_orders
    FOR VALUES FROM ('2024-04-01') TO ('2024-07-01');

INSERT INTO test_part_orders (order_date, customer_id, total) VALUES
    ('2024-01-15', 1, 100.00),
    ('2024-02-10', 2, 200.00),
    ('2024-05-20', 1, 150.00);

-- Join partitioned table with regular table
SELECT
    c.customer_name,
    po.order_date,
    po.total
FROM test_customers c
INNER JOIN test_part_orders po ON c.customer_id = po.customer_id
WHERE po.order_date >= '2024-01-01'
  AND po.order_date < '2024-04-01'
ORDER BY po.order_date;

-- ============================================================================
-- Section 16: Cross Join with Filtering (Alternative to INNER JOIN)
-- ============================================================================

-- Cross join with WHERE (equivalent to INNER JOIN for some cases)
SELECT
    s.student_name,
    c.course_name
FROM test_students s
CROSS JOIN test_courses c
WHERE EXISTS (
    SELECT 1
    FROM test_enrollments e
    WHERE e.student_id = s.student_id
      AND e.course_id = c.course_id
)
ORDER BY s.student_name, c.course_name;

-- ============================================================================
-- Section 17: Join with USING Clause
-- ============================================================================

CREATE TABLE test_using_a (
    id INT PRIMARY KEY,
    common_id INT,
    data_a VARCHAR(100)
);

CREATE TABLE test_using_b (
    id INT PRIMARY KEY,
    common_id INT,
    data_b VARCHAR(100)
);

INSERT INTO test_using_a VALUES (1, 100, 'A1'), (2, 200, 'A2'), (3, 300, 'A3');
INSERT INTO test_using_b VALUES (1, 100, 'B1'), (2, 200, 'B2'), (4, 400, 'B4');

-- Join using USING clause (simpler syntax for same-named columns)
SELECT *
FROM test_using_a
INNER JOIN test_using_b USING (common_id)
ORDER BY common_id;

-- Equivalent with ON clause
SELECT
    a.id AS a_id,
    b.id AS b_id,
    a.common_id,
    a.data_a,
    b.data_b
FROM test_using_a a
INNER JOIN test_using_b b ON a.common_id = b.common_id
ORDER BY a.common_id;

-- ============================================================================
-- Section 18: Natural Join (Automatic Join on Common Columns)
-- ============================================================================

-- NATURAL JOIN (joins on all common column names)
SELECT *
FROM test_using_a
NATURAL JOIN test_using_b
ORDER BY id;

-- Note: NATURAL JOIN can be dangerous if schema changes

-- ============================================================================
-- Section 19: Join Performance Patterns
-- ============================================================================

-- Small table to large table join order
EXPLAIN (ANALYZE, BUFFERS)
SELECT
    c.customer_name,
    COUNT(o.order_id) AS order_count
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name;

-- Join with pre-filtered subquery
SELECT
    c.customer_name,
    filtered.order_count
FROM test_customers c
INNER JOIN (
    SELECT
        customer_id,
        COUNT(*) AS order_count
    FROM test_orders
    WHERE order_date >= '2024-01-01'
    GROUP BY customer_id
) filtered ON c.customer_id = filtered.customer_id
ORDER BY c.customer_name;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_join_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_join_best_practices VALUES
    (1, 'Index foreign key columns for join performance'),
    (2, 'Use INNER JOIN for matching records only'),
    (3, 'Use LEFT/RIGHT JOIN to preserve non-matching rows'),
    (4, 'FULL OUTER JOIN combines LEFT and RIGHT JOIN results'),
    (5, 'Avoid CROSS JOIN unless Cartesian product intended'),
    (6, 'LATERAL join for correlated subqueries in FROM'),
    (7, 'Use EXISTS for semi-joins (better than IN for large sets)'),
    (8, 'Use NOT EXISTS or LEFT JOIN...IS NULL for anti-joins'),
    (9, 'Join on indexed columns for best performance'),
    (10, 'Filter early (WHERE) before joining when possible'),
    (11, 'Use USING clause for cleaner syntax on same-named columns'),
    (12, 'Avoid NATURAL JOIN (implicit, fragile to schema changes)'),
    (13, 'Partition pruning improves partitioned table joins'),
    (14, 'Small table first helps query planner (usually automatic)'),
    (15, 'Use EXPLAIN to analyze join strategies'),
    (16, 'Window functions can replace some self-joins'),
    (17, 'CTE can improve readability of complex joins'),
    (18, 'Inequality joins (BETWEEN) can be expensive'),
    (19, 'Many-to-many requires junction table'),
    (20, 'Monitor join cardinality (row count multiplication)');

SELECT id, guideline FROM test_join_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_join_best_practices;
DROP TABLE test_using_b;
DROP TABLE test_using_a;
DROP TABLE test_part_orders_2024_q2;
DROP TABLE test_part_orders_2024_q1;
DROP TABLE test_part_orders;
DROP TABLE test_large_table_b;
DROP TABLE test_large_table_a;
DROP TABLE test_category_managers;
DROP TABLE test_products;
DROP TABLE test_enrollments;
DROP TABLE test_courses;
DROP TABLE test_students;
DROP TABLE test_org_structure;
DROP TABLE test_promotions;
DROP TABLE test_price_ranges;
DROP TABLE test_order_items;
DROP TABLE test_orders;
DROP TABLE test_customers;

DROP DATABASE test_join_patterns_db;

-- End of JOIN patterns tests
