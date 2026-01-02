-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Advanced SQL - Common Table Expressions (CTEs)
-- Description: Comprehensive CTE and recursive query testing
-- ============================================================================

-- CTEs: WITH queries and recursive CTEs
-- Simple CTEs, chained CTEs, recursive CTEs
-- Hierarchical data, graph traversal, tree structures
-- CTE optimization and materialization

-- Create test database
CREATE DATABASE test_cte_recursive_db;
USE test_cte_recursive_db;

-- ============================================================================
-- Section 1: Basic CTEs (WITH Clause)
-- ============================================================================

CREATE TABLE test_employees (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(100),
    department VARCHAR(50),
    salary NUMERIC(10,2)
);

INSERT INTO test_employees VALUES
    (1, 'Alice', 'Engineering', 90000),
    (2, 'Bob', 'Engineering', 95000),
    (3, 'Charlie', 'Sales', 70000),
    (4, 'Diana', 'Sales', 75000),
    (5, 'Eve', 'HR', 65000);

-- Simple CTE
WITH high_earners AS (
    SELECT *
    FROM test_employees
    WHERE salary > 70000
)
SELECT emp_name, department, salary
FROM high_earners
ORDER BY salary DESC;

-- CTE with aggregation
WITH dept_stats AS (
    SELECT
        department,
        COUNT(*) AS emp_count,
        AVG(salary) AS avg_salary,
        MAX(salary) AS max_salary
    FROM test_employees
    GROUP BY department
)
SELECT *
FROM dept_stats
WHERE avg_salary > 70000
ORDER BY avg_salary DESC;

-- ============================================================================
-- Section 2: Multiple CTEs
-- ============================================================================

-- Multiple independent CTEs
WITH
engineering_emps AS (
    SELECT * FROM test_employees WHERE department = 'Engineering'
),
sales_emps AS (
    SELECT * FROM test_employees WHERE department = 'Sales'
)
SELECT 'Engineering' AS dept, COUNT(*) AS count, AVG(salary) AS avg_sal
FROM engineering_emps
UNION ALL
SELECT 'Sales' AS dept, COUNT(*) AS count, AVG(salary) AS avg_sal
FROM sales_emps;

-- Chained CTEs (second CTE references first)
WITH
dept_totals AS (
    SELECT department, SUM(salary) AS total_salary
    FROM test_employees
    GROUP BY department
),
ranked_depts AS (
    SELECT
        department,
        total_salary,
        RANK() OVER (ORDER BY total_salary DESC) AS rank
    FROM dept_totals
)
SELECT *
FROM ranked_depts
ORDER BY rank;

-- ============================================================================
-- Section 3: Recursive CTE Basics
-- ============================================================================

-- Generate numbers 1-10
WITH RECURSIVE numbers AS (
    -- Base case
    SELECT 1 AS n

    UNION ALL

    -- Recursive case
    SELECT n + 1
    FROM numbers
    WHERE n < 10
)
SELECT * FROM numbers;

-- Generate date series
WITH RECURSIVE date_series AS (
    SELECT '2024-01-01'::DATE AS date

    UNION ALL

    SELECT date + INTERVAL '1 day'
    FROM date_series
    WHERE date < '2024-01-31'
)
SELECT date, TO_CHAR(date, 'Day') AS day_name
FROM date_series
ORDER BY date;

-- ============================================================================
-- Section 4: Hierarchical Data - Organization Chart
-- ============================================================================

CREATE TABLE test_org_hierarchy (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(100),
    manager_id INT,
    title VARCHAR(100)
);

INSERT INTO test_org_hierarchy VALUES
    (1, 'CEO Alice', NULL, 'Chief Executive Officer'),
    (2, 'VP Bob', 1, 'VP Engineering'),
    (3, 'VP Charlie', 1, 'VP Sales'),
    (4, 'Manager Diana', 2, 'Engineering Manager'),
    (5, 'Manager Eve', 3, 'Sales Manager'),
    (6, 'Engineer Frank', 4, 'Senior Engineer'),
    (7, 'Engineer Grace', 4, 'Engineer'),
    (8, 'Sales Rep Henry', 5, 'Sales Representative'),
    (9, 'Sales Rep Iris', 5, 'Sales Representative');

-- Recursive CTE for full hierarchy
WITH RECURSIVE org_tree AS (
    -- Base case: top-level (CEO)
    SELECT
        emp_id,
        emp_name,
        manager_id,
        title,
        0 AS level,
        emp_name AS path,
        ARRAY[emp_id] AS id_path
    FROM test_org_hierarchy
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive case: all subordinates
    SELECT
        e.emp_id,
        e.emp_name,
        e.manager_id,
        e.title,
        ot.level + 1,
        ot.path || ' > ' || e.emp_name,
        ot.id_path || e.emp_id
    FROM test_org_hierarchy e
    INNER JOIN org_tree ot ON e.manager_id = ot.emp_id
)
SELECT
    REPEAT('  ', level) || emp_name AS hierarchy,
    title,
    level,
    path
FROM org_tree
ORDER BY id_path;

-- ============================================================================
-- Section 5: Recursive CTE - Ancestors
-- ============================================================================

-- Find all ancestors of an employee
WITH RECURSIVE ancestors AS (
    -- Base case: start with specific employee
    SELECT
        emp_id,
        emp_name,
        manager_id,
        0 AS level
    FROM test_org_hierarchy
    WHERE emp_id = 6  -- Engineer Frank

    UNION ALL

    -- Recursive case: climb up the hierarchy
    SELECT
        e.emp_id,
        e.emp_name,
        e.manager_id,
        a.level + 1
    FROM test_org_hierarchy e
    INNER JOIN ancestors a ON e.emp_id = a.manager_id
)
SELECT
    emp_name,
    level,
    CASE
        WHEN level = 0 THEN 'Self'
        WHEN level = 1 THEN 'Manager'
        WHEN level = 2 THEN 'Manager''s Manager'
        ELSE 'Level ' || level || ' Up'
    END AS relationship
FROM ancestors
ORDER BY level;

-- ============================================================================
-- Section 6: Recursive CTE - Descendants
-- ============================================================================

-- Find all descendants (reports) of an employee
WITH RECURSIVE descendants AS (
    -- Base case: start with specific employee
    SELECT
        emp_id,
        emp_name,
        manager_id,
        title,
        0 AS level
    FROM test_org_hierarchy
    WHERE emp_id = 2  -- VP Bob

    UNION ALL

    -- Recursive case: all direct and indirect reports
    SELECT
        e.emp_id,
        e.emp_name,
        e.manager_id,
        e.title,
        d.level + 1
    FROM test_org_hierarchy e
    INNER JOIN descendants d ON e.manager_id = d.emp_id
)
SELECT
    REPEAT('  ', level) || emp_name AS org_chart,
    title,
    level,
    CASE
        WHEN level = 0 THEN 'Self'
        WHEN level = 1 THEN 'Direct Report'
        ELSE 'Indirect Report'
    END AS relationship
FROM descendants
ORDER BY level, emp_name;

-- ============================================================================
-- Section 7: Graph Traversal - Network Paths
-- ============================================================================

CREATE TABLE test_connections (
    from_node INT,
    to_node INT,
    distance INT
);

INSERT INTO test_connections VALUES
    (1, 2, 5),
    (1, 3, 3),
    (2, 4, 2),
    (3, 4, 6),
    (3, 5, 4),
    (4, 6, 3),
    (5, 6, 2);

-- Find all paths from node 1
WITH RECURSIVE paths AS (
    -- Base case: start at node 1
    SELECT
        from_node,
        to_node,
        distance,
        ARRAY[from_node, to_node] AS path,
        distance AS total_distance
    FROM test_connections
    WHERE from_node = 1

    UNION ALL

    -- Recursive case: extend paths
    SELECT
        c.from_node,
        c.to_node,
        c.distance,
        p.path || c.to_node,
        p.total_distance + c.distance
    FROM test_connections c
    INNER JOIN paths p ON c.from_node = p.to_node
    WHERE NOT (c.to_node = ANY(p.path))  -- Prevent cycles
)
SELECT
    path[1] AS start_node,
    path[array_length(path, 1)] AS end_node,
    array_to_string(path, ' -> ') AS full_path,
    total_distance
FROM paths
ORDER BY path[array_length(path, 1)], total_distance;

-- ============================================================================
-- Section 8: Bill of Materials (BOM) - Parts Hierarchy
-- ============================================================================

CREATE TABLE test_parts (
    part_id INT PRIMARY KEY,
    part_name VARCHAR(100),
    parent_part_id INT,
    quantity INT
);

INSERT INTO test_parts VALUES
    (1, 'Bicycle', NULL, 1),
    (2, 'Frame', 1, 1),
    (3, 'Wheel Set', 1, 1),
    (4, 'Front Wheel', 3, 1),
    (5, 'Rear Wheel', 3, 1),
    (6, 'Tire', 4, 1),
    (7, 'Tire', 5, 1),
    (8, 'Rim', 4, 1),
    (9, 'Rim', 5, 1);

-- Exploded BOM with quantities
WITH RECURSIVE bom AS (
    -- Base case: top-level product
    SELECT
        part_id,
        part_name,
        parent_part_id,
        quantity,
        0 AS level,
        quantity AS total_quantity
    FROM test_parts
    WHERE parent_part_id IS NULL

    UNION ALL

    -- Recursive case: sub-assemblies and parts
    SELECT
        p.part_id,
        p.part_name,
        p.parent_part_id,
        p.quantity,
        b.level + 1,
        b.total_quantity * p.quantity
    FROM test_parts p
    INNER JOIN bom b ON p.parent_part_id = b.part_id
)
SELECT
    REPEAT('  ', level) || part_name AS indented_part,
    level,
    quantity AS qty_per_parent,
    total_quantity
FROM bom
ORDER BY part_id;

-- ============================================================================
-- Section 9: Fibonacci Sequence
-- ============================================================================

-- Generate Fibonacci numbers
WITH RECURSIVE fibonacci AS (
    -- Base cases
    SELECT 1 AS n, 0::BIGINT AS fib_n, 1::BIGINT AS fib_n_plus_1

    UNION ALL

    -- Recursive case
    SELECT
        n + 1,
        fib_n_plus_1,
        fib_n + fib_n_plus_1
    FROM fibonacci
    WHERE n < 20
)
SELECT n, fib_n AS fibonacci_number
FROM fibonacci;

-- ============================================================================
-- Section 10: Factorial Calculation
-- ============================================================================

-- Calculate factorials
WITH RECURSIVE factorial AS (
    SELECT 0 AS n, 1::BIGINT AS factorial

    UNION ALL

    SELECT n + 1, factorial * (n + 1)
    FROM factorial
    WHERE n < 10
)
SELECT n, factorial
FROM factorial;

-- ============================================================================
-- Section 11: Sudoku Solver (Grid Filling)
-- ============================================================================

-- Simple grid filling example (concept demonstration)
WITH RECURSIVE grid AS (
    SELECT 1 AS x, 1 AS y, 1 AS value

    UNION ALL

    SELECT
        CASE WHEN x < 3 THEN x + 1 ELSE 1 END,
        CASE WHEN x < 3 THEN y ELSE y + 1 END,
        value + 1
    FROM grid
    WHERE value < 9
)
SELECT x, y, value
FROM grid
ORDER BY y, x;

-- ============================================================================
-- Section 12: Tree Depth Calculation
-- ============================================================================

-- Calculate depth of each node in tree
WITH RECURSIVE tree_depth AS (
    -- Roots (nodes with no parent)
    SELECT
        emp_id,
        emp_name,
        manager_id,
        1 AS depth,
        1 AS max_depth
    FROM test_org_hierarchy
    WHERE manager_id IS NULL

    UNION ALL

    -- Children
    SELECT
        e.emp_id,
        e.emp_name,
        e.manager_id,
        td.depth + 1,
        td.depth + 1
    FROM test_org_hierarchy e
    INNER JOIN tree_depth td ON e.manager_id = td.emp_id
)
SELECT
    emp_name,
    depth,
    (SELECT MAX(depth) FROM tree_depth) AS tree_height
FROM tree_depth
ORDER BY depth, emp_name;

-- ============================================================================
-- Section 13: Category Hierarchy (E-commerce)
-- ============================================================================

CREATE TABLE test_categories (
    category_id INT PRIMARY KEY,
    category_name VARCHAR(100),
    parent_category_id INT
);

INSERT INTO test_categories VALUES
    (1, 'Electronics', NULL),
    (2, 'Computers', 1),
    (3, 'Phones', 1),
    (4, 'Laptops', 2),
    (5, 'Desktops', 2),
    (6, 'Smartphones', 3),
    (7, 'Feature Phones', 3),
    (8, 'Gaming Laptops', 4),
    (9, 'Business Laptops', 4);

-- Full category path
WITH RECURSIVE category_path AS (
    -- Leaf categories
    SELECT
        category_id,
        category_name,
        parent_category_id,
        category_name AS full_path,
        0 AS level
    FROM test_categories
    WHERE parent_category_id IS NULL

    UNION ALL

    -- Build path from root to leaf
    SELECT
        c.category_id,
        c.category_name,
        c.parent_category_id,
        cp.full_path || ' > ' || c.category_name,
        cp.level + 1
    FROM test_categories c
    INNER JOIN category_path cp ON c.parent_category_id = cp.category_id
)
SELECT
    category_name,
    full_path,
    level
FROM category_path
ORDER BY full_path;

-- ============================================================================
-- Section 14: Transitive Closure (All Reachable Nodes)
-- ============================================================================

-- Find all nodes reachable from a starting node
WITH RECURSIVE reachable AS (
    -- Start node
    SELECT
        from_node,
        to_node,
        ARRAY[from_node, to_node] AS path
    FROM test_connections
    WHERE from_node = 1

    UNION

    -- Transitively reachable nodes
    SELECT
        r.from_node,
        c.to_node,
        r.path || c.to_node
    FROM reachable r
    INNER JOIN test_connections c ON r.to_node = c.from_node
    WHERE NOT (c.to_node = ANY(r.path))
)
SELECT DISTINCT
    from_node AS start,
    to_node AS reachable_node,
    array_to_string(path, ' -> ') AS path
FROM reachable
ORDER BY to_node, array_length(path, 1);

-- ============================================================================
-- Section 15: Cycle Detection
-- ============================================================================

CREATE TABLE test_graph_with_cycle (
    from_node INT,
    to_node INT
);

INSERT INTO test_graph_with_cycle VALUES
    (1, 2),
    (2, 3),
    (3, 4),
    (4, 2),  -- Creates cycle: 2 -> 3 -> 4 -> 2
    (3, 5);

-- Detect cycles
WITH RECURSIVE paths AS (
    SELECT
        from_node,
        to_node,
        ARRAY[from_node, to_node] AS path,
        false AS has_cycle
    FROM test_graph_with_cycle
    WHERE from_node = 1

    UNION ALL

    SELECT
        p.from_node,
        g.to_node,
        p.path || g.to_node,
        g.to_node = ANY(p.path)
    FROM paths p
    INNER JOIN test_graph_with_cycle g ON p.to_node = g.from_node
    WHERE NOT (g.to_node = ANY(p.path)) OR g.to_node = ANY(p.path)
)
SELECT
    path,
    has_cycle,
    CASE WHEN has_cycle THEN 'CYCLE DETECTED' ELSE 'OK' END AS status
FROM paths
WHERE has_cycle OR array_length(path, 1) <= 10
ORDER BY array_length(path, 1), path;

-- ============================================================================
-- Section 16: Materialized CTEs
-- ============================================================================

-- Materialized CTE (PostgreSQL 12+)
-- Forces CTE to be evaluated once and stored
EXPLAIN ANALYZE
WITH expensive_calc AS MATERIALIZED (
    SELECT
        emp_id,
        emp_name,
        salary,
        salary * 1.1 AS projected_salary
    FROM test_employees
)
SELECT e1.emp_name, e1.salary, e2.emp_name AS higher_earner
FROM expensive_calc e1
CROSS JOIN expensive_calc e2
WHERE e2.salary > e1.salary
LIMIT 10;

-- Non-materialized CTE (inline)
EXPLAIN ANALYZE
WITH expensive_calc AS NOT MATERIALIZED (
    SELECT
        emp_id,
        emp_name,
        salary,
        salary * 1.1 AS projected_salary
    FROM test_employees
)
SELECT e1.emp_name, e1.salary, e2.emp_name AS higher_earner
FROM expensive_calc e1
CROSS JOIN expensive_calc e2
WHERE e2.salary > e1.salary
LIMIT 10;

-- ============================================================================
-- Section 17: Data-Modifying CTEs
-- ============================================================================

CREATE TABLE test_audit_log (
    log_id SERIAL PRIMARY KEY,
    action VARCHAR(50),
    emp_id INT,
    old_salary NUMERIC(10,2),
    new_salary NUMERIC(10,2),
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- CTE with INSERT/UPDATE/DELETE
WITH salary_update AS (
    UPDATE test_employees
    SET salary = salary * 1.05
    WHERE department = 'Engineering'
    RETURNING emp_id, emp_name, salary
)
INSERT INTO test_audit_log (action, emp_id, new_salary)
SELECT 'SALARY_INCREASE', emp_id, salary
FROM salary_update;

SELECT * FROM test_audit_log;

-- ============================================================================
-- Section 18: Window Functions in CTEs
-- ============================================================================

WITH ranked_employees AS (
    SELECT
        emp_name,
        department,
        salary,
        RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS dept_rank,
        RANK() OVER (ORDER BY salary DESC) AS overall_rank
    FROM test_employees
)
SELECT *
FROM ranked_employees
WHERE dept_rank <= 2
ORDER BY department, dept_rank;

-- ============================================================================
-- Section 19: Recursive CTE Limits and Safeguards
-- ============================================================================

-- Infinite recursion prevention (max recursion depth)
-- SET max_recursion_depth = 100;  -- PostgreSQL doesn't have this, uses work_mem

-- Runaway query detection
WITH RECURSIVE runaway AS (
    SELECT 1 AS n
    UNION ALL
    SELECT n + 1
    FROM runaway
    WHERE n < 100  -- Safety limit
)
SELECT COUNT(*) FROM runaway;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_cte_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_cte_best_practices VALUES
    (1, 'CTEs improve query readability and maintainability'),
    (2, 'Use CTEs to break complex queries into logical steps'),
    (3, 'Recursive CTEs require base case and recursive case'),
    (4, 'Always include termination condition in recursive CTEs'),
    (5, 'UNION ALL in recursive CTEs (UNION removes duplicates)'),
    (6, 'Prevent infinite loops with WHERE conditions'),
    (7, 'Track visited nodes to prevent cycles in graphs'),
    (8, 'Materialized CTEs evaluated once (PostgreSQL 12+)'),
    (9, 'Non-materialized CTEs can be inlined by optimizer'),
    (10, 'Use MATERIALIZED for expensive CTEs referenced multiple times'),
    (11, 'Data-modifying CTEs (INSERT/UPDATE/DELETE) with RETURNING'),
    (12, 'CTEs executed once per query (unlike views)'),
    (13, 'Recursive CTEs useful for hierarchies and graphs'),
    (14, 'Common uses: org charts, BOMs, category trees'),
    (15, 'Array operations useful for path tracking'),
    (16, 'Level/depth tracking important in hierarchies'),
    (17, 'Test recursive CTEs with small datasets first'),
    (18, 'Monitor performance of complex recursive queries'),
    (19, 'Consider alternatives: window functions, self-joins'),
    (20, 'Document recursive CTE logic for maintenance');

SELECT id, guideline FROM test_cte_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_cte_best_practices;
DROP TABLE test_graph_with_cycle;
DROP TABLE test_categories;
DROP TABLE test_parts;
DROP TABLE test_connections;
DROP TABLE test_audit_log;
DROP TABLE test_org_hierarchy;
DROP TABLE test_employees;

DROP DATABASE test_cte_recursive_db;

-- End of CTE and recursive query tests
