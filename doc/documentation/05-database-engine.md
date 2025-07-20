# ScratchBird Database Engine 🟡

Understanding the ScratchBird database engine will help you build more efficient applications and troubleshoot performance issues. This guide explains the core concepts and architecture.

## 🏗️ Engine Architecture

### **Multi-Generational Architecture (MGA)**
ScratchBird uses a unique Multi-Generational Architecture that provides several advantages:

```
┌─────────────────────────────────────────────────┐
│                  Transaction 1                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │  Record v1  │  │  Record v2  │  │   ...   │  │
│  └─────────────┘  └─────────────┘  └─────────┘  │
└─────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────┐
│                  Transaction 2                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────┐  │
│  │  Record v1  │  │  Record v3  │  │   ...   │  │
│  └─────────────┘  └─────────────┘  └─────────┘  │
└─────────────────────────────────────────────────┘
```

**Benefits:**
- ✅ **No Read Locks**: Readers never block writers
- ✅ **MVCC**: Multiple versions for concurrent access
- ✅ **Consistent Reads**: Each transaction sees consistent data snapshot
- ✅ **No Deadlocks**: Between readers and writers

### **Transaction Management**

#### **ACID Properties**
- **Atomicity**: All operations in a transaction succeed or all fail
- **Consistency**: Database remains in valid state after transaction
- **Isolation**: Transactions don't interfere with each other  
- **Durability**: Committed changes survive system failures

#### **Isolation Levels**
```sql
-- Set transaction isolation level
SET TRANSACTION ISOLATION LEVEL {
    READ COMMITTED |          -- Default, sees committed changes
    SNAPSHOT |               -- Consistent snapshot of database
    READ COMMITTED READ ONLY | -- Read-only, sees committed changes
    SNAPSHOT READ ONLY       -- Read-only snapshot
};
```

**Example Usage:**
```sql
-- Start a snapshot transaction for reporting
SET TRANSACTION SNAPSHOT READ ONLY;
SELECT COUNT(*) FROM sales WHERE date >= '2025-01-01';
SELECT SUM(amount) FROM sales WHERE date >= '2025-01-01';
COMMIT;

-- Both queries see the same data snapshot
```

### **Storage Engine**

#### **Page Structure**
ScratchBird stores data in fixed-size pages (default 8KB):

```
┌─────────────────────────────────────────────────┐
│                   Page Header                    │
│  ┌─────────────────────────────────────────────┐ │
│  │ Page Type | Flags | Record Count | ...      │ │
│  └─────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────┤
│                  Record Slots                   │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐         │
│  │Record 1 │  │Record 2 │  │Record 3 │   ...   │
│  └─────────┘  └─────────┘  └─────────┘         │
├─────────────────────────────────────────────────┤
│                  Free Space                     │
└─────────────────────────────────────────────────┘
```

#### **Page Types**
- **Data Pages**: Store table records
- **Index Pages**: Store index entries
- **Pointer Pages**: Track page allocations
- **Header Pages**: Database metadata
- **Blob Pages**: Large object storage

### **Memory Management**

#### **Buffer Cache**
```sql
-- Configure cache size (in pages)
-- In firebird.conf or database parameters
DefaultDbCachePages = 2048    -- 16MB with 8KB pages
TempCacheLimit = 67108864     -- 64MB temp cache
```

#### **Cache Algorithm**
ScratchBird uses an enhanced LRU (Least Recently Used) algorithm:

1. **Hot Pages**: Frequently accessed pages stay in memory
2. **Warm Pages**: Moderately accessed pages
3. **Cold Pages**: Rarely accessed pages (candidates for eviction)

### **Lock Manager**

#### **Lock Types**
- **Table Locks**: Protect entire tables
- **Record Locks**: Protect individual records  
- **Metadata Locks**: Protect database structure
- **Transaction Locks**: Coordinate transaction states

#### **Lock Compatibility Matrix**
| Request → | Shared | Protected | Exclusive |
|-----------|--------|-----------|-----------|
| **Shared** | ✅ Yes | ✅ Yes | ❌ No |
| **Protected** | ✅ Yes | ❌ No | ❌ No |
| **Exclusive** | ❌ No | ❌ No | ❌ No |

## 🚀 Performance Features

### **Query Optimizer**

#### **Cost-Based Optimization**
The optimizer chooses execution plans based on:
- **Table Statistics**: Row counts, data distribution
- **Index Statistics**: Selectivity, clustering
- **Join Order**: Optimal sequence for multi-table queries
- **Access Methods**: Index vs. table scan costs

#### **Plan Analysis**
```sql
-- See query execution plan
SET PLAN ON;
SELECT c.name, o.total 
FROM customers c 
JOIN orders o ON c.id = o.customer_id 
WHERE c.city = 'New York';

-- Output shows plan:
-- PLAN JOIN (C INDEX (IDX_CUSTOMERS_CITY), O INDEX (IDX_ORDERS_CUSTOMER))
```

### **Indexing System**

#### **Index Types**
```sql
-- B-tree index (default)
CREATE INDEX idx_customer_name ON customers (name);

-- Unique index
CREATE UNIQUE INDEX idx_customer_email ON customers (email);

-- Composite index
CREATE INDEX idx_order_date_status ON orders (order_date, status);

-- Descending index
CREATE DESCENDING INDEX idx_price_desc ON products (price);

-- Expression index
CREATE INDEX idx_upper_name ON customers COMPUTED BY (UPPER(name));
```

#### **Index Statistics**
```sql
-- Update index statistics for better optimization
SET STATISTICS INDEX idx_customer_name;

-- View index information
SELECT 
    RDB$INDEX_NAME,
    RDB$RELATION_NAME,
    RDB$INDEX_INACTIVE,
    RDB$STATISTICS
FROM RDB$INDICES 
WHERE RDB$RELATION_NAME = 'CUSTOMERS';
```

### **Parallel Processing**

#### **Parallel Query Execution**
```sql
-- Configure parallel workers
-- In firebird.conf
ParallelWorkers = 4

-- Queries automatically use parallel execution for:
-- - Large table scans
-- - Index builds
-- - Backup/restore operations
-- - Garbage collection
```

#### **Parallel Operations Example**
```sql
-- This query may use parallel execution
SELECT region, COUNT(*), AVG(sales_amount)
FROM large_sales_table 
WHERE sales_date >= '2024-01-01'
GROUP BY region;

-- Monitor parallel execution
SELECT MON$ATTACHMENT_ID, MON$STATEMENT_ID, MON$SQL_TEXT
FROM MON$STATEMENTS 
WHERE MON$SQL_TEXT CONTAINING 'large_sales_table';
```

## 🔧 Configuration and Tuning

### **Database Parameters**

#### **Page Size Selection**
```sql
-- Create database with specific page size
CREATE DATABASE 'mydb.fdb' 
PAGE_SIZE 16384  -- 16KB pages for large databases
USER 'SYSDBA' PASSWORD 'masterkey';
```

**Page Size Guidelines:**
- **4KB**: Small databases, OLTP applications
- **8KB**: Default, good for most applications  
- **16KB**: Large databases, data warehouses
- **32KB**: Very large databases, analytical workloads

#### **Cache Configuration**
```bash
# In scratchbird.conf
DefaultDbCachePages = 10000    # 80MB with 8KB pages
TempCacheLimit = 134217728     # 128MB temp space
LockMemSize = 1048576          # 1MB lock table
```

### **Performance Monitoring**

#### **Database Statistics**
```bash
# Comprehensive database analysis
sb_gstat -all -user SYSDBA -password masterkey mydb.fdb

# Table-specific statistics
sb_gstat -table customers -index -user SYSDBA mydb.fdb

# Header information
sb_gstat -header -user SYSDBA mydb.fdb
```

#### **Monitor Tables**
```sql
-- Current connections
SELECT 
    MON$ATTACHMENT_ID,
    MON$USER,
    MON$REMOTE_ADDRESS,
    MON$TIMESTAMP
FROM MON$ATTACHMENTS;

-- Active statements
SELECT 
    MON$ATTACHMENT_ID,
    MON$SQL_TEXT,
    MON$TIMESTAMP
FROM MON$STATEMENTS
WHERE MON$STATE = 1;  -- Active

-- Lock conflicts
SELECT 
    MON$LOCK_ID,
    MON$LOCK_STATE,
    MON$OBJECT_NAME,
    MON$LOCK_TIMEOUT
FROM MON$LOCKS
WHERE MON$LOCK_STATE = 'W';  -- Waiting
```

### **Backup and Recovery**

#### **Online Backup**
```bash
# Hot backup (database remains online)
sb_gbak -backup -online -user SYSDBA -password masterkey \
        mydb.fdb mydb_backup.fbk

# Incremental backup (ScratchBird enhancement)
sb_nbackup -level 0 -user SYSDBA mydb.fdb mydb_full.nb
sb_nbackup -level 1 -user SYSDBA mydb.fdb mydb_incr1.nb
```

#### **Point-in-Time Recovery**
```sql
-- Enable archival logging
ALTER DATABASE SET ARCHIVE MODE;

-- Archive log files automatically saved
-- Recovery possible to any point in time
```

## 🔐 Security Integration

### **Row-Level Security**
```sql
-- Create security policy
CREATE OR ALTER TRIGGER customer_security_trigger
FOR customers ACTIVE BEFORE SELECT
AS
BEGIN
    -- Only show customers for current user's region
    IF (USER <> 'SYSDBA' AND NEW.region <> CURRENT_USER_REGION()) THEN
        EXCEPTION E_SECURITY_VIOLATION 'Access denied to customer data';
END;
```

### **Audit Trail**
```sql
-- Built-in audit capabilities
CREATE TABLE audit_log (
    audit_id BIGINT PRIMARY KEY,
    table_name VARCHAR(63),
    operation VARCHAR(10),
    user_name VARCHAR(63),
    timestamp_val TIMESTAMP,
    old_values BLOB,
    new_values BLOB
);

-- Automatic audit trigger
CREATE TRIGGER customers_audit_trigger
FOR customers ACTIVE AFTER UPDATE OR INSERT OR DELETE
AS
BEGIN
    INSERT INTO audit_log (table_name, operation, user_name, timestamp_val)
    VALUES ('CUSTOMERS', 
            CASE 
                WHEN INSERTING THEN 'INSERT'
                WHEN UPDATING THEN 'UPDATE' 
                WHEN DELETING THEN 'DELETE'
            END,
            USER, CURRENT_TIMESTAMP);
END;
```

## 🔍 Troubleshooting Engine Issues

### **Common Performance Problems**

#### **Slow Queries**
```sql
-- Identify slow queries
SELECT 
    MON$SQL_TEXT,
    MON$TIMESTAMP,
    DATEDIFF(SECOND, MON$TIMESTAMP, CURRENT_TIMESTAMP) as duration_seconds
FROM MON$STATEMENTS
WHERE MON$STATE = 1
ORDER BY duration_seconds DESC;

-- Check if indexes are being used
SET PLAN ON;
-- Run your slow query to see execution plan
```

#### **Lock Contention**
```bash
# Monitor locks
sb_lock_print -owner -wait -user SYSDBA mydb.fdb

# Find lock conflicts
SELECT * FROM MON$LOCKS WHERE MON$LOCK_STATE = 'W';
```

#### **Memory Issues**
```bash
# Check cache hit ratios
sb_gstat -cache -user SYSDBA mydb.fdb

# Increase cache if hit ratio < 90%
# In scratchbird.conf:
DefaultDbCachePages = 20000  # Increase cache size
```

### **Database Corruption**

#### **Validation**
```bash
# Check database integrity
sb_gfix -validate -full -user SYSDBA mydb.fdb

# Repair minor corruption
sb_gfix -mend -user SYSDBA mydb.fdb

# Full repair (last resort)
sb_gfix -full -mend -user SYSDBA mydb.fdb
```

#### **Recovery Strategies**
```bash
# 1. Try backup/restore first
sb_gbak -backup -ignore_checksums -user SYSDBA mydb.fdb backup.fbk
sb_gbak -restore -user SYSDBA backup.fbk mydb_fixed.fdb

# 2. If backup fails, try pumping data
sb_isql -extract -user SYSDBA mydb.fdb > schema.sql
sb_isql -input pump_data.sql -user SYSDBA newdb.fdb
```

## 📊 Best Practices

### **Database Design**
- Use appropriate data types (don't use VARCHAR(1000) for codes)
- Create indexes on foreign keys and frequently queried columns
- Use constraints to maintain data integrity
- Design hierarchical schemas logically

### **Transaction Management**
- Keep transactions short to reduce lock contention
- Use appropriate isolation levels
- Always handle exceptions and rollback on errors
- Commit regularly in batch operations

### **Performance Optimization**
- Update statistics regularly: `SET STATISTICS`
- Monitor query plans with `SET PLAN ON`
- Use COMMIT RETAIN for batch processing
- Configure appropriate cache sizes

---

## 🎯 Next Steps

- **[SQL Language Guide](06-sql-language.md)** - Learn ScratchBird SQL features
- **[Hierarchical Schemas](07-hierarchical-schemas.md)** - Advanced schema design  
- **[Performance Tuning](20-performance.md)** - Detailed optimization guide
- **[Administrator Guide](21-admin-guide.md)** - Production database management

## 💡 Advanced Topics

For developers and DBAs who want to dive deeper:
- **[API Reference](17-api-reference.md)** - Programming interfaces
- **[Configuration Reference](30-configuration-reference.md)** - All configuration options
- **[Monitoring Guide](23-monitoring.md)** - Advanced monitoring techniques