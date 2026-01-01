-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - INT4RANGE and INT8RANGE
-- Description: Comprehensive integer range type testing
-- ============================================================================

-- Range types represent intervals with lower and upper bounds
-- INT4RANGE: range of INT (32-bit integers)
-- INT8RANGE: range of BIGINT (64-bit integers)
-- Used for: scheduling, versioning, numeric intervals, inventory levels

-- Create test database
CREATE DATABASE test_intrange_db;
USE test_intrange_db;

-- ============================================================================
-- Section 1: Basic INT4RANGE Creation
-- ============================================================================

CREATE TABLE test_int4range_basic (
    id INT PRIMARY KEY,
    range_value INT4RANGE,
    description VARCHAR(200)
);

-- Inclusive lower, exclusive upper: [lower, upper)
INSERT INTO test_int4range_basic VALUES (1, '[1,10)', 'Range from 1 to 9');
INSERT INTO test_int4range_basic VALUES (2, '[0,100)', 'Range from 0 to 99');
INSERT INTO test_int4range_basic VALUES (3, '[5,5)', 'Empty range');
INSERT INTO test_int4range_basic VALUES (4, '[10,20]', 'Inclusive on both ends: 10 to 20');
INSERT INTO test_int4range_basic VALUES (5, '(0,10)', 'Exclusive on both ends: 1 to 9');
INSERT INTO test_int4range_basic VALUES (6, '(0,10]', 'Exclusive lower, inclusive upper: 1 to 10');
INSERT INTO test_int4range_basic VALUES (7, '[1,)', 'Unbounded upper');
INSERT INTO test_int4range_basic VALUES (8, '(,100)', 'Unbounded lower');
INSERT INTO test_int4range_basic VALUES (9, '(,)', 'Completely unbounded');
INSERT INTO test_int4range_basic VALUES (10, 'empty', 'Explicitly empty range');

SELECT id, range_value, description FROM test_int4range_basic ORDER BY id;

-- ============================================================================
-- Section 2: Basic INT8RANGE Creation
-- ============================================================================

CREATE TABLE test_int8range_basic (
    id INT PRIMARY KEY,
    range_value INT8RANGE,
    description VARCHAR(200)
);

-- Large integer ranges
INSERT INTO test_int8range_basic VALUES (1, '[1000000000,2000000000)', 'Billion range');
INSERT INTO test_int8range_basic VALUES (2, '[0,9223372036854775807)', 'Near BIGINT max');
INSERT INTO test_int8range_basic VALUES (3, '[-1000,1000]', 'Negative to positive');
INSERT INTO test_int8range_basic VALUES (4, '[100,200)', 'Standard range');

SELECT id, range_value, description FROM test_int8range_basic ORDER BY id;

-- ============================================================================
-- Section 3: Range Bounds and Properties
-- ============================================================================

CREATE TABLE test_range_properties (
    id INT PRIMARY KEY,
    int_range INT4RANGE
);

INSERT INTO test_range_properties VALUES
    (1, '[10,20)'),
    (2, '[5,15]'),
    (3, 'empty'),
    (4, '[100,)'),
    (5, '(,50)');

-- lower: get lower bound
SELECT id, lower(int_range) AS lower_bound FROM test_range_properties ORDER BY id;

-- upper: get upper bound
SELECT id, upper(int_range) AS upper_bound FROM test_range_properties ORDER BY id;

-- isempty: check if range is empty
SELECT id, isempty(int_range) AS is_empty FROM test_range_properties ORDER BY id;

-- lower_inc: is lower bound inclusive?
SELECT id, lower_inc(int_range) AS lower_inclusive FROM test_range_properties ORDER BY id;

-- upper_inc: is upper bound inclusive?
SELECT id, upper_inc(int_range) AS upper_inclusive FROM test_range_properties ORDER BY id;

-- lower_inf, upper_inf: check for infinite bounds
SELECT id,
       lower_inf(int_range) AS lower_infinite,
       upper_inf(int_range) AS upper_infinite
FROM test_range_properties ORDER BY id;

-- ============================================================================
-- Section 4: Range Containment - @>, <@
-- ============================================================================

CREATE TABLE test_range_containment (
    id INT PRIMARY KEY,
    value_range INT4RANGE
);

INSERT INTO test_range_containment VALUES
    (1, '[1,100)'),
    (2, '[50,75)'),
    (3, '[25,50)'),
    (4, '[90,110)'),
    (5, '[200,300)');

-- @> operator: range contains range
SELECT id FROM test_range_containment WHERE value_range @> '[60,70)' ORDER BY id;
SELECT id FROM test_range_containment WHERE value_range @> '[10,20)' ORDER BY id;

-- @> operator: range contains element
SELECT id FROM test_range_containment WHERE value_range @> 50 ORDER BY id;
SELECT id FROM test_range_containment WHERE value_range @> 25 ORDER BY id;
SELECT id FROM test_range_containment WHERE value_range @> 150 ORDER BY id;

-- <@ operator: range is contained by range
SELECT id FROM test_range_containment WHERE '[55,65)' <@ value_range ORDER BY id;
SELECT id FROM test_range_containment WHERE '[1,200)' <@ value_range ORDER BY id;

-- <@ operator: element is contained by range
SELECT id FROM test_range_containment WHERE 60 <@ value_range ORDER BY id;

-- ============================================================================
-- Section 5: Range Overlap - &&
-- ============================================================================

-- && operator: ranges overlap
SELECT id FROM test_range_containment WHERE value_range && '[40,60)' ORDER BY id;
SELECT id FROM test_range_containment WHERE value_range && '[75,125)' ORDER BY id;
SELECT id FROM test_range_containment WHERE value_range && '[500,600)' ORDER BY id;

-- Overlap examples
CREATE TABLE test_range_overlap_examples (
    id INT,
    range1 INT4RANGE,
    range2 INT4RANGE,
    description VARCHAR(200)
);

INSERT INTO test_range_overlap_examples VALUES
    (1, '[1,10)', '[5,15)', 'Partial overlap'),
    (2, '[1,10)', '[10,20)', 'Adjacent, no overlap'),
    (3, '[1,10)', '[2,8)', 'One contains other'),
    (4, '[1,10)', '[20,30)', 'No overlap');

SELECT id, description,
       range1 && range2 AS overlaps
FROM test_range_overlap_examples ORDER BY id;

-- ============================================================================
-- Section 6: Range Adjacency - -|-
-- ============================================================================

-- -|- operator: ranges are adjacent (touch but don't overlap)
CREATE TABLE test_range_adjacency (
    id INT PRIMARY KEY,
    range_value INT4RANGE
);

INSERT INTO test_range_adjacency VALUES
    (1, '[1,10)'),
    (2, '[10,20)'),  -- Adjacent to id=1
    (3, '[20,30)'),  -- Adjacent to id=2
    (4, '[50,60)');  -- Not adjacent to others

SELECT r1.id AS id1, r2.id AS id2,
       r1.range_value -|- r2.range_value AS are_adjacent
FROM test_range_adjacency r1
CROSS JOIN test_range_adjacency r2
WHERE r1.id < r2.id
ORDER BY r1.id, r2.id;

-- ============================================================================
-- Section 7: Range Union and Intersection
-- ============================================================================

CREATE TABLE test_range_operations (
    id INT PRIMARY KEY,
    range1 INT4RANGE,
    range2 INT4RANGE
);

INSERT INTO test_range_operations VALUES
    (1, '[1,10)', '[5,15)'),
    (2, '[1,10)', '[10,20)'),
    (3, '[1,10)', '[20,30)');

-- + operator: union (only if ranges overlap or adjacent)
SELECT id, range1 + range2 AS union_range
FROM test_range_operations
WHERE id IN (1, 2)  -- These overlap or are adjacent
ORDER BY id;

-- * operator: intersection
SELECT id, range1 * range2 AS intersection
FROM test_range_operations ORDER BY id;

-- - operator: difference
SELECT id, range1 - range2 AS difference
FROM test_range_operations ORDER BY id;

-- ============================================================================
-- Section 8: Range Comparison Operators
-- ============================================================================

CREATE TABLE test_range_comparison (
    id INT PRIMARY KEY,
    range_value INT4RANGE
);

INSERT INTO test_range_comparison VALUES
    (1, '[1,10)'),
    (2, '[1,10)'),  -- Equal to id=1
    (3, '[5,15)'),
    (4, '[1,5)');

-- Equality
SELECT id FROM test_range_comparison WHERE range_value = '[1,10)' ORDER BY id;

-- Inequality
SELECT id FROM test_range_comparison WHERE range_value != '[1,10)' ORDER BY id;

-- Less than / greater than (based on lower bounds, then upper bounds)
SELECT id, range_value FROM test_range_comparison ORDER BY range_value;

SELECT r1.id AS id1, r2.id AS id2,
       r1.range_value < r2.range_value AS is_less_than
FROM test_range_comparison r1, test_range_comparison r2
WHERE r1.id = 1 AND r2.id IN (2, 3, 4)
ORDER BY r2.id;

-- ============================================================================
-- Section 9: Range Functions - range_merge
-- ============================================================================

CREATE TABLE test_range_merge (
    id INT,
    range_value INT4RANGE
);

INSERT INTO test_range_merge VALUES
    (1, '[1,10)'),
    (2, '[5,15)'),
    (3, '[20,30)');

-- range_merge: smallest range containing all input ranges
SELECT range_merge(range_value, '[8,25)') AS merged
FROM test_range_merge WHERE id = 1;

-- Merge multiple ranges
SELECT range_merge(r1.range_value, r2.range_value) AS merged
FROM test_range_merge r1, test_range_merge r2
WHERE r1.id = 1 AND r2.id = 2;

-- ============================================================================
-- Section 10: GiST Indexing for Ranges
-- ============================================================================

CREATE TABLE test_range_indexing (
    id INT PRIMARY KEY,
    valid_period INT4RANGE
);

-- GiST index for range operations
CREATE INDEX idx_valid_period ON test_range_indexing USING GIST (valid_period);

INSERT INTO test_range_indexing
SELECT i, int4range(i * 10, (i + 1) * 10)
FROM generate_series(1, 1000) i;

-- Queries using GiST index (fast)
SELECT id FROM test_range_indexing WHERE valid_period @> 55 ORDER BY id LIMIT 5;
SELECT id FROM test_range_indexing WHERE valid_period && '[45,65)' ORDER BY id LIMIT 5;
SELECT id FROM test_range_indexing WHERE valid_period <@ '[100,200)' ORDER BY id LIMIT 5;

-- ============================================================================
-- Section 11: NULL Handling
-- ============================================================================

CREATE TABLE test_range_nulls (
    id INT PRIMARY KEY,
    range_value INT4RANGE
);

INSERT INTO test_range_nulls VALUES
    (1, '[1,10)'),
    (2, NULL),
    (3, 'empty');

SELECT id, range_value, range_value IS NULL AS is_null FROM test_range_nulls ORDER BY id;

-- Operations with NULL
SELECT id, lower(range_value) AS lower_bound FROM test_range_nulls ORDER BY id;
SELECT id, range_value @> 5 AS contains_five FROM test_range_nulls ORDER BY id;

-- ============================================================================
-- Section 12: Real-world Use Case - Version Ranges
-- ============================================================================

CREATE TABLE test_range_versions (
    feature_id INT PRIMARY KEY,
    feature_name VARCHAR(200),
    supported_versions INT4RANGE  -- Version numbers when feature is supported
);

INSERT INTO test_range_versions VALUES
    (1, 'Basic Auth', '[1,)'),  -- Version 1 onwards
    (2, 'OAuth2', '[3,)'),  -- Version 3 onwards
    (3, 'Legacy API', '[1,5)'),  -- Versions 1-4 only
    (4, 'New Dashboard', '[5,)'),  -- Version 5 onwards
    (5, 'Experimental Feature', '[4,6)');  -- Versions 4-5 only

-- Check which features are available in version 4
SELECT feature_id, feature_name
FROM test_range_versions
WHERE supported_versions @> 4
ORDER BY feature_id;

-- Check which features are available in version 6
SELECT feature_id, feature_name
FROM test_range_versions
WHERE supported_versions @> 6
ORDER BY feature_id;

-- Find features that were available in version 2 but not in version 6
SELECT feature_id, feature_name
FROM test_range_versions
WHERE supported_versions @> 2 AND NOT (supported_versions @> 6)
ORDER BY feature_id;

-- ============================================================================
-- Section 13: Real-world Use Case - Inventory Levels
-- ============================================================================

CREATE TABLE test_range_inventory (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    acceptable_stock_level INT4RANGE,
    current_stock INT
);

INSERT INTO test_range_inventory VALUES
    (1, 'Widget A', '[50,200)', 75),
    (2, 'Widget B', '[100,500)', 25),  -- Below acceptable
    (3, 'Widget C', '[10,50)', 45),
    (4, 'Widget D', '[20,100)', 150);  -- Above acceptable

-- Find products with stock in acceptable range
SELECT product_id, product_name, current_stock
FROM test_range_inventory
WHERE acceptable_stock_level @> current_stock
ORDER BY product_id;

-- Find products that need restock
SELECT product_id, product_name, current_stock,
       lower(acceptable_stock_level) AS min_stock
FROM test_range_inventory
WHERE NOT (acceptable_stock_level @> current_stock) AND current_stock < lower(acceptable_stock_level)
ORDER BY product_id;

-- ============================================================================
-- Section 14: Real-world Use Case - Age Ranges
-- ============================================================================

CREATE TABLE test_range_age_groups (
    group_id INT PRIMARY KEY,
    group_name VARCHAR(100),
    age_range INT4RANGE
);

INSERT INTO test_range_age_groups VALUES
    (1, 'Children', '[0,13)'),
    (2, 'Teenagers', '[13,18)'),
    (3, 'Young Adults', '[18,30)'),
    (4, 'Adults', '[30,65)'),
    (5, 'Seniors', '[65,)');

-- Find age group for specific age
SELECT group_name FROM test_range_age_groups WHERE age_range @> 25;
SELECT group_name FROM test_range_age_groups WHERE age_range @> 70;
SELECT group_name FROM test_range_age_groups WHERE age_range @> 15;

-- Find overlapping age groups (should be none in this case)
SELECT g1.group_name AS group1,
       g2.group_name AS group2
FROM test_range_age_groups g1
CROSS JOIN test_range_age_groups g2
WHERE g1.group_id < g2.group_id AND g1.age_range && g2.age_range;

-- ============================================================================
-- Section 15: Real-world Use Case - Numeric ID Ranges
-- ============================================================================

CREATE TABLE test_range_id_assignments (
    region VARCHAR(50) PRIMARY KEY,
    assigned_id_range INT8RANGE
);

INSERT INTO test_range_id_assignments VALUES
    ('North America', '[1000000000,2000000000)'),
    ('Europe', '[2000000000,3000000000)'),
    ('Asia', '[3000000000,4000000000)'),
    ('South America', '[4000000000,5000000000)'),
    ('Africa', '[5000000000,6000000000)');

-- Find region for specific ID
SELECT region FROM test_range_id_assignments WHERE assigned_id_range @> 1500000000::BIGINT;
SELECT region FROM test_range_id_assignments WHERE assigned_id_range @> 3200000000::BIGINT;

-- Check if ID is assigned to any region
SELECT EXISTS(
    SELECT 1 FROM test_range_id_assignments WHERE assigned_id_range @> 7000000000::BIGINT
) AS id_is_assigned;

-- ============================================================================
-- Section 16: Range Canonicalization
-- ============================================================================

CREATE TABLE test_range_canonical (
    id INT PRIMARY KEY,
    input_range INT4RANGE,
    description VARCHAR(200)
);

-- Integer ranges are canonicalized to [lower, upper)
INSERT INTO test_range_canonical VALUES (1, '[1,10]', 'Inclusive both ends');
INSERT INTO test_range_canonical VALUES (2, '(0,10)', 'Exclusive both ends');
INSERT INTO test_range_canonical VALUES (3, '(0,10]', 'Mixed bounds');

-- All canonicalized to [lower, upper) form for integers
SELECT id, input_range, description FROM test_range_canonical ORDER BY id;

-- These are all equivalent after canonicalization
SELECT '[1,10]'::INT4RANGE = '[1,11)'::INT4RANGE AS are_equal;
SELECT '(0,10)'::INT4RANGE = '[1,10)'::INT4RANGE AS are_equal;
SELECT '(0,10]'::INT4RANGE = '[1,11)'::INT4RANGE AS are_equal;

-- ============================================================================
-- Section 17: Multirange (Array of Ranges)
-- ============================================================================

CREATE TABLE test_range_multirange (
    id INT PRIMARY KEY,
    available_slots INT4MULTIRANGE
);

-- INT4MULTIRANGE: multiple non-contiguous ranges
INSERT INTO test_range_multirange VALUES
    (1, '{[1,5), [10,15), [20,25)}'),  -- Three separate time slots
    (2, '{[0,100)}'),  -- Single continuous range
    (3, '{[1,10), [15,20), [25,30), [35,40)}');  -- Four separate ranges

SELECT id, available_slots FROM test_range_multirange ORDER BY id;

-- Check if value is in any of the ranges
SELECT id FROM test_range_multirange WHERE available_slots @> 12 ORDER BY id;
SELECT id FROM test_range_multirange WHERE available_slots @> 3 ORDER BY id;

-- ============================================================================
-- Section 18: Range Performance Characteristics
-- ============================================================================

CREATE TABLE test_range_performance (
    id INT PRIMARY KEY,
    note TEXT
);

INSERT INTO test_range_performance VALUES
    (1, 'GiST indexes enable fast containment (@>), overlap (&&), and adjacency (-|-) queries'),
    (2, 'Range operations are optimized for boundary comparisons'),
    (3, 'Integer ranges are canonicalized to [lower, upper) format automatically'),
    (4, 'Unbounded ranges use special internal representation (no memory overhead)'),
    (5, 'Range comparisons are based on lower bound first, then upper bound'),
    (6, 'Empty ranges are a special value, not NULL'),
    (7, 'Multiranges allow non-contiguous intervals in a single value'),
    (8, 'Range types are ideal for temporal data, versioning, and interval logic');

SELECT id, note FROM test_range_performance ORDER BY id;

-- ============================================================================
-- Section 19: Best Practices
-- ============================================================================

CREATE TABLE test_range_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_range_best_practices VALUES
    (1, 'Use INT4RANGE for version numbers, inventory levels, age groups'),
    (2, 'Use INT8RANGE for large ID ranges, timestamps (epoch), sequence numbers'),
    (3, 'Create GiST indexes on range columns for containment queries'),
    (4, 'Default [lower, upper) convention: inclusive lower, exclusive upper'),
    (5, 'Use @> operator to check if range contains value or range'),
    (6, 'Use && operator to check if ranges overlap'),
    (7, 'Use -|- operator to check if ranges are adjacent'),
    (8, 'Empty ranges represent "no valid interval" (different from NULL)'),
    (9, 'Unbounded ranges useful for "from version X onwards" scenarios'),
    (10, 'Range types eliminate complex WHERE clauses with multiple comparisons'),
    (11, 'Multiranges for non-contiguous availability (e.g., appointment slots)'),
    (12, 'Integer range operations automatically handle canonicalization'),
    (13, 'Use lower() and upper() to extract bounds when needed'),
    (14, 'Range types are space-efficient (just two boundary values)'),
    (15, 'Consider ranges for any "from-to" or "min-max" data modeling');

SELECT id, guideline FROM test_range_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_range_best_practices;
DROP TABLE test_range_performance;
DROP TABLE test_range_multirange;
DROP TABLE test_range_canonical;
DROP TABLE test_range_id_assignments;
DROP TABLE test_range_age_groups;
DROP TABLE test_range_inventory;
DROP TABLE test_range_versions;
DROP TABLE test_range_nulls;
DROP TABLE test_range_indexing;
DROP TABLE test_range_merge;
DROP TABLE test_range_comparison;
DROP TABLE test_range_operations;
DROP TABLE test_range_adjacency;
DROP TABLE test_range_overlap_examples;
DROP TABLE test_range_containment;
DROP TABLE test_range_properties;
DROP TABLE test_int8range_basic;
DROP TABLE test_int4range_basic;

DROP DATABASE test_intrange_db;

-- End of INT4RANGE and INT8RANGE data type tests
