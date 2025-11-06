/**
 * LSM-Tree Compaction Implementation
 *
 * CRITICAL MGA COMPLIANCE:
 * - See /MGA_RULES.md before modifying this file
 * - Uses TransactionId (uint64_t), NOT Snapshot
 * - TIP-based visibility via TransactionManager
 * - Garbage collection based on OIT (Oldest Interesting Transaction)
 *
 * Compaction Strategy:
 * - Leveled compaction (4 levels: 0-3)
 * - Level 0: 4 SSTables (32 MB total)
 * - Level 1: 10 SSTables (80 MB total)
 * - Level 2: 100 SSTables (800 MB total)
 * - Level 3: 1000 SSTables (8 GB total)
 *
 * K-Way Merge:
 * - Use priority queue to merge sorted SSTables
 * - Deduplication: Keep newest version (highest sequence number)
 * - Tombstone removal: Discard DELETE entries
 * - Garbage collection: Remove entries invisible to all active transactions
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/transaction_manager.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <chrono>

namespace scratchbird
{
namespace core
{

/**
 * Constructor
 */
LSMCompactionManager::LSMCompactionManager(TransactionManager *txn_mgr)
    : txn_mgr_(txn_mgr)
{
    // Initialize 4 levels with exponentially increasing size limits
    levels_.resize(4);
    levels_[0] = LevelMetadata(0, 32ULL * 1024 * 1024);    // Level 0: 32 MB (4 x 8MB files)
    levels_[1] = LevelMetadata(1, 80ULL * 1024 * 1024);    // Level 1: 80 MB (10 x 8MB files)
    levels_[2] = LevelMetadata(2, 800ULL * 1024 * 1024);   // Level 2: 800 MB (100 x 8MB files)
    levels_[3] = LevelMetadata(3, 8192ULL * 1024 * 1024);  // Level 3: 8 GB (1000 x 8MB files)
}

/**
 * Destructor
 */
LSMCompactionManager::~LSMCompactionManager()
{
}

/**
 * Initialize
 */
Status LSMCompactionManager::initialize(ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Reset all levels
    for (auto &level : levels_)
    {
        level.size_bytes = 0;
        level.sstable_paths.clear();
    }

    return Status::OK;
}

/**
 * Add SSTable to level
 */
Status LSMCompactionManager::addSSTable(uint32_t level,
                                         const std::string &sstable_path,
                                         uint64_t file_size,
                                         ErrorContext *ctx)
{
    if (level >= levels_.size())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         ("Invalid level: " + std::to_string(level)).c_str());
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    levels_[level].sstable_paths.push_back(sstable_path);
    levels_[level].size_bytes += file_size;

    return Status::OK;
}

/**
 * Check if compaction is needed
 */
bool LSMCompactionManager::needsCompaction() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Level 0: Trigger when we have 4+ SSTables (or exceed size limit)
    if (levels_[0].sstable_paths.size() >= 4 || levels_[0].size_bytes >= levels_[0].size_limit_bytes)
    {
        return true;
    }

    // Levels 1-3: Trigger when size exceeds limit
    for (size_t i = 1; i < levels_.size(); ++i)
    {
        if (levels_[i].size_bytes >= levels_[i].size_limit_bytes)
        {
            return true;
        }
    }

    return false;
}

/**
 * Select compaction task
 */
Status LSMCompactionManager::selectCompactionTask(CompactionTask *task, ErrorContext *ctx)
{
    if (!task)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "task is nullptr");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Priority 1: Level 0 → Level 1 compaction (most urgent)
    if (levels_[0].sstable_paths.size() >= 4 || levels_[0].size_bytes >= levels_[0].size_limit_bytes)
    {
        task->source_level = 0;
        task->target_level = 1;
        task->source_sstables = levels_[0].sstable_paths;

        // Find overlapping SSTables in Level 1
        // For Level 0, we need to check all files since they may overlap
        std::vector<uint8_t> global_min_key;
        std::vector<uint8_t> global_max_key;

        // Read min/max keys from all Level 0 SSTables
        for (const auto &path : task->source_sstables)
        {
            SSTableReader reader(path);
            Status status = reader.open(ctx);
            if (status != Status::OK)
            {
                continue;
            }

            std::vector<uint8_t> min_key = reader.getMinKey();
            std::vector<uint8_t> max_key = reader.getMaxKey();

            if (global_min_key.empty() || min_key < global_min_key)
            {
                global_min_key = min_key;
            }
            if (global_max_key.empty() || max_key > global_max_key)
            {
                global_max_key = max_key;
            }
        }

        // Find overlapping SSTables in Level 1
        findOverlappingSSTables(1, global_min_key, global_max_key, &task->overlapping_sstables);

        // Get OIT (Oldest Interesting Transaction) for Firebird MGA garbage collection
        // Per Firebird spec: "Record versions created by transactions < OIT can be garbage collected"
        task->oit = txn_mgr_->getOldestXid();

        return Status::OK;
    }

    // Priority 2: Level N → Level N+1 compaction
    for (size_t i = 1; i < levels_.size() - 1; ++i)
    {
        if (levels_[i].size_bytes >= levels_[i].size_limit_bytes)
        {
            task->source_level = i;
            task->target_level = i + 1;

            // For leveled compaction, pick one SSTable from source level
            // In a real implementation, this would be more sophisticated
            if (!levels_[i].sstable_paths.empty())
            {
                task->source_sstables.push_back(levels_[i].sstable_paths[0]);

                // Read min/max keys
                SSTableReader reader(task->source_sstables[0]);
                Status status = reader.open(ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                std::vector<uint8_t> min_key = reader.getMinKey();
                std::vector<uint8_t> max_key = reader.getMaxKey();

                // Find overlapping SSTables in target level
                findOverlappingSSTables(task->target_level, min_key, max_key, &task->overlapping_sstables);

                // Get OIT for MGA garbage collection
                task->oit = txn_mgr_->getOldestXid();

                return Status::OK;
            }
        }
    }

    // No compaction needed
    return Status::FILE_NOT_FOUND;
}

/**
 * Execute compaction
 */
Status LSMCompactionManager::executeCompaction(const CompactionTask &task, ErrorContext *ctx)
{
    // Generate output SSTable path
    std::string output_path = "lsm_level" + std::to_string(task.target_level) +
                             "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                             ".sst";

    // Combine source SSTables and overlapping SSTables
    std::vector<std::string> all_inputs = task.source_sstables;
    all_inputs.insert(all_inputs.end(), task.overlapping_sstables.begin(), task.overlapping_sstables.end());

    // Perform K-way merge with MGA garbage collection using OIT
    Status status = kWayMerge(all_inputs, output_path, task.oit, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Get output file size
    uint64_t output_size = std::filesystem::file_size(output_path);

    // Atomically replace old SSTables with new one
    std::vector<std::string> new_sstables = {output_path};
    status = replaceSSTablesAtomic(task.target_level, all_inputs, new_sstables, ctx);
    if (status != Status::OK)
    {
        // Clean up output file on error
        std::remove(output_path.c_str());
        return status;
    }

    // Update level metadata
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Remove from source level
        auto &source_level = levels_[task.source_level];
        for (const auto &path : task.source_sstables)
        {
            auto it = std::find(source_level.sstable_paths.begin(), source_level.sstable_paths.end(), path);
            if (it != source_level.sstable_paths.end())
            {
                source_level.sstable_paths.erase(it);
                // Approximate size reduction (we don't store individual sizes)
                uint64_t file_size = std::filesystem::file_size(path);
                source_level.size_bytes -= file_size;
            }
        }

        // Remove overlapping from target level
        auto &target_level = levels_[task.target_level];
        for (const auto &path : task.overlapping_sstables)
        {
            auto it = std::find(target_level.sstable_paths.begin(), target_level.sstable_paths.end(), path);
            if (it != target_level.sstable_paths.end())
            {
                target_level.sstable_paths.erase(it);
                uint64_t file_size = std::filesystem::file_size(path);
                target_level.size_bytes -= file_size;
            }
        }

        // Add new SSTable to target level
        target_level.sstable_paths.push_back(output_path);
        target_level.size_bytes += output_size;
    }

    // Delete old SSTable files
    deleteOldSSTables(all_inputs);

    return Status::OK;
}

/**
 * Get level metadata
 */
const LevelMetadata &LSMCompactionManager::getLevel(uint32_t level) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (level >= levels_.size())
    {
        static LevelMetadata empty;
        return empty;
    }

    return levels_[level];
}

/**
 * Get statistics
 */
void LSMCompactionManager::getStatistics(uint64_t *total_sstables, uint64_t *total_size_bytes) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    *total_sstables = 0;
    *total_size_bytes = 0;

    for (const auto &level : levels_)
    {
        *total_sstables += level.sstable_paths.size();
        *total_size_bytes += level.size_bytes;
    }
}

/**
 * Find overlapping SSTables in target level
 *
 * For Level 1+, SSTables do NOT overlap (leveled compaction invariant)
 * So we can use binary search on min/max keys
 */
void LSMCompactionManager::findOverlappingSSTables(uint32_t level,
                                                     const std::vector<uint8_t> &min_key,
                                                     const std::vector<uint8_t> &max_key,
                                                     std::vector<std::string> *overlapping)
{
    overlapping->clear();

    if (level >= levels_.size())
    {
        return;
    }

    const auto &level_sstables = levels_[level].sstable_paths;

    for (const auto &path : level_sstables)
    {
        SSTableReader reader(path);
        Status status = reader.open(nullptr);
        if (status != Status::OK)
        {
            continue;
        }

        std::vector<uint8_t> sstable_min = reader.getMinKey();
        std::vector<uint8_t> sstable_max = reader.getMaxKey();

        // Check if ranges overlap
        // Overlap if: sstable_min <= max_key AND sstable_max >= min_key
        if (sstable_min <= max_key && sstable_max >= min_key)
        {
            overlapping->push_back(path);
        }
    }
}

/**
 * K-way merge implementation
 *
 * CRITICAL: Firebird MGA compliance
 * - Uses TransactionId, NOT Snapshot
 * - Garbage collection based on OIT (Oldest Interesting Transaction)
 * - Per Firebird spec: "Record versions with xmax < OIT can be garbage collected"
 * - Deduplication: Keep newest version (highest sequence number)
 * - Tombstone removal: Discard DELETE entries when safe
 *
 * @param oit Oldest Interesting Transaction - boundary below which all transactions are committed
 */
Status LSMCompactionManager::kWayMerge(const std::vector<std::string> &source_sstables,
                                        const std::string &output_path,
                                        uint64_t oit,
                                        ErrorContext *ctx)
{
    // Open all source SSTables
    std::vector<std::unique_ptr<SSTableReader>> readers;
    for (const auto &path : source_sstables)
    {
        auto reader = std::make_unique<SSTableReader>(path);
        Status status = reader->open(ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, ("Failed to open SSTable: " + path).c_str());
            return status;
        }
        readers.push_back(std::move(reader));
    }

    // Create output SSTable writer
    SSTableWriter writer(output_path, 4096);
    Status status = writer.open(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Load all entries from all source SSTables
    std::vector<std::vector<MemtableEntry>> all_entries(readers.size());
    for (size_t i = 0; i < readers.size(); ++i)
    {
        status = readers[i]->loadAllEntries(&all_entries[i], ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    // Build priority queue for K-way merge (min-heap: smallest key first)
    std::priority_queue<MergeEntry, std::vector<MergeEntry>, std::greater<MergeEntry>> pq;

    // Initialize: Add first entry from each SSTable to priority queue
    for (size_t i = 0; i < all_entries.size(); ++i)
    {
        if (!all_entries[i].empty())
        {
            const auto &entry = all_entries[i][0];
            MergeEntry merge_entry;
            merge_entry.key = entry.key;
            merge_entry.value = entry.value;
            merge_entry.sequence_number = entry.sequence_number;
            merge_entry.entry_type = entry.entry_type;
            merge_entry.xmin = entry.xmin;
            merge_entry.xmax = entry.xmax;
            merge_entry.source_level = 0; // Will be set properly in selectCompactionTask
            merge_entry.source_index = i;
            merge_entry.entry_index = 0;
            pq.push(merge_entry);
        }
    }

    // K-way merge: Process entries in sorted order
    std::vector<uint8_t> last_key;
    uint64_t entries_written = 0;
    uint64_t entries_skipped_dup = 0;
    uint64_t entries_skipped_tombstone = 0;
    uint64_t entries_skipped_gc = 0;

    while (!pq.empty())
    {
        MergeEntry current = pq.top();
        pq.pop();

        // Deduplication: Skip duplicate keys (keep only first = newest version)
        if (!last_key.empty() && current.key == last_key)
        {
            entries_skipped_dup++;
        }
        else
        {
            // Garbage collection (MGA rules using OIT)
            if (canGarbageCollect(current, oit))
            {
                entries_skipped_gc++;
            }
            // Tombstone removal (if entry is DELETE and xmin < OIT, all transactions can see the delete)
            else if (current.entry_type == ENTRY_TYPE_DELETE && current.xmin < oit)
            {
                // Safe to remove tombstone if:
                // - The DELETE is committed
                // - All active transactions can see the delete
                TransactionState xmin_state;
                txn_mgr_->getTransactionState(current.xmin, xmin_state, nullptr);
                if (xmin_state == TransactionState::COMMITTED)
                {
                    entries_skipped_tombstone++;
                }
                else
                {
                    // Keep tombstone (DELETE not yet committed or visible)
                    status = writer.addEntry(current.key, current.value, current.sequence_number,
                                            current.entry_type, current.xmin, current.xmax, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    entries_written++;
                }
            }
            else
            {
                // Write entry to output
                status = writer.addEntry(current.key, current.value, current.sequence_number,
                                        current.entry_type, current.xmin, current.xmax, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                entries_written++;
            }

            last_key = current.key;
        }

        // Advance iterator: Add next entry from same source to priority queue
        size_t source_idx = current.source_index;
        size_t next_entry_idx = current.entry_index + 1;

        if (next_entry_idx < all_entries[source_idx].size())
        {
            const auto &entry = all_entries[source_idx][next_entry_idx];
            MergeEntry merge_entry;
            merge_entry.key = entry.key;
            merge_entry.value = entry.value;
            merge_entry.sequence_number = entry.sequence_number;
            merge_entry.entry_type = entry.entry_type;
            merge_entry.xmin = entry.xmin;
            merge_entry.xmax = entry.xmax;
            merge_entry.source_level = current.source_level;
            merge_entry.source_index = source_idx;
            merge_entry.entry_index = next_entry_idx;
            pq.push(merge_entry);
        }
    }

    // Finish writing output SSTable
    status = writer.finish(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Log compaction statistics
    std::cout << "[COMPACTION] Merged " << source_sstables.size() << " SSTables into " << output_path << "\n";
    std::cout << "  Entries written: " << entries_written << "\n";
    std::cout << "  Entries skipped (duplicate): " << entries_skipped_dup << "\n";
    std::cout << "  Entries skipped (tombstone): " << entries_skipped_tombstone << "\n";
    std::cout << "  Entries skipped (garbage collection): " << entries_skipped_gc << "\n";

    return Status::OK;
}

/**
 * Check if entry can be garbage collected (MGA rules)
 *
 * CRITICAL: Firebird MGA compliance
 * - Entry must be deleted (xmax != 0)
 * - Both xmin and xmax must be committed
 * - xmax < OIT (Oldest Interesting Transaction)
 *
 * Per Firebird spec:
 * "OIT = Oldest transaction NOT in committed state"
 * "Record versions created by transactions < OIT can be garbage collected"
 * "All back versions with xmax < OIT are candidates for removal"
 *
 * @param oit Oldest Interesting Transaction - boundary below which all transactions are committed
 */
bool LSMCompactionManager::canGarbageCollect(const MergeEntry &entry, uint64_t oit) const
{
    // Entry must be deleted (xmax != 0)
    if (entry.xmax == 0)
    {
        return false;
    }

    // Both xmin and xmax must be committed
    TransactionState xmin_state, xmax_state;
    txn_mgr_->getTransactionState(entry.xmin, xmin_state, nullptr);
    txn_mgr_->getTransactionState(entry.xmax, xmax_state, nullptr);

    if (xmin_state != TransactionState::COMMITTED || xmax_state != TransactionState::COMMITTED)
    {
        return false;
    }

    // Firebird MGA: Entry can be garbage collected if xmax < OIT
    // OIT is the boundary below which ALL transactions are committed
    // Therefore, no transaction can see versions with xmax < OIT
    if (entry.xmax < oit)
    {
        return true;
    }

    return false;
}

/**
 * Atomically replace old SSTables with new ones
 *
 * This is a simplified implementation. In production, we would use:
 * - Write-ahead manifest log
 * - Two-phase commit
 * - Crash recovery
 */
Status LSMCompactionManager::replaceSSTablesAtomic(uint32_t target_level,
                                                     const std::vector<std::string> &old_sstables,
                                                     const std::vector<std::string> &new_sstables,
                                                     ErrorContext *ctx)
{
    // For now, this is a no-op
    // In production, we would write to a manifest file
    return Status::OK;
}

/**
 * Delete old SSTable files
 */
void LSMCompactionManager::deleteOldSSTables(const std::vector<std::string> &sstable_paths)
{
    for (const auto &path : sstable_paths)
    {
        std::remove(path.c_str());
    }
}

} // namespace core
} // namespace scratchbird
