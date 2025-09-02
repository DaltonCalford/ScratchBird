# Temporary Tables and Result Sets Specification

## Overview

ScratchBird implements multiple types of temporary tables and introduces result sets as first-class data types that can be passed between procedures.

## Temporary Table Types

### 1. Transaction-Scoped Temporary Tables (GTT Type 1)

```sql
-- Data deleted on commit (Firebird-style)
CREATE GLOBAL TEMPORARY TABLE temp_calc (
    id INTEGER,
    value DECIMAL(10,2)
) ON COMMIT DELETE ROWS;

-- Data is transaction-private
-- Automatically cleared on COMMIT or ROLLBACK
-- Structure persists, data doesn't
```

### 2. Connection-Scoped Temporary Tables (GTT Type 2)

```sql
-- Data preserved until connection ends
CREATE GLOBAL TEMPORARY TABLE session_data (
    key VARCHAR(100),
    value TEXT
) ON COMMIT PRESERVE ROWS;

-- Data survives COMMIT
-- Cleared when connection closes
-- Each connection has isolated data
```

### 3. Statement-Scoped Temporary Tables

```sql
-- Exists only for duration of single statement
WITH TEMPORARY temp_calc AS MATERIALIZED (
    SELECT user_id, SUM(amount) as total
    FROM orders
    GROUP BY user_id
)
SELECT * FROM temp_calc WHERE total > 1000;
-- temp_calc destroyed after statement completes
```

### 4. Procedure-Scoped Temporary Tables

```sql
CREATE PROCEDURE process_data()
AS
BEGIN
    -- Local temporary table
    DECLARE LOCAL TEMPORARY TABLE work_table (
        id INTEGER,
        processed_value DECIMAL
    );
    
    -- Exists only within procedure
    INSERT INTO work_table SELECT id, value * 2 FROM source;
    
    -- Automatically dropped when procedure exits
END;
```

### 5. Anonymous Temporary Tables

```sql
-- Created implicitly by engine for optimization
-- Example: Materializing CTE
WITH RECURSIVE hierarchy AS MATERIALIZED (
    -- Engine creates hidden temp table
    SELECT id, parent_id, name FROM departments WHERE parent_id IS NULL
    UNION ALL
    SELECT d.id, d.parent_id, d.name 
    FROM departments d
    JOIN hierarchy h ON d.parent_id = h.id
)
SELECT * FROM hierarchy;
```

## Result Sets as First-Class Types

### Defining Result Set Types

```sql
-- Define a result set type
CREATE TYPE OrderSummarySet AS TABLE (
    customer_id INTEGER,
    total_orders INTEGER,
    total_amount DECIMAL(10,2)
);

-- Use in procedure signatures
CREATE PROCEDURE get_customer_summary(
    IN date_from DATE,
    IN date_to DATE
) RETURNS OrderSummarySet
AS
BEGIN
    RETURN SELECT 
        customer_id,
        COUNT(*) as total_orders,
        SUM(amount) as total_amount
    FROM orders
    WHERE order_date BETWEEN date_from AND date_to
    GROUP BY customer_id;
END;
```

### Passing Result Sets Between Procedures

```sql
-- Procedure that returns a result set
CREATE PROCEDURE fetch_active_users()
RETURNS TABLE (
    user_id INTEGER,
    username VARCHAR(100),
    last_active TIMESTAMP
)
AS
BEGIN
    RETURN SELECT user_id, username, last_active
           FROM users
           WHERE status = 'active';
END;

-- Procedure that accepts a result set
CREATE PROCEDURE process_users(
    IN users_set TABLE (user_id INTEGER, username VARCHAR(100))
)
AS
BEGIN
    -- Process the passed result set
    FOR SELECT user_id, username FROM users_set DO
        -- Process each row
        EXECUTE PROCEDURE send_notification(user_id);
    END FOR;
END;

-- Chain procedures with result sets
CREATE PROCEDURE workflow()
AS
DECLARE
    active_users TABLE (user_id INTEGER, username VARCHAR(100), last_active TIMESTAMP);
BEGIN
    -- Get result set from first procedure
    active_users = EXECUTE PROCEDURE fetch_active_users();
    
    -- Pass subset to second procedure
    EXECUTE PROCEDURE process_users(
        SELECT user_id, username FROM active_users WHERE last_active > CURRENT_DATE - 7
    );
END;
```

## Implementation Architecture

### Internal Temporary Table Manager

```cpp
class TempTableManager {
public:
    enum TempTableScope {
        TRANSACTION,      // ON COMMIT DELETE ROWS
        CONNECTION,       // ON COMMIT PRESERVE ROWS
        STATEMENT,        // Single statement only
        PROCEDURE,        // Procedure local
        MATERIALIZED_CTE  // Engine-managed for optimization
    };
    
    struct TempTable {
        UUID table_id;
        TempTableScope scope;
        UUID owner_transaction;
        UUID owner_connection;
        bool is_materialized;
        shared_ptr<HeapRelation> storage;
    };
    
private:
    // Separate storage areas for isolation
    map<UUID, TempTable> transaction_temps;
    map<UUID, TempTable> connection_temps;
    map<UUID, TempTable> statement_temps;
    
    // Automatic cleanup
    void on_transaction_end(UUID txn_id, bool committed) {
        if (committed) {
            // Delete ON COMMIT DELETE ROWS tables
            cleanup_transaction_temps(txn_id);
        }
    }
    
    void on_connection_close(UUID conn_id) {
        cleanup_connection_temps(conn_id);
    }
};
```

### Result Set Type System

```cpp
class ResultSetType {
    struct Column {
        string name;
        DataType type;
        bool nullable;
    };
    
    vector<Column> schema;
    UUID type_id;
    
    // Result set can be materialized or streaming
    enum ExecutionMode {
        MATERIALIZED,  // All rows in memory/temp table
        STREAMING      // Row-by-row processing
    };
};

class ResultSet {
    ResultSetType type;
    variant<
        shared_ptr<TempTable>,      // Materialized
        shared_ptr<QueryIterator>   // Streaming
    > data;
    
    // Convert between modes
    void materialize() {
        if (holds_alternative<shared_ptr<QueryIterator>>(data)) {
            auto temp = create_temp_table(type);
            auto iter = get<shared_ptr<QueryIterator>>(data);
            while (auto row = iter->next()) {
                temp->insert(row);
            }
            data = temp;
        }
    }
};
```

## Optimization Uses

### CTE Materialization

```sql
-- Optimizer decides when to materialize
WITH expensive_cte AS (
    SELECT /* complex aggregation */
    FROM large_table
    GROUP BY multiple_columns
)
SELECT * FROM expensive_cte e1
JOIN expensive_cte e2 ON e1.id = e2.parent_id;
-- Engine materializes once, uses twice
```

### View Materialization

```sql
-- Temporary materialization of complex view
CREATE VIEW complex_view AS
SELECT /* 10-way join with aggregations */;

-- Engine may materialize for session
SET SESSION materialize_complex_views = true;
SELECT * FROM complex_view;  -- Uses temp table
```

### Subquery Factoring

```sql
-- Engine converts correlated subquery to temp table
SELECT *
FROM orders o
WHERE amount > (
    SELECT AVG(amount) * 1.5
    FROM orders o2
    WHERE o2.customer_id = o.customer_id
);
-- Optimizer may materialize per-customer averages
```

## Memory Management

### Spill to Disk

```cpp
class TempTableStorage {
    size_t memory_limit = 100 * 1024 * 1024;  // 100MB default
    size_t current_memory = 0;
    
    enum StorageLocation {
        MEMORY_ONLY,
        DISK_ONLY,
        HYBRID  // Hot rows in memory, cold on disk
    };
    
    void add_row(Row row) {
        if (current_memory + row.size() > memory_limit) {
            spill_to_disk();
        }
        // Add row to appropriate storage
    }
};
```

### Cleanup Strategies

```sql
-- Configure temp table behavior
SET SESSION temp_table_memory_limit = '500MB';
SET SESSION temp_table_disk_location = '/fast/ssd/temp';
SET SESSION temp_table_compression = 'lz4';

-- Monitor temp table usage
SELECT * FROM system.temp_tables WHERE size_mb > 100;

-- Manual cleanup if needed
TRUNCATE TEMPORARY TABLE session_data;
DROP TEMPORARY TABLE IF EXISTS work_table;
```

## Use Cases

### 1. ETL Processing

```sql
CREATE PROCEDURE etl_daily_summary()
AS
BEGIN
    -- Create work table for intermediate results
    CREATE LOCAL TEMPORARY TABLE staging (
        date DATE,
        metric VARCHAR(50),
        value DECIMAL
    );
    
    -- Load and transform
    INSERT INTO staging
    SELECT date, 'revenue', SUM(amount)
    FROM transactions
    GROUP BY date;
    
    -- More transformations...
    
    -- Final insert
    INSERT INTO daily_summary
    SELECT * FROM staging;
    
    -- staging automatically dropped
END;
```

### 2. Recursive Processing

```sql
CREATE PROCEDURE calculate_hierarchy_metrics()
RETURNS TABLE (node_id INTEGER, level INTEGER, total DECIMAL)
AS
BEGIN
    CREATE LOCAL TEMPORARY TABLE work_queue (
        node_id INTEGER,
        level INTEGER,
        processed BOOLEAN DEFAULT FALSE
    );
    
    -- Seed with roots
    INSERT INTO work_queue (node_id, level)
    SELECT id, 0 FROM nodes WHERE parent_id IS NULL;
    
    -- Process iteratively
    WHILE EXISTS (SELECT 1 FROM work_queue WHERE NOT processed) DO
        -- Process batch and add children
    END WHILE;
    
    RETURN SELECT * FROM results;
END;
```

### 3. Set Operations Between Procedures

```sql
-- Procedure that analyzes a dataset
CREATE PROCEDURE analyze_dataset(
    IN dataset TABLE (id INTEGER, value DECIMAL)
)
RETURNS TABLE (
    metric VARCHAR(50),
    result DECIMAL
)
AS
BEGIN
    RETURN 
    SELECT 'mean', AVG(value) FROM dataset
    UNION ALL
    SELECT 'median', MEDIAN(value) FROM dataset
    UNION ALL
    SELECT 'stddev', STDDEV(value) FROM dataset;
END;

-- Use it with different sources
CREATE PROCEDURE run_analysis()
AS
DECLARE
    customer_data TABLE (id INTEGER, value DECIMAL);
    product_data TABLE (id INTEGER, value DECIMAL);
    analysis_results TABLE (metric VARCHAR(50), result DECIMAL);
BEGIN
    -- Get datasets
    customer_data = SELECT id, total_spent as value FROM customers;
    product_data = SELECT id, price as value FROM products;
    
    -- Analyze both
    analysis_results = EXECUTE PROCEDURE analyze_dataset(customer_data);
    INSERT INTO reports SELECT 'customer', * FROM analysis_results;
    
    analysis_results = EXECUTE PROCEDURE analyze_dataset(product_data);
    INSERT INTO reports SELECT 'product', * FROM analysis_results;
END;
```

## Performance Considerations

### Statistics on Temp Tables

```sql
-- Engine maintains statistics on temp tables
CREATE GLOBAL TEMPORARY TABLE temp_work (
    id INTEGER PRIMARY KEY,
    data TEXT
) ON COMMIT PRESERVE ROWS;

-- After loading data
INSERT INTO temp_work SELECT * FROM source;

-- Automatic statistics gathering
ANALYZE temp_work;  -- Or automatic after threshold

-- Optimizer uses statistics for joins
SELECT * FROM temp_work t
JOIN permanent_table p ON t.id = p.temp_id;
```

### Indexing Temp Tables

```sql
-- Indexes on temporary tables
CREATE GLOBAL TEMPORARY TABLE temp_lookup (
    key VARCHAR(100),
    value TEXT
) ON COMMIT DELETE ROWS;

-- Create index for session
CREATE INDEX idx_temp_key ON temp_lookup(key);

-- Index used within transaction
INSERT INTO temp_lookup SELECT ...;
SELECT * FROM temp_lookup WHERE key = 'specific';
```

## Compatibility

| Feature | PostgreSQL | MySQL | MSSQL | Firebird | ScratchBird |
|---------|------------|-------|-------|----------|-------------|
| Global Temp Tables | ✅ | ✅ | ✅ | ✅ | ✅ |
| Local Temp Tables | ✅ | ✅ | ✅ | ❌ | ✅ |
| ON COMMIT DELETE | ✅ | ❌ | ❌ | ✅ | ✅ |
| ON COMMIT PRESERVE | ✅ | ✅ | ✅ | ✅ | ✅ |
| Result Set Types | ❌ | ❌ | ✅ (TVP) | ❌ | ✅ Enhanced |
| Set Passing | ❌ | ❌ | ✅ | ❌ | ✅ Enhanced |
| CTE Materialization | ✅ | ✅ | ✅ | ✅ | ✅ Automatic |

## Configuration

```sql
-- Global settings
ALTER SYSTEM SET temp_table_max_size = '1GB';
ALTER SYSTEM SET temp_table_path = '/fast/nvme/temp';

-- Session settings
SET temp_table_statistics = 'auto';
SET temp_table_compression = 'zstd';
SET materialize_ctes = 'auto';  -- auto, always, never

-- Connection pool settings
SET CONNECTION POOL temp_table_cleanup = 'immediate';  -- or 'lazy'
```