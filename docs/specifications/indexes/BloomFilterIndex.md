# Bloom Filter Index Specification for ScratchBird

**Version:** 1.1
**Date:** November 20, 2025
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Alpha - Phase 2
**Update:** Page-size agnostic (supports 8K, 16K, 32K, 64K, 128K pages)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Mathematical Foundation](#mathematical-foundation)
4. [External Dependencies](#external-dependencies)
5. [On-Disk Structures](#on-disk-structures)
6. [Page Size Considerations](#page-size-considerations)
7. [MGA Compliance](#mga-compliance)
8. [Core API](#core-api)
9. [DML Integration](#dml-integration)
10. [Bytecode Integration](#bytecode-integration)
11. [Query Planner Integration](#query-planner-integration)
12. [Implementation Steps](#implementation-steps)
13. [Testing Requirements](#testing-requirements)
14. [Performance Targets](#performance-targets)
15. [Future Enhancements](#future-enhancements)

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
- **Page-size aware**: Adapts to database page size (8K/16K/32K/64K/128K)

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
└── Bit array (page_size - 72 bytes)
```

### 1. Bloom Filter Meta Page

**Note:** Meta page structure is page-size independent (uses fixed 192-byte header plus padding).

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

    // Total fixed header: 64 + 32 + 16 + 32 + 48 = 192 bytes
    // Padding: page_size - 192 bytes (filled with zeros)
    uint8_t bf_padding[];           // Flexible array member - size determined at runtime
} __attribute__((packed));

// Note: No static_assert - page size varies
// Minimum page size (8KB) verified at runtime

#pragma pack(pop)
```

### 2. Bloom Filter Data Page

**Note:** Data page uses flexible array member to adapt to page size.

```cpp
#pragma pack(push, 1)

struct SBBloomFilterDataPage {
    PageHeader bf_header;           // Standard page header (64 bytes)
    uint64_t bf_next_page;          // Next data page (0 if last) (8 bytes)

    // Bit array: page_size - 72 bytes (flexible)
    uint8_t bf_bits[];              // Flexible array member
} __attribute__((packed));

// No static_assert - size varies by page_size

#pragma pack(pop)
```

### Storage Calculation Helpers

```cpp
class BloomFilter {
private:
    Database* db_;

    // Get database page size
    uint32_t getPageSize() const {
        return db_->getPageSize();
    }

    // Calculate bits per data page (varies by page size)
    uint32_t getBitsPerPage() const {
        uint32_t page_size = getPageSize();
        uint32_t header_size = sizeof(PageHeader) + sizeof(uint64_t);  // 72 bytes
        uint32_t available_bytes = page_size - header_size;
        return available_bytes * 8;  // Convert bytes to bits
    }

    // Calculate number of pages needed for N keys
    uint32_t calculatePagesNeeded(uint64_t num_keys) const {
        uint64_t total_bits = num_keys * config_.bits_per_key;
        uint32_t bits_per_page = getBitsPerPage();
        return (total_bits + bits_per_page - 1) / bits_per_page;  // Ceiling division
    }
};
```

---

## Page Size Considerations

### Capacity by Page Size

Different page sizes provide different Bloom filter capacities per page:

| Page Size | Header | Bit Array Size | Bits Per Page | Keys @10bits/key |
|-----------|--------|---------------|---------------|------------------|
| 8 KB      | 72 B   | 8,120 B       | 64,960 bits   | 6,496 keys      |
| 16 KB     | 72 B   | 16,312 B      | 130,496 bits  | 13,050 keys     |
| 32 KB     | 72 B   | 32,696 B      | 261,568 bits  | 26,157 keys     |
| 64 KB     | 72 B   | 65,464 B      | 523,712 bits  | 52,371 keys     |
| 128 KB    | 72 B   | 131,000 B     | 1,048,000 bits| 104,800 keys    |

### Storage Examples

**Example 1: 1 million keys, 10 bits/key, 1% FPR**

| Page Size | Total Bits | Bits/Page | Pages Needed | Total Size |
|-----------|-----------|-----------|--------------|------------|
| 8 KB      | 10M       | 64,960    | 154 pages    | 1.23 MB    |
| 16 KB     | 10M       | 130,496   | 77 pages     | 1.23 MB    |
| 32 KB     | 10M       | 261,568   | 39 pages     | 1.25 MB    |
| 64 KB     | 10M       | 523,712   | 20 pages     | 1.28 MB    |
| 128 KB    | 10M       | 1,048,000 | 10 pages     | 1.28 MB    |

**Observation:** Total storage is similar (~1.25 MB), but larger pages = fewer pages to manage.

**Example 2: 100 million keys, 10 bits/key, 1% FPR**

| Page Size | Total Bits | Bits/Page | Pages Needed | Total Size |
|-----------|-----------|-----------|--------------|------------|
| 8 KB      | 1B        | 64,960    | 15,399 pages | 123.1 MB   |
| 16 KB     | 1B        | 130,496   | 7,663 pages  | 122.6 MB   |
| 32 KB     | 1B        | 261,568   | 3,825 pages  | 122.5 MB   |
| 64 KB     | 1B        | 523,712   | 1,911 pages  | 122.3 MB   |
| 128 KB    | 1B        | 1,048,000 | 955 pages    | 122.3 MB   |

### Recommendations by Database Size

**Small databases (< 1 GB):**
- Use **8 KB or 16 KB pages**
- More granular I/O control
- Lower memory overhead for small indexes

**Medium databases (1 GB - 100 GB):**
- Use **16 KB or 32 KB pages**
- Balanced I/O efficiency
- Standard for most workloads

**Large databases (> 100 GB):**
- Use **32 KB, 64 KB, or 128 KB pages**
- Fewer pages to manage
- Better sequential I/O
- Lower metadata overhead

### Implementation Validation

```cpp
// Runtime validation in BloomFilter::create()
Status BloomFilter::create(Database* db,
                          const BloomFilterConfig& config,
                          uint64_t estimated_keys,
                          uint32_t* meta_page_out,
                          ErrorContext* ctx) {
    // Validate page size
    uint32_t page_size = db->getPageSize();
    if (page_size < 8192 || page_size > 131072) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Invalid page size: must be 8K-128K");
        return Status::INVALID_ARGUMENT;
    }

    // Validate that page_size is power of 2
    if ((page_size & (page_size - 1)) != 0) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Page size must be power of 2");
        return Status::INVALID_ARGUMENT;
    }

    // Calculate capacity
    uint32_t bits_per_page = getBitsPerPage();
    LOG_INFO(Category::INDEX, "Bloom filter: %u bits per page (page_size=%u)",
            bits_per_page, page_size);

    // ... continue with creation ...
}
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
2. During sweep/GC
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
    uint32_t page_size;         // Database page size
    uint32_t bits_per_page;     // Bits per data page
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
    uint32_t getPageSize() const;
    uint32_t getBitsPerPage() const;
    uint32_t calculatePagesNeeded(uint64_t num_keys) const;

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

### Helper Method Implementations

```cpp
// Get database page size
uint32_t BloomFilter::getPageSize() const {
    return db_->getPageSize();
}

// Calculate bits per data page (varies by page size)
uint32_t BloomFilter::getBitsPerPage() const {
    uint32_t page_size = getPageSize();
    uint32_t header_size = sizeof(PageHeader) + sizeof(uint64_t);  // 72 bytes
    uint32_t available_bytes = page_size - header_size;
    return available_bytes * 8;  // Convert bytes to bits
}

// Calculate number of pages needed
uint32_t BloomFilter::calculatePagesNeeded(uint64_t num_keys) const {
    uint64_t total_bits = num_keys * config_.bits_per_key;
    uint32_t bits_per_page = getBitsPerPage();
    return (total_bits + bits_per_page - 1) / bits_per_page;  // Ceiling
}

// Calculate which page contains a bit
uint32_t BloomFilter::calculatePageNumber(uint64_t bit_index) {
    uint32_t bits_per_page = getBitsPerPage();
    return bit_index / bits_per_page;
}

// Calculate byte offset within page
uint32_t BloomFilter::calculateByteOffset(uint64_t bit_index) {
    uint32_t bits_per_page = getBitsPerPage();
    uint64_t bit_in_page = bit_index % bits_per_page;
    return bit_in_page / 8;
}

// Calculate bit offset within byte
uint8_t BloomFilter::calculateBitOffset(uint64_t bit_index) {
    return bit_index % 8;
}
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

**File:** `include/scratchbird/parser/ast_v2.h`

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

**File:** `src/sblr/bytecode_generator_v2.cpp`

```cpp
void BytecodeGeneratorV2::generateCreateIndex(ResolvedCreateIndexStmt* stmt) {
    // ... existing bytecode generation ...

    // NEW: Encode index options
    uint32_t options_flags = 0;
    if (stmt->options.bloom_filter_enabled) {
        options_flags |= 0x01;  // Bit 0: Bloom filter
    }
    encodeUint32(options_flags, &bytecode_);

    // Encode Bloom filter FPR if enabled
    if (stmt->options.bloom_filter_enabled) {
        encodeDouble(stmt->options.bloom_fpr, &bytecode_);
    }

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

2. **Implement page structures (3 hours)**
   - Define `SBBloomFilterMetaPage` with flexible array
   - Define `SBBloomFilterDataPage` with flexible array
   - Implement page-size helpers (getPageSize, getBitsPerPage)
   - Add runtime validation

3. **Implement BloomFilter class (8-12 hours)**
   - `create()` method (allocate pages, adapt to page size)
   - `open()` method (load meta page)
   - `insert()` method (hash + set bits)
   - `test()` method (hash + check bits)
   - Helper methods (hashKey, setBit, getBit, calculatePageNumber)

4. **Implement bit manipulation (2 hours)**
   ```cpp
   void BloomFilter::setBit(uint64_t bit_index, ErrorContext* ctx) {
       uint32_t bits_per_page = getBitsPerPage();
       uint32_t page_num = calculatePageNumber(bit_index);
       uint32_t byte_offset = calculateByteOffset(bit_index);
       uint8_t bit_offset = calculateBitOffset(bit_index);

       // Pin page
       BufferFrame* frame = nullptr;
       auto status = buffer_pool_->pinPage(page_num, &frame, ctx);
       if (status != Status::OK) return;

       // Set bit (page-size agnostic)
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
   - Include page_size and bits_per_page in statistics

6. **Unit tests (4 hours)**
   - Test insert + test operations
   - Verify FPR matches theoretical value
   - **Test with multiple page sizes (8K, 16K, 32K, 64K, 128K)**
   - Test page overflow for each page size

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
   - **Test across different page sizes**

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
3. **Document performance characteristics by page size**

**Total:** 42-64 hours (5-8 days full-time)

---

## Testing Requirements

### Unit Tests

**File:** `tests/unit/test_bloom_filter.cpp`

```cpp
// Test with 8KB pages
TEST(BloomFilterTest, InsertAndTest_8KB) {
    auto db = createTestDatabase(8192);  // 8KB page size

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

    // Verify bits per page
    auto stats = filter->getStatistics();
    EXPECT_EQ(stats.page_size, 8192);
    EXPECT_EQ(stats.bits_per_page, 64960);  // (8192 - 72) * 8

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

// Test with 32KB pages
TEST(BloomFilterTest, InsertAndTest_32KB) {
    auto db = createTestDatabase(32768);  // 32KB page size

    BloomFilterConfig config;
    config.bits_per_key = 10;
    config.num_hashes = 7;

    uint32_t meta_page = 0;
    ErrorContext ctx;
    auto status = BloomFilter::create(db.get(), config, 100000, &meta_page, &ctx);
    ASSERT_EQ(status, Status::OK);

    auto filter = BloomFilter::open(db.get(), meta_page, &ctx);

    // Verify bits per page
    auto stats_before = filter->getStatistics();
    EXPECT_EQ(stats_before.page_size, 32768);
    EXPECT_EQ(stats_before.bits_per_page, 261568);  // (32768 - 72) * 8

    // Insert 100K keys
    for (int i = 0; i < 100000; i++) {
        std::string key = std::to_string(i);
        status = filter->insert(key.data(), key.size(), &ctx);
        ASSERT_EQ(status, Status::OK);
    }

    auto stats_after = filter->getStatistics();

    // With 32KB pages, should use fewer pages than 8KB
    // 100K keys × 10 bits = 1M bits = 125KB
    // 32KB pages: 261,568 bits/page → ~4 pages
    EXPECT_LE(stats_after.num_pages, 5);
}

// Test page overflow across different page sizes
TEST(BloomFilterTest, PageOverflow_AllSizes) {
    std::vector<uint32_t> page_sizes = {8192, 16384, 32768, 64768, 131072};

    for (uint32_t page_size : page_sizes) {
        auto db = createTestDatabase(page_size);

        BloomFilterConfig config;
        config.bits_per_key = 10;
        config.num_hashes = 7;

        uint32_t meta_page = 0;
        ErrorContext ctx;
        auto status = BloomFilter::create(db.get(), config, 100000, &meta_page, &ctx);
        ASSERT_EQ(status, Status::OK);

        auto filter = BloomFilter::open(db.get(), meta_page, &ctx);

        // Insert 100K keys
        for (int i = 0; i < 100000; i++) {
            std::string key = std::to_string(i);
            status = filter->insert(key.data(), key.size(), &ctx);
            ASSERT_EQ(status, Status::OK);
        }

        auto stats = filter->getStatistics();

        // Verify pages allocated matches calculation
        uint64_t total_bits = 100000 * 10;
        uint32_t expected_pages = (total_bits + stats.bits_per_page - 1) / stats.bits_per_page;
        EXPECT_EQ(stats.num_pages, expected_pages);

        LOG_INFO(Category::TEST,
                "Page size %u: %u pages, %u bits/page",
                page_size, stats.num_pages, stats.bits_per_page);
    }
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

### Page Size Impact

**I/O Efficiency:**
- Larger pages → fewer page reads for large filters
- Smaller pages → more granular cache control

**Recommendation:** Use database's default page size for consistency

---

## Tablespace + TID/GPID Requirements

- **TID format:** All index entries must store `TID` with full `GPID + slot` (no legacy 32-bit page IDs).
- **Tablespace routing:** Bloom filter pages must be allocated/pinned via `root_gpid` and `tablespace_id`.
- **Cross-tablespace visibility:** Heap lookups from Bloom filter hits must pin heap pages via `pinPageGlobal` using the `GPID` in the `TID`.
- **Migration:** If table pages migrate between tablespaces, the Bloom filter must support `updateTIDsAfterMigration` using `GPID` mapping for any stored TIDs.

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

This specification provides a complete, implementation-ready design for Bloom Filter indexes in ScratchBird that **supports all page sizes (8K, 16K, 32K, 64K, 128K)**.

**Key Features:**
- ✅ **Page-size agnostic** - adapts to database page size at runtime
- ✅ MGA-compliant (conservative approach, no correctness issues)
- ✅ Flexible array members for dynamic sizing
- ✅ Runtime capacity calculations
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
3. Test thoroughly with unit tests **across all page sizes**
4. Integrate with B-Tree
5. Benchmark and tune

---

**Specification Status:** READY FOR REVIEW (v1.1 - Page-size agnostic)
**Reviewer:** Please provide feedback on:
- Page-size agnostic design (flexible arrays, runtime calculations)
- Architecture decisions (auxiliary vs. standalone)
- MGA compliance approach (conservative, no transaction tracking in filter)
- SQL syntax (WITH (bloom_filter = true))
- Missing considerations or edge cases

**Author:** ScratchBird Architecture Team
**Date:** November 20, 2025
**Version:** 1.1 (Updated for multi-page-size support)
