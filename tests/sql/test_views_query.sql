-- ScratchBird
-- Copyright (c) 2025-2026 Dalton Calford
--
-- Licensed under the Initial Developer's Public License Version 1.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at:
-- https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/

-- Test VIEW Query Execution
-- Tests view expansion and query execution
-- ALPHA Phase 1 - Complete Views Implementation

-- ===== Setup: Create test table =====
CREATE TABLE employees (
    id INTEGER,
    name VARCHAR(100),
    department VARCHAR(50),
    salary INTEGER,
    active BOOLEAN
);

INSERT INTO employees VALUES (1, 'Alice', 'Engineering', 100000, true);
INSERT INTO employees VALUES (2, 'Bob', 'Sales', 80000, true);
INSERT INTO employees VALUES (3, 'Charlie', 'Engineering', 95000, false);
INSERT INTO employees VALUES (4, 'David', 'Marketing', 75000, true);
INSERT INTO employees VALUES (5, 'Eve', 'Engineering', 105000, true);

-- ===== Test 1: CREATE VIEW and query it =====
CREATE VIEW active_employees AS
    SELECT id, name, department, salary
    FROM employees
    WHERE active = true;

-- Query the view (should return 4 rows: Alice, Bob, David, Eve)
SELECT * FROM active_employees;

-- ===== Test 2: CREATE VIEW with WHERE clause and query =====
CREATE VIEW high_earners AS
    SELECT name, salary
    FROM employees
    WHERE salary > 90000;

-- Query the view (should return 3 rows: Alice, Charlie, Eve)
SELECT * FROM high_earners;

-- ===== Test 3: CREATE VIEW with aggregation and query =====
CREATE VIEW department_stats AS
    SELECT department, COUNT(*) as employee_count
    FROM employees
    GROUP BY department;

-- Query the view (should return 3 rows: Engineering=3, Sales=1, Marketing=1)
SELECT * FROM department_stats;

-- ===== Test 4: Query view with additional WHERE clause =====
-- This tests applying filters on top of view expansion
SELECT name, salary FROM active_employees WHERE salary > 80000;

-- ===== Test 5: Views referencing views =====
CREATE VIEW senior_engineers AS
    SELECT * FROM active_employees WHERE department = 'Engineering';

-- Query nested view
SELECT * FROM senior_engineers;

-- ===== Test 6: Recursive view detection =====
-- This should detect the cycle and error
-- CREATE VIEW recursive_view AS SELECT * FROM recursive_view;

-- ===== Clean up =====
DROP VIEW IF EXISTS senior_engineers;
DROP VIEW IF EXISTS department_stats;
DROP VIEW IF EXISTS high_earners;
DROP VIEW IF EXISTS active_employees;
DROP TABLE employees;

-- Expected Results:
-- ✅ Test 1: 4 rows (active employees)
-- ✅ Test 2: 3 rows (high earners)
-- ✅ Test 3: 3 rows (department stats)
-- ✅ Test 4: 3 rows (Alice, David, Eve with salary > 80000)
-- ✅ Test 5: 2 rows (active engineers: Alice, Eve)
-- ❌ Test 6: Should error with "Recursive view reference detected"
