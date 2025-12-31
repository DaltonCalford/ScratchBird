-- ScratchBird Native Test: Temporal Types - INTERVAL
-- Test ID: datatypes_009
-- Description: Comprehensive testing of INTERVAL type and operations

CREATE DATABASE test_temporal_interval;
\c test_temporal_interval;

-- ============================================================================
-- INTERVAL Basic Testing
-- ============================================================================

CREATE TABLE test_interval_basic (
    id INTEGER PRIMARY KEY,
    duration INTERVAL,
    description VARCHAR(100)
);

-- Simple intervals
INSERT INTO test_interval_basic VALUES (1, INTERVAL '1 day', '1 day');
INSERT INTO test_interval_basic VALUES (2, INTERVAL '1 hour', '1 hour');
INSERT INTO test_interval_basic VALUES (3, INTERVAL '1 minute', '1 minute');
INSERT INTO test_interval_basic VALUES (4, INTERVAL '1 second', '1 second');

-- Multiple units
INSERT INTO test_interval_basic VALUES (10, INTERVAL '1 year', '1 year');
INSERT INTO test_interval_basic VALUES (11, INTERVAL '1 month', '1 month');
INSERT INTO test_interval_basic VALUES (12, INTERVAL '1 week', '1 week');

-- Compound intervals
INSERT INTO test_interval_basic VALUES (20, INTERVAL '1 day 2 hours', '1 day 2 hours');
INSERT INTO test_interval_basic VALUES (21, INTERVAL '3 hours 30 minutes', '3 hours 30 minutes');
INSERT INTO test_interval_basic VALUES (22, INTERVAL '2 minutes 45 seconds', '2 minutes 45 seconds');
INSERT INTO test_interval_basic VALUES (23, INTERVAL '1 year 2 months 3 days', '1 year 2 months 3 days');

-- Fractional intervals
INSERT INTO test_interval_basic VALUES (30, INTERVAL '1.5 days', '1.5 days (36 hours)');
INSERT INTO test_interval_basic VALUES (31, INTERVAL '2.5 hours', '2.5 hours (150 minutes)');
INSERT INTO test_interval_basic VALUES (32, INTERVAL '0.5 seconds', '0.5 seconds (500ms)');

-- Negative intervals
INSERT INTO test_interval_basic VALUES (40, INTERVAL '-1 day', 'Negative 1 day');
INSERT INTO test_interval_basic VALUES (41, INTERVAL '-3 hours', 'Negative 3 hours');
INSERT INTO test_interval_basic VALUES (42, INTERVAL '-30 minutes', 'Negative 30 minutes');

SELECT id, duration, description FROM test_interval_basic ORDER BY id;

-- ============================================================================
-- INTERVAL Input Formats
-- ============================================================================

CREATE TABLE test_interval_formats (
    id INTEGER PRIMARY KEY,
    parsed_interval INTERVAL,
    format_description VARCHAR(50)
);

-- SQL standard format
INSERT INTO test_interval_formats VALUES (1, INTERVAL '1' YEAR, '1 year');
INSERT INTO test_interval_formats VALUES (2, INTERVAL '6' MONTH, '6 months');
INSERT INTO test_interval_formats VALUES (3, INTERVAL '15' DAY, '15 days');
INSERT INTO test_interval_formats VALUES (4, INTERVAL '12' HOUR, '12 hours');
INSERT INTO test_interval_formats VALUES (5, INTERVAL '30' MINUTE, '30 minutes');
INSERT INTO test_interval_formats VALUES (6, INTERVAL '45' SECOND, '45 seconds');

-- PostgreSQL-style format
INSERT INTO test_interval_formats VALUES (10, '1 year'::INTERVAL, 'Cast from string');
INSERT INTO test_interval_formats VALUES (11, '2 months 15 days'::INTERVAL, 'Compound cast');
INSERT INTO test_interval_formats VALUES (12, '12:30:45'::INTERVAL, 'Time-style format');

-- ISO 8601 duration format (if supported)
INSERT INTO test_interval_formats VALUES (20, 'P1Y'::INTERVAL, 'ISO: 1 year');
INSERT INTO test_interval_formats VALUES (21, 'P1M'::INTERVAL, 'ISO: 1 month');
INSERT INTO test_interval_formats VALUES (22, 'P1D'::INTERVAL, 'ISO: 1 day');
INSERT INTO test_interval_formats VALUES (23, 'PT1H'::INTERVAL, 'ISO: 1 hour');
INSERT INTO test_interval_formats VALUES (24, 'PT1M'::INTERVAL, 'ISO: 1 minute');
INSERT INTO test_interval_formats VALUES (25, 'PT1S'::INTERVAL, 'ISO: 1 second');
INSERT INTO test_interval_formats VALUES (26, 'P1Y2M3DT4H5M6S'::INTERVAL, 'ISO: Complex');

SELECT id, parsed_interval, format_description FROM test_interval_formats ORDER BY id;

-- ============================================================================
-- INTERVAL Extraction Functions
-- ============================================================================

CREATE TABLE test_interval_extract (
    id INTEGER PRIMARY KEY,
    test_interval INTERVAL
);

INSERT INTO test_interval_extract VALUES (1, INTERVAL '1 year 2 months 3 days 4 hours 5 minutes 6 seconds');
INSERT INTO test_interval_extract VALUES (2, INTERVAL '90 days');
INSERT INTO test_interval_extract VALUES (3, INTERVAL '36 hours');
INSERT INTO test_interval_extract VALUES (4, INTERVAL '150 minutes');

-- Extract components
SELECT id, test_interval,
       EXTRACT(YEAR FROM test_interval) AS years,
       EXTRACT(MONTH FROM test_interval) AS months,
       EXTRACT(DAY FROM test_interval) AS days,
       EXTRACT(HOUR FROM test_interval) AS hours,
       EXTRACT(MINUTE FROM test_interval) AS minutes,
       EXTRACT(SECOND FROM test_interval) AS seconds
FROM test_interval_extract
ORDER BY id;

-- Extract total seconds (epoch)
SELECT id, test_interval,
       EXTRACT(EPOCH FROM test_interval) AS total_seconds
FROM test_interval_extract
ORDER BY id;

-- ============================================================================
-- INTERVAL Arithmetic
-- ============================================================================

CREATE TABLE test_interval_arithmetic (
    id INTEGER PRIMARY KEY,
    interval1 INTERVAL,
    interval2 INTERVAL
);

INSERT INTO test_interval_arithmetic VALUES (1, INTERVAL '1 day', INTERVAL '12 hours');
INSERT INTO test_interval_arithmetic VALUES (2, INTERVAL '2 hours', INTERVAL '30 minutes');
INSERT INTO test_interval_arithmetic VALUES (3, INTERVAL '1 week', INTERVAL '3 days');

-- Add intervals
SELECT id, interval1, interval2,
       interval1 + interval2 AS sum,
       EXTRACT(EPOCH FROM (interval1 + interval2)) AS sum_seconds
FROM test_interval_arithmetic
ORDER BY id;

-- Subtract intervals
SELECT id, interval1, interval2,
       interval1 - interval2 AS difference,
       EXTRACT(EPOCH FROM (interval1 - interval2)) AS diff_seconds
FROM test_interval_arithmetic
ORDER BY id;

-- Multiply interval by scalar
SELECT id, interval1,
       interval1 * 2 AS doubled,
       interval1 * 0.5 AS halved,
       interval1 * 3 AS tripled
FROM test_interval_arithmetic
ORDER BY id;

-- Divide interval by scalar
SELECT id, interval1,
       interval1 / 2 AS half,
       interval1 / 3 AS third,
       interval1 / 4 AS quarter
FROM test_interval_arithmetic
ORDER BY id;

-- ============================================================================
-- INTERVAL with DATE Arithmetic
-- ============================================================================

CREATE TABLE test_interval_with_date (
    id INTEGER PRIMARY KEY,
    base_date DATE,
    offset INTERVAL
);

INSERT INTO test_interval_with_date VALUES (1, DATE '2024-01-01', INTERVAL '1 month');
INSERT INTO test_interval_with_date VALUES (2, DATE '2024-06-15', INTERVAL '2 weeks');
INSERT INTO test_interval_with_date VALUES (3, DATE '2024-12-31', INTERVAL '-1 year');

-- Add interval to date
SELECT id, base_date, offset,
       base_date + offset AS result_date
FROM test_interval_with_date
ORDER BY id;

-- Subtract interval from date
SELECT id, base_date, offset,
       base_date - offset AS result_date
FROM test_interval_with_date
ORDER BY id;

-- ============================================================================
-- INTERVAL with TIMESTAMP Arithmetic
-- ============================================================================

CREATE TABLE test_interval_with_timestamp (
    id INTEGER PRIMARY KEY,
    base_timestamp TIMESTAMP,
    offset INTERVAL
);

INSERT INTO test_interval_with_timestamp VALUES (1,
    TIMESTAMP '2024-06-15 12:00:00',
    INTERVAL '3 hours 30 minutes'
);

INSERT INTO test_interval_with_timestamp VALUES (2,
    TIMESTAMP '2024-01-01 00:00:00',
    INTERVAL '1 month 2 days 3 hours'
);

INSERT INTO test_interval_with_timestamp VALUES (3,
    TIMESTAMP '2024-12-31 23:59:59',
    INTERVAL '-6 months'
);

-- Add interval to timestamp
SELECT id, base_timestamp, offset,
       base_timestamp + offset AS result_timestamp
FROM test_interval_with_timestamp
ORDER BY id;

-- Subtract interval from timestamp
SELECT id, base_timestamp, offset,
       base_timestamp - offset AS result_timestamp
FROM test_interval_with_timestamp
ORDER BY id;

-- ============================================================================
-- INTERVAL with TIME Arithmetic
-- ============================================================================

CREATE TABLE test_interval_with_time (
    id INTEGER PRIMARY KEY,
    base_time TIME,
    offset INTERVAL
);

INSERT INTO test_interval_with_time VALUES (1, TIME '12:00:00', INTERVAL '2 hours');
INSERT INTO test_interval_with_time VALUES (2, TIME '23:00:00', INTERVAL '2 hours');  -- Wraps to next day
INSERT INTO test_interval_with_time VALUES (3, TIME '08:30:00', INTERVAL '45 minutes');

-- Add interval to time
SELECT id, base_time, offset,
       base_time + offset AS result_time
FROM test_interval_with_time
ORDER BY id;

-- ============================================================================
-- INTERVAL Comparison
-- ============================================================================

CREATE TABLE test_interval_comparison (
    id INTEGER PRIMARY KEY,
    duration INTERVAL,
    description VARCHAR(50)
);

INSERT INTO test_interval_comparison VALUES (1, INTERVAL '1 hour', 'One hour');
INSERT INTO test_interval_comparison VALUES (2, INTERVAL '60 minutes', 'Sixty minutes');
INSERT INTO test_interval_comparison VALUES (3, INTERVAL '3600 seconds', '3600 seconds');
INSERT INTO test_interval_comparison VALUES (4, INTERVAL '2 hours', 'Two hours');
INSERT INTO test_interval_comparison VALUES (5, INTERVAL '30 minutes', 'Half hour');

-- Equality (different representations of same duration)
SELECT i1.id AS id1, i2.id AS id2, i1.description, i2.description
FROM test_interval_comparison i1
JOIN test_interval_comparison i2 ON i1.duration = i2.duration
WHERE i1.id < i2.id;

-- Greater than / Less than
SELECT id, description FROM test_interval_comparison
WHERE duration > INTERVAL '1 hour'
ORDER BY duration;

SELECT id, description FROM test_interval_comparison
WHERE duration < INTERVAL '1 hour'
ORDER BY duration;

-- BETWEEN
SELECT id, description, duration FROM test_interval_comparison
WHERE duration BETWEEN INTERVAL '30 minutes' AND INTERVAL '90 minutes'
ORDER BY duration;

-- ============================================================================
-- INTERVAL Formatting and Output
-- ============================================================================

CREATE TABLE test_interval_formatting (
    id INTEGER PRIMARY KEY,
    duration INTERVAL
);

INSERT INTO test_interval_formatting VALUES (1, INTERVAL '1 year 2 months 3 days');
INSERT INTO test_interval_formatting VALUES (2, INTERVAL '4 hours 30 minutes 15 seconds');
INSERT INTO test_interval_formatting VALUES (3, INTERVAL '90 days');
INSERT INTO test_interval_formatting VALUES (4, INTERVAL '36 hours');

-- Default string representation
SELECT id, duration, duration::VARCHAR AS as_string FROM test_interval_formatting ORDER BY id;

-- Format to specific output
SELECT id, duration,
       TO_CHAR(duration, 'YY years MM months DD days') AS year_month_day,
       TO_CHAR(duration, 'HH24:MI:SS') AS time_format
FROM test_interval_formatting
ORDER BY id;

-- ============================================================================
-- INTERVAL Justification and Normalization
-- ============================================================================

CREATE TABLE test_interval_justify (
    id INTEGER PRIMARY KEY,
    original INTERVAL
);

-- Intervals that can be normalized
INSERT INTO test_interval_justify VALUES (1, INTERVAL '30 days');      -- Can be ~1 month
INSERT INTO test_interval_justify VALUES (2, INTERVAL '25 hours');     -- Can be 1 day 1 hour
INSERT INTO test_interval_justify VALUES (3, INTERVAL '90 seconds');   -- Can be 1 minute 30 seconds
INSERT INTO test_interval_justify VALUES (4, INTERVAL '400 days');     -- Can be ~1 year 1 month

-- Justify (normalize) intervals
SELECT id, original,
       JUSTIFY_DAYS(original) AS days_justified,
       JUSTIFY_HOURS(original) AS hours_justified,
       JUSTIFY_INTERVAL(original) AS fully_justified
FROM test_interval_justify
ORDER BY id;

-- ============================================================================
-- INTERVAL NULL Handling
-- ============================================================================

CREATE TABLE test_interval_nulls (
    id INTEGER PRIMARY KEY,
    nullable_interval INTERVAL,
    description VARCHAR(50)
);

INSERT INTO test_interval_nulls VALUES (1, NULL, 'No duration set');
INSERT INTO test_interval_nulls VALUES (2, INTERVAL '1 day', 'Has duration');
INSERT INTO test_interval_nulls VALUES (3, NULL, 'Another null');
INSERT INTO test_interval_nulls VALUES (4, INTERVAL '3 hours', 'Short duration');

SELECT COUNT(*) AS null_count FROM test_interval_nulls WHERE nullable_interval IS NULL;

SELECT id, COALESCE(nullable_interval, INTERVAL '0') AS interval_with_default
FROM test_interval_nulls
ORDER BY id;

-- NULL in arithmetic
SELECT id, nullable_interval,
       nullable_interval + INTERVAL '1 hour' AS plus_hour,
       nullable_interval * 2 AS doubled
FROM test_interval_nulls;

-- ============================================================================
-- INTERVAL Aggregation
-- ============================================================================

CREATE TABLE test_interval_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    duration INTERVAL
);

INSERT INTO test_interval_aggregation VALUES (1, 'Task A', INTERVAL '2 hours');
INSERT INTO test_interval_aggregation VALUES (2, 'Task A', INTERVAL '1 hour 30 minutes');
INSERT INTO test_interval_aggregation VALUES (3, 'Task A', INTERVAL '45 minutes');
INSERT INTO test_interval_aggregation VALUES (4, 'Task B', INTERVAL '3 hours');
INSERT INTO test_interval_aggregation VALUES (5, 'Task B', INTERVAL '2 hours 15 minutes');
INSERT INTO test_interval_aggregation VALUES (6, 'Task C', INTERVAL '30 minutes');

-- SUM of intervals
SELECT category,
       SUM(duration) AS total_duration,
       EXTRACT(EPOCH FROM SUM(duration)) / 3600 AS total_hours
FROM test_interval_aggregation
GROUP BY category
ORDER BY category;

-- AVG of intervals
SELECT category,
       AVG(duration) AS average_duration
FROM test_interval_aggregation
GROUP BY category
ORDER BY category;

-- MIN/MAX of intervals
SELECT category,
       MIN(duration) AS shortest,
       MAX(duration) AS longest
FROM test_interval_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- INTERVAL Use Cases
-- ============================================================================

-- Age calculation using intervals
CREATE TABLE test_interval_age (
    id INTEGER PRIMARY KEY,
    person_name VARCHAR(50),
    birth_date DATE
);

INSERT INTO test_interval_age VALUES (1, 'Person A', DATE '1990-05-15');
INSERT INTO test_interval_age VALUES (2, 'Person B', DATE '1985-12-20');
INSERT INTO test_interval_age VALUES (3, 'Person C', DATE '2000-03-10');

SELECT id, person_name, birth_date,
       AGE(CURRENT_DATE, birth_date) AS age,
       EXTRACT(YEAR FROM AGE(CURRENT_DATE, birth_date)) AS years,
       EXTRACT(MONTH FROM AGE(CURRENT_DATE, birth_date)) AS months,
       EXTRACT(DAY FROM AGE(CURRENT_DATE, birth_date)) AS days
FROM test_interval_age
ORDER BY birth_date;

-- Duration between timestamps
CREATE TABLE test_interval_duration (
    id INTEGER PRIMARY KEY,
    event_name VARCHAR(50),
    start_time TIMESTAMP,
    end_time TIMESTAMP
);

INSERT INTO test_interval_duration VALUES (1, 'Meeting',
    TIMESTAMP '2024-06-15 10:00:00',
    TIMESTAMP '2024-06-15 11:30:00'
);

INSERT INTO test_interval_duration VALUES (2, 'Project Phase 1',
    TIMESTAMP '2024-01-01 00:00:00',
    TIMESTAMP '2024-03-31 23:59:59'
);

INSERT INTO test_interval_duration VALUES (3, 'Lunch Break',
    TIMESTAMP '2024-06-15 12:00:00',
    TIMESTAMP '2024-06-15 13:00:00'
);

SELECT id, event_name,
       start_time, end_time,
       end_time - start_time AS duration,
       EXTRACT(EPOCH FROM (end_time - start_time)) / 3600 AS hours,
       EXTRACT(EPOCH FROM (end_time - start_time)) / 86400 AS days
FROM test_interval_duration
ORDER BY id;

-- ============================================================================
-- INTERVAL with Business Logic
-- ============================================================================

CREATE TABLE test_interval_business (
    id INTEGER PRIMARY KEY,
    task_name VARCHAR(50),
    estimated_duration INTERVAL,
    actual_duration INTERVAL
);

INSERT INTO test_interval_business VALUES (1, 'Task A', INTERVAL '2 hours', INTERVAL '2 hours 15 minutes');
INSERT INTO test_interval_business VALUES (2, 'Task B', INTERVAL '4 hours', INTERVAL '3 hours 30 minutes');
INSERT INTO test_interval_business VALUES (3, 'Task C', INTERVAL '1 day', INTERVAL '1 day 2 hours');
INSERT INTO test_interval_business VALUES (4, 'Task D', INTERVAL '3 days', INTERVAL '2 days 18 hours');

-- Variance from estimate
SELECT id, task_name,
       estimated_duration,
       actual_duration,
       actual_duration - estimated_duration AS variance,
       CASE
           WHEN actual_duration > estimated_duration THEN 'Over estimate'
           WHEN actual_duration < estimated_duration THEN 'Under estimate'
           ELSE 'On time'
       END AS status
FROM test_interval_business
ORDER BY id;

-- Percentage over/under
SELECT id, task_name,
       estimated_duration,
       actual_duration,
       ((EXTRACT(EPOCH FROM actual_duration) / EXTRACT(EPOCH FROM estimated_duration)) - 1) * 100 AS percent_variance
FROM test_interval_business
ORDER BY id;

-- ============================================================================
-- INTERVAL Ranges and Bounds
-- ============================================================================

CREATE TABLE test_interval_ranges (
    id INTEGER PRIMARY KEY,
    range_name VARCHAR(50),
    min_duration INTERVAL,
    max_duration INTERVAL
);

INSERT INTO test_interval_ranges VALUES (1, 'Short tasks', INTERVAL '0', INTERVAL '30 minutes');
INSERT INTO test_interval_ranges VALUES (2, 'Medium tasks', INTERVAL '30 minutes', INTERVAL '4 hours');
INSERT INTO test_interval_ranges VALUES (3, 'Long tasks', INTERVAL '4 hours', INTERVAL '1 day');
INSERT INTO test_interval_ranges VALUES (4, 'Projects', INTERVAL '1 day', INTERVAL '1 month');

SELECT id, range_name, min_duration, max_duration,
       max_duration - min_duration AS range_span
FROM test_interval_ranges
ORDER BY min_duration;

-- ============================================================================
-- INTERVAL with Date/Time Generation
-- ============================================================================

CREATE TABLE test_interval_generation (
    id INTEGER PRIMARY KEY,
    base_timestamp TIMESTAMP,
    step INTERVAL
);

INSERT INTO test_interval_generation VALUES (1, TIMESTAMP '2024-01-01 00:00:00', INTERVAL '1 day');

-- Generate sequence of timestamps
SELECT base_timestamp + (step * generate_series) AS generated_timestamp
FROM test_interval_generation,
     generate_series(0, 10) AS generate_series
WHERE id = 1;

-- ============================================================================
-- INTERVAL Edge Cases
-- ============================================================================

CREATE TABLE test_interval_edge_cases (
    id INTEGER PRIMARY KEY,
    edge_interval INTERVAL,
    description VARCHAR(100)
);

-- Zero interval
INSERT INTO test_interval_edge_cases VALUES (1, INTERVAL '0', 'Zero duration');

-- Very large intervals
INSERT INTO test_interval_edge_cases VALUES (10, INTERVAL '100 years', 'Century');
INSERT INTO test_interval_edge_cases VALUES (11, INTERVAL '10000 days', 'Many days');
INSERT INTO test_interval_edge_cases VALUES (12, INTERVAL '1000000 seconds', 'Million seconds');

-- Very small intervals
INSERT INTO test_interval_edge_cases VALUES (20, INTERVAL '0.000001 seconds', 'One microsecond');
INSERT INTO test_interval_edge_cases VALUES (21, INTERVAL '0.001 seconds', 'One millisecond');

-- Negative large intervals
INSERT INTO test_interval_edge_cases VALUES (30, INTERVAL '-10 years', 'Negative decade');
INSERT INTO test_interval_edge_cases VALUES (31, INTERVAL '-365 days', 'Negative year');

SELECT id, edge_interval, description,
       EXTRACT(EPOCH FROM edge_interval) AS total_seconds
FROM test_interval_edge_cases
ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_interval_edge_cases;
DROP TABLE test_interval_generation;
DROP TABLE test_interval_ranges;
DROP TABLE test_interval_business;
DROP TABLE test_interval_duration;
DROP TABLE test_interval_age;
DROP TABLE test_interval_aggregation;
DROP TABLE test_interval_nulls;
DROP TABLE test_interval_justify;
DROP TABLE test_interval_formatting;
DROP TABLE test_interval_comparison;
DROP TABLE test_interval_with_time;
DROP TABLE test_interval_with_timestamp;
DROP TABLE test_interval_with_date;
DROP TABLE test_interval_arithmetic;
DROP TABLE test_interval_extract;
DROP TABLE test_interval_formats;
DROP TABLE test_interval_basic;

DROP DATABASE test_temporal_interval;
