-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Advanced Function Features
-- Description: Comprehensive advanced function feature testing
-- ============================================================================

-- Advanced features: Table functions, polymorphic functions,
-- security contexts, search paths, function dependencies,
-- composite types, domain types, performance optimization

-- Create test database
CREATE DATABASE test_advanced_functions_db;
USE test_advanced_functions_db;

-- ============================================================================
-- Section 1: Table-Returning Functions (SETOF RECORD)
-- ============================================================================

CREATE FUNCTION test_generate_numbers(start_val INT, end_val INT)
RETURNS TABLE(num INT, num_squared INT, num_cubed INT) AS $$
DECLARE
    i INT;
BEGIN
    FOR i IN start_val..end_val LOOP
        num := i;
        num_squared := i * i;
        num_cubed := i * i * i;
        RETURN NEXT;
    END LOOP;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_generate_numbers(1, 5);

-- ============================================================================
-- Section 2: Composite Type Functions
-- ============================================================================

CREATE TYPE test_employee_type AS (
    name VARCHAR(200),
    salary NUMERIC(12,2),
    department VARCHAR(100)
);

CREATE FUNCTION test_create_employee(
    emp_name VARCHAR,
    emp_salary NUMERIC,
    emp_dept VARCHAR
) RETURNS test_employee_type AS $$
DECLARE
    result test_employee_type;
BEGIN
    result.name := emp_name;
    result.salary := emp_salary;
    result.department := emp_dept;
    RETURN result;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_create_employee('Alice', 75000, 'Engineering');

SELECT (test_create_employee('Bob', 65000, 'Sales')).name AS employee_name;

-- ============================================================================
-- Section 3: Function Returning Array
-- ============================================================================

CREATE FUNCTION test_fibonacci_array(n INT) RETURNS BIGINT[] AS $$
DECLARE
    result BIGINT[] := ARRAY[0, 1];
    i INT;
BEGIN
    IF n <= 0 THEN
        RETURN ARRAY[]::BIGINT[];
    ELSIF n = 1 THEN
        RETURN ARRAY[0]::BIGINT[];
    END IF;

    FOR i IN 3..n LOOP
        result := result || (result[i-1] + result[i-2]);
    END LOOP;

    RETURN result;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_fibonacci_array(10) AS fibonacci_sequence;

-- ============================================================================
-- Section 4: Polymorphic Functions (ANYELEMENT)
-- ============================================================================

CREATE FUNCTION test_first_element(arr ANYARRAY) RETURNS ANYELEMENT AS $$
BEGIN
    RETURN arr[1];
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_first_element(ARRAY[1, 2, 3]) AS first_int;
SELECT test_first_element(ARRAY['a', 'b', 'c']) AS first_text;
SELECT test_first_element(ARRAY[1.5, 2.5, 3.5]) AS first_numeric;

-- ============================================================================
-- Section 5: Polymorphic Functions (ANYCOMPATIBLE)
-- ============================================================================

CREATE FUNCTION test_swap(
    INOUT a ANYCOMPATIBLE,
    INOUT b ANYCOMPATIBLE
) AS $$
DECLARE
    temp ALIAS FOR $1;
BEGIN
    a := b;
    b := temp;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_swap(10, 20);
SELECT * FROM test_swap('hello', 'world');

-- ============================================================================
-- Section 6: SECURITY DEFINER vs SECURITY INVOKER
-- ============================================================================

CREATE TABLE test_sensitive_data (
    data_id SERIAL PRIMARY KEY,
    secret_value TEXT
);

INSERT INTO test_sensitive_data (secret_value) VALUES ('Secret 1'), ('Secret 2');

-- SECURITY DEFINER: Runs with privileges of function creator
CREATE FUNCTION test_get_secrets_definer() RETURNS SETOF test_sensitive_data
SECURITY DEFINER
AS $$
BEGIN
    RETURN QUERY SELECT * FROM test_sensitive_data;
END;
$$ LANGUAGE plpgsql STABLE;

-- SECURITY INVOKER: Runs with privileges of function caller (default)
CREATE FUNCTION test_get_secrets_invoker() RETURNS SETOF test_sensitive_data
SECURITY INVOKER
AS $$
BEGIN
    RETURN QUERY SELECT * FROM test_sensitive_data;
END;
$$ LANGUAGE plpgsql STABLE;

SELECT * FROM test_get_secrets_definer();

-- ============================================================================
-- Section 7: Function with SET Parameters
-- ============================================================================

CREATE FUNCTION test_with_search_path() RETURNS TEXT
SET search_path TO public
AS $$
BEGIN
    RETURN current_setting('search_path');
END;
$$ LANGUAGE plpgsql STABLE;

SELECT test_with_search_path() AS search_path;

-- ============================================================================
-- Section 8: Function with STRICT (RETURNS NULL ON NULL INPUT)
-- ============================================================================

CREATE FUNCTION test_add_strict(a INT, b INT) RETURNS INT
STRICT
AS $$
BEGIN
    RETURN a + b;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_add_strict(5, 3) AS result;
SELECT test_add_strict(5, NULL) AS result;  -- Returns NULL without executing function

-- ============================================================================
-- Section 9: Function with PARALLEL SAFE/UNSAFE
-- ============================================================================

CREATE FUNCTION test_parallel_safe(x INT) RETURNS INT
PARALLEL SAFE
AS $$
BEGIN
    RETURN x * 2;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

CREATE FUNCTION test_parallel_unsafe() RETURNS INT
PARALLEL UNSAFE
AS $$
BEGIN
    -- Functions that use temp tables or cursors are parallel unsafe
    RETURN 42;
END;
$$ LANGUAGE plpgsql VOLATILE;

SELECT test_parallel_safe(10) AS result;

-- ============================================================================
-- Section 10: Function with Complex Return Types
-- ============================================================================

CREATE TYPE test_stats_type AS (
    mean NUMERIC,
    median NUMERIC,
    stddev NUMERIC,
    min_val NUMERIC,
    max_val NUMERIC
);

CREATE FUNCTION test_calculate_statistics(values NUMERIC[]) RETURNS test_stats_type AS $$
DECLARE
    result test_stats_type;
    sorted_values NUMERIC[];
    n INT;
BEGIN
    n := array_length(values, 1);

    IF n = 0 OR n IS NULL THEN
        RETURN NULL;
    END IF;

    -- Calculate mean
    result.mean := (SELECT AVG(v) FROM unnest(values) AS v);

    -- Calculate median
    sorted_values := ARRAY(SELECT v FROM unnest(values) AS v ORDER BY v);
    IF n % 2 = 1 THEN
        result.median := sorted_values[(n + 1) / 2];
    ELSE
        result.median := (sorted_values[n / 2] + sorted_values[n / 2 + 1]) / 2.0;
    END IF;

    -- Calculate stddev, min, max
    result.stddev := (SELECT STDDEV(v) FROM unnest(values) AS v);
    result.min_val := (SELECT MIN(v) FROM unnest(values) AS v);
    result.max_val := (SELECT MAX(v) FROM unnest(values) AS v);

    RETURN result;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_calculate_statistics(ARRAY[10, 20, 30, 40, 50]::NUMERIC[]);

-- ============================================================================
-- Section 11: Function with Domain Types
-- ============================================================================

CREATE DOMAIN test_email AS TEXT
CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$');

CREATE FUNCTION test_validate_email(email_addr TEXT) RETURNS test_email AS $$
BEGIN
    -- Domain constraint will automatically validate
    RETURN email_addr::test_email;
EXCEPTION
    WHEN check_violation THEN
        RAISE EXCEPTION 'Invalid email format: %', email_addr;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_validate_email('user@example.com') AS valid_email;

-- Invalid email (would fail)
-- SELECT test_validate_email('invalid-email') AS invalid_email;

-- ============================================================================
-- Section 12: Function with Nested Composite Types
-- ============================================================================

CREATE TYPE test_address_type AS (
    street VARCHAR(200),
    city VARCHAR(100),
    state VARCHAR(50),
    zip VARCHAR(20)
);

CREATE TYPE test_person_type AS (
    name VARCHAR(200),
    age INT,
    address test_address_type
);

CREATE FUNCTION test_create_person(
    p_name VARCHAR,
    p_age INT,
    p_street VARCHAR,
    p_city VARCHAR,
    p_state VARCHAR,
    p_zip VARCHAR
) RETURNS test_person_type AS $$
DECLARE
    result test_person_type;
BEGIN
    result.name := p_name;
    result.age := p_age;
    result.address.street := p_street;
    result.address.city := p_city;
    result.address.state := p_state;
    result.address.zip := p_zip;

    RETURN result;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_create_person('John Doe', 30, '123 Main St', 'Springfield', 'IL', '62701');

-- ============================================================================
-- Section 13: Function Metadata and Dependencies
-- ============================================================================

-- Query function metadata
SELECT
    proname AS function_name,
    pg_get_function_arguments(oid) AS arguments,
    pg_get_function_result(oid) AS return_type,
    prosrc AS source_code
FROM pg_proc
WHERE proname LIKE 'test_add_%'
ORDER BY proname;

-- ============================================================================
-- Section 14: Function with COST and ROWS Hints
-- ============================================================================

CREATE FUNCTION test_expensive_operation(n INT) RETURNS INT
COST 1000  -- Hint that this function is expensive
AS $$
DECLARE
    result INT := 0;
    i INT;
BEGIN
    FOR i IN 1..n LOOP
        result := result + i;
        PERFORM pg_sleep(0.0001);  -- Simulate expensive work
    END LOOP;
    RETURN result;
END;
$$ LANGUAGE plpgsql VOLATILE;

CREATE FUNCTION test_many_rows(n INT) RETURNS SETOF INT
ROWS 1000  -- Hint that this function returns many rows
AS $$
BEGIN
    RETURN QUERY SELECT generate_series(1, n);
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_expensive_operation(10) AS result;
SELECT COUNT(*) FROM test_many_rows(100);

-- ============================================================================
-- Section 15: Function with Caching/Memoization
-- ============================================================================

CREATE TABLE test_function_cache (
    input_value INT PRIMARY KEY,
    output_value BIGINT,
    computed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION test_cached_factorial(n INT) RETURNS BIGINT AS $$
DECLARE
    cached_result BIGINT;
    result BIGINT := 1;
    i INT;
BEGIN
    -- Check cache
    SELECT output_value INTO cached_result
    FROM test_function_cache
    WHERE input_value = n;

    IF FOUND THEN
        RETURN cached_result;
    END IF;

    -- Compute if not cached
    FOR i IN 1..n LOOP
        result := result * i;
    END LOOP;

    -- Store in cache
    INSERT INTO test_function_cache (input_value, output_value)
    VALUES (n, result)
    ON CONFLICT (input_value) DO NOTHING;

    RETURN result;
END;
$$ LANGUAGE plpgsql VOLATILE;

SELECT test_cached_factorial(10) AS factorial_10;
SELECT test_cached_factorial(10) AS factorial_10_cached;  -- Uses cache

SELECT * FROM test_function_cache ORDER BY input_value;

-- ============================================================================
-- Section 16: Window Function Definition
-- ============================================================================

-- Custom aggregate for use as window function
CREATE AGGREGATE test_custom_sum(NUMERIC) (
    SFUNC = numeric_add,
    STYPE = NUMERIC,
    INITCOND = '0'
);

CREATE TABLE test_values (
    id SERIAL PRIMARY KEY,
    value NUMERIC
);

INSERT INTO test_values (value) VALUES (10), (20), (30), (40), (50);

SELECT
    id,
    value,
    test_custom_sum(value) OVER (ORDER BY id) AS running_sum
FROM test_values
ORDER BY id;

-- ============================================================================
-- Section 17: Function with Text Search
-- ============================================================================

CREATE FUNCTION test_full_text_search(search_term TEXT)
RETURNS TABLE(doc_id INT, content TEXT, rank REAL) AS $$
BEGIN
    RETURN QUERY
    SELECT
        1 AS doc_id,
        'Sample document with searchable text'::TEXT AS content,
        ts_rank(
            to_tsvector('english', 'Sample document with searchable text'),
            plainto_tsquery('english', search_term)
        ) AS rank
    WHERE to_tsvector('english', 'Sample document with searchable text') @@
          plainto_tsquery('english', search_term);
END;
$$ LANGUAGE plpgsql STABLE;

SELECT * FROM test_full_text_search('document');

-- ============================================================================
-- Section 18: Function with JSON/JSONB Processing
-- ============================================================================

CREATE FUNCTION test_extract_json_fields(data JSONB)
RETURNS TABLE(key TEXT, value TEXT) AS $$
BEGIN
    RETURN QUERY
    SELECT
        jsonb_object_keys(data) AS key,
        (data ->> jsonb_object_keys(data))::TEXT AS value;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT * FROM test_extract_json_fields('{"name": "John", "age": "30", "city": "NYC"}'::JSONB);

-- ============================================================================
-- Section 19: Function with Regular Expressions
-- ============================================================================

CREATE FUNCTION test_extract_emails(text_content TEXT)
RETURNS TEXT[] AS $$
DECLARE
    email_pattern TEXT := '[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}';
    matches TEXT[];
BEGIN
    SELECT ARRAY_AGG(match[1])
    INTO matches
    FROM regexp_matches(text_content, email_pattern, 'g') AS match;

    RETURN COALESCE(matches, ARRAY[]::TEXT[]);
END;
$$ LANGUAGE plpgsql IMMUTABLE;

SELECT test_extract_emails('Contact us at support@example.com or sales@example.org') AS emails;

-- ============================================================================
-- Section 20: Function with Transaction ID Tracking
-- ============================================================================

CREATE TABLE test_operation_log (
    log_id SERIAL PRIMARY KEY,
    operation TEXT,
    transaction_id BIGINT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION test_log_operation(op TEXT) RETURNS VOID AS $$
BEGIN
    INSERT INTO test_operation_log (operation, transaction_id)
    VALUES (op, txid_current());
END;
$$ LANGUAGE plpgsql VOLATILE;

SELECT test_log_operation('Operation 1');
SELECT test_log_operation('Operation 2');

SELECT log_id, operation, transaction_id FROM test_operation_log ORDER BY log_id;

-- ============================================================================
-- Section 21: Function Dependency Management
-- ============================================================================

CREATE TABLE test_base_table (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE FUNCTION test_dependent_function() RETURNS SETOF test_base_table AS $$
BEGIN
    RETURN QUERY SELECT * FROM test_base_table;
END;
$$ LANGUAGE plpgsql STABLE;

-- Cannot drop test_base_table without dropping function first
-- DROP TABLE test_base_table;  -- Would fail

-- Must drop function first
-- DROP FUNCTION test_dependent_function();
-- DROP TABLE test_base_table;

-- ============================================================================
-- Section 22: Function with LEAKPROOF Attribute
-- ============================================================================

CREATE FUNCTION test_leakproof_check(x INT) RETURNS BOOLEAN
LEAKPROOF
AS $$
BEGIN
    RETURN x > 0;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- LEAKPROOF functions don't leak information about arguments
-- Safe to push down into security barrier views

SELECT test_leakproof_check(5) AS result;

-- ============================================================================
-- Section 23: Function with SUPPORT Function
-- ============================================================================

-- Create a support function for optimization hints
CREATE FUNCTION test_support_func(internal) RETURNS internal AS $$
BEGIN
    -- Support functions help the planner optimize queries
    -- This is a placeholder implementation
    RETURN $1;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- ============================================================================
-- Section 24: Function Performance Testing
-- ============================================================================

CREATE FUNCTION test_benchmark_function(iterations INT) RETURNS INTERVAL AS $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
    i INT;
    dummy INT;
BEGIN
    start_time := clock_timestamp();

    FOR i IN 1..iterations LOOP
        dummy := i * i;
    END LOOP;

    end_time := clock_timestamp();

    RETURN end_time - start_time;
END;
$$ LANGUAGE plpgsql VOLATILE;

SELECT test_benchmark_function(10000) AS execution_time;

-- ============================================================================
-- Section 25: Best Practices
-- ============================================================================

CREATE TABLE test_advanced_function_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_advanced_function_best_practices VALUES
    (1, 'Use SECURITY DEFINER carefully - it runs with creator privileges'),
    (2, 'Mark functions PARALLEL SAFE when possible for better performance'),
    (3, 'Use STRICT to avoid unnecessary execution with NULL inputs'),
    (4, 'Set appropriate COST and ROWS hints for query planning'),
    (5, 'Use composite types for complex data structures'),
    (6, 'Leverage polymorphic functions for type flexibility'),
    (7, 'Consider function dependencies when designing schema'),
    (8, 'Use LEAKPROOF for security-sensitive operations'),
    (9, 'Cache expensive computations when appropriate'),
    (10, 'Document security context (DEFINER vs INVOKER)'),
    (11, 'Test functions with various input types and edge cases'),
    (12, 'Monitor function performance in production'),
    (13, 'Use SET parameters to control function environment'),
    (14, 'Consider using domains for input validation'),
    (15, 'Benchmark critical functions under realistic load');

SELECT id, guideline FROM test_advanced_function_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_advanced_function_best_practices;
DROP FUNCTION test_benchmark_function(INT);
DROP FUNCTION test_support_func(internal);
DROP FUNCTION test_leakproof_check(INT);
DROP FUNCTION test_dependent_function();
DROP TABLE test_base_table;
DROP TABLE test_operation_log;
DROP FUNCTION test_log_operation(TEXT);
DROP FUNCTION test_extract_emails(TEXT);
DROP FUNCTION test_extract_json_fields(JSONB);
DROP FUNCTION test_full_text_search(TEXT);
DROP TABLE test_values;
DROP AGGREGATE test_custom_sum(NUMERIC);
DROP TABLE test_function_cache;
DROP FUNCTION test_cached_factorial(INT);
DROP FUNCTION test_many_rows(INT);
DROP FUNCTION test_expensive_operation(INT);
DROP FUNCTION test_create_person(VARCHAR, INT, VARCHAR, VARCHAR, VARCHAR, VARCHAR);
DROP TYPE test_person_type;
DROP TYPE test_address_type;
DROP FUNCTION test_validate_email(TEXT);
DROP DOMAIN test_email;
DROP FUNCTION test_calculate_statistics(NUMERIC[]);
DROP TYPE test_stats_type;
DROP FUNCTION test_parallel_unsafe();
DROP FUNCTION test_parallel_safe(INT);
DROP FUNCTION test_add_strict(INT, INT);
DROP FUNCTION test_with_search_path();
DROP FUNCTION test_get_secrets_invoker();
DROP FUNCTION test_get_secrets_definer();
DROP TABLE test_sensitive_data;
DROP FUNCTION test_swap(ANYCOMPATIBLE, ANYCOMPATIBLE);
DROP FUNCTION test_first_element(ANYARRAY);
DROP FUNCTION test_fibonacci_array(INT);
DROP FUNCTION test_create_employee(VARCHAR, NUMERIC, VARCHAR);
DROP TYPE test_employee_type;
DROP FUNCTION test_generate_numbers(INT, INT);

DROP DATABASE test_advanced_functions_db;

-- End of advanced function tests
