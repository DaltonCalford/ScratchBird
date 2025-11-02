# Row Identity & Data Transfer - Enhanced Design

**Date**: 2025-10-24
**Version**: 2.1 (Enhanced with Context Variables)
**Status**: DESIGN - Enhanced
**Priority**: HIGH (Required for ALPHA)
**Estimated Effort**: 44-64 hours
**Related Documents**:
- `ALPHA_CONTEXT_VARIABLES_DESIGN.md` - Context variables implementation (Firebird Chapter 12 pattern)
- `ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md` - Original design (v1.0)

---

## Document Updates

**Version 2.1** (2025-10-24):
- Added reference to `ALPHA_CONTEXT_VARIABLES_DESIGN.md`
- Context variables follow Firebird Chapter 12 pattern
- `sdb$key`, `rdb$row_uuid`, `rdb$xact_id` now part of broader context variable system
- Date/time context variables (CURRENT_DATE, CURRENT_TIMESTAMP, etc.)
- Trigger context variables (INSERTING, UPDATING, DELETING, NEW, OLD)

**Version 2.0** (2025-10-23):
- Added `sdb$key` physical row identifier
- Added `TRANSFER` command for data copy/move
- Added hash indexes on `sdb$key`
- Enhanced with user feedback

---

## Executive Summary

This document extends the row identity design with:

1. **`sdb$key`** - Physical row location identifier (like Firebird's `rdb$db_key`)
2. **`rdb$row_uuid`** - Permanent logical row identifier (UUID v7)
3. **`rdb$xact_id`** - Transaction visibility metadata
4. **`TRANSFER`** - Bulk data copy/move command
5. **Hash indexes on `sdb$key`** - Fast positional lookups

**Key Innovation**: Hybrid approach gives users choice between fast physical access and permanent identity.

---

## Feature Summary Table

| Feature | Type | Purpose | Stability | Storage | Hidden |
|---------|------|---------|-----------|---------|--------|
| **`sdb$key`** | Physical | Row location | Changes on migration | Derived (0 bytes) | Yes |
| **`rdb$row_uuid`** | Logical | Permanent ID | Never changes | 16 bytes | Yes |
| **`rdb$xact_id`** | Metadata | Last transaction | Dynamic | 0 bytes (xmin) | Yes |
| **`txn_current()`** | Function | Current txn | N/A | N/A | No |
| **`TRANSFER`** | Command | Data copy/move | N/A | N/A | No |

---

## Part 1: `sdb$key` - Physical Row Identifier

### Design: `sdb$key` Concept

**Purpose**: Fast physical row location access (similar to Firebird's `rdb$db_key`).

**Structure**:
```cpp
// Derived from TID (no storage overhead)
struct SDB_KEY {
    uint16_t tablespace_id;  // Tablespace identifier
    uint64_t page_id;        // Page within tablespace (48 bits used)
    uint16_t slot_id;        // Slot within page

    // Total: 12 bytes (compact)
};

// Convert TID to sdb$key
SDB_KEY tid_to_sdb_key(const TID& tid) {
    return SDB_KEY{
        .tablespace_id = extractTablespaceID(tid.gpid),
        .page_id = extractPageID(tid.gpid),
        .slot_id = tid.slot_id
    };
}
```

**Properties**:
- **Derived**: Computed from TID (no storage overhead)
- **Compact**: 12 bytes (vs 16 bytes for UUID)
- **Fast**: Integer comparison (no string/UUID comparison)
- **Unstable**: Changes on table migration, backup/restore
- **Hidden**: Not included in `SELECT *`

---

### SQL Access to `sdb$key`

```sql
-- Explicit select
SELECT sdb$key, name, email FROM users;
-- Result: '0001-00000042-0003', 'Alice', 'alice@example.com'

-- Fast positional update
UPDATE users SET status = 'active' WHERE sdb$key = '0001-00000042-0003';

-- Fast positional delete
DELETE FROM users WHERE sdb$key = '0001-00000042-0003';

-- Use in cursor positioning (internal)
FETCH ABSOLUTE '0001-00000042-0003' FROM cursor;
```

**Format**: `TTTT-PPPPPPPPPPPP-SSSS` (hex)
- `TTTT` = Tablespace ID (4 hex digits, 16 bits)
- `PPPPPPPPPPPP` = Page ID (12 hex digits, 48 bits)
- `SSSS` = Slot ID (4 hex digits, 16 bits)

**Example**: `0001-00000042-0003` = Tablespace 1, Page 66, Slot 3

---

### Implementation: `sdb$key` Pseudo-Column

#### Step 1: Virtual Column in Catalog

```cpp
// Add virtual column to system catalog
void CatalogManager::addSDBKeyColumn(TableInfo& table) {
    ColumnInfo col;
    col.column_name = "sdb$key";
    col.column_type = DataType::VARCHAR;  // 19-char string
    col.is_virtual = true;
    col.is_hidden = true;  // Not in SELECT *
    table.addVirtualColumn(col);
}
```

#### Step 2: Column Evaluation in Executor

```cpp
// src/sblr/executor.cpp

Value Executor::evaluateColumnRef(const ColumnRef& col_ref, const Tuple& tuple) {
    if (col_ref.column_name == "sdb$key") {
        // Get TID from tuple
        const TID& tid = tuple.getTID();

        // Format as string: "TTTT-PPPPPPPPPPPP-SSSS"
        char buf[20];
        uint16_t tablespace_id = extractTablespaceID(tid.gpid);
        uint64_t page_id = extractPageID(tid.gpid);
        uint16_t slot_id = tid.slot_id;

        snprintf(buf, sizeof(buf), "%04x-%012llx-%04x",
                 tablespace_id, page_id, slot_id);

        return Value::makeVarchar(std::string(buf));
    }

    // ... other columns
}
```

#### Step 3: WHERE Clause Support

```cpp
// Parse WHERE sdb$key = '...'
// Convert string back to TID for fast lookup

TID parse_sdb_key(const std::string& key_str) {
    // Parse "TTTT-PPPPPPPPPPPP-SSSS"
    uint16_t tablespace_id;
    uint64_t page_id;
    uint16_t slot_id;

    sscanf(key_str.c_str(), "%4hx-%12llx-%4hx",
           &tablespace_id, &page_id, &slot_id);

    GPID gpid = makeGPID(tablespace_id, page_id);
    return TID(gpid, slot_id);
}

// In executor
if (condition.column == "sdb$key" && condition.op == EQ) {
    TID target_tid = parse_sdb_key(condition.value.asString());
    // Direct page lookup (no scan)
    return fetchTupleByTID(target_tid);
}
```

---

### Use Case: Hash Index on `sdb$key`

**Why**: Fast O(1) lookup by physical position.

**System Hash Index**:
```sql
-- Automatic system index (created internally)
CREATE SYSTEM INDEX idx_orders_sdb_key ON orders USING HASH (sdb$key);
```

**Hash Function**:
```cpp
uint64_t hash_sdb_key(uint16_t tablespace_id, uint64_t page_id, uint16_t slot_id) {
    // Combine components
    uint64_t hash = 0;
    hash ^= (uint64_t)tablespace_id << 48;
    hash ^= page_id;
    hash ^= (uint64_t)slot_id << 32;
    return hash % HASH_BUCKETS;
}
```

**Query Optimization**:
```sql
-- Before: Table scan
UPDATE orders SET status = 'shipped' WHERE order_id = 12345;

-- After: Hash index on sdb$key (if available)
UPDATE orders SET status = shipped' WHERE sdb$key = '0001-00000042-0003';
-- → O(1) hash lookup, direct page access
```

---

## Part 2: `rdb$row_uuid` - Permanent Row Identity

(Same design as previous document - see ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md)

**Key Points**:
- 16 bytes UUID v7 stored in TupleHeader
- Permanent (survives migration/restore)
- Time-ordered
- Optional surfacing as identity column

---

## Part 3: `TRANSFER` Command - Bulk Data Copy/Move

### Design: `TRANSFER` Syntax

```sql
-- Basic syntax
TRANSFER FROM source_table TO target_table [WHERE condition];

-- With column mapping
TRANSFER FROM source_table (col1, col2, col3)
        TO target_table (new_col1, new_col2, new_col3)
        WHERE condition;

-- With options
TRANSFER FROM source_table TO target_table
        WHERE condition
        WITH (
            delete_source = false,    -- false = COPY, true = MOVE
            preserve_uuid = true,     -- Preserve rdb$row_uuid
            batch_size = 1000,        -- Rows per batch
            transaction_mode = 'batch', -- 'single' | 'batch' | 'row'
            conflict_action = 'error',  -- 'error' | 'skip' | 'update'
            parallel = 1              -- Parallel workers (future)
        );
```

---

### Options Explained

| Option | Values | Default | Description |
|--------|--------|---------|-------------|
| **`delete_source`** | `true` \| `false` | `false` | If true, delete from source (MOVE). If false, keep source (COPY) |
| **`preserve_uuid`** | `true` \| `false` | `false` | Preserve `rdb$row_uuid` from source |
| **`batch_size`** | Integer | `1000` | Rows per batch commit |
| **`transaction_mode`** | `single` \| `batch` \| `row` | `batch` | Transaction granularity |
| **`conflict_action`** | `error` \| `skip` \| `update` | `error` | How to handle duplicate keys |
| **`parallel`** | Integer | `1` | Number of parallel workers (future) |

---

### Examples

#### Example 1: Copy Data

```sql
-- Copy active users to new table
TRANSFER FROM users TO users_new
WHERE active = true;
```

#### Example 2: Move Data (Archive)

```sql
-- Move old orders to archive (delete from source)
TRANSFER FROM orders TO orders_archive
WHERE order_date < '2024-01-01'
WITH (delete_source = true);
```

#### Example 3: Column Mapping

```sql
-- Copy with column renaming
TRANSFER FROM users_old (user_id, name, email)
        TO users_new (id, full_name, email_address)
WHERE status = 'active';
```

#### Example 4: Preserve UUID for Foreign Keys

```sql
-- Copy preserving row UUIDs (foreign keys remain valid)
TRANSFER FROM customers TO customers_backup
WITH (preserve_uuid = true);
```

#### Example 5: Cross-Tablespace Migration

```sql
-- Move hot data to SSD tablespace
CREATE TABLE orders_hot TABLESPACE ssd_fast LIKE orders;

TRANSFER FROM orders TO orders_hot
WHERE order_date >= NOW() - INTERVAL '30 days'
WITH (delete_source = true, preserve_uuid = true);
```

#### Example 6: Remote Transfer (BETA - with database links)

```sql
-- Copy from remote database
TRANSFER FROM remote_db.sales TO local_sales
WHERE region = 'US'
WITH (batch_size = 5000);
```

---

### Implementation: `TRANSFER` Command

#### Step 1: Parser

```cpp
// src/parser/parser.cpp

Statement* Parser::parseTransfer() {
    // TRANSFER FROM source TO target [WHERE ...] [WITH (...)]

    auto* stmt = new TransferStmt();

    // FROM clause
    consume(TokenType::KW_FROM);
    stmt->source_table = parseTableName();

    if (match(TokenType::LEFT_PAREN)) {
        stmt->source_columns = parseColumnList();
        consume(TokenType::RIGHT_PAREN);
    }

    // TO clause
    consume(TokenType::KW_TO);
    stmt->target_table = parseTableName();

    if (match(TokenType::LEFT_PAREN)) {
        stmt->target_columns = parseColumnList();
        consume(TokenType::RIGHT_PAREN);
    }

    // WHERE clause (optional)
    if (match(TokenType::KW_WHERE)) {
        stmt->where_clause = parseExpression();
    }

    // WITH clause (optional)
    if (match(TokenType::KW_WITH)) {
        stmt->options = parseWithOptions();
    }

    return stmt;
}
```

#### Step 2: Executor

```cpp
// src/sblr/executor.cpp

void Executor::executeTransfer(const TransferStmt* stmt) {
    // Options
    bool delete_source = stmt->options.get("delete_source", false);
    bool preserve_uuid = stmt->options.get("preserve_uuid", false);
    int batch_size = stmt->options.get("batch_size", 1000);

    // Open source table
    Table* source = db_->openTable(stmt->source_table);
    Table* target = db_->openTable(stmt->target_table);

    // Scan source table
    TableScanIterator iter(source, stmt->where_clause);

    int rows_transferred = 0;
    Transaction* txn = conn_ctx_->current_transaction;

    while (iter.hasNext()) {
        Tuple tuple = iter.next();

        // Optionally preserve UUID
        if (preserve_uuid) {
            // Keep row_uuid from source tuple
            target_tuple.setRowUUID(tuple.getRowUUID());
        } else {
            // Generate new UUID
            target_tuple.setRowUUID(generateUuidV7());
        }

        // Insert into target
        TID target_tid;
        target->insert(target_tuple, &target_tid, &err_ctx);

        // Optionally delete from source
        if (delete_source) {
            source->deleteTuple(tuple.getTID(), &err_ctx);
        }

        rows_transferred++;

        // Commit batch
        if (rows_transferred % batch_size == 0) {
            txn->commit();
            txn = beginTransaction();
        }
    }

    // Final commit
    txn->commit();

    LOG_INFO(EXECUTOR, "TRANSFER complete: %d rows transferred", rows_transferred);
}
```

---

### Integration: `TRANSFER` as Primitive for Migration

**Current**:
```sql
ALTER TABLE orders MIGRATE TO TABLESPACE ssd_fast;
```

**Internal Implementation** (using `TRANSFER`):
```sql
BEGIN;
    -- Create shadow table
    CREATE TABLE orders_new TABLESPACE ssd_fast LIKE orders;

    -- Transfer data (preserve UUIDs)
    TRANSFER FROM orders TO orders_new
    WITH (
        preserve_uuid = true,
        delete_source = false,  -- Keep original for rollback
        batch_size = 10000
    );

    -- Update indexes
    -- ... (existing index TID update logic)

    -- Atomic swap
    ALTER TABLE orders RENAME TO orders_old;
    ALTER TABLE orders_new RENAME TO orders;

    -- Drop old table
    DROP TABLE orders_old;
COMMIT;
```

**Benefits**:
- Reusable infrastructure
- Testable independently
- User-accessible (manual migrations)
- Flexible (can customize options)

---

## Implementation Plan

### Phase 1: `sdb$key` Implementation (8-10 hours)

**Task 1.1**: Virtual column infrastructure (2 hours)
- Add virtual column support to catalog
- Mark `sdb$key` as hidden

**Task 1.2**: TID to `sdb$key` conversion (2 hours)
- Format TID as hex string
- Parse hex string back to TID

**Task 1.3**: Executor integration (2 hours)
- Column evaluation for `sdb$key`
- WHERE clause optimization

**Task 1.4**: Hash index on `sdb$key` (2 hours)
- System hash index creation
- Query optimization for `sdb$key` lookups

**Task 1.5**: Testing (2 hours)
- Unit tests for conversion
- Integration tests for queries

---

### Phase 2: `rdb$row_uuid` Implementation (12-16 hours)

(Same as previous design - see Part 2)

---

### Phase 3: `rdb$xact_id` Implementation (8-12 hours)

(Same as previous design)

---

### Phase 4: `TRANSFER` Command (16-24 hours)

**Task 4.1**: Parser (4-6 hours)
- `TRANSFER` syntax parsing
- Column list parsing
- `WITH` options parsing

**Task 4.2**: Executor (8-12 hours)
- Source table scan
- Target table insert
- Optional source delete
- Batch commit logic
- UUID preservation

**Task 4.3**: Options implementation (4-6 hours)
- `delete_source` logic
- `preserve_uuid` logic
- `batch_size` logic
- `conflict_action` logic

**Task 4.4**: Testing (4-6 hours)
- Copy scenarios
- Move scenarios
- Column mapping
- UUID preservation
- Error handling

---

### Phase 5: Integration & Optimization (8-12 hours)

**Task 5.1**: Refactor `ALTER TABLE MIGRATE` (4-6 hours)
- Use `TRANSFER` as primitive
- Update migration code

**Task 5.2**: Hash index optimization (2-3 hours)
- Auto-create hash index on `sdb$key`
- Query planner optimization

**Task 5.3**: Documentation (2-3 hours)
- User guide for `sdb$key`, `rdb$row_uuid`, `TRANSFER`
- SQL reference updates

---

## Testing Strategy

### Unit Tests (30 tests)

**`sdb$key` Tests** (8 tests):
- [ ] TID to `sdb$key` conversion
- [ ] `sdb$key` to TID parsing
- [ ] `SELECT sdb$key` returns correct value
- [ ] `WHERE sdb$key = X` uses direct lookup
- [ ] Hash index on `sdb$key` works
- [ ] `sdb$key` hidden from `SELECT *`
- [ ] `sdb$key` changes on migration
- [ ] `sdb$key` format validation

**`rdb$row_uuid` Tests** (12 tests):
- (Same as previous design)

**`TRANSFER` Tests** (10 tests):
- [ ] `TRANSFER` copies all rows
- [ ] `TRANSFER` with WHERE filters rows
- [ ] `TRANSFER` with column mapping
- [ ] `TRANSFER` with `delete_source = true` deletes from source
- [ ] `TRANSFER` with `preserve_uuid = true` preserves UUIDs
- [ ] `TRANSFER` with `batch_size` commits in batches
- [ ] `TRANSFER` handles duplicate key errors
- [ ] `TRANSFER` rollback on error
- [ ] `TRANSFER` cross-tablespace
- [ ] `TRANSFER` performance (1M rows)

---

## Performance Analysis

### Storage Overhead

| Feature | Overhead |
|---------|----------|
| `sdb$key` | 0 bytes (derived) |
| `rdb$row_uuid` | +16 bytes per tuple |
| `rdb$xact_id` | 0 bytes (uses xmin) |

**Total**: +16 bytes per tuple (44 → 60 byte header)

### CPU Overhead

| Operation | Overhead |
|-----------|----------|
| `sdb$key` access | ~5ns (read TID) |
| `rdb$row_uuid` access | ~5ns (read header) |
| `rdb$xact_id` access | ~5ns (read xmin) |
| `TRANSFER` (per row) | ~500ns (tuple copy) |

**Impact**: Negligible (< 1%)

---

## SQL Reference Summary

### Pseudo-Columns

```sql
-- Physical row location (unstable)
SELECT sdb$key FROM table;

-- Permanent row identifier (stable)
SELECT rdb$row_uuid FROM table;

-- Last-modifying transaction
SELECT rdb$xact_id FROM table;
```

### Functions

```sql
-- Current transaction number
SELECT txn_current();
```

### Commands

```sql
-- Copy data
TRANSFER FROM source TO target WHERE ... [WITH (...)];

-- Move data (delete from source)
TRANSFER FROM source TO target WHERE ... WITH (delete_source = true);
```

---

## Comparison: Firebird vs ScratchBird

| Feature | Firebird | ScratchBird |
|---------|----------|-------------|
| **Physical ID** | `rdb$db_key` (8 bytes) | `sdb$key` (12 bytes, derived) |
| **Logical ID** | ❌ None | `rdb$row_uuid` (16 bytes, permanent) |
| **Transaction Visibility** | ❌ Not exposed | `rdb$xact_id` |
| **Bulk Copy/Move** | ❌ Manual INSERT/DELETE | `TRANSFER` command |
| **Hash Index on Physical ID** | ❌ No | `sdb$key` hash index |

---

## Success Criteria

- [ ] `sdb$key` accessible via SQL
- [ ] `sdb$key` fast lookups (hash index)
- [ ] `rdb$row_uuid` permanent (survives migration)
- [ ] `rdb$xact_id` shows correct transaction
- [ ] `TRANSFER` copies data correctly
- [ ] `TRANSFER` with `delete_source` moves data
- [ ] `TRANSFER` preserves UUIDs when requested
- [ ] Performance overhead < 2%
- [ ] All tests pass (30 unit + integration)

---

**Document Version**: 2.0 (Enhanced)
**Date**: 2025-10-23
**Status**: DESIGN COMPLETE
**Next**: Implementation (44-64 hours)
