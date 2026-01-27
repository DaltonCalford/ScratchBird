# Trigger Context Variables and Introspection

## Overview

ScratchBird provides comprehensive trigger context variables that allow triggers to introspect their execution environment, understand what action triggered them, and access both old and new row values. These variables use SQL-style syntax for consistency.

Triggers may call procedures or functions that accept cursor handle parameters.
Cursor handles are allowed within the trigger body but cannot be passed into
the trigger itself. See:
`ScratchBird/docs/specifications/parser/PSQL_CURSOR_HANDLES.md`

## Core Trigger Variables

### Event Detection Variables

```sql
CREATE TRIGGER audit_trigger
AFTER INSERT OR UPDATE OR DELETE OR SELECT ON employees
FOR EACH ROW
EXECUTE FUNCTION audit_changes();

CREATE FUNCTION audit_changes() RETURNS TRIGGER AS $$
DECLARE
    @event_type VARCHAR;
    @event_time TIMESTAMP;
    @event_user VARCHAR;
BEGIN
    -- Get the trigger event type
    SET @event_type = GET TRIGGER_EVENT;  -- Returns: 'INSERT', 'UPDATE', 'DELETE', 'SELECT'
    
    -- Alternative syntax
    SET @event_type = TRIGGER_EVENT;
    SET @event_type = VALUE OF TRIGGER_EVENT;
    
    -- Conditional logic based on event
    IF GET TRIGGER_EVENT = 'INSERT' THEN
        -- Handle insert
    ELSIF GET TRIGGER_EVENT = 'UPDATE' THEN
        -- Handle update
    ELSIF GET TRIGGER_EVENT = 'DELETE' THEN
        -- Handle delete
    ELSIF GET TRIGGER_EVENT = 'SELECT' THEN
        -- Handle select (audit reads)
    END IF;
    
    RETURN NEW;  -- or OLD for DELETE
END;
$$ LANGUAGE plpgsql;
```

### Row Access Variables

```sql
-- NEW and OLD row access
CREATE TRIGGER update_tracker
BEFORE UPDATE ON products
FOR EACH ROW
EXECUTE FUNCTION track_changes();

CREATE FUNCTION track_changes() RETURNS TRIGGER AS $$
BEGIN
    -- Access column values
    IF NEW.price != OLD.price THEN
        INSERT INTO price_history (
            product_id,
            old_price,
            new_price,
            changed_by,
            changed_at
        ) VALUES (
            NEW.product_id,
            OLD.price,
            NEW.price,
            GET TRIGGER_USER,
            GET TRIGGER_TIMESTAMP
        );
    END IF;
    
    -- Check what columns changed
    IF IS COLUMN CHANGED(price) THEN
        -- Price was modified
    END IF;
    
    -- Get list of all changed columns
    DECLARE @changed_columns TEXT[];
    SET @changed_columns = GET CHANGED_COLUMNS;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

## Complete Trigger Context Variables

### Event Information

```sql
-- Basic event information
GET TRIGGER_EVENT           -- 'INSERT', 'UPDATE', 'DELETE', 'SELECT'
GET TRIGGER_TIMING          -- 'BEFORE', 'AFTER', 'INSTEAD OF'
GET TRIGGER_LEVEL           -- 'ROW', 'STATEMENT'
GET TRIGGER_NAME            -- Name of the current trigger
GET TRIGGER_SCHEMA          -- Schema of the trigger
GET TRIGGER_TABLE           -- Table that triggered the event
GET TRIGGER_POSITION        -- Position in trigger chain (1, 2, 3...)

-- Database context
GET TRIGGER_DATABASE        -- Current database name
GET TRIGGER_CATALOG         -- Current catalog name
GET TRIGGER_TABLESPACE      -- Tablespace of the table

-- User and session
GET TRIGGER_USER            -- User who caused the trigger
GET TRIGGER_SESSION_USER    -- Session user
GET TRIGGER_APPLICATION     -- Application name
GET TRIGGER_CLIENT_IP       -- Client IP address
GET TRIGGER_CLIENT_PORT     -- Client port
GET TRIGGER_PID             -- Process ID

-- Timing information
GET TRIGGER_TIMESTAMP       -- When trigger fired
GET TRIGGER_TRANSACTION_ID  -- Current transaction ID
GET TRIGGER_STATEMENT_ID    -- Statement ID within transaction
GET TRIGGER_COMMAND_TAG     -- SQL command that fired trigger

-- Nested trigger detection
GET TRIGGER_DEPTH           -- Nesting level (1 = not nested)
GET TRIGGER_PARENT          -- Parent trigger name (if nested)
GET TRIGGER_CHAIN           -- Array of trigger names in chain
```

### Column Change Detection

```sql
CREATE FUNCTION comprehensive_audit() RETURNS TRIGGER AS $$
DECLARE
    @changed_columns TEXT[];
    @column_name TEXT;
    @old_value VARIANT;
    @new_value VARIANT;
BEGIN
    -- Get list of changed columns
    SET @changed_columns = GET CHANGED_COLUMNS;
    
    -- Iterate through changed columns
    FOREACH @column_name IN ARRAY @changed_columns LOOP
        -- Get old and new values dynamically
        SET @old_value = GET COLUMN_VALUE(@column_name, OLD);
        SET @new_value = GET COLUMN_VALUE(@column_name, NEW);
        
        -- Alternative syntax
        SET @old_value = EXTRACT(COLUMN @column_name FROM OLD);
        SET @new_value = EXTRACT(COLUMN @column_name FROM NEW);
        
        -- Log the change
        INSERT INTO audit_log (
            table_name,
            column_name,
            old_value,
            new_value,
            change_type,
            changed_at
        ) VALUES (
            GET TRIGGER_TABLE,
            @column_name,
            @old_value::TEXT,
            @new_value::TEXT,
            GET TRIGGER_EVENT,
            GET TRIGGER_TIMESTAMP
        );
    END LOOP;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

### Column-Specific Functions

```sql
-- Check if specific column changed
IS COLUMN CHANGED(column_name)
HAS COLUMN CHANGED(column_name)
COLUMN_CHANGED(column_name)

-- Get change information for a column
GET COLUMN_CHANGE(column_name)  -- Returns (old_value, new_value)
GET COLUMN_OLD_VALUE(column_name)
GET COLUMN_NEW_VALUE(column_name)

-- Check multiple columns
ANY COLUMNS CHANGED(column1, column2, column3)
ALL COLUMNS CHANGED(column1, column2, column3)
NO COLUMNS CHANGED(column1, column2, column3)

-- Get change delta for numeric columns
GET COLUMN_DELTA(column_name)  -- NEW.column - OLD.column

-- Get percentage change
GET COLUMN_CHANGE_PERCENT(column_name)  -- ((NEW - OLD) / OLD) * 100
```

## Advanced Trigger Variables

### Statement-Level Variables

```sql
CREATE TRIGGER bulk_operation_tracker
AFTER INSERT OR UPDATE OR DELETE ON large_table
FOR EACH STATEMENT
EXECUTE FUNCTION track_bulk_changes();

CREATE FUNCTION track_bulk_changes() RETURNS TRIGGER AS $$
DECLARE
    @row_count INTEGER;
    @affected_ids UUID[];
BEGIN
    -- Get count of affected rows
    SET @row_count = GET TRIGGER_ROW_COUNT;
    
    -- Get affected row identifiers (if available)
    SET @affected_ids = GET TRIGGER_AFFECTED_IDS;
    
    -- Access transition tables (for statement triggers)
    INSERT INTO bulk_audit (
        operation,
        row_count,
        total_before,
        total_after
    )
    SELECT 
        GET TRIGGER_EVENT,
        GET TRIGGER_ROW_COUNT,
        (SELECT COUNT(*) FROM OLD_TABLE),  -- Transition table
        (SELECT COUNT(*) FROM NEW_TABLE)   -- Transition table
    ;
    
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

### Transition Tables

```sql
-- Statement-level triggers can access transition tables
CREATE TRIGGER summary_updater
AFTER UPDATE ON sales
REFERENCING OLD TABLE AS old_sales NEW TABLE AS new_sales
FOR EACH STATEMENT
EXECUTE FUNCTION update_summaries();

CREATE FUNCTION update_summaries() RETURNS TRIGGER AS $$
BEGIN
    -- Compare old and new states
    INSERT INTO sales_summary_changes (
        product_id,
        old_total,
        new_total,
        difference
    )
    SELECT 
        COALESCE(o.product_id, n.product_id),
        o.total,
        n.total,
        n.total - o.total
    FROM 
        (SELECT product_id, SUM(amount) as total 
         FROM old_sales GROUP BY product_id) o
    FULL OUTER JOIN
        (SELECT product_id, SUM(amount) as total 
         FROM new_sales GROUP BY product_id) n
    ON o.product_id = n.product_id
    WHERE o.total IS DISTINCT FROM n.total;
    
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

### Trigger Interaction Variables

```sql
-- Variables for trigger chain coordination
GET TRIGGER_SHARED_DATA        -- Shared data between triggers
SET TRIGGER_SHARED_DATA        -- Set data for next trigger
APPEND TRIGGER_SHARED_DATA     -- Add to shared data

-- Skip remaining triggers
SET TRIGGER_SKIP_REMAINING TO TRUE;

-- Modify the operation
SET TRIGGER_OPERATION TO 'UPDATE';  -- Convert INSERT to UPDATE
SET TRIGGER_CANCEL TO TRUE;         -- Cancel the operation

-- Example: Trigger coordination
CREATE FUNCTION first_trigger() RETURNS TRIGGER AS $$
BEGIN
    -- Set data for next trigger
    SET TRIGGER_SHARED_DATA TO JSON_BUILD_OBJECT(
        'validated', true,
        'validation_time', CURRENT_TIMESTAMP
    );
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION second_trigger() RETURNS TRIGGER AS $$
DECLARE
    @shared JSONB;
BEGIN
    -- Get data from previous trigger
    SET @shared = GET TRIGGER_SHARED_DATA;
    
    IF @shared->>'validated' = 'true' THEN
        -- Previous trigger validated, proceed
        RETURN NEW;
    ELSE
        -- Validation failed, cancel
        SET TRIGGER_CANCEL TO TRUE;
        RETURN NULL;
    END IF;
END;
$$ LANGUAGE plpgsql;
```

## Conditional Trigger Execution

### When Clause Variables

```sql
-- Trigger with WHEN clause can use special variables
CREATE TRIGGER price_increase_alert
AFTER UPDATE ON products
FOR EACH ROW
WHEN (NEW.price > OLD.price * 1.1)  -- 10% increase
EXECUTE FUNCTION alert_price_increase();

-- Inside trigger, check why it fired
CREATE FUNCTION alert_price_increase() RETURNS TRIGGER AS $$
BEGIN
    -- Get the condition that triggered this
    IF GET TRIGGER_WHEN_RESULT = TRUE THEN
        -- The WHEN condition was met
        INSERT INTO price_alerts (
            product_id,
            old_price,
            new_price,
            increase_percent,
            alerted_at
        ) VALUES (
            NEW.product_id,
            OLD.price,
            NEW.price,
            GET COLUMN_CHANGE_PERCENT(price),
            GET TRIGGER_TIMESTAMP
        );
    END IF;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

## Database-Level Trigger Variables

```sql
-- Database-level triggers have special variables
CREATE TRIGGER database_connection_tracker
ON DATABASE
AFTER CONNECT
EXECUTE FUNCTION track_connection();

CREATE FUNCTION track_connection() RETURNS EVENT_TRIGGER AS $$
DECLARE
    @event_type VARCHAR;
    @connection_info JSONB;
BEGIN
    SET @event_type = GET TRIGGER_DATABASE_EVENT;  -- 'CONNECT', 'DISCONNECT', etc.
    
    SET @connection_info = JSON_BUILD_OBJECT(
        'user', GET TRIGGER_USER,
        'application', GET TRIGGER_APPLICATION,
        'client_ip', GET TRIGGER_CLIENT_IP,
        'connected_at', GET TRIGGER_TIMESTAMP,
        'database', GET TRIGGER_DATABASE,
        'session_id', GET TRIGGER_SESSION_ID,
        'protocol_version', GET TRIGGER_PROTOCOL_VERSION
    );
    
    INSERT INTO connection_log (event_type, connection_info)
    VALUES (@event_type, @connection_info);
END;
$$ LANGUAGE plpgsql;
```

## DDL Trigger Variables

```sql
-- DDL triggers have access to object information
CREATE TRIGGER ddl_tracker
ON DATABASE
AFTER CREATE OR ALTER OR DROP
EXECUTE FUNCTION track_ddl_changes();

CREATE FUNCTION track_ddl_changes() RETURNS EVENT_TRIGGER AS $$
DECLARE
    @ddl_command VARCHAR;
    @object_type VARCHAR;
    @object_name VARCHAR;
    @object_schema VARCHAR;
BEGIN
    SET @ddl_command = GET TRIGGER_DDL_COMMAND;     -- 'CREATE TABLE', 'ALTER INDEX', etc.
    SET @object_type = GET TRIGGER_OBJECT_TYPE;     -- 'TABLE', 'INDEX', 'VIEW', etc.
    SET @object_name = GET TRIGGER_OBJECT_NAME;     -- Name of object
    SET @object_schema = GET TRIGGER_OBJECT_SCHEMA; -- Schema of object
    
    -- Get the actual DDL statement
    DECLARE @ddl_text TEXT;
    SET @ddl_text = GET TRIGGER_DDL_STATEMENT;
    
    INSERT INTO ddl_history (
        command,
        object_type,
        object_schema,
        object_name,
        ddl_text,
        executed_by,
        executed_at
    ) VALUES (
        @ddl_command,
        @object_type,
        @object_schema,
        @object_name,
        @ddl_text,
        GET TRIGGER_USER,
        GET TRIGGER_TIMESTAMP
    );
END;
$$ LANGUAGE plpgsql;
```

## Performance and Debugging Variables

```sql
-- Performance monitoring in triggers
CREATE FUNCTION performance_aware_trigger() RETURNS TRIGGER AS $$
DECLARE
    @start_time TIMESTAMP;
    @execution_time INTERVAL;
    @memory_before BIGINT;
    @memory_after BIGINT;
BEGIN
    SET @start_time = GET TRIGGER_START_TIME;
    SET @memory_before = GET TRIGGER_MEMORY_USAGE;
    
    -- Do trigger work...
    
    SET @execution_time = GET TRIGGER_EXECUTION_TIME;
    SET @memory_after = GET TRIGGER_MEMORY_USAGE;
    
    IF @execution_time > INTERVAL '1 second' THEN
        INSERT INTO slow_trigger_log (
            trigger_name,
            execution_time,
            memory_used,
            row_data
        ) VALUES (
            GET TRIGGER_NAME,
            @execution_time,
            @memory_after - @memory_before,
            ROW_TO_JSON(NEW)
        );
    END IF;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Debug information
GET TRIGGER_DEBUG_INFO          -- Comprehensive debug information
GET TRIGGER_STACK_TRACE         -- Call stack if nested
GET TRIGGER_EXECUTION_PLAN      -- Query plan that caused trigger
```

## Trigger Variable Summary Table

| Variable | Description | Available In |
|----------|-------------|--------------|
| `GET TRIGGER_EVENT` | INSERT, UPDATE, DELETE, SELECT | All triggers |
| `NEW` | New row values | INSERT, UPDATE |
| `OLD` | Old row values | UPDATE, DELETE |
| `GET CHANGED_COLUMNS` | Array of modified columns | UPDATE |
| `GET TRIGGER_TIMING` | BEFORE, AFTER, INSTEAD OF | All triggers |
| `GET TRIGGER_LEVEL` | ROW, STATEMENT | All triggers |
| `GET TRIGGER_ROW_COUNT` | Number of affected rows | Statement triggers |
| `OLD_TABLE` | Transition table of old rows | Statement triggers |
| `NEW_TABLE` | Transition table of new rows | Statement triggers |
| `GET TRIGGER_USER` | User who triggered event | All triggers |
| `GET TRIGGER_TIMESTAMP` | When trigger fired | All triggers |
| `GET TRIGGER_DEPTH` | Nesting level | All triggers |
| `IS COLUMN CHANGED()` | Check if column modified | UPDATE triggers |

## Examples

### Example 1: Comprehensive Audit Trigger

```sql
CREATE TRIGGER comprehensive_audit
AFTER INSERT OR UPDATE OR DELETE ON sensitive_table
FOR EACH ROW
EXECUTE FUNCTION audit_everything();

CREATE FUNCTION audit_everything() RETURNS TRIGGER AS $$
DECLARE
    @event VARCHAR;
    @changed_columns TEXT[];
    @audit_data JSONB;
BEGIN
    SET @event = GET TRIGGER_EVENT;
    
    -- Build audit record
    SET @audit_data = JSON_BUILD_OBJECT(
        'event', @event,
        'table', GET TRIGGER_TABLE,
        'user', GET TRIGGER_USER,
        'application', GET TRIGGER_APPLICATION,
        'client_ip', GET TRIGGER_CLIENT_IP,
        'timestamp', GET TRIGGER_TIMESTAMP,
        'transaction_id', GET TRIGGER_TRANSACTION_ID
    );
    
    -- Add row data based on event
    IF @event = 'INSERT' THEN
        SET @audit_data = @audit_data || 
            JSON_BUILD_OBJECT('new_data', ROW_TO_JSON(NEW));
    ELSIF @event = 'UPDATE' THEN
        SET @changed_columns = GET CHANGED_COLUMNS;
        SET @audit_data = @audit_data || JSON_BUILD_OBJECT(
            'old_data', ROW_TO_JSON(OLD),
            'new_data', ROW_TO_JSON(NEW),
            'changed_columns', @changed_columns
        );
    ELSIF @event = 'DELETE' THEN
        SET @audit_data = @audit_data || 
            JSON_BUILD_OBJECT('old_data', ROW_TO_JSON(OLD));
    END IF;
    
    INSERT INTO audit_log (audit_data) VALUES (@audit_data);
    
    RETURN CASE 
        WHEN @event = 'DELETE' THEN OLD
        ELSE NEW
    END;
END;
$$ LANGUAGE plpgsql;
```

### Example 2: Smart Update Trigger

```sql
CREATE TRIGGER smart_updater
BEFORE UPDATE ON products
FOR EACH ROW
EXECUTE FUNCTION smart_update();

CREATE FUNCTION smart_update() RETURNS TRIGGER AS $$
BEGIN
    -- Track price changes
    IF IS COLUMN CHANGED(price) THEN
        IF GET COLUMN_CHANGE_PERCENT(price) > 50 THEN
            -- Large price change, require approval
            NEW.status = 'PENDING_APPROVAL';
            NEW.approval_required = TRUE;
            NEW.change_reason = 'Price change > 50%: ' || 
                                OLD.price || ' -> ' || NEW.price;
        END IF;
    END IF;
    
    -- Auto-update timestamp only if other columns changed
    IF ANY COLUMNS CHANGED(name, description, price, category) THEN
        NEW.updated_at = GET TRIGGER_TIMESTAMP;
        NEW.updated_by = GET TRIGGER_USER;
    END IF;
    
    -- Maintain history
    IF OLD IS DISTINCT FROM NEW THEN
        INSERT INTO products_history
        SELECT OLD.*, GET TRIGGER_TIMESTAMP, GET TRIGGER_USER, 'UPDATE';
    END IF;
    
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

This comprehensive trigger variable system provides complete introspection and control over trigger execution!
