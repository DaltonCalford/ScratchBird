-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Data Types - COMPOSITE (User-Defined Types)
-- Description: Comprehensive COMPOSITE/ROW data type testing
-- ============================================================================

-- COMPOSITE types (also known as ROW types or user-defined types) allow grouping
-- multiple fields into a single type
-- Used for: complex data structures, object-relational mapping, nested data

-- Create test database
CREATE DATABASE test_composite_db;
USE test_composite_db;

-- ============================================================================
-- Section 1: Basic Composite Type Definition
-- ============================================================================

-- Define a simple composite type
CREATE TYPE address AS (
    street VARCHAR(200),
    city VARCHAR(100),
    state VARCHAR(50),
    zip VARCHAR(20)
);

CREATE TABLE test_composite_basic (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    home_address address,
    work_address address
);

-- Insert using ROW constructor
INSERT INTO test_composite_basic VALUES
    (1, 'Alice Johnson',
     ROW('123 Main St', 'New York', 'NY', '10001'),
     ROW('456 Broadway', 'New York', 'NY', '10002'));

INSERT INTO test_composite_basic VALUES
    (2, 'Bob Smith',
     ROW('789 Oak Ave', 'Los Angeles', 'CA', '90001'),
     ROW('321 Elm St', 'Los Angeles', 'CA', '90002'));

SELECT id, name, home_address, work_address FROM test_composite_basic ORDER BY id;

-- ============================================================================
-- Section 2: Accessing Composite Type Fields
-- ============================================================================

-- Access fields using dot notation
SELECT id, name,
       (home_address).street AS home_street,
       (home_address).city AS home_city,
       (home_address).state AS home_state,
       (home_address).zip AS home_zip
FROM test_composite_basic ORDER BY id;

-- Access work address fields
SELECT id, name,
       (work_address).street AS work_street,
       (work_address).city AS work_city
FROM test_composite_basic ORDER BY id;

-- ============================================================================
-- Section 3: Nested Composite Types
-- ============================================================================

-- Create nested composite types
CREATE TYPE contact_info AS (
    email VARCHAR(200),
    phone VARCHAR(50)
);

CREATE TYPE person AS (
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    contact contact_info,
    address address
);

CREATE TABLE test_composite_nested (
    id INT PRIMARY KEY,
    employee person
);

INSERT INTO test_composite_nested VALUES
    (1, ROW('John', 'Doe',
            ROW('john@example.com', '555-1234'),
            ROW('100 First St', 'Boston', 'MA', '02101')));

INSERT INTO test_composite_nested VALUES
    (2, ROW('Jane', 'Smith',
            ROW('jane@example.com', '555-5678'),
            ROW('200 Second Ave', 'Seattle', 'WA', '98101')));

SELECT id, employee FROM test_composite_nested ORDER BY id;

-- Access nested fields
SELECT id,
       (employee).first_name AS first_name,
       (employee).last_name AS last_name,
       ((employee).contact).email AS email,
       ((employee).contact).phone AS phone,
       ((employee).address).city AS city,
       ((employee).address).state AS state
FROM test_composite_nested ORDER BY id;

-- ============================================================================
-- Section 4: Composite Type in Functions
-- ============================================================================

-- Function returning composite type
CREATE FUNCTION create_address(
    p_street VARCHAR(200),
    p_city VARCHAR(100),
    p_state VARCHAR(50),
    p_zip VARCHAR(20)
) RETURNS address AS $$
BEGIN
    RETURN ROW(p_street, p_city, p_state, p_zip)::address;
END;
$$ LANGUAGE plpgsql;

-- Function accepting composite type
CREATE FUNCTION format_address(addr address) RETURNS TEXT AS $$
BEGIN
    RETURN addr.street || ', ' || addr.city || ', ' || addr.state || ' ' || addr.zip;
END;
$$ LANGUAGE plpgsql;

-- Use functions
SELECT create_address('500 Tech Blvd', 'San Francisco', 'CA', '94105') AS new_address;

SELECT id, name,
       format_address(home_address) AS formatted_home,
       format_address(work_address) AS formatted_work
FROM test_composite_basic ORDER BY id;

-- ============================================================================
-- Section 5: Composite Type Arrays
-- ============================================================================

CREATE TABLE test_composite_arrays (
    id INT PRIMARY KEY,
    name VARCHAR(100),
    addresses address[]  -- Array of composite type
);

INSERT INTO test_composite_arrays VALUES
    (1, 'MultiLocation Corp',
     ARRAY[
         ROW('HQ Building', 'New York', 'NY', '10001')::address,
         ROW('West Office', 'Los Angeles', 'CA', '90001')::address,
         ROW('East Office', 'Boston', 'MA', '02101')::address
     ]);

SELECT id, name, addresses FROM test_composite_arrays WHERE id = 1;

-- Access array elements and their fields
SELECT id, name,
       (addresses[1]).city AS first_location,
       (addresses[2]).city AS second_location,
       (addresses[3]).city AS third_location
FROM test_composite_arrays WHERE id = 1;

-- Unnest array of composite types
SELECT id, name,
       (addr).street,
       (addr).city,
       (addr).state
FROM test_composite_arrays,
     unnest(addresses) AS addr
WHERE id = 1;

-- ============================================================================
-- Section 6: Composite Type Comparison
-- ============================================================================

CREATE TABLE test_composite_comparison (
    id INT PRIMARY KEY,
    location address
);

INSERT INTO test_composite_comparison VALUES
    (1, ROW('123 Main St', 'New York', 'NY', '10001')),
    (2, ROW('123 Main St', 'New York', 'NY', '10001')),  -- Identical
    (3, ROW('456 Oak Ave', 'Boston', 'MA', '02101'));

-- Equality comparison
SELECT id FROM test_composite_comparison
WHERE location = ROW('123 Main St', 'New York', 'NY', '10001')::address
ORDER BY id;

-- Inequality
SELECT id FROM test_composite_comparison
WHERE location != ROW('456 Oak Ave', 'Boston', 'MA', '02101')::address
ORDER BY id;

-- ============================================================================
-- Section 7: Composite Type Constructors
-- ============================================================================

-- Different ways to construct composite values
CREATE TABLE test_composite_constructors (
    id INT PRIMARY KEY,
    addr address
);

-- Using ROW keyword
INSERT INTO test_composite_constructors VALUES
    (1, ROW('Street 1', 'City 1', 'State 1', '11111'));

-- Using cast
INSERT INTO test_composite_constructors VALUES
    (2, ('Street 2', 'City 2', 'State 2', '22222')::address);

-- Using type name directly
INSERT INTO test_composite_constructors VALUES
    (3, address('Street 3', 'City 3', 'State 3', '33333'));

SELECT id, addr FROM test_composite_constructors ORDER BY id;

-- ============================================================================
-- Section 8: Modifying Composite Type Fields
-- ============================================================================

CREATE TABLE test_composite_update (
    id INT PRIMARY KEY,
    location address
);

INSERT INTO test_composite_update VALUES
    (1, ROW('100 Old St', 'OldCity', 'OS', '00000'));

-- Update specific field
UPDATE test_composite_update
SET location = ROW(
    (location).street,
    'NewCity',  -- Update city only
    (location).state,
    (location).zip
)
WHERE id = 1;

SELECT location FROM test_composite_update WHERE id = 1;

-- Update multiple fields
UPDATE test_composite_update
SET location = ROW(
    '200 New St',
    'NewCity',
    'NS',
    (location).zip
)
WHERE id = 1;

SELECT location FROM test_composite_update WHERE id = 1;

-- ============================================================================
-- Section 9: Composite Types in Queries
-- ============================================================================

CREATE TYPE product_info AS (
    product_id INT,
    product_name VARCHAR(200),
    price DECIMAL(10, 2)
);

CREATE TABLE test_composite_queries (
    order_id INT PRIMARY KEY,
    customer_name VARCHAR(100),
    product product_info
);

INSERT INTO test_composite_queries VALUES
    (1, 'Alice', ROW(101, 'Laptop', 999.99)),
    (2, 'Bob', ROW(102, 'Mouse', 29.99)),
    (3, 'Charlie', ROW(103, 'Keyboard', 79.99)),
    (4, 'Diana', ROW(101, 'Laptop', 999.99));

-- Filter by composite field value
SELECT order_id, customer_name,
       (product).product_name AS product,
       (product).price AS price
FROM test_composite_queries
WHERE (product).price > 50
ORDER BY order_id;

-- Group by composite field
SELECT (product).product_name AS product,
       COUNT(*) AS order_count
FROM test_composite_queries
GROUP BY (product).product_name
ORDER BY product;

-- Aggregate on composite field
SELECT
    AVG((product).price) AS avg_price,
    MIN((product).price) AS min_price,
    MAX((product).price) AS max_price
FROM test_composite_queries;

-- ============================================================================
-- Section 10: Composite Type with Different Field Types
-- ============================================================================

CREATE TYPE mixed_type AS (
    id INT,
    name VARCHAR(100),
    active BOOLEAN,
    score DECIMAL(5, 2),
    created_at TIMESTAMP,
    tags TEXT[]
);

CREATE TABLE test_composite_mixed (
    record_id INT PRIMARY KEY,
    data mixed_type
);

INSERT INTO test_composite_mixed VALUES
    (1, ROW(100, 'Record A', true, 95.50, '2024-01-15 10:00:00'::TIMESTAMP, ARRAY['tag1', 'tag2']));

INSERT INTO test_composite_mixed VALUES
    (2, ROW(200, 'Record B', false, 78.25, '2024-02-20 15:30:00'::TIMESTAMP, ARRAY['tag3']));

SELECT record_id, data FROM test_composite_mixed ORDER BY record_id;

-- Access different field types
SELECT record_id,
       (data).id AS id,
       (data).name AS name,
       (data).active AS active,
       (data).score AS score,
       (data).created_at AS created_at,
       (data).tags AS tags
FROM test_composite_mixed ORDER BY record_id;

-- ============================================================================
-- Section 11: NULL Handling in Composite Types
-- ============================================================================

CREATE TABLE test_composite_nulls (
    id INT PRIMARY KEY,
    location address
);

INSERT INTO test_composite_nulls VALUES
    (1, ROW('123 Main St', 'NYC', 'NY', '10001')),
    (2, NULL),  -- Entire composite is NULL
    (3, ROW(NULL, 'Boston', 'MA', '02101')),  -- Field within composite is NULL
    (4, ROW('456 Oak', NULL, NULL, '12345'));  -- Multiple NULL fields

SELECT id, location, location IS NULL AS is_null_composite FROM test_composite_nulls ORDER BY id;

-- Access fields from NULL composite
SELECT id,
       (location).street AS street,
       (location).city AS city
FROM test_composite_nulls ORDER BY id;

-- Check if specific field is NULL
SELECT id,
       (location).street IS NULL AS street_is_null,
       (location).city IS NULL AS city_is_null
FROM test_composite_nulls ORDER BY id;

-- ============================================================================
-- Section 12: Composite Type as Function Parameter
-- ============================================================================

CREATE FUNCTION is_local_address(addr address, local_state VARCHAR(50))
RETURNS BOOLEAN AS $$
BEGIN
    RETURN addr.state = local_state;
END;
$$ LANGUAGE plpgsql;

SELECT id, name,
       is_local_address(home_address, 'NY') AS home_is_ny,
       is_local_address(work_address, 'NY') AS work_is_ny
FROM test_composite_basic ORDER BY id;

-- Function with multiple composite parameters
CREATE FUNCTION same_city(addr1 address, addr2 address)
RETURNS BOOLEAN AS $$
BEGIN
    RETURN addr1.city = addr2.city;
END;
$$ LANGUAGE plpgsql;

SELECT id, name,
       same_city(home_address, work_address) AS lives_and_works_same_city
FROM test_composite_basic ORDER BY id;

-- ============================================================================
-- Section 13: Composite Type in Joins
-- ============================================================================

CREATE TABLE test_composite_customers (
    customer_id INT PRIMARY KEY,
    customer_name VARCHAR(100),
    billing_address address
);

CREATE TABLE test_composite_orders (
    order_id INT PRIMARY KEY,
    customer_id INT,
    shipping_address address,
    order_total DECIMAL(10, 2)
);

INSERT INTO test_composite_customers VALUES
    (1, 'Alice', ROW('100 Home St', 'NYC', 'NY', '10001')),
    (2, 'Bob', ROW('200 Main Ave', 'LA', 'CA', '90001'));

INSERT INTO test_composite_orders VALUES
    (101, 1, ROW('100 Home St', 'NYC', 'NY', '10001'), 150.00),
    (102, 1, ROW('300 Gift St', 'Boston', 'MA', '02101'), 75.00),
    (103, 2, ROW('200 Main Ave', 'LA', 'CA', '90001'), 200.00);

-- Join and compare composite types
SELECT o.order_id,
       c.customer_name,
       CASE
           WHEN o.shipping_address = c.billing_address THEN 'Same as billing'
           ELSE 'Different from billing'
       END AS shipping_type
FROM test_composite_orders o
JOIN test_composite_customers c ON o.customer_id = c.customer_id
ORDER BY o.order_id;

-- ============================================================================
-- Section 14: Real-world Use Case - Geographic Coordinates
-- ============================================================================

CREATE TYPE coordinates AS (
    latitude DECIMAL(10, 7),
    longitude DECIMAL(10, 7)
);

CREATE TYPE location_data AS (
    name VARCHAR(200),
    coords coordinates,
    elevation INT  -- meters above sea level
);

CREATE TABLE test_composite_locations (
    id INT PRIMARY KEY,
    location location_data
);

INSERT INTO test_composite_locations VALUES
    (1, ROW('Mount Everest', ROW(27.9881, 86.9250), 8849)),
    (2, ROW('Death Valley', ROW(36.5054, -116.9325), -86)),
    (3, ROW('Tokyo Tower', ROW(35.6586, 139.7454), 333));

SELECT id,
       (location).name AS place_name,
       ((location).coords).latitude AS lat,
       ((location).coords).longitude AS lon,
       (location).elevation AS elevation_m
FROM test_composite_locations ORDER BY id;

-- Calculate distance between two points (simplified formula)
CREATE FUNCTION coord_distance(c1 coordinates, c2 coordinates)
RETURNS DECIMAL(10, 2) AS $$
DECLARE
    lat_diff DECIMAL(10, 7);
    lon_diff DECIMAL(10, 7);
BEGIN
    lat_diff := c1.latitude - c2.latitude;
    lon_diff := c1.longitude - c2.longitude;
    RETURN SQRT(lat_diff * lat_diff + lon_diff * lon_diff);
END;
$$ LANGUAGE plpgsql;

SELECT
    l1.id AS id1,
    (l1.location).name AS name1,
    l2.id AS id2,
    (l2.location).name AS name2,
    coord_distance((l1.location).coords, (l2.location).coords) AS simple_distance
FROM test_composite_locations l1
CROSS JOIN test_composite_locations l2
WHERE l1.id < l2.id
ORDER BY l1.id, l2.id;

-- ============================================================================
-- Section 15: Real-world Use Case - Money with Currency
-- ============================================================================

CREATE TYPE money_amount AS (
    amount DECIMAL(15, 2),
    currency VARCHAR(10)
);

CREATE TABLE test_composite_transactions (
    transaction_id INT PRIMARY KEY,
    description VARCHAR(200),
    amount money_amount,
    transaction_date DATE
);

INSERT INTO test_composite_transactions VALUES
    (1, 'Sale to US customer', ROW(100.00, 'USD'), '2024-01-15'),
    (2, 'Sale to EU customer', ROW(85.50, 'EUR'), '2024-01-16'),
    (3, 'Sale to UK customer', ROW(75.25, 'GBP'), '2024-01-17'),
    (4, 'Sale to US customer', ROW(250.00, 'USD'), '2024-01-18');

SELECT transaction_id,
       description,
       (amount).amount AS value,
       (amount).currency AS currency,
       transaction_date
FROM test_composite_transactions ORDER BY transaction_id;

-- Sum by currency
SELECT (amount).currency AS currency,
       SUM((amount).amount) AS total_amount,
       COUNT(*) AS transaction_count
FROM test_composite_transactions
GROUP BY (amount).currency
ORDER BY currency;

-- ============================================================================
-- Section 16: Real-world Use Case - Version Information
-- ============================================================================

CREATE TYPE version AS (
    major INT,
    minor INT,
    patch INT,
    pre_release VARCHAR(50)
);

CREATE TABLE test_composite_versions (
    app_name VARCHAR(100) PRIMARY KEY,
    current_version version,
    released_date DATE
);

INSERT INTO test_composite_versions VALUES
    ('App A', ROW(1, 2, 3, NULL), '2024-01-15'),
    ('App B', ROW(2, 0, 0, 'beta'), '2024-02-01'),
    ('App C', ROW(0, 9, 5, 'alpha'), '2024-03-10');

SELECT app_name,
       (current_version).major || '.' ||
       (current_version).minor || '.' ||
       (current_version).patch ||
       COALESCE('-' || (current_version).pre_release, '') AS version_string,
       released_date
FROM test_composite_versions ORDER BY app_name;

-- Function to compare versions
CREATE FUNCTION version_greater(v1 version, v2 version)
RETURNS BOOLEAN AS $$
BEGIN
    IF v1.major != v2.major THEN
        RETURN v1.major > v2.major;
    ELSIF v1.minor != v2.minor THEN
        RETURN v1.minor > v2.minor;
    ELSE
        RETURN v1.patch > v2.patch;
    END IF;
END;
$$ LANGUAGE plpgsql;

SELECT app_name,
       version_greater(current_version, ROW(1, 0, 0, NULL)::version) AS newer_than_1_0_0
FROM test_composite_versions ORDER BY app_name;

-- ============================================================================
-- Section 17: Composite Type in CTEs
-- ============================================================================

WITH enriched_data AS (
    SELECT
        id,
        name,
        home_address,
        ROW(
            (home_address).street,
            UPPER((home_address).city),
            (home_address).state,
            (home_address).zip
        )::address AS normalized_address
    FROM test_composite_basic
)
SELECT id, name,
       (normalized_address).city AS city_uppercase
FROM enriched_data ORDER BY id;

-- ============================================================================
-- Section 18: Composite Type Output Formats
-- ============================================================================

CREATE TABLE test_composite_output (
    id INT PRIMARY KEY,
    data address
);

INSERT INTO test_composite_output VALUES
    (1, ROW('123 Main', 'NYC', 'NY', '10001'));

-- Default output (parenthesized format)
SELECT id, data FROM test_composite_output WHERE id = 1;

-- Extract to JSON
SELECT id, ROW_TO_JSON(data) AS data_json FROM test_composite_output WHERE id = 1;

-- Individual fields
SELECT id,
       (data).street,
       (data).city,
       (data).state,
       (data).zip
FROM test_composite_output WHERE id = 1;

-- ============================================================================
-- Section 19: Composite Type Constraints
-- ============================================================================

-- Create type with validation via CHECK constraint
CREATE TYPE validated_age AS (
    years INT,
    CHECK (years >= 0 AND years <= 150)
);

-- Note: CHECK constraints on composite types may not be fully supported
-- Typically validation is done at table level or in application logic

CREATE TABLE test_composite_constraints (
    id INT PRIMARY KEY,
    person_name VARCHAR(100),
    addr address,
    CONSTRAINT valid_state CHECK ((addr).state ~ '^[A-Z]{2}$')  -- Two uppercase letters
);

INSERT INTO test_composite_constraints VALUES
    (1, 'Valid Person', ROW('123 St', 'City', 'NY', '10001'));

-- This would fail the constraint:
-- INSERT INTO test_composite_constraints VALUES
--     (2, 'Invalid Person', ROW('456 St', 'City', 'InvalidState', '10001'));

SELECT * FROM test_composite_constraints;

-- ============================================================================
-- Section 20: Composite Type Best Practices
-- ============================================================================

CREATE TABLE test_composite_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_composite_best_practices VALUES
    (1, 'Use composite types to group related fields logically'),
    (2, 'Composite types are ideal for address, coordinates, money+currency'),
    (3, 'Access fields using parentheses: (composite_col).field_name'),
    (4, 'Composite types can be nested for complex structures'),
    (5, 'Use ROW() constructor for clarity and type safety'),
    (6, 'Composite types can be used in arrays, function parameters, returns'),
    (7, 'Consider normalization vs denormalization trade-offs'),
    (8, 'Indexing on composite type fields requires expression indexes'),
    (9, 'NULL composite vs composite with NULL fields are different'),
    (10, 'Use composite types to reduce JOIN complexity for related data'),
    (11, 'Composite types improve code readability and maintainability'),
    (12, 'Can convert composite to JSON with ROW_TO_JSON'),
    (13, 'Useful for object-relational mapping (ORM) patterns'),
    (14, 'Consider performance: accessing fields requires field extraction'),
    (15, 'Composite types work well with procedural languages (PL/pgSQL)');

SELECT id, guideline FROM test_composite_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_composite_best_practices;
DROP TABLE test_composite_constraints;
DROP TABLE test_composite_output;
DROP TABLE test_composite_versions;
DROP FUNCTION version_greater(version, version);
DROP TABLE test_composite_transactions;
DROP TABLE test_composite_locations;
DROP FUNCTION coord_distance(coordinates, coordinates);
DROP TABLE test_composite_orders;
DROP TABLE test_composite_customers;
DROP FUNCTION same_city(address, address);
DROP FUNCTION is_local_address(address, VARCHAR);
DROP TABLE test_composite_nulls;
DROP TABLE test_composite_mixed;
DROP TABLE test_composite_queries;
DROP TABLE test_composite_update;
DROP TABLE test_composite_constructors;
DROP TABLE test_composite_comparison;
DROP TABLE test_composite_arrays;
DROP FUNCTION format_address(address);
DROP FUNCTION create_address(VARCHAR, VARCHAR, VARCHAR, VARCHAR);
DROP TABLE test_composite_nested;
DROP TABLE test_composite_basic;

DROP TYPE validated_age;
DROP TYPE money_amount;
DROP TYPE version;
DROP TYPE location_data;
DROP TYPE coordinates;
DROP TYPE product_info;
DROP TYPE mixed_type;
DROP TYPE person;
DROP TYPE contact_info;
DROP TYPE address;

DROP DATABASE test_composite_db;

-- End of COMPOSITE data type tests
