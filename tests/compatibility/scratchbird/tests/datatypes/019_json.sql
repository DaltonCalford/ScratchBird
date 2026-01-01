-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - JSON
-- Description: Comprehensive JSON data type testing
-- ============================================================================

-- JSON is a flexible data type for storing semi-structured data in text format
-- Used for configuration storage, API responses, flexible schemas, and document storage

-- Create test database
CREATE DATABASE test_json_db;
USE test_json_db;

-- ============================================================================
-- Section 1: Basic JSON Creation and Storage
-- ============================================================================

CREATE TABLE test_json_basic (
    id INT PRIMARY KEY,
    json_data JSON,
    description VARCHAR(200)
);

-- Various JSON input formats
INSERT INTO test_json_basic VALUES (1, '{"name": "John", "age": 30}', 'Simple object');
INSERT INTO test_json_basic VALUES (2, '{"name": "Alice", "age": 25, "city": "NYC"}', 'Object with multiple fields');
INSERT INTO test_json_basic VALUES (3, '[1, 2, 3, 4, 5]', 'Simple array');
INSERT INTO test_json_basic VALUES (4, '[{"id": 1, "name": "A"}, {"id": 2, "name": "B"}]', 'Array of objects');
INSERT INTO test_json_basic VALUES (5, '{"nested": {"level2": {"level3": "deep"}}}', 'Nested object');
INSERT INTO test_json_basic VALUES (6, '{"string": "text", "number": 42, "bool": true, "null": null}', 'Mixed types');
INSERT INTO test_json_basic VALUES (7, '{"array": [1, 2, 3], "object": {"key": "value"}}', 'Mixed structures');
INSERT INTO test_json_basic VALUES (8, '{}', 'Empty object');
INSERT INTO test_json_basic VALUES (9, '[]', 'Empty array');
INSERT INTO test_json_basic VALUES (10, 'null', 'JSON null');

SELECT id, json_data, description FROM test_json_basic ORDER BY id;

-- ============================================================================
-- Section 2: JSON Extraction with -> and ->> Operators
-- ============================================================================

CREATE TABLE test_json_extraction (
    id INT PRIMARY KEY,
    user_data JSON
);

INSERT INTO test_json_extraction VALUES
    (1, '{"name": "John Doe", "email": "john@example.com", "age": 30}'),
    (2, '{"name": "Jane Smith", "email": "jane@example.com", "age": 25}'),
    (3, '{"name": "Bob Johnson", "email": "bob@example.com", "age": 35}');

-- Extract JSON object (returns JSON)
SELECT id, user_data->'name' AS name_json FROM test_json_extraction ORDER BY id;

-- Extract as text (returns TEXT)
SELECT id, user_data->>'name' AS name_text FROM test_json_extraction ORDER BY id;

-- Extract multiple fields
SELECT id,
       user_data->>'name' AS name,
       user_data->>'email' AS email,
       user_data->>'age' AS age
FROM test_json_extraction ORDER BY id;

-- ============================================================================
-- Section 3: Nested JSON Path Extraction
-- ============================================================================

CREATE TABLE test_json_paths (
    id INT PRIMARY KEY,
    data JSON
);

INSERT INTO test_json_paths VALUES
    (1, '{"user": {"profile": {"name": "Alice", "age": 28}}}'),
    (2, '{"user": {"profile": {"name": "Bob", "age": 32}}}'),
    (3, '{"company": {"employees": [{"name": "Charlie", "role": "Dev"}, {"name": "Diana", "role": "PM"}]}}');

-- Extract nested fields with chained operators
SELECT id, data->'user'->'profile'->>'name' AS nested_name FROM test_json_paths WHERE id <= 2 ORDER BY id;

-- Extract from arrays within JSON
SELECT id, data->'company'->'employees'->0->>'name' AS first_employee FROM test_json_paths WHERE id = 3;
SELECT id, data->'company'->'employees'->1->>'role' AS second_employee_role FROM test_json_paths WHERE id = 3;

-- ============================================================================
-- Section 4: JSON Functions - JSON_EXTRACT, JSON_TYPE, JSON_VALID
-- ============================================================================

CREATE TABLE test_json_functions (
    id INT PRIMARY KEY,
    json_value JSON,
    description VARCHAR(200)
);

INSERT INTO test_json_functions VALUES
    (1, '{"type": "user", "id": 123, "active": true}', 'User object'),
    (2, '[10, 20, 30, 40]', 'Number array'),
    (3, '"simple string"', 'JSON string'),
    (4, '42', 'JSON number'),
    (5, 'true', 'JSON boolean'),
    (6, 'null', 'JSON null');

-- JSON_TYPE function
SELECT id, JSON_TYPE(json_value) AS value_type, description FROM test_json_functions ORDER BY id;

-- JSON_VALID function
SELECT id, JSON_VALID(json_value) AS is_valid FROM test_json_functions ORDER BY id;
SELECT JSON_VALID('{"valid": "json"}') AS valid_json;
SELECT JSON_VALID('{invalid json}') AS invalid_json;
SELECT JSON_VALID('not json at all') AS not_json;

-- ============================================================================
-- Section 5: JSON Construction - JSON_OBJECT, JSON_ARRAY
-- ============================================================================

-- JSON_OBJECT construction
SELECT JSON_OBJECT('name', 'Alice', 'age', 30) AS constructed_object;
SELECT JSON_OBJECT('id', 1, 'type', 'user', 'active', true) AS user_object;
SELECT JSON_OBJECT('nested', JSON_OBJECT('key', 'value')) AS nested_construction;

-- JSON_ARRAY construction
SELECT JSON_ARRAY(1, 2, 3, 4, 5) AS number_array;
SELECT JSON_ARRAY('a', 'b', 'c') AS string_array;
SELECT JSON_ARRAY(JSON_OBJECT('id', 1), JSON_OBJECT('id', 2)) AS array_of_objects;

-- Construct from table data
CREATE TABLE test_json_construction (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(100),
    age INT
);

INSERT INTO test_json_construction VALUES
    (1, 'Alice Johnson', 'alice@example.com', 28),
    (2, 'Bob Smith', 'bob@example.com', 32),
    (3, 'Charlie Brown', 'charlie@example.com', 25);

SELECT id, JSON_OBJECT('name', name, 'email', email, 'age', age) AS user_json
FROM test_json_construction ORDER BY id;

-- ============================================================================
-- Section 6: JSON Modification - JSON_SET, JSON_INSERT, JSON_REPLACE, JSON_REMOVE
-- ============================================================================

CREATE TABLE test_json_modification (
    id INT PRIMARY KEY,
    data JSON
);

INSERT INTO test_json_modification VALUES
    (1, '{"name": "Alice", "age": 28}'),
    (2, '{"name": "Bob", "city": "NYC"}');

-- JSON_SET (inserts or updates)
SELECT id, JSON_SET(data, '$.age', 30) AS with_age FROM test_json_modification ORDER BY id;
SELECT id, JSON_SET(data, '$.email', 'user@example.com') AS with_email FROM test_json_modification ORDER BY id;

-- JSON_INSERT (inserts only if path doesn't exist)
SELECT id, JSON_INSERT(data, '$.country', 'USA') AS with_country FROM test_json_modification ORDER BY id;

-- JSON_REPLACE (updates only if path exists)
SELECT id, JSON_REPLACE(data, '$.name', 'Updated Name') AS updated_name FROM test_json_modification ORDER BY id;

-- JSON_REMOVE (removes a path)
SELECT id, JSON_REMOVE(data, '$.age') AS without_age FROM test_json_modification WHERE id = 1;

-- ============================================================================
-- Section 7: JSON Array Functions
-- ============================================================================

CREATE TABLE test_json_arrays (
    id INT PRIMARY KEY,
    items JSON
);

INSERT INTO test_json_arrays VALUES
    (1, '[10, 20, 30, 40, 50]'),
    (2, '["apple", "banana", "cherry"]'),
    (3, '[{"id": 1, "name": "A"}, {"id": 2, "name": "B"}, {"id": 3, "name": "C"}]');

-- JSON_LENGTH (array/object length)
SELECT id, JSON_LENGTH(items) AS array_length FROM test_json_arrays ORDER BY id;

-- JSON_ARRAY_APPEND (append to array)
SELECT id, JSON_ARRAY_APPEND(items, '$', 999) AS with_appended FROM test_json_arrays WHERE id = 1;

-- JSON_ARRAY_INSERT (insert at specific position)
SELECT id, JSON_ARRAY_INSERT(items, '$[1]', 15) AS with_inserted FROM test_json_arrays WHERE id = 1;

-- Extract array elements
SELECT id, items->0 AS first_element FROM test_json_arrays ORDER BY id;
SELECT id, items->1 AS second_element FROM test_json_arrays ORDER BY id;
SELECT id, items->-1 AS last_element FROM test_json_arrays ORDER BY id;

-- ============================================================================
-- Section 8: JSON in WHERE Clauses and Filtering
-- ============================================================================

CREATE TABLE test_json_filtering (
    id INT PRIMARY KEY,
    user_info JSON
);

INSERT INTO test_json_filtering VALUES
    (1, '{"name": "Alice", "age": 28, "active": true, "role": "admin"}'),
    (2, '{"name": "Bob", "age": 35, "active": false, "role": "user"}'),
    (3, '{"name": "Charlie", "age": 22, "active": true, "role": "user"}'),
    (4, '{"name": "Diana", "age": 30, "active": true, "role": "admin"}'),
    (5, '{"name": "Eve", "age": 45, "active": false, "role": "moderator"}');

-- Filter by JSON field value
SELECT id, user_info->>'name' AS name FROM test_json_filtering
WHERE user_info->>'active' = 'true' ORDER BY id;

SELECT id, user_info->>'name' AS name FROM test_json_filtering
WHERE user_info->>'role' = 'admin' ORDER BY id;

-- Filter by numeric comparison (cast to INT)
SELECT id, user_info->>'name' AS name, user_info->>'age' AS age
FROM test_json_filtering
WHERE CAST(user_info->>'age' AS INT) >= 30 ORDER BY id;

-- Combine multiple JSON conditions
SELECT id, user_info->>'name' AS name FROM test_json_filtering
WHERE user_info->>'active' = 'true' AND user_info->>'role' = 'user' ORDER BY id;

-- ============================================================================
-- Section 9: JSON Operators - @> (contains), <@ (contained by), ? (key exists)
-- ============================================================================

CREATE TABLE test_json_operators (
    id INT PRIMARY KEY,
    config JSON
);

INSERT INTO test_json_operators VALUES
    (1, '{"features": ["auth", "logging", "caching"], "version": "1.0"}'),
    (2, '{"features": ["auth", "monitoring"], "version": "2.0"}'),
    (3, '{"features": ["logging", "caching"], "version": "1.5"}'),
    (4, '{"features": ["auth", "logging", "caching", "monitoring"], "version": "2.1"}');

-- Check if JSON contains specific structure (@>)
SELECT id FROM test_json_operators
WHERE config @> '{"version": "1.0"}' ORDER BY id;

-- Check if contained by specific structure (<@)
SELECT id FROM test_json_operators
WHERE '{"version": "2.0"}' <@ config ORDER BY id;

-- Check if key exists (?)
SELECT id FROM test_json_operators
WHERE config ? 'version' ORDER BY id;

SELECT id FROM test_json_operators
WHERE config ? 'features' ORDER BY id;

-- ============================================================================
-- Section 10: JSON Indexing
-- ============================================================================

CREATE TABLE test_json_index (
    id INT PRIMARY KEY,
    metadata JSON
);

INSERT INTO test_json_index VALUES
    (1, '{"category": "electronics", "brand": "Samsung", "price": 599.99}'),
    (2, '{"category": "books", "author": "John Doe", "price": 19.99}'),
    (3, '{"category": "electronics", "brand": "Apple", "price": 999.99}'),
    (4, '{"category": "clothing", "brand": "Nike", "price": 79.99}'),
    (5, '{"category": "books", "author": "Jane Smith", "price": 24.99}');

-- Create index on extracted JSON field
CREATE INDEX idx_json_category ON test_json_index ((metadata->>'category'));
CREATE INDEX idx_json_brand ON test_json_index ((metadata->>'brand'));

-- Query using indexed JSON field
SELECT id, metadata->>'category' AS category, metadata->>'price' AS price
FROM test_json_index
WHERE metadata->>'category' = 'electronics' ORDER BY id;

-- ============================================================================
-- Section 11: JSON Aggregation
-- ============================================================================

CREATE TABLE test_json_aggregation (
    id INT PRIMARY KEY,
    product_data JSON
);

INSERT INTO test_json_aggregation VALUES
    (1, '{"category": "electronics", "price": 599.99, "quantity": 10}'),
    (2, '{"category": "electronics", "price": 999.99, "quantity": 5}'),
    (3, '{"category": "books", "price": 19.99, "quantity": 100}'),
    (4, '{"category": "books", "price": 24.99, "quantity": 50}'),
    (5, '{"category": "clothing", "price": 79.99, "quantity": 30}');

-- Aggregate JSON extracted values
SELECT product_data->>'category' AS category,
       COUNT(*) AS count,
       SUM(CAST(product_data->>'price' AS DECIMAL(10,2))) AS total_price,
       AVG(CAST(product_data->>'quantity' AS INT)) AS avg_quantity
FROM test_json_aggregation
GROUP BY product_data->>'category'
ORDER BY category;

-- JSON_ARRAYAGG (aggregate rows into JSON array)
CREATE TABLE test_json_arrayagg (
    id INT,
    name VARCHAR(100)
);

INSERT INTO test_json_arrayagg VALUES
    (1, 'Alice'),
    (2, 'Bob'),
    (3, 'Charlie');

SELECT JSON_ARRAYAGG(name) AS names_array FROM test_json_arrayagg;

-- JSON_OBJECTAGG (aggregate key-value pairs into JSON object)
SELECT JSON_OBJECTAGG(id, name) AS id_name_mapping FROM test_json_arrayagg;

-- ============================================================================
-- Section 12: NULL Handling in JSON
-- ============================================================================

CREATE TABLE test_json_nulls (
    id INT PRIMARY KEY,
    data JSON
);

INSERT INTO test_json_nulls VALUES
    (1, '{"key": "value"}'),
    (2, '{"key": null}'),
    (3, NULL),
    (4, '{}');

SELECT id, data, data IS NULL AS is_null, data IS NOT NULL AS is_not_null
FROM test_json_nulls ORDER BY id;

-- Distinguish between SQL NULL and JSON null
SELECT id,
       data->>'key' AS extracted_value,
       data->>'key' IS NULL AS extracted_is_null
FROM test_json_nulls ORDER BY id;

-- ============================================================================
-- Section 13: JSON Comparison and Sorting
-- ============================================================================

CREATE TABLE test_json_comparison (
    id INT PRIMARY KEY,
    json_value JSON
);

INSERT INTO test_json_comparison VALUES
    (1, '{"priority": 1, "name": "High"}'),
    (2, '{"priority": 3, "name": "Low"}'),
    (3, '{"priority": 2, "name": "Medium"}'),
    (4, '{"priority": 1, "name": "Critical"}');

-- Sort by extracted JSON field
SELECT id, json_value->>'name' AS name, json_value->>'priority' AS priority
FROM test_json_comparison
ORDER BY CAST(json_value->>'priority' AS INT), json_value->>'name';

-- JSON equality comparison
SELECT id FROM test_json_comparison
WHERE json_value = '{"priority": 1, "name": "High"}' ORDER BY id;

-- ============================================================================
-- Section 14: JSON Schema Flexibility - Real-world Use Case
-- ============================================================================

CREATE TABLE test_json_flexible_schema (
    id INT PRIMARY KEY,
    entity_type VARCHAR(50),
    attributes JSON,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Different entity types with different schemas
INSERT INTO test_json_flexible_schema (id, entity_type, attributes) VALUES
    (1, 'user', '{"name": "Alice", "email": "alice@example.com", "preferences": {"theme": "dark", "notifications": true}}'),
    (2, 'product', '{"name": "Laptop", "price": 999.99, "specs": {"ram": "16GB", "cpu": "Intel i7", "storage": "512GB SSD"}}'),
    (3, 'order', '{"order_id": "ORD-001", "total": 1499.98, "items": [{"id": 1, "qty": 2}, {"id": 5, "qty": 1}], "status": "shipped"}'),
    (4, 'user', '{"name": "Bob", "email": "bob@example.com", "preferences": {"theme": "light", "language": "en"}}'),
    (5, 'product', '{"name": "Mouse", "price": 29.99, "specs": {"dpi": 1600, "wireless": true}}');

-- Query different entity types
SELECT id, entity_type, attributes->>'name' AS name FROM test_json_flexible_schema
WHERE entity_type = 'user' ORDER BY id;

SELECT id, attributes->>'name' AS product_name, attributes->>'price' AS price
FROM test_json_flexible_schema
WHERE entity_type = 'product' ORDER BY id;

-- Extract nested preferences for users
SELECT id,
       attributes->>'name' AS name,
       attributes->'preferences'->>'theme' AS theme
FROM test_json_flexible_schema
WHERE entity_type = 'user' ORDER BY id;

-- ============================================================================
-- Section 15: API Response Storage - Real-world Use Case
-- ============================================================================

CREATE TABLE test_json_api_responses (
    id INT PRIMARY KEY,
    endpoint VARCHAR(200),
    response_data JSON,
    status_code INT,
    fetched_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_json_api_responses (id, endpoint, response_data, status_code) VALUES
    (1, '/api/users/123', '{"id": 123, "username": "alice", "email": "alice@example.com", "created": "2024-01-15"}', 200),
    (2, '/api/products/search', '{"results": [{"id": 1, "name": "Laptop"}, {"id": 2, "name": "Mouse"}], "total": 2, "page": 1}', 200),
    (3, '/api/orders/456', '{"id": 456, "status": "shipped", "items": [{"sku": "ABC123", "qty": 2}], "tracking": "TRACK789"}', 200),
    (4, '/api/users/999', '{"error": "User not found", "code": 404}', 404);

-- Query successful responses
SELECT id, endpoint, response_data
FROM test_json_api_responses
WHERE status_code = 200 ORDER BY id;

-- Extract error messages from failed responses
SELECT id, endpoint, response_data->>'error' AS error_message, status_code
FROM test_json_api_responses
WHERE status_code != 200 ORDER BY id;

-- Extract nested data from search results
SELECT id,
       response_data->'results'->0->>'name' AS first_result,
       response_data->>'total' AS total_results
FROM test_json_api_responses
WHERE endpoint = '/api/products/search';

-- ============================================================================
-- Section 16: Configuration Storage - Real-world Use Case
-- ============================================================================

CREATE TABLE test_json_config (
    id INT PRIMARY KEY,
    app_name VARCHAR(100),
    config JSON,
    environment VARCHAR(50),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_json_config (id, app_name, config, environment) VALUES
    (1, 'web-app', '{"database": {"host": "localhost", "port": 5432}, "cache": {"enabled": true, "ttl": 3600}, "logging": {"level": "info"}}', 'development'),
    (2, 'web-app', '{"database": {"host": "prod-db.example.com", "port": 5432}, "cache": {"enabled": true, "ttl": 7200}, "logging": {"level": "warn"}}', 'production'),
    (3, 'api-service', '{"rate_limit": {"requests": 1000, "window": 60}, "auth": {"type": "jwt", "expiry": 3600}}', 'production');

-- Query configuration by environment
SELECT id, app_name, config
FROM test_json_config
WHERE environment = 'production' ORDER BY id;

-- Extract specific config values
SELECT id, app_name,
       config->'database'->>'host' AS db_host,
       config->'cache'->>'ttl' AS cache_ttl,
       config->'logging'->>'level' AS log_level
FROM test_json_config
WHERE app_name = 'web-app' ORDER BY id;

-- Update configuration
UPDATE test_json_config
SET config = JSON_SET(config, '$.logging.level', 'debug')
WHERE id = 1 AND environment = 'development';

SELECT config->'logging'->>'level' AS log_level FROM test_json_config WHERE id = 1;

-- ============================================================================
-- Section 17: JSON Performance Characteristics
-- ============================================================================

CREATE TABLE test_json_performance (
    id INT PRIMARY KEY,
    json_data JSON,
    extracted_field VARCHAR(100)
);

-- Populate with sample data
INSERT INTO test_json_performance VALUES
    (1, '{"field": "value1", "other": "data"}', 'value1'),
    (2, '{"field": "value2", "other": "data"}', 'value2'),
    (3, '{"field": "value3", "other": "data"}', 'value3');

-- Compare JSON extraction vs stored field performance (illustrative)
-- JSON extraction requires parsing on each query
SELECT id, json_data->>'field' AS from_json FROM test_json_performance WHERE id = 1;

-- Stored field is directly accessible (faster)
SELECT id, extracted_field AS from_column FROM test_json_performance WHERE id = 1;

-- Note: For frequently accessed fields, consider storing them as regular columns
-- JSON is best for semi-structured data that changes structure frequently

-- ============================================================================
-- Section 18: JSON vs JSONB Comparison
-- ============================================================================

CREATE TABLE test_json_vs_jsonb (
    id INT PRIMARY KEY,
    json_col JSON,
    jsonb_col JSONB
);

INSERT INTO test_json_vs_jsonb VALUES
    (1, '{"key": "value", "number": 42}', '{"key": "value", "number": 42}'),
    (2, '{"  whitespace  ":  "preserved"  }', '{"  whitespace  ":  "preserved"  }');

-- JSON preserves input formatting and whitespace
SELECT id, json_col FROM test_json_vs_jsonb ORDER BY id;

-- JSONB is binary format, normalized (whitespace removed, keys sorted)
SELECT id, jsonb_col FROM test_json_vs_jsonb ORDER BY id;

-- Note: JSONB is faster for operations but slower for insertion
-- JSONB supports indexing with GIN indexes for containment operations
-- JSONB does not preserve key order or whitespace

-- ============================================================================
-- Section 19: JSON Pretty Printing and Formatting
-- ============================================================================

CREATE TABLE test_json_formatting (
    id INT PRIMARY KEY,
    data JSON
);

INSERT INTO test_json_formatting VALUES
    (1, '{"name":"Alice","age":30,"address":{"city":"NYC","zip":"10001"}}');

-- JSON_PRETTY for human-readable output
SELECT id, JSON_PRETTY(data) AS formatted FROM test_json_formatting WHERE id = 1;

-- Compact JSON
SELECT id, JSON_COMPACT(data) AS compacted FROM test_json_formatting WHERE id = 1;

-- ============================================================================
-- Section 20: Complex Real-world Example - Event Sourcing
-- ============================================================================

CREATE TABLE test_json_event_log (
    id INT PRIMARY KEY,
    entity_id UUID,
    event_type VARCHAR(50),
    event_data JSON,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_json_event_log (id, entity_id, event_type, event_data) VALUES
    (1, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'user_created', '{"username": "alice", "email": "alice@example.com"}'),
    (2, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'profile_updated', '{"email": "alice.new@example.com"}'),
    (3, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'password_changed', '{"changed_at": "2024-01-15T10:30:00Z"}'),
    (4, '660e8400-e29b-41d4-a716-446655440001'::UUID, 'user_created', '{"username": "bob", "email": "bob@example.com"}'),
    (5, '550e8400-e29b-41d4-a716-446655440000'::UUID, 'login', '{"ip": "192.168.1.100", "device": "Chrome/MacOS"}');

-- Query event stream for specific entity
SELECT id, event_type, event_data, timestamp
FROM test_json_event_log
WHERE entity_id = '550e8400-e29b-41d4-a716-446655440000'::UUID
ORDER BY id;

-- Aggregate event types
SELECT event_type, COUNT(*) AS count FROM test_json_event_log
GROUP BY event_type ORDER BY count DESC;

-- Extract specific event data
SELECT id, event_type, event_data->>'email' AS email
FROM test_json_event_log
WHERE event_type IN ('user_created', 'profile_updated') ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_json_event_log;
DROP TABLE test_json_formatting;
DROP TABLE test_json_vs_jsonb;
DROP TABLE test_json_performance;
DROP TABLE test_json_config;
DROP TABLE test_json_api_responses;
DROP TABLE test_json_flexible_schema;
DROP TABLE test_json_comparison;
DROP TABLE test_json_nulls;
DROP TABLE test_json_arrayagg;
DROP TABLE test_json_aggregation;
DROP TABLE test_json_index;
DROP TABLE test_json_operators;
DROP TABLE test_json_filtering;
DROP TABLE test_json_arrays;
DROP TABLE test_json_modification;
DROP TABLE test_json_construction;
DROP TABLE test_json_functions;
DROP TABLE test_json_paths;
DROP TABLE test_json_extraction;
DROP TABLE test_json_basic;

DROP DATABASE test_json_db;

-- End of JSON data type tests
