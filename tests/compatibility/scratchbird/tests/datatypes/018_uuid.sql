-- ScratchBird Native Test: Special Types - UUID
-- Test ID: datatypes_018
-- Description: Comprehensive testing of UUID type and operations

CREATE DATABASE test_uuid;
\c test_uuid;

-- ============================================================================
-- UUID Basic Testing
-- ============================================================================

CREATE TABLE test_uuid_basic (
    id INTEGER PRIMARY KEY,
    uuid_value UUID,
    description VARCHAR(100)
);

-- Generate UUIDs
INSERT INTO test_uuid_basic VALUES (1, UUID(), 'Generated UUID v4');
INSERT INTO test_uuid_basic VALUES (2, UUID(), 'Another UUID v4');
INSERT INTO test_uuid_basic VALUES (3, UUID(), 'Third UUID v4');

-- UUIDv7 (time-ordered)
INSERT INTO test_uuid_basic VALUES (10, UUIDv7(), 'Generated UUIDv7');
INSERT INTO test_uuid_basic VALUES (11, UUIDv7(), 'Another UUIDv7');
INSERT INTO test_uuid_basic VALUES (12, UUIDv7(), 'Third UUIDv7');

-- Nil UUID (all zeros)
INSERT INTO test_uuid_basic VALUES (20, '00000000-0000-0000-0000-000000000000'::UUID, 'Nil UUID');

-- Specific UUID values
INSERT INTO test_uuid_basic VALUES (30, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'Specific UUID 1');
INSERT INTO test_uuid_basic VALUES (31, '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'Specific UUID 2');

SELECT id, uuid_value, description FROM test_uuid_basic ORDER BY id;

-- ============================================================================
-- UUID Input Formats
-- ============================================================================

CREATE TABLE test_uuid_formats (
    id INTEGER PRIMARY KEY,
    uuid_value UUID,
    format_description VARCHAR(50)
);

-- Standard hyphenated format
INSERT INTO test_uuid_formats VALUES (1, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'Standard format');

-- Uppercase
INSERT INTO test_uuid_formats VALUES (2, '550E8400-E29B-41D4-A716-446655440000'::UUID, 'Uppercase format');

-- Without hyphens (if supported)
INSERT INTO test_uuid_formats VALUES (10, '550e8400e29b41d4a716446655440000'::UUID, 'No hyphens');

-- With braces (if supported)
INSERT INTO test_uuid_formats VALUES (11, '{550e8400-e29b-41d4-a716-446655440000}'::UUID, 'With braces');

-- URN format (if supported)
INSERT INTO test_uuid_formats VALUES (12, 'urn:uuid:550e8400-e29b-41d4-a716-446655440000'::UUID, 'URN format');

SELECT id, uuid_value, format_description FROM test_uuid_formats ORDER BY id;

-- ============================================================================
-- UUID Versions
-- ============================================================================

CREATE TABLE test_uuid_versions (
    id INTEGER PRIMARY KEY,
    uuid_value UUID,
    version_info VARCHAR(100)
);

-- UUID v1 (timestamp + MAC address) - if supported
INSERT INTO test_uuid_versions VALUES (1, UUIDv1(), 'UUID v1 - timestamp based');

-- UUID v4 (random)
INSERT INTO test_uuid_versions VALUES (4, UUID(), 'UUID v4 - random');
INSERT INTO test_uuid_versions VALUES (5, UUIDv4(), 'UUID v4 - random explicit');

-- UUID v7 (timestamp, monotonic)
INSERT INTO test_uuid_versions VALUES (7, UUIDv7(), 'UUID v7 - time-ordered');
INSERT INTO test_uuid_versions VALUES (8, UUIDv7(), 'UUID v7 - time-ordered 2');

SELECT id, uuid_value, version_info FROM test_uuid_versions ORDER BY id;

-- Extract version number
SELECT id, uuid_value,
       UUID_VERSION(uuid_value) AS version
FROM test_uuid_versions
ORDER BY id;

-- ============================================================================
-- UUID Comparison and Sorting
-- ============================================================================

CREATE TABLE test_uuid_comparison (
    id INTEGER PRIMARY KEY,
    uuid_value UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert UUIDs in specific order
INSERT INTO test_uuid_comparison (id, uuid_value) VALUES (1, '00000000-0000-0000-0000-000000000001'::UUID);
INSERT INTO test_uuid_comparison (id, uuid_value) VALUES (2, '00000000-0000-0000-0000-000000000002'::UUID);
INSERT INTO test_uuid_comparison (id, uuid_value) VALUES (3, 'ffffffff-ffff-ffff-ffff-ffffffffffff'::UUID);
INSERT INTO test_uuid_comparison (id, uuid_value) VALUES (4, '550e8400-e29b-41d4-a716-446655440000'::UUID);
INSERT INTO test_uuid_comparison (id, uuid_value) VALUES (5, '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID);

-- Equality
SELECT id FROM test_uuid_comparison WHERE uuid_value = '550e8400-e29b-41d4-a716-446655440000'::UUID;

-- Inequality
SELECT id FROM test_uuid_comparison WHERE uuid_value != '00000000-0000-0000-0000-000000000001'::UUID
ORDER BY id;

-- Less than / Greater than
SELECT id, uuid_value FROM test_uuid_comparison WHERE uuid_value < '550e8400-e29b-41d4-a716-446655440000'::UUID
ORDER BY uuid_value;

-- Sorting (ascending)
SELECT id, uuid_value FROM test_uuid_comparison ORDER BY uuid_value ASC;

-- Sorting (descending)
SELECT id, uuid_value FROM test_uuid_comparison ORDER BY uuid_value DESC;

-- BETWEEN
SELECT id, uuid_value FROM test_uuid_comparison
WHERE uuid_value BETWEEN '00000000-0000-0000-0000-000000000000'::UUID
                    AND '550e8400-e29b-41d4-a716-446655440000'::UUID
ORDER BY uuid_value;

-- ============================================================================
-- UUID v7 Time-Ordering Properties
-- ============================================================================

CREATE TABLE test_uuidv7_ordering (
    id INTEGER PRIMARY KEY,
    uuid_value UUID,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Generate multiple UUIDv7 values with slight delays
INSERT INTO test_uuidv7_ordering (id, uuid_value) VALUES (1, UUIDv7());
INSERT INTO test_uuidv7_ordering (id, uuid_value) VALUES (2, UUIDv7());
INSERT INTO test_uuidv7_ordering (id, uuid_value) VALUES (3, UUIDv7());
INSERT INTO test_uuidv7_ordering (id, uuid_value) VALUES (4, UUIDv7());
INSERT INTO test_uuidv7_ordering (id, uuid_value) VALUES (5, UUIDv7());

-- Verify time ordering (UUIDv7 should increase with time)
SELECT id, uuid_value, created_at FROM test_uuidv7_ordering ORDER BY id;
SELECT id, uuid_value, created_at FROM test_uuidv7_ordering ORDER BY uuid_value;

-- Extract timestamp from UUIDv7 (if supported)
SELECT id, uuid_value,
       UUID_EXTRACT_TIMESTAMP(uuid_value) AS extracted_timestamp,
       created_at
FROM test_uuidv7_ordering
ORDER BY id;

-- ============================================================================
-- UUID as Primary Key
-- ============================================================================

CREATE TABLE test_uuid_primary_key (
    id UUID PRIMARY KEY DEFAULT UUIDv7(),
    name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert with auto-generated UUID
INSERT INTO test_uuid_primary_key (name) VALUES ('Record 1');
INSERT INTO test_uuid_primary_key (name) VALUES ('Record 2');
INSERT INTO test_uuid_primary_key (name) VALUES ('Record 3');

-- Insert with explicit UUID
INSERT INTO test_uuid_primary_key (id, name) VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'Record 4');

SELECT id, name, created_at FROM test_uuid_primary_key ORDER BY created_at;

-- Duplicate UUID should fail
-- INSERT INTO test_uuid_primary_key (id, name) VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'Duplicate');

-- ============================================================================
-- UUID as Foreign Key
-- ============================================================================

CREATE TABLE test_uuid_parent (
    id UUID PRIMARY KEY DEFAULT UUIDv7(),
    parent_name VARCHAR(100)
);

CREATE TABLE test_uuid_child (
    id UUID PRIMARY KEY DEFAULT UUIDv7(),
    parent_id UUID REFERENCES test_uuid_parent(id),
    child_name VARCHAR(100)
);

-- Insert parent records
INSERT INTO test_uuid_parent (id, parent_name) VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'Parent 1');
INSERT INTO test_uuid_parent (id, parent_name) VALUES ('6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'Parent 2');

-- Insert child records
INSERT INTO test_uuid_child (parent_id, child_name) VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'Child 1.1');
INSERT INTO test_uuid_child (parent_id, child_name) VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'Child 1.2');
INSERT INTO test_uuid_child (parent_id, child_name) VALUES ('6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'Child 2.1');

-- Join on UUID foreign key
SELECT p.parent_name, c.child_name
FROM test_uuid_parent p
JOIN test_uuid_child c ON p.id = c.parent_id
ORDER BY p.parent_name, c.child_name;

-- ============================================================================
-- UUID Conversion Functions
-- ============================================================================

CREATE TABLE test_uuid_conversion (
    id INTEGER PRIMARY KEY,
    uuid_value UUID
);

INSERT INTO test_uuid_conversion VALUES (1, '550e8400-e29b-41d4-a716-446655440000'::UUID);

-- UUID to string
SELECT id, uuid_value,
       uuid_value::VARCHAR AS as_string,
       UUID_TO_STRING(uuid_value) AS to_string
FROM test_uuid_conversion;

-- UUID to binary
SELECT id, uuid_value,
       UUID_TO_BINARY(uuid_value) AS as_binary,
       OCTET_LENGTH(UUID_TO_BINARY(uuid_value)) AS binary_length
FROM test_uuid_conversion;

-- Binary to UUID
SELECT id,
       BINARY_TO_UUID(UUID_TO_BINARY(uuid_value)) AS round_trip
FROM test_uuid_conversion;

-- ============================================================================
-- UUID NULL Handling
-- ============================================================================

CREATE TABLE test_uuid_nulls (
    id INTEGER PRIMARY KEY,
    nullable_uuid UUID,
    description VARCHAR(50)
);

INSERT INTO test_uuid_nulls VALUES (1, NULL, 'No UUID');
INSERT INTO test_uuid_nulls VALUES (2, UUIDv7(), 'Has UUID');
INSERT INTO test_uuid_nulls VALUES (3, NULL, 'Another null');
INSERT INTO test_uuid_nulls VALUES (4, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'Specific UUID');

SELECT COUNT(*) AS null_count FROM test_uuid_nulls WHERE nullable_uuid IS NULL;
SELECT COUNT(*) AS not_null_count FROM test_uuid_nulls WHERE nullable_uuid IS NOT NULL;

-- COALESCE with nil UUID
SELECT id, description,
       COALESCE(nullable_uuid, '00000000-0000-0000-0000-000000000000'::UUID) AS uuid_with_default
FROM test_uuid_nulls
ORDER BY id;

-- ============================================================================
-- UUID Indexing
-- ============================================================================

CREATE TABLE test_uuid_index (
    id UUID PRIMARY KEY,
    data VARCHAR(100),
    category VARCHAR(50)
);

-- Create additional indexes
CREATE INDEX idx_uuid_category ON test_uuid_index(category);
CREATE INDEX idx_uuid_data ON test_uuid_index(data);

-- Insert test data
INSERT INTO test_uuid_index VALUES (UUIDv7(), 'Data 1', 'Category A');
INSERT INTO test_uuid_index VALUES (UUIDv7(), 'Data 2', 'Category A');
INSERT INTO test_uuid_index VALUES (UUIDv7(), 'Data 3', 'Category B');
INSERT INTO test_uuid_index VALUES (UUIDv7(), 'Data 4', 'Category B');
INSERT INTO test_uuid_index VALUES (UUIDv7(), 'Data 5', 'Category C');

-- Query by UUID (uses primary key index)
SELECT data FROM test_uuid_index WHERE id = (SELECT MIN(id) FROM test_uuid_index);

-- Query by category (uses secondary index)
SELECT COUNT(*) FROM test_uuid_index WHERE category = 'Category A';

-- Range scan on UUID
SELECT id, data FROM test_uuid_index
WHERE id >= (SELECT MIN(id) FROM test_uuid_index)
  AND id <= (SELECT MAX(id) FROM test_uuid_index)
ORDER BY id
LIMIT 3;

-- ============================================================================
-- UUID Aggregation
-- ============================================================================

CREATE TABLE test_uuid_aggregation (
    id INTEGER PRIMARY KEY,
    category VARCHAR(20),
    uuid_value UUID
);

INSERT INTO test_uuid_aggregation VALUES (1, 'Group A', UUIDv7());
INSERT INTO test_uuid_aggregation VALUES (2, 'Group A', UUIDv7());
INSERT INTO test_uuid_aggregation VALUES (3, 'Group A', UUIDv7());
INSERT INTO test_uuid_aggregation VALUES (4, 'Group B', UUIDv7());
INSERT INTO test_uuid_aggregation VALUES (5, 'Group B', UUIDv7());

-- Count UUIDs by category
SELECT category, COUNT(uuid_value) AS uuid_count
FROM test_uuid_aggregation
GROUP BY category
ORDER BY category;

-- MIN/MAX UUID
SELECT category,
       MIN(uuid_value) AS min_uuid,
       MAX(uuid_value) AS max_uuid
FROM test_uuid_aggregation
GROUP BY category
ORDER BY category;

-- ============================================================================
-- UUID Array Operations
-- ============================================================================

CREATE TABLE test_uuid_array (
    id INTEGER PRIMARY KEY,
    uuid_list UUID[],
    description VARCHAR(100)
);

INSERT INTO test_uuid_array VALUES (1,
    ARRAY[UUIDv7(), UUIDv7(), UUIDv7()],
    'Array of 3 UUIDs'
);

-- Array access
SELECT id, description,
       uuid_list[1] AS first_uuid,
       uuid_list[2] AS second_uuid,
       uuid_list[3] AS third_uuid,
       array_length(uuid_list, 1) AS array_size
FROM test_uuid_array;

-- Check if UUID in array
SELECT id, description
FROM test_uuid_array
WHERE uuid_list[1] = ANY(uuid_list);

-- ============================================================================
-- UUID Uniqueness Testing
-- ============================================================================

CREATE TABLE test_uuid_uniqueness (
    id INTEGER PRIMARY KEY,
    uuid_value UUID UNIQUE
);

-- Insert UUIDs
INSERT INTO test_uuid_uniqueness VALUES (1, UUIDv7());
INSERT INTO test_uuid_uniqueness VALUES (2, UUIDv7());
INSERT INTO test_uuid_uniqueness VALUES (3, UUIDv7());

-- Verify uniqueness
SELECT COUNT(*) AS total_count,
       COUNT(DISTINCT uuid_value) AS unique_count
FROM test_uuid_uniqueness;

-- Duplicate should fail
-- INSERT INTO test_uuid_uniqueness VALUES (4, (SELECT uuid_value FROM test_uuid_uniqueness WHERE id = 1));

-- ============================================================================
-- UUID Performance Characteristics
-- ============================================================================

CREATE TABLE test_uuid_performance (
    uuidv4_pk UUID PRIMARY KEY,
    uuidv7_val UUID,
    sequential_int SERIAL,
    data VARCHAR(100)
);

-- Insert with UUIDv4 as PK (random, poor locality)
INSERT INTO test_uuid_performance (uuidv4_pk, uuidv7_val, data)
VALUES (UUIDv4(), UUIDv7(), 'Test 1');

INSERT INTO test_uuid_performance (uuidv4_pk, uuidv7_val, data)
VALUES (UUIDv4(), UUIDv7(), 'Test 2');

INSERT INTO test_uuid_performance (uuidv4_pk, uuidv7_val, data)
VALUES (UUIDv4(), UUIDv7(), 'Test 3');

-- Compare ordering
SELECT uuidv4_pk, uuidv7_val, sequential_int FROM test_uuid_performance ORDER BY uuidv4_pk;
SELECT uuidv4_pk, uuidv7_val, sequential_int FROM test_uuid_performance ORDER BY uuidv7_val;
SELECT uuidv4_pk, uuidv7_val, sequential_int FROM test_uuid_performance ORDER BY sequential_int;

-- ============================================================================
-- UUID Real-World Use Cases
-- ============================================================================

-- Distributed system IDs
CREATE TABLE test_distributed_records (
    record_id UUID PRIMARY KEY DEFAULT UUIDv7(),
    node_id VARCHAR(50),
    data TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_distributed_records (node_id, data) VALUES ('node-1', 'Data from node 1');
INSERT INTO test_distributed_records (node_id, data) VALUES ('node-2', 'Data from node 2');
INSERT INTO test_distributed_records (node_id, data) VALUES ('node-3', 'Data from node 3');

-- Records maintain global ordering via UUIDv7
SELECT record_id, node_id, data, created_at
FROM test_distributed_records
ORDER BY record_id;

-- API tokens/session IDs
CREATE TABLE test_sessions (
    session_id UUID PRIMARY KEY DEFAULT UUIDv7(),
    user_id INTEGER,
    expires_at TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_sessions (user_id, expires_at)
VALUES (101, CURRENT_TIMESTAMP + INTERVAL '1 hour');

INSERT INTO test_sessions (user_id, expires_at)
VALUES (102, CURRENT_TIMESTAMP + INTERVAL '2 hours');

-- Lookup by session ID
SELECT user_id, expires_at
FROM test_sessions
WHERE session_id = (SELECT MIN(session_id) FROM test_sessions);

-- Object storage keys
CREATE TABLE test_object_storage (
    object_id UUID PRIMARY KEY DEFAULT UUIDv7(),
    bucket VARCHAR(100),
    key VARCHAR(500),
    size_bytes BIGINT,
    content_type VARCHAR(100)
);

INSERT INTO test_object_storage (bucket, key, size_bytes, content_type)
VALUES ('images', 'user/123/avatar.jpg', 102400, 'image/jpeg');

INSERT INTO test_object_storage (bucket, key, size_bytes, content_type)
VALUES ('documents', 'reports/2024/q1.pdf', 512000, 'application/pdf');

-- Query objects
SELECT object_id, bucket, key, size_bytes
FROM test_object_storage
ORDER BY object_id;

-- Event sourcing
CREATE TABLE test_events (
    event_id UUID PRIMARY KEY DEFAULT UUIDv7(),
    aggregate_id UUID,
    event_type VARCHAR(100),
    event_data JSONB,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_events (aggregate_id, event_type, event_data)
VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'OrderCreated', '{"order_total": 99.99}');

INSERT INTO test_events (aggregate_id, event_type, event_data)
VALUES ('550e8400-e29b-41d4-a716-446655440000'::UUID, 'OrderShipped', '{"tracking": "123456"}');

-- Event stream ordered by UUID
SELECT event_id, aggregate_id, event_type, timestamp
FROM test_events
WHERE aggregate_id = '550e8400-e29b-41d4-a716-446655440000'::UUID
ORDER BY event_id;

-- ============================================================================
-- UUID Namespace-based Generation (v5)
-- ============================================================================

-- UUID v5 (namespace + name SHA-1 hash) - if supported
CREATE TABLE test_uuidv5 (
    id INTEGER PRIMARY KEY,
    namespace UUID,
    name VARCHAR(100),
    generated_uuid UUID
);

-- DNS namespace: 6ba7b810-9dad-11d1-80b4-00c04fd430c8
INSERT INTO test_uuidv5 VALUES (1, '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'example.com',
    UUIDv5('6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'example.com'));

-- Same namespace + name should produce same UUID
INSERT INTO test_uuidv5 VALUES (2, '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'example.com',
    UUIDv5('6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'example.com'));

-- Different name should produce different UUID
INSERT INTO test_uuidv5 VALUES (3, '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'different.com',
    UUIDv5('6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'different.com'));

-- Verify deterministic generation
SELECT id, name, generated_uuid FROM test_uuidv5 ORDER BY id;
SELECT name, COUNT(DISTINCT generated_uuid) AS unique_uuids
FROM test_uuidv5
GROUP BY name;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_uuidv5;
DROP TABLE test_events;
DROP TABLE test_object_storage;
DROP TABLE test_sessions;
DROP TABLE test_distributed_records;
DROP TABLE test_uuid_performance;
DROP TABLE test_uuid_uniqueness;
DROP TABLE test_uuid_array;
DROP TABLE test_uuid_aggregation;
DROP INDEX idx_uuid_data;
DROP INDEX idx_uuid_category;
DROP TABLE test_uuid_index;
DROP TABLE test_uuid_nulls;
DROP TABLE test_uuid_conversion;
DROP TABLE test_uuid_child;
DROP TABLE test_uuid_parent;
DROP TABLE test_uuid_primary_key;
DROP TABLE test_uuidv7_ordering;
DROP TABLE test_uuid_comparison;
DROP TABLE test_uuid_versions;
DROP TABLE test_uuid_formats;
DROP TABLE test_uuid_basic;

DROP DATABASE test_uuid;
