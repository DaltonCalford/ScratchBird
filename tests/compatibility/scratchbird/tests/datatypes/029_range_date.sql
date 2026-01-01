-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - DATERANGE
-- Description: Comprehensive date range type testing
-- ============================================================================

-- DATERANGE represents ranges of DATE values (day precision, no time)
-- Like integer ranges, date ranges are canonicalized to [lower, upper)
-- Used for: vacation periods, project timelines, validity dates, reservations

-- Create test database
CREATE DATABASE test_daterange_db;
USE test_daterange_db;

-- ============================================================================
-- Section 1: Basic DATERANGE Creation
-- ============================================================================

CREATE TABLE test_daterange_basic (
    id INT PRIMARY KEY,
    period DATERANGE,
    description VARCHAR(200)
);

-- Various date ranges
INSERT INTO test_daterange_basic VALUES
    (1, '[2024-01-01, 2024-01-31)', 'January 2024'),
    (2, '[2024-01-01, 2024-12-31]', 'Year 2024 inclusive'),
    (3, '[2024-03-01, 2024-06-01)', 'Q2 2024'),
    (4, '(2023-12-31, 2024-01-02)', 'Just 2024-01-01'),
    (5, '[2024-06-15,)', 'From mid-June onwards'),
    (6, '(, 2024-12-31]', 'Up to end of 2024'),
    (7, 'empty', 'Empty range'),
    (8, '[2024-01-01, 2024-01-01]', 'Single day'),
    (9, '[2024-01-01, 2024-01-01)', 'Empty (exclusive upper)');

SELECT id, period, description FROM test_daterange_basic ORDER BY id;

-- ============================================================================
-- Section 2: Canonicalization (Like Integer Ranges)
-- ============================================================================

CREATE TABLE test_daterange_canonical (
    id INT PRIMARY KEY,
    input_range DATERANGE,
    description VARCHAR(200)
);

-- Date ranges are canonicalized to [lower, upper)
INSERT INTO test_daterange_canonical VALUES
    (1, '[2024-01-01, 2024-01-10]', 'Inclusive both ends'),
    (2, '(2023-12-31, 2024-01-10)', 'Exclusive both ends'),
    (3, '(2023-12-31, 2024-01-10]', 'Mixed bounds');

-- All canonicalized to [lower, upper) form
SELECT id, input_range, description FROM test_daterange_canonical ORDER BY id;

-- These are equivalent after canonicalization
SELECT '[2024-01-01, 2024-01-10]'::DATERANGE = '[2024-01-01, 2024-01-11)'::DATERANGE AS are_equal;
SELECT '(2023-12-31, 2024-01-10)'::DATERANGE = '[2024-01-01, 2024-01-10)'::DATERANGE AS are_equal;
SELECT '(2023-12-31, 2024-01-10]'::DATERANGE = '[2024-01-01, 2024-01-11)'::DATERANGE AS are_equal;

-- ============================================================================
-- Section 3: Range Properties
-- ============================================================================

CREATE TABLE test_daterange_properties (
    id INT PRIMARY KEY,
    period DATERANGE
);

INSERT INTO test_daterange_properties VALUES
    (1, '[2024-01-15, 2024-01-20)'),
    (2, '[2024-02-01,)'),
    (3, 'empty');

-- Extract bounds
SELECT id,
       lower(period) AS start_date,
       upper(period) AS end_date
FROM test_daterange_properties ORDER BY id;

-- Bound inclusivity (always [lower, upper) after canonicalization)
SELECT id,
       lower_inc(period) AS lower_inclusive,
       upper_inc(period) AS upper_inclusive
FROM test_daterange_properties ORDER BY id;

-- Check for infinite bounds
SELECT id,
       lower_inf(period) AS starts_infinite,
       upper_inf(period) AS ends_infinite
FROM test_daterange_properties ORDER BY id;

-- Check if empty
SELECT id, isempty(period) AS is_empty FROM test_daterange_properties ORDER BY id;

-- ============================================================================
-- Section 4: Containment - Date in Range
-- ============================================================================

CREATE TABLE test_daterange_containment (
    season_id INT PRIMARY KEY,
    season_name VARCHAR(100),
    season_period DATERANGE
);

INSERT INTO test_daterange_containment VALUES
    (1, 'Winter 2024', '[2024-01-01, 2024-03-20)'),
    (2, 'Spring 2024', '[2024-03-20, 2024-06-21)'),
    (3, 'Summer 2024', '[2024-06-21, 2024-09-23)'),
    (4, 'Fall 2024', '[2024-09-23, 2024-12-21)');

-- Find season for specific date
SELECT season_name FROM test_daterange_containment
WHERE season_period @> '2024-02-14'::DATE;  -- Valentine's Day

SELECT season_name FROM test_daterange_containment
WHERE season_period @> '2024-07-04'::DATE;  -- Independence Day

SELECT season_name FROM test_daterange_containment
WHERE season_period @> '2024-11-28'::DATE;  -- Thanksgiving

-- Check if range contains range
SELECT season_name FROM test_daterange_containment
WHERE season_period @> '[2024-07-01, 2024-08-01)'::DATERANGE;

-- ============================================================================
-- Section 5: Overlap Detection
-- ============================================================================

CREATE TABLE test_daterange_overlap (
    project_id INT PRIMARY KEY,
    project_name VARCHAR(200),
    project_timeline DATERANGE
);

INSERT INTO test_daterange_overlap VALUES
    (1, 'Website Redesign', '[2024-01-15, 2024-03-31)'),
    (2, 'Mobile App Development', '[2024-02-01, 2024-05-31)'),
    (3, 'Marketing Campaign', '[2024-03-01, 2024-04-15)'),
    (4, 'Database Migration', '[2024-06-01, 2024-07-31)');

-- Find overlapping projects
SELECT p1.project_name AS project1,
       p2.project_name AS project2,
       p1.project_timeline * p2.project_timeline AS overlap_period
FROM test_daterange_overlap p1
JOIN test_daterange_overlap p2 ON p1.project_id < p2.project_id
WHERE p1.project_timeline && p2.project_timeline
ORDER BY p1.project_id, p2.project_id;

-- Find projects active in Q1 2024
SELECT project_name FROM test_daterange_overlap
WHERE project_timeline && '[2024-01-01, 2024-04-01)'::DATERANGE
ORDER BY project_id;

-- ============================================================================
-- Section 6: Adjacency - Consecutive Periods
-- ============================================================================

CREATE TABLE test_daterange_adjacency (
    quarter_id INT PRIMARY KEY,
    quarter_name VARCHAR(20),
    quarter_period DATERANGE
);

INSERT INTO test_daterange_adjacency VALUES
    (1, 'Q1 2024', '[2024-01-01, 2024-04-01)'),
    (2, 'Q2 2024', '[2024-04-01, 2024-07-01)'),
    (3, 'Q3 2024', '[2024-07-01, 2024-10-01)'),
    (4, 'Q4 2024', '[2024-10-01, 2025-01-01)');

-- Find adjacent quarters
SELECT q1.quarter_name AS quarter1,
       q2.quarter_name AS quarter2,
       q1.quarter_period -|- q2.quarter_period AS are_adjacent
FROM test_daterange_adjacency q1
CROSS JOIN test_daterange_adjacency q2
WHERE q1.quarter_id < q2.quarter_id
ORDER BY q1.quarter_id, q2.quarter_id;

-- ============================================================================
-- Section 7: Range Operations
-- ============================================================================

CREATE TABLE test_daterange_operations (
    id INT PRIMARY KEY,
    period1 DATERANGE,
    period2 DATERANGE
);

INSERT INTO test_daterange_operations VALUES
    (1, '[2024-01-01, 2024-01-15)', '[2024-01-10, 2024-01-25)'),
    (2, '[2024-01-01, 2024-01-15)', '[2024-01-15, 2024-01-31)'),
    (3, '[2024-01-01, 2024-01-15)', '[2024-02-01, 2024-02-15)');

-- Union (overlapping or adjacent)
SELECT id, period1 + period2 AS union_range
FROM test_daterange_operations
WHERE id IN (1, 2)
ORDER BY id;

-- Intersection
SELECT id, period1 * period2 AS intersection
FROM test_daterange_operations ORDER BY id;

-- Difference
SELECT id, period1 - period2 AS difference
FROM test_daterange_operations ORDER BY id;

-- Range merge
SELECT id, range_merge(period1, period2) AS merged
FROM test_daterange_operations ORDER BY id;

-- ============================================================================
-- Section 8: Real-world Use Case - Hotel Reservations
-- ============================================================================

CREATE TABLE test_daterange_reservations (
    reservation_id INT PRIMARY KEY,
    guest_name VARCHAR(100),
    room_number VARCHAR(20),
    stay_period DATERANGE
);

CREATE INDEX idx_stay_period ON test_daterange_reservations USING GIST (stay_period);

INSERT INTO test_daterange_reservations VALUES
    (1, 'Alice Johnson', 'Room 101', '[2024-01-15, 2024-01-20)'),
    (2, 'Bob Smith', 'Room 101', '[2024-01-20, 2024-01-25)'),
    (3, 'Charlie Brown', 'Room 102', '[2024-01-18, 2024-01-22)'),
    (4, 'Diana Prince', 'Room 101', '[2024-01-27, 2024-01-30)');

-- Check if room is available for new reservation
SELECT reservation_id, guest_name FROM test_daterange_reservations
WHERE room_number = 'Room 101'
  AND stay_period && '[2024-01-21, 2024-01-24)'::DATERANGE
ORDER BY reservation_id;

-- Find available dates for Room 101 in January
WITH occupied AS (
    SELECT stay_period FROM test_daterange_reservations
    WHERE room_number = 'Room 101'
      AND stay_period && '[2024-01-01, 2024-02-01)'::DATERANGE
)
SELECT stay_period AS occupied_dates FROM occupied ORDER BY stay_period;

-- Who is staying on specific date?
SELECT guest_name, room_number FROM test_daterange_reservations
WHERE stay_period @> '2024-01-19'::DATE
ORDER BY room_number;

-- ============================================================================
-- Section 9: Real-world Use Case - Employee Vacation
-- ============================================================================

CREATE TABLE test_daterange_vacation (
    vacation_id INT PRIMARY KEY,
    employee_name VARCHAR(100),
    vacation_period DATERANGE
);

INSERT INTO test_daterange_vacation VALUES
    (1, 'Alice', '[2024-07-01, 2024-07-15)'),
    (2, 'Bob', '[2024-07-08, 2024-07-22)'),
    (3, 'Charlie', '[2024-08-01, 2024-08-08)'),
    (4, 'Alice', '[2024-12-23, 2025-01-02)');

-- Who is on vacation on specific date?
SELECT employee_name FROM test_daterange_vacation
WHERE vacation_period @> '2024-07-10'::DATE
ORDER BY employee_name;

-- Find overlapping vacations
SELECT v1.employee_name AS employee1,
       v2.employee_name AS employee2,
       v1.vacation_period * v2.vacation_period AS overlap
FROM test_daterange_vacation v1
JOIN test_daterange_vacation v2 ON v1.vacation_id < v2.vacation_id
WHERE v1.vacation_period && v2.vacation_period
ORDER BY v1.vacation_id;

-- Total vacation days per employee
SELECT employee_name,
       SUM(upper(vacation_period) - lower(vacation_period)) AS total_days
FROM test_daterange_vacation
GROUP BY employee_name
ORDER BY employee_name;

-- ============================================================================
-- Section 10: Real-world Use Case - Promotional Campaigns
-- ============================================================================

CREATE TABLE test_daterange_promotions (
    promo_id INT PRIMARY KEY,
    promo_name VARCHAR(200),
    promo_code VARCHAR(50),
    valid_period DATERANGE,
    discount_percent DECIMAL(5, 2)
);

INSERT INTO test_daterange_promotions VALUES
    (1, 'New Year Sale', 'NEWYEAR2024', '[2024-01-01, 2024-01-07)', 20.00),
    (2, 'Valentine Special', 'LOVE2024', '[2024-02-10, 2024-02-15)', 15.00),
    (3, 'Summer Clearance', 'SUMMER2024', '[2024-07-01, 2024-08-01)', 30.00),
    (4, 'Black Friday', 'BFRIDAY2024', '[2024-11-29, 2024-12-02)', 50.00);

-- Get active promotions for specific date
SELECT promo_name, promo_code, discount_percent
FROM test_daterange_promotions
WHERE valid_period @> '2024-02-14'::DATE;

-- Get current promotions
SELECT promo_name, promo_code, discount_percent
FROM test_daterange_promotions
WHERE valid_period @> CURRENT_DATE
ORDER BY discount_percent DESC;

-- Find all promotions in Q4
SELECT promo_name, valid_period FROM test_daterange_promotions
WHERE valid_period && '[2024-10-01, 2025-01-01)'::DATERANGE
ORDER BY lower(valid_period);

-- ============================================================================
-- Section 11: Real-world Use Case - Product Availability
-- ============================================================================

CREATE TABLE test_daterange_products (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    available_period DATERANGE
);

INSERT INTO test_daterange_products VALUES
    (1, 'Seasonal Ice Cream', '[2024-05-01, 2024-09-30]'),
    (2, 'Holiday Decorations', '[2024-11-01, 2024-12-31]'),
    (3, 'Year-Round Product', '[2024-01-01,)'),
    (4, 'Limited Edition Widget', '[2024-03-01, 2024-03-31]');

-- Products available today
SELECT product_name FROM test_daterange_products
WHERE available_period @> CURRENT_DATE
ORDER BY product_name;

-- Products available in summer
SELECT product_name FROM test_daterange_products
WHERE available_period && '[2024-06-21, 2024-09-23)'::DATERANGE
ORDER BY product_name;

-- Products that will expire soon (within 30 days)
SELECT product_name,
       upper(available_period) AS expiry_date,
       upper(available_period) - CURRENT_DATE AS days_remaining
FROM test_daterange_products
WHERE NOT upper_inf(available_period)
  AND upper(available_period) BETWEEN CURRENT_DATE AND CURRENT_DATE + INTERVAL '30 days'
ORDER BY expiry_date;

-- ============================================================================
-- Section 12: Real-world Use Case - Contract Validity
-- ============================================================================

CREATE TABLE test_daterange_contracts (
    contract_id INT PRIMARY KEY,
    vendor_name VARCHAR(200),
    contract_period DATERANGE,
    annual_value DECIMAL(12, 2)
);

INSERT INTO test_daterange_contracts VALUES
    (1, 'CloudProvider Inc', '[2023-01-01, 2025-01-01)', 120000.00),
    (2, 'SecuritySoft LLC', '[2024-01-01, 2025-01-01)', 50000.00),
    (3, 'DataCorp', '[2022-06-01,)', 75000.00),
    (4, 'TempStaff Agency', '[2024-03-01, 2024-09-01)', 30000.00);

-- Currently active contracts
SELECT vendor_name, annual_value FROM test_daterange_contracts
WHERE contract_period @> CURRENT_DATE
ORDER BY annual_value DESC;

-- Contracts expiring in next 90 days
SELECT vendor_name,
       upper(contract_period) AS expiry_date
FROM test_daterange_contracts
WHERE NOT upper_inf(contract_period)
  AND upper(contract_period) BETWEEN CURRENT_DATE AND CURRENT_DATE + INTERVAL '90 days'
ORDER BY upper(contract_period);

-- Total annual contract value currently active
SELECT SUM(annual_value) AS total_active_value
FROM test_daterange_contracts
WHERE contract_period @> CURRENT_DATE;

-- ============================================================================
-- Section 13: GiST Indexing
-- ============================================================================

CREATE TABLE test_daterange_indexing (
    event_id INT PRIMARY KEY,
    event_name VARCHAR(200),
    event_period DATERANGE
);

CREATE INDEX idx_event_period_date ON test_daterange_indexing USING GIST (event_period);

INSERT INTO test_daterange_indexing
SELECT i,
       'Event ' || i,
       daterange(
           '2024-01-01'::DATE + (i || ' days')::INTERVAL,
           '2024-01-01'::DATE + ((i + 5) || ' days')::INTERVAL
       )
FROM generate_series(1, 1000) i;

-- Queries using GiST index
SELECT event_id, event_name FROM test_daterange_indexing
WHERE event_period @> '2024-02-15'::DATE
ORDER BY event_id LIMIT 5;

SELECT event_id, event_name FROM test_daterange_indexing
WHERE event_period && '[2024-03-01, 2024-03-10)'::DATERANGE
ORDER BY event_id LIMIT 5;

-- ============================================================================
-- Section 14: Exclusion Constraints
-- ============================================================================

CREATE TABLE test_daterange_exclusion (
    booking_id INT PRIMARY KEY,
    resource_name VARCHAR(100),
    booking_period DATERANGE,
    EXCLUDE USING GIST (resource_name WITH =, booking_period WITH &&)
);

-- First booking succeeds
INSERT INTO test_daterange_exclusion VALUES
    (1, 'Conference Room A', '[2024-01-15, 2024-01-17)');

-- Different resource, overlapping dates - succeeds
INSERT INTO test_daterange_exclusion VALUES
    (2, 'Conference Room B', '[2024-01-15, 2024-01-17)');

-- Same resource, adjacent dates - succeeds
INSERT INTO test_daterange_exclusion VALUES
    (3, 'Conference Room A', '[2024-01-17, 2024-01-20)');

-- This would fail: same resource, overlapping dates
-- INSERT INTO test_daterange_exclusion VALUES
--     (4, 'Conference Room A', '[2024-01-16, 2024-01-18)');

SELECT * FROM test_daterange_exclusion ORDER BY resource_name, booking_period;

-- ============================================================================
-- Section 15: Multirange - Non-contiguous Dates
-- ============================================================================

CREATE TABLE test_daterange_multi (
    schedule_id INT PRIMARY KEY,
    description VARCHAR(200),
    dates DATEMULTIRANGE
);

INSERT INTO test_daterange_multi VALUES
    (1, 'Office open days in January', '{[2024-01-02, 2024-01-06), [2024-01-08, 2024-01-13), [2024-01-15, 2024-01-20)}'),
    (2, 'Training days', '{[2024-02-05, 2024-02-10), [2024-02-19, 2024-02-24)}');

-- Check if date is included
SELECT description FROM test_daterange_multi
WHERE dates @> '2024-01-03'::DATE;

SELECT description FROM test_daterange_multi
WHERE dates @> '2024-01-07'::DATE;  -- Sunday, not included

-- ============================================================================
-- Section 16: NULL Handling
-- ============================================================================

CREATE TABLE test_daterange_nulls (
    id INT PRIMARY KEY,
    period DATERANGE
);

INSERT INTO test_daterange_nulls VALUES
    (1, '[2024-01-01, 2024-12-31]'),
    (2, NULL),
    (3, 'empty');

SELECT id, period,
       period IS NULL AS is_null,
       isempty(period) AS is_empty
FROM test_daterange_nulls ORDER BY id;

-- Operations with NULL
SELECT id, lower(period) AS start_date FROM test_daterange_nulls ORDER BY id;
SELECT id, period @> '2024-06-15'::DATE AS contains FROM test_daterange_nulls ORDER BY id;

-- ============================================================================
-- Section 17: Fiscal Year and Custom Periods
-- ============================================================================

CREATE TABLE test_daterange_fiscal (
    fiscal_year VARCHAR(20) PRIMARY KEY,
    fiscal_period DATERANGE
);

-- Fiscal year starts July 1
INSERT INTO test_daterange_fiscal VALUES
    ('FY2023', '[2022-07-01, 2023-07-01)'),
    ('FY2024', '[2023-07-01, 2024-07-01)'),
    ('FY2025', '[2024-07-01, 2025-07-01)');

-- Find fiscal year for specific date
SELECT fiscal_year FROM test_daterange_fiscal
WHERE fiscal_period @> '2024-03-15'::DATE;

SELECT fiscal_year FROM test_daterange_fiscal
WHERE fiscal_period @> '2024-08-01'::DATE;

-- ============================================================================
-- Section 18: Best Practices
-- ============================================================================

CREATE TABLE test_daterange_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_daterange_best_practices VALUES
    (1, 'Use DATERANGE for date-only periods (no time component)'),
    (2, 'Date ranges are canonicalized to [lower, upper) format'),
    (3, 'Use for: reservations, vacations, project timelines, validity periods'),
    (4, 'Create GiST indexes for efficient containment and overlap queries'),
    (5, 'Use EXCLUDE constraints to prevent double bookings'),
    (6, 'Default [lower, upper): inclusive start, exclusive end'),
    (7, '[2024-01-01, 2024-01-10] equals [2024-01-01, 2024-01-11) after canonicalization'),
    (8, 'Use @> for "is date in period" checks'),
    (9, 'Use && for overlapping period checks (conflicts)'),
    (10, 'Use -|- for adjacent period checks (consecutive periods)'),
    (11, 'Empty ranges represent "no valid dates" (different from NULL)'),
    (12, 'Unbounded ranges useful for "from date onwards" (ongoing contracts)'),
    (13, 'DATEMULTIRANGE for non-contiguous periods (office hours, availability)'),
    (14, 'Duration: upper(range) - lower(range) gives number of days'),
    (15, 'DATERANGE ideal for business logic with day-level precision');

SELECT id, guideline FROM test_daterange_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_daterange_best_practices;
DROP TABLE test_daterange_fiscal;
DROP TABLE test_daterange_nulls;
DROP TABLE test_daterange_multi;
DROP TABLE test_daterange_exclusion;
DROP TABLE test_daterange_indexing;
DROP TABLE test_daterange_contracts;
DROP TABLE test_daterange_products;
DROP TABLE test_daterange_promotions;
DROP TABLE test_daterange_vacation;
DROP TABLE test_daterange_reservations;
DROP TABLE test_daterange_operations;
DROP TABLE test_daterange_adjacency;
DROP TABLE test_daterange_overlap;
DROP TABLE test_daterange_containment;
DROP TABLE test_daterange_properties;
DROP TABLE test_daterange_canonical;
DROP TABLE test_daterange_basic;

DROP DATABASE test_daterange_db;

-- End of DATERANGE data type tests
