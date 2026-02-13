# Phase 1 Cleanup Implementation Guide

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Status**: Optional - Low Priority Enhancements
**Est. Time**: 24-37 hours total
**Value**: Code quality, statistics persistence, minor query plan improvements

---

## Overview

Phase 1 is **100% functionally complete** with all 8 critical tasks delivered. The items in this guide are **optional cleanup tasks** that improve code quality, enable statistics persistence, and provide minor query plan optimizations. None are blocking for Phase 2.

**Recommendation**: Defer these to a post-Phase 2 cleanup sprint unless statistics persistence becomes critical for your use case.

---

## A. Statistics Catalog Persistence (8-12 hours)

**Current State**: Statistics are collected and cached in-memory but lost on restart
**Goal**: Persist statistics to disk in `pg_statistic` catalog table
**Benefit**: Statistics survive database restarts, no need to re-run ANALYZE

### Implementation Steps

#### 1. Define pg_statistic Catalog Structure (2 hours)

**File**: `include/scratchbird/core/catalog_manager.h`

Add struct definition:
```cpp
// PostgreSQL-style pg_statistic catalog
struct PGStatisticRecord {
    uint8_t is_valid;                    // MGA: 1 = valid, 0 = deleted
    UuidV7Bytes table_id;                // FK to pg_class
    UuidV7Bytes column_id;               // Column UUID
    float null_fraction;                 // Fraction of NULL values (0.0-1.0)
    uint64_t n_distinct;                 // Estimated number of distinct values
    uint32_t avg_width;                  // Average column value width in bytes

    // Most Common Values (MCVs) - stored as TOAST reference
    uint32_t mcv_toast_ref;              // Reference to MCVEntry array in TOAST
    uint16_t mcv_count;                  // Number of MCVs

    // Histogram - stored as TOAST reference
    uint32_t histogram_toast_ref;        // Reference to HistogramBucket array
    uint16_t histogram_count;            // Number of histogram buckets
    uint8_t histogram_type;              // 0=equal-height, 1=equal-width

    uint64_t sample_size;                // Number of rows sampled
    uint64_t total_rows;                 // Total rows at analysis time
    uint64_t analysis_timestamp;         // Unix timestamp of last ANALYZE

    char padding[64];                    // Future expansion
} __attribute__((packed));
```

#### 2. Create pg_statistic Catalog Table (2 hours)

**File**: `src/core/catalog_manager.cpp`

Add to `CatalogManager::initializeRootPage()`:

```cpp
// Create pg_statistic catalog table
GPID pg_statistic_gpid = makeGPID(PRIMARY_TABLESPACE_ID, root->pg_statistic_page);
if (root->pg_statistic_page == 0) {
    // Allocate new page for pg_statistic catalog
    uint32_t page_id;
    Status status = page_mgr_->allocatePage(PRIMARY_TABLESPACE_ID, &page_id, ctx);
    if (status != Status::OK) {
        return status;
    }
    root->pg_statistic_page = page_id;

    // Initialize catalog heap page
    void *page_buffer;
    status = buffer_pool->pinPage(page_id, &page_buffer, ctx);
    if (status != Status::OK) {
        return status;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    heap->header.page_type = PageType::CATALOG_HEAP;
    heap->header.free_space = page_size - sizeof(CatalogHeapPage);
    heap->header.generation = 1;
    heap->record_count = 0;
    heap->free_offset = sizeof(CatalogHeapPage);

    buffer_pool->unpinPage(page_id, true, ctx);
}
```

Add member variable to `CatalogManager`:
```cpp
uint32_t pg_statistic_page_;  // Page ID for pg_statistic catalog
```

#### 3. Implement storeColumnStatistics() (3-4 hours)

**File**: `src/optimizer/statistics_manager.cpp`

Replace current in-memory-only implementation (line ~583):

```cpp
auto StatisticsManager::storeColumnStatistics(const ID &table_id,
                                               const ID &column_id,
                                               const ColumnStatistics &stats,
                                               ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(cache_mutex_);

    // 1. Update cache (keep existing code)
    uint64_t cache_key = getCacheKey(table_id, column_id);
    column_stats_cache_[cache_key] = stats;

    // 2. NEW: Persist to pg_statistic catalog
    PGStatisticRecord record;
    record.is_valid = 1;
    std::memcpy(record.table_id.bytes.data(), table_id.bytes.data(), 16);
    std::memcpy(record.column_id.bytes.data(), column_id.bytes.data(), 16);
    record.null_fraction = stats.null_fraction;
    record.n_distinct = stats.n_distinct;
    record.avg_width = stats.avg_width;
    record.sample_size = stats.sample_size;
    record.total_rows = stats.total_rows;
    record.analysis_timestamp = std::time(nullptr);

    // 3. Store MCVs to TOAST
    if (!stats.most_common_values.empty()) {
        // Serialize MCVs to bytes
        std::vector<uint8_t> mcv_data;
        // TODO: Serialize MCVEntry array to mcv_data

        // Store to TOAST
        uint32_t toast_ref;
        Status status = catalog_->storeToTOAST(mcv_data, &toast_ref, ctx);
        if (status != Status::OK) {
            return status;
        }
        record.mcv_toast_ref = toast_ref;
        record.mcv_count = static_cast<uint16_t>(stats.most_common_values.size());
    } else {
        record.mcv_toast_ref = 0;
        record.mcv_count = 0;
    }

    // 4. Store histogram to TOAST
    if (!stats.histogram.empty()) {
        // Serialize histogram to bytes
        std::vector<uint8_t> hist_data;
        // TODO: Serialize HistogramBucket array to hist_data

        // Store to TOAST
        uint32_t toast_ref;
        Status status = catalog_->storeToTOAST(hist_data, &toast_ref, ctx);
        if (status != Status::OK) {
            return status;
        }
        record.histogram_toast_ref = toast_ref;
        record.histogram_count = static_cast<uint16_t>(stats.histogram.size());
        record.histogram_type = 0; // equal-height
    } else {
        record.histogram_toast_ref = 0;
        record.histogram_count = 0;
    }

    // 5. Persist record to pg_statistic
    auto predicate = [&](const PGStatisticRecord &rec) {
        return std::memcmp(rec.table_id.bytes.data(), table_id.bytes.data(), 16) == 0 &&
               std::memcmp(rec.column_id.bytes.data(), column_id.bytes.data(), 16) == 0;
    };

    return catalog_->updateRecordInHeapPage<PGStatisticRecord>(
        catalog_->pg_statistic_page(), predicate, record, ctx);
}
```

#### 4. Implement loadColumnStatistics() (2-3 hours)

**File**: `src/optimizer/statistics_manager.cpp`

Replace current NOT_IMPLEMENTED stub (line ~998):

```cpp
auto StatisticsManager::loadColumnStatistics(const ID &table_id,
                                              const ID &column_id,
                                              ColumnStatistics &stats,
                                              ErrorContext *ctx) -> Status
{
    // 1. Find record in pg_statistic
    auto predicate = [&](const PGStatisticRecord &rec) {
        return rec.is_valid &&
               std::memcmp(rec.table_id.bytes.data(), table_id.bytes.data(), 16) == 0 &&
               std::memcmp(rec.column_id.bytes.data(), column_id.bytes.data(), 16) == 0;
    };

    auto result = catalog_->findRecordInHeapPage<PGStatisticRecord>(
        catalog_->pg_statistic_page(), predicate, ctx);

    if (result.status != Status::OK) {
        return result.status;
    }

    const auto &rec = result.record;

    // 2. Load basic statistics
    stats.null_fraction = rec.null_fraction;
    stats.n_distinct = rec.n_distinct;
    stats.avg_width = rec.avg_width;
    stats.sample_size = rec.sample_size;
    stats.total_rows = rec.total_rows;

    // 3. Load MCVs from TOAST
    if (rec.mcv_count > 0 && rec.mcv_toast_ref != 0) {
        std::vector<uint8_t> mcv_data;
        Status status = catalog_->loadFromTOAST(rec.mcv_toast_ref, mcv_data, ctx);
        if (status != Status::OK) {
            return status;
        }
        // TODO: Deserialize mcv_data to stats.most_common_values
    }

    // 4. Load histogram from TOAST
    if (rec.histogram_count > 0 && rec.histogram_toast_ref != 0) {
        std::vector<uint8_t> hist_data;
        Status status = catalog_->loadFromTOAST(rec.histogram_toast_ref, hist_data, ctx);
        if (status != Status::OK) {
            return status;
        }
        // TODO: Deserialize hist_data to stats.histogram
    }

    return Status::OK;
}
```

#### 5. Load Statistics on Startup (1 hour)

**File**: `src/core/database.cpp`

Add to `Database::open()` after catalog initialization:

```cpp
// Preload statistics cache from pg_statistic
// This populates the in-memory cache for fast query planning
Status stats_status = statistics_manager_->preloadStatistics(ctx);
if (stats_status != Status::OK) {
    // Non-fatal: log warning and continue with empty cache
    DEBUG_LOG_DB("Warning: Failed to preload statistics cache");
}
```

**File**: `src/optimizer/statistics_manager.cpp`

Add new method:
```cpp
auto StatisticsManager::preloadStatistics(ErrorContext *ctx) -> Status
{
    // Scan pg_statistic and populate cache
    std::vector<PGStatisticRecord> records;
    Status status = catalog_->scanHeapPage<PGStatisticRecord>(
        catalog_->pg_statistic_page(), records, ctx);

    if (status != Status::OK) {
        return status;
    }

    std::lock_guard<std::mutex> lock(cache_mutex_);
    for (const auto &rec : records) {
        if (!rec.is_valid) continue;

        ID table_id, column_id;
        std::memcpy(table_id.bytes.data(), rec.table_id.bytes.data(), 16);
        std::memcpy(column_id.bytes.data(), rec.column_id.bytes.data(), 16);

        ColumnStatistics stats;
        // Load full statistics (including TOAST data)
        Status load_status = loadColumnStatistics(table_id, column_id, stats, ctx);
        if (load_status == Status::OK) {
            uint64_t cache_key = getCacheKey(table_id, column_id);
            column_stats_cache_[cache_key] = stats;
        }
    }

    return Status::OK;
}
```

### Testing

Create test file: `tests/unit/test_statistics_persistence.cpp`

```cpp
#include <gtest/gtest.h>
#include "scratchbird/core/database.h"
#include "scratchbird/optimizer/statistics_manager.h"

TEST(StatisticsPersistence, StoreAndLoad) {
    // 1. Create database and table
    // 2. Run ANALYZE to collect statistics
    // 3. Close database
    // 4. Reopen database
    // 5. Verify statistics are loaded from pg_statistic
    // 6. Compare with original statistics
}

TEST(StatisticsPersistence, UpdateExisting) {
    // 1. Store initial statistics
    // 2. Update statistics (re-run ANALYZE)
    // 3. Verify old statistics replaced (not duplicated)
}

TEST(StatisticsPersistence, TOASTHandling) {
    // 1. Create table with many distinct values (large histogram)
    // 2. Run ANALYZE
    // 3. Verify MCVs and histogram stored to TOAST
    // 4. Reload and verify data integrity
}
```

---

## B. Query Planner Enhancements (6-10 hours)

**Current State**: Query planner generates functional plans but doesn't set all metadata
**Goal**: Set filter expressions and index conditions for better introspection
**Benefit**: Slightly better query plans, improved EXPLAIN output

### Implementation Steps

#### 1. Implement Filter Expression Setting (3-5 hours)

**File**: `src/optimizer/query_planner.cpp`

Replace TODO at line 448:

```cpp
// Build base scan paths
for (const auto &table : from_tables_) {
    // Sequential scan path
    auto seq_scan_path = std::make_shared<SeqScanPath>();
    seq_scan_path->table_id = table->table_id;
    seq_scan_path->table_name = table->name;
    seq_scan_path->rows = 1000.0; // Default estimate
    seq_scan_path->startup_cost = 0.0;

    // NEW: Set filter expression from WHERE clause
    if (select_stmt->where_clause()) {
        seq_scan_path->filter_expr = select_stmt->where_clause();

        // Estimate selectivity
        double selectivity = estimateSelectivity(
            select_stmt->where_clause(), table->table_id, ctx);
        seq_scan_path->rows *= selectivity;
    }

    // ... rest of scan path generation
}
```

#### 2. Add Index Condition Extraction (3-5 hours)

**File**: `src/optimizer/query_planner.cpp`

Replace TODO at line 473:

```cpp
// Generate index scan paths
for (const auto &index : indexes) {
    auto index_path = std::make_shared<IndexScanPath>();
    index_path->index_id = index.index_id;
    index_path->table_id = table->table_id;

    // NEW: Extract index-compatible conditions from WHERE clause
    std::vector<parser::Expression*> index_conditions;
    std::vector<parser::Expression*> filter_conditions;

    extractIndexConditions(select_stmt->where_clause(),
                           index,
                           index_conditions,
                           filter_conditions);

    index_path->index_conditions = index_conditions;
    index_path->filter_expr = combineConditions(filter_conditions);

    // Estimate index scan cost
    double index_selectivity = estimateIndexSelectivity(
        index_conditions, index, ctx);
    index_path->rows = base_rows * index_selectivity;

    // ... rest of index scan path generation
}
```

Add helper methods:

```cpp
void QueryPlanner::extractIndexConditions(
    parser::Expression *where_clause,
    const IndexInfo &index,
    std::vector<parser::Expression*> &index_conds,
    std::vector<parser::Expression*> &filter_conds)
{
    // Recursively traverse WHERE clause
    // Identify conditions that can use index (e.g., indexed_col = value)
    // Separate into index-usable and filter-only conditions

    if (!where_clause) return;

    // Handle AND chains
    if (auto *binary_op = dynamic_cast<parser::BinaryOpExpr*>(where_clause)) {
        if (binary_op->op() == parser::BinaryOp::AND) {
            extractIndexConditions(binary_op->left(), index, index_conds, filter_conds);
            extractIndexConditions(binary_op->right(), index, index_conds, filter_conds);
            return;
        }

        // Check if this condition can use the index
        if (canUseIndex(binary_op, index)) {
            index_conds.push_back(binary_op);
        } else {
            filter_conds.push_back(binary_op);
        }
    } else {
        // Single condition
        filter_conds.push_back(where_clause);
    }
}

bool QueryPlanner::canUseIndex(parser::BinaryOpExpr *expr, const IndexInfo &index)
{
    // Check if expression matches index columns
    // For now, simple equality checks on first index column

    if (expr->op() != parser::BinaryOp::EQ) {
        return false;
    }

    // Check if left side is indexed column
    if (auto *id_expr = dynamic_cast<parser::IdentifierExpr*>(expr->left())) {
        for (const auto &col : index.columns) {
            if (id_expr->name() == col.name) {
                // Right side should be constant or parameter
                return isConstant(expr->right());
            }
        }
    }

    return false;
}
```

### Testing

Add to existing `test_query_planner.cpp`:

```cpp
TEST(QueryPlanner, FilterExpressionSet) {
    // Verify filter_expr is set on scan paths
    auto plan = planner.generatePlan("SELECT * FROM users WHERE age > 25");
    ASSERT_NE(plan->filter_expr, nullptr);
}

TEST(QueryPlanner, IndexConditionExtraction) {
    // Verify index conditions extracted correctly
    auto plan = planner.generatePlan(
        "SELECT * FROM users WHERE user_id = 123 AND status = 'active'");

    auto *index_scan = dynamic_cast<IndexScanNode*>(plan.get());
    ASSERT_NE(index_scan, nullptr);
    ASSERT_FALSE(index_scan->index_conditions.empty());
}
```

---

## C. Catalog Helper Methods (10-15 hours)

**Current State**: `updateRecordInHeapPage` and `deleteRecordFromHeapPage` exist, but `findRecordInHeapPage` and `scanHeapPage` are missing
**Goal**: Implement missing helpers to clean up 14 TODOs
**Benefit**: Cleaner code, enables charset/collation catalog operations

### Implementation Steps

#### 1. Implement findRecordInHeapPage() (3-4 hours)

**File**: `src/core/catalog_manager.cpp`

Add after `deleteRecordFromHeapPage()` (around line 1422):

```cpp
// ============================================================================
// findRecordInHeapPage - Generic record finder
// ============================================================================
// Searches for a record matching a predicate and returns it with metadata.
// Returns the record and its slot index for potential updates.
// ============================================================================

template <typename RecordType, typename Predicate>
struct FindResult {
    Status status;
    RecordType record;
    uint32_t slot_index;  // For subsequent updates
};

template <typename RecordType, typename Predicate>
auto CatalogManager::findRecordInHeapPage(uint32_t page_id, Predicate matcher,
                                           ErrorContext *ctx)
    -> FindResult<RecordType>
{
    FindResult<RecordType> result;
    result.status = Status::NOT_FOUND;
    result.slot_index = 0;

    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    Status status = bp->pinPage(page_id, &page_buffer, ctx);
    if (status != Status::OK) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
        result.status = status;
        return result;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    uint32_t offset = sizeof(CatalogHeapPage);

    // Search for matching record
    for (uint32_t i = 0; i < heap->record_count; i++) {
        auto *record = reinterpret_cast<RecordType *>(
            reinterpret_cast<uint8_t *>(page_buffer) + offset);

        if (record->is_valid && matcher(*record)) {
            // Found matching record
            result.record = *record;
            result.slot_index = i;
            result.status = Status::OK;
            bp->unpinPage(page_id, false, ctx);
            return result;
        }

        offset += sizeof(RecordType);
    }

    // Record not found
    bp->unpinPage(page_id, false, ctx);
    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found in catalog");
    result.status = Status::NOT_FOUND;
    return result;
}
```

#### 2. Implement scanHeapPage() (4-5 hours)

**File**: `src/core/catalog_manager.cpp`

Add after `findRecordInHeapPage()`:

```cpp
// ============================================================================
// scanHeapPage - Generic page scanner with conversion
// ============================================================================
// Scans all valid records in a page and converts them to output type.
// Used for listing operations (e.g., listTimezones, listCharsets).
// ============================================================================

template <typename RecordType, typename OutputType, typename Converter>
auto CatalogManager::scanHeapPage(uint32_t page_id,
                                   std::vector<OutputType> &results,
                                   Converter converter,
                                   ErrorContext *ctx) -> Status
{
    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    Status status = bp->pinPage(page_id, &page_buffer, ctx);
    if (status != Status::OK) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
        return status;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    uint32_t offset = sizeof(CatalogHeapPage);

    // Scan all valid records
    for (uint32_t i = 0; i < heap->record_count; i++) {
        auto *record = reinterpret_cast<RecordType *>(
            reinterpret_cast<uint8_t *>(page_buffer) + offset);

        if (record->is_valid) {
            OutputType output;
            converter(*record, output);
            results.push_back(output);
        }

        offset += sizeof(RecordType);
    }

    bp->unpinPage(page_id, false, ctx);
    return Status::OK;
}

// Overload with filter predicate
template <typename RecordType, typename OutputType, typename Converter, typename Predicate>
auto CatalogManager::scanHeapPageWithFilter(uint32_t page_id,
                                             std::vector<OutputType> &results,
                                             Converter converter,
                                             Predicate filter,
                                             ErrorContext *ctx) -> Status
{
    BufferPool *bp = db_->buffer_pool();
    void *page_buffer;
    Status status = bp->pinPage(page_id, &page_buffer, ctx);
    if (status != Status::OK) {
        SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
        return status;
    }

    auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
    uint32_t offset = sizeof(CatalogHeapPage);

    // Scan and filter records
    for (uint32_t i = 0; i < heap->record_count; i++) {
        auto *record = reinterpret_cast<RecordType *>(
            reinterpret_cast<uint8_t *>(page_buffer) + offset);

        if (record->is_valid && filter(*record)) {
            OutputType output;
            converter(*record, output);
            results.push_back(output);
        }

        offset += sizeof(RecordType);
    }

    bp->unpinPage(page_id, false, ctx);
    return Status::OK;
}
```

#### 3. Add Method Declarations to Header (1 hour)

**File**: `include/scratchbird/core/catalog_manager.h`

Add in private section:

```cpp
// Generic catalog helper methods
template <typename RecordType, typename Predicate>
struct FindResult {
    Status status;
    RecordType record;
    uint32_t slot_index;
};

template <typename RecordType, typename Predicate>
auto findRecordInHeapPage(uint32_t page_id, Predicate matcher,
                          ErrorContext *ctx = nullptr)
    -> FindResult<RecordType>;

template <typename RecordType, typename OutputType, typename Converter>
auto scanHeapPage(uint32_t page_id, std::vector<OutputType> &results,
                  Converter converter, ErrorContext *ctx = nullptr) -> Status;

template <typename RecordType, typename OutputType, typename Converter, typename Predicate>
auto scanHeapPageWithFilter(uint32_t page_id, std::vector<OutputType> &results,
                            Converter converter, Predicate filter,
                            ErrorContext *ctx = nullptr) -> Status;
```

#### 4. Implement Charset Operations (2-3 hours)

**File**: `src/core/catalog_manager.cpp`

Replace TODOs at lines 2024, 2032, 2040, 2047:

```cpp
auto CatalogManager::getCharset(uint16_t charset_id, CharsetInfo &info,
                                ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto predicate = [charset_id](const CharsetRecord &rec) {
        return rec.charset_id == charset_id && rec.is_valid;
    };

    auto result = findRecordInHeapPage<CharsetRecord>(
        charsets_table_page_, predicate, ctx);

    if (result.status != Status::OK) {
        return result.status;
    }

    // Convert to CharsetInfo
    const auto &rec = result.record;
    info.charset_id = rec.charset_id;
    info.name = rec.name;
    info.default_collation_id = rec.default_collation_id;
    // ... rest of conversion

    return Status::OK;
}

auto CatalogManager::listCharsets(std::vector<CharsetInfo> &charsets,
                                   ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto converter = [](const CharsetRecord &rec, CharsetInfo &info) {
        info.charset_id = rec.charset_id;
        info.name = rec.name;
        info.default_collation_id = rec.default_collation_id;
        // ... rest of conversion
    };

    return scanHeapPage<CharsetRecord, CharsetInfo>(
        charsets_table_page_, charsets, converter, ctx);
}

// Similar for updateCharset, deleteCharset, getCharsetByName
```

#### 5. Implement Collation Operations (2-3 hours)

Similar pattern for collation operations at lines 2082, 2090, 2098, 2106, 2115, 2123.

### Testing

Add to existing catalog tests:

```cpp
TEST(CatalogHelpers, FindRecord) {
    // Test findRecordInHeapPage with various predicates
}

TEST(CatalogHelpers, ScanPage) {
    // Test scanHeapPage returns all valid records
}

TEST(CatalogHelpers, ScanPageWithFilter) {
    // Test filtered scan returns only matching records
}

TEST(CharsetOps, CreateAndRetrieve) {
    // Verify charset operations work end-to-end
}

TEST(CollationOps, CreateAndList) {
    // Verify collation operations work end-to-end
}
```

---

## Priority Recommendation

**If time is limited**, implement in this order:

1. **Skip all three** and proceed to Phase 2 (recommended)
   - Phase 1 is functionally complete
   - These are code quality improvements, not features
   - Can be done in a dedicated cleanup sprint later

2. **If statistics persistence is critical**:
   - Do A (Statistics Catalog Persistence) only
   - Defers B and C entirely
   - Est. time: 8-12 hours

3. **If doing a comprehensive cleanup**:
   - Do C first (enables A and B to have cleaner code)
   - Then do A (most user-visible benefit)
   - Finally do B (minor query improvements)
   - Total: 24-37 hours

---

## Success Criteria

After implementing any of these:

1. **Statistics Persistence (A)**:
   - ✅ ANALYZE results survive database restart
   - ✅ No need to re-run ANALYZE on startup
   - ✅ Test: Close DB, reopen, verify statistics loaded

2. **Query Planner (B)**:
   - ✅ EXPLAIN output shows filter expressions
   - ✅ Index scan paths have index_conditions set
   - ✅ Minor improvement in query plan costs

3. **Catalog Helpers (C)**:
   - ✅ All 14 charset/collation TODOs resolved
   - ✅ Charset and collation catalog ops work
   - ✅ Cleaner, more maintainable code

---

## Conclusion

These are **optional polish items** that improve code quality but don't add functionality. The audit confirmed Phase 1 is **100% functionally complete** without them.

**Recommendation**: Document completion of Phase 1 as-is and proceed to Phase 2. These items can be revisited during a dedicated "technical debt" cleanup sprint after Phase 2 or 3.

**Estimated ROI**: Low - significant time investment for minor improvements. Better to invest that time in Phase 2 features that add real user value (spatial types, triggers, CTEs).
