// =================================================================================================
// ScratchBird Database Engine
// LSM-Tree Index Implementation
// November 20, 2025 - Production Implementation
// =================================================================================================

#include "scratchbird/core/lsm_tree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include <algorithm>
#include <cstring>

namespace scratchbird {
namespace core {

    // Range scan iterator implementation
    class LSMRangeScanIterator : public LSMTree::Iterator
    {
    public:
        LSMRangeScanIterator(const std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>>& memtable,
                             const std::vector<uint8_t>* start_key,
                             const std::vector<uint8_t>* end_key,
                             uint64_t current_xid,
                             bool start_inclusive,
                             bool end_inclusive,
                             Database* db)
            : memtable_(memtable)
            , start_key_(start_key ? *start_key : std::vector<uint8_t>())
            , end_key_(end_key ? *end_key : std::vector<uint8_t>())
            , current_xid_(current_xid)
            , start_inclusive_(start_inclusive)
            , end_inclusive_(end_inclusive)
            , db_(db)
            , has_start_(start_key != nullptr)
            , has_end_(end_key != nullptr)
        {
            // Position iterator at start
            if (has_start_)
            {
                it_ = start_inclusive_ ? memtable_.lower_bound(start_key_)
                                       : memtable_.upper_bound(start_key_);
            }
            else
            {
                it_ = memtable_.begin();
            }

            // Find first valid entry
            advance();
        }

        bool hasNext() override
        {
            return current_entry_.has_value();
        }

        std::optional<LSMTree::Entry> next() override
        {
            if (!current_entry_)
            {
                return std::nullopt;
            }

            auto result = current_entry_;
            advance();
            return result;
        }

    private:
        void advance()
        {
            current_entry_ = std::nullopt;

            while (it_ != memtable_.end())
            {
                const auto& key = it_->first;

                // Check if past end key
                if (has_end_)
                {
                    int cmp = compareKeys(key, end_key_);
                    if (cmp > 0 || (cmp == 0 && !end_inclusive_))
                    {
                        return; // Past end
                    }
                }

                // Find visible entry in this key's version list
                for (const auto& internal : it_->second)
                {
                    if (internal.is_tombstone)
                    {
                        continue; // Skip tombstones
                    }

                    // Check visibility using TIP
                    auto txn_mgr = db_->transaction_manager();
                    if (txn_mgr)
                    {
                        bool xmin_visible = txn_mgr->isVersionVisible(internal.xmin, current_xid_);
                        bool xmax_visible = (internal.xmax == 0) ? false
                                          : txn_mgr->isVersionVisible(internal.xmax, current_xid_);

                        if (xmin_visible && !xmax_visible)
                        {
                            // Found visible entry
                            current_entry_ = LSMTree::Entry{
                                key,
                                internal.tid,
                                internal.xmin,
                                internal.xmax
                            };
                            ++it_;
                            return;
                        }
                    }
                }

                ++it_;
            }
        }

        static int compareKeys(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
        {
            size_t min_len = std::min(a.size(), b.size());
            int cmp = std::memcmp(a.data(), b.data(), min_len);
            if (cmp != 0) return cmp;
            if (a.size() < b.size()) return -1;
            if (a.size() > b.size()) return 1;
            return 0;
        }

        const std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>>& memtable_;
        std::map<std::vector<uint8_t>, std::vector<LSMTree::InternalEntry>>::const_iterator it_;
        std::vector<uint8_t> start_key_;
        std::vector<uint8_t> end_key_;
        uint64_t current_xid_;
        bool start_inclusive_;
        bool end_inclusive_;
        Database* db_;
        bool has_start_;
        bool has_end_;
        std::optional<LSMTree::Entry> current_entry_;
    };

    // =================================================================================================
    // LSMTree Implementation
    // =================================================================================================

    LSMTree::LSMTree(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page)
        : db_(db), index_uuid_(index_uuid), meta_page_(meta_page), memtable_size_bytes_(0)
    {
    }

    LSMTree::~LSMTree() = default;

    Status LSMTree::create(Database *db, const UuidV7Bytes &index_uuid,
                           uint32_t *meta_page_out, ErrorContext *ctx)
    {
        // Allocate meta page
        auto page_mgr = db->page_manager();
        uint32_t meta_page = page_mgr->allocatePage();

        if (meta_page_out)
        {
            *meta_page_out = meta_page;
        }

        return Status::OK;
    }

    std::unique_ptr<LSMTree> LSMTree::open(Database *db, const UuidV7Bytes &index_uuid,
                                           uint32_t meta_page, ErrorContext *ctx)
    {
        return std::make_unique<LSMTree>(db, index_uuid, meta_page);
    }

    Status LSMTree::put(const std::vector<uint8_t> &key, const TID &tid,
                        uint64_t xmin, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        // Create new entry
        InternalEntry entry;
        entry.tid = tid;
        entry.xmin = xmin;
        entry.xmax = 0; // Not deleted
        entry.is_tombstone = false;

        // Add to memtable
        auto& entries = memtable_[key];
        entries.push_back(entry);

        // Update approximate size
        memtable_size_bytes_ += key.size() + sizeof(InternalEntry);

        // TODO: Flush to SSTable when memtable exceeds threshold
        // For now, simple in-memory implementation

        return Status::OK;
    }

    Status LSMTree::get(const std::vector<uint8_t> &key, uint64_t current_xid,
                        std::vector<TID> *results_out, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        if (!results_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "results_out is null");
            return Status::INVALID_ARGUMENT;
        }

        results_out->clear();

        // Search in memtable
        auto it = memtable_.find(key);
        if (it == memtable_.end())
        {
            return Status::OK; // Key not found
        }

        // Find visible entries using TIP-based visibility (Firebird MGA)
        auto txn_mgr = db_->transaction_manager();
        for (const auto& entry : it->second)
        {
            if (entry.is_tombstone)
            {
                continue; // Skip tombstones
            }

            // Check visibility
            if (txn_mgr)
            {
                bool xmin_visible = txn_mgr->isVersionVisible(entry.xmin, current_xid);
                bool xmax_visible = (entry.xmax == 0) ? false
                                  : txn_mgr->isVersionVisible(entry.xmax, current_xid);

                if (xmin_visible && !xmax_visible)
                {
                    results_out->push_back(entry.tid);
                }
            }
            else
            {
                // No transaction manager - return all entries
                results_out->push_back(entry.tid);
            }
        }

        return Status::OK;
    }

    Status LSMTree::remove(const std::vector<uint8_t> &key, const TID &tid,
                           uint64_t xmax, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        // MGA logical deletion: mark entry with tombstone
        auto it = memtable_.find(key);
        if (it != memtable_.end())
        {
            // Find matching TID and mark with xmax
            for (auto& entry : it->second)
            {
                if (entry.tid == tid && entry.xmax == 0)
                {
                    entry.xmax = xmax;
                    entry.is_tombstone = true;
                    return Status::OK;
                }
            }
        }

        // If not found in memtable, write tombstone anyway
        // (entry might be in SSTable)
        InternalEntry tombstone;
        tombstone.tid = tid;
        tombstone.xmin = 0;
        tombstone.xmax = xmax;
        tombstone.is_tombstone = true;

        memtable_[key].push_back(tombstone);
        memtable_size_bytes_ += key.size() + sizeof(InternalEntry);

        return Status::OK;
    }

    std::unique_ptr<LSMTree::Iterator> LSMTree::rangeScan(const std::vector<uint8_t> *start_key,
                                                           const std::vector<uint8_t> *end_key,
                                                           uint64_t current_xid,
                                                           bool start_inclusive,
                                                           bool end_inclusive,
                                                           ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        return std::make_unique<LSMRangeScanIterator>(
            memtable_, start_key, end_key, current_xid,
            start_inclusive, end_inclusive, db_);
    }

    Status LSMTree::compact(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        // Basic compaction: remove tombstone entries older than active transactions
        auto txn_mgr = db_->transaction_manager();
        if (!txn_mgr)
        {
            return Status::OK; // No transaction manager, skip compaction
        }

        uint64_t oldest_xid = txn_mgr->getOldestActiveTransactionId();

        for (auto it = memtable_.begin(); it != memtable_.end(); )
        {
            auto& entries = it->second;

            // Remove tombstones and deleted entries older than oldest active transaction
            entries.erase(
                std::remove_if(entries.begin(), entries.end(),
                    [oldest_xid](const InternalEntry& e) {
                        return (e.is_tombstone && e.xmax < oldest_xid) ||
                               (e.xmax != 0 && e.xmax < oldest_xid);
                    }),
                entries.end()
            );

            // Remove key if no entries left
            if (entries.empty())
            {
                it = memtable_.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // Recalculate approximate size
        memtable_size_bytes_ = 0;
        for (const auto& kv : memtable_)
        {
            memtable_size_bytes_ += kv.first.size() + kv.second.size() * sizeof(InternalEntry);
        }

        return Status::OK;
    }

    Status LSMTree::vacuum(ErrorContext *ctx)
    {
        // Vacuum is same as compaction for simple LSM implementation
        return compact(ctx);
    }

    Status LSMTree::removeDeadEntries(const std::vector<TID> &dead_tids,
                                      uint64_t *entries_removed_out,
                                      uint64_t *pages_modified_out,
                                      ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(memtable_mutex_);

        uint64_t removed = 0;

        // Remove entries matching dead TIDs
        for (auto& kv : memtable_)
        {
            auto& entries = kv.second;
            size_t before = entries.size();

            entries.erase(
                std::remove_if(entries.begin(), entries.end(),
                    [&dead_tids](const InternalEntry& e) {
                        return std::find(dead_tids.begin(), dead_tids.end(), e.tid) != dead_tids.end();
                    }),
                entries.end()
            );

            removed += (before - entries.size());
        }

        // Remove empty keys
        for (auto it = memtable_.begin(); it != memtable_.end(); )
        {
            if (it->second.empty())
            {
                it = memtable_.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (entries_removed_out) *entries_removed_out = removed;
        if (pages_modified_out) *pages_modified_out = 0; // In-memory only

        return Status::OK;
    }

    int LSMTree::compareKeys(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
    {
        size_t min_len = std::min(a.size(), b.size());
        int cmp = std::memcmp(a.data(), b.data(), min_len);
        if (cmp != 0) return cmp;
        if (a.size() < b.size()) return -1;
        if (a.size() > b.size()) return 1;
        return 0;
    }

} // namespace core
} // namespace scratchbird
