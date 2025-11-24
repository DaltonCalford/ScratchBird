# CRUD Implementation Plan - Missing System Operations

**Status:** Implementation Required
**Priority:** High (Core functionality)
**Estimated Effort:** 60-80 hours
**Target:** Alpha 1 Completion

---

## Overview

This plan addresses all missing CRUD (Create, Read, Update, Delete) operations across system catalog tables and core subsystems. These stubbed implementations were identified during codebase analysis and need to be completed for full system functionality.

### Gap Analysis Summary

**Total Items:** 61 stubbed operations across 6 subsystems

| Subsystem | File | NOT_IMPLEMENTED Count | Primary Issues |
|-----------|------|----------------------|----------------|
| Catalog Manager | catalog_manager.cpp | 22 | Timezone/Charset/Collation CUD ops, helper functions |
| Domain Manager | domain_manager.cpp | 18 | RECORD/SET/VARIANT operations (TypedValue dependent) |
| Statistics Manager | statistics_manager.cpp | 6 | Table/column analysis, stats persistence |
| Storage Engine | storage_engine.cpp | 4 | Non-BTree index DML operations |
| GIN Index | gin_index.cpp | 4 | Custom tablespace support |
| Charset | charset.cpp | 2 | Character set conversions |
| Heap Page | heap_page.cpp | 2 | Multi-page version chains (3+) |
| Auth Provider | auth_provider.cpp | 2 | Authentication methods |
| BTree Vacuum | btree_vacuum.cpp | 1 | Leaf node merging |

---

## Agent Organization - 4 Parallel Tracks

### Agent A: Catalog CRUD Operations (20-25 hours)
**Focus:** Complete all missing catalog table CRUD operations
**Files:** `src/core/catalog_manager.cpp`, `include/scratchbird/core/catalog_manager.h`

### Agent B: Helper Functions & Infrastructure (15-20 hours)
**Focus:** Implement reusable helper functions for catalog operations
**Files:** `src/core/catalog_manager.cpp`, `src/core/heap_page.cpp`

### Agent C: Statistics & Analysis (10-15 hours)
**Focus:** Statistics collection and persistence
**Files:** `src/optimizer/statistics_manager.cpp`, catalog integration

### Agent D: Domain & Type Operations (15-20 hours)
**Focus:** Advanced type operations (RECORD, SET, VARIANT)
**Files:** `src/core/domain_manager.cpp`, `include/scratchbird/core/typed_value.h`

---

## Agent A: Catalog CRUD Operations

### Phase A.1: Timezone CRUD (4-6 hours)

**File:** `src/core/catalog_manager.cpp:3178-3183`

#### A.1.1: Implement `updateTimezone`

**Current State:**
```cpp
auto CatalogManager::updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info,
                                    ErrorContext *ctx) -> Status
{
    // TODO: Needs findRecordInHeapPage and updateRecordInHeapPage helper functions
    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED, "updateTimezone not fully implemented");
    return Status::NOT_IMPLEMENTED;
}
```

**Implementation:**
```cpp
auto CatalogManager::updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info,
                                    ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(timezone_cache_mutex_);

    // Step 1: Find existing record in pg_timezone
    auto converter = [&](const uint8_t* record_data, size_t size) -> std::optional<TimezoneRecord> {
        if (size < sizeof(TimezoneRecord)) return std::nullopt;
        TimezoneRecord rec;
        std::memcpy(&rec, record_data, sizeof(TimezoneRecord));
        return (rec.tz_id == timezone_id) ? std::optional(rec) : std::nullopt;
    };

    std::optional<TimezoneRecord> existing_rec;
    Status status = findRecordInHeapPage<TimezoneRecord>(
        pg_timezone_page_, converter, existing_rec, ctx);
    if (status != Status::OK || !existing_rec.has_value()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Timezone not found for update");
        return Status::NOT_FOUND;
    }

    // Step 2: Prepare updated record
    TimezoneRecord updated = *existing_rec;
    std::strncpy(updated.tz_name, tz_info.tz_name.c_str(), sizeof(updated.tz_name) - 1);
    updated.tz_name[sizeof(updated.tz_name) - 1] = '\0';
    std::strncpy(updated.posix_string, tz_info.posix_string.c_str(), sizeof(updated.posix_string) - 1);
    updated.posix_string[sizeof(updated.posix_string) - 1] = '\0';
    updated.is_dst = tz_info.is_dst;
    updated.gmt_offset_seconds = tz_info.gmt_offset_seconds;

    // Step 3: Update record in heap page
    status = updateRecordInHeapPage(pg_timezone_page_,
                                    reinterpret_cast<const uint8_t*>(&updated),
                                    sizeof(TimezoneRecord),
                                    [timezone_id](const uint8_t* data, size_t size) {
                                        if (size < sizeof(TimezoneRecord)) return false;
                                        auto rec = reinterpret_cast<const TimezoneRecord*>(data);
                                        return rec->tz_id == timezone_id;
                                    },
                                    ctx);

    if (status != Status::OK) {
        return status;
    }

    // Step 4: Update in-memory cache
    timezone_cache_[timezone_id] = tz_info;

    DEBUG_LOG_DB("Updated timezone: " << tz_info.tz_name << " (ID: " << timezone_id << ")");
    return Status::OK;
}
```

**Testing:**
```cpp
TEST_F(CatalogManagerTest, UpdateTimezone) {
    // Create initial timezone
    TimezoneInfo tz_info;
    tz_info.tz_name = "America/New_York";
    tz_info.posix_string = "EST5EDT,M3.2.0,M11.1.0";
    tz_info.is_dst = true;
    tz_info.gmt_offset_seconds = -18000;

    uint16_t tz_id;
    ASSERT_OK(catalog_mgr_->createTimezone(tz_info, tz_id, &ctx_));

    // Update timezone
    tz_info.posix_string = "EST5EDT,M3.2.0/02:00,M11.1.0/02:00";  // Updated DST transition times
    ASSERT_OK(catalog_mgr_->updateTimezone(tz_id, tz_info, &ctx_));

    // Verify update
    TimezoneInfo retrieved;
    ASSERT_OK(catalog_mgr_->getTimezone(tz_id, retrieved, &ctx_));
    EXPECT_EQ(retrieved.posix_string, tz_info.posix_string);
}
```

---

### Phase A.2: Charset CRUD (5-7 hours)

**Files:** `src/core/catalog_manager.cpp:3334-3429`

#### A.2.1: Implement `updateCharset`

**Current State:** Line 3334-3339 - NOT_IMPLEMENTED

**Implementation:**
```cpp
auto CatalogManager::updateCharset(uint16_t charset_id, const CharsetInfo &cs_info,
                                   ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(charset_cache_mutex_);

    // Step 1: Find existing charset record
    auto finder = [charset_id](const uint8_t* data, size_t size) -> bool {
        if (size < sizeof(CharsetCatalogRecord)) return false;
        auto rec = reinterpret_cast<const CharsetCatalogRecord*>(data);
        return rec->charset_id == charset_id;
    };

    std::optional<CharsetCatalogRecord> existing_rec;
    Status status = findRecordInHeapPage<CharsetCatalogRecord>(
        pg_charsets_page_,
        [&](const uint8_t* data, size_t size) -> std::optional<CharsetCatalogRecord> {
            if (!finder(data, size)) return std::nullopt;
            CharsetCatalogRecord rec;
            std::memcpy(&rec, data, sizeof(CharsetCatalogRecord));
            return rec;
        },
        existing_rec,
        ctx);

    if (status != Status::OK || !existing_rec.has_value()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Charset not found");
        return Status::NOT_FOUND;
    }

    // Step 2: Prepare updated record
    CharsetCatalogRecord updated = *existing_rec;
    std::strncpy(updated.charset_name, cs_info.charset_name.c_str(),
                 sizeof(updated.charset_name) - 1);
    updated.charset_name[sizeof(updated.charset_name) - 1] = '\0';
    updated.max_bytes_per_char = cs_info.max_bytes_per_char;
    updated.min_bytes_per_char = cs_info.min_bytes_per_char;
    updated.is_variable_width = cs_info.is_variable_width;

    // Step 3: Update in heap page
    status = updateRecordInHeapPage(pg_charsets_page_,
                                    reinterpret_cast<const uint8_t*>(&updated),
                                    sizeof(CharsetCatalogRecord),
                                    finder,
                                    ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 4: Update cache
    charset_cache_[charset_id] = cs_info;

    DEBUG_LOG_DB("Updated charset: " << cs_info.charset_name);
    return Status::OK;
}
```

#### A.2.2: Implement `deleteCharset`

**Current State:** Line 3425-3429 - NOT_IMPLEMENTED

**Implementation:**
```cpp
auto CatalogManager::deleteCharset(uint16_t charset_id, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(charset_cache_mutex_);

    // Step 1: Check for dependent collations
    std::vector<CollationCatalogInfo> dependent_collations;
    Status status = listCollationsForCharset(charset_id, dependent_collations, ctx);
    if (status == Status::OK && !dependent_collations.empty()) {
        SET_ERROR_CONTEXT(ctx, Status::DEPENDENCY_ERROR,
                          "Cannot delete charset with existing collations");
        return Status::DEPENDENCY_ERROR;
    }

    // Step 2: Check if charset is used by any tables (via column data types)
    // TODO: Add dependency check when column metadata tracking is complete

    // Step 3: Delete record from pg_charsets
    auto finder = [charset_id](const uint8_t* data, size_t size) -> bool {
        if (size < sizeof(CharsetCatalogRecord)) return false;
        auto rec = reinterpret_cast<const CharsetCatalogRecord*>(data);
        return rec->charset_id == charset_id;
    };

    status = deleteRecordInHeapPage(pg_charsets_page_, finder, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 4: Remove from cache
    charset_cache_.erase(charset_id);

    DEBUG_LOG_DB("Deleted charset ID: " << charset_id);
    return Status::OK;
}
```

**Testing:**
```cpp
TEST_F(CatalogManagerTest, DeleteCharsetWithDependencies) {
    // Create charset
    CharsetInfo cs_info;
    cs_info.charset_name = "UTF-8";
    cs_info.max_bytes_per_char = 4;
    cs_info.min_bytes_per_char = 1;
    uint16_t cs_id;
    ASSERT_OK(catalog_mgr_->createCharset(cs_info, cs_id, &ctx_));

    // Create collation using this charset
    CollationCatalogInfo coll_info;
    coll_info.collation_name = "utf8_general_ci";
    coll_info.charset_id = cs_id;
    uint32_t coll_id;
    ASSERT_OK(catalog_mgr_->createCollation(coll_info, coll_id, &ctx_));

    // Attempt to delete charset - should fail
    EXPECT_EQ(catalog_mgr_->deleteCharset(cs_id, &ctx_), Status::DEPENDENCY_ERROR);

    // Delete collation first
    ASSERT_OK(catalog_mgr_->deleteCollation(coll_id, &ctx_));

    // Now delete charset should succeed
    ASSERT_OK(catalog_mgr_->deleteCharset(cs_id, &ctx_));
}
```

---

### Phase A.3: Collation CRUD (5-7 hours)

**Files:** `src/core/catalog_manager.cpp:3556-3570`

#### A.3.1: Implement `listCollationsForCharset`

**Current State:** Line 3556-3564 - Needs `scanHeapPageWithFilter` helper

**Implementation:**
```cpp
auto CatalogManager::listCollationsForCharset(uint16_t charset_id,
                                              std::vector<CollationCatalogInfo> &collations,
                                              ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(collation_cache_mutex_);

    collations.clear();

    // Use scanHeapPageWithFilter to find all collations for this charset
    auto filter = [charset_id](const uint8_t* data, size_t size) -> bool {
        if (size < sizeof(CollationCatalogRecord)) return false;
        auto rec = reinterpret_cast<const CollationCatalogRecord*>(data);
        return rec->charset_id == charset_id;
    };

    auto converter = [](const uint8_t* data, size_t size) -> std::optional<CollationCatalogInfo> {
        if (size < sizeof(CollationCatalogRecord)) return std::nullopt;

        CollationCatalogRecord rec;
        std::memcpy(&rec, data, sizeof(CollationCatalogRecord));

        CollationCatalogInfo info;
        info.collation_id = rec.collation_id;
        info.collation_name = std::string(rec.collation_name);
        info.charset_id = rec.charset_id;
        info.case_insensitive = rec.case_insensitive;
        info.accent_insensitive = rec.accent_insensitive;
        info.language = std::string(rec.language);

        return info;
    };

    Status status = scanHeapPageWithFilter<CollationCatalogInfo>(
        pg_collations_page_, filter, converter, collations, ctx);

    DEBUG_LOG_DB("Found " << collations.size() << " collations for charset ID: " << charset_id);
    return status;
}
```

#### A.3.2: Implement `deleteCollation`

**Current State:** Line 3566-3570 - NOT_IMPLEMENTED

**Implementation:**
```cpp
auto CatalogManager::deleteCollation(uint32_t collation_id, ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(collation_cache_mutex_);

    // Step 1: Check for tables using this collation
    // TODO: Add dependency check when table column metadata is fully tracked

    // Step 2: Delete record from pg_collations
    auto finder = [collation_id](const uint8_t* data, size_t size) -> bool {
        if (size < sizeof(CollationCatalogRecord)) return false;
        auto rec = reinterpret_cast<const CollationCatalogRecord*>(data);
        return rec->collation_id == collation_id;
    };

    Status status = deleteRecordInHeapPage(pg_collations_page_, finder, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 3: Remove from cache
    collation_cache_.erase(collation_id);

    DEBUG_LOG_DB("Deleted collation ID: " << collation_id);
    return Status::OK;
}
```

---

## Agent B: Helper Functions & Infrastructure

### Phase B.1: Heap Page Helper Functions (10-15 hours)

**File:** `src/core/catalog_manager.cpp` (new helper methods)

#### B.1.1: Implement `findRecordInHeapPage` Template

**Purpose:** Generic function to find a single record in a heap page

**Location:** Add to `catalog_manager.cpp` private methods section

**Implementation:**
```cpp
template<typename RecordType>
auto CatalogManager::findRecordInHeapPage(
    PageID page_id,
    std::function<std::optional<RecordType>(const uint8_t*, size_t)> converter,
    std::optional<RecordType>& record_out,
    ErrorContext* ctx) -> Status
{
    // Step 1: Read page
    uint8_t page_data[PAGE_SIZE];
    Status status = page_manager_->readPage(page_id, page_data, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 2: Parse heap page header
    auto header = reinterpret_cast<HeapPageHeader*>(page_data);
    if (header->magic != HEAP_PAGE_MAGIC) {
        SET_ERROR_CONTEXT(ctx, Status::CORRUPTION,
                          "Invalid heap page magic number");
        return Status::CORRUPTION;
    }

    // Step 3: Iterate through line pointers
    auto line_pointers = reinterpret_cast<LinePointer*>(
        page_data + sizeof(HeapPageHeader));

    for (uint16_t i = 0; i < header->num_tuples; ++i) {
        if (line_pointers[i].flags & LP_DEAD) {
            continue;  // Skip dead tuples
        }

        // Get tuple data
        const uint8_t* tuple_data = page_data + line_pointers[i].offset;
        size_t tuple_size = line_pointers[i].length;

        // Try to convert
        auto result = converter(tuple_data, tuple_size);
        if (result.has_value()) {
            record_out = result;
            return Status::OK;
        }
    }

    // Not found
    record_out = std::nullopt;
    return Status::NOT_FOUND;
}
```

#### B.1.2: Implement `updateRecordInHeapPage`

**Implementation:**
```cpp
auto CatalogManager::updateRecordInHeapPage(
    PageID page_id,
    const uint8_t* new_data,
    size_t new_size,
    std::function<bool(const uint8_t*, size_t)> finder,
    ErrorContext* ctx) -> Status
{
    // Step 1: Read page
    uint8_t page_data[PAGE_SIZE];
    Status status = page_manager_->readPage(page_id, page_data, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 2: Parse heap page header
    auto header = reinterpret_cast<HeapPageHeader*>(page_data);
    if (header->magic != HEAP_PAGE_MAGIC) {
        SET_ERROR_CONTEXT(ctx, Status::CORRUPTION, "Invalid heap page");
        return Status::CORRUPTION;
    }

    // Step 3: Find record to update
    auto line_pointers = reinterpret_cast<LinePointer*>(
        page_data + sizeof(HeapPageHeader));

    for (uint16_t i = 0; i < header->num_tuples; ++i) {
        if (line_pointers[i].flags & LP_DEAD) {
            continue;
        }

        const uint8_t* tuple_data = page_data + line_pointers[i].offset;
        size_t tuple_size = line_pointers[i].length;

        if (finder(tuple_data, tuple_size)) {
            // Found the record - check size compatibility
            if (new_size > tuple_size) {
                // Need to relocate tuple (complex - may need page split)
                SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                  "Tuple expansion requires page reorganization");
                return Status::NOT_IMPLEMENTED;
            }

            // In-place update
            std::memcpy(const_cast<uint8_t*>(tuple_data), new_data, new_size);

            // Update line pointer if size changed
            if (new_size != tuple_size) {
                line_pointers[i].length = static_cast<uint16_t>(new_size);
            }

            // Write updated page
            return page_manager_->writePage(page_id, page_data, ctx);
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found for update");
    return Status::NOT_FOUND;
}
```

#### B.1.3: Implement `deleteRecordInHeapPage`

**Implementation:**
```cpp
auto CatalogManager::deleteRecordInHeapPage(
    PageID page_id,
    std::function<bool(const uint8_t*, size_t)> finder,
    ErrorContext* ctx) -> Status
{
    // Step 1: Read page
    uint8_t page_data[PAGE_SIZE];
    Status status = page_manager_->readPage(page_id, page_data, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 2: Find and mark record as dead
    auto header = reinterpret_cast<HeapPageHeader*>(page_data);
    auto line_pointers = reinterpret_cast<LinePointer*>(
        page_data + sizeof(HeapPageHeader));

    for (uint16_t i = 0; i < header->num_tuples; ++i) {
        if (line_pointers[i].flags & LP_DEAD) {
            continue;
        }

        const uint8_t* tuple_data = page_data + line_pointers[i].offset;
        size_t tuple_size = line_pointers[i].length;

        if (finder(tuple_data, tuple_size)) {
            // Mark as dead (Firebird MGA style - tombstone)
            line_pointers[i].flags |= LP_DEAD;

            // Update free space (but don't compact - leave for SWEEP)
            header->free_space += line_pointers[i].length;

            // Write updated page
            return page_manager_->writePage(page_id, page_data, ctx);
        }
    }

    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found for deletion");
    return Status::NOT_FOUND;
}
```

#### B.1.4: Implement `scanHeapPageWithFilter`

**Implementation:**
```cpp
template<typename RecordType>
auto CatalogManager::scanHeapPageWithFilter(
    PageID page_id,
    std::function<bool(const uint8_t*, size_t)> filter,
    std::function<std::optional<RecordType>(const uint8_t*, size_t)> converter,
    std::vector<RecordType>& records_out,
    ErrorContext* ctx) -> Status
{
    // Step 1: Read page
    uint8_t page_data[PAGE_SIZE];
    Status status = page_manager_->readPage(page_id, page_data, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 2: Parse heap page
    auto header = reinterpret_cast<HeapPageHeader*>(page_data);
    auto line_pointers = reinterpret_cast<LinePointer*>(
        page_data + sizeof(HeapPageHeader));

    // Step 3: Scan all tuples
    for (uint16_t i = 0; i < header->num_tuples; ++i) {
        if (line_pointers[i].flags & LP_DEAD) {
            continue;
        }

        const uint8_t* tuple_data = page_data + line_pointers[i].offset;
        size_t tuple_size = line_pointers[i].length;

        // Apply filter
        if (filter(tuple_data, tuple_size)) {
            auto record = converter(tuple_data, tuple_size);
            if (record.has_value()) {
                records_out.push_back(*record);
            }
        }
    }

    return Status::OK;
}
```

**Header Declaration:** Add to `catalog_manager.h` private section:
```cpp
// Heap page helper functions
template<typename RecordType>
Status findRecordInHeapPage(
    PageID page_id,
    std::function<std::optional<RecordType>(const uint8_t*, size_t)> converter,
    std::optional<RecordType>& record_out,
    ErrorContext* ctx);

Status updateRecordInHeapPage(
    PageID page_id,
    const uint8_t* new_data,
    size_t new_size,
    std::function<bool(const uint8_t*, size_t)> finder,
    ErrorContext* ctx);

Status deleteRecordInHeapPage(
    PageID page_id,
    std::function<bool(const uint8_t*, size_t)> finder,
    ErrorContext* ctx);

template<typename RecordType>
Status scanHeapPageWithFilter(
    PageID page_id,
    std::function<bool(const uint8_t*, size_t)> filter,
    std::function<std::optional<RecordType>(const uint8_t*, size_t)> converter,
    std::vector<RecordType>& records_out,
    ErrorContext* ctx);
```

---

### Phase B.2: Multi-Page Version Chain Support (5-7 hours)

**File:** `src/core/heap_page.cpp:1464-1468`

**Current Issue:**
```cpp
// For production use, implement recursive helper function
SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                  "Multi-page version chains (3+ pages) not yet supported");
return Status::NOT_IMPLEMENTED;
```

**Implementation:** Add recursive version chain traversal for Firebird MGA compliance

```cpp
// In heap_page.cpp - Add helper function
Status HeapPage::traverseVersionChain(
    PageID current_page_id,
    const TID& target_tid,
    uint64_t snapshot_xid,
    std::optional<std::vector<uint8_t>>& value_out,
    int depth,
    ErrorContext* ctx)
{
    // Prevent infinite recursion
    constexpr int MAX_CHAIN_DEPTH = 100;
    if (depth > MAX_CHAIN_DEPTH) {
        SET_ERROR_CONTEXT(ctx, Status::CORRUPTION,
                          "Version chain exceeds maximum depth");
        return Status::CORRUPTION;
    }

    // Read current page
    uint8_t page_data[PAGE_SIZE];
    Status status = page_manager_->readPage(current_page_id, page_data, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Find tuple in this page
    auto header = reinterpret_cast<HeapPageHeader*>(page_data);
    auto line_pointers = reinterpret_cast<LinePointer*>(
        page_data + sizeof(HeapPageHeader));

    uint16_t slot = target_tid.getSlot();
    if (slot >= header->num_tuples) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Invalid slot in version chain");
        return Status::NOT_FOUND;
    }

    LinePointer& lp = line_pointers[slot];

    // Check visibility
    const uint8_t* tuple_data = page_data + lp.offset;
    auto tuple_header = reinterpret_cast<const TupleHeader*>(tuple_data);

    if (isTupleVisible(tuple_header, snapshot_xid)) {
        // Found visible version
        size_t data_size = lp.length - sizeof(TupleHeader);
        value_out = std::vector<uint8_t>(
            tuple_data + sizeof(TupleHeader),
            tuple_data + sizeof(TupleHeader) + data_size);
        return Status::OK;
    }

    // Check for older version pointer
    if (tuple_header->prev_version_tid != INVALID_TID) {
        TID prev_tid = tuple_header->prev_version_tid;
        PageID prev_page_id = prev_tid.getPageID();

        // Recursive call to follow chain
        return traverseVersionChain(prev_page_id, prev_tid, snapshot_xid,
                                    value_out, depth + 1, ctx);
    }

    // No visible version found
    value_out = std::nullopt;
    return Status::NOT_FOUND;
}
```

**Testing:**
```cpp
TEST_F(HeapPageTest, MultiPageVersionChain) {
    // Create initial version
    std::vector<uint8_t> v1 = {1, 2, 3};
    TID tid1;
    ASSERT_OK(heap_page_->insertTuple(v1.data(), v1.size(), tid1, &ctx_));

    // Update 10 times to create long chain spanning multiple pages
    TID current_tid = tid1;
    for (int i = 2; i <= 10; ++i) {
        std::vector<uint8_t> v_new = {static_cast<uint8_t>(i), 2, 3};
        TID new_tid;
        ASSERT_OK(heap_page_->updateTuple(current_tid, v_new.data(),
                                          v_new.size(), new_tid, &ctx_));
        current_tid = new_tid;
    }

    // Read with old snapshot - should find version 1
    std::optional<std::vector<uint8_t>> result;
    ASSERT_OK(heap_page_->readTuple(tid1, 1, result, &ctx_));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)[0], 1);

    // Read with new snapshot - should find version 10
    ASSERT_OK(heap_page_->readTuple(current_tid, 100, result, &ctx_));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ((*result)[0], 10);
}
```

---

## Agent C: Statistics & Analysis

### Phase C.1: Statistics Manager Implementation (10-15 hours)

**File:** `src/optimizer/statistics_manager.cpp`

#### C.1.1: Implement `analyzeColumn` (4-6 hours)

**Current State:** Line 227-232 - NOT_IMPLEMENTED

**Implementation:**
```cpp
auto StatisticsManager::analyzeColumn(const ID& table_id, uint16_t column_index,
                                      ErrorContext* ctx) -> Status
{
    DEBUG_LOG_DB("Analyzing column " << column_index << " of table " << table_id.toString());

    // Step 1: Get table metadata
    TableInfo table_info;
    Status status = catalog_mgr_->getTable(table_id, table_info, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 2: Get column info
    if (column_index >= table_info.columns.size()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Column index out of range");
        return Status::INVALID_ARGUMENT;
    }
    const auto& column = table_info.columns[column_index];

    // Step 3: Scan table and collect statistics
    ColumnStatistics stats;
    stats.table_id = table_id;
    stats.column_index = column_index;
    stats.null_count = 0;
    stats.distinct_count = 0;
    stats.total_count = 0;

    // Use histogram for value distribution
    constexpr size_t HISTOGRAM_BUCKETS = 100;
    std::map<std::string, size_t> value_counts;  // For distinct counting

    // Scan table via storage engine
    auto scan_callback = [&](const std::vector<TypedValue>& row) -> bool {
        stats.total_count++;

        const TypedValue& value = row[column_index];
        if (value.isNull()) {
            stats.null_count++;
            return true;  // Continue scanning
        }

        // Track distinct values (sample if too many)
        std::string value_str = value.toString();
        value_counts[value_str]++;

        // Update min/max
        if (!stats.min_value.has_value() || value < stats.min_value.value()) {
            stats.min_value = value;
        }
        if (!stats.max_value.has_value() || value > stats.max_value.value()) {
            stats.max_value = value;
        }

        return true;  // Continue
    };

    status = storage_engine_->scanTable(table_id, scan_callback, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 4: Calculate distinct count
    stats.distinct_count = value_counts.size();

    // Step 5: Build histogram (Most Common Values)
    std::vector<std::pair<std::string, size_t>> sorted_values(
        value_counts.begin(), value_counts.end());
    std::sort(sorted_values.begin(), sorted_values.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    stats.most_common_values.clear();
    for (size_t i = 0; i < std::min(sorted_values.size(), size_t(10)); ++i) {
        stats.most_common_values.push_back(sorted_values[i].first);
        stats.most_common_freqs.push_back(
            static_cast<double>(sorted_values[i].second) / stats.total_count);
    }

    // Step 6: Calculate selectivity
    if (stats.total_count > 0) {
        stats.avg_width = 0;  // TODO: Calculate from actual data sizes
        stats.correlation = 0.0;  // TODO: Requires ordering analysis
    }

    // Step 7: Store statistics in cache
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        column_stats_[table_id][column_index] = stats;
    }

    // Step 8: Persist to catalog (pg_statistic table)
    status = persistColumnStatistics(stats, ctx);
    if (status != Status::OK) {
        LOG_WARNING(OPTIMIZER, "Failed to persist column statistics");
    }

    DEBUG_LOG_DB("Column analysis complete: " << stats.distinct_count
                 << " distinct values, " << stats.null_count << " nulls");
    return Status::OK;
}
```

#### C.1.2: Implement `getTableStatistics` (2-3 hours)

**Current State:** Line 275-280 - NOT_IMPLEMENTED

**Implementation:**
```cpp
auto StatisticsManager::getTableStatistics(const ID& table_id,
                                           TableStatistics& stats_out,
                                           ErrorContext* ctx) -> Status
{
    // Step 1: Check cache
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto it = table_stats_.find(table_id);
        if (it != table_stats_.end()) {
            stats_out = it->second;
            return Status::OK;
        }
    }

    // Step 2: Load from catalog (pg_class)
    TableInfo table_info;
    Status status = catalog_mgr_->getTable(table_id, table_info, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Step 3: Build statistics structure
    stats_out.table_id = table_id;
    stats_out.tuple_count = table_info.tuple_count;  // From catalog
    stats_out.page_count = table_info.num_pages;
    stats_out.avg_tuple_size = (stats_out.tuple_count > 0)
        ? (stats_out.page_count * PAGE_SIZE) / stats_out.tuple_count
        : 0;

    // Step 4: Load column statistics
    stats_out.column_stats.clear();
    for (size_t i = 0; i < table_info.columns.size(); ++i) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        auto col_it = column_stats_[table_id].find(static_cast<uint16_t>(i));
        if (col_it != column_stats_[table_id].end()) {
            stats_out.column_stats.push_back(col_it->second);
        }
    }

    // Step 5: Cache for future use
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        table_stats_[table_id] = stats_out;
    }

    return Status::OK;
}
```

#### C.1.3: Implement `dropStatistics` (2-3 hours)

**Current State:** Line 289-294 - NOT_IMPLEMENTED

**Implementation:**
```cpp
auto StatisticsManager::dropStatistics(const ID& table_id, ErrorContext* ctx) -> Status
{
    DEBUG_LOG_DB("Dropping statistics for table " << table_id.toString());

    // Step 1: Remove from cache
    invalidateCache(table_id);

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        table_stats_.erase(table_id);
        column_stats_.erase(table_id);
    }

    // Step 2: Delete from pg_statistic catalog table
    Status status = deleteStatisticsFromCatalog(table_id, ctx);
    if (status != Status::OK) {
        LOG_WARNING(OPTIMIZER, "Failed to delete statistics from catalog");
        return status;
    }

    DEBUG_LOG_DB("Statistics dropped successfully");
    return Status::OK;
}
```

#### C.1.4: Add Helper Methods

**Add to statistics_manager.cpp:**
```cpp
auto StatisticsManager::persistColumnStatistics(const ColumnStatistics& stats,
                                                ErrorContext* ctx) -> Status
{
    // Prepare catalog record for pg_statistic
    PgStatisticRecord rec;
    rec.table_id = stats.table_id;
    rec.column_index = stats.column_index;
    rec.null_frac = (stats.total_count > 0)
        ? static_cast<double>(stats.null_count) / stats.total_count
        : 0.0;
    rec.avg_width = stats.avg_width;
    rec.n_distinct = static_cast<double>(stats.distinct_count);

    // Store MCVs (Most Common Values) in TOAST if large
    // For now, inline storage for simplicity
    rec.most_common_vals_oid = 0;  // TODO: TOAST integration
    rec.most_common_freqs_oid = 0;

    // Write to pg_statistic page
    return catalog_mgr_->insertStatisticRecord(rec, ctx);
}

auto StatisticsManager::deleteStatisticsFromCatalog(const ID& table_id,
                                                    ErrorContext* ctx) -> Status
{
    // Delete all pg_statistic records for this table
    return catalog_mgr_->deleteStatisticsByTable(table_id, ctx);
}
```

---

## Agent D: Domain & Type Operations

### Phase D.1: RECORD Operations (5-7 hours)

**File:** `src/core/domain_manager.cpp:497-504`

**Status:** Requires TypedValue COMPOSITE support (deferred to Phase 2+)

**Current Implementation:** NOT_IMPLEMENTED with clear documentation

**Decision:** Mark as Phase 2 work, but document the API:

```cpp
/* Phase 2 Enhancement: RECORD Field Extraction
 *
 * Requires:
 *   1. TypedValue extension to hold CompositeValue
 *   2. Binary decoding of COMPOSITE from TypedValue storage
 *   3. Field extraction from decoded CompositeValue
 *
 * Signature:
 *   Status extractField(const TypedValue& record_value,
 *                      const std::string& field_name,
 *                      TypedValue& field_out,
 *                      ErrorContext* ctx);
 *
 * Example Usage:
 *   TypedValue employee_record = ...;  // (name: 'Alice', age: 30)
 *   TypedValue name_field;
 *   ASSERT_OK(domain_mgr_->extractField(employee_record, "name", name_field, &ctx));
 *   EXPECT_EQ(name_field.toString(), "Alice");
 */
SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
    "RECORD field extraction requires TypedValue COMPOSITE support (Phase 2)");
return Status::NOT_IMPLEMENTED;
```

---

### Phase D.2: SET Operations (5-7 hours)

**File:** `src/core/domain_manager.cpp:832-931`

**Status:** Requires TypedValue VECTOR element access (deferred)

**Items:**
1. `setContains` (line 832-839)
2. `setOverlap` (line 855-862)
3. `setUnion` (line 878-885)
4. `setIntersection` (line 901-908)
5. `setDifference` (line 924-931)

**Decision:** Phase 2 work, document APIs clearly

---

### Phase D.3: VARIANT Operations (3-5 hours)

**File:** `src/core/domain_manager.cpp:1012-1051`

**Status:** Requires TypedValue VARIANT support (future)

**Items:**
1. `variantGetType` (line 1012-1019)
2. `variantIs` (line 1027-1034)
3. `variantCast` (line 1044-1051)

**Decision:** Post-Alpha enhancement

---

## Testing Strategy

### Unit Tests (per agent)

**Agent A Tests:** `tests/unit/test_catalog_crud.cpp`
- Test all timezone CRUD operations
- Test all charset CRUD operations
- Test all collation CRUD operations with dependencies
- Test error cases (NOT_FOUND, DEPENDENCY_ERROR)

**Agent B Tests:** `tests/unit/test_heap_helpers.cpp`
- Test findRecordInHeapPage with various record types
- Test updateRecordInHeapPage in-place and expansion cases
- Test deleteRecordInHeapPage and tombstone marking
- Test scanHeapPageWithFilter with different filters
- Test multi-page version chain traversal (3, 5, 10 versions)

**Agent C Tests:** `tests/unit/test_statistics.cpp`
- Test analyzeColumn with various data types
- Test histogram generation and MCV calculation
- Test getTableStatistics cache and persistence
- Test dropStatistics cleanup

**Agent D Tests:** Deferred to Phase 2

### Integration Tests

```cpp
TEST_F(IntegrationTest, FullCatalogWorkflow) {
    // Create charset
    CharsetInfo cs;
    cs.charset_name = "UTF-8";
    uint16_t cs_id;
    ASSERT_OK(catalog_mgr_->createCharset(cs, cs_id, &ctx_));

    // Create collation
    CollationCatalogInfo coll;
    coll.collation_name = "utf8_general_ci";
    coll.charset_id = cs_id;
    uint32_t coll_id;
    ASSERT_OK(catalog_mgr_->createCollation(coll, coll_id, &ctx_));

    // Update charset
    cs.max_bytes_per_char = 4;
    ASSERT_OK(catalog_mgr_->updateCharset(cs_id, cs, &ctx_));

    // List collations for charset
    std::vector<CollationCatalogInfo> collations;
    ASSERT_OK(catalog_mgr_->listCollationsForCharset(cs_id, collations, &ctx_));
    EXPECT_EQ(collations.size(), 1);

    // Delete collation
    ASSERT_OK(catalog_mgr_->deleteCollation(coll_id, &ctx_));

    // Delete charset
    ASSERT_OK(catalog_mgr_->deleteCharset(cs_id, &ctx_));
}
```

---

## Completion Criteria

### Agent A: Complete
- ✅ updateTimezone implemented and tested
- ✅ updateCharset implemented and tested
- ✅ deleteCharset implemented with dependency checking
- ✅ listCollationsForCharset implemented
- ✅ deleteCollation implemented
- ✅ All unit tests passing (15+ tests)

### Agent B: Complete
- ✅ findRecordInHeapPage template implemented
- ✅ updateRecordInHeapPage implemented
- ✅ deleteRecordInHeapPage implemented (Firebird MGA tombstones)
- ✅ scanHeapPageWithFilter template implemented
- ✅ Multi-page version chain traversal implemented
- ✅ All unit tests passing (20+ tests)

### Agent C: Complete
- ✅ analyzeColumn fully implemented with histogram
- ✅ getTableStatistics implemented with caching
- ✅ dropStatistics implemented with catalog cleanup
- ✅ Helper methods for persistence added
- ✅ All unit tests passing (10+ tests)

### Agent D: Complete
- ✅ All 9 NOT_IMPLEMENTED items documented as Phase 2 work
- ✅ API signatures and usage examples added
- ✅ Dependencies clearly stated (TypedValue extensions)

---

## Dependencies

### Agent Dependencies
- **Agent A** depends on **Agent B** (helpers must exist first)
- **Agent C** is independent (can run in parallel)
- **Agent D** is documentation-only (no code changes)

### External Dependencies
- PageManager (existing, stable)
- Heap page format (existing, stable)
- Firebird MGA transaction visibility (existing)
- Catalog table schemas (existing)

### Phase 2 Dependencies (for deferred work)
- TypedValue extension to support COMPOSITE, VECTOR, VARIANT types
- TOAST integration for large values
- Full column metadata tracking in catalog

---

## Risks and Mitigations

### Risk 1: Helper function template instantiation
**Impact:** Medium
**Mitigation:** Explicitly instantiate templates for all catalog record types in catalog_manager.cpp

### Risk 2: Multi-page version chains performance
**Impact:** Low
**Mitigation:** Add depth limit (100 pages), log warning if chain exceeds 10 versions

### Risk 3: Statistics collection blocking table access
**Impact:** Medium
**Mitigation:** Use snapshot isolation, allow concurrent reads during ANALYZE

### Risk 4: Charset/collation deletion with active sessions
**Impact:** High
**Mitigation:** Add reference counting or session checks before deletion

---

## Post-Implementation Tasks

1. **Performance Testing**
   - Benchmark helper functions with 1M record catalog tables
   - Test version chain traversal with 100+ versions
   - Profile statistics collection on large tables (10M rows)

2. **Documentation Updates**
   - Update IMPROVEMENT_OPPORTUNITIES.md (mark P0-7, P0-8 as complete)
   - Add CRUD API examples to developer guide
   - Document Firebird MGA tombstone behavior

3. **Code Review Checklist**
   - ✅ All error paths have proper ErrorContext
   - ✅ Mutex locking follows existing patterns
   - ✅ Memory safety (no buffer overruns)
   - ✅ Firebird MGA compliance (no MVCC patterns)

---

**Last Updated:** November 23, 2025
**Estimated Completion:** 60-80 hours (4 agents × 15-20 hours each)
**Target Milestone:** Alpha 1 - 100% Complete
