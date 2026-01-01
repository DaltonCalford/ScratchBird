-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - BEFORE UPDATE Row-Level
-- Description: Comprehensive BEFORE UPDATE row-level trigger testing
-- ============================================================================

-- BEFORE UPDATE triggers fire before each row is updated
-- Can access and modify OLD and NEW values
-- Can prevent update by raising exception or returning NULL
-- Use cases: validation, change tracking, audit preparation, data transformation

-- Create test database
CREATE DATABASE test_before_update_row_db;
USE test_before_update_row_db;

-- ============================================================================
-- Section 1: Basic BEFORE UPDATE Trigger (Updated Timestamp)
-- ============================================================================

CREATE TABLE test_basic_update (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    value INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP
);

CREATE FUNCTION set_updated_timestamp() RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_set_updated_at
BEFORE UPDATE ON test_basic_update
FOR EACH ROW
EXECUTE FUNCTION set_updated_timestamp();

INSERT INTO test_basic_update (name, value) VALUES ('Item 1', 10), ('Item 2', 20);

-- Update triggers updated_at
UPDATE test_basic_update SET value = 15 WHERE id = 1;

SELECT id, name, value, updated_at FROM test_basic_update ORDER BY id;

-- ============================================================================
-- Section 2: Validation on Update
-- ============================================================================

CREATE TABLE test_validate_update (
    id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock_quantity INT
);

INSERT INTO test_validate_update (product_name, price, stock_quantity) VALUES
    ('Product A', 19.99, 100),
    ('Product B', 29.99, 50);

CREATE FUNCTION validate_update() RETURNS TRIGGER AS $$
BEGIN
    -- Prevent negative price
    IF NEW.price < 0 THEN
        RAISE EXCEPTION 'Price cannot be negative: %', NEW.price;
    END IF;

    -- Prevent negative stock
    IF NEW.stock_quantity < 0 THEN
        RAISE EXCEPTION 'Stock quantity cannot be negative: %', NEW.stock_quantity;
    END IF;

    -- Warn if large price change
    IF ABS(NEW.price - OLD.price) > (OLD.price * 0.5) THEN
        RAISE WARNING 'Large price change detected: % -> %', OLD.price, NEW.price;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_update
BEFORE UPDATE ON test_validate_update
FOR EACH ROW
EXECUTE FUNCTION validate_update();

-- Valid update
UPDATE test_validate_update SET price = 24.99 WHERE id = 1;

-- Invalid update (would fail)
-- UPDATE test_validate_update SET price = -10.00 WHERE id = 1;
-- UPDATE test_validate_update SET stock_quantity = -5 WHERE id = 2;

SELECT id, product_name, price, stock_quantity FROM test_validate_update ORDER BY id;

-- ============================================================================
-- Section 3: Prevent Updates to Specific Columns
-- ============================================================================

CREATE TABLE test_immutable_columns (
    id SERIAL PRIMARY KEY,
    order_number VARCHAR(50),
    customer_id INT,
    order_total NUMERIC(12,2),
    status VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_immutable_columns (order_number, customer_id, order_total, status) VALUES
    ('ORD-001', 100, 199.99, 'pending'),
    ('ORD-002', 101, 299.99, 'pending');

CREATE FUNCTION prevent_immutable_updates() RETURNS TRIGGER AS $$
BEGIN
    -- Prevent changes to order_number
    IF NEW.order_number IS DISTINCT FROM OLD.order_number THEN
        RAISE EXCEPTION 'Cannot modify order_number';
    END IF;

    -- Prevent changes to created_at
    IF NEW.created_at IS DISTINCT FROM OLD.created_at THEN
        RAISE EXCEPTION 'Cannot modify created_at timestamp';
    END IF;

    -- Allow status changes
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_immutable
BEFORE UPDATE ON test_immutable_columns
FOR EACH ROW
EXECUTE FUNCTION prevent_immutable_updates();

-- Valid update
UPDATE test_immutable_columns SET status = 'processing' WHERE id = 1;

-- Invalid update (would fail)
-- UPDATE test_immutable_columns SET order_number = 'ORD-999' WHERE id = 1;

SELECT id, order_number, status FROM test_immutable_columns ORDER BY id;

-- ============================================================================
-- Section 4: Track Changed Columns
-- ============================================================================

CREATE TABLE test_track_changes (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(200),
    phone VARCHAR(20),
    address TEXT,
    changed_fields TEXT[]
);

INSERT INTO test_track_changes (name, email, phone, address) VALUES
    ('Alice Smith', 'alice@example.com', '555-1234', '123 Main St'),
    ('Bob Jones', 'bob@example.com', '555-5678', '456 Oak Ave');

CREATE FUNCTION track_changed_fields() RETURNS TRIGGER AS $$
DECLARE
    changes TEXT[] := ARRAY[]::TEXT[];
BEGIN
    IF NEW.name IS DISTINCT FROM OLD.name THEN
        changes := array_append(changes, 'name');
    END IF;

    IF NEW.email IS DISTINCT FROM OLD.email THEN
        changes := array_append(changes, 'email');
    END IF;

    IF NEW.phone IS DISTINCT FROM OLD.phone THEN
        changes := array_append(changes, 'phone');
    END IF;

    IF NEW.address IS DISTINCT FROM OLD.address THEN
        changes := array_append(changes, 'address');
    END IF;

    NEW.changed_fields := changes;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_changes
BEFORE UPDATE ON test_track_changes
FOR EACH ROW
EXECUTE FUNCTION track_changed_fields();

UPDATE test_track_changes SET email = 'alice.smith@example.com', phone = '555-9999' WHERE id = 1;

SELECT id, name, email, phone, changed_fields FROM test_track_changes ORDER BY id;

-- ============================================================================
-- Section 5: Conditional Update Trigger (WHEN Clause)
-- ============================================================================

CREATE TABLE test_conditional_update (
    id SERIAL PRIMARY KEY,
    price NUMERIC(10,2),
    discount_price NUMERIC(10,2),
    is_on_sale BOOLEAN DEFAULT FALSE
);

INSERT INTO test_conditional_update (price, is_on_sale) VALUES
    (100.00, FALSE),
    (200.00, TRUE),
    (150.00, FALSE);

CREATE FUNCTION apply_discount() RETURNS TRIGGER AS $$
BEGIN
    -- Calculate 20% discount
    NEW.discount_price = NEW.price * 0.80;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Only apply discount when is_on_sale changes to TRUE
CREATE TRIGGER trg_apply_discount
BEFORE UPDATE ON test_conditional_update
FOR EACH ROW
WHEN (NEW.is_on_sale = TRUE AND OLD.is_on_sale = FALSE)
EXECUTE FUNCTION apply_discount();

UPDATE test_conditional_update SET is_on_sale = TRUE WHERE id = 1;

SELECT id, price, discount_price, is_on_sale FROM test_conditional_update ORDER BY id;

-- ============================================================================
-- Section 6: Version Control / Optimistic Locking
-- ============================================================================

CREATE TABLE test_versioning (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    version INT DEFAULT 1,
    updated_at TIMESTAMP
);

INSERT INTO test_versioning (data) VALUES ('Data 1'), ('Data 2');

CREATE FUNCTION increment_version() RETURNS TRIGGER AS $$
BEGIN
    -- Increment version number
    NEW.version = OLD.version + 1;
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_increment_version
BEFORE UPDATE ON test_versioning
FOR EACH ROW
EXECUTE FUNCTION increment_version();

UPDATE test_versioning SET data = 'Data 1 Updated' WHERE id = 1;
UPDATE test_versioning SET data = 'Data 1 Updated Again' WHERE id = 1;

SELECT id, data, version, updated_at FROM test_versioning ORDER BY id;

-- ============================================================================
-- Section 7: Calculate Derived Values
-- ============================================================================

CREATE TABLE test_derived_update (
    id SERIAL PRIMARY KEY,
    first_name VARCHAR(100),
    last_name VARCHAR(100),
    full_name VARCHAR(200),
    quantity INT,
    unit_price NUMERIC(10,2),
    total_price NUMERIC(12,2)
);

INSERT INTO test_derived_update (first_name, last_name, quantity, unit_price) VALUES
    ('Alice', 'Smith', 5, 19.99),
    ('Bob', 'Jones', 10, 9.99);

CREATE FUNCTION calculate_derived() RETURNS TRIGGER AS $$
BEGIN
    -- Update full name if first or last name changed
    IF NEW.first_name IS DISTINCT FROM OLD.first_name OR
       NEW.last_name IS DISTINCT FROM OLD.last_name THEN
        NEW.full_name = NEW.first_name || ' ' || NEW.last_name;
    END IF;

    -- Calculate total if quantity or price changed
    IF NEW.quantity IS DISTINCT FROM OLD.quantity OR
       NEW.unit_price IS DISTINCT FROM OLD.unit_price THEN
        NEW.total_price = NEW.quantity * NEW.unit_price;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_calculate_derived
BEFORE UPDATE ON test_derived_update
FOR EACH ROW
EXECUTE FUNCTION calculate_derived();

UPDATE test_derived_update SET first_name = 'Alicia' WHERE id = 1;
UPDATE test_derived_update SET quantity = 15 WHERE id = 2;

SELECT id, full_name, quantity, unit_price, total_price FROM test_derived_update ORDER BY id;

-- ============================================================================
-- Section 8: Status Transition Validation
-- ============================================================================

CREATE TABLE test_status_transitions (
    id SERIAL PRIMARY KEY,
    order_id VARCHAR(50),
    status VARCHAR(20),
    previous_status VARCHAR(20)
);

INSERT INTO test_status_transitions (order_id, status) VALUES
    ('ORD-001', 'pending'),
    ('ORD-002', 'pending');

CREATE FUNCTION validate_status_transition() RETURNS TRIGGER AS $$
BEGIN
    -- Store previous status
    NEW.previous_status = OLD.status;

    -- Validate state transitions
    IF OLD.status = 'pending' AND NEW.status NOT IN ('processing', 'cancelled') THEN
        RAISE EXCEPTION 'Invalid transition from pending to %', NEW.status;
    END IF;

    IF OLD.status = 'processing' AND NEW.status NOT IN ('shipped', 'cancelled') THEN
        RAISE EXCEPTION 'Invalid transition from processing to %', NEW.status;
    END IF;

    IF OLD.status = 'shipped' AND NEW.status != 'delivered' THEN
        RAISE EXCEPTION 'Invalid transition from shipped to %', NEW.status;
    END IF;

    IF OLD.status IN ('delivered', 'cancelled') THEN
        RAISE EXCEPTION 'Cannot change status from %', OLD.status;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_status
BEFORE UPDATE ON test_status_transitions
FOR EACH ROW
WHEN (NEW.status IS DISTINCT FROM OLD.status)
EXECUTE FUNCTION validate_status_transition();

-- Valid transitions
UPDATE test_status_transitions SET status = 'processing' WHERE id = 1;
UPDATE test_status_transitions SET status = 'shipped' WHERE id = 1;

-- Invalid transition (would fail)
-- UPDATE test_status_transitions SET status = 'delivered' WHERE id = 2;  -- pending -> delivered invalid

SELECT id, order_id, status, previous_status FROM test_status_transitions ORDER BY id;

-- ============================================================================
-- Section 9: Data Normalization on Update
-- ============================================================================

CREATE TABLE test_normalize_update (
    id SERIAL PRIMARY KEY,
    email VARCHAR(200),
    phone VARCHAR(20),
    zip_code VARCHAR(10)
);

INSERT INTO test_normalize_update (email, phone, zip_code) VALUES
    ('alice@example.com', '5551234567', '12345'),
    ('BOB@EXAMPLE.COM', '(555) 987-6543', '678');

CREATE FUNCTION normalize_on_update() RETURNS TRIGGER AS $$
BEGIN
    -- Normalize email to lowercase
    IF NEW.email IS DISTINCT FROM OLD.email THEN
        NEW.email = LOWER(TRIM(NEW.email));
    END IF;

    -- Remove non-digits from phone
    IF NEW.phone IS DISTINCT FROM OLD.phone THEN
        NEW.phone = regexp_replace(NEW.phone, '[^0-9]', '', 'g');
    END IF;

    -- Pad zip code
    IF NEW.zip_code IS DISTINCT FROM OLD.zip_code THEN
        IF LENGTH(NEW.zip_code) < 5 THEN
            NEW.zip_code = LPAD(NEW.zip_code, 5, '0');
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_normalize_update
BEFORE UPDATE ON test_normalize_update
FOR EACH ROW
EXECUTE FUNCTION normalize_on_update();

UPDATE test_normalize_update SET email = '  ALICE.SMITH@EXAMPLE.COM  ' WHERE id = 1;
UPDATE test_normalize_update SET phone = '555-111-2222', zip_code = '99' WHERE id = 2;

SELECT id, email, phone, zip_code FROM test_normalize_update ORDER BY id;

-- ============================================================================
-- Section 10: Prevent Specific Updates
-- ============================================================================

CREATE TABLE test_prevent_updates (
    id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    balance NUMERIC(10,2),
    account_type VARCHAR(20)
);

INSERT INTO test_prevent_updates (username, balance, account_type) VALUES
    ('alice', 1000.00, 'savings'),
    ('bob', 500.00, 'checking');

CREATE FUNCTION prevent_balance_decrease() RETURNS TRIGGER AS $$
BEGIN
    -- Prevent decreasing balance directly (use transactions)
    IF NEW.balance < OLD.balance THEN
        RAISE EXCEPTION 'Cannot decrease balance directly. Balance: % -> %',
            OLD.balance, NEW.balance;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_balance_decrease
BEFORE UPDATE ON test_prevent_updates
FOR EACH ROW
WHEN (NEW.balance IS DISTINCT FROM OLD.balance)
EXECUTE FUNCTION prevent_balance_decrease();

-- Valid update (increase)
UPDATE test_prevent_updates SET balance = 1100.00 WHERE id = 1;

-- Invalid update (would fail - decrease)
-- UPDATE test_prevent_updates SET balance = 900.00 WHERE id = 1;

SELECT id, username, balance FROM test_prevent_updates ORDER BY id;

-- ============================================================================
-- Section 11: Cascading Updates to Related Data
-- ============================================================================

CREATE TABLE test_parent_update (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(100),
    status VARCHAR(20)
);

CREATE TABLE test_child_cascade (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_parent_update(parent_id),
    child_name VARCHAR(100),
    parent_status VARCHAR(20)
);

INSERT INTO test_parent_update (parent_name, status) VALUES ('Parent 1', 'active');
INSERT INTO test_child_cascade (parent_id, child_name, parent_status) VALUES (1, 'Child 1', 'active');
INSERT INTO test_child_cascade (parent_id, child_name, parent_status) VALUES (1, 'Child 2', 'active');

CREATE FUNCTION cascade_parent_status() RETURNS TRIGGER AS $$
BEGIN
    -- Update children when parent status changes
    IF NEW.status IS DISTINCT FROM OLD.status THEN
        UPDATE test_child_cascade
        SET parent_status = NEW.status
        WHERE parent_id = NEW.parent_id;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cascade_status
BEFORE UPDATE ON test_parent_update
FOR EACH ROW
WHEN (NEW.status IS DISTINCT FROM OLD.status)
EXECUTE FUNCTION cascade_parent_status();

UPDATE test_parent_update SET status = 'inactive' WHERE parent_id = 1;

SELECT parent_id, parent_name, status FROM test_parent_update;
SELECT child_id, child_name, parent_status FROM test_child_cascade ORDER BY child_id;

-- ============================================================================
-- Section 12: Audit Preparation (Store Old Values)
-- ============================================================================

CREATE TABLE test_audit_prep (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    old_data VARCHAR(200),
    changed_at TIMESTAMP
);

INSERT INTO test_audit_prep (data) VALUES ('Original Data 1'), ('Original Data 2');

CREATE FUNCTION prepare_audit_data() RETURNS TRIGGER AS $$
BEGIN
    -- Store old value before update
    NEW.old_data = OLD.data;
    NEW.changed_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prepare_audit
BEFORE UPDATE ON test_audit_prep
FOR EACH ROW
WHEN (NEW.data IS DISTINCT FROM OLD.data)
EXECUTE FUNCTION prepare_audit_data();

UPDATE test_audit_prep SET data = 'Updated Data 1' WHERE id = 1;

SELECT id, data, old_data, changed_at FROM test_audit_prep ORDER BY id;

-- ============================================================================
-- Section 13: Recalculate Aggregates
-- ============================================================================

CREATE TABLE test_recalc_update (
    id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT,
    unit_price NUMERIC(10,2),
    subtotal NUMERIC(12,2),
    tax_rate NUMERIC(5,2) DEFAULT 8.5,
    tax_amount NUMERIC(10,2),
    total NUMERIC(12,2)
);

INSERT INTO test_recalc_update (item_name, quantity, unit_price) VALUES
    ('Item A', 5, 10.00),
    ('Item B', 10, 15.00);

CREATE FUNCTION recalculate_totals() RETURNS TRIGGER AS $$
BEGIN
    -- Recalculate if quantity or price changed
    IF NEW.quantity IS DISTINCT FROM OLD.quantity OR
       NEW.unit_price IS DISTINCT FROM OLD.unit_price THEN
        NEW.subtotal = NEW.quantity * NEW.unit_price;
        NEW.tax_amount = NEW.subtotal * (NEW.tax_rate / 100);
        NEW.total = NEW.subtotal + NEW.tax_amount;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_recalculate
BEFORE UPDATE ON test_recalc_update
FOR EACH ROW
EXECUTE FUNCTION recalculate_totals();

UPDATE test_recalc_update SET quantity = 8 WHERE id = 1;
UPDATE test_recalc_update SET unit_price = 20.00 WHERE id = 2;

SELECT id, item_name, quantity, unit_price, subtotal, tax_amount, total FROM test_recalc_update ORDER BY id;

-- ============================================================================
-- Section 14: Multiple BEFORE UPDATE Triggers
-- ============================================================================

CREATE TABLE test_multiple_before_update (
    id SERIAL PRIMARY KEY,
    value INT,
    doubled_value INT,
    updated_at TIMESTAMP
);

INSERT INTO test_multiple_before_update (value) VALUES (10), (20);

CREATE FUNCTION double_value() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.value IS DISTINCT FROM OLD.value THEN
        NEW.doubled_value = NEW.value * 2;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION set_update_time() RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_a_double
BEFORE UPDATE ON test_multiple_before_update
FOR EACH ROW
EXECUTE FUNCTION double_value();

CREATE TRIGGER trg_b_timestamp
BEFORE UPDATE ON test_multiple_before_update
FOR EACH ROW
EXECUTE FUNCTION set_update_time();

UPDATE test_multiple_before_update SET value = 25 WHERE id = 1;

SELECT id, value, doubled_value, updated_at FROM test_multiple_before_update ORDER BY id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_before_update_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_before_update_best_practices VALUES
    (1, 'BEFORE UPDATE triggers fire before row update'),
    (2, 'Can access both OLD and NEW record values'),
    (3, 'Can modify NEW values before update commits'),
    (4, 'Use for data validation and prevent invalid updates'),
    (5, 'Perfect for automatic timestamp updates (updated_at)'),
    (6, 'Great for versioning and optimistic locking'),
    (7, 'Use to prevent updates to immutable columns'),
    (8, 'Ideal for state transition validation'),
    (9, 'Use WHEN clause to fire only when specific columns change'),
    (10, 'Return NEW to proceed, NULL to skip, or RAISE EXCEPTION'),
    (11, 'Use IS DISTINCT FROM for NULL-safe comparisons'),
    (12, 'Keep trigger logic efficient (fires for each row)'),
    (13, 'Multiple triggers execute alphabetically by name'),
    (14, 'Track changed columns for selective audit logging'),
    (15, 'Document business rules enforced by triggers');

SELECT id, guideline FROM test_before_update_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_before_update_best_practices;
DROP TABLE test_multiple_before_update;
DROP FUNCTION set_update_time();
DROP FUNCTION double_value();
DROP TABLE test_recalc_update;
DROP FUNCTION recalculate_totals();
DROP TABLE test_audit_prep;
DROP FUNCTION prepare_audit_data();
DROP TABLE test_child_cascade;
DROP TABLE test_parent_update;
DROP FUNCTION cascade_parent_status();
DROP TABLE test_prevent_updates;
DROP FUNCTION prevent_balance_decrease();
DROP TABLE test_normalize_update;
DROP FUNCTION normalize_on_update();
DROP TABLE test_status_transitions;
DROP FUNCTION validate_status_transition();
DROP TABLE test_derived_update;
DROP FUNCTION calculate_derived();
DROP TABLE test_versioning;
DROP FUNCTION increment_version();
DROP TABLE test_conditional_update;
DROP FUNCTION apply_discount();
DROP TABLE test_track_changes;
DROP FUNCTION track_changed_fields();
DROP TABLE test_immutable_columns;
DROP FUNCTION prevent_immutable_updates();
DROP TABLE test_validate_update;
DROP FUNCTION validate_update();
DROP TABLE test_basic_update;
DROP FUNCTION set_updated_timestamp();

DROP DATABASE test_before_update_row_db;

-- End of BEFORE UPDATE row-level trigger tests
