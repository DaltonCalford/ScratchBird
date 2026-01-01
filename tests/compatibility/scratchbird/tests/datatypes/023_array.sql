-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - ARRAY
-- Description: Comprehensive ARRAY data type testing
-- ============================================================================

-- ARRAY type stores ordered collections of elements of the same type
-- Supports multi-dimensional arrays and various array operations
-- Used for: lists, collections, vector storage, batch operations

-- Create test database
CREATE DATABASE test_array_db;
USE test_array_db;

-- ============================================================================
-- Section 1: Basic 1D Arrays
-- ============================================================================

CREATE TABLE test_array_basic (
    id INT PRIMARY KEY,
    int_array INT[],
    text_array TEXT[],
    description VARCHAR(200)
);

-- Integer arrays
INSERT INTO test_array_basic VALUES (1, ARRAY[1, 2, 3, 4, 5], NULL, 'Simple integer array');
INSERT INTO test_array_basic VALUES (2, ARRAY[10, 20, 30], NULL, 'Three elements');
INSERT INTO test_array_basic VALUES (3, ARRAY[100], NULL, 'Single element');
INSERT INTO test_array_basic VALUES (4, ARRAY[]::INT[], NULL, 'Empty array');
INSERT INTO test_array_basic VALUES (5, ARRAY[1, 2, 3, 2, 1], NULL, 'Duplicate values');

-- Text arrays
UPDATE test_array_basic SET text_array = ARRAY['apple', 'banana', 'cherry'] WHERE id = 1;
UPDATE test_array_basic SET text_array = ARRAY['red', 'green', 'blue'] WHERE id = 2;
UPDATE test_array_basic SET text_array = ARRAY['hello'] WHERE id = 3;
UPDATE test_array_basic SET text_array = ARRAY[]::TEXT[] WHERE id = 4;
UPDATE test_array_basic SET text_array = ARRAY['a', 'b', 'a'] WHERE id = 5;

SELECT id, int_array, text_array, description FROM test_array_basic ORDER BY id;

-- ============================================================================
-- Section 2: Array Element Access
-- ============================================================================

CREATE TABLE test_array_access (
    id INT PRIMARY KEY,
    numbers INT[],
    labels TEXT[]
);

INSERT INTO test_array_access VALUES
    (1, ARRAY[10, 20, 30, 40, 50], ARRAY['first', 'second', 'third', 'fourth', 'fifth']),
    (2, ARRAY[100, 200, 300], ARRAY['a', 'b', 'c']);

-- Access by index (1-based indexing)
SELECT id, numbers[1] AS first_num FROM test_array_access ORDER BY id;
SELECT id, numbers[2] AS second_num FROM test_array_access ORDER BY id;
SELECT id, numbers[5] AS fifth_num FROM test_array_access WHERE id = 1;

-- Access text array elements
SELECT id, labels[1] AS first_label FROM test_array_access ORDER BY id;
SELECT id, labels[3] AS third_label FROM test_array_access ORDER BY id;

-- Negative indexing (from end)
SELECT id, numbers[array_length(numbers, 1)] AS last_num FROM test_array_access ORDER BY id;

-- Out of bounds access returns NULL
SELECT id, numbers[100] AS out_of_bounds FROM test_array_access WHERE id = 1;

-- ============================================================================
-- Section 3: Array Slicing
-- ============================================================================

CREATE TABLE test_array_slicing (
    id INT PRIMARY KEY,
    data INT[]
);

INSERT INTO test_array_slicing VALUES
    (1, ARRAY[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);

-- Slice syntax: array[lower:upper]
SELECT id, data[1:3] AS first_three FROM test_array_slicing WHERE id = 1;
SELECT id, data[4:6] AS middle_three FROM test_array_slicing WHERE id = 1;
SELECT id, data[8:10] AS last_three FROM test_array_slicing WHERE id = 1;

-- Open-ended slices
SELECT id, data[1:5] AS first_five FROM test_array_slicing WHERE id = 1;
SELECT id, data[6:10] AS last_five FROM test_array_slicing WHERE id = 1;

-- ============================================================================
-- Section 4: Multi-dimensional Arrays
-- ============================================================================

CREATE TABLE test_array_multidim (
    id INT PRIMARY KEY,
    matrix INT[][],
    description VARCHAR(200)
);

-- 2D array (matrix)
INSERT INTO test_array_multidim VALUES
    (1, ARRAY[[1, 2, 3], [4, 5, 6], [7, 8, 9]], '3x3 matrix'),
    (2, ARRAY[[10, 20], [30, 40]], '2x2 matrix');

SELECT id, matrix, description FROM test_array_multidim ORDER BY id;

-- Access 2D array elements
SELECT id, matrix[1][1] AS elem_1_1 FROM test_array_multidim WHERE id = 1;
SELECT id, matrix[2][2] AS elem_2_2 FROM test_array_multidim WHERE id = 1;
SELECT id, matrix[3][3] AS elem_3_3 FROM test_array_multidim WHERE id = 1;

-- Access entire row
SELECT id, matrix[1] AS first_row FROM test_array_multidim WHERE id = 1;
SELECT id, matrix[2] AS second_row FROM test_array_multidim WHERE id = 1;

-- ============================================================================
-- Section 5: Array Functions - array_length, cardinality
-- ============================================================================

CREATE TABLE test_array_functions (
    id INT PRIMARY KEY,
    items INT[]
);

INSERT INTO test_array_functions VALUES
    (1, ARRAY[1, 2, 3, 4, 5]),
    (2, ARRAY[10, 20]),
    (3, ARRAY[100]),
    (4, ARRAY[]::INT[]);

-- array_length: length of specified dimension (1-based)
SELECT id, array_length(items, 1) AS length FROM test_array_functions ORDER BY id;

-- cardinality: total number of elements
SELECT id, cardinality(items) AS total_elements FROM test_array_functions ORDER BY id;

-- array_dims: dimension bounds as text
SELECT id, array_dims(items) AS dimensions FROM test_array_functions ORDER BY id;

-- array_lower, array_upper: bounds of dimension
SELECT id, array_lower(items, 1) AS lower_bound FROM test_array_functions WHERE id = 1;
SELECT id, array_upper(items, 1) AS upper_bound FROM test_array_functions WHERE id = 1;

-- ============================================================================
-- Section 6: Array Operators - Concatenation
-- ============================================================================

CREATE TABLE test_array_concatenation (
    id INT PRIMARY KEY,
    arr1 INT[],
    arr2 INT[]
);

INSERT INTO test_array_concatenation VALUES
    (1, ARRAY[1, 2, 3], ARRAY[4, 5, 6]),
    (2, ARRAY[10], ARRAY[20, 30]),
    (3, ARRAY[]::INT[], ARRAY[1, 2]);

-- Array concatenation with ||
SELECT id, arr1 || arr2 AS concatenated FROM test_array_concatenation ORDER BY id;

-- Append single element
SELECT id, arr1 || 99 AS with_appended FROM test_array_concatenation WHERE id = 1;

-- Prepend single element
SELECT id, 0 || arr1 AS with_prepended FROM test_array_concatenation WHERE id = 1;

-- ============================================================================
-- Section 7: Array Comparison
-- ============================================================================

CREATE TABLE test_array_comparison (
    id INT PRIMARY KEY,
    data INT[]
);

INSERT INTO test_array_comparison VALUES
    (1, ARRAY[1, 2, 3]),
    (2, ARRAY[1, 2, 3]),
    (3, ARRAY[3, 2, 1]),
    (4, ARRAY[1, 2]);

-- Equality
SELECT id FROM test_array_comparison WHERE data = ARRAY[1, 2, 3] ORDER BY id;

-- Inequality
SELECT id FROM test_array_comparison WHERE data != ARRAY[1, 2, 3] ORDER BY id;

-- Note: Order matters for equality
SELECT id FROM test_array_comparison WHERE data = ARRAY[3, 2, 1] ORDER BY id;

-- ============================================================================
-- Section 8: Array Containment Operators (@>, <@, &&)
-- ============================================================================

CREATE TABLE test_array_containment (
    id INT PRIMARY KEY,
    tags TEXT[]
);

INSERT INTO test_array_containment VALUES
    (1, ARRAY['postgres', 'database', 'sql']),
    (2, ARRAY['postgres', 'nosql']),
    (3, ARRAY['mysql', 'database', 'sql']),
    (4, ARRAY['mongodb', 'nosql', 'database']);

-- @> operator: left contains right
SELECT id FROM test_array_containment WHERE tags @> ARRAY['postgres'] ORDER BY id;
SELECT id FROM test_array_containment WHERE tags @> ARRAY['database', 'sql'] ORDER BY id;

-- <@ operator: left is contained by right
SELECT id FROM test_array_containment WHERE ARRAY['postgres'] <@ tags ORDER BY id;
SELECT id FROM test_array_containment WHERE ARRAY['postgres', 'sql'] <@ tags ORDER BY id;

-- && operator: overlap (have elements in common)
SELECT id FROM test_array_containment WHERE tags && ARRAY['postgres', 'mysql'] ORDER BY id;
SELECT id FROM test_array_containment WHERE tags && ARRAY['nosql'] ORDER BY id;

-- ============================================================================
-- Section 9: Array Aggregation - array_agg
-- ============================================================================

CREATE TABLE test_array_aggregation (
    category VARCHAR(50),
    value INT
);

INSERT INTO test_array_aggregation VALUES
    ('A', 10),
    ('A', 20),
    ('A', 30),
    ('B', 100),
    ('B', 200);

-- Aggregate values into array
SELECT category, array_agg(value) AS values_array
FROM test_array_aggregation
GROUP BY category ORDER BY category;

-- array_agg with ORDER BY
SELECT category, array_agg(value ORDER BY value DESC) AS values_desc
FROM test_array_aggregation
GROUP BY category ORDER BY category;

-- Aggregate all values
SELECT array_agg(value) AS all_values FROM test_array_aggregation;

-- ============================================================================
-- Section 10: Array Expansion - unnest
-- ============================================================================

CREATE TABLE test_array_unnest (
    id INT PRIMARY KEY,
    items INT[]
);

INSERT INTO test_array_unnest VALUES
    (1, ARRAY[10, 20, 30]),
    (2, ARRAY[100, 200]);

-- unnest: expand array to rows
SELECT id, unnest(items) AS item FROM test_array_unnest ORDER BY id, item;

-- unnest with ordinality (element position)
SELECT id, item, ordinality FROM test_array_unnest, unnest(items) WITH ORDINALITY AS t(item, ordinality)
ORDER BY id, ordinality;

-- ============================================================================
-- Section 11: Array Modification Functions
-- ============================================================================

CREATE TABLE test_array_modification (
    id INT PRIMARY KEY,
    data INT[]
);

INSERT INTO test_array_modification VALUES
    (1, ARRAY[1, 2, 3, 4, 5]);

-- array_append: add element to end
SELECT id, array_append(data, 6) AS with_appended FROM test_array_modification WHERE id = 1;

-- array_prepend: add element to beginning
SELECT id, array_prepend(0, data) AS with_prepended FROM test_array_modification WHERE id = 1;

-- array_cat: concatenate arrays
SELECT id, array_cat(data, ARRAY[6, 7, 8]) AS concatenated FROM test_array_modification WHERE id = 1;

-- array_remove: remove all occurrences of element
INSERT INTO test_array_modification VALUES (2, ARRAY[1, 2, 3, 2, 4, 2, 5]);
SELECT id, array_remove(data, 2) AS without_twos FROM test_array_modification WHERE id = 2;

-- array_replace: replace all occurrences
SELECT id, array_replace(data, 2, 99) AS with_replaced FROM test_array_modification WHERE id = 2;

-- ============================================================================
-- Section 12: Array Searching
-- ============================================================================

CREATE TABLE test_array_search (
    id INT PRIMARY KEY,
    numbers INT[],
    words TEXT[]
);

INSERT INTO test_array_search VALUES
    (1, ARRAY[10, 20, 30, 40, 50], ARRAY['apple', 'banana', 'cherry']),
    (2, ARRAY[5, 15, 25, 35], ARRAY['dog', 'cat', 'bird']);

-- array_position: find first position of element
SELECT id, array_position(numbers, 30) AS position_of_30 FROM test_array_search WHERE id = 1;
SELECT id, array_position(words, 'banana') AS position_of_banana FROM test_array_search WHERE id = 1;

-- Element not found returns NULL
SELECT id, array_position(numbers, 999) AS not_found FROM test_array_search WHERE id = 1;

-- array_positions: find all positions
INSERT INTO test_array_search VALUES (3, ARRAY[1, 2, 3, 2, 4, 2], NULL);
SELECT id, array_positions(numbers, 2) AS all_positions_of_2 FROM test_array_search WHERE id = 3;

-- ============================================================================
-- Section 13: Array Type Arrays
-- ============================================================================

-- Arrays of different types
CREATE TABLE test_array_types (
    id INT PRIMARY KEY,
    bool_array BOOLEAN[],
    float_array FLOAT[],
    decimal_array DECIMAL(10,2)[],
    date_array DATE[],
    timestamp_array TIMESTAMP[]
);

INSERT INTO test_array_types VALUES
    (1,
     ARRAY[true, false, true],
     ARRAY[1.5, 2.7, 3.14],
     ARRAY[10.50, 20.99, 30.25],
     ARRAY['2024-01-01'::DATE, '2024-06-15'::DATE, '2024-12-31'::DATE],
     ARRAY['2024-01-01 10:00:00'::TIMESTAMP, '2024-01-01 15:30:00'::TIMESTAMP]);

SELECT id, bool_array, float_array, decimal_array FROM test_array_types WHERE id = 1;
SELECT id, date_array, timestamp_array FROM test_array_types WHERE id = 1;

-- ============================================================================
-- Section 14: Array in WHERE Clauses
-- ============================================================================

CREATE TABLE test_array_filtering (
    id INT PRIMARY KEY,
    tags TEXT[]
);

INSERT INTO test_array_filtering VALUES
    (1, ARRAY['urgent', 'bug', 'backend']),
    (2, ARRAY['feature', 'frontend']),
    (3, ARRAY['urgent', 'feature', 'database']),
    (4, ARRAY['documentation']);

-- Find rows with specific tag
SELECT id FROM test_array_filtering WHERE 'urgent' = ANY(tags) ORDER BY id;
SELECT id FROM test_array_filtering WHERE 'frontend' = ANY(tags) ORDER BY id;

-- Find rows where all tags are in a set
SELECT id FROM test_array_filtering WHERE tags <@ ARRAY['urgent', 'bug', 'backend', 'database'] ORDER BY id;

-- Find rows with overlapping tags
SELECT id FROM test_array_filtering WHERE tags && ARRAY['urgent', 'documentation'] ORDER BY id;

-- ============================================================================
-- Section 15: Array Sorting
-- ============================================================================

CREATE TABLE test_array_sorting (
    id INT PRIMARY KEY,
    unsorted INT[]
);

INSERT INTO test_array_sorting VALUES
    (1, ARRAY[5, 2, 8, 1, 9, 3]),
    (2, ARRAY[100, 50, 75, 25]);

-- array_sort: sort array in ascending order
SELECT id, array_sort(unsorted) AS sorted_asc FROM test_array_sorting ORDER BY id;

-- Sort descending (reverse)
SELECT id, array_reverse(array_sort(unsorted)) AS sorted_desc FROM test_array_sorting ORDER BY id;

-- ============================================================================
-- Section 16: Array Indexing
-- ============================================================================

CREATE TABLE test_array_indexing (
    id INT PRIMARY KEY,
    tags TEXT[]
);

-- GIN index for array containment operations
CREATE INDEX idx_tags_gin ON test_array_indexing USING GIN (tags);

INSERT INTO test_array_indexing VALUES
    (1, ARRAY['postgres', 'database']),
    (2, ARRAY['mysql', 'database']),
    (3, ARRAY['mongodb', 'nosql']),
    (4, ARRAY['redis', 'cache', 'nosql']);

-- Queries using GIN index (fast)
SELECT id FROM test_array_indexing WHERE tags @> ARRAY['database'] ORDER BY id;
SELECT id FROM test_array_indexing WHERE tags @> ARRAY['nosql'] ORDER BY id;
SELECT id FROM test_array_indexing WHERE tags && ARRAY['cache', 'database'] ORDER BY id;

-- ============================================================================
-- Section 17: NULL Handling in Arrays
-- ============================================================================

CREATE TABLE test_array_nulls (
    id INT PRIMARY KEY,
    data INT[]
);

INSERT INTO test_array_nulls VALUES
    (1, ARRAY[1, 2, 3]),
    (2, NULL),  -- NULL array
    (3, ARRAY[1, NULL, 3]),  -- Array with NULL element
    (4, ARRAY[]::INT[]);  -- Empty array

SELECT id, data, data IS NULL AS is_null_array FROM test_array_nulls ORDER BY id;

-- Array length with NULLs
SELECT id, array_length(data, 1) AS length FROM test_array_nulls ORDER BY id;

-- Access NULL element
SELECT id, data[2] AS second_element FROM test_array_nulls WHERE id = 3;

-- ============================================================================
-- Section 18: Real-world Use Case - Product Tags
-- ============================================================================

CREATE TABLE test_array_product_tags (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    tags TEXT[],
    price DECIMAL(10, 2)
);

CREATE INDEX idx_product_tags ON test_array_product_tags USING GIN (tags);

INSERT INTO test_array_product_tags VALUES
    (1, 'Laptop Pro', ARRAY['electronics', 'computers', 'portable', 'work'], 1299.99),
    (2, 'Wireless Mouse', ARRAY['electronics', 'accessories', 'wireless'], 29.99),
    (3, 'Office Desk', ARRAY['furniture', 'office', 'work'], 299.99),
    (4, 'Gaming Headset', ARRAY['electronics', 'gaming', 'audio'], 99.99),
    (5, 'Desk Lamp', ARRAY['lighting', 'office', 'accessories'], 49.99);

-- Find all electronics
SELECT product_id, product_name FROM test_array_product_tags
WHERE tags @> ARRAY['electronics'] ORDER BY product_id;

-- Find products for office/work
SELECT product_id, product_name FROM test_array_product_tags
WHERE tags && ARRAY['office', 'work'] ORDER BY product_id;

-- Find wireless products
SELECT product_id, product_name FROM test_array_product_tags
WHERE 'wireless' = ANY(tags) ORDER BY product_id;

-- ============================================================================
-- Section 19: Real-world Use Case - User Permissions
-- ============================================================================

CREATE TABLE test_array_user_permissions (
    user_id INT PRIMARY KEY,
    username VARCHAR(100),
    permissions TEXT[]
);

INSERT INTO test_array_user_permissions VALUES
    (1, 'alice', ARRAY['read', 'write', 'delete', 'admin']),
    (2, 'bob', ARRAY['read', 'write']),
    (3, 'charlie', ARRAY['read']);

-- Check if user has specific permission
SELECT user_id, username FROM test_array_user_permissions
WHERE 'admin' = ANY(permissions) ORDER BY user_id;

SELECT user_id, username FROM test_array_user_permissions
WHERE permissions @> ARRAY['write'] ORDER BY user_id;

-- Check if user has all required permissions
SELECT user_id, username FROM test_array_user_permissions
WHERE permissions @> ARRAY['read', 'write', 'delete'] ORDER BY user_id;

-- ============================================================================
-- Section 20: Array Best Practices
-- ============================================================================

CREATE TABLE test_array_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_array_best_practices VALUES
    (1, 'Use arrays for ordered collections of same-type values'),
    (2, 'Create GIN indexes for containment queries (@>, <@, &&)'),
    (3, 'Arrays are 1-indexed (first element is [1], not [0])'),
    (4, 'Use array_agg to aggregate rows into arrays'),
    (5, 'Use unnest to expand arrays into rows'),
    (6, 'Arrays preserve order and allow duplicates'),
    (7, 'For unordered unique values, consider using a separate table'),
    (8, 'Multi-dimensional arrays must be rectangular (all dimensions same size)'),
    (9, 'Array operations are not atomic - consider normalization for critical data'),
    (10, 'Use @> for "contains" and && for "overlaps" with GIN index'),
    (11, 'Arrays are ideal for: tags, permissions, time series, batch operations'),
    (12, 'NULL array vs empty array vs array with NULL elements are different'),
    (13, 'Use array_position to find element index, ANY to check membership'),
    (14, 'Consider JSONB arrays for heterogeneous or nested structures'),
    (15, 'Arrays work well with CTEs and window functions for complex queries');

SELECT id, guideline FROM test_array_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_array_best_practices;
DROP TABLE test_array_user_permissions;
DROP TABLE test_array_product_tags;
DROP TABLE test_array_nulls;
DROP TABLE test_array_indexing;
DROP TABLE test_array_sorting;
DROP TABLE test_array_filtering;
DROP TABLE test_array_types;
DROP TABLE test_array_search;
DROP TABLE test_array_modification;
DROP TABLE test_array_unnest;
DROP TABLE test_array_aggregation;
DROP TABLE test_array_containment;
DROP TABLE test_array_comparison;
DROP TABLE test_array_concatenation;
DROP TABLE test_array_functions;
DROP TABLE test_array_multidim;
DROP TABLE test_array_slicing;
DROP TABLE test_array_access;
DROP TABLE test_array_basic;

DROP DATABASE test_array_db;

-- End of ARRAY data type tests
