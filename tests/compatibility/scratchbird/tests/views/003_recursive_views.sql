-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Views - Recursive Views and CTEs
-- Description: Comprehensive recursive view testing
-- ============================================================================

-- Recursive views use WITH RECURSIVE CTEs
-- Useful for hierarchical and graph data
-- Tree traversal, organizational charts, bill of materials
-- Path finding, network analysis

-- Create test database
CREATE DATABASE test_recursive_views_db;
USE test_recursive_views_db;

-- ============================================================================
-- Section 1: Simple Number Sequence
-- ============================================================================

CREATE VIEW test_number_sequence AS
WITH RECURSIVE numbers AS (
    SELECT 1 AS n
    UNION ALL
    SELECT n + 1 FROM numbers WHERE n < 10
)
SELECT n FROM numbers;

SELECT n FROM test_number_sequence ORDER BY n;

-- ============================================================================
-- Section 2: Employee Hierarchy
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    manager_id INT REFERENCES test_employees(employee_id),
    title VARCHAR(100),
    salary NUMERIC(12,2)
);

INSERT INTO test_employees (employee_id, name, manager_id, title, salary) VALUES
    (1, 'CEO Alice', NULL, 'Chief Executive Officer', 200000),
    (2, 'VP Bob', 1, 'VP Engineering', 150000),
    (3, 'VP Charlie', 1, 'VP Sales', 145000),
    (4, 'Manager Diana', 2, 'Engineering Manager', 120000),
    (5, 'Manager Eve', 2, 'Engineering Manager', 115000),
    (6, 'Manager Frank', 3, 'Sales Manager', 110000),
    (7, 'Engineer Grace', 4, 'Senior Engineer', 95000),
    (8, 'Engineer Henry', 4, 'Engineer', 85000),
    (9, 'Engineer Ivy', 5, 'Engineer', 80000),
    (10, 'Sales Rep Jack', 6, 'Sales Representative', 70000);

CREATE VIEW test_org_chart AS
WITH RECURSIVE org_hierarchy AS (
    -- Base case: CEO (no manager)
    SELECT
        employee_id,
        name,
        manager_id,
        title,
        salary,
        0 AS level,
        ARRAY[employee_id] AS path,
        name AS path_string
    FROM test_employees
    WHERE manager_id IS NULL

    UNION ALL

    -- Recursive case: employees with managers
    SELECT
        e.employee_id,
        e.name,
        e.manager_id,
        e.title,
        e.salary,
        oh.level + 1,
        oh.path || e.employee_id,
        oh.path_string || ' > ' || e.name
    FROM test_employees e
    JOIN org_hierarchy oh ON e.manager_id = oh.employee_id
)
SELECT
    employee_id,
    name,
    manager_id,
    title,
    level,
    path_string
FROM org_hierarchy;

SELECT employee_id, name, title, level, path_string FROM test_org_chart ORDER BY path;

-- ============================================================================
-- Section 3: Category Tree
-- ============================================================================

CREATE TABLE test_categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(200),
    parent_id INT REFERENCES test_categories(category_id)
);

INSERT INTO test_categories (category_id, category_name, parent_id) VALUES
    (1, 'Electronics', NULL),
    (2, 'Computers', 1),
    (3, 'Phones', 1),
    (4, 'Laptops', 2),
    (5, 'Desktops', 2),
    (6, 'Smartphones', 3),
    (7, 'Feature Phones', 3),
    (8, 'Gaming Laptops', 4),
    (9, 'Business Laptops', 4);

CREATE VIEW test_category_tree AS
WITH RECURSIVE category_hierarchy AS (
    -- Root categories
    SELECT
        category_id,
        category_name,
        parent_id,
        0 AS depth,
        category_name AS full_path,
        ARRAY[category_id] AS id_path
    FROM test_categories
    WHERE parent_id IS NULL

    UNION ALL

    -- Child categories
    SELECT
        c.category_id,
        c.category_name,
        c.parent_id,
        ch.depth + 1,
        ch.full_path || ' > ' || c.category_name,
        ch.id_path || c.category_id
    FROM test_categories c
    JOIN category_hierarchy ch ON c.parent_id = ch.category_id
)
SELECT
    category_id,
    category_name,
    parent_id,
    depth,
    full_path,
    id_path
FROM category_hierarchy;

SELECT category_id, category_name, depth, full_path FROM test_category_tree ORDER BY id_path;

-- ============================================================================
-- Section 4: Bill of Materials (BOM)
-- ============================================================================

CREATE TABLE test_parts (
    part_id SERIAL PRIMARY KEY,
    part_name VARCHAR(200),
    unit_cost NUMERIC(10,2)
);

CREATE TABLE test_bom (
    parent_part_id INT REFERENCES test_parts(part_id),
    child_part_id INT REFERENCES test_parts(part_id),
    quantity INT,
    PRIMARY KEY (parent_part_id, child_part_id)
);

INSERT INTO test_parts (part_id, part_name, unit_cost) VALUES
    (1, 'Bicycle', 0),  -- Assembled product
    (2, 'Frame', 100.00),
    (3, 'Wheel', 30.00),
    (4, 'Handlebar', 20.00),
    (5, 'Seat', 15.00),
    (6, 'Tire', 10.00),
    (7, 'Rim', 15.00),
    (8, 'Spoke', 0.50);

INSERT INTO test_bom (parent_part_id, child_part_id, quantity) VALUES
    (1, 2, 1),  -- Bicycle needs 1 frame
    (1, 3, 2),  -- Bicycle needs 2 wheels
    (1, 4, 1),  -- Bicycle needs 1 handlebar
    (1, 5, 1),  -- Bicycle needs 1 seat
    (3, 6, 1),  -- Wheel needs 1 tire
    (3, 7, 1),  -- Wheel needs 1 rim
    (7, 8, 36); -- Rim needs 36 spokes

CREATE VIEW test_bom_explosion AS
WITH RECURSIVE bom_explosion AS (
    -- Top level: the product itself
    SELECT
        part_id,
        part_name,
        unit_cost,
        part_id AS top_level_part,
        1 AS quantity,
        0 AS level,
        ARRAY[part_id] AS path
    FROM test_parts
    WHERE part_id = 1  -- Bicycle

    UNION ALL

    -- Recursive: components
    SELECT
        p.part_id,
        p.part_name,
        p.unit_cost,
        be.top_level_part,
        be.quantity * bom.quantity,
        be.level + 1,
        be.path || p.part_id
    FROM bom_explosion be
    JOIN test_bom bom ON be.part_id = bom.parent_part_id
    JOIN test_parts p ON bom.child_part_id = p.part_id
)
SELECT
    part_id,
    part_name,
    unit_cost,
    quantity,
    level,
    unit_cost * quantity AS total_cost
FROM bom_explosion;

SELECT part_name, quantity, level, unit_cost, total_cost FROM test_bom_explosion ORDER BY level, part_name;

-- Total cost calculation
SELECT SUM(total_cost) AS bicycle_total_cost
FROM test_bom_explosion
WHERE level > 0;  -- Exclude the bicycle itself

-- ============================================================================
-- Section 5: Graph Traversal (Network/Social)
-- ============================================================================

CREATE TABLE test_users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100)
);

CREATE TABLE test_friendships (
    user_id INT REFERENCES test_users(user_id),
    friend_id INT REFERENCES test_users(user_id),
    PRIMARY KEY (user_id, friend_id)
);

INSERT INTO test_users (user_id, username) VALUES
    (1, 'Alice'),
    (2, 'Bob'),
    (3, 'Charlie'),
    (4, 'Diana'),
    (5, 'Eve'),
    (6, 'Frank');

INSERT INTO test_friendships (user_id, friend_id) VALUES
    (1, 2), (2, 1),  -- Alice <-> Bob
    (2, 3), (3, 2),  -- Bob <-> Charlie
    (3, 4), (4, 3),  -- Charlie <-> Diana
    (1, 5), (5, 1),  -- Alice <-> Eve
    (4, 6), (6, 4);  -- Diana <-> Frank

-- Find all connections from Alice (user_id = 1)
CREATE VIEW test_friend_network AS
WITH RECURSIVE friend_connections AS (
    -- Base: Direct friends of Alice
    SELECT
        1 AS source_user,
        user_id,
        friend_id,
        1 AS degree,
        ARRAY[1, friend_id] AS path
    FROM test_friendships
    WHERE user_id = 1

    UNION

    -- Recursive: Friends of friends (up to 3 degrees)
    SELECT
        fc.source_user,
        f.user_id,
        f.friend_id,
        fc.degree + 1,
        fc.path || f.friend_id
    FROM friend_connections fc
    JOIN test_friendships f ON fc.friend_id = f.user_id
    WHERE fc.degree < 3
      AND NOT (f.friend_id = ANY(fc.path))  -- Avoid cycles
)
SELECT DISTINCT
    fc.friend_id AS user_id,
    u.username,
    MIN(fc.degree) AS degrees_of_separation
FROM friend_connections fc
JOIN test_users u ON fc.friend_id = u.user_id
WHERE fc.friend_id != 1  -- Exclude Alice herself
GROUP BY fc.friend_id, u.username;

SELECT user_id, username, degrees_of_separation FROM test_friend_network ORDER BY degrees_of_separation, username;

-- ============================================================================
-- Section 6: Date/Time Series Generation
-- ============================================================================

CREATE VIEW test_date_series AS
WITH RECURSIVE dates AS (
    SELECT DATE '2024-01-01' AS date
    UNION ALL
    SELECT date + INTERVAL '1 day'
    FROM dates
    WHERE date < DATE '2024-01-31'
)
SELECT
    date,
    EXTRACT(DOW FROM date) AS day_of_week,
    TO_CHAR(date, 'Day') AS day_name
FROM dates;

SELECT date, day_name FROM test_date_series ORDER BY date LIMIT 10;

-- ============================================================================
-- Section 7: Factorial Calculation
-- ============================================================================

CREATE VIEW test_factorials AS
WITH RECURSIVE factorial AS (
    SELECT 1 AS n, 1::BIGINT AS factorial
    UNION ALL
    SELECT n + 1, factorial * (n + 1)
    FROM factorial
    WHERE n < 20
)
SELECT n, factorial FROM factorial;

SELECT n, factorial FROM test_factorials ORDER BY n;

-- ============================================================================
-- Section 8: Fibonacci Sequence
-- ============================================================================

CREATE VIEW test_fibonacci AS
WITH RECURSIVE fib AS (
    SELECT 1 AS n, 0::BIGINT AS fib_n, 1::BIGINT AS fib_n_plus_1
    UNION ALL
    SELECT n + 1, fib_n_plus_1, fib_n + fib_n_plus_1
    FROM fib
    WHERE n < 20
)
SELECT n, fib_n AS fibonacci FROM fib;

SELECT n, fibonacci FROM test_fibonacci ORDER BY n;

-- ============================================================================
-- Section 9: Path Finding
-- ============================================================================

CREATE TABLE test_locations (
    location_id SERIAL PRIMARY KEY,
    location_name VARCHAR(200)
);

CREATE TABLE test_routes (
    from_location INT REFERENCES test_locations(location_id),
    to_location INT REFERENCES test_locations(location_id),
    distance INT,
    PRIMARY KEY (from_location, to_location)
);

INSERT INTO test_locations (location_id, location_name) VALUES
    (1, 'City A'),
    (2, 'City B'),
    (3, 'City C'),
    (4, 'City D'),
    (5, 'City E');

INSERT INTO test_routes (from_location, to_location, distance) VALUES
    (1, 2, 10),
    (2, 3, 15),
    (1, 3, 30),
    (2, 4, 20),
    (3, 4, 10),
    (3, 5, 25),
    (4, 5, 5);

-- Find all paths from City A to City E
CREATE VIEW test_all_paths_a_to_e AS
WITH RECURSIVE paths AS (
    -- Base: Start at City A
    SELECT
        from_location,
        to_location,
        distance,
        distance AS total_distance,
        ARRAY[from_location, to_location] AS path,
        from_location::TEXT || ' -> ' || to_location::TEXT AS path_string
    FROM test_routes
    WHERE from_location = 1

    UNION ALL

    -- Recursive: Extend paths
    SELECT
        r.from_location,
        r.to_location,
        r.distance,
        p.total_distance + r.distance,
        p.path || r.to_location,
        p.path_string || ' -> ' || r.to_location::TEXT
    FROM paths p
    JOIN test_routes r ON p.to_location = r.from_location
    WHERE NOT (r.to_location = ANY(p.path))  -- Avoid cycles
      AND p.path_string NOT LIKE '%5%'  -- Stop when reaching destination (City E = 5)
)
SELECT
    path_string AS route,
    total_distance
FROM paths
WHERE to_location = 5  -- Destination is City E
ORDER BY total_distance;

SELECT route, total_distance FROM test_all_paths_a_to_e;

-- ============================================================================
-- Section 10: Ancestry/Genealogy
-- ============================================================================

CREATE TABLE test_persons (
    person_id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    father_id INT REFERENCES test_persons(person_id),
    mother_id INT REFERENCES test_persons(person_id),
    birth_year INT
);

INSERT INTO test_persons (person_id, name, father_id, mother_id, birth_year) VALUES
    (1, 'Grandpa John', NULL, NULL, 1940),
    (2, 'Grandma Mary', NULL, NULL, 1942),
    (3, 'Grandpa Bob', NULL, NULL, 1938),
    (4, 'Grandma Sue', NULL, NULL, 1941),
    (5, 'Father Tom', 1, 2, 1965),
    (6, 'Mother Jane', 3, 4, 1967),
    (7, 'Child Alice', 5, 6, 1990),
    (8, 'Child Bob', 5, 6, 1992);

CREATE VIEW test_ancestors AS
WITH RECURSIVE ancestors AS (
    -- Base: Start with Alice
    SELECT
        person_id,
        name,
        father_id,
        mother_id,
        0 AS generation,
        'self' AS relation
    FROM test_persons
    WHERE person_id = 7  -- Alice

    UNION ALL

    -- Recursive: Parents
    SELECT
        p.person_id,
        p.name,
        p.father_id,
        p.mother_id,
        a.generation + 1,
        CASE
            WHEN a.generation + 1 = 1 THEN 'parent'
            WHEN a.generation + 1 = 2 THEN 'grandparent'
            WHEN a.generation + 1 = 3 THEN 'great-grandparent'
            ELSE 'ancestor'
        END
    FROM ancestors a
    JOIN test_persons p ON p.person_id IN (a.father_id, a.mother_id)
)
SELECT
    person_id,
    name,
    generation,
    relation
FROM ancestors;

SELECT name, generation, relation FROM test_ancestors ORDER BY generation, name;

-- ============================================================================
-- Section 11: Dependency Resolution
-- ============================================================================

CREATE TABLE test_tasks (
    task_id SERIAL PRIMARY KEY,
    task_name VARCHAR(200),
    estimated_hours INT
);

CREATE TABLE test_task_dependencies (
    task_id INT REFERENCES test_tasks(task_id),
    depends_on_task_id INT REFERENCES test_tasks(task_id),
    PRIMARY KEY (task_id, depends_on_task_id)
);

INSERT INTO test_tasks (task_id, task_name, estimated_hours) VALUES
    (1, 'Design Database', 8),
    (2, 'Implement Backend API', 40),
    (3, 'Create Frontend UI', 30),
    (4, 'Write Unit Tests', 20),
    (5, 'Integration Testing', 16),
    (6, 'Deploy to Production', 8);

INSERT INTO test_task_dependencies (task_id, depends_on_task_id) VALUES
    (2, 1),  -- Backend depends on Database
    (3, 2),  -- Frontend depends on Backend
    (4, 2),  -- Unit tests depend on Backend
    (5, 3),  -- Integration tests depend on Frontend
    (5, 4),  -- Integration tests depend on Unit tests
    (6, 5);  -- Deployment depends on Integration tests

CREATE VIEW test_task_execution_order AS
WITH RECURSIVE task_order AS (
    -- Base: Tasks with no dependencies
    SELECT
        t.task_id,
        t.task_name,
        t.estimated_hours,
        0 AS depth,
        ARRAY[t.task_id] AS dependency_chain
    FROM test_tasks t
    WHERE NOT EXISTS (
        SELECT 1 FROM test_task_dependencies td WHERE td.task_id = t.task_id
    )

    UNION ALL

    -- Recursive: Tasks whose dependencies are satisfied
    SELECT
        t.task_id,
        t.task_name,
        t.estimated_hours,
        to2.depth + 1,
        to2.dependency_chain || t.task_id
    FROM task_order to2
    JOIN test_task_dependencies td ON td.depends_on_task_id = to2.task_id
    JOIN test_tasks t ON t.task_id = td.task_id
    WHERE NOT (t.task_id = ANY(to2.dependency_chain))
)
SELECT DISTINCT ON (task_id)
    task_id,
    task_name,
    estimated_hours,
    depth AS execution_stage
FROM task_order
ORDER BY task_id, depth DESC;

SELECT task_id, task_name, execution_stage FROM test_task_execution_order ORDER BY execution_stage, task_id;

-- ============================================================================
-- Section 12: Recursive Aggregation
-- ============================================================================

CREATE VIEW test_category_product_counts AS
WITH RECURSIVE category_counts AS (
    -- Leaf categories: count products directly
    SELECT
        c.category_id,
        c.category_name,
        c.parent_id,
        0 AS direct_product_count,  -- Simplified: no products table
        0 AS total_product_count,
        0 AS depth
    FROM test_categories c
    WHERE NOT EXISTS (
        SELECT 1 FROM test_categories c2 WHERE c2.parent_id = c.category_id
    )

    UNION ALL

    -- Parent categories: aggregate from children
    SELECT
        c.category_id,
        c.category_name,
        c.parent_id,
        0,
        SUM(cc.total_product_count),
        cc.depth + 1
    FROM category_counts cc
    JOIN test_categories c ON c.category_id = cc.parent_id
    GROUP BY c.category_id, c.category_name, c.parent_id, cc.depth
)
SELECT
    category_id,
    category_name,
    MAX(total_product_count) AS total_products_in_subtree
FROM category_counts
GROUP BY category_id, category_name;

SELECT category_id, category_name, total_products_in_subtree FROM test_category_product_counts ORDER BY category_id;

-- ============================================================================
-- Section 13: Cycle Detection
-- ============================================================================

-- Example of detecting cycles in a graph
CREATE TABLE test_graph_nodes (
    node_id INT PRIMARY KEY,
    node_name VARCHAR(100)
);

CREATE TABLE test_graph_edges (
    from_node INT REFERENCES test_graph_nodes(node_id),
    to_node INT REFERENCES test_graph_nodes(node_id)
);

INSERT INTO test_graph_nodes VALUES (1, 'A'), (2, 'B'), (3, 'C'), (4, 'D');
INSERT INTO test_graph_edges VALUES (1, 2), (2, 3), (3, 4);  -- No cycle

-- Create a view that would detect if there's a cycle
CREATE VIEW test_graph_traversal AS
WITH RECURSIVE graph_walk AS (
    SELECT
        from_node,
        to_node,
        ARRAY[from_node, to_node] AS path,
        FALSE AS has_cycle
    FROM test_graph_edges

    UNION ALL

    SELECT
        gw.from_node,
        e.to_node,
        gw.path || e.to_node,
        e.to_node = ANY(gw.path) AS has_cycle
    FROM graph_walk gw
    JOIN test_graph_edges e ON gw.to_node = e.from_node
    WHERE NOT (e.to_node = ANY(gw.path))
      AND array_length(gw.path, 1) < 10  -- Limit depth
)
SELECT
    from_node,
    to_node,
    path,
    has_cycle
FROM graph_walk;

SELECT from_node, to_node, path, has_cycle FROM test_graph_traversal ORDER BY array_length(path, 1), from_node;

-- ============================================================================
-- Section 14: Hierarchical Rollup
-- ============================================================================

CREATE VIEW test_employee_rollup AS
WITH RECURSIVE emp_hierarchy AS (
    -- Leaf employees (no subordinates)
    SELECT
        e.employee_id,
        e.name,
        e.manager_id,
        e.salary,
        e.salary AS total_team_salary,
        1 AS team_size
    FROM test_employees e
    WHERE NOT EXISTS (
        SELECT 1 FROM test_employees e2 WHERE e2.manager_id = e.employee_id
    )

    UNION ALL

    -- Managers: include their own salary + team's salary
    SELECT
        m.employee_id,
        m.name,
        m.manager_id,
        m.salary,
        m.salary + SUM(eh.total_team_salary),
        1 + SUM(eh.team_size)
    FROM emp_hierarchy eh
    JOIN test_employees m ON m.employee_id = eh.manager_id
    GROUP BY m.employee_id, m.name, m.manager_id, m.salary
)
SELECT
    employee_id,
    name,
    salary AS own_salary,
    total_team_salary,
    team_size
FROM emp_hierarchy;

SELECT name, own_salary, total_team_salary, team_size FROM test_employee_rollup ORDER BY total_team_salary DESC;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_recursive_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_recursive_best_practices VALUES
    (1, 'Use WITH RECURSIVE for hierarchical and graph data'),
    (2, 'Always include a termination condition to prevent infinite recursion'),
    (3, 'Track visited nodes in path arrays to avoid cycles'),
    (4, 'Limit recursion depth with explicit depth tracking'),
    (5, 'Use UNION (not UNION ALL) to avoid duplicate processing'),
    (6, 'Test with realistic data sizes and depths'),
    (7, 'Monitor query performance and recursion depth'),
    (8, 'Document the hierarchy structure and expected depth'),
    (9, 'Consider materialized views for complex recursive queries'),
    (10, 'Use recursive CTEs for org charts, BOMs, and graphs'),
    (11, 'Add indexes on foreign keys used in recursive joins'),
    (12, 'Validate data integrity to prevent cycles in hierarchies'),
    (13, 'Use path tracking for audit trails and debugging'),
    (14, 'Consider max_recursion_depth setting for safety'),
    (15, 'Test cycle detection logic thoroughly');

SELECT id, guideline FROM test_recursive_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_recursive_best_practices;
DROP VIEW test_employee_rollup;
DROP VIEW test_graph_traversal;
DROP TABLE test_graph_edges;
DROP TABLE test_graph_nodes;
DROP VIEW test_category_product_counts;
DROP VIEW test_task_execution_order;
DROP TABLE test_task_dependencies;
DROP TABLE test_tasks;
DROP VIEW test_ancestors;
DROP TABLE test_persons;
DROP VIEW test_all_paths_a_to_e;
DROP TABLE test_routes;
DROP TABLE test_locations;
DROP VIEW test_fibonacci;
DROP VIEW test_factorials;
DROP VIEW test_date_series;
DROP VIEW test_friend_network;
DROP TABLE test_friendships;
DROP TABLE test_users;
DROP VIEW test_bom_explosion;
DROP TABLE test_bom;
DROP TABLE test_parts;
DROP VIEW test_category_tree;
DROP TABLE test_categories;
DROP VIEW test_org_chart;
DROP TABLE test_employees;
DROP VIEW test_number_sequence;

DROP DATABASE test_recursive_views_db;

-- End of recursive view tests
