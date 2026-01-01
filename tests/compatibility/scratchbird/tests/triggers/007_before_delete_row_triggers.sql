-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - BEFORE DELETE Row-Level
-- Description: Comprehensive BEFORE DELETE row-level trigger testing
-- ============================================================================

-- BEFORE DELETE triggers fire before each row is deleted
-- Can access OLD record values only (NEW doesn't exist)
-- Can prevent deletion by raising exception or returning NULL
-- Use cases: soft delete, validation, archiving, referential integrity checks

-- Create test database
CREATE DATABASE test_before_delete_row_db;
USE test_before_delete_row_db;

-- ============================================================================
-- Section 1: Soft Delete Implementation
-- ============================================================================

CREATE TABLE test_soft_delete (
    id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    data TEXT,
    is_deleted BOOLEAN DEFAULT FALSE,
    deleted_at TIMESTAMP
);

INSERT INTO test_soft_delete (name, data) VALUES
    ('Record 1', 'Data 1'),
    ('Record 2', 'Data 2'),
    ('Record 3', 'Data 3');

CREATE FUNCTION soft_delete_instead() RETURNS TRIGGER AS $$
BEGIN
    -- Instead of deleting, mark as deleted
    UPDATE test_soft_delete
    SET is_deleted = TRUE,
        deleted_at = CURRENT_TIMESTAMP
    WHERE id = OLD.id;

    -- Return NULL to skip the actual DELETE
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_soft_delete
BEFORE DELETE ON test_soft_delete
FOR EACH ROW
EXECUTE FUNCTION soft_delete_instead();

-- Attempt to delete - will soft delete instead
DELETE FROM test_soft_delete WHERE id = 1;

SELECT id, name, is_deleted, deleted_at FROM test_soft_delete ORDER BY id;

-- ============================================================================
-- Section 2: Prevent Deletion with Validation
-- ============================================================================

CREATE TABLE test_prevent_delete (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    importance VARCHAR(20),
    data VARCHAR(200)
);

INSERT INTO test_prevent_delete (status, importance, data) VALUES
    ('active', 'critical', 'Important data'),
    ('inactive', 'low', 'Normal data'),
    ('active', 'high', 'Priority data');

CREATE FUNCTION prevent_critical_delete() RETURNS TRIGGER AS $$
BEGIN
    IF OLD.importance = 'critical' THEN
        RAISE EXCEPTION 'Cannot delete critical record: id=%', OLD.id;
    END IF;

    IF OLD.status = 'active' THEN
        RAISE WARNING 'Deleting active record: id=%', OLD.id;
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_critical_delete
BEFORE DELETE ON test_prevent_delete
FOR EACH ROW
EXECUTE FUNCTION prevent_critical_delete();

-- Allowed delete
DELETE FROM test_prevent_delete WHERE id = 2;

-- Prevented delete (would fail)
-- DELETE FROM test_prevent_delete WHERE id = 1;

SELECT id, status, importance FROM test_prevent_delete ORDER BY id;

-- ============================================================================
-- Section 3: Archive Before Delete
-- ============================================================================

CREATE TABLE test_active_records (
    record_id SERIAL PRIMARY KEY,
    record_name VARCHAR(200),
    record_value INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_archived_records (
    archive_id SERIAL PRIMARY KEY,
    original_record_id INT,
    record_name VARCHAR(200),
    record_value INT,
    created_at TIMESTAMP,
    archived_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_active_records (record_name, record_value) VALUES
    ('Record A', 100),
    ('Record B', 200),
    ('Record C', 300);

CREATE FUNCTION archive_before_delete() RETURNS TRIGGER AS $$
BEGIN
    -- Archive the record before deletion
    INSERT INTO test_archived_records (
        original_record_id, record_name, record_value, created_at
    ) VALUES (
        OLD.record_id, OLD.record_name, OLD.record_value, OLD.created_at
    );

    -- Allow the delete to proceed
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_archive_before_delete
BEFORE DELETE ON test_active_records
FOR EACH ROW
EXECUTE FUNCTION archive_before_delete();

DELETE FROM test_active_records WHERE record_id = 1;

SELECT record_id, record_name FROM test_active_records ORDER BY record_id;
SELECT archive_id, original_record_id, record_name, archived_at
FROM test_archived_records ORDER BY archive_id;

-- ============================================================================
-- Section 4: Referential Integrity Check
-- ============================================================================

CREATE TABLE test_parent_records (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_dependent_records (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_parent_records(parent_id),
    child_name VARCHAR(200)
);

INSERT INTO test_parent_records (parent_name) VALUES ('Parent 1'), ('Parent 2');
INSERT INTO test_dependent_records (parent_id, child_name) VALUES
    (1, 'Child 1A'),
    (1, 'Child 1B');

CREATE FUNCTION check_dependents_before_delete() RETURNS TRIGGER AS $$
DECLARE
    dependent_count INT;
BEGIN
    -- Check for dependent records
    SELECT COUNT(*) INTO dependent_count
    FROM test_dependent_records
    WHERE parent_id = OLD.parent_id;

    IF dependent_count > 0 THEN
        RAISE EXCEPTION 'Cannot delete parent %: has % dependent records',
            OLD.parent_id, dependent_count;
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_dependents
BEFORE DELETE ON test_parent_records
FOR EACH ROW
EXECUTE FUNCTION check_dependents_before_delete();

-- Allowed delete (no dependents)
DELETE FROM test_parent_records WHERE parent_id = 2;

-- Prevented delete (would fail - has dependents)
-- DELETE FROM test_parent_records WHERE parent_id = 1;

SELECT parent_id, parent_name FROM test_parent_records ORDER BY parent_id;

-- ============================================================================
-- Section 5: Conditional Delete (WHEN Clause)
-- ============================================================================

CREATE TABLE test_conditional_delete (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    can_delete BOOLEAN DEFAULT TRUE,
    data VARCHAR(200)
);

INSERT INTO test_conditional_delete (status, can_delete, data) VALUES
    ('pending', TRUE, 'Data 1'),
    ('approved', FALSE, 'Data 2'),
    ('rejected', TRUE, 'Data 3');

CREATE FUNCTION validate_deletable() RETURNS TRIGGER AS $$
BEGIN
    RAISE NOTICE 'Validating deletion of record %', OLD.id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

-- Only fire trigger when record is not deletable
CREATE TRIGGER trg_validate_deletable
BEFORE DELETE ON test_conditional_delete
FOR EACH ROW
WHEN (OLD.can_delete = FALSE)
EXECUTE FUNCTION validate_deletable();

DELETE FROM test_conditional_delete WHERE id = 1;  -- Trigger does not fire
-- DELETE FROM test_conditional_delete WHERE id = 2;  -- Trigger fires

SELECT id, status, can_delete FROM test_conditional_delete ORDER BY id;

-- ============================================================================
-- Section 6: Log Deletion Attempt
-- ============================================================================

CREATE TABLE test_deletion_tracked (
    id SERIAL PRIMARY KEY,
    critical_data TEXT,
    owner VARCHAR(100)
);

CREATE TABLE test_deletion_log (
    log_id SERIAL PRIMARY KEY,
    deleted_id INT,
    deleted_data TEXT,
    deleted_by VARCHAR(100),
    deleted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_deletion_tracked (critical_data, owner) VALUES
    ('Critical Info 1', 'alice'),
    ('Critical Info 2', 'bob');

CREATE FUNCTION log_deletion_attempt() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_deletion_log (deleted_id, deleted_data, deleted_by)
    VALUES (OLD.id, OLD.critical_data, CURRENT_USER);

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_deletion
BEFORE DELETE ON test_deletion_tracked
FOR EACH ROW
EXECUTE FUNCTION log_deletion_attempt();

DELETE FROM test_deletion_tracked WHERE id = 1;

SELECT id, critical_data FROM test_deletion_tracked ORDER BY id;
SELECT log_id, deleted_id, deleted_data, deleted_by FROM test_deletion_log;

-- ============================================================================
-- Section 7: Cascade Soft Delete
-- ============================================================================

CREATE TABLE test_cascade_parent (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200),
    is_deleted BOOLEAN DEFAULT FALSE
);

CREATE TABLE test_cascade_children (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_cascade_parent(parent_id),
    child_name VARCHAR(200),
    is_deleted BOOLEAN DEFAULT FALSE
);

INSERT INTO test_cascade_parent (parent_name) VALUES ('Parent A');
INSERT INTO test_cascade_children (parent_id, child_name) VALUES
    (1, 'Child A1'),
    (1, 'Child A2');

CREATE FUNCTION cascade_soft_delete() RETURNS TRIGGER AS $$
BEGIN
    -- Soft delete all children
    UPDATE test_cascade_children
    SET is_deleted = TRUE
    WHERE parent_id = OLD.parent_id;

    -- Soft delete parent instead of hard delete
    UPDATE test_cascade_parent
    SET is_deleted = TRUE
    WHERE parent_id = OLD.parent_id;

    -- Prevent actual deletion
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cascade_soft_delete
BEFORE DELETE ON test_cascade_parent
FOR EACH ROW
EXECUTE FUNCTION cascade_soft_delete();

DELETE FROM test_cascade_parent WHERE parent_id = 1;

SELECT parent_id, parent_name, is_deleted FROM test_cascade_parent;
SELECT child_id, child_name, is_deleted FROM test_cascade_children ORDER BY child_id;

-- ============================================================================
-- Section 8: Business Rule Validation
-- ============================================================================

CREATE TABLE test_orders_deletable (
    order_id SERIAL PRIMARY KEY,
    order_status VARCHAR(50),
    order_total NUMERIC(12,2),
    customer_id INT
);

INSERT INTO test_orders_deletable (order_status, order_total, customer_id) VALUES
    ('pending', 100.00, 1),
    ('shipped', 200.00, 2),
    ('delivered', 300.00, 3);

CREATE FUNCTION validate_order_deletion() RETURNS TRIGGER AS $$
BEGIN
    -- Cannot delete shipped or delivered orders
    IF OLD.order_status IN ('shipped', 'delivered') THEN
        RAISE EXCEPTION 'Cannot delete order with status: %', OLD.order_status;
    END IF;

    -- Cannot delete high-value orders
    IF OLD.order_total > 500 THEN
        RAISE EXCEPTION 'Cannot delete high-value order: $%', OLD.order_total;
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_order_deletion
BEFORE DELETE ON test_orders_deletable
FOR EACH ROW
EXECUTE FUNCTION validate_order_deletion();

-- Allowed
DELETE FROM test_orders_deletable WHERE order_id = 1;

-- Prevented (would fail)
-- DELETE FROM test_orders_deletable WHERE order_id = 2;

SELECT order_id, order_status, order_total FROM test_orders_deletable ORDER BY order_id;

-- ============================================================================
-- Section 9: Update Related Statistics
-- ============================================================================

CREATE TABLE test_items_with_stats (
    item_id SERIAL PRIMARY KEY,
    category VARCHAR(100),
    item_value NUMERIC(10,2)
);

CREATE TABLE test_category_stats_delete (
    category VARCHAR(100) PRIMARY KEY,
    item_count INT DEFAULT 0,
    total_value NUMERIC(12,2) DEFAULT 0
);

INSERT INTO test_category_stats_delete (category, item_count, total_value) VALUES
    ('electronics', 3, 600.00);

INSERT INTO test_items_with_stats (category, item_value) VALUES
    ('electronics', 100.00),
    ('electronics', 200.00),
    ('electronics', 300.00);

CREATE FUNCTION update_stats_on_delete() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_category_stats_delete
    SET item_count = item_count - 1,
        total_value = total_value - OLD.item_value
    WHERE category = OLD.category;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_stats_delete
BEFORE DELETE ON test_items_with_stats
FOR EACH ROW
EXECUTE FUNCTION update_stats_on_delete();

DELETE FROM test_items_with_stats WHERE item_id = 1;

SELECT category, item_count, total_value FROM test_category_stats_delete;

-- ============================================================================
-- Section 10: Prevent Delete During Time Window
-- ============================================================================

CREATE TABLE test_time_restricted_delete (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_time_restricted_delete (data, created_at) VALUES
    ('Recent data', CURRENT_TIMESTAMP),
    ('Old data', CURRENT_TIMESTAMP - INTERVAL '10 days');

CREATE FUNCTION prevent_recent_delete() RETURNS TRIGGER AS $$
DECLARE
    age_hours INT;
BEGIN
    age_hours := EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - OLD.created_at)) / 3600;

    IF age_hours < 24 THEN
        RAISE EXCEPTION 'Cannot delete records less than 24 hours old. Age: % hours', age_hours;
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_recent_delete
BEFORE DELETE ON test_time_restricted_delete
FOR EACH ROW
EXECUTE FUNCTION prevent_recent_delete();

-- Would fail (too recent)
-- DELETE FROM test_time_restricted_delete WHERE id = 1;

-- Allowed (old enough)
DELETE FROM test_time_restricted_delete WHERE id = 2;

SELECT id, data, created_at FROM test_time_restricted_delete ORDER BY id;

-- ============================================================================
-- Section 11: Complex Validation Logic
-- ============================================================================

CREATE TABLE test_complex_validation (
    id SERIAL PRIMARY KEY,
    user_id INT,
    balance NUMERIC(12,2),
    has_active_subscriptions BOOLEAN,
    has_pending_orders BOOLEAN
);

INSERT INTO test_complex_validation (user_id, balance, has_active_subscriptions, has_pending_orders) VALUES
    (1, 100.00, TRUE, FALSE),
    (2, 0.00, FALSE, FALSE),
    (3, 50.00, FALSE, TRUE);

CREATE FUNCTION validate_account_deletion() RETURNS TRIGGER AS $$
BEGIN
    -- Cannot delete if positive balance
    IF OLD.balance > 0 THEN
        RAISE EXCEPTION 'Cannot delete account with positive balance: $%', OLD.balance;
    END IF;

    -- Cannot delete if active subscriptions
    IF OLD.has_active_subscriptions THEN
        RAISE EXCEPTION 'Cannot delete account with active subscriptions';
    END IF;

    -- Cannot delete if pending orders
    IF OLD.has_pending_orders THEN
        RAISE EXCEPTION 'Cannot delete account with pending orders';
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_account_deletion
BEFORE DELETE ON test_complex_validation
FOR EACH ROW
EXECUTE FUNCTION validate_account_deletion();

-- Allowed
DELETE FROM test_complex_validation WHERE id = 2;

-- Prevented (would fail)
-- DELETE FROM test_complex_validation WHERE id = 1;  -- has balance
-- DELETE FROM test_complex_validation WHERE id = 3;  -- has pending orders

SELECT id, balance, has_active_subscriptions, has_pending_orders FROM test_complex_validation ORDER BY id;

-- ============================================================================
-- Section 12: Move to History Table
-- ============================================================================

CREATE TABLE test_current_data (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    status VARCHAR(50),
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_historical_data (
    history_id SERIAL PRIMARY KEY,
    original_id INT,
    data VARCHAR(200),
    status VARCHAR(50),
    active_until TIMESTAMP
);

INSERT INTO test_current_data (data, status) VALUES
    ('Current Record 1', 'active'),
    ('Current Record 2', 'inactive');

CREATE FUNCTION move_to_history() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_historical_data (original_id, data, status, active_until)
    VALUES (OLD.id, OLD.data, OLD.status, CURRENT_TIMESTAMP);

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_move_to_history
BEFORE DELETE ON test_current_data
FOR EACH ROW
EXECUTE FUNCTION move_to_history();

DELETE FROM test_current_data WHERE id = 1;

SELECT id, data, status FROM test_current_data ORDER BY id;
SELECT history_id, original_id, data, active_until FROM test_historical_data;

-- ============================================================================
-- Section 13: Deletion Quota/Rate Limiting
-- ============================================================================

CREATE TABLE test_rate_limited_deletes (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_deletion_quota (
    quota_date DATE PRIMARY KEY,
    deletions_today INT DEFAULT 0,
    max_deletions INT DEFAULT 10
);

INSERT INTO test_rate_limited_deletes (data)
SELECT 'Data ' || i FROM generate_series(1, 20) i;

INSERT INTO test_deletion_quota (quota_date, deletions_today, max_deletions)
VALUES (CURRENT_DATE, 0, 10);

CREATE FUNCTION check_deletion_quota() RETURNS TRIGGER AS $$
DECLARE
    current_count INT;
    max_allowed INT;
BEGIN
    SELECT deletions_today, max_deletions
    INTO current_count, max_allowed
    FROM test_deletion_quota
    WHERE quota_date = CURRENT_DATE;

    IF current_count >= max_allowed THEN
        RAISE EXCEPTION 'Daily deletion quota exceeded: % / %', current_count, max_allowed;
    END IF;

    -- Increment counter
    UPDATE test_deletion_quota
    SET deletions_today = deletions_today + 1
    WHERE quota_date = CURRENT_DATE;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_deletion_quota
BEFORE DELETE ON test_rate_limited_deletes
FOR EACH ROW
EXECUTE FUNCTION check_deletion_quota();

-- Delete within quota
DELETE FROM test_rate_limited_deletes WHERE id <= 5;

SELECT quota_date, deletions_today, max_deletions FROM test_deletion_quota;

-- ============================================================================
-- Section 14: Multiple BEFORE DELETE Triggers
-- ============================================================================

CREATE TABLE test_multi_before_delete (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_delete_log_a (
    log_id SERIAL PRIMARY KEY,
    message TEXT
);

CREATE TABLE test_delete_log_b (
    log_id SERIAL PRIMARY KEY,
    message TEXT
);

INSERT INTO test_multi_before_delete (data) VALUES ('Data 1');

CREATE FUNCTION log_delete_a() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_delete_log_a (message)
    VALUES ('Before delete A: ' || OLD.data);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_delete_b() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_delete_log_b (message)
    VALUES ('Before delete B: ' || OLD.data);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_delete_a
BEFORE DELETE ON test_multi_before_delete
FOR EACH ROW
EXECUTE FUNCTION log_delete_a();

CREATE TRIGGER trg_delete_b
BEFORE DELETE ON test_multi_before_delete
FOR EACH ROW
EXECUTE FUNCTION log_delete_b();

DELETE FROM test_multi_before_delete WHERE id = 1;

SELECT log_id, message FROM test_delete_log_a;
SELECT log_id, message FROM test_delete_log_b;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_before_delete_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_before_delete_best_practices VALUES
    (1, 'BEFORE DELETE triggers fire before row deletion'),
    (2, 'Can access OLD record values only (NEW does not exist)'),
    (3, 'Return NULL to prevent deletion, OLD to allow'),
    (4, 'Perfect for implementing soft delete patterns'),
    (5, 'Use for archiving data before permanent deletion'),
    (6, 'Great for referential integrity validation'),
    (7, 'Use WHEN clause for conditional execution'),
    (8, 'RAISE EXCEPTION to prevent deletion with message'),
    (9, 'Can update statistics before row is removed'),
    (10, 'Multiple triggers execute alphabetically by name'),
    (11, 'Keep trigger logic fast to avoid slowing deletes'),
    (12, 'Use for business rule enforcement'),
    (13, 'Perfect for audit trail creation'),
    (14, 'Can implement deletion quotas and rate limiting'),
    (15, 'Document deletion policies and restrictions');

SELECT id, guideline FROM test_before_delete_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_before_delete_best_practices;
DROP TABLE test_delete_log_b;
DROP TABLE test_delete_log_a;
DROP TABLE test_multi_before_delete;
DROP FUNCTION log_delete_b();
DROP FUNCTION log_delete_a();
DROP TABLE test_deletion_quota;
DROP TABLE test_rate_limited_deletes;
DROP FUNCTION check_deletion_quota();
DROP TABLE test_historical_data;
DROP TABLE test_current_data;
DROP FUNCTION move_to_history();
DROP TABLE test_complex_validation;
DROP FUNCTION validate_account_deletion();
DROP TABLE test_time_restricted_delete;
DROP FUNCTION prevent_recent_delete();
DROP TABLE test_category_stats_delete;
DROP TABLE test_items_with_stats;
DROP FUNCTION update_stats_on_delete();
DROP TABLE test_orders_deletable;
DROP FUNCTION validate_order_deletion();
DROP TABLE test_cascade_children;
DROP TABLE test_cascade_parent;
DROP FUNCTION cascade_soft_delete();
DROP TABLE test_deletion_log;
DROP TABLE test_deletion_tracked;
DROP FUNCTION log_deletion_attempt();
DROP TABLE test_conditional_delete;
DROP FUNCTION validate_deletable();
DROP TABLE test_dependent_records;
DROP TABLE test_parent_records;
DROP FUNCTION check_dependents_before_delete();
DROP TABLE test_archived_records;
DROP TABLE test_active_records;
DROP FUNCTION archive_before_delete();
DROP TABLE test_prevent_delete;
DROP FUNCTION prevent_critical_delete();
DROP TABLE test_soft_delete;
DROP FUNCTION soft_delete_instead();

DROP DATABASE test_before_delete_row_db;

-- End of BEFORE DELETE row-level trigger tests
