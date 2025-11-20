# Bloom Filter Index Specification for ScratchBird

**Version:** 1.0
**Date:** November 20, 2025
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Alpha - Phase 2

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Mathematical Foundation](#mathematical-foundation)
4. [External Dependencies](#external-dependencies)
5. [On-Disk Structures](#on-disk-structures)
6. [MGA Compliance](#mga-compliance)
7. [Core API](#core-api)
8. [DML Integration](#dml-integration)
9. [Bytecode Integration](#bytecode-integration)
10. [Query Planner Integration](#query-planner-integration)
11. [Implementation Steps](#implementation-steps)
12. [Testing Requirements](#testing-requirements)
13. [Performance Targets](#performance-targets)
14. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

Bloom filters reduce read I/O by quickly determining if a key **definitely does not exist** in a data structure. This is particularly valuable for:

- **B-Tree node pruning**: Skip entire B-Tree subtrees during point lookups
- **Heap page filtering**: Avoid reading heap pages that don't contain target keys
- **Index maintenance**: Reduce unnecessary index scans during UPDATE/DELETE

### Key Characteristics

- **Type**: Probabilistic membership test (no false negatives, controlled false positives)
- **Space efficiency**: 10 bits per key → 1% false positive rate (FPR)
- **Query time**: O(k) where k = number of hash functions (typically 7-10)
- **No deletions**: Bloom filters don't support removal (use counting Bloom filter variant if needed)

### Integration Strategy

**Design Decision:** Bloom filters will be **auxiliary structures** attached to existing indexes, NOT standalone index types.

```
B-Tree Index
├── Meta Page (existing)
├── Internal/Leaf Pages (existing)
└── Bloom Filter Pages (NEW)
    ├── Bloom Meta Page
    └── Bloom Data Pages (bit arrays)
```

**Rationale:**
- More practical than standalone Bloom filter "indexes"
- Transparent to users (automatically maintained)
- Easy to enable/disable per-index
- Fits ScratchBird's architecture (indexes are primary, Bloom filters are optimization)

---

## Architecture Decision

### Scope: Per-Index Bloom Filters

Each B-Tree, Hash, or GIN index can have an **optional** attached Bloom filter that stores hashes of all keys in the index.

### Use Case Example

```sql
-- Create table
CREATE TABLE users (
    id INT PRIMARY KEY,
    email VARCHAR(255),
    username VARCHAR(100)
);

-- Create B-Tree index with Bloom filter
CREATE INDEX idx_email ON users(email)
WITH (bloom_filter = true, bloom_fpr = 0.01);

-- Query optimization
SELECT * FROM users WHERE email = 'nonexistent@example.com';
-- 1. Check Bloom filter (10 microseconds) → "definitely not present"
-- 2. Skip B-Tree traversal entirely
-- 3. Return empty result (saved ~1-5ms of B-Tree I/O)
```

### Configuration Parameters

```cpp
struct BloomFilterConfig {
    bool enabled;              // Enable Bloom filter for this index
    double target_fpr;         // Target false positive rate (0.001 to 0.1)
    uint8_t bits_per_key;      // Bits per key (calculated from FPR)
    uint8_t num_hashes;        // Number of hash functions (k)
    uint32_t rebuild_threshold; // Rebuild after N insertions
};

// Defaults
static constexpr BloomFilterConfig DEFAULT_CONFIG = {
    .enabled = false,          // Opt-in per index
    .target_fpr = 0.01,        // 1% false positive rate
    .bits_per_key = 10,        // Calculated: -ln(0.01) / (ln(2))^2 ≈ 9.6
    .num_hashes = 7,           // Calculated: (10/1) * ln(2) ≈ 6.93
    .rebuild_threshold = 100000 // Rebuild after 100K inserts
};
```

---

## Mathematical Foundation

### Core Formulas

```
Optimal bit array size:
m = -n × ln(p) / (ln(2))²

Optimal hash functions:
k = (m/n) × ln(2) ≈ 0.693 × (m/n)

Actual false positive rate:
FPR = (1 - e^(-kn/m))^k
```

Where:
- `n` = number of elements
- `m` = number of bits
- `k` = number of hash functions
- `p` = target FPR

### Lookup Tables

| Target FPR | Bits/Key | Hash Functions (k) |
|------------|----------|-------------------|
| 10% | 4.8 | 3 |
| 5% | 6.2 | 4 |
| 1% | 9.6 | 7 |
| 0.1% | 14.4 | 10 |
| 0.01% | 19.2 | 13 |

**Recommendation:** Start with **1% FPR (10 bits/key, k=7)** as default.

### Memory Requirements

```
For 1 million keys with 1% FPR:
m = 1,000,000 × 10 bits = 10,000,000 bits = 1.25 MB

For 10 million keys with 1% FPR:
m = 10,000,000 × 10 bits = 100,000,000 bits = 12.5 MB

For 1 billion keys with 0.1% FPR:
m = 1,000,000,000 × 14.4 bits = 14,400,000,000 bits = 1.8 GB
```

---

## External Dependencies

### Hash Function Library: xxHash

**Library:** xxHash
**Version:** 0.8.2 (latest stable)
**License:** BSD 2-Clause (compatible with ScratchBird)
**Repository:** https://github.com/Cyan4973/xxHash
**Why xxHash:** Fastest non-cryptographic hash (10+ GB/s), battle-tested

#### Integration Method: Single-Header Inclusion

xxHash provides a single-header implementation, making integration trivial.

**Download:**
```bash
cd third_party/
mkdir -p xxhash
cd xxhash
wget https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.2/xxhash.h
```

**CMakeLists.txt:**
```cmake
# Add xxHash include path
include_directories(${CMAKE_SOURCE_DIR}/third_party/xxhash)

# No library linking needed (header-only)
```

**Usage in code:**
```cpp
#define XXH_INLINE_ALL  // Single-header mode
#include <xxhash.h>

// Hash a key
uint64_t hash = XXH3_64bits(key_data, key_len);
```

#### Alternative: MurmurHash3 (Already Available?)

Check if ScratchBird already uses MurmurHash3 (common in database systems):

```bash
grep -r "MurmurHash" include/ src/
```

If yes, use existing implementation. If no, use xxHash (simpler integration).

**Recommendation:** Use **xxHash3** for performance, fallback to **std::hash** if xxHash unavailable.

---

## On-Disk Structures

### Page Layout

Bloom filters are stored in dedicated pages linked from the parent index's meta page.

```
Index Meta Page (existing)
├── ... existing fields ...
└── bloom_filter_meta_page (NEW field, 8 bytes)
        ↓
Bloom Filter Meta Page
├── PageHeader (64 bytes)
├── Configuration (32 bytes)
├── Statistics (64 bytes)
└── First data page pointer
        ↓
Bloom Filter Data Pages (bit arrays)
├── PageHeader (64 bytes)
├── Next page pointer (8 bytes)
└── Bit array (8120 bytes = 64,960 bits)
```

### 1. Bloom Filter Meta Page

```cpp
#pragma pack(push, 1)

struct SBBloomFilterMetaPage {
    PageHeader bf_header;           // Standard page header (64 bytes)

    // Configuration (32 bytes)
    uint8_t bf_uuid[16];            // Parent index UUID (16 bytes)
    uint64_t bf_num_keys;           // Number of keys inserted (8 bytes)
    uint32_t bf_num_bits;           // Total bits in filter (4 bytes)
    uint16_t bf_num_hashes;         // Number of hash functions (k) (2 bytes)
    uint16_t bf_bits_per_key;       // Bits per key (2 bytes)

    // Storage (16 bytes)
    uint64_t bf_first_data_page;    // First data page (8 bytes)
    uint32_t bf_num_data_pages;     // Number of data pages (4 bytes)
    uint32_t bf_hash_seed;          // Hash function seed (4 bytes)

    // Statistics (32 bytes)
    uint64_t bf_false_positives;    // Estimated false positives (8 bytes)
    uint64_t bf_true_negatives;     // True negatives (8 bytes)
    uint64_t bf_total_queries;      // Total queries (8 bytes)
    uint64_t bf_last_rebuild_time;  // Unix timestamp (8 bytes)

    // Reserved (48 bytes)
    uint8_t bf_reserved[48];        // Future use

    // Total: 64 + 32 + 16 + 32 + 48 = 192 bytes
    uint8_t bf_padding[8000];       // Pad to 8192 bytes
} __attribute__((packed));

static_assert(sizeof(SBBloomFilterMetaPage) == 8192, "Meta page must be 8KB");

#pragma pack(pop)
```

### 2. Bloom Filter Data Page

```cpp
#pragma pack(push, 1)

struct SBBloomFilterDataPage {
    PageHeader bf_header;           // Standard page header (64 bytes)
    uint64_t bf_next_page;          // Next data page (0 if last) (8 bytes)

    // Bit array: 8192 - 72 = 8120 bytes = 64,960 bits
    uint8_t bf_bits[8120];          // Bit array
} __attribute__((packed));

static_assert(sizeof(SBBloomFilterDataPage) == 8192, "Data page must be 8KB");

// Bits per data page
constexpr uint32_t BITS_PER_DATA_PAGE = 8120 * 8;  // 64,960 bits

#pragma pack(pop)
```

### Storage Calculation

```cpp
// For 1 million keys with 10 bits/key:
// Total bits: 10,000,000
// Bits per page: 64,960
// Pages needed: ceil(10,000,000 / 64,960) = 154 pages = 1.23 MB

// For 10 million keys:
// Total bits: 100,000,000
// Pages needed: 1,540 pages = 12.3 MB
```

---

## MGA Compliance

### Challenge: Bloom Filters and MVCC

Bloom filters are **set data structures** that don't naturally support visibility or deletion. In an MGA system with concurrent transactions:

**Problem:**
- Transaction T1 inserts key K (xmin=100)
- Bloom filter records hash(K)
- Transaction T1 rolls back
- Transaction T2 queries for K
- Bloom filter says "might exist" (false positive)
- T2 wastes time searching B-Tree for non-existent key

### Solution: Conservative Approach

**Design Decision:** Bloom filters are **optimistic** and **ignore transaction visibility**.

```cpp
// Bloom filter check
bool might_exist = bloom_filter.test(key);
if (!might_exist) {
    return {};  // Definitely not present (safe to skip)
}

// Still need to check B-Tree with TIP visibility
auto tids = btree.search(key, current_xid);  // TIP-based filtering here
```

**Implications:**
1. **False positives increase temporarily** when transactions insert then rollback
2. **No correctness issues** (B-Tree still does TIP checks)
3. **Bloom filter cleanup** happens during garbage collection

### Garbage Collection Integration

```cpp
// During index GC (when OIT advances)
Status BloomFilter::rebuild(ErrorContext* ctx) {
    // 1. Create new empty Bloom filter
    auto new_filter = BloomFilter::create(...);

    // 2. Scan parent index for visible keys
    auto* txn_mgr = db_->getTransactionManager();
    TransactionId oit = txn_mgr->getOldestInterestingTransaction();

    for (auto& entry : parent_index->all_entries()) {
        // Only include committed, non-deleted entries
        if (entry.xmax == 0 || entry.xmax >= oit) {
            if (entry.xmin < oit) {
                new_filter->insert(entry.key);
            }
        }
    }

    // 3. Atomically swap filters
    std::swap(filter_, new_filter);

    return Status::OK;
}
```

**Rebuild triggers:**
1. After N insertions (e.g., 100K inserts → rebuild)
2. During VACUUM
3. Manual REINDEX command

### MGA Compliance Summary

✅ **Correctness:** Bloom filters don't affect correctness (TIP checks still happen)
✅ **Performance:** Temporary false positives are acceptable
⚠️ **Maintenance:** Require periodic rebuilds to remove rolled-back keys
✅ **Concurrency:** Lock-free reads, exclusive writes (standard index locking)

---

## Core API

### Class Definition

**File:** `include/scratchbird/core/bloom_filter.h`

```cpp
#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird {
namespace core {

// Forward declarations
class Database;
class BufferPool;

// Configuration
struct BloomFilterConfig {
    double target_fpr;         // Target false positive rate
    uint8_t bits_per_key;      // Bits per key
    uint8_t num_hashes;        // Number of hash functions (k)
    uint32_t rebuild_threshold; // Rebuild after N insertions
};

// Statistics
struct BloomFilterStatistics {
    uint64_t num_keys;          // Keys in filter
    uint64_t num_bits;          // Total bits
    uint64_t num_pages;         // Data pages
    uint64_t total_queries;     // Queries executed
    uint64_t true_negatives;    // Definite "not present"
    uint64_t false_positives;   // Estimated false positives
    double actual_fpr;          // Measured FPR
    double space_efficiency;    // Bytes per key
};

class BloomFilter {
public:
    // Constructor (for existing filter)
    BloomFilter(Database* db, uint32_t meta_page);

    // Destructor
    ~BloomFilter();

    // Create new Bloom filter
    static Status create(Database* db,
                        const BloomFilterConfig& config,
                        uint64_t estimated_keys,
                        uint32_t* meta_page_out,
                        ErrorContext* ctx = nullptr);

    // Open existing Bloom filter
    static std::unique_ptr<BloomFilter> open(Database* db,
                                            uint32_t meta_page,
                                            ErrorContext* ctx = nullptr);

    // Insert key (sets k bits)
    Status insert(const void* key_data, size_t key_len,
                 ErrorContext* ctx = nullptr);

    // Test key membership (checks k bits)
    // Returns: true = "might exist", false = "definitely not present"
    bool test(const void* key_data, size_t key_len,
             ErrorContext* ctx = nullptr);

    // Rebuild filter (GC integration)
    Status rebuild(ErrorContext* ctx = nullptr);

    // Get statistics
    BloomFilterStatistics getStatistics() const;

    // Get configuration
    const BloomFilterConfig& getConfig() const { return config_; }

    // Get meta page number
    uint32_t getMetaPage() const { return meta_page_; }

private:
    // Member variables
    Database* db_;
    uint32_t meta_page_;
    BloomFilterConfig config_;

    // Cached bit array (for performance)
    std::vector<uint8_t> bit_cache_;
    bool cache_dirty_;

    // Helper methods
    std::vector<uint64_t> hashKey(const void* key_data, size_t key_len);
    void setBit(uint64_t bit_index, ErrorContext* ctx);
    bool getBit(uint64_t bit_index, ErrorContext* ctx);
    uint32_t calculatePageNumber(uint64_t bit_index);
    uint32_t calculateByteOffset(uint64_t bit_index);
    uint8_t calculateBitOffset(uint64_t bit_index);

    // Flush cached bits to disk
    Status flushCache(ErrorContext* ctx);
};

} // namespace core
} // namespace scratchbird
```

---

## DML Integration

### Parent Index Extension

Modify existing index types (B-Tree, Hash, etc.) to support optional Bloom filters.

**File:** `include/scratchbird/core/btree.h` (additions)

```cpp
class BTree {
public:
    // ... existing methods ...

    // NEW: Bloom filter support
    Status attachBloomFilter(const BloomFilterConfig& config,
                            uint64_t estimated_keys,
                            ErrorContext* ctx = nullptr);

    Status detachBloomFilter(ErrorContext* ctx = nullptr);

    BloomFilter* getBloomFilter() const { return bloom_filter_.get(); }

private:
    // NEW: Bloom filter member
    std::unique_ptr<BloomFilter> bloom_filter_;
};
```

### DML Hook Modifications

**File:** `src/core/btree.cpp` (modifications)

```cpp
// INSERT hook
Status BTree::insert(const Key& key, const TID& tid, TransactionId xid,
                    ErrorContext* ctx) {
    // ... existing B-Tree insert logic ...

    // NEW: Update Bloom filter
    if (bloom_filter_) {
        auto status = bloom_filter_->insert(key.data, key.len, ctx);
        if (status != Status::OK) {
            LOG_WARN(Category::INDEX, "Failed to update Bloom filter");
            // Non-fatal: Continue with insert
        }
    }

    return Status::OK;
}

// SEARCH hook (optimization)
std::vector<TID> BTree::search(const Key& key, TransactionId current_xid,
                               ErrorContext* ctx) {
    // NEW: Check Bloom filter first
    if (bloom_filter_ && !bloom_filter_->test(key.data, key.len, ctx)) {
        // Definitely not present - skip B-Tree search
        LOG_DEBUG(Category::INDEX, "Bloom filter: key not present (skipped B-Tree)");
        return {};  // Empty result
    }

    // Bloom filter says "might exist" or not present
    // Continue with normal B-Tree search (with TIP visibility)
    return searchInternal(key, current_xid, ctx);
}

// DELETE hook (no Bloom filter update - filters don't support deletion)
Status BTree::remove(const Key& key, const TID& tid, TransactionId xid,
                    ErrorContext* ctx) {
    // ... existing B-Tree delete logic (sets xmax) ...

    // No Bloom filter update needed
    // Deleted keys remain in filter until rebuild

    return Status::OK;
}
```

### Rebuild During GC

**File:** `src/core/btree.cpp` (GC integration)

```cpp
Status BTree::removeDeadEntries(const std::vector<TID>& dead_tids,
                               uint64_t* entries_removed_out,
                               uint64_t* pages_modified_out,
                               ErrorContext* ctx) {
    // ... existing GC logic ...

    // Rebuild Bloom filter if present
    if (bloom_filter_) {
        auto stats = bloom_filter_->getStatistics();

        // Rebuild if many deletions (heuristic: >10% of keys deleted)
        if (entries_removed_out && *entries_removed_out > stats.num_keys * 0.1) {
            LOG_INFO(Category::INDEX, "Rebuilding Bloom filter after GC");
            auto status = bloom_filter_->rebuild(ctx);
            if (status != Status::OK) {
                LOG_WARN(Category::INDEX, "Bloom filter rebuild failed");
            }
        }
    }

    return Status::OK;
}
```

---

## Bytecode Integration

### SQL Syntax

```sql
-- Create index with Bloom filter
CREATE INDEX idx_email ON users(email)
WITH (bloom_filter = true, bloom_fpr = 0.01);

-- Create index without Bloom filter (default)
CREATE INDEX idx_username ON users(username);

-- Add Bloom filter to existing index
ALTER INDEX idx_username SET (bloom_filter = true);

-- Remove Bloom filter
ALTER INDEX idx_email SET (bloom_filter = false);

-- Rebuild Bloom filter
REINDEX INDEX idx_email;
```

### AST Additions

**File:** `include/scratchbird/parser/ast.h`

```cpp
struct IndexOptions {
    bool bloom_filter_enabled;      // Enable Bloom filter
    double bloom_fpr;               // Target false positive rate
    // ... existing options (unique, etc.) ...
};

struct CreateIndexStmt : public Statement {
    std::string table_name;
    std::string index_name;
    IndexType index_type;
    std::vector<std::string> columns;
    IndexOptions options;           // NEW
    // ... existing fields ...
};
```

### Bytecode Opcodes

**File:** `src/sblr/opcodes.h`

```cpp
// No new opcodes needed - Bloom filters are transparent
// CREATE_INDEX opcode (0x50) extended to include options

// Bytecode encoding:
// [CREATE_INDEX opcode]
// [index type]
// [table name length] [table name bytes]
// [index name length] [index name bytes]
// [column count] [column IDs...]
// [options flags]  // ← NEW: Bit 0 = bloom_filter_enabled
// [bloom_fpr (double 8 bytes)]  // ← NEW: If bit 0 set
```

### Bytecode Generation

**File:** `src/sblr/bytecode_generator.cpp`

```cpp
Status BytecodeGenerator::generateCreateIndex(const CreateIndexStmt* stmt,
                                              std::vector<uint8_t>* bytecode_out,
                                              ErrorContext* ctx) {
    // ... existing bytecode generation ...

    // NEW: Encode index options
    uint32_t options_flags = 0;
    if (stmt->options.bloom_filter_enabled) {
        options_flags |= 0x01;  // Bit 0: Bloom filter
    }
    encodeUint32(options_flags, bytecode_out);

    // Encode Bloom filter FPR if enabled
    if (stmt->options.bloom_filter_enabled) {
        encodeDouble(stmt->options.bloom_fpr, bytecode_out);
    }

    return Status::OK;
}
```

### Executor Integration

**File:** `src/sblr/executor.cpp`

```cpp
Status Executor::executeCreateIndex(const uint8_t* bytecode, size_t* offset,
                                   ErrorContext* ctx) {
    // ... decode table name, index name, columns, type ...

    // NEW: Decode options
    uint32_t options_flags = decodeUint32(bytecode, offset);
    bool bloom_enabled = (options_flags & 0x01) != 0;

    double bloom_fpr = 0.01;  // Default
    if (bloom_enabled) {
        bloom_fpr = decodeDouble(bytecode, offset);
    }

    // Create index via catalog
    auto* catalog = db_->getCatalogManager();
    auto index = catalog->createIndex(table_name, index_name, index_type, columns, ctx);
    if (!index) {
        return Status::INDEX_CREATION_FAILED;
    }

    // NEW: Attach Bloom filter if requested
    if (bloom_enabled) {
        BloomFilterConfig config;
        config.target_fpr = bloom_fpr;
        config.bits_per_key = calculateBitsPerKey(bloom_fpr);
        config.num_hashes = calculateNumHashes(config.bits_per_key);

        // Estimate keys from table statistics
        auto table = catalog->getTable(table_name, ctx);
        uint64_t estimated_keys = table ? table->estimated_row_count : 10000;

        auto status = index->attachBloomFilter(config, estimated_keys, ctx);
        if (status != Status::OK) {
            LOG_WARN(Category::EXECUTOR, "Failed to attach Bloom filter: %d", status);
            // Non-fatal: Index still created
        }
    }

    return Status::OK;
}
```

---

## Query Planner Integration

### Cost-Based Decision

The query planner should automatically use Bloom filters when beneficial.

**File:** `src/sblr/query_planner.cpp`

```cpp
Status QueryPlanner::planIndexScan(const Table* table,
                                  const Predicate& predicate,
                                  ScanPlan* plan_out,
                                  ErrorContext* ctx) {
    // Find candidate indexes
    auto indexes = findApplicableIndexes(table, predicate);

    for (auto& idx : indexes) {
        // Calculate cost with Bloom filter
        double cost = estimateIndexScanCost(idx, predicate);

        // NEW: Adjust cost if Bloom filter present
        if (idx->getBloomFilter()) {
            auto bf_stats = idx->getBloomFilter()->getStatistics();

            // For equality predicates, Bloom filter can skip scan entirely
            if (predicate.type == PredicateType::EQUALS) {
                // Probability of skipping scan = true negative rate
                double true_negative_rate = 1.0 - bf_stats.actual_fpr;

                // Expected cost = FPR × full_scan_cost + TNR × bloom_check_cost
                double bloom_check_cost = 0.01;  // 10 microseconds
                double expected_cost = bf_stats.actual_fpr * cost
                                     + true_negative_rate * bloom_check_cost;

                cost = expected_cost;

                LOG_DEBUG(Category::PLANNER,
                         "Bloom filter reduces cost: %.2f → %.2f",
                         cost, expected_cost);
            }
        }

        // ... select best index ...
    }

    return Status::OK;
}
```

### Statistics Tracking

Update statistics during query execution:

```cpp
// In B-Tree search
std::vector<TID> BTree::search(const Key& key, TransactionId current_xid,
                               ErrorContext* ctx) {
    if (bloom_filter_) {
        bool might_exist = bloom_filter_->test(key.data, key.len, ctx);

        if (!might_exist) {
            // True negative - update statistics
            bloom_filter_->incrementTrueNegatives();
            return {};
        }

        // Might exist - proceed with search
        bloom_filter_->incrementQuery();
        auto results = searchInternal(key, current_xid, ctx);

        // If no results found, this was a false positive
        if (results.empty()) {
            bloom_filter_->incrementFalsePositives();
        }

        return results;
    }

    return searchInternal(key, current_xid, ctx);
}
```

---

## Implementation Steps

### Phase 1: Core Implementation (16-24 hours)

1. **Setup xxHash (1 hour)**
   ```bash
   cd third_party/
   mkdir -p xxhash
   cd xxhash
   wget https://raw.githubusercontent.com/Cyan4973/xxHash/v0.8.2/xxhash.h
   ```

   Add to CMakeLists.txt:
   ```cmake
   include_directories(${CMAKE_SOURCE_DIR}/third_party/xxhash)
   ```

2. **Implement page structures (2 hours)**
   - Define `SBBloomFilterMetaPage`
   - Define `SBBloomFilterDataPage`
   - Add static assertions

3. **Implement BloomFilter class (8-12 hours)**
   - `create()` method (allocate pages)
   - `open()` method (load meta page)
   - `insert()` method (hash + set bits)
   - `test()` method (hash + check bits)
   - Helper methods (hashKey, setBit, getBit)

4. **Implement bit manipulation (2 hours)**
   ```cpp
   void BloomFilter::setBit(uint64_t bit_index, ErrorContext* ctx) {
       uint32_t page_num = calculatePageNumber(bit_index);
       uint32_t byte_offset = calculateByteOffset(bit_index);
       uint8_t bit_offset = calculateBitOffset(bit_index);

       // Pin page
       BufferFrame* frame = nullptr;
       auto status = buffer_pool_->pinPage(page_num, &frame, ctx);
       if (status != Status::OK) return;

       // Set bit
       auto* data_page = reinterpret_cast<SBBloomFilterDataPage*>(frame->getData());
       data_page->bf_bits[byte_offset] |= (1 << bit_offset);

       // Mark dirty and unpin
       frame->markDirty();
       buffer_pool_->unpinPage(page_num);
   }
   ```

5. **Implement statistics (2 hours)**
   - Track queries, true negatives, false positives
   - Calculate actual FPR

6. **Unit tests (4 hours)**
   - Test insert + test operations
   - Verify FPR matches theoretical value
   - Test page overflow

### Phase 2: Integration (12-16 hours)

1. **Extend B-Tree class (4 hours)**
   - Add `bloom_filter_` member
   - Implement `attachBloomFilter()`
   - Implement `detachBloomFilter()`

2. **DML hook modifications (4 hours)**
   - Update `BTree::insert()` to call `bloom_filter_->insert()`
   - Update `BTree::search()` to call `bloom_filter_->test()`
   - No changes to `remove()` (filters don't support deletion)

3. **GC integration (2 hours)**
   - Implement `BloomFilter::rebuild()`
   - Call rebuild from `BTree::removeDeadEntries()`

4. **Integration tests (4 hours)**
   - Test Bloom filter with INSERT/SELECT
   - Test false positive rate
   - Test rebuild after GC

### Phase 3: SQL/Bytecode (8-12 hours)

1. **Parser modifications (3 hours)**
   - Add `bloom_filter` option to CREATE INDEX
   - Add `bloom_fpr` option
   - Update AST structures

2. **Bytecode generation (2 hours)**
   - Extend CREATE_INDEX opcode encoding
   - Encode bloom_filter flags and FPR

3. **Executor integration (3 hours)**
   - Decode bloom_filter options
   - Call `attachBloomFilter()` during index creation

4. **SQL tests (2 hours)**
   - Test CREATE INDEX WITH (bloom_filter = true)
   - Test ALTER INDEX SET (bloom_filter = false)

### Phase 4: Query Planner (4-8 hours)

1. **Cost model integration (3 hours)**
   - Adjust index scan cost based on Bloom filter presence
   - Calculate expected cost reduction

2. **Statistics integration (2 hours)**
   - Expose Bloom filter statistics to planner
   - Update statistics during query execution

3. **Planner tests (2 hours)**
   - Verify Bloom filter used in query plans
   - Test cost estimates

### Phase 5: Documentation (2-4 hours)

1. **Update INDEX_ARCHITECTURE.md**
2. **Add usage examples**
3. **Document performance characteristics**

**Total:** 42-64 hours (5-8 days full-time)

---

## Testing Requirements

### Unit Tests

**File:** `tests/unit/test_bloom_filter.cpp`

```cpp
TEST(BloomFilterTest, InsertAndTest) {
    auto db = createTestDatabase();

    BloomFilterConfig config;
    config.target_fpr = 0.01;
    config.bits_per_key = 10;
    config.num_hashes = 7;

    uint32_t meta_page = 0;
    ErrorContext ctx;
    auto status = BloomFilter::create(db.get(), config, 10000, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto filter = BloomFilter::open(db.get(), meta_page, &ctx);
    ASSERT_NE(filter, nullptr);

    // Insert 1000 keys
    for (int i = 0; i < 1000; i++) {
        std::string key = "key_" + std::to_string(i);
        status = filter->insert(key.data(), key.size(), &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    // Test present keys (should all return true)
    int present_found = 0;
    for (int i = 0; i < 1000; i++) {
        std::string key = "key_" + std::to_string(i);
        if (filter->test(key.data(), key.size(), &ctx)) {
            present_found++;
        }
    }
    EXPECT_EQ(present_found, 1000);  // No false negatives

    // Test absent keys (should have ~1% false positives)
    int absent_found = 0;
    for (int i = 10000; i < 20000; i++) {
        std::string key = "key_" + std::to_string(i);
        if (filter->test(key.data(), key.size(), &ctx)) {
            absent_found++;
        }
    }
    double fpr = (double)absent_found / 10000.0;
    EXPECT_LT(fpr, 0.02);  // Less than 2% FPR
    EXPECT_GT(fpr, 0.005); // Greater than 0.5% FPR (within expected range)
}

TEST(BloomFilterTest, PageOverflow) {
    // Test with more keys than fit in one page
    auto db = createTestDatabase();

    BloomFilterConfig config;
    config.bits_per_key = 10;
    config.num_hashes = 7;

    uint32_t meta_page = 0;
    ErrorContext ctx;
    auto status = BloomFilter::create(db.get(), config, 100000, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto filter = BloomFilter::open(db.get(), meta_page, &ctx);

    // Insert 100K keys (requires multiple pages)
    for (int i = 0; i < 100000; i++) {
        std::string key = std::to_string(i);
        status = filter->insert(key.data(), key.size(), &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto stats = filter->getStatistics();
    EXPECT_GT(stats.num_pages, 1);  // Multiple pages used
}
```

### Integration Tests

**File:** `tests/integration/test_btree_bloom_filter.cpp`

```cpp
TEST(BTreeBloomFilterTest, InsertAndSearch) {
    auto db = createTestDatabase();
    auto table = createTestTable(db.get());

    // Create B-Tree with Bloom filter
    BloomFilterConfig config;
    config.target_fpr = 0.01;
    config.bits_per_key = 10;
    config.num_hashes = 7;

    auto btree = createBTreeIndex(table);
    ErrorContext ctx;
    auto status = btree->attachBloomFilter(config, 10000, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto xid = db->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);

    // Insert 1000 keys
    for (int i = 0; i < 1000; i++) {
        Key key = makeKey(i);
        TID tid(100 + i, 0);
        status = btree->insert(key, tid, xid, &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    db->getTransactionManager()->commitTransaction(xid);

    // Search for present keys
    auto xid2 = db->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);

    for (int i = 0; i < 1000; i++) {
        Key key = makeKey(i);
        auto results = btree->search(key, xid2, &ctx);
        EXPECT_EQ(results.size(), 1);
    }

    // Search for absent keys (should be skipped by Bloom filter)
    for (int i = 10000; i < 11000; i++) {
        Key key = makeKey(i);
        auto results = btree->search(key, xid2, &ctx);
        EXPECT_EQ(results.size(), 0);
    }

    db->getTransactionManager()->commitTransaction(xid2);

    // Check statistics
    auto bf_stats = btree->getBloomFilter()->getStatistics();
    EXPECT_GT(bf_stats.true_negatives, 900);  // Most absent keys skipped
}

TEST(BTreeBloomFilterTest, RebuildAfterGC) {
    auto db = createTestDatabase();
    auto table = createTestTable(db.get());
    auto btree = createBTreeIndex(table);

    BloomFilterConfig config;
    ErrorContext ctx;
    btree->attachBloomFilter(config, 10000, &ctx);

    // Insert and delete many keys
    auto xid1 = db->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);

    for (int i = 0; i < 1000; i++) {
        Key key = makeKey(i);
        TID tid(100 + i, 0);
        btree->insert(key, tid, xid1, &ctx);
    }
    db->getTransactionManager()->commitTransaction(xid1);

    // Delete half the keys
    auto xid2 = db->getTransactionManager()->beginTransaction(
        IsolationLevel::READ_COMMITTED, false);

    for (int i = 0; i < 500; i++) {
        Key key = makeKey(i);
        TID tid(100 + i, 0);
        btree->remove(key, tid, xid2, &ctx);
    }
    db->getTransactionManager()->commitTransaction(xid2);

    // Run GC (should rebuild Bloom filter)
    uint64_t removed = 0;
    auto status = btree->removeDeadEntries({}, &removed, nullptr, &ctx);
    ASSERT_EQ(status, Status::OK);

    // Bloom filter should be smaller now
    auto stats_after = btree->getBloomFilter()->getStatistics();
    EXPECT_LT(stats_after.num_keys, 1000);
}
```

### SQL Tests

**File:** `tests/integration/test_bloom_filter_sql.cpp`

```cpp
TEST(BloomFilterSQLTest, CreateIndexWithBloomFilter) {
    auto db = createTestDatabase();

    executeSQL(db.get(), R"(
        CREATE TABLE users (
            id INT PRIMARY KEY,
            email VARCHAR(255)
        )
    )");

    executeSQL(db.get(), R"(
        CREATE INDEX idx_email ON users(email)
        WITH (bloom_filter = true, bloom_fpr = 0.01)
    )");

    // Verify Bloom filter attached
    auto catalog = db->getCatalogManager();
    auto index = catalog->getIndex("idx_email");
    ASSERT_NE(index, nullptr);
    EXPECT_NE(index->getBloomFilter(), nullptr);

    auto config = index->getBloomFilter()->getConfig();
    EXPECT_DOUBLE_EQ(config.target_fpr, 0.01);
}

TEST(BloomFilterSQLTest, AlterIndexAddBloomFilter) {
    auto db = createTestDatabase();

    executeSQL(db.get(), R"(
        CREATE TABLE products (
            id INT PRIMARY KEY,
            name VARCHAR(255)
        )
    )");

    executeSQL(db.get(), R"(
        CREATE INDEX idx_name ON products(name)
    )");

    // Initially no Bloom filter
    auto catalog = db->getCatalogManager();
    auto index = catalog->getIndex("idx_name");
    EXPECT_EQ(index->getBloomFilter(), nullptr);

    // Add Bloom filter
    executeSQL(db.get(), R"(
        ALTER INDEX idx_name SET (bloom_filter = true)
    )");

    // Now should have Bloom filter
    index = catalog->getIndex("idx_name");
    EXPECT_NE(index->getBloomFilter(), nullptr);
}
```

---

## Performance Targets

### Latency

- **Bloom filter check:** < 10 microseconds (in-memory bit array)
- **Bloom filter insert:** < 20 microseconds (7 hash computations + bit sets)
- **Overhead on index insert:** < 5% additional latency
- **Overhead on index search (hit):** < 2% (false positive)
- **Speedup on index search (miss):** 10-100x (skip B-Tree traversal)

### Memory

- **1 million keys:** 1.25 MB (10 bits/key)
- **10 million keys:** 12.5 MB
- **100 million keys:** 125 MB
- **Acceptable overhead:** < 10% of index size

### False Positive Rate

- **Default:** 1% (10 bits/key, k=7)
- **Acceptable range:** 0.1% - 5%
- **Measure actual FPR:** Track false positives vs. total queries

### Benchmarks

```sql
-- Insert 1M rows
INSERT INTO users SELECT i, 'user' || i || '@example.com'
FROM generate_series(1, 1000000) AS i;

-- Create index with Bloom filter
CREATE INDEX idx_email ON users(email)
WITH (bloom_filter = true);

-- Benchmark: Point lookup (present key)
SELECT * FROM users WHERE email = 'user500000@example.com';
-- Target: 0.5-2ms (with Bloom filter: +0.01ms overhead)

-- Benchmark: Point lookup (absent key)
SELECT * FROM users WHERE email = 'nonexistent@example.com';
-- Target: Without Bloom: 1-5ms, With Bloom: 0.01-0.05ms (100x speedup)
```

---

## Future Enhancements

### Phase 2 Improvements (Future Work)

1. **Counting Bloom Filters**
   - Support deletion by tracking counts per bit
   - 4-8x memory overhead, but supports remove()

2. **Partitioned Bloom Filters**
   - Separate filters per tablespace/partition
   - Better for distributed queries

3. **Adaptive Bloom Filters**
   - Auto-adjust bits_per_key based on measured FPR
   - Rebuild with different parameters if needed

4. **Compressed Bloom Filters**
   - Use RLE or dictionary compression on bit arrays
   - Trade CPU for memory

5. **Bloom Filter Merge**
   - Efficiently merge multiple Bloom filters (during index merge)
   - Useful for multi-level indexes

6. **Per-Page Bloom Filters**
   - Attach small Bloom filters to B-Tree internal nodes
   - More granular pruning

### Advanced Features

1. **Learned Bloom Filters**
   - Use ML model to predict membership
   - Potentially lower FPR for same memory

2. **Cuckoo Filters**
   - Alternative to Bloom filters with deletion support
   - Slightly higher memory but more flexible

3. **XOR Filters**
   - Modern alternative, ~20% smaller than Bloom
   - Static (no insertions after construction)

---

## Conclusion

This specification provides a complete, implementation-ready design for Bloom Filter indexes in ScratchBird.

**Key Features:**
- ✅ MGA-compliant (conservative approach, no correctness issues)
- ✅ 8KB page-aligned storage
- ✅ Transparent integration with existing indexes
- ✅ Full SQL support via CREATE INDEX WITH (bloom_filter = true)
- ✅ Automatic GC and rebuild
- ✅ Query planner integration
- ✅ Production-ready defaults (1% FPR, 10 bits/key)

**Implementation Effort:** 42-64 hours (5-8 days)

**Risk Level:** LOW - Clear algorithm, no major architectural changes

**Next Steps:**
1. Review this specification with team
2. Begin Phase 1 implementation (core Bloom filter)
3. Test thoroughly with unit tests
4. Integrate with B-Tree
5. Benchmark and tune

---

**Specification Status:** READY FOR REVIEW
**Reviewer:** Please provide feedback on:
- Architecture decisions (auxiliary vs. standalone)
- MGA compliance approach (conservative, no transaction tracking in filter)
- SQL syntax (WITH (bloom_filter = true))
- Missing considerations or edge cases

**Author:** ScratchBird Architecture Team
**Date:** November 20, 2025
