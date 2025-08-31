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
**Runtime**: `src/engine/psql_executor.cpp` - Executes PSQL code  
**Dev Tools**: `src/engine/psql_dev_tools.cpp` - Development utilities

### Supported Statements

- **EXECUTE BLOCK**: Anonymous code blocks with parameters and return values
- **Control Flow**: IF/THEN/ELSE, WHILE, FOR SELECT, LEAVE, CONTINUE
- **Variables**: DECLARE with types, assignments, scope management
- **Cursors**: Declare, open, fetch, close; scrollable cursor support
- **Exceptions**: System and custom exceptions, WHEN handlers
- **Dynamic SQL**: EXECUTE STATEMENT with various options
- **Security**: DEFINER/INVOKER rights, privilege control
- **Parameters**: IN/OUT/INOUT parameter passing

## EXECUTE BLOCK Examples

### Basic Block
```sql
-- Simple execution
EXECUTE BLOCK
AS
BEGIN
    UPDATE products SET price = price * 1.10;
END;
```

### With Parameters
```sql
EXECUTE BLOCK (input_val INTEGER = 10) 
RETURNS (output_val INTEGER) 
AS
BEGIN
    output_val = input_val * 2;
    SUSPEND;
END;
```

### Complex Example
```sql
EXECUTE BLOCK (category_id INTEGER = 5)
RETURNS (product_name VARCHAR(100), new_price DECIMAL(10,2))
AS
DECLARE VARIABLE old_price DECIMAL(10,2);
BEGIN
    FOR SELECT name, price
        FROM products
        WHERE category_id = :category_id
        INTO :product_name, :old_price
    DO
    BEGIN
        new_price = old_price * 0.9;  -- 10% discount
        UPDATE products SET price = :new_price
        WHERE name = :product_name;
        SUSPEND;  -- Return row
    END
END;
```

## Variables and Data Types

### Declaration Syntax
```sql
DECLARE [VARIABLE] var_name type [= initial_value];
DECLARE var_name TYPE OF COLUMN table.column;
DECLARE var_name TYPE OF domain_name;
```

### Examples
```sql
EXECUTE BLOCK
AS
DECLARE VARIABLE counter INTEGER = 0;
DECLARE VARIABLE total DECIMAL(10,2);
DECLARE VARIABLE status VARCHAR(20) = 'pending';
DECLARE VARIABLE customer_id TYPE OF COLUMN customers.id;
BEGIN
    -- Variable usage
    counter = counter + 1;
    
    SELECT SUM(amount) FROM orders INTO :total;
    
    IF (total > 1000) THEN
        status = 'premium';
END;
```

## Control Flow

### IF/THEN/ELSE
```sql
IF (condition) THEN
    statement;
ELSE IF (condition) THEN
    statement;
ELSE
    statement;
```

### WHILE Loop
```sql
WHILE (condition) DO
BEGIN
    statements;
    IF (exit_condition) THEN
        LEAVE;  -- Exit loop
END
```

### FOR SELECT Loop
```sql
FOR SELECT columns
    FROM table
    WHERE condition
    INTO :variables
DO
BEGIN
    -- Process each row
    SUSPEND;  -- Return row if RETURNS clause present
END
```

## Cursors

### Cursor Operations
```sql
DECLARE cursor_name CURSOR FOR (SELECT ...);

OPEN cursor_name;

FETCH cursor_name INTO :variables;
FETCH NEXT FROM cursor_name INTO :variables;
FETCH PRIOR FROM cursor_name INTO :variables;

CLOSE cursor_name;
```

### Cursor Example
```sql
EXECUTE BLOCK
RETURNS (id INTEGER, name VARCHAR(100))
AS
DECLARE cust_cursor CURSOR FOR (
    SELECT id, name FROM customers
);
BEGIN
    OPEN cust_cursor;
    
    WHILE (1 = 1) DO
    BEGIN
        FETCH cust_cursor INTO :id, :name;
        IF (ROW_COUNT = 0) THEN LEAVE;
        SUSPEND;
    END
    
    CLOSE cust_cursor;
END;
```

## Exception Handling

### System Exceptions
- `ZERO_DIVIDE` - Division by zero
- `NUMERIC_OVERFLOW` - Numeric overflow
- `STRING_TRUNCATION` - String too long
- `FOREIGN_KEY_VIOLATION` - FK constraint violation
- `UNIQUE_VIOLATION` - Unique constraint violation
- `CHECK_VIOLATION` - Check constraint violation

### Exception Syntax
```sql
BEGIN
    -- Code that might fail
    
    WHEN exception_name DO
    BEGIN
        -- Handle specific exception
    END
    
    WHEN ANY DO
    BEGIN
        -- Handle any exception
        -- Use SQLCODE and GDSCODE for details
    END
END
```

### Exception Example
```sql
EXECUTE BLOCK (divisor INTEGER)
RETURNS (result DOUBLE PRECISION)
AS
BEGIN
    result = 100.0 / divisor;
    SUSPEND;
    
    WHEN ZERO_DIVIDE DO
    BEGIN
        result = 0;
        SUSPEND;
    END
END;
```

## Dynamic SQL (EXECUTE STATEMENT)

### Basic Syntax
```sql
EXECUTE STATEMENT sql_string
    [INTO :variables]
    [WITH options];
```

### Options
- `WITH CALLER PRIVILEGES` - Use caller's privileges
- `AS USER 'username' PASSWORD 'password'` - Specific credentials
- `ON EXTERNAL DATA SOURCE 'name'` - External database
- `WITH BIND (parameters)` - Bind parameters
- `WITH TIMEOUT seconds` - Execution timeout

### Dynamic SQL Example
```sql
EXECUTE BLOCK (table_name VARCHAR(50))
RETURNS (row_count INTEGER)
AS
DECLARE VARIABLE sql_text VARCHAR(500);
BEGIN
    sql_text = 'SELECT COUNT(*) FROM ' || table_name;
    
    EXECUTE STATEMENT sql_text
        WITH CALLER PRIVILEGES
        INTO :row_count;
    
    SUSPEND;
END;
```

## SUSPEND and RETURN

- **SUSPEND**: Returns current row and continues execution
- **RETURN**: Returns value and exits (functions only)

```sql
-- SUSPEND in EXECUTE BLOCK
EXECUTE BLOCK
RETURNS (n INTEGER, square INTEGER)
AS
BEGIN
    n = 1;
    WHILE (n <= 5) DO
    BEGIN
        square = n * n;
        SUSPEND;  -- Return row and continue
        n = n + 1;
    END
END;
-- Returns 5 rows
```

## Security Context

### DEFINER vs INVOKER
- **DEFINER** (default): Runs with creator's privileges
- **INVOKER**: Runs with caller's privileges

```sql
-- In EXECUTE BLOCK
EXECUTE STATEMENT 'DELETE FROM sensitive_table'
    WITH CALLER PRIVILEGES;
```

## Development Tools

### Available Utilities
- **Dependency Analyzer**: Find object dependencies
- **Code Formatter**: Format PSQL code
- **Performance Profiler**: Profile execution
- **Syntax Validator**: Validate without executing

```sql
-- Analyze dependencies
SELECT * FROM analyze_psql_dependencies('procedure_name');

-- Format code
SELECT format_psql_code('EXECUTE BLOCK AS BEGIN IF(x>0)THEN y=1;END');

-- Validate syntax
SELECT validate_psql_syntax('EXECUTE BLOCK AS BEGIN INVALID END');
```

## Best Practices

1. **Always handle exceptions** in production code
2. **Use parameters** instead of string concatenation for dynamic SQL
3. **Close cursors** explicitly to free resources
4. **Use SUSPEND** for multi-row results
5. **Declare variables** at the beginning of blocks
6. **Use meaningful variable names** for maintainability

## Implementation Details

**Code Anchors**:
- Parser: `src/engine/parser_psql.cpp` (parse_psql_block)
- Executor: `src/engine/psql_executor.cpp` (execute_psql_block)
- Dev Tools: `src/engine/psql_dev_tools.cpp`
- Exception Map: `src/engine/psql_executor.cpp` (system_exception_map)

## See also

- [Routines & Triggers](./psql-routines-and-triggers.md) - Stored procedures and functions
- [Session & Transaction](./session-and-transaction.md) - Transaction control
- [Exceptions & Comments](./ddl-exceptions-and-comments.md) - Custom exceptions
- [DML Operations](./sql-dml.md) - Data manipulation in PSQL
- [Developer Tools](./dev-tools.md) - PSQL development utilities

