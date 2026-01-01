-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - Edge Cases and Advanced Scenarios
-- Description: Comprehensive trigger edge case testing
-- ============================================================================

-- Edge cases covered:
-- - Trigger recursion prevention and detection
-- - Trigger chaining and cascading
-- - Error handling and exception management
-- - Transaction interaction (SAVEPOINT, ROLLBACK)
-- - NULL handling and edge values
-- - Concurrent modification scenarios
-- - Performance considerations
-- - Security and permission issues

-- Create test database
CREATE DATABASE test_trigger_edge_cases_db;
USE test_trigger_edge_cases_db;

-- ============================================================================
-- Section 1: Trigger Recursion Prevention
-- ============================================================================

CREATE TABLE test_recursive_updates (
    id SERIAL PRIMARY KEY,
    value INT,
    update_count INT DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_recursive_updates (value) VALUES (10), (20);

-- Track recursion depth
CREATE TEMP TABLE trigger_recursion_depth (depth INT DEFAULT 0);
INSERT INTO trigger_recursion_depth VALUES (0);

CREATE FUNCTION prevent_infinite_recursion() RETURNS TRIGGER AS $$
DECLARE
    current_depth INT;
    max_depth INT := 5;
BEGIN
    SELECT depth INTO current_depth FROM trigger_recursion_depth;

    IF current_depth >= max_depth THEN
        RAISE EXCEPTION 'Maximum trigger recursion depth (%) exceeded', max_depth;
    END IF;

    -- Increment depth
    UPDATE trigger_recursion_depth SET depth = depth + 1;

    -- Update the record
    NEW.update_count = OLD.update_count + 1;
    NEW.last_updated = CURRENT_TIMESTAMP;

    -- Only trigger recursion if value is being changed
    IF NEW.value != OLD.value AND NEW.value < 100 THEN
        -- This would trigger recursion
        NEW.value = NEW.value + 10;
    END IF;

    -- Decrement depth
    UPDATE trigger_recursion_depth SET depth = depth - 1;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_prevent_recursion
BEFORE UPDATE ON test_recursive_updates
FOR EACH ROW
EXECUTE FUNCTION prevent_infinite_recursion();

-- Update (will recurse a few times)
UPDATE test_recursive_updates SET value = 5 WHERE id = 1;

SELECT id, value, update_count FROM test_recursive_updates ORDER BY id;

-- ============================================================================
-- Section 2: Trigger Chaining Detection
-- ============================================================================

CREATE TABLE test_chain_a (
    id SERIAL PRIMARY KEY,
    value VARCHAR(200),
    chain_depth INT DEFAULT 0
);

CREATE TABLE test_chain_b (
    id SERIAL PRIMARY KEY,
    value VARCHAR(200),
    source_id INT
);

CREATE TABLE test_chain_c (
    id SERIAL PRIMARY KEY,
    value VARCHAR(200),
    source_id INT
);

CREATE TABLE test_chain_log (
    log_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    operation VARCHAR(50),
    chain_level INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_chain_a (value) VALUES ('Initial Value');

CREATE FUNCTION chain_to_b() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_chain_log (table_name, operation, chain_level)
    VALUES ('test_chain_a', TG_OP, 1);

    IF TG_OP = 'INSERT' THEN
        INSERT INTO test_chain_b (value, source_id)
        VALUES ('Chained from A: ' || NEW.value, NEW.id);
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION chain_to_c() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_chain_log (table_name, operation, chain_level)
    VALUES ('test_chain_b', TG_OP, 2);

    IF TG_OP = 'INSERT' THEN
        INSERT INTO test_chain_c (value, source_id)
        VALUES ('Chained from B: ' || NEW.value, NEW.id);
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_chain_end() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_chain_log (table_name, operation, chain_level)
    VALUES ('test_chain_c', TG_OP, 3);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_chain_a_to_b
AFTER INSERT ON test_chain_a
FOR EACH ROW
EXECUTE FUNCTION chain_to_b();

CREATE TRIGGER trg_chain_b_to_c
AFTER INSERT ON test_chain_b
FOR EACH ROW
EXECUTE FUNCTION chain_to_c();

CREATE TRIGGER trg_log_chain_c
AFTER INSERT ON test_chain_c
FOR EACH ROW
EXECUTE FUNCTION log_chain_end();

-- Insert into A, triggers chain to B and C
INSERT INTO test_chain_a (value) VALUES ('Trigger Chain Test');

SELECT log_id, table_name, operation, chain_level FROM test_chain_log ORDER BY log_id;
SELECT 'A' as tbl, id, value FROM test_chain_a WHERE id > 1
UNION ALL
SELECT 'B', id, value FROM test_chain_b
UNION ALL
SELECT 'C', id, value FROM test_chain_c;

-- ============================================================================
-- Section 3: Error Handling and Exception Recovery
-- ============================================================================

CREATE TABLE test_error_handling (
    id SERIAL PRIMARY KEY,
    value INT,
    status VARCHAR(50) DEFAULT 'pending'
);

CREATE TABLE test_error_log (
    error_id SERIAL PRIMARY KEY,
    error_message TEXT,
    sqlstate VARCHAR(10),
    record_id INT,
    occurred_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_error_handling (value) VALUES (10), (20), (30);

CREATE FUNCTION handle_trigger_errors() RETURNS TRIGGER AS $$
DECLARE
    error_msg TEXT;
    error_state VARCHAR(10);
BEGIN
    BEGIN
        -- Intentional error condition
        IF NEW.value < 0 THEN
            RAISE EXCEPTION 'Value cannot be negative';
        END IF;

        -- Potential division by zero
        IF NEW.value = 0 THEN
            NEW.status = 'invalid';
            RAISE EXCEPTION 'Value cannot be zero';
        END IF;

        -- Simulate constraint violation
        IF NEW.value > 1000 THEN
            RAISE EXCEPTION 'Value exceeds maximum allowed';
        END IF;

        NEW.status = 'valid';

    EXCEPTION
        WHEN OTHERS THEN
            GET STACKED DIAGNOSTICS
                error_msg = MESSAGE_TEXT,
                error_state = RETURNED_SQLSTATE;

            INSERT INTO test_error_log (error_message, sqlstate, record_id)
            VALUES (error_msg, error_state, NEW.id);

            -- Re-raise the exception to prevent the operation
            RAISE;
    END;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_handle_errors
BEFORE INSERT OR UPDATE ON test_error_handling
FOR EACH ROW
EXECUTE FUNCTION handle_trigger_errors();

-- Valid update
UPDATE test_error_handling SET value = 15 WHERE id = 1;

-- Invalid update (would fail and log error)
BEGIN;
UPDATE test_error_handling SET value = 0 WHERE id = 2;
EXCEPTION WHEN OTHERS THEN
    -- Error logged in test_error_log
END;
ROLLBACK;

SELECT id, value, status FROM test_error_handling ORDER BY id;
SELECT error_id, error_message, record_id FROM test_error_log;

-- ============================================================================
-- Section 4: Transaction Interaction with SAVEPOINT
-- ============================================================================

CREATE TABLE test_savepoint_interaction (
    id SERIAL PRIMARY KEY,
    value INT,
    processed BOOLEAN DEFAULT FALSE
);

CREATE TABLE test_savepoint_log (
    log_id SERIAL PRIMARY KEY,
    action VARCHAR(100),
    value INT,
    savepoint_level INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_with_savepoint() RETURNS TRIGGER AS $$
DECLARE
    sp_name TEXT := 'trigger_sp_' || NEW.id;
BEGIN
    -- Create savepoint within trigger
    EXECUTE 'SAVEPOINT ' || sp_name;

    BEGIN
        INSERT INTO test_savepoint_log (action, value, savepoint_level)
        VALUES ('Processing value', NEW.value, 1);

        -- Conditional rollback to savepoint
        IF NEW.value < 0 THEN
            EXECUTE 'ROLLBACK TO SAVEPOINT ' || sp_name;
            INSERT INTO test_savepoint_log (action, value, savepoint_level)
            VALUES ('Rolled back negative value', NEW.value, 0);
            RETURN NULL;  -- Prevent the insert
        END IF;

        EXECUTE 'RELEASE SAVEPOINT ' || sp_name;
        NEW.processed = TRUE;

    EXCEPTION WHEN OTHERS THEN
        EXECUTE 'ROLLBACK TO SAVEPOINT ' || sp_name;
        INSERT INTO test_savepoint_log (action, value, savepoint_level)
        VALUES ('Exception occurred', NEW.value, 0);
        RAISE;
    END;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_savepoint_test
BEFORE INSERT ON test_savepoint_interaction
FOR EACH ROW
EXECUTE FUNCTION log_with_savepoint();

-- Insert values (negative will be rejected)
INSERT INTO test_savepoint_interaction (value) VALUES (10), (20);
INSERT INTO test_savepoint_interaction (value) VALUES (-5);  -- Will be rejected

SELECT id, value, processed FROM test_savepoint_interaction ORDER BY id;
SELECT log_id, action, value, savepoint_level FROM test_savepoint_log ORDER BY log_id;

-- ============================================================================
-- Section 5: NULL Handling and Edge Values
-- ============================================================================

CREATE TABLE test_null_handling (
    id SERIAL PRIMARY KEY,
    nullable_string VARCHAR(200),
    nullable_int INT,
    nullable_date DATE,
    computed_value VARCHAR(200)
);

CREATE FUNCTION handle_null_values() RETURNS TRIGGER AS $$
BEGIN
    -- Handle NULL strings
    IF NEW.nullable_string IS NULL THEN
        NEW.nullable_string = '[NULL STRING]';
    ELSIF TRIM(NEW.nullable_string) = '' THEN
        NEW.nullable_string = '[EMPTY STRING]';
    END IF;

    -- Handle NULL integers
    IF NEW.nullable_int IS NULL THEN
        NEW.nullable_int = 0;
    END IF;

    -- Handle NULL dates
    IF NEW.nullable_date IS NULL THEN
        NEW.nullable_date = CURRENT_DATE;
    END IF;

    -- Compute value handling NULLs
    NEW.computed_value = COALESCE(NEW.nullable_string, 'NULL') ||
                         ':' ||
                         COALESCE(NEW.nullable_int::TEXT, 'NULL') ||
                         ':' ||
                         COALESCE(NEW.nullable_date::TEXT, 'NULL');

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_handle_nulls
BEFORE INSERT OR UPDATE ON test_null_handling
FOR EACH ROW
EXECUTE FUNCTION handle_null_values();

-- Insert with various NULL values
INSERT INTO test_null_handling (nullable_string, nullable_int, nullable_date) VALUES
    (NULL, NULL, NULL),
    ('', 42, NULL),
    ('test', NULL, '2024-01-01');

SELECT id, nullable_string, nullable_int, nullable_date, computed_value FROM test_null_handling ORDER BY id;

-- ============================================================================
-- Section 6: OLD/NEW Record Edge Cases
-- ============================================================================

CREATE TABLE test_old_new_edge_cases (
    id SERIAL PRIMARY KEY,
    field_a INT,
    field_b INT,
    change_type VARCHAR(100)
);

CREATE TABLE test_change_log (
    log_id SERIAL PRIMARY KEY,
    record_id INT,
    field_name VARCHAR(100),
    old_value TEXT,
    new_value TEXT,
    changed BOOLEAN
);

INSERT INTO test_old_new_edge_cases (field_a, field_b) VALUES (10, 20), (30, 40);

CREATE FUNCTION detect_field_changes() RETURNS TRIGGER AS $$
BEGIN
    -- Check if field_a changed
    IF TG_OP = 'UPDATE' THEN
        IF OLD.field_a IS DISTINCT FROM NEW.field_a THEN
            INSERT INTO test_change_log (record_id, field_name, old_value, new_value, changed)
            VALUES (NEW.id, 'field_a', OLD.field_a::TEXT, NEW.field_a::TEXT, TRUE);
        ELSE
            INSERT INTO test_change_log (record_id, field_name, old_value, new_value, changed)
            VALUES (NEW.id, 'field_a', OLD.field_a::TEXT, NEW.field_a::TEXT, FALSE);
        END IF;

        -- Check if field_b changed
        IF OLD.field_b IS DISTINCT FROM NEW.field_b THEN
            INSERT INTO test_change_log (record_id, field_name, old_value, new_value, changed)
            VALUES (NEW.id, 'field_b', OLD.field_b::TEXT, NEW.field_b::TEXT, TRUE);
        ELSE
            INSERT INTO test_change_log (record_id, field_name, old_value, new_value, changed)
            VALUES (NEW.id, 'field_b', OLD.field_b::TEXT, NEW.field_b::TEXT, FALSE);
        END IF;

        -- Set change type
        IF OLD.field_a IS DISTINCT FROM NEW.field_a AND OLD.field_b IS DISTINCT FROM NEW.field_b THEN
            NEW.change_type = 'both_changed';
        ELSIF OLD.field_a IS DISTINCT FROM NEW.field_a THEN
            NEW.change_type = 'field_a_only';
        ELSIF OLD.field_b IS DISTINCT FROM NEW.field_b THEN
            NEW.change_type = 'field_b_only';
        ELSE
            NEW.change_type = 'no_change';
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_detect_changes
BEFORE UPDATE ON test_old_new_edge_cases
FOR EACH ROW
EXECUTE FUNCTION detect_field_changes();

-- Update with different change patterns
UPDATE test_old_new_edge_cases SET field_a = 15 WHERE id = 1;  -- Only field_a changes
UPDATE test_old_new_edge_cases SET field_b = 25 WHERE id = 1;  -- Only field_b changes
UPDATE test_old_new_edge_cases SET field_a = 35, field_b = 45 WHERE id = 2;  -- Both change

SELECT id, field_a, field_b, change_type FROM test_old_new_edge_cases ORDER BY id;
SELECT log_id, record_id, field_name, old_value, new_value, changed FROM test_change_log ORDER BY log_id;

-- ============================================================================
-- Section 7: WHEN Clause Edge Cases
-- ============================================================================

CREATE TABLE test_when_clause (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    priority INT,
    processed BOOLEAN DEFAULT FALSE
);

CREATE TABLE test_when_log (
    log_id SERIAL PRIMARY KEY,
    record_id INT,
    trigger_fired BOOLEAN,
    reason VARCHAR(200)
);

INSERT INTO test_when_clause (status, priority) VALUES
    ('pending', 1),
    ('active', 5),
    ('completed', 10);

CREATE FUNCTION log_when_fired() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_when_log (record_id, trigger_fired, reason)
    VALUES (NEW.id, TRUE, 'WHEN condition matched');
    NEW.processed = TRUE;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_when_not_fired() RETURNS TRIGGER AS $$
BEGIN
    -- This trigger has no WHEN clause, always fires
    IF NOT EXISTS (SELECT 1 FROM test_when_log WHERE record_id = NEW.id) THEN
        INSERT INTO test_when_log (record_id, trigger_fired, reason)
        VALUES (NEW.id, FALSE, 'WHEN condition did not match');
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger only fires when priority > 3 and status is not 'completed'
CREATE TRIGGER trg_when_conditional
BEFORE UPDATE ON test_when_clause
FOR EACH ROW
WHEN (NEW.priority > 3 AND NEW.status != 'completed')
EXECUTE FUNCTION log_when_fired();

-- This trigger always fires (for comparison)
CREATE TRIGGER trg_when_always
AFTER UPDATE ON test_when_clause
FOR EACH ROW
EXECUTE FUNCTION log_when_not_fired();

-- These should trigger the WHEN clause
UPDATE test_when_clause SET priority = 6 WHERE id = 2;  -- Matches WHEN

-- These should NOT trigger the WHEN clause
UPDATE test_when_clause SET priority = 2 WHERE id = 1;  -- priority not > 3
UPDATE test_when_clause SET status = 'completed' WHERE id = 2;  -- status is 'completed'

SELECT id, status, priority, processed FROM test_when_clause ORDER BY id;
SELECT log_id, record_id, trigger_fired, reason FROM test_when_log ORDER BY log_id;

-- ============================================================================
-- Section 8: Trigger with Large Data Volumes
-- ============================================================================

CREATE TABLE test_bulk_operation (
    id SERIAL PRIMARY KEY,
    value INT,
    batch_id INT
);

CREATE TABLE test_bulk_stats (
    batch_id INT PRIMARY KEY,
    record_count INT DEFAULT 0,
    sum_value BIGINT DEFAULT 0,
    avg_value NUMERIC(12,2)
);

CREATE FUNCTION track_bulk_stats() RETURNS TRIGGER AS $$
DECLARE
    batch INT;
BEGIN
    IF TG_OP = 'INSERT' THEN
        batch = NEW.batch_id;
    ELSE
        batch = OLD.batch_id;
    END IF;

    -- Update statistics for the batch
    INSERT INTO test_bulk_stats (batch_id, record_count, sum_value)
    VALUES (batch, 0, 0)
    ON CONFLICT (batch_id) DO NOTHING;

    IF TG_OP = 'INSERT' THEN
        UPDATE test_bulk_stats
        SET record_count = record_count + 1,
            sum_value = sum_value + NEW.value
        WHERE batch_id = batch;
    ELSIF TG_OP = 'DELETE' THEN
        UPDATE test_bulk_stats
        SET record_count = record_count - 1,
            sum_value = sum_value - OLD.value
        WHERE batch_id = batch;
    END IF;

    -- Calculate average
    UPDATE test_bulk_stats
    SET avg_value = CASE WHEN record_count > 0 THEN sum_value::NUMERIC / record_count ELSE 0 END
    WHERE batch_id = batch;

    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_bulk
AFTER INSERT OR DELETE ON test_bulk_operation
FOR EACH ROW
EXECUTE FUNCTION track_bulk_stats();

-- Bulk insert
INSERT INTO test_bulk_operation (value, batch_id)
SELECT i, 1 FROM generate_series(1, 100) i;

SELECT batch_id, record_count, sum_value, avg_value FROM test_bulk_stats;

-- ============================================================================
-- Section 9: Trigger Return Value Edge Cases
-- ============================================================================

CREATE TABLE test_return_values (
    id SERIAL PRIMARY KEY,
    original_value INT,
    modified_value INT,
    was_modified BOOLEAN
);

CREATE FUNCTION test_return_null() RETURNS TRIGGER AS $$
BEGIN
    -- Returning NULL from BEFORE trigger skips the operation
    IF NEW.original_value < 0 THEN
        RETURN NULL;  -- Skip insert/update
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION test_modify_new() RETURNS TRIGGER AS $$
BEGIN
    -- Modify NEW before returning
    NEW.modified_value = NEW.original_value * 2;
    NEW.was_modified = TRUE;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_return_null
BEFORE INSERT ON test_return_values
FOR EACH ROW
EXECUTE FUNCTION test_return_null();

CREATE TRIGGER trg_modify_new
BEFORE INSERT ON test_return_values
FOR EACH ROW
EXECUTE FUNCTION test_modify_new();

-- Positive value (will be inserted and modified)
INSERT INTO test_return_values (original_value) VALUES (10);

-- Negative value (will be skipped)
INSERT INTO test_return_values (original_value) VALUES (-5);

-- Only one record should exist
SELECT id, original_value, modified_value, was_modified FROM test_return_values;

-- ============================================================================
-- Section 10: Trigger on System Columns
-- ============================================================================

CREATE TABLE test_system_columns (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200),
    row_version INT DEFAULT 1,
    last_xid BIGINT
);

CREATE FUNCTION track_system_info() RETURNS TRIGGER AS $$
BEGIN
    -- Track transaction ID
    NEW.last_xid = txid_current();

    -- Increment row version on update
    IF TG_OP = 'UPDATE' THEN
        NEW.row_version = OLD.row_version + 1;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_system_info
BEFORE INSERT OR UPDATE ON test_system_columns
FOR EACH ROW
EXECUTE FUNCTION track_system_info();

INSERT INTO test_system_columns (data) VALUES ('First insert');
UPDATE test_system_columns SET data = 'First update' WHERE id = 1;
UPDATE test_system_columns SET data = 'Second update' WHERE id = 1;

SELECT id, data, row_version, last_xid FROM test_system_columns;

-- ============================================================================
-- Section 11: Trigger Interaction with Foreign Keys
-- ============================================================================

CREATE TABLE test_fk_parent (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_fk_child (
    child_id SERIAL PRIMARY KEY,
    parent_id INT REFERENCES test_fk_parent(parent_id) ON DELETE CASCADE,
    child_name VARCHAR(200)
);

CREATE TABLE test_fk_cascade_log (
    log_id SERIAL PRIMARY KEY,
    operation VARCHAR(50),
    parent_id INT,
    child_id INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_fk_parent (parent_name) VALUES ('Parent 1'), ('Parent 2');
INSERT INTO test_fk_child (parent_id, child_name) VALUES
    (1, 'Child 1A'), (1, 'Child 1B'), (2, 'Child 2A');

CREATE FUNCTION log_fk_cascade() RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'DELETE' THEN
        INSERT INTO test_fk_cascade_log (operation, parent_id, child_id)
        VALUES ('child_cascade_delete', OLD.parent_id, OLD.child_id);
        RETURN OLD;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_parent_delete() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_fk_cascade_log (operation, parent_id, child_id)
    VALUES ('parent_delete', OLD.parent_id, NULL);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_child_cascade
BEFORE DELETE ON test_fk_child
FOR EACH ROW
EXECUTE FUNCTION log_fk_cascade();

CREATE TRIGGER trg_log_parent_delete
BEFORE DELETE ON test_fk_parent
FOR EACH ROW
EXECUTE FUNCTION log_parent_delete();

-- Delete parent (should cascade to children)
DELETE FROM test_fk_parent WHERE parent_id = 1;

SELECT log_id, operation, parent_id, child_id FROM test_fk_cascade_log ORDER BY log_id;
SELECT child_id, parent_id, child_name FROM test_fk_child;  -- Should only have child 2A

-- ============================================================================
-- Section 12: Trigger with Complex Conditionals
-- ============================================================================

CREATE TABLE test_complex_conditions (
    id SERIAL PRIMARY KEY,
    category VARCHAR(50),
    subcategory VARCHAR(50),
    value NUMERIC(12,2),
    status VARCHAR(50),
    flags JSONB DEFAULT '{}'::JSONB
);

CREATE FUNCTION apply_complex_rules() RETURNS TRIGGER AS $$
BEGIN
    -- Rule 1: Category-based validation
    IF NEW.category = 'premium' THEN
        IF NEW.value < 100 THEN
            RAISE EXCEPTION 'Premium category requires value >= 100';
        END IF;
        NEW.flags = jsonb_set(NEW.flags, '{premium}', 'true'::JSONB);
    END IF;

    -- Rule 2: Subcategory depends on category
    IF NEW.category = 'standard' AND NEW.subcategory NOT IN ('A', 'B', 'C') THEN
        RAISE EXCEPTION 'Invalid subcategory % for standard category', NEW.subcategory;
    END IF;

    -- Rule 3: Status transitions
    IF TG_OP = 'UPDATE' THEN
        IF OLD.status = 'locked' AND NEW.status != 'locked' THEN
            RAISE EXCEPTION 'Cannot unlock a locked record';
        END IF;
    END IF;

    -- Rule 4: Computed flags
    IF NEW.value > 1000 THEN
        NEW.flags = jsonb_set(NEW.flags, '{high_value}', 'true'::JSONB);
    END IF;

    IF NEW.category = 'premium' AND NEW.value > 500 THEN
        NEW.flags = jsonb_set(NEW.flags, '{vip}', 'true'::JSONB);
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_complex_rules
BEFORE INSERT OR UPDATE ON test_complex_conditions
FOR EACH ROW
EXECUTE FUNCTION apply_complex_rules();

-- Valid inserts
INSERT INTO test_complex_conditions (category, subcategory, value, status) VALUES
    ('premium', 'X', 150.00, 'active'),
    ('standard', 'A', 50.00, 'active'),
    ('premium', 'Y', 600.00, 'active');

-- Invalid insert (would fail)
-- INSERT INTO test_complex_conditions (category, subcategory, value, status) VALUES
--     ('premium', 'Z', 50.00, 'active');  -- Value too low for premium

SELECT id, category, value, status, flags FROM test_complex_conditions ORDER BY id;

-- ============================================================================
-- Section 13: Trigger Performance Monitoring
-- ============================================================================

CREATE TABLE test_performance_target (
    id SERIAL PRIMARY KEY,
    data TEXT
);

CREATE TABLE test_trigger_performance (
    perf_id SERIAL PRIMARY KEY,
    trigger_name VARCHAR(100),
    operation VARCHAR(50),
    execution_time_ms NUMERIC(10,3),
    record_id INT,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION monitor_trigger_performance() RETURNS TRIGGER AS $$
DECLARE
    start_time TIMESTAMP;
    end_time TIMESTAMP;
    duration_ms NUMERIC(10,3);
BEGIN
    start_time := clock_timestamp();

    -- Simulate some work
    PERFORM pg_sleep(0.001);  -- 1ms delay

    end_time := clock_timestamp();
    duration_ms := EXTRACT(EPOCH FROM (end_time - start_time)) * 1000;

    INSERT INTO test_trigger_performance (trigger_name, operation, execution_time_ms, record_id)
    VALUES ('monitor_trigger_performance', TG_OP, duration_ms, NEW.id);

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_monitor_performance
AFTER INSERT ON test_performance_target
FOR EACH ROW
EXECUTE FUNCTION monitor_trigger_performance();

-- Insert some records
INSERT INTO test_performance_target (data)
SELECT 'Data ' || i FROM generate_series(1, 10) i;

SELECT perf_id, trigger_name, operation, execution_time_ms, record_id
FROM test_trigger_performance
ORDER BY perf_id;

SELECT
    trigger_name,
    COUNT(*) as executions,
    AVG(execution_time_ms) as avg_ms,
    MIN(execution_time_ms) as min_ms,
    MAX(execution_time_ms) as max_ms
FROM test_trigger_performance
GROUP BY trigger_name;

-- ============================================================================
-- Section 14: Trigger with Dynamic SQL
-- ============================================================================

CREATE TABLE test_dynamic_trigger (
    id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    column_name VARCHAR(100),
    new_value TEXT
);

CREATE TABLE test_dynamic_log (
    log_id SERIAL PRIMARY KEY,
    executed_sql TEXT,
    result TEXT,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION execute_dynamic_operation() RETURNS TRIGGER AS $$
DECLARE
    sql_query TEXT;
    result_text TEXT;
BEGIN
    -- Construct dynamic SQL
    sql_query := format('SELECT %I FROM %I LIMIT 1',
                       NEW.column_name,
                       NEW.table_name);

    BEGIN
        EXECUTE sql_query INTO result_text;

        INSERT INTO test_dynamic_log (executed_sql, result)
        VALUES (sql_query, COALESCE(result_text, 'NULL'));

    EXCEPTION WHEN OTHERS THEN
        INSERT INTO test_dynamic_log (executed_sql, result)
        VALUES (sql_query, 'ERROR: ' || SQLERRM);
    END;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_dynamic_execution
AFTER INSERT ON test_dynamic_trigger
FOR EACH ROW
EXECUTE FUNCTION execute_dynamic_operation();

-- Execute dynamic queries
INSERT INTO test_dynamic_trigger (table_name, column_name) VALUES
    ('test_dynamic_log', 'log_id');

SELECT log_id, executed_sql, result FROM test_dynamic_log ORDER BY log_id;

-- ============================================================================
-- Section 15: Best Practices for Edge Cases
-- ============================================================================

CREATE TABLE test_edge_case_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_edge_case_best_practices VALUES
    (1, 'Always prevent infinite recursion with depth tracking'),
    (2, 'Document trigger chains and their dependencies'),
    (3, 'Use proper exception handling to log and recover from errors'),
    (4, 'Test trigger behavior with SAVEPOINT and ROLLBACK'),
    (5, 'Handle NULL values explicitly in trigger logic'),
    (6, 'Use IS DISTINCT FROM for NULL-safe comparisons'),
    (7, 'Test WHEN clause conditions with edge values'),
    (8, 'Consider performance impact on bulk operations'),
    (9, 'Understand RETURN NULL behavior in BEFORE triggers'),
    (10, 'Track system columns and transaction IDs when needed'),
    (11, 'Test trigger interaction with foreign key cascades'),
    (12, 'Validate complex conditional logic thoroughly'),
    (13, 'Monitor trigger performance in production'),
    (14, 'Be cautious with dynamic SQL in triggers'),
    (15, 'Always test error paths and exception scenarios');

SELECT id, guideline FROM test_edge_case_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_edge_case_best_practices;
DROP TABLE test_dynamic_log;
DROP TABLE test_dynamic_trigger;
DROP FUNCTION execute_dynamic_operation();
DROP TABLE test_trigger_performance;
DROP TABLE test_performance_target;
DROP FUNCTION monitor_trigger_performance();
DROP TABLE test_complex_conditions;
DROP FUNCTION apply_complex_rules();
DROP TABLE test_fk_cascade_log;
DROP TABLE test_fk_child;
DROP TABLE test_fk_parent;
DROP FUNCTION log_parent_delete();
DROP FUNCTION log_fk_cascade();
DROP TABLE test_system_columns;
DROP FUNCTION track_system_info();
DROP TABLE test_return_values;
DROP FUNCTION test_modify_new();
DROP FUNCTION test_return_null();
DROP TABLE test_bulk_stats;
DROP TABLE test_bulk_operation;
DROP FUNCTION track_bulk_stats();
DROP TABLE test_when_log;
DROP TABLE test_when_clause;
DROP FUNCTION log_when_not_fired();
DROP FUNCTION log_when_fired();
DROP TABLE test_change_log;
DROP TABLE test_old_new_edge_cases;
DROP FUNCTION detect_field_changes();
DROP TABLE test_null_handling;
DROP FUNCTION handle_null_values();
DROP TABLE test_savepoint_log;
DROP TABLE test_savepoint_interaction;
DROP FUNCTION log_with_savepoint();
DROP TABLE test_error_log;
DROP TABLE test_error_handling;
DROP FUNCTION handle_trigger_errors();
DROP TABLE test_chain_log;
DROP TABLE test_chain_c;
DROP TABLE test_chain_b;
DROP TABLE test_chain_a;
DROP FUNCTION log_chain_end();
DROP FUNCTION chain_to_c();
DROP FUNCTION chain_to_b();
DROP TABLE test_recursive_updates;
DROP FUNCTION prevent_infinite_recursion();

DROP DATABASE test_trigger_edge_cases_db;

-- End of trigger edge case tests
