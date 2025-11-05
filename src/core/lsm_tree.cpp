/**
 * LSM-Tree (Log-Structured Merge-Tree) Implementation
 *
 * See /MGA_RULES.md for Firebird MGA compliance requirements
 */

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Memtable Implementation
// ============================================================================

Memtable::Memtable(size_t max_size_bytes)
    : next_sequence_(0),
      current_size_(0),
      max_size_(max_size_bytes),
      is_full_(false)
{
}

Memtable::~Memtable()
{
}

Status Memtable::put(const std::vector<uint8_t> &key,
                     const std::vector<uint8_t> &value,
                     uint64_t current_xid,
                     ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Create entry
    MemtableEntry entry;
    entry.key = key;
    entry.value = value;
    entry.sequence_number = next_sequence_++;
    entry.entry_type = ENTRY_TYPE_INSERT;
    entry.xmin = current_xid;  // MGA: Transaction that created this entry
    entry.xmax = 0;            // MGA: Not deleted

    // Calculate size
    size_t entry_size = entry.getSize();

    // Check if adding this entry would exceed max size
    if (current_size_ + entry_size > max_size_)
    {
        is_full_ = true;  // Mark as full
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Memtable full");
        return Status::OOM;
    }

    // Insert into Red-Black Tree
    entries_[entry] = true;

    // Update size
    current_size_ += entry_size;

    return Status::OK;
}

Status Memtable::remove(const std::vector<uint8_t> &key,
                        uint64_t current_xid,
                        ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Create tombstone entry
    MemtableEntry entry;
    entry.key = key;
    entry.value = {};  // Empty value for tombstone
    entry.sequence_number = next_sequence_++;
    entry.entry_type = ENTRY_TYPE_DELETE;  // Tombstone
    entry.xmin = current_xid;              // MGA: Transaction that created tombstone
    entry.xmax = 0;                        // MGA: Not deleted

    // Calculate size
    size_t entry_size = entry.getSize();

    // Check if adding this entry would exceed max size
    if (current_size_ + entry_size > max_size_)
    {
        is_full_ = true;  // Mark as full
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Memtable full");
        return Status::OOM;
    }

    // Insert tombstone into Red-Black Tree
    entries_[entry] = true;

    // Update size
    current_size_ += entry_size;

    return Status::OK;
}

Status Memtable::get(const std::vector<uint8_t> &key,
                     uint64_t current_xid,
                     TransactionManager *txn_mgr,
                     std::vector<uint8_t> *value_out,
                     bool *found,
                     ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    *found = false;

    // Create search key with max sequence (to start iteration from newest)
    MemtableEntry search_key;
    search_key.key = key;
    search_key.sequence_number = UINT64_MAX;  // Start from newest

    // Find first entry with this key (newest version due to operator<)
    auto it = entries_.lower_bound(search_key);

    // Iterate through all versions of this key (newest to oldest)
    while (it != entries_.end() && it->first.key == key)
    {
        const MemtableEntry &entry = it->first;

        // Check MGA visibility (Firebird rules)
        if (isEntryVisible(entry, current_xid, txn_mgr))
        {
            if (entry.entry_type == ENTRY_TYPE_INSERT)
            {
                // Found visible insert - return value
                *value_out = entry.value;
                *found = true;
                return Status::OK;
            }
            else if (entry.entry_type == ENTRY_TYPE_DELETE)
            {
                // Found visible tombstone - key is deleted
                *found = false;
                return Status::OK;
            }
        }

        ++it;
    }

    // Key not found or no visible version
    *found = false;
    return Status::OK;
}

Status Memtable::scan(const std::vector<uint8_t> *start_key,
                      const std::vector<uint8_t> *end_key,
                      uint64_t current_xid,
                      TransactionManager *txn_mgr,
                      std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *entries_out,
                      ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    entries_out->clear();

    // Determine iteration range
    auto it_start = entries_.begin();
    auto it_end = entries_.end();

    if (start_key != nullptr)
    {
        MemtableEntry search_key;
        search_key.key = *start_key;
        search_key.sequence_number = UINT64_MAX;  // Start from newest
        it_start = entries_.lower_bound(search_key);
    }

    // Track last key to skip duplicate versions
    std::vector<uint8_t> last_key;
    bool last_key_set = false;

    // Iterate through range
    for (auto it = it_start; it != it_end; ++it)
    {
        const MemtableEntry &entry = it->first;

        // Check end_key boundary
        if (end_key != nullptr && entry.key > *end_key)
        {
            break;
        }

        // Skip duplicate keys (we already processed a newer version)
        if (last_key_set && entry.key == last_key)
        {
            continue;
        }

        // Check MGA visibility
        if (isEntryVisible(entry, current_xid, txn_mgr))
        {
            if (entry.entry_type == ENTRY_TYPE_INSERT)
            {
                // Add to results
                entries_out->push_back({entry.key, entry.value});
            }
            // For DELETE entries, just skip (key is deleted)

            // Mark key as processed
            last_key = entry.key;
            last_key_set = true;
        }
    }

    return Status::OK;
}

bool Memtable::isFull() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return is_full_ || current_size_ >= max_size_;
}

size_t Memtable::getSize() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return current_size_;
}

Status Memtable::getAllEntries(std::vector<MemtableEntry> *entries_out,
                               ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    entries_out->clear();
    entries_out->reserve(entries_.size());

    // Copy all entries (already in sorted order from std::map)
    for (const auto &pair : entries_)
    {
        entries_out->push_back(pair.first);
    }

    return Status::OK;
}

size_t Memtable::getNumEntries() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

bool Memtable::isEntryVisible(const MemtableEntry &entry,
                              uint64_t current_xid,
                              TransactionManager *txn_mgr) const
{
    // FIREBIRD MGA VISIBILITY RULES
    // See /MGA_RULES.md for complete specification

    // Rule 1: Own changes always visible
    if (entry.xmin == current_xid)
    {
        return entry.xmax == 0 || entry.xmax != current_xid;
    }

    // Rule 2: Check if creator transaction is committed
    // Use TIP-based visibility (NO PostgreSQL Snapshot!)
    TransactionState xmin_state;
    Status status = txn_mgr->getTransactionState(entry.xmin, xmin_state, nullptr);
    if (status != Status::OK || xmin_state != TransactionState::COMMITTED)
    {
        // Not committed = not visible
        return false;
    }

    // Rule 3: Creator must be older than current transaction
    if (entry.xmin >= current_xid)
    {
        // Future transaction = not visible
        return false;
    }

    // Rule 4: Check if deleted
    if (entry.xmax != 0)
    {
        // Entry was deleted - check if deletion is visible
        if (entry.xmax == current_xid)
        {
            // Deleted by us = not visible
            return false;
        }

        TransactionState xmax_state;
        status = txn_mgr->getTransactionState(entry.xmax, xmax_state, nullptr);
        if (status == Status::OK && xmax_state == TransactionState::COMMITTED && entry.xmax < current_xid)
        {
            // Deletion committed and older = not visible
            return false;
        }
    }

    // Entry is visible
    return true;
}

} // namespace core
} // namespace scratchbird
