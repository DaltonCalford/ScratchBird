# Offline Table Migration - Design Document

**Feature**: `ALTER TABLE ... SET TABLESPACE` (Offline Mode)
**Phase**: 4 - Migration
**Task**: 4.1 - Offline Table Migration
**Estimated Effort**: 20-28 hours across 4 sessions
**Status**: 📋 DESIGN COMPLETE - IMPLEMENTATION PENDING

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Session Breakdown](#session-breakdown)
4. [Detailed Design](#detailed-design)
5. [Data Structures](#data-structures)
6. [Error Handling](#error-handling)
7. [Testing Strategy](#testing-strategy)
8. [Performance Considerations](#performance-considerations)
9. [Session Todo Lists](#session-todo-lists)

---

## Overview

### Goal
Implement offline table migration between tablespaces using the `ALTER TABLE ... SET TABLESPACE` command. This allows users to move existing tables from one tablespace to another without requiring online/concurrent access.

### Syntax
```sql
ALTER TABLE table_name SET TABLESPACE tablespace_name;
ALTER TABLE table_name SET TABLESPACE tablespace_name ONLINE;  -- Rejected in Phase 4
```

### Constraints (Phase 4 - Offline Only)
- **Exclusive Lock Required**: Table is locked for entire migration duration
- **No Concurrent Access**: All queries blocked during migration
- **Single Transaction**: Migration happens in one atomic transaction
- **All-or-Nothing**: Complete success or complete rollback
- **ONLINE Clause**: Parsed but rejected (deferred to Phase 5)

### User Impact
- **Downtime**: Table unavailable during migration (can be minutes to hours for large tables)
- **Cancellation**: User can cancel migration (Ctrl+C) - changes rolled back
- **Progress Visibility**: Periodic progress logs (every 1000 pages or 5 seconds)

---

## Architecture

### Component Layers

```
┌─────────────────────────────────────────────────────────────┐
│                    SQL Layer (User Input)                    │
│  ALTER TABLE employees SET TABLESPACE fast_storage;         │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│                  Parser Layer (grammar.y)                    │
│  - Lexical Analysis                                          │
│  - Syntax Parsing                                            │
│  - AST Node Creation: AlterTableSetTablespaceStmt           │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│            Bytecode Generator (bytecode_generator.cpp)       │
│  - Visit AlterTableSetTablespaceStmt                         │
│  - Generate: OP_ALTER_TABLE_SET_TABLESPACE bytecode         │
│  - Encode: table_name, tablespace_name, online_flag         │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              Executor Layer (executor.cpp)                   │
│  - Decode bytecode                                           │
│  - Resolve table_id from table_name                          │
│  - Resolve tablespace_id from tablespace_name                │
│  - Validate ONLINE flag (reject if true in Phase 4)          │
│  - Call: catalog_mgr->moveTableToTablespace(...)             │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│         Core Migration Logic (catalog_manager.cpp)           │
│  CatalogManager::moveTableToTablespace()                     │
│                                                              │
│  8-Step Migration Process:                                  │
│  1. Acquire EXCLUSIVE lock on table                          │
│  2. Resolve target tablespace_id                             │
│  3. Allocate pages in target tablespace                      │
│  4. Copy heap pages (with TID mapping)                       │
│  5. Update all indexes (apply TID mapping)                   │
│  6. Update catalog (TableInfo.tablespace_id)                 │
│  7. Free old pages in source tablespace                      │
│  8. Release EXCLUSIVE lock                                   │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────────────┐
│           Storage Layer (page_manager.cpp)                   │
│  - allocatePageInTablespace() - Allocate new pages           │
│  - freePageGlobal() - Free old pages                         │
│  - Page I/O operations                                       │
└─────────────────────────────────────────────────────────────┘
```

---

## Session Breakdown

### **Session 1: Parser and AST Foundation** (3-4 hours)
**Goal**: Add language support for `ALTER TABLE ... SET TABLESPACE`

**Deliverables**:
- Grammar changes in `sql/parser/grammar.y`
- New AST node: `AlterTableSetTablespaceStmt`
- Visitor pattern support
- Basic bytecode generation stub

**Files Modified**:
- `sql/parser/grammar.y` (~60 lines)
- `include/scratchbird/parser/ast.h` (~40 lines)
- `include/scratchbird/sblr/bytecode_generator.h` (~5 lines)
- `src/scratchbird/sblr/bytecode_generator.cpp` (~30 lines)
- `include/scratchbird/sblr/opcodes.h` (~2 lines)

**Success Criteria**:
- [ ] Parser accepts `ALTER TABLE t1 SET TABLESPACE ts1;`
- [ ] Parser accepts `ALTER TABLE t1 SET TABLESPACE ts1 ONLINE;`
- [ ] AST node created with correct fields
- [ ] Bytecode generated (basic stub)
- [ ] Compiles without errors

---

### **Session 2: Core Migration Engine** (10-12 hours)
**Goal**: Implement the core table migration logic

**Deliverables**:
- `CatalogManager::moveTableToTablespace()` method
- Heap page scanning and copying
- TID mapping construction (`old_gpid → new_gpid`)
- Basic index update framework
- Page allocation/deallocation

**Files Modified**:
- `include/scratchbird/core/catalog_manager.h` (~50 lines)
- `src/core/catalog_manager.cpp` (~600-700 lines)

**Success Criteria**:
- [ ] Can copy all heap pages from source to target tablespace
- [ ] TID mapping correctly tracks all page moves
- [ ] Catalog updated with new tablespace_id
- [ ] Old pages freed
- [ ] Single table migration works (without indexes)

---

### **Session 3: Index Updates and Progress Tracking** (5-7 hours)
**Goal**: Complete index support and add user-facing progress features

**Deliverables**:
- Index TID update logic for all 6 index types
- Progress tracking (pages copied / total pages)
- Periodic logging (every 1000 pages or 5 seconds)
- Cancellation support (graceful rollback)
- Large table batching strategy

**Files Modified**:
- `src/core/catalog_manager.cpp` (add ~200 lines)
- Potentially: index-specific update methods

**Success Criteria**:
- [ ] All index types updated correctly
- [ ] Progress logs appear during migration
- [ ] Can cancel migration with Ctrl+C (rollback works)
- [ ] Large tables (1M+ rows) migrate successfully

---

### **Session 4: Executor Integration and Testing** (3-5 hours)
**Goal**: Wire everything together and validate end-to-end

**Deliverables**:
- Executor handler: `ExecuteAlterTableSetTablespace()`
- ONLINE clause validation (reject in Phase 4)
- Comprehensive error handling
- Integration tests
- Documentation updates

**Files Modified**:
- `sql/executor/executor.cpp` (~150-200 lines)
- `docs/planning/TABLESPACE_IMPLEMENTATION_PLAN.md` (update status)
- Test files (if applicable)

**Success Criteria**:
- [ ] End-to-end migration works: `ALTER TABLE t1 SET TABLESPACE ts2;`
- [ ] ONLINE clause rejected with clear error message
- [ ] All edge cases handled (errors, invalid inputs)
- [ ] Integration test passes (10,000 rows + indexes)

---

## Detailed Design

### Session 1 Details: Parser and AST

#### 1.1 Grammar Changes (`sql/parser/grammar.y`)

**New Productions**:
```yacc
alter_table_stmt:
    ALTER TABLE table_name SET TABLESPACE identifier
    {
        $$ = new AlterTableSetTablespaceStmt(@$, $3, $6, false);
    }
    | ALTER TABLE table_name SET TABLESPACE identifier ONLINE
    {
        $$ = new AlterTableSetTablespaceStmt(@$, $3, $6, true);
    }
    ;

table_name:
    identifier
    {
        $$ = $1;
    }
    ;
```

**Keywords to Add**:
- `ALTER` (likely already exists)
- `TABLE` (likely already exists)
- `SET` (may need to add)
- `ONLINE` (new keyword)

**Token Definitions**:
```yacc
%token <string_id> ALTER TABLE SET TABLESPACE ONLINE
```

#### 1.2 AST Node (`include/scratchbird/parser/ast.h`)

**New Class**:
```cpp
class AlterTableSetTablespaceStmt : public Statement
{
public:
    AlterTableSetTablespaceStmt(
        const SourceSpan &span,
        StringPool::StringId table_name,
        StringPool::StringId tablespace_name,
        bool online
    )
        : Statement(span)
        , table_name_(table_name)
        , tablespace_name_(tablespace_name)
        , online_(online)
    {
    }

    void accept(ASTVisitor *visitor) override
    {
        visitor->visit(this);
    }

    StringPool::StringId tableName() const { return table_name_; }
    StringPool::StringId tablespaceName() const { return tablespace_name_; }
    bool online() const { return online_; }

private:
    StringPool::StringId table_name_;
    StringPool::StringId tablespace_name_;
    bool online_;  // true if ONLINE clause present
};
```

**Visitor Interface Addition**:
```cpp
class ASTVisitor
{
public:
    // ... existing methods ...
    virtual void visit(AlterTableSetTablespaceStmt *node) = 0;  // Phase 4 Task 4.1
};
```

#### 1.3 Bytecode Generation (`bytecode_generator.cpp`)

**New Opcode** (`include/scratchbird/sblr/opcodes.h`):
```cpp
enum class Opcode : uint8_t
{
    // ... existing opcodes ...
    OP_ALTER_TABLE_SET_TABLESPACE = 0x??,  // Phase 4 Task 4.1
};
```

**Bytecode Format**:
```
┌─────────────────────────────────────────────────┐
│ OP_ALTER_TABLE_SET_TABLESPACE (1 byte)          │
├─────────────────────────────────────────────────┤
│ table_name_length (4 bytes)                     │
├─────────────────────────────────────────────────┤
│ table_name (variable bytes)                     │
├─────────────────────────────────────────────────┤
│ tablespace_name_length (4 bytes)                │
├─────────────────────────────────────────────────┤
│ tablespace_name (variable bytes)                │
├─────────────────────────────────────────────────┤
│ online_flag (1 byte: 0=offline, 1=online)       │
└─────────────────────────────────────────────────┘
```

**Generator Method**:
```cpp
void BytecodeGenerator::visit(AlterTableSetTablespaceStmt *node)
{
    current_result_->writeOpcode(Opcode::OP_ALTER_TABLE_SET_TABLESPACE);

    // Write table name
    writeStringId(node->tableName());

    // Write tablespace name
    writeStringId(node->tablespaceName());

    // Write online flag
    current_result_->writeByte(node->online() ? 1 : 0);
}
```

---

### Session 2 Details: Core Migration Engine

#### 2.1 Method Signature (`catalog_manager.h`)

```cpp
/**
 * moveTableToTablespace - Move a table to a different tablespace (OFFLINE mode)
 *
 * @param table_id Table ID to move
 * @param target_tablespace_id Destination tablespace ID
 * @param online If true, use online migration (REJECTED in Phase 4)
 * @param progress_callback Optional callback for progress updates
 * @param ctx Error context
 * @return Status::OK on success, error status otherwise
 *
 * Offline Migration Process (8 steps):
 * 1. Acquire EXCLUSIVE lock on table (blocks all access)
 * 2. Validate target tablespace exists and is different from current
 * 3. Allocate new heap pages in target tablespace
 * 4. Scan all heap pages in source tablespace:
 *    - For each tuple: Copy to new page, preserving slot number
 *    - Build TID mapping: old_gpid → new_gpid (slot unchanged)
 * 5. Update all indexes for this table:
 *    - Scan each index, apply TID mapping (old_gpid → new_gpid)
 * 6. Update catalog: TableInfo.tablespace_id = target_tablespace_id
 * 7. Free old heap pages in source tablespace
 * 8. Release EXCLUSIVE lock
 *
 * Thread-safe: Acquires exclusive table lock.
 * Transaction: Single atomic transaction (all-or-nothing).
 * Cancellation: Can be cancelled via progress_callback returning false.
 */
Status moveTableToTablespace(
    uint32_t table_id,
    uint16_t target_tablespace_id,
    bool online,
    std::function<bool(uint32_t pages_copied, uint32_t total_pages)> progress_callback = nullptr,
    ErrorContext *ctx = nullptr
);
```

#### 2.2 Data Structures

**TID Mapping Structure**:
```cpp
/**
 * TIDMapping - Maps old heap page GPIDs to new GPIDs
 *
 * Used during table migration to track where each page moved.
 * Slot numbers within pages remain unchanged.
 */
struct TIDMapping
{
    std::unordered_map<GPID, GPID> page_mapping;  // old_gpid -> new_gpid

    // Helper: Map a full TID (GPID + slot) to new TID
    TID mapTID(const TID &old_tid) const
    {
        auto it = page_mapping.find(old_tid.gpid);
        if (it == page_mapping.end())
        {
            // Page not found in mapping - should not happen
            throw std::runtime_error("TID mapping not found for page");
        }

        TID new_tid;
        new_tid.gpid = it->second;     // New page GPID
        new_tid.slot = old_tid.slot;   // Slot unchanged
        return new_tid;
    }
};
```

**Progress Tracking**:
```cpp
struct MigrationProgress
{
    uint32_t total_pages = 0;          // Total heap pages to copy
    uint32_t pages_copied = 0;         // Pages copied so far
    uint32_t total_indexes = 0;        // Total indexes to update
    uint32_t indexes_updated = 0;      // Indexes updated so far
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    std::chrono::time_point<std::chrono::steady_clock> last_log_time;

    bool shouldLog() const
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time);
        return elapsed.count() >= 5;  // Log every 5 seconds
    }

    void logProgress() const
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        double pages_per_sec = pages_copied / std::max(1.0, static_cast<double>(elapsed.count()));

        LOG_INFO(CATALOG,
                "Table migration progress: %u/%u pages (%.1f%%), %u/%u indexes, %.1f pages/sec",
                pages_copied, total_pages,
                (pages_copied * 100.0) / std::max(1u, total_pages),
                indexes_updated, total_indexes,
                pages_per_sec);
    }
};
```

#### 2.3 Implementation Outline (`catalog_manager.cpp`)

```cpp
Status CatalogManager::moveTableToTablespace(
    uint32_t table_id,
    uint16_t target_tablespace_id,
    bool online,
    std::function<bool(uint32_t, uint32_t)> progress_callback,
    ErrorContext *ctx)
{
    // ===== STEP 0: Reject ONLINE mode in Phase 4 =====
    if (online)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                         "ONLINE table migration not implemented in Phase 4 (deferred to Phase 5)");
        return Status::NOT_IMPLEMENTED;
    }

    // ===== STEP 1: Acquire EXCLUSIVE lock on table =====
    // TODO: Implement lock manager integration
    // For now, document that this blocks all concurrent access
    LOG_INFO(CATALOG, "Acquiring EXCLUSIVE lock on table %u", table_id);

    // ===== STEP 2: Validate inputs and get table info =====
    std::lock_guard<std::mutex> lock(mutex_);

    auto table_it = table_cache_.find(table_id);
    if (table_it == table_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("Table ID " + std::to_string(table_id) + " not found").c_str());
        return Status::NOT_FOUND;
    }

    TableInfo &table_info = table_it->second;
    uint16_t source_tablespace_id = table_info.tablespace_id;

    // Check if already in target tablespace
    if (source_tablespace_id == target_tablespace_id)
    {
        LOG_INFO(CATALOG, "Table %u already in tablespace %u, nothing to do",
                table_id, target_tablespace_id);
        return Status::OK;
    }

    // Validate target tablespace exists
    auto ts_it = tablespace_cache_.find(target_tablespace_id);
    if (ts_it == tablespace_cache_.end())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("Target tablespace " + std::to_string(target_tablespace_id) +
                          " not found").c_str());
        return Status::NOT_FOUND;
    }

    LOG_INFO(CATALOG,
            "Starting table migration: table_id=%u, from_tablespace=%u, to_tablespace=%u",
            table_id, source_tablespace_id, target_tablespace_id);

    // ===== STEP 3: Initialize progress tracking =====
    MigrationProgress progress;
    progress.start_time = std::chrono::steady_clock::now();
    progress.last_log_time = progress.start_time;

    // Count total heap pages
    // TODO: Scan heap to count pages
    // For now, estimate from table size
    progress.total_pages = table_info.row_count / 100;  // Rough estimate

    // Count indexes
    progress.total_indexes = 0;
    for (const auto &idx_pair : index_cache_)
    {
        if (idx_pair.second.table_id == table_id)
        {
            progress.total_indexes++;
        }
    }

    // ===== STEP 4: Allocate new heap pages in target tablespace =====
    TIDMapping tid_mapping;

    // TODO: Scan all heap pages in source tablespace
    // For each page:
    //   1. Allocate new page in target tablespace
    //   2. Copy page contents
    //   3. Update page header (new GPID)
    //   4. Record mapping: old_gpid -> new_gpid
    //   5. Update progress

    LOG_INFO(CATALOG, "Copying %u heap pages to tablespace %u",
            progress.total_pages, target_tablespace_id);

    // ===== STEP 5: Update all indexes =====
    LOG_INFO(CATALOG, "Updating %u indexes", progress.total_indexes);

    for (auto &idx_pair : index_cache_)
    {
        IndexInfo &idx_info = idx_pair.second;
        if (idx_info.table_id != table_id)
        {
            continue;  // Not an index on this table
        }

        // TODO: Update index TIDs
        // For each index entry:
        //   1. Read TID from index
        //   2. Apply TID mapping: old_gpid -> new_gpid (slot unchanged)
        //   3. Write updated TID back to index

        progress.indexes_updated++;

        if (progress.shouldLog())
        {
            progress.logProgress();
            progress.last_log_time = std::chrono::steady_clock::now();
        }

        // Check for cancellation
        if (progress_callback && !progress_callback(progress.pages_copied, progress.total_pages))
        {
            LOG_WARNING(CATALOG, "Table migration cancelled by user");
            // TODO: Rollback changes
            SET_ERROR_CONTEXT(ctx, Status::CANCELLED,
                             "Table migration cancelled by user");
            return Status::CANCELLED;
        }
    }

    // ===== STEP 6: Update catalog =====
    table_info.tablespace_id = target_tablespace_id;

    Status status = writeTableRecord(table_info, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(CATALOG, "Failed to update catalog for table %u", table_id);
        return status;
    }

    // ===== STEP 7: Free old heap pages =====
    LOG_INFO(CATALOG, "Freeing %u old heap pages from tablespace %u",
            progress.total_pages, source_tablespace_id);

    // TODO: Free all old pages using freePageGlobal()

    // ===== STEP 8: Release EXCLUSIVE lock =====
    LOG_INFO(CATALOG, "Releasing EXCLUSIVE lock on table %u", table_id);
    // TODO: Implement lock release

    LOG_INFO(CATALOG,
            "Table migration complete: table_id=%u, moved to tablespace=%u, pages=%u, indexes=%u",
            table_id, target_tablespace_id, progress.total_pages, progress.total_indexes);

    return Status::OK;
}
```

---

### Session 3 Details: Index Updates and Progress

#### 3.1 Index Update Strategy

**Per Index Type**:

1. **B-Tree Index**:
   - Scan all leaf pages
   - For each TID in leaf nodes: Apply TID mapping
   - Update leaf page with new TIDs
   - Mark page dirty

2. **Hash Index**:
   - Scan all bucket pages
   - For each TID: Apply TID mapping
   - Update bucket page

3. **GIN (Generalized Inverted Index)**:
   - Scan posting lists
   - For each TID: Apply TID mapping
   - Update posting tree

4. **Bitmap Index**:
   - Scan bitmap pages
   - TID positions may change if GPID changes
   - Rebuild bitmap if necessary

5. **BRIN (Block Range Index)**:
   - Update range summaries with new page GPIDs
   - Min/max values unchanged, only page references

6. **HNSW (Vector Index)**:
   - Update neighbor lists with new TIDs
   - Graph structure unchanged

**Common Pattern**:
```cpp
Status updateIndexTIDs(uint32_t index_id, const TIDMapping &tid_mapping, ErrorContext *ctx)
{
    // 1. Get index info
    auto it = index_cache_.find(index_id);
    if (it == index_cache_.end())
    {
        return Status::NOT_FOUND;
    }

    IndexInfo &idx_info = it->second;

    // 2. Dispatch to type-specific handler
    switch (idx_info.index_type)
    {
        case IndexType::BTREE:
            return updateBTreeIndexTIDs(index_id, tid_mapping, ctx);
        case IndexType::HASH:
            return updateHashIndexTIDs(index_id, tid_mapping, ctx);
        case IndexType::GIN:
            return updateGINIndexTIDs(index_id, tid_mapping, ctx);
        case IndexType::BITMAP:
            return updateBitmapIndexTIDs(index_id, tid_mapping, ctx);
        case IndexType::BRIN:
            return updateBRINIndexTIDs(index_id, tid_mapping, ctx);
        case IndexType::HNSW:
            return updateHNSWIndexTIDs(index_id, tid_mapping, ctx);
        default:
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Unknown index type");
            return Status::INVALID_ARGUMENT;
    }
}
```

#### 3.2 Cancellation Support

**Signal Handling**:
```cpp
// Global flag for cancellation
std::atomic<bool> g_migration_cancelled{false};

// Signal handler
void migration_signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM)
    {
        g_migration_cancelled = true;
    }
}

// In migration loop:
if (g_migration_cancelled)
{
    LOG_WARNING(CATALOG, "Migration cancelled by signal");
    // Rollback logic
    return Status::CANCELLED;
}
```

#### 3.3 Large Table Batching

**Batch Processing**:
```cpp
const uint32_t BATCH_SIZE = 1000;  // Pages per batch

for (uint32_t batch_start = 0; batch_start < total_pages; batch_start += BATCH_SIZE)
{
    uint32_t batch_end = std::min(batch_start + BATCH_SIZE, total_pages);

    // Process batch
    for (uint32_t i = batch_start; i < batch_end; i++)
    {
        // Copy page, update mapping
    }

    // Periodic checkpoint (optional - adds complexity)
    // For Phase 4, single transaction is safer

    // Progress update
    progress.pages_copied = batch_end;
    if (progress.shouldLog())
    {
        progress.logProgress();
    }
}
```

---

### Session 4 Details: Executor Integration

#### 4.1 Executor Handler (`executor.cpp`)

```cpp
Status Executor::ExecuteAlterTableSetTablespace(const uint8_t *bytecode, ErrorContext *ctx)
{
    // Decode bytecode
    size_t offset = 1;  // Skip opcode

    // Read table name
    uint32_t table_name_len = readInt32(bytecode + offset);
    offset += 4;
    std::string table_name(reinterpret_cast<const char *>(bytecode + offset), table_name_len);
    offset += table_name_len;

    // Read tablespace name
    uint32_t tablespace_name_len = readInt32(bytecode + offset);
    offset += 4;
    std::string tablespace_name(reinterpret_cast<const char *>(bytecode + offset), tablespace_name_len);
    offset += tablespace_name_len;

    // Read online flag
    bool online = (bytecode[offset] != 0);
    offset++;

    LOG_INFO(EXECUTOR,
            "Executing ALTER TABLE %s SET TABLESPACE %s %s",
            table_name.c_str(),
            tablespace_name.c_str(),
            online ? "ONLINE" : "");

    // Resolve table_id
    uint32_t table_id = catalog_mgr_->resolveTableId(table_name, ctx);
    if (table_id == INVALID_TABLE_ID)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("Table not found: " + table_name).c_str());
        return Status::NOT_FOUND;
    }

    // Resolve tablespace_id
    uint16_t tablespace_id = catalog_mgr_->resolveTablespaceId(tablespace_name, ctx);
    if (tablespace_id == INVALID_TABLESPACE_ID)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("Tablespace not found: " + tablespace_name).c_str());
        return Status::NOT_FOUND;
    }

    // Call migration
    return catalog_mgr_->moveTableToTablespace(table_id, tablespace_id, online, nullptr, ctx);
}
```

---

## Error Handling

### Error Scenarios

| Error | Handling | Rollback Required |
|-------|----------|-------------------|
| Table not found | Return NOT_FOUND | No |
| Tablespace not found | Return NOT_FOUND | No |
| Already in target tablespace | Return OK (no-op) | No |
| ONLINE in Phase 4 | Return NOT_IMPLEMENTED | No |
| Disk full during allocation | Return IO_ERROR | Yes - free allocated pages |
| Page copy failure | Return IO_ERROR | Yes - free allocated pages |
| Index update failure | Return IO_ERROR | Yes - restore old TIDs, free new pages |
| Catalog update failure | Return IO_ERROR | Yes - full rollback |
| User cancellation | Return CANCELLED | Yes - full rollback |

### Rollback Strategy

**Atomic Transaction**:
- All changes in single transaction
- On any error: Transaction aborted
- Database automatically reverts to pre-migration state

**Manual Cleanup** (if not using transactions):
```cpp
void rollbackMigration(
    const TIDMapping &tid_mapping,
    const std::vector<GPID> &allocated_pages)
{
    // Free all newly allocated pages
    for (const GPID &gpid : allocated_pages)
    {
        page_mgr_->freePageGlobal(gpid, nullptr);
    }

    // Restore old TIDs in indexes (if partially updated)
    // ... reverse TID mapping ...

    LOG_WARNING(CATALOG, "Migration rolled back");
}
```

---

## Testing Strategy

### Unit Tests

1. **Parser Tests**:
   - Valid syntax accepted
   - Invalid syntax rejected
   - ONLINE clause parsed correctly

2. **Bytecode Tests**:
   - Correct bytecode generated
   - Bytecode correctly decoded

3. **TID Mapping Tests**:
   - Mapping creation
   - TID translation correctness

### Integration Tests

**Test 1: Basic Migration**:
```sql
CREATE TABLESPACE ts1 LOCATION '/tmp/ts1.sbts';
CREATE TABLE t1 (id INT, name TEXT) TABLESPACE ts1;
INSERT INTO t1 SELECT generate_series(1, 1000), 'test';

CREATE TABLESPACE ts2 LOCATION '/tmp/ts2.sbts';
ALTER TABLE t1 SET TABLESPACE ts2;

-- Verify: All 1000 rows accessible
SELECT COUNT(*) FROM t1;  -- Should be 1000
```

**Test 2: Large Table Migration**:
```sql
CREATE TABLE big_table (id INT, data TEXT);
INSERT INTO big_table SELECT generate_series(1, 1000000), 'data';

CREATE INDEX idx_big ON big_table(id);

ALTER TABLE big_table SET TABLESPACE fast_storage;

-- Verify: Index still works
SELECT * FROM big_table WHERE id = 500000;
```

**Test 3: Multiple Indexes**:
```sql
CREATE TABLE multi_idx (a INT, b INT, c INT);
CREATE INDEX idx_a ON multi_idx(a);
CREATE INDEX idx_b ON multi_idx(b);
CREATE INDEX idx_c ON multi_idx(c);

INSERT INTO multi_idx SELECT i, i*2, i*3 FROM generate_series(1, 10000) i;

ALTER TABLE multi_idx SET TABLESPACE ts2;

-- Verify: All indexes work
SELECT * FROM multi_idx WHERE a = 5000;
SELECT * FROM multi_idx WHERE b = 10000;
SELECT * FROM multi_idx WHERE c = 15000;
```

**Test 4: Cancellation**:
```sql
-- Start migration in background
ALTER TABLE huge_table SET TABLESPACE ts2;

-- Send SIGINT (Ctrl+C)
-- Verify: Table still in original tablespace
-- Verify: No orphaned pages in ts2
```

**Test 5: Error Handling**:
```sql
-- Disk full scenario
ALTER TABLE t1 SET TABLESPACE full_disk_ts;  -- Should fail gracefully

-- Invalid tablespace
ALTER TABLE t1 SET TABLESPACE nonexistent;  -- Error

-- ONLINE rejection
ALTER TABLE t1 SET TABLESPACE ts2 ONLINE;  -- "Not implemented in Phase 4"
```

---

## Performance Considerations

### Benchmarks

**Small Table** (1,000 rows, 1 index):
- Expected: < 1 second
- Bottleneck: Lock acquisition overhead

**Medium Table** (100,000 rows, 3 indexes):
- Expected: 5-15 seconds
- Bottleneck: Page copying

**Large Table** (1,000,000 rows, 5 indexes):
- Expected: 1-5 minutes
- Bottleneck: Index updates

**Very Large Table** (10,000,000 rows, 5 indexes):
- Expected: 10-60 minutes
- Bottleneck: Disk I/O

### Optimization Opportunities (Future)

1. **Parallel Page Copying**: Use multiple threads
2. **Batch Index Updates**: Update indexes in batches
3. **Incremental Checkpoints**: Commit progress periodically
4. **Online Migration**: Phase 5 - allow concurrent access

---

## Session Todo Lists

See [OFFLINE_TABLE_MIGRATION_TODOS.md](./OFFLINE_TABLE_MIGRATION_TODOS.md) for detailed session-by-session todo lists.

---

## References

- PostgreSQL ALTER TABLE SET TABLESPACE: https://www.postgresql.org/docs/current/sql-altertable.html
- Oracle MOVE TABLESPACE: https://docs.oracle.com/en/database/oracle/oracle-database/19/sqlrf/ALTER-TABLE.html
- MySQL Tablespace Management: https://dev.mysql.com/doc/refman/8.0/en/innodb-tablespace-management.html

---

**Document Status**: ✅ COMPLETE - Ready for implementation
**Next Step**: Begin Session 1 (Parser and AST Foundation)
**Estimated Total Time**: 20-28 hours across 4 sessions
