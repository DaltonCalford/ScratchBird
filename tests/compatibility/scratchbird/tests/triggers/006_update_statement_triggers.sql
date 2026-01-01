-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - UPDATE Statement-Level
-- Description: BEFORE/AFTER UPDATE statement-level trigger testing
-- ============================================================================

-- Statement-level UPDATE triggers fire once per UPDATE statement
-- Access to OLD and NEW transition tables
-- Use cases: bulk validation, batch auditing, statement logging

-- Create test database
CREATE DATABASE test_update_statement_db;
USE test_update_statement_db;

-- ============================================================================
-- Section 1: Basic AFTER UPDATE Statement Trigger
-- ============================================================================

CREATE TABLE test_statement_update (
    id SERIAL PRIMARY KEY,
    value INT,
    category VARCHAR(50)
);

CREATE TABLE test_update_statement_log (
    log_id SERIAL PRIMARY KEY,
    rows_affected INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_statement_update (value, category)
SELECT i, 'cat_' || (i % 5) FROM generate_series(1, 100) i;

CREATE FUNCTION log_update_statement() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM new_table;

    INSERT INTO test_update_statement_log (rows_affected)
    VALUES (row_count);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_update_stmt
AFTER UPDATE ON test_statement_update
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_update_statement();

UPDATE test_statement_update SET value = value * 2 WHERE category = 'cat_1';

SELECT log_id, rows_affected FROM test_update_statement_log;

-- ============================================================================
-- Section 2: Bulk Update Audit with Transition Tables
-- ============================================================================

CREATE TABLE test_bulk_update_data (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    priority INT
);

CREATE TABLE test_bulk_update_audit (
    audit_id SERIAL PRIMARY KEY,
    total_updated INT,
    status_changes INT,
    priority_changes INT,
    audit_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_bulk_update_data (status, priority)
SELECT
    CASE (i % 3) WHEN 0 THEN 'pending' WHEN 1 THEN 'active' ELSE 'completed' END,
    (i % 5) + 1
FROM generate_series(1, 100) i;

CREATE FUNCTION audit_bulk_update() RETURNS TRIGGER AS $$
DECLARE
    total_rows INT;
    status_changed INT;
    priority_changed INT;
BEGIN
    SELECT COUNT(*) INTO total_rows FROM new_table;

    -- Count status changes
    SELECT COUNT(*)
    INTO status_changed
    FROM new_table n
    JOIN old_table o ON n.id = o.id
    WHERE n.status IS DISTINCT FROM o.status;

    -- Count priority changes
    SELECT COUNT(*)
    INTO priority_changed
    FROM new_table n
    JOIN old_table o ON n.id = o.id
    WHERE n.priority IS DISTINCT FROM o.priority;

    INSERT INTO test_bulk_update_audit (total_updated, status_changes, priority_changes)
    VALUES (total_rows, status_changed, priority_changed);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_bulk_update
AFTER UPDATE ON test_bulk_update_data
REFERENCING OLD TABLE AS old_table NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION audit_bulk_update();

UPDATE test_bulk_update_data SET status = 'completed' WHERE priority >= 4;

SELECT audit_id, total_updated, status_changes, priority_changes FROM test_bulk_update_audit;

-- ============================================================================
-- Section 3: BEFORE UPDATE Statement Validation
-- ============================================================================

CREATE TABLE test_validate_bulk_update (
    id SERIAL PRIMARY KEY,
    value INT,
    max_allowed INT DEFAULT 1000
);

INSERT INTO test_validate_bulk_update (value, max_allowed)
SELECT i, 1000 FROM generate_series(1, 50) i;

CREATE FUNCTION validate_bulk_update() RETURNS TRIGGER AS $$
DECLARE
    max_new_value INT;
    invalid_count INT;
BEGIN
    -- Find maximum new value
    SELECT MAX(value) INTO max_new_value FROM new_table;

    -- Count invalid updates
    SELECT COUNT(*)
    INTO invalid_count
    FROM new_table
    WHERE value > max_allowed;

    IF invalid_count > 0 THEN
        RAISE EXCEPTION 'Bulk update validation failed: % rows exceed max_allowed', invalid_count;
    END IF;

    RAISE NOTICE 'Bulk update validated: % rows, max value: %',
        (SELECT COUNT(*) FROM new_table), max_new_value;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_bulk
BEFORE UPDATE ON test_validate_bulk_update
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION validate_bulk_update();

-- Valid update
UPDATE test_validate_bulk_update SET value = 500 WHERE id <= 10;

-- Invalid update (would fail)
-- UPDATE test_validate_bulk_update SET value = 2000 WHERE id <= 10;

SELECT id, value FROM test_validate_bulk_update WHERE id <= 10 ORDER BY id;

-- ============================================================================
-- Section 4: Batch Change Summary
-- ============================================================================

CREATE TABLE test_prices (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2)
);

CREATE TABLE test_price_update_summary (
    summary_id SERIAL PRIMARY KEY,
    update_count INT,
    avg_old_price NUMERIC(10,2),
    avg_new_price NUMERIC(10,2),
    total_price_change NUMERIC(12,2),
    summary_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_prices (product_name, price)
SELECT 'Product ' || i, (random() * 100)::NUMERIC(10,2)
FROM generate_series(1, 100) i;

CREATE FUNCTION summarize_price_updates() RETURNS TRIGGER AS $$
DECLARE
    upd_count INT;
    avg_old NUMERIC(10,2);
    avg_new NUMERIC(10,2);
    total_change NUMERIC(12,2);
BEGIN
    SELECT COUNT(*) INTO upd_count FROM new_table;

    SELECT AVG(o.price), AVG(n.price), SUM(n.price - o.price)
    INTO avg_old, avg_new, total_change
    FROM old_table o
    JOIN new_table n ON n.product_id = o.product_id;

    INSERT INTO test_price_update_summary (
        update_count, avg_old_price, avg_new_price, total_price_change
    ) VALUES (upd_count, avg_old, avg_new, total_change);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_summarize_price_updates
AFTER UPDATE ON test_prices
REFERENCING OLD TABLE AS old_table NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION summarize_price_updates();

-- Apply 10% price increase
UPDATE test_prices SET price = price * 1.10 WHERE product_id <= 50;

SELECT summary_id, update_count, avg_old_price, avg_new_price, total_price_change
FROM test_price_update_summary;

-- ============================================================================
-- Section 5: Rate Limiting for Bulk Updates
-- ============================================================================

CREATE TABLE test_rate_limited_updates (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_rate_limit_log (
    log_id SERIAL PRIMARY KEY,
    update_size INT,
    threshold INT,
    action VARCHAR(50),
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_rate_limited_updates (data)
SELECT 'Data ' || i FROM generate_series(1, 200) i;

CREATE FUNCTION check_update_rate_limit() RETURNS TRIGGER AS $$
DECLARE
    update_count INT;
    max_batch_size INT;
BEGIN
    SELECT COUNT(*) INTO update_count FROM new_table;
    max_batch_size := 100;

    INSERT INTO test_rate_limit_log (update_size, threshold, action)
    VALUES (update_count, max_batch_size,
            CASE WHEN update_count > max_batch_size THEN 'REJECTED' ELSE 'ALLOWED' END);

    IF update_count > max_batch_size THEN
        RAISE EXCEPTION 'Bulk update exceeds rate limit: % rows (max: %)',
            update_count, max_batch_size;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_rate_limit
BEFORE UPDATE ON test_rate_limited_updates
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION check_update_rate_limit();

-- Allowed update
UPDATE test_rate_limited_updates SET data = 'Updated ' || id WHERE id <= 50;

-- Would be rejected
-- UPDATE test_rate_limited_updates SET data = 'Updated ' || id WHERE id <= 150;

SELECT log_id, update_size, threshold, action FROM test_rate_limit_log;

-- ============================================================================
-- Section 6: Aggregate Statistics Update
-- ============================================================================

CREATE TABLE test_transactions (
    txn_id SERIAL PRIMARY KEY,
    account_id INT,
    amount NUMERIC(12,2),
    txn_type VARCHAR(20)
);

CREATE TABLE test_account_summary (
    account_id INT PRIMARY KEY,
    total_debits NUMERIC(15,2) DEFAULT 0,
    total_credits NUMERIC(15,2) DEFAULT 0,
    transaction_count INT DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_transactions (account_id, amount, txn_type)
SELECT (i % 10) + 1, (random() * 1000)::NUMERIC(12,2),
       CASE WHEN i % 2 = 0 THEN 'debit' ELSE 'credit' END
FROM generate_series(1, 100) i;

-- Initialize summary
INSERT INTO test_account_summary (account_id, total_debits, total_credits, transaction_count, last_updated)
SELECT account_id, 0, 0, COUNT(*), CURRENT_TIMESTAMP
FROM test_transactions
GROUP BY account_id;

CREATE FUNCTION update_account_summary() RETURNS TRIGGER AS $$
BEGIN
    -- Recalculate affected accounts
    UPDATE test_account_summary s
    SET total_debits = COALESCE((
            SELECT SUM(amount) FROM new_table
            WHERE account_id = s.account_id AND txn_type = 'debit'
        ), 0),
        total_credits = COALESCE((
            SELECT SUM(amount) FROM new_table
            WHERE account_id = s.account_id AND txn_type = 'credit'
        ), 0),
        last_updated = CURRENT_TIMESTAMP
    WHERE s.account_id IN (SELECT DISTINCT account_id FROM new_table);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_summary
AFTER UPDATE ON test_transactions
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION update_account_summary();

-- Bulk update transaction amounts
UPDATE test_transactions SET amount = amount * 1.05 WHERE account_id = 1;

SELECT account_id, total_debits, total_credits FROM test_account_summary WHERE account_id = 1;

-- ============================================================================
-- Section 7: Data Quality Monitoring
-- ============================================================================

CREATE TABLE test_quality_monitored (
    id SERIAL PRIMARY KEY,
    value INT,
    quality_score NUMERIC(5,2)
);

CREATE TABLE test_quality_reports (
    report_id SERIAL PRIMARY KEY,
    total_updated INT,
    null_values INT,
    out_of_range INT,
    avg_quality_score NUMERIC(5,2),
    report_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_quality_monitored (value, quality_score)
SELECT i, (random() * 100)::NUMERIC(5,2)
FROM generate_series(1, 100) i;

CREATE FUNCTION monitor_data_quality() RETURNS TRIGGER AS $$
DECLARE
    total_rows INT;
    null_count INT;
    oor_count INT;
    avg_score NUMERIC(5,2);
BEGIN
    SELECT COUNT(*) INTO total_rows FROM new_table;

    SELECT COUNT(*) INTO null_count
    FROM new_table WHERE value IS NULL;

    SELECT COUNT(*) INTO oor_count
    FROM new_table WHERE value < 0 OR value > 1000;

    SELECT AVG(quality_score) INTO avg_score FROM new_table;

    INSERT INTO test_quality_reports (
        total_updated, null_values, out_of_range, avg_quality_score
    ) VALUES (total_rows, null_count, oor_count, avg_score);

    IF oor_count > 0 THEN
        RAISE WARNING 'Data quality issue: % out-of-range values in update', oor_count;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_monitor_quality
AFTER UPDATE ON test_quality_monitored
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION monitor_data_quality();

UPDATE test_quality_monitored SET quality_score = quality_score * 0.9 WHERE id <= 20;

SELECT report_id, total_updated, null_values, out_of_range, avg_quality_score
FROM test_quality_reports;

-- ============================================================================
-- Section 8: Performance Tracking
-- ============================================================================

CREATE TABLE test_perf_tracked (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_perf_metrics (
    metric_id SERIAL PRIMARY KEY,
    operation VARCHAR(50),
    rows_affected INT,
    execution_time_ms NUMERIC(10,2),
    recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_perf_tracked (data)
SELECT 'Data ' || i FROM generate_series(1, 1000) i;

CREATE FUNCTION track_update_performance_before() RETURNS TRIGGER AS $$
BEGIN
    -- Store start time in temp table
    CREATE TEMP TABLE IF NOT EXISTS temp_perf_start (start_time TIMESTAMP);
    DELETE FROM temp_perf_start;
    INSERT INTO temp_perf_start VALUES (clock_timestamp());
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION track_update_performance_after() RETURNS TRIGGER AS $$
DECLARE
    start_ts TIMESTAMP;
    duration_ms NUMERIC(10,2);
    row_count INT;
BEGIN
    SELECT start_time INTO start_ts FROM temp_perf_start;
    duration_ms := EXTRACT(MILLISECOND FROM (clock_timestamp() - start_ts));

    SELECT COUNT(*) INTO row_count FROM new_table;

    INSERT INTO test_perf_metrics (operation, rows_affected, execution_time_ms)
    VALUES ('UPDATE', row_count, duration_ms);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_perf_before
BEFORE UPDATE ON test_perf_tracked
FOR EACH STATEMENT
EXECUTE FUNCTION track_update_performance_before();

CREATE TRIGGER trg_perf_after
AFTER UPDATE ON test_perf_tracked
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_update_performance_after();

UPDATE test_perf_tracked SET data = 'Updated ' || id WHERE id <= 500;

SELECT metric_id, operation, rows_affected, execution_time_ms FROM test_perf_metrics;

-- ============================================================================
-- Section 9: Change Detection and Notification
-- ============================================================================

CREATE TABLE test_monitored_data (
    id SERIAL PRIMARY KEY,
    critical_value NUMERIC(10,2),
    threshold NUMERIC(10,2) DEFAULT 100.00
);

CREATE TABLE test_threshold_alerts (
    alert_id SERIAL PRIMARY KEY,
    exceeded_count INT,
    max_value NUMERIC(10,2),
    alert_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_monitored_data (critical_value, threshold)
SELECT (random() * 200)::NUMERIC(10,2), 100.00
FROM generate_series(1, 100) i;

CREATE FUNCTION detect_threshold_violations() RETURNS TRIGGER AS $$
DECLARE
    violation_count INT;
    max_val NUMERIC(10,2);
BEGIN
    SELECT COUNT(*), MAX(critical_value)
    INTO violation_count, max_val
    FROM new_table
    WHERE critical_value > threshold;

    IF violation_count > 0 THEN
        INSERT INTO test_threshold_alerts (exceeded_count, max_value)
        VALUES (violation_count, max_val);

        RAISE NOTICE 'Threshold violations detected: % records, max value: %',
            violation_count, max_val;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_detect_violations
AFTER UPDATE ON test_monitored_data
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION detect_threshold_violations();

UPDATE test_monitored_data SET critical_value = critical_value * 1.5 WHERE id <= 30;

SELECT alert_id, exceeded_count, max_value FROM test_threshold_alerts;

-- ============================================================================
-- Section 10: Versioning at Statement Level
-- ============================================================================

CREATE TABLE test_versioned_bulk (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    version INT DEFAULT 1
);

CREATE TABLE test_version_audit (
    audit_id SERIAL PRIMARY KEY,
    update_batch_id UUID DEFAULT gen_random_uuid(),
    rows_updated INT,
    version_increment INT,
    audit_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_versioned_bulk (data)
SELECT 'Data ' || i FROM generate_series(1, 100) i;

CREATE FUNCTION audit_version_update() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
    version_inc INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM new_table;

    -- All rows should have same version increment
    SELECT MAX(n.version - o.version)
    INTO version_inc
    FROM new_table n
    JOIN old_table o ON n.id = o.id;

    INSERT INTO test_version_audit (rows_updated, version_increment)
    VALUES (row_count, version_inc);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_versions
AFTER UPDATE ON test_versioned_bulk
REFERENCING OLD TABLE AS old_table NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION audit_version_update();

-- Version is incremented by row-level trigger (not shown here for simplicity)
UPDATE test_versioned_bulk SET version = version + 1 WHERE id <= 20;

SELECT audit_id, update_batch_id, rows_updated, version_increment FROM test_version_audit;

-- ============================================================================
-- Section 11: Conditional Statement Trigger
-- ============================================================================

CREATE TABLE test_conditional_stmt_update (
    id SERIAL PRIMARY KEY,
    important BOOLEAN DEFAULT FALSE,
    value INT
);

CREATE TABLE test_important_update_log (
    log_id SERIAL PRIMARY KEY,
    important_count INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_conditional_stmt_update (important, value)
SELECT (i % 3 = 0), i FROM generate_series(1, 100) i;

CREATE FUNCTION log_important_updates() RETURNS TRIGGER AS $$
DECLARE
    imp_count INT;
BEGIN
    SELECT COUNT(*) INTO imp_count
    FROM new_table
    WHERE important = TRUE;

    IF imp_count > 0 THEN
        INSERT INTO test_important_update_log (important_count)
        VALUES (imp_count);
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_important_updates
AFTER UPDATE ON test_conditional_stmt_update
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_important_updates();

UPDATE test_conditional_stmt_update SET value = value * 2 WHERE important = TRUE;

SELECT log_id, important_count FROM test_important_update_log;

-- ============================================================================
-- Section 12: Comparison with Row-Level Triggers
-- ============================================================================

CREATE TABLE test_trigger_comparison (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_row_trigger_log (
    log_id SERIAL PRIMARY KEY,
    message TEXT
);

CREATE TABLE test_stmt_trigger_log (
    log_id SERIAL PRIMARY KEY,
    message TEXT
);

INSERT INTO test_trigger_comparison (value)
SELECT i FROM generate_series(1, 10) i;

CREATE FUNCTION log_row_update() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_row_trigger_log (message)
    VALUES ('Row updated: ' || NEW.id);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_stmt_update() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM new_table;
    INSERT INTO test_stmt_trigger_log (message)
    VALUES ('Statement updated ' || row_count || ' rows');
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_row_update
AFTER UPDATE ON test_trigger_comparison
FOR EACH ROW
EXECUTE FUNCTION log_row_update();

CREATE TRIGGER trg_stmt_update
AFTER UPDATE ON test_trigger_comparison
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_stmt_update();

UPDATE test_trigger_comparison SET value = value * 2 WHERE id <= 5;

SELECT COUNT(*) AS row_trigger_fires FROM test_row_trigger_log;  -- Should be 5
SELECT COUNT(*) AS stmt_trigger_fires FROM test_stmt_trigger_log;  -- Should be 1

-- ============================================================================
-- Section 13: Transaction Tracking
-- ============================================================================

CREATE TABLE test_txn_data (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_txn_update_log (
    log_id SERIAL PRIMARY KEY,
    txn_id BIGINT,
    rows_updated INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_txn_data (data)
SELECT 'Data ' || i FROM generate_series(1, 100) i;

CREATE FUNCTION log_txn_update() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM new_table;

    INSERT INTO test_txn_update_log (txn_id, rows_updated)
    VALUES (txid_current(), row_count);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_txn_update
AFTER UPDATE ON test_txn_data
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_txn_update();

BEGIN;
UPDATE test_txn_data SET data = 'Updated 1' WHERE id <= 20;
UPDATE test_txn_data SET data = 'Updated 2' WHERE id > 20 AND id <= 40;
COMMIT;

SELECT log_id, txn_id, rows_updated FROM test_txn_update_log ORDER BY log_id;

-- ============================================================================
-- Section 14: Error Handling
-- ============================================================================

CREATE TABLE test_error_handling_update (
    id SERIAL PRIMARY KEY,
    value INT
);

INSERT INTO test_error_handling_update (value)
SELECT i FROM generate_series(1, 50) i;

CREATE FUNCTION validate_bulk_changes() RETURNS TRIGGER AS $$
DECLARE
    avg_change NUMERIC(10,2);
    max_change INT;
BEGIN
    -- Calculate average change
    SELECT AVG(ABS(n.value - o.value))
    INTO avg_change
    FROM new_table n
    JOIN old_table o ON n.id = o.id;

    SELECT MAX(ABS(n.value - o.value))
    INTO max_change
    FROM new_table n
    JOIN old_table o ON n.id = o.id;

    IF max_change > 100 THEN
        RAISE EXCEPTION 'Bulk update rejected: max change % exceeds limit (100)', max_change;
    END IF;

    RAISE NOTICE 'Bulk update accepted: avg change %, max change %', avg_change, max_change;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_bulk_changes
BEFORE UPDATE ON test_error_handling_update
REFERENCING OLD TABLE AS old_table NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION validate_bulk_changes();

-- Valid update
UPDATE test_error_handling_update SET value = value + 10 WHERE id <= 10;

-- Would fail
-- UPDATE test_error_handling_update SET value = value + 200 WHERE id <= 10;

SELECT id, value FROM test_error_handling_update WHERE id <= 10 ORDER BY id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_update_stmt_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_update_stmt_best_practices VALUES
    (1, 'Statement triggers fire once per UPDATE statement'),
    (2, 'Use REFERENCING for OLD TABLE and NEW TABLE access'),
    (3, 'No access to individual row NEW/OLD values'),
    (4, 'Perfect for bulk validation and batch auditing'),
    (5, 'More efficient than row triggers for large updates'),
    (6, 'Use for statement-level statistics and summaries'),
    (7, 'Great for rate limiting bulk operations'),
    (8, 'Ideal for monitoring data quality in batches'),
    (9, 'Can validate entire update batch before commit'),
    (10, 'Return value is ignored (return NULL)'),
    (11, 'Use JOIN on transition tables for change analysis'),
    (12, 'Keep logic efficient (processes all updated rows)'),
    (13, 'Use for performance monitoring of bulk updates'),
    (14, 'Consider async processing for expensive operations'),
    (15, 'Document batch processing behavior and thresholds');

SELECT id, guideline FROM test_update_stmt_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_update_stmt_best_practices;
DROP TABLE test_error_handling_update;
DROP FUNCTION validate_bulk_changes();
DROP TABLE test_txn_update_log;
DROP TABLE test_txn_data;
DROP FUNCTION log_txn_update();
DROP TABLE test_stmt_trigger_log;
DROP TABLE test_row_trigger_log;
DROP TABLE test_trigger_comparison;
DROP FUNCTION log_stmt_update();
DROP FUNCTION log_row_update();
DROP TABLE test_important_update_log;
DROP TABLE test_conditional_stmt_update;
DROP FUNCTION log_important_updates();
DROP TABLE test_version_audit;
DROP TABLE test_versioned_bulk;
DROP FUNCTION audit_version_update();
DROP TABLE test_threshold_alerts;
DROP TABLE test_monitored_data;
DROP FUNCTION detect_threshold_violations();
DROP TABLE test_perf_metrics;
DROP TABLE test_perf_tracked;
DROP FUNCTION track_update_performance_after();
DROP FUNCTION track_update_performance_before();
DROP TABLE test_quality_reports;
DROP TABLE test_quality_monitored;
DROP FUNCTION monitor_data_quality();
DROP TABLE test_account_summary;
DROP TABLE test_transactions;
DROP FUNCTION update_account_summary();
DROP TABLE test_rate_limit_log;
DROP TABLE test_rate_limited_updates;
DROP FUNCTION check_update_rate_limit();
DROP TABLE test_price_update_summary;
DROP TABLE test_prices;
DROP FUNCTION summarize_price_updates();
DROP TABLE test_validate_bulk_update;
DROP FUNCTION validate_bulk_update();
DROP TABLE test_bulk_update_audit;
DROP TABLE test_bulk_update_data;
DROP FUNCTION audit_bulk_update();
DROP TABLE test_update_statement_log;
DROP TABLE test_statement_update;
DROP FUNCTION log_update_statement();

DROP DATABASE test_update_statement_db;

-- End of UPDATE statement-level trigger tests
