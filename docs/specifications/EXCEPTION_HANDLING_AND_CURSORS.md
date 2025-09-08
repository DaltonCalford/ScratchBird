# Advanced Exception Handling and Universal Cursors

## Part 1: Exception Handling with TRY/EXCEPT

### TRY/EXCEPT Block Structure

```sql
CREATE PROCEDURE process_order(@order_id UUID) AS $$
DECLARE
    @status VARCHAR;
    @error_info RECORD;
BEGIN
    -- TRY block for protected code
    TRY
        -- Protected operations
        UPDATE orders SET status = 'PROCESSING' WHERE order_id = @order_id;
        PERFORM charge_customer(@order_id);
        PERFORM ship_order(@order_id);
        
    EXCEPT WHEN payment_failed THEN
        -- Handle specific custom exception
        SET @error_info = GET EXCEPTION_INFO;
        INSERT INTO payment_failures (order_id, reason, amount)
        VALUES (@order_id, @error_info.message, @error_info.context.amount);
        RAISE NOTICE 'Payment failed: %', @error_info.message;
        
    EXCEPT WHEN SQLSTATE '23505' THEN
        -- Handle duplicate key violation
        RAISE WARNING 'Duplicate order processing attempted';
        
    EXCEPT WHEN OTHERS THEN
        -- Catch all other exceptions
        SET @error_info = GET EXCEPTION_INFO;
        PERFORM log_error(@error_info);
        RAISE;  -- Re-raise the exception
    END TRY;
    
    -- Code continues here after exception handling
    UPDATE orders SET status = 'COMPLETED' WHERE order_id = @order_id;
    
-- WHEN blocks handle unhandled exceptions at procedure level
WHEN insufficient_inventory THEN
    -- Handle inventory exception not caught in TRY blocks
    ROLLBACK;
    RETURN 'INSUFFICIENT_INVENTORY';
    
WHEN OTHERS THEN
    -- Final catch-all for procedure
    ROLLBACK;
    PERFORM alert_admin(GET EXCEPTION_INFO);
    RETURN 'PROCESSING_ERROR';
END;
$$ LANGUAGE plpgsql;
```

### Custom Exception Definition

```sql
-- Define custom exceptions with templates
CREATE EXCEPTION payment_failed (
    amount MONEY,
    currency VARCHAR(3),
    card_last_four VARCHAR(4),
    decline_code VARCHAR(50)
) WITH MESSAGE TEMPLATE 
    'Payment of {amount} {currency} failed for card ending in {card_last_four}. Decline code: {decline_code}';

CREATE EXCEPTION insufficient_inventory (
    product_id UUID,
    requested_quantity INT,
    available_quantity INT
) WITH MESSAGE TEMPLATE
    'Insufficient inventory for product {product_id}: requested {requested_quantity}, available {available_quantity}'
WITH OPTIONS (
    SEVERITY = 'ERROR',
    SQLSTATE = 'P0001',
    HINT = 'Check inventory levels before processing order',
    DETAIL = 'This usually happens during high-demand periods'
);

-- Define exception with dynamic message builder
CREATE EXCEPTION validation_error (
    field_name VARCHAR,
    field_value VARIANT,
    constraint_name VARCHAR
) WITH MESSAGE BUILDER validate_error_message;

CREATE FUNCTION validate_error_message(context RECORD) RETURNS TEXT AS $$
BEGIN
    RETURN format('Validation failed for field "%s" with value "%s": %s',
                  context.field_name,
                  context.field_value,
                  CASE context.constraint_name
                      WHEN 'not_null' THEN 'Value cannot be null'
                      WHEN 'unique' THEN 'Value must be unique'
                      WHEN 'check' THEN 'Value fails check constraint'
                      ELSE 'Unknown constraint violation'
                  END);
END;
$$ LANGUAGE plpgsql;
```

### Raising Custom Exceptions

```sql
-- Raise with inline values
RAISE payment_failed (
    amount := 99.99,
    currency := 'USD',
    card_last_four := '1234',
    decline_code := 'INSUFFICIENT_FUNDS'
);

-- Raise with variables
DECLARE
    @amount MONEY = 150.00;
    @currency VARCHAR(3) = 'EUR';
BEGIN
    RAISE payment_failed (
        amount := @amount,
        currency := @currency,
        card_last_four := get_card_last_four(@customer_id),
        decline_code := @api_response.decline_code
    );
END;

-- Raise with custom severity
RAISE validation_error (
    field_name := 'email',
    field_value := @email,
    constraint_name := 'unique'
) WITH SEVERITY ERROR;

-- Conditional raising
IF @inventory_count < @requested_quantity THEN
    RAISE insufficient_inventory (
        product_id := @product_id,
        requested_quantity := @requested_quantity,
        available_quantity := @inventory_count
    );
END IF;
```

### Exception Information Domain

```sql
-- System-defined exception info record
CREATE DOMAIN exception_info AS RECORD (
    -- Standard fields
    exception_name VARCHAR(100),
    message TEXT,
    detail TEXT,
    hint TEXT,
    sqlstate CHAR(5),
    severity VARCHAR(10),
    
    -- Context information
    context RECORD,  -- Custom exception parameters
    
    -- Location information
    schema_name VARCHAR(100),
    routine_name VARCHAR(100),
    line_number INT,
    column_number INT,
    
    -- Stack trace
    stack_trace TEXT[],
    call_stack RECORD[],
    
    -- Transaction context
    transaction_id BIGINT,
    statement_id BIGINT,
    
    -- User context
    user_name VARCHAR(100),
    application_name VARCHAR(100),
    client_address INET,
    
    -- Timing
    occurred_at TIMESTAMP,
    
    -- Additional data
    error_data JSONB
);

-- Accessing exception information
DECLARE
    @error exception_info;
BEGIN
    TRY
        -- Some operation that might fail
        PERFORM risky_operation();
        
    EXCEPT WHEN OTHERS THEN
        SET @error = GET EXCEPTION_INFO;
        
        -- Access standard fields
        RAISE NOTICE 'Error: % (SQLSTATE: %)', @error.message, @error.sqlstate;
        
        -- Access context for custom exceptions
        IF @error.exception_name = 'payment_failed' THEN
            RAISE NOTICE 'Payment amount: %', @error.context.amount;
        END IF;
        
        -- Log complete error
        INSERT INTO error_log (error_info) VALUES (@error);
    END TRY;
END;
```

### Nested TRY/EXCEPT Blocks

```sql
CREATE FUNCTION complex_processing() RETURNS VOID AS $$
BEGIN
    TRY
        -- Outer try block
        PERFORM initial_setup();
        
        TRY
            -- Inner try block
            PERFORM dangerous_operation();
            
        EXCEPT WHEN specific_error THEN
            -- Handle at inner level
            PERFORM recovery_action();
            -- Continue outer block
            
        EXCEPT WHEN OTHERS THEN
            -- Log and re-raise to outer block
            PERFORM log_inner_error(GET EXCEPTION_INFO);
            RAISE;  -- Propagate to outer block
        END TRY;
        
        PERFORM cleanup_operation();
        
    EXCEPT WHEN OTHERS THEN
        -- Handle at outer level
        PERFORM emergency_cleanup();
        RAISE custom_wrapper_exception (
            original_error := GET EXCEPTION_INFO
        );
    END TRY;
END;
$$ LANGUAGE plpgsql;
```

## Part 2: Universal Cursor Support

### Cursor Declaration for Any Result Set

```sql
-- Cursor for tables
DECLARE cursor_name CURSOR FOR SELECT * FROM table_name;

-- Cursor for views
DECLARE view_cursor CURSOR FOR SELECT * FROM complex_view;

-- Cursor for sets (including SET types)
DECLARE set_cursor CURSOR FOR @my_set;

-- Cursor for procedure results
DECLARE proc_cursor CURSOR FOR CALL get_customer_orders(@customer_id);

-- Cursor for function returning set
DECLARE func_cursor CURSOR FOR SELECT * FROM get_active_users();

-- Cursor for CTE
DECLARE cte_cursor CURSOR FOR 
    WITH summary AS (
        SELECT category, SUM(amount) as total
        FROM sales
        GROUP BY category
    )
    SELECT * FROM summary;

-- Cursor for VALUES clause (literal set)
DECLARE literal_cursor CURSOR FOR 
    VALUES (1, 'Alice'), (2, 'Bob'), (3, 'Charlie');

-- Cursor for array unnesting
DECLARE array_cursor CURSOR FOR 
    SELECT * FROM UNNEST(@my_array) WITH ORDINALITY;

-- Cursor for JSON array
DECLARE json_cursor CURSOR FOR 
    SELECT * FROM JSON_ARRAY_ELEMENTS(@json_data);
```

### Dynamic Cursor Creation

```sql
-- Dynamic cursor from any source
CREATE PROCEDURE process_any_resultset(
    @source_type VARCHAR,
    @source_name VARCHAR
) AS $$
DECLARE
    @dynamic_cursor REFCURSOR;
    @sql TEXT;
    @record RECORD;
BEGIN
    -- Build cursor based on source type
    CASE @source_type
        WHEN 'TABLE' THEN
            SET @sql = 'SELECT * FROM ' || @source_name;
        WHEN 'VIEW' THEN
            SET @sql = 'SELECT * FROM ' || @source_name;
        WHEN 'PROCEDURE' THEN
            SET @sql = 'CALL ' || @source_name || '()';
        WHEN 'FUNCTION' THEN
            SET @sql = 'SELECT * FROM ' || @source_name || '()';
        WHEN 'SET' THEN
            -- For SET variables
            OPEN @dynamic_cursor FOR SELECT * FROM @source_name;
    END CASE;
    
    IF @sql IS NOT NULL THEN
        OPEN @dynamic_cursor FOR EXECUTE @sql;
    END IF;
    
    -- Process cursor
    LOOP
        FETCH @dynamic_cursor INTO @record;
        EXIT WHEN NOT FOUND;
        
        -- Process record
        PERFORM process_record(@record);
    END LOOP;
    
    CLOSE @dynamic_cursor;
END;
$$ LANGUAGE plpgsql;
```

### Cursor for Procedure Results

```sql
-- Procedure that returns multiple result sets
CREATE PROCEDURE get_customer_data(@customer_id UUID)
RETURNS MULTIPLE SETS AS $$
BEGIN
    -- First result set: customer info
    SELECT * FROM customers WHERE customer_id = @customer_id;
    
    -- Second result set: orders
    SELECT * FROM orders WHERE customer_id = @customer_id;
    
    -- Third result set: payments
    SELECT * FROM payments WHERE customer_id = @customer_id;
END;
$$ LANGUAGE plpgsql;

-- Using cursor with multi-set procedure
CREATE PROCEDURE process_customer(@customer_id UUID) AS $$
DECLARE
    @info_cursor CURSOR FOR CALL get_customer_data(@customer_id) RESULT SET 1;
    @order_cursor CURSOR FOR CALL get_customer_data(@customer_id) RESULT SET 2;
    @payment_cursor CURSOR FOR CALL get_customer_data(@customer_id) RESULT SET 3;
    @customer RECORD;
    @order RECORD;
    @payment RECORD;
BEGIN
    -- Process customer info
    OPEN @info_cursor;
    FETCH @info_cursor INTO @customer;
    CLOSE @info_cursor;
    
    -- Process orders
    OPEN @order_cursor;
    LOOP
        FETCH @order_cursor INTO @order;
        EXIT WHEN NOT FOUND;
        PERFORM process_order(@order);
    END LOOP;
    CLOSE @order_cursor;
    
    -- Process payments
    OPEN @payment_cursor;
    LOOP
        FETCH @payment_cursor INTO @payment;
        EXIT WHEN NOT FOUND;
        PERFORM process_payment(@payment);
    END LOOP;
    CLOSE @payment_cursor;
END;
$$ LANGUAGE plpgsql;
```

### Cursor for SET Types

```sql
-- Using cursor with SET variables
CREATE PROCEDURE process_id_set(@ids id_set) AS $$
DECLARE
    @cursor CURSOR FOR @ids;  -- Direct cursor on SET
    @current_id UUID;
BEGIN
    OPEN @cursor;
    
    LOOP
        FETCH @cursor INTO @current_id;
        EXIT WHEN NOT FOUND;
        
        -- Process each ID
        UPDATE records 
        SET processed = TRUE 
        WHERE record_id = @current_id;
    END LOOP;
    
    CLOSE @cursor;
END;
$$ LANGUAGE plpgsql;

-- Cursor for function returning SET
CREATE FUNCTION get_active_ids() RETURNS SET OF UUID AS $$
BEGIN
    RETURN QUERY
    SELECT id FROM entities WHERE status = 'ACTIVE';
END;
$$ LANGUAGE plpgsql;

CREATE PROCEDURE process_active() AS $$
DECLARE
    @active_cursor CURSOR FOR get_active_ids();
    @id UUID;
BEGIN
    FOR @id IN @active_cursor LOOP
        PERFORM process_entity(@id);
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

### Advanced Cursor Features

```sql
-- Scrollable cursor for any source
DECLARE scrollable_cursor SCROLL CURSOR FOR 
    SELECT * FROM large_table;

-- Cursor with hold (survives transaction)
DECLARE persistent_cursor CURSOR WITH HOLD FOR 
    SELECT * FROM important_data;

-- Cursor with parameters
DECLARE param_cursor CURSOR (min_amount MONEY, max_amount MONEY) FOR 
    SELECT * FROM orders 
    WHERE total BETWEEN min_amount AND max_amount;

-- Sensitive cursor (sees changes)
DECLARE sensitive_cursor SENSITIVE CURSOR FOR 
    SELECT * FROM live_data;

-- Insensitive cursor (snapshot)
DECLARE snapshot_cursor INSENSITIVE CURSOR FOR 
    SELECT * FROM volatile_table;

-- Using cursors in FOR loops
FOR record IN CURSOR FOR SELECT * FROM any_source LOOP
    -- Process record
END LOOP;

-- Cursor with dynamic options
DECLARE @cursor_options CURSOR_OPTIONS = RECORD(
    scrollable := TRUE,
    sensitive := FALSE,
    with_hold := TRUE,
    fetch_size := 1000
);

DECLARE optimized_cursor CURSOR WITH OPTIONS @cursor_options FOR 
    SELECT * FROM huge_table;
```

### Cursor Position and Information

```sql
CREATE PROCEDURE cursor_navigation() AS $$
DECLARE
    @my_cursor CURSOR FOR SELECT * FROM products;
    @record RECORD;
    @position INT;
    @row_count INT;
BEGIN
    OPEN @my_cursor;
    
    -- Get cursor information
    SET @row_count = GET CURSOR_ROW_COUNT(@my_cursor);
    SET @position = GET CURSOR_POSITION(@my_cursor);
    
    -- Navigate cursor
    FETCH FIRST FROM @my_cursor INTO @record;
    FETCH LAST FROM @my_cursor INTO @record;
    FETCH ABSOLUTE 10 FROM @my_cursor INTO @record;
    FETCH RELATIVE -5 FROM @my_cursor INTO @record;
    FETCH PRIOR FROM @my_cursor INTO @record;
    
    -- Bulk fetch
    FETCH FORWARD 100 FROM @my_cursor BULK COLLECT INTO @record_array;
    
    -- Check cursor state
    IF IS CURSOR_OPEN(@my_cursor) THEN
        IF NOT IS CURSOR_AT_END(@my_cursor) THEN
            FETCH NEXT FROM @my_cursor INTO @record;
        END IF;
    END IF;
    
    CLOSE @my_cursor;
END;
$$ LANGUAGE plpgsql;
```

## Combined Example: Exception Handling with Cursors

```sql
-- Define custom exception for cursor operations
CREATE EXCEPTION cursor_error (
    cursor_name VARCHAR,
    operation VARCHAR,
    row_number INT,
    error_detail TEXT
) WITH MESSAGE TEMPLATE
    'Cursor error in {cursor_name} during {operation} at row {row_number}: {error_detail}';

-- Procedure using both features
CREATE PROCEDURE safe_batch_process(
    @source_table VARCHAR,
    @batch_size INT DEFAULT 100
) AS $$
DECLARE
    @cursor REFCURSOR;
    @records RECORD[];
    @current_row INT = 0;
    @error_count INT = 0;
    @error_info exception_info;
BEGIN
    -- Main processing with exception handling
    TRY
        -- Open cursor for any table
        OPEN @cursor FOR EXECUTE 'SELECT * FROM ' || @source_table;
        
        LOOP
            TRY
                -- Fetch batch
                FETCH FORWARD @batch_size FROM @cursor 
                    BULK COLLECT INTO @records;
                
                EXIT WHEN CARDINALITY(@records) = 0;
                
                -- Process each record
                FOR i IN 1..CARDINALITY(@records) LOOP
                    SET @current_row = @current_row + 1;
                    
                    TRY
                        PERFORM process_record(@records[i]);
                        
                    EXCEPT WHEN OTHERS THEN
                        -- Handle individual record errors
                        SET @error_info = GET EXCEPTION_INFO;
                        SET @error_count = @error_count + 1;
                        
                        INSERT INTO processing_errors (
                            source_table,
                            row_number,
                            error_info,
                            record_data
                        ) VALUES (
                            @source_table,
                            @current_row,
                            @error_info,
                            ROW_TO_JSON(@records[i])
                        );
                        
                        -- Continue processing other records
                    END TRY;
                END LOOP;
                
                -- Commit batch
                COMMIT;
                
            EXCEPT WHEN cursor_error THEN
                -- Handle cursor-specific errors
                SET @error_info = GET EXCEPTION_INFO;
                RAISE WARNING 'Cursor error at row %: %', 
                    @error_info.context.row_number,
                    @error_info.message;
                
                -- Try to recover cursor position
                IF IS CURSOR_OPEN(@cursor) THEN
                    FETCH RELATIVE 1 FROM @cursor;  -- Skip bad record
                ELSE
                    RAISE;  -- Can't recover, re-raise
                END IF;
                
            EXCEPT WHEN insufficient_memory THEN
                -- Reduce batch size and retry
                SET @batch_size = @batch_size / 2;
                IF @batch_size < 1 THEN
                    RAISE;  -- Can't reduce further
                END IF;
                RAISE NOTICE 'Reducing batch size to %', @batch_size;
                
            END TRY;
        END LOOP;
        
        CLOSE @cursor;
        
    EXCEPT WHEN OTHERS THEN
        -- Cleanup on fatal error
        IF IS CURSOR_OPEN(@cursor) THEN
            CLOSE @cursor;
        END IF;
        
        -- Log summary
        INSERT INTO batch_process_log (
            table_name,
            total_rows,
            error_count,
            status,
            error_info
        ) VALUES (
            @source_table,
            @current_row,
            @error_count,
            'FAILED',
            GET EXCEPTION_INFO()
        );
        
        RAISE;  -- Re-raise for caller
    END TRY;
    
    -- Success logging
    INSERT INTO batch_process_log (
        table_name,
        total_rows,
        error_count,
        status
    ) VALUES (
        @source_table,
        @current_row,
        @error_count,
        'COMPLETED'
    );
    
-- Procedure-level exception handlers
WHEN connection_lost THEN
    PERFORM reconnect_and_resume(@cursor, @current_row);
    
WHEN OTHERS THEN
    PERFORM alert_operations_team(GET EXCEPTION_INFO);
    RAISE;
    
END;
$$ LANGUAGE plpgsql;
```

This comprehensive system provides robust exception handling with custom exceptions and universal cursor support for any multi-record source!