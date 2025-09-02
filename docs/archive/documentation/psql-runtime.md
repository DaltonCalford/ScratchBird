### PSQL Runtime

**What it is**

PSQL (Procedural SQL) is ScratchBird's procedural extension to SQL, providing a complete programming environment within the database. It enables complex business logic, control flow, error handling, and dynamic SQL execution. The PSQL runtime includes variables, cursors, exception handling, and security contexts, allowing you to build sophisticated database applications entirely within the database engine.

**Why it matters**

- **Performance**: Execute complex logic close to data, minimizing network overhead
- **Atomicity**: Encapsulate multi-step operations in transactions
- **Security**: Control execution privileges and data access
- **Maintainability**: Centralize business rules in the database
- **Portability**: PSQL code moves with the database

**How to use it**

Start with EXECUTE BLOCK for ad-hoc procedural code, then create stored procedures and functions for reusable logic. Use cursors for row-by-row processing, exception handlers for robust error management, and dynamic SQL for flexible queries.

## Core PSQL Features

**Parser**: `src/engine/parser_psql.cpp` - Parses PSQL syntax into AST  
**Compiler**: `src/engine/sblr_compiler.cpp` - Compiles to SBLR bytecode  
**Runtime**: `src/engine/sblr_vm.cpp` - Executes bytecode  
**Bytecode**: See [Complete SBLR/BLR Specification](/workspace/docs/scratchbird-bytecode-complete-specification.md)  
**Dev Tools**: `src/engine/psql_dev_tools.cpp` - Development utilities

### Supported Statements

- **EXECUTE BLOCK**: Anonymous code blocks with optional parameters and returns
- **Control Flow**: IF/THEN/ELSE, WHILE, FOR SELECT, LEAVE, CONTINUE
- **Variables**: DECLARE with types and defaults, assignment, scope management
- **Cursors**: DECLARE, OPEN, FETCH, CLOSE with scrollable support
- **Exceptions**: EXCEPTION/WHEN blocks, system exceptions, custom exceptions
- **Dynamic SQL**: EXECUTE STATEMENT with various options
- **Security**: DEFINER/INVOKER contexts, caller privileges

### EXECUTE BLOCK

Anonymous procedural blocks for immediate execution:

```sql
-- Simple block with variables
EXECUTE BLOCK
AS
DECLARE i INTEGER = 0;
DECLARE total DECIMAL(10,2) = 0.00;
BEGIN
    WHILE (i < 10) DO
    BEGIN
        total = total + i * 1.5;
        i = i + 1;
    END
    
    -- Return result
    SUSPEND;
END
```

With parameters and returns:

```sql
EXECUTE BLOCK (min_salary DECIMAL(10,2) = 50000)
RETURNS (dept_name VARCHAR(50), avg_salary DECIMAL(10,2))
AS
BEGIN
    FOR SELECT d.name, AVG(e.salary)
        FROM departments d
        JOIN employees e ON e.dept_id = d.id
        WHERE e.salary >= :min_salary
        GROUP BY d.name
        INTO :dept_name, :avg_salary
    DO
        SUSPEND;
END
```

### Variables and Types

PSQL supports all SQL data types plus special procedural types:

```sql
EXECUTE BLOCK
AS
DECLARE i INTEGER = 0;                    -- Integer with default
DECLARE name VARCHAR(100);                -- String, NULL default
DECLARE price DECIMAL(10,2) = 99.99;     -- Decimal with precision
DECLARE created_at TIMESTAMP;             -- Timestamp
DECLARE is_active BOOLEAN = TRUE;         -- Boolean
DECLARE data BLOB SUB_TYPE TEXT;         -- BLOB with subtype
DECLARE items INTEGER ARRAY[10];         -- Array type
BEGIN
    -- Variable assignment
    name = 'Product';
    created_at = CURRENT_TIMESTAMP;
    
    -- Type coercion
    price = i * 1.5;  -- Integer to decimal
    
    -- Array access
    items[0] = 100;
    items[1] = 200;
END
```

### Control Flow

#### IF Statement

```sql
IF (condition) THEN
    statement;
ELSE IF (other_condition) THEN
    statement;
ELSE
    statement;
```

#### WHILE Loop

```sql
WHILE (i < 100) DO
BEGIN
    -- Loop body
    i = i + 1;
    
    IF (i MOD 10 = 0) THEN
        CONTINUE;  -- Skip to next iteration
        
    IF (i > 50) THEN
        LEAVE;     -- Exit loop
END
```

#### FOR SELECT Loop

```sql
FOR SELECT id, name, price
    FROM products
    WHERE category_id = :cat_id
    ORDER BY price DESC
    INTO :prod_id, :prod_name, :prod_price
DO
BEGIN
    -- Process each row
    total = total + prod_price;
    
    -- Can use SUSPEND to return rows
    IF (prod_price > 100) THEN
        SUSPEND;
END
```

### Cursors

Explicit cursor management for complex row processing:

```sql
EXECUTE BLOCK
RETURNS (id INTEGER, name VARCHAR(100))
AS
DECLARE cur CURSOR FOR (
    SELECT id, name FROM customers
    WHERE status = 'ACTIVE'
    ORDER BY created_at
);
DECLARE cur_id INTEGER;
DECLARE cur_name VARCHAR(100);
BEGIN
    OPEN cur;
    
    FETCH cur INTO :cur_id, :cur_name;
    WHILE (ROW_COUNT > 0) DO
    BEGIN
        -- Process row
        id = cur_id;
        name = UPPER(cur_name);
        SUSPEND;
        
        FETCH cur INTO :cur_id, :cur_name;
    END
    
    CLOSE cur;
END
```

Scrollable cursors:

```sql
DECLARE cur SCROLL CURSOR FOR (SELECT * FROM large_table);
BEGIN
    OPEN cur;
    
    -- Position cursor
    FETCH FIRST FROM cur INTO ...;
    FETCH LAST FROM cur INTO ...;
    FETCH ABSOLUTE 100 FROM cur INTO ...;
    FETCH RELATIVE -5 FROM cur INTO ...;
    FETCH PRIOR FROM cur INTO ...;
    
    CLOSE cur;
END
```

### Exception Handling

Robust error management with system and custom exceptions:

```sql
EXECUTE BLOCK
AS
DECLARE custom_error EXCEPTION 'Custom error occurred';
BEGIN
    -- Protected code
    INSERT INTO accounts (id, balance)
    VALUES (1, 100.00);
    
    IF (some_condition) THEN
        EXCEPTION custom_error;
        
    -- May cause division by zero
    result = value / divisor;
    
    WHEN SQLCODE -803 DO  -- Duplicate key
    BEGIN
        -- Handle duplicate
        UPDATE accounts SET balance = balance + 100
        WHERE id = 1;
    END
    
    WHEN GDSCODE unique_key_violation DO
    BEGIN
        -- Alternative handling
        LOG_ERROR('Duplicate key on insert');
    END
    
    WHEN custom_error DO
    BEGIN
        -- Handle custom exception
        EXECUTE PROCEDURE log_custom_error();
    END
    
    WHEN ANY DO  -- Catch all
    BEGIN
        -- Log unknown error
        IN AUTONOMOUS TRANSACTION DO
            INSERT INTO error_log (error_code, error_msg, occurred_at)
            VALUES (SQLCODE, RDB$ERROR_MESSAGE, CURRENT_TIMESTAMP);
            
        -- Re-raise the exception
        EXCEPTION;
    END
END
```

### Dynamic SQL

Execute dynamically constructed SQL statements:

```sql
EXECUTE BLOCK (table_name VARCHAR(31) = 'products')
RETURNS (row_count INTEGER)
AS
DECLARE sql_stmt VARCHAR(1000);
BEGIN
    -- Build dynamic SQL
    sql_stmt = 'SELECT COUNT(*) FROM ' || table_name;
    
    -- Execute and get result
    EXECUTE STATEMENT sql_stmt INTO :row_count;
    
    -- With parameters
    sql_stmt = 'UPDATE ' || table_name || ' SET status = ? WHERE id = ?';
    EXECUTE STATEMENT sql_stmt ('ACTIVE', 123);
    
    -- With named parameters
    EXECUTE STATEMENT 'INSERT INTO logs (msg, created_by) VALUES (:msg, :user)'
        (msg := 'Action performed', user := CURRENT_USER);
    
    -- On external database
    EXECUTE STATEMENT sql_stmt
        ON EXTERNAL 'server:/path/to/database.fdb'
        AS USER 'remote_user' PASSWORD 'secret';
    
    -- With transaction control
    EXECUTE STATEMENT sql_stmt
        WITH AUTONOMOUS TRANSACTION;
        
    SUSPEND;
END
```

### Security Contexts

Control execution privileges:

```sql
-- Execute with definer's rights (default)
CREATE PROCEDURE secure_proc
AS
BEGIN
    -- Runs with procedure owner's privileges
    DELETE FROM sensitive_table WHERE expired = TRUE;
END;

-- Execute with caller's rights
CREATE PROCEDURE flexible_proc
SQL SECURITY INVOKER
AS
BEGIN
    -- Runs with calling user's privileges
    SELECT * FROM user_visible_table;
END;

-- Dynamic SQL with caller privileges
EXECUTE STATEMENT 'DELETE FROM table WHERE id = ?'
    WITH CALLER PRIVILEGES;
```

### Performance Features

PSQL procedures are compiled to SBLR bytecode with several optimization levels:

- **Bytecode Compilation**: PSQL → AST → SBLR bytecode
- **Adaptive Optimization**: Hot paths are specialized based on runtime types
- **JIT Compilation**: Frequently executed code compiled to native machine code
- **Inline Caching**: Method and field lookups cached for performance
- **Loop Optimization**: Loop unrolling and invariant hoisting

### Examples

#### Recursive CTE in PSQL

```sql
EXECUTE BLOCK
RETURNS (level INTEGER, name VARCHAR(100))
AS
BEGIN
    FOR WITH RECURSIVE tree AS (
            SELECT 1 as level, name, id
            FROM categories
            WHERE parent_id IS NULL
            
            UNION ALL
            
            SELECT t.level + 1, c.name, c.id
            FROM categories c
            JOIN tree t ON c.parent_id = t.id
        )
        SELECT level, name FROM tree
        INTO :level, :name
    DO
        SUSPEND;
END
```

#### Batch Processing with Error Recovery

```sql
EXECUTE BLOCK
AS
DECLARE batch_id INTEGER;
DECLARE processed INTEGER = 0;
DECLARE failed INTEGER = 0;
BEGIN
    FOR SELECT id FROM pending_orders
        INTO :batch_id
    DO
    BEGIN
        BEGIN
            EXECUTE PROCEDURE process_order(:batch_id);
            processed = processed + 1;
            
            WHEN ANY DO
            BEGIN
                failed = failed + 1;
                INSERT INTO failed_orders (order_id, error_msg)
                VALUES (:batch_id, RDB$ERROR_MESSAGE);
            END
        END
    END
    
    -- Log results
    INSERT INTO batch_log (processed, failed, run_date)
    VALUES (:processed, :failed, CURRENT_TIMESTAMP);
END
```

## Implementation Details

**Code Anchors**:
- Parser: `src/engine/parser_psql.cpp::parse_psql_block()`
- Compiler: `src/engine/sblr_compiler.cpp::compile_psql()`
- VM: `src/engine/sblr_vm.cpp::execute_bytecode()`
- Types: `include/scratchbird/engine/psql_types.h`
- Exceptions: `include/scratchbird/engine/psql_exceptions.h`

## See Also

- [Routines & Triggers](./psql-routines-and-triggers.md) - Stored procedures and functions
- [Session & Transaction](./session-and-transaction.md) - Transaction control
- [SQL DML](./sql-dml.md) - Data manipulation statements
- [Complete SBLR/BLR Specification](/workspace/docs/scratchbird-bytecode-complete-specification.md) - Bytecode details