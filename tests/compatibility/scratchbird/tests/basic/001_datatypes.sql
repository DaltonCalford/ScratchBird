-- ScratchBird Native Test: Data Types
-- Test ID: basic_001
-- Description: Basic data type creation, insertion, and retrieval

-- Create database
CREATE DATABASE test_datatypes;

-- Connect to database
\c test_datatypes;

-- Test INTEGER types
CREATE TABLE test_integers (
    id INTEGER PRIMARY KEY,
    small_val SMALLINT,
    int_val INTEGER,
    big_val BIGINT
);

INSERT INTO test_integers VALUES (1, 100, 50000, 9223372036854775807);
INSERT INTO test_integers VALUES (2, -100, -50000, -9223372036854775807);
INSERT INTO test_integers VALUES (3, 0, 0, 0);

SELECT * FROM test_integers ORDER BY id;

-- Test REAL types
CREATE TABLE test_reals (
    id INTEGER PRIMARY KEY,
    real_val REAL,
    double_val DOUBLE PRECISION
);

INSERT INTO test_reals VALUES (1, 3.14159, 2.718281828459045);
INSERT INTO test_reals VALUES (2, -1.5, -999.999999);
INSERT INTO test_reals VALUES (3, 0.0, 0.0);

SELECT * FROM test_reals ORDER BY id;

-- Test VARCHAR and TEXT
CREATE TABLE test_strings (
    id INTEGER PRIMARY KEY,
    varchar_val VARCHAR(50),
    text_val TEXT
);

INSERT INTO test_strings VALUES (1, 'Hello World', 'This is a longer text field');
INSERT INTO test_strings VALUES (2, '', '');
INSERT INTO test_strings VALUES (3, 'UTF-8: café', 'Multi-line
text
content');

SELECT * FROM test_strings ORDER BY id;

-- Test BOOLEAN
CREATE TABLE test_boolean (
    id INTEGER PRIMARY KEY,
    bool_val BOOLEAN
);

INSERT INTO test_boolean VALUES (1, TRUE);
INSERT INTO test_boolean VALUES (2, FALSE);
INSERT INTO test_boolean VALUES (3, NULL);

SELECT * FROM test_boolean ORDER BY id;

-- Test DATE, TIME, TIMESTAMP
CREATE TABLE test_temporal (
    id INTEGER PRIMARY KEY,
    date_val DATE,
    time_val TIME,
    timestamp_val TIMESTAMP
);

INSERT INTO test_temporal VALUES (1, '2025-12-31', '23:59:59', '2025-12-31 23:59:59');
INSERT INTO test_temporal VALUES (2, '1970-01-01', '00:00:00', '1970-01-01 00:00:00');
INSERT INTO test_temporal VALUES (3, '2000-06-15', '12:30:45', '2000-06-15 12:30:45.123456');

SELECT * FROM test_temporal ORDER BY id;

-- Test UUID
CREATE TABLE test_uuid (
    id UUID PRIMARY KEY,
    name VARCHAR(50)
);

-- These UUIDs are UUIDv7 format (time-ordered)
INSERT INTO test_uuid VALUES ('019b762b-6b23-7d2a-a7ca-4232a1c47a56', 'First');
INSERT INTO test_uuid VALUES ('019b762b-6b91-7099-b80d-63157d1adb15', 'Second');
INSERT INTO test_uuid VALUES ('019b762b-6c00-7e1c-9f12-3456789abcde', 'Third');

SELECT * FROM test_uuid ORDER BY id;

-- Test BLOB
CREATE TABLE test_blob (
    id INTEGER PRIMARY KEY,
    blob_val BLOB
);

-- Insert binary data (hex notation)
INSERT INTO test_blob VALUES (1, X'DEADBEEF');
INSERT INTO test_blob VALUES (2, X'48656C6C6F');  -- "Hello" in hex
INSERT INTO test_blob VALUES (3, X'');

SELECT id, LENGTH(blob_val) AS blob_length FROM test_blob ORDER BY id;

-- Test NULL values
CREATE TABLE test_nulls (
    id INTEGER PRIMARY KEY,
    nullable_int INTEGER,
    nullable_text TEXT,
    nullable_date DATE
);

INSERT INTO test_nulls VALUES (1, NULL, NULL, NULL);
INSERT INTO test_nulls VALUES (2, 42, 'Not NULL', '2025-01-01');
INSERT INTO test_nulls VALUES (3, NULL, 'Mixed', NULL);

SELECT * FROM test_nulls ORDER BY id;

-- Cleanup
DROP TABLE test_nulls;
DROP TABLE test_blob;
DROP TABLE test_uuid;
DROP TABLE test_temporal;
DROP TABLE test_boolean;
DROP TABLE test_strings;
DROP TABLE test_reals;
DROP TABLE test_integers;

-- Drop database
DROP DATABASE test_datatypes;
