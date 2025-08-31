# SELECT INTO and Automatic Set Conversion

## Overview

ScratchBird supports SELECT INTO for direct variable assignment and FOR SELECT loops for iteration. Additionally, any SELECT result can be automatically converted to a SET type for full cursor support with bidirectional navigation.

## SELECT INTO Syntax

### Single Row SELECT INTO

```sql
-- Basic SELECT INTO for single row
DECLARE
    @customer_name VARCHAR(100);
    @customer_email VARCHAR(255);
    @customer_balance MONEY;
BEGIN
    -- Select single row into variables
    SELECT name, email, balance 
    FROM customers 
    WHERE customer_id = '123e4567-e89b-12d3-a456-426614174000'
    INTO @customer_name, @customer_email, @customer_balance;
    
    -- Use the variables
    IF @customer_balance > 1000 THEN
        PERFORM send_vip_offer(@customer_email);
    END IF;
END;

-- Multiple columns into record variable
DECLARE
    @customer_record RECORD;
BEGIN
    SELECT * FROM customers 
    WHERE customer_id = @id
    INTO @customer_record;
    
    -- Access fields
    PRINT @customer_record.name;
    PRINT @customer_record.email;
END;

-- INTO with expressions
DECLARE
    @total MONEY;
    @average MONEY;
    @count INT;
BEGIN
    SELECT 
        SUM(amount),
        AVG(amount),
        COUNT(*)
    FROM orders
    WHERE order_date = CURRENT_DATE
    INTO @total, @average, @count;
END;
```

### SELECT INTO with Error Handling

```sql
-- Handle no rows found
DECLARE
    @value VARCHAR(100);
BEGIN
    SELECT name 
    FROM users 
    WHERE id = @user_id
    INTO @value;
    
    -- Check if row was found
    IF NOT FOUND THEN
        RAISE EXCEPTION 'User not found';
    END IF;
END;

-- Handle multiple rows (error by default)
BEGIN
    SELECT name 
    FROM users 
    WHERE status = 'ACTIVE'
    INTO @name;  -- ERROR if multiple rows
    
EXCEPTION
    WHEN TOO_MANY_ROWS THEN
        RAISE WARNING 'Multiple active users found';
END;

-- STRICT mode - requires exactly one row
SELECT name 
FROM users 
WHERE id = @id
INTO STRICT @name;  -- Raises exception if not exactly 1 row
```

## FOR SELECT Iteration

### Basic FOR SELECT Loop

```sql
-- Iterate through result set
DECLARE
    @total MONEY = 0;
    @order_id UUID;
    @amount MONEY;
BEGIN
    FOR SELECT order_id, amount 
        FROM orders 
        WHERE customer_id = @customer_id
        FOR UPDATE  -- Lock for update
        INTO @order_id, @amount
    DO
    BEGIN
        -- Process each row
        @total = @total + @amount;
        
        -- Positioned update (WHERE CURRENT OF)
        UPDATE orders 
        SET processed = TRUE,
            processed_amount = @amount * 1.1
        WHERE CURRENT OF;  -- Updates current row in iteration
        
        -- Or positioned delete
        IF @amount < 0 THEN
            DELETE FROM orders WHERE CURRENT OF;
        END IF;
    END;
END;

-- FOR SELECT with record
DECLARE
    @record RECORD;
BEGIN
    FOR SELECT * FROM products
        WHERE category = 'Electronics'
        INTO @record
    DO
    BEGIN
        -- Process each product
        IF @record.price > 1000 THEN
            INSERT INTO expensive_products VALUES (@record.*);
        END IF;
    END;
END;
```

### FOR SELECT with SUSPEND (Returning Rows)

```sql
CREATE PROCEDURE get_filtered_customers()
RETURNS (
    customer_id UUID,
    name VARCHAR(100),
    total_orders INT
)
AS
BEGIN
    FOR SELECT 
        c.customer_id,
        c.name,
        COUNT(o.order_id)
    FROM customers c
    LEFT JOIN orders o ON c.customer_id = o.customer_id
    GROUP BY c.customer_id, c.name
    INTO :customer_id, :name, :total_orders
    DO
    BEGIN
        -- Apply additional logic
        IF :total_orders > 10 THEN
            -- Return this row
            SUSPEND;
        END IF;
    END;
END;
```

## Automatic Set Conversion

### Convert SELECT to SET for Full Cursor Support

```sql
-- Traditional forward-only iteration
FOR SELECT * FROM large_table INTO @record DO
BEGIN
    -- Can only move forward
    PROCESS @record;
END;

-- Convert to SET for bidirectional cursor
DECLARE
    @result_set SET OF RECORD;
    @cursor CURSOR;
    @record RECORD;
BEGIN
    -- Convert SELECT result to SET
    SET @result_set = (
        SELECT * FROM large_table 
        WHERE condition = TRUE
    )::SET;
    
    -- Now have full cursor capabilities
    DECLARE @cursor CURSOR FOR @result_set;
    
    OPEN @cursor SCROLL;  -- Scrollable cursor
    
    -- Navigate in any direction
    FETCH LAST FROM @cursor INTO @record;  -- Go to end
    FETCH PRIOR FROM @cursor INTO @record;  -- Go back
    FETCH FIRST FROM @cursor INTO @record;  -- Go to start
    FETCH ABSOLUTE 10 FROM @cursor INTO @record;  -- Go to position 10
    
    CLOSE @cursor;
END;
```

### Implicit Set Conversion

```sql
-- Automatic conversion when assigned to SET variable
DECLARE
    @customer_set SET OF customers%ROWTYPE;
BEGIN
    -- SELECT automatically converts to SET
    @customer_set = SELECT * FROM customers WHERE status = 'ACTIVE';
    
    -- Full cursor operations available
    FOR @customer IN CURSOR FOR @customer_set LOOP
        -- Process customer
    END LOOP;
    
    -- Or use explicit cursor
    DECLARE @cursor CURSOR FOR @customer_set;
    -- ... cursor operations ...
END;

-- Function returning SET
CREATE FUNCTION get_active_users()
RETURNS SET OF users%ROWTYPE
AS $$
BEGIN
    -- Return converts to SET automatically
    RETURN SELECT * FROM users WHERE active = TRUE;
END;
$$ LANGUAGE plpgsql;

-- Using the function
DECLARE
    @user_set SET OF users%ROWTYPE;
    @cursor CURSOR;
BEGIN
    @user_set = get_active_users();
    
    -- Full cursor support on returned SET
    DECLARE @cursor CURSOR SCROLL FOR @user_set;
    -- Navigate bidirectionally
END;
```

### SET Operations on Converted Results

```sql
DECLARE
    @set1 SET OF RECORD;
    @set2 SET OF RECORD;
    @union_set SET OF RECORD;
    @intersect_set SET OF RECORD;
BEGIN
    -- Convert multiple SELECTs to SETs
    @set1 = (SELECT * FROM table1 WHERE condition1)::SET;
    @set2 = (SELECT * FROM table2 WHERE condition2)::SET;
    
    -- Set operations
    @union_set = @set1 UNION @set2;
    @intersect_set = @set1 INTERSECT @set2;
    
    -- Iterate through union
    FOR @record IN @union_set LOOP
        -- Process combined results
    END LOOP;
END;
```

## Advanced SELECT INTO Patterns

### Bulk SELECT INTO Arrays

```sql
-- Select multiple rows into array
DECLARE
    @id_array UUID[];
    @name_array VARCHAR[];
BEGIN
    -- Bulk collect into arrays
    SELECT ARRAY_AGG(customer_id), ARRAY_AGG(name)
    FROM customers
    WHERE status = 'ACTIVE'
    INTO @id_array, @name_array;
    
    -- Process arrays
    FOR i IN 1..CARDINALITY(@id_array) LOOP
        PRINT @name_array[i];
    END LOOP;
END;

-- Alternative: SELECT INTO array directly
SELECT customer_id
FROM customers
WHERE status = 'ACTIVE'
INTO @id_array[];  -- Collect all rows into array
```

### SELECT INTO with CTE

```sql
DECLARE
    @summary RECORD;
BEGIN
    -- CTE with SELECT INTO
    WITH monthly_summary AS (
        SELECT 
            DATE_TRUNC('month', order_date) as month,
            COUNT(*) as order_count,
            SUM(amount) as total_amount
        FROM orders
        GROUP BY DATE_TRUNC('month', order_date)
    )
    SELECT * FROM monthly_summary
    WHERE month = DATE_TRUNC('month', CURRENT_DATE)
    INTO @summary;
    
    PRINT 'This month: ' || @summary.order_count || ' orders';
END;
```

### Dynamic SELECT INTO

```sql
-- Dynamic SQL with INTO
DECLARE
    @table_name VARCHAR(100) = 'customers';
    @column_name VARCHAR(100) = 'email';
    @result VARCHAR(255);
    @sql TEXT;
BEGIN
    -- Build dynamic query
    @sql = FORMAT('SELECT %I FROM %I WHERE id = $1', 
                  @column_name, @table_name);
    
    -- Execute with INTO
    EXECUTE @sql 
    USING @customer_id
    INTO @result;
    
    PRINT 'Result: ' || @result;
END;
```

## SET Conversion Options

### Materialized vs Lazy Sets

```sql
-- Materialized SET (immediate execution)
DECLARE
    @eager_set SET OF RECORD MATERIALIZED;
BEGIN
    -- Query executes immediately, results stored
    @eager_set = (
        SELECT * FROM huge_table
        WHERE complex_condition
    )::SET MATERIALIZED;
    
    -- Fast cursor operations on in-memory set
    DECLARE @cursor CURSOR FOR @eager_set;
END;

-- Lazy SET (deferred execution)
DECLARE
    @lazy_set SET OF RECORD LAZY;
BEGIN
    -- Query not executed until accessed
    @lazy_set = (
        SELECT * FROM huge_table
        WHERE complex_condition
    )::SET LAZY;
    
    -- Query executes as cursor moves
    DECLARE @cursor CURSOR FOR @lazy_set;
END;
```

### Indexed Sets for Performance

```sql
-- Create indexed SET for fast access
DECLARE
    @indexed_set SET OF customers%ROWTYPE 
        INDEXED BY (customer_id, email);
BEGIN
    -- Convert with indexing
    @indexed_set = (
        SELECT * FROM customers
    )::SET INDEXED BY (customer_id, email);
    
    -- Fast lookups
    @customer = @indexed_set[customer_id = @id];
    
    -- Or use cursor with index hints
    DECLARE @cursor CURSOR FOR @indexed_set
        WHERE customer_id = @id;  -- Uses index
END;
```

## Combining SELECT INTO with Cursors

### Hybrid Approach

```sql
CREATE PROCEDURE process_orders_hybrid(@customer_id UUID)
AS
DECLARE
    @total_amount MONEY;
    @order_count INT;
    @order_set SET OF orders%ROWTYPE;
    @cursor CURSOR;
    @current_order orders%ROWTYPE;
BEGIN
    -- First, get summary with SELECT INTO
    SELECT COUNT(*), SUM(amount)
    FROM orders
    WHERE customer_id = @customer_id
    INTO @order_count, @total_amount;
    
    IF @order_count > 100 THEN
        -- Large result set - use cursor approach
        @order_set = (
            SELECT * FROM orders
            WHERE customer_id = @customer_id
            ORDER BY order_date DESC
        )::SET;
        
        DECLARE @cursor CURSOR SCROLL FOR @order_set;
        OPEN @cursor;
        
        -- Navigate to recent orders
        FETCH FIRST FROM @cursor INTO @current_order;
        
        WHILE FOUND LOOP
            -- Process with full navigation
            IF @current_order.amount > @total_amount / @order_count THEN
                -- This is above average
                FETCH RELATIVE 2 FROM @cursor INTO @current_order;  -- Skip next
            ELSE
                FETCH NEXT FROM @cursor INTO @current_order;
            END IF;
        END LOOP;
        
        CLOSE @cursor;
    ELSE
        -- Small result set - use simple FOR SELECT
        FOR SELECT * FROM orders
            WHERE customer_id = @customer_id
            INTO @current_order
        DO
            PERFORM process_order(@current_order);
        END;
    END IF;
END;
```

## Performance Considerations

### When to Use Each Approach

```sql
-- SELECT INTO: Best for single rows or aggregates
SELECT MAX(salary) FROM employees INTO @max_salary;

-- FOR SELECT: Best for forward-only processing
FOR SELECT * FROM small_table INTO @record DO
    -- Simple forward processing
END;

-- SET Conversion: Best for complex navigation or reuse
@result_set = (SELECT * FROM table)::SET;
-- Multiple passes, random access, or bidirectional navigation
```

### Memory Management

```sql
-- Limit SET size for memory efficiency
DECLARE
    @limited_set SET OF RECORD(MAX_SIZE = 10000);
BEGIN
    -- Only first 10000 rows converted to SET
    @limited_set = (
        SELECT * FROM huge_table
        ORDER BY priority DESC
    )::SET;
    
    -- Or use windowed processing
    @window_set = (
        SELECT * FROM huge_table
        WHERE id BETWEEN @start_id AND @end_id
    )::SET;
END;
```

## Examples

### Example 1: Report Generation with Mixed Approaches

```sql
CREATE PROCEDURE generate_customer_report(@customer_id UUID)
RETURNS TABLE (
    section VARCHAR(50),
    data JSONB
)
AS
DECLARE
    @customer customers%ROWTYPE;
    @recent_orders SET OF orders%ROWTYPE;
    @order_cursor CURSOR;
    @stats RECORD;
BEGIN
    -- Get customer details with SELECT INTO
    SELECT * FROM customers 
    WHERE customer_id = @customer_id
    INTO STRICT @customer;
    
    -- Get statistics with SELECT INTO
    SELECT 
        COUNT(*) as total_orders,
        SUM(amount) as total_spent,
        AVG(amount) as avg_order
    FROM orders
    WHERE customer_id = @customer_id
    INTO @stats;
    
    -- Return customer section
    section = 'CUSTOMER';
    data = ROW_TO_JSON(@customer);
    SUSPEND;
    
    -- Return stats section
    section = 'STATISTICS';
    data = ROW_TO_JSON(@stats);
    SUSPEND;
    
    -- Get recent orders as SET for flexible processing
    @recent_orders = (
        SELECT * FROM orders
        WHERE customer_id = @customer_id
        ORDER BY order_date DESC
        LIMIT 100
    )::SET;
    
    -- Process orders with full cursor control
    DECLARE @order_cursor CURSOR SCROLL FOR @recent_orders;
    OPEN @order_cursor;
    
    -- Return recent orders
    FETCH FIRST FROM @order_cursor INTO @order;
    WHILE FOUND LOOP
        section = 'ORDER';
        data = ROW_TO_JSON(@order);
        SUSPEND;
        
        FETCH NEXT FROM @order_cursor INTO @order;
    END LOOP;
    
    CLOSE @order_cursor;
END;
```

### Example 2: Data Validation with Set Conversion

```sql
CREATE PROCEDURE validate_batch(@batch_id UUID)
AS
DECLARE
    @records_set SET OF import_records%ROWTYPE INDEXED BY (record_id);
    @validation_cursor CURSOR;
    @record import_records%ROWTYPE;
    @error_count INT = 0;
    @warning_count INT = 0;
BEGIN
    -- Convert import records to indexed SET
    @records_set = (
        SELECT * FROM import_records
        WHERE batch_id = @batch_id
        AND status = 'PENDING'
    )::SET INDEXED BY (record_id);
    
    -- First pass: validate with random access
    DECLARE @validation_cursor CURSOR SCROLL FOR @records_set;
    OPEN @validation_cursor;
    
    FETCH FIRST FROM @validation_cursor INTO @record;
    WHILE FOUND LOOP
        -- Validate current record
        IF NOT validate_record(@record) THEN
            @error_count = @error_count + 1;
            
            -- Check related records using index
            IF EXISTS (
                SELECT 1 FROM @records_set 
                WHERE parent_id = @record.record_id
            ) THEN
                -- Mark children as invalid too
                UPDATE import_records 
                SET status = 'PARENT_INVALID'
                WHERE parent_id = @record.record_id;
                
                -- Skip children in cursor
                FETCH RELATIVE 5 FROM @validation_cursor INTO @record;
                CONTINUE;
            END IF;
        END IF;
        
        FETCH NEXT FROM @validation_cursor INTO @record;
    END LOOP;
    
    CLOSE @validation_cursor;
    
    -- Update batch status
    UPDATE import_batches 
    SET error_count = @error_count,
        warning_count = @warning_count,
        status = CASE 
            WHEN @error_count = 0 THEN 'VALID'
            ELSE 'INVALID'
        END
    WHERE batch_id = @batch_id;
END;
```

This comprehensive system provides flexible data retrieval with SELECT INTO for simple cases and automatic SET conversion for complex cursor operations!