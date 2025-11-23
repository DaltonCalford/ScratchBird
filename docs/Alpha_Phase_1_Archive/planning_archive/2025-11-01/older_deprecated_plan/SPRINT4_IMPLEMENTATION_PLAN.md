# Sprint 4: ONLINE Migration Core Infrastructure - Implementation Plan

**Document Status**: ✅ COMPLETE (Implementation Plan)
**Version**: 1.0
**Date**: October 21, 2025
**Sprint Goal**: Implement core infrastructure for ONLINE tablespace migration
**Estimated Effort**: 30-37 hours (implementation + testing)
**Dependencies**: Sprint 3 architecture design complete

---

## Executive Summary

This document provides a **detailed implementation plan** for Sprint 4, which implements the three core components of ONLINE migration: State Management, Dual-Source Visibility, and Write Routing.

**Recommendation**: Due to the complexity and scope (30-37 hours), Sprint 4 implementation should be executed in a dedicated focused session or across multiple sessions, with careful testing at each stage.

**This Document Provides**:
- File-by-file implementation guidance
- Exact code locations and modifications
- Step-by-step implementation order
- Testing checkpoints
- Integration points with existing code

---

## Part 1: Task 5.4.1 - State Management (8-10 hours)

### Overview

Implement the migration state tracking infrastructure, including catalog schema extensions, state machine, and API functions.

---

### Subtask 5.4.1.1: Catalog Schema Extensions (3-4 hours)

#### Step 1: Add MigrationPhase Enum

**File**: `include/scratchbird/core/catalog_manager.h`

**Location**: After `IndexType` enum (~line 174)

**Code to Add**:
```cpp
// Migration phase enumeration (Phase 5.4.1)
enum class MigrationPhase : uint8_t
{
    MIGRATION_INIT = 0,       // Initializing
    MIGRATION_COPYING = 1,    // Incremental page copy in progress
    MIGRATION_CATCH_UP = 2,   // Re-copying dirty pages
    MIGRATION_SWAP = 3,       // Atomic catalog swap
    MIGRATION_CLEANUP = 4,    // Cleaning up source pages
    MIGRATION_COMPLETE = 5,   // Migration finished
    MIGRATION_FAILED = 6      // Migration failed (needs rollback)
};

// Convert MigrationPhase to string for logging
inline const char* migrationPhaseToString(MigrationPhase phase) {
    switch (phase) {
        case MigrationPhase::MIGRATION_INIT: return "INIT";
        case MigrationPhase::MIGRATION_COPYING: return "COPYING";
        case MigrationPhase::MIGRATION_CATCH_UP: return "CATCH_UP";
        case MigrationPhase::MIGRATION_SWAP: return "SWAP";
        case MigrationPhase::MIGRATION_CLEANUP: return "CLEANUP";
        case MigrationPhase::MIGRATION_COMPLETE: return "COMPLETE";
        case MigrationPhase::MIGRATION_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}
```

#### Step 2: Add TableMigrationState Structure

**File**: `include/scratchbird/core/catalog_manager.h`

**Location**: After `IndexInfo` struct (~line 192)

**Code to Add**:
```cpp
// Table migration state (Phase 5.4.1)
struct TableMigrationState
{
    // Identity
    ID migration_id;              // Unique migration UUID
    ID table_id;                  // Table being migrated

    // Tablespaces
    uint16_t source_tablespace;   // Source tablespace ID
    uint16_t target_tablespace;   // Target tablespace ID

    // State
    MigrationPhase phase;         // Current migration phase
    uint64_t migration_xid;       // XID when migration started

    // Progress
    uint64_t total_pages;         // Total pages to migrate
    uint64_t copied_pages;        // Pages copied so far
    uint64_t dirty_pages;         // Pages modified during copy

    // Timing
    uint64_t start_time;          // Migration start timestamp
    uint64_t phase_start_time;    // Current phase start timestamp
    uint64_t estimated_end_time;  // Estimated completion time

    // Configuration
    uint32_t batch_size;          // Pages per batch (default 100)
    uint32_t yield_interval_ms;   // Yield delay in ms (default 100)

    // Statistics
    uint64_t bytes_copied;        // Total bytes copied
    float pages_per_second;       // Copy throughput

    // Dirty page bitmap (allocated dynamically)
    uint8_t *dirty_page_bitmap;   // 1 bit per page
    uint64_t dirty_bitmap_size;   // Size in bytes

    // TID mapping (for dual-source visibility)
    std::unordered_map<uint64_t, uint64_t> *tid_mapping;  // old_tid → new_tid

    // Bloom filter for migrated TIDs (allocated dynamically)
    void *migrated_tids_bloom;    // BloomFilter* (forward declaration avoidance)
};
```

#### Step 3: Extend TableInfo Structure

**File**: `include/scratchbird/core/catalog_manager.h`

**Location**: Inside `TableInfo` struct (~line 133, after `last_modified_time`)

**Code to Add**:
```cpp
    // ONLINE migration state (Phase 5.4.1)
    bool migration_in_progress = false;  // Is table currently being migrated?
    ID migration_id;                     // UUID of active migration (zero if none)
    uint64_t migration_xid = 0;          // XID when migration started
    uint16_t migration_target_ts = 0;    // Target tablespace ID during migration
    MigrationPhase migration_phase = MigrationPhase::MIGRATION_INIT;
```

#### Step 4: Add Migration API Function Declarations

**File**: `include/scratchbird/core/catalog_manager.h`

**Location**: After `moveTableToTablespace` declaration (~line 410)

**Code to Add**:
```cpp
// ONLINE Migration API (Phase 5.4.1)

/**
 * Start ONLINE table migration
 *
 * @param table_id Table to migrate
 * @param target_tablespace_id Target tablespace
 * @param ctx Error context
 * @return Status::OK if migration started successfully
 */
Status startOnlineMigration(
    const ID &table_id,
    uint16_t target_tablespace_id,
    ErrorContext *ctx = nullptr);

/**
 * Get migration state for a table
 *
 * @param table_id Table ID
 * @param state_out Output migration state
 * @param ctx Error context
 * @return Status::OK if migration state found
 */
Status getMigrationState(
    const ID &table_id,
    TableMigrationState *state_out,
    ErrorContext *ctx = nullptr);

/**
 * Update migration progress
 *
 * @param migration_id Migration UUID
 * @param pages_copied Number of pages copied
 * @param dirty_pages Number of dirty pages
 * @param ctx Error context
 * @return Status::OK if updated successfully
 */
Status updateMigrationProgress(
    const ID &migration_id,
    uint64_t pages_copied,
    uint64_t dirty_pages,
    ErrorContext *ctx = nullptr);

/**
 * Transition migration to new phase
 *
 * @param migration_id Migration UUID
 * @param new_phase New migration phase
 * @param ctx Error context
 * @return Status::OK if transitioned successfully
 */
Status setMigrationPhase(
    const ID &migration_id,
    MigrationPhase new_phase,
    ErrorContext *ctx = nullptr);

/**
 * Abort migration
 *
 * @param migration_id Migration UUID
 * @param reason Reason for abort
 * @param ctx Error context
 * @return Status::OK if aborted successfully
 */
Status abortMigration(
    const ID &migration_id,
    const char *reason,
    ErrorContext *ctx = nullptr);

/**
 * Get all active migrations
 *
 * @return Vector of active migration states
 */
std::vector<TableMigrationState> getActiveMigrations();
```

---

### Subtask 5.4.1.2: In-Memory Migration State Cache (2-3 hours)

#### Step 1: Add Migration State Cache to CatalogManager

**File**: `include/scratchbird/core/catalog_manager.h`

**Location**: In `private:` section of `CatalogManager` class (~line 530)

**Code to Add**:
```cpp
// ONLINE migration state cache (Phase 5.4.1)
std::unordered_map<ID, TableMigrationState> migration_cache_;  // migration_id → state
std::mutex migration_cache_mutex_;  // Protects migration_cache_
```

#### Step 2: Implement State Management Functions

**File**: `src/core/catalog_manager.cpp`

**Location**: End of file (after existing implementations)

**Code to Add** (~400 lines):

```cpp
// ==================================================================
// PHASE 5.4.1: ONLINE Migration State Management
// ==================================================================

Status CatalogManager::startOnlineMigration(
    const ID &table_id,
    uint16_t target_tablespace_id,
    ErrorContext *ctx)
{
    // 1. Get table info
    auto it = table_cache_.find(table_id);
    if (it == table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    TableInfo &table_info = it->second;

    // 2. Check if already migrating
    if (table_info.migration_in_progress) {
        SET_ERROR_CONTEXT(ctx, Status::ALREADY_EXISTS,
                         "Table migration already in progress");
        return Status::ALREADY_EXISTS;
    }

    // 3. Check if already in target tablespace
    if (table_info.tablespace_id == target_tablespace_id) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Table already in target tablespace");
        return Status::INVALID_ARGUMENT;
    }

    // 4. Create migration state
    TableMigrationState state;
    state.migration_id = generateUuidV7();  // New migration UUID
    state.table_id = table_id;
    state.source_tablespace = table_info.tablespace_id;
    state.target_tablespace = target_tablespace_id;
    state.phase = MigrationPhase::MIGRATION_INIT;
    state.migration_xid = db_->transaction_manager()->getCurrentXID();
    state.total_pages = calculateTablePages(table_id, ctx);
    state.copied_pages = 0;
    state.dirty_pages = 0;
    state.start_time = std::time(nullptr);
    state.phase_start_time = state.start_time;
    state.estimated_end_time = 0;
    state.batch_size = 100;  // Default
    state.yield_interval_ms = 100;  // Default
    state.bytes_copied = 0;
    state.pages_per_second = 0.0f;

    // 5. Allocate dirty page bitmap (1 bit per page)
    state.dirty_bitmap_size = (state.total_pages + 7) / 8;  // Round up to bytes
    state.dirty_page_bitmap = new uint8_t[state.dirty_bitmap_size]();  // Zero-initialized

    // 6. Allocate TID mapping
    state.tid_mapping = new std::unordered_map<uint64_t, uint64_t>();

    // 7. Allocate bloom filter (1% false positive rate for 1M entries)
    // TODO: Implement bloom filter allocation
    state.migrated_tids_bloom = nullptr;  // Placeholder

    // 8. Update table info
    table_info.migration_in_progress = true;
    table_info.migration_id = state.migration_id;
    table_info.migration_xid = state.migration_xid;
    table_info.migration_target_ts = target_tablespace_id;
    table_info.migration_phase = MigrationPhase::MIGRATION_INIT;

    // 9. Cache migration state
    {
        std::lock_guard<std::mutex> lock(migration_cache_mutex_);
        migration_cache_[state.migration_id] = state;
    }

    // 10. Persist migration state to catalog
    // TODO: Write to pg_table_migrations catalog table

    LOG_INFO(CATALOG, "Started ONLINE migration: table=%s, source_ts=%u, target_ts=%u, total_pages=%lu",
            table_info.table_name.c_str(), state.source_tablespace,
            state.target_tablespace, state.total_pages);

    return Status::OK;
}

Status CatalogManager::getMigrationState(
    const ID &table_id,
    TableMigrationState *state_out,
    ErrorContext *ctx)
{
    if (!state_out) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "state_out is null");
        return Status::INVALID_ARGUMENT;
    }

    // Get table info
    auto it = table_cache_.find(table_id);
    if (it == table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    const TableInfo &table_info = it->second;

    if (!table_info.migration_in_progress) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No migration in progress");
        return Status::NOT_FOUND;
    }

    // Get migration state from cache
    std::lock_guard<std::mutex> lock(migration_cache_mutex_);
    auto state_it = migration_cache_.find(table_info.migration_id);

    if (state_it == migration_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration state not found in cache");
        return Status::NOT_FOUND;
    }

    *state_out = state_it->second;
    return Status::OK;
}

Status CatalogManager::updateMigrationProgress(
    const ID &migration_id,
    uint64_t pages_copied,
    uint64_t dirty_pages,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(migration_cache_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Update progress
    if (pages_copied != UINT64_MAX) {  // Special value means "don't update"
        state.copied_pages = pages_copied;
    }

    if (dirty_pages != UINT64_MAX) {
        state.dirty_pages = dirty_pages;
    }

    // Update throughput
    time_t elapsed = std::time(nullptr) - state.start_time;
    if (elapsed > 0) {
        state.pages_per_second = static_cast<float>(state.copied_pages) / elapsed;

        // Estimate completion time
        uint64_t pages_remaining = state.total_pages - state.copied_pages;
        if (state.pages_per_second > 0) {
            uint64_t seconds_remaining = pages_remaining / state.pages_per_second;
            state.estimated_end_time = state.start_time + elapsed + seconds_remaining;
        }
    }

    state.bytes_copied = state.copied_pages * db_->page_size();

    // TODO: Persist to catalog

    return Status::OK;
}

Status CatalogManager::setMigrationPhase(
    const ID &migration_id,
    MigrationPhase new_phase,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(migration_cache_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    MigrationPhase old_phase = state.phase;
    state.phase = new_phase;
    state.phase_start_time = std::time(nullptr);

    LOG_INFO(CATALOG, "Migration phase transition: %s → %s (migration_id=%s)",
            migrationPhaseToString(old_phase),
            migrationPhaseToString(new_phase),
            uuidToString(migration_id).c_str());

    // Update table info cache
    auto table_it = table_cache_.find(state.table_id);
    if (table_it != table_cache_.end()) {
        table_it->second.migration_phase = new_phase;
    }

    // TODO: Persist to catalog

    return Status::OK;
}

Status CatalogManager::abortMigration(
    const ID &migration_id,
    const char *reason,
    ErrorContext *ctx)
{
    LOG_WARNING(CATALOG, "Aborting migration: %s (reason: %s)",
               uuidToString(migration_id).c_str(), reason);

    std::lock_guard<std::mutex> lock(migration_cache_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Mark as failed
    state.phase = MigrationPhase::MIGRATION_FAILED;

    // Update table info
    auto table_it = table_cache_.find(state.table_id);
    if (table_it != table_cache_.end()) {
        TableInfo &table_info = table_it->second;
        table_info.migration_in_progress = false;
        table_info.migration_phase = MigrationPhase::MIGRATION_FAILED;
        // Note: migration_id and migration_xid preserved for rollback
    }

    // TODO: Trigger rollback
    // TODO: Deallocate target pages
    // TODO: Free migration state resources

    LOG_INFO(CATALOG, "Migration aborted: %s", uuidToString(migration_id).c_str());

    return Status::OK;
}

std::vector<TableMigrationState> CatalogManager::getActiveMigrations()
{
    std::lock_guard<std::mutex> lock(migration_cache_mutex_);

    std::vector<TableMigrationState> active;
    for (const auto &[migration_id, state] : migration_cache_) {
        if (state.phase < MigrationPhase::MIGRATION_COMPLETE) {
            active.push_back(state);
        }
    }

    return active;
}

// Helper: Calculate total pages for a table
uint64_t CatalogManager::calculateTablePages(const ID &table_id, ErrorContext *ctx)
{
    auto it = table_cache_.find(table_id);
    if (it == table_cache_.end()) {
        return 0;
    }

    const TableInfo &table_info = it->second;

    // Estimate: row_count * avg_row_size / page_size
    // For now, use a simple heuristic
    uint64_t estimated_pages = (table_info.row_count * 100) / db_->page_size();

    if (estimated_pages == 0 && table_info.row_count > 0) {
        estimated_pages = 1;  // At least one page
    }

    return estimated_pages;
}
```

---

### Subtask 5.4.1.3: Migration Phase State Machine (2-3 hours)

**Implementation Note**: The state machine is already partially implemented in the functions above. The key transitions are:

```
INIT → COPYING   (startOnlineMigration → background thread)
COPYING → CATCH_UP   (all pages copied)
CATCH_UP → SWAP      (convergence achieved)
SWAP → CLEANUP       (catalog updated)
CLEANUP → COMPLETE   (source pages deallocated)
Any → FAILED        (error occurred)
```

**Testing Checkpoint**:
- [ ] Test startOnlineMigration()
- [ ] Test getMigrationState()
- [ ] Test updateMigrationProgress()
- [ ] Test setMigrationPhase()
- [ ] Test abortMigration()
- [ ] Verify state transitions logged correctly

---

## Part 2: Task 5.4.2 - Dual-Source Visibility (12-15 hours)

### Overview

Implement TID resolution service with bloom filter, modify heap fetch to support dual-source reads, and add query-level caching.

**Note**: This is the most complex part of Sprint 4, touching core query paths.

---

### Subtask 5.4.2.1: TID Resolver Service (5-6 hours)

#### Step 1: Create TID Resolver Header

**File**: `include/scratchbird/core/tid_resolver.h` (NEW FILE)

**Contents** (~200 lines):

```cpp
#pragma once

#include <cstdint>
#include <unordered_map>
#include <mutex>
#include "scratchbird/core/status.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{

// Forward declarations
class Database;
class CatalogManager;
struct TableInfo;

/**
 * Bloom Filter for TID Migration Tracking
 *
 * Simple bloom filter implementation for fast "has TID been migrated?" checks.
 * Uses 1% false positive rate for typical table sizes.
 */
class BloomFilter
{
public:
    BloomFilter(uint64_t expected_items, float false_positive_rate = 0.01f);
    ~BloomFilter();

    // Insert TID into filter
    void insert(uint64_t tid);

    // Check if TID might be in filter (may have false positives)
    bool contains(uint64_t tid) const;

    // Clear all entries
    void clear();

    // Get statistics
    uint64_t getInsertCount() const { return insert_count_; }
    uint64_t getSize() const { return size_bytes_; }

private:
    uint8_t *bits_;              // Bit array
    uint64_t size_bits_;         // Size in bits
    uint64_t size_bytes_;        // Size in bytes
    uint32_t num_hashes_;        // Number of hash functions
    uint64_t insert_count_;      // Number of items inserted

    // Hash functions (using different seeds)
    uint64_t hash1(uint64_t tid) const;
    uint64_t hash2(uint64_t tid) const;
    uint64_t hash3(uint64_t tid) const;
};

/**
 * TID Resolver Cache (Per-Query)
 *
 * Caches TID→Tablespace resolutions for a single query to avoid
 * repeated bloom filter lookups.
 */
class QueryTIDCache
{
public:
    QueryTIDCache() = default;

    // Resolve TID to tablespace (with caching)
    uint16_t resolve(const TID &tid, const TableInfo &table_info,
                    BloomFilter *bloom, const std::unordered_map<uint64_t, uint64_t> &tid_mapping);

    // Clear cache
    void clear();

    // Get statistics
    uint64_t getCacheHits() const { return cache_hits_; }
    uint64_t getCacheMisses() const { return cache_misses_; }

private:
    std::unordered_map<uint64_t, uint16_t> resolved_tids_;  // TID → tablespace
    uint64_t cache_hits_ = 0;
    uint64_t cache_misses_ = 0;
};

/**
 * TID Resolution Service
 *
 * Determines which tablespace a TID should be fetched from during ONLINE migration.
 */
class TIDResolver
{
public:
    explicit TIDResolver(Database *db);
    ~TIDResolver();

    /**
     * Resolve TID to tablespace
     *
     * Fast path: If table not migrating, return table.tablespace_id
     * Slow path: Check bloom filter + exact mapping
     *
     * @param tid TID to resolve
     * @param table_info Table metadata (includes migration state)
     * @param ctx Error context
     * @return Tablespace ID to fetch from
     */
    uint16_t resolveTablespace(
        const TID &tid,
        const TableInfo &table_info,
        ErrorContext *ctx = nullptr);

    /**
     * Invalidate cache for a table (after migration completes)
     *
     * @param table_id Table ID
     */
    void invalidateCache(const ID &table_id);

    // Statistics
    uint64_t getBloomHits() const { return bloom_hits_; }
    uint64_t getBloomFalsePositives() const { return bloom_false_positives_; }
    uint64_t getCacheHits() const { return cache_hits_; }

private:
    Database *db_;
    CatalogManager *catalog_mgr_;

    // Statistics
    uint64_t bloom_hits_ = 0;
    uint64_t bloom_false_positives_ = 0;
    uint64_t cache_hits_ = 0;
    uint64_t cache_misses_ = 0;
};

} // namespace scratchbird::core
```

#### Step 2: Implement TID Resolver

**File**: `src/core/tid_resolver.cpp` (NEW FILE)

**Contents** (~400 lines):

```cpp
#include "scratchbird/core/tid_resolver.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/logger.h"
#include <cmath>
#include <cstring>

namespace scratchbird::core
{

// ==================================================================
// Bloom Filter Implementation
// ==================================================================

BloomFilter::BloomFilter(uint64_t expected_items, float false_positive_rate)
{
    // Calculate optimal size: m = -(n * ln(p)) / (ln(2)^2)
    double m = -(expected_items * std::log(false_positive_rate)) / (std::log(2) * std::log(2));
    size_bits_ = static_cast<uint64_t>(std::ceil(m));
    size_bytes_ = (size_bits_ + 7) / 8;

    // Calculate optimal number of hashes: k = (m / n) * ln(2)
    num_hashes_ = static_cast<uint32_t>(std::ceil((size_bits_ / (double)expected_items) * std::log(2)));

    // Allocate bit array
    bits_ = new uint8_t[size_bytes_]();  // Zero-initialized

    insert_count_ = 0;

    LOG_INFO(STORAGE, "Created bloom filter: size=%lu bytes, hashes=%u, expected=%lu items",
            size_bytes_, num_hashes_, expected_items);
}

BloomFilter::~BloomFilter()
{
    delete[] bits_;
}

void BloomFilter::insert(uint64_t tid)
{
    uint64_t h1 = hash1(tid);
    uint64_t h2 = hash2(tid);
    uint64_t h3 = hash3(tid);

    // Set bits for all hash functions
    uint64_t bit_index1 = h1 % size_bits_;
    uint64_t bit_index2 = h2 % size_bits_;
    uint64_t bit_index3 = h3 % size_bits_;

    uint64_t byte_index1 = bit_index1 / 8;
    uint64_t byte_index2 = bit_index2 / 8;
    uint64_t byte_index3 = bit_index3 / 8;

    uint8_t bit_offset1 = bit_index1 % 8;
    uint8_t bit_offset2 = bit_index2 % 8;
    uint8_t bit_offset3 = bit_index3 % 8;

    bits_[byte_index1] |= (1 << bit_offset1);
    bits_[byte_index2] |= (1 << bit_offset2);
    bits_[byte_index3] |= (1 << bit_offset3);

    insert_count_++;
}

bool BloomFilter::contains(uint64_t tid) const
{
    uint64_t h1 = hash1(tid);
    uint64_t h2 = hash2(tid);
    uint64_t h3 = hash3(tid);

    uint64_t bit_index1 = h1 % size_bits_;
    uint64_t bit_index2 = h2 % size_bits_;
    uint64_t bit_index3 = h3 % size_bits_;

    uint64_t byte_index1 = bit_index1 / 8;
    uint64_t byte_index2 = bit_index2 / 8;
    uint64_t byte_index3 = bit_index3 / 8;

    uint8_t bit_offset1 = bit_index1 % 8;
    uint8_t bit_offset2 = bit_index2 % 8;
    uint8_t bit_offset3 = bit_index3 % 8;

    bool bit1_set = (bits_[byte_index1] & (1 << bit_offset1)) != 0;
    bool bit2_set = (bits_[byte_index2] & (1 << bit_offset2)) != 0;
    bool bit3_set = (bits_[byte_index3] & (1 << bit_offset3)) != 0;

    return bit1_set && bit2_set && bit3_set;
}

void BloomFilter::clear()
{
    std::memset(bits_, 0, size_bytes_);
    insert_count_ = 0;
}

uint64_t BloomFilter::hash1(uint64_t tid) const
{
    // MurmurHash3-inspired hash
    uint64_t h = tid;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

uint64_t BloomFilter::hash2(uint64_t tid) const
{
    // Different seed
    return hash1(tid * 0x9e3779b97f4a7c15ULL);
}

uint64_t BloomFilter::hash3(uint64_t tid) const
{
    // Another different seed
    return hash1(tid * 0x517cc1b727220a95ULL);
}

// ==================================================================
// Query TID Cache Implementation
// ==================================================================

uint16_t QueryTIDCache::resolve(
    const TID &tid,
    const TableInfo &table_info,
    BloomFilter *bloom,
    const std::unordered_map<uint64_t, uint64_t> &tid_mapping)
{
    uint64_t legacy_tid = convertTIDtoLegacy(tid);

    // Check cache first
    auto it = resolved_tids_.find(legacy_tid);
    if (it != resolved_tids_.end()) {
        cache_hits_++;
        return it->second;  // Cache hit
    }

    cache_misses_++;

    // Miss: resolve via bloom filter
    uint16_t tablespace;

    if (!table_info.migration_in_progress) {
        // Fast path: no migration
        tablespace = table_info.tablespace_id;
    } else {
        // Check bloom filter
        if (!bloom || !bloom->contains(legacy_tid)) {
            // Definitely NOT migrated → source tablespace
            tablespace = table_info.tablespace_id;
        } else {
            // Bloom says "probably migrated" → check exact mapping
            auto mapping_it = tid_mapping.find(legacy_tid);
            if (mapping_it != tid_mapping.end()) {
                // Confirmed migrated → target tablespace
                tablespace = table_info.migration_target_ts;
            } else {
                // Bloom false positive → actually not migrated
                tablespace = table_info.tablespace_id;
            }
        }
    }

    // Cache result
    resolved_tids_[legacy_tid] = tablespace;

    return tablespace;
}

void QueryTIDCache::clear()
{
    resolved_tids_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
}

// ==================================================================
// TID Resolver Implementation
// ==================================================================

TIDResolver::TIDResolver(Database *db)
    : db_(db), catalog_mgr_(db->catalog_manager())
{
}

TIDResolver::~TIDResolver()
{
}

uint16_t TIDResolver::resolveTablespace(
    const TID &tid,
    const TableInfo &table_info,
    ErrorContext *ctx)
{
    // Fast path: no migration
    if (!table_info.migration_in_progress) {
        return table_info.tablespace_id;
    }

    // Get migration state
    TableMigrationState migration_state;
    Status status = catalog_mgr_->getMigrationState(table_info.table_id, &migration_state, ctx);

    if (status != Status::OK) {
        // Migration state not found, fall back to source tablespace
        LOG_WARNING(STORAGE, "Migration state not found for table, using source tablespace");
        return table_info.tablespace_id;
    }

    uint64_t legacy_tid = convertTIDtoLegacy(tid);

    // Check bloom filter
    BloomFilter *bloom = static_cast<BloomFilter*>(migration_state.migrated_tids_bloom);

    if (!bloom || !bloom->contains(legacy_tid)) {
        // Definitely NOT migrated → source tablespace
        return table_info.tablespace_id;
    }

    bloom_hits_++;

    // Bloom says "probably migrated" → check exact mapping
    auto it = migration_state.tid_mapping->find(legacy_tid);
    if (it != migration_state.tid_mapping->end()) {
        // Confirmed migrated → target tablespace
        cache_hits_++;
        return table_info.migration_target_ts;
    }

    // Bloom false positive → actually not migrated
    bloom_false_positives_++;
    return table_info.tablespace_id;
}

void TIDResolver::invalidateCache(const ID &table_id)
{
    // TODO: Implement per-table cache invalidation
    LOG_DEBUG(STORAGE, "TID resolver cache invalidated for table %s",
             uuidToString(table_id).c_str());
}

} // namespace scratchbird::core
```

---

### Subtask 5.4.2.2: Integrate TID Resolver with Heap Fetch (4-5 hours)

**This is the CRITICAL part**: Modifying the core heap fetch path.

#### Step 1: Add TID Resolver to Database

**File**: `include/scratchbird/core/database.h`

**Location**: In `class Database` private members

**Code to Add**:
```cpp
#include "scratchbird/core/tid_resolver.h"

// ... inside class Database:

public:
    TIDResolver* tid_resolver() { return tid_resolver_.get(); }

private:
    std::unique_ptr<TIDResolver> tid_resolver_;
```

**File**: `src/core/database.cpp`

**Location**: In `Database::Database()` constructor

**Code to Add**:
```cpp
tid_resolver_ = std::make_unique<TIDResolver>(this);
```

#### Step 2: Modify Heap Fetch to Use TID Resolver

**File**: `src/core/heap_page.cpp`

**Location**: In `getTuple()` function (find existing implementation)

**Modification**: Add TID resolution before fetching page

**Before** (pseudocode):
```cpp
Status HeapPage::getTuple(const TID &tid, ...) {
    uint32_t page_number = extractPageNumber(tid);
    void *page_buffer = buffer_pool->pinPage(tablespace, page_number, ...);
    // ...
}
```

**After**:
```cpp
Status HeapPage::getTuple(const TID &tid, ...) {
    // NEW: Resolve tablespace based on migration state
    TableInfo table_info = catalog_mgr->getTableInfo(relation_id);
    uint16_t tablespace = db->tid_resolver()->resolveTablespace(tid, table_info, ctx);

    uint32_t page_number = extractPageNumber(tid);
    void *page_buffer = buffer_pool->pinPage(tablespace, page_number, ctx);
    // ... rest unchanged
}
```

**Testing Checkpoint**:
- [ ] Test TID resolution fast path (no migration)
- [ ] Test TID resolution during migration (source TIDs)
- [ ] Test TID resolution during migration (migrated TIDs)
- [ ] Benchmark query performance overhead (target: < 5%)

---

## Part 3: Task 5.4.3 - Write Routing (10-12 hours)

### Overview

Modify INSERT/UPDATE/DELETE paths to route writes to the correct tablespace during migration.

---

### Subtask 5.4.3.1: INSERT Routing (3-4 hours)

**File**: `src/core/storage_engine.cpp`

**Location**: In `insertTuple()` function

**Modification**:

```cpp
Status StorageEngine::insertTuple(
    const Relation &relation,
    const Tuple &tuple,
    TID *tid_out,
    ErrorContext *ctx)
{
    // 1. Get table info (includes migration state)
    TableInfo table_info = catalog_mgr_->getTableInfo(relation.rel_id, ctx);

    // 2. Determine target tablespace
    uint16_t target_tablespace;

    if (!table_info.migration_in_progress) {
        // Normal case: use table's current tablespace
        target_tablespace = table_info.tablespace_id;
    } else {
        // During migration: route to target tablespace
        target_tablespace = table_info.migration_target_ts;

        LOG_DEBUG(STORAGE, "INSERT routed to target tablespace %u during migration",
                 target_tablespace);
    }

    // 3. Rest of insertion logic unchanged, but use target_tablespace
    // ... existing code ...
}
```

---

### Subtask 5.4.3.2: UPDATE Routing (4-5 hours)

**File**: `src/core/storage_engine.cpp`

**Location**: In `updateTuple()` function

**Critical**: Preserve MGA in-place update, create back version in SAME tablespace.

**Modification** (simplified):

```cpp
Status StorageEngine::updateTuple(
    const TID &old_tid,
    const Tuple &new_tuple,
    TID *new_tid_out,
    ErrorContext *ctx)
{
    // 1. Fetch old tuple
    Tuple old_tuple;
    getTuple(old_tid, &old_tuple, ctx);

    // 2. Get tablespace where old tuple resides (use TID resolver)
    TableInfo table_info = catalog_mgr_->getTableInfo(relation_id, ctx);
    uint16_t old_tablespace = db_->tid_resolver()->resolveTablespace(old_tid, table_info, ctx);

    // 3. Check if in-place update possible
    if (canUpdateInPlace(old_tid, new_tuple.len)) {
        return updateInPlace(old_tid, new_tuple, new_tid_out, ctx);
    }

    // 4. Cross-page update: create back version in SAME tablespace as old tuple
    // Allocate new page in SAME tablespace for back version
    uint32_t back_page;
    page_manager_->allocatePage(old_tablespace, &back_page, ctx);

    // ... rest of MGA cross-page UPDATE logic ...
    // (This should already exist from Sprint 0 bug fix)

    // TID UNCHANGED (MGA principle)
    *new_tid_out = old_tid;

    return Status::OK;
}
```

---

### Subtask 5.4.3.3: DELETE Routing (2-3 hours)

**File**: `src/core/storage_engine.cpp`

**Location**: In `deleteTuple()` function

**Modification**:

```cpp
Status StorageEngine::deleteTuple(
    const TID &tid,
    ErrorContext *ctx)
{
    // 1. Resolve tablespace (handles dual-source)
    TableInfo table_info = catalog_mgr_->getTableInfo(relation_id, ctx);
    uint16_t tablespace = db_->tid_resolver()->resolveTablespace(tid, table_info, ctx);

    // 2. Pin page in resolved tablespace
    void *page_buffer = buffer_pool_->pinPage(tablespace, extractPageNumber(tid), ctx);

    // 3. Mark tuple as deleted (set xmax)
    HeapPage heap_page(page_buffer, db_->page_size());
    heap_page.deleteTuple(extractItemID(tid), current_xid, ctx);

    // 4. If migrating, mark page as dirty
    if (table_info.migration_in_progress) {
        markPageDirty(table_info.migration_id, extractPageNumber(tid), ctx);
    }

    // 5. Unpin and mark dirty
    buffer_pool_->unpinPage(extractPageNumber(tid), true, ctx);

    return Status::OK;
}
```

---

### Subtask 5.4.3.4: Dirty Page Tracking (3-4 hours)

**File**: `src/core/catalog_manager.cpp`

**Add Helper Function**:

```cpp
void CatalogManager::markPageDirty(
    const ID &migration_id,
    uint32_t page_number,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(migration_cache_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        return;  // Migration not found, ignore
    }

    TableMigrationState &state = it->second;

    // Set bit in dirty page bitmap
    uint64_t byte_offset = page_number / 8;
    uint8_t bit_offset = page_number % 8;

    if (byte_offset < state.dirty_bitmap_size) {
        state.dirty_page_bitmap[byte_offset] |= (1 << bit_offset);
        state.dirty_pages++;
    }
}
```

---

## Testing Strategy

### Unit Tests

1. **State Management Tests**:
   - [ ] Test startOnlineMigration()
   - [ ] Test getMigrationState()
   - [ ] Test updateMigrationProgress()
   - [ ] Test phase transitions

2. **TID Resolver Tests**:
   - [ ] Test bloom filter accuracy (false positive rate < 2%)
   - [ ] Test TID resolution (source vs target)
   - [ ] Test cache hit rates

3. **Write Routing Tests**:
   - [ ] Test INSERT routing to target tablespace
   - [ ] Test UPDATE routing (same tablespace)
   - [ ] Test DELETE routing
   - [ ] Test dirty page tracking

### Integration Tests

1. **Concurrent Reads**:
   - Start migration
   - Run SELECT queries during COPYING phase
   - Verify correct results

2. **Concurrent Writes**:
   - Start migration
   - Run INSERT/UPDATE/DELETE during COPYING phase
   - Verify writes routed correctly
   - Verify dirty page bitmap updated

3. **Performance Tests**:
   - Measure query latency overhead (target: < 5%)
   - Measure write throughput degradation (target: < 10%)

---

## Summary of Files to Modify/Create

### New Files (6 files):
1. `include/scratchbird/core/tid_resolver.h`
2. `src/core/tid_resolver.cpp`

### Modified Files (5 files):
3. `include/scratchbird/core/catalog_manager.h` - Add MigrationPhase, TableMigrationState, API declarations
4. `src/core/catalog_manager.cpp` - Implement state management API (~400 lines)
5. `include/scratchbird/core/database.h` - Add tid_resolver member
6. `src/core/database.cpp` - Initialize tid_resolver
7. `src/core/storage_engine.cpp` - Modify INSERT/UPDATE/DELETE routing

### Build System:
8. `CMakeLists.txt` - Add tid_resolver.cpp to build

---

## Implementation Effort Breakdown

| Task | Estimated Hours | Files Modified |
|------|----------------|----------------|
| 5.4.1.1: Catalog schema | 3-4 | 2 files (header + impl) |
| 5.4.1.2: State cache | 2-3 | 1 file (impl) |
| 5.4.1.3: State machine | 2-3 | 1 file (impl) |
| 5.4.2.1: TID resolver | 5-6 | 2 files (new) |
| 5.4.2.2: Heap integration | 4-5 | 2 files (heap + database) |
| 5.4.3.1: INSERT routing | 3-4 | 1 file (storage_engine) |
| 5.4.3.2: UPDATE routing | 4-5 | 1 file (storage_engine) |
| 5.4.3.3: DELETE routing | 2-3 | 1 file (storage_engine) |
| 5.4.3.4: Dirty tracking | 3-4 | 1 file (catalog_mgr) |
| **TOTAL** | **28-37 hours** | **~11 files** |

---

## Success Criteria

Sprint 4 is COMPLETE when:
- [ ] Migration state tracking implemented and tested
- [ ] TID resolution service implemented with bloom filter
- [ ] Heap fetch modified to support dual-source reads
- [ ] Write routing (INSERT/UPDATE/DELETE) implemented
- [ ] Dirty page tracking operational
- [ ] All unit tests pass
- [ ] Performance overhead < 10%
- [ ] Code compiles without warnings
- [ ] Documentation updated

---

## Conclusion

This implementation plan provides a **detailed roadmap** for Sprint 4. Due to the scope (30-37 hours), it is recommended to:

1. **Execute in dedicated sessions**: Allocate focused time for implementation
2. **Test incrementally**: After each subtask, run tests to verify correctness
3. **Review carefully**: Core query and write paths are modified, review is critical
4. **Benchmark performance**: Ensure < 10% overhead target is met

**Note**: This plan assumes Sprint 0 (MGA bug fix) was completed correctly. If not, UPDATE routing may need additional work.

---

**Document Version**: 1.0
**Status**: ✅ COMPLETE (Implementation Plan)
**Author**: Claude
**Date**: October 21, 2025
**Next Action**: Execute implementation in focused sessions with testing at each checkpoint
