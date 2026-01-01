-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - AFTER DELETE Row-Level
-- Description: Comprehensive AFTER DELETE row-level trigger testing
-- ============================================================================

-- AFTER DELETE triggers fire after each row is deleted
-- Can only access OLD record values
-- Cannot prevent deletion (already done)
-- Use cases: audit logging, cleanup, notifications, cascade operations

-- Create test database
CREATE DATABASE test_after_delete_row_db;
USE test_after_delete_row_db;

-- ============================================================================
-- Section 1: Basic Deletion Audit Trail
-- ============================================================================

CREATE TABLE test_users_deletable (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_deletion_audit (
    audit_id SERIAL PRIMARY KEY,
    deleted_user_id INT,
    username VARCHAR(100),
    email VARCHAR(200),
    deleted_by VARCHAR(100),
    deleted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_users_deletable (username, email) VALUES
    ('alice', 'alice@example.com'),
    ('bob', 'bob@example.com'),
    ('charlie', 'charlie@example.com');

CREATE FUNCTION audit_user_deletion() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_deletion_audit (deleted_user_id, username, email, deleted_by)
    VALUES (OLD.user_id, OLD.username, OLD.email, CURRENT_USER);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_deletion
AFTER DELETE ON test_users_deletable
FOR EACH ROW
EXECUTE FUNCTION audit_user_deletion();

DELETE FROM test_users_deletable WHERE user_id = 1;

SELECT user_id, username FROM test_users_deletable ORDER BY user_id;
SELECT audit_id, deleted_user_id, username, deleted_by FROM test_deletion_audit;

-- ============================================================================
-- Section 2: Cascade Delete to Related Tables
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200)
);

CREATE TABLE test_account_sessions (
    session_id SERIAL PRIMARY KEY,
    account_id INT,
    session_token VARCHAR(200)
);

CREATE TABLE test_account_preferences (
    pref_id SERIAL PRIMARY KEY,
    account_id INT,
    preferences JSONB
);

INSERT INTO test_accounts (account_name) VALUES ('Account A'), ('Account B');
INSERT INTO test_account_sessions (account_id, session_token) VALUES (1, 'token1'), (1, 'token2');
INSERT INTO test_account_preferences (account_id, preferences) VALUES (1, '{"theme": "dark"}'::JSONB);

CREATE FUNCTION cascade_delete_account_data() RETURNS TRIGGER AS $$
BEGIN
    -- Delete all sessions
    DELETE FROM test_account_sessions WHERE account_id = OLD.account_id;

    -- Delete preferences
    DELETE FROM test_account_preferences WHERE account_id = OLD.account_id;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cascade_delete
AFTER DELETE ON test_accounts
FOR EACH ROW
EXECUTE FUNCTION cascade_delete_account_data();

DELETE FROM test_accounts WHERE account_id = 1;

SELECT COUNT(*) AS sessions_remaining FROM test_account_sessions;
SELECT COUNT(*) AS prefs_remaining FROM test_account_preferences;

-- ============================================================================
-- Section 3: Update Statistics After Deletion
-- ============================================================================

CREATE TABLE test_products_deletable (
    product_id SERIAL PRIMARY KEY,
    category VARCHAR(100),
    price NUMERIC(10,2)
);

CREATE TABLE test_category_statistics (
    category VARCHAR(100) PRIMARY KEY,
    product_count INT DEFAULT 0,
    total_value NUMERIC(12,2) DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_category_statistics (category, product_count, total_value, last_updated) VALUES
    ('electronics', 3, 900.00, CURRENT_TIMESTAMP);

INSERT INTO test_products_deletable (category, price) VALUES
    ('electronics', 100.00),
    ('electronics', 400.00),
    ('electronics', 400.00);

CREATE FUNCTION update_category_stats_on_delete() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_category_statistics
    SET product_count = product_count - 1,
        total_value = total_value - OLD.price,
        last_updated = CURRENT_TIMESTAMP
    WHERE category = OLD.category;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_stats_on_delete
AFTER DELETE ON test_products_deletable
FOR EACH ROW
EXECUTE FUNCTION update_category_stats_on_delete();

DELETE FROM test_products_deletable WHERE product_id = 1;

SELECT category, product_count, total_value FROM test_category_statistics;

-- ============================================================================
-- Section 4: Notification on Deletion
-- ============================================================================

CREATE TABLE test_critical_records (
    record_id SERIAL PRIMARY KEY,
    record_type VARCHAR(50),
    importance VARCHAR(20),
    data TEXT
);

CREATE TABLE test_deletion_alerts (
    alert_id SERIAL PRIMARY KEY,
    deleted_record_id INT,
    record_type VARCHAR(50),
    importance VARCHAR(20),
    alert_message TEXT,
    alerted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_critical_records (record_type, importance, data) VALUES
    ('financial', 'critical', 'Important financial data'),
    ('system', 'high', 'System configuration'),
    ('user', 'low', 'User preference');

CREATE FUNCTION alert_on_deletion() RETURNS TRIGGER AS $$
BEGIN
    IF OLD.importance IN ('critical', 'high') THEN
        INSERT INTO test_deletion_alerts (deleted_record_id, record_type, importance, alert_message)
        VALUES (
            OLD.record_id,
            OLD.record_type,
            OLD.importance,
            'Critical record deleted: ' || OLD.record_type || ' (' || OLD.importance || ')'
        );
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_alert_deletion
AFTER DELETE ON test_critical_records
FOR EACH ROW
EXECUTE FUNCTION alert_on_deletion();

DELETE FROM test_critical_records WHERE record_id = 1;
DELETE FROM test_critical_records WHERE record_id = 3;  -- Won't trigger alert (low importance)

SELECT alert_id, record_type, importance, alert_message FROM test_deletion_alerts;

-- ============================================================================
-- Section 5: Cleanup Related Resources
-- ============================================================================

CREATE TABLE test_files (
    file_id SERIAL PRIMARY KEY,
    filename VARCHAR(200),
    file_path VARCHAR(500),
    file_size BIGINT
);

CREATE TABLE test_file_chunks (
    chunk_id SERIAL PRIMARY KEY,
    file_id INT,
    chunk_number INT,
    chunk_data BYTEA
);

CREATE TABLE test_file_metadata (
    metadata_id SERIAL PRIMARY KEY,
    file_id INT,
    mime_type VARCHAR(100),
    created_at TIMESTAMP
);

INSERT INTO test_files (filename, file_path, file_size) VALUES ('test.pdf', '/files/test.pdf', 1024);
INSERT INTO test_file_chunks (file_id, chunk_number, chunk_data) VALUES (1, 1, 'data1'), (1, 2, 'data2');
INSERT INTO test_file_metadata (file_id, mime_type, created_at) VALUES (1, 'application/pdf', CURRENT_TIMESTAMP);

CREATE FUNCTION cleanup_file_resources() RETURNS TRIGGER AS $$
BEGIN
    -- Delete all chunks
    DELETE FROM test_file_chunks WHERE file_id = OLD.file_id;

    -- Delete metadata
    DELETE FROM test_file_metadata WHERE file_id = OLD.file_id;

    RAISE NOTICE 'Cleaned up resources for file: %', OLD.filename;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cleanup_file
AFTER DELETE ON test_files
FOR EACH ROW
EXECUTE FUNCTION cleanup_file_resources();

DELETE FROM test_files WHERE file_id = 1;

SELECT COUNT(*) AS chunks_remaining FROM test_file_chunks;
SELECT COUNT(*) AS metadata_remaining FROM test_file_metadata;

-- ============================================================================
-- Section 6: Conditional AFTER DELETE (WHEN Clause)
-- ============================================================================

CREATE TABLE test_conditional_delete_after (
    id SERIAL PRIMARY KEY,
    status VARCHAR(50),
    is_important BOOLEAN DEFAULT FALSE,
    data TEXT
);

CREATE TABLE test_important_deletions (
    deletion_id SERIAL PRIMARY KEY,
    deleted_id INT,
    status VARCHAR(50),
    deleted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_conditional_delete_after (status, is_important, data) VALUES
    ('active', TRUE, 'Important data'),
    ('inactive', FALSE, 'Normal data'),
    ('pending', TRUE, 'Important pending');

CREATE FUNCTION log_important_deletion() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_important_deletions (deleted_id, status)
    VALUES (OLD.id, OLD.status);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

-- Only fire for important records
CREATE TRIGGER trg_log_important_deletion
AFTER DELETE ON test_conditional_delete_after
FOR EACH ROW
WHEN (OLD.is_important = TRUE)
EXECUTE FUNCTION log_important_deletion();

DELETE FROM test_conditional_delete_after WHERE id IN (1, 2);

SELECT deletion_id, deleted_id, status FROM test_important_deletions;  -- Only id=1

-- ============================================================================
-- Section 7: Recalculate Aggregates
-- ============================================================================

CREATE TABLE test_order_items_delete (
    item_id SERIAL PRIMARY KEY,
    order_id INT,
    quantity INT,
    price NUMERIC(10,2)
);

CREATE TABLE test_order_summary (
    order_id INT PRIMARY KEY,
    total_items INT DEFAULT 0,
    total_amount NUMERIC(12,2) DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_order_summary (order_id, total_items, total_amount, last_updated) VALUES
    (1, 3, 600.00, CURRENT_TIMESTAMP);

INSERT INTO test_order_items_delete (order_id, quantity, price) VALUES
    (1, 2, 100.00),
    (1, 3, 150.00),
    (1, 1, 50.00);

CREATE FUNCTION recalc_order_on_delete() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_order_summary
    SET total_items = total_items - 1,
        total_amount = total_amount - (OLD.quantity * OLD.price),
        last_updated = CURRENT_TIMESTAMP
    WHERE order_id = OLD.order_id;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_recalc_order
AFTER DELETE ON test_order_items_delete
FOR EACH ROW
EXECUTE FUNCTION recalc_order_on_delete();

DELETE FROM test_order_items_delete WHERE item_id = 1;

SELECT order_id, total_items, total_amount FROM test_order_summary;

-- ============================================================================
-- Section 8: Maintain Deletion History
-- ============================================================================

CREATE TABLE test_records (
    record_id SERIAL PRIMARY KEY,
    record_data VARCHAR(200),
    record_status VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_deletion_history (
    history_id SERIAL PRIMARY KEY,
    deleted_record_id INT,
    record_data VARCHAR(200),
    record_status VARCHAR(50),
    existed_for INTERVAL,
    deleted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_records (record_data, record_status, created_at) VALUES
    ('Record 1', 'active', CURRENT_TIMESTAMP - INTERVAL '10 days'),
    ('Record 2', 'inactive', CURRENT_TIMESTAMP - INTERVAL '5 days');

CREATE FUNCTION maintain_deletion_history() RETURNS TRIGGER AS $$
DECLARE
    lifespan INTERVAL;
BEGIN
    lifespan := CURRENT_TIMESTAMP - OLD.created_at;

    INSERT INTO test_deletion_history (
        deleted_record_id, record_data, record_status, existed_for
    ) VALUES (
        OLD.record_id, OLD.record_data, OLD.record_status, lifespan
    );

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_deletion_history
AFTER DELETE ON test_records
FOR EACH ROW
EXECUTE FUNCTION maintain_deletion_history();

DELETE FROM test_records WHERE record_id = 1;

SELECT history_id, deleted_record_id, record_data, existed_for FROM test_deletion_history;

-- ============================================================================
-- Section 9: Cache Invalidation
-- ============================================================================

CREATE TABLE test_cached_data (
    data_id SERIAL PRIMARY KEY,
    data_key VARCHAR(200),
    data_value TEXT
);

CREATE TABLE test_cache_invalidations_delete (
    invalidation_id SERIAL PRIMARY KEY,
    cache_key VARCHAR(200),
    reason VARCHAR(100),
    invalidated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_cached_data (data_key, data_value) VALUES
    ('config:app', '{"setting": "value"}'),
    ('config:db', '{"connection": "string"}');

CREATE FUNCTION invalidate_cache_on_delete() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_cache_invalidations_delete (cache_key, reason)
    VALUES (OLD.data_key, 'DATA_DELETED');

    -- Also invalidate list cache
    INSERT INTO test_cache_invalidations_delete (cache_key, reason)
    VALUES ('cache:list', 'ITEM_DELETED');

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_invalidate_cache_delete
AFTER DELETE ON test_cached_data
FOR EACH ROW
EXECUTE FUNCTION invalidate_cache_on_delete();

DELETE FROM test_cached_data WHERE data_id = 1;

SELECT invalidation_id, cache_key, reason FROM test_cache_invalidations_delete ORDER BY invalidation_id;

-- ============================================================================
-- Section 10: Event Logging with Details
-- ============================================================================

CREATE TABLE test_entities (
    entity_id SERIAL PRIMARY KEY,
    entity_type VARCHAR(50),
    entity_data JSONB
);

CREATE TABLE test_deletion_events (
    event_id SERIAL PRIMARY KEY,
    entity_id INT,
    entity_type VARCHAR(50),
    event_details JSONB,
    event_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_entities (entity_type, entity_data) VALUES
    ('user', '{"name": "Alice", "email": "alice@example.com"}'::JSONB),
    ('product', '{"name": "Widget", "price": 29.99}'::JSONB);

CREATE FUNCTION log_deletion_event() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_deletion_events (entity_id, entity_type, event_details)
    VALUES (
        OLD.entity_id,
        OLD.entity_type,
        jsonb_build_object(
            'deleted_data', OLD.entity_data,
            'deleted_by', CURRENT_USER,
            'transaction_id', txid_current()
        )
    );
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_deletion_event
AFTER DELETE ON test_entities
FOR EACH ROW
EXECUTE FUNCTION log_deletion_event();

DELETE FROM test_entities WHERE entity_id = 1;

SELECT event_id, entity_type, event_details FROM test_deletion_events;

-- ============================================================================
-- Section 11: Orphan Cleanup
-- ============================================================================

CREATE TABLE test_parent_delete (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200)
);

CREATE TABLE test_orphan_children (
    child_id SERIAL PRIMARY KEY,
    parent_id INT,
    child_name VARCHAR(200)
);

INSERT INTO test_parent_delete (parent_name) VALUES ('Parent X');
INSERT INTO test_orphan_children (parent_id, child_name) VALUES (1, 'Child X1'), (1, 'Child X2');

CREATE FUNCTION cleanup_orphans() RETURNS TRIGGER AS $$
DECLARE
    orphan_count INT;
BEGIN
    -- Delete orphaned children
    DELETE FROM test_orphan_children WHERE parent_id = OLD.parent_id;

    GET DIAGNOSTICS orphan_count = ROW_COUNT;
    RAISE NOTICE 'Cleaned up % orphan records', orphan_count;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cleanup_orphans
AFTER DELETE ON test_parent_delete
FOR EACH ROW
EXECUTE FUNCTION cleanup_orphans();

DELETE FROM test_parent_delete WHERE parent_id = 1;

SELECT COUNT(*) AS orphans_remaining FROM test_orphan_children;

-- ============================================================================
-- Section 12: Multiple AFTER DELETE Triggers
-- ============================================================================

CREATE TABLE test_multi_after_delete (
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

INSERT INTO test_multi_after_delete (data) VALUES ('Data A'), ('Data B');

CREATE FUNCTION log_after_delete_a() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_delete_log_a (message)
    VALUES ('After delete A: ' || OLD.data);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_after_delete_b() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_delete_log_b (message)
    VALUES ('After delete B: ' || OLD.data);
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_after_delete_a
AFTER DELETE ON test_multi_after_delete
FOR EACH ROW
EXECUTE FUNCTION log_after_delete_a();

CREATE TRIGGER trg_after_delete_b
AFTER DELETE ON test_multi_after_delete
FOR EACH ROW
EXECUTE FUNCTION log_after_delete_b();

DELETE FROM test_multi_after_delete WHERE id = 1;

SELECT log_id, message FROM test_delete_log_a;
SELECT log_id, message FROM test_delete_log_b;

-- ============================================================================
-- Section 13: Quota Management
-- ============================================================================

CREATE TABLE test_storage_items (
    item_id SERIAL PRIMARY KEY,
    user_id INT,
    item_size BIGINT
);

CREATE TABLE test_user_storage_quota (
    user_id INT PRIMARY KEY,
    used_space BIGINT DEFAULT 0,
    max_space BIGINT DEFAULT 10737418240
);

INSERT INTO test_user_storage_quota (user_id, used_space) VALUES (1, 5000000);
INSERT INTO test_storage_items (user_id, item_size) VALUES (1, 1000000), (1, 2000000);

CREATE FUNCTION update_quota_on_delete() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_user_storage_quota
    SET used_space = used_space - OLD.item_size
    WHERE user_id = OLD.user_id;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_quota_delete
AFTER DELETE ON test_storage_items
FOR EACH ROW
EXECUTE FUNCTION update_quota_on_delete();

DELETE FROM test_storage_items WHERE item_id = 1;

SELECT user_id, used_space FROM test_user_storage_quota;

-- ============================================================================
-- Section 14: Analytics Update
-- ============================================================================

CREATE TABLE test_events_deletable (
    event_id SERIAL PRIMARY KEY,
    event_type VARCHAR(50),
    event_date DATE,
    event_count INT DEFAULT 1
);

CREATE TABLE test_event_analytics (
    analytics_id SERIAL PRIMARY KEY,
    event_type VARCHAR(50),
    total_events INT DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_event_analytics (event_type, total_events, last_updated) VALUES
    ('login', 100, CURRENT_TIMESTAMP),
    ('logout', 95, CURRENT_TIMESTAMP);

INSERT INTO test_events_deletable (event_type, event_date, event_count) VALUES
    ('login', CURRENT_DATE, 5),
    ('logout', CURRENT_DATE, 3);

CREATE FUNCTION update_analytics_on_delete() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_event_analytics
    SET total_events = total_events - OLD.event_count,
        last_updated = CURRENT_TIMESTAMP
    WHERE event_type = OLD.event_type;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_analytics_delete
AFTER DELETE ON test_events_deletable
FOR EACH ROW
EXECUTE FUNCTION update_analytics_on_delete();

DELETE FROM test_events_deletable WHERE event_id = 1;

SELECT event_type, total_events FROM test_event_analytics ORDER BY event_type;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_after_delete_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_after_delete_best_practices VALUES
    (1, 'AFTER DELETE triggers fire after row deletion completes'),
    (2, 'Can only access OLD record values (NEW does not exist)'),
    (3, 'Cannot prevent deletion (already completed)'),
    (4, 'Perfect for audit logging and deletion history'),
    (5, 'Use for cascade deletes to related tables'),
    (6, 'Great for updating statistics and aggregates'),
    (7, 'Use WHEN clause for conditional execution'),
    (8, 'Return value is ignored (return OLD or NULL)'),
    (9, 'Ideal for cleanup of related resources'),
    (10, 'Multiple triggers execute alphabetically by name'),
    (11, 'Keep trigger logic fast to avoid slowing deletes'),
    (12, 'Use for notification and alert systems'),
    (13, 'Great for cache invalidation'),
    (14, 'Perfect for quota and resource management'),
    (15, 'Document cascade and cleanup behavior');

SELECT id, guideline FROM test_after_delete_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_after_delete_best_practices;
DROP TABLE test_event_analytics;
DROP TABLE test_events_deletable;
DROP FUNCTION update_analytics_on_delete();
DROP TABLE test_user_storage_quota;
DROP TABLE test_storage_items;
DROP FUNCTION update_quota_on_delete();
DROP TABLE test_delete_log_b;
DROP TABLE test_delete_log_a;
DROP TABLE test_multi_after_delete;
DROP FUNCTION log_after_delete_b();
DROP FUNCTION log_after_delete_a();
DROP TABLE test_orphan_children;
DROP TABLE test_parent_delete;
DROP FUNCTION cleanup_orphans();
DROP TABLE test_deletion_events;
DROP TABLE test_entities;
DROP FUNCTION log_deletion_event();
DROP TABLE test_cache_invalidations_delete;
DROP TABLE test_cached_data;
DROP FUNCTION invalidate_cache_on_delete();
DROP TABLE test_deletion_history;
DROP TABLE test_records;
DROP FUNCTION maintain_deletion_history();
DROP TABLE test_order_summary;
DROP TABLE test_order_items_delete;
DROP FUNCTION recalc_order_on_delete();
DROP TABLE test_important_deletions;
DROP TABLE test_conditional_delete_after;
DROP FUNCTION log_important_deletion();
DROP TABLE test_file_metadata;
DROP TABLE test_file_chunks;
DROP TABLE test_files;
DROP FUNCTION cleanup_file_resources();
DROP TABLE test_deletion_alerts;
DROP TABLE test_critical_records;
DROP FUNCTION alert_on_deletion();
DROP TABLE test_category_statistics;
DROP TABLE test_products_deletable;
DROP FUNCTION update_category_stats_on_delete();
DROP TABLE test_account_preferences;
DROP TABLE test_account_sessions;
DROP TABLE test_accounts;
DROP FUNCTION cascade_delete_account_data();
DROP TABLE test_deletion_audit;
DROP TABLE test_users_deletable;
DROP FUNCTION audit_user_deletion();

DROP DATABASE test_after_delete_row_db;

-- End of AFTER DELETE row-level trigger tests
