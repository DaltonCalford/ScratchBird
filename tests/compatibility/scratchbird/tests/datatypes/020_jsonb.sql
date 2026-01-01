-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - JSONB
-- Description: Comprehensive JSONB (Binary JSON) data type testing
-- ============================================================================

-- JSONB is a binary storage format for JSON data optimized for processing
-- Key differences from JSON:
--   - Binary format (faster operations, slower insertion)
--   - No whitespace preservation
--   - No key order preservation
--   - Supports GIN indexing for advanced queries
--   - Better for frequent querying/filtering

-- Create test database
CREATE DATABASE test_jsonb_db;
USE test_jsonb_db;

-- ============================================================================
-- Section 1: Basic JSONB Storage and Normalization
-- ============================================================================

CREATE TABLE test_jsonb_basic (
    id INT PRIMARY KEY,
    jsonb_data JSONB,
    description VARCHAR(200)
);

-- JSONB normalizes data (removes whitespace, sorts keys)
INSERT INTO test_jsonb_basic VALUES (1, '{"name": "John", "age": 30}', 'Simple object');
INSERT INTO test_jsonb_basic VALUES (2, '{"age": 30, "name": "John"}', 'Same data, different key order');
INSERT INTO test_jsonb_basic VALUES (3, '  {  "name"  :  "Alice"  }  ', 'Extra whitespace');
INSERT INTO test_jsonb_basic VALUES (4, '[1, 2, 3, 4, 5]', 'Simple array');
INSERT INTO test_jsonb_basic VALUES (5, '{"nested": {"level2": {"level3": "value"}}}', 'Deeply nested');
INSERT INTO test_jsonb_basic VALUES (6, '{}', 'Empty object');
INSERT INTO test_jsonb_basic VALUES (7, '[]', 'Empty array');
INSERT INTO test_jsonb_basic VALUES (8, 'null', 'JSONB null');

SELECT id, jsonb_data, description FROM test_jsonb_basic ORDER BY id;

-- Verify normalization: rows 1 and 2 should be identical after normalization
SELECT jsonb_data FROM test_jsonb_basic WHERE id IN (1, 2) ORDER BY id;

-- ============================================================================
-- Section 2: JSONB Containment Operators (@>, <@)
-- ============================================================================

CREATE TABLE test_jsonb_containment (
    id INT PRIMARY KEY,
    config JSONB
);

INSERT INTO test_jsonb_containment VALUES
    (1, '{"features": ["auth", "logging"], "version": "1.0", "enabled": true}'),
    (2, '{"features": ["auth", "caching"], "version": "2.0", "enabled": false}'),
    (3, '{"features": ["logging", "monitoring"], "version": "1.5", "enabled": true}'),
    (4, '{"features": ["auth", "logging", "caching"], "version": "2.0", "enabled": true}');

-- @> operator: left JSON contains right JSON
SELECT id FROM test_jsonb_containment
WHERE config @> '{"enabled": true}' ORDER BY id;

SELECT id FROM test_jsonb_containment
WHERE config @> '{"version": "2.0"}' ORDER BY id;

SELECT id FROM test_jsonb_containment
WHERE config @> '{"enabled": true, "version": "2.0"}' ORDER BY id;

-- <@ operator: left JSON is contained by right JSON
SELECT id FROM test_jsonb_containment
WHERE '{"enabled": true}' <@ config ORDER BY id;

-- Array containment
SELECT id FROM test_jsonb_containment
WHERE config->'features' @> '["auth"]'::JSONB ORDER BY id;

SELECT id FROM test_jsonb_containment
WHERE config->'features' @> '["auth", "logging"]'::JSONB ORDER BY id;

-- ============================================================================
-- Section 3: JSONB Existence Operators (?, ?&, ?|)
-- ============================================================================

CREATE TABLE test_jsonb_existence (
    id INT PRIMARY KEY,
    metadata JSONB
);

INSERT INTO test_jsonb_existence VALUES
    (1, '{"type": "user", "role": "admin", "verified": true}'),
    (2, '{"type": "product", "category": "electronics", "stock": 100}'),
    (3, '{"type": "order", "status": "shipped", "total": 299.99}'),
    (4, '{"type": "user", "role": "member", "verified": false}');

-- ? operator: does key exist?
SELECT id FROM test_jsonb_existence WHERE metadata ? 'role' ORDER BY id;
SELECT id FROM test_jsonb_existence WHERE metadata ? 'verified' ORDER BY id;
SELECT id FROM test_jsonb_existence WHERE metadata ? 'stock' ORDER BY id;

-- ?& operator: do all keys exist?
SELECT id FROM test_jsonb_existence WHERE metadata ?& ARRAY['type', 'role'] ORDER BY id;
SELECT id FROM test_jsonb_existence WHERE metadata ?& ARRAY['type', 'role', 'verified'] ORDER BY id;

-- ?| operator: does any key exist?
SELECT id FROM test_jsonb_existence WHERE metadata ?| ARRAY['role', 'category'] ORDER BY id;
SELECT id FROM test_jsonb_existence WHERE metadata ?| ARRAY['stock', 'total'] ORDER BY id;

-- ============================================================================
-- Section 4: GIN Indexing for JSONB
-- ============================================================================

CREATE TABLE test_jsonb_gin_index (
    id INT PRIMARY KEY,
    data JSONB
);

-- GIN index for containment and existence queries
CREATE INDEX idx_jsonb_data_gin ON test_jsonb_gin_index USING GIN (data);

INSERT INTO test_jsonb_gin_index VALUES
    (1, '{"user_id": 123, "action": "login", "timestamp": "2024-01-15T10:30:00Z"}'),
    (2, '{"user_id": 456, "action": "logout", "timestamp": "2024-01-15T11:45:00Z"}'),
    (3, '{"user_id": 123, "action": "purchase", "amount": 99.99, "timestamp": "2024-01-15T14:20:00Z"}'),
    (4, '{"user_id": 789, "action": "login", "timestamp": "2024-01-15T15:00:00Z"}'),
    (5, '{"user_id": 123, "action": "update_profile", "timestamp": "2024-01-15T16:30:00Z"}');

-- Queries using GIN index (fast containment checks)
SELECT id, data FROM test_jsonb_gin_index
WHERE data @> '{"user_id": 123}' ORDER BY id;

SELECT id, data FROM test_jsonb_gin_index
WHERE data @> '{"action": "login"}' ORDER BY id;

SELECT id, data FROM test_jsonb_gin_index
WHERE data ? 'amount' ORDER BY id;

-- ============================================================================
-- Section 5: JSONB Path Expression Index
-- ============================================================================

CREATE TABLE test_jsonb_path_index (
    id INT PRIMARY KEY,
    user_data JSONB
);

-- Index on specific JSONB path for faster extraction
CREATE INDEX idx_user_email ON test_jsonb_path_index ((user_data->>'email'));
CREATE INDEX idx_user_age ON test_jsonb_path_index (((user_data->>'age')::INT));

INSERT INTO test_jsonb_path_index VALUES
    (1, '{"name": "Alice", "email": "alice@example.com", "age": 28}'),
    (2, '{"name": "Bob", "email": "bob@example.com", "age": 35}'),
    (3, '{"name": "Charlie", "email": "charlie@example.com", "age": 22}'),
    (4, '{"name": "Diana", "email": "diana@example.com", "age": 30}');

-- Queries using path indexes
SELECT id, user_data->>'name' AS name FROM test_jsonb_path_index
WHERE user_data->>'email' = 'alice@example.com';

SELECT id, user_data->>'name' AS name FROM test_jsonb_path_index
WHERE (user_data->>'age')::INT > 25 ORDER BY id;

-- ============================================================================
-- Section 6: JSONB Modification Functions
-- ============================================================================

CREATE TABLE test_jsonb_modification (
    id INT PRIMARY KEY,
    data JSONB
);

INSERT INTO test_jsonb_modification VALUES
    (1, '{"name": "Alice", "age": 28, "city": "NYC"}'),
    (2, '{"name": "Bob", "role": "admin"}');

-- jsonb_set: set value at path
SELECT id, jsonb_set(data, '{age}', '30') AS updated_age FROM test_jsonb_modification WHERE id = 1;
SELECT id, jsonb_set(data, '{email}', '"alice@example.com"') AS with_email FROM test_jsonb_modification WHERE id = 1;

-- Nested path update
INSERT INTO test_jsonb_modification VALUES
    (3, '{"user": {"profile": {"name": "Charlie"}}}');
SELECT id, jsonb_set(data, '{user,profile,age}', '25') AS nested_update FROM test_jsonb_modification WHERE id = 3;

-- jsonb_insert: insert value at path
SELECT id, jsonb_insert(data, '{country}', '"USA"') AS with_country FROM test_jsonb_modification WHERE id = 1;

-- jsonb_delete_path: remove key
SELECT id, data - 'age' AS without_age FROM test_jsonb_modification WHERE id = 1;
SELECT id, data - 'city' AS without_city FROM test_jsonb_modification WHERE id = 1;

-- Remove multiple keys
SELECT id, data - '{age,city}' AS without_age_city FROM test_jsonb_modification WHERE id = 1;

-- ============================================================================
-- Section 7: JSONB Array Operations
-- ============================================================================

CREATE TABLE test_jsonb_arrays (
    id INT PRIMARY KEY,
    items JSONB
);

INSERT INTO test_jsonb_arrays VALUES
    (1, '[10, 20, 30]'),
    (2, '["apple", "banana", "cherry"]'),
    (3, '[{"id": 1, "name": "A"}, {"id": 2, "name": "B"}]');

-- Array element access by index
SELECT id, items->0 AS first_elem FROM test_jsonb_arrays ORDER BY id;
SELECT id, items->1 AS second_elem FROM test_jsonb_arrays ORDER BY id;
SELECT id, items->-1 AS last_elem FROM test_jsonb_arrays ORDER BY id;

-- Array length
SELECT id, jsonb_array_length(items) AS array_len FROM test_jsonb_arrays ORDER BY id;

-- Array containment
SELECT id FROM test_jsonb_arrays WHERE items @> '[10]'::JSONB ORDER BY id;
SELECT id FROM test_jsonb_arrays WHERE items @> '["apple"]'::JSONB ORDER BY id;

-- Append to array (concatenation)
SELECT id, items || '999'::JSONB AS with_appended FROM test_jsonb_arrays WHERE id = 1;
SELECT id, items || '["date"]'::JSONB AS with_appended FROM test_jsonb_arrays WHERE id = 2;

-- ============================================================================
-- Section 8: JSONB Concatenation Operator (||)
-- ============================================================================

CREATE TABLE test_jsonb_concatenation (
    id INT PRIMARY KEY,
    obj1 JSONB,
    obj2 JSONB
);

INSERT INTO test_jsonb_concatenation VALUES
    (1, '{"a": 1, "b": 2}', '{"c": 3, "d": 4}'),
    (2, '{"name": "Alice"}', '{"age": 30}'),
    (3, '{"x": 1}', '{"x": 2}');  -- Overlapping key

-- Concatenate objects
SELECT id, obj1 || obj2 AS concatenated FROM test_jsonb_concatenation ORDER BY id;

-- Note: for overlapping keys, right side wins
SELECT obj1, obj2, obj1 || obj2 AS merged FROM test_jsonb_concatenation WHERE id = 3;

-- ============================================================================
-- Section 9: JSONB to Record/Recordset
-- ============================================================================

CREATE TABLE test_jsonb_to_record (
    id INT PRIMARY KEY,
    user_info JSONB
);

INSERT INTO test_jsonb_to_record VALUES
    (1, '{"name": "Alice", "age": 28, "email": "alice@example.com"}'),
    (2, '{"name": "Bob", "age": 35, "email": "bob@example.com"}');

-- jsonb_each: expand to key-value pairs
SELECT id, key, value FROM test_jsonb_to_record, jsonb_each(user_info) ORDER BY id, key;

-- jsonb_each_text: expand to key-value pairs (text values)
SELECT id, key, value FROM test_jsonb_to_record, jsonb_each_text(user_info) ORDER BY id, key;

-- jsonb_object_keys: get all keys
SELECT id, jsonb_object_keys(user_info) AS keys FROM test_jsonb_to_record ORDER BY id;

-- ============================================================================
-- Section 10: JSONB Build Functions
-- ============================================================================

-- jsonb_build_object
SELECT jsonb_build_object('name', 'Alice', 'age', 30) AS user_obj;
SELECT jsonb_build_object('id', 1, 'active', true, 'score', 95.5) AS record_obj;

-- jsonb_build_array
SELECT jsonb_build_array(1, 2, 3, 4, 5) AS num_array;
SELECT jsonb_build_array('red', 'green', 'blue') AS color_array;

-- Nested construction
SELECT jsonb_build_object(
    'user', jsonb_build_object('name', 'Alice', 'age', 30),
    'tags', jsonb_build_array('admin', 'verified')
) AS complex_obj;

-- ============================================================================
-- Section 11: JSONB Aggregation
-- ============================================================================

CREATE TABLE test_jsonb_aggregation (
    id INT,
    category VARCHAR(50),
    item JSONB
);

INSERT INTO test_jsonb_aggregation VALUES
    (1, 'fruits', '{"name": "apple", "color": "red"}'),
    (2, 'fruits', '{"name": "banana", "color": "yellow"}'),
    (3, 'vegetables', '{"name": "carrot", "color": "orange"}'),
    (4, 'vegetables', '{"name": "lettuce", "color": "green"}');

-- jsonb_agg: aggregate into JSONB array
SELECT category, jsonb_agg(item) AS items
FROM test_jsonb_aggregation
GROUP BY category ORDER BY category;

-- jsonb_object_agg: aggregate into JSONB object
SELECT jsonb_object_agg(item->>'name', item->>'color') AS name_color_map
FROM test_jsonb_aggregation
WHERE category = 'fruits';

-- ============================================================================
-- Section 12: JSONB Comparison Operators
-- ============================================================================

CREATE TABLE test_jsonb_comparison (
    id INT PRIMARY KEY,
    data JSONB
);

INSERT INTO test_jsonb_comparison VALUES
    (1, '{"priority": 1}'),
    (2, '{"priority": 2}'),
    (3, '{"priority": 1}');

-- Equality
SELECT id FROM test_jsonb_comparison WHERE data = '{"priority": 1}' ORDER BY id;

-- Inequality
SELECT id FROM test_jsonb_comparison WHERE data != '{"priority": 1}' ORDER BY id;

-- Containment as comparison
SELECT id FROM test_jsonb_comparison WHERE data @> '{"priority": 2}' ORDER BY id;

-- ============================================================================
-- Section 13: JSONB Path Queries (Advanced)
-- ============================================================================

CREATE TABLE test_jsonb_path_queries (
    id INT PRIMARY KEY,
    document JSONB
);

INSERT INTO test_jsonb_path_queries VALUES
    (1, '{"user": {"name": "Alice", "contacts": [{"type": "email", "value": "alice@example.com"}, {"type": "phone", "value": "555-1234"}]}}'),
    (2, '{"user": {"name": "Bob", "contacts": [{"type": "email", "value": "bob@example.com"}]}}');

-- Extract nested array elements
SELECT id,
       document->'user'->>'name' AS name,
       document->'user'->'contacts'->0->>'value' AS first_contact
FROM test_jsonb_path_queries ORDER BY id;

-- jsonb_path_query: query using SQL/JSON path
SELECT id, jsonb_path_query(document, '$.user.name') AS user_name
FROM test_jsonb_path_queries ORDER BY id;

-- ============================================================================
-- Section 14: JSONB Performance - Insert vs Query Trade-off
-- ============================================================================

CREATE TABLE test_jsonb_performance (
    id INT PRIMARY KEY,
    data JSONB
);

-- JSONB insert is slower than JSON (requires binary conversion)
-- But queries are significantly faster

INSERT INTO test_jsonb_performance VALUES
    (1, '{"field1": "value1", "field2": "value2", "field3": "value3"}'),
    (2, '{"field1": "value4", "field2": "value5", "field3": "value6"}'),
    (3, '{"field1": "value7", "field2": "value8", "field3": "value9"}');

-- Fast queries due to binary format
SELECT id FROM test_jsonb_performance WHERE data @> '{"field1": "value1"}';
SELECT id FROM test_jsonb_performance WHERE data ? 'field2';

-- Use JSONB when:
-- - Frequent querying/filtering on JSON data
-- - Need GIN indexing for containment queries
-- - Need efficient extraction operations
--
-- Use JSON when:
-- - Mostly storing and retrieving without processing
-- - Need to preserve exact formatting
-- - Insert performance is critical

-- ============================================================================
-- Section 15: Real-world Use Case - User Permissions
-- ============================================================================

CREATE TABLE test_jsonb_permissions (
    id INT PRIMARY KEY,
    user_id INT,
    permissions JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_permissions_gin ON test_jsonb_permissions USING GIN (permissions);

INSERT INTO test_jsonb_permissions (id, user_id, permissions) VALUES
    (1, 101, '{"resources": {"users": ["read", "write"], "reports": ["read"]}}'),
    (2, 102, '{"resources": {"users": ["read"], "reports": ["read", "write", "delete"]}}'),
    (3, 103, '{"resources": {"users": ["read", "write", "delete"], "reports": ["read"]}}');

-- Check if user has specific permission
SELECT id, user_id FROM test_jsonb_permissions
WHERE permissions @> '{"resources": {"users": ["write"]}}' ORDER BY id;

SELECT id, user_id FROM test_jsonb_permissions
WHERE permissions->'resources'->'reports' @> '["delete"]'::JSONB ORDER BY id;

-- List all users who can read users
SELECT id, user_id FROM test_jsonb_permissions
WHERE permissions->'resources'->'users' @> '["read"]'::JSONB ORDER BY id;

-- ============================================================================
-- Section 16: Real-world Use Case - Product Catalog
-- ============================================================================

CREATE TABLE test_jsonb_product_catalog (
    id INT PRIMARY KEY,
    product_name VARCHAR(200),
    attributes JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_product_attributes ON test_jsonb_product_catalog USING GIN (attributes);

INSERT INTO test_jsonb_product_catalog (id, product_name, attributes) VALUES
    (1, 'Laptop Pro 15', '{"brand": "TechCorp", "specs": {"cpu": "Intel i7", "ram": "16GB", "storage": "512GB SSD"}, "price": 1299.99, "in_stock": true}'),
    (2, 'Wireless Mouse', '{"brand": "Peripherals Inc", "specs": {"dpi": 1600, "wireless": true, "battery": "AA"}, "price": 29.99, "in_stock": true}'),
    (3, 'USB-C Hub', '{"brand": "ConnectCo", "specs": {"ports": 7, "power_delivery": "100W"}, "price": 49.99, "in_stock": false}');

-- Find products by brand
SELECT id, product_name FROM test_jsonb_product_catalog
WHERE attributes @> '{"brand": "TechCorp"}' ORDER BY id;

-- Find in-stock products
SELECT id, product_name FROM test_jsonb_product_catalog
WHERE attributes @> '{"in_stock": true}' ORDER BY id;

-- Find wireless products
SELECT id, product_name FROM test_jsonb_product_catalog
WHERE attributes->'specs' @> '{"wireless": true}' ORDER BY id;

-- Extract price for comparison
SELECT id, product_name, (attributes->>'price')::DECIMAL(10,2) AS price
FROM test_jsonb_product_catalog
WHERE (attributes->>'price')::DECIMAL(10,2) < 100 ORDER BY price;

-- ============================================================================
-- Section 17: Real-world Use Case - Analytics Events
-- ============================================================================

CREATE TABLE test_jsonb_analytics (
    id INT PRIMARY KEY,
    event_name VARCHAR(100),
    properties JSONB,
    user_id INT,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_analytics_properties ON test_jsonb_analytics USING GIN (properties);

INSERT INTO test_jsonb_analytics (id, event_name, properties, user_id) VALUES
    (1, 'page_view', '{"page": "/home", "referrer": "google.com", "device": "mobile"}', 123),
    (2, 'button_click', '{"button_id": "signup", "page": "/home", "device": "desktop"}', 123),
    (3, 'page_view', '{"page": "/products", "referrer": "direct", "device": "desktop"}', 456),
    (4, 'purchase', '{"product_id": 789, "amount": 99.99, "currency": "USD"}', 123),
    (5, 'page_view', '{"page": "/home", "referrer": "facebook.com", "device": "mobile"}', 789);

-- Find all mobile page views
SELECT id, event_name, properties FROM test_jsonb_analytics
WHERE event_name = 'page_view' AND properties @> '{"device": "mobile"}' ORDER BY id;

-- Find events with specific property
SELECT id, event_name FROM test_jsonb_analytics
WHERE properties ? 'product_id' ORDER BY id;

-- Aggregate by device type
SELECT properties->>'device' AS device, COUNT(*) AS count
FROM test_jsonb_analytics
WHERE event_name = 'page_view'
GROUP BY properties->>'device'
ORDER BY count DESC;

-- ============================================================================
-- Section 18: NULL Handling
-- ============================================================================

CREATE TABLE test_jsonb_nulls (
    id INT PRIMARY KEY,
    data JSONB
);

INSERT INTO test_jsonb_nulls VALUES
    (1, '{"key": "value"}'),
    (2, '{"key": null}'),  -- JSON null value
    (3, NULL),  -- SQL NULL
    (4, '{}');

SELECT id, data, data IS NULL AS is_sql_null FROM test_jsonb_nulls ORDER BY id;

-- Distinguish JSON null from SQL NULL
SELECT id,
       data->>'key' AS extracted,
       data->>'key' IS NULL AS is_null,
       data ? 'key' AS key_exists
FROM test_jsonb_nulls ORDER BY id;

-- ============================================================================
-- Section 19: JSONB Size and Storage
-- ============================================================================

CREATE TABLE test_jsonb_size (
    id INT PRIMARY KEY,
    small_doc JSONB,
    large_doc JSONB
);

INSERT INTO test_jsonb_size VALUES
    (1, '{"key": "value"}', '{"users": [{"id": 1, "name": "User1", "details": {"age": 25, "city": "NYC"}}, {"id": 2, "name": "User2", "details": {"age": 30, "city": "LA"}}], "metadata": {"version": "1.0", "timestamp": "2024-01-15"}}');

-- Check storage size
SELECT id,
       pg_column_size(small_doc) AS small_size_bytes,
       pg_column_size(large_doc) AS large_size_bytes
FROM test_jsonb_size;

-- JSONB uses TOAST for large values (similar to TEXT)

-- ============================================================================
-- Section 20: JSONB Best Practices Summary
-- ============================================================================

CREATE TABLE test_jsonb_best_practices (
    id INT PRIMARY KEY,
    notes TEXT
);

INSERT INTO test_jsonb_best_practices VALUES
    (1, 'Use JSONB for frequent queries, JSON for storage-only'),
    (2, 'Create GIN indexes for containment queries (@>, ?, ?&, ?|)'),
    (3, 'Create expression indexes for frequently extracted paths'),
    (4, 'Use @> for containment checks instead of equality when possible'),
    (5, 'Consider denormalizing frequently-accessed fields to regular columns'),
    (6, 'JSONB is ideal for semi-structured data with varying schemas'),
    (7, 'Use jsonb_set for updates, not string concatenation'),
    (8, 'JSONB does not preserve key order or whitespace'),
    (9, 'Use containment operators with GIN index for best performance'),
    (10, 'JSONB works well for: user preferences, product attributes, analytics events, API responses');

SELECT id, notes FROM test_jsonb_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_jsonb_best_practices;
DROP TABLE test_jsonb_size;
DROP TABLE test_jsonb_nulls;
DROP TABLE test_jsonb_analytics;
DROP TABLE test_jsonb_product_catalog;
DROP TABLE test_jsonb_permissions;
DROP TABLE test_jsonb_performance;
DROP TABLE test_jsonb_path_queries;
DROP TABLE test_jsonb_comparison;
DROP TABLE test_jsonb_aggregation;
DROP TABLE test_jsonb_to_record;
DROP TABLE test_jsonb_concatenation;
DROP TABLE test_jsonb_arrays;
DROP TABLE test_jsonb_modification;
DROP TABLE test_jsonb_path_index;
DROP TABLE test_jsonb_gin_index;
DROP TABLE test_jsonb_existence;
DROP TABLE test_jsonb_containment;
DROP TABLE test_jsonb_basic;

DROP DATABASE test_jsonb_db;

-- End of JSONB data type tests
