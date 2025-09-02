# Bulk Insert Optimization Specification

## Overview

ScratchBird optimizes multi-row INSERT operations by preparing the execution plan once and reusing it for all rows, dramatically reducing parsing overhead and improving bulk insert performance.

## Core Concepts

### 1. Multi-Row INSERT Syntax

```sql
-- Traditional single-row insert (inefficient for bulk operations)
INSERT INTO customers (id, name, email) VALUES (1, 'John', 'john@example.com');
INSERT INTO customers (id, name, email) VALUES (2, 'Jane', 'jane@example.com');
INSERT INTO customers (id, name, email) VALUES (3, 'Bob', 'bob@example.com');
-- 3 parse operations, 3 plan generations, 3 executions

-- Multi-row insert (optimized)
INSERT INTO customers (id, name, email) VALUES 
    (1, 'John', 'john@example.com'),
    (2, 'Jane', 'jane@example.com'),
    (3, 'Bob', 'bob@example.com'),
    (4, 'Alice', 'alice@example.com'),
    (5, 'Charlie', 'charlie@example.com');
-- 1 parse operation, 1 plan generation, 1 execution with 5 rows

-- Extended syntax for massive inserts
INSERT INTO sales_data (date, product_id, quantity, price) VALUES
    ('2024-01-01', 101, 5, 29.99),
    ('2024-01-01', 102, 3, 49.99),
    ('2024-01-01', 103, 7, 19.99),
    ... -- thousands of rows
    ('2024-01-31', 999, 2, 99.99);
```

### 2. Prepared Statement Optimization

```sql
-- Server-side prepared statement for bulk inserts
PREPARE bulk_insert AS
INSERT INTO orders (customer_id, product_id, quantity, price) VALUES
    ($1, $2, $3, $4);

-- Execute with multiple row sets
EXECUTE bulk_insert USING
    (100, 501, 2, 49.99),
    (101, 502, 1, 29.99),
    (102, 503, 5, 9.99),
    ... -- thousands more
    (999, 599, 3, 19.99);

-- Or with array parameters
EXECUTE bulk_insert USING 
    ARRAY[100, 101, 102, ...],  -- customer_ids
    ARRAY[501, 502, 503, ...],  -- product_ids
    ARRAY[2, 1, 5, ...],         -- quantities
    ARRAY[49.99, 29.99, 9.99, ...]; -- prices
```

### 3. Binary Language Representation (BLR) Reuse

```cpp
// Internal representation - prepare once, execute many
class BulkInsertPlan {
private:
    struct InsertBLR {
        TableId target_table;
        vector<ColumnId> columns;
        CompiledExpression* value_expressions;
        ValidationRules* constraints;
        TriggerChain* triggers;
        IndexUpdatePlan* index_updates;
        
        // Prepared once for all rows
        bool is_prepared = false;
    };
    
    InsertBLR compiled_plan;
    
public:
    void prepare(const string& sql) {
        if (!compiled_plan.is_prepared) {
            // Parse SQL once
            auto ast = parse_insert_statement(sql);
            
            // Generate execution plan once
            compiled_plan.target_table = resolve_table(ast.table_name);
            compiled_plan.columns = resolve_columns(ast.column_list);
            
            // Compile value expressions once
            compiled_plan.value_expressions = compile_expressions(ast.values);
            
            // Prepare constraint checks once
            compiled_plan.constraints = prepare_constraints(compiled_plan.target_table);
            
            // Prepare trigger chain once
            compiled_plan.triggers = prepare_triggers(compiled_plan.target_table);
            
            // Prepare index update plan once
            compiled_plan.index_updates = prepare_index_updates(compiled_plan.target_table);
            
            compiled_plan.is_prepared = true;
        }
    }
    
    void execute_bulk(const vector<RowData>& rows) {
        // Reuse the prepared plan for all rows
        for (const auto& row : rows) {
            execute_single_row(compiled_plan, row);
        }
    }
};
```

### 4. Streaming Insert Protocol

```cpp
// Efficient wire protocol for bulk inserts
class StreamingInsertProtocol {
    enum PacketType {
        BEGIN_BULK_INSERT = 0x20,
        ROW_DATA = 0x21,
        END_BULK_INSERT = 0x22,
        BULK_INSERT_STATUS = 0x23
    };
    
    struct BeginBulkInsertPacket {
        uint8_t packet_type = BEGIN_BULK_INSERT;
        uint32_t table_name_length;
        char table_name[256];
        uint32_t column_count;
        struct {
            uint32_t column_name_length;
            char column_name[256];
            uint32_t data_type;
        } columns[];
        uint32_t estimated_row_count;  // For optimization hints
    };
    
    struct RowDataPacket {
        uint8_t packet_type = ROW_DATA;
        uint32_t row_number;
        uint32_t data_length;
        uint8_t null_bitmap[];  // Bit array for NULL values
        uint8_t data[];        // Packed row data
    };
    
    struct EndBulkInsertPacket {
        uint8_t packet_type = END_BULK_INSERT;
        uint32_t total_rows_sent;
        bool commit_on_success;
    };
    
    struct BulkInsertStatusPacket {
        uint8_t packet_type = BULK_INSERT_STATUS;
        uint32_t rows_inserted;
        uint32_t rows_failed;
        uint32_t first_error_row;
        char error_message[1024];
    };
};
```

### 5. Batch Processing Optimizations

```sql
-- Configurable batch sizes
SET SESSION bulk_insert_batch_size = 1000;  -- Process 1000 rows at a time
SET SESSION bulk_insert_buffer_size = '256MB';  -- Buffer size for bulk ops

-- Automatic batching for large inserts
INSERT INTO large_table (col1, col2, col3)
WITH BATCH SIZE 5000  -- Explicitly set batch size
VALUES
    (val1, val2, val3),
    ... -- millions of rows
    (valN, valN, valN);

-- Progress reporting for long-running bulk inserts
INSERT INTO huge_table (...)
WITH PROGRESS INTERVAL 10000  -- Report every 10,000 rows
VALUES ...;
-- Sends progress events: 'bulk_insert_progress' with row count
```

### 6. Memory-Efficient Bulk Loading

```cpp
class EfficientBulkLoader {
private:
    // Ring buffer for streaming inserts
    class RingBuffer {
        static constexpr size_t BUFFER_SIZE = 1024 * 1024;  // 1MB chunks
        uint8_t buffer[BUFFER_SIZE];
        size_t write_pos = 0;
        size_t read_pos = 0;
        
    public:
        void write_row(const RowData& row) {
            // Zero-copy write to buffer
            memcpy(&buffer[write_pos], row.data, row.size);
            write_pos = (write_pos + row.size) % BUFFER_SIZE;
        }
        
        RowData read_row() {
            // Zero-copy read from buffer
            RowData row;
            row.data = &buffer[read_pos];
            read_pos = (read_pos + row.size) % BUFFER_SIZE;
            return row;
        }
    };
    
    // Parallel insert workers
    class ParallelInserter {
        ThreadPool workers;
        Queue<RowBatch> work_queue;
        
    public:
        void insert_parallel(const InsertPlan& plan, RingBuffer& buffer) {
            // Distribute rows across workers
            while (buffer.has_data()) {
                RowBatch batch = buffer.read_batch(1000);
                workers.enqueue([plan, batch] {
                    insert_batch(plan, batch);
                });
            }
            workers.wait_all();
        }
    };
    
    // Write-ahead logging optimization
    class BulkWAL {
        void log_bulk_insert(TableId table, const vector<RowData>& rows) {
            // Single WAL entry for entire batch
            WALEntry entry{
                .type = WAL_BULK_INSERT,
                .table = table,
                .row_count = rows.size(),
                .data = compress_rows(rows)  // Compress for efficiency
            };
            wal_writer.write(entry);
        }
    };
};
```

### 7. Advanced Bulk Insert Features

#### COPY Command (PostgreSQL-compatible)

```sql
-- High-speed bulk loading from files
COPY customers (id, name, email) FROM '/data/customers.csv' 
WITH (
    FORMAT CSV,
    HEADER true,
    DELIMITER ',',
    NULL 'NULL',
    BATCH_SIZE 10000,
    PARALLEL 4  -- Use 4 parallel workers
);

-- Copy from stdin
COPY products (id, name, price) FROM STDIN WITH (FORMAT CSV);
101,Widget,19.99
102,Gadget,29.99
103,Doohickey,39.99
\.

-- Copy with transformation
COPY sales_data (date, amount) FROM '/data/sales.json'
WITH (
    FORMAT JSON,
    TRANSFORM 'date = to_date($.date, "YYYY-MM-DD"), amount = $.amount * 1.1'
);
```

#### Bulk Insert with Conflict Resolution

```sql
-- Insert with ON CONFLICT handling
INSERT INTO users (id, email, name) VALUES
    (1, 'john@example.com', 'John'),
    (2, 'jane@example.com', 'Jane'),
    (3, 'bob@example.com', 'Bob')
ON CONFLICT (email) DO UPDATE SET
    name = EXCLUDED.name,
    updated_at = CURRENT_TIMESTAMP;

-- Bulk MERGE operation
MERGE INTO target_table AS t
USING (VALUES
    (1, 'A', 100),
    (2, 'B', 200),
    (3, 'C', 300)
) AS s(id, code, value)
ON t.id = s.id
WHEN MATCHED THEN UPDATE SET
    t.code = s.code,
    t.value = s.value
WHEN NOT MATCHED THEN INSERT
    (id, code, value) VALUES (s.id, s.code, s.value);
```

#### Returning Clause for Bulk Inserts

```sql
-- Get generated IDs for all inserted rows
INSERT INTO orders (customer_id, total) VALUES
    (100, 299.99),
    (101, 149.99),
    (102, 499.99)
RETURNING id, customer_id, created_at;

-- Returns:
-- id | customer_id | created_at
-- 1001 | 100 | 2024-01-15 10:00:00
-- 1002 | 101 | 2024-01-15 10:00:00
-- 1003 | 102 | 2024-01-15 10:00:00
```

### 8. Performance Metrics and Monitoring

```sql
-- Monitor bulk insert performance
SELECT * FROM pg_stat_bulk_inserts WHERE table_name = 'large_table';
-- Shows: rows_per_second, bytes_per_second, cpu_usage, memory_usage

-- Set performance thresholds
ALTER TABLE large_table SET bulk_insert_monitoring = ON;
ALTER TABLE large_table SET bulk_insert_alert_threshold = 1000;  -- Alert if < 1000 rows/sec

-- View current bulk operations
SELECT * FROM pg_stat_progress_bulk_insert;
-- Shows: session_id, table_name, rows_processed, rows_total, percent_complete, eta
```

### 9. Client Library Support

#### Python

```python
import scratchbird

# Efficient bulk insert using prepared statement
conn = scratchbird.connect(...)
cursor = conn.cursor()

# Prepare once
cursor.prepare("""
    INSERT INTO sales (date, product_id, quantity, price) 
    VALUES (?, ?, ?, ?)
""")

# Execute with many rows
data = [
    ('2024-01-01', 101, 5, 29.99),
    ('2024-01-01', 102, 3, 49.99),
    # ... thousands more
]

# Automatic batching
cursor.executemany_bulk(data, batch_size=5000)

# Or use COPY for maximum speed
cursor.copy_from(
    file='sales_data.csv',
    table='sales',
    columns=['date', 'product_id', 'quantity', 'price'],
    format='CSV',
    batch_size=10000
)
```

#### Java/JDBC

```java
// Batch insert with prepared statement
PreparedStatement pstmt = conn.prepareStatement(
    "INSERT INTO orders (customer_id, product_id, quantity) VALUES (?, ?, ?)"
);

// Add batches
for (Order order : orders) {
    pstmt.setInt(1, order.getCustomerId());
    pstmt.setInt(2, order.getProductId());
    pstmt.setInt(3, order.getQuantity());
    pstmt.addBatch();
    
    // Execute in chunks
    if (++count % 1000 == 0) {
        pstmt.executeBatch();
        pstmt.clearBatch();
    }
}

// Execute remaining
pstmt.executeBatch();

// Or use ScratchBird's bulk API
SDBBulkInsert bulk = conn.createBulkInsert("orders");
bulk.setColumns("customer_id", "product_id", "quantity");
bulk.setBatchSize(5000);

for (Order order : orders) {
    bulk.addRow(order.getCustomerId(), order.getProductId(), order.getQuantity());
}

BulkResult result = bulk.execute();
System.out.println("Inserted: " + result.getRowsInserted());
```

### 10. Optimization Strategies

```cpp
class BulkInsertOptimizer {
    InsertStrategy choose_strategy(const BulkInsertRequest& request) {
        size_t row_count = request.row_count;
        size_t row_size = request.avg_row_size;
        
        if (row_count < 100) {
            return InsertStrategy::SIMPLE;  // Regular multi-row insert
        }
        else if (row_count < 10000) {
            return InsertStrategy::BATCHED;  // Batched inserts
        }
        else if (row_count < 100000) {
            return InsertStrategy::PARALLEL;  // Parallel workers
        }
        else {
            return InsertStrategy::BULK_LOAD;  // Direct page writes
        }
    }
    
    void optimize_for_bulk(TableId table) {
        // Temporarily disable non-critical operations
        disable_triggers(table, /*except_critical=*/true);
        defer_index_updates(table);
        increase_buffer_size(table);
        enable_parallel_workers(4);
        
        // Re-enable after bulk operation
        cleanup_guard([=] {
            enable_triggers(table);
            rebuild_indexes(table);
            restore_buffer_size(table);
        });
    }
};
```

## Performance Comparison

| Insert Method | Rows/Second | Parse Overhead | Memory Usage | Use Case |
|--------------|-------------|----------------|--------------|----------|
| Single INSERT | 100-500 | High (per row) | Low | Small updates |
| Multi-row INSERT | 5,000-10,000 | Low (once) | Medium | Moderate bulk |
| Prepared Batch | 20,000-50,000 | Minimal | Medium | Large bulk |
| COPY Command | 100,000-500,000 | None | Low | Massive imports |
| Direct Load | 1,000,000+ | None | High | Initial load |

## Configuration Options

```sql
-- System-wide settings
ALTER SYSTEM SET max_bulk_insert_batch_size = 10000;
ALTER SYSTEM SET bulk_insert_buffer_size = '512MB';
ALTER SYSTEM SET bulk_insert_parallel_workers = 4;
ALTER SYSTEM SET bulk_insert_wal_level = 'minimal';  -- Reduce WAL overhead

-- Per-session settings
SET SESSION bulk_insert_batch_size = 5000;
SET SESSION bulk_insert_commit_interval = 10000;  -- Commit every 10k rows
SET SESSION bulk_insert_error_handling = 'continue';  -- Skip bad rows

-- Per-table settings
ALTER TABLE large_table SET bulk_insert_enabled = true;
ALTER TABLE large_table SET bulk_insert_trigger_mode = 'disabled';  -- Skip triggers
ALTER TABLE large_table SET bulk_insert_index_mode = 'deferred';  -- Build indexes after
```

## Testing

```sql
-- Test multi-row insert performance
CREATE TABLE bulk_test (
    id INTEGER PRIMARY KEY,
    data VARCHAR(100),
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Generate test data
WITH RECURSIVE generate_series(value) AS (
    SELECT 1
    UNION ALL
    SELECT value + 1 FROM generate_series WHERE value < 100000
)
INSERT INTO bulk_test (id, data)
SELECT 
    value,
    'Test data ' || value
FROM generate_series;

-- Verify performance metrics
SELECT 
    rows_inserted,
    execution_time,
    rows_per_second,
    parse_time,
    execution_time - parse_time AS actual_insert_time
FROM pg_stat_statements
WHERE query LIKE 'INSERT INTO bulk_test%';
```

## Summary

ScratchBird's bulk insert optimization provides:

1. **Single Parse, Multiple Execute** - Parse once, insert many
2. **Prepared BLR Reuse** - Compile plan once for all rows
3. **Efficient Wire Protocol** - Streaming row data
4. **Parallel Processing** - Multiple workers for large batches
5. **Memory Optimization** - Ring buffers and zero-copy
6. **Flexible Batching** - Automatic or explicit batch control
7. **COPY Command** - PostgreSQL-compatible bulk loading
8. **Progress Monitoring** - Real-time bulk operation tracking

This makes bulk data loading 10-1000x faster than single-row inserts while maintaining ACID compliance and data integrity.