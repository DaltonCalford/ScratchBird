### Session and Transaction Statements

**What it is**

Session and transaction statements control database connections, transaction boundaries, and session-level settings. These statements manage database lifecycle, connection parameters, isolation levels, and system behavior. They form the foundation for multi-user concurrency, data consistency, and performance tuning.

**Why it matters**

- **Data Integrity**: Transaction boundaries ensure ACID properties
- **Concurrency**: Isolation levels prevent data anomalies
- **Performance**: Session settings optimize query execution
- **Security**: Role and authentication management
- **Diagnostics**: EXPLAIN/ANALYZE for query optimization

**How to use it**

Manage database lifecycle with CREATE/ALTER/DROP DATABASE. Control transactions with BEGIN/COMMIT/ROLLBACK. Set isolation levels before critical operations. Use EXPLAIN/ANALYZE to understand and optimize query performance.

## Supported Statements

**Parser**: `src/engine/parser_session.cpp`  
**AST**: `include/scratchbird/engine/ast.h` (SessionStmtAst)

### Database Management
- CREATE DATABASE with options
- ALTER DATABASE settings
- DROP DATABASE

### Connection Control
- CONNECT to database
- DISCONNECT from database

### Session Settings
- SET NAMES (character set)
- SET ROLE (active role)
- SET SQL DIALECT
- SET TIME ZONE
- SET SEARCH PATH
- Various SET options

### Transaction Control
- SET TRANSACTION
- BEGIN/START TRANSACTION
- COMMIT/ROLLBACK
- SAVEPOINT management

### Maintenance Operations
- EXPLAIN [ANALYZE]
- ANALYZE tables
- VACUUM tables
- SET CONSTRAINTS

## Database Management

### CREATE DATABASE
```sql
-- Basic creation
CREATE DATABASE 'mydb.fdb';

-- With options
CREATE DATABASE '/path/to/database.fdb'
    PAGE_SIZE 16384
    DEFAULT CHARACTER SET UTF8
    DIALECT 3;

-- Full options
CREATE DATABASE 'production.fdb'
    PAGE_SIZE 8192
    DEFAULT CHARACTER SET UTF8
    PAGE_CACHE 1000
    SWEEP_INTERVAL 20000
    RESERVE_SPACE 0.1
    FILE 'secondary.fdb' STARTING AT PAGE 10000
    SHADOW 1 'shadow1.fdb';
```

### ALTER DATABASE
```sql
-- Change settings
ALTER DATABASE SET DEFAULT CHARACTER SET UTF8;
ALTER DATABASE SET PAGE_CACHE 2000;
ALTER DATABASE SET SWEEP_INTERVAL 0;  -- Disable auto-sweep

-- Add files
ALTER DATABASE ADD FILE 'extension.fdb' STARTING AT PAGE 50000;
```

### DROP DATABASE
```sql
DROP DATABASE;  -- Must be connected to the database
```

## Connection Management

### CONNECT
```sql
-- Local connection
CONNECT 'database.fdb' USER 'sysdba' PASSWORD 'masterkey';

-- Remote connection
CONNECT 'server:/path/to/database.fdb' 
    USER 'username' 
    PASSWORD 'password'
    ROLE 'admin_role';

-- With character set
CONNECT 'database.fdb' 
    USER 'user' 
    PASSWORD 'pass'
    CHARACTER SET UTF8;
```

### DISCONNECT
```sql
DISCONNECT;  -- Disconnect current
DISCONNECT ALL;  -- Disconnect all connections
```

## Transaction Control

### SET TRANSACTION
```sql
-- Isolation levels
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;

-- Access modes
SET TRANSACTION READ ONLY;
SET TRANSACTION READ WRITE;

-- Combined settings
SET TRANSACTION 
    ISOLATION LEVEL READ COMMITTED
    READ WRITE
    WAIT;  -- Wait for locks

-- No wait for locks
SET TRANSACTION
    ISOLATION LEVEL SERIALIZABLE
    READ WRITE
    NO WAIT;

-- Table reservation
SET TRANSACTION
    READ COMMITTED
    RESERVING customers FOR PROTECTED WRITE,
              orders FOR SHARED READ;
```

### BEGIN/START TRANSACTION
```sql
-- Start transaction
BEGIN;
BEGIN TRANSACTION;
BEGIN WORK;
START TRANSACTION;

-- With options
START TRANSACTION
    ISOLATION LEVEL REPEATABLE READ
    READ WRITE;
```

### COMMIT and ROLLBACK
```sql
-- Commit transaction
COMMIT;
COMMIT WORK;
COMMIT RETAIN;  -- Commit but keep transaction context

-- Rollback transaction
ROLLBACK;
ROLLBACK WORK;
ROLLBACK RETAIN;  -- Rollback but keep transaction context
```

### SAVEPOINT
```sql
-- Create savepoint
SAVEPOINT sp1;

-- Nested savepoints
SAVEPOINT sp_outer;
    UPDATE accounts SET balance = balance - 100;
    SAVEPOINT sp_inner;
        UPDATE accounts SET balance = balance + 100;
    ROLLBACK TO SAVEPOINT sp_inner;
-- Can still commit sp_outer changes

-- Release savepoint
RELEASE SAVEPOINT sp1;

-- Rollback to savepoint
ROLLBACK TO SAVEPOINT sp1;
```

## Session Settings

### SET Statements
```sql
-- Character set
SET NAMES UTF8;
SET NAMES 'ISO8859_1';

-- Active role
SET ROLE admin_role;
SET ROLE DEFAULT;

-- SQL dialect
SET SQL DIALECT 1;  -- Legacy
SET SQL DIALECT 3;  -- Current

-- Time zone
SET TIME ZONE 'America/New_York';
SET TIME ZONE '+05:30';
SET TIME ZONE LOCAL;

-- Search path for schemas
SET SEARCH PATH TO schema1, schema2, public;

-- Lock timeout
SET LOCK TIMEOUT 5;  -- 5 seconds
SET LOCK TIMEOUT 0;  -- No wait
SET LOCK TIMEOUT -1; -- Infinite wait

-- Statement timeout
SET STATEMENT_TIMEOUT 30000;  -- 30 seconds in milliseconds

-- Optimization level
SET OPTIMIZE FOR FIRST ROWS;
SET OPTIMIZE FOR ALL ROWS;

-- Debug options
SET DEBUG OPTION 'trace_statements';
SET DEBUG OPTION 'log_connections';

-- Decimal float settings
SET DECFLOAT ROUND HALF_UP;
SET DECFLOAT TRAPS TO Division_by_zero, Invalid_operation;

-- Statistics collection
SET STATISTICS ON;
SET PLAN ON;
SET TIMING ON;

-- Reset session
SESSION RESET;  -- Reset all session settings
```

## EXPLAIN and ANALYZE

### EXPLAIN
```sql
-- Basic explain
EXPLAIN SELECT * FROM customers WHERE city = 'New York';

-- Explain with subquery
EXPLAIN 
SELECT c.name, COUNT(o.id)
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.name;

-- Output shows query plan
```

### EXPLAIN ANALYZE
```sql
-- Explain with execution statistics
EXPLAIN ANALYZE SELECT * FROM large_table WHERE status = 'active';

-- Shows:
-- - Actual row counts
-- - Actual time spent
-- - Buffer statistics
-- - I/O statistics

EXPLAIN (ANALYZE, BUFFERS) 
SELECT * FROM orders WHERE order_date >= '2024-01-01';

-- Verbose output
EXPLAIN (ANALYZE, VERBOSE, BUFFERS)
SELECT * FROM complex_view;
```

## Maintenance Operations

### ANALYZE
```sql
-- Analyze single table
ANALYZE customers;

-- Analyze specific columns
ANALYZE customers (id, email, created_at);

-- Verbose output
ANALYZE VERBOSE customers;

-- Analyze all tables in schema
ANALYZE;
```

### VACUUM
```sql
-- Basic vacuum
VACUUM customers;

-- Full vacuum (rebuilds table)
VACUUM FULL customers;

-- Verbose output
VACUUM VERBOSE customers;

-- Vacuum and analyze
VACUUM ANALYZE customers;

-- Vacuum specific columns
VACUUM customers (deleted_at);

-- Vacuum all tables
VACUUM;
```

### SET CONSTRAINTS
```sql
-- Defer constraint checking
SET CONSTRAINTS ALL DEFERRED;
SET CONSTRAINTS fk_customer, fk_product DEFERRED;

-- Immediate checking
SET CONSTRAINTS ALL IMMEDIATE;
SET CONSTRAINTS fk_customer IMMEDIATE;
```

## Transaction Isolation Examples

### READ UNCOMMITTED
```sql
SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED;
-- Can read uncommitted changes (dirty reads)
-- Lowest isolation, highest concurrency
```

### READ COMMITTED
```sql
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
-- Only reads committed data
-- Default level for many databases
-- Prevents dirty reads
```

### REPEATABLE READ
```sql
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
-- Same read returns same data
-- Prevents dirty reads and non-repeatable reads
-- May still have phantom reads
```

### SERIALIZABLE
```sql
SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;
-- Highest isolation level
-- Transactions execute as if serial
-- Prevents all anomalies
```

## Complex Transaction Examples

### Nested Transactions with Savepoints
```sql
BEGIN;
    UPDATE accounts SET balance = balance - 1000 WHERE id = 1;
    SAVEPOINT transfer_start;
    
    UPDATE accounts SET balance = balance + 1000 WHERE id = 2;
    -- Check business rule
    IF (SELECT balance FROM accounts WHERE id = 2) > 1000000 THEN
        ROLLBACK TO SAVEPOINT transfer_start;
        UPDATE accounts SET balance = balance + 1000 WHERE id = 3;
    END IF;
    
COMMIT;
```

### Optimistic Locking Pattern
```sql
BEGIN;
    SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
    
    SELECT version FROM documents WHERE id = 123 INTO @version;
    
    -- Process document...
    
    UPDATE documents 
    SET content = 'new content', version = version + 1
    WHERE id = 123 AND version = @version;
    
    IF ROW_COUNT = 0 THEN
        ROLLBACK;
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Concurrent modification';
    END IF;
    
COMMIT;
```

## Performance Tuning

### Using EXPLAIN ANALYZE
```sql
-- Identify slow queries
SET STATISTICS ON;
SET PLAN ON;
SET TIMING ON;

EXPLAIN ANALYZE
SELECT c.*, COUNT(o.id) as order_count
FROM customers c
LEFT JOIN orders o ON c.id = o.customer_id
GROUP BY c.id;

-- Check for:
-- - Table scans vs index scans
-- - Join methods (nested loop, hash, merge)
-- - Row count estimates vs actual
-- - Time spent in each operation
```

### Session Optimization
```sql
-- For OLTP workloads
SET OPTIMIZE FOR FIRST ROWS;
SET LOCK TIMEOUT 5;
SET STATEMENT_TIMEOUT 10000;

-- For OLAP/reporting
SET OPTIMIZE FOR ALL ROWS;
SET LOCK TIMEOUT -1;  -- Wait indefinitely
SET STATEMENT_TIMEOUT 0;  -- No timeout
```

## Implementation Details

**Parser** (`src/engine/parser_session.cpp`):
- Parses session and transaction statements
- Handles database options parsing
- Recognizes SET variants

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct SessionStmtAst {
    SessionKind kind;
    std::string database_name;
    DbOptionsRaw options;
    // Transaction settings
    IsolationLevel isolation;
    AccessMode access_mode;
    // SET options
    std::string setting_name;
    std::string setting_value;
};
```

**Code Anchors**:
- Session parser: `src/engine/parser_session.cpp` (parse_session_stmt)
- AST definitions: `include/scratchbird/engine/ast.h` (SessionStmtAst)
- Database options: Captures PAGE_SIZE, CHARACTER SET, etc.

## See also

- [DML Operations](./sql-dml.md) - Data modifications in transactions
- [PSQL Runtime](./psql-runtime.md) - Procedural code in transactions
- [Configuration](./configuration.md) - System-wide settings
- [Indexes](./ddl-indexes.md) - Index usage in query plans
- [Tables](./ddl-tables.md) - Constraint deferral
