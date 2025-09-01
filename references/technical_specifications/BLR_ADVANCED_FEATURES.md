# BLR (Binary Language Representation) for Advanced Features

## Overview

This document specifies how all advanced ScratchBird features are efficiently represented in BLR (Binary Language Representation), ensuring optimal execution while maintaining all capabilities.

## BLR Instruction Set Extensions

### Core BLR Structure

```
BLR Version: 3.0 (ScratchBird Extended)
Header: [Magic: 4 bytes]['BLR3'][Version: 2 bytes][Flags: 4 bytes]

Instruction Format:
[Opcode: 1-2 bytes][Operands: variable]

Extended Opcodes (0xFF prefix for 2-byte opcodes)
```

## SELECT INTO and Iteration BLR

### SELECT INTO Single Row

```
SQL:
SELECT name, email INTO @name, @email 
FROM customers WHERE id = @id;

BLR:
BLR_SELECT_INTO
  BLR_TARGETS
    BLR_VARIABLE @name
    BLR_VARIABLE @email
  BLR_RSE  ; Record Selection Expression
    BLR_RELATION customers
    BLR_BOOLEAN
      BLR_EQL
        BLR_FIELD customers.id
        BLR_VARIABLE @id
    BLR_PROJECT
      BLR_FIELD customers.name
      BLR_FIELD customers.email
  BLR_SINGLETON  ; Expect single row
```

### FOR SELECT Loop with WHERE CURRENT OF

```
SQL:
FOR SELECT id, amount FROM orders 
  FOR UPDATE INTO @id, @amount
DO
  UPDATE orders SET processed = TRUE WHERE CURRENT OF;

BLR:
BLR_FOR_SELECT
  BLR_FLAGS [UPDATABLE, LOCK_ROWS]
  BLR_CURSOR_ID 0x01  ; Internal cursor ID
  BLR_RSE
    BLR_RELATION orders
    BLR_PROJECT
      BLR_FIELD orders.id
      BLR_FIELD orders.amount
  BLR_INTO
    BLR_VARIABLE @id
    BLR_VARIABLE @amount
  BLR_LOOP_BODY
    BLR_UPDATE_CURRENT
      BLR_CURSOR_REF 0x01
      BLR_MODIFY
        BLR_FIELD orders.processed
        BLR_LITERAL TRUE
```

### SET Conversion with Bidirectional Cursor

```
SQL:
SET @result_set = (SELECT * FROM table)::SET;
DECLARE @cursor CURSOR SCROLL FOR @result_set;
FETCH LAST FROM @cursor INTO @record;

BLR:
BLR_SET_CONVERT
  BLR_TARGET @result_set
  BLR_FLAGS [MATERIALIZED, INDEXED]
  BLR_RSE
    BLR_RELATION table
    BLR_PROJECT_ALL
  
BLR_DECLARE_CURSOR
  BLR_CURSOR_ID 0x02
  BLR_FLAGS [SCROLLABLE, SENSITIVE]
  BLR_SET_SOURCE @result_set

BLR_FETCH
  BLR_CURSOR_REF 0x02
  BLR_DIRECTION LAST
  BLR_INTO @record
```

## Domain and Type System BLR

### Record Domain Operations

```
SQL:
CREATE DOMAIN person AS RECORD (
  first_name VARCHAR(50),
  last_name VARCHAR(50)
);
EXTRACT(first_name FROM @person);

BLR:
BLR_DOMAIN_RECORD
  BLR_DOMAIN_ID <UUID>
  BLR_FIELD_COUNT 2
  BLR_FIELD_DEF
    BLR_NAME first_name
    BLR_TYPE VARCHAR
    BLR_LENGTH 50
  BLR_FIELD_DEF
    BLR_NAME last_name
    BLR_TYPE VARCHAR
    BLR_LENGTH 50

BLR_EXTRACT_FIELD
  BLR_RECORD_VAR @person
  BLR_FIELD_INDEX 0  ; Optimized to index at compile time
```

### Enum with Positional Arithmetic

```
SQL:
SET NEXT VALUE FOR @day_enum;
GET POSITION FOR @day_enum;

BLR:
BLR_ENUM_ADVANCE
  BLR_VARIABLE @day_enum
  BLR_DIRECTION NEXT
  BLR_WRAP_MODE TRUE

BLR_ENUM_POSITION
  BLR_VARIABLE @day_enum
  BLR_RESULT_TYPE INT32
```

### Polymorphic Variable Handling

```
SQL:
DECLARE @data VARIANT;
IF EXTRACT(DATATYPE FROM @data) = 'INTEGER' THEN

BLR:
BLR_VARIANT_DECLARE @data

BLR_VARIANT_TYPE_CHECK
  BLR_VARIABLE @data
  BLR_COMPARE_TYPE INTEGER
  BLR_BRANCH_TRUE label_1
```

## UUID Identity and Advanced Types

### UUID Generation in BLR

```
SQL:
id UUID GENERATED ALWAYS AS IDENTITY (UUID VERSION 7)

BLR:
BLR_UUID_GEN
  BLR_UUID_VERSION 7
  BLR_UUID_FLAGS [MONOTONIC, TIME_ORDERED]
  BLR_NODE_ID 42  ; If specified
  BLR_TARGET_FIELD id
```

### 128-bit Integer Operations

```
SQL:
DECLARE @big INT128;
SET @big = @big + 1;

BLR:
BLR_INT128_DECLARE @big

BLR_INT128_ADD
  BLR_INT128_VAR @big
  BLR_INT128_LITERAL 1
  BLR_INT128_RESULT @big
```

## Exception Handling BLR

### TRY/EXCEPT Blocks

```
SQL:
TRY
  PERFORM risky_op();
EXCEPT WHEN custom_error THEN
  SET @error = GET EXCEPTION_INFO;
END TRY;

BLR:
BLR_TRY_BLOCK
  BLR_TRY_ID 0x01
  BLR_PROTECTED_BLOCK
    BLR_INVOKE risky_op
  BLR_EXCEPT_HANDLERS
    BLR_EXCEPT_WHEN
      BLR_EXCEPTION_NAME custom_error
      BLR_HANDLER_BLOCK
        BLR_GET_EXCEPTION_INFO
        BLR_STORE_VARIABLE @error
  BLR_END_TRY
```

### Custom Exception Definition

```
SQL:
CREATE EXCEPTION payment_failed (amount MONEY)
WITH MESSAGE TEMPLATE 'Payment of {amount} failed';

BLR:
BLR_EXCEPTION_DEF
  BLR_EXCEPTION_ID <UUID>
  BLR_EXCEPTION_NAME payment_failed
  BLR_PARAMETERS
    BLR_PARAM amount BLR_TYPE_MONEY
  BLR_MESSAGE_TEMPLATE
    BLR_STRING 'Payment of {amount} failed'
    BLR_SUBSTITUTION_COUNT 1
    BLR_SUBSTITUTION_MAP
      BLR_INDEX 0 BLR_PARAM_REF amount
```

## Trigger Context Variables BLR

### Trigger Event Detection

```
SQL:
SET @event = GET TRIGGER_EVENT;
IF IS COLUMN CHANGED(price) THEN

BLR:
BLR_TRIGGER_GET_EVENT
  BLR_STORE_VARIABLE @event

BLR_TRIGGER_COLUMN_CHANGED
  BLR_COLUMN_INDEX 3  ; Compile-time resolution
  BLR_BRANCH_TRUE label_changed
```

### NEW/OLD Row Access

```
SQL:
IF NEW.price != OLD.price THEN

BLR:
BLR_TRIGGER_NEW_VALUE
  BLR_FIELD_INDEX 3
  BLR_PUSH_STACK

BLR_TRIGGER_OLD_VALUE
  BLR_FIELD_INDEX 3
  BLR_PUSH_STACK

BLR_COMPARE_NEQ
BLR_BRANCH_TRUE label_different
```

## EXECUTE BLOCK and Autonomous Transactions

### EXECUTE BLOCK with Parameters

```
SQL:
EXECUTE BLOCK (start_date DATE = ?)
AS BEGIN ... END;

BLR:
BLR_EXECUTE_BLOCK
  BLR_BLOCK_ID <UUID>
  BLR_PARAMETERS
    BLR_PARAM start_date BLR_TYPE_DATE
  BLR_BLOCK_BODY
    ; Block instructions
  BLR_END_BLOCK
```

### Autonomous Transaction

```
SQL:
EXECUTE BLOCK WITH AUTONOMOUS TRANSACTION
AS BEGIN
  ON COMMIT DO INSERT INTO log VALUES (...);
END;

BLR:
BLR_AUTONOMOUS_BLOCK
  BLR_TRANSACTION_ID <new_id>
  BLR_ISOLATION_LEVEL READ_COMMITTED
  BLR_TRIGGER_DEFINITIONS
    BLR_ON_COMMIT
      BLR_INSERT
        BLR_RELATION log
        ; values
  BLR_BLOCK_BODY
    ; Main logic
  BLR_END_AUTONOMOUS
```

## CTE Materialization and Indexing

### Materialized CTE with Index

```
SQL:
WITH data AS MATERIALIZED (
  SELECT * FROM large_table
) INDEXED BY (id, status)
SELECT * FROM data WHERE status = 'ACTIVE';

BLR:
BLR_CTE_MATERIALIZED
  BLR_CTE_ID 0x01
  BLR_STORAGE_FLAGS [IN_MEMORY, COMPRESSED]
  BLR_RSE
    BLR_RELATION large_table
    BLR_PROJECT_ALL
  BLR_CREATE_INDEXES
    BLR_INDEX_BTREE
      BLR_INDEX_COLUMNS [id, status]
      BLR_INDEX_FLAGS [UNIQUE, CLUSTERED]
  
BLR_SELECT
  BLR_CTE_REF 0x01
  BLR_BOOLEAN
    BLR_EQL
      BLR_FIELD status
      BLR_LITERAL 'ACTIVE'
  BLR_USE_INDEX  ; Optimizer hint
```

## Schema Permissions and Role Composition

### Hierarchical Permission Check

```
SQL:
GRANT SELECT ON SCHEMA sales TO developer
  WITH INHERITANCE CASCADE;

BLR:
BLR_GRANT_SCHEMA
  BLR_PERMISSION SELECT
  BLR_SCHEMA_UUID <sales_uuid>
  BLR_GRANTEE_UUID <developer_uuid>
  BLR_INHERITANCE CASCADE
  BLR_PERMISSION_CACHE_INVALIDATE
```

### Role Composition

```
SQL:
GRANT read_only TO developer;

BLR:
BLR_GRANT_ROLE
  BLR_ROLE_UUID <read_only_uuid>
  BLR_TO_ROLE_UUID <developer_uuid>
  BLR_PROPAGATE_PERMISSIONS
```

## Optimization Flags and Hints

### BLR Optimization Metadata

```
BLR_OPTIMIZATION_BLOCK
  BLR_ESTIMATED_ROWS 10000
  BLR_ESTIMATED_COST 500
  BLR_PREFERRED_JOIN_ORDER [t1, t2, t3]
  BLR_INDEX_HINTS
    BLR_USE_INDEX idx_1 ON t1
    BLR_IGNORE_INDEX idx_2 ON t2
  BLR_PARALLEL_DEGREE 4
  BLR_MEMORY_LIMIT 100MB
```

### Query Plan Feedback

```
BLR_PLAN_FEEDBACK
  BLR_ACTUAL_ROWS 12543
  BLR_ACTUAL_TIME 234ms
  BLR_MEMORY_USED 45MB
  BLR_SPILL_TO_DISK FALSE
  BLR_CACHE_HITS 95%
```

## BLR Compression and Storage

### Compressed BLR Format

```
BLR_COMPRESSED_BLOCK
  BLR_COMPRESSION_TYPE ZSTD
  BLR_UNCOMPRESSED_SIZE 10240
  BLR_COMPRESSED_SIZE 2048
  BLR_DICTIONARY_ID <uuid>  ; Shared dictionary
  BLR_COMPRESSED_DATA [...]
```

### BLR Caching

```
BLR_CACHE_ENTRY
  BLR_CACHE_KEY <hash>
  BLR_CACHE_VERSION 1
  BLR_CACHE_TIMESTAMP
  BLR_CACHE_STATISTICS
    BLR_HIT_COUNT 1000
    BLR_AVG_EXECUTION_TIME 10ms
  BLR_COMPILED_PLAN [...]
```

## Advanced Cursor Operations in BLR

### Scrollable Cursor with Positioned Updates

```
SQL:
DECLARE @cursor CURSOR SCROLL FOR @set;
FETCH ABSOLUTE 10 FROM @cursor INTO @record;
UPDATE table SET col = value WHERE CURRENT OF @cursor;

BLR:
BLR_CURSOR_DECLARE
  BLR_CURSOR_ID 0x03
  BLR_FLAGS [SCROLLABLE, UPDATABLE, SENSITIVE]
  BLR_SET_SOURCE @set

BLR_FETCH_ABSOLUTE
  BLR_CURSOR_REF 0x03
  BLR_POSITION 10
  BLR_INTO @record

BLR_UPDATE_POSITIONED
  BLR_CURSOR_REF 0x03
  BLR_RELATION table
  BLR_MODIFY
    BLR_FIELD col
    BLR_VALUE value
```

### Bulk Fetch Operations

```
SQL:
FETCH FORWARD 100 FROM @cursor BULK COLLECT INTO @array;

BLR:
BLR_BULK_FETCH
  BLR_CURSOR_REF 0x04
  BLR_DIRECTION FORWARD
  BLR_COUNT 100
  BLR_TARGET_ARRAY @array
  BLR_FLAGS [PREFETCH, ASYNC_CAPABLE]
```

## BLR Execution Modes

### Interpreted vs Compiled

```
BLR_EXECUTION_MODE
  BLR_MODE COMPILED  ; or INTERPRETED
  BLR_JIT_FLAGS [AGGRESSIVE, PROFILE_GUIDED]
  BLR_NATIVE_CODE_PTR 0x...  ; If compiled
```

### Parallel Execution

```
BLR_PARALLEL_SCAN
  BLR_WORKER_COUNT 4
  BLR_PARTITION_METHOD HASH
  BLR_MERGE_METHOD SORTED
  BLR_WORK_DISTRIBUTION
    BLR_WORKER_0 [ROWS 0-2500]
    BLR_WORKER_1 [ROWS 2501-5000]
    ; ...
```

## BLR Validation and Security

### BLR Signature

```
BLR_SIGNATURE_BLOCK
  BLR_HASH_TYPE SHA256
  BLR_HASH_VALUE [32 bytes]
  BLR_SIGNED_BY <user_uuid>
  BLR_TIMESTAMP
  BLR_PERMISSIONS_SNAPSHOT  ; Permissions at compile time
```

### BLR Access Control

```
BLR_ACCESS_CHECK
  BLR_REQUIRED_PERMISSIONS [SELECT, UPDATE]
  BLR_SCHEMA_PERMISSIONS [...]
  BLR_ROLE_PERMISSIONS [...]
  BLR_FAIL_MODE EXCEPTION  ; or SKIP_ROW
```

## Performance Considerations

### BLR Instruction Optimization

1. **Constant Folding**: Pre-compute literals at compile time
2. **Dead Code Elimination**: Remove unreachable BLR blocks
3. **Common Subexpression Elimination**: Reuse computed values
4. **Index Resolution**: Convert names to indices at compile time
5. **Type Specialization**: Generate type-specific instructions

### Memory Layout

```
BLR Memory Structure:
[Header: 16 bytes]
[Constant Pool: variable]
[Instruction Stream: variable]
[Metadata: variable]
[Debug Info: optional]
```

### BLR Streaming

```
BLR_STREAM_HEADER
  BLR_CHUNK_SIZE 4096
  BLR_TOTAL_CHUNKS 10
  BLR_CHECKSUM_TYPE CRC32

BLR_STREAM_CHUNK
  BLR_CHUNK_ID 1
  BLR_CHUNK_DATA [...]
  BLR_CHUNK_CHECKSUM
```

This comprehensive BLR specification ensures all advanced features are efficiently represented in binary form for optimal execution!