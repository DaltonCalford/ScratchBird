-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - BEFORE INSERT Row-Level
-- Description: Comprehensive BEFORE INSERT row-level trigger testing
-- ============================================================================

-- BEFORE INSERT triggers fire before each row is inserted
-- Can modify NEW values before insertion
-- Can prevent insertion by raising exception or returning NULL (PostgreSQL)
-- Use cases: validation, default values, data transformation, audit preparation

-- Create test database
CREATE DATABASE test_before_insert_row_db;
USE test_before_insert_row_db;

-- ============================================================================
-- Section 1: Basic BEFORE INSERT Trigger
-- ============================================================================

CREATE TABLE test_basic_insert (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    created_at TIMESTAMP
);

-- Trigger function to set created_at
CREATE FUNCTION set_created_at() RETURNS TRIGGER AS $$
BEGIN
    NEW.created_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_set_created_at
BEFORE INSERT ON test_basic_insert
FOR EACH ROW
EXECUTE FUNCTION set_created_at();

-- Test: created_at should be set automatically
INSERT INTO test_basic_insert (name) VALUES ('Alice'), ('Bob'), ('Charlie');

SELECT id, name, created_at FROM test_basic_insert ORDER BY id;

-- ============================================================================
-- Section 2: Data Validation Trigger
-- ============================================================================

CREATE TABLE test_validate_insert (
    id SERIAL PRIMARY KEY,
    username VARCHAR(50),
    email VARCHAR(200),
    age INT
);

CREATE FUNCTION validate_user_insert() RETURNS TRIGGER AS $$
BEGIN
    -- Validate username length
    IF LENGTH(NEW.username) < 3 THEN
        RAISE EXCEPTION 'Username must be at least 3 characters';
    END IF;

    -- Validate email format
    IF NEW.email NOT LIKE '%@%' THEN
        RAISE EXCEPTION 'Invalid email format';
    END IF;

    -- Validate age range
    IF NEW.age < 0 OR NEW.age > 150 THEN
        RAISE EXCEPTION 'Age must be between 0 and 150';
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_user
BEFORE INSERT ON test_validate_insert
FOR EACH ROW
EXECUTE FUNCTION validate_user_insert();

-- Valid insert
INSERT INTO test_validate_insert (username, email, age) VALUES ('alice', 'alice@example.com', 30);

-- Invalid inserts (should fail)
-- INSERT INTO test_validate_insert (username, email, age) VALUES ('ab', 'alice@example.com', 30);  -- username too short
-- INSERT INTO test_validate_insert (username, email, age) VALUES ('alice', 'invalid-email', 30);  -- invalid email
-- INSERT INTO test_validate_insert (username, email, age) VALUES ('alice', 'alice@example.com', 200);  -- invalid age

SELECT id, username, email, age FROM test_validate_insert;

-- ============================================================================
-- Section 3: Data Normalization Trigger
-- ============================================================================

CREATE TABLE test_normalize_insert (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    phone VARCHAR(20),
    zip_code VARCHAR(10)
);

CREATE FUNCTION normalize_data() RETURNS TRIGGER AS $$
BEGIN
    -- Lowercase email
    NEW.email = LOWER(TRIM(NEW.email));

    -- Remove non-digits from phone
    NEW.phone = regexp_replace(NEW.phone, '[^0-9]', '', 'g');

    -- Pad zip code
    IF LENGTH(NEW.zip_code) < 5 THEN
        NEW.zip_code = LPAD(NEW.zip_code, 5, '0');
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_normalize_data
BEFORE INSERT ON test_normalize_insert
FOR EACH ROW
EXECUTE FUNCTION normalize_data();

-- Test normalization
INSERT INTO test_normalize_insert (email, phone, zip_code) VALUES
    ('Alice@Example.COM  ', '(555) 123-4567', '123'),
    ('BOB@EXAMPLE.com', '555-234-5678', '12345');

SELECT id, email, phone, zip_code FROM test_normalize_insert ORDER BY id;

-- ============================================================================
-- Section 4: Default Value Trigger
-- ============================================================================

CREATE TABLE test_defaults_insert (
    id SERIAL PRIMARY KEY,
    user_id INT,
    status VARCHAR(20),
    priority INT,
    assigned_to VARCHAR(100)
);

CREATE FUNCTION set_insert_defaults() RETURNS TRIGGER AS $$
BEGIN
    -- Set default status if not provided
    IF NEW.status IS NULL THEN
        NEW.status = 'pending';
    END IF;

    -- Set default priority
    IF NEW.priority IS NULL THEN
        NEW.priority = 5;
    END IF;

    -- Set default assigned_to
    IF NEW.assigned_to IS NULL THEN
        NEW.assigned_to = 'unassigned';
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_set_defaults
BEFORE INSERT ON test_defaults_insert
FOR EACH ROW
EXECUTE FUNCTION set_insert_defaults();

-- Insert with some defaults
INSERT INTO test_defaults_insert (user_id) VALUES (100);
INSERT INTO test_defaults_insert (user_id, status) VALUES (101, 'active');
INSERT INTO test_defaults_insert (user_id, priority, assigned_to) VALUES (102, 9, 'alice');

SELECT id, user_id, status, priority, assigned_to FROM test_defaults_insert ORDER BY id;

-- ============================================================================
-- Section 5: Auto-Generate ID/Code Trigger
-- ============================================================================

CREATE TABLE test_autogen_insert (
    id SERIAL PRIMARY KEY,
    order_code VARCHAR(50),
    customer_id INT,
    order_date DATE
);

CREATE SEQUENCE order_code_seq START 1000;

CREATE FUNCTION generate_order_code() RETURNS TRIGGER AS $$
BEGIN
    -- Generate order code: ORD-YYYY-NNNN
    NEW.order_code = 'ORD-' ||
                     EXTRACT(YEAR FROM CURRENT_DATE)::TEXT || '-' ||
                     LPAD(nextval('order_code_seq')::TEXT, 4, '0');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_generate_order_code
BEFORE INSERT ON test_autogen_insert
FOR EACH ROW
EXECUTE FUNCTION generate_order_code();

INSERT INTO test_autogen_insert (customer_id, order_date) VALUES
    (1, '2024-01-15'),
    (2, '2024-01-16'),
    (1, '2024-01-17');

SELECT id, order_code, customer_id, order_date FROM test_autogen_insert ORDER BY id;

-- ============================================================================
-- Section 6: Conditional Trigger (WHEN Clause)
-- ============================================================================

CREATE TABLE test_conditional_insert (
    id SERIAL PRIMARY KEY,
    amount NUMERIC(10,2),
    discount_percent NUMERIC(5,2),
    final_amount NUMERIC(10,2)
);

CREATE FUNCTION calculate_discount() RETURNS TRIGGER AS $$
BEGIN
    -- Calculate final amount with discount
    NEW.final_amount = NEW.amount * (1 - NEW.discount_percent / 100);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Only fire trigger when discount is provided
CREATE TRIGGER trg_calculate_discount
BEFORE INSERT ON test_conditional_insert
FOR EACH ROW
WHEN (NEW.discount_percent IS NOT NULL AND NEW.discount_percent > 0)
EXECUTE FUNCTION calculate_discount();

INSERT INTO test_conditional_insert (amount, discount_percent) VALUES
    (100.00, 10),    -- Trigger fires
    (200.00, 0),     -- Trigger does not fire
    (150.00, NULL),  -- Trigger does not fire
    (300.00, 25);    -- Trigger fires

SELECT id, amount, discount_percent, final_amount FROM test_conditional_insert ORDER BY id;

-- ============================================================================
-- Section 7: Trigger Modifying Multiple Columns
-- ============================================================================

CREATE TABLE test_multicolumn_insert (
    id SERIAL PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    full_name VARCHAR(200),
    email VARCHAR(200),
    username VARCHAR(100)
);

CREATE FUNCTION set_derived_fields() RETURNS TRIGGER AS $$
BEGIN
    -- Generate full name
    NEW.full_name = NEW.first_name || ' ' || NEW.last_name;

    -- Generate username from email
    IF NEW.username IS NULL AND NEW.email IS NOT NULL THEN
        NEW.username = SPLIT_PART(NEW.email, '@', 1);
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_set_derived_fields
BEFORE INSERT ON test_multicolumn_insert
FOR EACH ROW
EXECUTE FUNCTION set_derived_fields();

INSERT INTO test_multicolumn_insert (first_name, last_name, email) VALUES
    ('Alice', 'Smith', 'alice.smith@example.com'),
    ('Bob', 'Jones', 'bob.jones@example.com');

SELECT id, first_name, last_name, full_name, username FROM test_multicolumn_insert ORDER BY id;

-- ============================================================================
-- Section 8: Trigger with Business Logic
-- ============================================================================

CREATE TABLE test_business_logic_insert (
    id SERIAL PRIMARY KEY,
    product_code VARCHAR(50),
    quantity INT,
    unit_price NUMERIC(10,2),
    subtotal NUMERIC(10,2),
    tax_rate NUMERIC(5,2) DEFAULT 8.5,
    tax_amount NUMERIC(10,2),
    total NUMERIC(10,2)
);

CREATE FUNCTION calculate_order_totals() RETURNS TRIGGER AS $$
BEGIN
    -- Calculate subtotal
    NEW.subtotal = NEW.quantity * NEW.unit_price;

    -- Calculate tax
    NEW.tax_amount = NEW.subtotal * (NEW.tax_rate / 100);

    -- Calculate total
    NEW.total = NEW.subtotal + NEW.tax_amount;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_calculate_totals
BEFORE INSERT ON test_business_logic_insert
FOR EACH ROW
EXECUTE FUNCTION calculate_order_totals();

INSERT INTO test_business_logic_insert (product_code, quantity, unit_price) VALUES
    ('PROD-001', 5, 19.99),
    ('PROD-002', 10, 9.99),
    ('PROD-003', 2, 49.99);

SELECT id, product_code, quantity, unit_price, subtotal, tax_amount, total
FROM test_business_logic_insert ORDER BY id;

-- ============================================================================
-- Section 9: Trigger Preventing Invalid Data
-- ============================================================================

CREATE TABLE test_prevent_invalid_insert (
    id SERIAL PRIMARY KEY,
    event_date DATE,
    event_name VARCHAR(200)
);

CREATE FUNCTION prevent_past_dates() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.event_date < CURRENT_DATE THEN
        RAISE EXCEPTION 'Cannot insert events in the past. Date: %', NEW.event_date;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_past_dates
BEFORE INSERT ON test_prevent_invalid_insert
FOR EACH ROW
EXECUTE FUNCTION prevent_past_dates();

-- Valid insert (future date)
INSERT INTO test_prevent_invalid_insert (event_date, event_name)
VALUES (CURRENT_DATE + INTERVAL '30 days', 'Future Event');

-- Invalid insert (past date) - should fail
-- INSERT INTO test_prevent_invalid_insert (event_date, event_name)
-- VALUES (CURRENT_DATE - INTERVAL '10 days', 'Past Event');

SELECT id, event_date, event_name FROM test_prevent_invalid_insert;

-- ============================================================================
-- Section 10: Trigger with External Table Lookup
-- ============================================================================

CREATE TABLE test_categories (
    category_id INT PRIMARY KEY,
    category_name VARCHAR(100),
    max_items INT
);

CREATE TABLE test_items_with_lookup (
    id SERIAL PRIMARY KEY,
    category_id INT,
    item_name VARCHAR(200),
    is_valid BOOLEAN
);

INSERT INTO test_categories VALUES (1, 'Electronics', 1000), (2, 'Books', 5000);

CREATE FUNCTION validate_category() RETURNS TRIGGER AS $$
DECLARE
    cat_exists BOOLEAN;
BEGIN
    -- Check if category exists
    SELECT EXISTS(SELECT 1 FROM test_categories WHERE category_id = NEW.category_id)
    INTO cat_exists;

    IF NOT cat_exists THEN
        RAISE EXCEPTION 'Invalid category_id: %', NEW.category_id;
    END IF;

    NEW.is_valid = TRUE;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_category
BEFORE INSERT ON test_items_with_lookup
FOR EACH ROW
EXECUTE FUNCTION validate_category();

-- Valid insert
INSERT INTO test_items_with_lookup (category_id, item_name) VALUES (1, 'Laptop');

-- Invalid insert - should fail
-- INSERT INTO test_items_with_lookup (category_id, item_name) VALUES (999, 'Invalid Item');

SELECT id, category_id, item_name, is_valid FROM test_items_with_lookup;

-- ============================================================================
-- Section 11: Trigger for Data Enrichment
-- ============================================================================

CREATE TABLE test_enrich_insert (
    id SERIAL PRIMARY KEY,
    ip_address INET,
    country_code VARCHAR(2),
    city VARCHAR(100),
    request_timestamp TIMESTAMP
);

CREATE FUNCTION enrich_ip_data() RETURNS TRIGGER AS $$
BEGIN
    -- Simulate IP geolocation enrichment
    IF host(NEW.ip_address) LIKE '192.168.%' THEN
        NEW.country_code = 'US';
        NEW.city = 'Local Network';
    ELSIF host(NEW.ip_address) LIKE '10.%' THEN
        NEW.country_code = 'US';
        NEW.city = 'Private Network';
    ELSE
        NEW.country_code = 'XX';
        NEW.city = 'Unknown';
    END IF;

    NEW.request_timestamp = CURRENT_TIMESTAMP;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_enrich_ip
BEFORE INSERT ON test_enrich_insert
FOR EACH ROW
EXECUTE FUNCTION enrich_ip_data();

INSERT INTO test_enrich_insert (ip_address) VALUES
    ('192.168.1.1'::INET),
    ('10.0.0.1'::INET),
    ('8.8.8.8'::INET);

SELECT id, ip_address, country_code, city FROM test_enrich_insert ORDER BY id;

-- ============================================================================
-- Section 12: Trigger with UUID Generation
-- ============================================================================

CREATE TABLE test_uuid_insert (
    id SERIAL PRIMARY KEY,
    record_uuid UUID,
    record_name VARCHAR(100)
);

CREATE FUNCTION generate_uuid() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.record_uuid IS NULL THEN
        NEW.record_uuid = gen_random_uuid();
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_generate_uuid
BEFORE INSERT ON test_uuid_insert
FOR EACH ROW
EXECUTE FUNCTION generate_uuid();

INSERT INTO test_uuid_insert (record_name) VALUES ('Record 1'), ('Record 2'), ('Record 3');

SELECT id, record_uuid, record_name FROM test_uuid_insert ORDER BY id;

-- ============================================================================
-- Section 13: Trigger with JSONB Manipulation
-- ============================================================================

CREATE TABLE test_jsonb_insert (
    id SERIAL PRIMARY KEY,
    user_data JSONB
);

CREATE FUNCTION enrich_jsonb() RETURNS TRIGGER AS $$
BEGIN
    -- Add created_at timestamp to JSONB
    NEW.user_data = NEW.user_data ||
                    jsonb_build_object('created_at', CURRENT_TIMESTAMP::TEXT);

    -- Add version field
    NEW.user_data = NEW.user_data ||
                    jsonb_build_object('version', 1);

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_enrich_jsonb
BEFORE INSERT ON test_jsonb_insert
FOR EACH ROW
EXECUTE FUNCTION enrich_jsonb();

INSERT INTO test_jsonb_insert (user_data) VALUES
    ('{"name": "Alice", "email": "alice@example.com"}'::JSONB),
    ('{"name": "Bob", "email": "bob@example.com"}'::JSONB);

SELECT id, user_data FROM test_jsonb_insert ORDER BY id;

-- ============================================================================
-- Section 14: Multiple BEFORE INSERT Triggers
-- ============================================================================

CREATE TABLE test_multiple_triggers (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    normalized_name VARCHAR(100),
    created_at TIMESTAMP,
    updated_at TIMESTAMP
);

CREATE FUNCTION normalize_name() RETURNS TRIGGER AS $$
BEGIN
    NEW.normalized_name = LOWER(TRIM(NEW.name));
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION set_timestamps() RETURNS TRIGGER AS $$
BEGIN
    NEW.created_at = CURRENT_TIMESTAMP;
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- First trigger (executes first alphabetically by default)
CREATE TRIGGER trg_a_normalize
BEFORE INSERT ON test_multiple_triggers
FOR EACH ROW
EXECUTE FUNCTION normalize_name();

-- Second trigger
CREATE TRIGGER trg_b_timestamps
BEFORE INSERT ON test_multiple_triggers
FOR EACH ROW
EXECUTE FUNCTION set_timestamps();

INSERT INTO test_multiple_triggers (name) VALUES ('  Alice SMITH  '), ('BOB jones');

SELECT id, name, normalized_name, created_at FROM test_multiple_triggers ORDER BY id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_before_insert_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_before_insert_best_practices VALUES
    (1, 'BEFORE INSERT triggers fire before row insertion'),
    (2, 'Can modify NEW record values before insertion'),
    (3, 'Use for data validation and prevent invalid data'),
    (4, 'Perfect for setting default values and timestamps'),
    (5, 'Great for data normalization and transformation'),
    (6, 'Can generate auto-codes, UUIDs, sequences'),
    (7, 'Use WHEN clause for conditional trigger execution'),
    (8, 'Return NEW to proceed with insert, NULL to skip'),
    (9, 'RAISE EXCEPTION to prevent insert and rollback'),
    (10, 'Multiple BEFORE triggers execute alphabetically by name'),
    (11, 'Keep trigger logic simple and fast'),
    (12, 'Avoid complex queries in triggers (performance)'),
    (13, 'Use for business rule enforcement'),
    (14, 'Document trigger behavior and purpose'),
    (15, 'Test trigger error handling thoroughly');

SELECT id, guideline FROM test_before_insert_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_before_insert_best_practices;
DROP TABLE test_multiple_triggers;
DROP FUNCTION normalize_name();
DROP FUNCTION set_timestamps();
DROP TABLE test_jsonb_insert;
DROP FUNCTION enrich_jsonb();
DROP TABLE test_uuid_insert;
DROP FUNCTION generate_uuid();
DROP TABLE test_enrich_insert;
DROP FUNCTION enrich_ip_data();
DROP TABLE test_items_with_lookup;
DROP TABLE test_categories;
DROP FUNCTION validate_category();
DROP TABLE test_prevent_invalid_insert;
DROP FUNCTION prevent_past_dates();
DROP TABLE test_business_logic_insert;
DROP FUNCTION calculate_order_totals();
DROP TABLE test_multicolumn_insert;
DROP FUNCTION set_derived_fields();
DROP TABLE test_conditional_insert;
DROP FUNCTION calculate_discount();
DROP TABLE test_autogen_insert;
DROP SEQUENCE order_code_seq;
DROP FUNCTION generate_order_code();
DROP TABLE test_defaults_insert;
DROP FUNCTION set_insert_defaults();
DROP TABLE test_normalize_insert;
DROP FUNCTION normalize_data();
DROP TABLE test_validate_insert;
DROP FUNCTION validate_user_insert();
DROP TABLE test_basic_insert;
DROP FUNCTION set_created_at();

DROP DATABASE test_before_insert_row_db;

-- End of BEFORE INSERT row-level trigger tests
