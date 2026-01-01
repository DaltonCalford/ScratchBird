-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Views - Basic and Intermediate Views
-- Description: Comprehensive basic view testing
-- ============================================================================

-- Views are virtual tables defined by queries
-- Do not store data themselves (except materialized views)
-- Can simplify complex queries and provide abstraction
-- Can enforce security by limiting column/row access

-- Create test database
CREATE DATABASE test_basic_views_db;
USE test_basic_views_db;

-- ============================================================================
-- Section 1: Simple SELECT View
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    email VARCHAR(200),
    salary NUMERIC(12,2),
    department VARCHAR(100),
    hire_date DATE
);

INSERT INTO test_employees (first_name, last_name, email, salary, department, hire_date) VALUES
    ('Alice', 'Johnson', 'alice@example.com', 75000.00, 'Engineering', '2020-01-15'),
    ('Bob', 'Smith', 'bob@example.com', 65000.00, 'Engineering', '2021-03-20'),
    ('Charlie', 'Brown', 'charlie@example.com', 55000.00, 'Sales', '2019-11-10'),
    ('Diana', 'Prince', 'diana@example.com', 85000.00, 'Management', '2018-06-01');

CREATE VIEW test_employee_list AS
SELECT employee_id, first_name, last_name, department
FROM test_employees;

SELECT employee_id, first_name, last_name, department FROM test_employee_list ORDER BY employee_id;

-- ============================================================================
-- Section 2: View with WHERE Clause
-- ============================================================================

CREATE VIEW test_engineering_employees AS
SELECT employee_id, first_name, last_name, email, salary
FROM test_employees
WHERE department = 'Engineering';

SELECT employee_id, first_name, last_name FROM test_engineering_employees ORDER BY employee_id;

-- ============================================================================
-- Section 3: View with Computed Columns
-- ============================================================================

CREATE VIEW test_employee_info AS
SELECT
    employee_id,
    first_name || ' ' || last_name AS full_name,
    email,
    salary,
    salary * 12 AS annual_salary,
    department,
    EXTRACT(YEAR FROM AGE(CURRENT_DATE, hire_date)) AS years_employed
FROM test_employees;

SELECT employee_id, full_name, annual_salary, years_employed FROM test_employee_info ORDER BY employee_id;

-- ============================================================================
-- Section 4: View with Aggregation
-- ============================================================================

CREATE VIEW test_department_summary AS
SELECT
    department,
    COUNT(*) AS employee_count,
    AVG(salary) AS avg_salary,
    MIN(salary) AS min_salary,
    MAX(salary) AS max_salary,
    SUM(salary) AS total_payroll
FROM test_employees
GROUP BY department;

SELECT department, employee_count, avg_salary, total_payroll FROM test_department_summary ORDER BY department;

-- ============================================================================
-- Section 5: View with JOIN
-- ============================================================================

CREATE TABLE test_departments (
    department_id SERIAL PRIMARY KEY,
    department_name VARCHAR(100),
    manager_name VARCHAR(200),
    budget NUMERIC(15,2)
);

INSERT INTO test_departments (department_name, manager_name, budget) VALUES
    ('Engineering', 'Eve Wilson', 500000.00),
    ('Sales', 'Frank Miller', 300000.00),
    ('Management', 'Grace Lee', 200000.00);

-- Update employees to use department_id
ALTER TABLE test_employees ADD COLUMN department_id INT;
UPDATE test_employees SET department_id = 1 WHERE department = 'Engineering';
UPDATE test_employees SET department_id = 2 WHERE department = 'Sales';
UPDATE test_employees SET department_id = 3 WHERE department = 'Management';

CREATE VIEW test_employee_department_view AS
SELECT
    e.employee_id,
    e.first_name,
    e.last_name,
    e.salary,
    d.department_name,
    d.manager_name,
    d.budget
FROM test_employees e
JOIN test_departments d ON e.department_id = d.department_id;

SELECT employee_id, first_name, last_name, department_name, manager_name FROM test_employee_department_view ORDER BY employee_id;

-- ============================================================================
-- Section 6: View with Multiple JOINs
-- ============================================================================

CREATE TABLE test_projects (
    project_id SERIAL PRIMARY KEY,
    project_name VARCHAR(200),
    start_date DATE,
    end_date DATE
);

CREATE TABLE test_employee_projects (
    assignment_id SERIAL PRIMARY KEY,
    employee_id INT REFERENCES test_employees(employee_id),
    project_id INT REFERENCES test_projects(project_id),
    role VARCHAR(100),
    hours_allocated INT
);

INSERT INTO test_projects (project_name, start_date, end_date) VALUES
    ('Project Alpha', '2024-01-01', '2024-06-30'),
    ('Project Beta', '2024-03-01', '2024-12-31');

INSERT INTO test_employee_projects (employee_id, project_id, role, hours_allocated) VALUES
    (1, 1, 'Lead Developer', 40),
    (2, 1, 'Developer', 30),
    (1, 2, 'Consultant', 10),
    (3, 2, 'Sales Lead', 20);

CREATE VIEW test_project_assignments AS
SELECT
    e.employee_id,
    e.first_name || ' ' || e.last_name AS employee_name,
    d.department_name,
    p.project_name,
    ep.role,
    ep.hours_allocated
FROM test_employees e
JOIN test_employee_projects ep ON e.employee_id = ep.employee_id
JOIN test_projects p ON ep.project_id = p.project_id
JOIN test_departments d ON e.department_id = d.department_id;

SELECT employee_name, department_name, project_name, role, hours_allocated FROM test_project_assignments ORDER BY employee_name, project_name;

-- ============================================================================
-- Section 7: View with LEFT JOIN
-- ============================================================================

CREATE VIEW test_all_employees_projects AS
SELECT
    e.employee_id,
    e.first_name || ' ' || e.last_name AS employee_name,
    p.project_name,
    COALESCE(ep.role, 'No assignment') AS role,
    COALESCE(ep.hours_allocated, 0) AS hours_allocated
FROM test_employees e
LEFT JOIN test_employee_projects ep ON e.employee_id = ep.employee_id
LEFT JOIN test_projects p ON ep.project_id = p.project_id;

SELECT employee_name, project_name, role FROM test_all_employees_projects ORDER BY employee_name, project_name NULLS LAST;

-- ============================================================================
-- Section 8: View with UNION
-- ============================================================================

CREATE TABLE test_contractors (
    contractor_id SERIAL PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    hourly_rate NUMERIC(10,2),
    specialty VARCHAR(100)
);

INSERT INTO test_contractors (first_name, last_name, hourly_rate, specialty) VALUES
    ('Jane', 'Doe', 150.00, 'Database'),
    ('John', 'Roe', 120.00, 'Frontend');

CREATE VIEW test_all_workers AS
SELECT
    employee_id AS id,
    first_name,
    last_name,
    'Employee' AS worker_type,
    department AS category
FROM test_employees
UNION ALL
SELECT
    contractor_id AS id,
    first_name,
    last_name,
    'Contractor' AS worker_type,
    specialty AS category
FROM test_contractors;

SELECT id, first_name, last_name, worker_type, category FROM test_all_workers ORDER BY worker_type, id;

-- ============================================================================
-- Section 9: View with Subquery
-- ============================================================================

CREATE VIEW test_high_earners AS
SELECT
    employee_id,
    first_name,
    last_name,
    salary,
    department
FROM test_employees
WHERE salary > (SELECT AVG(salary) FROM test_employees);

SELECT employee_id, first_name, last_name, salary FROM test_high_earners ORDER BY salary DESC;

-- ============================================================================
-- Section 10: View with CASE Expression
-- ============================================================================

CREATE VIEW test_employee_classification AS
SELECT
    employee_id,
    first_name,
    last_name,
    salary,
    CASE
        WHEN salary >= 80000 THEN 'Senior'
        WHEN salary >= 60000 THEN 'Mid-Level'
        ELSE 'Junior'
    END AS salary_grade,
    CASE
        WHEN EXTRACT(YEAR FROM AGE(CURRENT_DATE, hire_date)) >= 5 THEN 'Veteran'
        WHEN EXTRACT(YEAR FROM AGE(CURRENT_DATE, hire_date)) >= 2 THEN 'Experienced'
        ELSE 'New'
    END AS tenure_status
FROM test_employees;

SELECT employee_id, first_name, last_name, salary_grade, tenure_status FROM test_employee_classification ORDER BY employee_id;

-- ============================================================================
-- Section 11: Nested Views (View on View)
-- ============================================================================

CREATE VIEW test_senior_engineers AS
SELECT
    employee_id,
    first_name,
    last_name,
    salary,
    salary_grade
FROM test_employee_classification
WHERE salary_grade = 'Senior';

-- View on top of the view
CREATE VIEW test_senior_engineer_summary AS
SELECT
    COUNT(*) AS senior_count,
    AVG(salary) AS avg_senior_salary,
    MAX(salary) AS max_senior_salary
FROM test_senior_engineers;

SELECT senior_count, avg_senior_salary, max_senior_salary FROM test_senior_engineer_summary;

-- ============================================================================
-- Section 12: View with Window Functions
-- ============================================================================

CREATE VIEW test_employee_rankings AS
SELECT
    employee_id,
    first_name,
    last_name,
    department,
    salary,
    RANK() OVER (ORDER BY salary DESC) AS overall_rank,
    RANK() OVER (PARTITION BY department ORDER BY salary DESC) AS department_rank,
    NTILE(4) OVER (ORDER BY salary) AS salary_quartile
FROM test_employees;

SELECT employee_id, first_name, last_name, overall_rank, department_rank, salary_quartile FROM test_employee_rankings ORDER BY overall_rank;

-- ============================================================================
-- Section 13: Security View (Column Filtering)
-- ============================================================================

-- View that hides sensitive columns
CREATE VIEW test_employee_public_info AS
SELECT
    employee_id,
    first_name,
    last_name,
    department,
    hire_date
FROM test_employees;
-- Note: email and salary are not exposed

SELECT employee_id, first_name, last_name, department FROM test_employee_public_info ORDER BY employee_id;

-- ============================================================================
-- Section 14: Security View (Row Filtering)
-- ============================================================================

-- View that shows only recent employees
CREATE VIEW test_recent_employees AS
SELECT
    employee_id,
    first_name,
    last_name,
    department,
    hire_date
FROM test_employees
WHERE hire_date >= '2020-01-01';

SELECT employee_id, first_name, last_name, hire_date FROM test_recent_employees ORDER BY hire_date;

-- ============================================================================
-- Section 15: View Metadata and Information
-- ============================================================================

-- Query view definition
SELECT
    viewname,
    definition
FROM pg_views
WHERE schemaname = 'public'
  AND viewname LIKE 'test_%'
ORDER BY viewname
LIMIT 5;

-- List all views
SELECT viewname
FROM pg_views
WHERE schemaname = 'public'
  AND viewname LIKE 'test_%'
ORDER BY viewname;

-- ============================================================================
-- Section 16: View Dependencies
-- ============================================================================

-- test_senior_engineer_summary depends on test_senior_engineers
-- which depends on test_employee_classification
-- which depends on test_employees

-- Attempting to drop test_employee_classification would fail
-- because test_senior_engineers depends on it

-- DROP VIEW test_employee_classification;  -- Would fail with dependency error

-- Must drop in reverse order:
-- DROP VIEW test_senior_engineer_summary;
-- DROP VIEW test_senior_engineers;
-- DROP VIEW test_employee_classification;

-- ============================================================================
-- Section 17: ALTER VIEW
-- ============================================================================

-- Recreate a view with OR REPLACE
CREATE OR REPLACE VIEW test_employee_list AS
SELECT
    employee_id,
    first_name,
    last_name,
    department,
    email  -- Added email column
FROM test_employees;

SELECT employee_id, first_name, email FROM test_employee_list ORDER BY employee_id;

-- ============================================================================
-- Section 18: DROP VIEW
-- ============================================================================

-- Create a temporary view for testing drop
CREATE VIEW test_temp_view AS
SELECT employee_id, first_name FROM test_employees;

-- Verify it exists
SELECT viewname FROM pg_views WHERE viewname = 'test_temp_view';

-- Drop the view
DROP VIEW test_temp_view;

-- Verify it's gone
SELECT viewname FROM pg_views WHERE viewname = 'test_temp_view';  -- Should return no rows

-- ============================================================================
-- Section 19: View with DISTINCT
-- ============================================================================

CREATE VIEW test_unique_departments AS
SELECT DISTINCT department FROM test_employees ORDER BY department;

SELECT department FROM test_unique_departments;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_view_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_view_best_practices VALUES
    (1, 'Use views to simplify complex queries and provide abstraction'),
    (2, 'Views do not store data (except materialized views)'),
    (3, 'Use views for security by hiding sensitive columns'),
    (4, 'Use views for row-level security with WHERE clauses'),
    (5, 'Views can be built on top of other views (nesting)'),
    (6, 'Be careful with view dependencies when dropping'),
    (7, 'Use CREATE OR REPLACE VIEW to modify existing views'),
    (8, 'Consider performance: views execute the query each time'),
    (9, 'Use materialized views for expensive queries that change infrequently'),
    (10, 'Document view purpose and dependencies'),
    (11, 'Limit nesting depth to maintain performance'),
    (12, 'Use views for consistent business logic across applications'),
    (13, 'Test view performance with realistic data volumes'),
    (14, 'Consider using indexes on underlying tables'),
    (15, 'Name views clearly to distinguish from tables');

SELECT id, guideline FROM test_view_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP VIEW test_unique_departments;
DROP TABLE test_view_best_practices;
DROP VIEW test_employee_list;
DROP VIEW test_senior_engineer_summary;
DROP VIEW test_senior_engineers;
DROP VIEW test_employee_rankings;
DROP VIEW test_employee_public_info;
DROP VIEW test_recent_employees;
DROP VIEW test_employee_classification;
DROP VIEW test_high_earners;
DROP VIEW test_all_workers;
DROP TABLE test_contractors;
DROP VIEW test_all_employees_projects;
DROP VIEW test_project_assignments;
DROP TABLE test_employee_projects;
DROP TABLE test_projects;
DROP VIEW test_employee_department_view;
DROP TABLE test_departments;
DROP VIEW test_department_summary;
DROP VIEW test_employee_info;
DROP VIEW test_engineering_employees;
DROP TABLE test_employees;

DROP DATABASE test_basic_views_db;

-- End of basic view tests
