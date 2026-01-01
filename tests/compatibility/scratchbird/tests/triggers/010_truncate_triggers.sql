-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - TRUNCATE
-- Description: Comprehensive TRUNCATE trigger testing
-- ============================================================================

-- TRUNCATE triggers are statement-level only (no row-level)
-- Fire BEFORE or AFTER TRUNCATE
-- No access to OLD/NEW data (TRUNCATE removes all rows)
-- Use cases: audit logging, cascade truncate, archiving entire tables

-- Create test database
CREATE DATABASE test_truncate_triggers_db;
USE test_truncate_triggers_db;

-- ============================================================================
-- Section 1: Basic TRUNCATE Trigger
-- ============================================================================

CREATE TABLE test_truncate_basic (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_truncate_log (
    log_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    truncated_by VARCHAR(100),
    truncated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_truncate_basic (data)
SELECT 'Data ' || i FROM generate_series(1, 100) i;

CREATE FUNCTION log_truncate() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_truncate_log (table_name, truncated_by)
    VALUES (TG_TABLE_NAME, CURRENT_USER);
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_truncate
AFTER TRUNCATE ON test_truncate_basic
FOR EACH STATEMENT
EXECUTE FUNCTION log_truncate();

TRUNCATE test_truncate_basic;

SELECT COUNT(*) AS rows_remaining FROM test_truncate_basic;  -- Should be 0
SELECT log_id, table_name, truncated_by FROM test_truncate_log;

-- ============================================================================
-- Section 2: BEFORE TRUNCATE Validation
-- ============================================================================

CREATE TABLE test_prevent_truncate (
    id SERIAL PRIMARY KEY,
    critical_data TEXT,
    importance VARCHAR(20)
);

INSERT INTO test_prevent_truncate (critical_data, importance) VALUES
    ('Critical system data', 'critical'),
    ('Normal data', 'normal');

CREATE FUNCTION prevent_truncate_if_critical() RETURNS TRIGGER AS $$
DECLARE
    critical_count INT;
BEGIN
    -- Check if table has critical data
    SELECT COUNT(*) INTO critical_count
    FROM test_prevent_truncate
    WHERE importance = 'critical';

    IF critical_count > 0 THEN
        RAISE EXCEPTION 'Cannot truncate table with % critical records', critical_count;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_truncate
BEFORE TRUNCATE ON test_prevent_truncate
FOR EACH STATEMENT
EXECUTE FUNCTION prevent_truncate_if_critical();

-- Would fail (has critical data)
-- TRUNCATE test_prevent_truncate;

-- Remove critical data first
DELETE FROM test_prevent_truncate WHERE importance = 'critical';

-- Now truncate works
TRUNCATE test_prevent_truncate;

SELECT COUNT(*) FROM test_prevent_truncate;

-- ============================================================================
-- Section 3: Archive Before Truncate
-- ============================================================================

CREATE TABLE test_archive_truncate (
    id SERIAL PRIMARY KEY,
    record_data VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_truncate_archive (
    archive_id SERIAL PRIMARY KEY,
    archived_data JSONB,
    record_count INT,
    archived_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_archive_truncate (record_data)
SELECT 'Record ' || i FROM generate_series(1, 50) i;

CREATE FUNCTION archive_before_truncate() RETURNS TRIGGER AS $$
DECLARE
    archived_records JSONB;
    rec_count INT;
BEGIN
    -- Archive all data as JSONB
    SELECT jsonb_agg(row_to_json(t))
    INTO archived_records
    FROM test_archive_truncate t;

    SELECT COUNT(*) INTO rec_count FROM test_archive_truncate;

    INSERT INTO test_truncate_archive (archived_data, record_count)
    VALUES (archived_records, rec_count);

    RAISE NOTICE 'Archived % records before truncate', rec_count;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_archive_before_truncate
BEFORE TRUNCATE ON test_archive_truncate
FOR EACH STATEMENT
EXECUTE FUNCTION archive_before_truncate();

TRUNCATE test_archive_truncate;

SELECT archive_id, record_count FROM test_truncate_archive;
SELECT COUNT(*) FROM test_archive_truncate;  -- Should be 0

-- ============================================================================
-- Section 4: Cascade Truncate
-- ============================================================================

CREATE TABLE test_parent_truncate (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_child_truncate (
    child_id SERIAL PRIMARY KEY,
    parent_id INT,
    child_name VARCHAR(200)
);

INSERT INTO test_parent_truncate (parent_name) VALUES ('Parent 1'), ('Parent 2');
INSERT INTO test_child_truncate (parent_id, child_name) VALUES
    (1, 'Child 1A'), (1, 'Child 1B'),
    (2, 'Child 2A');

CREATE FUNCTION cascade_truncate_children() RETURNS TRIGGER AS $$
BEGIN
    -- Truncate child table when parent is truncated
    TRUNCATE test_child_truncate;
    RAISE NOTICE 'Cascaded truncate to child table';
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cascade_truncate
BEFORE TRUNCATE ON test_parent_truncate
FOR EACH STATEMENT
EXECUTE FUNCTION cascade_truncate_children();

TRUNCATE test_parent_truncate;

SELECT COUNT(*) AS parent_count FROM test_parent_truncate;
SELECT COUNT(*) AS child_count FROM test_child_truncate;

-- ============================================================================
-- Section 5: Audit Trail for TRUNCATE
-- ============================================================================

CREATE TABLE test_audit_truncate (
    id SERIAL PRIMARY KEY,
    sensitive_data TEXT
);

CREATE TABLE test_truncate_audit_log (
    audit_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    row_count_before INT,
    truncated_by VARCHAR(100),
    truncate_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    transaction_id BIGINT
);

INSERT INTO test_audit_truncate (sensitive_data)
SELECT 'Sensitive ' || i FROM generate_series(1, 100) i;

CREATE FUNCTION audit_truncate_operation() RETURNS TRIGGER AS $$
DECLARE
    pre_count INT;
BEGIN
    -- Count rows before truncate
    SELECT COUNT(*) INTO pre_count FROM test_audit_truncate;

    INSERT INTO test_truncate_audit_log (
        table_name, row_count_before, truncated_by, transaction_id
    ) VALUES (
        TG_TABLE_NAME, pre_count, CURRENT_USER, txid_current()
    );

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_truncate
BEFORE TRUNCATE ON test_audit_truncate
FOR EACH STATEMENT
EXECUTE FUNCTION audit_truncate_operation();

TRUNCATE test_audit_truncate;

SELECT audit_id, table_name, row_count_before, truncated_by FROM test_truncate_audit_log;

-- ============================================================================
-- Section 6: Statistics Reset on TRUNCATE
-- ============================================================================

CREATE TABLE test_stats_truncate (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_table_statistics (
    table_name VARCHAR(100) PRIMARY KEY,
    total_rows INT DEFAULT 0,
    sum_value BIGINT DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_table_statistics (table_name, total_rows, sum_value, last_updated)
VALUES ('test_stats_truncate', 100, 5050, CURRENT_TIMESTAMP);

INSERT INTO test_stats_truncate (value)
SELECT i FROM generate_series(1, 100) i;

CREATE FUNCTION reset_statistics_on_truncate() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_table_statistics
    SET total_rows = 0,
        sum_value = 0,
        last_updated = CURRENT_TIMESTAMP
    WHERE table_name = TG_TABLE_NAME;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_reset_stats
AFTER TRUNCATE ON test_stats_truncate
FOR EACH STATEMENT
EXECUTE FUNCTION reset_statistics_on_truncate();

TRUNCATE test_stats_truncate;

SELECT table_name, total_rows, sum_value FROM test_table_statistics;

-- ============================================================================
-- Section 7: Notification on TRUNCATE
-- ============================================================================

CREATE TABLE test_notify_truncate (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_truncate_notifications (
    notification_id SERIAL PRIMARY KEY,
    message TEXT,
    severity VARCHAR(20),
    notified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_notify_truncate (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

CREATE FUNCTION notify_on_truncate() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM test_notify_truncate;

    INSERT INTO test_truncate_notifications (message, severity)
    VALUES (
        'Table ' || TG_TABLE_NAME || ' truncated. ' || row_count || ' rows removed.',
        CASE WHEN row_count > 500 THEN 'HIGH' ELSE 'NORMAL' END
    );

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_notify_truncate
BEFORE TRUNCATE ON test_notify_truncate
FOR EACH STATEMENT
EXECUTE FUNCTION notify_on_truncate();

TRUNCATE test_notify_truncate;

SELECT notification_id, message, severity FROM test_truncate_notifications;

-- ============================================================================
-- Section 8: Sequence Reset on TRUNCATE
-- ============================================================================

CREATE TABLE test_sequence_reset (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_sequence_reset_log (
    log_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    sequence_reset BOOLEAN,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_sequence_reset (data)
SELECT 'Data ' || i FROM generate_series(1, 50) i;

-- Note: TRUNCATE with RESTART IDENTITY resets sequences

CREATE FUNCTION log_sequence_reset() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_sequence_reset_log (table_name, sequence_reset)
    VALUES (TG_TABLE_NAME, TRUE);
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_sequence_reset
AFTER TRUNCATE ON test_sequence_reset
FOR EACH STATEMENT
EXECUTE FUNCTION log_sequence_reset();

TRUNCATE test_sequence_reset RESTART IDENTITY;

-- Insert new record - should start from id=1
INSERT INTO test_sequence_reset (data) VALUES ('New Data');

SELECT id, data FROM test_sequence_reset;
SELECT table_name, sequence_reset FROM test_sequence_reset_log;

-- ============================================================================
-- Section 9: Multiple Table TRUNCATE
-- ============================================================================

CREATE TABLE test_multi_truncate_a (
    id SERIAL PRIMARY KEY,
    data_a VARCHAR(200)
);

CREATE TABLE test_multi_truncate_b (
    id SERIAL PRIMARY KEY,
    data_b VARCHAR(200)
);

CREATE TABLE test_multi_truncate_log (
    log_id SERIAL PRIMARY KEY,
    tables_truncated TEXT[],
    truncated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_multi_truncate_a (data_a)
SELECT 'Data A ' || i FROM generate_series(1, 25) i;

INSERT INTO test_multi_truncate_b (data_b)
SELECT 'Data B ' || i FROM generate_series(1, 25) i;

CREATE FUNCTION log_multi_truncate_a() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_multi_truncate_log (tables_truncated)
    VALUES (ARRAY[TG_TABLE_NAME]);
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_multi_truncate_b() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_multi_truncate_log (tables_truncated)
    VALUES (ARRAY[TG_TABLE_NAME]);
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_truncate_a
AFTER TRUNCATE ON test_multi_truncate_a
FOR EACH STATEMENT
EXECUTE FUNCTION log_multi_truncate_a();

CREATE TRIGGER trg_log_truncate_b
AFTER TRUNCATE ON test_multi_truncate_b
FOR EACH STATEMENT
EXECUTE FUNCTION log_multi_truncate_b();

-- Truncate multiple tables in one statement
TRUNCATE test_multi_truncate_a, test_multi_truncate_b;

SELECT log_id, tables_truncated FROM test_multi_truncate_log ORDER BY log_id;

-- ============================================================================
-- Section 10: Best Practices
-- ============================================================================

CREATE TABLE test_truncate_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_truncate_best_practices VALUES
    (1, 'TRUNCATE triggers are statement-level only (no row-level)'),
    (2, 'Fires BEFORE or AFTER entire table truncation'),
    (3, 'No access to OLD/NEW row data'),
    (4, 'Use for audit logging of truncate operations'),
    (5, 'Perfect for archiving entire table before truncate'),
    (6, 'Can prevent truncate with RAISE EXCEPTION in BEFORE trigger'),
    (7, 'Use for cascade truncate to related tables'),
    (8, 'TRUNCATE is faster than DELETE for removing all rows'),
    (9, 'TRUNCATE RESTART IDENTITY resets sequences'),
    (10, 'Cannot be used within functions that modify data'),
    (11, 'Use for resetting statistics and aggregates'),
    (12, 'Great for notification systems on bulk data removal'),
    (13, 'Document truncate policies and restrictions'),
    (14, 'Consider archiving before allowing truncate'),
    (15, 'TRUNCATE requires TRUNCATE privilege (not just DELETE)');

SELECT id, guideline FROM test_truncate_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_truncate_best_practices;
DROP TABLE test_multi_truncate_log;
DROP TABLE test_multi_truncate_b;
DROP TABLE test_multi_truncate_a;
DROP FUNCTION log_multi_truncate_b();
DROP FUNCTION log_multi_truncate_a();
DROP TABLE test_sequence_reset_log;
DROP TABLE test_sequence_reset;
DROP FUNCTION log_sequence_reset();
DROP TABLE test_truncate_notifications;
DROP TABLE test_notify_truncate;
DROP FUNCTION notify_on_truncate();
DROP TABLE test_table_statistics;
DROP TABLE test_stats_truncate;
DROP FUNCTION reset_statistics_on_truncate();
DROP TABLE test_truncate_audit_log;
DROP TABLE test_audit_truncate;
DROP FUNCTION audit_truncate_operation();
DROP TABLE test_child_truncate;
DROP TABLE test_parent_truncate;
DROP FUNCTION cascade_truncate_children();
DROP TABLE test_truncate_archive;
DROP TABLE test_archive_truncate;
DROP FUNCTION archive_before_truncate();
DROP TABLE test_prevent_truncate;
DROP FUNCTION prevent_truncate_if_critical();
DROP TABLE test_truncate_log;
DROP TABLE test_truncate_basic;
DROP FUNCTION log_truncate();

DROP DATABASE test_truncate_triggers_db;

-- End of TRUNCATE trigger tests
