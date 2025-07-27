# ScratchBird SEQUENCE/GENERATOR - Complete DDL Documentation

**Version**: Alpha 0.6.0  
**Implementation Date**: July 2025  
**Status**: ✅ **Test Ready** - Still missing features to be Implemented  
**Documentation Type**: User Guide & Technical Reference

---

## Overview

SEQUENCE (also known as GENERATOR) objects in ScratchBird provide auto-incrementing numeric values essential for generating unique identifiers, primary keys, and sequential numbering systems. Sequences are database-level objects that guarantee unique, sequential values across concurrent transactions and sessions.

### Key Features and Capabilities

- **Auto-Incrementing Values**: Generate sequential numeric values automatically
- **Concurrent Safety**: Thread-safe value generation across multiple connections
- **Customizable Increment**: Configure step size for value generation
- **Starting Value Control**: Set initial sequence value
- **Legacy Compatibility**: Support both SEQUENCE (SQL standard) and GENERATOR (legacy) syntax
- **Transaction Independence**: Sequence values are not rolled back with transactions
- **High Performance**: Optimized for minimal overhead during value generation
- **Schema Integration**: Full support for hierarchical schema qualification

### ScratchBird-Specific Enhancements

1. **Dual Syntax Support**: Both SEQUENCE (SQL standard) and GENERATOR (legacy Firebird) syntax
2. **64-bit Value Range**: Full BIGINT range (-9,223,372,036,854,775,808 to 9,223,372,036,854,775,807)
3. **Hierarchical Schema Support**: 3-level qualified names (`schema.subschema.sequence`)
4. **Enhanced System Catalog**: Extended RDB$GENERATORS table with schema and ownership tracking
5. **Identity Column Integration**: Seamless integration with IDENTITY column definitions
6. **Performance Optimization**: Efficient caching and minimal locking overhead
7. **Restart Capability**: Ability to restart sequences from specified values
8. **Legacy SET GENERATOR**: Backward compatibility with Firebird SET GENERATOR syntax

---

## DDL Syntax Reference

### CREATE SEQUENCE/GENERATOR

Creates a new sequence or generator with optional starting value and increment specification.

#### **Basic Syntax**
```sql
CREATE [OR REPLACE] {SEQUENCE | GENERATOR} [IF NOT EXISTS] [schema_name.]sequence_name
    [START WITH start_value]
    [INCREMENT [BY] increment_value];
```

#### **Complete Syntax with All Options**
```sql
CREATE [OR REPLACE] {SEQUENCE | GENERATOR} [IF NOT EXISTS] [[catalog.]schema.]sequence_name
    [START WITH start_value]
    [INCREMENT [BY] increment_value];
```

#### **Parameters**

- **OR REPLACE**: Replace existing sequence if it exists
- **IF NOT EXISTS**: Skip creation if sequence already exists (no error)
- **SEQUENCE | GENERATOR**: Either keyword can be used (identical functionality)
- **schema_name**: Optional schema qualification (supports hierarchical schemas)
- **sequence_name**: Sequence identifier (63 characters max)
- **START WITH**: Initial value for the sequence (default: 1)
- **INCREMENT BY**: Step size for each value generation (default: 1)

---

## CREATE SEQUENCE Examples

### **Basic Sequence Creation**

#### **Simple Auto-Incrementing Sequences**
```sql
-- Basic sequence starting at 1, incrementing by 1
CREATE SEQUENCE customer_seq;

-- Sequence with custom starting value
CREATE SEQUENCE order_seq START WITH 1000;

-- Sequence with custom increment
CREATE SEQUENCE even_numbers START WITH 2 INCREMENT BY 2;

-- Sequence for decreasing values
CREATE SEQUENCE countdown_seq START WITH 100 INCREMENT BY -1;
```

#### **Legacy GENERATOR Syntax**
```sql
-- Generator syntax (identical to SEQUENCE)
CREATE GENERATOR customer_id_gen;
CREATE GENERATOR product_id_gen START WITH 5000 INCREMENT BY 5;
CREATE GENERATOR batch_number_gen START WITH 100000;
```

### **Advanced Sequence Examples**

#### **Business-Specific Sequences**
```sql
-- Invoice numbering system
CREATE SEQUENCE invoice_number_seq 
    START WITH 2024001 
    INCREMENT BY 1;

-- Employee ID generation
CREATE SEQUENCE employee_id_seq 
    START WITH 10000 
    INCREMENT BY 1;

-- Order tracking with large increments
CREATE SEQUENCE order_tracking_seq 
    START WITH 1000000 
    INCREMENT BY 10;

-- Batch processing sequence
CREATE SEQUENCE batch_seq 
    START WITH 1 
    INCREMENT BY 100;
```

#### **Date-Based Sequences**
```sql
-- Year-month prefixed sequences (managed externally)
CREATE SEQUENCE monthly_report_seq 
    START WITH 1 
    INCREMENT BY 1;

-- Daily transaction sequence
CREATE SEQUENCE daily_txn_seq 
    START WITH 1 
    INCREMENT BY 1;

-- Quarterly batch sequence
CREATE SEQUENCE quarterly_batch_seq 
    START WITH 1 
    INCREMENT BY 1;
```

### **Hierarchical Schema Sequences**

#### **3-Level Schema Qualification**
```sql
-- Financial department sequences
CREATE SEQUENCE finance.accounting.transaction_seq 
    START WITH 100000 
    INCREMENT BY 1;

CREATE SEQUENCE finance.payroll.payslip_seq 
    START WITH 50000 
    INCREMENT BY 1;

-- HR department sequences  
CREATE SEQUENCE hr.recruitment.candidate_seq 
    START WITH 1000 
    INCREMENT BY 1;

CREATE SEQUENCE hr.training.session_seq 
    START WITH 1 
    INCREMENT BY 1;

-- Inventory management sequences
CREATE SEQUENCE inventory.warehouse.item_seq 
    START WITH 10000 
    INCREMENT BY 1;

CREATE SEQUENCE inventory.shipping.package_seq 
    START WITH 1000000 
    INCREMENT BY 1;
```

### **Conditional Creation**

#### **IF NOT EXISTS Usage**
```sql
-- Safe sequence creation (no error if exists)
CREATE SEQUENCE IF NOT EXISTS global_counter_seq;

-- Complex conditional creation with schema
CREATE SEQUENCE IF NOT EXISTS finance.accounting.ledger_seq 
    START WITH 1 
    INCREMENT BY 1;

-- Replace existing sequence
CREATE OR REPLACE SEQUENCE temp_sequence_seq 
    START WITH 1 
    INCREMENT BY 1;
```

### **High-Volume Sequences**

#### **Performance-Optimized Sequences**
```sql
-- High-throughput transaction sequence
CREATE SEQUENCE high_volume_txn_seq 
    START WITH 1 
    INCREMENT BY 1;

-- Bulk processing sequence with large increments
CREATE SEQUENCE bulk_process_seq 
    START WITH 1000000 
    INCREMENT BY 1000;

-- Distributed system sequence (avoid conflicts)
CREATE SEQUENCE distributed_node_seq 
    START WITH 1000000 
    INCREMENT BY 10;
```

---

## ALTER SEQUENCE/GENERATOR

Modifies existing sequence properties including restart value and increment step.

### **ALTER SEQUENCE Syntax**
```sql
ALTER {SEQUENCE | GENERATOR} [schema_name.]sequence_name
    { RESTART [WITH restart_value] |
      INCREMENT [BY] new_increment };
```

### **ALTER SEQUENCE Examples**

#### **Restarting Sequences**
```sql
-- Restart sequence from current position
ALTER SEQUENCE customer_seq RESTART;

-- Restart from specific value
ALTER SEQUENCE customer_seq RESTART WITH 50000;

-- Restart with new value and increment
ALTER SEQUENCE order_seq RESTART WITH 100000;
ALTER SEQUENCE order_seq INCREMENT BY 5;
```

#### **Changing Increment Values**
```sql
-- Change increment step
ALTER SEQUENCE batch_seq INCREMENT BY 50;

-- Change to negative increment (countdown)
ALTER SEQUENCE countdown_seq INCREMENT BY -5;

-- Reset to default increment
ALTER SEQUENCE customer_seq INCREMENT BY 1;
```

#### **Complex Alterations**
```sql
-- Multiple operations (separate statements required)
ALTER SEQUENCE invoice_number_seq RESTART WITH 2025001;
ALTER SEQUENCE invoice_number_seq INCREMENT BY 1;

-- Restart hierarchical schema sequence
ALTER SEQUENCE finance.accounting.transaction_seq RESTART WITH 500000;

-- Performance tuning with larger increments
ALTER SEQUENCE high_volume_seq INCREMENT BY 100;
```

### **Legacy SET GENERATOR**

ScratchBird maintains backward compatibility with Firebird's SET GENERATOR syntax.

#### **SET GENERATOR Syntax**
```sql
SET GENERATOR generator_name TO new_value;
```

#### **SET GENERATOR Examples**
```sql
-- Legacy syntax to set generator value
SET GENERATOR customer_id_gen TO 25000;

-- Reset generator to specific value
SET GENERATOR order_id_gen TO 100000;

-- Set generator in schema (if supported)
SET GENERATOR finance.transaction_gen TO 500000;
```

---

## RECREATE SEQUENCE/GENERATOR

Drops and recreates sequence with new definition, preserving dependencies.

### **RECREATE SYNTAX**
```sql
RECREATE {SEQUENCE | GENERATOR} [schema_name.]sequence_name
    [START WITH start_value]
    [INCREMENT [BY] increment_value];
```

### **RECREATE Examples**

#### **Complete Sequence Redefinition**
```sql
-- Recreate with new parameters
RECREATE SEQUENCE customer_seq 
    START WITH 100000 
    INCREMENT BY 10;

-- Recreate generator with different settings
RECREATE GENERATOR batch_gen 
    START WITH 1 
    INCREMENT BY 50;

-- Recreate schema sequence
RECREATE SEQUENCE finance.invoice_seq 
    START WITH 2025001 
    INCREMENT BY 1;
```

---

## DROP SEQUENCE/GENERATOR

Removes sequence definitions from the database. Sequences can only be dropped if no tables or other database objects depend on them.

### **DROP SEQUENCE Syntax**
```sql
DROP {SEQUENCE | GENERATOR} [IF EXISTS] [schema_name.]sequence_name;
```

### **DROP SEQUENCE Examples**

#### **Basic Sequence Removal**
```sql
-- Drop sequence if it exists
DROP SEQUENCE IF EXISTS temp_seq;

-- Drop generator (legacy syntax)
DROP GENERATOR IF EXISTS old_generator;

-- Drop sequence from specific schema
DROP SEQUENCE finance.accounting.ledger_seq;

-- Drop sequence (will fail if in use)
DROP SEQUENCE customer_seq;
```

#### **Dependency Checking**
```sql
-- Check sequence dependencies before dropping
SELECT 
    r.RDB$RELATION_NAME as TABLE_NAME,
    rf.RDB$FIELD_NAME as COLUMN_NAME,
    rf.RDB$GENERATOR_NAME as SEQUENCE_NAME
FROM RDB$RELATION_FIELDS rf
JOIN RDB$RELATIONS r ON rf.RDB$RELATION_NAME = r.RDB$RELATION_NAME  
WHERE rf.RDB$GENERATOR_NAME = 'CUSTOMER_SEQ';

-- Drop sequence after removing dependencies
DROP SEQUENCE customer_seq;
```

---

## Using Sequences in Database Operations

### **Generating Values with NEXT VALUE FOR**

#### **Standard SQL Syntax**
```sql
-- Get next value from sequence
SELECT NEXT VALUE FOR customer_seq FROM RDB$DATABASE;

-- Use in INSERT statement
INSERT INTO customers (customer_id, customer_name)
VALUES (NEXT VALUE FOR customer_seq, 'John Doe');

-- Use in UPDATE statement
UPDATE orders 
SET order_number = NEXT VALUE FOR order_seq 
WHERE order_number IS NULL;
```

#### **Multiple Sequence Usage**
```sql
-- Use multiple sequences in single operation
INSERT INTO transactions (
    transaction_id, 
    batch_id, 
    invoice_id, 
    created_date
) VALUES (
    NEXT VALUE FOR transaction_seq,
    NEXT VALUE FOR batch_seq,
    NEXT VALUE FOR invoice_seq,
    CURRENT_TIMESTAMP
);
```

### **Legacy GEN_ID Function**

#### **GEN_ID Syntax**
```sql
-- Legacy Firebird syntax (still supported)
SELECT GEN_ID(generator_name, step) FROM RDB$DATABASE;
```

#### **GEN_ID Examples**
```sql
-- Get next value (equivalent to NEXT VALUE FOR)
SELECT GEN_ID(customer_id_gen, 1) FROM RDB$DATABASE;

-- Get current value without incrementing
SELECT GEN_ID(customer_id_gen, 0) FROM RDB$DATABASE;

-- Get multiple values at once
SELECT GEN_ID(batch_gen, 10) FROM RDB$DATABASE;  -- Advances by 10

-- Use in table operations
INSERT INTO products (product_id, product_name)
VALUES (GEN_ID(product_id_gen, 1), 'New Product');
```

### **Sequence Integration with Tables**

#### **IDENTITY Columns**
```sql
-- Create table with auto-incrementing IDENTITY column
CREATE TABLE customers (
    customer_id INTEGER GENERATED BY DEFAULT AS IDENTITY (START WITH 1000 INCREMENT BY 1),
    customer_name VARCHAR(100),
    email VARCHAR(255)
);

-- Create table with IDENTITY using existing sequence
CREATE TABLE orders (
    order_id INTEGER GENERATED BY DEFAULT AS IDENTITY (START WITH 1 INCREMENT BY 1),
    customer_id INTEGER,
    order_date DATE DEFAULT CURRENT_DATE
);
```

#### **Default Value Integration**
```sql
-- Use sequence as column default
CREATE TABLE invoices (
    invoice_id INTEGER DEFAULT NEXT VALUE FOR invoice_seq,
    invoice_number VARCHAR(20),
    amount DECIMAL(10,2),
    created_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Insert without specifying ID
INSERT INTO invoices (invoice_number, amount)
VALUES ('INV-2024-001', 1500.00);
-- invoice_id automatically generated from sequence
```

### **Batch Operations with Sequences**

#### **Bulk Insert Operations**
```sql
-- Bulk insert with sequence values
INSERT INTO batch_records (record_id, batch_name, process_date)
SELECT 
    NEXT VALUE FOR record_seq,
    'Batch-' || CAST(NEXT VALUE FOR batch_seq AS VARCHAR(10)),
    CURRENT_DATE
FROM generate_series(1, 1000);  -- Generate 1000 records
```

#### **Data Migration with Sequences**
```sql
-- Migrate data with new sequence values
INSERT INTO new_customers (new_id, old_id, customer_name, email)
SELECT 
    NEXT VALUE FOR new_customer_seq,
    old_customer_id,
    customer_name,
    email
FROM old_customers_table;
```

---

## System Catalog Integration

ScratchBird stores sequence definitions in the RDB$GENERATORS system table.

### **Querying Sequence Information**

#### **List All Sequences**
```sql
-- Show all user-defined sequences
SELECT 
    RDB$GENERATOR_NAME as SEQUENCE_NAME,
    RDB$INITIAL_VALUE as START_VALUE,
    RDB$INCREMENT as INCREMENT_BY,
    RDB$GENERATOR_ID as SEQUENCE_ID,
    RDB$DESCRIPTION as DESCRIPTION,
    RDB$OWNER_NAME as OWNER,
    RDB$SCHEMA_NAME as SCHEMA_NAME
FROM RDB$GENERATORS 
WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0
ORDER BY RDB$GENERATOR_NAME;
```

#### **Current Sequence Values**
```sql
-- Get current values of all sequences
SELECT 
    RDB$GENERATOR_NAME as SEQUENCE_NAME,
    GEN_ID(RDB$GENERATOR_NAME, 0) as CURRENT_VALUE,
    RDB$INCREMENT as INCREMENT_BY
FROM RDB$GENERATORS 
WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0
ORDER BY RDB$GENERATOR_NAME;
```

#### **Sequence Usage Analysis**
```sql
-- Find tables using specific sequences (through IDENTITY columns)
SELECT DISTINCT
    r.RDB$RELATION_NAME as TABLE_NAME,
    rf.RDB$FIELD_NAME as COLUMN_NAME,
    rf.RDB$GENERATOR_NAME as SEQUENCE_NAME,
    rf.RDB$IDENTITY_TYPE as IDENTITY_TYPE
FROM RDB$RELATION_FIELDS rf
JOIN RDB$RELATIONS r ON rf.RDB$RELATION_NAME = r.RDB$RELATION_NAME
WHERE rf.RDB$GENERATOR_NAME IS NOT NULL
ORDER BY rf.RDB$GENERATOR_NAME, r.RDB$RELATION_NAME;
```

#### **Sequence Dependencies**
```sql
-- Comprehensive sequence dependency analysis
SELECT 
    g.RDB$GENERATOR_NAME as SEQUENCE_NAME,
    g.RDB$INITIAL_VALUE as START_VALUE,
    g.RDB$INCREMENT as INCREMENT_BY,
    GEN_ID(g.RDB$GENERATOR_NAME, 0) as CURRENT_VALUE,
    COUNT(rf.RDB$FIELD_NAME) as DEPENDENT_COLUMNS
FROM RDB$GENERATORS g
LEFT JOIN RDB$RELATION_FIELDS rf ON rf.RDB$GENERATOR_NAME = g.RDB$GENERATOR_NAME
WHERE g.RDB$SYSTEM_FLAG IS NULL OR g.RDB$SYSTEM_FLAG = 0
GROUP BY g.RDB$GENERATOR_NAME, g.RDB$INITIAL_VALUE, g.RDB$INCREMENT
ORDER BY g.RDB$GENERATOR_NAME;
```

### **Schema-Aware Sequence Queries**

#### **Sequences by Schema**
```sql
-- List sequences grouped by schema
SELECT 
    COALESCE(RDB$SCHEMA_NAME, 'DEFAULT') as SCHEMA_NAME,
    RDB$GENERATOR_NAME as SEQUENCE_NAME,
    RDB$INITIAL_VALUE as START_VALUE,
    RDB$INCREMENT as INCREMENT_BY,
    GEN_ID(RDB$GENERATOR_NAME, 0) as CURRENT_VALUE
FROM RDB$GENERATORS 
WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0
ORDER BY RDB$SCHEMA_NAME, RDB$GENERATOR_NAME;
```

#### **Hierarchical Schema Sequences**
```sql
-- Find sequences in nested schemas
SELECT 
    RDB$SCHEMA_NAME as SCHEMA_NAME,
    RDB$GENERATOR_NAME as SEQUENCE_NAME,
    CASE 
        WHEN RDB$SCHEMA_NAME LIKE '%.%.%' THEN 'LEVEL_3'
        WHEN RDB$SCHEMA_NAME LIKE '%.%' THEN 'LEVEL_2'  
        WHEN RDB$SCHEMA_NAME IS NOT NULL THEN 'LEVEL_1'
        ELSE 'DEFAULT'
    END as SCHEMA_LEVEL,
    RDB$INCREMENT as INCREMENT_BY
FROM RDB$GENERATORS
WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0
  AND RDB$SCHEMA_NAME IS NOT NULL
ORDER BY SCHEMA_LEVEL, RDB$SCHEMA_NAME, RDB$GENERATOR_NAME;
```

---

## Advanced Sequence Features

### **Sequence Caching and Performance**

#### **High-Performance Sequence Usage**
```sql
-- Efficient sequence value generation for high-volume operations
BEGIN
    -- Cache multiple values for batch processing
    DECLARE VARIABLE start_id BIGINT;
    DECLARE VARIABLE end_id BIGINT;
    
    -- Get block of 1000 sequence values
    SELECT GEN_ID(high_volume_seq, 1000) FROM RDB$DATABASE INTO :end_id;
    start_id = :end_id - 999;
    
    -- Use cached range for batch operations
    -- (Implementation in application logic)
END
```

#### **Sequence Value Reservation**
```sql
-- Reserve sequence values for specific operations
CREATE PROCEDURE reserve_sequence_block(
    sequence_name VARCHAR(63),
    block_size INTEGER
) RETURNS (
    start_value BIGINT,
    end_value BIGINT
)
AS
BEGIN
    -- Reserve a block of sequence values
    EXECUTE STATEMENT 'SELECT GEN_ID(' || sequence_name || ', ?) FROM RDB$DATABASE'
        (block_size) INTO end_value;
    start_value = end_value - block_size + 1;
    SUSPEND;
END
```

### **Sequence Monitoring and Administration**

#### **Sequence Value Monitoring**
```sql
-- Monitor sequence value consumption
CREATE VIEW sequence_status AS
SELECT 
    g.RDB$GENERATOR_NAME as SEQUENCE_NAME,
    g.RDB$INITIAL_VALUE as START_VALUE,
    GEN_ID(g.RDB$GENERATOR_NAME, 0) as CURRENT_VALUE,
    g.RDB$INCREMENT as INCREMENT_BY,
    CASE 
        WHEN g.RDB$INCREMENT > 0 THEN 
            (9223372036854775807 - GEN_ID(g.RDB$GENERATOR_NAME, 0)) / g.RDB$INCREMENT
        WHEN g.RDB$INCREMENT < 0 THEN 
            (GEN_ID(g.RDB$GENERATOR_NAME, 0) - (-9223372036854775808)) / ABS(g.RDB$INCREMENT)
        ELSE NULL
    END as REMAINING_VALUES
FROM RDB$GENERATORS g
WHERE g.RDB$SYSTEM_FLAG IS NULL OR g.RDB$SYSTEM_FLAG = 0;
```

#### **Sequence Health Check**
```sql
-- Check for sequences approaching limits
SELECT 
    SEQUENCE_NAME,
    CURRENT_VALUE,
    INCREMENT_BY,
    REMAINING_VALUES,
    CASE 
        WHEN REMAINING_VALUES < 1000 THEN 'CRITICAL'
        WHEN REMAINING_VALUES < 10000 THEN 'WARNING'
        ELSE 'OK'
    END as STATUS
FROM sequence_status
WHERE REMAINING_VALUES IS NOT NULL
ORDER BY REMAINING_VALUES ASC;
```

### **Sequence Synchronization**

#### **Cross-Database Sequence Coordination**
```sql
-- Synchronize sequence values across databases
CREATE PROCEDURE sync_sequence_values(
    source_db VARCHAR(255),
    target_sequence VARCHAR(63)
)
AS
DECLARE VARIABLE source_value BIGINT;
BEGIN
    -- Get current value from source database
    EXECUTE STATEMENT 
        'SELECT GEN_ID(' || target_sequence || ', 0) FROM RDB$DATABASE'
        ON EXTERNAL source_db
        INTO source_value;
    
    -- Set target sequence to match source
    EXECUTE STATEMENT 
        'ALTER SEQUENCE ' || target_sequence || ' RESTART WITH ' || source_value;
END
```

#### **Sequence Gap Analysis**
```sql
-- Identify gaps in sequence usage
CREATE PROCEDURE find_sequence_gaps(
    table_name VARCHAR(63),
    id_column VARCHAR(63)
)
RETURNS (
    gap_start INTEGER,
    gap_end INTEGER
)
AS
DECLARE VARIABLE current_id INTEGER;
DECLARE VARIABLE next_id INTEGER;
BEGIN
    FOR EXECUTE STATEMENT 
        'SELECT ' || id_column || ' FROM ' || table_name || ' ORDER BY ' || id_column
        INTO current_id
    DO BEGIN
        IF (next_id IS NOT NULL AND current_id > next_id + 1) THEN
        BEGIN
            gap_start = next_id + 1;
            gap_end = current_id - 1;
            SUSPEND;
        END
        next_id = current_id;
    END
END
```

---

## Performance Considerations

### **Sequence Performance Characteristics**

1. **Lock-Free Generation**: Sequences use minimal locking for high concurrency
2. **Cache Efficiency**: Frequently used sequences are cached in memory
3. **Transaction Independence**: Sequence values are not rolled back
4. **Minimal Overhead**: Very low CPU and memory overhead per value generation
5. **Schema Resolution**: 3-level qualified sequences have negligible resolution overhead

### **Optimization Strategies**

#### **High-Volume Applications**
```sql
-- Use larger increments for bulk processing
CREATE SEQUENCE bulk_process_seq 
    START WITH 1000000 
    INCREMENT BY 1000;

-- Application reserves blocks and manages internally
DECLARE VARIABLE block_start BIGINT;
DECLARE VARIABLE block_end BIGINT;

-- Get 1000 values at once
SELECT GEN_ID(bulk_process_seq, 1000) FROM RDB$DATABASE INTO block_end;
block_start = block_end - 999;
```

#### **Distributed Systems**
```sql
-- Use different starting values per node to avoid conflicts
CREATE SEQUENCE node1_seq START WITH 1000000 INCREMENT BY 1;  -- Node 1
CREATE SEQUENCE node2_seq START WITH 2000000 INCREMENT BY 1;  -- Node 2  
CREATE SEQUENCE node3_seq START WITH 3000000 INCREMENT BY 1;  -- Node 3

-- Or use increment to separate values
CREATE SEQUENCE distributed_seq START WITH 1 INCREMENT BY 10;
-- Node 1 uses: 1, 11, 21, 31...
-- Node 2 uses: 2, 12, 22, 32...
-- Node 3 uses: 3, 13, 23, 33...
```

#### **Index-Friendly Sequences**
```sql
-- Sequential values optimize B-tree index performance
CREATE SEQUENCE order_id_seq START WITH 1 INCREMENT BY 1;

-- Random-like values may reduce index hot spots but hurt range queries
CREATE SEQUENCE random_like_seq START WITH 1000000 INCREMENT BY 7919;
```

---

## Error Handling and Troubleshooting

### **Common Sequence Errors**

#### **Sequence Creation Errors**
```sql
-- Error: Sequence already exists
CREATE SEQUENCE customer_seq;
-- Solution: Use IF NOT EXISTS or OR REPLACE

-- Error: Invalid increment value
CREATE SEQUENCE bad_seq INCREMENT BY 0;
-- Solution: Use non-zero increment value

-- Error: Invalid starting value
CREATE SEQUENCE overflow_seq START WITH 9223372036854775808;
-- Solution: Use value within BIGINT range
```

#### **Sequence Usage Errors**
```sql
-- Error: Sequence not found
SELECT NEXT VALUE FOR nonexistent_seq FROM RDB$DATABASE;
-- Solution: Verify sequence name and schema

-- Error: Sequence overflow
-- Sequence reaches maximum BIGINT value
-- Solution: Restart sequence or use negative increment
```

### **Debugging Sequence Issues**

#### **Sequence Value Tracking**
```sql
-- Check current sequence value
SELECT GEN_ID(customer_seq, 0) FROM RDB$DATABASE;

-- Check sequence definition
SELECT * FROM RDB$GENERATORS WHERE RDB$GENERATOR_NAME = 'CUSTOMER_SEQ';

-- Test sequence increment
SELECT 
    GEN_ID(customer_seq, 0) as BEFORE_VALUE,
    GEN_ID(customer_seq, 1) as AFTER_VALUE,
    GEN_ID(customer_seq, 0) as CURRENT_VALUE
FROM RDB$DATABASE;
```

#### **Performance Issues**
```sql
-- Identify heavily used sequences
SELECT 
    RDB$GENERATOR_NAME,
    GEN_ID(RDB$GENERATOR_NAME, 0) as CURRENT_VALUE,
    RDB$INCREMENT
FROM RDB$GENERATORS 
WHERE GEN_ID(RDB$GENERATOR_NAME, 0) > 1000000  -- High usage threshold
ORDER BY GEN_ID(RDB$GENERATOR_NAME, 0) DESC;
```

### **Sequence Recovery Procedures**

#### **Sequence Value Correction**
```sql
-- Reset sequence to match table data
DECLARE VARIABLE max_id INTEGER;

-- Find maximum ID in table
SELECT MAX(customer_id) FROM customers INTO max_id;

-- Reset sequence to next available value
ALTER SEQUENCE customer_seq RESTART WITH (:max_id + 1);
```

#### **Sequence Consistency Check**
```sql
-- Verify sequence consistency with table data
SELECT 
    'customer_seq' as SEQUENCE_NAME,
    GEN_ID(customer_seq, 0) as SEQUENCE_VALUE,
    MAX(customer_id) as MAX_TABLE_VALUE,
    CASE 
        WHEN GEN_ID(customer_seq, 0) <= MAX(customer_id) THEN 'INCONSISTENT'
        ELSE 'OK'
    END as STATUS
FROM customers;
```

---

## Best Practices

### **Sequence Design Guidelines**

1. **Naming Conventions**: Use descriptive names ending with `_seq` or `_gen`
2. **Starting Values**: Choose appropriate starting values to avoid conflicts
3. **Increment Planning**: Consider business requirements for increment values
4. **Schema Organization**: Use hierarchical schemas for sequence categorization
5. **Documentation**: Comment sequences for future maintenance understanding

### **Recommended Sequence Patterns**

#### **Standard Business Sequences**
```sql
-- Primary key sequences
CREATE SEQUENCE customer_seq START WITH 1000 INCREMENT BY 1;
CREATE SEQUENCE order_seq START WITH 100000 INCREMENT BY 1;
CREATE SEQUENCE product_seq START WITH 10000 INCREMENT BY 1;

-- Date-based sequences (reset externally by application)
CREATE SEQUENCE daily_batch_seq START WITH 1 INCREMENT BY 1;
CREATE SEQUENCE monthly_report_seq START WITH 1 INCREMENT BY 1;

-- Department-specific sequences
CREATE SEQUENCE finance.invoice_seq START WITH 2024001 INCREMENT BY 1;
CREATE SEQUENCE hr.employee_seq START WITH 10000 INCREMENT BY 1;
CREATE SEQUENCE inventory.item_seq START WITH 100000 INCREMENT BY 1;
```

#### **Performance-Optimized Sequences**
```sql
-- High-volume transaction sequence
CREATE SEQUENCE high_volume_txn_seq START WITH 1 INCREMENT BY 1;

-- Bulk processing with larger increments
CREATE SEQUENCE bulk_batch_seq START WITH 1000000 INCREMENT BY 1000;

-- Distributed processing sequences
CREATE SEQUENCE node1_seq START WITH 1000000 INCREMENT BY 1;
CREATE SEQUENCE node2_seq START WITH 2000000 INCREMENT BY 1;
```

---

## Migration from Other Database Systems

### **From PostgreSQL**
PostgreSQL sequences translate directly to ScratchBird:
```sql
-- PostgreSQL syntax
CREATE SEQUENCE customer_id_seq START 1000 INCREMENT 1;

-- ScratchBird equivalent (identical)
CREATE SEQUENCE customer_id_seq START WITH 1000 INCREMENT BY 1;
```

### **From Oracle**
Oracle sequences have more options, but basic functionality translates:
```sql
-- Oracle syntax
CREATE SEQUENCE customer_seq START WITH 1000 INCREMENT BY 1 CACHE 20;

-- ScratchBird equivalent (caching handled automatically)
CREATE SEQUENCE customer_seq START WITH 1000 INCREMENT BY 1;
```

### **From SQL Server**
SQL Server IDENTITY columns can be replaced with sequences:
```sql
-- SQL Server IDENTITY column
CREATE TABLE customers (
    customer_id int IDENTITY(1000,1) PRIMARY KEY,
    customer_name varchar(100)
);

-- ScratchBird equivalent with sequence
CREATE SEQUENCE customer_seq START WITH 1000 INCREMENT BY 1;

CREATE TABLE customers (
    customer_id INTEGER GENERATED BY DEFAULT AS IDENTITY (START WITH 1000 INCREMENT BY 1),
    customer_name VARCHAR(100)
);
```

---

## Implementation Details

### **Primary Implementation Files**

#### **Parser and Grammar**
- **File**: `src/dsql/parse.y:2259-2386`
- **Classes**: `generator_clause`, `create_sequence_options`, `alter_sequence_options`
- **Functionality**: Parsing CREATE/ALTER/DROP SEQUENCE/GENERATOR syntax

#### **DDL Node Classes**
- **File**: `src/dsql/DdlNodes.h:1169-1282`
- **Classes**:
  - `CreateAlterSequenceNode` (lines 1169-1235): Sequence creation and modification
  - `DropSequenceNode` (lines 1238-1266): Sequence removal with dependency checking
  - `RecreateSequenceNode` (line 1282): RECREATE SEQUENCE implementation

#### **System Catalog Integration**
- **File**: `src/jrd/relations.h:354-364`
- **Table**: `RDB$GENERATORS` - Stores sequence definitions
- **Fields**:
  - `f_gen_name`: Sequence/generator name
  - `f_gen_id`: Internal sequence ID
  - `f_gen_init_val`: Initial/starting value
  - `f_gen_increment`: Increment step value
  - `f_gen_schema`: Schema name for hierarchical support

#### **Value Generation Functions**
- **File**: `src/dsql/parse.y:10153-10164`
- **Functions**: 
  - `NEXT VALUE FOR` - SQL standard syntax
  - `GEN_ID()` - Legacy Firebird function
- **Integration**: GenIdNode class for value generation

### **Core Classes and Functions**

#### **CreateAlterSequenceNode Methods**
- `execute()`: Creates or modifies sequence in system catalog
- Handles both CREATE and ALTER operations in single class
- Supports legacy SET GENERATOR syntax
- Schema qualification and validation

#### **DropSequenceNode Methods**
- `deleteIdentity()`: Removes IDENTITY column associations
- Dependency checking before sequence removal
- Cascade deletion of related metadata

#### **Value Generation**
- `GenIdNode`: Implements both NEXT VALUE FOR and GEN_ID()
- Thread-safe value generation with minimal locking
- Transaction-independent value allocation
- High-performance caching mechanisms

### **Storage Structures**

Sequences are stored in the RDB$GENERATORS system table with:

- **Name Storage**: Sequence names with optional schema qualification
- **Value Management**: Current value, initial value, increment step
- **Metadata**: System flags, ownership, description
- **Schema Integration**: Schema name for hierarchical organization
- **Performance Data**: Cached values and generation statistics

---

## Administrative Operations

### **Sequence Backup and Restore**

#### **Backup Considerations**
- Sequence definitions are included in database backups as DDL
- Current sequence values are preserved during backup/restore
- Schema-qualified sequences maintain hierarchy during restore
- Dependencies on IDENTITY columns are preserved

#### **Restore Procedures**
```sql
-- Verify sequence values after restore
SELECT 
    RDB$GENERATOR_NAME,
    GEN_ID(RDB$GENERATOR_NAME, 0) as CURRENT_VALUE
FROM RDB$GENERATORS 
WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0;

-- Adjust sequences if needed after data restore
ALTER SEQUENCE customer_seq RESTART WITH 50001;
```

### **Sequence Maintenance**

#### **Regular Maintenance Tasks**
```sql
-- Monitor sequence value consumption
CREATE PROCEDURE sequence_maintenance
AS
BEGIN
    -- Log current sequence states
    INSERT INTO sequence_audit_log (
        log_date, 
        sequence_name, 
        current_value, 
        increment_by
    )
    SELECT 
        CURRENT_TIMESTAMP,
        RDB$GENERATOR_NAME,
        GEN_ID(RDB$GENERATOR_NAME, 0),
        RDB$INCREMENT
    FROM RDB$GENERATORS 
    WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0;
END
```

---

