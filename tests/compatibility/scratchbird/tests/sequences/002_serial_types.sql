-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Sequences - SERIAL Types
-- Description: Comprehensive SERIAL, BIGSERIAL, SMALLSERIAL testing
-- ============================================================================

-- SERIAL: Shorthand for INT with auto-incrementing sequence
-- BIGSERIAL: BIGINT with sequence (larger range)
-- SMALLSERIAL: SMALLINT with sequence (smaller range)
-- Automatically creates sequence and sets default

-- Create test database
CREATE DATABASE test_serial_types_db;
USE test_serial_types_db;

-- ============================================================================
-- Section 1: Basic SERIAL
-- ============================================================================

CREATE TABLE test_serial_basic (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200)
);

-- SERIAL automatically creates sequence
INSERT INTO test_serial_basic (name) VALUES ('Item 1'), ('Item 2'), ('Item 3');

SELECT id, name FROM test_serial_basic ORDER BY id;

-- Query the auto-generated sequence
SELECT pg_get_serial_sequence('test_serial_basic', 'id') AS sequence_name;

-- ============================================================================
-- Section 2: BIGSERIAL Type
-- ============================================================================

CREATE TABLE test_bigserial (
    id BIGSERIAL PRIMARY KEY,
    data TEXT
);

-- BIGSERIAL uses BIGINT (range: -9223372036854775808 to 9223372036854775807)
INSERT INTO test_bigserial (data) VALUES ('Data 1'), ('Data 2');

SELECT id, data FROM test_bigserial ORDER BY id;

-- Verify column type
SELECT
    column_name,
    data_type,
    column_default
FROM information_schema.columns
WHERE table_name = 'test_bigserial' AND column_name = 'id';

-- ============================================================================
-- Section 3: SMALLSERIAL Type
-- ============================================================================

CREATE TABLE test_smallserial (
    id SMALLSERIAL PRIMARY KEY,
    code VARCHAR(50)
);

-- SMALLSERIAL uses SMALLINT (range: -32768 to 32767)
INSERT INTO test_smallserial (code) VALUES ('CODE1'), ('CODE2');

SELECT id, code FROM test_smallserial ORDER BY id;

-- Verify column type
SELECT
    column_name,
    data_type
FROM information_schema.columns
WHERE table_name = 'test_smallserial' AND column_name = 'id';

-- ============================================================================
-- Section 4: SERIAL vs Manual Sequence
-- ============================================================================

-- Using SERIAL (shorthand)
CREATE TABLE test_serial_auto (
    id SERIAL PRIMARY KEY,
    value TEXT
);

-- Equivalent manual definition
CREATE SEQUENCE test_manual_seq;
CREATE TABLE test_serial_manual (
    id INT PRIMARY KEY DEFAULT nextval('test_manual_seq'),
    value TEXT
);
ALTER SEQUENCE test_manual_seq OWNED BY test_serial_manual.id;

-- Both work identically
INSERT INTO test_serial_auto (value) VALUES ('Auto 1'), ('Auto 2');
INSERT INTO test_serial_manual (value) VALUES ('Manual 1'), ('Manual 2');

SELECT id, value FROM test_serial_auto ORDER BY id;
SELECT id, value FROM test_serial_manual ORDER BY id;

-- ============================================================================
-- Section 5: Multiple SERIAL Columns
-- ============================================================================

CREATE TABLE test_multi_serial (
    id SERIAL PRIMARY KEY,
    order_number SERIAL,
    invoice_number SERIAL,
    description TEXT
);

-- Each SERIAL creates its own sequence
INSERT INTO test_multi_serial (description) VALUES ('Item 1'), ('Item 2');

SELECT id, order_number, invoice_number, description FROM test_multi_serial ORDER BY id;

-- Query all sequences for this table
SELECT
    pg_get_serial_sequence('test_multi_serial', 'id') AS id_seq,
    pg_get_serial_sequence('test_multi_serial', 'order_number') AS order_seq,
    pg_get_serial_sequence('test_multi_serial', 'invoice_number') AS invoice_seq;

-- ============================================================================
-- Section 6: Explicitly Setting SERIAL Values
-- ============================================================================

CREATE TABLE test_serial_explicit (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200)
);

-- Auto-generated IDs
INSERT INTO test_serial_explicit (name) VALUES ('Item 1');

-- Explicitly set ID
INSERT INTO test_serial_explicit (id, name) VALUES (100, 'Item 100');

-- Continue with auto-generated (may conflict!)
-- Need to update sequence to avoid conflicts
SELECT setval(pg_get_serial_sequence('test_serial_explicit', 'id'),
              (SELECT MAX(id) FROM test_serial_explicit));

-- Now auto-generated works correctly
INSERT INTO test_serial_explicit (name) VALUES ('Item 101');

SELECT id, name FROM test_serial_explicit ORDER BY id;

-- ============================================================================
-- Section 7: SERIAL with DEFAULT Override
-- ============================================================================

CREATE TABLE test_serial_default (
    id SERIAL PRIMARY KEY,
    custom_id INT DEFAULT 9999,
    name VARCHAR(200)
);

INSERT INTO test_serial_default (name) VALUES ('Item 1'), ('Item 2');

SELECT id, custom_id, name FROM test_serial_default ORDER BY id;

-- ============================================================================
-- Section 8: SERIAL in Partitioned Tables
-- ============================================================================

CREATE TABLE test_partitioned_serial (
    id SERIAL,
    created_date DATE NOT NULL,
    data TEXT,
    PRIMARY KEY (id, created_date)
) PARTITION BY RANGE (created_date);

CREATE TABLE test_partition_2024_01 PARTITION OF test_partitioned_serial
    FOR VALUES FROM ('2024-01-01') TO ('2024-02-01');

CREATE TABLE test_partition_2024_02 PARTITION OF test_partitioned_serial
    FOR VALUES FROM ('2024-02-01') TO ('2024-03-01');

-- SERIAL sequence is shared across partitions
INSERT INTO test_partitioned_serial (created_date, data) VALUES
    ('2024-01-15', 'January data'),
    ('2024-02-15', 'February data'),
    ('2024-01-20', 'More January data');

SELECT id, created_date, data FROM test_partitioned_serial ORDER BY id;

-- ============================================================================
-- Section 9: SERIAL Sequence Ownership
-- ============================================================================

CREATE TABLE test_serial_ownership (
    id SERIAL PRIMARY KEY,
    value TEXT
);

-- Query sequence ownership
SELECT
    s.sequencename,
    t.tablename,
    c.column_name
FROM pg_sequences s
JOIN pg_depend d ON d.objid = (s.schemaname || '.' || s.sequencename)::regclass
JOIN pg_class t ON t.oid = d.refobjid
JOIN information_schema.columns c ON c.table_name = t.relname
WHERE s.sequencename LIKE '%test_serial_ownership%';

-- Dropping table drops owned sequence
DROP TABLE test_serial_ownership;

-- Sequence is automatically dropped
SELECT sequencename FROM pg_sequences WHERE sequencename LIKE '%test_serial_ownership%';

-- ============================================================================
-- Section 10: SERIAL with INSERT...RETURNING
-- ============================================================================

CREATE TABLE test_serial_returning (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Return generated ID
INSERT INTO test_serial_returning (name) VALUES ('Item 1') RETURNING id, created_at;
INSERT INTO test_serial_returning (name) VALUES ('Item 2') RETURNING id, name;

-- Bulk insert with returning
INSERT INTO test_serial_returning (name)
VALUES ('Item 3'), ('Item 4'), ('Item 5')
RETURNING id, name;

-- ============================================================================
-- Section 11: SERIAL Range Limits
-- ============================================================================

-- SMALLSERIAL max value test
CREATE TABLE test_smallserial_limit (
    id SMALLSERIAL PRIMARY KEY
);

-- Set sequence near limit
SELECT setval(pg_get_serial_sequence('test_smallserial_limit', 'id'), 32765);

-- Can insert a few more
INSERT INTO test_smallserial_limit DEFAULT VALUES;
INSERT INTO test_smallserial_limit DEFAULT VALUES;

-- Would exceed SMALLINT range (would fail)
-- INSERT INTO test_smallserial_limit DEFAULT VALUES;

SELECT id FROM test_smallserial_limit ORDER BY id;

-- ============================================================================
-- Section 12: Converting Existing Column to SERIAL
-- ============================================================================

CREATE TABLE test_convert_to_serial (
    id INT PRIMARY KEY,
    name VARCHAR(200)
);

INSERT INTO test_convert_to_serial VALUES (1, 'Item 1'), (2, 'Item 2');

-- Create sequence
CREATE SEQUENCE test_convert_to_serial_id_seq;

-- Set sequence to max existing value
SELECT setval('test_convert_to_serial_id_seq', (SELECT MAX(id) FROM test_convert_to_serial));

-- Add default to use sequence
ALTER TABLE test_convert_to_serial
    ALTER COLUMN id SET DEFAULT nextval('test_convert_to_serial_id_seq');

-- Set ownership
ALTER SEQUENCE test_convert_to_serial_id_seq OWNED BY test_convert_to_serial.id;

-- Now works like SERIAL
INSERT INTO test_convert_to_serial (name) VALUES ('Item 3'), ('Item 4');

SELECT id, name FROM test_convert_to_serial ORDER BY id;

-- ============================================================================
-- Section 13: SERIAL with ON CONFLICT
-- ============================================================================

CREATE TABLE test_serial_upsert (
    id SERIAL PRIMARY KEY,
    code VARCHAR(50) UNIQUE,
    value TEXT
);

INSERT INTO test_serial_upsert (code, value) VALUES ('CODE1', 'Value 1');

-- Upsert with SERIAL
INSERT INTO test_serial_upsert (code, value) VALUES ('CODE1', 'Updated Value')
ON CONFLICT (code) DO UPDATE SET value = EXCLUDED.value;

-- New insert
INSERT INTO test_serial_upsert (code, value) VALUES ('CODE2', 'Value 2');

SELECT id, code, value FROM test_serial_upsert ORDER BY id;

-- Note: Sequence advances even on conflict
SELECT currval(pg_get_serial_sequence('test_serial_upsert', 'id'));

-- ============================================================================
-- Section 14: SERIAL in Temporary Tables
-- ============================================================================

CREATE TEMP TABLE test_temp_serial (
    id SERIAL PRIMARY KEY,
    data TEXT
);

INSERT INTO test_temp_serial (data) VALUES ('Temp 1'), ('Temp 2');

SELECT id, data FROM test_temp_serial ORDER BY id;

-- Temporary sequence created automatically
SELECT pg_get_serial_sequence('test_temp_serial', 'id');

-- ============================================================================
-- Section 15: SERIAL Sequence Synchronization
-- ============================================================================

CREATE TABLE test_serial_sync (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200)
);

-- Insert with explicit IDs
INSERT INTO test_serial_sync (id, name) VALUES (10, 'Item 10');
INSERT INTO test_serial_sync (id, name) VALUES (20, 'Item 20');
INSERT INTO test_serial_sync (id, name) VALUES (15, 'Item 15');

-- Sync sequence to max ID
SELECT setval(
    pg_get_serial_sequence('test_serial_sync', 'id'),
    (SELECT MAX(id) FROM test_serial_sync)
);

-- Now auto-increment continues correctly
INSERT INTO test_serial_sync (name) VALUES ('Item 21');

SELECT id, name FROM test_serial_sync ORDER BY id;

-- ============================================================================
-- Section 16: SERIAL Performance Comparison
-- ============================================================================

CREATE TABLE test_serial_perf (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_manual_id_perf (
    id INT PRIMARY KEY,
    data TEXT
);

-- SERIAL is faster (no manual ID management)
INSERT INTO test_serial_perf (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

-- Manual IDs require explicit values
INSERT INTO test_manual_id_perf (id, data)
SELECT i, 'Data ' || i FROM generate_series(1, 1000) i;

SELECT COUNT(*) FROM test_serial_perf;
SELECT COUNT(*) FROM test_manual_id_perf;

-- ============================================================================
-- Section 17: SERIAL Metadata Queries
-- ============================================================================

-- List all SERIAL sequences in database
SELECT
    t.tablename,
    c.column_name,
    pg_get_serial_sequence(t.tablename, c.column_name) AS sequence_name
FROM information_schema.columns c
JOIN pg_tables t ON c.table_name = t.tablename
WHERE c.column_default LIKE 'nextval%'
  AND t.schemaname = 'public'
ORDER BY t.tablename, c.column_name
LIMIT 5;

-- ============================================================================
-- Section 18: SERIAL Type Casting
-- ============================================================================

CREATE TABLE test_serial_cast (
    id SERIAL PRIMARY KEY,
    big_id BIGINT DEFAULT nextval('test_serial_cast_id_seq'),  -- Reuse SERIAL sequence
    value TEXT
);

INSERT INTO test_serial_cast (value) VALUES ('Item 1'), ('Item 2');

-- Both columns get same value from sequence
SELECT id, big_id, value FROM test_serial_cast ORDER BY id;

-- ============================================================================
-- Section 19: SERIAL with Inheritance
-- ============================================================================

CREATE TABLE test_parent_serial (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_child_serial (
    extra_field VARCHAR(100)
) INHERITS (test_parent_serial);

-- Child table doesn't inherit SERIAL behavior (needs own sequence)
-- Need to set up manually if desired
CREATE SEQUENCE test_child_serial_id_seq;
ALTER TABLE test_child_serial
    ALTER COLUMN id SET DEFAULT nextval('test_child_serial_id_seq');

INSERT INTO test_parent_serial (data) VALUES ('Parent 1');
INSERT INTO test_child_serial (data, extra_field) VALUES ('Child 1', 'Extra');

SELECT id, data FROM test_parent_serial;
SELECT id, data, extra_field FROM test_child_serial;

-- ============================================================================
-- Section 20: Best Practices
-- ============================================================================

CREATE TABLE test_serial_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_serial_best_practices VALUES
    (1, 'Use SERIAL for simple auto-incrementing integer PKs'),
    (2, 'Use BIGSERIAL for high-volume tables to avoid overflow'),
    (3, 'SMALLSERIAL for space-constrained, low-volume tables'),
    (4, 'SERIAL is shorthand for INT + sequence + default'),
    (5, 'Sequence automatically owned by column (auto-cleanup)'),
    (6, 'Use RETURNING clause to get generated SERIAL values'),
    (7, 'Sync sequence after manual ID inserts with setval()'),
    (8, 'SERIAL sequences advance even on failed inserts'),
    (9, 'Gaps in SERIAL IDs are normal and expected'),
    (10, 'Use INSERT...RETURNING to get generated IDs'),
    (11, 'SERIAL creates index via PRIMARY KEY or UNIQUE'),
    (12, 'Multiple SERIAL columns create multiple sequences'),
    (13, 'SERIAL range: INT (2B), BIGINT (9Q), SMALLINT (32K)'),
    (14, 'Query pg_get_serial_sequence() for sequence name'),
    (15, 'Consider UUID for distributed systems instead of SERIAL');

SELECT id, guideline FROM test_serial_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_serial_best_practices;
DROP TABLE test_child_serial;
DROP SEQUENCE test_child_serial_id_seq;
DROP TABLE test_parent_serial;
DROP TABLE test_serial_cast;
DROP TABLE test_manual_id_perf;
DROP TABLE test_serial_perf;
DROP TABLE test_serial_sync;
DROP TABLE test_serial_upsert;
DROP TABLE test_convert_to_serial;
DROP SEQUENCE test_convert_to_serial_id_seq CASCADE;
DROP TABLE test_smallserial_limit;
DROP TABLE test_serial_returning;
DROP TABLE test_partition_2024_02;
DROP TABLE test_partition_2024_01;
DROP TABLE test_partitioned_serial;
DROP TABLE test_serial_default;
DROP TABLE test_serial_explicit;
DROP TABLE test_multi_serial;
DROP TABLE test_serial_manual;
DROP SEQUENCE test_manual_seq;
DROP TABLE test_serial_auto;
DROP TABLE test_smallserial;
DROP TABLE test_bigserial;
DROP TABLE test_serial_basic;

DROP DATABASE test_serial_types_db;

-- End of SERIAL types tests
