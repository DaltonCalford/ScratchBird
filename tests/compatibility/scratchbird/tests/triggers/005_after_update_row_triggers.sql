-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - AFTER UPDATE Row-Level
-- Description: Comprehensive AFTER UPDATE row-level trigger testing
-- ============================================================================

-- AFTER UPDATE triggers fire after each row is updated
-- Can access OLD and NEW values (read-only)
-- Cannot modify the row (already updated)
-- Use cases: audit logging, notifications, cascade updates, change tracking

-- Create test database
CREATE DATABASE test_after_update_row_db;
USE test_after_update_row_db;

-- ============================================================================
-- Section 1: Basic AFTER UPDATE Audit Trail
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock INT,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_products_audit (
    audit_id SERIAL PRIMARY KEY,
    product_id INT,
    old_price NUMERIC(10,2),
    new_price NUMERIC(10,2),
    old_stock INT,
    new_stock INT,
    changed_by VARCHAR(100),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_products (product_name, price, stock) VALUES
    ('Widget A', 19.99, 100),
    ('Widget B', 29.99, 50);

CREATE FUNCTION audit_product_changes() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_products_audit (
        product_id, old_price, new_price, old_stock, new_stock, changed_by
    ) VALUES (
        NEW.product_id, OLD.price, NEW.price, OLD.stock, NEW.stock, CURRENT_USER
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_products
AFTER UPDATE ON test_products
FOR EACH ROW
EXECUTE FUNCTION audit_product_changes();

UPDATE test_products SET price = 24.99 WHERE product_id = 1;
UPDATE test_products SET stock = 75 WHERE product_id = 2;

SELECT product_id, product_name, price, stock FROM test_products ORDER BY product_id;
SELECT audit_id, product_id, old_price, new_price, old_stock, new_stock FROM test_products_audit ORDER BY audit_id;

-- ============================================================================
-- Section 2: Change History with Details
-- ============================================================================

CREATE TABLE test_documents (
    doc_id SERIAL PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    version INT DEFAULT 1,
    updated_at TIMESTAMP
);

CREATE TABLE test_document_history (
    history_id SERIAL PRIMARY KEY,
    doc_id INT,
    old_title VARCHAR(200),
    new_title VARCHAR(200),
    old_content TEXT,
    new_content TEXT,
    version INT,
    change_type VARCHAR(50),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_documents (title, content) VALUES
    ('Document 1', 'Original content 1'),
    ('Document 2', 'Original content 2');

CREATE FUNCTION track_document_changes() RETURNS TRIGGER AS $$
DECLARE
    change_desc VARCHAR(50);
BEGIN
    -- Determine what changed
    IF NEW.title IS DISTINCT FROM OLD.title AND NEW.content IS DISTINCT FROM OLD.content THEN
        change_desc := 'TITLE_AND_CONTENT';
    ELSIF NEW.title IS DISTINCT FROM OLD.title THEN
        change_desc := 'TITLE_ONLY';
    ELSIF NEW.content IS DISTINCT FROM OLD.content THEN
        change_desc := 'CONTENT_ONLY';
    ELSE
        change_desc := 'NO_CHANGE';
    END IF;

    INSERT INTO test_document_history (
        doc_id, old_title, new_title, old_content, new_content, version, change_type
    ) VALUES (
        NEW.doc_id, OLD.title, NEW.title, OLD.content, NEW.content, NEW.version, change_desc
    );

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_document_history
AFTER UPDATE ON test_documents
FOR EACH ROW
EXECUTE FUNCTION track_document_changes();

UPDATE test_documents SET title = 'Updated Document 1' WHERE doc_id = 1;
UPDATE test_documents SET content = 'Updated content 2' WHERE doc_id = 2;

SELECT doc_id, title, version FROM test_documents ORDER BY doc_id;
SELECT history_id, doc_id, old_title, new_title, change_type FROM test_document_history ORDER BY history_id;

-- ============================================================================
-- Section 3: Conditional AFTER UPDATE (WHEN Clause)
-- ============================================================================

CREATE TABLE test_inventory (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT,
    reorder_level INT DEFAULT 10
);

CREATE TABLE test_reorder_alerts (
    alert_id SERIAL PRIMARY KEY,
    item_id INT,
    item_name VARCHAR(200),
    current_quantity INT,
    reorder_level INT,
    alerted_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_inventory (item_name, quantity, reorder_level) VALUES
    ('Item A', 50, 10),
    ('Item B', 15, 20);

CREATE FUNCTION alert_low_inventory() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_reorder_alerts (item_id, item_name, current_quantity, reorder_level)
    VALUES (NEW.item_id, NEW.item_name, NEW.quantity, NEW.reorder_level);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Only fire when quantity drops below reorder level
CREATE TRIGGER trg_low_inventory_alert
AFTER UPDATE ON test_inventory
FOR EACH ROW
WHEN (NEW.quantity < NEW.reorder_level AND OLD.quantity >= OLD.reorder_level)
EXECUTE FUNCTION alert_low_inventory();

UPDATE test_inventory SET quantity = 8 WHERE item_id = 1;  -- Triggers alert

SELECT item_id, item_name, quantity, reorder_level FROM test_inventory ORDER BY item_id;
SELECT alert_id, item_name, current_quantity, reorder_level FROM test_reorder_alerts;

-- ============================================================================
-- Section 4: Cascade Updates to Related Tables
-- ============================================================================

CREATE TABLE test_categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(100),
    display_name VARCHAR(100)
);

CREATE TABLE test_items (
    item_id SERIAL PRIMARY KEY,
    category_id INT REFERENCES test_categories(category_id),
    item_name VARCHAR(200),
    category_display VARCHAR(100)
);

INSERT INTO test_categories (category_name, display_name) VALUES ('electronics', 'Electronics');
INSERT INTO test_items (category_id, item_name, category_display) VALUES
    (1, 'Laptop', 'Electronics'),
    (1, 'Phone', 'Electronics');

CREATE FUNCTION cascade_category_display() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.display_name IS DISTINCT FROM OLD.display_name THEN
        UPDATE test_items
        SET category_display = NEW.display_name
        WHERE category_id = NEW.category_id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cascade_category_display
AFTER UPDATE ON test_categories
FOR EACH ROW
EXECUTE FUNCTION cascade_category_display();

UPDATE test_categories SET display_name = 'Consumer Electronics' WHERE category_id = 1;

SELECT category_id, category_name, display_name FROM test_categories;
SELECT item_id, item_name, category_display FROM test_items ORDER BY item_id;

-- ============================================================================
-- Section 5: Notification System
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2),
    status VARCHAR(20)
);

CREATE TABLE test_notifications (
    notification_id SERIAL PRIMARY KEY,
    account_id INT,
    notification_type VARCHAR(50),
    message TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_accounts (account_name, balance, status) VALUES
    ('Account A', 1000.00, 'active'),
    ('Account B', 100.00, 'active');

CREATE FUNCTION notify_account_changes() RETURNS TRIGGER AS $$
BEGIN
    -- Notify on status change
    IF NEW.status IS DISTINCT FROM OLD.status THEN
        INSERT INTO test_notifications (account_id, notification_type, message)
        VALUES (
            NEW.account_id,
            'STATUS_CHANGE',
            'Account status changed from ' || OLD.status || ' to ' || NEW.status
        );
    END IF;

    -- Notify on low balance
    IF NEW.balance < 50 AND OLD.balance >= 50 THEN
        INSERT INTO test_notifications (account_id, notification_type, message)
        VALUES (
            NEW.account_id,
            'LOW_BALANCE',
            'Account balance dropped below $50: $' || NEW.balance
        );
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_notify_account
AFTER UPDATE ON test_accounts
FOR EACH ROW
EXECUTE FUNCTION notify_account_changes();

UPDATE test_accounts SET status = 'suspended' WHERE account_id = 1;
UPDATE test_accounts SET balance = 25.00 WHERE account_id = 2;

SELECT account_id, account_name, balance, status FROM test_accounts ORDER BY account_id;
SELECT notification_id, account_id, notification_type, message FROM test_notifications ORDER BY notification_id;

-- ============================================================================
-- Section 6: Update Statistics/Aggregates
-- ============================================================================

CREATE TABLE test_sales (
    sale_id SERIAL PRIMARY KEY,
    product_id INT,
    quantity INT,
    sale_amount NUMERIC(12,2),
    sale_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE test_product_stats (
    product_id INT PRIMARY KEY,
    total_sales NUMERIC(15,2) DEFAULT 0,
    units_sold INT DEFAULT 0,
    last_updated TIMESTAMP
);

INSERT INTO test_sales (product_id, quantity, sale_amount) VALUES (1, 5, 99.95);
INSERT INTO test_product_stats (product_id, total_sales, units_sold, last_updated) VALUES
    (1, 99.95, 5, CURRENT_TIMESTAMP);

CREATE FUNCTION update_product_stats() RETURNS TRIGGER AS $$
DECLARE
    amount_delta NUMERIC(12,2);
    quantity_delta INT;
BEGIN
    -- Calculate deltas
    amount_delta := NEW.sale_amount - OLD.sale_amount;
    quantity_delta := NEW.quantity - OLD.quantity;

    -- Update stats
    UPDATE test_product_stats
    SET total_sales = total_sales + amount_delta,
        units_sold = units_sold + quantity_delta,
        last_updated = CURRENT_TIMESTAMP
    WHERE product_id = NEW.product_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_product_stats
AFTER UPDATE ON test_sales
FOR EACH ROW
WHEN (NEW.sale_amount IS DISTINCT FROM OLD.sale_amount OR NEW.quantity IS DISTINCT FROM OLD.quantity)
EXECUTE FUNCTION update_product_stats();

UPDATE test_sales SET quantity = 8, sale_amount = 159.92 WHERE sale_id = 1;

SELECT product_id, total_sales, units_sold FROM test_product_stats;

-- ============================================================================
-- Section 7: Track Specific Column Changes
-- ============================================================================

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200),
    salary NUMERIC(12,2),
    department VARCHAR(100),
    title VARCHAR(100)
);

CREATE TABLE test_salary_changes (
    change_id SERIAL PRIMARY KEY,
    emp_id INT,
    old_salary NUMERIC(12,2),
    new_salary NUMERIC(12,2),
    percent_change NUMERIC(5,2),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_employees (emp_name, salary, department, title) VALUES
    ('Alice Smith', 75000.00, 'Engineering', 'Developer'),
    ('Bob Jones', 80000.00, 'Sales', 'Sales Manager');

CREATE FUNCTION track_salary_changes() RETURNS TRIGGER AS $$
DECLARE
    pct_change NUMERIC(5,2);
BEGIN
    pct_change := ((NEW.salary - OLD.salary) / OLD.salary) * 100;

    INSERT INTO test_salary_changes (emp_id, old_salary, new_salary, percent_change)
    VALUES (NEW.emp_id, OLD.salary, NEW.salary, pct_change);

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_salary
AFTER UPDATE ON test_employees
FOR EACH ROW
WHEN (NEW.salary IS DISTINCT FROM OLD.salary)
EXECUTE FUNCTION track_salary_changes();

UPDATE test_employees SET salary = 82500.00 WHERE emp_id = 1;

SELECT emp_id, emp_name, salary FROM test_employees ORDER BY emp_id;
SELECT change_id, emp_id, old_salary, new_salary, percent_change FROM test_salary_changes;

-- ============================================================================
-- Section 8: Event Logging with JSONB
-- ============================================================================

CREATE TABLE test_configurations (
    config_id SERIAL PRIMARY KEY,
    config_name VARCHAR(100),
    config_value JSONB
);

CREATE TABLE test_config_events (
    event_id SERIAL PRIMARY KEY,
    config_id INT,
    event_type VARCHAR(50),
    old_value JSONB,
    new_value JSONB,
    event_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_configurations (config_name, config_value) VALUES
    ('app_settings', '{"theme": "light", "language": "en", "notifications": true}'::JSONB);

CREATE FUNCTION log_config_changes() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_config_events (config_id, event_type, old_value, new_value)
    VALUES (NEW.config_id, 'CONFIG_UPDATE', OLD.config_value, NEW.config_value);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_config_changes
AFTER UPDATE ON test_configurations
FOR EACH ROW
WHEN (NEW.config_value IS DISTINCT FROM OLD.config_value)
EXECUTE FUNCTION log_config_changes();

UPDATE test_configurations
SET config_value = '{"theme": "dark", "language": "en", "notifications": true}'::JSONB
WHERE config_id = 1;

SELECT config_id, config_name, config_value FROM test_configurations;
SELECT event_id, config_id, event_type, old_value, new_value FROM test_config_events;

-- ============================================================================
-- Section 9: Multi-Table Update Synchronization
-- ============================================================================

CREATE TABLE test_users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    is_active BOOLEAN DEFAULT TRUE
);

CREATE TABLE test_user_sessions (
    session_id SERIAL PRIMARY KEY,
    user_id INT REFERENCES test_users(user_id),
    session_token VARCHAR(200),
    is_valid BOOLEAN DEFAULT TRUE
);

INSERT INTO test_users (username, email) VALUES ('alice', 'alice@example.com');
INSERT INTO test_user_sessions (user_id, session_token) VALUES (1, 'token_abc123');

CREATE FUNCTION invalidate_sessions_on_deactivation() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.is_active = FALSE AND OLD.is_active = TRUE THEN
        UPDATE test_user_sessions
        SET is_valid = FALSE
        WHERE user_id = NEW.user_id AND is_valid = TRUE;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_invalidate_sessions
AFTER UPDATE ON test_users
FOR EACH ROW
EXECUTE FUNCTION invalidate_sessions_on_deactivation();

UPDATE test_users SET is_active = FALSE WHERE user_id = 1;

SELECT user_id, username, is_active FROM test_users;
SELECT session_id, user_id, is_valid FROM test_user_sessions;

-- ============================================================================
-- Section 10: Performance Metrics Tracking
-- ============================================================================

CREATE TABLE test_api_endpoints (
    endpoint_id SERIAL PRIMARY KEY,
    endpoint_path VARCHAR(200),
    avg_response_time NUMERIC(10,2),
    request_count BIGINT,
    last_updated TIMESTAMP
);

CREATE TABLE test_performance_history (
    history_id SERIAL PRIMARY KEY,
    endpoint_id INT,
    old_avg_time NUMERIC(10,2),
    new_avg_time NUMERIC(10,2),
    performance_change NUMERIC(5,2),
    recorded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_api_endpoints (endpoint_path, avg_response_time, request_count, last_updated) VALUES
    ('/api/users', 150.00, 1000, CURRENT_TIMESTAMP);

CREATE FUNCTION track_performance_changes() RETURNS TRIGGER AS $$
DECLARE
    perf_change NUMERIC(5,2);
BEGIN
    IF NEW.avg_response_time IS DISTINCT FROM OLD.avg_response_time THEN
        perf_change := ((NEW.avg_response_time - OLD.avg_response_time) / OLD.avg_response_time) * 100;

        INSERT INTO test_performance_history (endpoint_id, old_avg_time, new_avg_time, performance_change)
        VALUES (NEW.endpoint_id, OLD.avg_response_time, NEW.avg_response_time, perf_change);

        -- Alert on significant degradation
        IF perf_change > 20 THEN
            RAISE WARNING 'Performance degradation detected: %% increase for endpoint %',
                perf_change, NEW.endpoint_path;
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_performance
AFTER UPDATE ON test_api_endpoints
FOR EACH ROW
EXECUTE FUNCTION track_performance_changes();

UPDATE test_api_endpoints SET avg_response_time = 200.00 WHERE endpoint_id = 1;

SELECT endpoint_id, endpoint_path, avg_response_time FROM test_api_endpoints;
SELECT history_id, endpoint_id, old_avg_time, new_avg_time, performance_change FROM test_performance_history;

-- ============================================================================
-- Section 11: Compliance/Audit Requirements
-- ============================================================================

CREATE TABLE test_financial_records (
    record_id SERIAL PRIMARY KEY,
    account_number VARCHAR(50),
    amount NUMERIC(15,2),
    record_type VARCHAR(50),
    is_audited BOOLEAN DEFAULT FALSE
);

CREATE TABLE test_audit_trail (
    audit_id SERIAL PRIMARY KEY,
    record_id INT,
    field_name VARCHAR(100),
    old_value TEXT,
    new_value TEXT,
    changed_by VARCHAR(100),
    ip_address INET,
    audit_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_financial_records (account_number, amount, record_type) VALUES
    ('ACC-001', 10000.00, 'debit');

CREATE FUNCTION create_audit_trail() RETURNS TRIGGER AS $$
BEGIN
    -- Audit amount changes
    IF NEW.amount IS DISTINCT FROM OLD.amount THEN
        INSERT INTO test_audit_trail (record_id, field_name, old_value, new_value, changed_by, ip_address)
        VALUES (
            NEW.record_id,
            'amount',
            OLD.amount::TEXT,
            NEW.amount::TEXT,
            CURRENT_USER,
            '127.0.0.1'::INET
        );
    END IF;

    -- Audit type changes
    IF NEW.record_type IS DISTINCT FROM OLD.record_type THEN
        INSERT INTO test_audit_trail (record_id, field_name, old_value, new_value, changed_by, ip_address)
        VALUES (
            NEW.record_id,
            'record_type',
            OLD.record_type,
            NEW.record_type,
            CURRENT_USER,
            '127.0.0.1'::INET
        );
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_financial_audit
AFTER UPDATE ON test_financial_records
FOR EACH ROW
EXECUTE FUNCTION create_audit_trail();

UPDATE test_financial_records SET amount = 12500.00, record_type = 'credit' WHERE record_id = 1;

SELECT record_id, account_number, amount, record_type FROM test_financial_records;
SELECT audit_id, record_id, field_name, old_value, new_value, changed_by FROM test_audit_trail ORDER BY audit_id;

-- ============================================================================
-- Section 12: Cache Invalidation
-- ============================================================================

CREATE TABLE test_products_cache (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    cache_key VARCHAR(200)
);

CREATE TABLE test_cache_invalidations (
    invalidation_id SERIAL PRIMARY KEY,
    cache_key VARCHAR(200),
    reason VARCHAR(100),
    invalidated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_products_cache (product_name, price, cache_key) VALUES
    ('Product A', 29.99, 'product:1');

CREATE FUNCTION invalidate_product_cache() RETURNS TRIGGER AS $$
BEGIN
    -- Invalidate individual product cache
    INSERT INTO test_cache_invalidations (cache_key, reason)
    VALUES (NEW.cache_key, 'PRODUCT_UPDATE');

    -- Invalidate list cache
    INSERT INTO test_cache_invalidations (cache_key, reason)
    VALUES ('products:list', 'PRODUCT_UPDATE');

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_invalidate_cache
AFTER UPDATE ON test_products_cache
FOR EACH ROW
EXECUTE FUNCTION invalidate_product_cache();

UPDATE test_products_cache SET price = 34.99 WHERE product_id = 1;

SELECT invalidation_id, cache_key, reason FROM test_cache_invalidations ORDER BY invalidation_id;

-- ============================================================================
-- Section 13: Multiple AFTER UPDATE Triggers
-- ============================================================================

CREATE TABLE test_multi_update (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_log_update_a (
    log_id SERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_log_update_b (
    log_id SERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_multi_update (value) VALUES (10);

CREATE FUNCTION log_update_a() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_log_update_a (message)
    VALUES ('Update A: ' || OLD.value || ' -> ' || NEW.value);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_update_b() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_log_update_b (message)
    VALUES ('Update B: ' || OLD.value || ' -> ' || NEW.value);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_a
AFTER UPDATE ON test_multi_update
FOR EACH ROW
EXECUTE FUNCTION log_update_a();

CREATE TRIGGER trg_log_b
AFTER UPDATE ON test_multi_update
FOR EACH ROW
EXECUTE FUNCTION log_update_b();

UPDATE test_multi_update SET value = 20 WHERE id = 1;

SELECT log_id, message FROM test_log_update_a;
SELECT log_id, message FROM test_log_update_b;

-- ============================================================================
-- Section 14: Workflow State Tracking
-- ============================================================================

CREATE TABLE test_tasks (
    task_id SERIAL PRIMARY KEY,
    task_name VARCHAR(200),
    status VARCHAR(50),
    assigned_to VARCHAR(100)
);

CREATE TABLE test_task_state_history (
    history_id SERIAL PRIMARY KEY,
    task_id INT,
    old_status VARCHAR(50),
    new_status VARCHAR(50),
    transition_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    duration_in_old_status INTERVAL
);

INSERT INTO test_tasks (task_name, status, assigned_to) VALUES
    ('Task 1', 'todo', 'alice');

CREATE FUNCTION track_task_state() RETURNS TRIGGER AS $$
DECLARE
    time_in_state INTERVAL;
BEGIN
    IF NEW.status IS DISTINCT FROM OLD.status THEN
        -- Calculate time spent in old status
        time_in_state := CURRENT_TIMESTAMP - OLD.updated_at;

        INSERT INTO test_task_state_history (task_id, old_status, new_status, duration_in_old_status)
        VALUES (NEW.task_id, OLD.status, NEW.status, time_in_state);
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Add updated_at column first
ALTER TABLE test_tasks ADD COLUMN updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP;

CREATE TRIGGER trg_track_task_state
AFTER UPDATE ON test_tasks
FOR EACH ROW
EXECUTE FUNCTION track_task_state();

UPDATE test_tasks SET status = 'in_progress' WHERE task_id = 1;

SELECT task_id, task_name, status FROM test_tasks;
SELECT history_id, task_id, old_status, new_status FROM test_task_state_history;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_after_update_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_after_update_best_practices VALUES
    (1, 'AFTER UPDATE triggers fire after row update completes'),
    (2, 'Can access both OLD and NEW values (read-only)'),
    (3, 'Cannot modify the row being updated'),
    (4, 'Perfect for audit logging and change tracking'),
    (5, 'Use for notification and alert systems'),
    (6, 'Great for cascading updates to related tables'),
    (7, 'Ideal for updating statistics and aggregates'),
    (8, 'Use WHEN clause to fire only on specific column changes'),
    (9, 'Return value is ignored (return NEW or NULL)'),
    (10, 'Multiple triggers execute alphabetically by name'),
    (11, 'Keep trigger logic fast to avoid slowing updates'),
    (12, 'Use for cache invalidation'),
    (13, 'Perfect for compliance and audit requirements'),
    (14, 'Track workflow state transitions and durations'),
    (15, 'Consider async processing for expensive operations');

SELECT id, guideline FROM test_after_update_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_after_update_best_practices;
DROP TABLE test_task_state_history;
DROP TABLE test_tasks;
DROP FUNCTION track_task_state();
DROP TABLE test_log_update_b;
DROP TABLE test_log_update_a;
DROP TABLE test_multi_update;
DROP FUNCTION log_update_b();
DROP FUNCTION log_update_a();
DROP TABLE test_cache_invalidations;
DROP TABLE test_products_cache;
DROP FUNCTION invalidate_product_cache();
DROP TABLE test_audit_trail;
DROP TABLE test_financial_records;
DROP FUNCTION create_audit_trail();
DROP TABLE test_performance_history;
DROP TABLE test_api_endpoints;
DROP FUNCTION track_performance_changes();
DROP TABLE test_user_sessions;
DROP TABLE test_users;
DROP FUNCTION invalidate_sessions_on_deactivation();
DROP TABLE test_config_events;
DROP TABLE test_configurations;
DROP FUNCTION log_config_changes();
DROP TABLE test_salary_changes;
DROP TABLE test_employees;
DROP FUNCTION track_salary_changes();
DROP TABLE test_product_stats;
DROP TABLE test_sales;
DROP FUNCTION update_product_stats();
DROP TABLE test_notifications;
DROP TABLE test_accounts;
DROP FUNCTION notify_account_changes();
DROP TABLE test_items;
DROP TABLE test_categories;
DROP FUNCTION cascade_category_display();
DROP TABLE test_reorder_alerts;
DROP TABLE test_inventory;
DROP FUNCTION alert_low_inventory();
DROP TABLE test_document_history;
DROP TABLE test_documents;
DROP FUNCTION track_document_changes();
DROP TABLE test_products_audit;
DROP TABLE test_products;
DROP FUNCTION audit_product_changes();

DROP DATABASE test_after_update_row_db;

-- End of AFTER UPDATE row-level trigger tests
