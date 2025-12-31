-- ScratchBird Native Test: Numeric Integer Types
-- Test ID: datatypes_001
-- Description: Comprehensive testing of all integer types (INT8, INT16, INT32, INT64, INT128, UINT8, UINT16, UINT32, UINT64)

CREATE DATABASE test_numeric_integers;
\c test_numeric_integers;

-- ============================================================================
-- INT8 Testing (-128 to 127)
-- ============================================================================

CREATE TABLE test_int8 (
    id INTEGER PRIMARY KEY,
    val_int8 INT8
);

-- Boundary values
INSERT INTO test_int8 VALUES (1, -128);  -- MIN_INT8
INSERT INTO test_int8 VALUES (2, 127);   -- MAX_INT8
INSERT INTO test_int8 VALUES (3, 0);
INSERT INTO test_int8 VALUES (4, -1);
INSERT INTO test_int8 VALUES (5, 1);

-- Arithmetic operations
SELECT val_int8, val_int8 + 10 AS add_10, val_int8 * 2 AS mult_2 FROM test_int8 WHERE id = 5;

-- Overflow test (should fail or warn)
-- INSERT INTO test_int8 VALUES (10, 128);   -- Exceeds MAX
-- INSERT INTO test_int8 VALUES (11, -129);  -- Below MIN

-- Comparison operations
SELECT COUNT(*) AS negative_count FROM test_int8 WHERE val_int8 < 0;
SELECT COUNT(*) AS positive_count FROM test_int8 WHERE val_int8 > 0;

-- Aggregate functions
SELECT MIN(val_int8) AS min_val, MAX(val_int8) AS max_val, AVG(val_int8) AS avg_val, SUM(val_int8) AS sum_val FROM test_int8;

-- ============================================================================
-- INT16 Testing (SMALLINT: -32768 to 32767)
-- ============================================================================

CREATE TABLE test_int16 (
    id INTEGER PRIMARY KEY,
    val_int16 INT16,
    val_smallint SMALLINT  -- Alias test
);

INSERT INTO test_int16 VALUES (1, -32768, -32768);  -- MIN_INT16
INSERT INTO test_int16 VALUES (2, 32767, 32767);    -- MAX_INT16
INSERT INTO test_int16 VALUES (3, 0, 0);
INSERT INTO test_int16 VALUES (4, -1000, -1000);
INSERT INTO test_int16 VALUES (5, 1000, 1000);

-- Type alias verification
SELECT id FROM test_int16 WHERE val_int16 = val_smallint;

-- Range queries
SELECT COUNT(*) FROM test_int16 WHERE val_int16 BETWEEN -1000 AND 1000;

-- Division and modulo
SELECT val_int16, val_int16 / 10 AS div_10, val_int16 % 10 AS mod_10 FROM test_int16 WHERE id = 5;

-- ============================================================================
-- INT32 Testing (INTEGER/INT: -2147483648 to 2147483647)
-- ============================================================================

CREATE TABLE test_int32 (
    id INT32 PRIMARY KEY,
    val_int32 INT32,
    val_integer INTEGER,  -- Alias test
    val_int INT           -- Alias test
);

INSERT INTO test_int32 VALUES (1, -2147483648, -2147483648, -2147483648);  -- MIN_INT32
INSERT INTO test_int32 VALUES (2, 2147483647, 2147483647, 2147483647);     -- MAX_INT32
INSERT INTO test_int32 VALUES (3, 0, 0, 0);
INSERT INTO test_int32 VALUES (4, -1000000, -1000000, -1000000);
INSERT INTO test_int32 VALUES (5, 1000000, 1000000, 1000000);

-- Type alias verification
SELECT id FROM test_int32 WHERE val_int32 = val_integer AND val_integer = val_int;

-- Bitwise operations
SELECT val_int32, val_int32 & 15 AS bitwise_and, val_int32 | 15 AS bitwise_or, val_int32 ^ 15 AS bitwise_xor FROM test_int32 WHERE id = 5;

-- Sorting and ordering
SELECT val_int32 FROM test_int32 ORDER BY val_int32 ASC;

-- ============================================================================
-- INT64 Testing (BIGINT: -9223372036854775808 to 9223372036854775807)
-- ============================================================================

CREATE TABLE test_int64 (
    id INTEGER PRIMARY KEY,
    val_int64 INT64,
    val_bigint BIGINT  -- Alias test
);

INSERT INTO test_int64 VALUES (1, -9223372036854775808, -9223372036854775808);  -- MIN_INT64
INSERT INTO test_int64 VALUES (2, 9223372036854775807, 9223372036854775807);    -- MAX_INT64
INSERT INTO test_int64 VALUES (3, 0, 0);
INSERT INTO test_int64 VALUES (4, -1000000000000, -1000000000000);
INSERT INTO test_int64 VALUES (5, 1000000000000, 1000000000000);

-- Large number arithmetic
SELECT val_int64, val_int64 / 1000000 AS millions FROM test_int64 WHERE id = 5;

-- Comparison with INT32
CREATE TABLE int64_vs_int32 (
    id INTEGER PRIMARY KEY,
    large_val INT64,
    overflow_test INT32
);

INSERT INTO int64_vs_int32 VALUES (1, 9223372036854775807, 2147483647);

-- ============================================================================
-- INT128 Testing (16-byte signed integer)
-- ============================================================================

CREATE TABLE test_int128 (
    id INTEGER PRIMARY KEY,
    val_int128 INT128
);

-- Very large values
INSERT INTO test_int128 VALUES (1, 0);
INSERT INTO test_int128 VALUES (2, 1);
INSERT INTO test_int128 VALUES (3, -1);
INSERT INTO test_int128 VALUES (4, 170141183460469231731687303715884105727);   -- Close to MAX_INT128
INSERT INTO test_int128 VALUES (5, -170141183460469231731687303715884105728);  -- Close to MIN_INT128

SELECT id, val_int128 FROM test_int128 ORDER BY val_int128;

-- ============================================================================
-- UINT8 Testing (0 to 255)
-- ============================================================================

CREATE TABLE test_uint8 (
    id INTEGER PRIMARY KEY,
    val_uint8 UINT8
);

INSERT INTO test_uint8 VALUES (1, 0);    -- MIN_UINT8
INSERT INTO test_uint8 VALUES (2, 255);  -- MAX_UINT8
INSERT INTO test_uint8 VALUES (3, 128);
INSERT INTO test_uint8 VALUES (4, 64);
INSERT INTO test_uint8 VALUES (5, 192);

-- Unsigned arithmetic (no negative results)
SELECT val_uint8, val_uint8 + 10 AS add_10, val_uint8 * 2 AS mult_2 FROM test_uint8;

-- Negative value test (should fail)
-- INSERT INTO test_uint8 VALUES (10, -1);  -- Negative not allowed

-- ============================================================================
-- UINT16 Testing (0 to 65535)
-- ============================================================================

CREATE TABLE test_uint16 (
    id INTEGER PRIMARY KEY,
    val_uint16 UINT16
);

INSERT INTO test_uint16 VALUES (1, 0);      -- MIN_UINT16
INSERT INTO test_uint16 VALUES (2, 65535);  -- MAX_UINT16
INSERT INTO test_uint16 VALUES (3, 32768);
INSERT INTO test_uint16 VALUES (4, 16384);
INSERT INTO test_uint16 VALUES (5, 49152);

SELECT AVG(val_uint16) AS avg_val FROM test_uint16;

-- ============================================================================
-- UINT32 Testing (0 to 4294967295)
-- ============================================================================

CREATE TABLE test_uint32 (
    id INTEGER PRIMARY KEY,
    val_uint32 UINT32
);

INSERT INTO test_uint32 VALUES (1, 0);           -- MIN_UINT32
INSERT INTO test_uint32 VALUES (2, 4294967295);  -- MAX_UINT32
INSERT INTO test_uint32 VALUES (3, 2147483648);  -- > MAX_INT32
INSERT INTO test_uint32 VALUES (4, 1000000000);
INSERT INTO test_uint32 VALUES (5, 3000000000);

SELECT val_uint32, val_uint32 / 1000000 AS millions FROM test_uint32 ORDER BY val_uint32 DESC;

-- ============================================================================
-- UINT64 Testing (0 to 18446744073709551615)
-- ============================================================================

CREATE TABLE test_uint64 (
    id INTEGER PRIMARY KEY,
    val_uint64 UINT64
);

INSERT INTO test_uint64 VALUES (1, 0);                     -- MIN_UINT64
INSERT INTO test_uint64 VALUES (2, 18446744073709551615);  -- MAX_UINT64
INSERT INTO test_uint64 VALUES (3, 9223372036854775808);   -- > MAX_INT64
INSERT INTO test_uint64 VALUES (4, 10000000000000000);
INSERT INTO test_uint64 VALUES (5, 15000000000000000);

SELECT MIN(val_uint64) AS min_val, MAX(val_uint64) AS max_val FROM test_uint64;

-- ============================================================================
-- Cross-Type Comparisons and Conversions
-- ============================================================================

CREATE TABLE type_conversions (
    id INTEGER PRIMARY KEY,
    from_int8 INT8,
    from_int16 INT16,
    from_int32 INT32,
    from_int64 INT64,
    to_uint8 UINT8,
    to_uint16 UINT16,
    to_uint32 UINT32
);

INSERT INTO type_conversions VALUES (1, 100, 1000, 100000, 1000000, 100, 1000, 100000);

-- Type casting
SELECT CAST(from_int8 AS INT16) AS int8_to_int16,
       CAST(from_int16 AS INT32) AS int16_to_int32,
       CAST(from_int32 AS INT64) AS int32_to_int64
FROM type_conversions WHERE id = 1;

-- Mixed type arithmetic
SELECT from_int8 + from_int16 AS mixed_add,
       from_int32 * 2 AS mult_result
FROM type_conversions WHERE id = 1;

-- ============================================================================
-- NULL Handling
-- ============================================================================

CREATE TABLE int_nulls (
    id INTEGER PRIMARY KEY,
    nullable_int8 INT8,
    nullable_int32 INT32,
    nullable_int64 INT64
);

INSERT INTO int_nulls VALUES (1, NULL, NULL, NULL);
INSERT INTO int_nulls VALUES (2, 10, 1000, 100000);
INSERT INTO int_nulls VALUES (3, NULL, 2000, 200000);

SELECT COUNT(*) AS null_count FROM int_nulls WHERE nullable_int8 IS NULL;
SELECT COALESCE(nullable_int8, 0) AS coalesced_val FROM int_nulls;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE int_nulls;
DROP TABLE type_conversions;
DROP TABLE test_uint64;
DROP TABLE test_uint32;
DROP TABLE test_uint16;
DROP TABLE test_uint8;
DROP TABLE test_int128;
DROP TABLE int64_vs_int32;
DROP TABLE test_int64;
DROP TABLE test_int32;
DROP TABLE test_int16;
DROP TABLE test_int8;

DROP DATABASE test_numeric_integers;
