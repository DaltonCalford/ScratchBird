-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - AFTER INSERT Row-Level
-- Description: Comprehensive AFTER INSERT row-level trigger testing
-- ============================================================================

-- AFTER INSERT triggers fire after each row is inserted
-- Cannot modify NEW record (already inserted)
-- Can read NEW values and perform side effects
-- Use cases: audit logging, notifications, cascade operations, stats updates

-- Create test database
CREATE DATABASE test_after_insert_row_db;
USE test_after_insert_row_db;

-- ============================================================================
-- Section 1: Basic AFTER INSERT Trigger (Audit Log)
-- ============================================================================

CREATE TABLE test_users (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    email VARCHAR(200),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_users_audit (
    audit_id SERIAL PRIMARY KEY,
    user_id INT,
    username VARCHAR(100),
    email VARCHAR(200),
    action VARCHAR(20),
    action_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION audit_user_insert() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_users_audit (user_id, username, email, action)
    VALUES (NEW.user_id, NEW.username, NEW.email, 'INSERT');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_user_insert
AFTER INSERT ON test_users
FOR EACH ROW
EXECUTE FUNCTION audit_user_insert();

-- Test
INSERT INTO test_users (username, email) VALUES
    ('alice', 'alice@example.com'),
    ('bob', 'bob@example.com');

SELECT user_id, username, email FROM test_users ORDER BY user_id;
SELECT audit_id, user_id, username, action, action_timestamp FROM test_users_audit ORDER BY audit_id;

-- ============================================================================
-- Section 2: Cascade Insert to Related Table
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    email VARCHAR(200)
);

CREATE TABLE test_customer_preferences (
    pref_id SERIAL PRIMARY KEY,
    customer_id INT REFERENCES test_customers(customer_id),
    theme VARCHAR(20) DEFAULT 'light',
    language VARCHAR(10) DEFAULT 'en',
    notifications_enabled BOOLEAN DEFAULT TRUE
);

CREATE FUNCTION create_default_preferences() RETURNS TRIGGER AS $$
BEGIN
    -- Auto-create default preferences for new customer
    INSERT INTO test_customer_preferences (customer_id)
    VALUES (NEW.customer_id);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_create_preferences
AFTER INSERT ON test_customers
FOR EACH ROW
EXECUTE FUNCTION create_default_preferences();

INSERT INTO test_customers (customer_name, email) VALUES
    ('Alice Corporation', 'alice@corp.com'),
    ('Bob Industries', 'bob@industries.com');

SELECT customer_id, customer_name FROM test_customers ORDER BY customer_id;
SELECT pref_id, customer_id, theme, language FROM test_customer_preferences ORDER BY pref_id;

-- ============================================================================
-- Section 3: Update Statistics/Counters
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    category VARCHAR(100)
);

CREATE TABLE test_category_stats (
    category VARCHAR(100) PRIMARY KEY,
    product_count INT DEFAULT 0,
    last_updated TIMESTAMP
);

-- Initialize categories
INSERT INTO test_category_stats (category, product_count, last_updated) VALUES
    ('Electronics', 0, CURRENT_TIMESTAMP),
    ('Books', 0, CURRENT_TIMESTAMP),
    ('Clothing', 0, CURRENT_TIMESTAMP);

CREATE FUNCTION update_category_count() RETURNS TRIGGER AS $$
BEGIN
    -- Increment product count for category
    UPDATE test_category_stats
    SET product_count = product_count + 1,
        last_updated = CURRENT_TIMESTAMP
    WHERE category = NEW.category;

    -- Create category if doesn't exist
    IF NOT FOUND THEN
        INSERT INTO test_category_stats (category, product_count, last_updated)
        VALUES (NEW.category, 1, CURRENT_TIMESTAMP);
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_category_count
AFTER INSERT ON test_products
FOR EACH ROW
EXECUTE FUNCTION update_category_count();

INSERT INTO test_products (product_name, category) VALUES
    ('Laptop', 'Electronics'),
    ('Phone', 'Electronics'),
    ('Novel', 'Books'),
    ('Shirt', 'Clothing'),
    ('Tablet', 'Electronics');

SELECT category, product_count, last_updated FROM test_category_stats ORDER BY category;

-- ============================================================================
-- Section 4: Notification/Alert Trigger
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    order_total NUMERIC(12,2),
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_notifications (
    notification_id SERIAL PRIMARY KEY,
    message TEXT,
    priority VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION notify_large_order() RETURNS TRIGGER AS $$
BEGIN
    -- Notify if order total > $10,000
    IF NEW.order_total > 10000 THEN
        INSERT INTO test_notifications (message, priority)
        VALUES (
            'Large order detected: Order #' || NEW.order_id || ' - $' || NEW.order_total,
            'HIGH'
        );
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_notify_large_order
AFTER INSERT ON test_orders
FOR EACH ROW
EXECUTE FUNCTION notify_large_order();

INSERT INTO test_orders (customer_id, order_total) VALUES
    (100, 5000.00),
    (101, 15000.00),  -- Triggers notification
    (102, 25000.00),  -- Triggers notification
    (103, 500.00);

SELECT order_id, customer_id, order_total FROM test_orders ORDER BY order_id;
SELECT notification_id, message, priority FROM test_notifications ORDER BY notification_id;

-- ============================================================================
-- Section 5: History/Changelog Tracking
-- ============================================================================

CREATE TABLE test_documents (
    doc_id SERIAL PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    version INT DEFAULT 1
);

CREATE TABLE test_document_history (
    history_id SERIAL PRIMARY KEY,
    doc_id INT,
    title VARCHAR(200),
    content TEXT,
    version INT,
    change_type VARCHAR(20),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION track_document_history() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_document_history (doc_id, title, content, version, change_type)
    VALUES (NEW.doc_id, NEW.title, NEW.content, NEW.version, 'CREATE');
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_track_document_history
AFTER INSERT ON test_documents
FOR EACH ROW
EXECUTE FUNCTION track_document_history();

INSERT INTO test_documents (title, content) VALUES
    ('Document 1', 'Initial content for document 1'),
    ('Document 2', 'Initial content for document 2');

SELECT doc_id, title, version FROM test_documents ORDER BY doc_id;
SELECT history_id, doc_id, title, change_type, changed_at FROM test_document_history ORDER BY history_id;

-- ============================================================================
-- Section 6: Conditional AFTER INSERT (WHEN Clause)
-- ============================================================================

CREATE TABLE test_transactions (
    txn_id SERIAL PRIMARY KEY,
    account_id INT,
    amount NUMERIC(12,2),
    txn_type VARCHAR(20),
    txn_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_suspicious_transactions (
    suspicious_id SERIAL PRIMARY KEY,
    txn_id INT,
    account_id INT,
    amount NUMERIC(12,2),
    reason TEXT,
    flagged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION flag_suspicious_transaction() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_suspicious_transactions (txn_id, account_id, amount, reason)
    VALUES (
        NEW.txn_id,
        NEW.account_id,
        NEW.amount,
        'Large withdrawal flagged for review'
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Only fire for large withdrawals
CREATE TRIGGER trg_flag_suspicious
AFTER INSERT ON test_transactions
FOR EACH ROW
WHEN (NEW.txn_type = 'withdrawal' AND NEW.amount > 5000)
EXECUTE FUNCTION flag_suspicious_transaction();

INSERT INTO test_transactions (account_id, amount, txn_type) VALUES
    (1001, 100.00, 'withdrawal'),     -- Not flagged
    (1002, 10000.00, 'withdrawal'),   -- Flagged
    (1003, 5000.00, 'deposit'),       -- Not flagged (deposit)
    (1004, 7500.00, 'withdrawal');    -- Flagged

SELECT txn_id, account_id, amount, txn_type FROM test_transactions ORDER BY txn_id;
SELECT suspicious_id, txn_id, account_id, amount, reason FROM test_suspicious_transactions ORDER BY suspicious_id;

-- ============================================================================
-- Section 7: Synchronize Derived Data
-- ============================================================================

CREATE TABLE test_order_items (
    item_id SERIAL PRIMARY KEY,
    order_id INT,
    product_name VARCHAR(200),
    quantity INT,
    unit_price NUMERIC(10,2),
    line_total NUMERIC(12,2)
);

CREATE TABLE test_order_totals (
    order_id INT PRIMARY KEY,
    subtotal NUMERIC(12,2) DEFAULT 0,
    item_count INT DEFAULT 0,
    last_updated TIMESTAMP
);

CREATE FUNCTION update_order_totals() RETURNS TRIGGER AS $$
BEGIN
    -- Insert or update order totals
    INSERT INTO test_order_totals (order_id, subtotal, item_count, last_updated)
    VALUES (NEW.order_id, NEW.line_total, 1, CURRENT_TIMESTAMP)
    ON CONFLICT (order_id) DO UPDATE
    SET subtotal = test_order_totals.subtotal + NEW.line_total,
        item_count = test_order_totals.item_count + 1,
        last_updated = CURRENT_TIMESTAMP;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_order_totals
AFTER INSERT ON test_order_items
FOR EACH ROW
EXECUTE FUNCTION update_order_totals();

INSERT INTO test_order_items (order_id, product_name, quantity, unit_price, line_total) VALUES
    (1, 'Product A', 2, 19.99, 39.98),
    (1, 'Product B', 1, 29.99, 29.99),
    (2, 'Product C', 5, 9.99, 49.95);

SELECT order_id, subtotal, item_count FROM test_order_totals ORDER BY order_id;

-- ============================================================================
-- Section 8: Insert into Multiple Related Tables
-- ============================================================================

CREATE TABLE test_employees (
    emp_id SERIAL PRIMARY KEY,
    emp_name VARCHAR(200),
    department VARCHAR(100),
    hire_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE test_employee_accounts (
    account_id SERIAL PRIMARY KEY,
    emp_id INT,
    username VARCHAR(100),
    email VARCHAR(200)
);

CREATE TABLE test_employee_benefits (
    benefit_id SERIAL PRIMARY KEY,
    emp_id INT,
    health_insurance BOOLEAN DEFAULT TRUE,
    retirement_plan BOOLEAN DEFAULT TRUE
);

CREATE FUNCTION setup_new_employee() RETURNS TRIGGER AS $$
BEGIN
    -- Create account
    INSERT INTO test_employee_accounts (emp_id, username, email)
    VALUES (
        NEW.emp_id,
        LOWER(REPLACE(NEW.emp_name, ' ', '.')),
        LOWER(REPLACE(NEW.emp_name, ' ', '.')) || '@company.com'
    );

    -- Enroll in benefits
    INSERT INTO test_employee_benefits (emp_id)
    VALUES (NEW.emp_id);

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_setup_new_employee
AFTER INSERT ON test_employees
FOR EACH ROW
EXECUTE FUNCTION setup_new_employee();

INSERT INTO test_employees (emp_name, department) VALUES
    ('Alice Smith', 'Engineering'),
    ('Bob Jones', 'Sales');

SELECT emp_id, emp_name, department FROM test_employees ORDER BY emp_id;
SELECT account_id, emp_id, username, email FROM test_employee_accounts ORDER BY account_id;
SELECT benefit_id, emp_id, health_insurance, retirement_plan FROM test_employee_benefits ORDER BY benefit_id;

-- ============================================================================
-- Section 9: Event Logging with Details
-- ============================================================================

CREATE TABLE test_inventory (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT,
    warehouse_location VARCHAR(100)
);

CREATE TABLE test_inventory_events (
    event_id SERIAL PRIMARY KEY,
    item_id INT,
    event_type VARCHAR(50),
    event_details JSONB,
    event_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_inventory_event() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_inventory_events (item_id, event_type, event_details)
    VALUES (
        NEW.item_id,
        'ITEM_ADDED',
        jsonb_build_object(
            'item_name', NEW.item_name,
            'initial_quantity', NEW.quantity,
            'location', NEW.warehouse_location
        )
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_inventory_event
AFTER INSERT ON test_inventory
FOR EACH ROW
EXECUTE FUNCTION log_inventory_event();

INSERT INTO test_inventory (item_name, quantity, warehouse_location) VALUES
    ('Widget A', 100, 'Warehouse 1'),
    ('Widget B', 250, 'Warehouse 2');

SELECT item_id, item_name, quantity FROM test_inventory ORDER BY item_id;
SELECT event_id, item_id, event_type, event_details FROM test_inventory_events ORDER BY event_id;

-- ============================================================================
-- Section 10: Trigger Calling External Function
-- ============================================================================

CREATE TABLE test_blog_posts (
    post_id SERIAL PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    author_id INT,
    published_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_search_index (
    index_id SERIAL PRIMARY KEY,
    post_id INT,
    search_vector TSVECTOR,
    indexed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION index_blog_post() RETURNS TRIGGER AS $$
BEGIN
    -- Create full-text search index
    INSERT INTO test_search_index (post_id, search_vector)
    VALUES (
        NEW.post_id,
        to_tsvector('english', COALESCE(NEW.title, '') || ' ' || COALESCE(NEW.content, ''))
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_index_blog_post
AFTER INSERT ON test_blog_posts
FOR EACH ROW
EXECUTE FUNCTION index_blog_post();

INSERT INTO test_blog_posts (title, content, author_id) VALUES
    ('PostgreSQL Tutorial', 'Learn about PostgreSQL triggers and functions', 1),
    ('Database Best Practices', 'Best practices for database design', 2);

SELECT post_id, title, author_id FROM test_blog_posts ORDER BY post_id;
SELECT index_id, post_id, indexed_at FROM test_search_index ORDER BY index_id;

-- ============================================================================
-- Section 11: Quota/Limit Enforcement
-- ============================================================================

CREATE TABLE test_user_files (
    file_id SERIAL PRIMARY KEY,
    user_id INT,
    filename VARCHAR(200),
    file_size BIGINT,
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_user_quotas (
    user_id INT PRIMARY KEY,
    total_size BIGINT DEFAULT 0,
    file_count INT DEFAULT 0,
    max_size BIGINT DEFAULT 1073741824  -- 1GB
);

CREATE FUNCTION update_user_quota() RETURNS TRIGGER AS $$
DECLARE
    current_total BIGINT;
    quota_limit BIGINT;
BEGIN
    -- Get current quota
    SELECT total_size, max_size INTO current_total, quota_limit
    FROM test_user_quotas
    WHERE user_id = NEW.user_id;

    -- Create quota record if doesn't exist
    IF NOT FOUND THEN
        INSERT INTO test_user_quotas (user_id, total_size, file_count)
        VALUES (NEW.user_id, NEW.file_size, 1);
    ELSE
        -- Update quota
        UPDATE test_user_quotas
        SET total_size = total_size + NEW.file_size,
            file_count = file_count + 1
        WHERE user_id = NEW.user_id;

        -- Check if exceeded (just log, don't prevent)
        IF (current_total + NEW.file_size) > quota_limit THEN
            RAISE NOTICE 'User % exceeded quota: % / %', NEW.user_id, current_total + NEW.file_size, quota_limit;
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_update_user_quota
AFTER INSERT ON test_user_files
FOR EACH ROW
EXECUTE FUNCTION update_user_quota();

INSERT INTO test_user_files (user_id, filename, file_size) VALUES
    (100, 'document1.pdf', 1048576),    -- 1MB
    (100, 'photo.jpg', 2097152),        -- 2MB
    (101, 'video.mp4', 104857600);      -- 100MB

SELECT user_id, total_size, file_count FROM test_user_quotas ORDER BY user_id;

-- ============================================================================
-- Section 12: Cache Invalidation
-- ============================================================================

CREATE TABLE test_products_catalog (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2)
);

CREATE TABLE test_cache_invalidation (
    cache_id SERIAL PRIMARY KEY,
    cache_key VARCHAR(200),
    action VARCHAR(50),
    invalidated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION invalidate_product_cache() RETURNS TRIGGER AS $$
BEGIN
    -- Log cache invalidation
    INSERT INTO test_cache_invalidation (cache_key, action)
    VALUES ('product:' || NEW.product_id, 'INSERT');

    -- Could also invalidate list cache
    INSERT INTO test_cache_invalidation (cache_key, action)
    VALUES ('product:list', 'INSERT');

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_invalidate_cache
AFTER INSERT ON test_products_catalog
FOR EACH ROW
EXECUTE FUNCTION invalidate_product_cache();

INSERT INTO test_products_catalog (product_name, price) VALUES
    ('New Product A', 29.99),
    ('New Product B', 49.99);

SELECT cache_id, cache_key, action FROM test_cache_invalidation ORDER BY cache_id;

-- ============================================================================
-- Section 13: Multiple AFTER INSERT Triggers
-- ============================================================================

CREATE TABLE test_multi_after (
    id SERIAL PRIMARY KEY,
    data VARCHAR(200)
);

CREATE TABLE test_log_a (
    log_id SERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_log_b (
    log_id SERIAL PRIMARY KEY,
    message TEXT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION log_to_a() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_log_a (message) VALUES ('Logged to A: ' || NEW.data);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_to_b() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_log_b (message) VALUES ('Logged to B: ' || NEW.data);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_log_a
AFTER INSERT ON test_multi_after
FOR EACH ROW
EXECUTE FUNCTION log_to_a();

CREATE TRIGGER trg_log_b
AFTER INSERT ON test_multi_after
FOR EACH ROW
EXECUTE FUNCTION log_to_b();

INSERT INTO test_multi_after (data) VALUES ('Test Data 1'), ('Test Data 2');

SELECT log_id, message FROM test_log_a ORDER BY log_id;
SELECT log_id, message FROM test_log_b ORDER BY log_id;

-- ============================================================================
-- Section 14: Performance Considerations
-- ============================================================================

-- Demonstration: AFTER INSERT with minimal overhead
CREATE TABLE test_performance (
    id SERIAL PRIMARY KEY,
    value INT
);

CREATE TABLE test_performance_log (
    log_id SERIAL PRIMARY KEY,
    row_count INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION efficient_logging() RETURNS TRIGGER AS $$
BEGIN
    -- Efficient: just log the count every 100 rows (in practice would use counter)
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_efficient_log
AFTER INSERT ON test_performance
FOR EACH ROW
EXECUTE FUNCTION efficient_logging();

-- Insert test data
INSERT INTO test_performance (value)
SELECT i FROM generate_series(1, 1000) i;

SELECT COUNT(*) AS inserted_rows FROM test_performance;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_after_insert_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_after_insert_best_practices VALUES
    (1, 'AFTER INSERT triggers fire after row is committed to table'),
    (2, 'Cannot modify NEW record (already inserted)'),
    (3, 'Perfect for audit logging and history tracking'),
    (4, 'Use for cascade inserts to related tables'),
    (5, 'Great for updating statistics and counters'),
    (6, 'Ideal for notification and alert systems'),
    (7, 'Use WHEN clause for conditional execution'),
    (8, 'Return value is ignored (return NEW or NULL)'),
    (9, 'Can insert into multiple tables (side effects)'),
    (10, 'Multiple AFTER triggers execute in alphabetical order'),
    (11, 'Keep trigger logic fast to avoid slowing inserts'),
    (12, 'Consider async processing for heavy operations'),
    (13, 'Use for cache invalidation'),
    (14, 'Great for search index maintenance'),
    (15, 'Document side effects and dependencies');

SELECT id, guideline FROM test_after_insert_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_after_insert_best_practices;
DROP TABLE test_performance_log;
DROP TABLE test_performance;
DROP FUNCTION efficient_logging();
DROP TABLE test_log_b;
DROP TABLE test_log_a;
DROP TABLE test_multi_after;
DROP FUNCTION log_to_b();
DROP FUNCTION log_to_a();
DROP TABLE test_cache_invalidation;
DROP TABLE test_products_catalog;
DROP FUNCTION invalidate_product_cache();
DROP TABLE test_user_quotas;
DROP TABLE test_user_files;
DROP FUNCTION update_user_quota();
DROP TABLE test_search_index;
DROP TABLE test_blog_posts;
DROP FUNCTION index_blog_post();
DROP TABLE test_inventory_events;
DROP TABLE test_inventory;
DROP FUNCTION log_inventory_event();
DROP TABLE test_employee_benefits;
DROP TABLE test_employee_accounts;
DROP TABLE test_employees;
DROP FUNCTION setup_new_employee();
DROP TABLE test_order_totals;
DROP TABLE test_order_items;
DROP FUNCTION update_order_totals();
DROP TABLE test_suspicious_transactions;
DROP TABLE test_transactions;
DROP FUNCTION flag_suspicious_transaction();
DROP TABLE test_document_history;
DROP TABLE test_documents;
DROP FUNCTION track_document_history();
DROP TABLE test_notifications;
DROP TABLE test_orders;
DROP FUNCTION notify_large_order();
DROP TABLE test_category_stats;
DROP TABLE test_products;
DROP FUNCTION update_category_count();
DROP TABLE test_customer_preferences;
DROP TABLE test_customers;
DROP FUNCTION create_default_preferences();
DROP TABLE test_users_audit;
DROP TABLE test_users;
DROP FUNCTION audit_user_insert();

DROP DATABASE test_after_insert_row_db;

-- End of AFTER INSERT row-level trigger tests
