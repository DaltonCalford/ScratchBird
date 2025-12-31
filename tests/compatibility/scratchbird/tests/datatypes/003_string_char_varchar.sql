-- ScratchBird Native Test: String Types - CHAR and VARCHAR
-- Test ID: datatypes_003
-- Description: Comprehensive testing of CHAR(n) and VARCHAR(n) types

CREATE DATABASE test_string_char_varchar;
\c test_string_char_varchar;

-- ============================================================================
-- CHAR(n) Testing (fixed-length strings, space-padded)
-- ============================================================================

CREATE TABLE test_char (
    id INTEGER PRIMARY KEY,
    char1 CHAR(1),
    char10 CHAR(10),
    char50 CHAR(50),
    char255 CHAR(255)
);

-- Basic values
INSERT INTO test_char VALUES (1, 'A', 'Hello', 'Short string', 'Test');
INSERT INTO test_char VALUES (2, 'Z', 'World', 'Medium length string here', 'Another test value');
INSERT INTO test_char VALUES (3, '!', '1234567890', 'Exactly 50 characters padding test right here!', 'Special chars: @#$%^&*()');

-- Empty and single character
INSERT INTO test_char VALUES (10, '', '', '', '');
INSERT INTO test_char VALUES (11, 'X', 'X', 'X', 'X');

-- Whitespace handling
INSERT INTO test_char VALUES (20, ' ', '  spaces  ', 'leading and trailing spaces', '   padded   ');
INSERT INTO test_char VALUES (21, '\t', 'tab\there', 'newline\ntest', 'mixed\twhite\nspace');

-- Length verification (CHAR pads with spaces to fixed length)
SELECT id, LENGTH(char10) AS len_char10, LENGTH(char50) AS len_char50 FROM test_char WHERE id = 1;

-- Trailing space behavior
SELECT id, char10, RTRIM(char10) AS trimmed FROM test_char WHERE id = 1;

-- Comparison (trailing spaces may be ignored in comparisons)
SELECT COUNT(*) FROM test_char WHERE char10 = 'Hello';
SELECT COUNT(*) FROM test_char WHERE char10 = 'Hello     ';  -- Should match if space-padded

-- UPPER/LOWER case conversions
SELECT id, char10, UPPER(char10) AS uppercase, LOWER(char10) AS lowercase FROM test_char WHERE id = 1;

-- ============================================================================
-- VARCHAR(n) Testing (variable-length strings, no padding)
-- ============================================================================

CREATE TABLE test_varchar (
    id INTEGER PRIMARY KEY,
    varchar1 VARCHAR(1),
    varchar10 VARCHAR(10),
    varchar50 VARCHAR(50),
    varchar255 VARCHAR(255),
    varchar1000 VARCHAR(1000)
);

-- Basic values
INSERT INTO test_varchar VALUES (1, 'A', 'Hello', 'Short', 'Medium length', 'Long text here');
INSERT INTO test_varchar VALUES (2, 'B', 'World', 'Test string', 'Another test', 'More text content');
INSERT INTO test_varchar VALUES (3, '!', '123', 'Numbers', '123-456-7890', 'Phone: 555-1234');

-- Empty strings
INSERT INTO test_varchar VALUES (10, '', '', '', '', '');

-- Length verification (VARCHAR stores actual length, no padding)
SELECT id, LENGTH(varchar10) AS len10, LENGTH(varchar50) AS len50, LENGTH(varchar255) AS len255
FROM test_varchar WHERE id = 1;

-- Maximum length test
INSERT INTO test_varchar VALUES (20, 'X', 'XXXXXXXXXX',
    'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX',  -- 50 chars
    'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX',  -- 255 chars
    NULL);

-- Overflow test (should fail or truncate)
-- INSERT INTO test_varchar VALUES (30, 'AB', 'TooLongText', '', '', '');  -- Too long for VARCHAR(1)

-- ============================================================================
-- String Concatenation
-- ============================================================================

SELECT varchar10 || ' ' || varchar50 AS concatenated FROM test_varchar WHERE id = 1;
SELECT CONCAT(varchar10, ' - ', varchar50, ' - ', varchar255) AS multi_concat FROM test_varchar WHERE id = 1;

-- Concatenation with NULL (NULL propagates)
INSERT INTO test_varchar VALUES (40, 'A', NULL, 'Test', 'Value', 'Text');
SELECT varchar10 || varchar50 AS concat_with_null FROM test_varchar WHERE id = 40;
SELECT CONCAT(varchar10, COALESCE(varchar50, ''), varchar255) AS concat_coalesce FROM test_varchar WHERE id = 40;

-- ============================================================================
-- UTF-8 and Unicode Testing
-- ============================================================================

CREATE TABLE test_unicode (
    id INTEGER PRIMARY KEY,
    ascii_char CHAR(20),
    utf8_char CHAR(20),
    ascii_varchar VARCHAR(50),
    utf8_varchar VARCHAR(50)
);

-- ASCII characters
INSERT INTO test_unicode VALUES (1, 'ASCII text', 'ASCII text', 'Standard ASCII', 'Standard ASCII');

-- UTF-8 characters (accented, emoji, CJK)
INSERT INTO test_unicode VALUES (2, 'Café', 'Café', 'Résumé', 'Résumé');
INSERT INTO test_unicode VALUES (3, 'Привет', 'Привет', 'Здравствуйте', 'Здравствуйте');  -- Russian
INSERT INTO test_unicode VALUES (4, '你好', '你好', '你好世界', '你好世界');  -- Chinese
INSERT INTO test_unicode VALUES (5, '🎉🎊', '🎉🎊', 'Party 🎉 Time', 'Celebration 🎊');  -- Emoji

-- Character length vs byte length
SELECT id, utf8_varchar, LENGTH(utf8_varchar) AS char_len, OCTET_LENGTH(utf8_varchar) AS byte_len
FROM test_unicode WHERE id >= 2 AND id <= 5;

-- Unicode upper/lower (if supported)
SELECT utf8_varchar, UPPER(utf8_varchar) AS upper, LOWER(utf8_varchar) AS lower
FROM test_unicode WHERE id = 2;

-- ============================================================================
-- Pattern Matching (LIKE, ILIKE)
-- ============================================================================

CREATE TABLE test_patterns (
    id INTEGER PRIMARY KEY,
    text VARCHAR(100)
);

INSERT INTO test_patterns VALUES (1, 'apple');
INSERT INTO test_patterns VALUES (2, 'Apple');
INSERT INTO test_patterns VALUES (3, 'APPLE');
INSERT INTO test_patterns VALUES (4, 'application');
INSERT INTO test_patterns VALUES (5, 'pineapple');
INSERT INTO test_patterns VALUES (6, 'grape');
INSERT INTO test_patterns VALUES (10, 'test123');
INSERT INTO test_patterns VALUES (11, 'test456');
INSERT INTO test_patterns VALUES (12, 'testing');
INSERT INTO test_patterns VALUES (13, '123test');

-- LIKE with wildcards
SELECT id, text FROM test_patterns WHERE text LIKE 'app%' ORDER BY id;
SELECT id, text FROM test_patterns WHERE text LIKE '%apple%' ORDER BY id;
SELECT id, text FROM test_patterns WHERE text LIKE 'test___' ORDER BY id;  -- Exactly 3 chars after 'test'

-- ILIKE (case-insensitive LIKE)
SELECT id, text FROM test_patterns WHERE text ILIKE 'APPLE' ORDER BY id;
SELECT id, text FROM test_patterns WHERE text ILIKE 'APP%' ORDER BY id;

-- NOT LIKE
SELECT id, text FROM test_patterns WHERE text NOT LIKE '%apple%' ORDER BY id;

-- Escape character
INSERT INTO test_patterns VALUES (20, '100% complete');
INSERT INTO test_patterns VALUES (21, '50% done');
SELECT id, text FROM test_patterns WHERE text LIKE '%\%%' ORDER BY id;  -- Find strings with %

-- ============================================================================
-- String Functions
-- ============================================================================

CREATE TABLE test_string_funcs (
    id INTEGER PRIMARY KEY,
    text VARCHAR(100)
);

INSERT INTO test_string_funcs VALUES (1, '  leading spaces');
INSERT INTO test_string_funcs VALUES (2, 'trailing spaces  ');
INSERT INTO test_string_funcs VALUES (3, '  both sides  ');
INSERT INTO test_string_funcs VALUES (4, 'Mixed CASE Text');
INSERT INTO test_string_funcs VALUES (5, 'abcdefghij');

-- Trimming functions
SELECT id, text, LTRIM(text) AS left_trim, RTRIM(text) AS right_trim, TRIM(text) AS both_trim
FROM test_string_funcs WHERE id <= 3;

-- Substring extraction
SELECT id, text, SUBSTRING(text FROM 1 FOR 5) AS first_5, SUBSTRING(text FROM 4) AS from_4th
FROM test_string_funcs WHERE id = 5;

-- Position/Index
SELECT id, text, POSITION('CASE' IN text) AS pos FROM test_string_funcs WHERE id = 4;

-- Replace
SELECT id, text, REPLACE(text, 'spaces', 'SPACES') AS replaced FROM test_string_funcs WHERE id = 1;

-- Length and octet_length
SELECT id, text, LENGTH(text) AS char_count, OCTET_LENGTH(text) AS byte_count
FROM test_string_funcs;

-- Reverse (if supported)
SELECT id, text, REVERSE(text) AS reversed FROM test_string_funcs WHERE id = 5;

-- ============================================================================
-- Collation and Sorting
-- ============================================================================

CREATE TABLE test_collation (
    id INTEGER PRIMARY KEY,
    name VARCHAR(50)
);

INSERT INTO test_collation VALUES (1, 'apple');
INSERT INTO test_collation VALUES (2, 'Apple');
INSERT INTO test_collation VALUES (3, 'APPLE');
INSERT INTO test_collation VALUES (4, 'Banana');
INSERT INTO test_collation VALUES (5, 'cherry');
INSERT INTO test_collation VALUES (6, 'Cherry');
INSERT INTO test_collation VALUES (10, 'Café');
INSERT INTO test_collation VALUES (11, 'cafe');
INSERT INTO test_collation VALUES (12, 'Zebra');
INSERT INTO test_collation VALUES (13, 'zebra');

-- Default collation sort
SELECT id, name FROM test_collation ORDER BY name;

-- Case-insensitive comparison
SELECT COUNT(*) FROM test_collation WHERE LOWER(name) = 'apple';
SELECT COUNT(*) FROM test_collation WHERE UPPER(name) = 'CHERRY';

-- ============================================================================
-- CHAR vs VARCHAR Comparison
-- ============================================================================

CREATE TABLE char_vs_varchar (
    id INTEGER PRIMARY KEY,
    fixed_char CHAR(20),
    variable_varchar VARCHAR(20)
);

INSERT INTO char_vs_varchar VALUES (1, 'Test', 'Test');
INSERT INTO char_vs_varchar VALUES (2, 'Short', 'Short');
INSERT INTO char_vs_varchar VALUES (3, 'Twenty chars exactly', 'Twenty chars exactly');

-- Storage length comparison
SELECT id,
       LENGTH(fixed_char) AS char_len,
       LENGTH(variable_varchar) AS varchar_len,
       OCTET_LENGTH(fixed_char) AS char_bytes,
       OCTET_LENGTH(variable_varchar) AS varchar_bytes
FROM char_vs_varchar;

-- Equality test (may differ due to padding)
SELECT id FROM char_vs_varchar WHERE fixed_char = variable_varchar;

-- ============================================================================
-- NULL Handling
-- ============================================================================

CREATE TABLE string_nulls (
    id INTEGER PRIMARY KEY,
    nullable_char CHAR(20),
    nullable_varchar VARCHAR(50)
);

INSERT INTO string_nulls VALUES (1, NULL, NULL);
INSERT INTO string_nulls VALUES (2, 'Value', 'Value');
INSERT INTO string_nulls VALUES (3, NULL, 'Only varchar');
INSERT INTO string_nulls VALUES (4, 'Only char', NULL);

SELECT COUNT(*) AS null_char_count FROM string_nulls WHERE nullable_char IS NULL;
SELECT COUNT(*) AS null_varchar_count FROM string_nulls WHERE nullable_varchar IS NULL;

-- NULL string operations
SELECT id, nullable_varchar, COALESCE(nullable_varchar, 'DEFAULT') AS coalesced
FROM string_nulls;

SELECT id, nullable_char || nullable_varchar AS concat_result FROM string_nulls;

-- ============================================================================
-- Aggregate Functions
-- ============================================================================

CREATE TABLE test_string_agg (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    value VARCHAR(50)
);

INSERT INTO test_string_agg VALUES (1, 'A', 'apple');
INSERT INTO test_string_agg VALUES (2, 'A', 'apricot');
INSERT INTO test_string_agg VALUES (3, 'A', 'avocado');
INSERT INTO test_string_agg VALUES (4, 'B', 'banana');
INSERT INTO test_string_agg VALUES (5, 'B', 'blueberry');
INSERT INTO test_string_agg VALUES (6, 'C', 'cherry');

-- MIN/MAX (lexicographic order)
SELECT category, MIN(value) AS min_val, MAX(value) AS max_val
FROM test_string_agg
GROUP BY category
ORDER BY category;

-- COUNT
SELECT category, COUNT(value) AS count
FROM test_string_agg
GROUP BY category
ORDER BY category;

-- String aggregation (if supported)
SELECT category, STRING_AGG(value, ', ') AS aggregated
FROM test_string_agg
GROUP BY category
ORDER BY category;

-- ============================================================================
-- Special Characters and Escaping
-- ============================================================================

CREATE TABLE test_special_chars (
    id INTEGER PRIMARY KEY,
    text VARCHAR(100)
);

-- Quotes and apostrophes
INSERT INTO test_special_chars VALUES (1, 'It''s a test');  -- Escaped apostrophe
INSERT INTO test_special_chars VALUES (2, 'He said "Hello"');
INSERT INTO test_special_chars VALUES (3, 'Multiple''s apostrophe''s here');

-- Backslashes
INSERT INTO test_special_chars VALUES (10, 'Path: C:\Users\Test');
INSERT INTO test_special_chars VALUES (11, 'Escaped: \\n \\t \\r');

-- Control characters
INSERT INTO test_special_chars VALUES (20, 'Line1\nLine2');
INSERT INTO test_special_chars VALUES (21, 'Tab\tSeparated');
INSERT INTO test_special_chars VALUES (22, 'Return\rCarriage');

SELECT id, text FROM test_special_chars ORDER BY id;

-- ============================================================================
-- Performance Test (Large Strings)
-- ============================================================================

CREATE TABLE test_large_strings (
    id INTEGER PRIMARY KEY,
    small VARCHAR(50),
    medium VARCHAR(500),
    large VARCHAR(5000)
);

-- Insert with varying sizes
INSERT INTO test_large_strings VALUES (
    1,
    'Small string test',
    'Medium string: ' || REPEAT('X', 400),
    'Large string: ' || REPEAT('Y', 4900)
);

-- Verify lengths
SELECT id,
       LENGTH(small) AS small_len,
       LENGTH(medium) AS medium_len,
       LENGTH(large) AS large_len
FROM test_large_strings;

-- Substring on large string
SELECT id, SUBSTRING(large FROM 1 FOR 50) AS first_50 FROM test_large_strings WHERE id = 1;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_large_strings;
DROP TABLE test_special_chars;
DROP TABLE test_string_agg;
DROP TABLE string_nulls;
DROP TABLE char_vs_varchar;
DROP TABLE test_collation;
DROP TABLE test_string_funcs;
DROP TABLE test_patterns;
DROP TABLE test_unicode;
DROP TABLE test_varchar;
DROP TABLE test_char;

DROP DATABASE test_string_char_varchar;
