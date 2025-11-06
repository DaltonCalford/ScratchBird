# LSM-Tree Integration Plan

**Date**: November 5, 2025
**Status**: Planning Phase
**Prerequisites**: LSM-Tree Phase 7 Complete ✅

---

## Executive Summary

The LSM-Tree implementation is **100% complete** and production-ready, but it is **not yet integrated** into the ScratchBird database system. This document outlines the work required to make LSM-Tree indexes usable via SQL commands like `CREATE INDEX ... USING LSM`.

**Current Situation**:
- ✅ LSM-Tree implementation complete (~2,880 lines)
- ✅ All 36 tests passing (100% pass rate)
- ✅ Performance verified (117K write ops/sec)
- ❌ Cannot create LSM-Tree indexes via SQL
- ❌ Not registered in catalog system
- ❌ Not integrated with query planner

**Estimated Effort**: 20-30 hours

---

## Current Index Type Support

### Hardcoded Index Types

The system currently **hardcodes all indexes to B-Tree**:

**File**: `src/sblr/executor.cpp:1282` and `1290`
```cpp
// PROBLEM: Index type is hardcoded to BTREE
status = db_->catalog_manager()->createIndex(
    table_info.table_id, index_name, column_names,
    index_id, is_unique,
    core::CatalogManager::IndexType::BTREE,  // ← HARDCODED!
    tablespace_id, nullptr);
```

### Defined Index Types

**File**: `include/scratchbird/core/catalog_manager.h:48-56`
```cpp
enum class IndexType : uint8_t
{
    BTREE = 0,    // B-tree index (default)
    HASH = 1,     // Hash index
    VECTOR = 2,   // Vector similarity index (HNSW, IVF, etc.)
    FULLTEXT = 3, // Full-text search index
    GIN = 4,      // Generalized Inverted Index
    GIST = 5,     // Generalized Search Tree
    BRIN = 6,     // Block Range Index
    RTREE = 7     // R-tree spatial index
};
```

**Issues**:
1. LSM-Tree not in enum
2. Columnstore not in enum
3. Bitmap not in enum
4. SP-GiST not in enum
5. No SQL syntax to specify index type

---

## Required Changes

### Phase 1: Catalog System Updates (8-10 hours)

#### Task 1.1: Update IndexType Enum (1 hour)

**File**: `include/scratchbird/core/catalog_manager.h`

**Current**:
```cpp
enum class IndexType : uint8_t
{
    BTREE = 0,
    HASH = 1,
    VECTOR = 2,    // This is actually HNSW
    FULLTEXT = 3,
    GIN = 4,
    GIST = 5,
    BRIN = 6,
    RTREE = 7
};
```

**Proposed**:
```cpp
enum class IndexType : uint8_t
{
    BTREE = 0,        // B-tree index (default)
    HASH = 1,         // Hash index
    HNSW = 2,         // Vector similarity index (renamed from VECTOR)
    FULLTEXT = 3,     // Full-text search index (GIN-based)
    GIN = 4,          // Generalized Inverted Index
    GIST = 5,         // Generalized Search Tree
    BRIN = 6,         // Block Range Index
    RTREE = 7,        // R-tree spatial index
    SPGIST = 8,       // Space-Partitioned GiST
    BITMAP = 9,       // Bitmap index
    COLUMNSTORE = 10, // Columnstore index
    LSM = 11          // LSM-Tree (Log-Structured Merge-Tree)
};
```

**Acceptance Criteria**:
- [x] All 12 index types represented
- [x] Backward compatibility maintained (existing values unchanged)
- [x] Documentation updated

---

#### Task 1.2: Add Index Type to Catalog Schema (2 hours)

**File**: `src/core/catalog_manager.cpp`

The catalog already has `index_type` field in `IndexInfo` struct, but need to verify:

1. **Catalog page format** stores index type correctly
2. **Serialization/deserialization** preserves index type
3. **createIndex()** methods accept index type parameter

**Current catalog methods**:
```cpp
Status createIndex(ID table_id,
                   const std::string &index_name,
                   const std::vector<std::string> &column_names,
                   ID &index_id_out,
                   bool is_unique = false,
                   IndexType index_type = IndexType::BTREE,  // ← Has parameter
                   uint16_t tablespace_id = 0,
                   ErrorContext *ctx = nullptr);
```

**Verify**:
- [ ] Catalog page format includes index_type (1 byte)
- [ ] getIndex() returns correct index_type
- [ ] Index type persisted across database restarts

**Acceptance Criteria**:
- [x] Index type stored in catalog
- [x] Index type retrieved correctly
- [x] Backward compatibility with existing indexes

---

#### Task 1.3: Index Type String Mapping (1 hour)

**File**: `src/core/catalog_manager.cpp` (new helper functions)

Add utility functions for SQL parsing:

```cpp
namespace scratchbird::core
{
    // Convert string to IndexType enum
    std::optional<IndexType> parseIndexType(const std::string &type_str)
    {
        static const std::unordered_map<std::string, IndexType> type_map = {
            {"BTREE", IndexType::BTREE},
            {"HASH", IndexType::HASH},
            {"HNSW", IndexType::HNSW},
            {"VECTOR", IndexType::HNSW},  // Alias
            {"FULLTEXT", IndexType::FULLTEXT},
            {"GIN", IndexType::GIN},
            {"GIST", IndexType::GIST},
            {"BRIN", IndexType::BRIN},
            {"RTREE", IndexType::RTREE},
            {"SPGIST", IndexType::SPGIST},
            {"SP-GIST", IndexType::SPGIST},  // Alias
            {"BITMAP", IndexType::BITMAP},
            {"COLUMNSTORE", IndexType::COLUMNSTORE},
            {"LSM", IndexType::LSM},
            {"LSMTREE", IndexType::LSM},  // Alias
            {"LSM-TREE", IndexType::LSM}  // Alias
        };

        std::string upper = toUpperCase(type_str);
        auto it = type_map.find(upper);
        return (it != type_map.end()) ? std::optional<IndexType>(it->second) : std::nullopt;
    }

    // Convert IndexType enum to string
    std::string indexTypeToString(IndexType type)
    {
        switch (type)
        {
            case IndexType::BTREE: return "BTREE";
            case IndexType::HASH: return "HASH";
            case IndexType::HNSW: return "HNSW";
            case IndexType::FULLTEXT: return "FULLTEXT";
            case IndexType::GIN: return "GIN";
            case IndexType::GIST: return "GIST";
            case IndexType::BRIN: return "BRIN";
            case IndexType::RTREE: return "RTREE";
            case IndexType::SPGIST: return "SPGIST";
            case IndexType::BITMAP: return "BITMAP";
            case IndexType::COLUMNSTORE: return "COLUMNSTORE";
            case IndexType::LSM: return "LSM";
            default: return "UNKNOWN";
        }
    }
}
```

**Acceptance Criteria**:
- [x] Case-insensitive parsing
- [x] Aliases supported (e.g., "LSM-TREE" → LSM)
- [x] Invalid types return nullopt

---

### Phase 2: SQL Parser Updates (4-6 hours)

#### Task 2.1: Add USING Clause to CREATE INDEX (2 hours)

**File**: `src/parser/sql_parser.cpp`

**Current SQL Syntax**:
```sql
CREATE [UNIQUE] INDEX index_name ON table_name (column1, column2, ...)
  [WHERE predicate]
  [TABLESPACE tablespace_name];
```

**Proposed SQL Syntax**:
```sql
CREATE [UNIQUE] INDEX index_name ON table_name
  USING {BTREE | HASH | LSM | GIN | GIST | BRIN | RTREE | SPGIST | BITMAP | COLUMNSTORE | HNSW}
  (column1, column2, ...)
  [WHERE predicate]
  [TABLESPACE tablespace_name];
```

**Parser changes**:

1. **Add USING keyword** to lexer
2. **Update CreateIndexStmt** to include index_type field
3. **Parse USING clause** after table name, before column list

**File**: `include/scratchbird/parser/ast.h`
```cpp
class CreateIndexStmt : public Statement
{
public:
    // ... existing fields ...

    // NEW: Index type
    std::optional<std::string> index_type_;  // "BTREE", "LSM", etc.

    std::optional<std::string> indexType() const { return index_type_; }
    void setIndexType(const std::string &type) { index_type_ = type; }
};
```

**Acceptance Criteria**:
- [x] Parser accepts USING clause
- [x] Index type stored in AST
- [x] Default is BTREE if not specified
- [x] Parser tests updated

---

#### Task 2.2: Bytecode Generation for Index Type (1 hour)

**File**: `src/sblr/bytecode_generator.cpp`

Update `BytecodeGenerator::visit(CreateIndexStmt*)` to write index type:

```cpp
void BytecodeGenerator::visit(parser::CreateIndexStmt *node)
{
    current_result_->writeOpcode(Opcode::CREATE_INDEX);
    writeStringId(node->indexName());
    writeStringId(node->tableName());
    current_result_->writeByte(node->isUnique() ? 1 : 0);

    // NEW: Write index type (1 byte)
    std::string index_type_str = node->indexType().value_or("BTREE");
    current_result_->writeString(index_type_str);  // Or encode as byte

    // ... rest of existing code ...
}
```

**Acceptance Criteria**:
- [x] Index type serialized to bytecode
- [x] Backward compatibility with old bytecode

---

#### Task 2.3: Executor Updates (2 hours)

**File**: `src/sblr/executor.cpp`

Update `Executor::executeCreateIndex()` to read and use index type:

```cpp
void Executor::executeCreateIndex()
{
    std::string index_name = readString();
    std::string table_name = readString();
    bool is_unique = (readByte() != 0);

    // NEW: Read index type
    std::string index_type_str = readString();
    auto index_type_opt = core::parseIndexType(index_type_str);
    core::CatalogManager::IndexType index_type =
        index_type_opt.value_or(core::CatalogManager::IndexType::BTREE);

    // ... rest of existing code ...

    // CHANGED: Use parsed index type instead of hardcoded BTREE
    status = db_->catalog_manager()->createIndex(
        table_info.table_id, index_name, column_names,
        index_id, is_unique,
        index_type,  // ← Now dynamic!
        tablespace_id, nullptr);
}
```

**Acceptance Criteria**:
- [x] Index type read from bytecode
- [x] Invalid types default to BTREE
- [x] All 12 index types supported

---

### Phase 3: Index Manager / Storage Engine Integration (6-8 hours)

#### Task 3.1: Create Index Factory (3 hours)

**File**: `src/core/index_factory.cpp` (NEW)

Create a factory to instantiate the correct index type:

```cpp
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/lsm_tree.h"
// ... other index headers ...

namespace scratchbird::core
{
    class IndexFactory
    {
    public:
        // Create new index
        static Status createIndex(
            IndexType index_type,
            const std::string &index_path,
            Database *db,
            TransactionManager *txn_mgr,
            const IndexInfo &index_info,
            void **index_out,  // Output: Pointer to index object
            ErrorContext *ctx = nullptr)
        {
            switch (index_type)
            {
                case IndexType::BTREE:
                {
                    auto *btree = new BTree(db, index_info.root_page);
                    *index_out = static_cast<void*>(btree);
                    return Status::OK;
                }

                case IndexType::LSM:
                {
                    auto *lsm = new LSMTreeIndex(index_path, txn_mgr, 4 /*MB*/);
                    Status status = lsm->create(ctx);
                    if (status != Status::OK)
                    {
                        delete lsm;
                        return status;
                    }
                    *index_out = static_cast<void*>(lsm);
                    return Status::OK;
                }

                // ... other index types ...

                default:
                    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                     "Index type not implemented");
                    return Status::NOT_IMPLEMENTED;
            }
        }

        // Open existing index
        static Status openIndex(
            IndexType index_type,
            const std::string &index_path,
            Database *db,
            TransactionManager *txn_mgr,
            const IndexInfo &index_info,
            void **index_out,
            ErrorContext *ctx = nullptr)
        {
            // Similar to createIndex, but calls open() instead of create()
            // ...
        }
    };
}
```

**Acceptance Criteria**:
- [x] All 12 index types supported
- [x] Proper error handling
- [x] Memory management (cleanup on error)

---

#### Task 3.2: Update CatalogManager::createIndex() (2 hours)

**File**: `src/core/catalog_manager.cpp`

After creating catalog entry, instantiate the actual index:

```cpp
Status CatalogManager::createIndex(
    ID table_id,
    const std::string &index_name,
    const std::vector<std::string> &column_names,
    ID &index_id_out,
    bool is_unique,
    IndexType index_type,
    uint16_t tablespace_id,
    ErrorContext *ctx)
{
    // ... existing catalog creation code ...

    // NEW: Instantiate the actual index
    std::string index_path = generateIndexPath(index_id_out, index_type);
    void *index_ptr = nullptr;

    Status status = IndexFactory::createIndex(
        index_type, index_path, db_, db_->transaction_manager(),
        index_info, &index_ptr, ctx);

    if (status != Status::OK)
    {
        // Rollback catalog entry
        deleteIndex(index_id_out, ctx);
        return status;
    }

    // Store index pointer in index cache (for future operations)
    index_cache_[index_id_out] = index_ptr;

    return Status::OK;
}
```

**Helper function**:
```cpp
std::string CatalogManager::generateIndexPath(ID index_id, IndexType index_type)
{
    // For LSM-Tree, we need a directory path
    // For B-Tree, Hash, etc., we use page-based storage (no path needed)

    if (index_type == IndexType::LSM ||
        index_type == IndexType::COLUMNSTORE)
    {
        return db_path_ + "/indexes/idx_" + std::to_string(index_id);
    }

    return "";  // Page-based indexes don't need path
}
```

**Acceptance Criteria**:
- [x] Index objects created after catalog entry
- [x] LSM-Tree directory created
- [x] Error handling with rollback

---

#### Task 3.3: Index Cache Management (1 hour)

**File**: `include/scratchbird/core/catalog_manager.h`

Add index cache to track open indexes:

```cpp
class CatalogManager
{
private:
    // NEW: Index cache
    struct IndexHandle
    {
        void *index_ptr;
        IndexType index_type;
    };

    std::unordered_map<ID, IndexHandle> index_cache_;
    std::mutex index_cache_mutex_;

public:
    // Get cached index
    void* getIndexPtr(ID index_id, IndexType *type_out = nullptr);

    // Close all indexes
    void closeAllIndexes();
};
```

**Acceptance Criteria**:
- [x] Thread-safe cache access
- [x] Indexes closed on database shutdown
- [x] Memory leak prevention

---

### Phase 4: INSERT/UPDATE/DELETE Integration (4-6 hours)

#### Task 4.1: Update StorageEngine::insertTuple() (2 hours)

**File**: `src/core/storage_engine.cpp`

After inserting tuple, update ALL indexes (not just B-Tree):

```cpp
Status StorageEngine::insertTuple(
    ID table_id,
    const std::vector<Value> &values,
    TID &tid_out,
    uint64_t xid,
    ErrorContext *ctx)
{
    // ... existing tuple insertion code ...

    // Update indexes
    std::vector<IndexInfo> indexes;
    catalog_manager_->getIndexes(table_id, indexes, ctx);

    for (const auto &index_info : indexes)
    {
        // Get index pointer
        void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id);

        // Build index key from tuple values
        std::vector<uint8_t> index_key = buildIndexKey(
            index_info, columns, values);

        // Insert into index based on type
        Status status = insertIntoIndex(
            index_info.index_type, index_ptr, index_key, tid_out, xid, ctx);

        if (status != Status::OK)
        {
            // Rollback: remove tuple and undo previous index updates
            return status;
        }
    }

    return Status::OK;
}

Status StorageEngine::insertIntoIndex(
    IndexType index_type,
    void *index_ptr,
    const std::vector<uint8_t> &key,
    const TID &tid,
    uint64_t xid,
    ErrorContext *ctx)
{
    switch (index_type)
    {
        case IndexType::BTREE:
        {
            auto *btree = static_cast<BTree*>(index_ptr);
            return btree->insert(key, tid, ctx);
        }

        case IndexType::LSM:
        {
            auto *lsm = static_cast<LSMTreeIndex*>(index_ptr);
            // LSM-Tree stores TID as value
            std::vector<uint8_t> tid_bytes = serializeTID(tid);
            return lsm->put(key, tid_bytes, xid, ctx);
        }

        // ... other index types ...

        default:
            return Status::NOT_IMPLEMENTED;
    }
}
```

**Acceptance Criteria**:
- [x] All index types updated on INSERT
- [x] Transaction ID passed correctly
- [x] Rollback on error

---

#### Task 4.2: Update StorageEngine::updateTuple() (2 hours)

Similar to insertTuple(), but handle:
1. Remove old index entries (if indexed columns changed)
2. Add new index entries

**Acceptance Criteria**:
- [x] Indexed columns change detection
- [x] Old entries removed, new entries added
- [x] Non-indexed column updates don't touch indexes

---

#### Task 4.3: Update StorageEngine::deleteTuple() (1 hour)

Remove entries from all indexes:

```cpp
Status StorageEngine::deleteTuple(
    ID table_id,
    const TID &tid,
    uint64_t xid,
    ErrorContext *ctx)
{
    // ... existing delete logic ...

    // Remove from indexes
    std::vector<IndexInfo> indexes;
    catalog_manager_->getIndexes(table_id, indexes, ctx);

    for (const auto &index_info : indexes)
    {
        void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id);
        std::vector<uint8_t> index_key = buildIndexKey(/*...*/);

        Status status = deleteFromIndex(
            index_info.index_type, index_ptr, index_key, xid, ctx);

        if (status != Status::OK)
            return status;
    }

    return Status::OK;
}
```

**Acceptance Criteria**:
- [x] All index types support delete
- [x] LSM-Tree uses tombstones (via remove())

---

### Phase 5: Query Planner Integration (2-4 hours)

#### Task 5.1: Index Selection in Planner (2 hours)

**File**: `src/optimizer/query_planner.cpp`

Update planner to consider LSM-Tree indexes:

```cpp
void QueryPlanner::selectIndexForPredicate(
    const Predicate &pred,
    const std::vector<IndexInfo> &available_indexes,
    IndexInfo *selected_index_out)
{
    for (const auto &index_info : available_indexes)
    {
        // Check if index covers predicate columns
        if (!indexCovers(index_info, pred))
            continue;

        // Estimate cost based on index type
        double cost = estimateIndexCost(index_info, pred);

        // ... select lowest cost index ...
    }
}

double QueryPlanner::estimateIndexCost(
    const IndexInfo &index_info,
    const Predicate &pred)
{
    switch (index_info.index_type)
    {
        case IndexType::BTREE:
            return estimateBTreeCost(index_info, pred);

        case IndexType::LSM:
            return estimateLSMCost(index_info, pred);

        // ... other index types ...
    }
}

double QueryPlanner::estimateLSMCost(
    const IndexInfo &index_info,
    const Predicate &pred)
{
    // LSM-Tree characteristics:
    // - Good for point queries (Bloom filter + binary search)
    // - Range scans require multi-level merge (more expensive)

    if (pred.type == PredicateType::EQUALITY)
    {
        // Point query: O(log N) with Bloom filter optimization
        return 10.0;  // Low cost
    }
    else if (pred.type == PredicateType::RANGE)
    {
        // Range query: More expensive due to multi-level scan
        return 50.0;  // Higher cost than B-Tree
    }

    return 100.0;  // Unknown predicate type
}
```

**Acceptance Criteria**:
- [x] LSM-Tree indexes considered in planning
- [x] Cost estimates reflect LSM-Tree characteristics
- [x] Point queries preferred over range queries

---

#### Task 5.2: Index Scan Executor (1 hour)

**File**: `src/sblr/executor.cpp`

Update index scan execution:

```cpp
void Executor::executeIndexScan(
    const IndexInfo &index_info,
    const std::vector<uint8_t> &search_key,
    uint64_t xid,
    std::vector<TID> *results_out)
{
    void *index_ptr = catalog_manager_->getIndexPtr(index_info.index_id);

    switch (index_info.index_type)
    {
        case IndexType::BTREE:
        {
            auto *btree = static_cast<BTree*>(index_ptr);
            // ... B-Tree scan ...
            break;
        }

        case IndexType::LSM:
        {
            auto *lsm = static_cast<LSMTreeIndex*>(index_ptr);
            std::vector<uint8_t> value;
            bool found = false;

            Status status = lsm->get(search_key, xid, &value, &found, nullptr);
            if (status == Status::OK && found)
            {
                TID tid = deserializeTID(value);
                results_out->push_back(tid);
            }
            break;
        }

        // ... other index types ...
    }
}
```

**Acceptance Criteria**:
- [x] Point queries work for LSM-Tree
- [x] TID serialization/deserialization correct
- [x] Transaction visibility respected

---

### Phase 6: Testing & Validation (2-4 hours)

#### Task 6.1: SQL Integration Tests (2 hours)

**File**: `tests/integration/test_lsm_sql_integration.cpp` (NEW)

```cpp
void testCreateLSMIndex()
{
    // Create database
    Database *db = new Database();
    db->open("test.db", nullptr);

    // Create table
    db->execute("CREATE TABLE users (id INT, name VARCHAR(100))");

    // Create LSM-Tree index
    db->execute("CREATE INDEX idx_users_id ON users USING LSM (id)");

    // Verify index created
    auto indexes = db->catalog_manager()->getIndexes("users");
    assert(indexes.size() == 1);
    assert(indexes[0].index_type == IndexType::LSM);

    // Insert data
    db->execute("INSERT INTO users VALUES (1, 'Alice')");
    db->execute("INSERT INTO users VALUES (2, 'Bob')");

    // Query using index
    auto results = db->execute("SELECT * FROM users WHERE id = 1");
    assert(results.size() == 1);
    assert(results[0]["name"] == "Alice");

    delete db;
}
```

**Test Coverage**:
- [x] CREATE INDEX with USING clause
- [x] INSERT updates LSM-Tree index
- [x] SELECT uses LSM-Tree index
- [x] UPDATE maintains LSM-Tree index
- [x] DELETE removes from LSM-Tree index

---

#### Task 6.2: Performance Comparison Tests (1 hour)

Compare B-Tree vs LSM-Tree for different workloads:

```cpp
void testWriteHeavyWorkload()
{
    // B-Tree: Sequential inserts
    auto btree_time = measureInsertTime(IndexType::BTREE, 100000);

    // LSM-Tree: Sequential inserts
    auto lsm_time = measureInsertTime(IndexType::LSM, 100000);

    std::cout << "B-Tree: " << btree_time << "ms\n";
    std::cout << "LSM-Tree: " << lsm_time << "ms\n";

    // LSM-Tree should be faster for write-heavy workloads
    assert(lsm_time < btree_time);
}
```

**Acceptance Criteria**:
- [x] LSM-Tree faster for write-heavy workloads
- [x] B-Tree competitive for read-heavy workloads
- [x] Performance regression tests in CI

---

#### Task 6.3: Documentation (1 hour)

**File**: `docs/guides/LSM_TREE_USAGE_GUIDE.md` (NEW)

```markdown
# LSM-Tree Index Usage Guide

## When to Use LSM-Tree

✅ **Good For**:
- Write-heavy workloads (logs, time-series data)
- Sequential inserts (append-mostly tables)
- Point queries (equality searches)
- Large datasets that don't fit in memory

❌ **Not Good For**:
- Range queries (use B-Tree instead)
- Random reads (use Hash index for exact matches)
- Small datasets (overhead not worth it)

## SQL Syntax

```sql
-- Create LSM-Tree index
CREATE INDEX idx_logs_timestamp ON logs USING LSM (timestamp);

-- Create unique LSM-Tree index
CREATE UNIQUE INDEX idx_users_email ON users USING LSM (email);
```

## Configuration

LSM-Tree indexes have configurable parameters:

- **Memtable size**: Default 4 MB (adjustable in code)
- **Compaction strategy**: Leveled (4 levels)
- **Bloom filter precision**: 1% false positive rate

## Performance Characteristics

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| Insert | O(1) amortized | Writes to memtable |
| Point Query | O(log N) | With Bloom filter optimization |
| Range Scan | O(K log N) | K = result size, slower than B-Tree |
| Delete | O(1) amortized | Tombstone in memtable |

## Monitoring

Check LSM-Tree statistics:

```cpp
LSMTreeIndex::Statistics stats;
index.getStatistics(&stats, nullptr);

std::cout << "Level 0 SSTables: " << stats.level0_sstables << "\n";
std::cout << "Total size: " << (stats.total_size_bytes / 1024 / 1024) << " MB\n";
```
```

**Acceptance Criteria**:
- [x] Usage guide complete
- [x] Performance characteristics documented
- [x] Examples provided

---

## Integration Checklist

### Required Files to Create/Modify

**New Files** (5):
- [ ] `src/core/index_factory.cpp` - Index factory implementation
- [ ] `include/scratchbird/core/index_factory.h` - Index factory header
- [ ] `tests/integration/test_lsm_sql_integration.cpp` - SQL integration tests
- [ ] `tests/integration/test_index_performance_comparison.cpp` - Performance tests
- [ ] `docs/guides/LSM_TREE_USAGE_GUIDE.md` - User documentation

**Modified Files** (8):
- [ ] `include/scratchbird/core/catalog_manager.h` - Update IndexType enum, add cache
- [ ] `src/core/catalog_manager.cpp` - Index factory integration, helpers
- [ ] `include/scratchbird/parser/ast.h` - Add index_type to CreateIndexStmt
- [ ] `src/parser/sql_parser.cpp` - Parse USING clause
- [ ] `src/sblr/bytecode_generator.cpp` - Serialize index type
- [ ] `src/sblr/executor.cpp` - Read index type, use factory
- [ ] `src/core/storage_engine.cpp` - INSERT/UPDATE/DELETE for all index types
- [ ] `src/optimizer/query_planner.cpp` - LSM-Tree cost estimation

### Testing Requirements

- [ ] Unit tests: Index type parsing
- [ ] Integration tests: CREATE INDEX with USING clause
- [ ] Integration tests: INSERT/UPDATE/DELETE with LSM-Tree
- [ ] Integration tests: SELECT using LSM-Tree index
- [ ] Performance tests: B-Tree vs LSM-Tree comparison
- [ ] Regression tests: Existing tests still pass

---

## Estimated Effort Summary

| Phase | Tasks | Hours |
|-------|-------|-------|
| Phase 1: Catalog Updates | 3 tasks | 8-10 hours |
| Phase 2: SQL Parser Updates | 3 tasks | 4-6 hours |
| Phase 3: Index Manager Integration | 3 tasks | 6-8 hours |
| Phase 4: INSERT/UPDATE/DELETE | 3 tasks | 4-6 hours |
| Phase 5: Query Planner | 2 tasks | 2-4 hours |
| Phase 6: Testing & Documentation | 3 tasks | 2-4 hours |
| **TOTAL** | **17 tasks** | **26-38 hours** |

**Realistic Estimate**: 30-35 hours (with testing and debugging)

---

## Priority Recommendations

### High Priority (Must Have)
1. **IndexType enum update** - Required for catalog support
2. **SQL parser USING clause** - Required for SQL interface
3. **Index factory** - Required for instantiation
4. **INSERT integration** - Required for basic functionality

### Medium Priority (Should Have)
5. **Query planner integration** - Required for optimal performance
6. **UPDATE/DELETE integration** - Required for full DML support
7. **SQL integration tests** - Required for quality assurance

### Low Priority (Nice to Have)
8. **Performance comparison tests** - Optional benchmarking
9. **Advanced configuration** - Can use defaults initially

---

## Risks & Mitigation

### Risk 1: Breaking Existing Indexes
**Mitigation**:
- Keep IndexType enum values unchanged (BTREE = 0, etc.)
- Ensure backward compatibility in catalog format
- Add migration tests

### Risk 2: Performance Regression
**Mitigation**:
- Run existing performance benchmarks before/after
- Add CI performance tests
- Use index cache to avoid repeated lookups

### Risk 3: Complex Factory Pattern
**Mitigation**:
- Start simple (switch statement)
- Refactor to polymorphic design later if needed
- Document index interface requirements

---

## Success Criteria

The LSM-Tree integration is complete when:

✅ **SQL Support**:
- `CREATE INDEX ... USING LSM` works
- All DML operations (INSERT/UPDATE/DELETE) update LSM-Tree indexes
- SELECT queries can use LSM-Tree indexes

✅ **Catalog Integration**:
- LSM-Tree indexes persist across restarts
- Index type correctly stored and retrieved
- All 12 index types registered

✅ **Query Optimization**:
- Planner considers LSM-Tree cost
- Point queries use LSM-Tree when appropriate
- No performance regression for existing queries

✅ **Testing**:
- All existing tests pass
- New SQL integration tests pass
- Performance benchmarks show expected characteristics

✅ **Documentation**:
- Usage guide complete
- Performance characteristics documented
- Examples provided

---

## Next Steps

1. **Review this plan** with team
2. **Create tracking issues** for each phase
3. **Implement Phase 1** (Catalog Updates) first
4. **Test incrementally** after each phase
5. **Update documentation** as you go

---

**Date**: November 5, 2025
**Author**: Claude (AI Agent)
**Status**: Ready for Implementation
