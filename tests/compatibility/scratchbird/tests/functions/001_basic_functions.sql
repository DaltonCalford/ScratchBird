-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Functions - Basic SQL and PL/pgSQL Functions
-- Description: Comprehensive basic function testing
-- ============================================================================

-- Functions are reusable code blocks that return values
-- Can be written in SQL, PL/pgSQL, or other languages
-- Support parameters, local variables, control flow
-- Used for calculations, data transformations, business logic

-- Create test database
CREATE DATABASE test_basic_functions_db;
USE test_basic_functions_db;

-- ============================================================================
-- Section 1: Simple SQL Function
-- ============================================================================

CREATE FUNCTION test_add_numbers(a INT, b INT) RETURNS INT AS $$
    SELECT a + b;
$$ LANGUAGE SQL IMMUTABLE;

SELECT test_add_numbers(5, 3) AS result;  -- Should return 8
SELECT test_add_numbers(10, -5) AS result;  -- Should return 5

-- ============================================================================
-- Section 2: SQL Function with Multiple Statements
-- ============================================================================

CREATE FUNCTION test_calculate_total(quantity INT, price NUMERIC) RETURNS NUMERIC AS $$
    SELECT quantity * price * 1.1;  -- Add 10% markup
$$ LANGUAGE SQL IMMUTABLE;

SELECT test_calculate_total(10, 25.50) AS total;

-- ============================================================================
-- Section 3: Basic PL/pgSQL Function
-- ============================================================================

CREATE FUNCTION test_factorial(n INT) RETURNS BIGINT AS $$
DECLARE
    result BIGINT := 1;
    i INT;
BEGIN
    IF n < 0 THEN
        RAISE EXCEPTION 'Factorial not defined for negative numbers';
    END IF;

    FOR i IN 1..n LOOP
        result := result * i;
    END LOOP;

    RETURN result;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_factorial(5) AS factorial_5;  -- Should return 120
SELECT test_factorial(10) AS factorial_10;  -- Should return 3628800

-- ============================================================================
-- Section 4: Function with CASE Statement
-- ============================================================================

CREATE FUNCTION test_grade_letter(score INT) RETURNS VARCHAR(2) AS $$
BEGIN
    RETURN CASE
        WHEN score >= 90 THEN 'A'
        WHEN score >= 80 THEN 'B'
        WHEN score >= 70 THEN 'C'
        WHEN score >= 60 THEN 'D'
        ELSE 'F'
    END;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_grade_letter(95) AS grade;  -- Should return 'A'
SELECT test_grade_letter(75) AS grade;  -- Should return 'C'
SELECT test_grade_letter(55) AS grade;  -- Should return 'F'

-- ============================================================================
-- Section 5: Function with IF-THEN-ELSE
-- ============================================================================

CREATE FUNCTION test_is_even(n INT) RETURNS BOOLEAN AS $$
BEGIN
    IF n % 2 = 0 THEN
        RETURN TRUE;
    ELSE
        RETURN FALSE;
    END IF;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_is_even(4) AS is_four_even;  -- TRUE
SELECT test_is_even(7) AS is_seven_even;  -- FALSE

-- ============================================================================
-- Section 6: Function with LOOP
-- ============================================================================

CREATE FUNCTION test_sum_to_n(n INT) RETURNS BIGINT AS $$
DECLARE
    total BIGINT := 0;
    i INT := 1;
BEGIN
    LOOP
        EXIT WHEN i > n;
        total := total + i;
        i := i + 1;
    END LOOP;

    RETURN total;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_sum_to_n(10) AS sum;  -- Should return 55 (1+2+3+...+10)
SELECT test_sum_to_n(100) AS sum;  -- Should return 5050

-- ============================================================================
-- Section 7: Function with WHILE Loop
-- ============================================================================

CREATE FUNCTION test_fibonacci(n INT) RETURNS BIGINT AS $$
DECLARE
    a BIGINT := 0;
    b BIGINT := 1;
    temp BIGINT;
    i INT := 0;
BEGIN
    IF n <= 0 THEN
        RETURN 0;
    END IF;

    WHILE i < n LOOP
        temp := a + b;
        a := b;
        b := temp;
        i := i + 1;
    END LOOP;

    RETURN a;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_fibonacci(10) AS fib_10;  -- Should return 55
SELECT test_fibonacci(15) AS fib_15;  -- Should return 610

-- ============================================================================
-- Section 8: Function with FOR Loop
-- ============================================================================

CREATE FUNCTION test_power(base INT, exponent INT) RETURNS BIGINT AS $$
DECLARE
    result BIGINT := 1;
    i INT;
BEGIN
    FOR i IN 1..exponent LOOP
        result := result * base;
    END LOOP;

    RETURN result;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_power(2, 10) AS power;  -- Should return 1024
SELECT test_power(5, 3) AS power;  -- Should return 125

-- ============================================================================
-- Section 9: Function with Query Result
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    salary NUMERIC(12,2),
    department VARCHAR(100)
);

INSERT INTO test_employees (name, salary, department) VALUES
    ('Alice', 75000, 'Engineering'),
    ('Bob', 65000, 'Engineering'),
    ('Charlie', 55000, 'Sales'),
    ('Diana', 85000, 'Management');

CREATE FUNCTION test_get_avg_salary(dept VARCHAR) RETURNS NUMERIC AS $$
DECLARE
    avg_sal NUMERIC;
BEGIN
    SELECT AVG(salary) INTO avg_sal
    FROM test_employees
    WHERE department = dept;

    RETURN COALESCE(avg_sal, 0);
END;
$$ LANGUAGE plpgsql STABLE;

SELECT test_get_avg_salary('Engineering') AS avg_eng_salary;
SELECT test_get_avg_salary('Sales') AS avg_sales_salary;

-- ============================================================================
-- Section 10: Function with Multiple OUT Parameters
-- ============================================================================

CREATE FUNCTION test_min_max(a INT, b INT, c INT, OUT minimum INT, OUT maximum INT) AS $$
BEGIN
    minimum := LEAST(a, b, c);
    maximum := GREATEST(a, b, c);
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_min_max(5, 12, 8);

-- ============================================================================
-- Section 11: Function Returning RECORD
-- ============================================================================

CREATE FUNCTION test_employee_stats(dept VARCHAR)
RETURNS TABLE(avg_salary NUMERIC, min_salary NUMERIC, max_salary NUMERIC, emp_count BIGINT) AS $$
BEGIN
    RETURN QUERY
    SELECT
        AVG(salary)::NUMERIC,
        MIN(salary)::NUMERIC,
        MAX(salary)::NUMERIC,
        COUNT(*)
    FROM test_employees
    WHERE department = dept;
END;
$$ LANGUAGE plpgsql STABLE;

SELECT * FROM test_employee_stats('Engineering');

-- ============================================================================
-- Section 12: Function with DEFAULT Parameters
-- ============================================================================

CREATE FUNCTION test_greeting(name VARCHAR, title VARCHAR DEFAULT 'Mr./Ms.') RETURNS TEXT AS $$
BEGIN
    RETURN 'Hello, ' || title || ' ' || name || '!';
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_greeting('Smith') AS greeting;  -- Uses default title
SELECT test_greeting('Johnson', 'Dr.') AS greeting;  -- Uses custom title

-- ============================================================================
-- Section 13: Function with VARIADIC Parameters
-- ============================================================================

CREATE FUNCTION test_sum_all(VARIADIC numbers INT[]) RETURNS BIGINT AS $$
DECLARE
    total BIGINT := 0;
    num INT;
BEGIN
    FOREACH num IN ARRAY numbers LOOP
        total := total + num;
    END LOOP;

    RETURN total;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_sum_all(1, 2, 3, 4, 5) AS sum;  -- Should return 15
SELECT test_sum_all(10, 20, 30) AS sum;  -- Should return 60

-- ============================================================================
-- Section 14: Function with EXCEPTION Handling
-- ============================================================================

CREATE FUNCTION test_safe_divide(numerator NUMERIC, denominator NUMERIC) RETURNS NUMERIC AS $$
BEGIN
    IF denominator = 0 THEN
        RAISE EXCEPTION 'Division by zero';
    END IF;

    RETURN numerator / denominator;
EXCEPTION
    WHEN division_by_zero THEN
        RETURN NULL;
    WHEN OTHERS THEN
        RAISE NOTICE 'An error occurred: %', SQLERRM;
        RETURN NULL;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_safe_divide(10, 2) AS result;  -- Should return 5
SELECT test_safe_divide(10, 0) AS result;  -- Should return NULL

-- ============================================================================
-- Section 15: Function with RETURN NEXT
-- ============================================================================

CREATE FUNCTION test_generate_series_custom(start_val INT, end_val INT)
RETURNS SETOF INT AS $$
DECLARE
    current_val INT := start_val;
BEGIN
    WHILE current_val <= end_val LOOP
        RETURN NEXT current_val;
        current_val := current_val + 1;
    END LOOP;
    RETURN;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_generate_series_custom(5, 10);

-- ============================================================================
-- Section 16: Function with RETURN QUERY
-- ============================================================================

CREATE FUNCTION test_get_high_earners(min_salary NUMERIC)
RETURNS TABLE(emp_name VARCHAR, emp_salary NUMERIC, emp_dept VARCHAR) AS $$
BEGIN
    RETURN QUERY
    SELECT name, salary, department
    FROM test_employees
    WHERE salary >= min_salary
    ORDER BY salary DESC;
END;
$$ LANGUAGE plpgsql STABLE;

SELECT * FROM test_get_high_earners(70000);

-- ============================================================================
-- Section 17: Recursive Function
-- ============================================================================

CREATE FUNCTION test_recursive_factorial(n INT) RETURNS BIGINT AS $$
BEGIN
    IF n <= 1 THEN
        RETURN 1;
    ELSE
        RETURN n * test_recursive_factorial(n - 1);
    END IF;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_recursive_factorial(5) AS factorial;
SELECT test_recursive_factorial(7) AS factorial;

-- ============================================================================
-- Section 18: Function with FOUND Variable
-- ============================================================================

CREATE FUNCTION test_employee_exists(emp_name VARCHAR) RETURNS BOOLEAN AS $$
DECLARE
    emp_record RECORD;
BEGIN
    SELECT * INTO emp_record FROM test_employees WHERE name = emp_name;

    IF FOUND THEN
        RETURN TRUE;
    ELSE
        RETURN FALSE;
    END IF;
END;
$$ LANGUAGE plpgsql STABLE;

SELECT test_employee_exists('Alice') AS alice_exists;  -- TRUE
SELECT test_employee_exists('Zack') AS zack_exists;  -- FALSE

-- ============================================================================
-- Section 19: Function with Dynamic SQL
-- ============================================================================

CREATE FUNCTION test_table_row_count(table_name TEXT) RETURNS BIGINT AS $$
DECLARE
    row_count BIGINT;
BEGIN
    EXECUTE format('SELECT COUNT(*) FROM %I', table_name) INTO row_count;
    RETURN row_count;
END;
$$ LANGUAGE plpgsql STABLE;

SELECT test_table_row_count('test_employees') AS employee_count;

-- ============================================================================
-- Section 20: Function Volatility Categories
-- ============================================================================

-- IMMUTABLE: Always returns same result for same input (can be optimized)
CREATE FUNCTION test_immutable(x INT) RETURNS INT AS $$
    SELECT x * 2;
$$ LANGUAGE SQL IMMUTABLE;

-- STABLE: Returns same result for same input within a single statement
CREATE FUNCTION test_stable() RETURNS TIMESTAMP AS $$
    SELECT CURRENT_TIMESTAMP;
$$ LANGUAGE SQL STABLE;

-- VOLATILE: Can return different results (default, least optimizable)
CREATE FUNCTION test_volatile() RETURNS TIMESTAMP AS $$
    SELECT clock_timestamp();
$$ LANGUAGE SQL VOLATILE;

SELECT test_immutable(5) AS immutable_result;
SELECT test_stable() AS stable_result;
SELECT test_volatile() AS volatile_result;

-- ============================================================================
-- Section 21: Function with INOUT Parameters
-- ============================================================================

CREATE FUNCTION test_increment(INOUT value INT, increment_by INT DEFAULT 1) AS $$
BEGIN
    value := value + increment_by;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_increment(10) AS result;  -- Should return 11
SELECT test_increment(10, 5) AS result;  -- Should return 15

-- ============================================================================
-- Section 22: Function Overloading
-- ============================================================================

-- Same function name, different parameter types
CREATE FUNCTION test_double(x INT) RETURNS INT AS $$
    SELECT x * 2;
$$ LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION test_double(x NUMERIC) RETURNS NUMERIC AS $$
    SELECT x * 2.0;
$$ LANGUAGE SQL IMMUTABLE;

CREATE FUNCTION test_double(x TEXT) RETURNS TEXT AS $$
    SELECT x || x;
$$ LANGUAGE SQL IMMUTABLE;

SELECT test_double(5) AS int_double;  -- Calls INT version
SELECT test_double(5.5) AS numeric_double;  -- Calls NUMERIC version
SELECT test_double('abc') AS text_double;  -- Calls TEXT version

-- ============================================================================
-- Section 23: Function with RAISE Statements
-- ============================================================================

CREATE FUNCTION test_validate_age(age INT) RETURNS BOOLEAN AS $$
BEGIN
    IF age < 0 THEN
        RAISE EXCEPTION 'Age cannot be negative: %', age;
    ELSIF age < 18 THEN
        RAISE WARNING 'Age is below 18: %', age;
    END IF;

    RAISE NOTICE 'Age validated: %', age;
    RETURN TRUE;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_validate_age(25) AS result;

-- ============================================================================
-- Section 24: Function with PERFORM
-- ============================================================================

CREATE TABLE test_audit_log (
    log_id SERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION test_log_message(msg TEXT) RETURNS VOID AS $$
BEGIN
    -- PERFORM is used to execute a query and discard results
    PERFORM pg_sleep(0.001);  -- Simulate some work

    INSERT INTO test_audit_log (message) VALUES (msg);
END;
$$ LANGUAGE plpgsql VOLATILE;

SELECT test_log_message('Test log entry');

SELECT log_id, message FROM test_audit_log ORDER BY log_id;

-- ============================================================================
-- Section 25: Best Practices
-- ============================================================================

CREATE TABLE test_function_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_function_best_practices VALUES
    (1, 'Use IMMUTABLE for functions with deterministic results'),
    (2, 'Use STABLE for functions that read database but don''t modify'),
    (3, 'Use VOLATILE for functions that modify database or use random'),
    (4, 'Properly handle NULL inputs and edge cases'),
    (5, 'Use appropriate parameter modes: IN, OUT, INOUT, VARIADIC'),
    (6, 'Add EXCEPTION handling for robust error management'),
    (7, 'Use RETURN QUERY for set-returning functions'),
    (8, 'Document function purpose, parameters, and return values'),
    (9, 'Test functions with boundary values and edge cases'),
    (10, 'Use RAISE statements for debugging and logging'),
    (11, 'Validate input parameters at function start'),
    (12, 'Consider security with SECURITY DEFINER vs SECURITY INVOKER'),
    (13, 'Keep functions focused and single-purpose'),
    (14, 'Use function overloading when appropriate'),
    (15, 'Monitor function performance and optimize as needed');

SELECT id, guideline FROM test_function_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_function_best_practices;
DROP TABLE test_audit_log;
DROP FUNCTION test_log_message(TEXT);
DROP FUNCTION test_validate_age(INT);
DROP FUNCTION test_double(TEXT);
DROP FUNCTION test_double(NUMERIC);
DROP FUNCTION test_double(INT);
DROP FUNCTION test_increment(INT, INT);
DROP FUNCTION test_volatile();
DROP FUNCTION test_stable();
DROP FUNCTION test_immutable(INT);
DROP FUNCTION test_table_row_count(TEXT);
DROP FUNCTION test_employee_exists(VARCHAR);
DROP FUNCTION test_recursive_factorial(INT);
DROP FUNCTION test_get_high_earners(NUMERIC);
DROP FUNCTION test_generate_series_custom(INT, INT);
DROP FUNCTION test_safe_divide(NUMERIC, NUMERIC);
DROP FUNCTION test_sum_all(INT[]);
DROP FUNCTION test_greeting(VARCHAR, VARCHAR);
DROP FUNCTION test_employee_stats(VARCHAR);
DROP FUNCTION test_min_max(INT, INT, INT);
DROP FUNCTION test_get_avg_salary(VARCHAR);
DROP TABLE test_employees;
DROP FUNCTION test_power(INT, INT);
DROP FUNCTION test_fibonacci(INT);
DROP FUNCTION test_sum_to_n(INT);
DROP FUNCTION test_is_even(INT);
DROP FUNCTION test_grade_letter(INT);
DROP FUNCTION test_factorial(INT);
DROP FUNCTION test_calculate_total(INT, NUMERIC);
DROP FUNCTION test_add_numbers(INT, INT);

DROP DATABASE test_basic_functions_db;

-- End of basic function tests
