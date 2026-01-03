# ScratchBird Index Implementation Guide
**Version:** 1.0
**Date:** November 20, 2025
**Audience:** ScratchBird Developers

---

## Table of Contents

1. [Introduction](#introduction)
2. [Prerequisites](#prerequisites)
3. [Index Implementation Checklist](#index-implementation-checklist)
4. [Step-by-Step Implementation](#step-by-step-implementation)
5. [MGA Compliance Requirements](#mga-compliance-requirements)
6. [DML Integration Template](#dml-integration-template)
7. [Bytecode Integration Template](#bytecode-integration-template)
8. [Testing Requirements](#testing-requirements)
9. [Common Pitfalls](#common-pitfalls)
10. [Example Implementation](#example-implementation)

---

## Introduction

This guide provides a step-by-step process for implementing new index types in ScratchBird. All index implementations must follow **Firebird MGA principles** with TIP-based visibility and stable TIDs.

### What You'll Build

A new index implementation consists of:

1. **Core Index Class** - Data structure and operations (insert, search, delete)
2. **MGA Compliance** - xmin/xmax tracking, TIP-based visibility, logical deletion
3. **DML Integration** - Hooks in StorageEngine for INSERT/UPDATE/DELETE
4. **Bytecode Support** - Opcodes, bytecode generation, executor integration
5. **Tests** - Unit tests, integration tests, MGA compliance tests

### Time Estimates

- Core implementation: 20-40 hours
- MGA compliance: 8-12 hours
- DML integration: 6-8 hours
- Bytecode support: 12-16 hours
- Testing: 8-12 hours
- **Total: 54-88 hours** (1.5-2 weeks)

---

## Prerequisites

### Required Knowledge

- **C++17** proficiency (RAII, smart pointers, templates)
- **Firebird MGA architecture** (read `/MGA_RULES.md`)
- **ScratchBird page structure** (`ondisk.h`)
- **Buffer pool API** (`buffer_pool.h`)
- **Transaction manager API** (`transaction_manager.h`)

### Required Reading

**MANDATORY:**
1. `/MGA_RULES.md` - Firebird MGA vs PostgreSQL MVCC
2. `/docs/specifications/INDEX_ARCHITECTURE.md` - Index types overview
3. `/docs/IMPLEMENTATION_AUDIT.md` - Existing implementations reference

**RECOMMENDED:**
4. `/docs/specifications/MGA_IMPLEMENTATION.md` - TIP details
5. `/docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md` - Transaction markers

### Development Environment

```bash
# Build ScratchBird
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run tests
ctest --output-on-failure

# Run specific test
./bin/test_btree
```

---

## Index Implementation Checklist

Use this checklist to track implementation progress:

### Phase 1: Core Data Structure (20-40h)
- [ ] Define on-disk page structures (`#pragma pack(push, 1)`)
- [ ] Implement meta page layout
- [ ] Implement data page layout(s)
- [ ] Add xmin/xmax to all entry structures
- [ ] Implement constructor/destructor
- [ ] Implement `create()` static method
- [ ] Implement `open()` static method

### Phase 2: Core Operations (20-40h)
- [ ] Implement `insert()` method
- [ ] Implement `search()` method
- [ ] Implement `remove()` method (logical deletion only)
- [ ] Implement `rangeScan()` or equivalent (if applicable)
- [ ] Add error handling with ErrorContext

### Phase 3: MGA Compliance (8-12h)
- [ ] Read `/MGA_RULES.md` again
- [ ] Verify all entries have xmin/xmax fields
- [ ] Implement `isEntryVisible()` with TIP checks
- [ ] Use `TransactionId current_xid` parameters (NOT snapshots)
- [ ] Implement logical deletion (set xmax, never physically remove)
- [ ] Implement `IndexGCInterface::removeDeadEntries()`
- [ ] Run MGA compliance tests

### Phase 4: DML Integration (6-8h)
- [ ] Add index type to `IndexType` enum (`storage_engine.h`)
- [ ] Add DML hooks in `StorageEngine::insertTuple()`
- [ ] Add DML hooks in `StorageEngine::updateTuple()`
- [ ] Add DML hooks in `StorageEngine::deleteTuple()`
- [ ] Remove `Status::NOT_IMPLEMENTED` returns
- [ ] Test with concurrent transactions

### Phase 5: Bytecode Support (12-16h)
- [ ] Define opcodes in `src/sblr/opcodes.h`
- [ ] Implement bytecode generation in `bytecode_generator_v2.cpp`
- [ ] Implement executor handlers in `executor.cpp`
- [ ] Add query planner integration (cost estimation)
- [ ] Test CREATE/DROP INDEX statements

### Phase 6: Testing (8-12h)
- [ ] Write unit tests (basic operations)
- [ ] Write integration tests (DML + visibility)
- [ ] Write MGA compliance tests
- [ ] Write performance benchmarks
- [ ] Run full test suite

### Phase 7: Documentation (2-4h)
- [ ] Update `INDEX_ARCHITECTURE.md`
- [ ] Add usage examples
- [ ] Document performance characteristics
- [ ] Update `PROJECT_CONTEXT.md`

---

## Step-by-Step Implementation

### Step 1: Create Header File

**File:** `include/scratchbird/core/myindex.h`

```cpp
#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird {
namespace core {

// Forward declarations
class Database;
class BufferPool;
class TransactionManager;

// ===== On-Disk Structures =====

#pragma pack(push, 1)

// Meta Page - Page 0 of your index
struct SBMyIndexMetaPage {
    PageHeader header;           // Standard page header (64 bytes)
    uint8_t index_uuid[16];      // Index UUID bytes (16 bytes)
    uint64_t root_page;          // Root page number (8 bytes)
    uint64_t num_entries;        // Total entries (8 bytes)
    uint8_t reserved[8096];      // Reserved (8192 - 96 = 8096)
} __attribute__((packed));

static_assert(sizeof(SBMyIndexMetaPage) == 8192, "Meta page must be exactly 8KB");

// Index Entry - MUST include xmin/xmax for MGA compliance
struct MyIndexEntry {
    GPID gpid;                   // Global Page ID (8 bytes)
    uint16_t slot;               // Slot number (2 bytes)
    uint16_t padding;            // Padding for alignment (2 bytes)
    uint64_t xmin;               // Creating transaction (8 bytes) - REQUIRED
    uint64_t xmax;               // Deleting transaction (8 bytes) - REQUIRED
    // ... your index-specific fields ...

    // Helper methods
    TID getTID() const { return TID(gpid, slot); }
    void setTID(const TID& tid) { gpid = tid.gpid; slot = tid.slot; }
} __attribute__((packed));

// Data Page - Store your index entries
struct SBMyIndexDataPage {
    PageHeader header;           // Standard page header (64 bytes)
    uint16_t entry_count;        // Number of entries (2 bytes)
    uint16_t flags;              // Page flags (2 bytes)
    uint32_t reserved;           // Reserved (4 bytes)
    MyIndexEntry entries[(8192 - 72) / sizeof(MyIndexEntry)];
} __attribute__((packed));

static_assert(sizeof(SBMyIndexDataPage) <= 8192, "Data page must fit in 8KB");

#pragma pack(pop)

// ===== Index Class =====

class MyIndex : public IndexGCInterface {
public:
    // Constructor
    MyIndex(Database* db, const UuidV7Bytes& index_uuid, uint32_t meta_page);

    // Destructor
    ~MyIndex();

    // Create new index
    static Status create(Database* db, const UuidV7Bytes& index_uuid,
                        uint32_t* meta_page_out, ErrorContext* ctx = nullptr);

    // Open existing index
    static std::unique_ptr<MyIndex> open(Database* db, const UuidV7Bytes& index_uuid,
                                         uint32_t meta_page, ErrorContext* ctx = nullptr);

    // Core operations
    Status insert(const Key& key, const TID& tid, TransactionId xid,
                 ErrorContext* ctx = nullptr);

    Status search(const Key& key, TransactionId current_xid,
                 std::vector<TID>* results_out, ErrorContext* ctx = nullptr);

    Status remove(const Key& key, const TID& tid, TransactionId xid,
                 ErrorContext* ctx = nullptr);

    // IndexGCInterface implementation
    Status removeDeadEntries(const std::vector<TID>& dead_tids,
                            uint64_t* entries_removed_out = nullptr,
                            uint64_t* pages_modified_out = nullptr,
                            ErrorContext* ctx = nullptr) override;

    const char* indexTypeName() const override { return "MyIndex"; }

    // Getters
    const UuidV7Bytes& getIndexUuid() const { return index_uuid_; }
    uint32_t getMetaPage() const { return meta_page_; }

private:
    // Member variables
    Database* db_;
    UuidV7Bytes index_uuid_;
    uint32_t meta_page_;

    // Helper methods
    Status isEntryVisible(uint64_t entry_xmin, uint64_t entry_xmax,
                         TransactionId current_xid, bool* visible_out,
                         ErrorContext* ctx);

    TransactionId getCurrentTransactionId();
};

} // namespace core
} // namespace scratchbird
```

---

### Step 2: Implement Core Operations

**File:** `src/core/myindex.cpp`

```cpp
#include "scratchbird/core/myindex.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logging.h"
#include <cstring>

namespace scratchbird {
namespace core {

// Constructor
MyIndex::MyIndex(Database* db, const UuidV7Bytes& index_uuid, uint32_t meta_page)
    : db_(db), index_uuid_(index_uuid), meta_page_(meta_page) {
}

// Destructor
MyIndex::~MyIndex() {
}

// Create new index
Status MyIndex::create(Database* db, const UuidV7Bytes& index_uuid,
                      uint32_t* meta_page_out, ErrorContext* ctx) {
    if (!db || !meta_page_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null arguments");
        return Status::INVALID_ARGUMENT;
    }

    // Allocate meta page
    auto buffer_pool = db->getBufferPool();
    uint32_t meta_page = 0;
    auto status = buffer_pool->allocatePage(&meta_page, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Pin meta page
    BufferFrame* frame = nullptr;
    status = buffer_pool->pinPage(meta_page, &frame, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Initialize meta page
    auto* meta = reinterpret_cast<SBMyIndexMetaPage*>(frame->getData());
    std::memset(meta, 0, sizeof(SBMyIndexMetaPage));

    // Set page header
    meta->header.pg_lsn = 0;
    meta->header.pg_type = static_cast<uint16_t>(PageType::INDEX_PAGE);
    meta->header.pg_flags = 0;
    meta->header.pg_lower = sizeof(SBMyIndexMetaPage);
    meta->header.pg_upper = 8192;
    meta->header.pg_special = 8192;

    // Set index metadata
    std::memcpy(meta->index_uuid, index_uuid.bytes, 16);
    meta->root_page = 0;  // No root yet
    meta->num_entries = 0;

    // Mark page dirty and unpin
    frame->markDirty();
    buffer_pool->unpinPage(meta_page);

    *meta_page_out = meta_page;
    LOG_INFO(Category::INDEX, "Created MyIndex with meta page %u", meta_page);
    return Status::OK;
}

// Open existing index
std::unique_ptr<MyIndex> MyIndex::open(Database* db, const UuidV7Bytes& index_uuid,
                                       uint32_t meta_page, ErrorContext* ctx) {
    if (!db) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null database");
        return nullptr;
    }

    // Verify meta page exists
    auto buffer_pool = db->getBufferPool();
    BufferFrame* frame = nullptr;
    auto status = buffer_pool->pinPage(meta_page, &frame, ctx);
    if (status != Status::OK) {
        return nullptr;
    }

    // Verify UUID matches
    auto* meta = reinterpret_cast<SBMyIndexMetaPage*>(frame->getData());
    if (std::memcmp(meta->index_uuid, index_uuid.bytes, 16) != 0) {
        buffer_pool->unpinPage(meta_page);
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "UUID mismatch");
        return nullptr;
    }

    buffer_pool->unpinPage(meta_page);

    return std::make_unique<MyIndex>(db, index_uuid, meta_page);
}

// Insert entry
Status MyIndex::insert(const Key& key, const TID& tid, TransactionId xid,
                      ErrorContext* ctx) {
    // TODO: Implement your insert logic

    // REQUIRED: Set xmin and xmax
    MyIndexEntry entry;
    entry.setTID(tid);
    entry.xmin = xid;      // ← REQUIRED
    entry.xmax = 0;        // ← REQUIRED (0 = not deleted)

    // ... insert entry into your data structure ...

    LOG_DEBUG(Category::INDEX, "Inserted key into MyIndex, xmin=%lu", xid);
    return Status::OK;
}

// Search for entries
Status MyIndex::search(const Key& key, TransactionId current_xid,
                      std::vector<TID>* results_out, ErrorContext* ctx) {
    if (!results_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null results");
        return Status::INVALID_ARGUMENT;
    }

    results_out->clear();

    // TODO: Search your data structure for matching entries

    // REQUIRED: Check visibility for each entry
    for (const auto& entry : matching_entries) {
        bool visible = false;
        auto status = isEntryVisible(entry.xmin, entry.xmax, current_xid,
                                     &visible, ctx);
        if (status != Status::OK) {
            return status;
        }

        if (visible) {
            results_out->push_back(entry.getTID());
        }
    }

    return Status::OK;
}

// Remove entry (LOGICAL DELETION ONLY)
Status MyIndex::remove(const Key& key, const TID& tid, TransactionId xid,
                      ErrorContext* ctx) {
    // TODO: Find entry in your data structure

    // REQUIRED: Logical deletion - set xmax
    entry.xmax = xid;  // ← REQUIRED

    // ❌ FORBIDDEN: DO NOT physically remove entry
    // eraseFromDataStructure(entry);  // ← NEVER DO THIS

    LOG_DEBUG(Category::INDEX, "Logically deleted entry, xmax=%lu", xid);
    return Status::OK;
}

// Visibility check (MGA compliance)
Status MyIndex::isEntryVisible(uint64_t entry_xmin, uint64_t entry_xmax,
                              TransactionId current_xid, bool* visible_out,
                              ErrorContext* ctx) {
    if (!visible_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null visible_out");
        return Status::INVALID_ARGUMENT;
    }

    // Own transaction's changes always visible
    if (entry_xmin == current_xid) {
        *visible_out = (entry_xmax == 0);  // Not deleted by self
        return Status::OK;
    }

    // Get transaction manager
    auto* txn_mgr = db_->getTransactionManager();
    if (!txn_mgr) {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "No transaction manager");
        return Status::INTERNAL_ERROR;
    }

    // Check xmin state via TIP
    TxState xmin_state = txn_mgr->getTransactionState(entry_xmin);
    if (xmin_state != TxState::TX_COMMITTED || entry_xmin >= current_xid) {
        *visible_out = false;
        return Status::OK;
    }

    // Check if deleted
    if (entry_xmax == 0) {
        *visible_out = true;  // Not deleted
        return Status::OK;
    }

    // Check xmax state via TIP
    TxState xmax_state = txn_mgr->getTransactionState(entry_xmax);
    *visible_out = (xmax_state != TxState::TX_COMMITTED || entry_xmax >= current_xid);

    return Status::OK;
}

// Garbage collection
Status MyIndex::removeDeadEntries(const std::vector<TID>& dead_tids,
                                 uint64_t* entries_removed_out,
                                 uint64_t* pages_modified_out,
                                 ErrorContext* ctx) {
    uint64_t removed = 0;
    uint64_t modified_pages = 0;

    auto* txn_mgr = db_->getTransactionManager();
    TransactionId oit = 0, oat = 0, ost = 0, next = 0;
    txn_mgr->getTransactionMarkers(oit, oat, ost, next);

    // TODO: Scan your index for entries with xmax < oit
    // Only these entries are safe to physically remove

    for (const auto& entry : all_entries) {
        if (entry.xmax != 0 && entry.xmax < oit) {
            // Safe to physically remove
            // ... remove from data structure ...
            removed++;
        }
    }

    if (entries_removed_out) *entries_removed_out = removed;
    if (pages_modified_out) *pages_modified_out = modified_pages;

    LOG_INFO(Category::INDEX, "GC removed %lu dead entries from MyIndex", removed);
    return Status::OK;
}

// Get current transaction ID
TransactionId MyIndex::getCurrentTransactionId() {
    auto* txn_mgr = db_->getTransactionManager();
    return txn_mgr ? txn_mgr->getCurrentTransactionId() : 0;
}

} // namespace core
} // namespace scratchbird
```

---

## MGA Compliance Requirements

### Mandatory Patterns

#### 1. xmin/xmax in All Entries

```cpp
// ✅ CORRECT
struct MyIndexEntry {
    GPID gpid;
    uint16_t slot;
    uint64_t xmin;  // ← REQUIRED
    uint64_t xmax;  // ← REQUIRED
    // ... other fields ...
};
```

#### 2. TIP-Based Visibility (NOT Snapshots)

```cpp
// ✅ CORRECT - TIP-based
Status search(const Key& key, TransactionId current_xid,  // ← Use TransactionId
             std::vector<TID>* results_out, ErrorContext* ctx);

// ❌ FORBIDDEN - Snapshot-based
Status search(const Key& key, const Snapshot* snapshot,   // ← NEVER use Snapshot
             std::vector<TID>* results_out, ErrorContext* ctx);
```

#### 3. Logical Deletion Only

```cpp
// ✅ CORRECT
Status remove(const Key& key, const TID& tid, TransactionId xid, ErrorContext* ctx) {
    entry.xmax = xid;  // Mark as deleted
    return Status::OK;
}

// ❌ FORBIDDEN
Status remove(const Key& key, const TID& tid, TransactionId xid, ErrorContext* ctx) {
    eraseEntry(entry);  // Physical removal violates MGA
    return Status::OK;
}
```

#### 4. Visibility Check Pattern

```cpp
Status isEntryVisible(uint64_t entry_xmin, uint64_t entry_xmax,
                     TransactionId current_xid, bool* visible_out,
                     ErrorContext* ctx) {
    // 1. Own changes always visible
    if (entry_xmin == current_xid) {
        *visible_out = (entry_xmax == 0);
        return Status::OK;
    }

    // 2. Check TIP for xmin state
    TxState xmin_state = txn_mgr->getTransactionState(entry_xmin);
    if (xmin_state != TX_COMMITTED || entry_xmin >= current_xid) {
        *visible_out = false;
        return Status::OK;
    }

    // 3. Check if deleted
    if (entry_xmax == 0) {
        *visible_out = true;
        return Status::OK;
    }

    // 4. Check TIP for xmax state
    TxState xmax_state = txn_mgr->getTransactionState(entry_xmax);
    *visible_out = (xmax_state != TX_COMMITTED || entry_xmax >= current_xid);
    return Status::OK;
}
```

### Verification Checklist

Run these checks before claiming MGA compliance:

```bash
# 1. Check for forbidden patterns
grep -r "Snapshot" include/scratchbird/core/myindex.h  # Should be empty
grep -r "isSnapshotVisible" src/core/myindex.cpp       # Should be empty

# 2. Verify xmin/xmax present
grep "uint64_t xmin" include/scratchbird/core/myindex.h  # Should find entries
grep "uint64_t xmax" include/scratchbird/core/myindex.h  # Should find entries

# 3. Check for logical deletion
grep "xmax = xid" src/core/myindex.cpp  # Should find in remove()

# 4. Verify TIP usage
grep "getTransactionState" src/core/myindex.cpp  # Should find in visibility checks
```

---

## DML Integration Template

### Step 1: Add Index Type Enum

**File:** `include/scratchbird/core/storage_engine.h`

```cpp
enum class IndexType : uint8_t {
    BTREE = 0,
    HASH = 1,
    GIN = 2,
    HNSW = 3,
    GIST = 4,
    SPGIST = 5,
    BRIN = 6,
    BITMAP = 7,
    LSM_TREE = 8,
    RTREE = 9,
    COLUMNSTORE = 10,
    MYINDEX = 11  // ← Add your index type
};
```

### Step 2: Add INSERT Hook

**File:** `src/core/storage_engine.cpp`

```cpp
Status StorageEngine::insertTuple(Table* table, const std::vector<Value>& values,
                                 TID* tid_out, ErrorContext* ctx) {
    // ... heap insertion logic ...

    // Maintain indexes
    for (auto& idx_info : table->indexes) {
        switch (idx_info.type) {
            case IndexType::BTREE:
                // ... btree insertion ...
                break;

            case IndexType::MYINDEX: {  // ← Add your case
                auto* my_idx = static_cast<MyIndex*>(idx_info.index.get());
                Key key = extractKey(values, idx_info.columns);
                status = my_idx->insert(key, *tid_out, current_xid, ctx);
                if (status != Status::OK) {
                    return status;
                }
                break;
            }

            default:
                return Status::NOT_IMPLEMENTED;
        }
    }

    return Status::OK;
}
```

### Step 3: Add UPDATE Hook

```cpp
Status StorageEngine::updateTuple(Table* table, const TID& tid,
                                 const std::vector<Value>& new_values,
                                 ErrorContext* ctx) {
    // ... fetch old tuple ...
    // ... update heap tuple ...

    // Maintain indexes (only if indexed columns changed)
    for (auto& idx_info : table->indexes) {
        bool indexed_cols_changed = checkIfColumnsChanged(old_values, new_values,
                                                          idx_info.columns);
        if (!indexed_cols_changed) {
            continue;  // Skip index update (stable TID)
        }

        Key old_key = extractKey(old_values, idx_info.columns);
        Key new_key = extractKey(new_values, idx_info.columns);

        switch (idx_info.type) {
            case IndexType::MYINDEX: {
                auto* my_idx = static_cast<MyIndex*>(idx_info.index.get());

                // Remove old key (sets xmax)
                status = my_idx->remove(old_key, tid, current_xid, ctx);
                if (status != Status::OK) {
                    return status;
                }

                // Insert new key (sets xmin)
                status = my_idx->insert(new_key, tid, current_xid, ctx);
                if (status != Status::OK) {
                    return status;
                }
                break;
            }

            default:
                return Status::NOT_IMPLEMENTED;
        }
    }

    return Status::OK;
}
```

### Step 4: Add DELETE Hook

```cpp
Status StorageEngine::deleteTuple(Table* table, const TID& tid, ErrorContext* ctx) {
    // ... mark heap tuple as deleted ...

    // Maintain indexes (logical deletion)
    for (auto& idx_info : table->indexes) {
        Key key = extractKeyFromTuple(table, tid, idx_info.columns, ctx);

        switch (idx_info.type) {
            case IndexType::MYINDEX: {
                auto* my_idx = static_cast<MyIndex*>(idx_info.index.get());
                status = my_idx->remove(key, tid, current_xid, ctx);
                if (status != Status::OK) {
                    return status;
                }
                break;
            }

            default:
                return Status::NOT_IMPLEMENTED;
        }
    }

    return Status::OK;
}
```

---

## Bytecode Integration Template

### Step 1: Define Opcodes

**File:** `src/sblr/opcodes.h`

```cpp
// Index operations
constexpr uint8_t CREATE_INDEX = 0x50;
constexpr uint8_t DROP_INDEX = 0x51;
constexpr uint8_t INDEX_SEARCH = 0x52;  // ← Add if needed for your index
constexpr uint8_t INDEX_SCAN = 0x53;
```

### Step 2: Bytecode Generation

**File:** `src/sblr/bytecode_generator_v2.cpp`

```cpp
void BytecodeGeneratorV2::generateCreateIndex(ResolvedCreateIndexStmt* stmt) {
    // Opcode
    bytecode_.push_back(CREATE_INDEX);

    // Index type
    bytecode_.push_back(static_cast<uint8_t>(stmt->index_type));

    // Table name (length + bytes)
    encodeString(stmt->table_name, &bytecode_);

    // Index name (length + bytes)
    encodeString(stmt->index_name, &bytecode_);

    // Column count
    bytecode_.push_back(stmt->columns.size());

    // Column IDs
    for (uint16_t col_id : stmt->columns) {
        encodeUint16(col_id, &bytecode_);
    }

    // Flags (UNIQUE, etc.)
    encodeUint16(stmt->flags, &bytecode_);
}
```

### Step 3: Bytecode Execution

**File:** `src/sblr/executor.cpp`

```cpp
Status Executor::executeCreateIndex(const uint8_t* bytecode, size_t* offset,
                                   ErrorContext* ctx) {
    // Decode index type
    auto index_type = static_cast<IndexType>(bytecode[(*offset)++]);

    // Decode table name
    std::string table_name = decodeString(bytecode, offset);

    // Decode index name
    std::string index_name = decodeString(bytecode, offset);

    // Decode columns
    uint8_t col_count = bytecode[(*offset)++];
    std::vector<uint16_t> columns;
    for (uint8_t i = 0; i < col_count; i++) {
        columns.push_back(decodeUint16(bytecode, offset));
    }

    // Decode flags
    uint16_t flags = decodeUint16(bytecode, offset);

    // Create index via catalog
    auto* catalog = db_->getCatalogManager();
    IndexInfo idx_info;
    idx_info.name = index_name;
    idx_info.type = index_type;
    idx_info.columns = columns;
    idx_info.unique = (flags & INDEX_FLAG_UNIQUE) != 0;

    auto status = catalog->createIndex(table_name, idx_info, ctx);
    if (status != Status::OK) {
        return status;
    }

    LOG_INFO(Category::EXECUTOR, "Created index '%s' on table '%s'",
            index_name.c_str(), table_name.c_str());
    return Status::OK;
}
```

---

## Testing Requirements

### Unit Tests

**File:** `tests/unit/test_myindex.cpp`

```cpp
#include <gtest/gtest.h>
#include "scratchbird/core/myindex.h"
#include "scratchbird/core/database.h"

class MyIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test database
        db_ = Database::create("/tmp/test_myindex.db", 8192, &ctx_);
        ASSERT_NE(db_, nullptr);

        // Create index
        UuidV7Bytes uuid = generateUuidV7();
        uint32_t meta_page = 0;
        auto status = MyIndex::create(db_.get(), uuid, &meta_page, &ctx_);
        ASSERT_EQ(status, Status::OK);

        // Open index
        index_ = MyIndex::open(db_.get(), uuid, meta_page, &ctx_);
        ASSERT_NE(index_, nullptr);

        xid_ = db_->getTransactionManager()->beginTransaction(
            IsolationLevel::READ_COMMITTED, false);
    }

    void TearDown() override {
        if (xid_ != 0) {
            db_->getTransactionManager()->commitTransaction(xid_);
        }
        index_.reset();
        db_.reset();
    }

    std::unique_ptr<Database> db_;
    std::unique_ptr<MyIndex> index_;
    ErrorContext ctx_;
    TransactionId xid_ = 0;
};

TEST_F(MyIndexTest, InsertAndSearch) {
    Key key = makeKey(12345);
    TID tid = TID(100, 10);

    // Insert
    auto status = index_->insert(key, tid, xid_, &ctx_);
    ASSERT_EQ(status, Status::OK);

    // Search
    std::vector<TID> results;
    status = index_->search(key, xid_, &results, &ctx_);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0], tid);
}

TEST_F(MyIndexTest, LogicalDeletion) {
    Key key = makeKey(99999);
    TID tid = TID(200, 20);

    // Insert
    auto status = index_->insert(key, tid, xid_, &ctx_);
    ASSERT_EQ(status, Status::OK);

    // Delete (logical)
    status = index_->remove(key, tid, xid_, &ctx_);
    ASSERT_EQ(status, Status::OK);

    // Search (should return empty - own transaction sees deletion)
    std::vector<TID> results;
    status = index_->search(key, xid_, &results, &ctx_);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 0);
}

TEST_F(MyIndexTest, VisibilityAcrossTransactions) {
    Key key = makeKey(54321);
    TID tid = TID(300, 30);

    // Transaction 1: Insert
    auto xid1 = db_->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);
    auto status = index_->insert(key, tid, xid1, &ctx_);
    ASSERT_EQ(status, Status::OK);
    db_->getTransactionManager()->commitTransaction(xid1);

    // Transaction 2: Should see committed entry
    auto xid2 = db_->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);
    std::vector<TID> results;
    status = index_->search(key, xid2, &results, &ctx_);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1);
    db_->getTransactionManager()->commitTransaction(xid2);

    // Transaction 3: Delete
    auto xid3 = db_->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);
    status = index_->remove(key, tid, xid3, &ctx_);
    ASSERT_EQ(status, Status::OK);
    db_->getTransactionManager()->commitTransaction(xid3);

    // Transaction 4: Should NOT see deleted entry
    auto xid4 = db_->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);
    results.clear();
    status = index_->search(key, xid4, &results, &ctx_);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 0);
    db_->getTransactionManager()->commitTransaction(xid4);
}
```

### Integration Tests

**File:** `tests/integration/test_myindex_dml.cpp`

```cpp
TEST(MyIndexDMLTest, InsertMaintainsIndex) {
    // Create table with MyIndex
    auto db = createTestDatabase();
    auto table = createTableWithMyIndex(db.get());

    // Insert row
    std::vector<Value> values = {Value::makeInt32(100), Value::makeText("Test")};
    TID tid;
    ErrorContext ctx;
    auto status = db->getStorageEngine()->insertTuple(table, values, &tid, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Verify index contains entry
    auto* my_idx = getMyIndexFromTable(table);
    Key key = makeKeyFromValues(values, table->indexes[0].columns);
    auto xid = db->getTransactionManager()->getCurrentTransactionId();

    std::vector<TID> results;
    status = my_idx->search(key, xid, &results, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(results.size(), 1);
    ASSERT_EQ(results[0], tid);
}

TEST(MyIndexDMLTest, UpdateMaintainsIndex) {
    // ... similar test for UPDATE ...
}

TEST(MyIndexDMLTest, DeleteMaintainsIndex) {
    // ... similar test for DELETE ...
}
```

---

## Common Pitfalls

### Pitfall 1: Using Snapshots Instead of TIP

❌ **WRONG:**
```cpp
Status search(const Key& key, const Snapshot* snapshot, ...) {
    // NEVER use Snapshot structures
}
```

✅ **CORRECT:**
```cpp
Status search(const Key& key, TransactionId current_xid, ...) {
    // Always use TransactionId + TIP lookups
}
```

### Pitfall 2: Physical Deletion

❌ **WRONG:**
```cpp
Status remove(const Key& key, const TID& tid, TransactionId xid, ...) {
    eraseFromTree(key);  // Physical removal violates MGA
}
```

✅ **CORRECT:**
```cpp
Status remove(const Key& key, const TID& tid, TransactionId xid, ...) {
    entry.xmax = xid;  // Logical deletion
}
```

### Pitfall 3: Missing xmin/xmax

❌ **WRONG:**
```cpp
struct MyIndexEntry {
    GPID gpid;
    uint16_t slot;
    // Missing xmin/xmax
};
```

✅ **CORRECT:**
```cpp
struct MyIndexEntry {
    GPID gpid;
    uint16_t slot;
    uint64_t xmin;  // Required
    uint64_t xmax;  // Required
};
```

### Pitfall 4: Incorrect Visibility Logic

❌ **WRONG:**
```cpp
bool visible = (entry.xmin < current_xid);  // Oversimplified
```

✅ **CORRECT:**
```cpp
// Must check TIP state
TxState xmin_state = txn_mgr->getTransactionState(entry.xmin);
bool visible = (xmin_state == TX_COMMITTED && entry.xmin < current_xid && entry.xmax == 0);
```

### Pitfall 5: Premature Physical GC

❌ **WRONG:**
```cpp
if (entry.xmax != 0) {
    physicallyRemove(entry);  // May still be visible to active transactions
}
```

✅ **CORRECT:**
```cpp
TransactionId oit = txn_mgr->getOldestInterestingTransaction();
if (entry.xmax != 0 && entry.xmax < oit) {
    physicallyRemove(entry);  // Safe - no active txn can see this
}
```

---

## Example Implementation

See existing implementations for reference:

- **Simple Index:** `src/core/hash_index.cpp` (~800 lines)
- **Complex Index:** `src/core/btree.cpp` (~2,836 lines)
- **Inverted Index:** `src/core/gin_index.cpp` (~1,500 lines)
- **Vector Index:** `src/core/hnsw_index.cpp` (~1,200 lines)

### Recommended Study Path

1. **Start with Hash Index** - Simplest MGA-compliant implementation
2. **Study B-Tree** - Learn page management and tree operations
3. **Review GIN** - Understand multi-value indexing (but note MGA violation)
4. **Examine HNSW** - See graph-based index with visibility checks

---

## Additional Resources

- **MGA Rules:** `/MGA_RULES.md`
- **Index Architecture:** `/docs/specifications/INDEX_ARCHITECTURE.md`
- **Implementation Audit:** `/docs/IMPLEMENTATION_AUDIT.md`
- **Remediation Plan:** `/docs/audit/INDEX_SYSTEM_REMEDIATION_PLAN.md`

---

## Questions?

For implementation questions:
1. Check `/MGA_RULES.md` for architectural guidance
2. Review existing index implementations
3. Consult `INDEX_SYSTEM_COMPREHENSIVE_AUDIT.md` for patterns
4. Open issue in project repository

---

**Document Version:** 1.0
**Last Updated:** November 20, 2025
**Status:** Production Documentation
**Next Review:** After first external index contribution
