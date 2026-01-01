-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - TSTZRANGE (Timestamp with Timezone Range)
-- Description: Comprehensive timestamp with timezone range type testing
-- ============================================================================

-- TSTZRANGE represents ranges of TIMESTAMP WITH TIME ZONE values
-- Automatically handles timezone conversions and DST transitions
-- Used for: global scheduling, international events, distributed systems

-- Create test database
CREATE DATABASE test_tstzrange_db;
USE test_tstzrange_db;

-- ============================================================================
-- Section 1: Basic TSTZRANGE Creation
-- ============================================================================

CREATE TABLE test_tstzrange_basic (
    id INT PRIMARY KEY,
    period TSTZRANGE,
    description VARCHAR(200)
);

-- Timestamp with timezone ranges
INSERT INTO test_tstzrange_basic VALUES
    (1, '[2024-01-01 00:00:00+00, 2024-01-02 00:00:00+00)', 'One day in UTC'),
    (2, '[2024-01-15 09:00:00-05, 2024-01-15 17:00:00-05)', 'Business hours EST'),
    (3, '[2024-06-15 09:00:00+02, 2024-06-15 17:00:00+02)', 'Business hours CEST'),
    (4, '[2024-01-01 00:00:00+00, 2024-12-31 23:59:59+00]', 'Year 2024 UTC'),
    (5, '[2024-03-10 02:00:00-05,)', 'DST start onwards'),
    (6, 'empty', 'Empty period'),
    (7, '[2024-01-01 12:00:00+00, 2024-01-01 12:00:00+00]', 'Single instant');

SELECT id, period, description FROM test_tstzrange_basic ORDER BY id;

-- ============================================================================
-- Section 2: Timezone Conversion and Display
-- ============================================================================

CREATE TABLE test_tstzrange_timezones (
    id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_period TSTZRANGE
);

-- Same absolute time, different timezone representations
INSERT INTO test_tstzrange_timezones VALUES
    (1, 'Global Meeting', '[2024-01-15 14:00:00+00, 2024-01-15 15:00:00+00)'),
    (2, 'US East Meeting', '[2024-01-15 09:00:00-05, 2024-01-15 10:00:00-05)'),
    (3, 'Europe Meeting', '[2024-01-15 15:00:00+01, 2024-01-15 16:00:00+01)');

-- All stored in UTC internally
SELECT id, event_name, period FROM test_tstzrange_timezones ORDER BY id;

-- Extract bounds (will be in session timezone)
SELECT id, event_name,
       lower(event_period) AS start_time,
       upper(event_period) AS end_time
FROM test_tstzrange_timezones ORDER BY id;

-- Check if same absolute time (id 1 and 2 are same meeting)
SELECT t1.event_name AS event1,
       t2.event_name AS event2,
       t1.event_period = t2.event_period AS same_time
FROM test_tstzrange_timezones t1
CROSS JOIN test_tstzrange_timezones t2
WHERE t1.id = 1 AND t2.id IN (2, 3);

-- ============================================================================
-- Section 3: DST (Daylight Saving Time) Handling
-- ============================================================================

CREATE TABLE test_tstzrange_dst (
    id INT PRIMARY KEY,
    description VARCHAR(200),
    time_period TSTZRANGE
);

-- DST transition in US (second Sunday in March, 2:00 AM -> 3:00 AM)
INSERT INTO test_tstzrange_dst VALUES
    (1, 'Before DST', '[2024-03-10 00:00:00-05, 2024-03-10 01:59:59-05]'),
    (2, 'During DST transition', '[2024-03-10 01:00:00-05, 2024-03-10 03:00:00-04)'),
    (3, 'After DST', '[2024-03-10 03:00:00-04, 2024-03-10 04:00:00-04)');

SELECT id, description, time_period FROM test_tstzrange_dst ORDER BY id;

-- DST fall back (first Sunday in November, 2:00 AM -> 1:00 AM)
INSERT INTO test_tstzrange_dst VALUES
    (4, 'Before fall back', '[2024-11-03 00:00:00-04, 2024-11-03 01:59:59-04]'),
    (5, 'During fall back', '[2024-11-03 01:00:00-04, 2024-11-03 02:00:00-05)'),
    (6, 'After fall back', '[2024-11-03 02:00:00-05, 2024-11-03 03:00:00-05)');

-- Durations across DST transitions
SELECT id, description,
       upper(time_period) - lower(time_period) AS duration
FROM test_tstzrange_dst
WHERE id IN (2, 5)
ORDER BY id;

-- ============================================================================
-- Section 4: Global Event Scheduling
-- ============================================================================

CREATE TABLE test_tstzrange_global_events (
    event_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_time TSTZRANGE,
    timezone_description VARCHAR(100)
);

INSERT INTO test_tstzrange_global_events VALUES
    (1, 'Tokyo Office Hours', '[2024-01-15 09:00:00+09, 2024-01-15 18:00:00+09)', 'JST'),
    (2, 'London Office Hours', '[2024-01-15 09:00:00+00, 2024-01-15 18:00:00+00)', 'GMT'),
    (3, 'New York Office Hours', '[2024-01-15 09:00:00-05, 2024-01-15 18:00:00-05)', 'EST'),
    (4, 'San Francisco Office Hours', '[2024-01-15 09:00:00-08, 2024-01-15 18:00:00-08)', 'PST');

-- Find overlapping office hours for global teams
SELECT e1.event_name AS office1,
       e2.event_name AS office2,
       e1.event_time * e2.event_time AS overlap_period
FROM test_tstzrange_global_events e1
CROSS JOIN test_tstzrange_global_events e2
WHERE e1.event_id < e2.event_id
  AND e1.event_time && e2.event_time
ORDER BY e1.event_id, e2.event_id;

-- Find best time for global meeting (overlaps all offices)
WITH overlap_all AS (
    SELECT
        e1.event_time * e2.event_time * e3.event_time * e4.event_time AS meeting_window
    FROM test_tstzrange_global_events e1,
         test_tstzrange_global_events e2,
         test_tstzrange_global_events e3,
         test_tstzrange_global_events e4
    WHERE e1.event_id = 1 AND e2.event_id = 2 AND e3.event_id = 3 AND e4.event_id = 4
)
SELECT meeting_window FROM overlap_all WHERE NOT isempty(meeting_window);

-- ============================================================================
-- Section 5: Containment with Timezone Awareness
-- ============================================================================

CREATE TABLE test_tstzrange_containment (
    shift_id INT PRIMARY KEY,
    location VARCHAR(100),
    shift_period TSTZRANGE
);

INSERT INTO test_tstzrange_containment VALUES
    (1, 'New York', '[2024-01-15 09:00:00-05, 2024-01-15 17:00:00-05)'),
    (2, 'London', '[2024-01-15 09:00:00+00, 2024-01-15 17:00:00+00)'),
    (3, 'Tokyo', '[2024-01-15 09:00:00+09, 2024-01-15 17:00:00+09)');

-- Check if specific UTC time is during any shift
SELECT location FROM test_tstzrange_containment
WHERE shift_period @> '2024-01-15 14:00:00+00'::TIMESTAMPTZ
ORDER BY shift_id;

SELECT location FROM test_tstzrange_containment
WHERE shift_period @> '2024-01-15 09:00:00+00'::TIMESTAMPTZ
ORDER BY shift_id;

-- ============================================================================
-- Section 6: International Flight Schedules
-- ============================================================================

CREATE TABLE test_tstzrange_flights (
    flight_id INT PRIMARY KEY,
    flight_number VARCHAR(20),
    departure_airport VARCHAR(100),
    arrival_airport VARCHAR(100),
    flight_duration TSTZRANGE
);

INSERT INTO test_tstzrange_flights VALUES
    (1, 'AA100', 'New York (JFK)', 'London (LHR)',
        '[2024-01-15 18:00:00-05, 2024-01-16 06:00:00+00)'),
    (2, 'BA200', 'London (LHR)', 'New York (JFK)',
        '[2024-01-16 10:00:00+00, 2024-01-16 13:00:00-05)'),
    (3, 'NH300', 'Tokyo (NRT)', 'San Francisco (SFO)',
        '[2024-01-15 17:00:00+09, 2024-01-15 10:00:00-08)');

-- Calculate actual flight time
SELECT flight_number,
       departure_airport,
       arrival_airport,
       upper(flight_duration) - lower(flight_duration) AS actual_flight_time
FROM test_tstzrange_flights
ORDER BY flight_id;

-- Find flights in the air at specific UTC time
SELECT flight_number, departure_airport, arrival_airport
FROM test_tstzrange_flights
WHERE flight_duration @> '2024-01-15 23:00:00+00'::TIMESTAMPTZ
ORDER BY flight_id;

-- ============================================================================
-- Section 7: Server Maintenance Across Timezones
-- ============================================================================

CREATE TABLE test_tstzrange_maintenance (
    server_id INT PRIMARY KEY,
    server_location VARCHAR(100),
    maintenance_window TSTZRANGE
);

INSERT INTO test_tstzrange_maintenance VALUES
    (1, 'US East (Virginia)', '[2024-01-15 02:00:00-05, 2024-01-15 04:00:00-05)'),
    (2, 'US West (Oregon)', '[2024-01-15 02:00:00-08, 2024-01-15 04:00:00-08)'),
    (3, 'EU (Ireland)', '[2024-01-15 02:00:00+00, 2024-01-15 04:00:00+00)'),
    (4, 'Asia (Tokyo)', '[2024-01-15 02:00:00+09, 2024-01-15 04:00:00+09)');

-- Find servers under maintenance at specific UTC time
SELECT server_location FROM test_tstzrange_maintenance
WHERE maintenance_window @> '2024-01-15 07:00:00+00'::TIMESTAMPTZ
ORDER BY server_id;

-- Find overlapping maintenance windows
SELECT s1.server_location AS server1,
       s2.server_location AS server2
FROM test_tstzrange_maintenance s1
JOIN test_tstzrange_maintenance s2 ON s1.server_id < s2.server_id
WHERE s1.maintenance_window && s2.maintenance_window
ORDER BY s1.server_id, s2.server_id;

-- ============================================================================
-- Section 8: Conference Call Scheduling
-- ============================================================================

CREATE TABLE test_tstzrange_calls (
    call_id INT PRIMARY KEY,
    meeting_name VARCHAR(200),
    scheduled_time TSTZRANGE,
    organizer_timezone VARCHAR(50)
);

INSERT INTO test_tstzrange_calls VALUES
    (1, 'Weekly Team Sync', '[2024-01-15 10:00:00-05, 2024-01-15 11:00:00-05)', 'America/New_York'),
    (2, 'Client Demo', '[2024-01-15 14:00:00+00, 2024-01-15 15:00:00+00)', 'Europe/London'),
    (3, 'Asia-Pacific Review', '[2024-01-15 16:00:00+09, 2024-01-15 17:00:00+09)', 'Asia/Tokyo');

-- Show all meetings in UTC
SELECT call_id, meeting_name,
       lower(scheduled_time) AT TIME ZONE 'UTC' AS start_utc,
       upper(scheduled_time) AT TIME ZONE 'UTC' AS end_utc
FROM test_tstzrange_calls
ORDER BY lower(scheduled_time);

-- Check conflicts at specific location timezone
SELECT meeting_name FROM test_tstzrange_calls
WHERE scheduled_time @> '2024-01-15 15:00:00+00'::TIMESTAMPTZ
ORDER BY call_id;

-- ============================================================================
-- Section 9: Trading Hours Across Markets
-- ============================================================================

CREATE TABLE test_tstzrange_markets (
    market_id INT PRIMARY KEY,
    market_name VARCHAR(100),
    trading_hours TSTZRANGE
);

INSERT INTO test_tstzrange_markets VALUES
    (1, 'Tokyo Stock Exchange', '[2024-01-15 09:00:00+09, 2024-01-15 15:00:00+09)'),
    (2, 'London Stock Exchange', '[2024-01-15 08:00:00+00, 2024-01-15 16:30:00+00)'),
    (3, 'New York Stock Exchange', '[2024-01-15 09:30:00-05, 2024-01-15 16:00:00-05)');

-- Find markets open at specific UTC time
SELECT market_name FROM test_tstzrange_markets
WHERE trading_hours @> '2024-01-15 14:00:00+00'::TIMESTAMPTZ
ORDER BY market_id;

-- Find overlap between markets (for arbitrage opportunities)
SELECT m1.market_name AS market1,
       m2.market_name AS market2,
       m1.trading_hours * m2.trading_hours AS overlap
FROM test_tstzrange_markets m1
CROSS JOIN test_tstzrange_markets m2
WHERE m1.market_id < m2.market_id
  AND m1.trading_hours && m2.trading_hours
ORDER BY m1.market_id, m2.market_id;

-- ============================================================================
-- Section 10: GiST Indexing
-- ============================================================================

CREATE TABLE test_tstzrange_indexing (
    event_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_period TSTZRANGE
);

CREATE INDEX idx_event_period_tz ON test_tstzrange_indexing USING GIST (event_period);

INSERT INTO test_tstzrange_indexing
SELECT i,
       'Event ' || i,
       tstzrange(
           '2024-01-01 00:00:00+00'::TIMESTAMPTZ + (i || ' hours')::INTERVAL,
           '2024-01-01 00:00:00+00'::TIMESTAMPTZ + ((i + 2) || ' hours')::INTERVAL
       )
FROM generate_series(1, 1000) i;

-- Queries using index
SELECT event_id, event_name FROM test_tstzrange_indexing
WHERE event_period @> '2024-01-01 10:30:00+00'::TIMESTAMPTZ
ORDER BY event_id LIMIT 5;

-- ============================================================================
-- Section 11: Multirange - Non-contiguous Availability
-- ============================================================================

CREATE TABLE test_tstzrange_multi (
    resource_id INT PRIMARY KEY,
    resource_name VARCHAR(100),
    available_periods TSTZMULTIRANGE
);

INSERT INTO test_tstzrange_multi VALUES
    (1, 'Server A', '{[2024-01-15 09:00:00+00, 2024-01-15 12:00:00+00),
                       [2024-01-15 14:00:00+00, 2024-01-15 17:00:00+00)}'),
    (2, 'Server B', '{[2024-01-15 08:00:00+00, 2024-01-15 18:00:00+00)}');

-- Check availability at specific time
SELECT resource_name FROM test_tstzrange_multi
WHERE available_periods @> '2024-01-15 13:00:00+00'::TIMESTAMPTZ
ORDER BY resource_id;

SELECT resource_name FROM test_tstzrange_multi
WHERE available_periods @> '2024-01-15 10:00:00+00'::TIMESTAMPTZ
ORDER BY resource_id;

-- ============================================================================
-- Section 12: Time-based Access Control
-- ============================================================================

CREATE TABLE test_tstzrange_access (
    access_id INT PRIMARY KEY,
    user_name VARCHAR(100),
    resource VARCHAR(100),
    access_period TSTZRANGE
);

INSERT INTO test_tstzrange_access VALUES
    (1, 'Alice', 'Database', '[2024-01-01 00:00:00+00, 2024-06-30 23:59:59+00]'),
    (2, 'Bob', 'Database', '[2024-01-01 00:00:00+00,)'),
    (3, 'Charlie', 'API', '[2024-01-15 09:00:00-05, 2024-01-15 17:00:00-05)');

-- Check access at current time
SELECT user_name, resource FROM test_tstzrange_access
WHERE access_period @> CURRENT_TIMESTAMP
ORDER BY user_name;

-- Check access at specific time
SELECT user_name, resource FROM test_tstzrange_access
WHERE access_period @> '2024-03-15 14:00:00+00'::TIMESTAMPTZ
ORDER BY user_name;

-- ============================================================================
-- Section 13: Historical Data with Timezone
-- ============================================================================

CREATE TABLE test_tstzrange_history (
    record_id INT PRIMARY KEY,
    data_value VARCHAR(200),
    valid_period TSTZRANGE
);

INSERT INTO test_tstzrange_history VALUES
    (1, 'Version 1', '[2024-01-01 00:00:00+00, 2024-02-01 00:00:00+00)'),
    (2, 'Version 2', '[2024-02-01 00:00:00+00, 2024-03-01 00:00:00+00)'),
    (3, 'Version 3', '[2024-03-01 00:00:00+00,)');

-- Get current version
SELECT data_value FROM test_tstzrange_history
WHERE valid_period @> CURRENT_TIMESTAMP;

-- Get version at specific time
SELECT data_value FROM test_tstzrange_history
WHERE valid_period @> '2024-02-15 12:00:00+00'::TIMESTAMPTZ;

-- ============================================================================
-- Section 14: NULL Handling
-- ============================================================================

CREATE TABLE test_tstzrange_nulls (
    id INT PRIMARY KEY,
    period TSTZRANGE
);

INSERT INTO test_tstzrange_nulls VALUES
    (1, '[2024-01-01 00:00:00+00, 2024-12-31 23:59:59+00]'),
    (2, NULL),
    (3, 'empty');

SELECT id, period,
       period IS NULL AS is_null,
       isempty(period) AS is_empty
FROM test_tstzrange_nulls ORDER BY id;

-- ============================================================================
-- Section 15: Comparison and Sorting
-- ============================================================================

CREATE TABLE test_tstzrange_sorting (
    id INT PRIMARY KEY,
    period TSTZRANGE
);

INSERT INTO test_tstzrange_sorting VALUES
    (1, '[2024-06-01 00:00:00+00, 2024-07-01 00:00:00+00)'),
    (2, '[2024-01-01 00:00:00+00, 2024-02-01 00:00:00+00)'),
    (3, '[2024-01-01 00:00:00-05, 2024-03-01 00:00:00-05)'),  -- Earlier in UTC
    (4, '[2024-03-01 00:00:00+00, 2024-04-01 00:00:00+00)');

-- Sort by range (normalized to UTC internally)
SELECT id, period FROM test_tstzrange_sorting ORDER BY period;

-- ============================================================================
-- Section 16: Exclusion Constraints for Global Resources
-- ============================================================================

CREATE TABLE test_tstzrange_global_booking (
    booking_id INT PRIMARY KEY,
    conference_line VARCHAR(100),
    booking_period TSTZRANGE,
    EXCLUDE USING GIST (conference_line WITH =, booking_period WITH &&)
);

INSERT INTO test_tstzrange_global_booking VALUES
    (1, 'Line 1', '[2024-01-15 09:00:00-05, 2024-01-15 10:00:00-05)');

INSERT INTO test_tstzrange_global_booking VALUES
    (2, 'Line 1', '[2024-01-15 14:00:00+00, 2024-01-15 15:00:00+00)');  -- Same UTC time as booking 1

-- This would fail: same conference line, overlapping time
-- INSERT INTO test_tstzrange_global_booking VALUES
--     (3, 'Line 1', '[2024-01-15 09:30:00-05, 2024-01-15 10:30:00-05)');

SELECT * FROM test_tstzrange_global_booking ORDER BY booking_id;

-- ============================================================================
-- Section 17: Best Practices
-- ============================================================================

CREATE TABLE test_tstzrange_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_tstzrange_best_practices VALUES
    (1, 'Use TSTZRANGE for global applications, international scheduling'),
    (2, 'Timestamps automatically normalized to UTC for storage and comparison'),
    (3, 'Handles DST transitions automatically'),
    (4, 'Use for: international flights, global office hours, trading windows'),
    (5, 'Create GiST indexes for temporal queries across timezones'),
    (6, 'Use EXCLUDE constraints for global resource booking'),
    (7, 'AT TIME ZONE for displaying in specific timezone'),
    (8, 'Containment (@>) works correctly across timezone boundaries'),
    (9, 'Overlap (&&) automatically handles timezone conversion'),
    (10, 'Duration calculations account for DST transitions'),
    (11, 'Prefer TSTZRANGE over TSRANGE for distributed systems'),
    (12, 'Store in UTC (TSTZRANGE does this), display in local time'),
    (13, 'Use for bi-temporal databases with timezone awareness'),
    (14, 'TSTZMULTIRANGE for non-contiguous global availability'),
    (15, 'Essential for applications spanning multiple timezones');

SELECT id, guideline FROM test_tstzrange_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_tstzrange_best_practices;
DROP TABLE test_tstzrange_global_booking;
DROP TABLE test_tstzrange_sorting;
DROP TABLE test_tstzrange_nulls;
DROP TABLE test_tstzrange_history;
DROP TABLE test_tstzrange_access;
DROP TABLE test_tstzrange_multi;
DROP TABLE test_tstzrange_indexing;
DROP TABLE test_tstzrange_markets;
DROP TABLE test_tstzrange_calls;
DROP TABLE test_tstzrange_maintenance;
DROP TABLE test_tstzrange_flights;
DROP TABLE test_tstzrange_containment;
DROP TABLE test_tstzrange_global_events;
DROP TABLE test_tstzrange_dst;
DROP TABLE test_tstzrange_timezones;
DROP TABLE test_tstzrange_basic;

DROP DATABASE test_tstzrange_db;

-- End of TSTZRANGE data type tests
