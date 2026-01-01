-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - Hash Basic
-- Description: Comprehensive Hash index testing
-- ============================================================================

-- Hash indexes are optimized for equality comparisons only
-- Supports: = operator
-- Does NOT support: <, <=, >, >=, BETWEEN, ORDER BY
-- Best for: exact match queries on high-cardinality columns
-- Note: Since PostgreSQL 10, hash indexes are WAL-logged and crash-safe

-- Create test database
CREATE DATABASE test_hash_basic_db;
USE test_hash_basic_db;

-- ============================================================================
-- Section 1: Basic Hash Index Creation
-- ============================================================================

CREATE TABLE test_hash_simple (
    id INT PRIMARY KEY,
    username VARCHAR(100),
    user_id UUID,
    session_token VARCHAR(200)
);

-- Create hash indexes
CREATE INDEX idx_username_hash ON test_hash_simple USING HASH (username);
CREATE INDEX idx_user_id_hash ON test_hash_simple USING HASH (user_id);
CREATE INDEX idx_session_hash ON test_hash_simple USING HASH (session_token);

INSERT INTO test_hash_simple VALUES
    (1, 'alice', '550e8400-e29b-41d4-a716-446655440000'::UUID, 'token_abc123'),
    (2, 'bob', '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, 'token_def456'),
    (3, 'charlie', '7c9e6679-7425-40de-944b-e07fc1f90ae7'::UUID, 'token_ghi789'),
    (4, 'diana', '886313e1-3b8a-5372-9b90-0c9aee199e5d'::UUID, 'token_jkl012');

-- Equality queries (uses hash index)
SELECT id, username FROM test_hash_simple WHERE username = 'alice';
SELECT id FROM test_hash_simple WHERE user_id = '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID;
SELECT id, username FROM test_hash_simple WHERE session_token = 'token_ghi789';

-- ============================================================================
-- Section 2: Hash vs B-tree Comparison
-- ============================================================================

CREATE TABLE test_hash_vs_btree (
    record_id INT PRIMARY KEY,
    hash_col VARCHAR(100),
    btree_col VARCHAR(100)
);

CREATE INDEX idx_hash_col ON test_hash_vs_btree USING HASH (hash_col);
CREATE INDEX idx_btree_col ON test_hash_vs_btree USING BTREE (btree_col);

INSERT INTO test_hash_vs_btree
SELECT i, 'hash_' || i, 'btree_' || i
FROM generate_series(1, 1000) i;

-- Equality: both work
SELECT record_id FROM test_hash_vs_btree WHERE hash_col = 'hash_500';
SELECT record_id FROM test_hash_vs_btree WHERE btree_col = 'btree_500';

-- Range query: only B-tree works (hash index not used)
SELECT record_id FROM test_hash_vs_btree WHERE btree_col > 'btree_900' LIMIT 5;

-- Sorting: only B-tree helps (hash index not used)
SELECT record_id, btree_col FROM test_hash_vs_btree ORDER BY btree_col LIMIT 5;

-- ============================================================================
-- Section 3: Hash Index on Different Data Types
-- ============================================================================

CREATE TABLE test_hash_types (
    id INT PRIMARY KEY,
    int_col INT,
    text_col TEXT,
    varchar_col VARCHAR(100),
    uuid_col UUID,
    boolean_col BOOLEAN,
    inet_col INET,
    macaddr_col MACADDR
);

-- Hash indexes on various types
CREATE INDEX idx_int_hash ON test_hash_types USING HASH (int_col);
CREATE INDEX idx_text_hash ON test_hash_types USING HASH (text_col);
CREATE INDEX idx_varchar_hash ON test_hash_types USING HASH (varchar_col);
CREATE INDEX idx_uuid_hash ON test_hash_types USING HASH (uuid_col);
CREATE INDEX idx_boolean_hash ON test_hash_types USING HASH (boolean_col);
CREATE INDEX idx_inet_hash ON test_hash_types USING HASH (inet_col);
CREATE INDEX idx_macaddr_hash ON test_hash_types USING HASH (macaddr_col);

INSERT INTO test_hash_types VALUES
    (1, 100, 'sample text', 'varchar data',
     '550e8400-e29b-41d4-a716-446655440000'::UUID, true,
     '192.168.1.1'::INET, '00:11:22:33:44:55'::MACADDR),
    (2, 200, 'more text', 'more varchar',
     '6ba7b810-9dad-11d1-80b4-00c04fd430c8'::UUID, false,
     '10.0.0.1'::INET, 'aa:bb:cc:dd:ee:ff'::MACADDR);

-- Query each type
SELECT id FROM test_hash_types WHERE int_col = 100;
SELECT id FROM test_hash_types WHERE text_col = 'sample text';
SELECT id FROM test_hash_types WHERE uuid_col = '550e8400-e29b-41d4-a716-446655440000'::UUID;
SELECT id FROM test_hash_types WHERE boolean_col = true;
SELECT id FROM test_hash_types WHERE inet_col = '192.168.1.1'::INET;
SELECT id FROM test_hash_types WHERE macaddr_col = '00:11:22:33:44:55'::MACADDR;

-- ============================================================================
-- Section 4: Hash Index for Exact Lookups
-- ============================================================================

CREATE TABLE test_hash_lookups (
    product_id INT PRIMARY KEY,
    sku VARCHAR(50),
    barcode VARCHAR(100),
    serial_number VARCHAR(100)
);

CREATE INDEX idx_sku_hash ON test_hash_lookups USING HASH (sku);
CREATE INDEX idx_barcode_hash ON test_hash_lookups USING HASH (barcode);
CREATE INDEX idx_serial_hash ON test_hash_lookups USING HASH (serial_number);

INSERT INTO test_hash_lookups VALUES
    (1, 'SKU-ABC-001', '123456789012', 'SN-2024-001'),
    (2, 'SKU-ABC-002', '123456789013', 'SN-2024-002'),
    (3, 'SKU-XYZ-001', '987654321098', 'SN-2024-003'),
    (4, 'SKU-XYZ-002', '987654321099', 'SN-2024-004');

-- Fast exact lookups
SELECT product_id, sku FROM test_hash_lookups WHERE sku = 'SKU-ABC-001';
SELECT product_id FROM test_hash_lookups WHERE barcode = '987654321098';
SELECT product_id FROM test_hash_lookups WHERE serial_number = 'SN-2024-003';

-- ============================================================================
-- Section 5: Hash Index NULL Handling
-- ============================================================================

CREATE TABLE test_hash_nulls (
    id INT PRIMARY KEY,
    nullable_col VARCHAR(100)
);

CREATE INDEX idx_nullable_hash ON test_hash_nulls USING HASH (nullable_col);

INSERT INTO test_hash_nulls VALUES
    (1, 'value1'),
    (2, NULL),
    (3, 'value3'),
    (4, NULL);

-- Hash indexes handle NULLs
SELECT id FROM test_hash_nulls WHERE nullable_col IS NULL ORDER BY id;
SELECT id FROM test_hash_nulls WHERE nullable_col IS NOT NULL ORDER BY id;

-- Exact NULL lookup
SELECT id FROM test_hash_nulls WHERE nullable_col = 'value1';

-- ============================================================================
-- Section 6: Hash Index for Session Management
-- ============================================================================

CREATE TABLE test_hash_sessions (
    session_id VARCHAR(64) PRIMARY KEY,
    user_id INT,
    session_data TEXT,
    created_at TIMESTAMP,
    last_accessed TIMESTAMP
);

-- Hash index on session_id for fast lookups
CREATE INDEX idx_session_hash ON test_hash_sessions USING HASH (session_id);

INSERT INTO test_hash_sessions VALUES
    ('sess_a1b2c3d4e5f6', 101, 'session data 1', '2024-01-15 10:00:00', '2024-01-15 10:30:00'),
    ('sess_g7h8i9j0k1l2', 102, 'session data 2', '2024-01-15 11:00:00', '2024-01-15 11:15:00'),
    ('sess_m3n4o5p6q7r8', 103, 'session data 3', '2024-01-15 12:00:00', '2024-01-15 12:45:00');

-- Fast session lookup
SELECT user_id, session_data FROM test_hash_sessions
WHERE session_id = 'sess_g7h8i9j0k1l2';

-- ============================================================================
-- Section 7: Hash Index for API Keys/Tokens
-- ============================================================================

CREATE TABLE test_hash_api_keys (
    key_id INT PRIMARY KEY,
    api_key VARCHAR(128),
    user_id INT,
    permissions TEXT,
    created_at TIMESTAMP
);

CREATE INDEX idx_api_key_hash ON test_hash_api_keys USING HASH (api_key);

INSERT INTO test_hash_api_keys VALUES
    (1, 'sk_live_abc123def456ghi789', 201, 'read,write', '2024-01-15 10:00:00'),
    (2, 'sk_live_jkl012mno345pqr678', 202, 'read', '2024-01-16 11:00:00'),
    (3, 'sk_test_stu901vwx234yz567', 203, 'read,write,admin', '2024-01-17 12:00:00');

-- Validate API key
SELECT key_id, user_id, permissions FROM test_hash_api_keys
WHERE api_key = 'sk_live_abc123def456ghi789';

-- ============================================================================
-- Section 8: Hash Index for Cache Keys
-- ============================================================================

CREATE TABLE test_hash_cache (
    cache_key VARCHAR(200) PRIMARY KEY,
    cache_value TEXT,
    expires_at TIMESTAMP
);

CREATE INDEX idx_cache_key_hash ON test_hash_cache USING HASH (cache_key);

INSERT INTO test_hash_cache VALUES
    ('user:101:profile', '{"name":"Alice","email":"alice@example.com"}', '2024-01-15 11:00:00'),
    ('product:501:details', '{"name":"Widget","price":29.99}', '2024-01-15 12:00:00'),
    ('session:abc123:data', '{"cart_items":5}', '2024-01-15 13:00:00');

-- Fast cache lookup
SELECT cache_value FROM test_hash_cache
WHERE cache_key = 'user:101:profile' AND expires_at > CURRENT_TIMESTAMP;

-- ============================================================================
-- Section 9: Hash Index Size and Performance
-- ============================================================================

CREATE TABLE test_hash_performance (
    id INT PRIMARY KEY,
    hash_lookup VARCHAR(100),
    data VARCHAR(200)
);

CREATE INDEX idx_hash_perf ON test_hash_performance USING HASH (hash_lookup);

-- Insert test data
INSERT INTO test_hash_performance
SELECT i, 'key_' || i, 'data_' || i
FROM generate_series(1, 10000) i;

-- Exact match query (fast with hash index)
SELECT id, data FROM test_hash_performance WHERE hash_lookup = 'key_5000';

-- Check index size
SELECT pg_size_pretty(pg_relation_size('idx_hash_perf')) AS hash_index_size;

-- ============================================================================
-- Section 10: Hash Index Limitations
-- ============================================================================

CREATE TABLE test_hash_limitations (
    id INT PRIMARY KEY,
    value VARCHAR(100)
);

CREATE INDEX idx_value_hash ON test_hash_limitations USING HASH (value);

INSERT INTO test_hash_limitations VALUES
    (1, 'apple'),
    (2, 'banana'),
    (3, 'cherry'),
    (4, 'date'),
    (5, 'elderberry');

-- Works: exact match
SELECT id FROM test_hash_limitations WHERE value = 'banana';

-- Does NOT use hash index: range queries
SELECT id FROM test_hash_limitations WHERE value > 'cherry';
SELECT id FROM test_hash_limitations WHERE value BETWEEN 'apple' AND 'cherry';

-- Does NOT use hash index: pattern matching
SELECT id FROM test_hash_limitations WHERE value LIKE 'ba%';

-- Does NOT use hash index: sorting
SELECT id, value FROM test_hash_limitations ORDER BY value;

-- Does NOT use hash index: IN with many values might not
SELECT id FROM test_hash_limitations WHERE value IN ('apple', 'banana');

-- ============================================================================
-- Section 11: Hash Index Maintenance
-- ============================================================================

CREATE TABLE test_hash_maintenance (
    id INT PRIMARY KEY,
    lookup_key VARCHAR(100)
);

CREATE INDEX idx_lookup_hash ON test_hash_maintenance USING HASH (lookup_key);

INSERT INTO test_hash_maintenance
SELECT i, 'key_' || i FROM generate_series(1, 1000) i;

-- Reindex hash index
REINDEX INDEX idx_lookup_hash;

-- Vacuum table
VACUUM test_hash_maintenance;

-- ============================================================================
-- Section 12: When to Use Hash Indexes
-- ============================================================================

CREATE TABLE test_hash_use_cases (
    id INT PRIMARY KEY,
    use_case TEXT,
    example TEXT
);

INSERT INTO test_hash_use_cases VALUES
    (1, 'Session lookups', 'WHERE session_id = ''abc123'''),
    (2, 'API key validation', 'WHERE api_key = ''sk_live_...'''),
    (3, 'UUID lookups', 'WHERE user_uuid = ''550e8400-...'''),
    (4, 'Cache key lookups', 'WHERE cache_key = ''user:101:profile'''),
    (5, 'Product SKU lookups', 'WHERE sku = ''SKU-ABC-001'''),
    (6, 'Barcode scanning', 'WHERE barcode = ''123456789012'''),
    (7, 'MAC address lookup', 'WHERE mac_address = ''00:11:22:33:44:55'''),
    (8, 'IP address exact match', 'WHERE ip_address = ''192.168.1.1'''),
    (9, 'Token verification', 'WHERE token = ''verification_token_xyz'''),
    (10, 'Hash-based sharding keys', 'WHERE shard_key = ''shard_5''');

SELECT id, use_case, example FROM test_hash_use_cases ORDER BY id;

-- ============================================================================
-- Section 13: Hash Index Statistics
-- ============================================================================

CREATE TABLE test_hash_stats (
    id INT PRIMARY KEY,
    key_col VARCHAR(100)
);

CREATE INDEX idx_key_hash_stats ON test_hash_stats USING HASH (key_col);

INSERT INTO test_hash_stats
SELECT i, 'key_' || (i % 100)
FROM generate_series(1, 1000) i;

-- Check index usage
SELECT id FROM test_hash_stats WHERE key_col = 'key_50' LIMIT 5;

-- ============================================================================
-- Section 14: Hash Index Concurrency
-- ============================================================================

CREATE TABLE test_hash_concurrency (
    id SERIAL PRIMARY KEY,
    user_token VARCHAR(128),
    user_data TEXT
);

CREATE INDEX idx_token_hash ON test_hash_concurrency USING HASH (user_token);

-- Hash indexes support concurrent inserts
INSERT INTO test_hash_concurrency (user_token, user_data) VALUES
    ('token_001', 'data1'),
    ('token_002', 'data2'),
    ('token_003', 'data3');

-- Concurrent lookups
SELECT id FROM test_hash_concurrency WHERE user_token = 'token_002';

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_hash_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_hash_best_practices VALUES
    (1, 'Use hash indexes ONLY for equality comparisons (=)'),
    (2, 'Hash indexes are WAL-logged (since PostgreSQL 10)'),
    (3, 'Best for high-cardinality columns with exact lookups'),
    (4, 'Do NOT use for: <, >, <=, >=, BETWEEN, ORDER BY, LIKE'),
    (5, 'Ideal for: session IDs, API keys, UUIDs, tokens'),
    (6, 'Generally smaller than equivalent B-tree indexes'),
    (7, 'Cannot be used for index-only scans'),
    (8, 'Cannot enforce UNIQUE constraints (use B-tree)'),
    (9, 'Good for cache key lookups and hash-based routing'),
    (10, 'Consider B-tree if you need range queries or sorting'),
    (11, 'Hash indexes handle NULL values'),
    (12, 'Reindex periodically if many updates/deletes'),
    (13, 'Faster than B-tree for pure equality, but less versatile'),
    (14, 'Use when ONLY equality lookups are needed'),
    (15, 'Monitor index usage to ensure benefit over B-tree');

SELECT id, guideline FROM test_hash_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_hash_best_practices;
DROP TABLE test_hash_concurrency;
DROP TABLE test_hash_stats;
DROP TABLE test_hash_use_cases;
DROP TABLE test_hash_maintenance;
DROP TABLE test_hash_limitations;
DROP TABLE test_hash_performance;
DROP TABLE test_hash_cache;
DROP TABLE test_hash_api_keys;
DROP TABLE test_hash_sessions;
DROP TABLE test_hash_nulls;
DROP TABLE test_hash_lookups;
DROP TABLE test_hash_types;
DROP TABLE test_hash_vs_btree;
DROP TABLE test_hash_simple;

DROP DATABASE test_hash_basic_db;

-- End of Hash index tests
