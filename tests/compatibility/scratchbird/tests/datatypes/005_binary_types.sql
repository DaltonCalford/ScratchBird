-- ScratchBird Native Test: Binary Types
-- Test ID: datatypes_005
-- Description: Comprehensive testing of BINARY, VARBINARY, BLOB, and BYTEA types

CREATE DATABASE test_binary_types;
\c test_binary_types;

-- ============================================================================
-- BINARY Testing (fixed-length binary data)
-- ============================================================================

CREATE TABLE test_binary (
    id INTEGER PRIMARY KEY,
    bin4 BINARY(4),
    bin16 BINARY(16),
    bin32 BINARY(32),
    bin64 BINARY(64)
);

-- Basic binary values (hex literals)
INSERT INTO test_binary VALUES (1,
    '\x00010203'::BINARY(4),
    '\x000102030405060708090A0B0C0D0E0F'::BINARY(16),
    '\x0000000000000000000000000000000000000000000000000000000000000000'::BINARY(32),
    NULL
);

INSERT INTO test_binary VALUES (2,
    '\xDEADBEEF'::BINARY(4),
    '\xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF'::BINARY(16),
    '\x0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF'::BINARY(32),
    NULL
);

-- All zeros
INSERT INTO test_binary VALUES (3,
    '\x00000000'::BINARY(4),
    '\x00000000000000000000000000000000'::BINARY(16),
    NULL,
    NULL
);

-- All ones
INSERT INTO test_binary VALUES (4,
    '\xFFFFFFFF'::BINARY(4),
    '\xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF'::BINARY(16),
    NULL,
    NULL
);

-- Length verification (BINARY is fixed-length)
SELECT id, OCTET_LENGTH(bin4) AS bin4_len, OCTET_LENGTH(bin16) AS bin16_len
FROM test_binary WHERE id = 1;

-- Comparison
SELECT id FROM test_binary WHERE bin4 = '\xDEADBEEF'::BINARY(4);

-- Bitwise operations (if supported)
SELECT id, bin4, bin4 & '\xFF00FF00'::BINARY(4) AS bitwise_and
FROM test_binary WHERE id = 2;

-- ============================================================================
-- VARBINARY Testing (variable-length binary data)
-- ============================================================================

CREATE TABLE test_varbinary (
    id INTEGER PRIMARY KEY,
    varbin_small VARBINARY(16),
    varbin_medium VARBINARY(256),
    varbin_large VARBINARY(4096)
);

-- Various lengths
INSERT INTO test_varbinary VALUES (1,
    '\x01'::VARBINARY,
    '\x0123456789ABCDEF'::VARBINARY,
    NULL
);

INSERT INTO test_varbinary VALUES (2,
    '\x00'::VARBINARY,
    '\xDEADBEEF'::VARBINARY,
    '\xCAFEBABE'::VARBINARY
);

INSERT INTO test_varbinary VALUES (3,
    '\xFF'::VARBINARY,
    '\x0000000000000000000000000000000000000000'::VARBINARY,
    NULL
);

-- Empty binary
INSERT INTO test_varbinary VALUES (10,
    '\x'::VARBINARY,
    '\x'::VARBINARY,
    '\x'::VARBINARY
);

-- Length verification (VARBINARY stores actual length)
SELECT id,
       OCTET_LENGTH(varbin_small) AS small_len,
       OCTET_LENGTH(varbin_medium) AS medium_len
FROM test_varbinary WHERE id <= 3;

-- Maximum size test
INSERT INTO test_varbinary VALUES (20,
    NULL,
    NULL,
    REPEAT('\xAB'::VARBINARY, 2048)  -- 4096 bytes
);

SELECT id, OCTET_LENGTH(varbin_large) AS large_len FROM test_varbinary WHERE id = 20;

-- ============================================================================
-- BLOB Testing (Binary Large OBject)
-- ============================================================================

CREATE TABLE test_blob (
    id INTEGER PRIMARY KEY,
    small_blob BLOB,
    large_blob BLOB
);

-- Small binary data
INSERT INTO test_blob VALUES (1,
    '\x48656C6C6F'::BLOB,  -- "Hello" in hex
    NULL
);

-- Medium binary data (1KB)
INSERT INTO test_blob VALUES (2,
    REPEAT('\x00'::BLOB, 1024),
    NULL
);

-- Large binary data (100KB)
INSERT INTO test_blob VALUES (3,
    NULL,
    REPEAT('\xFF'::BLOB, 102400)
);

-- Very large binary data (1MB)
INSERT INTO test_blob VALUES (4,
    NULL,
    REPEAT('\xAA55'::BLOB, 524288)  -- Alternating pattern
);

-- Length checks
SELECT id,
       OCTET_LENGTH(small_blob) AS small_bytes,
       OCTET_LENGTH(large_blob) AS large_bytes
FROM test_blob;

-- Substring extraction from BLOB
SELECT id, SUBSTRING(large_blob FROM 1 FOR 16) AS first_16_bytes
FROM test_blob WHERE id = 3;

-- ============================================================================
-- BYTEA Testing (PostgreSQL-style binary type)
-- ============================================================================

CREATE TABLE test_bytea (
    id INTEGER PRIMARY KEY,
    data BYTEA,
    description VARCHAR(100)
);

-- Basic bytea values
INSERT INTO test_bytea VALUES (1, '\x00'::BYTEA, 'Single null byte');
INSERT INTO test_bytea VALUES (2, '\xFF'::BYTEA, 'Single 0xFF byte');
INSERT INTO test_bytea VALUES (3, '\xDEADBEEF'::BYTEA, 'Classic hex pattern');
INSERT INTO test_bytea VALUES (4, '\x0123456789ABCDEF'::BYTEA, 'Sequential hex');

-- Text converted to bytea
INSERT INTO test_bytea VALUES (10, 'Hello World'::BYTEA, 'Text as bytea');
INSERT INTO test_bytea VALUES (11, 'ScratchBird Database'::BYTEA, 'Product name');

-- Empty bytea
INSERT INTO test_bytea VALUES (20, '\x'::BYTEA, 'Empty bytes');

-- Length verification
SELECT id, description, OCTET_LENGTH(data) AS byte_count FROM test_bytea ORDER BY id;

-- Bytea concatenation
SELECT id, data || '\xFFFF'::BYTEA AS appended FROM test_bytea WHERE id = 3;

-- ============================================================================
-- Binary Encoding and Representation
-- ============================================================================

CREATE TABLE test_binary_encoding (
    id INTEGER PRIMARY KEY,
    raw_binary BYTEA,
    hex_string VARCHAR(100),
    base64_string VARCHAR(100)
);

-- Raw binary data
INSERT INTO test_binary_encoding VALUES (1,
    '\xDEADBEEF'::BYTEA,
    NULL,
    NULL
);

-- Hex encoding (if supported)
UPDATE test_binary_encoding SET hex_string = ENCODE(raw_binary, 'hex') WHERE id = 1;

-- Base64 encoding (if supported)
UPDATE test_binary_encoding SET base64_string = ENCODE(raw_binary, 'base64') WHERE id = 1;

SELECT id, raw_binary, hex_string, base64_string FROM test_binary_encoding;

-- Decoding (if supported)
CREATE TABLE test_binary_decode (
    id INTEGER PRIMARY KEY,
    from_hex BYTEA,
    from_base64 BYTEA
);

INSERT INTO test_binary_decode VALUES (1,
    DECODE('DEADBEEF', 'hex'),
    DECODE('3q2+7w==', 'base64')
);

SELECT id, from_hex, from_base64 FROM test_binary_decode;

-- ============================================================================
-- Binary Data Patterns
-- ============================================================================

CREATE TABLE test_binary_patterns (
    id INTEGER PRIMARY KEY,
    pattern_name VARCHAR(50),
    pattern_data BYTEA
);

-- All zeros
INSERT INTO test_binary_patterns VALUES (1, 'All Zeros', REPEAT('\x00'::BYTEA, 32));

-- All ones
INSERT INTO test_binary_patterns VALUES (2, 'All Ones', REPEAT('\xFF'::BYTEA, 32));

-- Alternating bits (0xAA = 10101010, 0x55 = 01010101)
INSERT INTO test_binary_patterns VALUES (3, 'Alternating AA55', REPEAT('\xAA55'::BYTEA, 16));

-- Sequential bytes
INSERT INTO test_binary_patterns VALUES (4, 'Sequential 0-255',
    '\x000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F'::BYTEA);

-- Random-like pattern
INSERT INTO test_binary_patterns VALUES (5, 'Pseudo Random',
    '\x6A09E667BB67AE853C6EF372A54FF53A510E527F9B05688C1F83D9AB5BE0CD19'::BYTEA);

SELECT id, pattern_name, OCTET_LENGTH(pattern_data) AS bytes,
       SUBSTRING(pattern_data FROM 1 FOR 8) AS first_8
FROM test_binary_patterns
ORDER BY id;

-- ============================================================================
-- Binary Operations and Manipulation
-- ============================================================================

CREATE TABLE test_binary_ops (
    id INTEGER PRIMARY KEY,
    data1 BYTEA,
    data2 BYTEA
);

INSERT INTO test_binary_ops VALUES (1, '\x0F0F0F0F'::BYTEA, '\xF0F0F0F0'::BYTEA);
INSERT INTO test_binary_ops VALUES (2, '\x12345678'::BYTEA, '\x87654321'::BYTEA);
INSERT INTO test_binary_ops VALUES (3, '\xAAAAAAAA'::BYTEA, '\x55555555'::BYTEA);

-- Concatenation
SELECT id, data1 || data2 AS concatenated FROM test_binary_ops;

-- Substring
SELECT id, SUBSTRING(data1 FROM 2 FOR 2) AS middle_bytes FROM test_binary_ops WHERE id = 2;

-- Length
SELECT id, OCTET_LENGTH(data1) AS len1, OCTET_LENGTH(data2) AS len2 FROM test_binary_ops;

-- Position (if supported)
CREATE TABLE test_binary_position (
    id INTEGER PRIMARY KEY,
    haystack BYTEA,
    needle BYTEA
);

INSERT INTO test_binary_position VALUES (1,
    '\x00112233445566778899AABBCCDDEEFF'::BYTEA,
    '\x5566'::BYTEA
);

SELECT id, POSITION(needle IN haystack) AS found_at FROM test_binary_position;

-- ============================================================================
-- Binary Data Storage for Specific Use Cases
-- ============================================================================

-- UUID as binary (16 bytes)
CREATE TABLE test_binary_uuid (
    id INTEGER PRIMARY KEY,
    uuid_binary BINARY(16),
    description VARCHAR(100)
);

INSERT INTO test_binary_uuid VALUES (1,
    '\x550e8400e29b41d4a716446655440000'::BINARY(16),
    'UUIDv4 example'
);

INSERT INTO test_binary_uuid VALUES (2,
    '\x01234567890abcdef0123456789abcd'::BINARY(16),
    'Custom UUID'
);

SELECT id, uuid_binary, description FROM test_binary_uuid;

-- Hash values (SHA-256 = 32 bytes)
CREATE TABLE test_binary_hashes (
    id INTEGER PRIMARY KEY,
    hash_type VARCHAR(20),
    hash_value BINARY(32)
);

INSERT INTO test_binary_hashes VALUES (1, 'SHA-256',
    '\xe3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'::BINARY(32)
);

INSERT INTO test_binary_hashes VALUES (2, 'SHA-256',
    '\x2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae'::BINARY(32)
);

SELECT id, hash_type, OCTET_LENGTH(hash_value) AS hash_bytes FROM test_binary_hashes;

-- Cryptographic keys (256-bit = 32 bytes)
CREATE TABLE test_binary_keys (
    id INTEGER PRIMARY KEY,
    key_type VARCHAR(20),
    key_data BINARY(32)
);

INSERT INTO test_binary_keys VALUES (1, 'AES-256',
    '\x000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F'::BINARY(32)
);

SELECT id, key_type, OCTET_LENGTH(key_data) AS key_bytes FROM test_binary_keys;

-- ============================================================================
-- Binary NULL Handling
-- ============================================================================

CREATE TABLE test_binary_nulls (
    id INTEGER PRIMARY KEY,
    nullable_binary BINARY(8),
    nullable_varbinary VARBINARY(64),
    nullable_blob BLOB,
    nullable_bytea BYTEA
);

INSERT INTO test_binary_nulls VALUES (1, NULL, NULL, NULL, NULL);
INSERT INTO test_binary_nulls VALUES (2,
    '\x0123456789ABCDEF'::BINARY(8),
    '\xDEADBEEF'::VARBINARY,
    '\xCAFEBABE'::BLOB,
    '\xFEEDFACE'::BYTEA
);
INSERT INTO test_binary_nulls VALUES (3,
    '\x0000000000000000'::BINARY(8),
    NULL,
    NULL,
    '\x'::BYTEA
);

-- NULL checks
SELECT COUNT(*) AS null_binary FROM test_binary_nulls WHERE nullable_binary IS NULL;
SELECT COUNT(*) AS null_blob FROM test_binary_nulls WHERE nullable_blob IS NULL;

-- NULL coalescence
SELECT id, COALESCE(nullable_bytea, '\xFFFF'::BYTEA) AS coalesced FROM test_binary_nulls;

-- ============================================================================
-- Binary Comparison and Sorting
-- ============================================================================

CREATE TABLE test_binary_compare (
    id INTEGER PRIMARY KEY,
    value BYTEA
);

INSERT INTO test_binary_compare VALUES (1, '\x00'::BYTEA);
INSERT INTO test_binary_compare VALUES (2, '\x01'::BYTEA);
INSERT INTO test_binary_compare VALUES (3, '\x0F'::BYTEA);
INSERT INTO test_binary_compare VALUES (4, '\xFF'::BYTEA);
INSERT INTO test_binary_compare VALUES (5, '\x00FF'::BYTEA);
INSERT INTO test_binary_compare VALUES (6, '\xFF00'::BYTEA);

-- Sorting (lexicographic byte order)
SELECT id, value FROM test_binary_compare ORDER BY value;

-- Equality
SELECT id FROM test_binary_compare WHERE value = '\x0F'::BYTEA;

-- Range query
SELECT id FROM test_binary_compare WHERE value >= '\x01'::BYTEA AND value <= '\x0F'::BYTEA
ORDER BY value;

-- ============================================================================
-- Binary vs Text Conversion
-- ============================================================================

CREATE TABLE test_binary_text_conversion (
    id INTEGER PRIMARY KEY,
    original_text VARCHAR(100),
    as_binary BYTEA,
    back_to_text VARCHAR(100)
);

-- Text to binary
INSERT INTO test_binary_text_conversion (id, original_text, as_binary) VALUES
(1, 'Hello', 'Hello'::BYTEA);

INSERT INTO test_binary_text_conversion (id, original_text, as_binary) VALUES
(2, 'World', 'World'::BYTEA);

-- Binary back to text (if conversion supported)
UPDATE test_binary_text_conversion SET back_to_text = CONVERT_FROM(as_binary, 'UTF8');

SELECT id, original_text, as_binary, back_to_text FROM test_binary_text_conversion;

-- ============================================================================
-- Binary Aggregates
-- ============================================================================

CREATE TABLE test_binary_agg (
    id INTEGER PRIMARY KEY,
    category CHAR(1),
    data BYTEA
);

INSERT INTO test_binary_agg VALUES (1, 'A', '\x01'::BYTEA);
INSERT INTO test_binary_agg VALUES (2, 'A', '\x02'::BYTEA);
INSERT INTO test_binary_agg VALUES (3, 'A', '\x03'::BYTEA);
INSERT INTO test_binary_agg VALUES (4, 'B', '\xFF'::BYTEA);
INSERT INTO test_binary_agg VALUES (5, 'B', '\xFE'::BYTEA);

-- Count
SELECT category, COUNT(data) AS count FROM test_binary_agg GROUP BY category ORDER BY category;

-- Min/Max (lexicographic)
SELECT category, MIN(data) AS min_data, MAX(data) AS max_data
FROM test_binary_agg GROUP BY category ORDER BY category;

-- ============================================================================
-- Binary Performance with Large Data
-- ============================================================================

CREATE TABLE test_binary_performance (
    id INTEGER PRIMARY KEY,
    size_category VARCHAR(20),
    data BLOB
);

-- 1 KB
INSERT INTO test_binary_performance VALUES (1, '1KB', REPEAT('\xAB'::BLOB, 1024));

-- 10 KB
INSERT INTO test_binary_performance VALUES (2, '10KB', REPEAT('\xCD'::BLOB, 10240));

-- 100 KB
INSERT INTO test_binary_performance VALUES (3, '100KB', REPEAT('\xEF'::BLOB, 102400));

-- 1 MB
INSERT INTO test_binary_performance VALUES (4, '1MB', REPEAT('\x12'::BLOB, 1048576));

-- 10 MB
INSERT INTO test_binary_performance VALUES (5, '10MB', REPEAT('\x34'::BLOB, 10485760));

-- Size verification
SELECT id, size_category, OCTET_LENGTH(data) AS bytes FROM test_binary_performance ORDER BY id;

-- Extraction performance
SELECT id, size_category, SUBSTRING(data FROM 1 FOR 64) AS first_64_bytes
FROM test_binary_performance WHERE id = 5;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_binary_performance;
DROP TABLE test_binary_agg;
DROP TABLE test_binary_text_conversion;
DROP TABLE test_binary_compare;
DROP TABLE test_binary_nulls;
DROP TABLE test_binary_keys;
DROP TABLE test_binary_hashes;
DROP TABLE test_binary_uuid;
DROP TABLE test_binary_position;
DROP TABLE test_binary_ops;
DROP TABLE test_binary_patterns;
DROP TABLE test_binary_decode;
DROP TABLE test_binary_encoding;
DROP TABLE test_bytea;
DROP TABLE test_blob;
DROP TABLE test_varbinary;
DROP TABLE test_binary;

DROP DATABASE test_binary_types;
