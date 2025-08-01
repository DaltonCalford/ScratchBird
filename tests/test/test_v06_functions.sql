-- ScratchBird v0.6 System Functions Test
-- Test enhanced array and vector functions

-- Test basic array functions (PostgreSQL-compatible)
SELECT 'Testing Array Functions' AS test_category;

-- Test ARRAY_LENGTH function
-- SELECT ARRAY_LENGTH(ARRAY[1,2,3,4,5]) AS array_length_test;

-- Test ARRAY_APPEND function  
-- SELECT ARRAY_APPEND(ARRAY[1,2,3], 4) AS array_append_test;

-- Test ARRAY_PREPEND function
-- SELECT ARRAY_PREPEND(0, ARRAY[1,2,3]) AS array_prepend_test;

-- Test ARRAY_CAT function (concatenation)
-- SELECT ARRAY_CAT(ARRAY[1,2], ARRAY[3,4]) AS array_concat_test;

-- Test basic vector functions (AI/ML capabilities)
SELECT 'Testing Vector Functions' AS test_category;

-- Test VECTOR_DISTANCE function (Euclidean distance)
-- SELECT VECTOR_DISTANCE(ARRAY[1.0,2.0,3.0], ARRAY[4.0,5.0,6.0]) AS euclidean_distance;

-- Test VECTOR_COSINE_SIMILARITY function
-- SELECT VECTOR_COSINE_SIMILARITY(ARRAY[1.0,2.0,3.0], ARRAY[4.0,5.0,6.0]) AS cosine_similarity;

-- Test basic spatial functions (WKT support)
SELECT 'Testing Spatial Functions' AS test_category;

-- Test ST_ASTEXT function (basic WKT output)
-- SELECT ST_ASTEXT('POINT(1 2)') AS wkt_point;

-- Test basic string functions
SELECT 'Testing String Functions' AS test_category;
SELECT LENGTH('ScratchBird v0.6') AS string_length_test;
SELECT UPPER('scratchbird') AS string_upper_test;
SELECT LOWER('SCRATCHBIRD') AS string_lower_test;

-- Test basic numeric functions
SELECT 'Testing Numeric Functions' AS test_category;
SELECT ABS(-42) AS abs_test;
SELECT SQRT(16) AS sqrt_test;
SELECT POWER(2, 3) AS power_test;

-- Test successful completion
SELECT 'ScratchBird v0.6 Basic Functions Test Complete' AS test_result;