-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - INSERT Statement-Level
-- Description: BEFORE/AFTER INSERT statement-level trigger testing
-- ============================================================================

-- Statement-level triggers fire once per INSERT statement
-- BEFORE: fires before any rows are inserted
-- AFTER: fires after all rows are inserted
-- No access to NEW/OLD (use transition tables instead)
-- Use cases: bulk validation, batch processing, statement logging

-- Create test database
CREATE DATABASE test_insert_statement_db;
USE test_insert_statement_db;

-- ============================================================================
-- Section 1: Basic BEFORE INSERT Statement Trigger
-- ============================================================================

CREATE TABLE test_statement_insert (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_statement_log (
    log_id SERIAL PRIMARY KEY,
    statement_type VARCHAR(50),
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_insert_statement() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_statement_log (statement_type)
    VALUES ('BEFORE INSERT STATEMENT');
    RETURN NULL;  -- Return value ignored for statement triggers
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_insert_stmt
BEFORE INSERT ON test_statement_insert
FOR EACH STATEMENT
EXECUTE FUNCTION log_insert_statement();

-- Single INSERT
INSERT INTO test_statement_insert (data) VALUES ('Row 1');

-- Multi-row INSERT (still only one trigger fire)
INSERT INTO test_statement_insert (data) VALUES ('Row 2'), ('Row 3'), ('Row 4');

SELECT id, data FROM test_statement_insert ORDER BY id;
SELECT log_id, statement_type, logged_at FROM test_statement_log ORDER BY log_id;
-- Should see 2 log entries (one for each INSERT statement)

-- ============================================================================
-- Section 2: AFTER INSERT Statement Trigger
-- ============================================================================

CREATE TABLE test_after_statement (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_after_statement_log (
    log_id SERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_after_insert_statement() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_after_statement_log (message)
    VALUES ('AFTER INSERT statement completed');
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_after_insert_stmt
AFTER INSERT ON test_after_statement
FOR EACH STATEMENT
EXECUTE FUNCTION log_after_insert_statement();

INSERT INTO test_after_statement (value) VALUES (1), (2), (3);
INSERT INTO test_after_statement (value) VALUES (4);

SELECT log_id, message FROM test_after_statement_log ORDER BY log_id;

-- ============================================================================
-- Section 3: Transition Tables (Referencing NEW TABLE)
-- ============================================================================

CREATE TABLE test_bulk_insert (
    id SERIAL PRIMARY KEY,
    amount NUMERIC(10,2),
    category VARCHAR(50)
);

CREATE TABLE test_bulk_summary (
    summary_id SERIAL PRIMARY KEY,
    insert_count INT,
    total_amount NUMERIC(12,2),
    summarized_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION summarize_bulk_insert() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
    sum_amount NUMERIC(12,2);
BEGIN
    -- Count and sum from transition table
    SELECT COUNT(*), COALESCE(SUM(amount), 0)
    INTO row_count, sum_amount
    FROM new_table;

    INSERT INTO test_bulk_summary (insert_count, total_amount)
    VALUES (row_count, sum_amount);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_summarize_bulk_insert
AFTER INSERT ON test_bulk_insert
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION summarize_bulk_insert();

-- Bulk insert
INSERT INTO test_bulk_insert (amount, category)
SELECT (random() * 1000)::NUMERIC(10,2), 'category_' || (i % 5)
FROM generate_series(1, 100) i;

SELECT summary_id, insert_count, total_amount FROM test_bulk_summary;

-- ============================================================================
-- Section 4: Statement-Level Validation
-- ============================================================================

CREATE TABLE test_daily_limits (
    limit_id INT PRIMARY KEY,
    limit_type VARCHAR(50),
    daily_limit INT,
    current_count INT DEFAULT 0,
    limit_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE test_limited_inserts (
    id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    inserted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Initialize limit
INSERT INTO test_daily_limits (limit_id, limit_type, daily_limit, current_count)
VALUES (1, 'max_inserts_per_day', 50, 0);

CREATE FUNCTION check_daily_limit() RETURNS TRIGGER AS $$
DECLARE
    current_limit INT;
    current_count INT;
    insert_count INT;
BEGIN
    -- Get current count from new_table
    SELECT COUNT(*) INTO insert_count FROM new_table;

    -- Get daily limit
    SELECT daily_limit, current_count INTO current_limit, current_count
    FROM test_daily_limits
    WHERE limit_id = 1 AND limit_date = CURRENT_DATE;

    IF NOT FOUND THEN
        -- Reset daily counter
        UPDATE test_daily_limits
        SET current_count = 0, limit_date = CURRENT_DATE
        WHERE limit_id = 1;
        current_count = 0;
        current_limit = 50;
    END IF;

    -- Check if would exceed limit
    IF (current_count + insert_count) > current_limit THEN
        RAISE EXCEPTION 'Daily insert limit exceeded: % + % > %',
            current_count, insert_count, current_limit;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_daily_limit
BEFORE INSERT ON test_limited_inserts
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION check_daily_limit();

-- Should succeed (within limit)
INSERT INTO test_limited_inserts (item_name) VALUES ('Item 1'), ('Item 2');

SELECT id, item_name FROM test_limited_inserts ORDER BY id;

-- ============================================================================
-- Section 5: Batch Processing Trigger
-- ============================================================================

CREATE TABLE test_incoming_data (
    id SERIAL PRIMARY KEY,
    raw_data TEXT,
    processed BOOLEAN DEFAULT FALSE
);

CREATE TABLE test_processing_queue (
    queue_id SERIAL PRIMARY KEY,
    batch_size INT,
    queued_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION queue_for_processing() RETURNS TRIGGER AS $$
DECLARE
    batch_count INT;
BEGIN
    SELECT COUNT(*) INTO batch_count FROM new_table;

    -- Add to processing queue
    INSERT INTO test_processing_queue (batch_size)
    VALUES (batch_count);

    -- Could trigger async processing here
    RAISE NOTICE 'Queued % rows for processing', batch_count;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_queue_processing
AFTER INSERT ON test_incoming_data
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION queue_for_processing();

INSERT INTO test_incoming_data (raw_data)
SELECT 'Data row ' || i FROM generate_series(1, 25) i;

SELECT queue_id, batch_size, queued_at FROM test_processing_queue;

-- ============================================================================
-- Section 6: Statement-Level Audit
-- ============================================================================

CREATE TABLE test_sensitive_data (
    id SERIAL PRIMARY KEY,
    confidential_info TEXT
);

CREATE TABLE test_statement_audit (
    audit_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    operation VARCHAR(20),
    row_count INT,
    username VARCHAR(100),
    audit_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION audit_statement() RETURNS TRIGGER AS $$
DECLARE
    affected_rows INT;
BEGIN
    SELECT COUNT(*) INTO affected_rows FROM new_table;

    INSERT INTO test_statement_audit (table_name, operation, row_count, username)
    VALUES (TG_TABLE_NAME, TG_OP, affected_rows, CURRENT_USER);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_statement
AFTER INSERT ON test_sensitive_data
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION audit_statement();

INSERT INTO test_sensitive_data (confidential_info)
VALUES ('Secret 1'), ('Secret 2'), ('Secret 3');

SELECT audit_id, table_name, operation, row_count, username FROM test_statement_audit;

-- ============================================================================
-- Section 7: Rate Limiting
-- ============================================================================

CREATE TABLE test_api_requests (
    request_id SERIAL PRIMARY KEY,
    endpoint VARCHAR(200),
    request_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_rate_limit_violations (
    violation_id SERIAL PRIMARY KEY,
    batch_size INT,
    threshold INT,
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION check_rate_limit() RETURNS TRIGGER AS $$
DECLARE
    request_count INT;
    rate_threshold INT;
BEGIN
    request_count := (SELECT COUNT(*) FROM new_table);
    rate_threshold := 100;

    IF request_count > rate_threshold THEN
        INSERT INTO test_rate_limit_violations (batch_size, threshold)
        VALUES (request_count, rate_threshold);

        RAISE EXCEPTION 'Rate limit exceeded: % requests in single batch (threshold: %)',
            request_count, rate_threshold;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_check_rate_limit
BEFORE INSERT ON test_api_requests
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION check_rate_limit();

-- Should succeed
INSERT INTO test_api_requests (endpoint)
SELECT '/api/users' FROM generate_series(1, 50) i;

-- Would fail (exceeds threshold)
-- INSERT INTO test_api_requests (endpoint)
-- SELECT '/api/users' FROM generate_series(1, 150) i;

SELECT request_id, endpoint FROM test_api_requests ORDER BY request_id LIMIT 10;

-- ============================================================================
-- Section 8: Data Quality Checks
-- ============================================================================

CREATE TABLE test_quality_data (
    id SERIAL PRIMARY KEY,
    value INT,
    status VARCHAR(20)
);

CREATE TABLE test_quality_issues (
    issue_id SERIAL PRIMARY KEY,
    issue_type VARCHAR(100),
    issue_count INT,
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION check_data_quality() RETURNS TRIGGER AS $$
DECLARE
    null_count INT;
    invalid_count INT;
BEGIN
    -- Check for NULL values
    SELECT COUNT(*) INTO null_count
    FROM new_table
    WHERE value IS NULL;

    -- Check for invalid values
    SELECT COUNT(*) INTO invalid_count
    FROM new_table
    WHERE value < 0;

    IF null_count > 0 THEN
        INSERT INTO test_quality_issues (issue_type, issue_count)
        VALUES ('NULL_VALUES', null_count);
    END IF;

    IF invalid_count > 0 THEN
        INSERT INTO test_quality_issues (issue_type, issue_count)
        VALUES ('NEGATIVE_VALUES', invalid_count);

        RAISE EXCEPTION 'Data quality check failed: % negative values found', invalid_count;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_quality_check
BEFORE INSERT ON test_quality_data
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION check_data_quality();

-- Should succeed
INSERT INTO test_quality_data (value, status) VALUES (10, 'valid'), (20, 'valid');

-- Would fail (negative values)
-- INSERT INTO test_quality_data (value, status) VALUES (10, 'valid'), (-5, 'invalid');

SELECT id, value, status FROM test_quality_data ORDER BY id;

-- ============================================================================
-- Section 9: Performance Monitoring
-- ============================================================================

CREATE TABLE test_monitored_inserts (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_performance_metrics (
    metric_id SERIAL PRIMARY KEY,
    operation VARCHAR(50),
    batch_size INT,
    execution_start TIMESTAMP,
    execution_end TIMESTAMP,
    duration_ms NUMERIC(10,2)
);

CREATE FUNCTION track_insert_performance() RETURNS TRIGGER AS $$
DECLARE
    start_time TIMESTAMP;
    row_count INT;
BEGIN
    start_time := clock_timestamp();

    SELECT COUNT(*) INTO row_count FROM new_table;

    INSERT INTO test_performance_metrics (operation, batch_size, execution_start)
    VALUES ('INSERT', row_count, start_time);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION complete_performance_tracking() RETURNS TRIGGER AS $$
DECLARE
    row_count INT;
BEGIN
    SELECT COUNT(*) INTO row_count FROM new_table;

    UPDATE test_performance_metrics
    SET execution_end = clock_timestamp(),
        duration_ms = EXTRACT(MILLISECOND FROM (clock_timestamp() - execution_start))
    WHERE metric_id = (SELECT MAX(metric_id) FROM test_performance_metrics);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_before
BEFORE INSERT ON test_monitored_inserts
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_insert_performance();

CREATE TRIGGER trg_track_after
AFTER INSERT ON test_monitored_inserts
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION complete_performance_tracking();

INSERT INTO test_monitored_inserts (data)
SELECT 'Row ' || i FROM generate_series(1, 1000) i;

SELECT metric_id, operation, batch_size, duration_ms FROM test_performance_metrics;

-- ============================================================================
-- Section 10: Conditional Statement Trigger (WHEN Clause)
-- ============================================================================

CREATE TABLE test_conditional_statement (
    id SERIAL PRIMARY KEY,
    important BOOLEAN,
    data TEXT
);

CREATE TABLE test_important_inserts_log (
    log_id SERIAL PRIMARY KEY,
    important_count INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_important_inserts() RETURNS TRIGGER AS $$
DECLARE
    imp_count INT;
BEGIN
    SELECT COUNT(*) INTO imp_count
    FROM new_table
    WHERE important = TRUE;

    IF imp_count > 0 THEN
        INSERT INTO test_important_inserts_log (important_count)
        VALUES (imp_count);
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_important
AFTER INSERT ON test_conditional_statement
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION log_important_inserts();

INSERT INTO test_conditional_statement (important, data) VALUES
    (TRUE, 'Important 1'),
    (FALSE, 'Normal 1'),
    (TRUE, 'Important 2');

SELECT log_id, important_count FROM test_important_inserts_log;

-- ============================================================================
-- Section 11: Comparison of Row vs Statement Triggers
-- ============================================================================

CREATE TABLE test_comparison (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_row_log (
    log_id SERIAL PRIMARY KEY,
    message TEXT
);

CREATE TABLE test_stmt_log (
    log_id SERIAL PRIMARY KEY,
    message TEXT
);

CREATE FUNCTION log_row_trigger() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_row_log (message) VALUES ('Row trigger fired');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_stmt_trigger() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_stmt_log (message) VALUES ('Statement trigger fired');
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_row
AFTER INSERT ON test_comparison
FOR EACH ROW
EXECUTE FUNCTION log_row_trigger();

CREATE TRIGGER trg_stmt
AFTER INSERT ON test_comparison
FOR EACH STATEMENT
EXECUTE FUNCTION log_stmt_trigger();

-- Insert 5 rows
INSERT INTO test_comparison (value) VALUES (1), (2), (3), (4), (5);

SELECT COUNT(*) AS row_trigger_fires FROM test_row_log;  -- Should be 5
SELECT COUNT(*) AS stmt_trigger_fires FROM test_stmt_log;  -- Should be 1

-- ============================================================================
-- Section 12: Bulk Statistics Update
-- ============================================================================

CREATE TABLE test_bulk_stats (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    amount NUMERIC(10,2)
);

CREATE TABLE test_category_totals (
    category VARCHAR(50) PRIMARY KEY,
    total_amount NUMERIC(12,2) DEFAULT 0,
    row_count INT DEFAULT 0
);

CREATE FUNCTION update_bulk_stats() RETURNS TRIGGER AS $$
BEGIN
    -- Efficiently update stats for all affected categories
    INSERT INTO test_category_totals (category, total_amount, row_count)
    SELECT category, SUM(amount), COUNT(*)
    FROM new_table
    GROUP BY category
    ON CONFLICT (category) DO UPDATE
    SET total_amount = test_category_totals.total_amount + EXCLUDED.total_amount,
        row_count = test_category_totals.row_count + EXCLUDED.row_count;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_bulk_stats
AFTER INSERT ON test_bulk_stats
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION update_bulk_stats();

INSERT INTO test_bulk_stats (category, amount)
SELECT 'cat_' || (i % 5), (random() * 100)::NUMERIC(10,2)
FROM generate_series(1, 100) i;

SELECT category, total_amount, row_count FROM test_category_totals ORDER BY category;

-- ============================================================================
-- Section 13: Transaction Context Tracking
-- ============================================================================

CREATE TABLE test_txn_tracking (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_txn_log (
    txn_log_id SERIAL PRIMARY KEY,
    txn_id BIGINT,
    operation VARCHAR(50),
    row_count INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION track_transaction() RETURNS TRIGGER AS $$
DECLARE
    affected_rows INT;
BEGIN
    SELECT COUNT(*) INTO affected_rows FROM new_table;

    INSERT INTO test_txn_log (txn_id, operation, row_count)
    VALUES (txid_current(), TG_OP, affected_rows);

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_txn
AFTER INSERT ON test_txn_tracking
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_transaction();

BEGIN;
INSERT INTO test_txn_tracking (data) VALUES ('TXN 1 - Row 1'), ('TXN 1 - Row 2');
INSERT INTO test_txn_tracking (data) VALUES ('TXN 1 - Row 3');
COMMIT;

SELECT txn_log_id, txn_id, operation, row_count FROM test_txn_log;

-- ============================================================================
-- Section 14: Error Handling in Statement Triggers
-- ============================================================================

CREATE TABLE test_error_handling (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE FUNCTION validate_batch() RETURNS TRIGGER AS $$
DECLARE
    max_val INT;
    min_val INT;
BEGIN
    SELECT MAX(value), MIN(value) INTO max_val, min_val FROM new_table;

    IF max_val > 1000 THEN
        RAISE EXCEPTION 'Batch contains value > 1000: %', max_val;
    END IF;

    IF min_val < 0 THEN
        RAISE WARNING 'Batch contains negative value: %', min_val;
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_validate_batch
BEFORE INSERT ON test_error_handling
REFERENCING NEW TABLE AS new_table
FOR EACH STATEMENT
EXECUTE FUNCTION validate_batch();

-- Should succeed
INSERT INTO test_error_handling (value) VALUES (10), (20), (30);

-- Would fail
-- INSERT INTO test_error_handling (value) VALUES (10), (2000), (30);

SELECT id, value FROM test_error_handling ORDER BY id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_statement_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_statement_best_practices VALUES
    (1, 'Statement triggers fire once per SQL statement'),
    (2, 'Use REFERENCING NEW TABLE AS to access inserted rows'),
    (3, 'No access to individual NEW/OLD records'),
    (4, 'Perfect for bulk operations and batch validation'),
    (5, 'More efficient than row triggers for large inserts'),
    (6, 'Use for statement-level audit logging'),
    (7, 'Great for rate limiting and quota enforcement'),
    (8, 'Ideal for bulk statistics updates'),
    (9, 'Can validate entire batch before commit'),
    (10, 'Return value is ignored (return NULL)'),
    (11, 'WHEN clause not available for statement triggers'),
    (12, 'Use for performance monitoring of statements'),
    (13, 'Keep logic efficient (processes all rows)'),
    (14, 'Consider async processing for heavy operations'),
    (15, 'Document batch processing behavior');

SELECT id, guideline FROM test_statement_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_statement_best_practices;
DROP TABLE test_error_handling;
DROP FUNCTION validate_batch();
DROP TABLE test_txn_log;
DROP TABLE test_txn_tracking;
DROP FUNCTION track_transaction();
DROP TABLE test_category_totals;
DROP TABLE test_bulk_stats;
DROP FUNCTION update_bulk_stats();
DROP TABLE test_stmt_log;
DROP TABLE test_row_log;
DROP TABLE test_comparison;
DROP FUNCTION log_stmt_trigger();
DROP FUNCTION log_row_trigger();
DROP TABLE test_important_inserts_log;
DROP TABLE test_conditional_statement;
DROP FUNCTION log_important_inserts();
DROP TABLE test_performance_metrics;
DROP TABLE test_monitored_inserts;
DROP FUNCTION complete_performance_tracking();
DROP FUNCTION track_insert_performance();
DROP TABLE test_quality_issues;
DROP TABLE test_quality_data;
DROP FUNCTION check_data_quality();
DROP TABLE test_rate_limit_violations;
DROP TABLE test_api_requests;
DROP FUNCTION check_rate_limit();
DROP TABLE test_statement_audit;
DROP TABLE test_sensitive_data;
DROP FUNCTION audit_statement();
DROP TABLE test_processing_queue;
DROP TABLE test_incoming_data;
DROP FUNCTION queue_for_processing();
DROP TABLE test_daily_limits;
DROP TABLE test_limited_inserts;
DROP FUNCTION check_daily_limit();
DROP TABLE test_bulk_summary;
DROP TABLE test_bulk_insert;
DROP FUNCTION summarize_bulk_insert();
DROP TABLE test_after_statement_log;
DROP TABLE test_after_statement;
DROP FUNCTION log_after_insert_statement();
DROP TABLE test_statement_log;
DROP TABLE test_statement_insert;
DROP FUNCTION log_insert_statement();

DROP DATABASE test_insert_statement_db;

-- End of INSERT statement-level trigger tests
