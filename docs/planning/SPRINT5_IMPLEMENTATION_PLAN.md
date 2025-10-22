# Sprint 5: ONLINE Migration - Copy and Swap - Implementation Plan

**Document Status**: ✅ COMPLETE
**Version**: 1.0
**Date**: October 21, 2025
**Sprint Goal**: Implement incremental page copy, catch-up phase, atomic swap, and cleanup
**Estimated Effort**: 26-33 hours
**Prerequisites**: Sprint 4 (State Management, Dual-Source Visibility, Write Routing) must be complete

---

## Table of Contents

1. [Overview](#overview)
2. [Task 5.4.4: Incremental Page Copy (8-10 hours)](#task-544-incremental-page-copy)
3. [Task 5.4.5: Catch-Up Phase (6-8 hours)](#task-545-catch-up-phase)
4. [Task 5.4.6: Atomic Swap (8-10 hours)](#task-546-atomic-swap)
5. [Task 5.4.7: Cleanup Phase (4-5 hours)](#task-547-cleanup-phase)
6. [Testing Strategy](#testing-strategy)
7. [Integration Points](#integration-points)
8. [Performance Targets](#performance-targets)

---

## Overview

Sprint 5 implements the core migration execution phases:

1. **Incremental Copy**: Background thread copies pages from source to target tablespace
2. **Catch-Up**: Re-copies dirty pages until convergence is achieved
3. **Atomic Swap**: Performs final cutover in < 100ms with exclusive lock
4. **Cleanup**: Deallocates source pages and updates free space tracking

**Architecture Foundation**: Built on Sprint 3 architecture and Sprint 4 infrastructure (state management, TID resolution, write routing).

**Implementation Approach**: Each task builds on the previous one, with clear testing checkpoints.

---

## Task 5.4.4: Incremental Page Copy

**Effort**: 8-10 hours
**Deliverable**: Background migration thread that copies pages incrementally

### Subtask 5.4.4.1: MigrationWorker Class (3-4 hours)

**Create**: `include/scratchbird/core/migration_worker.h`

```cpp
#pragma once

#include "scratchbird/common/types.h"
#include "scratchbird/common/status.h"
#include "scratchbird/common/error.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace scratchbird
{

// Forward declarations
class Database;
class CatalogManager;
class PageManager;
class BufferPool;

/**
 * @brief Background worker thread for ONLINE table migration
 *
 * Responsibilities:
 * - Incrementally copy pages from source to target tablespace
 * - Track dirty pages for catch-up phase
 * - Monitor convergence conditions
 * - Coordinate with atomic swap phase
 */
class MigrationWorker
{
public:
    /**
     * @brief Construct migration worker
     * @param db Database instance
     */
    explicit MigrationWorker(Database *db);

    /**
     * @brief Destructor - ensures worker thread is stopped
     */
    ~MigrationWorker();

    /**
     * @brief Start migration for a table
     * @param migration_id Migration ID from catalog
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status startMigration(const ID &migration_id, ErrorContext *ctx);

    /**
     * @brief Stop migration (abort or after completion)
     * @param migration_id Migration ID
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status stopMigration(const ID &migration_id, ErrorContext *ctx);

    /**
     * @brief Check if migration is active
     */
    bool isActive() const { return active_.load(std::memory_order_acquire); }

    /**
     * @brief Get current migration progress (0.0 to 1.0)
     */
    float getProgress() const;

private:
    /**
     * @brief Main worker thread function
     */
    void workerThreadMain();

    /**
     * @brief Execute copying phase
     * @return Status::OK if phase completes, error otherwise
     */
    Status executeCopyingPhase(ErrorContext *ctx);

    /**
     * @brief Copy a single page from source to target
     * @param table_id Table being migrated
     * @param source_page_num Page number in source tablespace
     * @param target_page_num Allocated page in target tablespace
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status copyPage(
        const ID &table_id,
        uint32_t source_page_num,
        uint32_t target_page_num,
        ErrorContext *ctx);

    /**
     * @brief Copy a batch of pages (100 pages per batch)
     * @param start_page Starting page number
     * @param end_page Ending page number (exclusive)
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status copyPageBatch(
        uint32_t start_page,
        uint32_t end_page,
        ErrorContext *ctx);

    /**
     * @brief Update TID resolver with migrated tuples
     * @param page_num Source page number
     * @param tuples_migrated Number of tuples migrated from this page
     * @param ctx Error context
     */
    Status updateTIDMapping(
        uint32_t page_num,
        uint32_t tuples_migrated,
        ErrorContext *ctx);

    // Database components
    Database *db_;
    CatalogManager *catalog_;
    PageManager *page_manager_;
    BufferPool *buffer_pool_;

    // Worker thread
    std::thread worker_thread_;
    std::atomic<bool> active_{false};
    std::atomic<bool> shutdown_{false};
    std::mutex mutex_;
    std::condition_variable cv_;

    // Current migration
    ID current_migration_id_;
    ID current_table_id_;
    uint16_t source_tablespace_;
    uint16_t target_tablespace_;
    uint32_t total_pages_;
    std::atomic<uint32_t> pages_copied_{0};

    // Performance tuning
    static constexpr uint32_t BATCH_SIZE = 100;          // Pages per batch
    static constexpr uint32_t YIELD_INTERVAL_MS = 100;   // Yield every 100ms
    static constexpr uint32_t PROGRESS_LOG_INTERVAL = 1000; // Log every 1000 pages
};

} // namespace scratchbird
```

**Create**: `src/core/migration_worker.cpp`

```cpp
#include "scratchbird/core/migration_worker.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/tid_resolver.h"
#include "scratchbird/common/logging.h"
#include <chrono>
#include <thread>

namespace scratchbird
{

MigrationWorker::MigrationWorker(Database *db)
    : db_(db),
      catalog_(db->catalog_manager()),
      page_manager_(db->page_manager()),
      buffer_pool_(db->buffer_pool())
{
}

MigrationWorker::~MigrationWorker()
{
    // Ensure worker thread is stopped
    shutdown_.store(true, std::memory_order_release);
    cv_.notify_all();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

Status MigrationWorker::startMigration(const ID &migration_id, ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if already active
    if (active_.load(std::memory_order_acquire)) {
        SET_ERROR_CONTEXT(ctx, Status::BUSY, "Migration worker already active");
        return Status::BUSY;
    }

    // Get migration state from catalog
    auto state = catalog_->getMigrationState(migration_id, ctx);
    if (!state.has_value()) {
        return Status::NOT_FOUND;
    }

    // Initialize worker state
    current_migration_id_ = migration_id;
    current_table_id_ = state->table_id;
    source_tablespace_ = state->source_tablespace;
    target_tablespace_ = state->target_tablespace;
    total_pages_ = state->total_pages;
    pages_copied_.store(0, std::memory_order_release);

    // Start worker thread
    active_.store(true, std::memory_order_release);
    worker_thread_ = std::thread(&MigrationWorker::workerThreadMain, this);

    LOG_INFO("Migration worker started for table {} (migration {})",
             current_table_id_, migration_id);

    return Status::OK;
}

Status MigrationWorker::stopMigration(const ID &migration_id, ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!active_.load(std::memory_order_acquire)) {
        return Status::OK; // Already stopped
    }

    if (current_migration_id_ != migration_id) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Migration ID does not match active migration");
        return Status::INVALID_ARGUMENT;
    }

    // Signal worker to stop
    active_.store(false, std::memory_order_release);
    cv_.notify_all();

    // Wait for worker thread to finish
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    LOG_INFO("Migration worker stopped for migration {}", migration_id);

    return Status::OK;
}

float MigrationWorker::getProgress() const
{
    uint32_t copied = pages_copied_.load(std::memory_order_acquire);
    if (total_pages_ == 0) {
        return 0.0f;
    }
    return static_cast<float>(copied) / static_cast<float>(total_pages_);
}

void MigrationWorker::workerThreadMain()
{
    ErrorContext ctx;

    LOG_INFO("Migration worker thread started");

    // Transition to COPYING phase
    Status status = catalog_->setMigrationPhase(
        current_migration_id_,
        MigrationPhase::MIGRATION_COPYING,
        &ctx);

    if (status != Status::OK) {
        LOG_ERROR("Failed to set migration phase to COPYING: {}", ctx.message);
        active_.store(false, std::memory_order_release);
        return;
    }

    // Execute copying phase
    status = executeCopyingPhase(&ctx);

    if (status != Status::OK) {
        LOG_ERROR("Copying phase failed: {}", ctx.message);

        // Mark migration as failed
        catalog_->setMigrationPhase(
            current_migration_id_,
            MigrationPhase::MIGRATION_FAILED,
            &ctx);

        active_.store(false, std::memory_order_release);
        return;
    }

    // Copying phase complete - transition to CATCH_UP
    status = catalog_->setMigrationPhase(
        current_migration_id_,
        MigrationPhase::MIGRATION_CATCH_UP,
        &ctx);

    if (status != Status::OK) {
        LOG_ERROR("Failed to set migration phase to CATCH_UP: {}", ctx.message);
        active_.store(false, std::memory_order_release);
        return;
    }

    LOG_INFO("Migration worker completed COPYING phase, transitioning to CATCH_UP");

    active_.store(false, std::memory_order_release);
}

Status MigrationWorker::executeCopyingPhase(ErrorContext *ctx)
{
    uint32_t pages_remaining = total_pages_;
    uint32_t current_batch_start = 0;

    auto phase_start = std::chrono::steady_clock::now();

    while (pages_remaining > 0 && active_.load(std::memory_order_acquire)) {
        // Calculate batch size
        uint32_t batch_size = std::min(BATCH_SIZE, pages_remaining);
        uint32_t batch_end = current_batch_start + batch_size;

        // Copy batch
        Status status = copyPageBatch(current_batch_start, batch_end, ctx);
        if (status != Status::OK) {
            return status;
        }

        // Update progress
        current_batch_start = batch_end;
        pages_remaining -= batch_size;

        // Update catalog progress
        uint32_t copied = pages_copied_.load(std::memory_order_acquire);
        catalog_->updateMigrationProgress(
            current_migration_id_,
            copied,
            ctx);

        // Log progress periodically
        if (copied % PROGRESS_LOG_INTERVAL == 0) {
            float progress = getProgress();
            LOG_INFO("Migration progress: {:.1f}% ({} / {} pages)",
                     progress * 100.0f, copied, total_pages_);
        }

        // Yield to other operations
        std::this_thread::sleep_for(
            std::chrono::milliseconds(YIELD_INTERVAL_MS));
    }

    auto phase_end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        phase_end - phase_start).count();

    LOG_INFO("Copying phase complete: {} pages in {} ms ({:.1f} pages/sec)",
             total_pages_,
             duration,
             (total_pages_ * 1000.0f) / duration);

    return Status::OK;
}

Status MigrationWorker::copyPageBatch(
    uint32_t start_page,
    uint32_t end_page,
    ErrorContext *ctx)
{
    for (uint32_t page_num = start_page; page_num < end_page; ++page_num) {
        // Check if worker should stop
        if (!active_.load(std::memory_order_acquire)) {
            return Status::CANCELLED;
        }

        // Allocate target page
        uint32_t target_page_num = page_manager_->allocatePage(
            target_tablespace_,
            ctx);

        if (target_page_num == INVALID_PAGE) {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                             "Failed to allocate target page");
            return Status::IO_ERROR;
        }

        // Copy page
        Status status = copyPage(
            current_table_id_,
            page_num,
            target_page_num,
            ctx);

        if (status != Status::OK) {
            return status;
        }

        // Increment counter
        pages_copied_.fetch_add(1, std::memory_order_release);
    }

    return Status::OK;
}

Status MigrationWorker::copyPage(
    const ID &table_id,
    uint32_t source_page_num,
    uint32_t target_page_num,
    ErrorContext *ctx)
{
    // 1. Pin source page
    GPID source_gpid = makeGPID(source_tablespace_, source_page_num);
    auto source_frame = buffer_pool_->pinPage(source_gpid, ctx);
    if (!source_frame) {
        return Status::IO_ERROR;
    }

    // 2. Pin target page (new, should be empty)
    GPID target_gpid = makeGPID(target_tablespace_, target_page_num);
    auto target_frame = buffer_pool_->pinPage(target_gpid, ctx);
    if (!target_frame) {
        buffer_pool_->unpinPage(source_gpid);
        return Status::IO_ERROR;
    }

    // 3. Get source heap page
    HeapPage source_page(source_frame->data(), PAGE_SIZE);

    // 4. Initialize target heap page
    HeapPage target_page(target_frame->data(), PAGE_SIZE);
    target_page.initPage(target_page_num);

    // 5. Copy all visible tuples
    uint32_t slot_count = source_page.getSlotCount();
    uint32_t tuples_copied = 0;

    for (uint32_t slot = 0; slot < slot_count; ++slot) {
        // Get tuple from source
        auto tuple_data = source_page.getTuple(slot, ctx);
        if (!tuple_data) {
            continue; // Deleted or invalid slot
        }

        // Parse tuple header
        RecordHeaderData rhd;
        std::memcpy(&rhd, tuple_data->data(), sizeof(RecordHeaderData));

        // Copy tuple to target page
        // Note: Preserve original xmin/xmax for visibility
        uint32_t target_slot = target_page.insertTuple(
            tuple_data->data(),
            tuple_data->size(),
            ctx);

        if (target_slot == INVALID_SLOT) {
            LOG_ERROR("Failed to insert tuple into target page {} slot {}",
                     target_page_num, slot);
            buffer_pool_->unpinPage(source_gpid);
            buffer_pool_->unpinPage(target_gpid);
            return Status::IO_ERROR;
        }

        // Build TID for this tuple
        TID source_tid;
        source_tid.page_number = source_page_num;
        source_tid.slot_number = slot;

        TID target_tid;
        target_tid.page_number = target_page_num;
        target_tid.slot_number = target_slot;

        // Update TID resolver
        db_->tid_resolver()->recordMigration(
            table_id,
            source_tid,
            target_tid,
            ctx);

        tuples_copied++;
    }

    // 6. Mark target page as dirty
    target_frame->setDirty(true);

    // 7. Unpin pages
    buffer_pool_->unpinPage(source_gpid);
    buffer_pool_->unpinPage(target_gpid);

    return Status::OK;
}

} // namespace scratchbird
```

**Testing Checkpoint 5.4.4.1**:
- [ ] MigrationWorker can be constructed and destroyed
- [ ] Worker thread starts and stops correctly
- [ ] Progress tracking works (0.0 to 1.0)

### Subtask 5.4.4.2: Integration with Database Class (1-2 hours)

**Modify**: `include/scratchbird/core/database.h`

Add migration worker member:

```cpp
class Database
{
public:
    // ... existing methods ...

    /**
     * @brief Get migration worker
     */
    MigrationWorker *migration_worker() { return migration_worker_.get(); }

private:
    // ... existing members ...

    // Migration worker
    std::unique_ptr<MigrationWorker> migration_worker_;
};
```

**Modify**: `src/core/database.cpp`

Initialize migration worker in constructor:

```cpp
Database::Database(const std::string &data_dir)
    : data_dir_(data_dir),
      // ... other initializations ...
      migration_worker_(std::make_unique<MigrationWorker>(this))
{
    // ... rest of constructor ...
}
```

**Testing Checkpoint 5.4.4.2**:
- [ ] Database initializes migration worker
- [ ] Migration worker accessible via getter

### Subtask 5.4.4.3: Migration Execution API (2-3 hours)

**Modify**: `include/scratchbird/core/catalog_manager.h`

Add migration execution method:

```cpp
/**
 * @brief Execute table migration
 * @param table_id Table to migrate
 * @param target_tablespace_id Target tablespace
 * @param ctx Error context
 * @return Migration ID on success
 */
std::optional<ID> executeMigration(
    const ID &table_id,
    uint16_t target_tablespace_id,
    ErrorContext *ctx);
```

**Modify**: `src/core/catalog_manager.cpp`

```cpp
std::optional<ID> CatalogManager::executeMigration(
    const ID &table_id,
    uint16_t target_tablespace_id,
    ErrorContext *ctx)
{
    // 1. Start migration (creates state)
    Status status = startOnlineMigration(table_id, target_tablespace_id, ctx);
    if (status != Status::OK) {
        return std::nullopt;
    }

    // 2. Get migration ID
    auto it = table_cache_.find(table_id);
    if (it == table_cache_.end()) {
        return std::nullopt;
    }

    ID migration_id = it->second.migration_id;

    // 3. Start migration worker
    status = db_->migration_worker()->startMigration(migration_id, ctx);
    if (status != Status::OK) {
        // Abort migration
        abortMigration(migration_id, ctx);
        return std::nullopt;
    }

    return migration_id;
}
```

**Testing Checkpoint 5.4.4.3**:
- [ ] executeMigration() starts background worker
- [ ] Migration state transitions to COPYING
- [ ] Pages are copied incrementally
- [ ] Progress updates correctly

---

## Task 5.4.5: Catch-Up Phase

**Effort**: 6-8 hours
**Deliverable**: Catch-up phase with convergence detection

### Subtask 5.4.5.1: Dirty Page Tracking (2-3 hours)

**Already implemented in Sprint 4** (Task 5.4.3), but add catch-up specific methods:

**Modify**: `include/scratchbird/core/catalog_manager.h`

```cpp
/**
 * @brief Get dirty pages for catch-up phase
 * @param migration_id Migration ID
 * @param ctx Error context
 * @return Vector of dirty page numbers
 */
std::vector<uint32_t> getDirtyPages(
    const ID &migration_id,
    ErrorContext *ctx);

/**
 * @brief Clear dirty page bitmap
 * @param migration_id Migration ID
 * @param ctx Error context
 */
Status clearDirtyPages(
    const ID &migration_id,
    ErrorContext *ctx);

/**
 * @brief Get dirty page count
 * @param migration_id Migration ID
 * @return Number of dirty pages
 */
uint32_t getDirtyPageCount(const ID &migration_id);
```

**Modify**: `src/core/catalog_manager.cpp`

```cpp
std::vector<uint32_t> CatalogManager::getDirtyPages(
    const ID &migration_id,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(migration_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        return {};
    }

    TableMigrationState &state = it->second;
    std::vector<uint32_t> dirty_pages;

    // Scan dirty page bitmap
    for (uint32_t page = 0; page < state.total_pages; ++page) {
        uint32_t byte_idx = page / 8;
        uint32_t bit_idx = page % 8;

        if (state.dirty_pages_bitmap[byte_idx] & (1u << bit_idx)) {
            dirty_pages.push_back(page);
        }
    }

    return dirty_pages;
}

Status CatalogManager::clearDirtyPages(
    const ID &migration_id,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(migration_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Clear bitmap
    size_t bitmap_bytes = (state.total_pages + 7) / 8;
    std::memset(state.dirty_pages_bitmap.get(), 0, bitmap_bytes);

    return Status::OK;
}

uint32_t CatalogManager::getDirtyPageCount(const ID &migration_id)
{
    std::lock_guard<std::mutex> lock(migration_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        return 0;
    }

    TableMigrationState &state = it->second;
    uint32_t count = 0;

    // Count set bits in bitmap
    size_t bitmap_bytes = (state.total_pages + 7) / 8;
    for (size_t i = 0; i < bitmap_bytes; ++i) {
        uint8_t byte = state.dirty_pages_bitmap[i];
        // Count bits using Brian Kernighan's algorithm
        while (byte) {
            byte &= (byte - 1);
            count++;
        }
    }

    return count;
}
```

**Testing Checkpoint 5.4.5.1**:
- [ ] Dirty pages tracked during COPYING phase
- [ ] getDirtyPages() returns correct pages
- [ ] clearDirtyPages() clears bitmap
- [ ] getDirtyPageCount() accurate

### Subtask 5.4.5.2: Catch-Up Execution (2-3 hours)

**Modify**: `src/core/migration_worker.cpp`

Add catch-up phase execution to worker thread:

```cpp
Status MigrationWorker::executeCatchUpPhase(ErrorContext *ctx)
{
    uint32_t iteration = 0;
    static constexpr uint32_t MAX_ITERATIONS = 100;
    static constexpr float CONVERGENCE_RATIO = 0.5f;

    LOG_INFO("Starting catch-up phase for migration {}", current_migration_id_);

    while (iteration < MAX_ITERATIONS && active_.load(std::memory_order_acquire)) {
        iteration++;

        // Get dirty pages
        auto dirty_pages = catalog_->getDirtyPages(current_migration_id_, ctx);
        uint32_t dirty_count = dirty_pages.size();

        LOG_INFO("Catch-up iteration {}: {} dirty pages", iteration, dirty_count);

        if (dirty_count == 0) {
            // Convergence achieved!
            LOG_INFO("Catch-up converged after {} iterations", iteration);
            return Status::OK;
        }

        // Clear dirty bitmap before re-copy
        catalog_->clearDirtyPages(current_migration_id_, ctx);

        // Re-copy dirty pages
        auto batch_start = std::chrono::steady_clock::now();

        for (uint32_t source_page : dirty_pages) {
            // Check if stopped
            if (!active_.load(std::memory_order_acquire)) {
                return Status::CANCELLED;
            }

            // Get target page number for this source page
            // (already allocated during initial copy)
            uint32_t target_page = source_page; // 1:1 mapping

            Status status = copyPage(
                current_table_id_,
                source_page,
                target_page,
                ctx);

            if (status != Status::OK) {
                LOG_ERROR("Failed to re-copy page {} during catch-up", source_page);
                return status;
            }
        }

        auto batch_end = std::chrono::steady_clock::now();
        auto batch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            batch_end - batch_start).count();

        float copy_rate = (dirty_count * 1000.0f) / batch_duration; // pages/sec

        // Wait a bit to measure new dirty rate
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Measure dirty rate
        uint32_t new_dirty_count = catalog_->getDirtyPageCount(current_migration_id_);
        float dirty_rate = new_dirty_count / 0.5f; // pages/sec (measured over 500ms)

        LOG_INFO("Catch-up iteration {}: copy_rate={:.1f} pages/sec, dirty_rate={:.1f} pages/sec",
                 iteration, copy_rate, dirty_rate);

        // Check convergence condition
        if (dirty_rate < copy_rate * CONVERGENCE_RATIO) {
            LOG_INFO("Convergence condition met (dirty_rate < copy_rate * 0.5)");

            // One more iteration to be sure
            if (catalog_->getDirtyPageCount(current_migration_id_) == 0) {
                LOG_INFO("Catch-up converged after {} iterations", iteration);
                return Status::OK;
            }
        }

        // Yield between iterations
        std::this_thread::sleep_for(std::chrono::milliseconds(YIELD_INTERVAL_MS));
    }

    // Did not converge
    LOG_ERROR("Catch-up did not converge after {} iterations", MAX_ITERATIONS);
    SET_ERROR_CONTEXT(ctx, Status::TIMEOUT,
                     "Catch-up phase did not converge (write load too high)");
    return Status::TIMEOUT;
}
```

**Modify**: `MigrationWorker::workerThreadMain()` to call catch-up phase:

```cpp
void MigrationWorker::workerThreadMain()
{
    ErrorContext ctx;

    // ... COPYING phase (already implemented) ...

    // Transition to CATCH_UP
    status = catalog_->setMigrationPhase(
        current_migration_id_,
        MigrationPhase::MIGRATION_CATCH_UP,
        &ctx);

    if (status != Status::OK) {
        LOG_ERROR("Failed to set migration phase to CATCH_UP: {}", ctx.message);
        active_.store(false, std::memory_order_release);
        return;
    }

    // Execute catch-up phase
    status = executeCatchUpPhase(&ctx);

    if (status != Status::OK) {
        LOG_ERROR("Catch-up phase failed: {}", ctx.message);

        // Mark migration as failed
        catalog_->setMigrationPhase(
            current_migration_id_,
            MigrationPhase::MIGRATION_FAILED,
            &ctx);

        active_.store(false, std::memory_order_release);
        return;
    }

    // Catch-up complete - ready for SWAP
    status = catalog_->setMigrationPhase(
        current_migration_id_,
        MigrationPhase::MIGRATION_READY_FOR_SWAP,
        &ctx);

    LOG_INFO("Migration ready for atomic swap");

    active_.store(false, std::memory_order_release);
}
```

**Testing Checkpoint 5.4.5.2**:
- [ ] Catch-up phase executes after COPYING
- [ ] Dirty pages re-copied correctly
- [ ] Convergence detection works
- [ ] Non-convergence detected (fails gracefully after 100 iterations)

### Subtask 5.4.5.3: Non-Convergence Handling (2 hours)

**Add**: Optional write pause for forced convergence

**Modify**: `src/core/migration_worker.cpp`

```cpp
/**
 * @brief Attempt brief write pause to force convergence
 * @param max_pause_ms Maximum pause duration (default 1000ms)
 * @return Status::OK if convergence achieved
 */
Status MigrationWorker::forceConvergence(
    uint32_t max_pause_ms,
    ErrorContext *ctx)
{
    LOG_WARN("Attempting forced convergence with brief write pause");

    // Acquire exclusive lock on table (blocks all writes)
    // Note: This is a last resort, should rarely be needed

    auto lock_start = std::chrono::steady_clock::now();

    // TODO: Implement table-level exclusive lock
    // For now, log warning
    LOG_WARN("Write pause not yet implemented - migration may fail");

    // Copy remaining dirty pages with exclusive lock held
    auto dirty_pages = catalog_->getDirtyPages(current_migration_id_, ctx);

    for (uint32_t page : dirty_pages) {
        Status status = copyPage(current_table_id_, page, page, ctx);
        if (status != Status::OK) {
            return status;
        }
    }

    auto lock_end = std::chrono::steady_clock::now();
    auto pause_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        lock_end - lock_start).count();

    LOG_INFO("Forced convergence completed in {} ms", pause_duration);

    if (pause_duration > max_pause_ms) {
        LOG_WARN("Write pause exceeded target ({} ms > {} ms)",
                 pause_duration, max_pause_ms);
    }

    return Status::OK;
}
```

**Testing Checkpoint 5.4.5.3**:
- [ ] Non-convergence detected correctly
- [ ] Migration fails gracefully with clear error message
- [ ] (Future) Write pause implementation for forced convergence

---

## Task 5.4.6: Atomic Swap

**Effort**: 8-10 hours
**Deliverable**: Atomic cutover with < 100ms downtime

### Subtask 5.4.6.1: Swap Execution (4-5 hours)

**Add**: `src/core/migration_worker.cpp`

```cpp
/**
 * @brief Execute atomic swap phase
 * @param ctx Error context
 * @return Status::OK on success
 */
Status MigrationWorker::executeSwapPhase(ErrorContext *ctx)
{
    LOG_INFO("Starting atomic swap for migration {}", current_migration_id_);

    auto swap_start = std::chrono::steady_clock::now();

    // Step 1: Acquire exclusive lock on table (blocks reads and writes)
    // Target: < 100ms total lock duration
    auto lock_start = std::chrono::steady_clock::now();

    // TODO: Table-level exclusive lock
    LOG_INFO("Acquiring exclusive lock on table {}", current_table_id_);

    // Step 2: Copy final dirty pages (should be very few)
    auto dirty_pages = catalog_->getDirtyPages(current_migration_id_, ctx);
    uint32_t final_dirty_count = dirty_pages.size();

    LOG_INFO("Final dirty page count: {}", final_dirty_count);

    if (final_dirty_count > 100) {
        LOG_WARN("High dirty page count during swap ({} pages) - may exceed 100ms target",
                 final_dirty_count);
    }

    // Copy final dirty pages
    for (uint32_t page : dirty_pages) {
        Status status = copyPage(current_table_id_, page, page, ctx);
        if (status != Status::OK) {
            LOG_ERROR("Failed to copy final dirty page {}", page);
            // TODO: Release lock
            return status;
        }
    }

    // Step 3: Update catalog (single transaction)
    Status status = performAtomicCatalogUpdate(ctx);
    if (status != Status::OK) {
        LOG_ERROR("Failed to update catalog during swap");
        // TODO: Release lock
        return status;
    }

    // Step 4: Batch update all index TIDs
    status = updateAllIndexTIDs(ctx);
    if (status != Status::OK) {
        LOG_ERROR("Failed to update index TIDs during swap");
        // TODO: Release lock, rollback catalog
        return status;
    }

    // Step 5: Release exclusive lock
    // TODO: Release table lock

    auto lock_end = std::chrono::steady_clock::now();
    auto lock_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        lock_end - lock_start).count();

    auto swap_end = std::chrono::steady_clock::now();
    auto swap_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        swap_end - swap_start).count();

    LOG_INFO("Atomic swap complete: total={} ms, lock_held={} ms",
             swap_duration, lock_duration);

    if (lock_duration > 100) {
        LOG_WARN("Swap lock duration exceeded 100ms target ({} ms)", lock_duration);
    } else {
        LOG_INFO("Swap lock duration within target (< 100ms): {} ms", lock_duration);
    }

    return Status::OK;
}

/**
 * @brief Perform atomic catalog update
 */
Status MigrationWorker::performAtomicCatalogUpdate(ErrorContext *ctx)
{
    // Get table info
    auto it = catalog_->table_cache_.find(current_table_id_);
    if (it == catalog_->table_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Table not found");
        return Status::NOT_FOUND;
    }

    TableInfo &table_info = it->second;

    // Begin transaction
    // TODO: Transaction support
    LOG_INFO("Updating catalog: table {} → tablespace {}",
             current_table_id_, target_tablespace_);

    // Update table's tablespace_id
    table_info.tablespace_id = target_tablespace_;

    // Clear migration flags
    table_info.migration_in_progress = false;
    table_info.migration_id = ID(); // Clear
    table_info.migration_xid = 0;
    table_info.migration_target_ts = 0;
    table_info.migration_phase = 0;

    // Update migration state
    auto state_it = catalog_->migration_cache_.find(current_migration_id_);
    if (state_it != catalog_->migration_cache_.end()) {
        state_it->second.phase = MigrationPhase::MIGRATION_SWAP;
        state_it->second.end_time = std::time(nullptr);
    }

    // Commit transaction
    // TODO: Commit

    LOG_INFO("Catalog updated successfully");

    return Status::OK;
}

/**
 * @brief Batch update all index TIDs (uses Sprint 2 code)
 */
Status MigrationWorker::updateAllIndexTIDs(ErrorContext *ctx)
{
    // Get all indexes for this table
    auto indexes = catalog_->getTableIndexes(current_table_id_, ctx);

    LOG_INFO("Updating TIDs for {} indexes", indexes.size());

    for (const auto &index_info : indexes) {
        Status status = updateIndexTIDs(index_info, ctx);
        if (status != Status::OK) {
            LOG_ERROR("Failed to update index {} TIDs", index_info.index_id);
            return status;
        }
    }

    LOG_INFO("All index TIDs updated successfully");

    return Status::OK;
}

/**
 * @brief Update TIDs for a single index
 */
Status MigrationWorker::updateIndexTIDs(
    const IndexInfo &index_info,
    ErrorContext *ctx)
{
    // Use TID resolution service to get old→new TID mappings
    auto mappings = db_->tid_resolver()->getAllMappings(
        current_table_id_,
        ctx);

    LOG_INFO("Updating {} TID mappings for index {}",
             mappings.size(), index_info.index_id);

    // Batch update index entries
    // This uses the Sprint 2 code for index TID updates
    // (already implemented for tablespace migration)

    // TODO: Get index implementation and call batchUpdateTIDs()

    return Status::OK;
}
```

**Testing Checkpoint 5.4.6.1**:
- [ ] Swap phase executes after catch-up
- [ ] Final dirty pages copied
- [ ] Catalog updated atomically
- [ ] Lock duration < 100ms for small dirty page counts

### Subtask 5.4.6.2: Integration with Worker Thread (2-3 hours)

**Modify**: `MigrationWorker::workerThreadMain()` to call swap phase:

```cpp
void MigrationWorker::workerThreadMain()
{
    ErrorContext ctx;

    // ... COPYING phase ...
    // ... CATCH_UP phase ...

    // Check if ready for swap
    auto state = catalog_->getMigrationState(current_migration_id_, &ctx);
    if (!state.has_value() ||
        state->phase != MigrationPhase::MIGRATION_READY_FOR_SWAP) {
        LOG_ERROR("Migration not ready for swap (phase: {})",
                 static_cast<int>(state->phase));
        active_.store(false, std::memory_order_release);
        return;
    }

    // Execute swap phase
    Status status = executeSwapPhase(&ctx);

    if (status != Status::OK) {
        LOG_ERROR("Swap phase failed: {}", ctx.message);

        // Mark migration as failed
        catalog_->setMigrationPhase(
            current_migration_id_,
            MigrationPhase::MIGRATION_FAILED,
            &ctx);

        active_.store(false, std::memory_order_release);
        return;
    }

    // Swap complete - transition to CLEANUP
    status = catalog_->setMigrationPhase(
        current_migration_id_,
        MigrationPhase::MIGRATION_CLEANUP,
        &ctx);

    LOG_INFO("Swap complete, transitioning to cleanup phase");

    active_.store(false, std::memory_order_release);
}
```

**Testing Checkpoint 5.4.6.2**:
- [ ] Worker thread executes full pipeline (COPYING → CATCH_UP → SWAP)
- [ ] Table accessible after swap
- [ ] Queries return correct results
- [ ] Table now using target tablespace

### Subtask 5.4.6.3: Rollback on Failure (2 hours)

**Add**: Rollback support for failed swap:

```cpp
/**
 * @brief Rollback failed swap
 */
Status MigrationWorker::rollbackSwap(ErrorContext *ctx)
{
    LOG_WARN("Rolling back failed swap for migration {}", current_migration_id_);

    // Get table info
    auto it = catalog_->table_cache_.find(current_table_id_);
    if (it == catalog_->table_cache_.end()) {
        return Status::NOT_FOUND;
    }

    TableInfo &table_info = it->second;

    // Restore original tablespace_id
    table_info.tablespace_id = source_tablespace_;

    // Keep migration_in_progress = true for potential retry

    LOG_INFO("Swap rollback complete - table restored to source tablespace");

    return Status::OK;
}
```

**Testing Checkpoint 5.4.6.3**:
- [ ] Rollback restores table to source tablespace
- [ ] Table remains queryable after rollback
- [ ] Migration can be retried after rollback

---

## Task 5.4.7: Cleanup Phase

**Effort**: 4-5 hours
**Deliverable**: Source page deallocation and state cleanup

### Subtask 5.4.7.1: Deferred Source Page Deallocation (2-3 hours)

**Add**: `src/core/migration_worker.cpp`

```cpp
/**
 * @brief Execute cleanup phase (deferred deallocation)
 */
Status MigrationWorker::executeCleanupPhase(ErrorContext *ctx)
{
    LOG_INFO("Starting cleanup phase for migration {}", current_migration_id_);

    // Step 1: Wait for all active transactions to complete
    // This ensures no transaction is still reading from source pages

    // Get migration_xid
    auto state = catalog_->getMigrationState(current_migration_id_, ctx);
    if (!state.has_value()) {
        return Status::NOT_FOUND;
    }

    uint64_t migration_xid = state->migration_xid;

    LOG_INFO("Waiting for transactions older than {} to complete", migration_xid);

    // Wait for old transactions
    Status status = waitForOldTransactions(migration_xid, ctx);
    if (status != Status::OK) {
        LOG_ERROR("Failed to wait for old transactions");
        return status;
    }

    // Step 2: Deallocate all source pages
    LOG_INFO("Deallocating {} source pages", total_pages_);

    for (uint32_t page_num = 0; page_num < total_pages_; ++page_num) {
        // Deallocate source page
        status = page_manager_->deallocatePage(
            source_tablespace_,
            page_num,
            ctx);

        if (status != Status::OK) {
            LOG_ERROR("Failed to deallocate source page {}", page_num);
            // Continue with other pages
        }

        // Progress logging
        if (page_num % 1000 == 0 && page_num > 0) {
            LOG_INFO("Cleanup progress: {} / {} pages", page_num, total_pages_);
        }
    }

    // Step 3: Update FSM (Free Space Map) for source tablespace
    LOG_INFO("Updating FSM for source tablespace");

    // TODO: FSM update (if implemented)

    // Step 4: Clear migration state from catalog
    status = catalog_->completeMigration(current_migration_id_, ctx);
    if (status != Status::OK) {
        LOG_ERROR("Failed to complete migration in catalog");
        return status;
    }

    // Step 5: Clear TID resolver state
    db_->tid_resolver()->clearMigration(current_table_id_, ctx);

    LOG_INFO("Cleanup phase complete");

    return Status::OK;
}

/**
 * @brief Wait for all transactions older than migration_xid to complete
 */
Status MigrationWorker::waitForOldTransactions(
    uint64_t migration_xid,
    ErrorContext *ctx)
{
    static constexpr uint32_t MAX_WAIT_SECONDS = 60;
    static constexpr uint32_t POLL_INTERVAL_MS = 100;

    uint32_t iterations = (MAX_WAIT_SECONDS * 1000) / POLL_INTERVAL_MS;

    for (uint32_t i = 0; i < iterations; ++i) {
        // Check if any active transaction has xid < migration_xid
        bool old_txn_active = db_->transaction_manager()->hasActiveTransaction(
            migration_xid,
            ctx);

        if (!old_txn_active) {
            LOG_INFO("All old transactions completed after {} ms", i * POLL_INTERVAL_MS);
            return Status::OK;
        }

        // Wait and retry
        std::this_thread::sleep_for(
            std::chrono::milliseconds(POLL_INTERVAL_MS));
    }

    // Timeout
    LOG_ERROR("Timeout waiting for old transactions to complete");
    SET_ERROR_CONTEXT(ctx, Status::TIMEOUT,
                     "Old transactions still active after 60 seconds");
    return Status::TIMEOUT;
}
```

**Testing Checkpoint 5.4.7.1**:
- [ ] Cleanup waits for old transactions
- [ ] Source pages deallocated correctly
- [ ] FSM updated (if applicable)
- [ ] No data loss during cleanup

### Subtask 5.4.7.2: Migration State Cleanup (1-2 hours)

**Modify**: `include/scratchbird/core/catalog_manager.h`

```cpp
/**
 * @brief Complete migration and clean up state
 * @param migration_id Migration ID
 * @param ctx Error context
 * @return Status::OK on success
 */
Status completeMigration(const ID &migration_id, ErrorContext *ctx);
```

**Modify**: `src/core/catalog_manager.cpp`

```cpp
Status CatalogManager::completeMigration(const ID &migration_id, ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(migration_mutex_);

    auto it = migration_cache_.find(migration_id);
    if (it == migration_cache_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Migration not found");
        return Status::NOT_FOUND;
    }

    TableMigrationState &state = it->second;

    // Set phase to COMPLETE
    state.phase = MigrationPhase::MIGRATION_COMPLETE;
    state.end_time = std::time(nullptr);

    // Calculate total duration
    uint64_t duration_seconds = state.end_time - state.start_time;

    LOG_INFO("Migration {} completed in {} seconds ({} pages)",
             migration_id,
             duration_seconds,
             state.total_pages);

    // Remove from active migration cache (move to history)
    // TODO: Persist migration history to disk

    migration_cache_.erase(it);

    return Status::OK;
}
```

**Testing Checkpoint 5.4.7.2**:
- [ ] Migration state cleaned up correctly
- [ ] Migration marked as COMPLETE
- [ ] No memory leaks

### Subtask 5.4.7.3: Integration with Worker Thread (1 hour)

**Modify**: `MigrationWorker::workerThreadMain()` to call cleanup:

```cpp
void MigrationWorker::workerThreadMain()
{
    ErrorContext ctx;

    // ... COPYING, CATCH_UP, SWAP phases ...

    // Execute cleanup phase (in background, can take time)
    Status status = executeCleanupPhase(&ctx);

    if (status != Status::OK) {
        LOG_ERROR("Cleanup phase failed: {}", ctx.message);
        // Note: Migration is already complete from user perspective,
        // cleanup failure is not fatal
    }

    LOG_INFO("Migration workflow complete for migration {}", current_migration_id_);

    active_.store(false, std::memory_order_release);
}
```

**Testing Checkpoint 5.4.7.3**:
- [ ] Full migration pipeline executes (COPYING → CATCH_UP → SWAP → CLEANUP → COMPLETE)
- [ ] Source pages deallocated after completion
- [ ] Table fully migrated and accessible

---

## Testing Strategy

### Unit Tests

**Test Suite**: `test/core/test_migration_worker.cpp`

```cpp
// Test 1: Basic migration worker lifecycle
TEST_F(MigrationWorkerTest, StartStop)
{
    // Start worker
    // Stop worker
    // Verify clean shutdown
}

// Test 2: Incremental copy
TEST_F(MigrationWorkerTest, IncrementalCopy)
{
    // Insert 10,000 tuples
    // Start migration
    // Wait for COPYING phase complete
    // Verify all tuples copied
    // Verify TID mappings correct
}

// Test 3: Dirty page tracking
TEST_F(MigrationWorkerTest, DirtyPageTracking)
{
    // Start migration
    // During COPYING, update some tuples
    // Verify dirty pages tracked
    // Verify catch-up re-copies dirty pages
}

// Test 4: Catch-up convergence
TEST_F(MigrationWorkerTest, CatchUpConvergence)
{
    // Start migration with moderate write load
    // Verify catch-up converges
    // Verify final dirty count = 0
}

// Test 5: Non-convergence detection
TEST_F(MigrationWorkerTest, NonConvergence)
{
    // Start migration with very high write load
    // Verify non-convergence detected
    // Verify migration fails gracefully
}

// Test 6: Atomic swap
TEST_F(MigrationWorkerTest, AtomicSwap)
{
    // Complete migration to READY_FOR_SWAP
    // Execute swap
    // Verify table switched to target tablespace
    // Verify queries work
    // Verify lock duration < 100ms
}

// Test 7: Cleanup
TEST_F(MigrationWorkerTest, Cleanup)
{
    // Complete full migration
    // Verify source pages deallocated
    // Verify migration state cleaned up
}

// Test 8: Concurrent queries during migration
TEST_F(MigrationWorkerTest, ConcurrentQueries)
{
    // Start migration
    // Run concurrent SELECT queries
    // Verify correct results
    // Verify performance overhead < 10%
}

// Test 9: Concurrent writes during migration
TEST_F(MigrationWorkerTest, ConcurrentWrites)
{
    // Start migration
    // Run concurrent INSERT/UPDATE/DELETE
    // Verify writes routed correctly
    // Verify no data loss
    // Verify final table state correct
}
```

### Integration Tests

**Test Suite**: `test/integration/test_online_migration.cpp`

```cpp
// Test 1: Small table migration (< 1000 pages)
TEST_F(OnlineMigrationTest, SmallTable)
{
    // Create table with 500 pages
    // Migrate to new tablespace
    // Verify success in < 1 second
}

// Test 2: Large table migration (100K pages)
TEST_F(OnlineMigrationTest, LargeTable)
{
    // Create table with 100,000 pages
    // Migrate with concurrent workload
    // Verify success in < 2 minutes
    // Verify throughput > 500 pages/sec
}

// Test 3: Migration with indexes
TEST_F(OnlineMigrationTest, TableWithIndexes)
{
    // Create table with 3 indexes
    // Populate with data
    // Migrate table
    // Verify indexes updated correctly
    // Verify index scans work after migration
}

// Test 4: Migration abort
TEST_F(OnlineMigrationTest, AbortMigration)
{
    // Start migration
    // Abort during COPYING phase
    // Verify table still accessible
    // Verify in source tablespace
}

// Test 5: End-to-end with high write load
TEST_F(OnlineMigrationTest, HighWriteLoad)
{
    // Start migration
    // Run high write load (1000 writes/sec)
    // Verify catch-up converges
    // Verify final table correct
}
```

### Performance Tests

**Test Suite**: `test/performance/test_migration_performance.cpp`

```cpp
// Test 1: Query overhead measurement
TEST_F(MigrationPerformanceTest, QueryOverhead)
{
    // Baseline: Run 10,000 queries without migration
    // Measurement: Run 10,000 queries during migration
    // Verify overhead < 10%
}

// Test 2: Write overhead measurement
TEST_F(MigrationPerformanceTest, WriteOverhead)
{
    // Baseline: 1000 writes/sec without migration
    // Measurement: writes/sec during migration
    // Verify throughput degradation < 10%
}

// Test 3: Swap downtime measurement
TEST_F(MigrationPerformanceTest, SwapDowntime)
{
    // Measure time table is locked during swap
    // Verify < 100ms for < 100 dirty pages
}

// Test 4: Throughput measurement
TEST_F(MigrationPerformanceTest, CopyThroughput)
{
    // Measure pages/sec during COPYING phase
    // Verify > 500 pages/sec (target 1000+)
}
```

---

## Integration Points

### Sprint 4 Dependencies

Sprint 5 builds on Sprint 4 infrastructure:

1. **Migration State Management** (Task 5.4.1)
   - `CatalogManager::startOnlineMigration()`
   - `CatalogManager::setMigrationPhase()`
   - `CatalogManager::getMigrationState()`
   - `TableMigrationState` structure

2. **TID Resolution Service** (Task 5.4.2)
   - `TIDResolver::recordMigration()`
   - `TIDResolver::resolveTablespace()`
   - `TIDResolver::getAllMappings()`

3. **Write Routing** (Task 5.4.3)
   - Dirty page tracking during writes
   - INSERT/UPDATE/DELETE routing logic

### Sprint 2 Dependencies

Sprint 5 uses Sprint 2 code:

1. **Index TID Updates**
   - Batch TID update for indexes (already implemented)
   - Used during SWAP phase

### External Dependencies

1. **Buffer Pool**
   - Page pinning/unpinning
   - Dirty page flushing

2. **Page Manager**
   - Page allocation (`allocatePage()`)
   - Page deallocation (`deallocatePage()`)

3. **Transaction Manager**
   - XID generation
   - Active transaction tracking

---

## Performance Targets

### Throughput

| Metric | Target | Acceptable |
|--------|--------|------------|
| Copy throughput | 1000+ pages/sec | 500+ pages/sec |
| Catch-up iterations | < 10 | < 100 |
| Convergence time | < 10 seconds | < 60 seconds |

### Overhead

| Metric | Target | Acceptable |
|--------|--------|------------|
| Query overhead (non-migrating) | 0% | < 1% |
| Query overhead (migrating) | < 5% | < 10% |
| Write overhead (migrating) | < 5% | < 10% |

### Downtime

| Metric | Target | Acceptable |
|--------|--------|------------|
| Swap lock duration | < 50ms | < 100ms |
| Final dirty pages | < 50 | < 100 |

### Scalability

| Table Size | Expected Duration |
|------------|-------------------|
| Small (< 1000 pages) | < 1 second |
| Medium (1K-100K pages) | 1 sec - 2 min |
| Large (100K-10M pages) | 2 min - 3 hours |

---

## Success Criteria

Sprint 5 is **COMPLETE** when:

- [ ] **Task 5.4.4**: Incremental copy implemented
  - [ ] MigrationWorker class functional
  - [ ] Background thread copies pages
  - [ ] Progress tracking accurate
  - [ ] TID mappings updated

- [ ] **Task 5.4.5**: Catch-up phase implemented
  - [ ] Dirty pages re-copied
  - [ ] Convergence detection works
  - [ ] Non-convergence fails gracefully

- [ ] **Task 5.4.6**: Atomic swap implemented
  - [ ] Final dirty pages copied
  - [ ] Catalog updated atomically
  - [ ] Index TIDs batch updated
  - [ ] Swap duration < 100ms

- [ ] **Task 5.4.7**: Cleanup implemented
  - [ ] Source pages deallocated
  - [ ] Migration state cleaned up
  - [ ] No memory leaks

- [ ] **Testing**: All tests pass
  - [ ] Unit tests (9 tests)
  - [ ] Integration tests (5 tests)
  - [ ] Performance tests (4 tests)
  - [ ] Performance targets met

- [ ] **Build**: No compilation warnings or errors

- [ ] **Documentation**: Updated
  - [ ] Sprint 5 summary created
  - [ ] Roadmap updated

---

## Implementation Order

**Recommended Sequence**:

1. **Day 1** (8-10 hours): Task 5.4.4 - Incremental Copy
   - Create MigrationWorker class
   - Implement page copying
   - Test incremental copy

2. **Day 2** (6-8 hours): Task 5.4.5 - Catch-Up Phase
   - Implement catch-up execution
   - Add convergence detection
   - Test with concurrent writes

3. **Day 3** (8-10 hours): Task 5.4.6 - Atomic Swap
   - Implement swap execution
   - Add rollback support
   - Test swap performance

4. **Day 4** (4-5 hours): Task 5.4.7 - Cleanup
   - Implement deferred deallocation
   - Add state cleanup
   - Test full pipeline

**Total**: 26-33 hours

---

## Files to Create/Modify

### New Files (2)

1. `include/scratchbird/core/migration_worker.h` (~250 lines)
2. `src/core/migration_worker.cpp` (~800 lines)

### Modified Files (4)

1. `include/scratchbird/core/database.h` (add migration_worker member)
2. `src/core/database.cpp` (initialize migration_worker)
3. `include/scratchbird/core/catalog_manager.h` (add cleanup methods)
4. `src/core/catalog_manager.cpp` (implement cleanup)

### Build System (1)

1. `CMakeLists.txt` (add migration_worker.cpp)

**Total**: 7 files touched, ~1050 lines new code

---

## Risk Assessment

### High Risks

1. **Swap phase timeout**
   - **Mitigation**: Limit dirty pages before swap, optimize batch TID updates
   - **Contingency**: Increase timeout to 500ms

2. **Non-convergence on high write load**
   - **Mitigation**: Convergence detection, fail gracefully
   - **Contingency**: Suggest OFFLINE migration, or implement brief write pause

### Medium Risks

3. **Cleanup phase delays**
   - **Mitigation**: Deferred cleanup in background
   - **Contingency**: Cleanup can be retried later

4. **Index TID update performance**
   - **Mitigation**: Use Sprint 2 batch update code
   - **Contingency**: Parallelize index updates

### Low Risks

5. **Memory overhead**
   - **Mitigation**: Release pages after copy, yield regularly
   - **Contingency**: Reduce batch size

---

## Conclusion

Sprint 5 implements the core execution phases of ONLINE migration:

- **Incremental copy** moves data in the background
- **Catch-up** ensures convergence under write load
- **Atomic swap** performs cutover in < 100ms
- **Cleanup** deallocates source pages

**Status**: Implementation plan complete and ready for execution.

**Next Action**: Execute implementation following this plan, testing incrementally at each checkpoint.

---

**Document Version**: 1.0
**Last Updated**: October 21, 2025
**Status**: ✅ PLAN COMPLETE
**Next Sprint**: Sprint 6 (Error Handling and Integration Testing)
