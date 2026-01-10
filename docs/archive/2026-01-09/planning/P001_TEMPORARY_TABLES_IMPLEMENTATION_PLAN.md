# P-001: TEMPORARY TABLES Implementation Plan

**Date:** 2026-01-08
**Status:** READY FOR IMPLEMENTATION
**Authority:** Parser Remediation Master Plan - ALPHA BLOCKER
**Estimated Effort:** 8-10 days

---

## Executive Summary

This plan details the complete implementation of temporary tables for ScratchBird using Firebird MGA transaction architecture. The implementation leverages MGA's natural transaction isolation to provide three types of temporary tables:

1. **GTT (Global Temporary Table)** - Transaction-scoped data visibility
2. **Session-scoped GTT** - Data visible across transactions within session
3. **UTT (User Temporary Table)** - DDL rolled back on commit

**Key Insight:** Sessions are ALWAYS in a transaction in Firebird MGA. This means:
- Transaction isolation naturally prevents cross-transaction visibility
- No separate storage directories needed
- Leverage TIP (Transaction Inventory Pages) for visibility
- On commit: temp table data marked for GC, non-temp data commits normally

---

## 1. Architectural Overview

### 1.1 Core Concept

**Firebird MGA Always-in-Transaction Model:**
```
Session 1: Transaction 100 → INSERT into temp table → working...
Session 2: Transaction 101 → SELECT from temp table → sees nothing (MGA prevents dirty reads)
Session 1: Transaction 100 → COMMIT → temp data marked for GC, non-temp data commits
Session 2: Transaction 101 → SELECT from temp table → still sees nothing (temp data never commits)
```

### 1.2 Visibility Rules

**GTT (Transaction-Scoped):**
```c
bool is_visible_gtt(HeapTupleHeader tuple, TransactionId current_xid) {
    // ONLY see your own transaction's work
    return tuple->t_xmin == current_xid;
}
```

**Session-Scoped GTT:**
```c
bool is_visible_session_gtt(HeapTupleHeader tuple,
                             TransactionId current_xid,
                             UUID current_session_id) {
    // See work from same session, across transactions
    if (tuple->session_id != current_session_id) {
        return false;
    }

    // Apply normal MGA visibility rules
    return xid_visible_in_snapshot(tuple->t_xmin, current_snapshot);
}
```

**UTT (User Temporary Table):**
- Same visibility as GTT (transaction-scoped)
- DDL rolled back on commit (table definition removed from catalog)

### 1.3 Commit Behavior

**On Transaction Commit:**
1. Mark transaction as COMMITTED in TIP (normal commit)
2. For temporary tables: mark records for garbage collection
3. For regular tables: records commit normally

**Implementation Options:**
- **Option A:** Walk dirty pages, mark temp table tuples with HEAP_XMIN_ABORTED hint bit
- **Option B:** Add HEAP_XMIN_TEMP_TABLE hint bit, GC recognizes and collects
- **Option C:** Store table_type in tuple header, check during visibility

**Recommended:** Option B (HEAP_XMIN_TEMP_TABLE hint bit)

---

## 2. Data Structure Changes

### 2.1 AST Extensions (ast_v2.h)

```cpp
// Temporary table scope
enum class TemporaryScope : uint8_t {
    NONE = 0,                    // Not a temporary table
    TRANSACTION_SCOPED = 1,      // GTT - transaction-scoped (Firebird GTT)
    SESSION_SCOPED = 2,          // GTT - session-scoped (PostgreSQL/MySQL style)
    USER_TEMPORARY = 3           // UTT - DDL rolled back on commit
};

// ON COMMIT action for temporary tables
enum class OnCommitAction : uint8_t {
    NONE = 0,                    // Not specified
    DELETE_ROWS = 1,             // Delete all rows on commit (default for GTT)
    PRESERVE_ROWS = 2,           // Keep rows after commit (session-scoped)
    DROP = 3                     // Drop table on commit (Firebird-specific)
};

// Extended CREATE TABLE AST node
struct CreateTableStatement {
    // ... existing fields ...

    // Temporary table support
    bool is_temporary = false;
    TemporaryScope temp_scope = TemporaryScope::NONE;
    OnCommitAction on_commit_action = OnCommitAction::NONE;

    // GLOBAL vs LOCAL (for spec compatibility)
    bool is_global_temp = false;  // GLOBAL TEMPORARY TABLE
    bool is_local_temp = false;   // LOCAL TEMPORARY TABLE (session-scoped)
};
```

### 2.2 Catalog Extensions (catalog_manager.h)

```cpp
// TableInfo structure extensions
struct TableInfo {
    // ... existing fields ...

    // Temporary table metadata
    TemporaryScope temp_scope = TemporaryScope::NONE;
    OnCommitAction on_commit_action = OnCommitAction::NONE;

    // Ownership tracking (for UTT and session-scoped GTT)
    ID owner_session_id;         // Session that created (for UTT/session GTT)
    ID owner_transaction_id;     // Transaction that created (for UTT)

    // Lifecycle flags
    bool is_temporary = false;   // Quick check: is this a temp table?
};

// CatalogManager new methods
class CatalogManager {
public:
    // Create temporary table
    auto createTemporaryTable(
        const ID& schema_id,
        const std::string& table_name,
        const std::vector<ColumnInfo>& columns,
        TemporaryScope temp_scope,
        OnCommitAction on_commit_action,
        const ID& session_id,
        const ID& transaction_id,
        ID& table_id,
        ErrorContext* ctx = nullptr
    ) -> Status;

    // Get temporary tables for session
    auto getTemporaryTablesForSession(const ID& session_id)
        -> std::vector<TableInfo>;

    // Get temporary tables for transaction (UTT only)
    auto getTemporaryTablesForTransaction(const ID& transaction_id)
        -> std::vector<TableInfo>;

    // Drop temporary tables on commit (UTT)
    auto dropUserTemporaryTablesOnCommit(const ID& transaction_id) -> Status;

    // Drop temporary tables on disconnect (session cleanup)
    auto dropTemporaryTablesOnDisconnect(const ID& session_id) -> Status;
};
```

### 2.3 Tuple Header Extensions (heap_page.h)

```cpp
// Heap tuple header hint bits (add to existing infomask)
#define HEAP_XMIN_TEMP_TABLE    0x0800  // Tuple from temporary table
#define HEAP_SESSION_ID_VALID   0x1000  // Tuple has valid session_id

// Optional: Add session_id to tuple header (for session-scoped GTT)
struct HeapTupleHeader {
    // ... existing fields (t_xmin, t_xmax, t_cmin, t_cmax, t_ctid) ...

    // New field for session-scoped temporary tables
    uint64_t t_session_id;       // Session ID (UuidV7 low 64 bits)

    // Note: This increases tuple overhead by 8 bytes
    // Alternative: Store full session in separate page or reuse existing field
};
```

### 2.4 Transaction Manager Extensions (transaction_manager.h)

```cpp
// TransactionContext additions
class TransactionContext {
public:
    // Track temporary table operations
    std::vector<ID> modified_temp_tables_;

    // On commit callback for temp table handling
    using TempTableCommitCallback = std::function<void(const ID& table_id)>;
    void registerTempTableCommitCallback(TempTableCommitCallback callback);

    // Mark temp table tuples for GC on commit
    auto markTemporaryTableDataForGC() -> Status;
};
```

### 2.5 Connection Context Extensions (connection_context.h)

```cpp
// ConnectionContext additions
class ConnectionContext {
public:
    // Track temporary tables created in this session
    std::unordered_set<ID> session_temp_tables_;

    // Cleanup on disconnect
    void cleanupTemporaryTablesOnDisconnect();
};
```

---

## 3. Implementation Phases

### Phase 1: AST and Type Definitions (1 day)

**Files to modify:**
- `include/scratchbird/parser/ast_v2.h`
- `src/parser/ast_v2.cpp`

**Tasks:**
1. Add `TemporaryScope` enum
2. Add `OnCommitAction` enum
3. Extend `CreateTableStatement` with temporary table fields
4. Add serialization/deserialization for new fields

**Acceptance Criteria:**
- [ ] AST nodes compile without errors
- [ ] Enum values match Firebird/PostgreSQL/MySQL semantics
- [ ] AST can represent all temporary table variants

---

### Phase 2: Parser Updates (All 4 Parsers) (2 days)

**Files to modify:**
- `src/parser/parser_v2.cpp` (V2 Parser - Firebird style)
- `src/parser/postgresql_parser.cpp` (PostgreSQL emulated parser)
- `src/parser/mysql_parser.cpp` (MySQL emulated parser)
- `src/parser/firebird_parser.cpp` (Firebird emulated parser)

**Syntax to Support:**

**Firebird (V2 + Firebird Parser):**
```sql
-- GTT (transaction-scoped by default)
CREATE GLOBAL TEMPORARY TABLE temp_orders (
    order_id INT,
    product_name VARCHAR(100)
) ON COMMIT DELETE ROWS;

-- Session-scoped GTT
CREATE GLOBAL TEMPORARY TABLE temp_session (
    session_data VARCHAR(1000)
) ON COMMIT PRESERVE ROWS;

-- UTT (user temporary table)
CREATE TEMPORARY TABLE temp_work (
    id INT,
    data VARCHAR(100)
);  -- Dropped on commit
```

**PostgreSQL Parser:**
```sql
-- Transaction-scoped (not standard PostgreSQL, but we support it)
CREATE TEMPORARY TABLE temp_orders (
    order_id INT,
    product_name VARCHAR(100)
) ON COMMIT DELETE ROWS;

-- Session-scoped (standard PostgreSQL behavior)
CREATE TEMPORARY TABLE temp_session (
    session_data VARCHAR(1000)
);

-- Explicit session-scoped
CREATE TEMPORARY TABLE temp_explicit (
    data VARCHAR(100)
) ON COMMIT PRESERVE ROWS;

-- Drop on commit
CREATE TEMPORARY TABLE temp_drop (
    data VARCHAR(100)
) ON COMMIT DROP;
```

**MySQL Parser:**
```sql
-- Session-scoped (MySQL default)
CREATE TEMPORARY TABLE temp_orders (
    order_id INT,
    product_name VARCHAR(100)
);

-- Note: MySQL doesn't support ON COMMIT clause,
-- all temporary tables are session-scoped
```

**Parser Implementation Tasks:**
1. Parse `TEMPORARY` / `TEMP` keyword
2. Parse `GLOBAL` vs `LOCAL` modifiers
3. Parse `ON COMMIT` clause (DELETE ROWS | PRESERVE ROWS | DROP)
4. Set `CreateTableStatement` fields correctly based on syntax
5. Default behaviors:
   - Firebird: `GLOBAL TEMPORARY` → transaction-scoped
   - PostgreSQL: `TEMPORARY` → session-scoped
   - MySQL: `TEMPORARY` → session-scoped
   - No `GLOBAL`/`LOCAL` → user temporary (UTT)

**Acceptance Criteria:**
- [ ] All 4 parsers recognize temporary table syntax
- [ ] AST correctly populated with temp_scope and on_commit_action
- [ ] Syntax errors rejected appropriately
- [ ] Parser tests pass for all temporary table variants

---

### Phase 3: Bytecode Generator (1 day)

**Files to modify:**
- `src/sblr/bytecode_generator_v2.cpp`
- `include/scratchbird/sblr/opcodes.h`

**Opcodes to Add/Extend:**
```cpp
// Extend CREATE_TABLE opcode payload
struct CreateTablePayload {
    // ... existing fields ...

    uint8_t is_temporary;        // 0=permanent, 1=temporary
    uint8_t temp_scope;          // TemporaryScope enum
    uint8_t on_commit_action;    // OnCommitAction enum
    UUID owner_session_id;       // For UTT/session GTT
    uint64_t owner_transaction_id; // For UTT
};
```

**Tasks:**
1. Extend `CREATE_TABLE` opcode payload with temporary table fields
2. Serialize temp_scope and on_commit_action to bytecode
3. Include owner_session_id and owner_transaction_id in bytecode

**Acceptance Criteria:**
- [ ] Bytecode includes all temporary table metadata
- [ ] Bytecode deserialization works correctly
- [ ] Round-trip test: SQL → AST → Bytecode → AST matches

---

### Phase 4: Catalog Manager (2 days)

**Files to modify:**
- `src/core/catalog_manager.cpp`
- `include/scratchbird/core/catalog_manager.h`

**Tasks:**

**4.1 Schema Changes:**
1. Add columns to `scratchbird_tables` table:
   - `temp_scope TINYINT` (0=NONE, 1=TRANSACTION, 2=SESSION, 3=USER_TEMPORARY)
   - `on_commit_action TINYINT` (0=NONE, 1=DELETE_ROWS, 2=PRESERVE_ROWS, 3=DROP)
   - `owner_session_id UUID` (NULL for permanent tables)
   - `owner_transaction_id BIGINT` (NULL for permanent tables)

2. Update catalog persistence format

**4.2 Create Temporary Table:**
```cpp
Status CatalogManager::createTemporaryTable(
    const ID& schema_id,
    const std::string& table_name,
    const std::vector<ColumnInfo>& columns,
    TemporaryScope temp_scope,
    OnCommitAction on_commit_action,
    const ID& session_id,
    const ID& transaction_id,
    ID& table_id,
    ErrorContext* ctx)
{
    // 1. Validate temporary table name doesn't conflict with permanent tables
    //    (Allow same name in different sessions for UTT/session GTT)

    // 2. Create TableInfo with temporary flags
    TableInfo table_info;
    table_info.table_type = TableType::TEMPORARY;
    table_info.temp_scope = temp_scope;
    table_info.on_commit_action = on_commit_action;
    table_info.owner_session_id = session_id;
    table_info.owner_transaction_id = transaction_id;
    table_info.is_temporary = true;

    // 3. Store in catalog (in-memory for UTT, persisted for GTT)
    if (temp_scope == TemporaryScope::USER_TEMPORARY) {
        // Store only in memory, not persisted to disk
        in_memory_temp_tables_[table_id] = table_info;
    } else {
        // GTT: Store in catalog (schema is permanent)
        persistTableInfo(table_info);
    }

    // 4. Track in connection context
    connection_context_->session_temp_tables_.insert(table_id);

    // 5. Return table_id
    table_id = table_info.table_id;
    return Status::OK();
}
```

**4.3 Table Name Resolution:**
- When resolving table names, check session-specific temporary tables first
- UTT and session GTT can shadow permanent tables in the same schema
- Transaction-scoped GTT have permanent schema (no shadowing)

**4.4 Cleanup Methods:**
```cpp
// Called on transaction commit
Status CatalogManager::dropUserTemporaryTablesOnCommit(const ID& transaction_id) {
    // Find all UTT created by this transaction
    for (auto& [table_id, table_info] : in_memory_temp_tables_) {
        if (table_info.owner_transaction_id == transaction_id &&
            table_info.temp_scope == TemporaryScope::USER_TEMPORARY) {

            // Drop table definition from catalog
            dropTableInternal(table_id);
            in_memory_temp_tables_.erase(table_id);
        }
    }
    return Status::OK();
}

// Called on session disconnect
Status CatalogManager::dropTemporaryTablesOnDisconnect(const ID& session_id) {
    // Find all temporary tables owned by this session
    for (auto& [table_id, table_info] : in_memory_temp_tables_) {
        if (table_info.owner_session_id == session_id) {
            // Drop table and all data
            dropTableInternal(table_id);
            in_memory_temp_tables_.erase(table_id);
        }
    }
    return Status::OK();
}
```

**Acceptance Criteria:**
- [ ] Temporary tables stored in catalog with correct metadata
- [ ] GTT schema persisted, UTT schema in-memory only
- [ ] Table name resolution respects session/transaction ownership
- [ ] Cleanup methods drop temporary tables correctly
- [ ] Restart test: GTT schema survives restart, UTT does not

---

### Phase 5: Executor and Visibility (2 days)

**Files to modify:**
- `src/sblr/executor.cpp`
- `src/core/storage_engine.cpp`
- `src/core/heap_page.cpp`

**Tasks:**

**5.1 Visibility Function Extensions:**

```cpp
// In heap_page.cpp or storage_engine.cpp

bool tuple_satisfies_mvcc_with_temp_check(
    HeapTupleHeader tuple,
    TransactionSnapshot* snap,
    TransactionId current_xid,
    UUID current_session_id,
    const TableInfo& table_info)
{
    // If not a temporary table, use normal visibility
    if (!table_info.is_temporary) {
        return tuple_satisfies_mvcc(tuple, snap);
    }

    // GTT (transaction-scoped): Only see own transaction's work
    if (table_info.temp_scope == TemporaryScope::TRANSACTION_SCOPED) {
        return tuple->t_xmin == current_xid;
    }

    // Session-scoped GTT: Check session ID first
    if (table_info.temp_scope == TemporaryScope::SESSION_SCOPED) {
        // Check session ID (if stored in tuple)
        if (tuple->t_infomask & HEAP_SESSION_ID_VALID) {
            if (tuple->t_session_id != current_session_id) {
                return false;  // Different session, not visible
            }
        }

        // Same session: apply normal MGA visibility rules
        return tuple_satisfies_mvcc(tuple, snap);
    }

    // UTT: Same as transaction-scoped
    if (table_info.temp_scope == TemporaryScope::USER_TEMPORARY) {
        return tuple->t_xmin == current_xid;
    }

    return false;
}
```

**5.2 Insert/Update/Delete Extensions:**

```cpp
// When inserting into temporary table
Status storage_insert_tuple_temp(
    Relation rel,
    HeapTuple tuple,
    TransactionId xid,
    UUID session_id)
{
    HeapTupleHeader header = tuple->t_data;

    // Set transaction ID (normal)
    header->t_xmin = xid;

    // Set temporary table hint bit
    header->t_infomask |= HEAP_XMIN_TEMP_TABLE;

    // If session-scoped, store session ID
    if (rel->table_info.temp_scope == TemporaryScope::SESSION_SCOPED) {
        header->t_session_id = session_id;
        header->t_infomask |= HEAP_SESSION_ID_VALID;
    }

    // Insert tuple normally
    return heap_insert(rel, tuple);
}
```

**5.3 Commit-Time Garbage Collection Marking:**

```cpp
// In transaction_manager.cpp or executor.cpp

Status TransactionContext::markTemporaryTableDataForGC() {
    // Walk through all modified pages in this transaction
    for (auto& page : dirty_pages_) {
        Page p = buffer_get_page(page.buffer);

        // Check each tuple on the page
        OffsetNumber max_off = page_get_max_offset(p);
        for (OffsetNumber off = FirstOffsetNumber; off <= max_off; off++) {
            ItemId itemid = page_get_itemid(p, off);
            if (!ItemIdIsNormal(itemid)) continue;

            HeapTupleHeader tuple = (HeapTupleHeader) page_get_item(p, itemid);

            // Check if tuple is from temporary table
            if (tuple->t_infomask & HEAP_XMIN_TEMP_TABLE) {
                // Mark for garbage collection
                // Option A: Set hint bit (recommended)
                tuple->t_infomask |= HEAP_XMIN_ABORTED;

                // Option B: Rely on GC to check table_type during sweep
                // (slower but doesn't require walking dirty pages on commit)
            }
        }

        mark_buffer_dirty(page.buffer);
    }

    return Status::OK();
}

// Call this during commit:
Status commit_transaction(TransactionContext* txn) {
    // ... normal commit steps ...

    // Mark temporary table data for GC
    txn->markTemporaryTableDataForGC();

    // Update TIP to COMMITTED (normal)
    set_transaction_state(txn->txn_id, TXN_STATE_COMMITTED);

    // Drop UTT tables
    catalog_manager->dropUserTemporaryTablesOnCommit(txn->txn_id);

    // ... rest of commit ...
}
```

**5.4 ON COMMIT Handling:**

```cpp
// After successful commit, before releasing locks
void handle_on_commit_actions(TransactionContext* txn) {
    for (const auto& table_id : txn->modified_temp_tables_) {
        TableInfo table_info = catalog_manager->getTableInfo(table_id);

        if (!table_info.is_temporary) continue;

        switch (table_info.on_commit_action) {
            case OnCommitAction::DELETE_ROWS:
                // Already handled by marking tuples as aborted
                break;

            case OnCommitAction::PRESERVE_ROWS:
                // For session-scoped GTT: do nothing (rows persist)
                break;

            case OnCommitAction::DROP:
                // Drop table definition (UTT)
                catalog_manager->dropTable(table_id);
                break;

            case OnCommitAction::NONE:
                // Default: DELETE_ROWS for transaction-scoped
                break;
        }
    }
}
```

**Acceptance Criteria:**
- [ ] Visibility rules correctly implemented for all temp table types
- [ ] Transaction-scoped GTT: only own transaction sees data
- [ ] Session-scoped GTT: all transactions in session see data
- [ ] UTT: data and DDL cleaned up on commit
- [ ] ON COMMIT actions work correctly
- [ ] Garbage collector recognizes and collects temp table tuples

---

### Phase 6: Garbage Collector Extensions (1 day)

**Files to modify:**
- `src/core/garbage_collector.cpp`

**Tasks:**

**6.1 Recognize Temporary Table Tuples:**

```cpp
// Enhanced garbage collection for temporary tables
bool heap_tuple_is_dead_temp(
    HeapTupleHeader tuple,
    TransactionId oldest_xmin,
    const TableInfo& table_info)
{
    // If marked as temp table and creator committed, it's dead
    if (tuple->t_infomask & HEAP_XMIN_TEMP_TABLE) {
        TransactionId xmin = tuple->t_xmin;

        // Check if creator transaction committed
        TransactionState state = get_transaction_state(xmin);
        if (state == TXN_STATE_COMMITTED) {
            // Temp table data never survives commit (for transaction-scoped)
            if (table_info.temp_scope == TemporaryScope::TRANSACTION_SCOPED ||
                table_info.temp_scope == TemporaryScope::USER_TEMPORARY) {
                return true;  // Dead - can be vacuumed
            }

            // Session-scoped: check if session still active
            if (table_info.temp_scope == TemporaryScope::SESSION_SCOPED) {
                if (!is_session_active(table_info.owner_session_id)) {
                    return true;  // Session ended, tuple is dead
                }
            }
        }

        // Creator aborted: dead
        if (state == TXN_STATE_ABORTED) {
            return true;
        }

        // Creator still active: not dead
        return false;
    }

    // Regular tuple: use normal logic
    return heap_tuple_is_dead(tuple, oldest_xmin);
}
```

**6.2 Session Cleanup Integration:**

```cpp
// When session disconnects
void cleanup_session_temporary_data(UUID session_id) {
    // 1. Drop temporary tables for this session
    catalog_manager->dropTemporaryTablesOnDisconnect(session_id);

    // 2. Mark session-scoped GTT data for GC
    //    (handled automatically by session check in GC)

    // 3. Remove in-memory tracking
    connection_context->session_temp_tables_.clear();
}
```

**Acceptance Criteria:**
- [ ] GC correctly identifies temp table tuples as dead after commit
- [ ] Session-scoped GTT data cleaned up when session disconnects
- [ ] No memory leaks or orphaned temporary tables
- [ ] GC statistics track temporary table cleanup

---

### Phase 7: Integration and Testing (1-2 days)

**Files to create:**
- `tests/integration/test_temporary_tables_firebird.cpp`
- `tests/integration/test_temporary_tables_postgresql.cpp`
- `tests/integration/test_temporary_tables_mysql.cpp`
- `tests/integration/test_temporary_tables_advanced.cpp`

**Test Coverage:**

**7.1 Transaction-Scoped GTT Tests:**
```cpp
TEST(TemporaryTables, TransactionScopedGTT) {
    // Create GTT
    execute("CREATE GLOBAL TEMPORARY TABLE temp_orders (id INT, name VARCHAR(100)) "
            "ON COMMIT DELETE ROWS");

    // Transaction 1: Insert data
    begin_transaction();
    execute("INSERT INTO temp_orders VALUES (1, 'Order 1')");

    // Verify visible in same transaction
    auto result = execute("SELECT * FROM temp_orders");
    ASSERT_EQ(result.row_count(), 1);

    // Transaction 2 (concurrent): See nothing
    auto txn2 = begin_transaction_concurrent();
    auto result2 = execute_in_transaction(txn2, "SELECT * FROM temp_orders");
    ASSERT_EQ(result2.row_count(), 0);  // MGA isolation

    // Commit Transaction 1
    commit_transaction();

    // Transaction 3: Still see nothing (data deleted on commit)
    auto result3 = execute("SELECT * FROM temp_orders");
    ASSERT_EQ(result3.row_count(), 0);
}

TEST(TemporaryTables, SessionScopedGTT) {
    // Create session-scoped GTT
    execute("CREATE GLOBAL TEMPORARY TABLE session_temp (id INT) "
            "ON COMMIT PRESERVE ROWS");

    // Transaction 1: Insert
    begin_transaction();
    execute("INSERT INTO session_temp VALUES (1)");
    commit_transaction();

    // Transaction 2 (same session): Should see row
    begin_transaction();
    auto result = execute("SELECT * FROM session_temp");
    ASSERT_EQ(result.row_count(), 1);
    commit_transaction();

    // Different session: Should see nothing
    auto other_session = create_new_session();
    auto result2 = execute_in_session(other_session, "SELECT * FROM session_temp");
    ASSERT_EQ(result2.row_count(), 0);
}

TEST(TemporaryTables, UserTemporaryTable) {
    // Create UTT within transaction
    begin_transaction();
    execute("CREATE TEMPORARY TABLE utt_test (id INT)");
    execute("INSERT INTO utt_test VALUES (1)");

    // Should see row
    auto result = execute("SELECT * FROM utt_test");
    ASSERT_EQ(result.row_count(), 1);

    // Commit: table should be dropped
    commit_transaction();

    // Try to select again: should fail (table doesn't exist)
    EXPECT_THROW(execute("SELECT * FROM utt_test"), TableNotFoundException);
}
```

**7.2 Restart/Persistence Tests:**
```cpp
TEST(TemporaryTables, RestartPersistence) {
    // Create GTT (schema should persist)
    execute("CREATE GLOBAL TEMPORARY TABLE persist_gtt (id INT)");

    // Shutdown and restart database
    shutdown_database();
    restart_database();

    // GTT schema should still exist
    auto result = execute("SELECT * FROM persist_gtt");  // Should succeed
    ASSERT_EQ(result.row_count(), 0);  // But no data
}

TEST(TemporaryTables, NoRestartForUTT) {
    begin_transaction();
    execute("CREATE TEMPORARY TABLE utt_no_persist (id INT)");
    commit_transaction();

    // UTT dropped on commit, but let's imagine we didn't commit
    // and instead crashed

    begin_transaction();
    execute("CREATE TEMPORARY TABLE utt_crash_test (id INT)");
    // Simulate crash (don't commit)

    crash_and_restart_database();

    // UTT should not exist after restart
    EXPECT_THROW(execute("SELECT * FROM utt_crash_test"), TableNotFoundException);
}
```

**7.3 Garbage Collection Tests:**
```cpp
TEST(TemporaryTables, GarbageCollection) {
    // Create GTT and insert many rows
    execute("CREATE GLOBAL TEMPORARY TABLE gc_test (id INT, data VARCHAR(1000))");

    begin_transaction();
    for (int i = 0; i < 10000; i++) {
        execute("INSERT INTO gc_test VALUES (?, ?)", i, generate_random_string(1000));
    }
    commit_transaction();

    // Force garbage collection
    execute("VACUUM ANALYZE gc_test");

    // Check that temp table data was cleaned up
    auto gc_stats = get_garbage_collector_stats();
    ASSERT_GT(gc_stats.temp_tuples_collected, 9000);
}
```

**7.4 Cross-Parser Compatibility Tests:**
```cpp
TEST(TemporaryTables, FirebirdSyntax) {
    execute("CREATE GLOBAL TEMPORARY TABLE fb_temp (id INT) ON COMMIT DELETE ROWS");
    // Test with Firebird parser
}

TEST(TemporaryTables, PostgreSQLSyntax) {
    execute("CREATE TEMPORARY TABLE pg_temp (id INT)");  // Session-scoped by default
    execute("CREATE TEMPORARY TABLE pg_temp2 (id INT) ON COMMIT DROP");
    // Test with PostgreSQL parser
}

TEST(TemporaryTables, MySQLSyntax) {
    execute("CREATE TEMPORARY TABLE mysql_temp (id INT)");  // Session-scoped
    // Test with MySQL parser
}
```

**7.5 Error Tests:**
```cpp
TEST(TemporaryTables, ErrorHandling) {
    // Can't create permanent table with same name as active temp table
    execute("CREATE TEMPORARY TABLE conflict_test (id INT)");
    EXPECT_THROW(execute("CREATE TABLE conflict_test (id INT)"), TableAlreadyExistsException);

    // Can't specify ON COMMIT for permanent table
    EXPECT_THROW(execute("CREATE TABLE bad_on_commit (id INT) ON COMMIT DELETE ROWS"),
                 SyntaxException);

    // Rollback should clean up temp table data
    begin_transaction();
    execute("CREATE GLOBAL TEMPORARY TABLE rollback_test (id INT)");
    execute("INSERT INTO rollback_test VALUES (1)");
    rollback_transaction();

    // Temp table schema still exists (for GTT)
    auto result = execute("SELECT * FROM rollback_test");
    ASSERT_EQ(result.row_count(), 0);
}
```

**Acceptance Criteria:**
- [ ] All test suites pass (100% pass rate)
- [ ] No memory leaks (valgrind clean)
- [ ] No race conditions (thread safety tests)
- [ ] Cross-parser compatibility verified
- [ ] Restart/persistence behavior correct
- [ ] Garbage collection works correctly

---

## 4. Implementation Checklist

### Pre-Implementation
- [ ] Read and understand MGA_RULES.md
- [ ] Read transaction specifications (TRANSACTION_MAIN.md, TRANSACTION_MGA_CORE.md)
- [ ] Review existing catalog infrastructure
- [ ] Review existing transaction manager implementation

### Phase 1: AST and Type Definitions (1 day)
- [ ] Add TemporaryScope enum
- [ ] Add OnCommitAction enum
- [ ] Extend CreateTableStatement
- [ ] Add serialization support
- [ ] Verify AST compilation

### Phase 2: Parser Updates (2 days)
- [ ] Update V2 parser (Firebird syntax)
- [ ] Update PostgreSQL parser
- [ ] Update MySQL parser
- [ ] Update Firebird parser (legacy)
- [ ] Add parser tests for each dialect
- [ ] Verify all syntaxes parse correctly

### Phase 3: Bytecode Generator (1 day)
- [ ] Extend CREATE_TABLE opcode payload
- [ ] Add temp table metadata to bytecode
- [ ] Test bytecode round-trip
- [ ] Verify serialization/deserialization

### Phase 4: Catalog Manager (2 days)
- [ ] Extend scratchbird_tables schema
- [ ] Implement createTemporaryTable()
- [ ] Implement table name resolution with shadowing
- [ ] Implement dropUserTemporaryTablesOnCommit()
- [ ] Implement dropTemporaryTablesOnDisconnect()
- [ ] Add catalog tests

### Phase 5: Executor and Visibility (2 days)
- [ ] Implement tuple_satisfies_mvcc_with_temp_check()
- [ ] Extend insert/update/delete for temp tables
- [ ] Implement markTemporaryTableDataForGC()
- [ ] Implement ON COMMIT action handling
- [ ] Add visibility tests

### Phase 6: Garbage Collector Extensions (1 day)
- [ ] Implement heap_tuple_is_dead_temp()
- [ ] Add session cleanup integration
- [ ] Test GC with temp table tuples
- [ ] Verify no memory leaks

### Phase 7: Integration and Testing (1-2 days)
- [ ] Write transaction-scoped GTT tests
- [ ] Write session-scoped GTT tests
- [ ] Write UTT tests
- [ ] Write restart/persistence tests
- [ ] Write garbage collection tests
- [ ] Write cross-parser compatibility tests
- [ ] Write error handling tests
- [ ] Run full test suite (all parsers)
- [ ] Performance regression tests
- [ ] Memory leak tests (valgrind)
- [ ] Thread safety tests
- [ ] Security audit (no cross-session leakage)

### Post-Implementation
- [ ] Update user documentation
- [ ] Update developer documentation
- [ ] Update API documentation
- [ ] Write migration guide (if breaking changes)
- [ ] Update release notes
- [ ] Peer review
- [ ] QA sign-off
- [ ] Leadership sign-off

---

## 5. Success Criteria

### Functional Requirements (ALL MANDATORY)
- [ ] Create temporary tables in all 4 parsers → verify session-scoped
- [ ] Transaction-scoped GTT: only own transaction sees data
- [ ] Session-scoped GTT: all transactions in session see data
- [ ] UTT: DDL rolled back on commit
- [ ] ON COMMIT DELETE ROWS works
- [ ] ON COMMIT PRESERVE ROWS works
- [ ] ON COMMIT DROP works
- [ ] Cleanup on disconnect verified
- [ ] Restart persistence tests pass (GTT schema survives, UTT does not)
- [ ] No cross-session data leakage

### Technical Requirements (ALL MANDATORY)
- [ ] All parser test suites passing (100% pass rate)
- [ ] Integration tests passing (all 4 parsers)
- [ ] Cross-parser compatibility tests passing
- [ ] Performance regression tests passing (no slowdowns)
- [ ] Memory leak tests passing (valgrind clean)
- [ ] Security audit passing (no temp table leakage)
- [ ] Thread safety tests passing
- [ ] Crash recovery tests passing
- [ ] No critical bugs in issue tracker
- [ ] No medium bugs in issue tracker

### Documentation Requirements (ALL MANDATORY)
- [ ] User documentation complete (all features)
- [ ] Developer documentation updated
- [ ] API documentation current
- [ ] Migration guides provided for breaking changes
- [ ] Release notes documenting all changes
- [ ] Known limitations documented (if any)

---

## 6. Risk Assessment

### High Risk
1. **Complexity Risk:** Cross-cutting changes affecting parsers, catalog, executor, and GC
   - **Mitigation:** Incremental implementation, extensive testing after each phase

2. **Session Cleanup Risk:** Memory leaks or orphaned temp tables on disconnect
   - **Mitigation:** Explicit cleanup hooks in ConnectionContext, leak detection tests

3. **Visibility Risk:** Incorrect visibility rules leading to cross-transaction/session leakage
   - **Mitigation:** Security-focused testing, formal verification of visibility logic

### Medium Risk
1. **Performance Risk:** Overhead of checking table_type during visibility checks
   - **Mitigation:** Use hint bits for fast path, benchmark critical paths

2. **Restart Risk:** GTT schema not persisting or UTT schema persisting incorrectly
   - **Mitigation:** Extensive restart tests, catalog persistence validation

### Low Risk
1. **Parser Syntax Risk:** Minor differences in syntax across dialects
   - **Mitigation:** Reference official Firebird/PostgreSQL/MySQL documentation

---

## 7. Alternative Approaches Considered

### Alternative 1: Separate Storage Directories (REJECTED)
**Approach:** Create `/temp/{session_id}/` directories for session-scoped storage.
**Why Rejected:**
- Violates Firebird MGA principles
- Unnecessary complexity (MGA already provides isolation)
- Performance overhead (separate storage management)

### Alternative 2: Reject Syntax with Error (REJECTED)
**Approach:** Parse temporary table syntax but reject with error message.
**Why Rejected:**
- Not acceptable for production-grade Alpha
- Breaks SQL compatibility claims
- User trust issue (silent failures)

### Alternative 3: Defer to Beta (NOT ALLOWED)
**Approach:** Implement partial solution in Alpha, complete in Beta.
**Why Not Allowed:**
- Absolute Alpha requirement: 100% completion
- Parser layer is foundation - cannot be incomplete
- Project leadership mandate

---

## 8. Open Questions (RESOLVED)

### Q1: How to mark temp table data for GC without marking transaction as ABORTED?
**A:** Use HEAP_XMIN_TEMP_TABLE hint bit. GC checks this bit and table_type to determine if tuple is dead even when transaction is COMMITTED.

### Q2: Should session_id be stored in tuple header or separate structure?
**A:** Store in tuple header (t_session_id field) for session-scoped GTT. This adds 8 bytes overhead per tuple but provides fast visibility checks without external lookups.

### Q3: Do GTT schemas persist across restarts?
**A:** YES for GLOBAL TEMPORARY TABLE (schema is permanent, data is temporary). NO for user temporary tables (both schema and data are temporary).

### Q4: How to handle concurrent CREATE TEMPORARY TABLE with same name?
**A:**
- UTT: Allow, each transaction has its own table definition (in-memory only)
- Session GTT: Allow, each session has its own table (catalog tracks owner_session_id)
- Transaction GTT: Schema is permanent, only one definition exists

---

## 9. Dependencies

### Required Infrastructure (Must Exist Before Implementation)
- [x] Transaction Manager with TIP support
- [x] MGA visibility rules (tuple_satisfies_mvcc)
- [x] Catalog Manager with TableInfo
- [x] ConnectionContext with session tracking
- [x] Garbage collector with tuple_is_dead logic
- [x] All 4 parsers (V2, Firebird, PostgreSQL, MySQL)

### Optional Enhancements (Nice to Have)
- [ ] WAL integration for temporary table DDL (optional for Alpha)
- [ ] Statistics collection for temporary table usage
- [ ] Query planner optimizations for temp tables (no persistence needed)

---

## 10. Timeline and Resource Allocation

**Estimated Effort:** 8-10 days (1 developer, full-time)

**Day 1:** Phase 1 - AST and Type Definitions
**Day 2-3:** Phase 2 - Parser Updates (all 4 parsers)
**Day 4:** Phase 3 - Bytecode Generator
**Day 5-6:** Phase 4 - Catalog Manager
**Day 7-8:** Phase 5 - Executor and Visibility
**Day 9:** Phase 6 - Garbage Collector Extensions
**Day 10:** Phase 7 - Integration and Testing

**Buffer:** +2 days for unexpected issues

**Total:** 10-12 days

---

## 11. References

**Internal Documents:**
- `/docs/archive/2026-01-09/planning/PARSER_REMEDIATION_MASTER_PLAN.md` - Overall plan
- `/docs/archive/2026-01-09/planning/PARSER_ALPHA_REQUIREMENTS_FINAL.md` - Alpha requirements
- `/MGA_RULES.md` - **CRITICAL** - MGA architecture rules
- `/docs/specifications/TRANSACTION_MAIN.md` - Transaction system
- `/docs/specifications/TRANSACTION_MGA_CORE.md` - MGA implementation
- `/docs/specifications/EMULATED_DATABASE_PARSER_SPECIFICATION.md` - Parser rules

**External References:**
- Firebird 5.0 Language Reference - Global Temporary Tables
- PostgreSQL Documentation - Temporary Tables
- MySQL Documentation - CREATE TEMPORARY TABLE

---

**Status:** READY FOR IMPLEMENTATION
**Next Action:** Assign developer and begin Phase 1
**Blocking Issues:** None - all dependencies satisfied

---

**End of Implementation Plan**
