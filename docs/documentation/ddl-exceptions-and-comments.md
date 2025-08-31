### DDL: Exceptions and Comments

**What it is**

Exceptions are user-defined error conditions that can be raised in PSQL code, while comments provide documentation directly in the database schema. Exceptions standardize error handling across procedures and triggers, and comments make database objects self-documenting for developers and administrators.

**Why it matters**

- **Error Standardization**: Consistent error handling across applications
- **Business Rules**: Encode domain-specific error conditions
- **Documentation**: Built-in schema documentation accessible via system catalogs
- **Maintenance**: Comments help future developers understand design decisions
- **Debugging**: Named exceptions provide clear error context

**How to use it**

Create exceptions for domain-specific error conditions that procedures and triggers can raise. Add comments to tables, columns, and other objects to document their purpose, constraints, and relationships.

## Exceptions

### CREATE EXCEPTION

```sql
-- Basic exception
CREATE EXCEPTION exception_name 'Error message';

-- Examples
CREATE EXCEPTION insufficient_funds 'Account balance insufficient for transaction';
CREATE EXCEPTION invalid_status 'Invalid status transition';
CREATE EXCEPTION duplicate_entry 'Record already exists';
CREATE EXCEPTION data_validation_failed 'Data validation failed';
CREATE EXCEPTION unauthorized_access 'User not authorized for this operation';
```

### ALTER EXCEPTION

```sql
-- Change exception message
ALTER EXCEPTION insufficient_funds 'Insufficient balance for withdrawal';

-- Update to more detailed message
ALTER EXCEPTION data_validation_failed 
    'Data validation failed: Check field constraints';
```

### DROP EXCEPTION

```sql
-- Drop exception
DROP EXCEPTION old_exception;

-- Drop if exists
DROP EXCEPTION IF EXISTS temporary_exception;
```

### Using Exceptions in PSQL

```sql
-- In stored procedure
CREATE PROCEDURE withdraw_funds(
    account_id INTEGER,
    amount DECIMAL(10,2)
)
AS
DECLARE VARIABLE balance DECIMAL(10,2);
BEGIN
    SELECT current_balance FROM accounts 
    WHERE id = :account_id 
    INTO :balance;
    
    IF (balance < amount) THEN
        EXCEPTION insufficient_funds;
    
    UPDATE accounts 
    SET current_balance = current_balance - :amount
    WHERE id = :account_id;
END;

-- In trigger
CREATE TRIGGER validate_order
BEFORE INSERT OR UPDATE ON orders
FOR EACH ROW
AS
BEGIN
    IF (NEW.quantity <= 0) THEN
        EXCEPTION data_validation_failed;
    
    IF (NEW.status = 'shipped' AND OLD.status = 'cancelled') THEN
        EXCEPTION invalid_status;
END;
```

### Exception Handling

```sql
EXECUTE BLOCK
AS
BEGIN
    EXECUTE PROCEDURE withdraw_funds(123, 1000.00);
    
    WHEN insufficient_funds DO
    BEGIN
        INSERT INTO failed_transactions (
            account_id, amount, reason, attempted_at
        ) VALUES (
            123, 1000.00, 'Insufficient funds', CURRENT_TIMESTAMP
        );
    END
    
    WHEN ANY DO
    BEGIN
        -- Handle any other exception
        INSERT INTO error_log (error_code, error_time)
        VALUES (SQLCODE, CURRENT_TIMESTAMP);
    END
END;
```

## Comments

### COMMENT ON TABLE

```sql
-- Add table comment
COMMENT ON TABLE customers IS 
    'Customer master table containing all customer information';

COMMENT ON TABLE orders IS 
    'Order transactions with foreign key to customers table';

COMMENT ON TABLE audit_log IS 
    'Audit trail for all database modifications - DO NOT DELETE';

-- Remove comment
COMMENT ON TABLE temp_table IS NULL;
```

### COMMENT ON COLUMN

```sql
-- Column comments
COMMENT ON COLUMN customers.id IS 
    'Primary key, auto-generated';

COMMENT ON COLUMN customers.email IS 
    'Unique email address, used for login';

COMMENT ON COLUMN customers.created_at IS 
    'Timestamp when customer record was created';

COMMENT ON COLUMN orders.status IS 
    'Order status: pending, processing, shipped, delivered, cancelled';

COMMENT ON COLUMN products.price IS 
    'Current price in USD, must be greater than 0';
```

### COMMENT ON Other Objects

```sql
-- Index comments
COMMENT ON INDEX idx_customers_email IS 
    'Unique index for email-based lookups and login';

-- View comments
COMMENT ON VIEW active_customers IS 
    'Filtered view showing only active customers with recent orders';

-- Sequence comments
COMMENT ON SEQUENCE order_id_seq IS 
    'Generates unique order IDs, starts at 10000';

-- Function comments
COMMENT ON FUNCTION calculate_tax IS 
    'Calculates tax based on customer location and product category';

-- Procedure comments
COMMENT ON PROCEDURE process_payment IS 
    'Processes payment and updates order status - called by payment gateway';

-- Trigger comments
COMMENT ON TRIGGER audit_trigger IS 
    'Maintains audit trail for compliance - CRITICAL: Do not disable';

-- Schema comments
COMMENT ON SCHEMA analytics IS 
    'Data warehouse schema for business intelligence reports';

-- Database comments
COMMENT ON DATABASE production IS 
    'Production database - Version 2.5 - Last migration: 2024-01-15';
```

### Complex Documentation Examples

```sql
-- Comprehensive table documentation
COMMENT ON TABLE user_sessions IS 
'Active user sessions for web application.
Cleaned up by scheduled job every hour.
Foreign keys: user_id -> users.id
Indexes: user_id, token (unique), expires_at
Retention: 30 days for expired sessions';

-- Detailed column documentation
COMMENT ON COLUMN financial_transactions.amount IS 
'Transaction amount in cents (multiply by 100 for dollar amount).
Positive for credits, negative for debits.
Maximum: 999999999 ($9,999,999.99)
Precision: 2 decimal places when divided by 100';

-- Relationship documentation
COMMENT ON TABLE order_items IS 
'Line items for orders - many-to-one with orders table.
Constraints:
  - quantity must be > 0
  - price must match products.price at order time
  - product_id must be active at order time
Business rules:
  - Cannot modify after order is shipped
  - Deleted when parent order is cancelled';
```

## Best Practices

### Exception Guidelines

1. **Meaningful Names**: Use descriptive exception names
2. **Clear Messages**: Provide helpful error messages
3. **Consistent Usage**: Use same exception for same condition
4. **Documentation**: Document when exceptions are raised
5. **Granularity**: Balance between too many and too few

```sql
-- Good: Specific exceptions
CREATE EXCEPTION account_locked 'Account locked due to failed login attempts';
CREATE EXCEPTION password_expired 'Password has expired, please reset';
CREATE EXCEPTION session_timeout 'Session timed out after inactivity';

-- Bad: Too generic
CREATE EXCEPTION error1 'Error';
CREATE EXCEPTION problem 'Something went wrong';
```

### Comment Standards

1. **Consistency**: Use consistent format and style
2. **Completeness**: Document purpose, not just restate name
3. **Maintenance**: Update comments when schema changes
4. **Business Rules**: Include business logic and constraints
5. **Warnings**: Note critical dependencies or gotchas

```sql
-- Good: Informative comments
COMMENT ON COLUMN users.status IS 
'User account status:
- active: Can log in and use system
- suspended: Temporarily disabled, can be reactivated
- deleted: Soft delete, retained for audit trail
Default: active on creation
Transitions: active <-> suspended, any -> deleted (one-way)';

-- Bad: Redundant comment
COMMENT ON COLUMN users.email IS 'Email';  -- Adds no value
```

## Metadata Queries

### View Comments

```sql
-- View table comments
SELECT 
    schemaname,
    tablename,
    obj_description(c.oid) AS comment
FROM pg_tables t
JOIN pg_class c ON c.relname = t.tablename
WHERE schemaname = 'public'
  AND obj_description(c.oid) IS NOT NULL;

-- View column comments
SELECT 
    table_name,
    column_name,
    col_description(pgc.oid, a.attnum) AS comment
FROM pg_catalog.pg_attribute a
JOIN pg_catalog.pg_class pgc ON a.attrelid = pgc.oid
JOIN information_schema.columns c 
    ON c.column_name = a.attname 
    AND c.table_name = pgc.relname
WHERE c.table_schema = 'public'
  AND col_description(pgc.oid, a.attnum) IS NOT NULL;
```

### View Exceptions

```sql
-- List all exceptions (system-specific query)
SELECT 
    exception_name,
    exception_message
FROM system_exceptions
ORDER BY exception_name;
```

## Implementation Details

**Parser** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_exception`: Handles CREATE/ALTER EXCEPTION
- `parse_ddl_comment`: Handles COMMENT ON statements

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlExceptionAst {
    std::string name;
    std::string message;
    std::string action;  // CREATE|ALTER|DROP
};

struct DdlCommentAst {
    std::string object_type;  // TABLE|COLUMN|INDEX|etc
    std::string object_name;
    std::string comment_text;
};
```

**Code Anchors**:
- Exception parser: `src/engine/parser_ddl.cpp` (parse_ddl_exception)
- Comment parser: `src/engine/parser_ddl.cpp` (parse_ddl_comment)
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [PSQL Runtime](./psql-runtime.md) - Using exceptions in PSQL code
- [Routines & Triggers](./psql-routines-and-triggers.md) - Raising exceptions
- [Tables](./ddl-tables.md) - Commenting on table objects
- [Session & Transaction](./session-and-transaction.md) - Exception handling in transactions