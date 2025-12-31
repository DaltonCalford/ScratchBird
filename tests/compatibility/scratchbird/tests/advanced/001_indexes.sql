-- ScratchBird Native Test: Advanced Indexing
-- Test ID: advanced_001
-- Description: B-Tree, GiST, GIN, BRIN, and other index types

-- Create database
CREATE DATABASE test_indexes;
\c test_indexes;

-- Test 1: B-Tree Index (default)
CREATE TABLE users (
    user_id INTEGER PRIMARY KEY,
    username VARCHAR(50) UNIQUE,
    email VARCHAR(100),
    created_at TIMESTAMP
);

CREATE INDEX idx_users_email ON users(email);
CREATE INDEX idx_users_created ON users(created_at);

-- Insert test data
INSERT INTO users SELECT
    generate_series(1, 1000) AS user_id,
    'user_' || generate_series(1, 1000) AS username,
    'user' || generate_series(1, 1000) || '@example.com' AS email,
    CURRENT_TIMESTAMP - (generate_series(1, 1000) || ' days')::INTERVAL AS created_at;

-- Query using index
EXPLAIN SELECT * FROM users WHERE email = 'user500@example.com';
SELECT user_id, username FROM users WHERE email = 'user500@example.com';

-- Test 2: Composite Index
CREATE INDEX idx_users_username_email ON users(username, email);

EXPLAIN SELECT * FROM users WHERE username = 'user_100' AND email = 'user100@example.com';

-- Test 3: Partial Index
CREATE INDEX idx_recent_users ON users(created_at)
    WHERE created_at > CURRENT_TIMESTAMP - INTERVAL '30 days';

EXPLAIN SELECT * FROM users
WHERE created_at > CURRENT_TIMESTAMP - INTERVAL '15 days';

-- Test 4: Expression Index
CREATE INDEX idx_users_lower_username ON users(LOWER(username));

EXPLAIN SELECT * FROM users WHERE LOWER(username) = 'user_50';

-- Test 5: GiST Index for Geometric Data
CREATE TABLE locations (
    location_id INTEGER PRIMARY KEY,
    location_name VARCHAR(100),
    coordinates POINT
);

CREATE INDEX idx_locations_gist ON locations USING GIST(coordinates);

INSERT INTO locations VALUES
    (1, 'Point A', POINT(10.0, 20.0)),
    (2, 'Point B', POINT(15.0, 25.0)),
    (3, 'Point C', POINT(30.0, 40.0));

-- Query using GiST index
SELECT location_name FROM locations
WHERE coordinates <-> POINT(12.0, 22.0) < 10;

-- Test 6: GIN Index for Full-Text Search
CREATE TABLE documents (
    doc_id INTEGER PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    search_vector TSVECTOR
);

CREATE INDEX idx_documents_gin ON documents USING GIN(search_vector);

INSERT INTO documents VALUES
    (1, 'Database Systems', 'This document discusses database management systems',
     to_tsvector('This document discusses database management systems')),
    (2, 'Web Development', 'Guide to modern web development techniques',
     to_tsvector('Guide to modern web development techniques'));

-- Full-text search using GIN index
SELECT title FROM documents
WHERE search_vector @@ to_tsquery('database');

-- Test 7: BRIN Index for Large Sequential Data
CREATE TABLE sensor_readings (
    reading_id BIGINT PRIMARY KEY,
    sensor_id INTEGER,
    temperature DECIMAL(5, 2),
    recorded_at TIMESTAMP
);

CREATE INDEX idx_readings_brin ON sensor_readings USING BRIN(recorded_at);

-- Insert sequential data
INSERT INTO sensor_readings SELECT
    generate_series(1, 10000) AS reading_id,
    (random() * 100)::INTEGER AS sensor_id,
    (random() * 100)::DECIMAL(5, 2) AS temperature,
    CURRENT_TIMESTAMP - (generate_series(1, 10000) || ' minutes')::INTERVAL AS recorded_at;

-- Range query using BRIN index
EXPLAIN SELECT AVG(temperature) FROM sensor_readings
WHERE recorded_at BETWEEN CURRENT_TIMESTAMP - INTERVAL '1 day'
                     AND CURRENT_TIMESTAMP;

-- Test 8: Hash Index
CREATE TABLE hash_test (
    id INTEGER PRIMARY KEY,
    hash_key VARCHAR(32)
);

CREATE INDEX idx_hash_key ON hash_test USING HASH(hash_key);

INSERT INTO hash_test SELECT
    generate_series(1, 100),
    md5(generate_series(1, 100)::TEXT);

-- Equality search using hash index
EXPLAIN SELECT * FROM hash_test WHERE hash_key = md5('50');

-- Test 9: Unique Index
CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    sku VARCHAR(50),
    product_name VARCHAR(100)
);

CREATE UNIQUE INDEX idx_products_sku ON products(sku);

INSERT INTO products VALUES (1, 'SKU-001', 'Widget A');
INSERT INTO products VALUES (2, 'SKU-002', 'Widget B');

-- This should fail due to unique constraint
-- INSERT INTO products VALUES (3, 'SKU-001', 'Widget C');

-- Test 10: Covering Index
CREATE INDEX idx_users_covering ON users(username) INCLUDE (email);

-- Index-only scan
EXPLAIN SELECT username, email FROM users WHERE username = 'user_100';

-- Test 11: Index Rebuild
REINDEX INDEX idx_users_email;
REINDEX TABLE users;

-- Test 12: Concurrent Index Creation (background)
CREATE INDEX CONCURRENTLY idx_users_concurrent ON users(user_id, username);

-- Test 13: Index Statistics
ANALYZE users;
ANALYZE documents;

-- View index usage
SELECT schemaname, tablename, indexname, idx_scan, idx_tup_read, idx_tup_fetch
FROM pg_stat_user_indexes
WHERE tablename IN ('users', 'documents', 'sensor_readings')
ORDER BY tablename, indexname;

-- Test 14: Drop Unused Indexes
DROP INDEX idx_users_concurrent;
DROP INDEX idx_hash_key;

-- Verify indexes
\di;

-- Cleanup
DROP TABLE sensor_readings CASCADE;
DROP TABLE documents CASCADE;
DROP TABLE locations CASCADE;
DROP TABLE hash_test CASCADE;
DROP TABLE products CASCADE;
DROP TABLE users CASCADE;

DROP DATABASE test_indexes;
