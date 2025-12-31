-- ScratchBird Native Test: Temporal Types - TIME
-- Test ID: datatypes_007
-- Description: Comprehensive testing of TIME type and operations

CREATE DATABASE test_temporal_time;
\c test_temporal_time;

-- ============================================================================
-- TIME Basic Testing
-- ============================================================================

CREATE TABLE test_time_basic (
    id INTEGER PRIMARY KEY,
    event_time TIME,
    description VARCHAR(100)
);

-- Current time
INSERT INTO test_time_basic VALUES (1, CURRENT_TIME, 'Now');
INSERT INTO test_time_basic VALUES (2, LOCALTIME, 'Local time');

-- Specific times
INSERT INTO test_time_basic VALUES (10, TIME '00:00:00', 'Midnight');
INSERT INTO test_time_basic VALUES (11, TIME '12:00:00', 'Noon');
INSERT INTO test_time_basic VALUES (12, TIME '23:59:59', 'End of day');
INSERT INTO test_time_basic VALUES (13, TIME '08:30:00', 'Morning work start');
INSERT INTO test_time_basic VALUES (14, TIME '17:00:00', 'Evening work end');

-- Times with fractional seconds
INSERT INTO test_time_basic VALUES (20, TIME '12:34:56.123456', 'Microsecond precision');
INSERT INTO test_time_basic VALUES (21, TIME '23:59:59.999999', 'Almost midnight');
INSERT INTO test_time_basic VALUES (22, TIME '00:00:00.000001', 'Just after midnight');

-- Common times
INSERT INTO test_time_basic VALUES (30, TIME '06:00:00', 'Early morning');
INSERT INTO test_time_basic VALUES (31, TIME '09:00:00', 'Morning');
INSERT INTO test_time_basic VALUES (32, TIME '14:30:00', 'Afternoon');
INSERT INTO test_time_basic VALUES (33, TIME '18:45:00', 'Evening');
INSERT INTO test_time_basic VALUES (34, TIME '22:00:00', 'Night');

SELECT id, event_time, description FROM test_time_basic ORDER BY event_time;

-- ============================================================================
-- TIME Input Formats
-- ============================================================================

CREATE TABLE test_time_formats (
    id INTEGER PRIMARY KEY,
    parsed_time TIME,
    format_description VARCHAR(50)
);

-- Standard format (HH:MM:SS)
INSERT INTO test_time_formats VALUES (1, TIME '14:30:00', 'HH:MM:SS');
INSERT INTO test_time_formats VALUES (2, '14:30:00'::TIME, 'Cast from string');

-- With fractional seconds
INSERT INTO test_time_formats VALUES (10, TIME '14:30:00.5', 'Half second');
INSERT INTO test_time_formats VALUES (11, TIME '14:30:00.123', 'Milliseconds');
INSERT INTO test_time_formats VALUES (12, TIME '14:30:00.123456', 'Microseconds');

-- Abbreviated formats (if supported)
INSERT INTO test_time_formats VALUES (20, TIME '14:30', 'HH:MM (seconds = 0)');
INSERT INTO test_time_formats VALUES (21, TIME '2:30 PM', '12-hour format with AM/PM');
INSERT INTO test_time_formats VALUES (22, TIME '02:30 AM', '12-hour AM format');

SELECT id, parsed_time, format_description FROM test_time_formats ORDER BY id;

-- ============================================================================
-- TIME Extraction Functions
-- ============================================================================

CREATE TABLE test_time_extract (
    id INTEGER PRIMARY KEY,
    test_time TIME
);

INSERT INTO test_time_extract VALUES (1, TIME '14:30:45.123456');
INSERT INTO test_time_extract VALUES (2, TIME '00:00:00');
INSERT INTO test_time_extract VALUES (3, TIME '23:59:59.999999');
INSERT INTO test_time_extract VALUES (4, TIME '12:00:00');

-- Extract components
SELECT id, test_time,
       EXTRACT(HOUR FROM test_time) AS hour,
       EXTRACT(MINUTE FROM test_time) AS minute,
       EXTRACT(SECOND FROM test_time) AS second
FROM test_time_extract
ORDER BY id;

-- Extract milliseconds and microseconds
SELECT id, test_time,
       EXTRACT(MILLISECONDS FROM test_time) AS milliseconds,
       EXTRACT(MICROSECONDS FROM test_time) AS microseconds
FROM test_time_extract
WHERE id = 1;

-- Extract epoch (seconds since midnight)
SELECT id, test_time,
       EXTRACT(EPOCH FROM test_time) AS seconds_since_midnight
FROM test_time_extract
ORDER BY id;

-- ============================================================================
-- TIME Arithmetic
-- ============================================================================

CREATE TABLE test_time_arithmetic (
    id INTEGER PRIMARY KEY,
    base_time TIME
);

INSERT INTO test_time_arithmetic VALUES (1, TIME '12:00:00');
INSERT INTO test_time_arithmetic VALUES (2, TIME '23:00:00');
INSERT INTO test_time_arithmetic VALUES (3, TIME '00:30:00');

-- Add intervals
SELECT id, base_time,
       base_time + INTERVAL '1 hour' AS plus_1_hour,
       base_time + INTERVAL '30 minutes' AS plus_30_min,
       base_time + INTERVAL '45 seconds' AS plus_45_sec
FROM test_time_arithmetic
ORDER BY id;

-- Subtract intervals
SELECT id, base_time,
       base_time - INTERVAL '1 hour' AS minus_1_hour,
       base_time - INTERVAL '30 minutes' AS minus_30_min,
       base_time - INTERVAL '15 seconds' AS minus_15_sec
FROM test_time_arithmetic
ORDER BY id;

-- Time difference
SELECT t1.id AS id1, t2.id AS id2,
       t1.base_time AS time1,
       t2.base_time AS time2,
       t2.base_time - t1.base_time AS time_difference
FROM test_time_arithmetic t1
CROSS JOIN test_time_arithmetic t2
WHERE t1.id < t2.id
ORDER BY t1.id, t2.id;

-- Wrap around midnight
SELECT id, base_time,
       base_time + INTERVAL '2 hours' AS plus_2_hours,
       CASE
           WHEN id = 2 THEN 'Wraps to next day'
           ELSE 'Same day'
       END AS note
FROM test_time_arithmetic
WHERE id = 2;

-- ============================================================================
-- TIME Comparison
-- ============================================================================

CREATE TABLE test_time_comparison (
    id INTEGER PRIMARY KEY,
    event_name VARCHAR(50),
    event_time TIME
);

INSERT INTO test_time_comparison VALUES (1, 'Breakfast', TIME '07:00:00');
INSERT INTO test_time_comparison VALUES (2, 'Morning Meeting', TIME '09:30:00');
INSERT INTO test_time_comparison VALUES (3, 'Lunch', TIME '12:00:00');
INSERT INTO test_time_comparison VALUES (4, 'Afternoon Break', TIME '15:00:00');
INSERT INTO test_time_comparison VALUES (5, 'Dinner', TIME '18:30:00');
INSERT INTO test_time_comparison VALUES (6, 'Bedtime', TIME '22:00:00');

-- Equality
SELECT id, event_name FROM test_time_comparison WHERE event_time = TIME '12:00:00';

-- Greater than / Less than
SELECT id, event_name FROM test_time_comparison WHERE event_time > TIME '15:00:00'
ORDER BY event_time;

SELECT id, event_name FROM test_time_comparison WHERE event_time < TIME '10:00:00'
ORDER BY event_time;

-- BETWEEN
SELECT id, event_name, event_time FROM test_time_comparison
WHERE event_time BETWEEN TIME '09:00:00' AND TIME '17:00:00'
ORDER BY event_time;

-- Time of day categorization
SELECT id, event_name, event_time,
       CASE
           WHEN event_time >= TIME '00:00:00' AND event_time < TIME '06:00:00' THEN 'Night'
           WHEN event_time >= TIME '06:00:00' AND event_time < TIME '12:00:00' THEN 'Morning'
           WHEN event_time >= TIME '12:00:00' AND event_time < TIME '18:00:00' THEN 'Afternoon'
           ELSE 'Evening'
       END AS time_of_day
FROM test_time_comparison
ORDER BY event_time;

-- ============================================================================
-- TIME Formatting and Output
-- ============================================================================

CREATE TABLE test_time_formatting (
    id INTEGER PRIMARY KEY,
    some_time TIME
);

INSERT INTO test_time_formatting VALUES (1, TIME '14:30:45');
INSERT INTO test_time_formatting VALUES (2, TIME '09:05:03');
INSERT INTO test_time_formatting VALUES (3, TIME '23:59:59');

-- Various output formats
SELECT id, some_time,
       TO_CHAR(some_time, 'HH24:MI:SS') AS format_24hr,
       TO_CHAR(some_time, 'HH12:MI:SS AM') AS format_12hr,
       TO_CHAR(some_time, 'HH:MI AM') AS format_12hr_short,
       TO_CHAR(some_time, 'HH24:MI') AS format_24hr_short
FROM test_time_formatting
ORDER BY id;

-- With milliseconds
SELECT id, some_time,
       TO_CHAR(some_time, 'HH24:MI:SS.MS') AS with_milliseconds
FROM test_time_formatting
ORDER BY id;

-- ============================================================================
-- TIME Precision Testing
-- ============================================================================

CREATE TABLE test_time_precision (
    id INTEGER PRIMARY KEY,
    time_0 TIME(0),    -- No fractional seconds
    time_3 TIME(3),    -- Millisecond precision
    time_6 TIME(6)     -- Microsecond precision
);

INSERT INTO test_time_precision VALUES (1,
    TIME '12:34:56.789123',
    TIME '12:34:56.789123',
    TIME '12:34:56.789123'
);

-- Check precision rounding/truncation
SELECT id,
       time_0 AS no_fraction,
       time_3 AS millisecond,
       time_6 AS microsecond
FROM test_time_precision;

-- Precision in arithmetic
SELECT id,
       time_6,
       time_6 + INTERVAL '0.000001 seconds' AS plus_1_microsecond,
       time_6 + INTERVAL '0.001 seconds' AS plus_1_millisecond
FROM test_time_precision;

-- ============================================================================
-- TIME with Timezone (if supported)
-- ============================================================================

CREATE TABLE test_time_with_timezone (
    id INTEGER PRIMARY KEY,
    time_no_tz TIME,
    time_with_tz TIME WITH TIME ZONE
);

INSERT INTO test_time_with_timezone VALUES (1,
    TIME '14:30:00',
    TIME WITH TIME ZONE '14:30:00+00:00'
);

INSERT INTO test_time_with_timezone VALUES (2,
    TIME '14:30:00',
    TIME WITH TIME ZONE '14:30:00-05:00'  -- EST
);

INSERT INTO test_time_with_timezone VALUES (3,
    TIME '14:30:00',
    TIME WITH TIME ZONE '14:30:00+09:00'  -- JST
);

SELECT id, time_no_tz, time_with_tz FROM test_time_with_timezone ORDER BY id;

-- ============================================================================
-- TIME Aggregation
-- ============================================================================

CREATE TABLE test_time_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    event_time TIME
);

INSERT INTO test_time_aggregation VALUES (1, 'Category A', TIME '08:00:00');
INSERT INTO test_time_aggregation VALUES (2, 'Category A', TIME '14:00:00');
INSERT INTO test_time_aggregation VALUES (3, 'Category A', TIME '20:00:00');
INSERT INTO test_time_aggregation VALUES (4, 'Category B', TIME '09:30:00');
INSERT INTO test_time_aggregation VALUES (5, 'Category B', TIME '17:45:00');
INSERT INTO test_time_aggregation VALUES (6, 'Category C', TIME '12:00:00');

-- MIN/MAX times
SELECT category,
       MIN(event_time) AS earliest,
       MAX(event_time) AS latest
FROM test_time_aggregation
GROUP BY category
ORDER BY category;

-- Count by hour of day
SELECT EXTRACT(HOUR FROM event_time) AS hour,
       COUNT(*) AS event_count
FROM test_time_aggregation
GROUP BY EXTRACT(HOUR FROM event_time)
ORDER BY hour;

-- ============================================================================
-- TIME NULL Handling
-- ============================================================================

CREATE TABLE test_time_nulls (
    id INTEGER PRIMARY KEY,
    nullable_time TIME,
    description VARCHAR(50)
);

INSERT INTO test_time_nulls VALUES (1, NULL, 'No time set');
INSERT INTO test_time_nulls VALUES (2, TIME '10:30:00', 'Has time');
INSERT INTO test_time_nulls VALUES (3, NULL, 'Another null');
INSERT INTO test_time_nulls VALUES (4, TIME '16:45:00', 'Afternoon time');

SELECT COUNT(*) AS null_time_count FROM test_time_nulls WHERE nullable_time IS NULL;

SELECT id, COALESCE(nullable_time, TIME '00:00:00') AS time_with_default
FROM test_time_nulls
ORDER BY id;

-- NULL in arithmetic
SELECT id, nullable_time, nullable_time + INTERVAL '1 hour' AS hour_later FROM test_time_nulls;

-- ============================================================================
-- TIME Sorting and Indexing
-- ============================================================================

CREATE TABLE test_time_sorting (
    id INTEGER PRIMARY KEY,
    event_time TIME,
    event_name VARCHAR(50)
);

INSERT INTO test_time_sorting VALUES (5, TIME '20:00:00', 'Event E');
INSERT INTO test_time_sorting VALUES (2, TIME '08:30:00', 'Event B');
INSERT INTO test_time_sorting VALUES (4, TIME '16:15:00', 'Event D');
INSERT INTO test_time_sorting VALUES (1, TIME '06:00:00', 'Event A');
INSERT INTO test_time_sorting VALUES (3, TIME '12:45:00', 'Event C');

-- Ascending sort
SELECT id, event_time, event_name FROM test_time_sorting ORDER BY event_time ASC;

-- Descending sort
SELECT id, event_time, event_name FROM test_time_sorting ORDER BY event_time DESC;

-- Create index on time column
CREATE INDEX idx_event_time ON test_time_sorting(event_time);

-- Query using index
SELECT id, event_name FROM test_time_sorting
WHERE event_time BETWEEN TIME '08:00:00' AND TIME '17:00:00'
ORDER BY event_time;

-- ============================================================================
-- TIME Constraints
-- ============================================================================

CREATE TABLE test_time_constraints (
    id INTEGER PRIMARY KEY,
    start_time TIME NOT NULL,
    end_time TIME,
    CHECK (end_time IS NULL OR end_time > start_time)  -- End must be after start
);

INSERT INTO test_time_constraints VALUES (1, TIME '09:00:00', TIME '17:00:00');
INSERT INTO test_time_constraints VALUES (2, TIME '14:00:00', TIME '15:30:00');
INSERT INTO test_time_constraints VALUES (3, TIME '08:00:00', NULL);  -- Open-ended

-- This should fail the CHECK constraint
-- INSERT INTO test_time_constraints VALUES (4, TIME '17:00:00', TIME '09:00:00');

SELECT id, start_time, end_time,
       CASE
           WHEN end_time IS NOT NULL THEN end_time - start_time
           ELSE NULL
       END AS duration
FROM test_time_constraints
ORDER BY id;

-- ============================================================================
-- TIME with Business Logic
-- ============================================================================

CREATE TABLE test_time_business (
    id INTEGER PRIMARY KEY,
    employee_id INTEGER,
    clock_in TIME,
    clock_out TIME
);

INSERT INTO test_time_business VALUES (1, 101, TIME '08:00:00', TIME '17:00:00');
INSERT INTO test_time_business VALUES (2, 102, TIME '09:30:00', TIME '18:30:00');
INSERT INTO test_time_business VALUES (3, 103, TIME '07:45:00', TIME '16:00:00');
INSERT INTO test_time_business VALUES (4, 104, TIME '10:00:00', TIME '19:00:00');

-- Calculate work hours
SELECT id, employee_id, clock_in, clock_out,
       clock_out - clock_in AS hours_worked,
       EXTRACT(HOUR FROM (clock_out - clock_in)) AS whole_hours,
       EXTRACT(MINUTE FROM (clock_out - clock_in)) AS extra_minutes
FROM test_time_business
ORDER BY id;

-- Overtime calculation (over 8 hours)
SELECT id, employee_id,
       clock_out - clock_in AS total_time,
       CASE
           WHEN EXTRACT(EPOCH FROM (clock_out - clock_in)) > 8 * 3600
           THEN (clock_out - clock_in) - INTERVAL '8 hours'
           ELSE INTERVAL '0'
       END AS overtime
FROM test_time_business
ORDER BY id;

-- ============================================================================
-- TIME Special Cases
-- ============================================================================

CREATE TABLE test_time_special (
    id INTEGER PRIMARY KEY,
    test_time TIME,
    description VARCHAR(100)
);

-- Boundary times
INSERT INTO test_time_special VALUES (1, TIME '00:00:00.000000', 'Absolute start');
INSERT INTO test_time_special VALUES (2, TIME '23:59:59.999999', 'Almost end');

-- Common special times
INSERT INTO test_time_special VALUES (10, TIME '00:00:00', 'Midnight');
INSERT INTO test_time_special VALUES (11, TIME '12:00:00', 'Noon');
INSERT INTO test_time_special VALUES (12, TIME '06:00:00', 'Dawn');
INSERT INTO test_time_special VALUES (13, TIME '18:00:00', 'Dusk');

-- Fractional second boundaries
INSERT INTO test_time_special VALUES (20, TIME '12:34:56.000001', 'One microsecond');
INSERT INTO test_time_special VALUES (21, TIME '12:34:56.001000', 'One millisecond');
INSERT INTO test_time_special VALUES (22, TIME '12:34:56.100000', 'One tenth second');
INSERT INTO test_time_special VALUES (23, TIME '12:34:56.500000', 'Half second');

SELECT id, test_time, description FROM test_time_special ORDER BY test_time;

-- ============================================================================
-- TIME Duration Calculations
-- ============================================================================

CREATE TABLE test_time_duration (
    id INTEGER PRIMARY KEY,
    task_name VARCHAR(50),
    start_time TIME,
    end_time TIME
);

INSERT INTO test_time_duration VALUES (1, 'Task A', TIME '09:00:00', TIME '09:45:00');
INSERT INTO test_time_duration VALUES (2, 'Task B', TIME '10:00:00', TIME '11:30:00');
INSERT INTO test_time_duration VALUES (3, 'Task C', TIME '14:00:00', TIME '14:15:00');
INSERT INTO test_time_duration VALUES (4, 'Task D', TIME '15:30:00', TIME '17:45:00');

-- Duration in different units
SELECT id, task_name, start_time, end_time,
       end_time - start_time AS duration_interval,
       EXTRACT(EPOCH FROM (end_time - start_time)) AS duration_seconds,
       EXTRACT(EPOCH FROM (end_time - start_time)) / 60 AS duration_minutes,
       EXTRACT(EPOCH FROM (end_time - start_time)) / 3600 AS duration_hours
FROM test_time_duration
ORDER BY id;

-- Total duration
SELECT SUM(EXTRACT(EPOCH FROM (end_time - start_time))) / 3600 AS total_hours
FROM test_time_duration;

-- ============================================================================
-- TIME Conversion Functions
-- ============================================================================

CREATE TABLE test_time_conversion (
    id INTEGER PRIMARY KEY,
    time_value TIME
);

INSERT INTO test_time_conversion VALUES (1, TIME '14:30:45');

-- Convert to different formats
SELECT id, time_value,
       time_value::VARCHAR AS as_string,
       EXTRACT(EPOCH FROM time_value) AS as_epoch_seconds,
       CAST(time_value AS TIME(0)) AS no_fractional_seconds
FROM test_time_conversion;

-- Build time from components
SELECT id,
       MAKE_TIME(14, 30, 45) AS constructed_time,
       MAKE_TIME(EXTRACT(HOUR FROM time_value)::INTEGER,
                 EXTRACT(MINUTE FROM time_value)::INTEGER,
                 EXTRACT(SECOND FROM time_value)) AS reconstructed
FROM test_time_conversion;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_time_conversion;
DROP TABLE test_time_duration;
DROP TABLE test_time_special;
DROP TABLE test_time_business;
DROP TABLE test_time_constraints;
DROP INDEX idx_event_time;
DROP TABLE test_time_sorting;
DROP TABLE test_time_nulls;
DROP TABLE test_time_aggregation;
DROP TABLE test_time_with_timezone;
DROP TABLE test_time_precision;
DROP TABLE test_time_formatting;
DROP TABLE test_time_comparison;
DROP TABLE test_time_arithmetic;
DROP TABLE test_time_extract;
DROP TABLE test_time_formats;
DROP TABLE test_time_basic;

DROP DATABASE test_temporal_time;
