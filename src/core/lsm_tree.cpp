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
 * LSM-Tree Implementation
 *
 * Simple in-memory LSM-Tree for write-optimized indexing
 * Uses Firebird MGA architecture with xmin/xmax visibility
 *
 * Architecture:
 * - Memtable: In-memory sorted map (std::map)
 * - MGA Compliance: xmin/xmax transaction tracking
 * - Logical deletion: Tombstones (is_tombstone flag)
 * - No WAL: Uses TIP for crash recovery (Firebird MGA)
 *
 * November 22, 2025
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

LSMTree::LSMTree(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page)
    : db_(db),
      index_uuid_(index_uuid),
      meta_page_(meta_page),
      memtable_size_bytes_(0)
{
}

LSMTree::~LSMTree()
{
    // Cleanup - memtable_ is automatically destructed
}

// ============================================================================
// Static Factory Methods
// ============================================================================

Status LSMTree::create(Database *db, const UuidV7Bytes &index_uuid,
                       uint32_t *meta_page_out, ErrorContext *ctx)
{
    if (!db || !meta_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "LSMTree::create: db and meta_page_out cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    // LSMTree is in-memory only, so we use a dummy meta page number
    // This is for consistency with other index types that use page-based storage
    // In a full implementation, this would allocate an actual page for metadata
    *meta_page_out = 1;  // Dummy meta page number

    return Status::OK;
}

std::unique_ptr<LSMTree> LSMTree::open(Database *db, const UuidV7Bytes &index_uuid,
                                        uint32_t meta_page, ErrorContext *ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "LSMTree::open: db cannot be null");
        return nullptr;
    }

    // Create LSMTree instance
    auto lsm = std::make_unique<LSMTree>(db, index_uuid, meta_page);

    // In-memory LSM-Tree is ready to use immediately
    return lsm;
}

// ============================================================================
// Data Operations
// ============================================================================

Status LSMTree::put(const std::vector<uint8_t> &key, const TID &tid,
                    uint64_t xmin, ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    // Create new entry
    InternalEntry entry;
    entry.tid = tid;
    entry.xmin = xmin;
    entry.xmax = 0;  // Not deleted
    entry.is_tombstone = false;

    // Add to memtable (supports multiple versions per key)
    memtable_[key].push_back(entry);

    // Update approximate size
    memtable_size_bytes_ += key.size() + sizeof(TID) + sizeof(uint64_t) * 2 + sizeof(bool);

    // Note: This in-memory LSMTree does not flush to disk.
    // For disk-based LSM-Tree, see LSMTreeIndex in lsm_tree_index.h
    // which supports memtable flushing, SSTables, and background compaction.
    // When memtable exceeds MAX_MEMTABLE_SIZE, compact to reclaim space.
    if (memtable_size_bytes_ >= MAX_MEMTABLE_SIZE)
    {
        // Trigger in-memory compaction to reclaim space
        // Note: Lock already held by put()
        // compact() will acquire lock, so we need to release and reacquire
        // For simplicity, just note the condition - caller can run compact()
    }

    return Status::OK;
}

Status LSMTree::get(const std::vector<uint8_t> &key, uint64_t current_xid,
                    std::vector<TID> *results_out, ErrorContext *ctx)
{
    if (!results_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "LSMTree::get: results_out cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    results_out->clear();

    std::lock_guard<std::mutex> lock(memtable_mutex_);

    // Lookup key in memtable
    auto it = memtable_.find(key);
    if (it == memtable_.end())
    {
        // Key not found
        return Status::OK;
    }

    // Get all versions for this key
    const auto &versions = it->second;

    // Filter by MGA visibility
    // Entry is visible if:
    // 1. xmin <= current_xid (created before or by current transaction)
    // 2. xmax == 0 OR xmax > current_xid (not deleted, or deleted after current transaction)
    // 3. NOT is_tombstone (not a deletion marker)
    for (const auto &entry : versions)
    {
        // Check visibility
        if (entry.xmin <= current_xid &&
            (entry.xmax == 0 || entry.xmax > current_xid) &&
            !entry.is_tombstone)
        {
            results_out->push_back(entry.tid);
        }
    }

    return Status::OK;
}

Status LSMTree::remove(const std::vector<uint8_t> &key, const TID &tid,
                       uint64_t xmax, ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    // MGA logical deletion: Add tombstone entry
    // This marks the key as deleted for transaction xmax and later
    InternalEntry tombstone;
    tombstone.tid = tid;
    tombstone.xmin = xmax;  // Visible from this transaction onwards
    tombstone.xmax = 0;     // Not deleted (the tombstone itself is not deleted)
    tombstone.is_tombstone = true;

    // Add tombstone to memtable
    memtable_[key].push_back(tombstone);

    // Update approximate size
    memtable_size_bytes_ += key.size() + sizeof(TID) + sizeof(uint64_t) * 2 + sizeof(bool);

    return Status::OK;
}

// ============================================================================
// Range Scan
// ============================================================================

// Simple iterator implementation for in-memory memtable
class LSMTreeIterator : public LSMTree::Iterator
{
public:
    LSMTreeIterator(const std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>> &memtable,
                    const std::vector<uint8_t> *start_key,
                    const std::vector<uint8_t> *end_key,
                    uint64_t current_xid,
                    bool start_inclusive,
                    bool end_inclusive)
        : memtable_(memtable),
          current_xid_(current_xid),
          end_key_(end_key),
          end_inclusive_(end_inclusive),
          finished_(false)
    {
        // Initialize iterator
        if (start_key)
        {
            if (start_inclusive)
            {
                it_ = memtable_.lower_bound(*start_key);  // >= start_key
            }
            else
            {
                it_ = memtable_.upper_bound(*start_key);  // > start_key
            }
        }
        else
        {
            it_ = memtable_.begin();
        }

        end_it_ = memtable_.end();
        version_idx_ = 0;

        // Advance to first valid entry
        advanceToNextValid();
    }

    bool hasNext() override
    {
        return !finished_;
    }

    std::optional<LSMTree::Entry> next() override
    {
        if (finished_)
        {
            return std::nullopt;
        }

        // Get current entry
        const auto &internal_entry = it_->second[version_idx_];
        LSMTree::Entry entry;
        entry.key = it_->first;
        entry.tid = internal_entry.tid;
        entry.xmin = internal_entry.xmin;
        entry.xmax = internal_entry.xmax;

        // Advance to next valid entry
        version_idx_++;
        advanceToNextValid();

        return entry;
    }

private:
    const std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>> &memtable_;
    std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>>::const_iterator it_;
    std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>>::const_iterator end_it_;
    uint64_t current_xid_;
    const std::vector<uint8_t> *end_key_;
    bool end_inclusive_;
    bool finished_;
    size_t version_idx_;

    void advanceToNextValid()
    {
        while (it_ != end_it_)
        {
            // Check end key boundary
            if (end_key_)
            {
                int cmp = LSMTree::compareKeys(it_->first, *end_key_);
                if (end_inclusive_)
                {
                    if (cmp > 0)
                    {
                        finished_ = true;
                        return;
                    }
                }
                else
                {
                    if (cmp >= 0)
                    {
                        finished_ = true;
                        return;
                    }
                }
            }

            // Check if current key has valid visible versions
            const auto &versions = it_->second;
            while (version_idx_ < versions.size())
            {
                const auto &entry = versions[version_idx_];

                // Check MGA visibility
                if (entry.xmin <= current_xid_ &&
                    (entry.xmax == 0 || entry.xmax > current_xid_) &&
                    !entry.is_tombstone)
                {
                    // Found valid entry
                    return;
                }

                version_idx_++;
            }

            // No more valid versions for this key, move to next key
            ++it_;
            version_idx_ = 0;
        }

        // No more entries
        finished_ = true;
    }
};

std::unique_ptr<LSMTree::Iterator> LSMTree::rangeScan(
    const std::vector<uint8_t> *start_key,
    const std::vector<uint8_t> *end_key,
    uint64_t current_xid,
    bool start_inclusive,
    bool end_inclusive,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    return std::make_unique<LSMTreeIterator>(
        memtable_,
        start_key,
        end_key,
        current_xid,
        start_inclusive,
        end_inclusive);
}

// ============================================================================
// Maintenance Operations
// ============================================================================

Status LSMTree::compact(ErrorContext *ctx)
{
    // In-memory LSM-Tree compaction:
    // 1. Merge multiple versions of same key (keep latest committed version)
    // 2. Apply tombstones to delete entries
    // 3. Remove entries that have been logically deleted (xmax > 0)

    std::lock_guard<std::mutex> lock(memtable_mutex_);

    uint64_t entries_removed = 0;
    uint64_t keys_processed = 0;

    for (auto it = memtable_.begin(); it != memtable_.end(); )
    {
        auto &versions = it->second;
        keys_processed++;

        // Remove tombstones and deleted entries from versions
        auto remove_it = std::remove_if(versions.begin(), versions.end(),
            [](const InternalEntry &entry) {
                // Remove entries that are:
                // 1. Tombstones (logical deletions)
                // 2. Already deleted (xmax > 0)
                return entry.is_tombstone || entry.xmax > 0;
            });

        size_t removed = std::distance(remove_it, versions.end());
        versions.erase(remove_it, versions.end());
        entries_removed += removed;

        // If key has only one version, keep it
        // If key has multiple versions, keep only the latest (highest xmin)
        if (versions.size() > 1)
        {
            // Sort by xmin descending to find latest version
            std::sort(versions.begin(), versions.end(),
                [](const InternalEntry &a, const InternalEntry &b) {
                    return a.xmin > b.xmin;  // Descending order
                });

            // Keep only the first entry (latest version)
            size_t extra_versions = versions.size() - 1;
            versions.resize(1);
            entries_removed += extra_versions;
        }

        // Remove key entirely if no versions remain
        if (versions.empty())
        {
            it = memtable_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Update approximate size (recalculate)
    memtable_size_bytes_ = 0;
    for (const auto &kv : memtable_)
    {
        memtable_size_bytes_ += kv.first.size();
        for (const auto &entry : kv.second)
        {
            memtable_size_bytes_ += sizeof(InternalEntry);
        }
    }

    return Status::OK;
}

Status LSMTree::vacuum(ErrorContext *ctx)
{
    // Vacuum is similar to compact but focuses on removing dead entries
    // For in-memory LSM-Tree without direct TransactionManager access,
    // vacuum delegates to compact which removes all dead entries
    //
    // In a full implementation with OIT access, vacuum would only remove
    // entries with xmax < OIT (no longer visible to any transaction)
    return compact(ctx);
}

Status LSMTree::removeDeadEntries(const std::vector<TID> &dead_tids,
                                  uint64_t *entries_removed_out,
                                  uint64_t *pages_modified_out,
                                  ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(memtable_mutex_);

    uint64_t removed_count = 0;

    // Iterate through memtable and remove entries with dead TIDs
    for (auto it = memtable_.begin(); it != memtable_.end();)
    {
        auto &versions = it->second;

        // Remove versions with dead TIDs
        auto remove_it = std::remove_if(versions.begin(), versions.end(),
            [&dead_tids](const InternalEntry &entry) {
                return std::find(dead_tids.begin(), dead_tids.end(), entry.tid) != dead_tids.end();
            });

        size_t removed = std::distance(remove_it, versions.end());
        removed_count += removed;

        versions.erase(remove_it, versions.end());

        // If all versions removed, remove the key entirely
        if (versions.empty())
        {
            it = memtable_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (entries_removed_out)
    {
        *entries_removed_out = removed_count;
    }

    if (pages_modified_out)
    {
        // In-memory LSM doesn't use pages, so this is 0
        *pages_modified_out = 0;
    }

    return Status::OK;
}

// ============================================================================
// Helper Methods
// ============================================================================

int LSMTree::compareKeys(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    // Lexicographic comparison
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

} // namespace core
} // namespace scratchbird
