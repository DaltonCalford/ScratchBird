-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - DELETE Statement-Level
-- Description: BEFORE/AFTER DELETE statement-level trigger testing
-- ============================================================================

-- Statement-level DELETE triggers fire once per DELETE statement
-- Access to OLD transition table only
-- Use cases: bulk audit logging, batch validation, deletion quotas

-- Create test database
CREATE DATABASE test_delete_statement_db;
USE test_delete_statement_db;

-- ============================================================================
-- Section 1: Basic AFTER DELETE Statement Trigger
-- ============================================================================

CREATE TABLE test_stmt_delete (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    category VARCHAR(50)
);

CREATE TABLE test_delete_stmt_log (
    log_id SERIAL PRIMARY KEY,
    rows_deleted INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_stmt_delete (data, category)
SELECT 'Data ' || i, 'cat_' || (i % 5) FROM generate_series(1, 100) i;

CREATE FUNCTION log_delete_statement() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM old_table;

    INSERT INTO test_delete_stmt_log (rows_deleted)
    VALUES (row_count);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_delete_stmt
AFTER DELETE ON test_stmt_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_delete_statement();

DELETE FROM test_stmt_delete WHERE category = 'cat_1';

SELECT log_id, rows_deleted FROM test_delete_stmt_log;

-- ============================================================================
-- Section 2: Bulk Deletion Audit
-- ============================================================================

CREATE TABLE test_bulk_delete_data (
    id SERIAL PRIMARY KEY,
    record_type VARCHAR(50),
    importance VARCHAR(20),
    data TEXT
);

CREATE TABLE test_bulk_delete_audit (
    audit_id SERIAL PRIMARY KEY,
    total_deleted INT,
    critical_deleted INT,
    deleted_types JSONB,
    audit_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_bulk_delete_data (record_type, importance, data)
SELECT
    CASE (i % 3) WHEN 0 THEN 'financial' WHEN 1 THEN 'system' ELSE 'user' END,
    CASE (i % 4) WHEN 0 THEN 'critical' WHEN 1 THEN 'high' ELSE 'normal' END,
    'Data ' || i
FROM generate_series(1, 100) i;

CREATE FUNCTION audit_bulk_delete() RETURNS TRIGGER AS $$
DECLARE
    total_rows INT;
    critical_count INT;
    types_json JSONB;
BEGIN
    SELECT COUNT(*) INTO total_rows FROM old_table;

    SELECT COUNT(*) INTO critical_count
    FROM old_table WHERE importance = 'critical';

    SELECT jsonb_object_agg(record_type, type_count)
    INTO types_json
    FROM (
        SELECT record_type, COUNT(*) as type_count
        FROM old_table
        GROUP BY record_type
    ) t;

    INSERT INTO test_bulk_delete_audit (total_deleted, critical_deleted, deleted_types)
    VALUES (total_rows, critical_count, types_json);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_bulk_delete
AFTER DELETE ON test_bulk_delete_data
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION audit_bulk_delete();

DELETE FROM test_bulk_delete_data WHERE importance = 'normal';

SELECT audit_id, total_deleted, critical_deleted, deleted_types FROM test_bulk_delete_audit;

-- ============================================================================
-- Section 3: BEFORE DELETE Validation
-- ============================================================================

CREATE TABLE test_validate_bulk_delete (
    id SERIAL PRIMARY KEY,
    protected BOOLEAN DEFAULT FALSE,
    data VARCHAR(200)
);

INSERT INTO test_validate_bulk_delete (protected, data)
SELECT (i % 10 = 0), 'Data ' || i FROM generate_series(1, 100) i;

CREATE FUNCTION validate_bulk_delete() RETURNS TRIGGER AS $$
DECLARE
    protected_count INT;
BEGIN
    SELECT COUNT(*) INTO protected_count
    FROM old_table WHERE protected = TRUE;

    IF protected_count > 0 THEN
        RAISE EXCEPTION 'Bulk delete validation failed: % protected records in deletion set',
            protected_count;
    END IF;

    RAISE NOTICE 'Bulk delete validated: % rows', (SELECT COUNT(*) FROM old_table);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_bulk_delete
BEFORE DELETE ON test_validate_bulk_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION validate_bulk_delete();

-- Valid delete (no protected records)
DELETE FROM test_validate_bulk_delete WHERE id BETWEEN 1 AND 5;

-- Invalid delete (would fail - contains protected records)
-- DELETE FROM test_validate_bulk_delete WHERE id BETWEEN 1 AND 20;

SELECT COUNT(*) AS remaining FROM test_validate_bulk_delete;

-- ============================================================================
-- Section 4: Deletion Quota Enforcement
-- ============================================================================

CREATE TABLE test_quota_deletes (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_deletion_quotas (
    quota_date DATE PRIMARY KEY,
    deletions_today INT DEFAULT 0,
    max_daily_deletions INT DEFAULT 100
);

INSERT INTO test_quota_deletes (data)
SELECT 'Data ' || i FROM generate_series(1, 200) i;

INSERT INTO test_deletion_quotas (quota_date, deletions_today, max_daily_deletions)
VALUES (CURRENT_DATE, 0, 100);

CREATE FUNCTION enforce_deletion_quota() RETURNS TRIGGER AS $$
DECLARE
    delete_count INT;
    current_total INT;
    max_allowed INT;
BEGIN
    SELECT COUNT(*) INTO delete_count FROM old_table;

    SELECT deletions_today, max_daily_deletions
    INTO current_total, max_allowed
    FROM test_deletion_quotas
    WHERE quota_date = CURRENT_DATE;

    IF (current_total + delete_count) > max_allowed THEN
        RAISE EXCEPTION 'Daily deletion quota would be exceeded: % + % > %',
            current_total, delete_count, max_allowed;
    END IF;

    -- Update quota
    UPDATE test_deletion_quotas
    SET deletions_today = deletions_today + delete_count
    WHERE quota_date = CURRENT_DATE;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_enforce_quota
BEFORE DELETE ON test_quota_deletes
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION enforce_deletion_quota();

-- Within quota
DELETE FROM test_quota_deletes WHERE id <= 50;

-- Would exceed quota
-- DELETE FROM test_quota_deletes WHERE id <= 150;

SELECT quota_date, deletions_today, max_daily_deletions FROM test_deletion_quotas;

-- ============================================================================
-- Section 5: Aggregate Statistics Update
-- ============================================================================

CREATE TABLE test_metrics_delete (
    metric_id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    value NUMERIC(10,2)
);

CREATE TABLE test_category_metrics (
    category VARCHAR(50) PRIMARY KEY,
    total_value NUMERIC(15,2) DEFAULT 0,
    record_count INT DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_category_metrics (category, total_value, record_count, last_updated)
SELECT 'cat_' || i, 0, 0, CURRENT_TIMESTAMP
FROM generate_series(1, 5) i;

INSERT INTO test_metrics_delete (category, value)
SELECT 'cat_' || (i % 5), (random() * 100)::NUMERIC(10,2)
FROM generate_series(1, 100) i;

-- Initialize counts
UPDATE test_category_metrics c
SET total_value = (SELECT SUM(value) FROM test_metrics_delete WHERE category = c.category),
    record_count = (SELECT COUNT(*) FROM test_metrics_delete WHERE category = c.category);

CREATE FUNCTION update_metrics_on_delete() RETURNS TRIGGER AS $$
BEGIN
    -- Update affected categories
    UPDATE test_category_metrics m
    SET total_value = m.total_value - COALESCE((
            SELECT SUM(value) FROM old_table WHERE category = m.category
        ), 0),
        record_count = m.record_count - (
            SELECT COUNT(*) FROM old_table WHERE category = m.category
        ),
        last_updated = CURRENT_TIMESTAMP
    WHERE m.category IN (SELECT DISTINCT category FROM old_table);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_metrics
AFTER DELETE ON test_metrics_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION update_metrics_on_delete();

DELETE FROM test_metrics_delete WHERE category = 'cat_0';

SELECT category, record_count, total_value FROM test_category_metrics ORDER BY category LIMIT 3;

-- ============================================================================
-- Section 6: Batch Deletion Summary
-- ============================================================================

CREATE TABLE test_batch_delete (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_deletion_summary (
    summary_id SERIAL PRIMARY KEY,
    total_deleted INT,
    avg_age_days NUMERIC(10,2),
    oldest_record TIMESTAMP,
    newest_record TIMESTAMP,
    summary_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_batch_delete (status, created_at)
SELECT
    CASE (i % 3) WHEN 0 THEN 'old' ELSE 'active' END,
    CURRENT_TIMESTAMP - (i || ' days')::INTERVAL
FROM generate_series(1, 100) i;

CREATE FUNCTION summarize_deletion() RETURNS TRIGGER AS $$
DECLARE
    del_count INT;
    avg_age NUMERIC(10,2);
    oldest TIMESTAMP;
    newest TIMESTAMP;
BEGIN
    SELECT COUNT(*),
           AVG(EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - created_at)) / 86400),
           MIN(created_at),
           MAX(created_at)
    INTO del_count, avg_age, oldest, newest
    FROM old_table;

    INSERT INTO test_deletion_summary (
        total_deleted, avg_age_days, oldest_record, newest_record
    ) VALUES (del_count, avg_age, oldest, newest);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_summarize_deletion
AFTER DELETE ON test_batch_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION summarize_deletion();

DELETE FROM test_batch_delete WHERE status = 'old';

SELECT summary_id, total_deleted, avg_age_days FROM test_deletion_summary;

-- ============================================================================
-- Section 7: Compliance Logging
-- ============================================================================

CREATE TABLE test_regulated_data (
    record_id SERIAL PRIMARY KEY,
    data_classification VARCHAR(50),
    sensitive_data TEXT
);

CREATE TABLE test_compliance_log (
    log_id SERIAL PRIMARY KEY,
    operation VARCHAR(50),
    affected_records INT,
    data_classifications JSONB,
    compliance_officer VARCHAR(100),
    log_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_regulated_data (data_classification, sensitive_data)
SELECT
    CASE (i % 3) WHEN 0 THEN 'public' WHEN 1 THEN 'internal' ELSE 'confidential' END,
    'Sensitive data ' || i
FROM generate_series(1, 50) i;

CREATE FUNCTION log_compliance_deletion() RETURNS TRIGGER AS $$
DECLARE
    rec_count INT;
    classifications JSONB;
BEGIN
    SELECT COUNT(*) INTO rec_count FROM old_table;

    SELECT jsonb_object_agg(data_classification, class_count)
    INTO classifications
    FROM (
        SELECT data_classification, COUNT(*) as class_count
        FROM old_table
        GROUP BY data_classification
    ) t;

    INSERT INTO test_compliance_log (
        operation, affected_records, data_classifications, compliance_officer
    ) VALUES ('BULK_DELETE', rec_count, classifications, CURRENT_USER);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_compliance
AFTER DELETE ON test_regulated_data
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_compliance_deletion();

DELETE FROM test_regulated_data WHERE data_classification = 'public';

SELECT log_id, operation, affected_records, data_classifications FROM test_compliance_log;

-- ============================================================================
-- Section 8: Performance Tracking
-- ============================================================================

CREATE TABLE test_perf_delete (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_delete_performance (
    perf_id SERIAL PRIMARY KEY,
    rows_deleted INT,
    execution_time_ms NUMERIC(10,2),
    recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_perf_delete (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

CREATE FUNCTION track_delete_perf_before() RETURNS TRIGGER AS $$
BEGIN
    CREATE TEMP TABLE IF NOT EXISTS temp_delete_start (start_time TIMESTAMP);
    DELETE FROM temp_delete_start;
    INSERT INTO temp_delete_start VALUES (clock_timestamp());
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION track_delete_perf_after() RETURNS TRIGGER AS $$
DECLARE
    start_ts TIMESTAMP;
    duration_ms NUMERIC(10,2);
    row_count INT;
BEGIN
    SELECT start_time INTO start_ts FROM temp_delete_start;
    duration_ms := EXTRACT(MILLISECOND FROM (clock_timestamp() - start_ts));

    SELECT COUNT(*) INTO row_count FROM old_table;

    INSERT INTO test_delete_performance (rows_deleted, execution_time_ms)
    VALUES (row_count, duration_ms);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_delete_perf_before
BEFORE DELETE ON test_perf_delete
FOR EACH STATEMENT
EXECUTE FUNCTION track_delete_perf_before();

CREATE TRIGGER trg_delete_perf_after
AFTER DELETE ON test_perf_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_delete_perf_after();

DELETE FROM test_perf_delete WHERE id <= 500;

SELECT perf_id, rows_deleted, execution_time_ms FROM test_delete_performance;

-- ============================================================================
-- Section 9: Archive Before Bulk Delete
-- ============================================================================

CREATE TABLE test_bulk_archive_source (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    status VARCHAR(50)
);

CREATE TABLE test_bulk_archive_dest (
    archive_id SERIAL PRIMARY KEY,
    original_id INT,
    data VARCHAR(200),
    status VARCHAR(50),
    archived_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_bulk_archive_source (data, status)
SELECT 'Data ' || i, CASE WHEN i % 3 = 0 THEN 'old' ELSE 'active' END
FROM generate_series(1, 100) i;

CREATE FUNCTION archive_bulk_delete() RETURNS TRIGGER AS $$
BEGIN
    -- Archive all deleted records
    INSERT INTO test_bulk_archive_dest (original_id, data, status)
    SELECT id, data, status FROM old_table;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_archive_bulk
BEFORE DELETE ON test_bulk_archive_source
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION archive_bulk_delete();

DELETE FROM test_bulk_archive_source WHERE status = 'old';

SELECT COUNT(*) AS archived FROM test_bulk_archive_dest;
SELECT COUNT(*) AS remaining FROM test_bulk_archive_source;

-- ============================================================================
-- Section 10: Notification on Bulk Deletion
-- ============================================================================

CREATE TABLE test_notify_delete (
    id SERIAL PRIMARY KEY,
    importance VARCHAR(20),
    data TEXT
);

CREATE TABLE test_bulk_delete_notifications (
    notification_id SERIAL PRIMARY KEY,
    total_deleted INT,
    critical_deleted INT,
    notification_sent_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_notify_delete (importance, data)
SELECT
    CASE (i % 5) WHEN 0 THEN 'critical' WHEN 1 THEN 'high' ELSE 'normal' END,
    'Data ' || i
FROM generate_series(1, 100) i;

CREATE FUNCTION notify_bulk_deletion() RETURNS TRIGGER AS $$
DECLARE
    total_count INT;
    critical_count INT;
BEGIN
    SELECT COUNT(*) INTO total_count FROM old_table;

    SELECT COUNT(*) INTO critical_count
    FROM old_table WHERE importance = 'critical';

    IF critical_count > 0 OR total_count > 50 THEN
        INSERT INTO test_bulk_delete_notifications (total_deleted, critical_deleted)
        VALUES (total_count, critical_count);

        RAISE WARNING 'Bulk deletion notification: % total, % critical',
            total_count, critical_count;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_notify_bulk_deletion
AFTER DELETE ON test_notify_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION notify_bulk_deletion();

DELETE FROM test_notify_delete WHERE importance IN ('critical', 'high');

SELECT notification_id, total_deleted, critical_deleted FROM test_bulk_delete_notifications;

-- ============================================================================
-- Section 11: Transaction Tracking
-- ============================================================================

CREATE TABLE test_txn_delete (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_delete_transactions (
    txn_log_id SERIAL PRIMARY KEY,
    txn_id BIGINT,
    rows_deleted INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_txn_delete (data)
SELECT 'Data ' || i FROM generate_series(1, 100) i;

CREATE FUNCTION track_delete_transaction() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM old_table;

    INSERT INTO test_delete_transactions (txn_id, rows_deleted)
    VALUES (txid_current(), row_count);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_delete_txn
AFTER DELETE ON test_txn_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_delete_transaction();

BEGIN;
DELETE FROM test_txn_delete WHERE id <= 25;
DELETE FROM test_txn_delete WHERE id > 75;
COMMIT;

SELECT txn_log_id, txn_id, rows_deleted FROM test_delete_transactions ORDER BY txn_log_id;

-- ============================================================================
-- Section 12: Data Quality Monitoring
-- ============================================================================

CREATE TABLE test_quality_delete (
    id SERIAL PRIMARY KEY,
    value INT,
    quality_flag BOOLEAN
);

CREATE TABLE test_deletion_quality_report (
    report_id SERIAL PRIMARY KEY,
    total_deleted INT,
    low_quality_deleted INT,
    avg_value NUMERIC(10,2),
    report_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_quality_delete (value, quality_flag)
SELECT i, (i % 3 != 0) FROM generate_series(1, 100) i;

CREATE FUNCTION monitor_deletion_quality() RETURNS TRIGGER AS $$
DECLARE
    total_rows INT;
    low_quality INT;
    avg_val NUMERIC(10,2);
BEGIN
    SELECT COUNT(*) INTO total_rows FROM old_table;

    SELECT COUNT(*) INTO low_quality
    FROM old_table WHERE quality_flag = FALSE;

    SELECT AVG(value) INTO avg_val FROM old_table;

    INSERT INTO test_deletion_quality_report (
        total_deleted, low_quality_deleted, avg_value
    ) VALUES (total_rows, low_quality, avg_val);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_monitor_deletion_quality
AFTER DELETE ON test_quality_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION monitor_deletion_quality();

DELETE FROM test_quality_delete WHERE quality_flag = FALSE;

SELECT report_id, total_deleted, low_quality_deleted, avg_value
FROM test_deletion_quality_report;

-- ============================================================================
-- Section 13: Cascade Cleanup Tracking
-- ============================================================================

CREATE TABLE test_cascade_tracking (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_cascade_cleanup_log (
    cleanup_id SERIAL PRIMARY KEY,
    primary_deletes INT,
    cascaded_operations INT,
    cleanup_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_cascade_tracking (data)
SELECT 'Data ' || i FROM generate_series(1, 50) i;

CREATE FUNCTION track_cascade_cleanup() RETURNS TRIGGER AS $$
DECLARE
    primary_count INT;
BEGIN
    SELECT COUNT(*) INTO primary_count FROM old_table;

    -- Simulate cascade operations (would actually happen in related tables)
    INSERT INTO test_cascade_cleanup_log (primary_deletes, cascaded_operations)
    VALUES (primary_count, primary_count * 3);  -- Example: 3 related records per primary

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_cascade
AFTER DELETE ON test_cascade_tracking
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_cascade_cleanup();

DELETE FROM test_cascade_tracking WHERE id <= 20;

SELECT cleanup_id, primary_deletes, cascaded_operations FROM test_cascade_cleanup_log;

-- ============================================================================
-- Section 14: Rate Limiting
-- ============================================================================

CREATE TABLE test_rate_limit_delete (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_delete_rate_log (
    rate_log_id SERIAL PRIMARY KEY,
    delete_size INT,
    threshold INT,
    action VARCHAR(50),
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_rate_limit_delete (data)
SELECT 'Data ' || i FROM generate_series(1, 200) i;

CREATE FUNCTION check_delete_rate_limit() RETURNS TRIGGER AS $$
DECLARE
    delete_count INT;
    max_batch INT;
BEGIN
    SELECT COUNT(*) INTO delete_count FROM old_table;
    max_batch := 100;

    INSERT INTO test_delete_rate_log (delete_size, threshold, action)
    VALUES (delete_count, max_batch,
            CASE WHEN delete_count > max_batch THEN 'REJECTED' ELSE 'ALLOWED' END);

    IF delete_count > max_batch THEN
        RAISE EXCEPTION 'Bulk delete exceeds rate limit: % rows (max: %)',
            delete_count, max_batch;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_delete_rate
BEFORE DELETE ON test_rate_limit_delete
REFERENCING OLD TABLE AS old_table
FOR EACH STATEMENT
EXECUTE FUNCTION check_delete_rate_limit();

-- Allowed
DELETE FROM test_rate_limit_delete WHERE id <= 50;

-- Would be rejected
-- DELETE FROM test_rate_limit_delete WHERE id <= 150;

SELECT rate_log_id, delete_size, threshold, action FROM test_delete_rate_log;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_delete_stmt_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_delete_stmt_best_practices VALUES
    (1, 'Statement triggers fire once per DELETE statement'),
    (2, 'Use REFERENCING OLD TABLE AS to access deleted rows'),
    (3, 'No access to individual row values'),
    (4, 'Perfect for bulk deletion auditing and logging'),
    (5, 'More efficient than row triggers for large deletes'),
    (6, 'Use for deletion quota enforcement'),
    (7, 'Great for batch archiving before deletion'),
    (8, 'Ideal for aggregate statistics updates'),
    (9, 'Can validate entire deletion batch'),
    (10, 'Return value is ignored (return NULL)'),
    (11, 'Use for compliance and regulatory logging'),
    (12, 'Keep logic efficient (processes all deleted rows)'),
    (13, 'Use for performance monitoring of bulk deletes'),
    (14, 'Great for notification systems'),
    (15, 'Document batch processing behavior and limits');

SELECT id, guideline FROM test_delete_stmt_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_delete_stmt_best_practices;
DROP TABLE test_delete_rate_log;
DROP TABLE test_rate_limit_delete;
DROP FUNCTION check_delete_rate_limit();
DROP TABLE test_cascade_cleanup_log;
DROP TABLE test_cascade_tracking;
DROP FUNCTION track_cascade_cleanup();
DROP TABLE test_deletion_quality_report;
DROP TABLE test_quality_delete;
DROP FUNCTION monitor_deletion_quality();
DROP TABLE test_delete_transactions;
DROP TABLE test_txn_delete;
DROP FUNCTION track_delete_transaction();
DROP TABLE test_bulk_delete_notifications;
DROP TABLE test_notify_delete;
DROP FUNCTION notify_bulk_deletion();
DROP TABLE test_bulk_archive_dest;
DROP TABLE test_bulk_archive_source;
DROP FUNCTION archive_bulk_delete();
DROP TABLE test_delete_performance;
DROP TABLE test_perf_delete;
DROP FUNCTION track_delete_perf_after();
DROP FUNCTION track_delete_perf_before();
DROP TABLE test_compliance_log;
DROP TABLE test_regulated_data;
DROP FUNCTION log_compliance_deletion();
DROP TABLE test_deletion_summary;
DROP TABLE test_batch_delete;
DROP FUNCTION summarize_deletion();
DROP TABLE test_category_metrics;
DROP TABLE test_metrics_delete;
DROP FUNCTION update_metrics_on_delete();
DROP TABLE test_deletion_quotas;
DROP TABLE test_quota_deletes;
DROP FUNCTION enforce_deletion_quota();
DROP TABLE test_validate_bulk_delete;
DROP FUNCTION validate_bulk_delete();
DROP TABLE test_bulk_delete_audit;
DROP TABLE test_bulk_delete_data;
DROP FUNCTION audit_bulk_delete();
DROP TABLE test_delete_stmt_log;
DROP TABLE test_stmt_delete;
DROP FUNCTION log_delete_statement();

DROP DATABASE test_delete_statement_db;

-- End of DELETE statement-level trigger tests
