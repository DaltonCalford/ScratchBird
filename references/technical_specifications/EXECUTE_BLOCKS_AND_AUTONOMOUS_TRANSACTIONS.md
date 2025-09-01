# EXECUTE BLOCKS and Autonomous Transactions

## Overview

ScratchBird supports anonymous code blocks (EXECUTE BLOCK) that can run immediately without being stored as procedures/functions. These blocks can have parameters, return values, and critically, can run in autonomous transactions with their own trigger events.

## Basic EXECUTE BLOCK Syntax

### Simple Anonymous Block

```sql
-- Basic block without parameters
EXECUTE BLOCK
AS
DECLARE 
    i INT = 0;
BEGIN
    WHILE (i < 128) DO
    BEGIN
        INSERT INTO ascii_table VALUES (i, ascii_char(i));
        i = i + 1;
    END
END;

-- Block with local variables and logic
EXECUTE BLOCK
AS
DECLARE
    @total MONEY;
    @tax_rate DECIMAL(5,4) = 0.0825;
BEGIN
    SELECT SUM(amount) INTO @total FROM orders WHERE date = CURRENT_DATE;
    INSERT INTO daily_summary (date, total, tax)
    VALUES (CURRENT_DATE, @total, @total * @tax_rate);
END;
```

### Parameterized EXECUTE BLOCK

```sql
-- Block with input parameters
EXECUTE BLOCK (
    start_date DATE = ?,
    end_date DATE = ?,
    customer_id UUID = ?
)
AS
BEGIN
    DELETE FROM temp_reports 
    WHERE report_date BETWEEN start_date AND end_date
      AND customer_id = customer_id;
    
    INSERT INTO temp_reports
    SELECT * FROM generate_report(start_date, end_date, customer_id);
END;

-- Calling with parameters
EXECUTE BLOCK (
    start_date DATE = '2024-01-01',
    end_date DATE = '2024-12-31',
    customer_id UUID = '123e4567-e89b-12d3-a456-426614174000'
)
AS
BEGIN
    -- Block body
END;

-- Using bind variables
EXECUTE BLOCK (
    amount MONEY = :payment_amount,
    account_id INT = :account_number
)
AS
BEGIN
    UPDATE accounts 
    SET balance = balance + amount
    WHERE id = account_id;
END;
```

### EXECUTE BLOCK with Return Values

```sql
-- Block that returns values (like a function)
EXECUTE BLOCK (
    product_id UUID = ?
)
RETURNS (
    name VARCHAR(100),
    price MONEY,
    stock_level INT
)
AS
BEGIN
    SELECT name, price, stock_level
    INTO :name, :price, :stock_level
    FROM products
    WHERE id = product_id;
    
    SUSPEND;  -- Return the row
END;

-- Block returning multiple rows
EXECUTE BLOCK
RETURNS (
    category VARCHAR(50),
    total_sales MONEY
)
AS
BEGIN
    FOR SELECT category, SUM(amount) 
        FROM sales 
        GROUP BY category
        INTO :category, :total_sales
    DO
        SUSPEND;  -- Return each row
END;

-- Using returned values
SELECT * FROM (
    EXECUTE BLOCK
    RETURNS (id INT, value VARCHAR(100))
    AS
    BEGIN
        FOR SELECT id, value FROM temp_table
        INTO :id, :value
        DO
            SUSPEND;
    END
);
```

## Autonomous Transaction Blocks

### Basic Autonomous Transaction

```sql
-- Block running in separate transaction
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
AS
BEGIN
    -- This runs in its own transaction
    INSERT INTO audit_log (event, timestamp, user_name)
    VALUES ('Critical Operation', CURRENT_TIMESTAMP, CURRENT_USER);
    
    COMMIT;  -- Commits only the autonomous transaction
    
    -- Even if outer transaction rolls back, audit log persists
END;

-- Example in procedure with autonomous block
CREATE PROCEDURE transfer_funds(
    @from_account INT,
    @to_account INT,
    @amount MONEY
) AS $$
BEGIN
    -- Main transaction
    UPDATE accounts SET balance = balance - @amount
    WHERE account_id = @from_account;
    
    -- Autonomous transaction for audit
    EXECUTE BLOCK
    WITH AUTONOMOUS TRANSACTION
    AS
    BEGIN
        INSERT INTO transfer_audit (
            from_account, to_account, amount, 
            timestamp, status
        ) VALUES (
            @from_account, @to_account, @amount,
            CURRENT_TIMESTAMP, 'ATTEMPTED'
        );
        COMMIT;
    END;
    
    -- Continue main transaction
    UPDATE accounts SET balance = balance + @amount
    WHERE account_id = @to_account;
    
    -- If this fails, audit still shows attempt
END;
$$ LANGUAGE plpgsql;
```

### Autonomous Transaction Options

```sql
-- Autonomous block with options
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION (
    ISOLATION LEVEL = 'READ COMMITTED',
    READ WRITE,
    DEFERRABLE,
    TIMEOUT = '5 seconds'
)
AS
BEGIN
    -- Isolated operations
    PERFORM independent_cleanup();
    COMMIT;
END;

-- Nested autonomous transactions
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
AS
BEGIN
    -- First level autonomous
    INSERT INTO log1 VALUES ('Start');
    
    EXECUTE BLOCK
    WITH AUTONOMOUS TRANSACTION
    AS
    BEGIN
        -- Nested autonomous transaction
        INSERT INTO log2 VALUES ('Nested');
        COMMIT;
    END;
    
    INSERT INTO log1 VALUES ('End');
    COMMIT;
END;
```

## Block-Level Triggers

### Defining Triggers on EXECUTE BLOCK

```sql
-- Block with inline triggers
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
AS
DECLARE
    @status VARCHAR(20);
BEGIN
    -- Define block-level triggers
    ON COMMIT DO
    BEGIN
        INSERT INTO block_audit (event, status, timestamp)
        VALUES ('BLOCK_COMMIT', 'SUCCESS', CURRENT_TIMESTAMP);
        
        -- Notify external system
        PERFORM notify_completion('SUCCESS');
    END;
    
    ON ROLLBACK DO
    BEGIN
        INSERT INTO block_audit (event, status, timestamp)
        VALUES ('BLOCK_ROLLBACK', 'FAILED', CURRENT_TIMESTAMP);
        
        -- Alert administrators
        PERFORM send_alert('Transaction failed in block');
    END;
    
    -- Main block logic
    BEGIN
        -- Operations that might fail
        PERFORM risky_operation();
        
        IF some_condition THEN
            SET @status = 'COMPLETED';
            COMMIT;  -- Triggers ON COMMIT
        ELSE
            SET @status = 'FAILED';
            ROLLBACK;  -- Triggers ON ROLLBACK
        END IF;
    END;
END;
```

### Advanced Block Triggers

```sql
-- Block with multiple trigger points
EXECUTE BLOCK (
    batch_id UUID = ?
)
WITH AUTONOMOUS TRANSACTION
AS
BEGIN
    -- Before transaction starts
    ON TRANSACTION START DO
    BEGIN
        INSERT INTO batch_log (batch_id, event, timestamp)
        VALUES (batch_id, 'START', CURRENT_TIMESTAMP);
    END;
    
    -- On successful commit
    ON COMMIT DO
    BEGIN
        UPDATE batch_status 
        SET status = 'COMPLETED',
            end_time = CURRENT_TIMESTAMP
        WHERE batch_id = batch_id;
        
        -- Chain next batch
        EXECUTE BLOCK (batch_id = get_next_batch_id())
        WITH AUTONOMOUS TRANSACTION
        AS
        BEGIN
            PERFORM process_batch(batch_id);
        END;
    END;
    
    -- On rollback
    ON ROLLBACK DO
    BEGIN
        UPDATE batch_status 
        SET status = 'FAILED',
            error_time = CURRENT_TIMESTAMP,
            error_info = GET TRANSACTION_ERROR_INFO
        WHERE batch_id = batch_id;
        
        -- Attempt recovery
        PERFORM schedule_retry(batch_id);
    END;
    
    -- On any error
    ON ERROR DO
    BEGIN
        DECLARE @error exception_info;
        SET @error = GET EXCEPTION_INFO;
        
        INSERT INTO error_details (
            batch_id, error_message, error_code,
            stack_trace, occurred_at
        ) VALUES (
            batch_id, @error.message, @error.sqlstate,
            @error.stack_trace, @error.occurred_at
        );
    END;
    
    -- Main processing
    PERFORM process_batch_data(batch_id);
    
    IF validation_passed() THEN
        COMMIT;
    ELSE
        ROLLBACK;
    END IF;
END;
```

### Conditional Block Triggers

```sql
-- Triggers with conditions
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
AS
DECLARE
    @row_count INT = 0;
    @start_time TIMESTAMP = CURRENT_TIMESTAMP;
BEGIN
    -- Conditional commit trigger
    ON COMMIT WHEN @row_count > 1000 DO
    BEGIN
        -- Only fires if more than 1000 rows processed
        INSERT INTO large_batch_log (rows, duration)
        VALUES (@row_count, CURRENT_TIMESTAMP - @start_time);
    END;
    
    ON COMMIT WHEN @row_count <= 1000 DO
    BEGIN
        -- Different action for small batches
        UPDATE statistics 
        SET small_batch_count = small_batch_count + 1;
    END;
    
    -- Process data
    FOR record IN SELECT * FROM source_table LOOP
        INSERT INTO target_table VALUES (record.*);
        SET @row_count = @row_count + 1;
    END LOOP;
    
    COMMIT;
END;
```

## Block State and Context

### Accessing Block Context

```sql
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
AS
DECLARE
    @block_id UUID = GET BLOCK_ID;  -- Unique block execution ID
    @parent_transaction_id BIGINT = GET PARENT_TRANSACTION_ID;
BEGIN
    -- Block context variables
    INSERT INTO block_execution_log (
        block_id,
        parent_transaction,
        start_time,
        user_name,
        application,
        client_ip
    ) VALUES (
        @block_id,
        @parent_transaction_id,
        GET BLOCK_START_TIME,
        GET BLOCK_USER,
        GET BLOCK_APPLICATION,
        GET BLOCK_CLIENT_IP
    );
    
    -- Block can access its own metadata
    IF GET BLOCK_PARAMETER_COUNT > 0 THEN
        INSERT INTO parameter_log
        SELECT @block_id, * FROM GET BLOCK_PARAMETERS;
    END IF;
    
    COMMIT;
END;
```

### Block Return Status

```sql
-- Block with return status handling
DECLARE
    @result INT;
    @status VARCHAR(20);
BEGIN
    -- Execute block and capture status
    SET @result = EXECUTE BLOCK
    WITH AUTONOMOUS TRANSACTION
    RETURNS (status_code INT)
    AS
    BEGIN
        TRY
            PERFORM complex_operation();
            status_code = 0;  -- Success
        EXCEPT WHEN OTHERS THEN
            status_code = SQLSTATE;  -- Error code
        END TRY;
        
        SUSPEND;
    END;
    
    -- Check block execution status
    SET @status = GET LAST_BLOCK_STATUS;  -- 'COMMITTED', 'ROLLED_BACK', 'ERROR'
    
    IF @status = 'COMMITTED' THEN
        -- Block succeeded
        CONTINUE;
    ELSE
        -- Block failed
        RAISE EXCEPTION 'Autonomous block failed';
    END IF;
END;
```

## Savepoint Management in Blocks

```sql
EXECUTE BLOCK
AS
DECLARE
    @savepoint_name VARCHAR(50);
BEGIN
    -- Create savepoint
    SET @savepoint_name = 'sp_' || GET BLOCK_ID;
    SAVEPOINT @savepoint_name;
    
    TRY
        -- First operation
        INSERT INTO table1 VALUES (...);
        
        -- Nested savepoint
        SAVEPOINT nested_sp;
        
        TRY
            -- Risky operation
            PERFORM dangerous_update();
        EXCEPT WHEN OTHERS THEN
            -- Rollback to nested savepoint
            ROLLBACK TO SAVEPOINT nested_sp;
            -- Continue with alternative approach
            PERFORM safe_update();
        END TRY;
        
        -- More operations
        UPDATE table2 SET ...;
        
    EXCEPT WHEN OTHERS THEN
        -- Rollback to main savepoint
        ROLLBACK TO SAVEPOINT @savepoint_name;
        RAISE;
    END TRY;
END;
```

## Scheduling and Async Execution

```sql
-- Schedule block for later execution
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
AT TIME '2024-12-31 23:59:59'
AS
BEGIN
    PERFORM year_end_processing();
    COMMIT;
END;

-- Async execution
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION ASYNC
AS
BEGIN
    -- Runs in background
    PERFORM long_running_task();
    
    ON COMMIT DO
    BEGIN
        -- Notify completion
        PERFORM pg_notify('task_complete', GET BLOCK_ID::TEXT);
    END;
END;

-- Recurring block execution
EXECUTE BLOCK
WITH AUTONOMOUS TRANSACTION
EVERY INTERVAL '1 hour'
AS
BEGIN
    PERFORM hourly_maintenance();
    
    -- Stop recurring after condition
    IF should_stop() THEN
        CANCEL RECURRING;
    END IF;
END;
```

## Block Templates and Reuse

```sql
-- Define reusable block template
CREATE BLOCK TEMPLATE audit_wrapper AS
WITH AUTONOMOUS TRANSACTION
AS
DECLARE
    @operation_id UUID = gen_random_uuid();
BEGIN
    ON TRANSACTION START DO
        INSERT INTO audit_start VALUES (@operation_id, CURRENT_TIMESTAMP);
    
    ON COMMIT DO
        INSERT INTO audit_complete VALUES (@operation_id, CURRENT_TIMESTAMP);
    
    ON ROLLBACK DO
        INSERT INTO audit_failed VALUES (@operation_id, CURRENT_TIMESTAMP);
    
    -- Placeholder for actual logic
    $BLOCK_BODY$
END;

-- Use template
EXECUTE BLOCK USING TEMPLATE audit_wrapper
AS
BEGIN
    -- This code runs within the template wrapper
    UPDATE sensitive_table SET value = value * 1.1;
    COMMIT;
END;
```

## Examples

### Example 1: Data Migration with Autonomous Logging

```sql
EXECUTE BLOCK (
    source_table VARCHAR(100) = 'old_customers',
    target_table VARCHAR(100) = 'new_customers'
)
WITH AUTONOMOUS TRANSACTION
AS
DECLARE
    @migrated_count INT = 0;
    @error_count INT = 0;
    @migration_id UUID = gen_random_uuid();
BEGIN
    -- Setup triggers
    ON COMMIT DO
    BEGIN
        UPDATE migration_status 
        SET status = 'COMPLETED',
            records_migrated = @migrated_count,
            errors = @error_count,
            completed_at = CURRENT_TIMESTAMP
        WHERE migration_id = @migration_id;
    END;
    
    ON ROLLBACK DO
    BEGIN
        UPDATE migration_status 
        SET status = 'FAILED',
            records_migrated = @migrated_count,
            errors = @error_count,
            failed_at = CURRENT_TIMESTAMP
        WHERE migration_id = @migration_id;
    END;
    
    -- Initialize migration
    INSERT INTO migration_status (migration_id, source, target, status)
    VALUES (@migration_id, source_table, target_table, 'RUNNING');
    
    -- Migrate data
    FOR record IN EXECUTE 'SELECT * FROM ' || source_table LOOP
        TRY
            EXECUTE 'INSERT INTO ' || target_table || ' VALUES ($1.*)' 
                USING record;
            SET @migrated_count = @migrated_count + 1;
            
        EXCEPT WHEN OTHERS THEN
            SET @error_count = @error_count + 1;
            
            -- Log error in autonomous transaction
            EXECUTE BLOCK
            WITH AUTONOMOUS TRANSACTION
            AS
            BEGIN
                INSERT INTO migration_errors (
                    migration_id, record_data, error_message
                ) VALUES (
                    @migration_id, 
                    ROW_TO_JSON(record),
                    GET EXCEPTION_INFO.message
                );
                COMMIT;
            END;
        END TRY;
    END LOOP;
    
    IF @error_count = 0 THEN
        COMMIT;
    ELSE
        ROLLBACK;
    END IF;
END;
```

### Example 2: Batch Processing with Checkpoints

```sql
EXECUTE BLOCK (
    batch_size INT = 1000
)
WITH AUTONOMOUS TRANSACTION
AS
DECLARE
    @processed INT = 0;
    @checkpoint_interval INT = 5000;
BEGIN
    -- Process in batches with checkpoints
    LOOP
        -- Process batch
        WITH batch AS (
            SELECT * FROM unprocessed_records
            LIMIT batch_size
            FOR UPDATE SKIP LOCKED
        )
        UPDATE unprocessed_records r
        SET processed = TRUE
        FROM batch b
        WHERE r.id = b.id;
        
        SET @processed = @processed + batch_size;
        
        -- Checkpoint in autonomous transaction
        IF @processed % @checkpoint_interval = 0 THEN
            EXECUTE BLOCK
            WITH AUTONOMOUS TRANSACTION
            AS
            BEGIN
                INSERT INTO processing_checkpoint (
                    timestamp, records_processed
                ) VALUES (
                    CURRENT_TIMESTAMP, @processed
                );
                COMMIT;
            END;
        END IF;
        
        EXIT WHEN NOT FOUND;
    END LOOP;
    
    COMMIT;
END;
```

This comprehensive EXECUTE BLOCK system with autonomous transactions and block-level triggers provides powerful capabilities for complex operations!