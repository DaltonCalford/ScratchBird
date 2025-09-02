### DDL: Sequences

**What it is**

Sequences (also called generators in some databases) are database objects that generate unique sequential numeric values. They're commonly used for auto-incrementing primary keys, generating unique identifiers, and creating sequential numbers for various business requirements. ScratchBird supports both standalone sequences and identity columns that use sequences internally.

**Why it matters**

- **Unique Values**: Guarantee unique sequential numbers without conflicts
- **Performance**: Generate IDs without table locks or contention
- **Flexibility**: Control increment, start values, cycles, and caching
- **Portability**: Standard SQL feature across database systems
- **Gap Tolerance**: Handle rollbacks and failures gracefully

**How to use it**

Create sequences for auto-incrementing columns or when you need controlled number generation. Use identity columns for simple auto-increment needs, or standalone sequences when you need more control or want to share sequences across tables.

## CREATE SEQUENCE

### Basic Syntax

```sql
CREATE SEQUENCE [IF NOT EXISTS] sequence_name
[START [WITH] start_value]
[INCREMENT [BY] increment_value]
[MINVALUE min_value | NO MINVALUE]
[MAXVALUE max_value | NO MAXVALUE]
[CYCLE | NO CYCLE]
[CACHE cache_size]
[OWNED BY table.column | NONE];
```

### Simple Sequences

```sql
-- Basic sequence with defaults
CREATE SEQUENCE user_id_seq;
-- Defaults: START 1, INCREMENT 1, NO CYCLE

-- Sequence with custom start
CREATE SEQUENCE invoice_number_seq START WITH 1000;

-- Sequence with custom increment
CREATE SEQUENCE even_numbers INCREMENT BY 2;

-- Descending sequence
CREATE SEQUENCE countdown START WITH 1000 INCREMENT BY -1;
```

### Advanced Sequence Options

```sql
-- Full specification
CREATE SEQUENCE order_id_seq
    START WITH 10000
    INCREMENT BY 1
    MINVALUE 10000
    MAXVALUE 999999
    NO CYCLE
    CACHE 20;

-- Cycling sequence
CREATE SEQUENCE rotating_batch
    START WITH 1
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 100
    CYCLE;  -- Restart at MINVALUE after MAXVALUE

-- Large range sequence
CREATE SEQUENCE global_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 50;  -- Cache for performance

-- Negative increment with cycle
CREATE SEQUENCE priority_seq
    START WITH 1
    INCREMENT BY -1
    MINVALUE -999
    MAXVALUE 1
    CYCLE;
```

### Owned Sequences

Link sequences to table columns for automatic cleanup:

```sql
-- Create table with sequence
CREATE SEQUENCE products_id_seq;
CREATE TABLE products (
    id INTEGER DEFAULT NEXTVAL('products_id_seq') PRIMARY KEY,
    name VARCHAR(100)
);
-- Link sequence to column
ALTER SEQUENCE products_id_seq OWNED BY products.id;

-- When table is dropped, sequence is also dropped
DROP TABLE products;  -- Also drops products_id_seq
```

## Using Sequences

### NEXTVAL Function

Get the next value from a sequence:

```sql
-- Get next value
SELECT NEXTVAL('user_id_seq');

-- Use in INSERT
INSERT INTO users (id, username) 
VALUES (NEXTVAL('user_id_seq'), 'john_doe');

-- Multiple inserts with sequence
INSERT INTO orders (order_id, customer_id, order_date)
SELECT 
    NEXTVAL('order_id_seq'),
    customer_id,
    CURRENT_DATE
FROM customers
WHERE status = 'active';
```

### CURRVAL Function

Get the current value (last generated in this session):

```sql
-- Get current value (must call NEXTVAL first in session)
SELECT CURRVAL('user_id_seq');

-- Use for related records
BEGIN;
    INSERT INTO orders (id, customer_id) 
    VALUES (NEXTVAL('order_id_seq'), 123);
    
    INSERT INTO order_audit (order_id, action) 
    VALUES (CURRVAL('order_id_seq'), 'created');
COMMIT;
```

### SETVAL Function

Set sequence to specific value:

```sql
-- Set to specific value
SELECT SETVAL('user_id_seq', 1000);

-- Set value and control next value
SELECT SETVAL('user_id_seq', 1000, true);   -- Next value will be 1001
SELECT SETVAL('user_id_seq', 1000, false);  -- Next value will be 1000

-- Reset sequence to start value
SELECT SETVAL('user_id_seq', 1, false);

-- Set to maximum existing value
SELECT SETVAL('user_id_seq', (SELECT MAX(id) FROM users));
```

### LASTVAL Function

Get the last sequence value generated in the session:

```sql
-- Get last generated value from any sequence
SELECT LASTVAL();

-- Useful in triggers or procedures
CREATE FUNCTION log_sequence_use() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO sequence_log (seq_value, created_at)
    VALUES (LASTVAL(), NOW());
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

## ALTER SEQUENCE

Modify existing sequences:

### Change Increment

```sql
-- Change increment
ALTER SEQUENCE user_id_seq INCREMENT BY 10;

-- Change to negative increment
ALTER SEQUENCE countdown_seq INCREMENT BY -1;
```

### Adjust Range

```sql
-- Set new minimum
ALTER SEQUENCE order_id_seq MINVALUE 10000;

-- Set new maximum
ALTER SEQUENCE batch_seq MAXVALUE 999999;

-- Remove limits
ALTER SEQUENCE unlimited_seq NO MINVALUE NO MAXVALUE;
```

### Restart Sequence

```sql
-- Restart from beginning
ALTER SEQUENCE user_id_seq RESTART;

-- Restart from specific value
ALTER SEQUENCE invoice_seq RESTART WITH 2000;

-- Restart based on existing data
ALTER SEQUENCE product_id_seq RESTART WITH (
    SELECT COALESCE(MAX(id), 0) + 1 FROM products
);
```

### Cycle Options

```sql
-- Enable cycling
ALTER SEQUENCE batch_number CYCLE;

-- Disable cycling
ALTER SEQUENCE order_id_seq NO CYCLE;
```

### Cache Settings

```sql
-- Increase cache for performance
ALTER SEQUENCE high_volume_seq CACHE 100;

-- Disable cache for strict ordering
ALTER SEQUENCE audit_seq CACHE 1;
```

### Ownership

```sql
-- Change owner
ALTER SEQUENCE user_id_seq OWNER TO admin_user;

-- Link to column
ALTER SEQUENCE product_id_seq OWNED BY products.id;

-- Remove ownership
ALTER SEQUENCE shared_seq OWNED BY NONE;
```

## DROP SEQUENCE

Remove sequences:

```sql
-- Basic drop
DROP SEQUENCE old_sequence;

-- Drop if exists
DROP SEQUENCE IF EXISTS temp_seq;

-- Drop with cascade (removes dependent objects)
DROP SEQUENCE user_id_seq CASCADE;

-- Drop with restrict (fail if dependencies)
DROP SEQUENCE important_seq RESTRICT;

-- Drop multiple sequences
DROP SEQUENCE seq1, seq2, seq3;
```

## RECREATE SEQUENCE

Replace existing sequence:

```sql
-- Recreate with new settings
RECREATE SEQUENCE batch_id_seq
    START WITH 1
    INCREMENT BY 1
    MAXVALUE 9999
    CYCLE;
```

## Identity Columns vs Sequences

### Identity Columns (Recommended)

```sql
-- GENERATED ALWAYS AS IDENTITY
CREATE TABLE users (
    id INTEGER GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    username VARCHAR(50)
);

-- GENERATED BY DEFAULT AS IDENTITY
CREATE TABLE products (
    id INTEGER GENERATED BY DEFAULT AS IDENTITY (
        START WITH 1000
        INCREMENT BY 1
    ) PRIMARY KEY,
    name VARCHAR(100)
);

-- Identity with all options
CREATE TABLE invoices (
    invoice_id BIGINT GENERATED ALWAYS AS IDENTITY (
        START WITH 10000
        INCREMENT BY 1
        MINVALUE 10000
        MAXVALUE 999999999
        CYCLE
        CACHE 50
    ) PRIMARY KEY,
    invoice_date DATE
);
```

### Manual Sequences (More Control)

```sql
-- Shared sequence across tables
CREATE SEQUENCE global_id_seq;

CREATE TABLE customers (
    id BIGINT DEFAULT NEXTVAL('global_id_seq') PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE suppliers (
    id BIGINT DEFAULT NEXTVAL('global_id_seq') PRIMARY KEY,
    name VARCHAR(100)
);

-- Custom default expression
CREATE TABLE events (
    id BIGINT DEFAULT (NEXTVAL('event_seq') * 100 + 1) PRIMARY KEY,
    event_type VARCHAR(50)
);
```

## Sequence Patterns

### Gap-Free Sequences

```sql
-- Note: True gap-free sequences impact performance
-- Use only when absolutely required (e.g., legal invoices)

CREATE TABLE invoice_numbers (
    year INTEGER,
    last_number INTEGER,
    PRIMARY KEY (year)
);

CREATE FUNCTION next_invoice_number(p_year INTEGER) 
RETURNS INTEGER AS $$
DECLARE
    v_number INTEGER;
BEGIN
    -- Lock the row to prevent concurrent access
    UPDATE invoice_numbers 
    SET last_number = last_number + 1
    WHERE year = p_year
    RETURNING last_number INTO v_number;
    
    IF NOT FOUND THEN
        INSERT INTO invoice_numbers (year, last_number)
        VALUES (p_year, 1)
        RETURNING last_number INTO v_number;
    END IF;
    
    RETURN v_number;
END;
$$ LANGUAGE plpgsql;
```

### Prefixed IDs

```sql
-- Sequence with prefix
CREATE SEQUENCE customer_seq START WITH 1000;

CREATE TABLE customers (
    id VARCHAR(20) PRIMARY KEY DEFAULT 'CUST-' || NEXTVAL('customer_seq'),
    name VARCHAR(100)
);

-- Year-based prefixes
CREATE TABLE invoices (
    invoice_number VARCHAR(20) PRIMARY KEY DEFAULT 
        'INV-' || EXTRACT(YEAR FROM CURRENT_DATE) || '-' || 
        LPAD(NEXTVAL('invoice_seq')::TEXT, 6, '0'),
    amount DECIMAL(10,2)
);
```

### Partitioned Sequences

```sql
-- Different sequences per partition
CREATE SEQUENCE orders_2023_seq;
CREATE SEQUENCE orders_2024_seq;

CREATE TABLE orders_2023 (
    id BIGINT DEFAULT NEXTVAL('orders_2023_seq'),
    order_date DATE CHECK (order_date >= '2023-01-01' AND order_date < '2024-01-01')
) INHERITS (orders);

CREATE TABLE orders_2024 (
    id BIGINT DEFAULT NEXTVAL('orders_2024_seq'),
    order_date DATE CHECK (order_date >= '2024-01-01' AND order_date < '2025-01-01')
) INHERITS (orders);
```

### UUID vs Sequence

```sql
-- Sequential ID (predictable, sortable, compact)
CREATE TABLE with_sequence (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    data TEXT
);

-- UUID (unpredictable, distributed-friendly, larger)
CREATE TABLE with_uuid (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    data TEXT
);

-- Hybrid approach
CREATE TABLE hybrid_ids (
    id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,  -- Internal use
    public_id UUID DEFAULT gen_random_uuid() UNIQUE,     -- External API
    data TEXT
);
```

## Performance Considerations

### Cache Size

```sql
-- High-volume inserts benefit from larger cache
CREATE SEQUENCE high_volume_seq CACHE 1000;

-- Strict ordering requires no cache
CREATE SEQUENCE audit_trail_seq CACHE 1;

-- Balance cache size with potential gaps
CREATE SEQUENCE balanced_seq CACHE 20;  -- Default is often 1 or 20
```

### Monitoring Sequences

```sql
-- View sequence information
SELECT 
    sequence_schema,
    sequence_name,
    start_value,
    minimum_value,
    maximum_value,
    increment,
    cycle_option
FROM information_schema.sequences
ORDER BY sequence_schema, sequence_name;

-- Check current values
SELECT 
    schemaname,
    sequencename,
    last_value,
    start_value,
    increment_by,
    max_value,
    min_value,
    cache_value,
    is_cycled
FROM pg_sequences;

-- Find sequences near maximum
SELECT 
    schemaname,
    sequencename,
    last_value,
    max_value,
    (last_value::FLOAT / max_value::FLOAT * 100)::NUMERIC(5,2) AS percent_used
FROM pg_sequences
WHERE max_value IS NOT NULL
  AND (last_value::FLOAT / max_value::FLOAT) > 0.8
ORDER BY percent_used DESC;
```

## Troubleshooting

### Reset After Import

```sql
-- After bulk import, reset sequences
DO $$
DECLARE
    r RECORD;
BEGIN
    FOR r IN 
        SELECT 
            sequence_name,
            table_name,
            column_name
        FROM information_schema.columns
        WHERE column_default LIKE 'nextval%'
    LOOP
        EXECUTE format('SELECT SETVAL(%L, COALESCE(MAX(%I), 1)) FROM %I',
            r.sequence_name,
            r.column_name,
            r.table_name);
    END LOOP;
END $$;
```

### Handle Gaps

```sql
-- Gaps are normal and expected
-- Reasons for gaps:
-- 1. Rolled back transactions
-- 2. Cached values on server restart
-- 3. Concurrent sessions

-- If gaps are unacceptable, don't use sequences
-- Use a gapless number table with row locking instead
```

## Implementation Details

**Parser Implementation** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_sequence`: Handles CREATE/ALTER/DROP/RECREATE SEQUENCE
- Also recognizes Firebird-style generators (CREATE GENERATOR)
- Captures START WITH, INCREMENT BY, CYCLE options

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlSequenceAst {
    std::string name;
    std::string action;  // CREATE|ALTER|DROP|RECREATE|SET
    int64_t start_with{1};
    int64_t increment_by{1};
    int64_t min_value{1};
    int64_t max_value{LLONG_MAX};
    bool cycle{false};
    int32_t cache{1};
};
```

**Code Anchors**:
- Sequence parser: `src/engine/parser_ddl.cpp` (parse_ddl_sequence)
- Generator support: Recognizes SET GENERATOR syntax
- AST definition: `include/scratchbird/engine/ast.h` (DdlSequenceAst)

## See also

- [Tables](./ddl-tables.md) - Identity columns using sequences
- [Data Types](./sql-data-types.md) - Numeric types for sequence values
- [PSQL Runtime](./psql-runtime.md) - Using sequences in procedures
- [Triggers](./psql-routines-and-triggers.md) - Sequence usage in triggers