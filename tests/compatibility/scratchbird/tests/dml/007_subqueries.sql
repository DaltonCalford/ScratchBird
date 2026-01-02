-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - Subqueries and Correlated Queries
-- Description: Comprehensive subquery pattern testing
-- ============================================================================

-- Subqueries: Nested queries and correlated subqueries
-- Scalar, row, table subqueries
-- Correlated vs non-correlated
-- Performance optimization patterns

-- Create test database
CREATE DATABASE test_subqueries_db;
USE test_subqueries_db;

-- ============================================================================
-- Section 1: Scalar Subqueries
-- ============================================================================

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(100),
    department VARCHAR(50),
    salary NUMERIC(10,2),
    hire_date DATE
);

INSERT INTO test_employees VALUES
    (1, 'Alice', 'Engineering', 90000, '2020-01-15'),
    (2, 'Bob', 'Engineering', 95000, '2019-06-10'),
    (3, 'Charlie', 'Sales', 70000, '2021-03-20'),
    (4, 'Diana', 'Sales', 75000, '2020-11-05'),
    (5, 'Eve', 'HR', 65000, '2022-01-10');

-- Scalar subquery (returns single value)
SELECT
    emp_name,
    department,
    salary,
    (SELECT AVG(salary) FROM test_employees) AS company_avg_salary,
    salary - (SELECT AVG(salary) FROM test_employees) AS diff_from_avg
FROM test_employees
ORDER BY salary DESC;

-- Multiple scalar subqueries
SELECT
    emp_name,
    salary,
    (SELECT MIN(salary) FROM test_employees) AS min_salary,
    (SELECT MAX(salary) FROM test_employees) AS max_salary,
    (SELECT AVG(salary) FROM test_employees) AS avg_salary
FROM test_employees
ORDER BY emp_id;

-- ============================================================================
-- Section 2: Row Subqueries
-- ============================================================================

-- Row subquery (returns single row with multiple columns)
SELECT emp_name, department, salary
FROM test_employees
WHERE (department, salary) = (
    SELECT department, MAX(salary)
    FROM test_employees
    GROUP BY department
    ORDER BY MAX(salary) DESC
    LIMIT 1
);

-- Row comparison
SELECT emp_name, salary, hire_date
FROM test_employees
WHERE (salary, hire_date) > (70000, '2020-01-01')
ORDER BY salary;

-- ============================================================================
-- Section 3: Table Subqueries (FROM Clause)
-- ============================================================================

-- Subquery in FROM clause (derived table)
SELECT
    dept_stats.department,
    dept_stats.emp_count,
    dept_stats.avg_salary,
    dept_stats.total_payroll
FROM (
    SELECT
        department,
        COUNT(*) AS emp_count,
        AVG(salary) AS avg_salary,
        SUM(salary) AS total_payroll
    FROM test_employees
    GROUP BY department
) AS dept_stats
WHERE dept_stats.emp_count > 1
ORDER BY dept_stats.total_payroll DESC;

-- Multiple subqueries in FROM with JOIN
SELECT
    high.emp_name AS high_earner,
    high.salary AS high_salary,
    low.emp_name AS low_earner,
    low.salary AS low_salary,
    (high.salary - low.salary) AS salary_difference
FROM (
    SELECT emp_name, salary
    FROM test_employees
    WHERE salary > 80000
) high
CROSS JOIN (
    SELECT emp_name, salary
    FROM test_employees
    WHERE salary < 70000
) low
ORDER BY salary_difference DESC;

-- ============================================================================
-- Section 4: WHERE Clause Subqueries
-- ============================================================================

-- Employees earning above average
SELECT emp_name, salary
FROM test_employees
WHERE salary > (SELECT AVG(salary) FROM test_employees)
ORDER BY salary DESC;

-- Employees in departments with more than 1 person
SELECT emp_name, department
FROM test_employees
WHERE department IN (
    SELECT department
    FROM test_employees
    GROUP BY department
    HAVING COUNT(*) > 1
)
ORDER BY department, emp_name;

-- Employees with highest salary in their department
SELECT emp_name, department, salary
FROM test_employees e1
WHERE salary = (
    SELECT MAX(salary)
    FROM test_employees e2
    WHERE e2.department = e1.department
)
ORDER BY department;

-- ============================================================================
-- Section 5: IN and NOT IN Subqueries
-- ============================================================================

CREATE TABLE test_departments (
    dept_id INT PRIMARY KEY,
    dept_name VARCHAR(50),
    location VARCHAR(100)
);

INSERT INTO test_departments VALUES
    (1, 'Engineering', 'Building A'),
    (2, 'Sales', 'Building B'),
    (3, 'Marketing', 'Building C');

-- IN subquery
SELECT dept_name, location
FROM test_departments
WHERE dept_name IN (
    SELECT DISTINCT department
    FROM test_employees
)
ORDER BY dept_name;

-- NOT IN subquery (departments with no employees)
SELECT dept_name, location
FROM test_departments
WHERE dept_name NOT IN (
    SELECT DISTINCT department
    FROM test_employees
    WHERE department IS NOT NULL
)
ORDER BY dept_name;

-- ============================================================================
-- Section 6: EXISTS and NOT EXISTS
-- ============================================================================

-- EXISTS (departments with employees)
SELECT dept_name, location
FROM test_departments d
WHERE EXISTS (
    SELECT 1
    FROM test_employees e
    WHERE e.department = d.dept_name
)
ORDER BY dept_name;

-- NOT EXISTS (departments with no employees)
SELECT dept_name, location
FROM test_departments d
WHERE NOT EXISTS (
    SELECT 1
    FROM test_employees e
    WHERE e.department = d.dept_name
)
ORDER BY dept_name;

-- EXISTS with correlation (employees who are highest paid in their dept)
SELECT emp_name, department, salary
FROM test_employees e1
WHERE NOT EXISTS (
    SELECT 1
    FROM test_employees e2
    WHERE e2.department = e1.department
      AND e2.salary > e1.salary
)
ORDER BY department;

-- ============================================================================
-- Section 7: Correlated Subqueries in SELECT
-- ============================================================================

-- Correlated subquery (references outer query)
SELECT
    emp_name,
    department,
    salary,
    (
        SELECT AVG(salary)
        FROM test_employees e2
        WHERE e2.department = e1.department
    ) AS dept_avg_salary,
    salary - (
        SELECT AVG(salary)
        FROM test_employees e2
        WHERE e2.department = e1.department
    ) AS diff_from_dept_avg
FROM test_employees e1
ORDER BY department, emp_name;

-- Multiple correlated subqueries
SELECT
    emp_name,
    department,
    (
        SELECT COUNT(*)
        FROM test_employees e2
        WHERE e2.department = e1.department
    ) AS dept_employee_count,
    (
        SELECT MAX(salary)
        FROM test_employees e2
        WHERE e2.department = e1.department
    ) AS dept_max_salary,
    (
        SELECT MIN(hire_date)
        FROM test_employees e2
        WHERE e2.department = e1.department
    ) AS dept_first_hire_date
FROM test_employees e1
ORDER BY department, emp_name;

-- ============================================================================
-- Section 8: Correlated Subqueries in WHERE
-- ============================================================================

-- Find employees earning more than their department average
SELECT emp_name, department, salary
FROM test_employees e1
WHERE salary > (
    SELECT AVG(salary)
    FROM test_employees e2
    WHERE e2.department = e1.department
)
ORDER BY department, salary DESC;

-- Find newest hire in each department
SELECT emp_name, department, hire_date
FROM test_employees e1
WHERE hire_date = (
    SELECT MAX(hire_date)
    FROM test_employees e2
    WHERE e2.department = e1.department
)
ORDER BY department;

-- ============================================================================
-- Section 9: ALL, ANY, SOME Operators
-- ============================================================================

CREATE TABLE test_products (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(100),
    price NUMERIC(10,2)
);

CREATE TABLE test_competitor_prices (
    competitor_id INT,
    product_name VARCHAR(100),
    price NUMERIC(10,2)
);

INSERT INTO test_products VALUES
    (1, 'Widget A', 50.00),
    (2, 'Widget B', 75.00),
    (3, 'Gadget A', 100.00);

INSERT INTO test_competitor_prices VALUES
    (1, 'Widget A', 55.00),
    (2, 'Widget A', 52.00),
    (1, 'Widget B', 80.00),
    (2, 'Widget B', 78.00);

-- ALL operator (price lower than all competitors)
SELECT product_name, price
FROM test_products p
WHERE price < ALL (
    SELECT price
    FROM test_competitor_prices c
    WHERE c.product_name = p.product_name
)
ORDER BY product_name;

-- ANY/SOME operator (price lower than at least one competitor)
SELECT product_name, price
FROM test_products p
WHERE price < ANY (
    SELECT price
    FROM test_competitor_prices c
    WHERE c.product_name = p.product_name
)
ORDER BY product_name;

-- Greater than ALL (highest price)
SELECT emp_name, salary
FROM test_employees
WHERE salary >= ALL (SELECT salary FROM test_employees)
ORDER BY emp_name;

-- ============================================================================
-- Section 10: Subqueries with Aggregates
-- ============================================================================

-- Department with highest average salary
SELECT department, AVG(salary) AS avg_salary
FROM test_employees
GROUP BY department
HAVING AVG(salary) >= ALL (
    SELECT AVG(salary)
    FROM test_employees
    GROUP BY department
);

-- Employees in departments with total payroll > threshold
SELECT emp_name, department, salary
FROM test_employees
WHERE department IN (
    SELECT department
    FROM test_employees
    GROUP BY department
    HAVING SUM(salary) > 150000
)
ORDER BY department, emp_name;

-- ============================================================================
-- Section 11: Nested Subqueries (Multiple Levels)
-- ============================================================================

-- Three levels of nesting
SELECT emp_name, department, salary
FROM test_employees
WHERE department IN (
    SELECT department
    FROM test_employees
    WHERE salary > (
        SELECT AVG(salary)
        FROM test_employees
        WHERE department IN (
            SELECT DISTINCT department
            FROM test_employees
            WHERE hire_date > '2020-01-01'
        )
    )
    GROUP BY department
)
ORDER BY department, salary DESC;

-- ============================================================================
-- Section 12: Subquery vs JOIN Performance
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    order_date DATE,
    total NUMERIC(10,2)
);

CREATE TABLE test_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100),
    city VARCHAR(100)
);

INSERT INTO test_customers VALUES
    (1, 'Customer A', 'New York'),
    (2, 'Customer B', 'Los Angeles'),
    (3, 'Customer C', 'Chicago');

INSERT INTO test_orders VALUES
    (1, 1, '2024-01-15', 150.00),
    (2, 1, '2024-02-20', 200.00),
    (3, 2, '2024-01-10', 100.00),
    (4, 3, '2024-03-05', 300.00);

-- Using subquery
SELECT
    customer_name,
    (
        SELECT COUNT(*)
        FROM test_orders o
        WHERE o.customer_id = c.customer_id
    ) AS order_count,
    (
        SELECT COALESCE(SUM(total), 0)
        FROM test_orders o
        WHERE o.customer_id = c.customer_id
    ) AS total_spent
FROM test_customers c
ORDER BY customer_name;

-- Using JOIN (often more efficient)
SELECT
    c.customer_name,
    COUNT(o.order_id) AS order_count,
    COALESCE(SUM(o.total), 0) AS total_spent
FROM test_customers c
LEFT JOIN test_orders o ON c.customer_id = o.customer_id
GROUP BY c.customer_id, c.customer_name
ORDER BY c.customer_name;

-- ============================================================================
-- Section 13: Subqueries in CASE Expressions
-- ============================================================================

SELECT
    emp_name,
    department,
    salary,
    CASE
        WHEN salary > (
            SELECT AVG(salary)
            FROM test_employees
            WHERE department = e.department
        ) THEN 'Above Average'
        WHEN salary = (
            SELECT AVG(salary)
            FROM test_employees
            WHERE department = e.department
        ) THEN 'Average'
        ELSE 'Below Average'
    END AS salary_category
FROM test_employees e
ORDER BY department, salary DESC;

-- ============================================================================
-- Section 14: Subqueries with UPDATE
-- ============================================================================

CREATE TABLE test_employee_bonuses (
    emp_id INT PRIMARY KEY,
    bonus_amount NUMERIC(10,2)
);

INSERT INTO test_employee_bonuses (emp_id)
SELECT emp_id FROM test_employees;

-- Update using subquery
UPDATE test_employee_bonuses
SET bonus_amount = (
    SELECT salary * 0.10
    FROM test_employees
    WHERE test_employees.emp_id = test_employee_bonuses.emp_id
);

SELECT * FROM test_employee_bonuses ORDER BY emp_id;

-- Update with correlated subquery and WHERE
UPDATE test_employee_bonuses eb
SET bonus_amount = (
    SELECT
        CASE
            WHEN salary > 80000 THEN salary * 0.15
            WHEN salary > 70000 THEN salary * 0.12
            ELSE salary * 0.10
        END
    FROM test_employees e
    WHERE e.emp_id = eb.emp_id
)
WHERE emp_id IN (
    SELECT emp_id
    FROM test_employees
    WHERE department = 'Engineering'
);

SELECT * FROM test_employee_bonuses ORDER BY emp_id;

-- ============================================================================
-- Section 15: Subqueries with DELETE
-- ============================================================================

CREATE TABLE test_inactive_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100),
    last_order_date DATE
);

INSERT INTO test_inactive_customers
SELECT
    c.customer_id,
    c.customer_name,
    (SELECT MAX(order_date) FROM test_orders WHERE customer_id = c.customer_id)
FROM test_customers c;

-- Delete using subquery
DELETE FROM test_inactive_customers
WHERE customer_id NOT IN (
    SELECT DISTINCT customer_id
    FROM test_orders
    WHERE order_date > CURRENT_DATE - INTERVAL '90 days'
);

SELECT * FROM test_inactive_customers ORDER BY customer_id;

-- ============================================================================
-- Section 16: Subqueries with INSERT
-- ============================================================================

CREATE TABLE test_high_earners (
    emp_id INT PRIMARY KEY,
    emp_name VARCHAR(100),
    salary NUMERIC(10,2),
    department VARCHAR(50)
);

-- Insert using subquery
INSERT INTO test_high_earners (emp_id, emp_name, salary, department)
SELECT emp_id, emp_name, salary, department
FROM test_employees
WHERE salary > (SELECT AVG(salary) FROM test_employees);

SELECT * FROM test_high_earners ORDER BY salary DESC;

-- Insert with complex subquery
CREATE TABLE test_dept_summary (
    department VARCHAR(50) PRIMARY KEY,
    total_employees INT,
    avg_salary NUMERIC(10,2),
    min_salary NUMERIC(10,2),
    max_salary NUMERIC(10,2)
);

INSERT INTO test_dept_summary
SELECT
    department,
    COUNT(*) AS total_employees,
    AVG(salary) AS avg_salary,
    MIN(salary) AS min_salary,
    MAX(salary) AS max_salary
FROM test_employees
WHERE department IN (
    SELECT department
    FROM test_employees
    GROUP BY department
    HAVING COUNT(*) > 1
)
GROUP BY department;

SELECT * FROM test_dept_summary ORDER BY department;

-- ============================================================================
-- Section 17: Subqueries in HAVING Clause
-- ============================================================================

-- Departments with average salary above company average
SELECT
    department,
    COUNT(*) AS emp_count,
    AVG(salary) AS dept_avg_salary
FROM test_employees
GROUP BY department
HAVING AVG(salary) > (
    SELECT AVG(salary)
    FROM test_employees
)
ORDER BY dept_avg_salary DESC;

-- Departments with employee count above average department size
SELECT
    department,
    COUNT(*) AS emp_count
FROM test_employees
GROUP BY department
HAVING COUNT(*) > (
    SELECT AVG(dept_size)
    FROM (
        SELECT COUNT(*) AS dept_size
        FROM test_employees
        GROUP BY department
    ) dept_counts
)
ORDER BY emp_count DESC;

-- ============================================================================
-- Section 18: Lateral Subqueries
-- ============================================================================

-- LATERAL allows correlated subqueries in FROM clause
SELECT
    c.customer_name,
    recent_orders.order_count,
    recent_orders.total_amount
FROM test_customers c
LEFT JOIN LATERAL (
    SELECT
        COUNT(*) AS order_count,
        SUM(total) AS total_amount
    FROM test_orders o
    WHERE o.customer_id = c.customer_id
      AND o.order_date > CURRENT_DATE - INTERVAL '30 days'
) recent_orders ON true
ORDER BY c.customer_name;

-- LATERAL with top N per group
SELECT
    d.dept_name,
    top_earners.emp_name,
    top_earners.salary,
    top_earners.rank
FROM test_departments d
LEFT JOIN LATERAL (
    SELECT
        emp_name,
        salary,
        ROW_NUMBER() OVER (ORDER BY salary DESC) AS rank
    FROM test_employees e
    WHERE e.department = d.dept_name
    ORDER BY salary DESC
    LIMIT 2
) top_earners ON true
WHERE top_earners.emp_name IS NOT NULL
ORDER BY d.dept_name, top_earners.rank;

-- ============================================================================
-- Section 19: Common Subquery Pitfalls
-- ============================================================================

-- NULL handling in NOT IN
CREATE TABLE test_values (value INT);
INSERT INTO test_values VALUES (1), (2), (3);

CREATE TABLE test_exclude (value INT);
INSERT INTO test_exclude VALUES (2), (NULL);

-- NOT IN with NULL returns empty result (pitfall)
SELECT value
FROM test_values
WHERE value NOT IN (SELECT value FROM test_exclude);
-- Returns empty due to NULL

-- Safe alternative: NOT EXISTS
SELECT value
FROM test_values v
WHERE NOT EXISTS (
    SELECT 1
    FROM test_exclude e
    WHERE e.value = v.value
)
ORDER BY value;

-- Or use IS NOT NULL filter
SELECT value
FROM test_values
WHERE value NOT IN (
    SELECT value FROM test_exclude WHERE value IS NOT NULL
)
ORDER BY value;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_subquery_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_subquery_best_practices VALUES
    (1, 'Scalar subqueries return single value (one row, one column)'),
    (2, 'Table subqueries can return multiple rows and columns'),
    (3, 'Correlated subqueries reference outer query (slower)'),
    (4, 'Non-correlated subqueries execute once (faster)'),
    (5, 'EXISTS often faster than IN for large datasets'),
    (6, 'NOT EXISTS safer than NOT IN (NULL handling)'),
    (7, 'Consider JOIN instead of repeated correlated subqueries'),
    (8, 'LATERAL for correlated subqueries in FROM clause'),
    (9, 'ALL operator checks against all values'),
    (10, 'ANY/SOME checks against at least one value'),
    (11, 'Subqueries in SELECT execute per row (expensive)'),
    (12, 'Use window functions instead of correlated subqueries'),
    (13, 'Index columns used in subquery join conditions'),
    (14, 'Avoid nested subqueries beyond 2-3 levels'),
    (15, 'Use CTEs for complex subqueries (readability)'),
    (16, 'NOT IN with NULLs returns empty set (pitfall)'),
    (17, 'Subqueries with aggregates useful for filtering'),
    (18, 'Test subquery performance with EXPLAIN'),
    (19, 'Consider materialized CTEs for reused subqueries'),
    (20, 'Document complex subquery logic for maintenance');

SELECT id, guideline FROM test_subquery_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_subquery_best_practices;
DROP TABLE test_exclude;
DROP TABLE test_values;
DROP TABLE test_dept_summary;
DROP TABLE test_high_earners;
DROP TABLE test_inactive_customers;
DROP TABLE test_employee_bonuses;
DROP TABLE test_customers;
DROP TABLE test_orders;
DROP TABLE test_competitor_prices;
DROP TABLE test_products;
DROP TABLE test_departments;
DROP TABLE test_employees;

DROP DATABASE test_subqueries_db;

-- End of subqueries tests
