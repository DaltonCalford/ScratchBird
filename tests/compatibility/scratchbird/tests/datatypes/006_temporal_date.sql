-- ScratchBird Native Test: Temporal Types - DATE
-- Test ID: datatypes_006
-- Description: Comprehensive testing of DATE type and operations

CREATE DATABASE test_temporal_date;
\c test_temporal_date;

-- ============================================================================
-- DATE Basic Testing
-- ============================================================================

CREATE TABLE test_date_basic (
    id INTEGER PRIMARY KEY,
    event_date DATE,
    description VARCHAR(100)
);

-- Current date
INSERT INTO test_date_basic VALUES (1, CURRENT_DATE, 'Today');
INSERT INTO test_date_basic VALUES (2, CURRENT_DATE - 1, 'Yesterday');
INSERT INTO test_date_basic VALUES (3, CURRENT_DATE + 1, 'Tomorrow');

-- Specific dates
INSERT INTO test_date_basic VALUES (10, DATE '2024-01-01', 'New Year 2024');
INSERT INTO test_date_basic VALUES (11, DATE '2024-12-31', 'Last day of 2024');
INSERT INTO test_date_basic VALUES (12, DATE '2024-07-04', 'Independence Day');
INSERT INTO test_date_basic VALUES (13, DATE '2024-12-25', 'Christmas');

-- Historical dates
INSERT INTO test_date_basic VALUES (20, DATE '1970-01-01', 'Unix Epoch');
INSERT INTO test_date_basic VALUES (21, DATE '2000-01-01', 'Y2K');
INSERT INTO test_date_basic VALUES (22, DATE '1969-07-20', 'Moon Landing');
INSERT INTO test_date_basic VALUES (23, DATE '1776-07-04', 'US Independence');

-- Future dates
INSERT INTO test_date_basic VALUES (30, DATE '2100-01-01', 'New Century');
INSERT INTO test_date_basic VALUES (31, DATE '2050-06-15', 'Mid-21st century');

-- Edge cases
INSERT INTO test_date_basic VALUES (40, DATE '0001-01-01', 'Minimum date (if supported)');
INSERT INTO test_date_basic VALUES (41, DATE '9999-12-31', 'Maximum date (if supported)');

SELECT id, event_date, description FROM test_date_basic ORDER BY event_date;

-- ============================================================================
-- DATE Input Formats
-- ============================================================================

CREATE TABLE test_date_formats (
    id INTEGER PRIMARY KEY,
    parsed_date DATE,
    format_description VARCHAR(50)
);

-- ISO format (YYYY-MM-DD) - standard
INSERT INTO test_date_formats VALUES (1, DATE '2024-03-15', 'ISO format');
INSERT INTO test_date_formats VALUES (2, '2024-03-15'::DATE, 'Cast from string');

-- Other formats (if supported)
INSERT INTO test_date_formats VALUES (10, TO_DATE('15/03/2024', 'DD/MM/YYYY'), 'DD/MM/YYYY format');
INSERT INTO test_date_formats VALUES (11, TO_DATE('03-15-2024', 'MM-DD-YYYY'), 'MM-DD-YYYY format');
INSERT INTO test_date_formats VALUES (12, TO_DATE('March 15, 2024', 'Month DD, YYYY'), 'Month name format');

SELECT id, parsed_date, format_description FROM test_date_formats ORDER BY id;

-- ============================================================================
-- DATE Extraction Functions
-- ============================================================================

CREATE TABLE test_date_extract (
    id INTEGER PRIMARY KEY,
    test_date DATE
);

INSERT INTO test_date_extract VALUES (1, DATE '2024-03-15');
INSERT INTO test_date_extract VALUES (2, DATE '2024-12-31');
INSERT INTO test_date_extract VALUES (3, DATE '2024-01-01');
INSERT INTO test_date_extract VALUES (4, DATE '2024-02-29');  -- Leap year

-- Extract components
SELECT id, test_date,
       EXTRACT(YEAR FROM test_date) AS year,
       EXTRACT(MONTH FROM test_date) AS month,
       EXTRACT(DAY FROM test_date) AS day
FROM test_date_extract
ORDER BY id;

-- Day of week
SELECT id, test_date,
       EXTRACT(DOW FROM test_date) AS day_of_week,     -- 0=Sunday, 6=Saturday
       EXTRACT(ISODOW FROM test_date) AS iso_day_of_week  -- 1=Monday, 7=Sunday
FROM test_date_extract
ORDER BY id;

-- Day of year
SELECT id, test_date,
       EXTRACT(DOY FROM test_date) AS day_of_year
FROM test_date_extract
ORDER BY id;

-- Week number
SELECT id, test_date,
       EXTRACT(WEEK FROM test_date) AS week_number
FROM test_date_extract
ORDER BY id;

-- Quarter
SELECT id, test_date,
       EXTRACT(QUARTER FROM test_date) AS quarter
FROM test_date_extract
ORDER BY id;

-- ============================================================================
-- DATE Arithmetic
-- ============================================================================

CREATE TABLE test_date_arithmetic (
    id INTEGER PRIMARY KEY,
    base_date DATE
);

INSERT INTO test_date_arithmetic VALUES (1, DATE '2024-03-15');
INSERT INTO test_date_arithmetic VALUES (2, DATE '2024-12-31');
INSERT INTO test_date_arithmetic VALUES (3, DATE '2024-02-28');

-- Add/subtract days
SELECT id, base_date,
       base_date + 1 AS next_day,
       base_date + 7 AS next_week,
       base_date + 30 AS approx_next_month,
       base_date + 365 AS next_year
FROM test_date_arithmetic
ORDER BY id;

-- Subtract days
SELECT id, base_date,
       base_date - 1 AS previous_day,
       base_date - 7 AS previous_week,
       base_date - 30 AS approx_previous_month,
       base_date - 365 AS previous_year
FROM test_date_arithmetic
ORDER BY id;

-- Date difference (returns number of days)
SELECT d1.id AS id1, d2.id AS id2,
       d1.base_date AS date1,
       d2.base_date AS date2,
       d2.base_date - d1.base_date AS days_between
FROM test_date_arithmetic d1
CROSS JOIN test_date_arithmetic d2
WHERE d1.id < d2.id
ORDER BY d1.id, d2.id;

-- Add intervals
SELECT id, base_date,
       base_date + INTERVAL '1 day' AS plus_1_day,
       base_date + INTERVAL '1 week' AS plus_1_week,
       base_date + INTERVAL '1 month' AS plus_1_month,
       base_date + INTERVAL '1 year' AS plus_1_year
FROM test_date_arithmetic WHERE id = 1;

-- ============================================================================
-- DATE Comparison and Ranges
-- ============================================================================

CREATE TABLE test_date_comparison (
    id INTEGER PRIMARY KEY,
    event_name VARCHAR(50),
    event_date DATE
);

INSERT INTO test_date_comparison VALUES (1, 'Event A', DATE '2024-01-15');
INSERT INTO test_date_comparison VALUES (2, 'Event B', DATE '2024-03-20');
INSERT INTO test_date_comparison VALUES (3, 'Event C', DATE '2024-06-10');
INSERT INTO test_date_comparison VALUES (4, 'Event D', DATE '2024-09-05');
INSERT INTO test_date_comparison VALUES (5, 'Event E', DATE '2024-12-15');

-- Equality
SELECT id, event_name FROM test_date_comparison WHERE event_date = DATE '2024-06-10';

-- Greater than / Less than
SELECT id, event_name FROM test_date_comparison WHERE event_date > DATE '2024-06-01'
ORDER BY event_date;

SELECT id, event_name FROM test_date_comparison WHERE event_date < DATE '2024-06-01'
ORDER BY event_date;

-- BETWEEN
SELECT id, event_name, event_date FROM test_date_comparison
WHERE event_date BETWEEN DATE '2024-03-01' AND DATE '2024-09-30'
ORDER BY event_date;

-- Future/past relative to current date
SELECT id, event_name,
       CASE
           WHEN event_date < CURRENT_DATE THEN 'Past'
           WHEN event_date = CURRENT_DATE THEN 'Today'
           ELSE 'Future'
       END AS time_category
FROM test_date_comparison
ORDER BY event_date;

-- ============================================================================
-- DATE Formatting and Output
-- ============================================================================

CREATE TABLE test_date_formatting (
    id INTEGER PRIMARY KEY,
    some_date DATE
);

INSERT INTO test_date_formatting VALUES (1, DATE '2024-03-15');
INSERT INTO test_date_formatting VALUES (2, DATE '2024-12-25');
INSERT INTO test_date_formatting VALUES (3, DATE '2024-07-04');

-- Various output formats
SELECT id, some_date,
       TO_CHAR(some_date, 'YYYY-MM-DD') AS iso_format,
       TO_CHAR(some_date, 'DD/MM/YYYY') AS european,
       TO_CHAR(some_date, 'MM/DD/YYYY') AS american,
       TO_CHAR(some_date, 'Month DD, YYYY') AS long_format,
       TO_CHAR(some_date, 'Mon DD, YYYY') AS abbrev_month,
       TO_CHAR(some_date, 'Day, Month DD, YYYY') AS full_format
FROM test_date_formatting
ORDER BY id;

-- Day names
SELECT id, some_date,
       TO_CHAR(some_date, 'Day') AS day_name,
       TO_CHAR(some_date, 'Dy') AS day_abbrev
FROM test_date_formatting
ORDER BY id;

-- ============================================================================
-- DATE Special Cases
-- ============================================================================

CREATE TABLE test_date_special (
    id INTEGER PRIMARY KEY,
    test_date DATE,
    description VARCHAR(100)
);

-- Leap years
INSERT INTO test_date_special VALUES (1, DATE '2024-02-29', 'Leap day 2024');
INSERT INTO test_date_special VALUES (2, DATE '2020-02-29', 'Leap day 2020');
INSERT INTO test_date_special VALUES (3, DATE '2000-02-29', 'Leap day 2000 (century)');

-- Month boundaries
INSERT INTO test_date_special VALUES (10, DATE '2024-01-31', 'Jan 31');
INSERT INTO test_date_special VALUES (11, DATE '2024-02-01', 'Feb 1');
INSERT INTO test_date_special VALUES (12, DATE '2024-03-31', 'Mar 31');
INSERT INTO test_date_special VALUES (13, DATE '2024-04-01', 'Apr 1');
INSERT INTO test_date_special VALUES (14, DATE '2024-04-30', 'Apr 30');

-- Year boundaries
INSERT INTO test_date_special VALUES (20, DATE '2023-12-31', 'Last day of 2023');
INSERT INTO test_date_special VALUES (21, DATE '2024-01-01', 'First day of 2024');
INSERT INTO test_date_special VALUES (22, DATE '2024-12-31', 'Last day of 2024');
INSERT INTO test_date_special VALUES (23, DATE '2025-01-01', 'First day of 2025');

SELECT id, test_date, description FROM test_date_special ORDER BY test_date;

-- Leap year detection
SELECT id, test_date,
       EXTRACT(YEAR FROM test_date) AS year,
       CASE
           WHEN EXTRACT(YEAR FROM test_date) % 4 = 0 AND
                (EXTRACT(YEAR FROM test_date) % 100 != 0 OR EXTRACT(YEAR FROM test_date) % 400 = 0)
           THEN 'Leap Year'
           ELSE 'Regular Year'
       END AS year_type
FROM test_date_special
WHERE EXTRACT(MONTH FROM test_date) = 2 AND EXTRACT(DAY FROM test_date) = 29;

-- ============================================================================
-- DATE Functions and Utilities
-- ============================================================================

CREATE TABLE test_date_functions (
    id INTEGER PRIMARY KEY,
    test_date DATE
);

INSERT INTO test_date_functions VALUES (1, DATE '2024-03-15');
INSERT INTO test_date_functions VALUES (2, DATE '2024-06-20');
INSERT INTO test_date_functions VALUES (3, DATE '2024-12-10');

-- First day of month
SELECT id, test_date,
       DATE_TRUNC('month', test_date) AS first_of_month
FROM test_date_functions
ORDER BY id;

-- Last day of month
SELECT id, test_date,
       (DATE_TRUNC('month', test_date) + INTERVAL '1 month' - INTERVAL '1 day')::DATE AS last_of_month
FROM test_date_functions
ORDER BY id;

-- First day of year
SELECT id, test_date,
       DATE_TRUNC('year', test_date) AS first_of_year
FROM test_date_functions
ORDER BY id;

-- First day of week (Monday)
SELECT id, test_date,
       DATE_TRUNC('week', test_date) AS week_start_monday
FROM test_date_functions
ORDER BY id;

-- Age calculation
CREATE TABLE test_date_age (
    id INTEGER PRIMARY KEY,
    birth_date DATE,
    person_name VARCHAR(50)
);

INSERT INTO test_date_age VALUES (1, DATE '1990-05-15', 'Person A');
INSERT INTO test_date_age VALUES (2, DATE '1985-12-20', 'Person B');
INSERT INTO test_date_age VALUES (3, DATE '2000-03-10', 'Person C');

SELECT id, person_name, birth_date,
       AGE(CURRENT_DATE, birth_date) AS age,
       EXTRACT(YEAR FROM AGE(CURRENT_DATE, birth_date)) AS years_old
FROM test_date_age
ORDER BY birth_date;

-- ============================================================================
-- DATE Aggregation
-- ============================================================================

CREATE TABLE test_date_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    event_date DATE
);

INSERT INTO test_date_aggregation VALUES (1, 'Category A', DATE '2024-01-15');
INSERT INTO test_date_aggregation VALUES (2, 'Category A', DATE '2024-03-20');
INSERT INTO test_date_aggregation VALUES (3, 'Category A', DATE '2024-05-10');
INSERT INTO test_date_aggregation VALUES (4, 'Category B', DATE '2024-02-14');
INSERT INTO test_date_aggregation VALUES (5, 'Category B', DATE '2024-08-30');
INSERT INTO test_date_aggregation VALUES (6, 'Category C', DATE '2024-12-25');

-- MIN/MAX dates
SELECT category,
       MIN(event_date) AS earliest,
       MAX(event_date) AS latest,
       MAX(event_date) - MIN(event_date) AS days_span
FROM test_date_aggregation
GROUP BY category
ORDER BY category;

-- Count by month
SELECT EXTRACT(YEAR FROM event_date) AS year,
       EXTRACT(MONTH FROM event_date) AS month,
       COUNT(*) AS event_count
FROM test_date_aggregation
GROUP BY EXTRACT(YEAR FROM event_date), EXTRACT(MONTH FROM event_date)
ORDER BY year, month;

-- ============================================================================
-- DATE NULL Handling
-- ============================================================================

CREATE TABLE test_date_nulls (
    id INTEGER PRIMARY KEY,
    nullable_date DATE,
    description VARCHAR(50)
);

INSERT INTO test_date_nulls VALUES (1, NULL, 'No date set');
INSERT INTO test_date_nulls VALUES (2, DATE '2024-01-01', 'Has date');
INSERT INTO test_date_nulls VALUES (3, NULL, 'Another null');
INSERT INTO test_date_nulls VALUES (4, DATE '2024-12-31', 'End of year');

SELECT COUNT(*) AS null_date_count FROM test_date_nulls WHERE nullable_date IS NULL;

SELECT id, COALESCE(nullable_date, DATE '1970-01-01') AS date_with_default
FROM test_date_nulls
ORDER BY id;

-- NULL in arithmetic
SELECT id, nullable_date, nullable_date + 7 AS week_later FROM test_date_nulls;

-- ============================================================================
-- DATE Sorting and Indexing
-- ============================================================================

CREATE TABLE test_date_sorting (
    id INTEGER PRIMARY KEY,
    event_date DATE,
    event_name VARCHAR(50)
);

INSERT INTO test_date_sorting VALUES (5, DATE '2024-12-15', 'Event E');
INSERT INTO test_date_sorting VALUES (2, DATE '2024-03-10', 'Event B');
INSERT INTO test_date_sorting VALUES (4, DATE '2024-09-20', 'Event D');
INSERT INTO test_date_sorting VALUES (1, DATE '2024-01-05', 'Event A');
INSERT INTO test_date_sorting VALUES (3, DATE '2024-06-30', 'Event C');

-- Ascending sort
SELECT id, event_date, event_name FROM test_date_sorting ORDER BY event_date ASC;

-- Descending sort
SELECT id, event_date, event_name FROM test_date_sorting ORDER BY event_date DESC;

-- Create index on date column
CREATE INDEX idx_event_date ON test_date_sorting(event_date);

-- Query using index
SELECT id, event_name FROM test_date_sorting
WHERE event_date BETWEEN DATE '2024-03-01' AND DATE '2024-09-30'
ORDER BY event_date;

-- ============================================================================
-- DATE Constraints
-- ============================================================================

CREATE TABLE test_date_constraints (
    id INTEGER PRIMARY KEY,
    start_date DATE NOT NULL,
    end_date DATE,
    CHECK (end_date IS NULL OR end_date >= start_date)  -- End must be after start
);

INSERT INTO test_date_constraints VALUES (1, DATE '2024-01-01', DATE '2024-12-31');
INSERT INTO test_date_constraints VALUES (2, DATE '2024-06-01', NULL);  -- Ongoing
INSERT INTO test_date_constraints VALUES (3, DATE '2024-03-15', DATE '2024-03-20');

-- This should fail the CHECK constraint
-- INSERT INTO test_date_constraints VALUES (4, DATE '2024-12-31', DATE '2024-01-01');

SELECT id, start_date, end_date, end_date - start_date AS duration_days
FROM test_date_constraints
ORDER BY id;

-- ============================================================================
-- DATE with Business Logic
-- ============================================================================

CREATE TABLE test_date_business (
    id INTEGER PRIMARY KEY,
    transaction_date DATE,
    amount DECIMAL(10, 2)
);

INSERT INTO test_date_business VALUES (1, DATE '2024-01-15', 100.00);
INSERT INTO test_date_business VALUES (2, DATE '2024-02-20', 200.00);
INSERT INTO test_date_business VALUES (3, DATE '2024-03-10', 150.00);
INSERT INTO test_date_business VALUES (4, DATE '2024-04-05', 300.00);
INSERT INTO test_date_business VALUES (5, DATE '2024-05-25', 250.00);

-- Fiscal quarter calculation (Q1=Jan-Mar, Q2=Apr-Jun, etc.)
SELECT id, transaction_date, amount,
       EXTRACT(QUARTER FROM transaction_date) AS fiscal_quarter,
       CASE EXTRACT(QUARTER FROM transaction_date)
           WHEN 1 THEN 'Q1 (Jan-Mar)'
           WHEN 2 THEN 'Q2 (Apr-Jun)'
           WHEN 3 THEN 'Q3 (Jul-Sep)'
           WHEN 4 THEN 'Q4 (Oct-Dec)'
       END AS quarter_name
FROM test_date_business
ORDER BY transaction_date;

-- Running total by date
SELECT id, transaction_date, amount,
       SUM(amount) OVER (ORDER BY transaction_date) AS running_total
FROM test_date_business
ORDER BY transaction_date;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_date_business;
DROP TABLE test_date_constraints;
DROP INDEX idx_event_date;
DROP TABLE test_date_sorting;
DROP TABLE test_date_nulls;
DROP TABLE test_date_aggregation;
DROP TABLE test_date_age;
DROP TABLE test_date_functions;
DROP TABLE test_date_special;
DROP TABLE test_date_formatting;
DROP TABLE test_date_comparison;
DROP TABLE test_date_arithmetic;
DROP TABLE test_date_extract;
DROP TABLE test_date_formats;
DROP TABLE test_date_basic;

DROP DATABASE test_temporal_date;
