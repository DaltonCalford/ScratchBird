-- ScratchBird Native Test: Temporal Types - TIMESTAMP
-- Test ID: datatypes_008
-- Description: Comprehensive testing of TIMESTAMP and TIMESTAMP WITH TIME ZONE types

CREATE DATABASE test_temporal_timestamp;
\c test_temporal_timestamp;

-- ============================================================================
-- TIMESTAMP Basic Testing
-- ============================================================================

CREATE TABLE test_timestamp_basic (
    id INTEGER PRIMARY KEY,
    event_timestamp TIMESTAMP,
    description VARCHAR(100)
);

-- Current timestamp
INSERT INTO test_timestamp_basic VALUES (1, CURRENT_TIMESTAMP, 'Now');
INSERT INTO test_timestamp_basic VALUES (2, NOW(), 'Now via NOW()');
INSERT INTO test_timestamp_basic VALUES (3, LOCALTIMESTAMP, 'Local timestamp');

-- Specific timestamps
INSERT INTO test_timestamp_basic VALUES (10, TIMESTAMP '2024-01-01 00:00:00', 'New Year 2024');
INSERT INTO test_timestamp_basic VALUES (11, TIMESTAMP '2024-12-31 23:59:59', 'End of 2024');
INSERT INTO test_timestamp_basic VALUES (12, TIMESTAMP '2024-07-04 12:00:00', 'Independence Day noon');
INSERT INTO test_timestamp_basic VALUES (13, TIMESTAMP '2024-12-25 08:30:00', 'Christmas morning');

-- Historical timestamps
INSERT INTO test_timestamp_basic VALUES (20, TIMESTAMP '1970-01-01 00:00:00', 'Unix Epoch');
INSERT INTO test_timestamp_basic VALUES (21, TIMESTAMP '2000-01-01 00:00:00', 'Y2K');
INSERT INTO test_timestamp_basic VALUES (22, TIMESTAMP '1969-07-20 20:17:40', 'Moon Landing');

-- Timestamps with fractional seconds
INSERT INTO test_timestamp_basic VALUES (30, TIMESTAMP '2024-06-15 14:30:45.123456', 'Microsecond precision');
INSERT INTO test_timestamp_basic VALUES (31, TIMESTAMP '2024-06-15 14:30:45.999999', 'Almost next second');

SELECT id, event_timestamp, description FROM test_timestamp_basic ORDER BY event_timestamp;

-- ============================================================================
-- TIMESTAMP Input Formats
-- ============================================================================

CREATE TABLE test_timestamp_formats (
    id INTEGER PRIMARY KEY,
    parsed_timestamp TIMESTAMP,
    format_description VARCHAR(50)
);

-- ISO 8601 format
INSERT INTO test_timestamp_formats VALUES (1, TIMESTAMP '2024-03-15 14:30:00', 'ISO format');
INSERT INTO test_timestamp_formats VALUES (2, '2024-03-15 14:30:00'::TIMESTAMP, 'Cast from string');

-- With T separator (ISO 8601)
INSERT INTO test_timestamp_formats VALUES (10, TIMESTAMP '2024-03-15T14:30:00', 'ISO with T separator');

-- With fractional seconds
INSERT INTO test_timestamp_formats VALUES (20, TIMESTAMP '2024-03-15 14:30:00.5', 'Half second');
INSERT INTO test_timestamp_formats VALUES (21, TIMESTAMP '2024-03-15 14:30:00.123', 'Milliseconds');
INSERT INTO test_timestamp_formats VALUES (22, TIMESTAMP '2024-03-15 14:30:00.123456', 'Microseconds');

-- Various date/time separators
INSERT INTO test_timestamp_formats VALUES (30, TO_TIMESTAMP('15/03/2024 14:30:00', 'DD/MM/YYYY HH24:MI:SS'), 'European format');
INSERT INTO test_timestamp_formats VALUES (31, TO_TIMESTAMP('03-15-2024 02:30:00 PM', 'MM-DD-YYYY HH12:MI:SS AM'), '12-hour format');

SELECT id, parsed_timestamp, format_description FROM test_timestamp_formats ORDER BY id;

-- ============================================================================
-- TIMESTAMP Extraction Functions
-- ============================================================================

CREATE TABLE test_timestamp_extract (
    id INTEGER PRIMARY KEY,
    test_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_extract VALUES (1, TIMESTAMP '2024-03-15 14:30:45.123456');
INSERT INTO test_timestamp_extract VALUES (2, TIMESTAMP '2024-12-31 23:59:59.999999');
INSERT INTO test_timestamp_extract VALUES (3, TIMESTAMP '2024-01-01 00:00:00');
INSERT INTO test_timestamp_extract VALUES (4, TIMESTAMP '2024-02-29 12:00:00');  -- Leap year

-- Extract date and time components
SELECT id, test_timestamp,
       EXTRACT(YEAR FROM test_timestamp) AS year,
       EXTRACT(MONTH FROM test_timestamp) AS month,
       EXTRACT(DAY FROM test_timestamp) AS day,
       EXTRACT(HOUR FROM test_timestamp) AS hour,
       EXTRACT(MINUTE FROM test_timestamp) AS minute,
       EXTRACT(SECOND FROM test_timestamp) AS second
FROM test_timestamp_extract
ORDER BY id;

-- Extract date and time separately
SELECT id, test_timestamp,
       DATE(test_timestamp) AS date_part,
       TIME(test_timestamp) AS time_part
FROM test_timestamp_extract
ORDER BY id;

-- Day of week and day of year
SELECT id, test_timestamp,
       EXTRACT(DOW FROM test_timestamp) AS day_of_week,
       EXTRACT(DOY FROM test_timestamp) AS day_of_year,
       EXTRACT(WEEK FROM test_timestamp) AS week_number,
       EXTRACT(QUARTER FROM test_timestamp) AS quarter
FROM test_timestamp_extract
ORDER BY id;

-- Epoch (seconds since Unix epoch)
SELECT id, test_timestamp,
       EXTRACT(EPOCH FROM test_timestamp) AS epoch_seconds
FROM test_timestamp_extract
ORDER BY id;

-- ============================================================================
-- TIMESTAMP Arithmetic
-- ============================================================================

CREATE TABLE test_timestamp_arithmetic (
    id INTEGER PRIMARY KEY,
    base_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_arithmetic VALUES (1, TIMESTAMP '2024-06-15 12:00:00');
INSERT INTO test_timestamp_arithmetic VALUES (2, TIMESTAMP '2024-12-31 23:00:00');
INSERT INTO test_timestamp_arithmetic VALUES (3, TIMESTAMP '2024-01-01 00:30:00');

-- Add intervals
SELECT id, base_timestamp,
       base_timestamp + INTERVAL '1 day' AS plus_1_day,
       base_timestamp + INTERVAL '1 hour' AS plus_1_hour,
       base_timestamp + INTERVAL '30 minutes' AS plus_30_min,
       base_timestamp + INTERVAL '45 seconds' AS plus_45_sec
FROM test_timestamp_arithmetic
ORDER BY id;

-- Subtract intervals
SELECT id, base_timestamp,
       base_timestamp - INTERVAL '1 day' AS minus_1_day,
       base_timestamp - INTERVAL '1 week' AS minus_1_week,
       base_timestamp - INTERVAL '1 month' AS minus_1_month,
       base_timestamp - INTERVAL '1 year' AS minus_1_year
FROM test_timestamp_arithmetic
ORDER BY id;

-- Complex intervals
SELECT id, base_timestamp,
       base_timestamp + INTERVAL '1 day 2 hours 30 minutes' AS complex_add,
       base_timestamp + INTERVAL '90 minutes' AS ninety_minutes,
       base_timestamp + INTERVAL '36 hours' AS day_and_half
FROM test_timestamp_arithmetic WHERE id = 1;

-- Timestamp difference (returns interval)
SELECT t1.id AS id1, t2.id AS id2,
       t1.base_timestamp AS ts1,
       t2.base_timestamp AS ts2,
       t2.base_timestamp - t1.base_timestamp AS time_difference,
       EXTRACT(EPOCH FROM (t2.base_timestamp - t1.base_timestamp)) AS seconds_diff
FROM test_timestamp_arithmetic t1
CROSS JOIN test_timestamp_arithmetic t2
WHERE t1.id < t2.id
ORDER BY t1.id, t2.id;

-- ============================================================================
-- TIMESTAMP Comparison
-- ============================================================================

CREATE TABLE test_timestamp_comparison (
    id INTEGER PRIMARY KEY,
    event_name VARCHAR(50),
    event_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_comparison VALUES (1, 'Event A', TIMESTAMP '2024-01-15 09:00:00');
INSERT INTO test_timestamp_comparison VALUES (2, 'Event B', TIMESTAMP '2024-03-20 14:30:00');
INSERT INTO test_timestamp_comparison VALUES (3, 'Event C', TIMESTAMP '2024-06-10 11:15:00');
INSERT INTO test_timestamp_comparison VALUES (4, 'Event D', TIMESTAMP '2024-09-05 16:45:00');
INSERT INTO test_timestamp_comparison VALUES (5, 'Event E', TIMESTAMP '2024-12-15 08:00:00');

-- Equality
SELECT id, event_name FROM test_timestamp_comparison
WHERE event_timestamp = TIMESTAMP '2024-06-10 11:15:00';

-- Greater than / Less than
SELECT id, event_name, event_timestamp FROM test_timestamp_comparison
WHERE event_timestamp > TIMESTAMP '2024-06-01 00:00:00'
ORDER BY event_timestamp;

-- BETWEEN
SELECT id, event_name, event_timestamp FROM test_timestamp_comparison
WHERE event_timestamp BETWEEN TIMESTAMP '2024-03-01 00:00:00' AND TIMESTAMP '2024-09-30 23:59:59'
ORDER BY event_timestamp;

-- Relative to current time
SELECT id, event_name, event_timestamp,
       CASE
           WHEN event_timestamp < CURRENT_TIMESTAMP THEN 'Past'
           WHEN event_timestamp = CURRENT_TIMESTAMP THEN 'Now'
           ELSE 'Future'
       END AS time_category,
       AGE(CURRENT_TIMESTAMP, event_timestamp) AS time_ago
FROM test_timestamp_comparison
ORDER BY event_timestamp;

-- ============================================================================
-- TIMESTAMP WITH TIME ZONE
-- ============================================================================

CREATE TABLE test_timestamp_tz (
    id INTEGER PRIMARY KEY,
    ts_no_tz TIMESTAMP,
    ts_with_tz TIMESTAMP WITH TIME ZONE,
    location VARCHAR(50)
);

-- UTC timestamps
INSERT INTO test_timestamp_tz VALUES (1,
    TIMESTAMP '2024-06-15 12:00:00',
    TIMESTAMP WITH TIME ZONE '2024-06-15 12:00:00+00:00',
    'UTC'
);

-- US Eastern Time (UTC-5)
INSERT INTO test_timestamp_tz VALUES (2,
    TIMESTAMP '2024-06-15 12:00:00',
    TIMESTAMP WITH TIME ZONE '2024-06-15 12:00:00-05:00',
    'New York (EST)'
);

-- Japan Standard Time (UTC+9)
INSERT INTO test_timestamp_tz VALUES (3,
    TIMESTAMP '2024-06-15 12:00:00',
    TIMESTAMP WITH TIME ZONE '2024-06-15 12:00:00+09:00',
    'Tokyo (JST)'
);

-- UK Time (UTC+0 or UTC+1 depending on DST)
INSERT INTO test_timestamp_tz VALUES (4,
    TIMESTAMP '2024-06-15 12:00:00',
    TIMESTAMP WITH TIME ZONE '2024-06-15 12:00:00+01:00',
    'London (BST)'
);

SELECT id, location, ts_no_tz, ts_with_tz FROM test_timestamp_tz ORDER BY id;

-- Convert to specific timezone
SELECT id, location, ts_with_tz,
       ts_with_tz AT TIME ZONE 'UTC' AS in_utc,
       ts_with_tz AT TIME ZONE 'America/New_York' AS in_ny,
       ts_with_tz AT TIME ZONE 'Asia/Tokyo' AS in_tokyo
FROM test_timestamp_tz
ORDER BY id;

-- ============================================================================
-- TIMESTAMP Formatting and Output
-- ============================================================================

CREATE TABLE test_timestamp_formatting (
    id INTEGER PRIMARY KEY,
    some_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_formatting VALUES (1, TIMESTAMP '2024-03-15 14:30:45');
INSERT INTO test_timestamp_formatting VALUES (2, TIMESTAMP '2024-12-25 08:00:00');
INSERT INTO test_timestamp_formatting VALUES (3, TIMESTAMP '2024-07-04 18:45:30');

-- Various output formats
SELECT id, some_timestamp,
       TO_CHAR(some_timestamp, 'YYYY-MM-DD HH24:MI:SS') AS iso_format,
       TO_CHAR(some_timestamp, 'DD/MM/YYYY HH24:MI:SS') AS european,
       TO_CHAR(some_timestamp, 'MM/DD/YYYY HH12:MI:SS AM') AS american_12hr,
       TO_CHAR(some_timestamp, 'Month DD, YYYY at HH24:MI') AS long_format,
       TO_CHAR(some_timestamp, 'Day, Mon DD YYYY, HH12:MI AM') AS full_format
FROM test_timestamp_formatting
ORDER BY id;

-- With fractional seconds
INSERT INTO test_timestamp_formatting VALUES (10, TIMESTAMP '2024-06-15 14:30:45.123456');

SELECT id, some_timestamp,
       TO_CHAR(some_timestamp, 'YYYY-MM-DD HH24:MI:SS.MS') AS with_milliseconds,
       TO_CHAR(some_timestamp, 'YYYY-MM-DD HH24:MI:SS.US') AS with_microseconds
FROM test_timestamp_formatting WHERE id = 10;

-- ============================================================================
-- TIMESTAMP Precision Testing
-- ============================================================================

CREATE TABLE test_timestamp_precision (
    id INTEGER PRIMARY KEY,
    ts_0 TIMESTAMP(0),    -- No fractional seconds
    ts_3 TIMESTAMP(3),    -- Millisecond precision
    ts_6 TIMESTAMP(6)     -- Microsecond precision
);

INSERT INTO test_timestamp_precision VALUES (1,
    TIMESTAMP '2024-06-15 12:34:56.789123',
    TIMESTAMP '2024-06-15 12:34:56.789123',
    TIMESTAMP '2024-06-15 12:34:56.789123'
);

-- Check precision rounding/truncation
SELECT id,
       ts_0 AS no_fraction,
       ts_3 AS millisecond,
       ts_6 AS microsecond
FROM test_timestamp_precision;

-- ============================================================================
-- TIMESTAMP Truncation and Rounding
-- ============================================================================

CREATE TABLE test_timestamp_truncate (
    id INTEGER PRIMARY KEY,
    original_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_truncate VALUES (1, TIMESTAMP '2024-06-15 14:37:42.123456');

-- Truncate to various units
SELECT id, original_timestamp,
       DATE_TRUNC('microseconds', original_timestamp) AS to_microseconds,
       DATE_TRUNC('milliseconds', original_timestamp) AS to_milliseconds,
       DATE_TRUNC('second', original_timestamp) AS to_second,
       DATE_TRUNC('minute', original_timestamp) AS to_minute,
       DATE_TRUNC('hour', original_timestamp) AS to_hour,
       DATE_TRUNC('day', original_timestamp) AS to_day,
       DATE_TRUNC('week', original_timestamp) AS to_week,
       DATE_TRUNC('month', original_timestamp) AS to_month,
       DATE_TRUNC('quarter', original_timestamp) AS to_quarter,
       DATE_TRUNC('year', original_timestamp) AS to_year
FROM test_timestamp_truncate;

-- ============================================================================
-- TIMESTAMP Aggregation
-- ============================================================================

CREATE TABLE test_timestamp_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    event_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_aggregation VALUES (1, 'Category A', TIMESTAMP '2024-01-15 09:00:00');
INSERT INTO test_timestamp_aggregation VALUES (2, 'Category A', TIMESTAMP '2024-03-20 14:30:00');
INSERT INTO test_timestamp_aggregation VALUES (3, 'Category A', TIMESTAMP '2024-05-10 11:15:00');
INSERT INTO test_timestamp_aggregation VALUES (4, 'Category B', TIMESTAMP '2024-02-14 16:45:00');
INSERT INTO test_timestamp_aggregation VALUES (5, 'Category B', TIMESTAMP '2024-08-30 08:00:00');
INSERT INTO test_timestamp_aggregation VALUES (6, 'Category C', TIMESTAMP '2024-12-25 12:00:00');

-- MIN/MAX timestamps
SELECT category,
       MIN(event_timestamp) AS earliest,
       MAX(event_timestamp) AS latest,
       MAX(event_timestamp) - MIN(event_timestamp) AS time_span
FROM test_timestamp_aggregation
GROUP BY category
ORDER BY category;

-- Count by month
SELECT EXTRACT(YEAR FROM event_timestamp) AS year,
       EXTRACT(MONTH FROM event_timestamp) AS month,
       COUNT(*) AS event_count
FROM test_timestamp_aggregation
GROUP BY EXTRACT(YEAR FROM event_timestamp), EXTRACT(MONTH FROM event_timestamp)
ORDER BY year, month;

-- ============================================================================
-- TIMESTAMP NULL Handling
-- ============================================================================

CREATE TABLE test_timestamp_nulls (
    id INTEGER PRIMARY KEY,
    nullable_timestamp TIMESTAMP,
    description VARCHAR(50)
);

INSERT INTO test_timestamp_nulls VALUES (1, NULL, 'No timestamp set');
INSERT INTO test_timestamp_nulls VALUES (2, TIMESTAMP '2024-01-01 00:00:00', 'Has timestamp');
INSERT INTO test_timestamp_nulls VALUES (3, NULL, 'Another null');
INSERT INTO test_timestamp_nulls VALUES (4, TIMESTAMP '2024-12-31 23:59:59', 'End of year');

SELECT COUNT(*) AS null_count FROM test_timestamp_nulls WHERE nullable_timestamp IS NULL;

SELECT id, COALESCE(nullable_timestamp, TIMESTAMP '1970-01-01 00:00:00') AS ts_with_default
FROM test_timestamp_nulls
ORDER BY id;

-- ============================================================================
-- TIMESTAMP Constraints
-- ============================================================================

CREATE TABLE test_timestamp_constraints (
    id INTEGER PRIMARY KEY,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP,
    deleted_at TIMESTAMP,
    CHECK (updated_at IS NULL OR updated_at >= created_at),
    CHECK (deleted_at IS NULL OR deleted_at >= created_at)
);

INSERT INTO test_timestamp_constraints (id) VALUES (1);
INSERT INTO test_timestamp_constraints (id, created_at, updated_at) VALUES
    (2, TIMESTAMP '2024-01-01 10:00:00', TIMESTAMP '2024-01-15 14:30:00');
INSERT INTO test_timestamp_constraints (id, created_at, deleted_at) VALUES
    (3, TIMESTAMP '2024-02-01 09:00:00', TIMESTAMP '2024-03-01 17:00:00');

-- This should fail
-- INSERT INTO test_timestamp_constraints (id, created_at, updated_at) VALUES
--     (4, TIMESTAMP '2024-01-15 14:30:00', TIMESTAMP '2024-01-01 10:00:00');

SELECT id, created_at, updated_at, deleted_at FROM test_timestamp_constraints ORDER BY id;

-- ============================================================================
-- TIMESTAMP with Business Logic
-- ============================================================================

CREATE TABLE test_timestamp_business (
    id INTEGER PRIMARY KEY,
    order_id INTEGER,
    order_timestamp TIMESTAMP,
    shipped_timestamp TIMESTAMP,
    delivered_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_business VALUES (1, 1001,
    TIMESTAMP '2024-06-01 10:00:00',
    TIMESTAMP '2024-06-02 14:30:00',
    TIMESTAMP '2024-06-05 16:45:00'
);

INSERT INTO test_timestamp_business VALUES (2, 1002,
    TIMESTAMP '2024-06-03 11:15:00',
    TIMESTAMP '2024-06-04 09:00:00',
    NULL  -- Not yet delivered
);

-- Calculate processing times
SELECT id, order_id,
       order_timestamp,
       shipped_timestamp,
       delivered_timestamp,
       shipped_timestamp - order_timestamp AS time_to_ship,
       CASE
           WHEN delivered_timestamp IS NOT NULL
           THEN delivered_timestamp - shipped_timestamp
           ELSE NULL
       END AS time_in_transit,
       CASE
           WHEN delivered_timestamp IS NOT NULL
           THEN delivered_timestamp - order_timestamp
           ELSE CURRENT_TIMESTAMP - order_timestamp
       END AS total_time
FROM test_timestamp_business
ORDER BY id;

-- ============================================================================
-- TIMESTAMP Age and Duration
-- ============================================================================

CREATE TABLE test_timestamp_age (
    id INTEGER PRIMARY KEY,
    event_name VARCHAR(50),
    event_timestamp TIMESTAMP
);

INSERT INTO test_timestamp_age VALUES (1, 'Account Created', TIMESTAMP '2020-01-15 10:00:00');
INSERT INTO test_timestamp_age VALUES (2, 'First Purchase', TIMESTAMP '2022-06-20 14:30:00');
INSERT INTO test_timestamp_age VALUES (3, 'Last Login', TIMESTAMP '2024-05-10 09:15:00');

-- Calculate age from timestamps
SELECT id, event_name, event_timestamp,
       AGE(CURRENT_TIMESTAMP, event_timestamp) AS time_ago,
       EXTRACT(YEAR FROM AGE(CURRENT_TIMESTAMP, event_timestamp)) AS years_ago,
       EXTRACT(MONTH FROM AGE(CURRENT_TIMESTAMP, event_timestamp)) AS months_ago,
       EXTRACT(DAY FROM AGE(CURRENT_TIMESTAMP, event_timestamp)) AS days_ago
FROM test_timestamp_age
ORDER BY event_timestamp;

-- ============================================================================
-- TIMESTAMP Performance with Large Ranges
-- ============================================================================

CREATE TABLE test_timestamp_performance (
    id INTEGER PRIMARY KEY,
    log_timestamp TIMESTAMP,
    log_message VARCHAR(100)
);

-- Insert timestamps spanning years
INSERT INTO test_timestamp_performance VALUES (1, TIMESTAMP '2020-01-01 00:00:00', 'Start of 2020');
INSERT INTO test_timestamp_performance VALUES (2, TIMESTAMP '2021-06-15 12:00:00', 'Mid 2021');
INSERT INTO test_timestamp_performance VALUES (3, TIMESTAMP '2022-12-31 23:59:59', 'End of 2022');
INSERT INTO test_timestamp_performance VALUES (4, TIMESTAMP '2023-09-20 15:30:00', 'Late 2023');
INSERT INTO test_timestamp_performance VALUES (5, TIMESTAMP '2024-03-10 08:45:00', 'Early 2024');

-- Range queries
SELECT id, log_timestamp, log_message FROM test_timestamp_performance
WHERE log_timestamp BETWEEN TIMESTAMP '2021-01-01 00:00:00' AND TIMESTAMP '2023-12-31 23:59:59'
ORDER BY log_timestamp;

-- Create index for performance
CREATE INDEX idx_log_timestamp ON test_timestamp_performance(log_timestamp);

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP INDEX idx_log_timestamp;
DROP TABLE test_timestamp_performance;
DROP TABLE test_timestamp_age;
DROP TABLE test_timestamp_business;
DROP TABLE test_timestamp_constraints;
DROP TABLE test_timestamp_nulls;
DROP TABLE test_timestamp_aggregation;
DROP TABLE test_timestamp_truncate;
DROP TABLE test_timestamp_precision;
DROP TABLE test_timestamp_formatting;
DROP TABLE test_timestamp_tz;
DROP TABLE test_timestamp_comparison;
DROP TABLE test_timestamp_arithmetic;
DROP TABLE test_timestamp_extract;
DROP TABLE test_timestamp_formats;
DROP TABLE test_timestamp_basic;

DROP DATABASE test_temporal_timestamp;
