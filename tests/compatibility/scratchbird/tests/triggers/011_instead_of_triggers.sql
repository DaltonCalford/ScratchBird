-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - INSTEAD OF (View Triggers)
-- Description: Comprehensive INSTEAD OF trigger testing for views
-- ============================================================================

-- INSTEAD OF triggers are only for views (not tables)
-- Fire in place of INSERT/UPDATE/DELETE on non-updatable views
-- Allow making complex views appear updatable
-- Execute custom logic to modify underlying base tables

-- Create test database
CREATE DATABASE test_instead_of_triggers_db;
USE test_instead_of_triggers_db;

-- ============================================================================
-- Section 1: Basic INSTEAD OF INSERT on Simple View
-- ============================================================================

CREATE TABLE test_users_base (
    user_id SERIAL PRIMARY KEY,
    username VARCHAR(100) NOT NULL,
    email VARCHAR(200) NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE VIEW test_users_view AS
SELECT user_id, username, email
FROM test_users_base;

CREATE FUNCTION instead_of_insert_user() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_users_base (username, email)
    VALUES (NEW.username, NEW.email);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_insert_user
INSTEAD OF INSERT ON test_users_view
FOR EACH ROW
EXECUTE FUNCTION instead_of_insert_user();

-- Insert via the view
INSERT INTO test_users_view (username, email) VALUES
    ('alice', 'alice@example.com'),
    ('bob', 'bob@example.com');

SELECT user_id, username, email FROM test_users_view ORDER BY user_id;
SELECT user_id, username, email, created_at FROM test_users_base ORDER BY user_id;

-- ============================================================================
-- Section 2: INSTEAD OF UPDATE on Simple View
-- ============================================================================

CREATE TABLE test_products_base (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock INT,
    updated_at TIMESTAMP
);

INSERT INTO test_products_base (product_name, price, stock) VALUES
    ('Widget A', 29.99, 100),
    ('Widget B', 49.99, 50);

CREATE VIEW test_products_view AS
SELECT product_id, product_name, price, stock
FROM test_products_base;

CREATE FUNCTION instead_of_update_product() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_products_base
    SET product_name = NEW.product_name,
        price = NEW.price,
        stock = NEW.stock,
        updated_at = CURRENT_TIMESTAMP
    WHERE product_id = OLD.product_id;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_product
INSTEAD OF UPDATE ON test_products_view
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_product();

-- Update via the view
UPDATE test_products_view
SET price = 39.99, stock = 75
WHERE product_id = 1;

SELECT product_id, product_name, price, stock FROM test_products_view ORDER BY product_id;
SELECT product_id, updated_at FROM test_products_base WHERE product_id = 1;

-- ============================================================================
-- Section 3: INSTEAD OF DELETE on Simple View
-- ============================================================================

CREATE TABLE test_orders_base (
    order_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    order_total NUMERIC(12,2),
    is_deleted BOOLEAN DEFAULT FALSE,
    deleted_at TIMESTAMP
);

INSERT INTO test_orders_base (customer_name, order_total) VALUES
    ('Customer A', 100.00),
    ('Customer B', 200.00),
    ('Customer C', 150.00);

-- View shows only non-deleted orders
CREATE VIEW test_active_orders AS
SELECT order_id, customer_name, order_total
FROM test_orders_base
WHERE is_deleted = FALSE;

CREATE FUNCTION instead_of_delete_order() RETURNS TRIGGER AS $$
BEGIN
    -- Soft delete instead of hard delete
    UPDATE test_orders_base
    SET is_deleted = TRUE,
        deleted_at = CURRENT_TIMESTAMP
    WHERE order_id = OLD.order_id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_delete_order
INSTEAD OF DELETE ON test_active_orders
FOR EACH ROW
EXECUTE FUNCTION instead_of_delete_order();

-- Delete via the view (actually soft deletes)
DELETE FROM test_active_orders WHERE order_id = 1;

SELECT order_id, customer_name FROM test_active_orders ORDER BY order_id;  -- Should not show order 1
SELECT order_id, is_deleted, deleted_at FROM test_orders_base ORDER BY order_id;

-- ============================================================================
-- Section 4: Complex View with JOIN - INSTEAD OF INSERT
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200) NOT NULL
);

CREATE TABLE test_addresses (
    address_id SERIAL PRIMARY KEY,
    customer_id INT REFERENCES test_customers(customer_id),
    street VARCHAR(200),
    city VARCHAR(100),
    state VARCHAR(50),
    zip VARCHAR(20)
);

CREATE VIEW test_customer_addresses AS
SELECT
    c.customer_id,
    c.customer_name,
    a.address_id,
    a.street,
    a.city,
    a.state,
    a.zip
FROM test_customers c
LEFT JOIN test_addresses a ON c.customer_id = a.customer_id;

CREATE FUNCTION instead_of_insert_customer_address() RETURNS TRIGGER AS $$
DECLARE
    new_customer_id INT;
BEGIN
    -- Check if customer exists
    SELECT customer_id INTO new_customer_id
    FROM test_customers
    WHERE customer_name = NEW.customer_name;

    -- If not, create customer
    IF new_customer_id IS NULL THEN
        INSERT INTO test_customers (customer_name)
        VALUES (NEW.customer_name)
        RETURNING customer_id INTO new_customer_id;
    END IF;

    -- Insert address
    INSERT INTO test_addresses (customer_id, street, city, state, zip)
    VALUES (new_customer_id, NEW.street, NEW.city, NEW.state, NEW.zip);

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_insert_customer_address
INSTEAD OF INSERT ON test_customer_addresses
FOR EACH ROW
EXECUTE FUNCTION instead_of_insert_customer_address();

-- Insert via the complex view
INSERT INTO test_customer_addresses (customer_name, street, city, state, zip) VALUES
    ('John Doe', '123 Main St', 'Springfield', 'IL', '62701'),
    ('John Doe', '456 Oak Ave', 'Chicago', 'IL', '60601'),
    ('Jane Smith', '789 Elm St', 'Boston', 'MA', '02101');

SELECT customer_id, customer_name, address_id, street, city, state, zip
FROM test_customer_addresses
ORDER BY customer_id, address_id;

-- ============================================================================
-- Section 5: Complex View with JOIN - INSTEAD OF UPDATE
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    employee_name VARCHAR(200),
    department_id INT
);

CREATE TABLE test_departments (
    department_id SERIAL PRIMARY KEY,
    department_name VARCHAR(100)
);

INSERT INTO test_departments (department_name) VALUES ('Engineering'), ('Sales'), ('HR');
INSERT INTO test_employees (employee_name, department_id) VALUES
    ('Alice', 1),
    ('Bob', 1),
    ('Charlie', 2);

CREATE VIEW test_employee_dept_view AS
SELECT
    e.employee_id,
    e.employee_name,
    d.department_id,
    d.department_name
FROM test_employees e
JOIN test_departments d ON e.department_id = d.department_id;

CREATE FUNCTION instead_of_update_employee_dept() RETURNS TRIGGER AS $$
DECLARE
    dept_id INT;
BEGIN
    -- Find or create department
    SELECT department_id INTO dept_id
    FROM test_departments
    WHERE department_name = NEW.department_name;

    IF dept_id IS NULL THEN
        INSERT INTO test_departments (department_name)
        VALUES (NEW.department_name)
        RETURNING department_id INTO dept_id;
    END IF;

    -- Update employee
    UPDATE test_employees
    SET employee_name = NEW.employee_name,
        department_id = dept_id
    WHERE employee_id = OLD.employee_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_employee_dept
INSTEAD OF UPDATE ON test_employee_dept_view
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_employee_dept();

-- Update via the view (change Bob's department to new dept)
UPDATE test_employee_dept_view
SET department_name = 'Marketing'
WHERE employee_id = 2;

SELECT employee_id, employee_name, department_name
FROM test_employee_dept_view
ORDER BY employee_id;

SELECT department_id, department_name FROM test_departments ORDER BY department_id;

-- ============================================================================
-- Section 6: Complex View with JOIN - INSTEAD OF DELETE
-- ============================================================================

CREATE TABLE test_projects (
    project_id SERIAL PRIMARY KEY,
    project_name VARCHAR(200)
);

CREATE TABLE test_project_members (
    member_id SERIAL PRIMARY KEY,
    project_id INT REFERENCES test_projects(project_id),
    employee_id INT REFERENCES test_employees(employee_id)
);

INSERT INTO test_projects (project_name) VALUES ('Project Alpha'), ('Project Beta');
INSERT INTO test_project_members (project_id, employee_id) VALUES
    (1, 1), (1, 2), (2, 3);

CREATE VIEW test_project_assignments AS
SELECT
    pm.member_id,
    p.project_id,
    p.project_name,
    e.employee_id,
    e.employee_name
FROM test_project_members pm
JOIN test_projects p ON pm.project_id = p.project_id
JOIN test_employees e ON pm.employee_id = e.employee_id;

CREATE FUNCTION instead_of_delete_assignment() RETURNS TRIGGER AS $$
BEGIN
    -- Remove project assignment
    DELETE FROM test_project_members
    WHERE member_id = OLD.member_id;

    -- Check if project has no more members
    IF NOT EXISTS (
        SELECT 1 FROM test_project_members WHERE project_id = OLD.project_id
    ) THEN
        -- Archive or delete the project
        DELETE FROM test_projects WHERE project_id = OLD.project_id;
    END IF;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_delete_assignment
INSTEAD OF DELETE ON test_project_assignments
FOR EACH ROW
EXECUTE FUNCTION instead_of_delete_assignment();

-- Delete assignment via view
DELETE FROM test_project_assignments WHERE member_id = 3;

SELECT member_id, project_name, employee_name FROM test_project_assignments ORDER BY member_id;
SELECT project_id, project_name FROM test_projects ORDER BY project_id;  -- Project Beta should be gone

-- ============================================================================
-- Section 7: Conditional INSTEAD OF Triggers with Validation
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2),
    account_type VARCHAR(50),
    is_locked BOOLEAN DEFAULT FALSE
);

INSERT INTO test_accounts (account_name, balance, account_type) VALUES
    ('Checking', 1000.00, 'checking'),
    ('Savings', 5000.00, 'savings');

CREATE VIEW test_unlocked_accounts AS
SELECT account_id, account_name, balance, account_type
FROM test_accounts
WHERE is_locked = FALSE;

CREATE FUNCTION instead_of_update_account_with_validation() RETURNS TRIGGER AS $$
BEGIN
    -- Validation: Prevent negative balance
    IF NEW.balance < 0 THEN
        RAISE EXCEPTION 'Balance cannot be negative for account %', NEW.account_name;
    END IF;

    -- Validation: Savings accounts must maintain minimum balance
    IF NEW.account_type = 'savings' AND NEW.balance < 100.00 THEN
        RAISE EXCEPTION 'Savings account must maintain minimum balance of $100.00';
    END IF;

    -- Update the account
    UPDATE test_accounts
    SET account_name = NEW.account_name,
        balance = NEW.balance,
        account_type = NEW.account_type
    WHERE account_id = OLD.account_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_account
INSTEAD OF UPDATE ON test_unlocked_accounts
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_account_with_validation();

-- Valid update
UPDATE test_unlocked_accounts SET balance = 1500.00 WHERE account_id = 1;

-- Invalid update (would fail with minimum balance error)
-- UPDATE test_unlocked_accounts SET balance = 50.00 WHERE account_id = 2;

SELECT account_id, account_name, balance FROM test_unlocked_accounts ORDER BY account_id;

-- ============================================================================
-- Section 8: Audit Logging via INSTEAD OF Triggers
-- ============================================================================

CREATE TABLE test_sensitive_data (
    data_id SERIAL PRIMARY KEY,
    data_value TEXT,
    owner VARCHAR(100)
);

CREATE TABLE test_sensitive_audit (
    audit_id SERIAL PRIMARY KEY,
    data_id INT,
    old_value TEXT,
    new_value TEXT,
    operation VARCHAR(20),
    performed_by VARCHAR(100),
    performed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_sensitive_data (data_value, owner) VALUES
    ('Secret 1', 'alice'),
    ('Secret 2', 'bob');

CREATE VIEW test_sensitive_view AS
SELECT data_id, data_value, owner
FROM test_sensitive_data;

CREATE FUNCTION instead_of_update_sensitive() RETURNS TRIGGER AS $$
BEGIN
    -- Log the change
    INSERT INTO test_sensitive_audit (data_id, old_value, new_value, operation, performed_by)
    VALUES (OLD.data_id, OLD.data_value, NEW.data_value, 'UPDATE', CURRENT_USER);

    -- Perform the update
    UPDATE test_sensitive_data
    SET data_value = NEW.data_value,
        owner = NEW.owner
    WHERE data_id = OLD.data_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION instead_of_delete_sensitive() RETURNS TRIGGER AS $$
BEGIN
    -- Log the deletion
    INSERT INTO test_sensitive_audit (data_id, old_value, new_value, operation, performed_by)
    VALUES (OLD.data_id, OLD.data_value, NULL, 'DELETE', CURRENT_USER);

    -- Perform the delete
    DELETE FROM test_sensitive_data WHERE data_id = OLD.data_id;

    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_sensitive
INSTEAD OF UPDATE ON test_sensitive_view
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_sensitive();

CREATE TRIGGER trg_instead_of_delete_sensitive
INSTEAD OF DELETE ON test_sensitive_view
FOR EACH ROW
EXECUTE FUNCTION instead_of_delete_sensitive();

UPDATE test_sensitive_view SET data_value = 'Secret 1 Modified' WHERE data_id = 1;
DELETE FROM test_sensitive_view WHERE data_id = 2;

SELECT audit_id, data_id, old_value, new_value, operation FROM test_sensitive_audit ORDER BY audit_id;

-- ============================================================================
-- Section 9: Aggregated View with INSTEAD OF INSERT
-- ============================================================================

CREATE TABLE test_sales (
    sale_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    quantity INT,
    unit_price NUMERIC(10,2),
    sale_date DATE
);

INSERT INTO test_sales (product_name, quantity, unit_price, sale_date) VALUES
    ('Widget', 10, 5.00, '2024-01-01'),
    ('Widget', 5, 5.00, '2024-01-02'),
    ('Gadget', 8, 10.00, '2024-01-01');

CREATE VIEW test_sales_summary AS
SELECT
    product_name,
    SUM(quantity) as total_quantity,
    AVG(unit_price) as avg_price,
    COUNT(*) as num_sales
FROM test_sales
GROUP BY product_name;

CREATE FUNCTION instead_of_insert_sales_summary() RETURNS TRIGGER AS $$
DECLARE
    i INT;
BEGIN
    -- Distribute the total quantity across individual sales
    FOR i IN 1..NEW.total_quantity LOOP
        INSERT INTO test_sales (product_name, quantity, unit_price, sale_date)
        VALUES (NEW.product_name, 1, NEW.avg_price, CURRENT_DATE);
    END LOOP;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_insert_summary
INSTEAD OF INSERT ON test_sales_summary
FOR EACH ROW
EXECUTE FUNCTION instead_of_insert_sales_summary();

-- Insert into aggregated view
INSERT INTO test_sales_summary (product_name, total_quantity, avg_price) VALUES
    ('Doohickey', 10, 7.50);

SELECT product_name, total_quantity, avg_price, num_sales FROM test_sales_summary ORDER BY product_name;

-- ============================================================================
-- Section 10: Union View with INSTEAD OF Triggers
-- ============================================================================

CREATE TABLE test_current_inventory (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT,
    location VARCHAR(100)
);

CREATE TABLE test_archived_inventory (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    quantity INT,
    location VARCHAR(100),
    archived_date DATE
);

INSERT INTO test_current_inventory (item_name, quantity, location) VALUES
    ('Item A', 100, 'Warehouse 1'),
    ('Item B', 50, 'Warehouse 2');

INSERT INTO test_archived_inventory (item_name, quantity, location, archived_date) VALUES
    ('Item C', 25, 'Warehouse 1', '2023-12-01');

CREATE VIEW test_all_inventory AS
SELECT item_id, item_name, quantity, location, 'current' as status
FROM test_current_inventory
UNION ALL
SELECT item_id, item_name, quantity, location, 'archived' as status
FROM test_archived_inventory;

CREATE FUNCTION instead_of_insert_inventory() RETURNS TRIGGER AS $$
BEGIN
    -- Always insert into current inventory
    INSERT INTO test_current_inventory (item_name, quantity, location)
    VALUES (NEW.item_name, NEW.quantity, NEW.location);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION instead_of_delete_inventory() RETURNS TRIGGER AS $$
BEGIN
    IF OLD.status = 'current' THEN
        -- Move to archive instead of deleting
        INSERT INTO test_archived_inventory (item_name, quantity, location, archived_date)
        SELECT item_name, quantity, location, CURRENT_DATE
        FROM test_current_inventory
        WHERE item_id = OLD.item_id;

        DELETE FROM test_current_inventory WHERE item_id = OLD.item_id;
    ELSE
        -- Already archived, permanently delete
        DELETE FROM test_archived_inventory WHERE item_id = OLD.item_id;
    END IF;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_insert_inventory
INSTEAD OF INSERT ON test_all_inventory
FOR EACH ROW
EXECUTE FUNCTION instead_of_insert_inventory();

CREATE TRIGGER trg_instead_of_delete_inventory
INSTEAD OF DELETE ON test_all_inventory
FOR EACH ROW
EXECUTE FUNCTION instead_of_delete_inventory();

INSERT INTO test_all_inventory (item_name, quantity, location) VALUES
    ('Item D', 75, 'Warehouse 3');

DELETE FROM test_all_inventory WHERE item_name = 'Item A' AND status = 'current';

SELECT item_id, item_name, quantity, location, status FROM test_all_inventory ORDER BY status, item_id;

-- ============================================================================
-- Section 11: Recursive View Updates
-- ============================================================================

CREATE TABLE test_categories (
    category_id SERIAL PRIMARY KEY,
    category_name VARCHAR(200),
    parent_id INT REFERENCES test_categories(category_id)
);

INSERT INTO test_categories (category_name, parent_id) VALUES
    ('Electronics', NULL),
    ('Computers', 1),
    ('Laptops', 2),
    ('Desktops', 2);

CREATE VIEW test_category_tree AS
WITH RECURSIVE category_hierarchy AS (
    SELECT category_id, category_name, parent_id,
           category_name as path,
           0 as level
    FROM test_categories
    WHERE parent_id IS NULL
    UNION ALL
    SELECT c.category_id, c.category_name, c.parent_id,
           ch.path || ' > ' || c.category_name,
           ch.level + 1
    FROM test_categories c
    JOIN category_hierarchy ch ON c.parent_id = ch.category_id
)
SELECT category_id, category_name, parent_id, path, level
FROM category_hierarchy;

CREATE FUNCTION instead_of_update_category() RETURNS TRIGGER AS $$
BEGIN
    -- Prevent moving category to be its own descendant (would create cycle)
    IF NEW.parent_id = OLD.category_id THEN
        RAISE EXCEPTION 'Cannot make category its own parent';
    END IF;

    UPDATE test_categories
    SET category_name = NEW.category_name,
        parent_id = NEW.parent_id
    WHERE category_id = OLD.category_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_category
INSTEAD OF UPDATE ON test_category_tree
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_category();

-- Move "Laptops" to top level
UPDATE test_category_tree SET parent_id = NULL WHERE category_id = 3;

SELECT category_id, category_name, path, level FROM test_category_tree ORDER BY path;

-- ============================================================================
-- Section 12: Security and Permission Enforcement
-- ============================================================================

CREATE TABLE test_documents (
    document_id SERIAL PRIMARY KEY,
    title VARCHAR(200),
    content TEXT,
    owner VARCHAR(100),
    security_level INT
);

INSERT INTO test_documents (title, content, owner, security_level) VALUES
    ('Public Doc', 'Public content', 'alice', 0),
    ('Confidential Doc', 'Secret content', 'bob', 5);

CREATE VIEW test_user_documents AS
SELECT document_id, title, content, owner, security_level
FROM test_documents;

CREATE FUNCTION instead_of_update_document() RETURNS TRIGGER AS $$
BEGIN
    -- Only owner can update
    IF OLD.owner != CURRENT_USER THEN
        RAISE EXCEPTION 'Only document owner can modify this document';
    END IF;

    -- Cannot change ownership
    IF NEW.owner != OLD.owner THEN
        RAISE EXCEPTION 'Cannot transfer document ownership via update';
    END IF;

    UPDATE test_documents
    SET title = NEW.title,
        content = NEW.content,
        security_level = NEW.security_level
    WHERE document_id = OLD.document_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION instead_of_delete_document() RETURNS TRIGGER AS $$
BEGIN
    -- Only owner can delete
    IF OLD.owner != CURRENT_USER THEN
        RAISE EXCEPTION 'Only document owner can delete this document';
    END IF;

    -- High security documents cannot be deleted, only archived
    IF OLD.security_level >= 5 THEN
        RAISE EXCEPTION 'High security documents must be archived, not deleted';
    END IF;

    DELETE FROM test_documents WHERE document_id = OLD.document_id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_document
INSTEAD OF UPDATE ON test_user_documents
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_document();

CREATE TRIGGER trg_instead_of_delete_document
INSTEAD OF DELETE ON test_user_documents
FOR EACH ROW
EXECUTE FUNCTION instead_of_delete_document();

-- Valid delete (security_level = 0)
DELETE FROM test_user_documents WHERE document_id = 1;

-- Invalid delete would fail (security_level = 5)
-- DELETE FROM test_user_documents WHERE document_id = 2;

SELECT document_id, title, owner, security_level FROM test_user_documents ORDER BY document_id;

-- ============================================================================
-- Section 13: Data Transformation on Insert
-- ============================================================================

CREATE TABLE test_raw_events (
    event_id SERIAL PRIMARY KEY,
    event_type VARCHAR(50),
    event_data JSONB,
    processed BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE VIEW test_events_api AS
SELECT
    event_id,
    event_data->>'event_name' as event_name,
    event_data->>'user_id' as user_id,
    event_data->>'timestamp' as event_timestamp
FROM test_raw_events
WHERE processed = FALSE;

CREATE FUNCTION instead_of_insert_event() RETURNS TRIGGER AS $$
DECLARE
    normalized_data JSONB;
BEGIN
    -- Transform and normalize the data
    normalized_data = jsonb_build_object(
        'event_name', LOWER(TRIM(NEW.event_name)),
        'user_id', NEW.user_id,
        'timestamp', COALESCE(NEW.event_timestamp, to_char(CURRENT_TIMESTAMP, 'YYYY-MM-DD HH24:MI:SS'))
    );

    -- Determine event type
    INSERT INTO test_raw_events (event_type, event_data)
    VALUES (
        CASE
            WHEN NEW.event_name ILIKE '%login%' THEN 'authentication'
            WHEN NEW.event_name ILIKE '%purchase%' THEN 'transaction'
            ELSE 'general'
        END,
        normalized_data
    );

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_insert_event
INSTEAD OF INSERT ON test_events_api
FOR EACH ROW
EXECUTE FUNCTION instead_of_insert_event();

INSERT INTO test_events_api (event_name, user_id) VALUES
    ('User Login', '12345'),
    ('Purchase Complete', '67890'),
    ('Page View', '11111');

SELECT event_id, event_type, event_data FROM test_raw_events ORDER BY event_id;

-- ============================================================================
-- Section 14: Materialized View Pattern with INSTEAD OF
-- ============================================================================

CREATE TABLE test_transactions (
    transaction_id SERIAL PRIMARY KEY,
    account_id INT,
    amount NUMERIC(12,2),
    transaction_date DATE
);

CREATE TABLE test_account_balances (
    account_id INT PRIMARY KEY,
    balance NUMERIC(12,2),
    last_updated TIMESTAMP
);

INSERT INTO test_transactions (account_id, amount, transaction_date) VALUES
    (1, 100.00, '2024-01-01'),
    (1, -25.00, '2024-01-02'),
    (2, 500.00, '2024-01-01');

-- Initialize balances
INSERT INTO test_account_balances (account_id, balance, last_updated)
SELECT account_id, SUM(amount), CURRENT_TIMESTAMP
FROM test_transactions
GROUP BY account_id;

CREATE VIEW test_account_balance_view AS
SELECT account_id, balance, last_updated
FROM test_account_balances;

CREATE FUNCTION instead_of_update_balance() RETURNS TRIGGER AS $$
DECLARE
    balance_diff NUMERIC(12,2);
BEGIN
    balance_diff = NEW.balance - OLD.balance;

    -- Create offsetting transaction
    INSERT INTO test_transactions (account_id, amount, transaction_date)
    VALUES (OLD.account_id, balance_diff, CURRENT_DATE);

    -- Update materialized balance
    UPDATE test_account_balances
    SET balance = NEW.balance,
        last_updated = CURRENT_TIMESTAMP
    WHERE account_id = OLD.account_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_instead_of_update_balance
INSTEAD OF UPDATE ON test_account_balance_view
FOR EACH ROW
EXECUTE FUNCTION instead_of_update_balance();

-- Adjust balance via view
UPDATE test_account_balance_view SET balance = 100.00 WHERE account_id = 1;

SELECT account_id, balance FROM test_account_balance_view ORDER BY account_id;
SELECT transaction_id, account_id, amount FROM test_transactions ORDER BY transaction_id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_instead_of_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_instead_of_best_practices VALUES
    (1, 'INSTEAD OF triggers only work on views, not tables'),
    (2, 'Use INSTEAD OF to make non-updatable views appear updatable'),
    (3, 'Perfect for views with JOINs, aggregates, or UNION'),
    (4, 'Handle INSERT, UPDATE, and DELETE separately with different triggers'),
    (5, 'Access NEW for INSERT/UPDATE, OLD for UPDATE/DELETE'),
    (6, 'Return NEW for INSERT/UPDATE, OLD for DELETE, or NULL'),
    (7, 'Implement business logic and validation in INSTEAD OF triggers'),
    (8, 'Use for mapping view columns to multiple base tables'),
    (9, 'Great for audit logging without modifying base tables'),
    (10, 'Can enforce security policies at view level'),
    (11, 'Transform or normalize data during insert via views'),
    (12, 'Prevent cycles in recursive structures with validation'),
    (13, 'Document which views are updatable and how'),
    (14, 'Test all code paths: successful operations and exceptions'),
    (15, 'Consider performance impact of complex INSTEAD OF logic');

SELECT id, guideline FROM test_instead_of_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_instead_of_best_practices;
DROP TABLE test_account_balances;
DROP TABLE test_transactions;
DROP FUNCTION instead_of_update_balance();
DROP TABLE test_raw_events;
DROP FUNCTION instead_of_insert_event();
DROP TABLE test_documents;
DROP FUNCTION instead_of_delete_document();
DROP FUNCTION instead_of_update_document();
DROP TABLE test_categories;
DROP FUNCTION instead_of_update_category();
DROP TABLE test_archived_inventory;
DROP TABLE test_current_inventory;
DROP FUNCTION instead_of_delete_inventory();
DROP FUNCTION instead_of_insert_inventory();
DROP TABLE test_sales;
DROP FUNCTION instead_of_insert_sales_summary();
DROP TABLE test_sensitive_audit;
DROP TABLE test_sensitive_data;
DROP FUNCTION instead_of_delete_sensitive();
DROP FUNCTION instead_of_update_sensitive();
DROP TABLE test_accounts;
DROP FUNCTION instead_of_update_account_with_validation();
DROP TABLE test_project_members;
DROP TABLE test_projects;
DROP FUNCTION instead_of_delete_assignment();
DROP TABLE test_departments;
DROP TABLE test_employees;
DROP FUNCTION instead_of_update_employee_dept();
DROP TABLE test_addresses;
DROP TABLE test_customers;
DROP FUNCTION instead_of_insert_customer_address();
DROP TABLE test_orders_base;
DROP FUNCTION instead_of_delete_order();
DROP TABLE test_products_base;
DROP FUNCTION instead_of_update_product();
DROP TABLE test_users_base;
DROP FUNCTION instead_of_insert_user();

DROP DATABASE test_instead_of_triggers_db;

-- End of INSTEAD OF trigger tests
