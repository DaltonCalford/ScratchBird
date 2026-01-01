-- ============================================================================
-- ScratchBird Compatibility Test Suite
-- Test Category: Stored Procedures
-- Description: Comprehensive stored procedure testing
-- ============================================================================

-- Stored procedures differ from functions:
-- - Called with CALL, not SELECT
-- - Can have transactions (COMMIT/ROLLBACK)
-- - No return value (use OUT parameters instead)
-- - Support all parameter modes (IN, OUT, INOUT)

-- Create test database
CREATE DATABASE test_stored_procedures_db;
USE test_stored_procedures_db;

-- ============================================================================
-- Section 1: Basic Stored Procedure
-- ============================================================================

CREATE PROCEDURE test_basic_proc()
LANGUAGE plpgsql
AS $$
BEGIN
    RAISE NOTICE 'Basic stored procedure executed';
END;
$$;

CALL test_basic_proc();

-- ============================================================================
-- Section 2: Procedure with IN Parameters
-- ============================================================================

CREATE TABLE test_logs (
    log_id SERIAL PRIMARY KEY,
    log_message TEXT,
    log_level VARCHAR(20),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE PROCEDURE test_log_message(
    IN message TEXT,
    IN level VARCHAR DEFAULT 'INFO'
)
LANGUAGE plpgsql
AS $$
BEGIN
    INSERT INTO test_logs (log_message, log_level)
    VALUES (message, level);
END;
$$;

CALL test_log_message('System started');
CALL test_log_message('Error occurred', 'ERROR');

SELECT log_id, log_message, log_level FROM test_logs ORDER BY log_id;

-- ============================================================================
-- Section 3: Procedure with OUT Parameters
-- ============================================================================

CREATE PROCEDURE test_calculate_stats(
    IN values INT[],
    OUT total INT,
    OUT average NUMERIC,
    OUT count INT
)
LANGUAGE plpgsql
AS $$
DECLARE
    val INT;
BEGIN
    total := 0;
    count := 0;

    FOREACH val IN ARRAY values LOOP
        total := total + val;
        count := count + 1;
    END LOOP;

    IF count > 0 THEN
        average := total::NUMERIC / count;
    ELSE
        average := 0;
    END IF;
END;
$$;

CALL test_calculate_stats(ARRAY[10, 20, 30, 40, 50], NULL, NULL, NULL);

-- ============================================================================
-- Section 4: Procedure with INOUT Parameters
-- ============================================================================

CREATE PROCEDURE test_double_value(INOUT value INT)
LANGUAGE plpgsql
AS $$
BEGIN
    value := value * 2;
END;
$$;

-- Usage with INOUT requires a variable in some clients
-- For testing, we can use a different approach
CREATE TEMP TABLE test_temp_value (val INT);
INSERT INTO test_temp_value VALUES (10);

DO $$
DECLARE
    my_value INT := 10;
BEGIN
    CALL test_double_value(my_value);
    RAISE NOTICE 'Doubled value: %', my_value;
END;
$$;

-- ============================================================================
-- Section 5: Procedure with Transaction Control
-- ============================================================================

CREATE TABLE test_accounts (
    account_id SERIAL PRIMARY KEY,
    account_name VARCHAR(200),
    balance NUMERIC(12,2)
);

INSERT INTO test_accounts (account_name, balance) VALUES
    ('Account A', 1000.00),
    ('Account B', 500.00);

CREATE PROCEDURE test_transfer_funds(
    IN from_account INT,
    IN to_account INT,
    IN amount NUMERIC,
    OUT success BOOLEAN,
    OUT error_message TEXT
)
LANGUAGE plpgsql
AS $$
DECLARE
    from_balance NUMERIC;
BEGIN
    success := FALSE;
    error_message := NULL;

    -- Check source balance
    SELECT balance INTO from_balance
    FROM test_accounts
    WHERE account_id = from_account;

    IF from_balance IS NULL THEN
        error_message := 'Source account not found';
        RETURN;
    END IF;

    IF from_balance < amount THEN
        error_message := 'Insufficient funds';
        RETURN;
    END IF;

    -- Perform transfer
    UPDATE test_accounts SET balance = balance - amount WHERE account_id = from_account;
    UPDATE test_accounts SET balance = balance + amount WHERE account_id = to_account;

    success := TRUE;
    COMMIT;

EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        success := FALSE;
        error_message := SQLERRM;
END;
$$;

CALL test_transfer_funds(1, 2, 200.00, NULL, NULL);

SELECT account_id, account_name, balance FROM test_accounts ORDER BY account_id;

-- ============================================================================
-- Section 6: Procedure with Complex Logic
-- ============================================================================

CREATE TABLE test_employees (
    employee_id SERIAL PRIMARY KEY,
    name VARCHAR(200),
    salary NUMERIC(12,2),
    performance_rating INT,
    last_raise_date DATE
);

INSERT INTO test_employees (name, salary, performance_rating, last_raise_date) VALUES
    ('Alice', 75000, 5, '2023-01-01'),
    ('Bob', 65000, 4, '2023-06-01'),
    ('Charlie', 55000, 3, '2024-01-01'),
    ('Diana', 85000, 5, '2022-12-01');

CREATE PROCEDURE test_apply_raises(
    IN min_rating INT,
    IN raise_percentage NUMERIC,
    OUT employees_affected INT,
    OUT total_raise_amount NUMERIC
)
LANGUAGE plpgsql
AS $$
DECLARE
    emp RECORD;
    raise_amount NUMERIC;
BEGIN
    employees_affected := 0;
    total_raise_amount := 0;

    FOR emp IN
        SELECT employee_id, name, salary
        FROM test_employees
        WHERE performance_rating >= min_rating
          AND (last_raise_date IS NULL OR last_raise_date < CURRENT_DATE - INTERVAL '6 months')
    LOOP
        raise_amount := emp.salary * (raise_percentage / 100);
        total_raise_amount := total_raise_amount + raise_amount;

        UPDATE test_employees
        SET salary = salary + raise_amount,
            last_raise_date = CURRENT_DATE
        WHERE employee_id = emp.employee_id;

        employees_affected := employees_affected + 1;

        RAISE NOTICE 'Raise applied to %: $%', emp.name, raise_amount;
    END LOOP;
END;
$$;

CALL test_apply_raises(4, 10.0, NULL, NULL);

SELECT employee_id, name, salary FROM test_employees ORDER BY employee_id;

-- ============================================================================
-- Section 7: Procedure with Cursor
-- ============================================================================

CREATE PROCEDURE test_process_employees_cursor(
    IN dept_filter VARCHAR,
    OUT processed_count INT
)
LANGUAGE plpgsql
AS $$
DECLARE
    emp_cursor CURSOR FOR
        SELECT employee_id, name, salary
        FROM test_employees
        ORDER BY salary DESC;
    emp_record RECORD;
BEGIN
    processed_count := 0;

    OPEN emp_cursor;

    LOOP
        FETCH emp_cursor INTO emp_record;
        EXIT WHEN NOT FOUND;

        -- Process each employee
        RAISE NOTICE 'Processing employee: % (Salary: $%)', emp_record.name, emp_record.salary;
        processed_count := processed_count + 1;
    END LOOP;

    CLOSE emp_cursor;
END;
$$;

CALL test_process_employees_cursor('Engineering', NULL);

-- ============================================================================
-- Section 8: Procedure with Dynamic SQL
-- ============================================================================

CREATE PROCEDURE test_create_backup_table(
    IN source_table TEXT,
    IN backup_suffix TEXT,
    OUT backup_table_name TEXT,
    OUT rows_copied INT
)
LANGUAGE plpgsql
AS $$
BEGIN
    backup_table_name := source_table || '_' || backup_suffix;

    -- Create backup table
    EXECUTE format('CREATE TABLE %I AS SELECT * FROM %I', backup_table_name, source_table);

    -- Count rows
    EXECUTE format('SELECT COUNT(*) FROM %I', backup_table_name) INTO rows_copied;

    RAISE NOTICE 'Created backup table % with % rows', backup_table_name, rows_copied;
END;
$$;

CALL test_create_backup_table('test_employees', 'backup_20240101', NULL, NULL);

-- ============================================================================
-- Section 9: Procedure with Error Handling
-- ============================================================================

CREATE PROCEDURE test_safe_update_salary(
    IN emp_id INT,
    IN new_salary NUMERIC,
    OUT success BOOLEAN,
    OUT message TEXT
)
LANGUAGE plpgsql
AS $$
BEGIN
    success := FALSE;

    -- Validate input
    IF new_salary < 0 THEN
        message := 'Salary cannot be negative';
        RETURN;
    END IF;

    IF new_salary > 1000000 THEN
        message := 'Salary exceeds maximum allowed';
        RETURN;
    END IF;

    -- Update salary
    UPDATE test_employees
    SET salary = new_salary
    WHERE employee_id = emp_id;

    IF NOT FOUND THEN
        message := 'Employee not found';
        RETURN;
    END IF;

    success := TRUE;
    message := 'Salary updated successfully';

EXCEPTION
    WHEN OTHERS THEN
        success := FALSE;
        message := 'Error: ' || SQLERRM;
END;
$$;

CALL test_safe_update_salary(1, 80000, NULL, NULL);
CALL test_safe_update_salary(999, 50000, NULL, NULL);  -- Non-existent employee

-- ============================================================================
-- Section 10: Procedure with Multiple Tables
-- ============================================================================

CREATE TABLE test_orders (
    order_id SERIAL PRIMARY KEY,
    customer_name VARCHAR(200),
    order_total NUMERIC(12,2),
    order_date DATE DEFAULT CURRENT_DATE
);

CREATE TABLE test_order_items (
    item_id SERIAL PRIMARY KEY,
    order_id INT REFERENCES test_orders(order_id),
    product_name VARCHAR(200),
    quantity INT,
    price NUMERIC(10,2)
);

CREATE PROCEDURE test_create_order(
    IN customer VARCHAR,
    IN items JSONB,
    OUT new_order_id INT,
    OUT order_total NUMERIC
)
LANGUAGE plpgsql
AS $$
DECLARE
    item JSONB;
    item_total NUMERIC;
BEGIN
    order_total := 0;

    -- Create order
    INSERT INTO test_orders (customer_name, order_total)
    VALUES (customer, 0)
    RETURNING order_id INTO new_order_id;

    -- Add items
    FOR item IN SELECT * FROM jsonb_array_elements(items)
    LOOP
        item_total := (item->>'quantity')::INT * (item->>'price')::NUMERIC;
        order_total := order_total + item_total;

        INSERT INTO test_order_items (order_id, product_name, quantity, price)
        VALUES (
            new_order_id,
            item->>'product',
            (item->>'quantity')::INT,
            (item->>'price')::NUMERIC
        );
    END LOOP;

    -- Update order total
    UPDATE test_orders
    SET order_total = order_total
    WHERE order_id = new_order_id;
END;
$$;

CALL test_create_order(
    'John Doe',
    '[{"product": "Widget", "quantity": 2, "price": 29.99},
      {"product": "Gadget", "quantity": 1, "price": 49.99}]'::JSONB,
    NULL,
    NULL
);

SELECT order_id, customer_name, order_total FROM test_orders;
SELECT item_id, product_name, quantity, price FROM test_order_items ORDER BY item_id;

-- ============================================================================
-- Section 11: Procedure with Conditional Logic
-- ============================================================================

CREATE PROCEDURE test_categorize_employees(
    OUT junior_count INT,
    OUT mid_count INT,
    OUT senior_count INT
)
LANGUAGE plpgsql
AS $$
DECLARE
    emp RECORD;
BEGIN
    junior_count := 0;
    mid_count := 0;
    senior_count := 0;

    FOR emp IN SELECT salary FROM test_employees
    LOOP
        IF emp.salary < 60000 THEN
            junior_count := junior_count + 1;
        ELSIF emp.salary < 80000 THEN
            mid_count := mid_count + 1;
        ELSE
            senior_count := senior_count + 1;
        END IF;
    END LOOP;
END;
$$;

CALL test_categorize_employees(NULL, NULL, NULL);

-- ============================================================================
-- Section 12: Procedure with Bulk Operations
-- ============================================================================

CREATE TABLE test_products (
    product_id SERIAL PRIMARY KEY,
    product_name VARCHAR(200),
    price NUMERIC(10,2),
    stock INT,
    last_updated TIMESTAMP
);

CREATE PROCEDURE test_bulk_price_update(
    IN price_adjustment_pct NUMERIC,
    IN min_stock INT,
    OUT products_updated INT
)
LANGUAGE plpgsql
AS $$
BEGIN
    UPDATE test_products
    SET price = price * (1 + price_adjustment_pct / 100),
        last_updated = CURRENT_TIMESTAMP
    WHERE stock >= min_stock;

    GET DIAGNOSTICS products_updated = ROW_COUNT;

    RAISE NOTICE 'Updated % products', products_updated;
END;
$$;

INSERT INTO test_products (product_name, price, stock) VALUES
    ('Product A', 10.00, 100),
    ('Product B', 20.00, 50),
    ('Product C', 30.00, 25);

CALL test_bulk_price_update(10.0, 50, NULL);

SELECT product_id, product_name, price, stock FROM test_products ORDER BY product_id;

-- ============================================================================
-- Section 13: Procedure with Temporary Tables
-- ============================================================================

CREATE PROCEDURE test_analyze_sales(
    IN start_date DATE,
    IN end_date DATE,
    OUT total_sales NUMERIC,
    OUT avg_order_value NUMERIC
)
LANGUAGE plpgsql
AS $$
BEGIN
    -- Create temporary table for analysis
    CREATE TEMP TABLE temp_sales_analysis AS
    SELECT order_id, order_total
    FROM test_orders
    WHERE order_date BETWEEN start_date AND end_date;

    -- Calculate metrics
    SELECT
        COALESCE(SUM(order_total), 0),
        COALESCE(AVG(order_total), 0)
    INTO total_sales, avg_order_value
    FROM temp_sales_analysis;

    -- Clean up
    DROP TABLE IF EXISTS temp_sales_analysis;
END;
$$;

CALL test_analyze_sales('2024-01-01', '2024-12-31', NULL, NULL);

-- ============================================================================
-- Section 14: Procedure with Nested Calls
-- ============================================================================

CREATE PROCEDURE test_validate_employee(
    IN emp_id INT,
    OUT is_valid BOOLEAN,
    OUT validation_message TEXT
)
LANGUAGE plpgsql
AS $$
DECLARE
    emp_salary NUMERIC;
BEGIN
    SELECT salary INTO emp_salary
    FROM test_employees
    WHERE employee_id = emp_id;

    IF NOT FOUND THEN
        is_valid := FALSE;
        validation_message := 'Employee not found';
        RETURN;
    END IF;

    IF emp_salary < 30000 THEN
        is_valid := FALSE;
        validation_message := 'Salary below minimum threshold';
    ELSIF emp_salary > 200000 THEN
        is_valid := FALSE;
        validation_message := 'Salary exceeds maximum threshold';
    ELSE
        is_valid := TRUE;
        validation_message := 'Employee validated successfully';
    END IF;
END;
$$;

CREATE PROCEDURE test_process_employee_with_validation(
    IN emp_id INT,
    OUT processing_result TEXT
)
LANGUAGE plpgsql
AS $$
DECLARE
    is_valid BOOLEAN;
    val_message TEXT;
BEGIN
    -- Call nested procedure
    CALL test_validate_employee(emp_id, is_valid, val_message);

    IF is_valid THEN
        processing_result := 'Employee processed: ' || val_message;
    ELSE
        processing_result := 'Processing failed: ' || val_message;
    END IF;
END;
$$;

CALL test_process_employee_with_validation(1, NULL);

-- ============================================================================
-- Section 15: Procedure with Array Processing
-- ============================================================================

CREATE PROCEDURE test_batch_insert_employees(
    IN employee_data JSONB[],
    OUT inserted_count INT
)
LANGUAGE plpgsql
AS $$
DECLARE
    emp JSONB;
BEGIN
    inserted_count := 0;

    FOREACH emp IN ARRAY employee_data
    LOOP
        INSERT INTO test_employees (name, salary, performance_rating)
        VALUES (
            emp->>'name',
            (emp->>'salary')::NUMERIC,
            (emp->>'rating')::INT
        );

        inserted_count := inserted_count + 1;
    END LOOP;

    RAISE NOTICE 'Inserted % employees', inserted_count;
END;
$$;

CALL test_batch_insert_employees(
    ARRAY[
        '{"name": "Eve", "salary": 60000, "rating": 4}'::JSONB,
        '{"name": "Frank", "salary": 70000, "rating": 5}'::JSONB
    ],
    NULL
);

SELECT employee_id, name, salary FROM test_employees WHERE name IN ('Eve', 'Frank');

-- ============================================================================
-- Section 16: Best Practices
-- ============================================================================

CREATE TABLE test_procedure_best_practices (
    id INT PRIMARY KEY,
    guideline TEXT
);

INSERT INTO test_procedure_best_practices VALUES
    (1, 'Use procedures for operations that modify data'),
    (2, 'Use OUT parameters to return multiple values'),
    (3, 'Use INOUT when you need to modify and return a parameter'),
    (4, 'Include transaction control (COMMIT/ROLLBACK) when appropriate'),
    (5, 'Always handle exceptions with proper error messages'),
    (6, 'Use RAISE NOTICE for debugging and logging'),
    (7, 'Validate all input parameters at the start'),
    (8, 'Document procedure purpose, parameters, and side effects'),
    (9, 'Use cursors for row-by-row processing when needed'),
    (10, 'Clean up temporary tables and resources'),
    (11, 'Use GET DIAGNOSTICS to get affected row counts'),
    (12, 'Consider security implications of dynamic SQL'),
    (13, 'Test procedures with edge cases and invalid inputs'),
    (14, 'Keep procedures focused and modular'),
    (15, 'Monitor procedure performance and optimize as needed');

SELECT id, guideline FROM test_procedure_best_practices ORDER BY id;

-- ============================================================================
-- Cleanup
-- ============================================================================

DROP TABLE test_procedure_best_practices;
DROP PROCEDURE test_batch_insert_employees(JSONB[]);
DROP PROCEDURE test_process_employee_with_validation(INT);
DROP PROCEDURE test_validate_employee(INT);
DROP PROCEDURE test_analyze_sales(DATE, DATE);
DROP TABLE test_products;
DROP PROCEDURE test_bulk_price_update(NUMERIC, INT);
DROP PROCEDURE test_categorize_employees();
DROP TABLE test_order_items;
DROP TABLE test_orders;
DROP PROCEDURE test_create_order(VARCHAR, JSONB);
DROP PROCEDURE test_safe_update_salary(INT, NUMERIC);
DROP PROCEDURE test_create_backup_table(TEXT, TEXT);
DROP PROCEDURE test_process_employees_cursor(VARCHAR);
DROP TABLE test_employees;
DROP PROCEDURE test_apply_raises(INT, NUMERIC);
DROP TABLE test_accounts;
DROP PROCEDURE test_transfer_funds(INT, INT, NUMERIC);
DROP PROCEDURE test_double_value(INT);
DROP PROCEDURE test_calculate_stats(INT[]);
DROP TABLE test_logs;
DROP PROCEDURE test_log_message(TEXT, VARCHAR);
DROP PROCEDURE test_basic_proc();

DROP DATABASE test_stored_procedures_db;

-- End of stored procedure tests
