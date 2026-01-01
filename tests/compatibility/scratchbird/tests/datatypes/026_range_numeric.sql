-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - NUMRANGE
-- Description: Comprehensive numeric range type testing
-- ============================================================================

-- NUMRANGE represents ranges of NUMERIC/DECIMAL values
-- Unlike integer ranges, numeric ranges are NOT canonicalized
-- Used for: price ranges, measurement intervals, scientific data, percentages

-- Create test database
CREATE DATABASE test_numrange_db;
USE test_numrange_db;

-- ============================================================================
-- Section 1: Basic NUMRANGE Creation
-- ============================================================================

CREATE TABLE test_numrange_basic (
    id INT PRIMARY KEY,
    range_value NUMRANGE,
    description VARCHAR(200)
);

-- Various bound types
INSERT INTO test_numrange_basic VALUES (1, '[0.0,1.0)', 'Unit interval');
INSERT INTO test_numrange_basic VALUES (2, '[10.5,20.5]', 'Inclusive both ends');
INSERT INTO test_numrange_basic VALUES (3, '(0.0,100.0)', 'Exclusive both ends');
INSERT INTO test_numrange_basic VALUES (4, '[99.99,199.99)', 'Price range');
INSERT INTO test_numrange_basic VALUES (5, '[0.0,0.0]', 'Single point');
INSERT INTO test_numrange_basic VALUES (6, '(0.0,0.0)', 'Empty range');
INSERT INTO test_numrange_basic VALUES (7, '[1.5,)', 'Unbounded upper');
INSERT INTO test_numrange_basic VALUES (8, '(,100.0]', 'Unbounded lower');
INSERT INTO test_numrange_basic VALUES (9, 'empty', 'Explicitly empty');

SELECT id, range_value, description FROM test_numrange_basic ORDER BY id;

-- ============================================================================
-- Section 2: Decimal Precision in Ranges
-- ============================================================================

CREATE TABLE test_numrange_precision (
    id INT PRIMARY KEY,
    precise_range NUMRANGE,
    description VARCHAR(200)
);

INSERT INTO test_numrange_precision VALUES
    (1, '[0.001,0.999)', 'Three decimal places'),
    (2, '[3.14159,3.14160)', 'Pi approximation'),
    (3, '[0.00001,0.00002]', 'Very small values'),
    (4, '[1000000.50,2000000.75)', 'Large values with decimals');

SELECT id, precise_range, description FROM test_numrange_precision ORDER BY id;

-- Extract bounds with precision
SELECT id,
       lower(precise_range) AS lower_bound,
       upper(precise_range) AS upper_bound,
       lower_inc(precise_range) AS lower_inclusive,
       upper_inc(precise_range) AS upper_inclusive
FROM test_numrange_precision ORDER BY id;

-- ============================================================================
-- Section 3: NO Canonicalization (Unlike Integer Ranges)
-- ============================================================================

CREATE TABLE test_numrange_no_canonical (
    id INT PRIMARY KEY,
    range_value NUMRANGE
);

-- These are NOT equivalent (numeric ranges preserve bound types)
INSERT INTO test_numrange_no_canonical VALUES (1, '[1.0,10.0)');
INSERT INTO test_numrange_no_canonical VALUES (2, '[1.0,10.0]');
INSERT INTO test_numrange_no_canonical VALUES (3, '(0.999999,10.0)');

SELECT id, range_value,
       lower_inc(range_value) AS lower_inc,
       upper_inc(range_value) AS upper_inc
FROM test_numrange_no_canonical ORDER BY id;

-- These are different ranges (not canonicalized)
SELECT '[1.0,10.0)'::NUMRANGE = '[1.0,10.0]'::NUMRANGE AS are_equal;
SELECT '(0.999,1.0)'::NUMRANGE = '[1.0,1.0]'::NUMRANGE AS are_equal;

-- ============================================================================
-- Section 4: Range Containment with Decimals
-- ============================================================================

CREATE TABLE test_numrange_containment (
    id INT PRIMARY KEY,
    price_range NUMRANGE
);

INSERT INTO test_numrange_containment VALUES
    (1, '[0.00,50.00)'),     -- Budget items
    (2, '[50.00,200.00)'),   -- Mid-range
    (3, '[200.00,1000.00)'), -- Premium
    (4, '[1000.00,)');       -- Luxury

-- Check containment of specific values
SELECT id FROM test_numrange_containment WHERE price_range @> 25.99::NUMERIC ORDER BY id;
SELECT id FROM test_numrange_containment WHERE price_range @> 199.99::NUMERIC ORDER BY id;
SELECT id FROM test_numrange_containment WHERE price_range @> 500.00::NUMERIC ORDER BY id;
SELECT id FROM test_numrange_containment WHERE price_range @> 5000.00::NUMERIC ORDER BY id;

-- Check if range contains range
SELECT id FROM test_numrange_containment WHERE price_range @> '[100.00,150.00)'::NUMRANGE ORDER BY id;

-- ============================================================================
-- Section 5: Range Overlap
-- ============================================================================

CREATE TABLE test_numrange_overlap (
    discount_id INT PRIMARY KEY,
    discount_name VARCHAR(100),
    applicable_price_range NUMRANGE
);

INSERT INTO test_numrange_overlap VALUES
    (1, '10% off under $100', '[0.00,100.00)'),
    (2, '15% off $50-$150', '[50.00,150.00)'),
    (3, '20% off over $200', '[200.00,)');

-- Find overlapping discounts
SELECT d1.discount_name AS discount1,
       d2.discount_name AS discount2
FROM test_numrange_overlap d1
CROSS JOIN test_numrange_overlap d2
WHERE d1.discount_id < d2.discount_id
  AND d1.applicable_price_range && d2.applicable_price_range
ORDER BY d1.discount_id;

-- Find discounts applicable to specific price range
SELECT discount_name FROM test_numrange_overlap
WHERE applicable_price_range && '[75.00,125.00)'::NUMRANGE
ORDER BY discount_id;

-- ============================================================================
-- Section 6: Range Operations - Union, Intersection, Difference
-- ============================================================================

CREATE TABLE test_numrange_operations (
    id INT PRIMARY KEY,
    range1 NUMRANGE,
    range2 NUMRANGE
);

INSERT INTO test_numrange_operations VALUES
    (1, '[0.0,10.0)', '[5.0,15.0)'),
    (2, '[0.0,10.0]', '[10.0,20.0]'),  -- Adjacent at boundary
    (3, '[0.0,5.0)', '[10.0,15.0)');   -- Disjoint

-- Union (only works for overlapping or adjacent ranges)
SELECT id, range1 + range2 AS union_range
FROM test_numrange_operations
WHERE id IN (1, 2)
ORDER BY id;

-- Intersection
SELECT id, range1 * range2 AS intersection
FROM test_numrange_operations ORDER BY id;

-- Difference
SELECT id, range1 - range2 AS difference
FROM test_numrange_operations ORDER BY id;

-- ============================================================================
-- Section 7: Range Merge
-- ============================================================================

-- range_merge: smallest range containing both inputs
SELECT range_merge('[0.0,10.0)'::NUMRANGE, '[15.0,25.0)'::NUMRANGE) AS merged;
SELECT range_merge('[5.5,10.5)'::NUMRANGE, '[8.0,15.0)'::NUMRANGE) AS merged;

-- ============================================================================
-- Section 8: Real-world Use Case - Price Tiers
-- ============================================================================

CREATE TABLE test_numrange_price_tiers (
    tier_id INT PRIMARY KEY,
    tier_name VARCHAR(100),
    price_range NUMRANGE,
    shipping_cost DECIMAL(10, 2)
);

INSERT INTO test_numrange_price_tiers VALUES
    (1, 'Micro Purchase', '[0.00,25.00)', 5.99),
    (2, 'Small Purchase', '[25.00,75.00)', 3.99),
    (3, 'Medium Purchase', '[75.00,150.00)', 1.99),
    (4, 'Large Purchase', '[150.00,)', 0.00);  -- Free shipping

-- Find tier and shipping cost for order
SELECT tier_name, shipping_cost
FROM test_numrange_price_tiers
WHERE price_range @> 99.99::NUMERIC;

SELECT tier_name, shipping_cost
FROM test_numrange_price_tiers
WHERE price_range @> 12.50::NUMERIC;

-- Calculate total cost including shipping
CREATE FUNCTION calculate_total_with_shipping(order_amount NUMERIC)
RETURNS NUMERIC AS $$
DECLARE
    shipping NUMERIC;
BEGIN
    SELECT shipping_cost INTO shipping
    FROM test_numrange_price_tiers
    WHERE price_range @> order_amount;

    RETURN order_amount + COALESCE(shipping, 0);
END;
$$ LANGUAGE plpgsql;

SELECT calculate_total_with_shipping(49.99) AS total_with_shipping;
SELECT calculate_total_with_shipping(175.00) AS total_with_shipping;

-- ============================================================================
-- Section 9: Real-world Use Case - Temperature Ranges
-- ============================================================================

CREATE TABLE test_numrange_temperature (
    zone_id INT PRIMARY KEY,
    zone_name VARCHAR(100),
    acceptable_temp_celsius NUMRANGE
);

INSERT INTO test_numrange_temperature VALUES
    (1, 'Freezer', '[-20.0,-10.0]'),
    (2, 'Refrigerator', '[2.0,8.0]'),
    (3, 'Room Temperature', '[18.0,24.0]'),
    (4, 'Warm Storage', '[25.0,35.0]');

-- Check if temperature is in acceptable range
SELECT zone_name FROM test_numrange_temperature
WHERE acceptable_temp_celsius @> 5.5::NUMERIC;

SELECT zone_name FROM test_numrange_temperature
WHERE acceptable_temp_celsius @> -15.0::NUMERIC;

SELECT zone_name FROM test_numrange_temperature
WHERE acceptable_temp_celsius @> 30.0::NUMERIC;

-- Find zones with overlapping temperature ranges
SELECT z1.zone_name AS zone1, z2.zone_name AS zone2
FROM test_numrange_temperature z1
CROSS JOIN test_numrange_temperature z2
WHERE z1.zone_id < z2.zone_id
  AND z1.acceptable_temp_celsius && z2.acceptable_temp_celsius;

-- ============================================================================
-- Section 10: Real-world Use Case - Measurement Tolerances
-- ============================================================================

CREATE TABLE test_numrange_measurements (
    measurement_id INT PRIMARY KEY,
    part_name VARCHAR(200),
    target_dimension_mm NUMERIC(10, 3),
    acceptable_range NUMRANGE  -- Tolerance range
);

INSERT INTO test_numrange_measurements VALUES
    (1, 'Shaft Diameter', 25.400, '[25.390,25.410]'),  -- ±0.01mm tolerance
    (2, 'Plate Thickness', 5.000, '[4.950,5.050]'),    -- ±0.05mm tolerance
    (3, 'Bearing Width', 12.000, '[11.990,12.010]');   -- ±0.01mm tolerance

-- Check if actual measurement is within tolerance
SELECT part_name,
       target_dimension_mm,
       acceptable_range @> 25.395::NUMERIC AS is_acceptable
FROM test_numrange_measurements WHERE measurement_id = 1;

SELECT part_name,
       target_dimension_mm,
       acceptable_range @> 5.055::NUMERIC AS is_acceptable
FROM test_numrange_measurements WHERE measurement_id = 2;

-- Find parts where measurement is out of tolerance
CREATE TABLE test_actual_measurements (
    measurement_id INT,
    actual_value NUMERIC(10, 3)
);

INSERT INTO test_actual_measurements VALUES (1, 25.405), (2, 5.055), (3, 11.985);

SELECT m.part_name,
       m.target_dimension_mm,
       a.actual_value,
       m.acceptable_range @> a.actual_value AS within_tolerance
FROM test_numrange_measurements m
JOIN test_actual_measurements a ON m.measurement_id = a.measurement_id
ORDER BY m.measurement_id;

-- ============================================================================
-- Section 11: Real-world Use Case - Percentage Ranges
-- ============================================================================

CREATE TABLE test_numrange_percentages (
    grade_id INT PRIMARY KEY,
    grade_letter VARCHAR(10),
    percentage_range NUMRANGE
);

INSERT INTO test_numrange_percentages VALUES
    (1, 'A', '[90.0,100.0]'),
    (2, 'B', '[80.0,90.0)'),
    (3, 'C', '[70.0,80.0)'),
    (4, 'D', '[60.0,70.0)'),
    (5, 'F', '[0.0,60.0)');

-- Find letter grade for percentage
SELECT grade_letter FROM test_numrange_percentages
WHERE percentage_range @> 87.5::NUMERIC;

SELECT grade_letter FROM test_numrange_percentages
WHERE percentage_range @> 72.3::NUMERIC;

SELECT grade_letter FROM test_numrange_percentages
WHERE percentage_range @> 55.0::NUMERIC;

-- Note the importance of bound types: 90.0 is an A, not a B
SELECT grade_letter FROM test_numrange_percentages
WHERE percentage_range @> 90.0::NUMERIC;

-- ============================================================================
-- Section 12: GiST Indexing for NUMRANGE
-- ============================================================================

CREATE TABLE test_numrange_indexing (
    id INT PRIMARY KEY,
    value_range NUMRANGE
);

CREATE INDEX idx_numrange_gist ON test_numrange_indexing USING GIST (value_range);

INSERT INTO test_numrange_indexing
SELECT i, numrange(i::NUMERIC * 0.5, (i + 10)::NUMERIC * 0.5)
FROM generate_series(1, 1000) i;

-- Queries using GiST index
SELECT id FROM test_numrange_indexing WHERE value_range @> 25.5::NUMERIC ORDER BY id LIMIT 5;
SELECT id FROM test_numrange_indexing WHERE value_range && '[40.0,50.0)'::NUMRANGE ORDER BY id LIMIT 5;

-- ============================================================================
-- Section 13: Range Adjacency
-- ============================================================================

CREATE TABLE test_numrange_adjacency (
    id INT PRIMARY KEY,
    range_value NUMRANGE
);

INSERT INTO test_numrange_adjacency VALUES
    (1, '[0.0,10.0)'),
    (2, '[10.0,20.0]'),  -- Could be considered adjacent
    (3, '[20.0,30.0)');

-- Check adjacency
SELECT r1.id AS id1, r2.id AS id2,
       r1.range_value -|- r2.range_value AS are_adjacent
FROM test_numrange_adjacency r1
CROSS JOIN test_numrange_adjacency r2
WHERE r1.id < r2.id
ORDER BY r1.id, r2.id;

-- Note: For numeric ranges, adjacency depends on exact boundary values
-- [0.0,10.0) and [10.0,20.0) are adjacent
-- [0.0,10.0) and (10.0,20.0) are NOT adjacent (gap exists)

-- ============================================================================
-- Section 14: Empty and NULL Ranges
-- ============================================================================

CREATE TABLE test_numrange_special (
    id INT PRIMARY KEY,
    range_value NUMRANGE,
    description VARCHAR(200)
);

INSERT INTO test_numrange_special VALUES
    (1, '[1.0,10.0)', 'Normal range'),
    (2, 'empty', 'Empty range'),
    (3, NULL, 'NULL range');

SELECT id, range_value,
       isempty(range_value) AS is_empty,
       range_value IS NULL AS is_null
FROM test_numrange_special ORDER BY id;

-- Operations on empty and NULL
SELECT id,
       lower(range_value) AS lower_bound,
       upper(range_value) AS upper_bound
FROM test_numrange_special ORDER BY id;

-- ============================================================================
-- Section 15: Multirange for Non-Contiguous Intervals
-- ============================================================================

CREATE TABLE test_numrange_multi (
    id INT PRIMARY KEY,
    valid_ranges NUMMULTIRANGE
);

INSERT INTO test_numrange_multi VALUES
    (1, '{[0.0,10.0), [20.0,30.0), [40.0,50.0)}'),
    (2, '{[100.0,200.0)}');

SELECT id, valid_ranges FROM test_numrange_multi ORDER BY id;

-- Check containment in multirange
SELECT id FROM test_numrange_multi WHERE valid_ranges @> 5.5::NUMERIC ORDER BY id;
SELECT id FROM test_numrange_multi WHERE valid_ranges @> 25.0::NUMERIC ORDER BY id;
SELECT id FROM test_numrange_multi WHERE valid_ranges @> 15.0::NUMERIC ORDER BY id;

-- ============================================================================
-- Section 16: Scientific Data Example
-- ============================================================================

CREATE TABLE test_numrange_scientific (
    experiment_id INT PRIMARY KEY,
    experiment_name VARCHAR(200),
    valid_ph_range NUMRANGE,
    valid_salinity_ppt NUMRANGE  -- Parts per thousand
);

INSERT INTO test_numrange_scientific VALUES
    (1, 'Freshwater Biology', '[6.5,8.5]', '[0.0,0.5]'),
    (2, 'Marine Biology', '[7.8,8.4]', '[33.0,37.0]'),
    (3, 'Acidic Environment', '[3.0,5.0]', '[0.0,1.0]');

-- Check if conditions are suitable for experiment
SELECT experiment_name
FROM test_numrange_scientific
WHERE valid_ph_range @> 7.2::NUMERIC
  AND valid_salinity_ppt @> 0.1::NUMERIC;

SELECT experiment_name
FROM test_numrange_scientific
WHERE valid_ph_range @> 8.1::NUMERIC
  AND valid_salinity_ppt @> 35.0::NUMERIC;

-- ============================================================================
-- Section 17: Financial Ranges Example
-- ============================================================================

CREATE TABLE test_numrange_financial (
    bracket_id INT PRIMARY KEY,
    tax_bracket_name VARCHAR(100),
    income_range NUMRANGE,
    tax_rate DECIMAL(5, 2)  -- Percentage
);

INSERT INTO test_numrange_financial VALUES
    (1, '10% Bracket', '[0.00,11000.00)', 10.00),
    (2, '12% Bracket', '[11000.00,44725.00)', 12.00),
    (3, '22% Bracket', '[44725.00,95375.00)', 22.00),
    (4, '24% Bracket', '[95375.00,182100.00)', 24.00),
    (5, '32% Bracket', '[182100.00,)', 32.00);

-- Find tax bracket for income
SELECT tax_bracket_name, tax_rate
FROM test_numrange_financial
WHERE income_range @> 75000.00::NUMERIC;

SELECT tax_bracket_name, tax_rate
FROM test_numrange_financial
WHERE income_range @> 250000.00::NUMERIC;

-- ============================================================================
-- Section 18: Best Practices
-- ============================================================================

CREATE TABLE test_numrange_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_numrange_best_practices VALUES
    (1, 'NUMRANGE preserves bound types: [a,b) != [a,b] (no canonicalization)'),
    (2, 'Use NUMRANGE for price ranges, measurements, percentages, scientific data'),
    (3, 'Specify precision for NUMERIC types used in ranges'),
    (4, 'GiST indexes enable fast containment and overlap queries'),
    (5, 'Default [lower, upper) for consistency with integer ranges'),
    (6, 'Use inclusive bounds [a,b] when exact boundaries matter'),
    (7, 'Empty ranges represent "no valid interval" (different from NULL)'),
    (8, 'Multiranges for non-contiguous valid intervals'),
    (9, 'Consider boundary precision: (9.999,10.0) has a gap, [9.999,10.0] does not'),
    (10, 'Use @> for "in range" checks instead of >= AND <='),
    (11, 'NUMRANGE ideal for tolerance specifications, tax brackets, grading scales'),
    (12, 'Adjacency (-|-) depends on exact boundary match'),
    (13, 'Range merge useful for combining overlapping or adjacent intervals'),
    (14, 'NULL range vs empty range: NULL = no data, empty = no valid values'),
    (15, 'Numeric ranges more flexible but slightly slower than integer ranges');

SELECT id, guideline FROM test_numrange_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP FUNCTION calculate_total_with_shipping(NUMERIC);
DROP TABLE test_numrange_best_practices;
DROP TABLE test_numrange_financial;
DROP TABLE test_numrange_scientific;
DROP TABLE test_numrange_multi;
DROP TABLE test_numrange_special;
DROP TABLE test_numrange_adjacency;
DROP TABLE test_numrange_indexing;
DROP TABLE test_numrange_percentages;
DROP TABLE test_actual_measurements;
DROP TABLE test_numrange_measurements;
DROP TABLE test_numrange_temperature;
DROP TABLE test_numrange_price_tiers;
DROP TABLE test_numrange_operations;
DROP TABLE test_numrange_overlap;
DROP TABLE test_numrange_containment;
DROP TABLE test_numrange_no_canonical;
DROP TABLE test_numrange_precision;
DROP TABLE test_numrange_basic;

DROP DATABASE test_numrange_db;

-- End of NUMRANGE data type tests
