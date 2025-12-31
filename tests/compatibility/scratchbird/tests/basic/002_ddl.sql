-- ScratchBird Native Test: DDL Operations
-- Test ID: basic_002
-- Description: CREATE, ALTER, DROP operations for tables, indexes, and constraints

-- Create database
CREATE DATABASE test_ddl;
\c test_ddl;

-- Test CREATE TABLE
CREATE TABLE employees (
    emp_id INTEGER PRIMARY KEY,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE,
    hire_date DATE,
    salary DECIMAL(10, 2),
    department_id INTEGER
);

\d employees;

-- Test ALTER TABLE - Add column
ALTER TABLE employees ADD COLUMN phone VARCHAR(20);
\d employees;

-- Test ALTER TABLE - Add constraint
ALTER TABLE employees ADD CONSTRAINT check_salary CHECK (salary > 0);

-- Test CREATE INDEX
CREATE INDEX idx_employees_last_name ON employees(last_name);
CREATE INDEX idx_employees_dept ON employees(department_id);

\di employees;

-- Test INSERT with constraints
INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary, department_id)
VALUES (1, 'John', 'Doe', 'john.doe@example.com', '2020-01-15', 75000.00, 10);

INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary, department_id)
VALUES (2, 'Jane', 'Smith', 'jane.smith@example.com', '2021-03-20', 82000.00, 20);

SELECT * FROM employees ORDER BY emp_id;

-- Test UNIQUE constraint violation (should fail)
INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary)
VALUES (3, 'Bob', 'Johnson', 'john.doe@example.com', '2022-05-10', 68000.00);

-- Test CHECK constraint violation (should fail)
INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary)
VALUES (4, 'Alice', 'Williams', 'alice.w@example.com', '2022-07-01', -5000.00);

-- Test NOT NULL constraint violation (should fail)
INSERT INTO employees (emp_id, first_name, last_name, email)
VALUES (5, NULL, 'Brown', 'brown@example.com');

-- Test ALTER TABLE - Drop column
ALTER TABLE employees DROP COLUMN phone;
\d employees;

-- Test DROP INDEX
DROP INDEX idx_employees_dept;
\di employees;

-- Test CREATE TABLE with FOREIGN KEY
CREATE TABLE departments (
    dept_id INTEGER PRIMARY KEY,
    dept_name VARCHAR(100) NOT NULL,
    location VARCHAR(100)
);

ALTER TABLE employees ADD CONSTRAINT fk_department
    FOREIGN KEY (department_id) REFERENCES departments(dept_id);

-- Insert department data
INSERT INTO departments VALUES (10, 'Engineering', 'Building A');
INSERT INTO departments VALUES (20, 'Sales', 'Building B');

-- Test foreign key constraint (should succeed)
INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary, department_id)
VALUES (6, 'Charlie', 'Davis', 'charlie.d@example.com', '2023-01-10', 72000.00, 10);

-- Test foreign key violation (should fail)
INSERT INTO employees (emp_id, first_name, last_name, email, hire_date, salary, department_id)
VALUES (7, 'Eve', 'Wilson', 'eve.w@example.com', '2023-02-15', 70000.00, 99);

-- Test DROP TABLE with foreign key (should fail)
DROP TABLE departments;

-- Test DROP TABLE with CASCADE
DROP TABLE employees CASCADE;
DROP TABLE departments;

-- Test CREATE TABLE AS SELECT
CREATE TABLE temp_data AS SELECT generate_series(1, 10) AS id, 'Value_' || generate_series(1, 10) AS value;
SELECT COUNT(*) FROM temp_data;
DROP TABLE temp_data;

-- Cleanup
DROP DATABASE test_ddl;
