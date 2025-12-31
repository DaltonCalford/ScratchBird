-- ScratchBird Native Test: String Types - TEXT
-- Test ID: datatypes_004
-- Description: Comprehensive testing of TEXT type (unlimited length strings)

CREATE DATABASE test_string_text;
\c test_string_text;

-- ============================================================================
-- TEXT Basic Testing
-- ============================================================================

CREATE TABLE test_text_basic (
    id INTEGER PRIMARY KEY,
    content TEXT,
    description TEXT
);

-- Basic values
INSERT INTO test_text_basic VALUES (1, 'Short text', 'Simple short string');
INSERT INTO test_text_basic VALUES (2, 'Medium length text content here', 'Testing medium length strings');
INSERT INTO test_text_basic VALUES (3, '', 'Empty string test');
INSERT INTO test_text_basic VALUES (4, NULL, 'NULL value test');

-- Very long text (thousands of characters)
INSERT INTO test_text_basic VALUES (10,
    'Lorem ipsum dolor sit amet, consectetur adipiscing elit. ' ||
    'Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ' ||
    'Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris. ' ||
    'Nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in ' ||
    'reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. ' ||
    'Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia ' ||
    'deserunt mollit anim id est laborum. ' || REPEAT('X', 1000),
    'Long text test with 1000+ characters'
);

-- Multi-line text
INSERT INTO test_text_basic VALUES (20,
    'Line 1: First line of text\n' ||
    'Line 2: Second line with more content\n' ||
    'Line 3: Third line\n' ||
    'Line 4: Final line',
    'Multi-line text test'
);

-- Text with special formatting
INSERT INTO test_text_basic VALUES (30,
    'Paragraph 1:\nThis is the first paragraph.\n\n' ||
    'Paragraph 2:\nThis is the second paragraph with more details.\n\n' ||
    'Paragraph 3:\nConclusion paragraph.',
    'Formatted paragraphs'
);

-- Length verification
SELECT id, LENGTH(content) AS content_length, OCTET_LENGTH(content) AS byte_length
FROM test_text_basic WHERE id <= 4;

-- Substring extraction
SELECT id, SUBSTRING(content FROM 1 FOR 50) AS preview FROM test_text_basic WHERE id = 10;

-- ============================================================================
-- TEXT with UTF-8 Content
-- ============================================================================

CREATE TABLE test_text_utf8 (
    id INTEGER PRIMARY KEY,
    language VARCHAR(20),
    content TEXT
);

-- English
INSERT INTO test_text_utf8 VALUES (1, 'English',
    'The quick brown fox jumps over the lazy dog. ' ||
    'This pangram contains every letter of the English alphabet.');

-- Spanish (accented characters)
INSERT INTO test_text_utf8 VALUES (2, 'Spanish',
    'El veloz murciélago hindú comía feliz cardillo y kiwi. ' ||
    'La cigüeña tocaba el saxofón detrás del palenque de paja.');

-- French (accented characters)
INSERT INTO test_text_utf8 VALUES (3, 'French',
    'Portez ce vieux whisky au juge blond qui fume. ' ||
    'Voilà que je faxe un exemplaire de bon whisky.');

-- German (umlauts)
INSERT INTO test_text_utf8 VALUES (4, 'German',
    'Victor jagt zwölf Boxkämpfer quer über den großen Sylter Deich. ' ||
    'Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.');

-- Russian (Cyrillic)
INSERT INTO test_text_utf8 VALUES (5, 'Russian',
    'Съешь же ещё этих мягких французских булок да выпей чаю. ' ||
    'Широкая электрификация южных губерний даст мощный толчок подъёму сельского хозяйства.');

-- Chinese (CJK characters)
INSERT INTO test_text_utf8 VALUES (6, 'Chinese',
    '天地玄黃，宇宙洪荒。日月盈昃，辰宿列張。' ||
    '寒來暑往，秋收冬藏。閏餘成歲，律呂調陽。');

-- Japanese (mixed Hiragana, Katakana, Kanji)
INSERT INTO test_text_utf8 VALUES (7, 'Japanese',
    'いろはにほへと ちりぬるを わかよたれそ つねならむ。' ||
    'カタカナ・テキスト・サンプル。漢字混在文章例。');

-- Arabic (right-to-left)
INSERT INTO test_text_utf8 VALUES (8, 'Arabic',
    'صِف خَلقَ خَودِ كَمِثلِ الشَمسِ إِذ بَزَغَت — يَحظى الضَجيعُ بِها نَجلاءَ مِعطارِ');

-- Emoji and symbols
INSERT INTO test_text_utf8 VALUES (9, 'Emoji',
    'Hello 👋 World 🌍! This text contains emoji: 😀😃😄😁😆😅🤣😂 ' ||
    'Symbols: ©®™€£¥§¶†‡ Math: ∑∏∫∂∆∇ Arrows: ←→↑↓↔');

-- Character vs byte length for UTF-8
SELECT id, language,
       LENGTH(content) AS char_count,
       OCTET_LENGTH(content) AS byte_count,
       OCTET_LENGTH(content) - LENGTH(content) AS utf8_overhead
FROM test_text_utf8
ORDER BY id;

-- ============================================================================
-- TEXT Pattern Matching and Search
-- ============================================================================

CREATE TABLE test_text_search (
    id INTEGER PRIMARY KEY,
    title VARCHAR(100),
    body TEXT
);

INSERT INTO test_text_search VALUES (1, 'Database Basics',
    'A database is an organized collection of structured information or data. ' ||
    'Databases are typically managed by database management systems (DBMS). ' ||
    'The most common type of database is a relational database.');

INSERT INTO test_text_search VALUES (2, 'SQL Introduction',
    'SQL (Structured Query Language) is a programming language used to manage ' ||
    'and manipulate relational databases. SQL allows users to create, read, ' ||
    'update, and delete database records. It is the standard language for RDBMS.');

INSERT INTO test_text_search VALUES (3, 'MVCC Overview',
    'Multi-Version Concurrency Control (MVCC) is a method used in database systems ' ||
    'to provide concurrent access to the database. MVCC allows readers to access ' ||
    'data without blocking writers, and writers without blocking readers.');

INSERT INTO test_text_search VALUES (4, 'Index Types',
    'Database indexes improve query performance by creating efficient lookup structures. ' ||
    'Common index types include B-Tree, Hash, GiST, GIN, and BRIN. Each index type ' ||
    'is optimized for different use cases and data patterns.');

-- Full-text search with LIKE
SELECT id, title FROM test_text_search WHERE body LIKE '%database%' ORDER BY id;
SELECT id, title FROM test_text_search WHERE body LIKE '%MVCC%' ORDER BY id;

-- Case-insensitive search with ILIKE
SELECT id, title FROM test_text_search WHERE body ILIKE '%SQL%' ORDER BY id;

-- Multiple pattern search
SELECT id, title FROM test_text_search
WHERE body LIKE '%database%' AND body LIKE '%concurrent%'
ORDER BY id;

-- Word boundary search (if supported)
SELECT id, title FROM test_text_search WHERE body LIKE '%index%' ORDER BY id;

-- Position of substring
SELECT id, title, POSITION('database' IN body) AS pos
FROM test_text_search
WHERE body LIKE '%database%'
ORDER BY id;

-- ============================================================================
-- TEXT Manipulation Functions
-- ============================================================================

CREATE TABLE test_text_functions (
    id INTEGER PRIMARY KEY,
    original TEXT
);

INSERT INTO test_text_functions VALUES (1,
    '  Leading and trailing whitespace should be removed.  ');

INSERT INTO test_text_functions VALUES (2,
    'This is a MIXED case STRING with Various CAPITALIZATIONS.');

INSERT INTO test_text_functions VALUES (3,
    'Replace all occurrences of the word test in this test sentence for testing.');

INSERT INTO test_text_functions VALUES (4,
    'abcdefghijklmnopqrstuvwxyz0123456789');

-- Trimming
SELECT id, original, TRIM(original) AS trimmed, LENGTH(TRIM(original)) AS trimmed_len
FROM test_text_functions WHERE id = 1;

-- Case conversion
SELECT id, UPPER(original) AS uppercase, LOWER(original) AS lowercase
FROM test_text_functions WHERE id = 2;

-- Replace
SELECT id, REPLACE(original, 'test', 'TEST') AS replaced
FROM test_text_functions WHERE id = 3;

-- Substring
SELECT id, SUBSTRING(original FROM 1 FOR 10) AS first_10,
       SUBSTRING(original FROM 20 FOR 10) AS chars_20_to_30
FROM test_text_functions WHERE id = 4;

-- Reverse
SELECT id, REVERSE(original) AS reversed FROM test_text_functions WHERE id = 4;

-- ============================================================================
-- TEXT Concatenation and Building
-- ============================================================================

CREATE TABLE test_text_concat (
    id INTEGER PRIMARY KEY,
    part1 TEXT,
    part2 TEXT,
    part3 TEXT
);

INSERT INTO test_text_concat VALUES (1,
    'First paragraph of content. ',
    'Second paragraph with more details. ',
    'Final paragraph with conclusions.'
);

INSERT INTO test_text_concat VALUES (2,
    'Header: Important Document\n\n',
    'Body: This is the main content of the document.\n\n',
    'Footer: End of document.'
);

-- Simple concatenation
SELECT id, part1 || part2 || part3 AS combined FROM test_text_concat WHERE id = 1;

-- Concatenation with separator
SELECT id, part1 || '\n---\n' || part2 || '\n---\n' || part3 AS separated
FROM test_text_concat WHERE id = 2;

-- CONCAT function
SELECT id, CONCAT(part1, part2, part3) AS concatenated FROM test_text_concat;

-- Building large text iteratively
CREATE TABLE test_large_build (
    id INTEGER PRIMARY KEY,
    content TEXT
);

INSERT INTO test_large_build VALUES (1, 'Start: ');
UPDATE test_large_build SET content = content || REPEAT('A', 100) WHERE id = 1;
UPDATE test_large_build SET content = content || REPEAT('B', 200) WHERE id = 1;
UPDATE test_large_build SET content = content || REPEAT('C', 300) WHERE id = 1;

SELECT id, LENGTH(content) AS final_length FROM test_large_build WHERE id = 1;

-- ============================================================================
-- TEXT Storage Efficiency
-- ============================================================================

CREATE TABLE test_text_storage (
    id INTEGER PRIMARY KEY,
    tiny TEXT,
    small TEXT,
    medium TEXT,
    large TEXT,
    huge TEXT
);

INSERT INTO test_text_storage VALUES (
    1,
    'Tiny',
    REPEAT('S', 100),
    REPEAT('M', 1000),
    REPEAT('L', 10000),
    REPEAT('H', 100000)
);

-- Size analysis
SELECT id,
       LENGTH(tiny) AS tiny_len,
       LENGTH(small) AS small_len,
       LENGTH(medium) AS medium_len,
       LENGTH(large) AS large_len,
       LENGTH(huge) AS huge_len
FROM test_text_storage;

SELECT id,
       OCTET_LENGTH(tiny) AS tiny_bytes,
       OCTET_LENGTH(small) AS small_bytes,
       OCTET_LENGTH(medium) AS medium_bytes,
       OCTET_LENGTH(large) AS large_bytes,
       OCTET_LENGTH(huge) AS huge_bytes
FROM test_text_storage;

-- ============================================================================
-- TEXT vs VARCHAR Comparison
-- ============================================================================

CREATE TABLE text_vs_varchar (
    id INTEGER PRIMARY KEY,
    as_text TEXT,
    as_varchar VARCHAR(1000)
);

-- Same content in both columns
INSERT INTO text_vs_varchar VALUES (1,
    'This is identical content stored in both TEXT and VARCHAR columns.',
    'This is identical content stored in both TEXT and VARCHAR columns.'
);

INSERT INTO text_vs_varchar VALUES (2,
    REPEAT('X', 500),
    REPEAT('X', 500)
);

-- Length comparison
SELECT id,
       LENGTH(as_text) AS text_len,
       LENGTH(as_varchar) AS varchar_len,
       OCTET_LENGTH(as_text) AS text_bytes,
       OCTET_LENGTH(as_varchar) AS varchar_bytes
FROM text_vs_varchar;

-- TEXT can exceed VARCHAR limit
INSERT INTO text_vs_varchar VALUES (3,
    REPEAT('Y', 5000),  -- TEXT can handle this
    NULL                -- Can't fit in VARCHAR(1000)
);

SELECT id, LENGTH(as_text) AS text_len FROM text_vs_varchar WHERE id = 3;

-- ============================================================================
-- TEXT NULL Handling
-- ============================================================================

CREATE TABLE test_text_nulls (
    id INTEGER PRIMARY KEY,
    nullable_text TEXT,
    not_null_text TEXT NOT NULL DEFAULT 'Default value'
);

INSERT INTO test_text_nulls (id, nullable_text) VALUES (1, NULL);
INSERT INTO test_text_nulls (id, nullable_text, not_null_text) VALUES (2, 'Has value', 'Also has value');
INSERT INTO test_text_nulls (id, nullable_text) VALUES (3, '');

-- NULL checks
SELECT COUNT(*) AS null_count FROM test_text_nulls WHERE nullable_text IS NULL;
SELECT COUNT(*) AS empty_count FROM test_text_nulls WHERE nullable_text = '';
SELECT COUNT(*) AS null_or_empty FROM test_text_nulls
WHERE nullable_text IS NULL OR nullable_text = '';

-- NULL coalescence
SELECT id, COALESCE(nullable_text, 'NULL_VALUE') AS coalesced FROM test_text_nulls;

-- NULL in concatenation
SELECT id, nullable_text || ' extra' AS concat_result FROM test_text_nulls;

-- ============================================================================
-- TEXT Aggregation
-- ============================================================================

CREATE TABLE test_text_agg (
    id INTEGER PRIMARY KEY,
    category CHAR(1),
    item TEXT
);

INSERT INTO test_text_agg VALUES (1, 'A', 'First item in A');
INSERT INTO test_text_agg VALUES (2, 'A', 'Second item in A');
INSERT INTO test_text_agg VALUES (3, 'A', 'Third item in A');
INSERT INTO test_text_agg VALUES (4, 'B', 'First item in B');
INSERT INTO test_text_agg VALUES (5, 'B', 'Second item in B');
INSERT INTO test_text_agg VALUES (6, 'C', 'Only item in C');

-- String aggregation
SELECT category, STRING_AGG(item, '; ') AS aggregated
FROM test_text_agg
GROUP BY category
ORDER BY category;

-- Count
SELECT category, COUNT(item) AS item_count
FROM test_text_agg
GROUP BY category
ORDER BY category;

-- Min/Max (lexicographic)
SELECT category, MIN(item) AS min_item, MAX(item) AS max_item
FROM test_text_agg
GROUP BY category
ORDER BY category;

-- ============================================================================
-- TEXT with Special Content
-- ============================================================================

CREATE TABLE test_text_special (
    id INTEGER PRIMARY KEY,
    content_type VARCHAR(50),
    content TEXT
);

-- JSON-like content
INSERT INTO test_text_special VALUES (1, 'JSON',
    '{"name": "John Doe", "age": 30, "email": "john@example.com", "active": true}');

-- XML-like content
INSERT INTO test_text_special VALUES (2, 'XML',
    '<?xml version="1.0"?><person><name>Jane Smith</name><age>25</age></person>');

-- CSV content
INSERT INTO test_text_special VALUES (3, 'CSV',
    'Name,Age,City\nAlice,28,New York\nBob,32,Los Angeles\nCarol,29,Chicago');

-- HTML content
INSERT INTO test_text_special VALUES (4, 'HTML',
    '<!DOCTYPE html><html><body><h1>Title</h1><p>Paragraph text.</p></body></html>');

-- Code snippet
INSERT INTO test_text_special VALUES (5, 'Code',
    'function example() {\n  let x = 10;\n  return x * 2;\n}');

-- SQL content
INSERT INTO test_text_special VALUES (6, 'SQL',
    'SELECT * FROM users WHERE age > 25 AND status = ''active'' ORDER BY name;');

-- Base64-like content
INSERT INTO test_text_special VALUES (7, 'Base64',
    'SGVsbG8gV29ybGQhIFRoaXMgaXMgYSB0ZXN0IG1lc3NhZ2Uu');

SELECT id, content_type, LENGTH(content) AS content_length,
       SUBSTRING(content FROM 1 FOR 50) AS preview
FROM test_text_special
ORDER BY id;

-- ============================================================================
-- TEXT Performance with Large Content
-- ============================================================================

CREATE TABLE test_text_performance (
    id INTEGER PRIMARY KEY,
    content TEXT
);

-- Insert progressively larger text
INSERT INTO test_text_performance VALUES (1, REPEAT('A', 1000));
INSERT INTO test_text_performance VALUES (2, REPEAT('B', 10000));
INSERT INTO test_text_performance VALUES (3, REPEAT('C', 100000));
INSERT INTO test_text_performance VALUES (4, REPEAT('D', 1000000));

-- Measure lengths
SELECT id, LENGTH(content) AS chars, OCTET_LENGTH(content) AS bytes
FROM test_text_performance
ORDER BY id;

-- Substring extraction from large text
SELECT id, LENGTH(content) AS total_len, SUBSTRING(content FROM 1 FOR 100) AS first_100
FROM test_text_performance
WHERE id = 4;

-- Search in large text
SELECT id, POSITION('CCC' IN content) AS found_at
FROM test_text_performance
WHERE id = 3 AND content LIKE '%CCC%';

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_text_performance;
DROP TABLE test_text_special;
DROP TABLE test_text_agg;
DROP TABLE test_text_nulls;
DROP TABLE text_vs_varchar;
DROP TABLE test_text_storage;
DROP TABLE test_large_build;
DROP TABLE test_text_concat;
DROP TABLE test_text_functions;
DROP TABLE test_text_search;
DROP TABLE test_text_utf8;
DROP TABLE test_text_basic;

DROP DATABASE test_string_text;
