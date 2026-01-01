-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - TSRANGE (Timestamp Range)
-- Description: Comprehensive timestamp range type testing
-- ============================================================================

-- TSRANGE represents ranges of TIMESTAMP (without timezone) values
-- Used for: time periods, event scheduling, validity periods, temporal data

-- Create test database
CREATE DATABASE test_tsrange_db;
USE test_tsrange_db;

-- ============================================================================
-- Section 1: Basic TSRANGE Creation
-- ============================================================================

CREATE TABLE test_tsrange_basic (
    id INT PRIMARY KEY,
    period TSRANGE,
    description VARCHAR(200)
);

-- Various time periods
INSERT INTO test_tsrange_basic VALUES
    (1, '[2024-01-01 00:00:00, 2024-01-02 00:00:00)', 'One day period'),
    (2, '[2024-01-15 09:00:00, 2024-01-15 17:00:00)', 'Business hours'),
    (3, '[2024-03-01 00:00:00, 2024-06-01 00:00:00)', 'Q2 2024'),
    (4, '[2024-01-01 00:00:00, 2024-12-31 23:59:59]', 'Year 2024 inclusive'),
    (5, '[2024-06-15 14:30:00,)', 'From specific time onwards'),
    (6, '(,2024-12-31 23:59:59]', 'Up to end of 2024'),
    (7, 'empty', 'Empty period'),
    (8, '[2024-01-01 00:00:00, 2024-01-01 00:00:00)', 'Empty (same bounds exclusive)'),
    (9, '[2024-01-01 00:00:00, 2024-01-01 00:00:00]', 'Single instant');

SELECT id, period, description FROM test_tsrange_basic ORDER BY id;

-- ============================================================================
-- Section 2: Range Properties
-- ============================================================================

CREATE TABLE test_tsrange_properties (
    id INT PRIMARY KEY,
    event_period TSRANGE
);

INSERT INTO test_tsrange_properties VALUES
    (1, '[2024-01-15 10:00:00, 2024-01-15 12:00:00)'),
    (2, '[2024-02-01 00:00:00,)'),
    (3, 'empty');

-- Extract bounds
SELECT id,
       lower(event_period) AS start_time,
       upper(event_period) AS end_time
FROM test_tsrange_properties ORDER BY id;

-- Check bound inclusivity
SELECT id,
       lower_inc(event_period) AS start_inclusive,
       upper_inc(event_period) AS end_inclusive
FROM test_tsrange_properties ORDER BY id;

-- Check for infinite bounds
SELECT id,
       lower_inf(event_period) AS starts_infinite,
       upper_inf(event_period) AS ends_infinite
FROM test_tsrange_properties ORDER BY id;

-- Check if empty
SELECT id, isempty(event_period) AS is_empty FROM test_tsrange_properties ORDER BY id;

-- ============================================================================
-- Section 3: Containment - Does Period Contain Timestamp?
-- ============================================================================

CREATE TABLE test_tsrange_containment (
    shift_id INT PRIMARY KEY,
    shift_name VARCHAR(100),
    shift_period TSRANGE
);

INSERT INTO test_tsrange_containment VALUES
    (1, 'Morning Shift', '[2024-01-15 06:00:00, 2024-01-15 14:00:00)'),
    (2, 'Afternoon Shift', '[2024-01-15 14:00:00, 2024-01-15 22:00:00)'),
    (3, 'Night Shift', '[2024-01-15 22:00:00, 2024-01-16 06:00:00)');

-- Check which shift contains specific timestamp
SELECT shift_name FROM test_tsrange_containment
WHERE shift_period @> '2024-01-15 10:30:00'::TIMESTAMP;

SELECT shift_name FROM test_tsrange_containment
WHERE shift_period @> '2024-01-15 16:45:00'::TIMESTAMP;

SELECT shift_name FROM test_tsrange_containment
WHERE shift_period @> '2024-01-16 02:00:00'::TIMESTAMP;

-- Boundary case: 14:00:00 is in afternoon (not morning due to exclusive upper)
SELECT shift_name FROM test_tsrange_containment
WHERE shift_period @> '2024-01-15 14:00:00'::TIMESTAMP;

-- Range contains range
SELECT shift_id FROM test_tsrange_containment
WHERE shift_period @> '[2024-01-15 07:00:00, 2024-01-15 09:00:00)'::TSRANGE
ORDER BY shift_id;

-- ============================================================================
-- Section 4: Overlap Detection
-- ============================================================================

CREATE TABLE test_tsrange_overlap (
    booking_id INT PRIMARY KEY,
    resource_name VARCHAR(100),
    reserved_period TSRANGE
);

INSERT INTO test_tsrange_overlap VALUES
    (1, 'Conference Room A', '[2024-01-15 09:00:00, 2024-01-15 11:00:00)'),
    (2, 'Conference Room A', '[2024-01-15 10:00:00, 2024-01-15 12:00:00)'),
    (3, 'Conference Room A', '[2024-01-15 13:00:00, 2024-01-15 15:00:00)'),
    (4, 'Conference Room A', '[2024-01-15 11:00:00, 2024-01-15 13:00:00)');

-- Find overlapping bookings
SELECT b1.booking_id AS booking1,
       b2.booking_id AS booking2,
       b1.reserved_period AS period1,
       b2.reserved_period AS period2
FROM test_tsrange_overlap b1
JOIN test_tsrange_overlap b2 ON b1.booking_id < b2.booking_id
WHERE b1.reserved_period && b2.reserved_period
ORDER BY b1.booking_id, b2.booking_id;

-- Check if new booking overlaps with existing
SELECT booking_id FROM test_tsrange_overlap
WHERE reserved_period && '[2024-01-15 10:30:00, 2024-01-15 11:30:00)'::TSRANGE
ORDER BY booking_id;

-- ============================================================================
-- Section 5: Adjacency - Consecutive Periods
-- ============================================================================

CREATE TABLE test_tsrange_adjacency (
    slot_id INT PRIMARY KEY,
    time_slot TSRANGE
);

INSERT INTO test_tsrange_adjacency VALUES
    (1, '[2024-01-15 09:00:00, 2024-01-15 10:00:00)'),
    (2, '[2024-01-15 10:00:00, 2024-01-15 11:00:00)'),
    (3, '[2024-01-15 11:00:00, 2024-01-15 12:00:00)'),
    (4, '[2024-01-15 14:00:00, 2024-01-15 15:00:00)');

-- Find adjacent time slots
SELECT s1.slot_id AS slot1,
       s2.slot_id AS slot2,
       s1.time_slot -|- s2.time_slot AS are_adjacent
FROM test_tsrange_adjacency s1
CROSS JOIN test_tsrange_adjacency s2
WHERE s1.slot_id < s2.slot_id
ORDER BY s1.slot_id, s2.slot_id;

-- ============================================================================
-- Section 6: Range Operations - Union, Intersection, Difference
-- ============================================================================

CREATE TABLE test_tsrange_operations (
    id INT PRIMARY KEY,
    period1 TSRANGE,
    period2 TSRANGE
);

INSERT INTO test_tsrange_operations VALUES
    (1, '[2024-01-01 10:00:00, 2024-01-01 15:00:00)',
        '[2024-01-01 12:00:00, 2024-01-01 17:00:00)'),
    (2, '[2024-01-01 09:00:00, 2024-01-01 12:00:00)',
        '[2024-01-01 12:00:00, 2024-01-01 15:00:00)'),
    (3, '[2024-01-01 10:00:00, 2024-01-01 12:00:00)',
        '[2024-01-01 14:00:00, 2024-01-01 16:00:00)');

-- Union (overlapping or adjacent)
SELECT id, period1 + period2 AS union_period
FROM test_tsrange_operations
WHERE id IN (1, 2)
ORDER BY id;

-- Intersection
SELECT id, period1 * period2 AS intersection
FROM test_tsrange_operations ORDER BY id;

-- Difference
SELECT id, period1 - period2 AS difference
FROM test_tsrange_operations ORDER BY id;

-- Range merge
SELECT id, range_merge(period1, period2) AS merged
FROM test_tsrange_operations ORDER BY id;

-- ============================================================================
-- Section 7: GiST Indexing
-- ============================================================================

CREATE TABLE test_tsrange_indexing (
    event_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_period TSRANGE
);

CREATE INDEX idx_event_period ON test_tsrange_indexing USING GIST (event_period);

-- Insert sample events
INSERT INTO test_tsrange_indexing
SELECT i,
       'Event ' || i,
       tsrange(
           '2024-01-01 00:00:00'::TIMESTAMP + (i || ' hours')::INTERVAL,
           '2024-01-01 00:00:00'::TIMESTAMP + ((i + 2) || ' hours')::INTERVAL
       )
FROM generate_series(1, 1000) i;

-- Queries using GiST index
SELECT event_id, event_name FROM test_tsrange_indexing
WHERE event_period @> '2024-01-01 10:30:00'::TIMESTAMP
ORDER BY event_id LIMIT 5;

SELECT event_id, event_name FROM test_tsrange_indexing
WHERE event_period && '[2024-01-01 20:00:00, 2024-01-01 22:00:00)'::TSRANGE
ORDER BY event_id LIMIT 5;

-- ============================================================================
-- Section 8: Real-world Use Case - Event Scheduling
-- ============================================================================

CREATE TABLE test_tsrange_events (
    event_id INT PRIMARY KEY,
    event_title VARCHAR(200),
    event_time TSRANGE,
    organizer VARCHAR(100)
);

CREATE INDEX idx_event_time ON test_tsrange_events USING GIST (event_time);

INSERT INTO test_tsrange_events VALUES
    (1, 'Team Standup', '[2024-01-15 09:00:00, 2024-01-15 09:30:00)', 'Alice'),
    (2, 'Client Meeting', '[2024-01-15 10:00:00, 2024-01-15 11:30:00)', 'Bob'),
    (3, 'Lunch Break', '[2024-01-15 12:00:00, 2024-01-15 13:00:00)', 'All'),
    (4, 'Development Sprint', '[2024-01-15 14:00:00, 2024-01-15 17:00:00)', 'Charlie'),
    (5, 'Code Review', '[2024-01-15 15:00:00, 2024-01-15 16:00:00)', 'Diana');

-- Find all events happening at specific time
SELECT event_title, organizer FROM test_tsrange_events
WHERE event_time @> '2024-01-15 15:30:00'::TIMESTAMP
ORDER BY event_id;

-- Find events in a time window
SELECT event_title FROM test_tsrange_events
WHERE event_time && '[2024-01-15 09:00:00, 2024-01-15 12:00:00)'::TSRANGE
ORDER BY event_id;

-- Find free time slots (no events)
WITH all_slots AS (
    SELECT tsrange(
        '2024-01-15 09:00:00'::TIMESTAMP + (i || ' hours')::INTERVAL,
        '2024-01-15 09:00:00'::TIMESTAMP + ((i + 1) || ' hours')::INTERVAL
    ) AS slot
    FROM generate_series(0, 8) i
)
SELECT slot FROM all_slots
WHERE NOT EXISTS (
    SELECT 1 FROM test_tsrange_events
    WHERE event_time && slot
)
ORDER BY slot;

-- ============================================================================
-- Section 9: Real-world Use Case - Employment History
-- ============================================================================

CREATE TABLE test_tsrange_employment (
    employment_id INT PRIMARY KEY,
    employee_name VARCHAR(100),
    company_name VARCHAR(200),
    employment_period TSRANGE
);

INSERT INTO test_tsrange_employment VALUES
    (1, 'John Doe', 'TechCorp', '[2020-01-15 00:00:00, 2022-06-30 23:59:59]'),
    (2, 'John Doe', 'StartupInc', '[2022-07-01 00:00:00, 2023-12-31 23:59:59]'),
    (3, 'John Doe', 'BigCompany', '[2024-01-01 00:00:00,)'),
    (4, 'Jane Smith', 'Agency123', '[2019-03-01 00:00:00, 2021-08-15 23:59:59]'),
    (5, 'Jane Smith', 'Freelance', '[2021-09-01 00:00:00,)');

-- Find current employer
SELECT employee_name, company_name FROM test_tsrange_employment
WHERE employment_period @> CURRENT_TIMESTAMP
ORDER BY employee_name;

-- Find who worked at specific time
SELECT employee_name, company_name FROM test_tsrange_employment
WHERE employment_period @> '2022-08-01 00:00:00'::TIMESTAMP
ORDER BY employee_name;

-- Find employment gaps
SELECT e1.employee_name,
       upper(e1.employment_period) AS left_company,
       lower(e2.employment_period) AS started_next,
       lower(e2.employment_period) - upper(e1.employment_period) AS gap_duration
FROM test_tsrange_employment e1
JOIN test_tsrange_employment e2
    ON e1.employee_name = e2.employee_name
    AND upper(e1.employment_period) < lower(e2.employment_period)
    AND NOT EXISTS (
        SELECT 1 FROM test_tsrange_employment e3
        WHERE e3.employee_name = e1.employee_name
          AND lower(e3.employment_period) > upper(e1.employment_period)
          AND lower(e3.employment_period) < lower(e2.employment_period)
    )
ORDER BY e1.employee_name, e1.employment_id;

-- ============================================================================
-- Section 10: Real-world Use Case - Price Validity
-- ============================================================================

CREATE TABLE test_tsrange_pricing (
    price_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    price DECIMAL(10, 2),
    valid_period TSRANGE
);

INSERT INTO test_tsrange_pricing VALUES
    (1, 'Widget A', 99.99, '[2024-01-01 00:00:00, 2024-03-31 23:59:59]'),
    (2, 'Widget A', 89.99, '[2024-04-01 00:00:00, 2024-06-30 23:59:59]'),
    (3, 'Widget A', 79.99, '[2024-07-01 00:00:00,)'),
    (4, 'Widget B', 149.99, '[2024-01-01 00:00:00,)');

-- Get current price
SELECT product_name, price FROM test_tsrange_pricing
WHERE product_name = 'Widget A'
  AND valid_period @> CURRENT_TIMESTAMP;

-- Get price at specific date
SELECT product_name, price FROM test_tsrange_pricing
WHERE product_name = 'Widget A'
  AND valid_period @> '2024-05-15 12:00:00'::TIMESTAMP;

-- Price history
SELECT product_name, price, valid_period FROM test_tsrange_pricing
WHERE product_name = 'Widget A'
ORDER BY lower(valid_period);

-- ============================================================================
-- Section 11: Real-world Use Case - System Maintenance Windows
-- ============================================================================

CREATE TABLE test_tsrange_maintenance (
    window_id INT PRIMARY KEY,
    system_name VARCHAR(100),
    maintenance_period TSRANGE,
    description VARCHAR(200)
);

INSERT INTO test_tsrange_maintenance VALUES
    (1, 'Database Server', '[2024-01-15 02:00:00, 2024-01-15 04:00:00)', 'Backup'),
    (2, 'Web Server', '[2024-01-15 03:00:00, 2024-01-15 05:00:00)', 'Security patches'),
    (3, 'Database Server', '[2024-01-22 02:00:00, 2024-01-22 04:00:00)', 'Backup'),
    (4, 'API Gateway', '[2024-01-15 01:00:00, 2024-01-15 02:00:00)', 'Config update');

-- Check if system is under maintenance at specific time
SELECT system_name, description FROM test_tsrange_maintenance
WHERE maintenance_period @> '2024-01-15 03:30:00'::TIMESTAMP
ORDER BY system_name;

-- Find all maintenance windows in date range
SELECT system_name, maintenance_period, description
FROM test_tsrange_maintenance
WHERE maintenance_period && '[2024-01-15 00:00:00, 2024-01-16 00:00:00)'::TSRANGE
ORDER BY lower(maintenance_period);

-- ============================================================================
-- Section 12: Duration Calculations
-- ============================================================================

CREATE TABLE test_tsrange_durations (
    session_id INT PRIMARY KEY,
    user_name VARCHAR(100),
    session_period TSRANGE
);

INSERT INTO test_tsrange_durations VALUES
    (1, 'Alice', '[2024-01-15 09:00:00, 2024-01-15 17:30:00)'),
    (2, 'Bob', '[2024-01-15 10:00:00, 2024-01-15 14:00:00)'),
    (3, 'Charlie', '[2024-01-15 08:30:00,)');  -- Still active

-- Calculate session duration
SELECT session_id, user_name,
       CASE
           WHEN upper_inf(session_period) THEN
               CURRENT_TIMESTAMP - lower(session_period)
           ELSE
               upper(session_period) - lower(session_period)
       END AS duration
FROM test_tsrange_durations
ORDER BY session_id;

-- Total time
SELECT user_name,
       SUM(upper(session_period) - lower(session_period)) AS total_time
FROM test_tsrange_durations
WHERE NOT upper_inf(session_period)
GROUP BY user_name
ORDER BY user_name;

-- ============================================================================
-- Section 13: Temporal Queries with CURRENT_TIMESTAMP
-- ============================================================================

CREATE TABLE test_tsrange_current (
    subscription_id INT PRIMARY KEY,
    user_name VARCHAR(100),
    subscription_period TSRANGE
);

INSERT INTO test_tsrange_current VALUES
    (1, 'Alice', '[2023-01-01 00:00:00, 2024-01-01 00:00:00)'),  -- Expired
    (2, 'Bob', '[2024-01-01 00:00:00, 2025-01-01 00:00:00)'),    -- Active
    (3, 'Charlie', '[2024-06-01 00:00:00,)'),                    -- Active indefinitely
    (4, 'Diana', '[2025-01-01 00:00:00, 2026-01-01 00:00:00)'); -- Future

-- Find currently active subscriptions
SELECT user_name FROM test_tsrange_current
WHERE subscription_period @> CURRENT_TIMESTAMP
ORDER BY user_name;

-- Find expired subscriptions
SELECT user_name FROM test_tsrange_current
WHERE upper(subscription_period) < CURRENT_TIMESTAMP
ORDER BY user_name;

-- Find future subscriptions
SELECT user_name FROM test_tsrange_current
WHERE lower(subscription_period) > CURRENT_TIMESTAMP
ORDER BY user_name;

-- ============================================================================
-- Section 14: Multirange - Non-contiguous Periods
-- ============================================================================

CREATE TABLE test_tsrange_multi (
    availability_id INT PRIMARY KEY,
    doctor_name VARCHAR(100),
    available_periods TSMULTIRANGE
);

INSERT INTO test_tsrange_multi VALUES
    (1, 'Dr. Smith', '{[2024-01-15 09:00:00, 2024-01-15 12:00:00),
                        [2024-01-15 14:00:00, 2024-01-15 17:00:00)}'),
    (2, 'Dr. Jones', '{[2024-01-15 08:00:00, 2024-01-15 16:00:00)}');

SELECT availability_id, doctor_name, available_periods
FROM test_tsrange_multi ORDER BY availability_id;

-- Check if doctor is available at specific time
SELECT doctor_name FROM test_tsrange_multi
WHERE available_periods @> '2024-01-15 13:00:00'::TIMESTAMP
ORDER BY doctor_name;

SELECT doctor_name FROM test_tsrange_multi
WHERE available_periods @> '2024-01-15 10:00:00'::TIMESTAMP
ORDER BY doctor_name;

-- ============================================================================
-- Section 15: NULL Handling
-- ============================================================================

CREATE TABLE test_tsrange_nulls (
    id INT PRIMARY KEY,
    period TSRANGE
);

INSERT INTO test_tsrange_nulls VALUES
    (1, '[2024-01-01 00:00:00, 2024-12-31 23:59:59]'),
    (2, NULL),
    (3, 'empty');

SELECT id, period,
       period IS NULL AS is_null,
       isempty(period) AS is_empty
FROM test_tsrange_nulls ORDER BY id;

-- Operations with NULL
SELECT id, lower(period) AS start_time FROM test_tsrange_nulls ORDER BY id;
SELECT id, period @> '2024-06-15 12:00:00'::TIMESTAMP AS contains FROM test_tsrange_nulls ORDER BY id;

-- ============================================================================
-- Section 16: Comparison and Sorting
-- ============================================================================

CREATE TABLE test_tsrange_sorting (
    id INT PRIMARY KEY,
    period TSRANGE
);

INSERT INTO test_tsrange_sorting VALUES
    (1, '[2024-06-01 00:00:00, 2024-07-01 00:00:00)'),
    (2, '[2024-01-01 00:00:00, 2024-02-01 00:00:00)'),
    (3, '[2024-01-01 00:00:00, 2024-03-01 00:00:00)'),
    (4, '[2024-03-01 00:00:00, 2024-04-01 00:00:00)');

-- Sort by range (lower bound first, then upper bound)
SELECT id, period FROM test_tsrange_sorting ORDER BY period;

-- Equality
SELECT id FROM test_tsrange_sorting
WHERE period = '[2024-01-01 00:00:00, 2024-02-01 00:00:00)'::TSRANGE;

-- ============================================================================
-- Section 17: Exclusion Constraints
-- ============================================================================

CREATE TABLE test_tsrange_exclusion (
    booking_id INT PRIMARY KEY,
    room_name VARCHAR(100),
    booking_period TSRANGE,
    EXCLUDE USING GIST (room_name WITH =, booking_period WITH &&)
);

-- First booking succeeds
INSERT INTO test_tsrange_exclusion VALUES
    (1, 'Room A', '[2024-01-15 09:00:00, 2024-01-15 11:00:00)');

-- Second booking in different room succeeds
INSERT INTO test_tsrange_exclusion VALUES
    (2, 'Room B', '[2024-01-15 09:00:00, 2024-01-15 11:00:00)');

-- Adjacent booking in same room succeeds
INSERT INTO test_tsrange_exclusion VALUES
    (3, 'Room A', '[2024-01-15 11:00:00, 2024-01-15 13:00:00)');

-- This would fail: overlapping period in same room
-- INSERT INTO test_tsrange_exclusion VALUES
--     (4, 'Room A', '[2024-01-15 10:00:00, 2024-01-15 12:00:00)');

SELECT * FROM test_tsrange_exclusion ORDER BY room_name, booking_period;

-- ============================================================================
-- Section 18: Best Practices
-- ============================================================================

CREATE TABLE test_tsrange_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_tsrange_best_practices VALUES
    (1, 'Use TSRANGE for event scheduling, validity periods, employment history'),
    (2, 'Create GiST indexes for temporal queries (@>, &&, -|-)'),
    (3, 'Use EXCLUDE constraints to prevent overlapping bookings/reservations'),
    (4, 'Default [lower, upper) for consistency: inclusive start, exclusive end'),
    (5, 'Use @> with CURRENT_TIMESTAMP for "currently active" queries'),
    (6, 'Unbounded ranges useful for "from date onwards" (e.g., current employment)'),
    (7, 'Calculate duration: upper(range) - lower(range)'),
    (8, 'Use && to find overlapping periods (conflicts, double bookings)'),
    (9, 'Use -|- to find adjacent periods (consecutive shifts, appointments)'),
    (10, 'Empty ranges represent "no valid time period" (different from NULL)'),
    (11, 'TSMULTIRANGE for non-contiguous availability (office hours with breaks)'),
    (12, 'Consider timezone implications: use TSTZRANGE if timezone matters'),
    (13, 'Exclusion constraints enforce temporal integrity at database level'),
    (14, 'Range operations are more efficient than multiple timestamp comparisons'),
    (15, 'TSRANGE ideal for temporal databases, bi-temporal data, SCD Type 2');

SELECT id, guideline FROM test_tsrange_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_tsrange_best_practices;
DROP TABLE test_tsrange_exclusion;
DROP TABLE test_tsrange_sorting;
DROP TABLE test_tsrange_nulls;
DROP TABLE test_tsrange_multi;
DROP TABLE test_tsrange_current;
DROP TABLE test_tsrange_durations;
DROP TABLE test_tsrange_maintenance;
DROP TABLE test_tsrange_pricing;
DROP TABLE test_tsrange_employment;
DROP TABLE test_tsrange_events;
DROP TABLE test_tsrange_indexing;
DROP TABLE test_tsrange_operations;
DROP TABLE test_tsrange_adjacency;
DROP TABLE test_tsrange_overlap;
DROP TABLE test_tsrange_containment;
DROP TABLE test_tsrange_properties;
DROP TABLE test_tsrange_basic;

DROP DATABASE test_tsrange_db;

-- End of TSRANGE data type tests
