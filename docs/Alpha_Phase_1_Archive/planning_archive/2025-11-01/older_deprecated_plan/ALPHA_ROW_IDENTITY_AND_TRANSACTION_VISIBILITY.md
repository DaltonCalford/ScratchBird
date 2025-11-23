# Row Identity and Transaction Visibility - Research & Design

**Date**: 2025-10-23
**Status**: RESEARCH & DESIGN
**Priority**: HIGH (Required for ALPHA completion)
**Estimated Effort**: 20-28 hours

---

## Executive Summary

This document researches and designs two critical features for ScratchBird ALPHA:

1. **Row-Level UUID v7 Identity** (`rdb$row_uuid`) - Stable row identifier that survives backup/restore
2. **Transaction Visibility Metadata** (`rdb$xact_id`) - Last-modifying transaction number visible to current transaction

**Key Innovation**: Unlike Firebird's `rdb$db_key` which changes on restore, ScratchBird's `rdb$row_uuid` is **permanent** and migrates with the row.

---

## Feature 1: Row-Level UUID v7 Identity

### Background: Firebird's `rdb$db_key`

**Firebird's `rdb$db_key`**:
- 8-byte hidden column for tables (page_number + record_number)
- Identifies row location on disk
- **LIMITATION**: Changes on backup/restore
- **LIMITATION**: Changes on table reorganization
- Not suitable for permanent row references

**Quote from Firebird documentation**:
> "RDB$DB_KEY of visible rows is persistent until database re-creation (restoring from backup). However, there is no warranty it will always be so, it is implementation detail coming with no warranties."

### ScratchBird's Enhancement: `rdb$row_uuid`

**Design Goals**:
1. **Permanent identifier**: Survives backup/restore, migration, replication
2. **Time-ordered**: UUID v7 for natural chronological ordering
3. **Optional surfacing**: If user declares UUID column as identity, use row UUID instead of generating new one
4. **Hidden by default**: Like `rdb$db_key`, hidden unless explicitly selected

---

### Design: `rdb$row_uuid` Implementation

#### 1. Storage in TupleHeader

Add UUID v7 to `TupleHeader` struct:

```cpp
// include/scratchbird/core/heap_page.h

struct TupleHeader {
    // Transaction info (16 bytes)
    uint64_t xmin;  // Transaction ID that inserted this tuple
    uint64_t xmax;  // Transaction ID that deleted/updated this tuple (or 0)

    // Version chain (12 bytes)
    uint64_t back_version_gpid;
    uint16_t back_version_slot;
    uint16_t reserved1;

    // Tuple metadata (12 bytes)
    GPID ctid_gpid;
    uint16_t ctid_slot;
    uint16_t infomask;

    // Null bitmap (4 bytes)
    uint16_t null_bitmap_offset;
    uint16_t padding;

    // NEW: Row UUID (16 bytes) - Permanent row identifier
    UuidV7Bytes row_uuid;  // UUID v7 for time-ordered, globally unique row ID

    // Total: 60 bytes (increased from 44 bytes)
};
```

**Cost**: +16 bytes per tuple (44 → 60 bytes header)
**Benefit**: Permanent, globally unique row identifier

---

#### 2. UUID Generation Strategy

**When to Generate**:
- **INSERT**: Generate UUID v7 on row creation
- **UPDATE (in-place)**: Preserve existing `row_uuid` (stable identity)
- **UPDATE (cross-page)**: Preserve existing `row_uuid` (migrate with row)
- **RESTORE**: Preserve `row_uuid` from backup

**How to Generate**:
```cpp
// On INSERT
void HeapPage::insertTuple(const Tuple& tuple, TransactionId xid, TID* out_tid, ErrorContext* ctx) {
    TupleHeader header;
    header.xmin = xid;
    header.xmax = 0;
    header.row_uuid = generateUuidV7();  // NEW: Generate permanent UUID
    // ... rest of insertion logic
}
```

**UUID v7 Properties**:
- Time-ordered (first 48 bits = millisecond timestamp)
- Globally unique (74 bits of randomness)
- Sortable chronologically
- RFC 9562 compliant

---

#### 3. Optional User-Defined UUID Column

**Feature**: If user declares a UUID column with `GENERATED ALWAYS AS IDENTITY`, use `row_uuid` instead of creating separate column.

**SQL Syntax**:
```sql
-- Option 1: Explicit row UUID column (stores in user column, no header overhead)
CREATE TABLE users (
    id UUID GENERATED ALWAYS AS IDENTITY PRIMARY KEY,  -- Use rdb$row_uuid
    name TEXT,
    email TEXT
);

-- Option 2: No explicit UUID column (hidden in header, accessible via rdb$row_uuid)
CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    customer_id INT,
    total DECIMAL(10,2)
    -- rdb$row_uuid is hidden but accessible
);
```

**Implementation**:

```cpp
// Catalog: Mark column as surfaced row UUID
struct ColumnRecord {
    // ... existing fields ...
    bool is_row_uuid_surface;  // NEW: If true, this column surfaces rdb$row_uuid
};

// On INSERT: If table has surfaced UUID column, copy row_uuid to column
void StorageEngine::insert(const Table& table, const Tuple& tuple, TID* out_tid, ErrorContext* ctx) {
    // Generate row UUID
    UuidV7Bytes row_uuid = generateUuidV7();

    // If table has UUID identity column, populate it with row_uuid
    if (table.has_uuid_identity_column) {
        tuple.setColumn(table.uuid_identity_column_idx, row_uuid);
    }

    // Store in tuple header regardless
    header.row_uuid = row_uuid;

    // ... rest of insertion
}
```

**Benefits**:
- User can use UUID as primary key without redundant storage
- Single source of truth (tuple header)
- Automatically time-ordered by insertion

---

#### 4. SQL Access to `rdb$row_uuid`

**Hidden Column Access**:
```sql
-- Select row UUID (hidden column)
SELECT rdb$row_uuid, name, email FROM users;

-- Use in WHERE clause
SELECT * FROM users WHERE rdb$row_uuid = '0192e6e6-4c7a-7c1e-a8f3-9c8e6b5d4a3c';

-- Use in JOIN
SELECT o.*, u.name
FROM orders o
JOIN users u ON o.rdb$row_uuid = u.user_ref_uuid;
```

**Implementation**:
- Add `rdb$row_uuid` as virtual column in catalog
- Column type: `UUID`
- Column storage: `VIRTUAL` (stored in tuple header, not in tuple data)
- Always available, never NULL

---

### Design: Migration and Backup/Restore

#### Backup Format

**Include `row_uuid` in backup**:
```sql
-- Backup format (SBLR bytecode)
INSERT INTO users (id, name, email, rdb$row_uuid)
VALUES (1, 'Alice', 'alice@example.com', '0192e6e6-4c7a-7c1e-a8f3-9c8e6b5d4a3c');
```

**Critical**: `row_uuid` must be preserved during backup/restore to maintain stable references.

#### Table Migration

**When migrating table to new tablespace**:
- Preserve `row_uuid` during page copy
- Update TID mapping (`old_TID → new_TID`)
- **DO NOT** regenerate `row_uuid`

```cpp
// During migration
void CatalogManager::migrateTablePage(const HeapPage& source_page, HeapPage& target_page) {
    for (uint16_t slot = 0; slot < source_page.slot_count; slot++) {
        const TupleHeader* source_header = source_page.getTupleHeader(slot);

        TupleHeader target_header = *source_header;
        target_header.row_uuid = source_header->row_uuid;  // PRESERVE UUID
        target_header.ctid_gpid = target_page.gpid;
        target_header.ctid_slot = target_slot;

        target_page.insertTupleWithHeader(target_header, tuple_data);
    }
}
```

---

## Feature 2: Transaction Visibility Metadata

### Background: Firebird's Transaction Visibility

**Firebird**: Does not expose last-modifying transaction to SQL queries (internal only).

**ScratchBird Enhancement**: Expose visible transaction number via `rdb$xact_id` pseudo-column.

---

### Design: `rdb$xact_id` Implementation

#### 1. Concept: Visible Transaction Number

**Definition**: The transaction number of the transaction that last modified this row **visible to the current transaction**.

**MGA Implications**:
- Current transaction sees its own uncommitted changes
- Other transactions see only committed changes
- Transaction must traverse back-version chain to find visible version

**Example**:
```
Transaction 10: UPDATE row SET value = 100; COMMIT;
Transaction 11: SELECT value, rdb$xact_id FROM row;
  → Result: value = 100, rdb$xact_id = 10

Transaction 12: UPDATE row SET value = 200; (not committed yet)
Transaction 13: SELECT value, rdb$xact_id FROM row;
  → Result: value = 100, rdb$xact_id = 10 (transaction 12 not visible)
```

---

#### 2. Storage: Already Available in `TupleHeader`

**No additional storage needed**:
```cpp
struct TupleHeader {
    uint64_t xmin;  // Inserting transaction
    uint64_t xmax;  // Deleting/updating transaction
    // ...
};
```

**Visible transaction**: For a visible tuple, `rdb$xact_id = xmin`.

---

#### 3. SQL Access to `rdb$xact_id`

**Hidden Column Access**:
```sql
-- Get current transaction number
SELECT txn_current();  -- Returns current transaction's XID

-- Get row's last-modifying transaction
SELECT name, email, rdb$xact_id FROM users WHERE user_id = 123;

-- Find rows modified by specific transaction
SELECT * FROM audit_log WHERE rdb$xact_id = 100;

-- Find rows modified after specific transaction
SELECT * FROM users WHERE rdb$xact_id > 1000;

-- Join with transaction log
SELECT u.name, t.commit_timestamp
FROM users u
JOIN pg_transactions t ON u.rdb$xact_id = t.transaction_id;
```

---

#### 4. Implementation: Visibility Resolution

**Executor Integration**:

```cpp
// src/sblr/executor.cpp

Value Executor::evaluateColumnRef(const ColumnRef& col_ref, const Tuple& tuple) {
    // Handle virtual columns
    if (col_ref.column_name == "rdb$xact_id") {
        // Get tuple header
        const TupleHeader* header = tuple.getHeader();

        // Return visible transaction (xmin for inserts, xmax for updates)
        if (header->isUpdated()) {
            return Value::makeInt64(header->xmax);  // Last update transaction
        } else {
            return Value::makeInt64(header->xmin);  // Insert transaction
        }
    }

    // Handle rdb$row_uuid
    if (col_ref.column_name == "rdb$row_uuid") {
        const TupleHeader* header = tuple.getHeader();
        return Value::makeUUID(header->row_uuid);
    }

    // Normal column access
    return tuple.getColumn(col_ref.column_index);
}
```

---

#### 5. Current Transaction Number Function

**SQL Function**:
```sql
SELECT txn_current();  -- Returns current transaction's XID
```

**Implementation**:
```cpp
// Built-in function: txn_current()
Value Function_TXN_CURRENT(const std::vector<Value>& args, ConnectionContext* conn_ctx) {
    if (!args.empty()) {
        throw RuntimeError("txn_current() takes no arguments");
    }

    Transaction* txn = conn_ctx->current_transaction;
    if (!txn) {
        throw RuntimeError("No active transaction");
    }

    return Value::makeInt64(txn->tra_number);
}
```

---

### Design: Use Cases for `rdb$xact_id`

#### Use Case 1: Audit Trail

**Problem**: Track when each row was last modified.

**Solution**:
```sql
-- Find recently modified rows
SELECT * FROM users
WHERE rdb$xact_id > txn_current() - 1000;

-- Audit log with transaction numbers
CREATE TABLE audit_log (
    log_id SERIAL PRIMARY KEY,
    table_name TEXT,
    row_uuid UUID,
    action TEXT,
    txn_id BIGINT DEFAULT txn_current(),
    timestamp TIMESTAMP DEFAULT NOW()
);
```

---

#### Use Case 2: Incremental Sync

**Problem**: Sync changes from database to external system.

**Solution**:
```sql
-- Track last synced transaction
CREATE TABLE sync_state (
    sync_name TEXT PRIMARY KEY,
    last_txn_id BIGINT
);

-- Get new changes since last sync
SELECT * FROM users
WHERE rdb$xact_id > (SELECT last_txn_id FROM sync_state WHERE sync_name = 'user_sync');

-- Update sync state
UPDATE sync_state
SET last_txn_id = txn_current()
WHERE sync_name = 'user_sync';
```

---

#### Use Case 3: Change Data Capture (CDC)

**Problem**: Detect which rows changed in a transaction.

**Solution**:
```sql
-- Before transaction
SELECT txn_current() AS start_txn;  -- e.g., 1000

-- ... perform updates ...

-- After transaction
SELECT * FROM users WHERE rdb$xact_id >= 1000;  -- All modified rows
```

---

#### Use Case 4: Debugging

**Problem**: Understand when data changed.

**Solution**:
```sql
-- Find rows modified by transaction 12345
SELECT * FROM orders WHERE rdb$xact_id = 12345;

-- Join with pg_transactions to get commit time
SELECT o.*, t.commit_timestamp
FROM orders o
JOIN pg_transactions t ON o.rdb$xact_id = t.transaction_id
WHERE o.order_id = 42;
```

---

## Implementation Plan

### Phase A: `rdb$row_uuid` Implementation (12-16 hours)

#### Task A.1: Extend TupleHeader (2 hours)

**Files**:
- `include/scratchbird/core/heap_page.h` (~20 lines)

**Changes**:
```cpp
struct TupleHeader {
    // ... existing fields ...
    UuidV7Bytes row_uuid;  // NEW: 16 bytes
    // Total: 60 bytes (was 44 bytes)
};
```

**Testing**:
- Verify header size calculation
- Verify alignment

---

#### Task A.2: UUID Generation on INSERT (3 hours)

**Files**:
- `src/core/heap_page.cpp` (~80 lines)
- `src/core/storage_engine.cpp` (~60 lines)

**Changes**:
```cpp
void HeapPage::insertTuple(...) {
    header.row_uuid = generateUuidV7();
    // ... rest of insertion
}
```

**Testing**:
- Verify UUID generated on INSERT
- Verify UUID unique
- Verify UUID time-ordered

---

#### Task A.3: UUID Preservation on UPDATE (2 hours)

**Files**:
- `src/core/heap_page.cpp` (~40 lines)

**Changes**:
```cpp
void HeapPage::updateTuple(...) {
    // Preserve row_uuid from old tuple
    new_header.row_uuid = old_header.row_uuid;
    // ... rest of update
}
```

**Testing**:
- Verify UUID preserved on in-place UPDATE
- Verify UUID preserved on cross-page UPDATE

---

#### Task A.4: SQL Access to `rdb$row_uuid` (4 hours)

**Files**:
- `src/core/catalog_manager.cpp` (~100 lines) - Add virtual column
- `src/sblr/executor.cpp` (~80 lines) - Column evaluation
- `src/parser/parser.cpp` (~40 lines) - Column reference parsing

**Changes**:
- Add `rdb$row_uuid` as virtual column in system catalog
- Handle `rdb$row_uuid` in column references
- Return `row_uuid` from tuple header

**Testing**:
- `SELECT rdb$row_uuid FROM table`
- `WHERE rdb$row_uuid = '...'`
- `ORDER BY rdb$row_uuid`

---

#### Task A.5: Optional UUID Identity Column (5 hours)

**Files**:
- `src/parser/parser.cpp` (~120 lines) - Parse `GENERATED AS IDENTITY`
- `src/core/catalog_manager.cpp` (~100 lines) - Mark column as surfaced UUID
- `src/core/storage_engine.cpp` (~80 lines) - Populate UUID column on INSERT

**Changes**:
```sql
CREATE TABLE users (
    id UUID GENERATED ALWAYS AS IDENTITY PRIMARY KEY
);
```

**Testing**:
- Verify UUID column populated from `row_uuid`
- Verify no redundant storage
- Verify primary key constraint works

---

#### Task A.6: Backup/Restore with UUID (4 hours)

**Files**:
- `src/tools/backup.cpp` (~80 lines) - Include `row_uuid` in backup
- `src/tools/restore.cpp` (~80 lines) - Restore `row_uuid`

**Changes**:
- Backup format includes `row_uuid` for each tuple
- Restore preserves `row_uuid` from backup

**Testing**:
- Backup table, drop table, restore → verify UUIDs preserved
- Verify UUID references still valid after restore

---

### Phase B: `rdb$xact_id` Implementation (8-12 hours)

#### Task B.1: SQL Access to `rdb$xact_id` (3 hours)

**Files**:
- `src/core/catalog_manager.cpp` (~60 lines) - Add virtual column
- `src/sblr/executor.cpp` (~80 lines) - Column evaluation

**Changes**:
```cpp
if (col_ref.column_name == "rdb$xact_id") {
    const TupleHeader* header = tuple.getHeader();
    return Value::makeInt64(header->xmin);
}
```

**Testing**:
- `SELECT rdb$xact_id FROM table`
- `WHERE rdb$xact_id = 123`
- `WHERE rdb$xact_id > 1000`

---

#### Task B.2: Current Transaction Function (2 hours)

**Files**:
- `src/sblr/builtin_functions.cpp` (~60 lines)
- `include/scratchbird/sblr/builtin_functions.h` (~10 lines)

**Changes**:
```cpp
Value Function_TXN_CURRENT(const std::vector<Value>& args, ConnectionContext* conn_ctx) {
    return Value::makeInt64(conn_ctx->current_transaction->tra_number);
}
```

**SQL**:
```sql
SELECT txn_current();
```

**Testing**:
- Verify function returns current transaction ID
- Verify changes across transactions

---

#### Task B.3: Transaction Catalog View (3 hours)

**Files**:
- `src/core/catalog_manager.cpp` (~120 lines)

**Create system view**:
```sql
CREATE VIEW pg_transactions AS
SELECT
    transaction_id,
    start_time,
    commit_time,
    status,
    isolation_level
FROM sys_transactions;
```

**Testing**:
- Query `pg_transactions`
- Join with user tables on `rdb$xact_id`

---

#### Task B.4: Integration Tests (4 hours)

**Test Cases**:
1. Insert row, verify `rdb$xact_id` equals inserting transaction
2. Update row, verify `rdb$xact_id` equals updating transaction
3. Concurrent transactions, verify visibility
4. Incremental sync use case
5. CDC use case

---

## Testing Strategy

### Unit Tests

**`rdb$row_uuid` Tests** (12 tests):
- [ ] UUID generated on INSERT
- [ ] UUID preserved on in-place UPDATE
- [ ] UUID preserved on cross-page UPDATE
- [ ] UUID preserved on table migration
- [ ] UUID preserved on backup/restore
- [ ] UUID accessible via `SELECT rdb$row_uuid`
- [ ] UUID usable in WHERE clause
- [ ] UUID usable in ORDER BY
- [ ] UUID identity column populated correctly
- [ ] UUID unique across all rows
- [ ] UUID time-ordered
- [ ] UUID format valid (RFC 9562)

**`rdb$xact_id` Tests** (8 tests):
- [ ] `rdb$xact_id` equals inserting transaction
- [ ] `rdb$xact_id` equals updating transaction
- [ ] `rdb$xact_id` visible only for committed transactions
- [ ] `txn_current()` returns current transaction ID
- [ ] `rdb$xact_id` usable in WHERE clause
- [ ] `rdb$xact_id` usable in ORDER BY
- [ ] `rdb$xact_id` joins with `pg_transactions`
- [ ] `rdb$xact_id` updates on row modification

**Total**: 20 new unit tests

---

### Integration Tests

**Scenarios**:
1. **Backup/Restore**: Verify UUIDs preserved
2. **Table Migration**: Verify UUIDs preserved
3. **Replication** (future): Verify UUIDs replicated
4. **Incremental Sync**: Use `rdb$xact_id` for CDC
5. **Audit Trail**: Track changes with `rdb$xact_id`
6. **Concurrent Transactions**: Verify `rdb$xact_id` visibility

---

## Performance Impact

### `rdb$row_uuid` Overhead

**Storage**:
- +16 bytes per tuple header (44 → 60 bytes)
- ~36% increase in header size
- For 1M rows: +16 MB storage

**CPU**:
- UUID generation: ~100ns per INSERT (negligible)
- UUID comparison: ~20ns (same as 16-byte memcmp)

**Impact**: Minimal (< 1% on typical workloads)

---

### `rdb$xact_id` Overhead

**Storage**:
- No additional storage (uses existing `xmin` field)

**CPU**:
- Column access: ~5ns (read from header)

**Impact**: Negligible (< 0.1%)

---

## SQL Examples

### Row UUID Examples

```sql
-- 1. Get row UUID
SELECT rdb$row_uuid, name, email FROM users;

-- 2. Find row by UUID
SELECT * FROM users WHERE rdb$row_uuid = '0192e6e6-4c7a-7c1e-a8f3-9c8e6b5d4a3c';

-- 3. Use UUID as foreign key
CREATE TABLE orders (
    order_id SERIAL PRIMARY KEY,
    user_uuid UUID REFERENCES users(rdb$row_uuid),  -- Permanent reference
    total DECIMAL(10,2)
);

-- 4. UUID identity column
CREATE TABLE documents (
    doc_id UUID GENERATED ALWAYS AS IDENTITY PRIMARY KEY,  -- Surfaces rdb$row_uuid
    title TEXT,
    content TEXT
);

INSERT INTO documents (title, content) VALUES ('Report', 'Data...');
SELECT doc_id FROM documents;  -- Returns time-ordered UUID v7

-- 5. Track row lineage across backups
SELECT rdb$row_uuid, * FROM users WHERE email = 'alice@example.com';
-- Backup database
-- Restore database
SELECT rdb$row_uuid, * FROM users WHERE email = 'alice@example.com';
-- rdb$row_uuid is SAME before and after restore
```

---

### Transaction ID Examples

```sql
-- 1. Get current transaction
SELECT txn_current();  -- e.g., 12345

-- 2. Find rows modified by current transaction
SELECT * FROM users WHERE rdb$xact_id = txn_current();

-- 3. Incremental sync
-- Last sync was at transaction 1000
SELECT * FROM users WHERE rdb$xact_id > 1000;

-- 4. Audit trail
SELECT
    u.*,
    u.rdb$xact_id AS last_modified_txn,
    t.commit_timestamp AS last_modified_time
FROM users u
JOIN pg_transactions t ON u.rdb$xact_id = t.transaction_id
WHERE u.user_id = 42;

-- 5. Change data capture
BEGIN;
SELECT txn_current() AS start_txn;  -- 2000

-- ... perform updates ...

SELECT * FROM users WHERE rdb$xact_id >= 2000;  -- All changed rows
COMMIT;

-- 6. Find recent changes
SELECT * FROM orders
WHERE rdb$xact_id > txn_current() - 1000
ORDER BY rdb$xact_id DESC;
```

---

## Documentation Requirements

### User Documentation

- [ ] **User Guide**: Row UUID feature
- [ ] **User Guide**: Transaction visibility feature
- [ ] **SQL Reference**: `rdb$row_uuid` pseudo-column
- [ ] **SQL Reference**: `rdb$xact_id` pseudo-column
- [ ] **SQL Reference**: `txn_current()` function
- [ ] **SQL Reference**: `GENERATED AS IDENTITY` clause
- [ ] **Migration Guide**: Backup/restore with UUIDs

### Developer Documentation

- [ ] **API Reference**: `row_uuid` in TupleHeader
- [ ] **Design Doc**: UUID generation strategy
- [ ] **Design Doc**: Transaction visibility resolution

---

## Success Criteria

### `rdb$row_uuid`

- [ ] UUID generated on INSERT
- [ ] UUID preserved on UPDATE (in-place and cross-page)
- [ ] UUID preserved on backup/restore
- [ ] UUID preserved on table migration
- [ ] UUID accessible via SQL (`SELECT rdb$row_uuid`)
- [ ] UUID identity column works (`GENERATED AS IDENTITY`)
- [ ] UUIDs are unique
- [ ] UUIDs are time-ordered (UUID v7)
- [ ] Performance overhead < 2%

### `rdb$xact_id`

- [ ] Transaction ID accessible via SQL (`SELECT rdb$xact_id`)
- [ ] Transaction ID reflects last-modifying transaction
- [ ] Transaction ID respects MVCC visibility
- [ ] `txn_current()` function works
- [ ] `pg_transactions` view works
- [ ] Performance overhead < 1%

---

## Comparison with Firebird

| Feature | Firebird `rdb$db_key` | ScratchBird `rdb$row_uuid` |
|---------|----------------------|----------------------------|
| **Type** | 8-byte page/slot | 16-byte UUID v7 |
| **Stability** | Changes on restore | Permanent (survives restore) |
| **Global uniqueness** | No (database-local) | Yes (globally unique) |
| **Time-ordered** | No | Yes (UUID v7) |
| **Surfaceable** | No | Yes (identity column) |
| **Overhead** | 0 bytes (derived) | 16 bytes (stored) |

| Feature | Firebird | ScratchBird `rdb$xact_id` |
|---------|----------|----------------------------|
| **Last-modified XID** | Not exposed | Exposed via `rdb$xact_id` |
| **Current XID** | Not exposed | `txn_current()` function |
| **Transaction catalog** | Limited | Full `pg_transactions` view |

---

## Next Steps

1. **Review design** with stakeholders
2. **Approve 16-byte overhead** for `row_uuid`
3. **Implement Phase A** (rdb$row_uuid) - 12-16 hours
4. **Implement Phase B** (rdb$xact_id) - 8-12 hours
5. **Test thoroughly** (20 unit tests + integration)
6. **Document features** (user + developer docs)

---

**Document Version**: 1.0
**Date**: 2025-10-23
**Status**: RESEARCH & DESIGN COMPLETE
**Next**: Stakeholder review and approval
**Estimated Total Effort**: 20-28 hours
