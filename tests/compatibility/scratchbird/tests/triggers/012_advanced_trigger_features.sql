-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Triggers - Advanced Features
-- Description: Comprehensive advanced trigger feature testing
-- ============================================================================

-- Advanced trigger features include:
-- - Constraint triggers and DEFERRABLE constraints
-- - Complex multi-level audit trails
-- - Sophisticated cascade operations
-- - Trigger execution order and dependencies
-- - Advanced state management
-- - Complex business rules enforcement

-- Create test database
CREATE DATABASE test_advanced_triggers_db;
USE test_advanced_triggers_db;

-- ============================================================================
-- Section 1: CONSTRAINT Triggers with DEFERRABLE
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_number VARCHAR(20) UNIQUE,
    balance NUMERIC(12,2) CHECK (balance >= 0)
);

CREATE TABLE test_transfers (
    transfer_id SERIAL PRIMARY KEY,
    from_account_id INT REFERENCES test_accounts(account_id),
    to_account_id INT REFERENCES test_accounts(account_id),
    amount NUMERIC(12,2) CHECK (amount > 0),
    transfer_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_accounts (account_number, balance) VALUES
    ('ACC001', 1000.00),
    ('ACC002', 500.00);

-- Constraint trigger to enforce total balance consistency
CREATE FUNCTION check_transfer_balance() RETURNS TRIGGER AS $$
DECLARE
    from_balance NUMERIC(12,2);
BEGIN
    SELECT balance INTO from_balance
    FROM test_accounts
    WHERE account_id = NEW.from_account_id;

    IF from_balance < NEW.amount THEN
        RAISE EXCEPTION 'Insufficient funds in account %', NEW.from_account_id;
    END IF;

    -- Debit source account
    UPDATE test_accounts
    SET balance = balance - NEW.amount
    WHERE account_id = NEW.from_account_id;

    -- Credit destination account
    UPDATE test_accounts
    SET balance = balance + NEW.amount
    WHERE account_id = NEW.to_account_id;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE CONSTRAINT TRIGGER trg_check_transfer
AFTER INSERT ON test_transfers
DEFERRABLE INITIALLY DEFERRED
FOR EACH ROW
EXECUTE FUNCTION check_transfer_balance();

-- Transfer within transaction
BEGIN;
INSERT INTO test_transfers (from_account_id, to_account_id, amount)
VALUES (1, 2, 200.00);
COMMIT;

SELECT account_id, account_number, balance FROM test_accounts ORDER BY account_id;
SELECT transfer_id, from_account_id, to_account_id, amount FROM test_transfers;

-- ============================================================================
-- Section 2: Multi-Level Audit Trail System
-- ============================================================================

CREATE TABLE test_customers (
    customer_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    email VARCHAR(200),
    status VARCHAR(50),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP
);

CREATE TABLE test_audit_log (
    audit_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    record_id INT,
    operation VARCHAR(20),
    old_values JSONB,
    new_values JSONB,
    changed_by VARCHAR(100),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    session_id VARCHAR(100)
);

CREATE TABLE test_audit_summary (
    summary_id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    operation VARCHAR(20),
    operation_count INT DEFAULT 0,
    last_operation TIMESTAMP
);

INSERT INTO test_customers (customer_name, email, status) VALUES
    ('Alice Johnson', 'alice@example.com', 'active'),
    ('Bob Smith', 'bob@example.com', 'active');

CREATE FUNCTION audit_customer_changes() RETURNS TRIGGER AS $$
DECLARE
    old_data JSONB;
    new_data JSONB;
BEGIN
    IF TG_OP = 'DELETE' THEN
        old_data = row_to_json(OLD)::JSONB;
        INSERT INTO test_audit_log (table_name, record_id, operation, old_values, changed_by, session_id)
        VALUES (TG_TABLE_NAME, OLD.customer_id, TG_OP, old_data, CURRENT_USER,
                to_hex(txid_current()));
        RETURN OLD;
    ELSIF TG_OP = 'UPDATE' THEN
        old_data = row_to_json(OLD)::JSONB;
        new_data = row_to_json(NEW)::JSONB;
        INSERT INTO test_audit_log (table_name, record_id, operation, old_values, new_values, changed_by, session_id)
        VALUES (TG_TABLE_NAME, NEW.customer_id, TG_OP, old_data, new_data, CURRENT_USER,
                to_hex(txid_current()));
        RETURN NEW;
    ELSIF TG_OP = 'INSERT' THEN
        new_data = row_to_json(NEW)::JSONB;
        INSERT INTO test_audit_log (table_name, record_id, operation, new_values, changed_by, session_id)
        VALUES (TG_TABLE_NAME, NEW.customer_id, TG_OP, new_data, CURRENT_USER,
                to_hex(txid_current()));
        RETURN NEW;
    END IF;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION update_audit_summary() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_audit_summary (table_name, operation, operation_count, last_operation)
    VALUES (NEW.table_name, NEW.operation, 1, NEW.changed_at)
    ON CONFLICT (table_name, operation)
    DO UPDATE SET
        operation_count = test_audit_summary.operation_count + 1,
        last_operation = EXCLUDED.last_operation;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_audit_customers
AFTER INSERT OR UPDATE OR DELETE ON test_customers
FOR EACH ROW
EXECUTE FUNCTION audit_customer_changes();

CREATE TRIGGER trg_update_audit_summary
AFTER INSERT ON test_audit_log
FOR EACH ROW
EXECUTE FUNCTION update_audit_summary();

-- Make changes and observe multi-level audit
UPDATE test_customers SET status = 'inactive' WHERE customer_id = 1;
DELETE FROM test_customers WHERE customer_id = 2;

SELECT audit_id, table_name, operation, record_id, changed_by FROM test_audit_log ORDER BY audit_id;
SELECT summary_id, table_name, operation, operation_count FROM test_audit_summary ORDER BY summary_id;

-- ============================================================================
-- Section 3: Complex Cascade Operations
-- ============================================================================

CREATE TABLE test_organizations (
    org_id SERIAL PRIMARY KEY,
    org_name VARCHAR(200),
    is_active BOOLEAN DEFAULT TRUE
);

CREATE TABLE test_teams (
    team_id SERIAL PRIMARY KEY,
    org_id INT REFERENCES test_organizations(org_id),
    team_name VARCHAR(200),
    is_active BOOLEAN DEFAULT TRUE
);

CREATE TABLE test_members (
    member_id SERIAL PRIMARY KEY,
    team_id INT REFERENCES test_teams(team_id),
    member_name VARCHAR(200),
    is_active BOOLEAN DEFAULT TRUE
);

CREATE TABLE test_cascade_log (
    log_id SERIAL PRIMARY KEY,
    entity_type VARCHAR(50),
    entity_id INT,
    action VARCHAR(50),
    cascade_level INT,
    logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_organizations (org_name) VALUES ('Org Alpha'), ('Org Beta');
INSERT INTO test_teams (org_id, team_name) VALUES
    (1, 'Team A1'), (1, 'Team A2'), (2, 'Team B1');
INSERT INTO test_members (team_id, member_name) VALUES
    (1, 'Alice'), (1, 'Bob'), (2, 'Charlie'), (3, 'David');

CREATE FUNCTION cascade_deactivate_teams() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.is_active = FALSE AND OLD.is_active = TRUE THEN
        -- Log organization deactivation
        INSERT INTO test_cascade_log (entity_type, entity_id, action, cascade_level)
        VALUES ('organization', NEW.org_id, 'deactivated', 1);

        -- Cascade to teams
        UPDATE test_teams
        SET is_active = FALSE
        WHERE org_id = NEW.org_id AND is_active = TRUE;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION cascade_deactivate_members() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.is_active = FALSE AND OLD.is_active = TRUE THEN
        -- Log team deactivation
        INSERT INTO test_cascade_log (entity_type, entity_id, action, cascade_level)
        VALUES ('team', NEW.team_id, 'deactivated', 2);

        -- Cascade to members
        UPDATE test_members
        SET is_active = FALSE
        WHERE team_id = NEW.team_id AND is_active = TRUE;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION log_member_deactivation() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.is_active = FALSE AND OLD.is_active = TRUE THEN
        INSERT INTO test_cascade_log (entity_type, entity_id, action, cascade_level)
        VALUES ('member', NEW.member_id, 'deactivated', 3);
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_cascade_org_to_teams
AFTER UPDATE ON test_organizations
FOR EACH ROW
EXECUTE FUNCTION cascade_deactivate_teams();

CREATE TRIGGER trg_cascade_team_to_members
AFTER UPDATE ON test_teams
FOR EACH ROW
EXECUTE FUNCTION cascade_deactivate_members();

CREATE TRIGGER trg_log_member_deactivation
AFTER UPDATE ON test_members
FOR EACH ROW
EXECUTE FUNCTION log_member_deactivation();

-- Deactivate organization - should cascade to teams and members
UPDATE test_organizations SET is_active = FALSE WHERE org_id = 1;

SELECT log_id, entity_type, entity_id, action, cascade_level FROM test_cascade_log ORDER BY log_id;
SELECT 'Org' as type, org_id as id, is_active FROM test_organizations
UNION ALL
SELECT 'Team', team_id, is_active FROM test_teams
UNION ALL
SELECT 'Member', member_id, is_active FROM test_members
ORDER BY type, id;

-- ============================================================================
-- Section 4: Trigger Execution Order and Dependencies
-- ============================================================================

CREATE TABLE test_execution_order (
    id SERIAL PRIMARY KEY,
    value INT,
    state VARCHAR(50)
);

CREATE TABLE test_trigger_execution_log (
    log_id SERIAL PRIMARY KEY,
    trigger_name VARCHAR(100),
    trigger_timing VARCHAR(20),
    old_value INT,
    new_value INT,
    execution_order INT,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE SEQUENCE test_execution_seq;

CREATE FUNCTION trigger_step_1() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_trigger_execution_log (trigger_name, trigger_timing, old_value, new_value, execution_order)
    VALUES ('trigger_step_1', 'BEFORE', OLD.value, NEW.value, nextval('test_execution_seq'));

    NEW.value = NEW.value * 2;  -- Double the value
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION trigger_step_2() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_trigger_execution_log (trigger_name, trigger_timing, old_value, new_value, execution_order)
    VALUES ('trigger_step_2', 'BEFORE', OLD.value, NEW.value, nextval('test_execution_seq'));

    NEW.value = NEW.value + 10;  -- Add 10
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION trigger_step_3() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO test_trigger_execution_log (trigger_name, trigger_timing, old_value, new_value, execution_order)
    VALUES ('trigger_step_3', 'AFTER', OLD.value, NEW.value, nextval('test_execution_seq'));
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Triggers fire in alphabetical order by trigger name within same timing
CREATE TRIGGER a_trigger_step_1
BEFORE INSERT ON test_execution_order
FOR EACH ROW
EXECUTE FUNCTION trigger_step_1();

CREATE TRIGGER b_trigger_step_2
BEFORE INSERT ON test_execution_order
FOR EACH ROW
EXECUTE FUNCTION trigger_step_2();

CREATE TRIGGER c_trigger_step_3
AFTER INSERT ON test_execution_order
FOR EACH ROW
EXECUTE FUNCTION trigger_step_3();

INSERT INTO test_execution_order (value, state) VALUES (5, 'initial');

SELECT id, value, state FROM test_execution_order;  -- value should be (5 * 2) + 10 = 20
SELECT log_id, trigger_name, trigger_timing, old_value, new_value, execution_order
FROM test_trigger_execution_log
ORDER BY execution_order;

-- ============================================================================
-- Section 5: Advanced State Management with Triggers
-- ============================================================================

CREATE TABLE test_workflow_items (
    item_id SERIAL PRIMARY KEY,
    item_name VARCHAR(200),
    workflow_state VARCHAR(50),
    previous_state VARCHAR(50),
    state_entered_at TIMESTAMP,
    state_duration INTERVAL
);

CREATE TABLE test_workflow_history (
    history_id SERIAL PRIMARY KEY,
    item_id INT,
    from_state VARCHAR(50),
    to_state VARCHAR(50),
    transition_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    duration_in_state INTERVAL
);

INSERT INTO test_workflow_items (item_name, workflow_state, state_entered_at) VALUES
    ('Item 1', 'draft', CURRENT_TIMESTAMP),
    ('Item 2', 'draft', CURRENT_TIMESTAMP);

CREATE FUNCTION manage_workflow_state() RETURNS TRIGGER AS $$
DECLARE
    valid_transition BOOLEAN := FALSE;
BEGIN
    -- Only track updates, not inserts
    IF TG_OP = 'UPDATE' AND OLD.workflow_state IS DISTINCT FROM NEW.workflow_state THEN
        -- Validate state transitions
        valid_transition := (
            (OLD.workflow_state = 'draft' AND NEW.workflow_state IN ('review', 'cancelled')) OR
            (OLD.workflow_state = 'review' AND NEW.workflow_state IN ('approved', 'rejected', 'draft')) OR
            (OLD.workflow_state = 'approved' AND NEW.workflow_state = 'published') OR
            (OLD.workflow_state = 'rejected' AND NEW.workflow_state = 'draft')
        );

        IF NOT valid_transition THEN
            RAISE EXCEPTION 'Invalid workflow transition: % -> %', OLD.workflow_state, NEW.workflow_state;
        END IF;

        -- Calculate duration in previous state
        NEW.state_duration = CURRENT_TIMESTAMP - OLD.state_entered_at;

        -- Record history
        INSERT INTO test_workflow_history (item_id, from_state, to_state, duration_in_state)
        VALUES (NEW.item_id, OLD.workflow_state, NEW.workflow_state, NEW.state_duration);

        -- Update state tracking
        NEW.previous_state = OLD.workflow_state;
        NEW.state_entered_at = CURRENT_TIMESTAMP;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_manage_workflow
BEFORE UPDATE ON test_workflow_items
FOR EACH ROW
EXECUTE FUNCTION manage_workflow_state();

-- Valid transitions
UPDATE test_workflow_items SET workflow_state = 'review' WHERE item_id = 1;
UPDATE test_workflow_items SET workflow_state = 'approved' WHERE item_id = 1;
UPDATE test_workflow_items SET workflow_state = 'published' WHERE item_id = 1;

-- Invalid transition (would fail)
-- UPDATE test_workflow_items SET workflow_state = 'published' WHERE item_id = 2;

SELECT item_id, workflow_state, previous_state FROM test_workflow_items ORDER BY item_id;
SELECT history_id, item_id, from_state, to_state FROM test_workflow_history ORDER BY history_id;

-- ============================================================================
-- Section 6: Complex Business Rules Enforcement
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    employee_name VARCHAR(200),
    department VARCHAR(100),
    salary NUMERIC(12,2),
    hire_date DATE,
    manager_id INT REFERENCES test_employees(employee_id)
);

CREATE TABLE test_department_budgets (
    department VARCHAR(100) PRIMARY KEY,
    budget NUMERIC(15,2),
    allocated NUMERIC(15,2) DEFAULT 0
);

INSERT INTO test_department_budgets (department, budget) VALUES
    ('Engineering', 1000000.00),
    ('Sales', 500000.00);

INSERT INTO test_employees (employee_name, department, salary, hire_date) VALUES
    ('Manager A', 'Engineering', 150000.00, '2020-01-01'),
    ('Engineer 1', 'Engineering', 100000.00, '2021-01-01');

CREATE FUNCTION enforce_employee_business_rules() RETURNS TRIGGER AS $$
DECLARE
    dept_budget NUMERIC(15,2);
    dept_allocated NUMERIC(15,2);
    manager_salary NUMERIC(12,2);
    employee_count INT;
BEGIN
    IF TG_OP = 'INSERT' OR (TG_OP = 'UPDATE' AND (OLD.salary != NEW.salary OR OLD.department != NEW.department)) THEN
        -- Rule 1: Check department budget
        SELECT budget, allocated INTO dept_budget, dept_allocated
        FROM test_department_budgets
        WHERE department = NEW.department;

        IF dept_allocated + NEW.salary > dept_budget THEN
            RAISE EXCEPTION 'Salary $ would exceed department budget ($ / $)',
                NEW.salary, dept_allocated + NEW.salary, dept_budget;
        END IF;

        -- Rule 2: Manager must earn more than subordinate
        IF NEW.manager_id IS NOT NULL THEN
            SELECT salary INTO manager_salary
            FROM test_employees
            WHERE employee_id = NEW.manager_id;

            IF NEW.salary >= manager_salary THEN
                RAISE EXCEPTION 'Employee salary ($ ) must be less than manager salary ($ )',
                    NEW.salary, manager_salary;
            END IF;
        END IF;

        -- Rule 3: Department size limit
        SELECT COUNT(*) INTO employee_count
        FROM test_employees
        WHERE department = NEW.department;

        IF employee_count >= 10 THEN
            RAISE EXCEPTION 'Department % has reached maximum size of 10 employees', NEW.department;
        END IF;

        -- Update department allocation
        IF TG_OP = 'INSERT' THEN
            UPDATE test_department_budgets
            SET allocated = allocated + NEW.salary
            WHERE department = NEW.department;
        ELSIF TG_OP = 'UPDATE' THEN
            -- Adjust old department
            UPDATE test_department_budgets
            SET allocated = allocated - OLD.salary
            WHERE department = OLD.department;

            -- Adjust new department
            UPDATE test_department_budgets
            SET allocated = allocated + NEW.salary
            WHERE department = NEW.department;
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION adjust_budget_on_delete() RETURNS TRIGGER AS $$
BEGIN
    UPDATE test_department_budgets
    SET allocated = allocated - OLD.salary
    WHERE department = OLD.department;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_enforce_employee_rules
BEFORE INSERT OR UPDATE ON test_employees
FOR EACH ROW
EXECUTE FUNCTION enforce_employee_business_rules();

CREATE TRIGGER trg_adjust_budget_delete
AFTER DELETE ON test_employees
FOR EACH ROW
EXECUTE FUNCTION adjust_budget_on_delete();

-- Valid insert
INSERT INTO test_employees (employee_name, department, salary, hire_date, manager_id) VALUES
    ('Engineer 2', 'Engineering', 95000.00, '2022-01-01', 1);

-- Invalid insert (would fail - salary >= manager)
-- INSERT INTO test_employees (employee_name, department, salary, hire_date, manager_id) VALUES
--     ('Engineer 3', 'Engineering', 160000.00, '2023-01-01', 1);

SELECT department, budget, allocated FROM test_department_budgets ORDER BY department;
SELECT employee_id, employee_name, salary, manager_id FROM test_employees ORDER BY employee_id;

-- ============================================================================
-- Section 7: Temporal Triggers with Versioning
-- ============================================================================

CREATE TABLE test_contracts (
    contract_id SERIAL PRIMARY KEY,
    contract_name VARCHAR(200),
    terms TEXT,
    effective_from DATE,
    effective_to DATE,
    version INT DEFAULT 1,
    is_current BOOLEAN DEFAULT TRUE
);

CREATE TABLE test_contract_versions (
    version_id SERIAL PRIMARY KEY,
    contract_id INT,
    contract_name VARCHAR(200),
    terms TEXT,
    effective_from DATE,
    effective_to DATE,
    version INT,
    superseded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    superseded_by INT
);

INSERT INTO test_contracts (contract_name, terms, effective_from, effective_to) VALUES
    ('Contract A', 'Original terms', '2024-01-01', '2024-12-31');

CREATE FUNCTION version_contract_changes() RETURNS TRIGGER AS $$
DECLARE
    next_version INT;
BEGIN
    IF TG_OP = 'UPDATE' AND (OLD.terms != NEW.terms OR OLD.effective_from != NEW.effective_from OR OLD.effective_to != NEW.effective_to) THEN
        -- Archive old version
        INSERT INTO test_contract_versions (
            contract_id, contract_name, terms, effective_from, effective_to, version, superseded_by
        ) VALUES (
            OLD.contract_id, OLD.contract_name, OLD.terms, OLD.effective_from, OLD.effective_to, OLD.version, NEW.version + 1
        );

        -- Increment version
        NEW.version = OLD.version + 1;

        -- Validate temporal consistency
        IF NEW.effective_from > NEW.effective_to THEN
            RAISE EXCEPTION 'Effective from date must be before effective to date';
        END IF;

        -- Check for overlapping periods with other current contracts
        IF EXISTS (
            SELECT 1 FROM test_contracts
            WHERE contract_id != NEW.contract_id
              AND is_current = TRUE
              AND contract_name = NEW.contract_name
              AND (NEW.effective_from, NEW.effective_to) OVERLAPS (effective_from, effective_to)
        ) THEN
            RAISE EXCEPTION 'Contract period overlaps with another current contract';
        END IF;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_version_contracts
BEFORE UPDATE ON test_contracts
FOR EACH ROW
EXECUTE FUNCTION version_contract_changes();

-- Update contract (creates new version)
UPDATE test_contracts
SET terms = 'Updated terms',
    effective_from = '2024-06-01',
    effective_to = '2025-05-31'
WHERE contract_id = 1;

SELECT contract_id, contract_name, version, effective_from, effective_to FROM test_contracts;
SELECT version_id, contract_id, version, terms, superseded_by FROM test_contract_versions;

-- ============================================================================
-- Section 8: Trigger-Based Data Partitioning
-- ============================================================================

CREATE TABLE test_events_2024_q1 (
    event_id SERIAL PRIMARY KEY,
    event_name VARCHAR(200),
    event_date DATE CHECK (event_date >= '2024-01-01' AND event_date < '2024-04-01')
);

CREATE TABLE test_events_2024_q2 (
    event_id SERIAL PRIMARY KEY,
    event_name VARCHAR(200),
    event_date DATE CHECK (event_date >= '2024-04-01' AND event_date < '2024-07-01')
);

CREATE TABLE test_events_2024_q3 (
    event_id SERIAL PRIMARY KEY,
    event_name VARCHAR(200),
    event_date DATE CHECK (event_date >= '2024-07-01' AND event_date < '2024-10-01')
);

CREATE TABLE test_events_2024_q4 (
    event_id SERIAL PRIMARY KEY,
    event_name VARCHAR(200),
    event_date DATE CHECK (event_date >= '2024-10-01' AND event_date < '2025-01-01')
);

CREATE VIEW test_events AS
SELECT event_id, event_name, event_date FROM test_events_2024_q1
UNION ALL
SELECT event_id, event_name, event_date FROM test_events_2024_q2
UNION ALL
SELECT event_id, event_name, event_date FROM test_events_2024_q3
UNION ALL
SELECT event_id, event_name, event_date FROM test_events_2024_q4;

CREATE FUNCTION partition_event_insert() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.event_date >= '2024-01-01' AND NEW.event_date < '2024-04-01' THEN
        INSERT INTO test_events_2024_q1 (event_name, event_date) VALUES (NEW.event_name, NEW.event_date);
    ELSIF NEW.event_date >= '2024-04-01' AND NEW.event_date < '2024-07-01' THEN
        INSERT INTO test_events_2024_q2 (event_name, event_date) VALUES (NEW.event_name, NEW.event_date);
    ELSIF NEW.event_date >= '2024-07-01' AND NEW.event_date < '2024-10-01' THEN
        INSERT INTO test_events_2024_q3 (event_name, event_date) VALUES (NEW.event_name, NEW.event_date);
    ELSIF NEW.event_date >= '2024-10-01' AND NEW.event_date < '2025-01-01' THEN
        INSERT INTO test_events_2024_q4 (event_name, event_date) VALUES (NEW.event_name, NEW.event_date);
    ELSE
        RAISE EXCEPTION 'Event date % is outside partitioning range', NEW.event_date;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_partition_events
INSTEAD OF INSERT ON test_events
FOR EACH ROW
EXECUTE FUNCTION partition_event_insert();

-- Insert events into different partitions
INSERT INTO test_events (event_name, event_date) VALUES
    ('Q1 Event', '2024-02-15'),
    ('Q2 Event', '2024-05-10'),
    ('Q3 Event', '2024-08-20'),
    ('Q4 Event', '2024-11-05');

SELECT event_id, event_name, event_date FROM test_events ORDER BY event_date;

-- ============================================================================
-- Section 9: Trigger-Based Cache Invalidation
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    category VARCHAR(100)
);

CREATE TABLE test_product_cache (
    cache_key VARCHAR(200) PRIMARY KEY,
    cache_data JSONB,
    cached_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_valid BOOLEAN DEFAULT TRUE
);

INSERT INTO test_products (product_name, price, category) VALUES
    ('Product A', 29.99, 'Electronics'),
    ('Product B', 49.99, 'Electronics'),
    ('Product C', 19.99, 'Books');

-- Populate cache
INSERT INTO test_product_cache (cache_key, cache_data)
SELECT
    'category:' || category,
    jsonb_build_object(
        'category', category,
        'product_count', COUNT(*),
        'avg_price', AVG(price)
    )
FROM test_products
GROUP BY category;

CREATE FUNCTION invalidate_product_cache() RETURNS TRIGGER AS $$
DECLARE
    affected_category VARCHAR(100);
BEGIN
    IF TG_OP = 'DELETE' THEN
        affected_category = OLD.category;
    ELSIF TG_OP = 'INSERT' THEN
        affected_category = NEW.category;
    ELSIF TG_OP = 'UPDATE' THEN
        -- Invalidate both old and new category caches
        UPDATE test_product_cache
        SET is_valid = FALSE
        WHERE cache_key = 'category:' || OLD.category;
        affected_category = NEW.category;
    END IF;

    -- Invalidate cache for affected category
    UPDATE test_product_cache
    SET is_valid = FALSE
    WHERE cache_key = 'category:' || affected_category;

    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_invalidate_cache
AFTER INSERT OR UPDATE OR DELETE ON test_products
FOR EACH ROW
EXECUTE FUNCTION invalidate_product_cache();

-- Modify product (should invalidate cache)
UPDATE test_products SET price = 39.99 WHERE product_id = 1;

SELECT cache_key, is_valid FROM test_product_cache ORDER BY cache_key;

-- ============================================================================
-- Section 10: Trigger-Based Rate Limiting
-- ============================================================================

CREATE TABLE test_api_calls (
    call_id SERIAL PRIMARY KEY,
    api_key VARCHAR(100),
    endpoint VARCHAR(200),
    call_timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_rate_limits (
    api_key VARCHAR(100) PRIMARY KEY,
    calls_per_minute INT DEFAULT 60,
    calls_this_minute INT DEFAULT 0,
    window_start TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

INSERT INTO test_rate_limits (api_key, calls_per_minute) VALUES
    ('key_123', 5),
    ('key_456', 10);

CREATE FUNCTION enforce_rate_limit() RETURNS TRIGGER AS $$
DECLARE
    current_calls INT;
    max_calls INT;
    window_start_time TIMESTAMP;
BEGIN
    SELECT calls_this_minute, calls_per_minute, window_start
    INTO current_calls, max_calls, window_start_time
    FROM test_rate_limits
    WHERE api_key = NEW.api_key;

    -- Reset counter if we're in a new minute window
    IF CURRENT_TIMESTAMP - window_start_time > INTERVAL '1 minute' THEN
        UPDATE test_rate_limits
        SET calls_this_minute = 0,
            window_start = CURRENT_TIMESTAMP
        WHERE api_key = NEW.api_key;
        current_calls = 0;
    END IF;

    -- Check if rate limit exceeded
    IF current_calls >= max_calls THEN
        RAISE EXCEPTION 'Rate limit exceeded for API key %. Limit: % calls per minute', NEW.api_key, max_calls;
    END IF;

    -- Increment counter
    UPDATE test_rate_limits
    SET calls_this_minute = calls_this_minute + 1
    WHERE api_key = NEW.api_key;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_enforce_rate_limit
BEFORE INSERT ON test_api_calls
FOR EACH ROW
EXECUTE FUNCTION enforce_rate_limit();

-- Make API calls (within limit)
INSERT INTO test_api_calls (api_key, endpoint) VALUES
    ('key_123', '/api/users'),
    ('key_123', '/api/products'),
    ('key_123', '/api/orders');

SELECT api_key, calls_per_minute, calls_this_minute FROM test_rate_limits ORDER BY api_key;

-- ============================================================================
-- Section 11: Trigger-Based Referential Integrity Enforcement
-- ============================================================================

CREATE TABLE test_parent_entities (
    parent_id SERIAL PRIMARY KEY,
    parent_name VARCHAR(200),
    child_count INT DEFAULT 0
);

CREATE TABLE test_child_entities (
    child_id SERIAL PRIMARY KEY,
    parent_id INT,
    child_name VARCHAR(200)
);

INSERT INTO test_parent_entities (parent_name) VALUES ('Parent A'), ('Parent B');

CREATE FUNCTION maintain_child_count() RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        UPDATE test_parent_entities
        SET child_count = child_count + 1
        WHERE parent_id = NEW.parent_id;
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        UPDATE test_parent_entities
        SET child_count = child_count - 1
        WHERE parent_id = OLD.parent_id;
        RETURN OLD;
    ELSIF TG_OP = 'UPDATE' AND OLD.parent_id != NEW.parent_id THEN
        UPDATE test_parent_entities
        SET child_count = child_count - 1
        WHERE parent_id = OLD.parent_id;

        UPDATE test_parent_entities
        SET child_count = child_count + 1
        WHERE parent_id = NEW.parent_id;
        RETURN NEW;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION prevent_delete_with_children() RETURNS TRIGGER AS $$
BEGIN
    IF OLD.child_count > 0 THEN
        RAISE EXCEPTION 'Cannot delete parent % with % children', OLD.parent_name, OLD.child_count;
    END IF;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_maintain_child_count
AFTER INSERT OR UPDATE OR DELETE ON test_child_entities
FOR EACH ROW
EXECUTE FUNCTION maintain_child_count();

CREATE TRIGGER trg_prevent_delete_with_children
BEFORE DELETE ON test_parent_entities
FOR EACH ROW
EXECUTE FUNCTION prevent_delete_with_children();

-- Add children
INSERT INTO test_child_entities (parent_id, child_name) VALUES
    (1, 'Child A1'),
    (1, 'Child A2'),
    (2, 'Child B1');

SELECT parent_id, parent_name, child_count FROM test_parent_entities ORDER BY parent_id;

-- Try to delete parent with children (would fail)
-- DELETE FROM test_parent_entities WHERE parent_id = 1;

-- ============================================================================
-- Section 12: Trigger-Based Data Encryption/Decryption
-- ============================================================================

CREATE TABLE test_secure_data (
    record_id SERIAL PRIMARY KEY,
    username VARCHAR(100),
    encrypted_email TEXT,
    encrypted_phone TEXT,
    encryption_key_id INT DEFAULT 1
);

CREATE TABLE test_encryption_keys (
    key_id INT PRIMARY KEY,
    key_value VARCHAR(100)
);

INSERT INTO test_encryption_keys (key_id, key_value) VALUES
    (1, 'simple_key_123');  -- In reality, use proper encryption

CREATE FUNCTION encrypt_sensitive_data() RETURNS TRIGGER AS $$
DECLARE
    encryption_key VARCHAR(100);
BEGIN
    SELECT key_value INTO encryption_key
    FROM test_encryption_keys
    WHERE key_id = 1;

    -- Simple "encryption" (in reality, use pgcrypto or similar)
    -- This is just for demonstration
    NEW.encrypted_email = encode(convert_to(NEW.encrypted_email || ':' || encryption_key, 'UTF8'), 'base64');
    NEW.encrypted_phone = encode(convert_to(NEW.encrypted_phone || ':' || encryption_key, 'UTF8'), 'base64');

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_encrypt_data
BEFORE INSERT ON test_secure_data
FOR EACH ROW
EXECUTE FUNCTION encrypt_sensitive_data();

INSERT INTO test_secure_data (username, encrypted_email, encrypted_phone) VALUES
    ('alice', 'alice@example.com', '555-0001');

SELECT record_id, username, encrypted_email, encrypted_phone FROM test_secure_data;

-- ============================================================================
-- Section 13: Trigger-Based Notification System
-- ============================================================================

CREATE TABLE test_notifications_source (
    source_id SERIAL PRIMARY KEY,
    event_type VARCHAR(50),
    event_data JSONB,
    priority VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_notification_queue (
    notification_id SERIAL PRIMARY KEY,
    recipient VARCHAR(100),
    message TEXT,
    priority VARCHAR(20),
    status VARCHAR(50) DEFAULT 'pending',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE test_notification_subscriptions (
    subscription_id SERIAL PRIMARY KEY,
    user_name VARCHAR(100),
    event_type VARCHAR(50),
    notification_method VARCHAR(50)
);

INSERT INTO test_notification_subscriptions (user_name, event_type, notification_method) VALUES
    ('alice', 'order_placed', 'email'),
    ('bob', 'order_placed', 'sms'),
    ('alice', 'payment_received', 'email');

CREATE FUNCTION create_notifications() RETURNS TRIGGER AS $$
DECLARE
    sub RECORD;
    message_text TEXT;
BEGIN
    -- Find all subscribers for this event type
    FOR sub IN
        SELECT user_name, notification_method
        FROM test_notification_subscriptions
        WHERE event_type = NEW.event_type
    LOOP
        -- Create notification message
        message_text = format('Event: %s, Data: %s', NEW.event_type, NEW.event_data::TEXT);

        -- Queue notification
        INSERT INTO test_notification_queue (recipient, message, priority)
        VALUES (sub.user_name, message_text, NEW.priority);
    END LOOP;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_create_notifications
AFTER INSERT ON test_notifications_source
FOR EACH ROW
EXECUTE FUNCTION create_notifications();

-- Create events that trigger notifications
INSERT INTO test_notifications_source (event_type, event_data, priority) VALUES
    ('order_placed', '{"order_id": 123, "amount": 99.99}'::JSONB, 'high'),
    ('payment_received', '{"payment_id": 456, "amount": 99.99}'::JSONB, 'normal');

SELECT notification_id, recipient, message, priority, status FROM test_notification_queue ORDER BY notification_id;

-- ============================================================================
-- Section 14: Trigger-Based Data Quality Enforcement
-- ============================================================================

CREATE TABLE test_customer_data (
    customer_id SERIAL PRIMARY KEY,
    full_name VARCHAR(200),
    email VARCHAR(200),
    phone VARCHAR(50),
    postal_code VARCHAR(20),
    data_quality_score INT DEFAULT 0
);

CREATE TABLE test_data_quality_issues (
    issue_id SERIAL PRIMARY KEY,
    customer_id INT,
    issue_type VARCHAR(100),
    issue_description TEXT,
    severity VARCHAR(20),
    detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE FUNCTION enforce_data_quality() RETURNS TRIGGER AS $$
DECLARE
    quality_score INT := 100;
    has_issues BOOLEAN := FALSE;
BEGIN
    -- Check full_name
    IF NEW.full_name IS NULL OR LENGTH(TRIM(NEW.full_name)) < 3 THEN
        quality_score := quality_score - 25;
        has_issues := TRUE;
        INSERT INTO test_data_quality_issues (customer_id, issue_type, issue_description, severity)
        VALUES (NEW.customer_id, 'invalid_name', 'Name is missing or too short', 'high');
    END IF;

    -- Check email format
    IF NEW.email IS NULL OR NEW.email !~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$' THEN
        quality_score := quality_score - 30;
        has_issues := TRUE;
        INSERT INTO test_data_quality_issues (customer_id, issue_type, issue_description, severity)
        VALUES (NEW.customer_id, 'invalid_email', 'Email format is invalid', 'high');
    END IF;

    -- Check phone format (simple check)
    IF NEW.phone IS NULL OR LENGTH(NEW.phone) < 10 THEN
        quality_score := quality_score - 20;
        has_issues := TRUE;
        INSERT INTO test_data_quality_issues (customer_id, issue_type, issue_description, severity)
        VALUES (NEW.customer_id, 'invalid_phone', 'Phone number is invalid', 'medium');
    END IF;

    -- Check postal code
    IF NEW.postal_code IS NULL OR LENGTH(TRIM(NEW.postal_code)) < 5 THEN
        quality_score := quality_score - 15;
        has_issues := TRUE;
        INSERT INTO test_data_quality_issues (customer_id, issue_type, issue_description, severity)
        VALUES (NEW.customer_id, 'invalid_postal', 'Postal code is invalid', 'low');
    END IF;

    NEW.data_quality_score = GREATEST(quality_score, 0);

    -- Optionally reject very poor quality data
    IF NEW.data_quality_score < 50 THEN
        RAISE EXCEPTION 'Data quality score (%) is below minimum threshold of 50', NEW.data_quality_score;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trg_enforce_data_quality
BEFORE INSERT OR UPDATE ON test_customer_data
FOR EACH ROW
EXECUTE FUNCTION enforce_data_quality();

-- Insert valid data
INSERT INTO test_customer_data (full_name, email, phone, postal_code) VALUES
    ('John Doe', 'john@example.com', '555-123-4567', '12345');

-- Insert data with quality issues (but above threshold)
INSERT INTO test_customer_data (full_name, email, phone, postal_code) VALUES
    ('Jane Smith', 'jane@example.com', '555-9999', NULL);

SELECT customer_id, full_name, email, data_quality_score FROM test_customer_data ORDER BY customer_id;
SELECT issue_id, customer_id, issue_type, severity FROM test_data_quality_issues ORDER BY issue_id;

-- ============================================================================
-- Section 15: Best Practices
-- ============================================================================

CREATE TABLE test_advanced_trigger_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_advanced_trigger_best_practices VALUES
    (1, 'Use CONSTRAINT triggers for deferred constraint checking'),
    (2, 'Implement multi-level audit trails for complete change tracking'),
    (3, 'Design cascade operations carefully to avoid infinite loops'),
    (4, 'Control trigger execution order using alphabetical naming'),
    (5, 'Use triggers for complex state machine validation'),
    (6, 'Enforce sophisticated business rules in BEFORE triggers'),
    (7, 'Implement temporal versioning with trigger-based archiving'),
    (8, 'Use triggers for automatic data partitioning strategies'),
    (9, 'Implement cache invalidation patterns with triggers'),
    (10, 'Enforce rate limits and quotas using trigger logic'),
    (11, 'Maintain referential integrity counts automatically'),
    (12, 'Consider encryption/decryption in triggers for sensitive data'),
    (13, 'Build notification systems with trigger-based queuing'),
    (14, 'Enforce data quality rules and calculate quality scores'),
    (15, 'Monitor trigger performance and optimize complex logic');

SELECT id, guideline FROM test_advanced_trigger_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_advanced_trigger_best_practices;
DROP TABLE test_data_quality_issues;
DROP TABLE test_customer_data;
DROP FUNCTION enforce_data_quality();
DROP TABLE test_notification_queue;
DROP TABLE test_notification_subscriptions;
DROP TABLE test_notifications_source;
DROP FUNCTION create_notifications();
DROP TABLE test_secure_data;
DROP TABLE test_encryption_keys;
DROP FUNCTION encrypt_sensitive_data();
DROP TABLE test_child_entities;
DROP TABLE test_parent_entities;
DROP FUNCTION prevent_delete_with_children();
DROP FUNCTION maintain_child_count();
DROP TABLE test_api_calls;
DROP TABLE test_rate_limits;
DROP FUNCTION enforce_rate_limit();
DROP TABLE test_product_cache;
DROP TABLE test_products;
DROP FUNCTION invalidate_product_cache();
DROP TABLE test_events_2024_q1 CASCADE;
DROP TABLE test_events_2024_q2;
DROP TABLE test_events_2024_q3;
DROP TABLE test_events_2024_q4;
DROP FUNCTION partition_event_insert();
DROP TABLE test_contract_versions;
DROP TABLE test_contracts;
DROP FUNCTION version_contract_changes();
DROP TABLE test_employees;
DROP TABLE test_department_budgets;
DROP FUNCTION adjust_budget_on_delete();
DROP FUNCTION enforce_employee_business_rules();
DROP TABLE test_workflow_history;
DROP TABLE test_workflow_items;
DROP FUNCTION manage_workflow_state();
DROP TABLE test_trigger_execution_log;
DROP SEQUENCE test_execution_seq;
DROP TABLE test_execution_order;
DROP FUNCTION trigger_step_3();
DROP FUNCTION trigger_step_2();
DROP FUNCTION trigger_step_1();
DROP TABLE test_cascade_log;
DROP TABLE test_members;
DROP TABLE test_teams;
DROP TABLE test_organizations;
DROP FUNCTION log_member_deactivation();
DROP FUNCTION cascade_deactivate_members();
DROP FUNCTION cascade_deactivate_teams();
DROP TABLE test_audit_summary;
DROP TABLE test_audit_log;
DROP TABLE test_customers;
DROP FUNCTION update_audit_summary();
DROP FUNCTION audit_customer_changes();
DROP TABLE test_transfers;
DROP TABLE test_accounts;
DROP FUNCTION check_transfer_balance();

DROP DATABASE test_advanced_triggers_db;

-- End of advanced trigger tests
