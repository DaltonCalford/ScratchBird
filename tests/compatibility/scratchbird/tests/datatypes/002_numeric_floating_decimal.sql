-- ScratchBird Native Test: Numeric Floating Point and Decimal Types
-- Test ID: datatypes_002
-- Description: Comprehensive testing of FLOAT32, FLOAT64, DECIMAL, and MONEY types

CREATE DATABASE test_numeric_float_decimal;
\c test_numeric_float_decimal;

-- ============================================================================
-- FLOAT32 Testing (IEEE 754 single precision)
-- ============================================================================

CREATE TABLE test_float32 (
    id INTEGER PRIMARY KEY,
    val_float32 FLOAT32,
    val_real REAL,      -- Alias test
    val_float FLOAT     -- Alias test
);

-- Basic values
INSERT INTO test_float32 VALUES (1, 0.0, 0.0, 0.0);
INSERT INTO test_float32 VALUES (2, 1.0, 1.0, 1.0);
INSERT INTO test_float32 VALUES (3, -1.0, -1.0, -1.0);
INSERT INTO test_float32 VALUES (4, 3.14159265, 3.14159265, 3.14159265);  -- Pi
INSERT INTO test_float32 VALUES (5, 2.71828183, 2.71828183, 2.71828183);  -- e

-- Scientific notation
INSERT INTO test_float32 VALUES (10, 1.23e10, 1.23e10, 1.23e10);
INSERT INTO test_float32 VALUES (11, 4.56e-8, 4.56e-8, 4.56e-8);
INSERT INTO test_float32 VALUES (12, -9.87e15, -9.87e15, -9.87e15);

-- Boundary values
INSERT INTO test_float32 VALUES (20, 3.402823e+38, 3.402823e+38, 3.402823e+38);    -- Near MAX_FLOAT32
INSERT INTO test_float32 VALUES (21, -3.402823e+38, -3.402823e+38, -3.402823e+38); -- Near MIN_FLOAT32
INSERT INTO test_float32 VALUES (22, 1.175494e-38, 1.175494e-38, 1.175494e-38);    -- Near smallest positive

-- Type alias verification
SELECT id FROM test_float32 WHERE val_float32 = val_real AND val_real = val_float;

-- Arithmetic operations
SELECT val_float32,
       val_float32 + 1.0 AS add_one,
       val_float32 * 2.0 AS double_val,
       val_float32 / 2.0 AS half_val
FROM test_float32 WHERE id = 4;

-- Precision loss demonstration
SELECT val_float32,
       val_float32 + 0.000001 AS tiny_increment,
       (val_float32 + 0.000001) - val_float32 AS difference
FROM test_float32 WHERE id = 4;

-- Math functions
SELECT val_float32,
       SQRT(val_float32) AS sqrt_val,
       POWER(val_float32, 2) AS squared,
       ABS(val_float32) AS abs_val
FROM test_float32 WHERE val_float32 > 0 LIMIT 3;

-- Aggregate functions
SELECT AVG(val_float32) AS avg_val,
       STDDEV(val_float32) AS stddev_val,
       VARIANCE(val_float32) AS variance_val
FROM test_float32 WHERE id < 10;

-- Special values
INSERT INTO test_float32 VALUES (30, 'Infinity', 'Infinity', 'Infinity');
INSERT INTO test_float32 VALUES (31, '-Infinity', '-Infinity', '-Infinity');
INSERT INTO test_float32 VALUES (32, 'NaN', 'NaN', 'NaN');

SELECT id, val_float32 FROM test_float32 WHERE id >= 30 ORDER BY id;

-- Comparison with special values
SELECT COUNT(*) FROM test_float32 WHERE val_float32 IS NOT NULL AND val_float32 <> 'NaN';

-- ============================================================================
-- FLOAT64 Testing (IEEE 754 double precision)
-- ============================================================================

CREATE TABLE test_float64 (
    id INTEGER PRIMARY KEY,
    val_float64 FLOAT64,
    val_double DOUBLE,                  -- Alias test
    val_double_precision DOUBLE PRECISION  -- Alias test
);

-- High precision values
INSERT INTO test_float64 VALUES (1, 0.0, 0.0, 0.0);
INSERT INTO test_float64 VALUES (2, 3.141592653589793, 3.141592653589793, 3.141592653589793);  -- Pi
INSERT INTO test_float64 VALUES (3, 2.718281828459045, 2.718281828459045, 2.718281828459045);  -- e
INSERT INTO test_float64 VALUES (4, 1.6180339887498948, 1.6180339887498948, 1.6180339887498948);  -- Golden ratio

-- Very large and small numbers
INSERT INTO test_float64 VALUES (10, 1.7976931348623157e+308, 1.7976931348623157e+308, 1.7976931348623157e+308);  -- Near MAX
INSERT INTO test_float64 VALUES (11, 2.2250738585072014e-308, 2.2250738585072014e-308, 2.2250738585072014e-308);  -- Near smallest positive
INSERT INTO test_float64 VALUES (12, -1.7976931348623157e+308, -1.7976931348623157e+308, -1.7976931348623157e+308);

-- Type alias verification
SELECT id FROM test_float64 WHERE val_float64 = val_double AND val_double = val_double_precision;

-- Precision comparison with FLOAT32
CREATE TABLE float_precision_compare (
    id INTEGER PRIMARY KEY,
    as_float32 FLOAT32,
    as_float64 FLOAT64
);

INSERT INTO float_precision_compare VALUES (1, 3.141592653589793, 3.141592653589793);
INSERT INTO float_precision_compare VALUES (2, 1.23456789012345, 1.23456789012345);

SELECT id, as_float32, as_float64, (as_float64 - CAST(as_float32 AS FLOAT64)) AS precision_diff
FROM float_precision_compare;

-- Trigonometric functions
SELECT val_float64,
       SIN(val_float64) AS sine,
       COS(val_float64) AS cosine,
       TAN(val_float64) AS tangent
FROM test_float64 WHERE id = 2;

-- Exponential and logarithmic
SELECT val_float64,
       EXP(val_float64) AS exponential,
       LN(val_float64) AS natural_log,
       LOG10(val_float64) AS log_base_10
FROM test_float64 WHERE id = 3 AND val_float64 > 0;

-- ============================================================================
-- DECIMAL Testing (arbitrary precision)
-- ============================================================================

CREATE TABLE test_decimal (
    id INTEGER PRIMARY KEY,
    dec_default DECIMAL,
    dec_10_2 DECIMAL(10, 2),    -- 10 total digits, 2 decimal places
    dec_18_6 DECIMAL(18, 6),    -- 18 total digits, 6 decimal places
    dec_38_10 DECIMAL(38, 10)   -- Maximum precision
);

-- Exact decimal values
INSERT INTO test_decimal VALUES (1, 0, 0.00, 0.000000, 0.0000000000);
INSERT INTO test_decimal VALUES (2, 123.45, 123.45, 123.456789, 123.4567890123);
INSERT INTO test_decimal VALUES (3, -999.99, -999.99, -999.999999, -999.9999999999);

-- Boundary values for DECIMAL(10,2)
INSERT INTO test_decimal VALUES (10, NULL, 99999999.99, NULL, NULL);   -- MAX for (10,2)
INSERT INTO test_decimal VALUES (11, NULL, -99999999.99, NULL, NULL);  -- MIN for (10,2)

-- Exact arithmetic (no rounding errors unlike floating point)
SELECT dec_10_2,
       dec_10_2 + 0.01 AS add_penny,
       dec_10_2 * 1.05 AS add_5_percent,
       dec_10_2 / 3 AS divide_by_three
FROM test_decimal WHERE id = 2;

-- Precision preservation
SELECT dec_18_6,
       dec_18_6 + 0.000001 AS tiny_increment,
       (dec_18_6 + 0.000001) - dec_18_6 AS exact_difference
FROM test_decimal WHERE id = 2;

-- Rounding and truncation
SELECT dec_10_2,
       ROUND(dec_10_2) AS rounded,
       TRUNC(dec_10_2) AS truncated,
       CEIL(dec_10_2) AS ceiling,
       FLOOR(dec_10_2) AS floor_val
FROM test_decimal WHERE id = 2;

-- Scale overflow test (should round or error)
-- INSERT INTO test_decimal VALUES (20, NULL, 123.456, NULL, NULL);  -- Too many decimal places

-- Comparison with floating point
CREATE TABLE decimal_vs_float (
    id INTEGER PRIMARY KEY,
    as_decimal DECIMAL(18, 10),
    as_float64 FLOAT64
);

INSERT INTO decimal_vs_float VALUES (1, 0.1, 0.1);
INSERT INTO decimal_vs_float VALUES (2, 0.2, 0.2);
INSERT INTO decimal_vs_float VALUES (3, 0.3, 0.3);

-- Demonstrate floating point vs decimal precision
SELECT id, as_decimal, as_float64,
       as_decimal + as_decimal + as_decimal AS decimal_sum,
       as_float64 + as_float64 + as_float64 AS float_sum
FROM decimal_vs_float WHERE id = 1;

-- ============================================================================
-- MONEY Testing (currency type)
-- ============================================================================

CREATE TABLE test_money (
    id INTEGER PRIMARY KEY,
    amount MONEY,
    currency_code CHAR(3)
);

-- Currency values
INSERT INTO test_money VALUES (1, 0.00, 'USD');
INSERT INTO test_money VALUES (2, 1234.56, 'USD');
INSERT INTO test_money VALUES (3, -500.00, 'USD');
INSERT INTO test_money VALUES (4, 999999.99, 'EUR');
INSERT INTO test_money VALUES (5, 0.01, 'JPY');  -- Smallest unit

-- Money arithmetic
SELECT amount,
       amount * 1.15 AS with_15_percent_tax,
       amount / 100 AS in_dollars,
       amount - 100.00 AS minus_hundred
FROM test_money WHERE id = 2;

-- Aggregate operations on money
SELECT currency_code,
       SUM(amount) AS total,
       AVG(amount) AS average,
       MIN(amount) AS minimum,
       MAX(amount) AS maximum
FROM test_money
GROUP BY currency_code
ORDER BY currency_code;

-- Money formatting (if supported)
SELECT id, amount, TO_CHAR(amount, '$9,999.99') AS formatted FROM test_money WHERE id = 2;

-- Exchange rate calculation
CREATE TABLE exchange_rates (
    from_currency CHAR(3),
    to_currency CHAR(3),
    rate DECIMAL(10, 6)
);

INSERT INTO exchange_rates VALUES ('USD', 'EUR', 0.85);
INSERT INTO exchange_rates VALUES ('USD', 'GBP', 0.73);
INSERT INTO exchange_rates VALUES ('EUR', 'USD', 1.18);

SELECT tm.id, tm.amount, tm.currency_code,
       tm.amount * er.rate AS converted_amount,
       er.to_currency
FROM test_money tm
JOIN exchange_rates er ON tm.currency_code = er.from_currency
WHERE tm.id = 2;

-- ============================================================================
-- NULL Handling
-- ============================================================================

CREATE TABLE numeric_nulls (
    id INTEGER PRIMARY KEY,
    nullable_float32 FLOAT32,
    nullable_float64 FLOAT64,
    nullable_decimal DECIMAL(10, 2),
    nullable_money MONEY
);

INSERT INTO numeric_nulls VALUES (1, NULL, NULL, NULL, NULL);
INSERT INTO numeric_nulls VALUES (2, 1.5, 2.5, 3.50, 4.50);
INSERT INTO numeric_nulls VALUES (3, NULL, 5.5, NULL, 7.50);

SELECT COUNT(*) AS null_float32 FROM numeric_nulls WHERE nullable_float32 IS NULL;
SELECT COALESCE(nullable_decimal, 0.00) AS coalesced_decimal FROM numeric_nulls;

-- NULL arithmetic
SELECT id, nullable_float32, nullable_float32 + 10 AS add_result FROM numeric_nulls;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE numeric_nulls;
DROP TABLE exchange_rates;
DROP TABLE test_money;
DROP TABLE decimal_vs_float;
DROP TABLE test_decimal;
DROP TABLE float_precision_compare;
DROP TABLE test_float64;
DROP TABLE test_float32;

DROP DATABASE test_numeric_float_decimal;
