/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * LSM Compaction Manager Implementation
 *
 * Manages background compaction for LSMTreeIndex
 * Implements leveled compaction strategy
 *
 * Architecture:
 * - Level 0: 4 SSTables max (2 MB each, overlapping keys)
 * - Level 1: 100 MB total (10 MB SSTables, non-overlapping)
 * - Level 2: 1 GB total (10 MB SSTables, non-overlapping)
 * - Level 3: 10 GB total (10 MB SSTables, non-overlapping)
 *
 * Compaction Strategy:
 * - Trigger: Level 0 has 4+ SSTables OR level exceeds size limit
 * - Algorithm: K-way merge with MGA garbage collection
 * - Uses TIP for visibility checks (Firebird MGA)
 *
 * November 22, 2025
 */

#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/lsm_thread_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include <algorithm>
#include <queue>
#include <cstring>
#include <ctime>
#include <sys/stat.h>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

LSMCompactionManager::LSMCompactionManager(TransactionManager *txn_mgr, bool enable_parallel)
    : txn_mgr_(txn_mgr),
      index_path_("/tmp/scratchbird"),
      block_size_(4096),
      parallel_enabled_(enable_parallel)
{
    // Initialize 4 levels (0-3)
    levels_.resize(4);

    // Level 0: 4 SSTables max, 2 MB each = 8 MB total
    levels_[0] = LevelMetadata(0, 8 * 1024 * 1024);

    // Level 1: 100 MB total (10x Level 0)
    levels_[1] = LevelMetadata(1, 100 * 1024 * 1024);

    // Level 2: 1 GB total (10x Level 1)
    levels_[2] = LevelMetadata(2, 1024 * 1024 * 1024);

    // Level 3: 10 GB total (10x Level 2)
    levels_[3] = LevelMetadata(3, 10ULL * 1024 * 1024 * 1024);

    // Create thread pool if parallel compaction is enabled
    if (parallel_enabled_)
    {
        thread_pool_ = std::make_unique<LSMThreadPool>();
    }
}

LSMCompactionManager::~LSMCompactionManager()
{
    // Stop thread pool (waits for pending tasks)
    if (thread_pool_)
    {
        thread_pool_->stop(true);
    }
}

// ============================================================================
// Initialization
// ============================================================================

Status LSMCompactionManager::initialize(ErrorContext *ctx)
{
    // Start thread pool if parallel compaction enabled
    if (thread_pool_)
    {
        thread_pool_->start();
    }

    return Status::OK;
}

void LSMCompactionManager::setParallelCompaction(bool enable)
{
    if (enable && !thread_pool_)
    {
        // Create and start thread pool
        thread_pool_ = std::make_unique<LSMThreadPool>();
        thread_pool_->start();
        parallel_enabled_ = true;
    }
    else if (!enable && thread_pool_)
    {
        // Stop and destroy thread pool
        thread_pool_->stop(true);
        thread_pool_.reset();
        parallel_enabled_ = false;
    }
}

// ============================================================================
// SSTable Management
// ============================================================================

Status LSMCompactionManager::addSSTable(uint32_t level,
                                        const std::string &sstable_path,
                                        uint64_t file_size,
                                        ErrorContext *ctx)
{
    if (level >= levels_.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid level (must be 0-3)");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Add SSTable to level
    levels_[level].sstable_paths.push_back(sstable_path);
    levels_[level].size_bytes += file_size;

    return Status::OK;
}

// ============================================================================
// Compaction Triggering
// ============================================================================

bool LSMCompactionManager::needsCompaction()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check Level 0: Trigger if 4+ SSTables
    if (levels_[0].sstable_paths.size() >= 4)
    {
        return true;
    }

    // Check Level 1-3: Trigger if size exceeds limit
    for (size_t i = 1; i < levels_.size(); i++)
    {
        if (levels_[i].size_bytes > levels_[i].size_limit_bytes)
        {
            return true;
        }
    }

    return false;
}

Status LSMCompactionManager::selectCompactionTask(CompactionTask *task_out,
                                                  ErrorContext *ctx)
{
    if (!task_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "task_out cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Priority 1: Compact Level 0 if 4+ SSTables
    if (levels_[0].sstable_paths.size() >= 4)
    {
        task_out->source_level = 0;
        task_out->target_level = 1;
        task_out->source_sstables = levels_[0].sstable_paths;

        // Find overlapping Level 1 SSTables using key range detection
        // First, determine min/max keys from source SSTables
        std::vector<uint8_t> min_key, max_key;
        for (const auto &path : task_out->source_sstables)
        {
            SSTableReader reader(path);
            if (reader.open(ctx) == Status::OK)
            {
                auto sst_min = reader.getMinKey();
                auto sst_max = reader.getMaxKey();
                if (min_key.empty() || sst_min < min_key) min_key = sst_min;
                if (max_key.empty() || sst_max > max_key) max_key = sst_max;
                reader.close(ctx);
            }
        }
        // Find overlapping SSTables in target level
        findOverlappingSSTables(1, min_key, max_key, &task_out->overlapping_sstables);

        // Set OIT for garbage collection from TransactionManager
        task_out->oit = txn_mgr_ ? txn_mgr_->getOldestXid() : 0;

        return Status::OK;
    }

    // Priority 2: Compact Level 1 if exceeds limit
    if (levels_[1].size_bytes > levels_[1].size_limit_bytes)
    {
        task_out->source_level = 1;
        task_out->target_level = 2;

        // Select oldest SSTable from Level 1
        if (!levels_[1].sstable_paths.empty())
        {
            task_out->source_sstables.push_back(levels_[1].sstable_paths.front());
        }

        // Find overlapping Level 2 SSTables using key range detection
        std::vector<uint8_t> min_key, max_key;
        for (const auto &path : task_out->source_sstables)
        {
            SSTableReader reader(path);
            if (reader.open(ctx) == Status::OK)
            {
                auto sst_min = reader.getMinKey();
                auto sst_max = reader.getMaxKey();
                if (min_key.empty() || sst_min < min_key) min_key = sst_min;
                if (max_key.empty() || sst_max > max_key) max_key = sst_max;
                reader.close(ctx);
            }
        }
        findOverlappingSSTables(2, min_key, max_key, &task_out->overlapping_sstables);

        // Set OIT for garbage collection from TransactionManager
        task_out->oit = txn_mgr_ ? txn_mgr_->getOldestXid() : 0;

        return Status::OK;
    }

    // Priority 3: Compact Level 2 if exceeds limit
    if (levels_[2].size_bytes > levels_[2].size_limit_bytes)
    {
        task_out->source_level = 2;
        task_out->target_level = 3;

        // Select oldest SSTable from Level 2
        if (!levels_[2].sstable_paths.empty())
        {
            task_out->source_sstables.push_back(levels_[2].sstable_paths.front());
        }

        // Find overlapping Level 3 SSTables using key range detection
        std::vector<uint8_t> min_key, max_key;
        for (const auto &path : task_out->source_sstables)
        {
            SSTableReader reader(path);
            if (reader.open(ctx) == Status::OK)
            {
                auto sst_min = reader.getMinKey();
                auto sst_max = reader.getMaxKey();
                if (min_key.empty() || sst_min < min_key) min_key = sst_min;
                if (max_key.empty() || sst_max > max_key) max_key = sst_max;
                reader.close(ctx);
            }
        }
        findOverlappingSSTables(3, min_key, max_key, &task_out->overlapping_sstables);

        // Set OIT for garbage collection from TransactionManager
        task_out->oit = txn_mgr_ ? txn_mgr_->getOldestXid() : 0;

        return Status::OK;
    }

    // No compaction needed
    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
        "No compaction task available");
    return Status::NOT_FOUND;
}

// ============================================================================
// Compaction Execution
// ============================================================================

Status LSMCompactionManager::executeCompaction(const CompactionTask &task,
                                               ErrorContext *ctx)
{
    // Combine source and overlapping SSTables for merge
    std::vector<std::string> input_sstables;
    input_sstables.insert(input_sstables.end(),
                         task.source_sstables.begin(),
                         task.source_sstables.end());
    input_sstables.insert(input_sstables.end(),
                         task.overlapping_sstables.begin(),
                         task.overlapping_sstables.end());

    if (input_sstables.empty())
    {
        // Nothing to compact
        return Status::OK;
    }

    // Generate output SSTable path using configured index path
    std::string output_sstable = index_path_ + "/L" +
                                std::to_string(task.target_level) + "_" +
                                std::to_string(std::time(nullptr)) + "_compacted.sst";

    // Execute k-way merge
    Status s = kWayMerge(input_sstables, output_sstable, task.oit, ctx);
    if (s != Status::OK)
    {
        return s;
    }

    // Get size of new SSTable
    struct stat st;
    uint64_t new_size = 0;
    if (stat(output_sstable.c_str(), &st) == 0)
    {
        new_size = st.st_size;
    }

    // Atomically replace old SSTables with new one
    std::vector<std::string> old_sstables = input_sstables;
    std::vector<std::string> new_sstables = {output_sstable};

    s = replaceSSTablesAtomic(task.target_level, old_sstables, new_sstables, ctx);
    if (s != Status::OK)
    {
        // Failed to replace - clean up new SSTable
        std::remove(output_sstable.c_str());
        return s;
    }

    // Delete old SSTable files
    for (const auto &old_path : old_sstables)
    {
        std::remove(old_path.c_str());
    }

    return Status::OK;
}

Status LSMCompactionManager::executeCompactionParallel(const CompactionTask &task,
                                                       ErrorContext *ctx)
{
    if (!thread_pool_)
    {
        // Fall back to single-threaded execution if thread pool not available
        return executeCompaction(task, ctx);
    }

    // Create task function that wraps executeCompaction
    // Note: We capture 'this' and copy the task to avoid lifetime issues
    auto task_fn = [this](const CompactionTask& t) -> bool {
        ErrorContext local_ctx;
        Status s = executeCompaction(t, &local_ctx);
        return s == Status::OK;
    };

    // Submit to thread pool (non-blocking)
    bool submitted = thread_pool_->submit(task, task_fn);
    if (!submitted)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
            "Failed to submit compaction task to thread pool");
        return Status::INTERNAL_ERROR;
    }

    return Status::OK;
}

// ============================================================================
// Statistics
// ============================================================================

void LSMCompactionManager::getStatistics(uint64_t *total_sstables_out,
                                        uint64_t *total_size_out)
{
    std::lock_guard<std::mutex> lock(mutex_);

    uint64_t total_sstables = 0;
    uint64_t total_size = 0;

    for (const auto &level : levels_)
    {
        total_sstables += level.sstable_paths.size();
        total_size += level.size_bytes;
    }

    if (total_sstables_out)
    {
        *total_sstables_out = total_sstables;
    }

    if (total_size_out)
    {
        *total_size_out = total_size;
    }
}

void LSMCompactionManager::recalculateLevelSizes()
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &level : levels_)
    {
        uint64_t level_size = 0;
        for (const auto &path : level.sstable_paths)
        {
            struct stat st;
            if (stat(path.c_str(), &st) == 0)
            {
                level_size += static_cast<uint64_t>(st.st_size);
            }
        }
        level.size_bytes = level_size;
    }
}

// ============================================================================
// Helper Methods (Private)
// ============================================================================

void LSMCompactionManager::findOverlappingSSTables(
    uint32_t level,
    const std::vector<uint8_t> &min_key,
    const std::vector<uint8_t> &max_key,
    std::vector<std::string> *overlapping_out)
{
    if (!overlapping_out || level >= levels_.size())
    {
        return;
    }

    overlapping_out->clear();

    // If source keys are empty, return all SSTables at this level (conservative)
    if (min_key.empty() && max_key.empty())
    {
        *overlapping_out = levels_[level].sstable_paths;
        return;
    }

    // Check each SSTable in the target level for key range overlap
    // Two ranges [a,b] and [c,d] overlap if: a <= d AND c <= b
    for (const auto &sst_path : levels_[level].sstable_paths)
    {
        SSTableReader reader(sst_path);
        if (reader.open(nullptr) != Status::OK)
        {
            // If we can't read the SSTable, include it conservatively
            overlapping_out->push_back(sst_path);
            continue;
        }

        auto sst_min = reader.getMinKey();
        auto sst_max = reader.getMaxKey();
        reader.close(nullptr);

        // Check for overlap: source[min,max] overlaps with sst[sst_min,sst_max]
        // Overlap exists if: min_key <= sst_max AND sst_min <= max_key
        bool overlaps = true;
        if (!min_key.empty() && !sst_max.empty())
        {
            // min_key > sst_max means no overlap (source starts after SSTable ends)
            if (min_key > sst_max)
            {
                overlaps = false;
            }
        }
        if (!max_key.empty() && !sst_min.empty())
        {
            // sst_min > max_key means no overlap (SSTable starts after source ends)
            if (sst_min > max_key)
            {
                overlaps = false;
            }
        }

        if (overlaps)
        {
            overlapping_out->push_back(sst_path);
        }
    }
}

// Merge entry for priority queue
struct MergeEntry
{
    std::vector<uint8_t> key;
    std::vector<uint8_t> value;
    uint64_t sequence_number;
    uint8_t entry_type;
    uint64_t xmin;
    uint64_t xmax;
    SSTableReader::Iterator* iterator;  // Source iterator
    size_t sstable_index;  // Which SSTable this came from

    // Comparator for priority queue (min-heap: smallest key first)
    // If keys are equal, higher sequence number comes first (newer version)
    bool operator>(const MergeEntry& other) const
    {
        int cmp = std::memcmp(key.data(), other.key.data(),
                             std::min(key.size(), other.key.size()));
        if (cmp != 0) return cmp > 0;
        if (key.size() != other.key.size()) return key.size() > other.key.size();

        // Same key: newer version (higher sequence number) comes first
        return sequence_number < other.sequence_number;
    }
};

Status LSMCompactionManager::kWayMerge(const std::vector<std::string> &input_sstables,
                                       const std::string &output_sstable,
                                       uint64_t oit,
                                       ErrorContext *ctx)
{
    if (input_sstables.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No input SSTables for merge");
        return Status::INVALID_ARGUMENT;
    }

    // Step 1: Open all input SSTables and create iterators
    std::vector<std::unique_ptr<SSTableReader>> readers;
    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;

    for (const auto& path : input_sstables)
    {
        auto reader = std::make_unique<SSTableReader>(path);
        Status s = reader->open(ctx);
        if (s != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, s, ("Failed to open SSTable: " + path).c_str());
            return s;
        }

        auto it = reader->createIterator();
        if (!it)
        {
            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                ("Failed to create iterator for SSTable: " + path).c_str());
            return Status::INTERNAL_ERROR;
        }

        iterators.push_back(std::move(it));
        readers.push_back(std::move(reader));
    }

    // Step 2: Create output SSTable writer with configurable block size
    SSTableWriter writer(output_sstable, block_size_);
    Status s = writer.open(ctx);
    if (s != Status::OK)
    {
        return s;
    }

    // Step 3: Initialize priority queue with first entry from each SSTable
    std::priority_queue<MergeEntry, std::vector<MergeEntry>, std::greater<MergeEntry>> pq;

    for (size_t i = 0; i < iterators.size(); i++)
    {
        if (iterators[i]->isValid())
        {
            MergeEntry entry;
            entry.key = iterators[i]->key();
            entry.value = iterators[i]->value();
            entry.sequence_number = iterators[i]->sequenceNumber();
            entry.entry_type = iterators[i]->entryType();
            entry.xmin = iterators[i]->xmin();
            entry.xmax = iterators[i]->xmax();
            entry.iterator = iterators[i].get();
            entry.sstable_index = i;

            pq.push(entry);
        }
    }

    // Step 4: K-way merge with deduplication and garbage collection
    std::vector<uint8_t> last_key;
    uint64_t entries_written = 0;
    uint64_t entries_skipped_duplicate = 0;
    uint64_t entries_skipped_gc = 0;

    while (!pq.empty())
    {
        MergeEntry entry = pq.top();
        pq.pop();

        // Skip duplicate keys (keep only newest version - first occurrence)
        if (!last_key.empty() && entry.key == last_key)
        {
            entries_skipped_duplicate++;

            // Advance iterator and add next entry from same SSTable
            entry.iterator->next();
            if (entry.iterator->isValid())
            {
                MergeEntry next_entry;
                next_entry.key = entry.iterator->key();
                next_entry.value = entry.iterator->value();
                next_entry.sequence_number = entry.iterator->sequenceNumber();
                next_entry.entry_type = entry.iterator->entryType();
                next_entry.xmin = entry.iterator->xmin();
                next_entry.xmax = entry.iterator->xmax();
                next_entry.iterator = entry.iterator;
                next_entry.sstable_index = entry.sstable_index;

                pq.push(next_entry);
            }
            continue;
        }

        // MGA Garbage Collection: Remove entries invisible to all transactions
        // An entry is garbage if:
        // 1. It was created and deleted before OIT (xmin < oit && xmax < oit && xmax != 0)
        // 2. It's a tombstone for a version that's already been deleted
        bool is_garbage = false;
        if (oit > 0)
        {
            // Entry was committed and deleted before OIT
            if (entry.xmin < oit && entry.xmax > 0 && entry.xmax < oit)
            {
                is_garbage = true;
            }
        }

        if (is_garbage)
        {
            entries_skipped_gc++;

            // Advance iterator
            entry.iterator->next();
            if (entry.iterator->isValid())
            {
                MergeEntry next_entry;
                next_entry.key = entry.iterator->key();
                next_entry.value = entry.iterator->value();
                next_entry.sequence_number = entry.iterator->sequenceNumber();
                next_entry.entry_type = entry.iterator->entryType();
                next_entry.xmin = entry.iterator->xmin();
                next_entry.xmax = entry.iterator->xmax();
                next_entry.iterator = entry.iterator;
                next_entry.sstable_index = entry.sstable_index;

                pq.push(next_entry);
            }
            continue;
        }

        // Write entry to output SSTable
        s = writer.addEntry(entry.key, entry.value, entry.sequence_number,
                          entry.entry_type, entry.xmin, entry.xmax, ctx);
        if (s != Status::OK)
        {
            writer.close(ctx);
            return s;
        }

        entries_written++;
        last_key = entry.key;

        // Advance iterator and add next entry from same SSTable
        entry.iterator->next();
        if (entry.iterator->isValid())
        {
            MergeEntry next_entry;
            next_entry.key = entry.iterator->key();
            next_entry.value = entry.iterator->value();
            next_entry.sequence_number = entry.iterator->sequenceNumber();
            next_entry.entry_type = entry.iterator->entryType();
            next_entry.xmin = entry.iterator->xmin();
            next_entry.xmax = entry.iterator->xmax();
            next_entry.iterator = entry.iterator;
            next_entry.sstable_index = entry.sstable_index;

            pq.push(next_entry);
        }
    }

    // Step 5: Finish writing output SSTable
    s = writer.finish(ctx);
    if (s != Status::OK)
    {
        writer.close(ctx);
        return s;
    }

    s = writer.close(ctx);
    if (s != Status::OK)
    {
        return s;
    }

    // Log compaction statistics using structured logging
    LOG_INFO(STORAGE, "LSM k-way merge complete: %lu entries written, %lu duplicates skipped, %lu garbage collected",
             entries_written, entries_skipped_duplicate, entries_skipped_gc);

    return Status::OK;
}

Status LSMCompactionManager::replaceSSTablesAtomic(
    uint32_t level,
    const std::vector<std::string> &old_sstables,
    const std::vector<std::string> &new_sstables,
    ErrorContext *ctx)
{
    if (level >= levels_.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Invalid level");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Remove old SSTables from level
    auto &sstables = levels_[level].sstable_paths;
    for (const auto &old_path : old_sstables)
    {
        auto it = std::find(sstables.begin(), sstables.end(), old_path);
        if (it != sstables.end())
        {
            sstables.erase(it);

            // Update size (approximate)
            struct stat st;
            if (stat(old_path.c_str(), &st) == 0)
            {
                levels_[level].size_bytes -= st.st_size;
            }
        }
    }

    // Add new SSTables to level
    for (const auto &new_path : new_sstables)
    {
        sstables.push_back(new_path);

        // Update size
        struct stat st;
        if (stat(new_path.c_str(), &st) == 0)
        {
            levels_[level].size_bytes += st.st_size;
        }
    }

    return Status::OK;
}

} // namespace core
} // namespace scratchbird
