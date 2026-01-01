-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Indexes - GIN Basic
-- Description: Comprehensive GIN (Generalized Inverted Index) testing
-- ============================================================================

-- GIN indexes are optimized for composite values (arrays, JSONB, full-text)
-- Supports: @>, <@, ?, ?&, ?|, && operators
-- Best for: ARRAY containment, JSONB queries, full-text search
-- Characteristics: Fast search, slower updates, larger than B-tree
-- Note: GIN can use "fastupdate" option to speed up insertions

-- Create test database
CREATE DATABASE test_gin_basic_db;
USE test_gin_basic_db;

-- ============================================================================
-- Section 1: GIN on ARRAY - Basic Containment
-- ============================================================================

CREATE TABLE test_gin_array_basic (
    id INT PRIMARY KEY,
    tags TEXT[],
    numbers INT[],
    features VARCHAR(100)[]
);

-- GIN index on arrays
CREATE INDEX idx_tags_gin ON test_gin_array_basic USING GIN (tags);
CREATE INDEX idx_numbers_gin ON test_gin_array_basic USING GIN (numbers);
CREATE INDEX idx_features_gin ON test_gin_array_basic USING GIN (features);

INSERT INTO test_gin_array_basic VALUES
    (1, ARRAY['postgresql', 'database', 'sql'], ARRAY[1, 2, 3, 4], ARRAY['feature1', 'feature2']),
    (2, ARRAY['python', 'programming', 'scripting'], ARRAY[5, 6, 7], ARRAY['feature2', 'feature3']),
    (3, ARRAY['database', 'nosql', 'mongodb'], ARRAY[2, 4, 6, 8], ARRAY['feature1', 'feature3']),
    (4, ARRAY['sql', 'query', 'database'], ARRAY[10, 20, 30], ARRAY['feature4']);

-- Array containment: @> (contains)
SELECT id, tags FROM test_gin_array_basic WHERE tags @> ARRAY['database'];
SELECT id FROM test_gin_array_basic WHERE tags @> ARRAY['database', 'sql'];

-- Array overlap: && (overlaps)
SELECT id FROM test_gin_array_basic WHERE tags && ARRAY['postgresql', 'python'];

-- Array is contained: <@ (is contained by)
SELECT id FROM test_gin_array_basic WHERE ARRAY['sql'] <@ tags;

-- ============================================================================
-- Section 2: GIN on JSONB - Document Containment
-- ============================================================================

CREATE TABLE test_gin_jsonb_basic (
    id INT PRIMARY KEY,
    metadata JSONB,
    config JSONB
);

CREATE INDEX idx_metadata_gin ON test_gin_jsonb_basic USING GIN (metadata);
CREATE INDEX idx_config_gin ON test_gin_jsonb_basic USING GIN (config);

INSERT INTO test_gin_jsonb_basic VALUES
    (1, '{"name": "Alice", "age": 30, "city": "NYC", "tags": ["developer", "postgresql"]}'::JSONB,
        '{"theme": "dark", "notifications": true}'::JSONB),
    (2, '{"name": "Bob", "age": 25, "city": "LA", "tags": ["designer", "ui"]}'::JSONB,
        '{"theme": "light", "notifications": false}'::JSONB),
    (3, '{"name": "Charlie", "age": 35, "city": "NYC", "tags": ["developer", "python"]}'::JSONB,
        '{"theme": "dark", "notifications": true, "beta": true}'::JSONB);

-- JSONB containment: @> (contains)
SELECT id, metadata->>'name' AS name FROM test_gin_jsonb_basic
WHERE metadata @> '{"city": "NYC"}'::JSONB;

SELECT id FROM test_gin_jsonb_basic
WHERE metadata @> '{"tags": ["developer"]}'::JSONB;

-- Key existence: ? (has key)
SELECT id FROM test_gin_jsonb_basic WHERE config ? 'beta';

-- Any key exists: ?| (has any key)
SELECT id FROM test_gin_jsonb_basic WHERE metadata ?| ARRAY['age', 'salary'];

-- All keys exist: ?& (has all keys)
SELECT id FROM test_gin_jsonb_basic WHERE metadata ?& ARRAY['name', 'age', 'city'];

-- ============================================================================
-- Section 3: GIN on TSVECTOR - Full-Text Search
-- ============================================================================

CREATE TABLE test_gin_fulltext (
    doc_id INT PRIMARY KEY,
    title TEXT,
    content TEXT,
    search_vector TSVECTOR
);

CREATE INDEX idx_search_vector_gin ON test_gin_fulltext USING GIN (search_vector);

INSERT INTO test_gin_fulltext VALUES
    (1, 'PostgreSQL Tutorial',
        'Learn PostgreSQL database management and SQL queries',
        to_tsvector('english', 'PostgreSQL Tutorial Learn PostgreSQL database management and SQL queries')),
    (2, 'Python Programming',
        'Introduction to Python programming language and frameworks',
        to_tsvector('english', 'Python Programming Introduction to Python programming language and frameworks')),
    (3, 'Database Design',
        'Best practices for relational database design and normalization',
        to_tsvector('english', 'Database Design Best practices for relational database design and normalization')),
    (4, 'Advanced SQL',
        'Advanced SQL techniques including window functions and CTEs',
        to_tsvector('english', 'Advanced SQL Advanced SQL techniques including window functions and CTEs'));

-- Full-text search with GIN index
SELECT doc_id, title FROM test_gin_fulltext
WHERE search_vector @@ to_tsquery('english', 'postgresql');

SELECT doc_id, title FROM test_gin_fulltext
WHERE search_vector @@ to_tsquery('english', 'database & design');

SELECT doc_id, title FROM test_gin_fulltext
WHERE search_vector @@ to_tsquery('english', 'SQL | python');

-- ============================================================================
-- Section 4: GIN vs GiST for Full-Text Search
-- ============================================================================

CREATE TABLE test_gin_vs_gist_fulltext (
    id INT PRIMARY KEY,
    document TEXT,
    search_vector TSVECTOR
);

-- Create both GIN and GiST indexes
CREATE INDEX idx_gin_fulltext ON test_gin_vs_gist_fulltext USING GIN (search_vector);
CREATE INDEX idx_gist_fulltext ON test_gin_vs_gist_fulltext USING GIST (search_vector);

INSERT INTO test_gin_vs_gist_fulltext
SELECT i,
       'Document ' || i || ' contains various keywords about databases and programming',
       to_tsvector('english', 'Document ' || i || ' contains various keywords about databases and programming')
FROM generate_series(1, 1000) i;

-- Query using indexes (optimizer chooses based on statistics)
SELECT id FROM test_gin_vs_gist_fulltext
WHERE search_vector @@ to_tsquery('english', 'database & programming')
LIMIT 10;

-- GIN: faster search, larger size, slower updates
-- GiST: slower search, smaller size, faster updates

-- ============================================================================
-- Section 5: GIN on ARRAY - Tagging System
-- ============================================================================

CREATE TABLE test_gin_tagging (
    post_id INT PRIMARY KEY,
    title VARCHAR(200),
    tags TEXT[],
    categories TEXT[]
);

CREATE INDEX idx_tags_tagging ON test_gin_tagging USING GIN (tags);
CREATE INDEX idx_categories_gin ON test_gin_tagging USING GIN (categories);

INSERT INTO test_gin_tagging VALUES
    (1, 'Introduction to PostgreSQL', ARRAY['database', 'postgresql', 'tutorial'], ARRAY['education', 'technical']),
    (2, 'Python Best Practices', ARRAY['python', 'programming', 'best-practices'], ARRAY['education', 'code-quality']),
    (3, 'Database Performance Tuning', ARRAY['database', 'performance', 'optimization'], ARRAY['technical', 'advanced']),
    (4, 'Web Development with Django', ARRAY['python', 'django', 'web'], ARRAY['web-dev', 'frameworks']),
    (5, 'SQL Query Optimization', ARRAY['sql', 'database', 'optimization', 'performance'], ARRAY['technical', 'database']);

-- Find posts with specific tag
SELECT post_id, title FROM test_gin_tagging WHERE tags @> ARRAY['database'];

-- Find posts with multiple tags (AND)
SELECT post_id, title FROM test_gin_tagging WHERE tags @> ARRAY['database', 'optimization'];

-- Find posts with any of multiple tags (OR)
SELECT post_id, title FROM test_gin_tagging WHERE tags && ARRAY['python', 'django'];

-- Combine tag and category filters
SELECT post_id, title FROM test_gin_tagging
WHERE tags @> ARRAY['database'] AND categories @> ARRAY['technical'];

-- ============================================================================
-- Section 6: GIN on JSONB - User Preferences
-- ============================================================================

CREATE TABLE test_gin_user_prefs (
    user_id INT PRIMARY KEY,
    username VARCHAR(100),
    preferences JSONB
);

CREATE INDEX idx_preferences_gin ON test_gin_user_prefs USING GIN (preferences);

INSERT INTO test_gin_user_prefs VALUES
    (1, 'alice', '{"theme": "dark", "language": "en", "notifications": {"email": true, "push": false}, "features": ["beta", "premium"]}'::JSONB),
    (2, 'bob', '{"theme": "light", "language": "es", "notifications": {"email": false, "push": true}, "features": ["premium"]}'::JSONB),
    (3, 'charlie', '{"theme": "dark", "language": "en", "notifications": {"email": true, "push": true}, "features": ["beta"]}'::JSONB),
    (4, 'diana', '{"theme": "auto", "language": "fr", "notifications": {"email": false, "push": false}, "features": []}'::JSONB);

-- Find users with dark theme
SELECT user_id, username FROM test_gin_user_prefs
WHERE preferences @> '{"theme": "dark"}'::JSONB;

-- Find users with email notifications enabled
SELECT user_id, username FROM test_gin_user_prefs
WHERE preferences @> '{"notifications": {"email": true}}'::JSONB;

-- Find users with beta features
SELECT user_id, username FROM test_gin_user_prefs
WHERE preferences @> '{"features": ["beta"]}'::JSONB;

-- Find users with notifications key
SELECT user_id, username FROM test_gin_user_prefs
WHERE preferences ? 'notifications';

-- ============================================================================
-- Section 7: GIN on JSONB - Product Attributes
-- ============================================================================

CREATE TABLE test_gin_products (
    product_id INT PRIMARY KEY,
    product_name VARCHAR(200),
    attributes JSONB
);

CREATE INDEX idx_attributes_gin ON test_gin_products USING GIN (attributes);

INSERT INTO test_gin_products VALUES
    (1, 'Laptop Pro', '{"brand": "TechCorp", "cpu": "Intel i7", "ram": 16, "storage": 512, "specs": {"wifi": "WiFi 6", "bluetooth": "5.0"}}'::JSONB),
    (2, 'Laptop Basic', '{"brand": "TechCorp", "cpu": "Intel i5", "ram": 8, "storage": 256, "specs": {"wifi": "WiFi 5", "bluetooth": "4.2"}}'::JSONB),
    (3, 'Desktop Workstation', '{"brand": "PowerPC", "cpu": "AMD Ryzen 9", "ram": 32, "storage": 1024, "specs": {"wifi": "WiFi 6E"}}'::JSONB),
    (4, 'Tablet Ultra', '{"brand": "TabletCo", "cpu": "ARM A15", "ram": 8, "storage": 128, "specs": {"wifi": "WiFi 6", "5g": true}}'::JSONB);

-- Find products by brand
SELECT product_id, product_name FROM test_gin_products
WHERE attributes @> '{"brand": "TechCorp"}'::JSONB;

-- Find products with WiFi 6
SELECT product_id, product_name FROM test_gin_products
WHERE attributes @> '{"specs": {"wifi": "WiFi 6"}}'::JSONB;

-- Find products with 5G capability
SELECT product_id, product_name FROM test_gin_products
WHERE attributes ? '5g';

-- Complex query: Intel CPU AND >= 16GB RAM (Note: numeric comparison requires path operations)
SELECT product_id, product_name FROM test_gin_products
WHERE attributes @> '{"brand": "TechCorp"}'::JSONB
  AND (attributes->>'ram')::INT >= 16;

-- ============================================================================
-- Section 8: GIN on ARRAY - Multi-Language Support
-- ============================================================================

CREATE TABLE test_gin_multilang (
    content_id INT PRIMARY KEY,
    title VARCHAR(200),
    available_languages TEXT[],
    supported_regions TEXT[]
);

CREATE INDEX idx_languages_gin ON test_gin_multilang USING GIN (available_languages);
CREATE INDEX idx_regions_gin ON test_gin_multilang USING GIN (supported_regions);

INSERT INTO test_gin_multilang VALUES
    (1, 'Product Manual', ARRAY['en', 'es', 'fr', 'de'], ARRAY['US', 'EU', 'LATAM']),
    (2, 'Software License', ARRAY['en', 'ja', 'zh'], ARRAY['US', 'APAC']),
    (3, 'User Guide', ARRAY['en', 'es'], ARRAY['US', 'LATAM']),
    (4, 'Technical Specs', ARRAY['en', 'de', 'fr', 'it'], ARRAY['EU']);

-- Find content available in Spanish
SELECT content_id, title FROM test_gin_multilang
WHERE available_languages @> ARRAY['es'];

-- Find content available in BOTH English and French
SELECT content_id, title FROM test_gin_multilang
WHERE available_languages @> ARRAY['en', 'fr'];

-- Find content for EU region
SELECT content_id, title FROM test_gin_multilang
WHERE supported_regions @> ARRAY['EU'];

-- Find content available in ANY of these languages
SELECT content_id, title FROM test_gin_multilang
WHERE available_languages && ARRAY['ja', 'zh', 'ko'];

-- ============================================================================
-- Section 9: GIN on ARRAY - Permission System
-- ============================================================================

CREATE TABLE test_gin_permissions (
    role_id INT PRIMARY KEY,
    role_name VARCHAR(100),
    permissions TEXT[]
);

CREATE INDEX idx_permissions_gin ON test_gin_permissions USING GIN (permissions);

INSERT INTO test_gin_permissions VALUES
    (1, 'Admin', ARRAY['read', 'write', 'delete', 'manage_users', 'manage_roles']),
    (2, 'Editor', ARRAY['read', 'write', 'delete']),
    (3, 'Author', ARRAY['read', 'write']),
    (4, 'Viewer', ARRAY['read']);

-- Find roles with write permission
SELECT role_id, role_name FROM test_gin_permissions
WHERE permissions @> ARRAY['write'];

-- Find roles with BOTH write and delete permissions
SELECT role_id, role_name FROM test_gin_permissions
WHERE permissions @> ARRAY['write', 'delete'];

-- Find roles with user management capability
SELECT role_id, role_name FROM test_gin_permissions
WHERE permissions && ARRAY['manage_users', 'manage_roles'];

-- Check if a specific set of permissions is fully contained
SELECT role_id, role_name FROM test_gin_permissions
WHERE permissions @> ARRAY['read', 'write', 'delete', 'manage_users'];

-- ============================================================================
-- Section 10: GIN Index Performance
-- ============================================================================

CREATE TABLE test_gin_performance (
    id INT PRIMARY KEY,
    tags TEXT[],
    metadata JSONB,
    search_data TSVECTOR
);

CREATE INDEX idx_tags_perf ON test_gin_performance USING GIN (tags);
CREATE INDEX idx_metadata_perf ON test_gin_performance USING GIN (metadata);
CREATE INDEX idx_search_perf ON test_gin_performance USING GIN (search_data);

-- Insert test data
INSERT INTO test_gin_performance
SELECT i,
       ARRAY['tag' || (i % 10), 'tag' || (i % 20), 'tag' || (i % 30)],
       ('{"category": "cat' || (i % 5) || '", "priority": ' || (i % 3) || '}')::JSONB,
       to_tsvector('english', 'Document ' || i || ' with searchable content about topic ' || (i % 50))
FROM generate_series(1, 10000) i;

-- Fast array containment query
SELECT id FROM test_gin_performance WHERE tags @> ARRAY['tag5'] LIMIT 10;

-- Fast JSONB query
SELECT id FROM test_gin_performance WHERE metadata @> '{"category": "cat2"}'::JSONB LIMIT 10;

-- Fast full-text search
SELECT id FROM test_gin_performance
WHERE search_data @@ to_tsquery('english', 'document & searchable')
LIMIT 10;

-- Check index sizes
SELECT pg_size_pretty(pg_relation_size('idx_tags_perf')) AS tags_index_size,
       pg_size_pretty(pg_relation_size('idx_metadata_perf')) AS metadata_index_size,
       pg_size_pretty(pg_relation_size('idx_search_perf')) AS search_index_size;

-- ============================================================================
-- Section 11: GIN with jsonb_path_ops Operator Class
-- ============================================================================

CREATE TABLE test_gin_jsonb_path_ops (
    id INT PRIMARY KEY,
    data JSONB
);

-- Default operator class (supports ?, ?&, ?|, @>, @@)
CREATE INDEX idx_data_gin_default ON test_gin_jsonb_path_ops USING GIN (data);

-- Path ops operator class (only supports @>, @@, but smaller and faster)
CREATE INDEX idx_data_gin_path_ops ON test_gin_jsonb_path_ops USING GIN (data jsonb_path_ops);

INSERT INTO test_gin_jsonb_path_ops VALUES
    (1, '{"user": {"name": "Alice", "age": 30}, "active": true}'::JSONB),
    (2, '{"user": {"name": "Bob", "age": 25}, "active": false}'::JSONB),
    (3, '{"user": {"name": "Charlie", "age": 35}, "active": true}'::JSONB);

-- Containment query (works with both indexes)
SELECT id FROM test_gin_jsonb_path_ops WHERE data @> '{"active": true}'::JSONB;

-- Nested containment (works with both)
SELECT id FROM test_gin_jsonb_path_ops WHERE data @> '{"user": {"name": "Alice"}}'::JSONB;

-- Key existence (only works with default GIN, not jsonb_path_ops)
-- This would use idx_data_gin_default, not idx_data_gin_path_ops
SELECT id FROM test_gin_jsonb_path_ops WHERE data ? 'active';

-- ============================================================================
-- Section 12: GIN Fast Update
-- ============================================================================

CREATE TABLE test_gin_fastupdate (
    id SERIAL PRIMARY KEY,
    tags TEXT[],
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- GIN with fast update enabled (default since PostgreSQL 9.4)
CREATE INDEX idx_tags_fastupdate ON test_gin_fastupdate USING GIN (tags) WITH (fastupdate = on);

-- Fast update trades search speed for faster inserts by buffering updates
INSERT INTO test_gin_fastupdate (tags)
SELECT ARRAY['tag' || (i % 100), 'tag' || (i % 200)]
FROM generate_series(1, 1000) i;

-- Query still works efficiently
SELECT id FROM test_gin_fastupdate WHERE tags @> ARRAY['tag50'] LIMIT 5;

-- ============================================================================
-- Section 13: GIN Index Maintenance
-- ============================================================================

CREATE TABLE test_gin_maintenance (
    id INT PRIMARY KEY,
    keywords TEXT[],
    properties JSONB
);

CREATE INDEX idx_keywords_maint ON test_gin_maintenance USING GIN (keywords);
CREATE INDEX idx_properties_maint ON test_gin_maintenance USING GIN (properties);

INSERT INTO test_gin_maintenance
SELECT i,
       ARRAY['keyword' || i, 'keyword' || (i % 10)],
       ('{"type": "type' || (i % 5) || '", "value": ' || i || '}')::JSONB
FROM generate_series(1, 1000) i;

-- Reindex GIN index
REINDEX INDEX idx_keywords_maint;

-- Vacuum to clean up pending list
VACUUM test_gin_maintenance;

-- Get pending list size (bytes in pending list)
SELECT pg_size_pretty(pg_relation_size('idx_keywords_maint')) AS index_size;

-- ============================================================================
-- Section 14: GIN on ARRAY - Real-World E-commerce
-- ============================================================================

CREATE TABLE test_gin_ecommerce (
    product_id INT PRIMARY KEY,
    sku VARCHAR(50),
    name VARCHAR(200),
    categories TEXT[],
    tags TEXT[],
    compatible_with TEXT[]
);

CREATE INDEX idx_categories_ecom ON test_gin_ecommerce USING GIN (categories);
CREATE INDEX idx_tags_ecom ON test_gin_ecommerce USING GIN (tags);
CREATE INDEX idx_compatible_ecom ON test_gin_ecommerce USING GIN (compatible_with);

INSERT INTO test_gin_ecommerce VALUES
    (1, 'LAPTOP-001', 'Business Laptop', ARRAY['electronics', 'computers', 'laptops'], ARRAY['professional', 'productivity'], ARRAY['acc-001', 'acc-005']),
    (2, 'MOUSE-001', 'Wireless Mouse', ARRAY['electronics', 'accessories', 'peripherals'], ARRAY['wireless', 'ergonomic'], ARRAY['LAPTOP-001']),
    (3, 'MONITOR-001', '4K Monitor', ARRAY['electronics', 'monitors', 'displays'], ARRAY['4k', 'gaming'], ARRAY['LAPTOP-001']),
    (4, 'KEYBOARD-001', 'Mechanical Keyboard', ARRAY['electronics', 'accessories', 'peripherals'], ARRAY['mechanical', 'rgb'], ARRAY['LAPTOP-001']);

-- Browse by category
SELECT product_id, name FROM test_gin_ecommerce WHERE categories @> ARRAY['laptops'];

-- Find accessories
SELECT product_id, name FROM test_gin_ecommerce WHERE categories @> ARRAY['accessories'];

-- Find products with specific tags
SELECT product_id, name FROM test_gin_ecommerce WHERE tags && ARRAY['wireless', 'ergonomic'];

-- Find compatible accessories for a product
SELECT product_id, name FROM test_gin_ecommerce WHERE compatible_with @> ARRAY['LAPTOP-001'];

-- ============================================================================
-- Section 15: GIN Best Practices
-- ============================================================================

CREATE TABLE test_gin_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_gin_best_practices VALUES
    (1, 'Use GIN for ARRAY containment (@>, <@, &&)'),
    (2, 'Use GIN for JSONB containment (@>, ?, ?&, ?|)'),
    (3, 'Use GIN for full-text search (TSVECTOR)'),
    (4, 'GIN indexes are larger than B-tree but faster for composite searches'),
    (5, 'GIN searches are fast, updates are slower than B-tree'),
    (6, 'Use jsonb_path_ops for smaller index when only @> is needed'),
    (7, 'Default GIN supports all JSONB operators (?, ?&, ?|, @>)'),
    (8, 'GIN with fastupdate=on buffers updates for faster inserts'),
    (9, 'VACUUM regularly to flush GIN pending list'),
    (10, 'GIN is ideal for tagging systems and multi-valued attributes'),
    (11, 'For full-text: GIN is faster search, GiST is faster update'),
    (12, 'GIN build time is longer than B-tree'),
    (13, 'Monitor GIN index size - can be 2-3x table size for arrays'),
    (14, 'Use GIN for "contains" queries, B-tree for equality/range'),
    (15, 'Cannot use GIN for sorting or range scans');

SELECT id, guideline FROM test_gin_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_gin_best_practices;
DROP TABLE test_gin_ecommerce;
DROP TABLE test_gin_maintenance;
DROP TABLE test_gin_fastupdate;
DROP TABLE test_gin_jsonb_path_ops;
DROP TABLE test_gin_performance;
DROP TABLE test_gin_permissions;
DROP TABLE test_gin_multilang;
DROP TABLE test_gin_products;
DROP TABLE test_gin_user_prefs;
DROP TABLE test_gin_tagging;
DROP TABLE test_gin_vs_gist_fulltext;
DROP TABLE test_gin_fulltext;
DROP TABLE test_gin_jsonb_basic;
DROP TABLE test_gin_array_basic;

DROP DATABASE test_gin_basic_db;

-- End of GIN index tests
