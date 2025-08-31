# SQL-Style Syntax and Polymorphic Types Specification

## Overview

ScratchBird implements SQL-style syntax for sequences and enums, avoiding function-like syntax in favor of more readable SQL statements. Additionally, it supports polymorphic variables in procedures/functions with runtime type extraction.

## SQL-Style Sequence Operations

### Traditional vs SQL-Style Syntax

```sql
-- TRADITIONAL (Not Preferred):
SELECT CURRVAL('my_sequence');
SELECT NEXTVAL('my_sequence');
SELECT SETVAL('my_sequence', 100);
SELECT LASTVAL();

-- SQL-STYLE (ScratchBird Preferred):
GET CURRENT VALUE FOR my_sequence;
GET NEXT VALUE FOR my_sequence;
SET CURRENT VALUE FOR my_sequence TO 100;
GET LAST VALUE;

-- Alternative SQL-style variations
NEXT VALUE FOR my_sequence;
CURRENT VALUE FOR my_sequence;
SET my_sequence TO 100;
```

### Complete Sequence Syntax

```sql
-- Create sequence
CREATE SEQUENCE order_id_seq
    START WITH 1000
    INCREMENT BY 1
    MINVALUE 1000
    MAXVALUE 999999999
    CACHE 20;

-- Get next value (all equivalent)
GET NEXT VALUE FOR order_id_seq;
NEXT VALUE FOR order_id_seq;
ADVANCE order_id_seq;

-- Get current value (without advancing)
GET CURRENT VALUE FOR order_id_seq;
CURRENT VALUE FOR order_id_seq;
VALUE OF order_id_seq;

-- Set value
SET CURRENT VALUE FOR order_id_seq TO 5000;
SET order_id_seq TO 5000;
RESET order_id_seq TO 5000;

-- Reset to start
RESET order_id_seq;
RESTART order_id_seq;

-- Get last generated value in session
GET LAST VALUE;
LAST GENERATED VALUE;

-- Alter sequence
ALTER SEQUENCE order_id_seq
    RESTART WITH 2000
    INCREMENT BY 5
    CACHE 50;
```

### Using Sequences in Tables

```sql
-- In DEFAULT clause
CREATE TABLE orders (
    order_id BIGINT DEFAULT (NEXT VALUE FOR order_id_seq),
    customer_id UUID,
    order_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- In INSERT
INSERT INTO orders (order_id, customer_id)
VALUES (NEXT VALUE FOR order_id_seq, customer_uuid);

-- Multiple values from sequence
INSERT INTO orders (order_id, customer_id)
SELECT 
    NEXT VALUE FOR order_id_seq,
    customer_id
FROM customers
WHERE status = 'Active';
```

## SQL-Style Enum Operations

### Enum Navigation Syntax

```sql
-- Create enum
CREATE DOMAIN day_of_week AS ENUM (
    'Sunday', 'Monday', 'Tuesday', 'Wednesday', 
    'Thursday', 'Friday', 'Saturday'
) WITH OPTIONS (WRAP = TRUE);

-- Declare variable
DECLARE @current_day day_of_week = 'Wednesday';

-- Get value (multiple styles)
GET VALUE FOR @current_day;           -- Returns 'Wednesday'
VALUE OF @current_day;                 -- Returns 'Wednesday'
CURRENT VALUE OF @current_day;        -- Returns 'Wednesday'

-- Get position
GET POSITION FOR @current_day;        -- Returns 3
POSITION OF @current_day;             -- Returns 3
ORDINAL OF @current_day;              -- Returns 3

-- Move to next value
SET NEXT VALUE FOR @current_day;      -- Sets to 'Thursday'
ADVANCE @current_day;                  -- Sets to 'Thursday'
@current_day := NEXT VALUE;           -- Sets to 'Thursday'

-- Move to previous value
SET PRIOR VALUE FOR @current_day;     -- Sets to 'Tuesday'
SET PREVIOUS VALUE FOR @current_day;  -- Sets to 'Tuesday'
RETREAT @current_day;                  -- Sets to 'Tuesday'
@current_day := PRIOR VALUE;          -- Sets to 'Tuesday'

-- Jump multiple positions
ADVANCE @current_day BY 3;             -- Move forward 3 positions
RETREAT @current_day BY 2;             -- Move backward 2 positions
SET @current_day FORWARD 3;            -- Move forward 3
SET @current_day BACKWARD 2;           -- Move backward 2

-- Set to specific value
SET VALUE FOR @current_day TO 'Friday';
SET @current_day TO 'Friday';
@current_day := 'Friday';

-- Set by position
SET POSITION FOR @current_day TO 5;    -- Sets to 'Friday' (position 5)
SET @current_day TO POSITION 5;        -- Sets to 'Friday'
```

### Enum Wrap-Around Behavior

```sql
-- With WRAP = TRUE
DECLARE @day day_of_week = 'Saturday';
SET NEXT VALUE FOR @day;               -- Wraps to 'Sunday'

DECLARE @day2 day_of_week = 'Sunday';  
SET PRIOR VALUE FOR @day2;             -- Wraps to 'Saturday'

-- With WRAP = FALSE (default)
CREATE DOMAIN status AS ENUM ('New', 'Active', 'Completed');
DECLARE @status status = 'Completed';
SET NEXT VALUE FOR @status;            -- ERROR: Enum overflow

-- Conditional advancement
IF CAN ADVANCE @status THEN
    SET NEXT VALUE FOR @status;
END IF;

-- Check boundaries
IF @day IS FIRST VALUE THEN
    PRINT 'At beginning of week';
END IF;

IF @day IS LAST VALUE THEN
    PRINT 'At end of week';
END IF;
```

### Enum Comparisons and Ranges

```sql
-- Compare positions
IF @current_day > 'Wednesday' THEN
    -- Thursday, Friday, Saturday
END IF;

-- Range operations
IF @current_day BETWEEN 'Monday' AND 'Friday' THEN
    PRINT 'Weekday';
END IF;

-- Get range of values
GET VALUES FROM 'Monday' TO 'Friday' FOR day_of_week;
-- Returns: ['Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday']

-- Count steps between values
GET DISTANCE FROM 'Monday' TO 'Friday' FOR day_of_week;
-- Returns: 4
```

## Polymorphic Variables

### Variable Declaration with Variant Type

```sql
-- Declare polymorphic variable
DECLARE @data VARIANT;
DECLARE @result ANYTYPE;
DECLARE @flexible POLYMORPHIC;

-- Assignment from different types
SET @data = 42;                        -- Integer
SET @data = 'Hello World';             -- String
SET @data = CURRENT_TIMESTAMP;         -- Timestamp
SET @data = ARRAY[1, 2, 3];           -- Array
SET @data = ROW('John', 'Doe', 30);   -- Record
SET @data = UUID();                    -- UUID
```

### Runtime Type Extraction

```sql
-- Extract type information
EXTRACT(DATATYPE FROM @data);          -- Returns 'INTEGER', 'VARCHAR', etc.
GET DATATYPE OF @data;                 -- Alternative syntax
TYPE OF @data;                          -- Shorter syntax

-- Extract type properties
EXTRACT(TYPE_NAME FROM @data);         -- 'INTEGER'
EXTRACT(TYPE_CATEGORY FROM @data);     -- 'NUMERIC'
EXTRACT(TYPE_SIZE FROM @data);         -- 4 (bytes)
EXTRACT(TYPE_PRECISION FROM @data);    -- For numeric types
EXTRACT(TYPE_SCALE FROM @data);        -- For decimal types
EXTRACT(TYPE_LENGTH FROM @data);       -- For string types
EXTRACT(TYPE_NULLABLE FROM @data);     -- TRUE/FALSE

-- Conditional logic based on type
IF EXTRACT(DATATYPE FROM @data) = 'INTEGER' THEN
    SET @data = @data + 1;
ELSIF EXTRACT(DATATYPE FROM @data) = 'VARCHAR' THEN
    SET @data = @data || ' (modified)';
END IF;

-- Type checking
IF @data IS OF TYPE INTEGER THEN
    -- Integer-specific logic
END IF;

IF @data IS NUMERIC TYPE THEN
    -- Works for INTEGER, DECIMAL, FLOAT, etc.
END IF;
```

### Polymorphic Functions

```sql
-- Function accepting any type
CREATE FUNCTION process_value(@input VARIANT) 
RETURNS VARIANT AS $$
DECLARE
    @result VARIANT;
    @type_name VARCHAR(50);
BEGIN
    -- Get the type
    SET @type_name = EXTRACT(DATATYPE FROM @input);
    
    -- Process based on type
    CASE @type_name
        WHEN 'INTEGER' THEN
            SET @result = @input * 2;
        WHEN 'VARCHAR' THEN
            SET @result = UPPER(@input);
        WHEN 'DATE' THEN
            SET @result = @input + INTERVAL '1 day';
        WHEN 'ARRAY' THEN
            SET @result = ARRAY_LENGTH(@input);
        ELSE
            SET @result = @input;  -- Return unchanged
    END CASE;
    
    RETURN @result;
END;
$$ LANGUAGE plpgsql;

-- Usage
SELECT process_value(42);              -- Returns 84
SELECT process_value('hello');         -- Returns 'HELLO'
SELECT process_value(CURRENT_DATE);    -- Returns tomorrow
```

### Type-Safe Casting

```sql
-- Safe casting with type checking
CREATE FUNCTION safe_cast_to_int(@value VARIANT) 
RETURNS INTEGER AS $$
BEGIN
    -- Check if castable
    IF CAN CAST @value TO INTEGER THEN
        RETURN CAST(@value AS INTEGER);
    ELSE
        RETURN NULL;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Try casting with default
CREATE FUNCTION try_cast(@value VARIANT, @target_type VARCHAR, @default VARIANT)
RETURNS VARIANT AS $$
BEGIN
    IF CAN CAST @value TO @target_type THEN
        RETURN CAST(@value AS @target_type);
    ELSE
        RETURN @default;
    END IF;
END;
$$ LANGUAGE plpgsql;
```

### Polymorphic Collections

```sql
-- Array of variants
DECLARE @mixed_array VARIANT[];
SET @mixed_array = ARRAY[42, 'text', CURRENT_DATE, 3.14];

-- Process each element
FOR i IN 1..ARRAY_LENGTH(@mixed_array) LOOP
    DECLARE @element VARIANT = @mixed_array[i];
    DECLARE @type VARCHAR = EXTRACT(DATATYPE FROM @element);
    
    PRINT 'Element ' || i || ' is of type ' || @type;
END LOOP;

-- Record with variant fields
CREATE TYPE flexible_record AS RECORD (
    id INTEGER,
    data VARIANT,
    metadata VARIANT
);

-- Table with variant column
CREATE TABLE audit_log (
    log_id UUID GENERATED ALWAYS AS IDENTITY,
    table_name VARCHAR(100),
    old_value VARIANT,  -- Can store any type
    new_value VARIANT,  -- Can store any type
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## Backward Compatibility Functions

For compatibility, function-style wrappers are provided:

```sql
-- Sequence compatibility functions (call SQL-style internally)
CREATE FUNCTION NEXTVAL(seq_name VARCHAR) RETURNS BIGINT AS $$
BEGIN
    EXECUTE 'GET NEXT VALUE FOR ' || seq_name INTO result;
    RETURN result;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION CURRVAL(seq_name VARCHAR) RETURNS BIGINT AS $$
BEGIN
    EXECUTE 'GET CURRENT VALUE FOR ' || seq_name INTO result;
    RETURN result;
END;
$$ LANGUAGE plpgsql;

CREATE FUNCTION SETVAL(seq_name VARCHAR, value BIGINT) RETURNS BIGINT AS $$
BEGIN
    EXECUTE 'SET CURRENT VALUE FOR ' || seq_name || ' TO ' || value;
    RETURN value;
END;
$$ LANGUAGE plpgsql;

-- But SQL-style is preferred and optimized
```

## System Views for Type Information

```sql
-- View for variant type registry
CREATE VIEW sys.variant_types AS
SELECT 
    type_id,
    type_name,
    type_category,
    base_type,
    storage_size,
    is_composite,
    is_array,
    is_domain
FROM sys.types
WHERE supports_variant = TRUE;

-- Current session variables
CREATE VIEW sys.session_variables AS
SELECT 
    variable_name,
    variable_type,
    current_value,
    is_variant,
    actual_type,  -- For variant types
    declaration_scope,
    last_modified
FROM sys.variables
WHERE session_id = CURRENT_SESSION_ID();
```

## Performance Considerations

### Variant Storage

```sql
-- Variants use tagged union storage
-- Header (8 bytes): Type ID (4) + Flags (4)
-- Data: Inline for small types, pointer for large

-- Optimize variant columns
CREATE TABLE events (
    event_id UUID,
    event_data VARIANT
) WITH (
    VARIANT_STORAGE = 'COLUMNAR'  -- Store by type for better compression
);

-- Index on variant type
CREATE INDEX idx_event_type 
ON events((EXTRACT(DATATYPE FROM event_data)));
```

### SQL-Style Optimization

```sql
-- SQL-style syntax is optimized at parse time
-- No function call overhead

-- These are equivalent in result but SQL-style is faster:
NEXT VALUE FOR my_sequence;        -- Direct execution
NEXTVAL('my_sequence');            -- Function call overhead

-- Enum operations are inlined
SET NEXT VALUE FOR @enum_var;      -- Direct manipulation
advance_enum(@enum_var);            -- Function call overhead
```

## Examples

### Example 1: Day Scheduler

```sql
CREATE DOMAIN weekday AS ENUM (
    'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday'
) WITH OPTIONS (WRAP = FALSE);

CREATE PROCEDURE schedule_week() AS $$
DECLARE
    @current_day weekday = 'Monday';
BEGIN
    WHILE @current_day IS NOT NULL LOOP
        PRINT 'Scheduling for ' || VALUE OF @current_day;
        
        -- Schedule tasks for this day
        PERFORM schedule_day_tasks(@current_day);
        
        -- Move to next day
        IF CAN ADVANCE @current_day THEN
            SET NEXT VALUE FOR @current_day;
        ELSE
            EXIT;  -- End of week
        END IF;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

### Example 2: Generic Logger

```sql
CREATE FUNCTION log_change(
    @table_name VARCHAR,
    @column_name VARCHAR,
    @old_value VARIANT,
    @new_value VARIANT
) RETURNS VOID AS $$
DECLARE
    @old_type VARCHAR = EXTRACT(DATATYPE FROM @old_value);
    @new_type VARCHAR = EXTRACT(DATATYPE FROM @new_value);
BEGIN
    INSERT INTO change_log (
        table_name,
        column_name,
        old_value,
        old_type,
        new_value,
        new_type,
        changed_at
    ) VALUES (
        @table_name,
        @column_name,
        @old_value,
        @old_type,
        @new_value,
        @new_type,
        CURRENT_TIMESTAMP
    );
    
    -- Type change warning
    IF @old_type <> @new_type THEN
        RAISE WARNING 'Type changed from % to %', @old_type, @new_type;
    END IF;
END;
$$ LANGUAGE plpgsql;
```

### Example 3: State Machine

```sql
CREATE DOMAIN order_state AS ENUM (
    'Draft', 'Submitted', 'Approved', 'Processing', 'Shipped', 'Delivered'
) WITH OPTIONS (WRAP = FALSE);

CREATE PROCEDURE advance_order(@order_id UUID) AS $$
DECLARE
    @current_state order_state;
BEGIN
    -- Get current state
    SELECT state INTO @current_state
    FROM orders WHERE order_id = @order_id;
    
    -- Check if can advance
    IF CAN ADVANCE @current_state THEN
        SET NEXT VALUE FOR @current_state;
        
        UPDATE orders 
        SET state = @current_state,
            updated_at = CURRENT_TIMESTAMP
        WHERE order_id = @order_id;
        
        PRINT 'Order advanced to ' || VALUE OF @current_state;
    ELSE
        RAISE EXCEPTION 'Order already in final state';
    END IF;
END;
$$ LANGUAGE plpgsql;
```

This SQL-style syntax makes ScratchBird more readable and consistent while providing powerful polymorphic capabilities!