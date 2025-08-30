### PSQL Routines, Packages, and Triggers

**What it is**

Stored routines (procedures and functions), packages, and triggers provide reusable code modules and event-driven logic within the database. Functions return values for computations, procedures perform actions, packages group related routines, and triggers automatically execute on data changes.

**Why it matters**

- **Code Reuse**: Write once, use many times
- **Performance**: Execute complex logic server-side
- **Encapsulation**: Hide implementation details
- **Event-Driven**: Automatic response to data changes
- **Security**: Control access through routine permissions

**How to use it**

Create functions for calculations that return values. Use procedures for operations with side effects. Group related routines in packages. Implement triggers for automatic auditing, validation, or cascading changes.

## Stored Functions

### CREATE FUNCTION Syntax
```sql
CREATE [OR ALTER] FUNCTION function_name 
    [(parameter_list)]
    RETURNS return_type
    [DETERMINISTIC | NOT DETERMINISTIC]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE declarations]
BEGIN
    statements;
    RETURN value;
END
```

### Function Examples
```sql
-- Simple function
CREATE FUNCTION add_numbers(a INTEGER, b INTEGER)
RETURNS INTEGER
AS
BEGIN
    RETURN a + b;
END;

-- Function with local variables
CREATE FUNCTION calculate_discount(
    price DECIMAL(10,2),
    customer_level VARCHAR(20)
)
RETURNS DECIMAL(10,2)
AS
DECLARE VARIABLE discount_rate DECIMAL(5,2);
BEGIN
    discount_rate = CASE customer_level
        WHEN 'VIP' THEN 20.0
        WHEN 'GOLD' THEN 15.0
        WHEN 'SILVER' THEN 10.0
        ELSE 5.0
    END;
    
    RETURN price * (1 - discount_rate / 100);
END;

-- Deterministic function (same input = same output)
CREATE FUNCTION format_phone(phone VARCHAR(20))
RETURNS VARCHAR(20)
DETERMINISTIC
AS
BEGIN
    -- Remove all non-digits
    phone = REGEXP_REPLACE(phone, '[^0-9]', '', 'g');
    
    IF (CHAR_LENGTH(phone) = 10) THEN
        RETURN '(' || SUBSTRING(phone FROM 1 FOR 3) || ') ' ||
               SUBSTRING(phone FROM 4 FOR 3) || '-' ||
               SUBSTRING(phone FROM 7 FOR 4);
    ELSE
        RETURN phone;
    END IF;
END;
```

## Stored Procedures

### CREATE PROCEDURE Syntax
```sql
CREATE [OR ALTER] PROCEDURE procedure_name
    [(parameter_list)]
    [RETURNS (output_parameters)]
    [SQL SECURITY {DEFINER | INVOKER}]
AS
[DECLARE declarations]
BEGIN
    statements;
END
```

### Procedure Examples
```sql
-- Simple procedure
CREATE PROCEDURE update_inventory(
    IN product_id INTEGER,
    IN quantity_change INTEGER
)
AS
BEGIN
    UPDATE products
    SET stock_quantity = stock_quantity + quantity_change,
        last_updated = CURRENT_TIMESTAMP
    WHERE id = product_id;
    
    IF (ROW_COUNT = 0) THEN
        EXCEPTION product_not_found;
END;

-- Procedure with output parameters
CREATE PROCEDURE get_customer_stats(
    IN customer_id INTEGER,
    OUT order_count INTEGER,
    OUT total_spent DECIMAL(12,2),
    OUT last_order_date DATE
)
AS
BEGIN
    SELECT COUNT(*), SUM(total), MAX(order_date)
    FROM orders
    WHERE customer_id = :customer_id
    INTO :order_count, :total_spent, :last_order_date;
END;

-- Procedure returning result set
CREATE PROCEDURE get_active_orders()
RETURNS (
    order_id INTEGER,
    customer_name VARCHAR(100),
    total DECIMAL(10,2),
    status VARCHAR(20)
)
AS
BEGIN
    FOR SELECT o.id, c.name, o.total, o.status
        FROM orders o
        JOIN customers c ON o.customer_id = c.id
        WHERE o.status IN ('pending', 'processing')
        INTO :order_id, :customer_name, :total, :status
    DO
        SUSPEND;
END;
```

### CALL and EXECUTE PROCEDURE
```sql
-- Call procedure
CALL update_inventory(123, -5);

-- Execute procedure with output
EXECUTE PROCEDURE get_customer_stats(456, :count, :total, :last_date);

-- In PSQL block
EXECUTE BLOCK
AS
DECLARE VARIABLE cnt INTEGER;
DECLARE VARIABLE tot DECIMAL(12,2);
DECLARE VARIABLE dt DATE;
BEGIN
    EXECUTE PROCEDURE get_customer_stats(789, cnt, tot, dt);
    -- Use the output values
    IF (cnt > 10 AND tot > 1000) THEN
        UPDATE customers SET vip_status = TRUE WHERE id = 789;
END;
```

## Packages

### Package Structure
```sql
-- Package header (specification)
CREATE PACKAGE package_name
AS
BEGIN
    -- Public declarations
    PROCEDURE public_proc(param INTEGER);
    FUNCTION public_func(x INTEGER) RETURNS INTEGER;
END;

-- Package body (implementation)
CREATE PACKAGE BODY package_name
AS
    -- Private variables
    DECLARE VARIABLE private_counter INTEGER = 0;
    
    -- Private procedure
    PROCEDURE private_helper
    AS
    BEGIN
        private_counter = private_counter + 1;
    END;
    
    -- Public procedure implementation
    PROCEDURE public_proc(param INTEGER)
    AS
    BEGIN
        EXECUTE PROCEDURE private_helper;
        -- Implementation
    END;
    
    -- Public function implementation
    FUNCTION public_func(x INTEGER) RETURNS INTEGER
    AS
    BEGIN
        RETURN x * private_counter;
    END;
BEGIN
    -- Package initialization
    private_counter = 0;
END;
```

### Package Examples
```sql
-- Math utilities package
CREATE PACKAGE math_utils
AS
BEGIN
    FUNCTION factorial(n INTEGER) RETURNS BIGINT;
    FUNCTION gcd(a INTEGER, b INTEGER) RETURNS INTEGER;
    FUNCTION is_prime(n INTEGER) RETURNS BOOLEAN;
END;

CREATE PACKAGE BODY math_utils
AS
    FUNCTION factorial(n INTEGER) RETURNS BIGINT
    AS
    DECLARE VARIABLE result BIGINT = 1;
    BEGIN
        WHILE (n > 1) DO
        BEGIN
            result = result * n;
            n = n - 1;
        END
        RETURN result;
    END;
    
    FUNCTION gcd(a INTEGER, b INTEGER) RETURNS INTEGER
    AS
    BEGIN
        WHILE (b != 0) DO
        BEGIN
            DECLARE VARIABLE temp INTEGER;
            temp = b;
            b = a % b;
            a = temp;
        END
        RETURN a;
    END;
    
    FUNCTION is_prime(n INTEGER) RETURNS BOOLEAN
    AS
    DECLARE VARIABLE i INTEGER = 2;
    BEGIN
        IF (n <= 1) THEN RETURN FALSE;
        WHILE (i * i <= n) DO
        BEGIN
            IF (n % i = 0) THEN RETURN FALSE;
            i = i + 1;
        END
        RETURN TRUE;
    END;
END;

-- Using package members
SELECT math_utils.factorial(5);  -- Returns 120
SELECT math_utils.gcd(48, 18);   -- Returns 6
```

## Triggers

### CREATE TRIGGER Syntax
```sql
CREATE [OR ALTER] TRIGGER trigger_name
    {BEFORE | AFTER} {INSERT | UPDATE | DELETE}
    [OR {INSERT | UPDATE | DELETE}]...
    ON table_name
    [FOR EACH {ROW | STATEMENT}]
    [POSITION number]
    [ACTIVE | INACTIVE]
AS
[DECLARE declarations]
BEGIN
    statements;
END
```

### Trigger Variables
- **NEW**: New row values (INSERT, UPDATE)
- **OLD**: Old row values (UPDATE, DELETE)
- **INSERTING**: TRUE if INSERT operation
- **UPDATING**: TRUE if UPDATE operation
- **DELETING**: TRUE if DELETE operation

### Trigger Examples
```sql
-- Audit trigger
CREATE TRIGGER audit_customers
AFTER INSERT OR UPDATE OR DELETE ON customers
FOR EACH ROW
AS
BEGIN
    INSERT INTO audit_log (
        table_name,
        operation,
        user_name,
        timestamp,
        old_data,
        new_data
    ) VALUES (
        'customers',
        CASE
            WHEN INSERTING THEN 'INSERT'
            WHEN UPDATING THEN 'UPDATE'
            WHEN DELETING THEN 'DELETE'
        END,
        CURRENT_USER,
        CURRENT_TIMESTAMP,
        CASE WHEN DELETING OR UPDATING THEN OLD.id || ':' || OLD.name END,
        CASE WHEN INSERTING OR UPDATING THEN NEW.id || ':' || NEW.name END
    );
END;

-- Validation trigger
CREATE TRIGGER validate_order
BEFORE INSERT OR UPDATE ON orders
FOR EACH ROW
AS
BEGIN
    -- Check business rules
    IF (NEW.total < 0) THEN
        EXCEPTION invalid_order_total 'Order total cannot be negative';
    
    IF (NEW.order_date > CURRENT_DATE) THEN
        EXCEPTION future_order 'Cannot create orders in the future';
    
    -- Auto-set fields
    IF (INSERTING) THEN
        NEW.created_at = CURRENT_TIMESTAMP;
        NEW.created_by = CURRENT_USER;
    ELSE
        NEW.updated_at = CURRENT_TIMESTAMP;
        NEW.updated_by = CURRENT_USER;
END;

-- Cascade trigger
CREATE TRIGGER cascade_delete_orders
AFTER DELETE ON customers
FOR EACH ROW
AS
BEGIN
    DELETE FROM orders WHERE customer_id = OLD.id;
    DELETE FROM customer_addresses WHERE customer_id = OLD.id;
    DELETE FROM customer_contacts WHERE customer_id = OLD.id;
END;

-- Statement-level trigger
CREATE TRIGGER log_bulk_updates
AFTER UPDATE ON products
FOR EACH STATEMENT
AS
BEGIN
    INSERT INTO bulk_update_log (
        table_name,
        update_time,
        row_count
    ) VALUES (
        'products',
        CURRENT_TIMESTAMP,
        ROW_COUNT
    );
END;
```

### Trigger Control
```sql
-- Disable trigger
ALTER TRIGGER audit_customers INACTIVE;

-- Enable trigger
ALTER TRIGGER audit_customers ACTIVE;

-- Change trigger position (execution order)
ALTER TRIGGER validate_order POSITION 1;
ALTER TRIGGER audit_customers POSITION 2;

-- Drop trigger
DROP TRIGGER old_trigger;
```

## ALTER and RECREATE

### ALTER FUNCTION/PROCEDURE
```sql
-- Modify existing routine
ALTER FUNCTION calculate_tax
AS
BEGIN
    -- New implementation
    RETURN amount * 0.08;  -- Changed tax rate
END;

-- Change security
ALTER PROCEDURE sensitive_operation
    SQL SECURITY INVOKER;
```

### RECREATE
```sql
-- Drop and recreate in one statement
RECREATE FUNCTION get_status(id INTEGER)
RETURNS VARCHAR(20)
AS
BEGIN
    -- Completely new implementation
    RETURN 'active';
END;

RECREATE TRIGGER new_trigger
BEFORE INSERT ON table_name
AS
BEGIN
    -- New trigger logic
END;
```

## Parameter Modes

### IN Parameters (Default)
```sql
CREATE PROCEDURE process_data(
    IN input_value INTEGER,      -- Explicitly IN
    data VARCHAR(100)            -- Implicitly IN
)
```

### OUT Parameters
```sql
CREATE PROCEDURE get_stats(
    IN dept_id INTEGER,
    OUT emp_count INTEGER,
    OUT avg_salary DECIMAL(10,2)
)
```

### INOUT Parameters
```sql
CREATE PROCEDURE transform_value(
    INOUT value VARCHAR(100)
)
AS
BEGIN
    value = UPPER(TRIM(value));
END;
```

## Best Practices

### Function vs Procedure
- Use **functions** for calculations that return a value
- Use **procedures** for operations with side effects
- Functions can be used in SQL expressions
- Procedures must be called explicitly

### Trigger Guidelines
1. Keep triggers simple and fast
2. Avoid recursive trigger calls
3. Use BEFORE triggers for validation
4. Use AFTER triggers for logging/cascading
5. Document trigger dependencies

### Package Design
1. Group related functionality
2. Hide implementation details
3. Provide clear public interface
4. Initialize package state properly

## Implementation Details

**Parser** (`src/engine/parser_psql.cpp`):
- Parses CREATE/ALTER/RECREATE statements
- Handles parameter lists and modes
- Captures routine bodies

**AST Structures** (`include/scratchbird/engine/ast.h`):
```cpp
struct PsqlRoutineAst {
    std::string kind;  // PROCEDURE|FUNCTION
    std::string name;
    std::vector<Parameter> params;
    std::string returns;
    std::string body;
    bool deterministic;
    SecurityMode security;
};

struct PsqlTriggerAst {
    std::string name;
    std::string table;
    TriggerTiming timing;  // BEFORE|AFTER
    TriggerEvents events;  // INSERT|UPDATE|DELETE
    TriggerLevel level;    // ROW|STATEMENT
    bool active;
    int position;
    std::string body;
};
```

**Code Anchors**:
- Routine parser: `src/engine/parser_psql.cpp` (parse_psql_routine)
- Trigger parser: `src/engine/parser_psql.cpp` (parse_psql_trigger)
- Package parser: `src/engine/parser_psql.cpp` (parse_psql_package)
- Router: `src/engine/parser.cpp` (routes to PSQL parsers)

## See also

- [PSQL Runtime](./psql-runtime.md) - Procedural SQL execution
- [DML Operations](./sql-dml.md) - Triggers on DML statements
- [Exceptions](./ddl-exceptions-and-comments.md) - Custom exceptions in routines
- [Session & Transaction](./session-and-transaction.md) - Transaction context
- [Developer Tools](./dev-tools.md) - Routine debugging and profiling

