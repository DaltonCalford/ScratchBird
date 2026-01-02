-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: DML - Expressions and Functions
-- Description: Comprehensive expression and function testing in DML
-- ============================================================================

-- Expressions and Functions: SQL functions in queries
-- String, numeric, date/time functions
-- Conditional expressions, type conversions
-- JSON, array, aggregate functions

-- Create test database
CREATE DATABASE test_expressions_functions_db;
USE test_expressions_functions_db;

-- ============================================================================
-- Section 1: String Functions
-- ============================================================================

CREATE TABLE test_string_data (
    id SERIAL PRIMARY KEY,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    email VARCHAR(100),
    notes TEXT
);

INSERT INTO test_string_data VALUES
    (1, 'Alice', 'Smith', 'alice.smith@example.com', 'Important customer'),
    (2, 'Bob', 'Jones', 'BOB.JONES@EXAMPLE.COM', 'Regular customer'),
    (3, 'Charlie', 'Brown', 'charlie@test.org', NULL);

-- String concatenation
SELECT
    id,
    first_name || ' ' || last_name AS full_name,
    CONCAT(first_name, ' ', last_name) AS full_name_concat,
    CONCAT_WS(', ', last_name, first_name) AS name_last_first
FROM test_string_data
ORDER BY id;

-- Case conversion
SELECT
    email,
    UPPER(email) AS upper_email,
    LOWER(email) AS lower_email,
    INITCAP(first_name || ' ' || last_name) AS proper_name
FROM test_string_data
ORDER BY id;

-- String extraction and manipulation
SELECT
    email,
    LENGTH(email) AS email_length,
    POSITION('@' IN email) AS at_position,
    SUBSTRING(email FROM 1 FOR POSITION('@' IN email) - 1) AS username,
    SUBSTRING(email FROM POSITION('@' IN email) + 1) AS domain,
    SPLIT_PART(email, '@', 1) AS username_split,
    SPLIT_PART(email, '@', 2) AS domain_split
FROM test_string_data
ORDER BY id;

-- String padding and trimming
SELECT
    id,
    LPAD(id::TEXT, 5, '0') AS padded_id,
    RPAD(first_name, 15, '.') AS padded_name,
    TRIM(BOTH ' ' FROM '  ' || first_name || '  ') AS trimmed,
    LTRIM('  left spaces  ') AS left_trimmed,
    RTRIM('  right spaces  ') AS right_trimmed
FROM test_string_data
ORDER BY id;

-- ============================================================================
-- Section 2: Pattern Matching (LIKE, ILIKE, Regex)
-- ============================================================================

-- LIKE pattern matching (case-sensitive)
SELECT id, email
FROM test_string_data
WHERE email LIKE '%example.com'
ORDER BY id;

-- ILIKE pattern matching (case-insensitive, PostgreSQL)
SELECT id, email
FROM test_string_data
WHERE email ILIKE '%EXAMPLE%'
ORDER BY id;

-- Regular expressions
SELECT
    id,
    email,
    email ~ '^[a-z]' AS starts_with_lowercase,
    email ~* '^[A-Z]' AS starts_with_letter_caseinsensitive,
    REGEXP_REPLACE(email, '@.*$', '@domain.com') AS replaced_domain,
    REGEXP_MATCH(email, '^([^@]+)@(.+)$') AS email_parts
FROM test_string_data
ORDER BY id;

-- ============================================================================
-- Section 3: Numeric Functions and Operators
-- ============================================================================

CREATE TABLE test_numeric_data (
    id SERIAL PRIMARY KEY,
    quantity INT,
    unit_price NUMERIC(10,2),
    discount_pct NUMERIC(5,2),
    weight FLOAT
);

INSERT INTO test_numeric_data VALUES
    (1, 10, 25.50, 10.00, 5.75),
    (2, 20, 15.75, 5.00, 3.25),
    (3, 5, 100.00, 15.00, 10.50);

-- Arithmetic operations
SELECT
    id,
    quantity,
    unit_price,
    quantity * unit_price AS subtotal,
    discount_pct / 100 AS discount_decimal,
    quantity * unit_price * (1 - discount_pct / 100) AS total,
    ROUND(quantity * unit_price * (1 - discount_pct / 100), 2) AS total_rounded
FROM test_numeric_data
ORDER BY id;

-- Mathematical functions
SELECT
    id,
    weight,
    ABS(weight - 5.0) AS diff_from_5,
    CEIL(weight) AS ceiling,
    FLOOR(weight) AS floor,
    ROUND(weight, 1) AS rounded_1_decimal,
    TRUNC(weight, 1) AS truncated,
    MOD(quantity, 3) AS modulo_3,
    POWER(quantity, 2) AS quantity_squared,
    SQRT(quantity) AS quantity_sqrt
FROM test_numeric_data
ORDER BY id;

-- Aggregate numeric functions
SELECT
    COUNT(*) AS record_count,
    SUM(quantity) AS total_quantity,
    AVG(unit_price) AS avg_price,
    MIN(unit_price) AS min_price,
    MAX(unit_price) AS max_price,
    STDDEV(unit_price) AS price_stddev,
    VARIANCE(unit_price) AS price_variance
FROM test_numeric_data;

-- ============================================================================
-- Section 4: Date and Time Functions
-- ============================================================================

CREATE TABLE test_datetime_data (
    id SERIAL PRIMARY KEY,
    event_name VARCHAR(100),
    event_date DATE,
    event_timestamp TIMESTAMP,
    event_time TIME
);

INSERT INTO test_datetime_data VALUES
    (1, 'Event A', '2024-01-15', '2024-01-15 14:30:00', '14:30:00'),
    (2, 'Event B', '2024-02-20', '2024-02-20 09:15:30', '09:15:30'),
    (3, 'Event C', '2024-03-10', '2024-03-10 18:45:15', '18:45:15');

-- Current date/time functions
SELECT
    CURRENT_DATE AS today,
    CURRENT_TIME AS now_time,
    CURRENT_TIMESTAMP AS now_timestamp,
    NOW() AS now_func,
    LOCALTIMESTAMP AS local_timestamp;

-- Date extraction
SELECT
    event_date,
    EXTRACT(YEAR FROM event_date) AS year,
    EXTRACT(MONTH FROM event_date) AS month,
    EXTRACT(DAY FROM event_date) AS day,
    EXTRACT(DOW FROM event_date) AS day_of_week,
    EXTRACT(DOY FROM event_date) AS day_of_year,
    EXTRACT(QUARTER FROM event_date) AS quarter
FROM test_datetime_data
ORDER BY id;

-- Date arithmetic
SELECT
    event_date,
    event_date + INTERVAL '7 days' AS one_week_later,
    event_date - INTERVAL '1 month' AS one_month_ago,
    event_date + INTERVAL '1 year' AS next_year,
    AGE(CURRENT_DATE, event_date) AS age_from_now,
    event_timestamp + INTERVAL '2 hours 30 minutes' AS later_time
FROM test_datetime_data
ORDER BY id;

-- Date truncation and formatting
SELECT
    event_timestamp,
    DATE_TRUNC('year', event_timestamp) AS trunc_year,
    DATE_TRUNC('month', event_timestamp) AS trunc_month,
    DATE_TRUNC('day', event_timestamp) AS trunc_day,
    DATE_TRUNC('hour', event_timestamp) AS trunc_hour,
    TO_CHAR(event_timestamp, 'YYYY-MM-DD HH24:MI:SS') AS formatted_full,
    TO_CHAR(event_date, 'Mon DD, YYYY') AS formatted_date,
    TO_CHAR(event_date, 'Day, FMMonth DD, YYYY') AS formatted_long
FROM test_datetime_data
ORDER BY id;

-- ============================================================================
-- Section 5: CASE Expressions
-- ============================================================================

-- Simple CASE
SELECT
    id,
    quantity,
    CASE quantity
        WHEN 5 THEN 'Small order'
        WHEN 10 THEN 'Medium order'
        WHEN 20 THEN 'Large order'
        ELSE 'Other'
    END AS order_size
FROM test_numeric_data
ORDER BY id;

-- Searched CASE
SELECT
    id,
    quantity,
    unit_price,
    quantity * unit_price AS total,
    CASE
        WHEN quantity * unit_price > 300 THEN 'High value'
        WHEN quantity * unit_price > 150 THEN 'Medium value'
        ELSE 'Low value'
    END AS value_category
FROM test_numeric_data
ORDER BY id;

-- Nested CASE
SELECT
    id,
    quantity,
    discount_pct,
    CASE
        WHEN quantity >= 20 THEN
            CASE
                WHEN discount_pct >= 10 THEN 'Bulk with high discount'
                ELSE 'Bulk with normal discount'
            END
        WHEN quantity >= 10 THEN 'Medium quantity'
        ELSE 'Small quantity'
    END AS sales_category
FROM test_numeric_data
ORDER BY id;

-- ============================================================================
-- Section 6: COALESCE and NULLIF
-- ============================================================================

-- COALESCE - return first non-NULL
SELECT
    id,
    notes,
    COALESCE(notes, 'No notes available') AS notes_with_default,
    COALESCE(NULL, NULL, 'fallback value') AS multi_coalesce
FROM test_string_data
ORDER BY id;

-- NULLIF - return NULL if values equal
SELECT
    id,
    discount_pct,
    NULLIF(discount_pct, 0) AS discount_or_null,
    100.0 / NULLIF(discount_pct, 0) AS safe_division
FROM test_numeric_data
ORDER BY id;

-- ============================================================================
-- Section 7: Type Conversion and Casting
-- ============================================================================

-- Explicit casting
SELECT
    id,
    id::TEXT AS id_as_text,
    quantity::NUMERIC AS quantity_as_numeric,
    unit_price::INT AS price_as_int,
    CAST(quantity AS TEXT) AS quantity_text,
    CAST(unit_price AS FLOAT) AS price_as_float
FROM test_numeric_data
ORDER BY id;

-- String to number conversion
SELECT
    '123'::INT AS str_to_int,
    '45.67'::NUMERIC AS str_to_numeric,
    '123.45'::FLOAT AS str_to_float;

-- Date/time conversions
SELECT
    '2024-01-15'::DATE AS str_to_date,
    '2024-01-15 14:30:00'::TIMESTAMP AS str_to_timestamp,
    TO_DATE('15/01/2024', 'DD/MM/YYYY') AS formatted_str_to_date,
    TO_TIMESTAMP('2024-01-15 14:30:00', 'YYYY-MM-DD HH24:MI:SS') AS str_to_ts;

-- ============================================================================
-- Section 8: JSON/JSONB Functions
-- ============================================================================

CREATE TABLE test_json_data (
    id SERIAL PRIMARY KEY,
    data JSONB
);

INSERT INTO test_json_data VALUES
    (1, '{"name": "Alice", "age": 30, "city": "New York", "hobbies": ["reading", "hiking"]}'::JSONB),
    (2, '{"name": "Bob", "age": 25, "city": "Los Angeles", "hobbies": ["gaming", "coding"]}'::JSONB),
    (3, '{"name": "Charlie", "age": 35, "city": "Chicago"}'::JSONB);

-- JSON extraction
SELECT
    id,
    data->>'name' AS name,
    data->>'city' AS city,
    data->'age' AS age_json,
    (data->>'age')::INT AS age_int,
    data->'hobbies' AS hobbies,
    data->'hobbies'->0 AS first_hobby
FROM test_json_data
ORDER BY id;

-- JSON operators and functions
SELECT
    id,
    data ? 'hobbies' AS has_hobbies,
    data @> '{"city": "New York"}'::JSONB AS is_from_ny,
    jsonb_typeof(data->'age') AS age_type,
    jsonb_array_length(data->'hobbies') AS hobby_count
FROM test_json_data
ORDER BY id;

-- JSON modification
SELECT
    id,
    jsonb_set(data, '{age}', '31'::JSONB) AS updated_age,
    data || '{"country": "USA"}'::JSONB AS with_country,
    data - 'city' AS without_city
FROM test_json_data
ORDER BY id;

-- ============================================================================
-- Section 9: Array Functions
-- ============================================================================

CREATE TABLE test_array_data (
    id SERIAL PRIMARY KEY,
    tags TEXT[],
    scores INT[]
);

INSERT INTO test_array_data VALUES
    (1, ARRAY['tag1', 'tag2', 'tag3'], ARRAY[10, 20, 30]),
    (2, ARRAY['tag2', 'tag4'], ARRAY[15, 25]),
    (3, ARRAY['tag1', 'tag3', 'tag5'], ARRAY[5, 15, 25, 35]);

-- Array operators
SELECT
    id,
    tags,
    array_length(tags, 1) AS tag_count,
    tags[1] AS first_tag,
    tags[2:3] AS second_and_third_tags,
    'tag2' = ANY(tags) AS has_tag2,
    tags @> ARRAY['tag1'] AS contains_tag1,
    tags && ARRAY['tag2', 'tag3'] AS overlaps_tags
FROM test_array_data
ORDER BY id;

-- Array functions
SELECT
    id,
    tags,
    array_append(tags, 'new_tag') AS with_appended,
    array_prepend('first_tag', tags) AS with_prepended,
    array_cat(tags, ARRAY['extra1', 'extra2']) AS concatenated,
    array_remove(tags, 'tag2') AS without_tag2,
    array_position(tags, 'tag3') AS position_of_tag3
FROM test_array_data
ORDER BY id;

-- Unnest arrays
SELECT
    id,
    unnest(tags) AS individual_tag
FROM test_array_data
ORDER BY id, individual_tag;

-- ============================================================================
-- Section 10: Conditional Aggregates (FILTER)
-- ============================================================================

CREATE TABLE test_sales_data (
    id SERIAL PRIMARY KEY,
    region VARCHAR(50),
    product_category VARCHAR(50),
    sale_amount NUMERIC(10,2),
    sale_date DATE
);

INSERT INTO test_sales_data VALUES
    (1, 'North', 'Electronics', 1000, '2024-01-15'),
    (2, 'North', 'Clothing', 500, '2024-01-20'),
    (3, 'South', 'Electronics', 1500, '2024-01-18'),
    (4, 'South', 'Clothing', 700, '2024-02-05'),
    (5, 'North', 'Electronics', 1200, '2024-02-10');

-- FILTER clause in aggregates
SELECT
    region,
    COUNT(*) AS total_sales,
    COUNT(*) FILTER (WHERE product_category = 'Electronics') AS electronics_count,
    SUM(sale_amount) AS total_amount,
    SUM(sale_amount) FILTER (WHERE product_category = 'Electronics') AS electronics_total,
    AVG(sale_amount) FILTER (WHERE product_category = 'Clothing') AS clothing_avg
FROM test_sales_data
GROUP BY region
ORDER BY region;

-- ============================================================================
-- Section 11: String Aggregation
-- ============================================================================

-- STRING_AGG for concatenation
SELECT
    region,
    STRING_AGG(product_category, ', ' ORDER BY product_category) AS categories,
    STRING_AGG(DISTINCT product_category, ', ' ORDER BY product_category) AS unique_categories
FROM test_sales_data
GROUP BY region
ORDER BY region;

-- ARRAY_AGG for array creation
SELECT
    region,
    ARRAY_AGG(sale_amount ORDER BY sale_date) AS sale_amounts,
    ARRAY_AGG(DISTINCT product_category) AS categories
FROM test_sales_data
GROUP BY region
ORDER BY region;

-- ============================================================================
-- Section 12: Comparison Operators and Expressions
-- ============================================================================

-- Standard comparisons
SELECT
    id,
    quantity,
    quantity = 10 AS equals_10,
    quantity <> 10 AS not_equals_10,
    quantity > 10 AS greater_than_10,
    quantity <= 10 AS lte_10,
    quantity BETWEEN 5 AND 15 AS in_range
FROM test_numeric_data
ORDER BY id;

-- NULL comparisons
SELECT
    id,
    notes,
    notes IS NULL AS is_null,
    notes IS NOT NULL AS is_not_null,
    COALESCE(notes, 'default') = 'default' AS is_default
FROM test_string_data
ORDER BY id;

-- ============================================================================
-- Section 13: Boolean Logic and Operators
-- ============================================================================

-- Boolean operators
SELECT
    id,
    quantity,
    discount_pct,
    (quantity > 10 AND discount_pct > 5) AS high_qty_and_discount,
    (quantity > 15 OR discount_pct > 10) AS high_qty_or_discount,
    NOT (discount_pct = 0) AS has_discount
FROM test_numeric_data
ORDER BY id;

-- Three-valued logic with NULLs
SELECT
    id,
    notes,
    notes = 'Important customer' AS exact_match,
    notes LIKE '%customer%' AS contains_customer,
    (notes IS NULL) OR (notes LIKE '%customer%') AS null_or_match
FROM test_string_data
ORDER BY id;

-- ============================================================================
-- Section 14: Subquery Expressions
-- ============================================================================

-- Scalar subquery in SELECT
SELECT
    id,
    sale_amount,
    (SELECT AVG(sale_amount) FROM test_sales_data) AS overall_avg,
    sale_amount - (SELECT AVG(sale_amount) FROM test_sales_data) AS diff_from_avg
FROM test_sales_data
ORDER BY id;

-- Existence checks
SELECT
    id,
    region,
    EXISTS (
        SELECT 1
        FROM test_sales_data s2
        WHERE s2.region = s1.region
          AND s2.id <> s1.id
    ) AS has_other_sales_in_region
FROM test_sales_data s1
ORDER BY id;

-- ============================================================================
-- Section 15: Row Constructors and Comparisons
-- ============================================================================

-- Row value comparisons
SELECT
    id,
    quantity,
    unit_price,
    (quantity, unit_price) > (10, 20.00) AS row_comparison,
    ROW(quantity, unit_price) = ROW(10, 25.50) AS row_equality
FROM test_numeric_data
ORDER BY id;

-- ============================================================================
-- Section 16: Set-Returning Functions
-- ============================================================================

-- generate_series
SELECT *
FROM generate_series(1, 10) AS n;

-- generate_series with dates
SELECT *
FROM generate_series(
    '2024-01-01'::DATE,
    '2024-01-10'::DATE,
    '1 day'::INTERVAL
) AS date_series;

-- generate_series for test data
SELECT
    i AS id,
    'Item ' || i AS name,
    (random() * 100)::NUMERIC(10,2) AS price
FROM generate_series(1, 5) i;

-- ============================================================================
-- Section 17: Bit String Functions
-- ============================================================================

CREATE TABLE test_permissions (
    user_id INT,
    permissions BIT(8)
);

INSERT INTO test_permissions VALUES
    (1, B'11110000'),
    (2, B'00001111'),
    (3, B'10101010');

-- Bit operations
SELECT
    user_id,
    permissions,
    permissions & B'11000000' AS masked,
    permissions | B'00000011' AS with_bits_set,
    permissions # B'11111111' AS xor_all,
    ~permissions AS complement,
    get_bit(permissions, 0) AS first_bit
FROM test_permissions
ORDER BY user_id;

-- ============================================================================
-- Section 18: System Information Functions
-- ============================================================================

-- Database and session information
SELECT
    current_database() AS database_name,
    current_schema() AS schema_name,
    current_user AS user_name,
    session_user,
    version() AS pg_version;

-- ============================================================================
-- Section 19: Formatting Functions
-- ============================================================================

-- Number formatting
SELECT
    id,
    unit_price,
    TO_CHAR(unit_price, '999.99') AS formatted_price,
    TO_CHAR(unit_price, '$999.99') AS price_with_dollar,
    TO_CHAR(unit_price * 1000, '9,999,999.99') AS formatted_large
FROM test_numeric_data
ORDER BY id;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_expression_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_expression_best_practices VALUES
    (1, 'Use COALESCE for NULL handling with defaults'),
    (2, 'NULLIF prevents division by zero errors'),
    (3, 'Explicit casting (::) clearer than implicit conversion'),
    (4, 'CASE expressions allow complex conditional logic'),
    (5, 'Use appropriate string functions for text manipulation'),
    (6, 'Date arithmetic with INTERVAL for readability'),
    (7, 'EXTRACT for getting date/time components'),
    (8, 'DATE_TRUNC for date bucketing and grouping'),
    (9, 'JSON operators (->>, ->) for efficient extraction'),
    (10, 'Array operators (@>, &&) for containment checks'),
    (11, 'FILTER clause for conditional aggregation'),
    (12, 'STRING_AGG for concatenating grouped values'),
    (13, 'Use BETWEEN for inclusive range checks'),
    (14, 'IS NULL/IS NOT NULL for NULL comparisons'),
    (15, 'Regular expressions for complex pattern matching'),
    (16, 'ROUND/TRUNC for numeric precision control'),
    (17, 'TO_CHAR for custom date/number formatting'),
    (18, 'Avoid functions on indexed columns in WHERE'),
    (19, 'Use EXISTS instead of IN with subqueries'),
    (20, 'Test expressions for NULL behavior');

SELECT id, guideline FROM test_expression_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_expression_best_practices;
DROP TABLE test_permissions;
DROP TABLE test_sales_data;
DROP TABLE test_array_data;
DROP TABLE test_json_data;
DROP TABLE test_datetime_data;
DROP TABLE test_numeric_data;
DROP TABLE test_string_data;

DROP DATABASE test_expressions_functions_db;

-- End of expressions and functions tests
