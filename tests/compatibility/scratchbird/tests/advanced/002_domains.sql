-- ScratchBird Native Test: Domains and Custom Types
-- Test ID: advanced_002
-- Description: Domain creation, constraints, and custom types

-- Create database
CREATE DATABASE test_domains;
\c test_domains;

-- Test 1: Basic Domain with Constraint
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}$');

CREATE TABLE contacts (
    contact_id INTEGER PRIMARY KEY,
    name VARCHAR(100),
    email email_address
);

-- Valid email
INSERT INTO contacts VALUES (1, 'John Doe', 'john.doe@example.com');

-- Invalid email (should fail)
-- INSERT INTO contacts VALUES (2, 'Jane Smith', 'invalid-email');

SELECT * FROM contacts;

-- Test 2: Domain with DEFAULT
CREATE DOMAIN status_code AS VARCHAR(20)
    DEFAULT 'PENDING'
    CHECK (VALUE IN ('PENDING', 'APPROVED', 'REJECTED', 'CANCELLED'));

CREATE TABLE requests (
    request_id INTEGER PRIMARY KEY,
    description TEXT,
    status status_code
);

-- Insert without status (uses default)
INSERT INTO requests (request_id, description) VALUES (1, 'Request A');

-- Insert with explicit status
INSERT INTO requests VALUES (2, 'Request B', 'APPROVED');

-- Invalid status (should fail)
-- INSERT INTO requests VALUES (3, 'Request C', 'INVALID_STATUS');

SELECT * FROM requests ORDER BY request_id;

-- Test 3: Domain with NOT NULL
CREATE DOMAIN positive_integer AS INTEGER
    NOT NULL
    CHECK (VALUE > 0);

CREATE TABLE inventory (
    item_id INTEGER PRIMARY KEY,
    quantity positive_integer
);

INSERT INTO inventory VALUES (1, 100);

-- NULL value (should fail)
-- INSERT INTO inventory VALUES (2, NULL);

-- Negative value (should fail)
-- INSERT INTO inventory VALUES (3, -5);

SELECT * FROM inventory;

-- Test 4: Nested Domain
CREATE DOMAIN price_range AS DECIMAL(10, 2)
    CHECK (VALUE >= 0 AND VALUE <= 1000000);

CREATE DOMAIN retail_price AS price_range
    CHECK (VALUE >= 0.01);

CREATE TABLE products (
    product_id INTEGER PRIMARY KEY,
    product_name VARCHAR(100),
    price retail_price
);

INSERT INTO products VALUES (1, 'Widget', 49.99);

-- Price of 0 (should fail nested constraint)
-- INSERT INTO products VALUES (2, 'Gadget', 0.00);

SELECT * FROM products;

-- Test 5: ALTER DOMAIN
ALTER DOMAIN status_code ADD CONSTRAINT status_not_empty CHECK (LENGTH(VALUE) > 0);

-- Test adding constraint to existing data
ALTER DOMAIN email_address ADD CONSTRAINT email_max_length CHECK (LENGTH(VALUE) <= 100);

-- Test 6: DROP CONSTRAINT
ALTER DOMAIN status_code DROP CONSTRAINT status_not_empty;

-- Test 7: RENAME DOMAIN
ALTER DOMAIN status_code RENAME TO request_status;

-- Verify rename
\dD request_status;

-- Test 8: Enum Domain
CREATE DOMAIN priority_level AS VARCHAR(10)
    CHECK (VALUE IN ('LOW', 'MEDIUM', 'HIGH', 'CRITICAL'));

CREATE TABLE tasks (
    task_id INTEGER PRIMARY KEY,
    task_name VARCHAR(100),
    priority priority_level DEFAULT 'MEDIUM'
);

INSERT INTO tasks VALUES (1, 'Task A', 'HIGH');
INSERT INTO tasks (task_id, task_name) VALUES (2, 'Task B');

SELECT * FROM tasks ORDER BY task_id;

-- Test 9: Composite Type
CREATE TYPE address AS (
    street VARCHAR(100),
    city VARCHAR(50),
    state VARCHAR(2),
    zip_code VARCHAR(10)
);

CREATE TABLE customers (
    customer_id INTEGER PRIMARY KEY,
    customer_name VARCHAR(100),
    shipping_address address
);

INSERT INTO customers VALUES
    (1, 'Alice Johnson', ROW('123 Main St', 'Springfield', 'IL', '62701'));

SELECT customer_id, customer_name,
       (shipping_address).street,
       (shipping_address).city,
       (shipping_address).state
FROM customers;

-- Test 10: Array Type
CREATE DOMAIN tag_array AS VARCHAR(50)[];

CREATE TABLE articles (
    article_id INTEGER PRIMARY KEY,
    title VARCHAR(200),
    tags tag_array
);

INSERT INTO articles VALUES
    (1, 'Database Design', ARRAY['database', 'design', 'sql']),
    (2, 'Web Development', ARRAY['web', 'javascript', 'html']);

SELECT title, tags FROM articles WHERE 'database' = ANY(tags);

-- Test 11: Range Type
CREATE DOMAIN date_range AS DATERANGE;

CREATE TABLE events (
    event_id INTEGER PRIMARY KEY,
    event_name VARCHAR(100),
    event_period date_range
);

INSERT INTO events VALUES
    (1, 'Conference 2025', '[2025-06-01,2025-06-03)'),
    (2, 'Workshop', '[2025-07-15,2025-07-16)');

SELECT event_name FROM events
WHERE event_period @> '2025-06-02'::DATE;

-- Test 12: Domain on Domain
CREATE DOMAIN us_zip_code AS VARCHAR(10)
    CHECK (VALUE ~ '^\d{5}(-\d{4})?$');

CREATE DOMAIN extended_zip AS us_zip_code
    CHECK (LENGTH(VALUE) = 10 OR LENGTH(VALUE) = 5);

CREATE TABLE addresses (
    address_id INTEGER PRIMARY KEY,
    zip extended_zip
);

INSERT INTO addresses VALUES (1, '12345');
INSERT INTO addresses VALUES (2, '12345-6789');

-- Invalid format (should fail)
-- INSERT INTO addresses VALUES (3, '1234');

SELECT * FROM addresses;

-- Test 13: Drop Domain with CASCADE
-- Create dependency
CREATE TABLE test_table (
    id INTEGER PRIMARY KEY,
    email email_address
);

-- Drop domain (should fail without CASCADE)
-- DROP DOMAIN email_address;

-- Drop domain with CASCADE
DROP DOMAIN email_address CASCADE;

-- test_table and contacts should lose the email column type

-- Verify domain is gone
-- \dD email_address;

-- Test 14: Domain with Multiple Constraints
CREATE DOMAIN username AS VARCHAR(50)
    CHECK (LENGTH(VALUE) >= 3)
    CHECK (LENGTH(VALUE) <= 20)
    CHECK (VALUE ~ '^[a-zA-Z0-9_]+$');

CREATE TABLE users (
    user_id INTEGER PRIMARY KEY,
    username username
);

INSERT INTO users VALUES (1, 'john_doe');
INSERT INTO users VALUES (2, 'alice123');

-- Too short (should fail)
-- INSERT INTO users VALUES (3, 'ab');

-- Invalid characters (should fail)
-- INSERT INTO users VALUES (4, 'john@doe');

SELECT * FROM users;

-- Cleanup
DROP TABLE users;
DROP DOMAIN username;
DROP TABLE addresses;
DROP DOMAIN extended_zip;
DROP DOMAIN us_zip_code;
DROP TABLE events;
DROP DOMAIN date_range;
DROP TABLE articles;
DROP DOMAIN tag_array;
DROP TABLE customers;
DROP TYPE address;
DROP TABLE tasks;
DROP DOMAIN priority_level;
DROP TABLE requests;
DROP DOMAIN request_status;
DROP TABLE inventory;
DROP DOMAIN positive_integer;
DROP TABLE products;
DROP DOMAIN retail_price;
DROP DOMAIN price_range;
DROP TABLE contacts;
DROP TABLE test_table CASCADE;

DROP DATABASE test_domains;
