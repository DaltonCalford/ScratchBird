-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - Expression/Functional Indexes
-- Description: Comprehensive expression (functional) index testing
-- ============================================================================

-- Expression indexes index the result of functions/expressions
-- Best for: case-insensitive searches, date part queries, computed values
-- Note: Query must use the EXACT same expression to use the index

-- Create test database
CREATE DATABASE test_expression_idx_db;
USE test_expression_idx_db;

-- ============================================================================
-- Section 1: Basic Expression Index - LOWER()
-- ============================================================================

CREATE TABLE test_expression_basic (
    id INT PRIMARY KEY,
    email VARCHAR(200),
    username VARCHAR(100),
    full_name VARCHAR(200)
);

-- Expression index for case-insensitive email search
CREATE INDEX idx_email_lower ON test_expression_basic (LOWER(email));

INSERT INTO test_expression_basic VALUES
    (1, 'Alice@Example.COM', 'alice', 'Alice Smith'),
    (2, 'Bob@EXAMPLE.com', 'bob', 'Bob Jones'),
    (3, 'Charlie@example.com', 'charlie', 'Charlie Brown'),
    (4, 'DIANA@EXAMPLE.COM', 'diana', 'Diana Prince');

-- Uses expression index (exact match)
SELECT id, email, username FROM test_expression_basic
WHERE LOWER(email) = 'alice@example.com';

-- Does NOT use index (different expression)
SELECT id FROM test_expression_basic
WHERE email = 'alice@example.com';  -- Case-sensitive, no index

-- Case-insensitive search
SELECT id, email FROM test_expression_basic
WHERE LOWER(email) LIKE '%bob%';

-- ============================================================================
-- Section 2: Expression Index on Date Functions
-- ============================================================================

CREATE TABLE test_expression_dates (
    event_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_timestamp TIMESTAMP
);

-- Index on date part (ignoring time)
CREATE INDEX idx_event_date ON test_expression_dates (DATE(event_timestamp));

-- Index on year
CREATE INDEX idx_event_year ON test_expression_dates (EXTRACT(YEAR FROM event_timestamp));

-- Index on month
CREATE INDEX idx_event_month ON test_expression_dates (EXTRACT(MONTH FROM event_timestamp));

INSERT INTO test_expression_dates
SELECT i,
       'Event ' || i,
       TIMESTAMP '2024-01-01 00:00:00' + (i || ' hours')::INTERVAL
FROM generate_series(1, 10000) i;

-- Uses DATE() index
SELECT event_id, event_name FROM test_expression_dates
WHERE DATE(event_timestamp) = '2024-02-15'
LIMIT 10;

-- Uses YEAR index
SELECT event_id, event_name FROM test_expression_dates
WHERE EXTRACT(YEAR FROM event_timestamp) = 2024
LIMIT 10;

-- Uses MONTH index (find all events in March)
SELECT event_id, event_name FROM test_expression_dates
WHERE EXTRACT(MONTH FROM event_timestamp) = 3
LIMIT 10;

-- ============================================================================
-- Section 3: Expression Index on String Functions
-- ============================================================================

CREATE TABLE test_expression_strings (
    id INT PRIMARY KEY,
    product_code VARCHAR(100),
    description TEXT
);

-- Index on substring
CREATE INDEX idx_code_prefix ON test_expression_strings (SUBSTRING(product_code, 1, 3));

-- Index on string length
CREATE INDEX idx_desc_length ON test_expression_strings (LENGTH(description));

INSERT INTO test_expression_strings
SELECT i,
       CASE (i % 5) WHEN 0 THEN 'ABC' WHEN 1 THEN 'DEF' WHEN 2 THEN 'GHI' WHEN 3 THEN 'JKL' ELSE 'MNO' END || '-' || i,
       'Description with ' || i || ' characters'
FROM generate_series(1, 10000) i;

-- Uses substring index
SELECT id, product_code FROM test_expression_strings
WHERE SUBSTRING(product_code, 1, 3) = 'ABC'
LIMIT 10;

-- Uses length index
SELECT id, description FROM test_expression_strings
WHERE LENGTH(description) > 30
LIMIT 10;

-- ============================================================================
-- Section 4: Expression Index on Math Functions
-- ============================================================================

CREATE TABLE test_expression_math (
    id INT PRIMARY KEY,
    price NUMERIC(10,2),
    quantity INT,
    discount_percent NUMERIC(5,2)
);

-- Index on calculated total (price * quantity)
CREATE INDEX idx_total_value ON test_expression_math ((price * quantity));

-- Index on discounted price
CREATE INDEX idx_discounted_price ON test_expression_math ((price * (1 - discount_percent / 100)));

INSERT INTO test_expression_math
SELECT i,
       (random() * 100)::NUMERIC(10,2),
       (random() * 50)::INT + 1,
       (random() * 30)::NUMERIC(5,2)
FROM generate_series(1, 10000) i;

-- Uses total value index
SELECT id, price, quantity, (price * quantity) AS total FROM test_expression_math
WHERE (price * quantity) > 1000
LIMIT 10;

-- Uses discounted price index
SELECT id, price, discount_percent, (price * (1 - discount_percent / 100)) AS final_price
FROM test_expression_math
WHERE (price * (1 - discount_percent / 100)) < 50
LIMIT 10;

-- ============================================================================
-- Section 5: Expression Index with COALESCE
-- ============================================================================

CREATE TABLE test_expression_coalesce (
    id INT PRIMARY KEY,
    primary_email VARCHAR(200),
    secondary_email VARCHAR(200),
    backup_email VARCHAR(200)
);

-- Index on first non-null email
CREATE INDEX idx_first_email ON test_expression_coalesce (COALESCE(primary_email, secondary_email, backup_email));

INSERT INTO test_expression_coalesce
SELECT i,
       CASE WHEN i % 3 = 0 THEN 'primary' || i || '@example.com' ELSE NULL END,
       CASE WHEN i % 2 = 0 THEN 'secondary' || i || '@example.com' ELSE NULL END,
       'backup' || i || '@example.com'
FROM generate_series(1, 1000) i;

-- Uses COALESCE index
SELECT id, COALESCE(primary_email, secondary_email, backup_email) AS email
FROM test_expression_coalesce
WHERE COALESCE(primary_email, secondary_email, backup_email) LIKE '%500%';

-- ============================================================================
-- Section 6: Expression Index on JSONB
-- ============================================================================

CREATE TABLE test_expression_jsonb (
    id INT PRIMARY KEY,
    user_data JSONB
);

-- Index on specific JSONB field
CREATE INDEX idx_user_email ON test_expression_jsonb ((user_data->>'email'));

-- Index on nested field
CREATE INDEX idx_user_city ON test_expression_jsonb ((user_data->'address'->>'city'));

INSERT INTO test_expression_jsonb
SELECT i,
       ('{"email": "user' || i || '@example.com", "name": "User ' || i || '", "address": {"city": "City' || (i % 20) || '", "zip": "' || (10000 + i % 90000) || '"}}')::JSONB
FROM generate_series(1, 10000) i;

-- Uses JSONB email index
SELECT id, user_data->>'email' AS email FROM test_expression_jsonb
WHERE user_data->>'email' = 'user500@example.com';

-- Uses nested city index
SELECT id, user_data->>'name' AS name FROM test_expression_jsonb
WHERE user_data->'address'->>'city' = 'City10'
LIMIT 10;

-- ============================================================================
-- Section 7: Expression Index with UPPER() and TRIM()
-- ============================================================================

CREATE TABLE test_expression_upper_trim (
    id INT PRIMARY KEY,
    company_name VARCHAR(200)
);

-- Index on uppercase trimmed name
CREATE INDEX idx_company_normalized ON test_expression_upper_trim (UPPER(TRIM(company_name)));

INSERT INTO test_expression_upper_trim VALUES
    (1, '  Acme Corporation  '),
    (2, 'Global Industries'),
    (3, '  Tech Solutions  '),
    (4, 'Innovative Systems');

-- Uses normalized index
SELECT id, company_name FROM test_expression_upper_trim
WHERE UPPER(TRIM(company_name)) = 'ACME CORPORATION';

-- ============================================================================
-- Section 8: Expression Index on Array Elements
-- ============================================================================

CREATE TABLE test_expression_arrays (
    id INT PRIMARY KEY,
    tags TEXT[]
);

-- Index on array length
CREATE INDEX idx_tag_count ON test_expression_arrays (array_length(tags, 1));

-- Index on first array element
CREATE INDEX idx_first_tag ON test_expression_arrays ((tags[1]));

INSERT INTO test_expression_arrays
SELECT i,
       ARRAY['tag' || (i % 10), 'tag' || (i % 20), 'tag' || (i % 30)]
FROM generate_series(1, 10000) i;

-- Uses array length index
SELECT id, tags FROM test_expression_arrays
WHERE array_length(tags, 1) >= 5
LIMIT 10;

-- Uses first element index
SELECT id, tags FROM test_expression_arrays
WHERE tags[1] = 'tag5'
LIMIT 10;

-- ============================================================================
-- Section 9: Expression Index on Timezone Conversion
-- ============================================================================

CREATE TABLE test_expression_timezone (
    id INT PRIMARY KEY,
    event_time TIMESTAMPTZ
);

-- Index on time converted to specific timezone
CREATE INDEX idx_time_utc ON test_expression_timezone ((event_time AT TIME ZONE 'UTC'));
CREATE INDEX idx_time_est ON test_expression_timezone ((event_time AT TIME ZONE 'America/New_York'));

INSERT INTO test_expression_timezone
SELECT i,
       TIMESTAMPTZ '2024-01-01 00:00:00+00' + (i || ' hours')::INTERVAL
FROM generate_series(1, 10000) i;

-- Uses UTC conversion index
SELECT id, event_time AT TIME ZONE 'UTC' AS utc_time FROM test_expression_timezone
WHERE (event_time AT TIME ZONE 'UTC')::DATE = '2024-02-01'
LIMIT 10;

-- ============================================================================
-- Section 10: Expression Index for Full-Text Search
-- ============================================================================

CREATE TABLE test_expression_fulltext (
    doc_id INT PRIMARY KEY,
    title TEXT,
    content TEXT
);

-- Index on to_tsvector expression
CREATE INDEX idx_content_search ON test_expression_fulltext USING GIN (to_tsvector('english', content));

-- Combined title and content search
CREATE INDEX idx_combined_search ON test_expression_fulltext USING GIN (
    to_tsvector('english', COALESCE(title, '') || ' ' || COALESCE(content, ''))
);

INSERT INTO test_expression_fulltext
SELECT i,
       'Document Title ' || i,
       'This is the content of document number ' || i || ' with searchable text about databases and programming'
FROM generate_series(1, 10000) i;

-- Uses GIN expression index
SELECT doc_id, title FROM test_expression_fulltext
WHERE to_tsvector('english', content) @@ to_tsquery('english', 'database')
LIMIT 10;

-- Uses combined index
SELECT doc_id, title FROM test_expression_fulltext
WHERE to_tsvector('english', COALESCE(title, '') || ' ' || COALESCE(content, '')) @@ to_tsquery('english', 'document & programming')
LIMIT 10;

-- ============================================================================
-- Section 11: Expression Index with Type Casting
-- ============================================================================

CREATE TABLE test_expression_casting (
    id INT PRIMARY KEY,
    code VARCHAR(20),
    numeric_value TEXT  -- Stored as text but represents number
);

-- Index on cast to numeric
CREATE INDEX idx_numeric_cast ON test_expression_casting ((numeric_value::NUMERIC));

INSERT INTO test_expression_casting
SELECT i,
       'CODE-' || i,
       (random() * 1000)::NUMERIC(10,2)::TEXT
FROM generate_series(1, 10000) i;

-- Uses cast index
SELECT id, code, numeric_value FROM test_expression_casting
WHERE numeric_value::NUMERIC > 500
LIMIT 10;

-- ============================================================================
-- Section 12: Expression Index on Regular Expressions
-- ============================================================================

CREATE TABLE test_expression_regex (
    id INT PRIMARY KEY,
    phone_number VARCHAR(50)
);

-- Index on normalized phone (digits only)
CREATE INDEX idx_phone_normalized ON test_expression_regex (regexp_replace(phone_number, '[^0-9]', '', 'g'));

INSERT INTO test_expression_regex VALUES
    (1, '(555) 123-4567'),
    (2, '555-234-5678'),
    (3, '555.345.6789'),
    (4, '5554567890');

-- Uses regex expression index
SELECT id, phone_number FROM test_expression_regex
WHERE regexp_replace(phone_number, '[^0-9]', '', 'g') = '5551234567';

-- ============================================================================
-- Section 13: Expression Index for Age Calculation
-- ============================================================================

CREATE TABLE test_expression_age (
    user_id INT PRIMARY KEY,
    birth_date DATE,
    name VARCHAR(100)
);

-- Index on calculated age
CREATE INDEX idx_age ON test_expression_age (EXTRACT(YEAR FROM AGE(CURRENT_DATE, birth_date)));

INSERT INTO test_expression_age
SELECT i,
       DATE '2000-01-01' - (i % 365 * 30 || ' days')::INTERVAL,
       'User ' || i
FROM generate_series(1, 1000) i;

-- Query users by age
SELECT user_id, name, birth_date,
       EXTRACT(YEAR FROM AGE(CURRENT_DATE, birth_date)) AS age
FROM test_expression_age
WHERE EXTRACT(YEAR FROM AGE(CURRENT_DATE, birth_date)) BETWEEN 25 AND 35
LIMIT 10;

-- ============================================================================
-- Section 14: Multicolumn Expression Index
-- ============================================================================

CREATE TABLE test_expression_multicolumn (
    id INT PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    amount NUMERIC(10,2),
    tax_rate NUMERIC(5,2)
);

-- Expression index on multiple columns/expressions
CREATE INDEX idx_full_name_lower ON test_expression_multicolumn (LOWER(first_name || ' ' || last_name));

-- Expression index on calculated tax
CREATE INDEX idx_amount_with_tax ON test_expression_multicolumn ((amount * (1 + tax_rate / 100)));

INSERT INTO test_expression_multicolumn
SELECT i,
       'FirstName' || i,
       'LastName' || i,
       (random() * 1000)::NUMERIC(10,2),
       (random() * 15)::NUMERIC(5,2)
FROM generate_series(1, 10000) i;

-- Uses full name index
SELECT id, first_name, last_name FROM test_expression_multicolumn
WHERE LOWER(first_name || ' ' || last_name) = 'firstname500 lastname500';

-- Uses tax calculation index
SELECT id, amount, tax_rate, (amount * (1 + tax_rate / 100)) AS total FROM test_expression_multicolumn
WHERE (amount * (1 + tax_rate / 100)) > 500
LIMIT 10;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_expression_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_expression_best_practices VALUES
    (1, 'Query must use EXACT same expression as index definition'),
    (2, 'Perfect for case-insensitive searches: LOWER(column)'),
    (3, 'Use for date part queries: EXTRACT, DATE(), DATE_TRUNC()'),
    (4, 'Ideal for computed values: price * quantity, discounts'),
    (5, 'Supports complex expressions and function calls'),
    (6, 'Expression indexes are larger than simple column indexes'),
    (7, 'Update cost is higher (expression must be recalculated)'),
    (8, 'Use for JSONB field extraction: data->>"field"'),
    (9, 'Combine with COALESCE for fallback values'),
    (10, 'Works with array functions: array_length, element access'),
    (11, 'Can use with GIN/GiST for to_tsvector'),
    (12, 'Useful for type casting: text::numeric'),
    (13, 'Expression must be immutable (deterministic)'),
    (14, 'Test with EXPLAIN to verify index usage'),
    (15, 'Monitor expression evaluation cost for updates');

SELECT id, guideline FROM test_expression_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_expression_best_practices;
DROP TABLE test_expression_multicolumn;
DROP TABLE test_expression_age;
DROP TABLE test_expression_regex;
DROP TABLE test_expression_casting;
DROP TABLE test_expression_fulltext;
DROP TABLE test_expression_timezone;
DROP TABLE test_expression_arrays;
DROP TABLE test_expression_upper_trim;
DROP TABLE test_expression_jsonb;
DROP TABLE test_expression_coalesce;
DROP TABLE test_expression_math;
DROP TABLE test_expression_strings;
DROP TABLE test_expression_dates;
DROP TABLE test_expression_basic;

DROP DATABASE test_expression_idx_db;

-- End of expression index tests
