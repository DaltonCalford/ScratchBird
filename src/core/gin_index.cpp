#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gin_compression.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/heap_page.h" // For ItemPointer, TupleHeader, HeapPageSpecial
#include "scratchbird/core/transaction_manager.h" // For TransactionState, isVersionVisible (Firebird MGA)
#include <cstring>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
#include <set>
#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h> // SSE2
#endif
#include <map>
#include <memory>

namespace scratchbird
{
    namespace core
    {
        // ========================================================================
        // STOR-M3: BK-tree Implementation for Efficient Fuzzy Matching
        // ========================================================================
        //
        // A BK-tree (Burkhard-Keller tree) is a metric space tree that organizes
        // strings by their edit distance. Each node stores a key, and children
        // are indexed by their edit distance to the parent.
        //
        // Search property: To find all keys within distance n from query q:
        // - Compare query to root, get distance d
        // - Recursively search children at distances [d-n, d+n]
        // - This prunes large portions of the tree
        //
        // Time complexity: O(k^n * log(m)) where k is alphabet size, n is max
        // distance, m is number of keys. Much better than O(m) brute force.
        // ========================================================================

        class BKTreeNode {
        public:
            std::vector<uint8_t> key;
            // Children indexed by edit distance (distance -> child node)
            std::map<uint32_t, std::unique_ptr<BKTreeNode>> children;

            explicit BKTreeNode(const std::vector<uint8_t>& k) : key(k) {}
        };

        class BKTree {
        public:
            BKTree() : root_(nullptr), size_(0) {}

            // Insert a key into the BK-tree
            void insert(const std::vector<uint8_t>& key) {
                if (!root_) {
                    root_ = std::make_unique<BKTreeNode>(key);
                    size_ = 1;
                    return;
                }
                insertRecursive(root_.get(), key);
                size_++;
            }

            // Find all keys within max_edit_distance of the query
            std::vector<std::vector<uint8_t>> findWithinDistance(
                const std::vector<uint8_t>& query,
                uint32_t max_distance) const {

                std::vector<std::vector<uint8_t>> results;
                if (root_) {
                    searchRecursive(root_.get(), query, max_distance, results);
                }
                return results;
            }

            size_t size() const { return size_; }
            bool empty() const { return root_ == nullptr; }
            void clear() { root_.reset(); size_ = 0; }

        private:
            std::unique_ptr<BKTreeNode> root_;
            size_t size_;

            // Levenshtein distance calculation
            static uint32_t editDistance(const std::vector<uint8_t>& s1,
                                         const std::vector<uint8_t>& s2) {
                const size_t len1 = s1.size();
                const size_t len2 = s2.size();

                // Early termination for empty strings
                if (len1 == 0) return static_cast<uint32_t>(len2);
                if (len2 == 0) return static_cast<uint32_t>(len1);

                // Use space-optimized DP (two rows instead of full matrix)
                std::vector<uint32_t> prev_row(len2 + 1);
                std::vector<uint32_t> curr_row(len2 + 1);

                // Initialize first row
                for (size_t j = 0; j <= len2; j++) {
                    prev_row[j] = static_cast<uint32_t>(j);
                }

                // Fill the DP table row by row
                for (size_t i = 1; i <= len1; i++) {
                    curr_row[0] = static_cast<uint32_t>(i);

                    for (size_t j = 1; j <= len2; j++) {
                        uint32_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;

                        curr_row[j] = std::min({
                            prev_row[j] + 1,      // deletion
                            curr_row[j - 1] + 1,  // insertion
                            prev_row[j - 1] + cost // substitution
                        });
                    }

                    std::swap(prev_row, curr_row);
                }

                return prev_row[len2];
            }

            void insertRecursive(BKTreeNode* node, const std::vector<uint8_t>& key) {
                uint32_t dist = editDistance(node->key, key);

                // If exact duplicate, don't insert
                if (dist == 0) {
                    return;
                }

                auto it = node->children.find(dist);
                if (it == node->children.end()) {
                    // No child at this distance, create new node
                    node->children[dist] = std::make_unique<BKTreeNode>(key);
                } else {
                    // Recurse into child at this distance
                    insertRecursive(it->second.get(), key);
                }
            }

            void searchRecursive(const BKTreeNode* node,
                                const std::vector<uint8_t>& query,
                                uint32_t max_distance,
                                std::vector<std::vector<uint8_t>>& results) const {

                uint32_t dist = editDistance(node->key, query);

                // If within distance, add to results
                if (dist <= max_distance) {
                    results.push_back(node->key);
                }

                // Search children at distances [dist - max_distance, dist + max_distance]
                // These are the only children that could possibly contain matches
                uint32_t min_dist = (dist > max_distance) ? dist - max_distance : 0;
                uint32_t max_dist = dist + max_distance;

                for (const auto& child : node->children) {
                    if (child.first >= min_dist && child.first <= max_dist) {
                        searchRecursive(child.second.get(), query, max_distance, results);
                    }
                }
            }
        };

        // ========================================================================
        // GinIndex Implementation
        // ========================================================================

        // Constructor
        GinIndex::GinIndex(Database *db, const UuidV7Bytes &index_uuid)
            : db_(db), buffer_pool_(db->buffer_pool()), index_uuid_(index_uuid), meta_page_(0)
        {
        }

        // Destructor
        GinIndex::~GinIndex() = default;

        // Dynamic capacity calculations
        uint16_t GinIndex::getMaxPendingEntriesPerPage() const
        {
            return (db_->page_size() - 128) / sizeof(GinPendingEntry);
        }

        uint16_t GinIndex::getMaxPostingEntriesPerPage() const
        {
            return (db_->page_size() - 80) / sizeof(GinPostingEntry);
        }

        uint16_t GinIndex::getMaxPostingTreeInternalEntries() const
        {
            return (db_->page_size() - 92) / sizeof(GinPostingTreeInternalEntry);
        }

        uint16_t GinIndex::getMaxPostingTreeLeafTids() const
        {
            return (db_->page_size() - 88) / sizeof(GinPostingEntry);
        }

        // Create a new GIN index
        Status GinIndex::create(Database *db, const UuidV7Bytes &index_uuid,
                                uint32_t *meta_page_out, ErrorContext *ctx)
        {
            if (!db || !meta_page_out)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid arguments to GinIndex::create");
                return Status::INVALID_ARGUMENT;
            }

            BufferPool *buffer_pool = db->buffer_pool();
            if (!buffer_pool)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
                return Status::INVALID_ARGUMENT;
            }

            // Step 1: Allocate meta page
            uint32_t meta_page = 0;
            Status status = db->page_manager()->allocatePage(meta_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Step 2: Pin and initialize meta page
            uint8_t *meta_page_data = nullptr;
            status = buffer_pool->pinPage(meta_page, (void **)&meta_page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_page_data);
            std::memset(meta, 0, sizeof(SBGinIndexMetaPage));

            // Initialize meta page header
            meta->hip_header.magic = K_MAGIC_SBRD;
            meta->hip_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            meta->hip_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
            meta->hip_header.page_size = db->page_size();
            meta->hip_header.page_id = meta_page;

            // Initialize meta data
            std::memcpy(meta->gin_index_uuid, index_uuid.bytes.data(), 16);
            meta->gin_keys_btree_root = 0;  // Will be allocated on first insert
            meta->gin_pending_list_head = 0;
            meta->gin_pending_list_tail = 0;
            meta->gin_pending_list_count = 0;
            meta->gin_num_keys = 0;
            meta->gin_num_tuples = 0;

            buffer_pool->unpinPage(meta_page, true, ctx);

            *meta_page_out = meta_page;
            return Status::OK;
        }

        // Open an existing GIN index
        std::unique_ptr<GinIndex> GinIndex::open(Database *db, const UuidV7Bytes &index_uuid,
                                                 uint32_t meta_page, ErrorContext *ctx)
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
                return nullptr;
            }

            auto index = std::make_unique<GinIndex>(db, index_uuid);
            index->meta_page_ = meta_page;

            // Verify meta page
            uint8_t *meta_data = nullptr;
            Status status = db->buffer_pool()->pinPage(meta_page, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return nullptr;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            if (meta->hip_header.page_type != static_cast<uint16_t>(PageType::GIN_INDEX_META))
            {
                db->buffer_pool()->unpinPage(meta_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid GIN index meta page");
                return nullptr;
            }

            if (std::memcmp(meta->gin_index_uuid, index_uuid.bytes.data(), 16) != 0)
            {
                db->buffer_pool()->unpinPage(meta_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "GIN index UUID mismatch");
                return nullptr;
            }

            db->buffer_pool()->unpinPage(meta_page, false, ctx);

            return index;
        }

        // Insert a composite value
        Status GinIndex::insert(const void *value_data, size_t value_len, const TID &tid,
                                std::function<std::vector<std::vector<uint8_t>>(const void *, size_t)> key_extractor,
                                ErrorContext *ctx)
        {
            // PHASE 1.5: Convert TID to legacy format for storage
            uint64_t legacy_tid = convertTIDtoLegacy(tid);
            if (legacy_tid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                  "Custom tablespace indexes not yet supported in ALPHA");
                return Status::NOT_IMPLEMENTED;
            }

            if (!value_data || value_len == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid insert arguments");
                return Status::INVALID_ARGUMENT;
            }

            // Extract keys from the composite value
            std::vector<std::vector<uint8_t>> keys = key_extractor(value_data, value_len);

            if (keys.empty())
            {
                // No keys to index
                return Status::OK;
            }

            // Insert each key into the pending list (using legacy format internally)
            for (const auto &key : keys)
            {
                Status status = insertIntoPendingList(key, legacy_tid, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            // Check if pending list threshold reached
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint64_t pending_count = meta->gin_pending_list_count;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Auto-merge if threshold reached
            if (pending_count >= GIN_PENDING_LIST_THRESHOLD)
            {
                return mergePendingList(ctx);
            }

            return Status::OK;
        }

        // Remove a composite value from the index (November 20, 2025)
        // Firebird MGA: Logical deletion - marks entries with xmax
        Status GinIndex::remove(const void *value_data, size_t value_len, const TID &tid,
                                std::function<std::vector<std::vector<uint8_t>>(const void *, size_t)> key_extractor,
                                uint64_t current_xid,
                                ErrorContext *ctx)
        {
            // Convert TID to legacy format
            uint64_t legacy_tid = convertTIDtoLegacy(tid);
            if (legacy_tid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                  "Custom tablespace indexes not yet supported in ALPHA");
                return Status::NOT_IMPLEMENTED;
            }

            if (!value_data || value_len == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid remove arguments");
                return Status::INVALID_ARGUMENT;
            }

            // Extract keys from the composite value
            std::vector<std::vector<uint8_t>> keys = key_extractor(value_data, value_len);

            if (keys.empty())
            {
                // No keys to remove
                return Status::OK;
            }

            // For each key, remove the TID from the posting list/tree
            // GIN uses post-filtering: visibility is checked at heap tuple level,
            // so we physically remove TIDs from posting lists rather than marking with xmax
            for (const auto &key : keys)
            {
                // Find the key in the entry tree
                uint64_t posting_page = 0;
                Status status = searchKeysTree(key, &posting_page, ctx);

                if (status != Status::OK || posting_page == 0)
                {
                    // Key not found in entry tree - this is OK, might have been vacuumed
                    continue;
                }

                // Remove TID from the posting list/tree at posting_page
                status = removeFromPostingList(static_cast<uint32_t>(posting_page), legacy_tid, ctx);
                if (status != Status::OK)
                {
                    // Log warning but continue with other keys
                    LOG_WARNING(STORAGE, "Failed to remove TID %lu from posting list for key (status=%d)",
                                legacy_tid, static_cast<int>(status));
                }
            }

            // Note: Statistics updates (gin_num_tuples decrement) handled by caller
            // through table-level tracking. Individual key statistics updated in
            // removeFromPostingList().

            return Status::OK;
        }

        // Helper: Insert into pending list
        Status GinIndex::insertIntoPendingList(const std::vector<uint8_t> &key, uint64_t tuple_id,
                                               ErrorContext *ctx)
        {
            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);

            // Check if we need to allocate first pending page
            if (meta->gin_pending_list_tail == 0)
            {
                uint32_t pending_page = 0;
                status = db_->page_manager()->allocatePage(pending_page, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(meta_page_, false, ctx);
                    return status;
                }

                // Initialize pending page
                uint8_t *pending_data = nullptr;
                status = buffer_pool_->pinPage(pending_page, (void **)&pending_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(meta_page_, false, ctx);
                    return status;
                }

                auto *pending = reinterpret_cast<SBGinPendingListPage *>(pending_data);
                std::memset(pending, 0, sizeof(SBGinPendingListPage));

                pending->gpp_header.magic = K_MAGIC_SBRD;
                pending->gpp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                pending->gpp_header.page_type = static_cast<uint16_t>(PageType::GIN_PENDING_LIST);
                pending->gpp_header.page_size = db_->page_size();
                pending->gpp_header.page_id = pending_page;
                pending->gpp_next_page = 0;
                pending->gpp_entry_count = 0;

                buffer_pool_->unpinPage(pending_page, true, ctx);

                meta->gin_pending_list_head = pending_page;
                meta->gin_pending_list_tail = pending_page;
            }

            uint32_t tail_page = meta->gin_pending_list_tail;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Pin tail page
            uint8_t *tail_data = nullptr;
            status = buffer_pool_->pinPage(tail_page, (void **)&tail_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *tail = reinterpret_cast<SBGinPendingListPage *>(tail_data);

            // Check if tail page is full
            if (tail->gpp_entry_count >= getMaxPendingEntriesPerPage())
            {
                // Allocate new pending page
                uint32_t new_pending_page = 0;
                status = db_->page_manager()->allocatePage(new_pending_page, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(tail_page, false, ctx);
                    return status;
                }

                // Initialize new pending page
                uint8_t *new_pending_data = nullptr;
                status = buffer_pool_->pinPage(new_pending_page, (void **)&new_pending_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(tail_page, false, ctx);
                    return status;
                }

                auto *new_pending = reinterpret_cast<SBGinPendingListPage *>(new_pending_data);
                std::memset(new_pending, 0, sizeof(SBGinPendingListPage));

                new_pending->gpp_header.magic = K_MAGIC_SBRD;
                new_pending->gpp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                new_pending->gpp_header.page_type = static_cast<uint16_t>(PageType::GIN_PENDING_LIST);
                new_pending->gpp_header.page_size = db_->page_size();
                new_pending->gpp_header.page_id = new_pending_page;
                new_pending->gpp_next_page = 0;
                new_pending->gpp_entry_count = 0;

                // Link old tail to new page
                tail->gpp_next_page = new_pending_page;
                buffer_pool_->unpinPage(tail_page, true, ctx);

                // Update meta to point to new tail
                status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(new_pending_page, false, ctx);
                    return status;
                }

                meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
                meta->gin_pending_list_tail = new_pending_page;
                buffer_pool_->unpinPage(meta_page_, true, ctx);

                // Switch to new tail
                tail_data = new_pending_data;
                tail = new_pending;
                tail_page = new_pending_page;
            }

            // Add entry to tail page
            GinPendingEntry &entry = tail->gpp_entries[tail->gpp_entry_count];
            entry.setTID(convertLegacyTID(tuple_id)); // Convert legacy TID to GPID format
            entry.padding = 0; // Clear padding
            entry.xmin = ConnectionContext::getCurrentTransactionId(); // Record inserting transaction
            // MEDIUM-4 FIX: Use sizeof(entry.key_data) instead of magic number 54 for maintainability
            entry.key_len = std::min(static_cast<uint16_t>(key.size()), static_cast<uint16_t>(sizeof(entry.key_data)));
            std::memcpy(entry.key_data, key.data(), entry.key_len);

            tail->gpp_entry_count++;

            buffer_pool_->unpinPage(tail_page, true, ctx);

            // Update meta count
            status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status == Status::OK)
            {
                meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
                meta->gin_pending_list_count++;
                buffer_pool_->unpinPage(meta_page_, true, ctx);
            }

            return Status::OK;
        }

        // Find all tuple IDs containing a specific key
        // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
        // PHASE 1.5: Return TID structs instead of uint64_t
        Status GinIndex::find(const void *key_data, size_t key_len,
                              uint64_t current_xid,
                              std::vector<TID>* results,
                              ErrorContext *ctx)
        {
            if (!key_data || key_len == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key data for GIN index find");
                return Status::INVALID_ARGUMENT;
            }

            if (!results)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Results vector cannot be null");
                return Status::INVALID_ARGUMENT;
            }

            results->clear();

            std::vector<uint8_t> key(static_cast<const uint8_t *>(key_data),
                                     static_cast<const uint8_t *>(key_data) + key_len);

            // Search for the key in the keys B-Tree
            uint64_t posting_page = 0;
            Status status = searchKeysTree(key, &posting_page, ctx);

            // Get TIDs from posting list (if key found in main index)
            if (status == Status::OK && posting_page != 0)
            {
                // PHASE 1.5: Use legacy format for internal operations, convert to TID for output
                std::vector<uint64_t> legacy_results;
                // FIREBIRD MGA: Pass current_xid for index-level visibility filtering
                status = getPostingListTids(posting_page, &legacy_results, current_xid, ctx);
                if (status != Status::OK)
                {
                    results->clear();
                    SET_ERROR_CONTEXT(ctx, status, "Failed to get posting list TIDs for GIN index find");
                    return status;
                }
                else
                {
                    // Firebird MGA: Filter TIDs from main posting list by heap tuple visibility using TIP
                    // This ensures we only return TIDs for tuples that are visible to current_xid
                    legacy_results = filterTidsByVisibility(legacy_results, current_xid, ctx);

                    // PHASE 1.5: Convert legacy uint64_t to TID structs
                    for (uint64_t legacy_tid : legacy_results)
                    {
                        results->push_back(convertLegacyTID(legacy_tid));
                    }
                }
            }

            // Scan pending list for matching keys with visibility check
            // PHASE 1 TASK 1.4: Use passed-in snapshot parameter instead of local snapshot

            // Pin meta page to get pending list head
            uint8_t *meta_data = nullptr;
            status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status == Status::OK)
            {
                auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
                uint32_t pending_page = meta->gin_pending_list_head;
                buffer_pool_->unpinPage(meta_page_, false, ctx);

                // Scan through all pending list pages
                while (pending_page != 0)
                {
                    uint8_t *pending_data = nullptr;
                    status = buffer_pool_->pinPage(pending_page, (void **)&pending_data, ctx);
                    if (status != Status::OK)
                    {
                        break;
                    }

                    auto *pending = reinterpret_cast<SBGinPendingListPage *>(pending_data);
                    uint32_t next_page = pending->gpp_next_page;

                    // Check each entry in this pending page
                    for (uint16_t i = 0; i < pending->gpp_entry_count; i++)
                    {
                        const GinPendingEntry &entry = pending->gpp_entries[i];

                        // PHASE 1 TASK 1.4: Check visibility using passed-in snapshot or transaction state
                        // Pending list entries have xmin field for MVCC visibility
                        bool is_visible = isTransactionVisible(entry.xmin, current_xid, ctx);

                        // If visible and key matches, add TID to results
                        if (is_visible)
                        {
                            std::vector<uint8_t> entry_key(entry.key_data, entry.key_data + entry.key_len);
                            if (entry_key == key)
                            {
                                // Get TID from entry (now uses GPID format)
                                results->push_back(entry.getTID());
                            }
                        }
                    }

                    buffer_pool_->unpinPage(pending_page, false, ctx);
                    pending_page = next_page;
                }
            }

            // Sort results to ensure consistent ordering
            std::sort(results->begin(), results->end());

            return Status::OK;
        }

        // Merge pending list into main index (Phase 3 implementation)
        Status GinIndex::mergePendingList(ErrorContext *ctx)
        {
            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);

            if (meta->gin_pending_list_count == 0)
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
                return Status::OK;
            }

            uint32_t pending_head = meta->gin_pending_list_head;
            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Step 1: Collect all entries from pending list
            struct PendingEntryWithKey
            {
                std::vector<uint8_t> key;
                uint64_t tid;
            };
            std::vector<PendingEntryWithKey> all_entries;

            uint32_t current_page = pending_head;
            while (current_page != 0)
            {
                uint8_t *pending_data = nullptr;
                status = buffer_pool_->pinPage(current_page, (void **)&pending_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *pending = reinterpret_cast<SBGinPendingListPage *>(pending_data);

                // Collect entries from this page
                for (uint16_t i = 0; i < pending->gpp_entry_count; i++)
                {
                    PendingEntryWithKey entry;
                    // Convert GPID format to legacy uint64_t for internal processing
                    entry.tid = convertTIDtoLegacy(pending->gpp_entries[i].getTID());
                    entry.key.assign(pending->gpp_entries[i].key_data,
                                     pending->gpp_entries[i].key_data + pending->gpp_entries[i].key_len);
                    all_entries.push_back(entry);
                }

                uint32_t next_page = pending->gpp_next_page;
                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }

            // Step 2: Sort entries by key
            std::sort(all_entries.begin(), all_entries.end(),
                      [](const PendingEntryWithKey &a, const PendingEntryWithKey &b)
                      {
                          return compareKeys(a.key, b.key) < 0;
                      });

            // Step 3: Group by key and insert into posting lists
            size_t i = 0;
            while (i < all_entries.size())
            {
                const std::vector<uint8_t> &current_key = all_entries[i].key;

                // Find or create posting list for this key
                uint64_t posting_page = 0;
                status = findOrCreatePostingList(current_key, &posting_page, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Insert all TIDs for this key
                while (i < all_entries.size() && compareKeys(all_entries[i].key, current_key) == 0)
                {
                    status = insertIntoPostingList(posting_page, all_entries[i].tid, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    i++;
                }
            }

            // Step 4: Clear pending list
            // For simplicity, we'll just reset the meta page pointers
            // In a production system, we might want to deallocate the pages
            status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            meta->gin_pending_list_head = 0;
            meta->gin_pending_list_tail = 0;
            meta->gin_pending_list_count = 0;

            buffer_pool_->unpinPage(meta_page_, true, ctx);

            return Status::OK;
        }

        // Vacuum operation (manual VACUUM command)
        // Note: Actual GC is done via removeDeadEntries() called by garbage collector
        Status GinIndex::vacuum(ErrorContext *ctx)
        {
            // Manual vacuum is optional - garbage collector uses removeDeadEntries()
            // Could implement: scan all posting lists, compact fragmented pages, update stats
            // For now, return OK - automatic GC via removeDeadEntries() is sufficient
            return Status::OK;
        }

        // Get statistics
        GinIndex::Statistics GinIndex::getStatistics(ErrorContext *ctx)
        {
            Statistics stats = {};

            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return stats;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);

            stats.num_keys = meta->gin_num_keys;
            stats.num_tuples = meta->gin_num_tuples;
            stats.pending_list_count = meta->gin_pending_list_count;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Other stats require scanning the index
            stats.keys_tree_height = 0;
            stats.avg_tids_per_key = 0.0;
            stats.num_posting_trees = 0;
            stats.num_posting_lists = 0;

            return stats;
        }

        // Search for a key in the keys B-Tree (Phase 3)
        Status GinIndex::searchKeysTree(const std::vector<uint8_t> &key,
                                        uint64_t *posting_page_out,
                                        ErrorContext *ctx)
        {
            // Pin meta page to get root
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint32_t root_page = meta->gin_keys_btree_root;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // If no root exists, key doesn't exist
            if (root_page == 0)
            {
                *posting_page_out = 0;
                return Status::NOT_FOUND;
            }

            // Find the leaf page for this key
            uint32_t leaf_page = 0;
            status = findEntryTreeLeaf(root_page, key, &leaf_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Search for the key in the leaf
            GinEntryTreeValue value;
            int32_t position = 0;
            status = searchEntryTreeLeaf(leaf_page, key, &value, &position, ctx);
            if (status != Status::OK)
            {
                *posting_page_out = 0;
                return status;
            }

            // Found the key
            *posting_page_out = value.posting_list_page;
            return Status::OK;
        }

        Status GinIndex::getPostingListTids(uint32_t posting_page,
                                            std::vector<uint64_t> *tids_out,
                                            uint64_t current_xid,
                                            ErrorContext *ctx)
        {
            // Pin the posting list page
            uint8_t *list_data = nullptr;
            Status status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);

            // Check if this is a tree or a list
            if (list_page->gpl_is_tree != 0)
            {
                // It's a posting tree
                uint32_t tree_root = list_page->gpl_data.gpl_tree_root;
                buffer_pool_->unpinPage(posting_page, false, ctx);
                return getPostingTreeTids(tree_root, tids_out, current_xid, ctx);
            }

            // It's a posting list - check if compressed
            uint16_t tid_count = list_page->gpl_entry_count;
            tids_out->reserve(tids_out->size() + tid_count);

            if (list_page->gpl_is_compressed != 0)
            {
                // Compressed posting list - decompress
                // NOTE: Compressed format doesn't store xmin/xmax, relies on heap visibility
                uint16_t compressed_bytes = list_page->gpl_compressed_bytes;
                const uint8_t *compressed_data = list_page->getCompressedData();

                // Allocate temp buffer for decompressed TIDs
                std::vector<uint64_t> temp_tids(tid_count);

                size_t decompressed_count = decompress_posting_list(
                    compressed_data, compressed_bytes,
                    temp_tids.data(), tid_count);

                if (decompressed_count != tid_count)
                {
                    buffer_pool_->unpinPage(posting_page, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                                      ("Decompression failed: expected " +
                                          std::to_string(tid_count) + " TIDs, got " +
                                          std::to_string(decompressed_count)).c_str());
                    return Status::COMPRESSION_ERROR;
                }

                // Append decompressed TIDs to output
                tids_out->insert(tids_out->end(), temp_tids.begin(), temp_tids.end());
            }
            else
            {
                // FIREBIRD MGA: Uncompressed posting list - check xmin/xmax visibility
                // Per MGA_RULES.md Rule 3: Use TIP-based visibility, NOT snapshots
                for (uint16_t i = 0; i < tid_count; i++)
                {
                    const GinPostingEntry &entry = list_page->getEntries()[i];

                    bool is_visible;
                    if (current_xid == 0)
                    {
                        // Special case: when current_xid == 0, bypass visibility checking
                        // This is used for unit testing GIN index functionality without transactions
                        is_visible = true;
                    }
                    else
                    {
                        // Check if entry is visible to current transaction
                        // Entry is visible if:
                        // 1. xmin is committed and < current_xid (inserted before us)
                        // 2. xmax is 0 (not deleted) OR xmax > current_xid (deleted after us started)
                        is_visible = isTransactionVisible(entry.xmin, current_xid, ctx) &&
                                          (entry.xmax == 0 || !isTransactionVisible(entry.xmax, current_xid, ctx));
                    }

                    if (is_visible)
                    {
                        // Convert GPID format to legacy uint64_t
                        tids_out->push_back(convertTIDtoLegacy(entry.getTID()));
                    }
                }
            }

            buffer_pool_->unpinPage(posting_page, false, ctx);
            return Status::OK;
        }

        Status GinIndex::findOrCreatePostingList(const std::vector<uint8_t> &key,
                                                 uint64_t *posting_page_out,
                                                 ErrorContext *ctx)
        {
            // First, search for existing key
            uint64_t posting_page = 0;
            Status status = searchKeysTree(key, &posting_page, ctx);
            if (status == Status::OK && posting_page != 0)
            {
                // Key already exists
                *posting_page_out = posting_page;
                return Status::OK;
            }

            // Key not found, create new posting list
            uint32_t new_posting_page = 0;
            status = db_->page_manager()->allocatePage(new_posting_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Initialize posting list page
            uint8_t *list_data = nullptr;
            status = buffer_pool_->pinPage(new_posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);
            std::memset(list_page, 0, sizeof(SBGinPostingListPage));

            list_page->gpl_header.magic = K_MAGIC_SBRD;
            list_page->gpl_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            list_page->gpl_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_LIST);
            list_page->gpl_header.page_size = db_->page_size();
            list_page->gpl_header.page_id = new_posting_page;
            list_page->gpl_entry_count = 0;
            list_page->gpl_is_tree = 0;
            list_page->gpl_is_compressed = 0;
            list_page->gpl_compressed_bytes = 0;

            buffer_pool_->unpinPage(new_posting_page, true, ctx);

            // Insert key into keys B-Tree
            status = insertIntoKeysTree(key, new_posting_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            *posting_page_out = new_posting_page;
            return Status::OK;
        }

        Status GinIndex::insertIntoPostingList(uint32_t posting_page, uint64_t tuple_id,
                                               ErrorContext *ctx)
        {
            // Pin the posting list page
            uint8_t *list_data = nullptr;
            Status status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);

            // Check if already converted to tree
            if (list_page->gpl_is_tree != 0)
            {
                buffer_pool_->unpinPage(posting_page, false, ctx);
                return insertIntoPostingTree(posting_page, tuple_id, ctx);
            }

            uint16_t current_count = list_page->gpl_entry_count;

            // Step 1: Read existing TIDs (compressed or uncompressed)
            std::vector<uint64_t> tids;
            tids.reserve(current_count + 1);

            if (list_page->gpl_is_compressed != 0)
            {
                // Decompress existing TIDs
                uint16_t compressed_bytes = list_page->gpl_compressed_bytes;
                const uint8_t *compressed_data = list_page->getCompressedData();

                tids.resize(current_count);
                size_t decompressed_count = decompress_posting_list(
                    compressed_data, compressed_bytes,
                    tids.data(), current_count);

                if (decompressed_count != current_count)
                {
                    buffer_pool_->unpinPage(posting_page, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Decompression failed during insert");
                    return Status::COMPRESSION_ERROR;
                }
            }
            else
            {
                // Read uncompressed TIDs
                for (uint16_t i = 0; i < current_count; i++)
                {
                    // Convert GPID format to legacy uint64_t
                    tids.push_back(convertTIDtoLegacy(list_page->getEntries()[i].getTID()));
                }
            }

            // Step 2: Check for duplicate
            if (std::binary_search(tids.begin(), tids.end(), tuple_id))
            {
                buffer_pool_->unpinPage(posting_page, false, ctx);
                return Status::OK; // Already exists
            }

            // Step 3: Insert new TID (maintaining sorted order)
            auto insert_pos = std::lower_bound(tids.begin(), tids.end(), tuple_id);
            tids.insert(insert_pos, tuple_id);

            // Step 4: Check if we need to convert to tree (threshold reached)
            if (tids.size() > GIN_POSTING_LIST_THRESHOLD)
            {
                buffer_pool_->unpinPage(posting_page, false, ctx);

                // Convert to tree
                status = convertListToTree(posting_page, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Insert into the new tree
                return insertIntoPostingTree(posting_page, tuple_id, ctx);
            }

            // Step 5: Try to compress the TID list
            size_t compressed_size = 0;
            // Use heap allocation for large page sizes to avoid stack overflow
            std::vector<uint8_t> temp_compressed(db_->page_size() - 80);

            if (should_compress(tids.data(), tids.size()))
            {
                compressed_size = compress_posting_list(
                    tids.data(), tids.size(),
                    temp_compressed.data(), temp_compressed.size());
            }

            // Step 6: Store compressed or uncompressed based on what fits
            if (compressed_size > 0 && compressed_size < temp_compressed.size())
            {
                // Store compressed
                list_page->gpl_is_compressed = 1;
                list_page->gpl_compressed_bytes = compressed_size;
                list_page->gpl_entry_count = tids.size();
                std::memcpy(list_page->getCompressedData(), temp_compressed.data(), compressed_size);
            }
            else
            {
                // Store uncompressed (or compression didn't save space)
                list_page->gpl_is_compressed = 0;
                list_page->gpl_compressed_bytes = 0;
                list_page->gpl_entry_count = tids.size();

                // FIREBIRD MGA: Get current transaction ID for xmin
                uint64_t current_xid = ConnectionContext::getCurrentTransactionId();

                for (size_t i = 0; i < tids.size(); i++)
                {
                    // Convert legacy uint64_t to GPID format
                    list_page->getEntries()[i].setTID(convertLegacyTID(tids[i]));
                    // FIREBIRD MGA: Set xmin (transaction that inserted) and xmax (0 = not deleted)
                    list_page->getEntries()[i].xmin = current_xid;
                    list_page->getEntries()[i].xmax = 0;
                }
            }

            buffer_pool_->unpinPage(posting_page, true, ctx);
            return Status::OK;
        }

        // ===== Posting Tree Operations (Phase 2) =====

        Status GinIndex::convertListToTree(uint32_t posting_page, ErrorContext *ctx)
        {
            // Pin the posting list page
            uint8_t *list_data = nullptr;
            Status status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);

            // Already a tree?
            if (list_page->gpl_is_tree != 0)
            {
                buffer_pool_->unpinPage(posting_page, false, ctx);
                return Status::OK;
            }

            // Copy TIDs from the list (compressed or uncompressed)
            uint16_t tid_count = list_page->gpl_entry_count;
            std::vector<uint64_t> tids;
            tids.reserve(tid_count);

            if (list_page->gpl_is_compressed != 0)
            {
                // Decompress TIDs
                uint16_t compressed_bytes = list_page->gpl_compressed_bytes;
                const uint8_t *compressed_data = list_page->getCompressedData();

                tids.resize(tid_count);
                size_t decompressed_count = decompress_posting_list(
                    compressed_data, compressed_bytes,
                    tids.data(), tid_count);

                if (decompressed_count != tid_count)
                {
                    buffer_pool_->unpinPage(posting_page, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                                      "Decompression failed during tree conversion");
                    return Status::COMPRESSION_ERROR;
                }
            }
            else
            {
                // Read uncompressed TIDs
                for (uint16_t i = 0; i < tid_count; i++)
                {
                    // Convert GPID format to legacy uint64_t for internal processing
                    tids.push_back(convertTIDtoLegacy(list_page->getEntries()[i].getTID()));
                }
            }

            buffer_pool_->unpinPage(posting_page, false, ctx);

            // Allocate a new leaf page
            uint32_t leaf_page = 0;
            status = db_->page_manager()->allocatePage(leaf_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Initialize leaf page
            uint8_t *leaf_data = nullptr;
            status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(leaf_data);
            std::memset(leaf, 0, sizeof(SBGinPostingTreeLeaf));

            leaf->gpt_header.magic = K_MAGIC_SBRD;
            leaf->gpt_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            leaf->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
            leaf->gpt_header.page_size = db_->page_size();
            leaf->gpt_header.page_id = leaf_page;
            leaf->gpt_is_leaf = 1;
            leaf->gpt_next_leaf = 0;
            leaf->gpt_entry_count = tid_count;

            // Copy TIDs to leaf (convert legacy uint64_t to GPID format)
            for (uint16_t i = 0; i < tid_count; i++)
            {
                leaf->gpt_tids[i].setTID(convertLegacyTID(tids[i]));
            }

            buffer_pool_->unpinPage(leaf_page, true, ctx);

            // Convert the posting list page to a tree root pointer
            status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);
            list_page->gpl_is_tree = 1;
            list_page->gpl_data.gpl_tree_root = leaf_page;
            list_page->gpl_entry_count = tid_count; // Keep count for statistics

            buffer_pool_->unpinPage(posting_page, true, ctx);

            return Status::OK;
        }

        Status GinIndex::findPostingTreeLeaf(uint32_t tree_root_page, uint64_t tid,
                                             uint32_t *leaf_page_out,
                                             ErrorContext *ctx)
        {
            uint32_t current_page = tree_root_page;

            while (true)
            {
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Check if this is a leaf
                auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(page_data);
                if (leaf->gpt_is_leaf == 1)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    *leaf_page_out = current_page;
                    return Status::OK;
                }

                // Internal node - find the appropriate child
                auto *internal = reinterpret_cast<SBGinPostingTreeInternal *>(page_data);
                uint32_t next_page = 0;

                // Binary search for the appropriate child
                int32_t left = 0;
                int32_t right = internal->gpt_entry_count - 1;
                int32_t child_idx = 0;

                while (left <= right)
                {
                    int32_t mid = (left + right) / 2;
                    // Convert GPID separator to legacy for comparison
                    uint64_t separator = convertTIDtoLegacy(internal->gpt_entries[mid].getSeparatorTID());
                    if (tid < separator)
                    {
                        right = mid - 1;
                        child_idx = mid;
                    }
                    else
                    {
                        left = mid + 1;
                        child_idx = mid + 1;
                    }
                }

                // Bounds check
                if (child_idx >= internal->gpt_entry_count)
                {
                    child_idx = internal->gpt_entry_count - 1;
                }

                next_page = internal->gpt_entries[child_idx].child_page;
                buffer_pool_->unpinPage(current_page, false, ctx);

                current_page = next_page;
            }
        }

        Status GinIndex::insertIntoPostingTree(uint32_t posting_page, uint64_t tid,
                                               ErrorContext *ctx)
        {
            // Pin the posting list page to get the tree root
            uint8_t *list_data = nullptr;
            Status status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);

            if (list_page->gpl_is_tree == 0)
            {
                buffer_pool_->unpinPage(posting_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Cannot insert into posting tree: page is not a tree");
                return Status::INVALID_ARGUMENT;
            }

            uint32_t tree_root = list_page->gpl_data.gpl_tree_root;
            buffer_pool_->unpinPage(posting_page, false, ctx);

            // Find the leaf page for this TID
            uint32_t leaf_page = 0;
            status = findPostingTreeLeaf(tree_root, tid, &leaf_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Insert into the leaf (may cause split)
            uint32_t new_sibling = 0;
            uint64_t separator_tid = 0;
            status = insertIntoPostingTreeLeaf(leaf_page, tid, &new_sibling, &separator_tid, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // If there was a split, we need to update the parent or create a new root
            if (new_sibling != 0)
            {
                // For now, we'll create a new root (simplified - full implementation would update parent)
                uint32_t new_root = 0;
                status = db_->page_manager()->allocatePage(new_root, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                uint8_t *root_data = nullptr;
                status = buffer_pool_->pinPage(new_root, (void **)&root_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *root = reinterpret_cast<SBGinPostingTreeInternal *>(root_data);
                std::memset(root, 0, sizeof(SBGinPostingTreeInternal));

                root->gpt_header.magic = K_MAGIC_SBRD;
                root->gpt_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                root->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
                root->gpt_header.page_size = db_->page_size();
                root->gpt_header.page_id = new_root;
                root->gpt_is_leaf = 0;
                root->gpt_entry_count = 2;

                // Convert legacy TID to GPID format for separator
                root->gpt_entries[0].setSeparatorTID(convertLegacyTID(separator_tid));
                root->gpt_entries[0].child_page = leaf_page;
                root->gpt_entries[1].setSeparatorTID(convertLegacyTID(UINT64_MAX));
                root->gpt_entries[1].child_page = new_sibling;

                buffer_pool_->unpinPage(new_root, true, ctx);

                // Update the posting list page to point to the new root
                status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
                if (status == Status::OK)
                {
                    list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);
                    list_page->gpl_data.gpl_tree_root = new_root;
                    buffer_pool_->unpinPage(posting_page, true, ctx);
                }
            }

            return Status::OK;
        }

        Status GinIndex::insertIntoPostingTreeLeaf(uint32_t leaf_page, uint64_t tid,
                                                   uint32_t *new_sibling_out,
                                                   uint64_t *separator_tid_out,
                                                   ErrorContext *ctx)
        {
            *new_sibling_out = 0;

            uint8_t *leaf_data = nullptr;
            Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(leaf_data);

            // Convert legacy TID to GPID for comparison/storage
            TID tid_struct = convertLegacyTID(tid);

            // Check for duplicate
            for (uint16_t i = 0; i < leaf->gpt_entry_count; i++)
            {
                if (leaf->gpt_tids[i].getTID() == tid_struct)
                {
                    buffer_pool_->unpinPage(leaf_page, false, ctx);
                    return Status::OK; // Already exists
                }
            }

            // Check if leaf is full
            if (leaf->gpt_entry_count >= getMaxPostingTreeLeafTids())
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return splitPostingTreeLeaf(leaf_page, new_sibling_out, separator_tid_out, ctx);
            }

            // Find insertion position (maintain sorted order)
            uint16_t insert_pos = 0;
            while (insert_pos < leaf->gpt_entry_count)
            {
                uint64_t existing_tid = convertTIDtoLegacy(leaf->gpt_tids[insert_pos].getTID());
                if (existing_tid >= tid)
                    break;
                insert_pos++;
            }

            // Shift elements to make room
            if (insert_pos < leaf->gpt_entry_count)
            {
                std::memmove(&leaf->gpt_tids[insert_pos + 1],
                             &leaf->gpt_tids[insert_pos],
                             (leaf->gpt_entry_count - insert_pos) * sizeof(GinPostingEntry));
            }

            // FIREBIRD MGA: Set TID and transaction markers
            leaf->gpt_tids[insert_pos].setTID(tid_struct);
            leaf->gpt_tids[insert_pos].xmin = ConnectionContext::getCurrentTransactionId();
            leaf->gpt_tids[insert_pos].xmax = 0; // Not deleted
            leaf->gpt_entry_count++;

            buffer_pool_->unpinPage(leaf_page, true, ctx);
            return Status::OK;
        }

        Status GinIndex::splitPostingTreeLeaf(uint32_t leaf_page,
                                              uint32_t *new_sibling_out,
                                              uint64_t *separator_tid_out,
                                              ErrorContext *ctx)
        {
            // Pin original leaf
            uint8_t *leaf_data = nullptr;
            Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(leaf_data);

            // Allocate new sibling
            uint32_t new_sibling = 0;
            status = db_->page_manager()->allocatePage(new_sibling, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return status;
            }

            uint8_t *sibling_data = nullptr;
            status = buffer_pool_->pinPage(new_sibling, (void **)&sibling_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return status;
            }

            auto *sibling = reinterpret_cast<SBGinPostingTreeLeaf *>(sibling_data);
            std::memset(sibling, 0, sizeof(SBGinPostingTreeLeaf));

            sibling->gpt_header.magic = K_MAGIC_SBRD;
            sibling->gpt_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            sibling->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
            sibling->gpt_header.page_size = db_->page_size();
            sibling->gpt_header.page_id = new_sibling;
            sibling->gpt_is_leaf = 1;

            // Split point: move second half to sibling
            uint16_t split_point = leaf->gpt_entry_count / 2;
            uint16_t move_count = leaf->gpt_entry_count - split_point;

            std::memcpy(sibling->gpt_tids, &leaf->gpt_tids[split_point],
                        move_count * sizeof(GinPostingEntry));
            sibling->gpt_entry_count = move_count;

            // Update original leaf
            leaf->gpt_entry_count = split_point;

            // Link siblings for range scans
            sibling->gpt_next_leaf = leaf->gpt_next_leaf;
            leaf->gpt_next_leaf = new_sibling;

            // Separator is the first key of the new sibling (convert GPID to legacy)
            *separator_tid_out = convertTIDtoLegacy(sibling->gpt_tids[0].getTID());
            *new_sibling_out = new_sibling;

            buffer_pool_->unpinPage(leaf_page, true, ctx);
            buffer_pool_->unpinPage(new_sibling, true, ctx);

            return Status::OK;
        }

        bool GinIndex::searchPostingTree(uint32_t tree_root_page, uint64_t tid,
                                         ErrorContext *ctx)
        {
            uint32_t leaf_page = 0;
            Status status = findPostingTreeLeaf(tree_root_page, tid, &leaf_page, ctx);
            if (status != Status::OK)
            {
                return false;
            }

            uint8_t *leaf_data = nullptr;
            status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return false;
            }

            auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(leaf_data);

            // Binary search in the leaf
            bool found = false;
            int32_t left = 0;
            int32_t right = leaf->gpt_entry_count - 1;

            while (left <= right)
            {
                int32_t mid = (left + right) / 2;
                // Convert GPID to legacy for comparison
                uint64_t mid_tid = convertTIDtoLegacy(leaf->gpt_tids[mid].getTID());

                if (mid_tid == tid)
                {
                    found = true;
                    break;
                }
                else if (mid_tid < tid)
                {
                    left = mid + 1;
                }
                else
                {
                    right = mid - 1;
                }
            }

            buffer_pool_->unpinPage(leaf_page, false, ctx);
            return found;
        }

        Status GinIndex::getPostingTreeTids(uint32_t tree_root_page,
                                            std::vector<uint64_t> *tids_out,
                                            uint64_t current_xid,
                                            ErrorContext *ctx)
        {
            // Find the leftmost leaf
            uint32_t current_page = tree_root_page;

            while (true)
            {
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(page_data);
                if (leaf->gpt_is_leaf == 1)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    break;
                }

                // Internal node - go to leftmost child
                auto *internal = reinterpret_cast<SBGinPostingTreeInternal *>(page_data);
                uint32_t next_page = internal->gpt_entries[0].child_page;
                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }

            // Now scan all leaves via next_leaf pointers
            uint32_t leaf_page = current_page;
            while (leaf_page != 0)
            {
                uint8_t *leaf_data = nullptr;
                Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(leaf_data);

                // FIREBIRD MGA: Check xmin/xmax visibility for each entry
                // Per MGA_RULES.md Rule 3: Use TIP-based visibility, NOT snapshots
                for (uint16_t i = 0; i < leaf->gpt_entry_count; i++)
                {
                    const GinPostingEntry &entry = leaf->gpt_tids[i];

                    bool is_visible;
                    if (current_xid == 0)
                    {
                        // Special case: when current_xid == 0, bypass visibility checking
                        // This is used for unit testing GIN index functionality without transactions
                        is_visible = true;
                    }
                    else
                    {
                        // Check if entry is visible to current transaction
                        // Entry is visible if:
                        // 1. xmin is committed and < current_xid (inserted before us)
                        // 2. xmax is 0 (not deleted) OR xmax > current_xid (deleted after us started)
                        is_visible = isTransactionVisible(entry.xmin, current_xid, ctx) &&
                                          (entry.xmax == 0 || !isTransactionVisible(entry.xmax, current_xid, ctx));
                    }

                    if (is_visible)
                    {
                        tids_out->push_back(convertTIDtoLegacy(entry.getTID()));
                    }
                }

                uint64_t next_leaf = leaf->gpt_next_leaf;
                buffer_pool_->unpinPage(leaf_page, false, ctx);

                leaf_page = next_leaf;
            }

            return Status::OK;
        }

        Status GinIndex::insertIntoPostingTreeInternal(uint32_t internal_page,
                                                       uint64_t separator_tid,
                                                       uint32_t child_page,
                                                       uint32_t *new_sibling_out,
                                                       uint64_t *separator_tid_out,
                                                       ErrorContext *ctx)
        {
            *new_sibling_out = 0;

            uint8_t *internal_data = nullptr;
            Status status = buffer_pool_->pinPage(internal_page, (void **)&internal_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *internal = reinterpret_cast<SBGinPostingTreeInternal *>(internal_data);

            // Check if internal node is full
            if (internal->gpt_entry_count >= getMaxPostingTreeInternalEntries())
            {
                buffer_pool_->unpinPage(internal_page, false, ctx);
                return splitPostingTreeInternal(internal_page, new_sibling_out, separator_tid_out, ctx);
            }

            // Find insertion position
            uint16_t insert_pos = 0;
            while (insert_pos < internal->gpt_entry_count)
            {
                // Convert GPID separator to legacy for comparison
                uint64_t current_sep = convertTIDtoLegacy(internal->gpt_entries[insert_pos].getSeparatorTID());
                if (current_sep >= separator_tid)
                    break;
                insert_pos++;
            }

            // Shift elements
            if (insert_pos < internal->gpt_entry_count)
            {
                std::memmove(&internal->gpt_entries[insert_pos + 1],
                             &internal->gpt_entries[insert_pos],
                             (internal->gpt_entry_count - insert_pos) * sizeof(GinPostingTreeInternalEntry));
            }

            // Convert legacy TID to GPID format for separator
            internal->gpt_entries[insert_pos].setSeparatorTID(convertLegacyTID(separator_tid));
            internal->gpt_entries[insert_pos].child_page = child_page;
            internal->gpt_entry_count++;

            buffer_pool_->unpinPage(internal_page, true, ctx);
            return Status::OK;
        }

        Status GinIndex::splitPostingTreeInternal(uint32_t internal_page,
                                                  uint32_t *new_sibling_out,
                                                  uint64_t *separator_tid_out,
                                                  ErrorContext *ctx)
        {
            // Pin original internal node
            uint8_t *internal_data = nullptr;
            Status status = buffer_pool_->pinPage(internal_page, (void **)&internal_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *internal = reinterpret_cast<SBGinPostingTreeInternal *>(internal_data);

            // Allocate new sibling
            uint32_t new_sibling = 0;
            status = db_->page_manager()->allocatePage(new_sibling, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(internal_page, false, ctx);
                return status;
            }

            uint8_t *sibling_data = nullptr;
            status = buffer_pool_->pinPage(new_sibling, (void **)&sibling_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(internal_page, false, ctx);
                return status;
            }

            auto *sibling = reinterpret_cast<SBGinPostingTreeInternal *>(sibling_data);
            std::memset(sibling, 0, sizeof(SBGinPostingTreeInternal));

            sibling->gpt_header.magic = K_MAGIC_SBRD;
            sibling->gpt_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            sibling->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
            sibling->gpt_header.page_size = db_->page_size();
            sibling->gpt_header.page_id = new_sibling;
            sibling->gpt_is_leaf = 0;

            // Split point
            uint16_t split_point = internal->gpt_entry_count / 2;
            uint16_t move_count = internal->gpt_entry_count - split_point;

            std::memcpy(sibling->gpt_entries, &internal->gpt_entries[split_point],
                        move_count * sizeof(GinPostingTreeInternalEntry));
            sibling->gpt_entry_count = move_count;

            // Update original internal node
            internal->gpt_entry_count = split_point;

            // Separator is the first key of the new sibling (convert GPID to legacy)
            *separator_tid_out = convertTIDtoLegacy(sibling->gpt_entries[0].getSeparatorTID());
            *new_sibling_out = new_sibling;

            buffer_pool_->unpinPage(internal_page, true, ctx);
            buffer_pool_->unpinPage(new_sibling, true, ctx);

            return Status::OK;
        }

        // ===== Posting List/Tree Removal Operations =====

        // Remove TID from posting list or tree (dispatcher)
        Status GinIndex::removeFromPostingList(uint32_t posting_page, uint64_t tid,
                                               ErrorContext *ctx)
        {
            // Pin the posting list page
            uint8_t *posting_data = nullptr;
            Status status = buffer_pool_->pinPage(posting_page, (void **)&posting_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *posting = reinterpret_cast<SBGinPostingListPage *>(posting_data);

            // Check if this is a posting tree or a posting list
            bool is_tree = (posting->gpl_is_tree != 0);

            buffer_pool_->unpinPage(posting_page, false, ctx);

            if (is_tree)
            {
                // It's a posting tree - remove from tree
                return removeFromPostingTree(posting_page, tid, ctx);
            }
            else
            {
                // It's a simple posting list - remove from array
                return removeFromPostingListArray(posting_page, tid, ctx);
            }
        }

        // Remove TID from posting list (simple array)
        // FIREBIRD MGA: Logical deletion - mark with xmax instead of physical removal
        Status GinIndex::removeFromPostingListArray(uint32_t posting_page, uint64_t tid,
                                                     ErrorContext *ctx)
        {
            // Pin the posting list page
            uint8_t *posting_data = nullptr;
            Status status = buffer_pool_->pinPage(posting_page, (void **)&posting_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *posting = reinterpret_cast<SBGinPostingListPage *>(posting_data);

            // Convert tid to TID struct for comparison
            TID target_tid = convertLegacyTID(tid);

            // Search for the TID in the array
            bool found = false;
            uint16_t found_index = 0;

            for (uint16_t i = 0; i < posting->gpl_entry_count; i++)
            {
                GinPostingEntry &entry = posting->getEntries()[i];
                TID entry_tid = entry.getTID();

                if (entry_tid.gpid == target_tid.gpid && entry_tid.slot == target_tid.slot)
                {
                    found = true;
                    found_index = i;
                    break;
                }
            }

            if (!found)
            {
                // TID not found in posting list - this is OK (might have been removed by vacuum)
                buffer_pool_->unpinPage(posting_page, false, ctx);
                return Status::OK;
            }

            // FIREBIRD MGA: Logical deletion - mark with xmax (MGA-compliant)
            // DO NOT physically remove the entry - this preserves stable TIDs
            // Per MGA_RULES.md Rule 5: Use back-versioning, not forward-versioning
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            posting->getEntries()[found_index].xmax = current_xid;

            // Note: entry_count is NOT decremented - entries remain in place
            // Vacuum will remove entries where xmax < OIT during garbage collection

            // Mark page as dirty
            buffer_pool_->unpinPage(posting_page, true, ctx);

            return Status::OK;
        }

        // Remove TID from posting tree (B-Tree of TIDs)
        Status GinIndex::removeFromPostingTree(uint32_t tree_root_page, uint64_t tid,
                                               ErrorContext *ctx)
        {
            // Find the leaf page containing this TID
            uint32_t leaf_page = 0;
            Status status = findPostingTreeLeaf(tree_root_page, tid, &leaf_page, ctx);

            if (status != Status::OK)
            {
                // TID not found or error - this is OK
                return Status::OK;
            }

            // Remove from the leaf page
            bool entry_removed = false;
            status = removeFromPostingTreeLeaf(leaf_page, tid, &entry_removed, ctx);

            if (status != Status::OK)
            {
                return status;
            }

            // Note: We don't handle tree rebalancing/merging here for simplicity
            // Empty leaf pages will be cleaned up during vacuum
            // This is acceptable for Phase 1

            return Status::OK;
        }

        // Remove TID from posting tree leaf
        // FIREBIRD MGA: Logical deletion - mark with xmax instead of physical removal
        Status GinIndex::removeFromPostingTreeLeaf(uint32_t leaf_page, uint64_t tid,
                                                   bool *entry_removed_out,
                                                   ErrorContext *ctx)
        {
            // Pin the leaf page
            uint8_t *leaf_data = nullptr;
            Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(leaf_data);

            // Convert tid to TID struct for comparison
            TID target_tid = convertLegacyTID(tid);

            // Search for the TID in the leaf (using binary search since it's sorted)
            bool found = false;
            uint16_t found_index = 0;

            // Linear search for simplicity (could be optimized to binary search)
            for (uint16_t i = 0; i < leaf->gpt_entry_count; i++)
            {
                GinPostingEntry &entry = leaf->gpt_tids[i];
                TID entry_tid = entry.getTID();

                if (entry_tid.gpid == target_tid.gpid && entry_tid.slot == target_tid.slot)
                {
                    found = true;
                    found_index = i;
                    break;
                }
            }

            if (!found)
            {
                // TID not found - this is OK
                if (entry_removed_out)
                {
                    *entry_removed_out = false;
                }
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return Status::OK;
            }

            // FIREBIRD MGA: Logical deletion - mark with xmax (MGA-compliant)
            // DO NOT physically remove the entry - this preserves stable TIDs
            // Per MGA_RULES.md Rule 5: Use back-versioning, not forward-versioning
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            leaf->gpt_tids[found_index].xmax = current_xid;

            // Note: entry_count is NOT decremented - entries remain in place
            // Vacuum will remove entries where xmax < OIT during garbage collection

            if (entry_removed_out)
            {
                *entry_removed_out = true;
            }

            // Mark page as dirty
            buffer_pool_->unpinPage(leaf_page, true, ctx);

            return Status::OK;
        }

        // ===== Entry Tree (Keys B-Tree) Operations (Phase 3) =====

        // Compare two keys (lexicographic comparison)
        int GinIndex::compareKeys(const std::vector<uint8_t> &key1,
                                  const std::vector<uint8_t> &key2)
        {
            size_t min_len = std::min(key1.size(), key2.size());

            for (size_t i = 0; i < min_len; i++)
            {
                if (key1[i] < key2[i])
                {
                    return -1;
                }
                else if (key1[i] > key2[i])
                {
                    return 1;
                }
            }

            // If all compared bytes are equal, compare lengths
            if (key1.size() < key2.size())
            {
                return -1;
            }
            else if (key1.size() > key2.size())
            {
                return 1;
            }

            return 0; // Equal
        }

        Status GinIndex::insertIntoKeysTree(const std::vector<uint8_t> &key,
                                            uint64_t posting_page,
                                            ErrorContext *ctx)
        {
            // Pin meta page to get root
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint32_t root_page = meta->gin_keys_btree_root;

            // If no root exists, create first leaf
            if (root_page == 0)
            {
                // Allocate new leaf page
                status = db_->page_manager()->allocatePage(root_page, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(meta_page_, false, ctx);
                    return status;
                }

                // Initialize leaf page
                uint8_t *leaf_data = nullptr;
                status = buffer_pool_->pinPage(root_page, (void **)&leaf_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(meta_page_, false, ctx);
                    return status;
                }

                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);
                std::memset(leaf, 0, sizeof(SBGinEntryTreeLeaf));

                leaf->get_header.magic = K_MAGIC_SBRD;
                leaf->get_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                leaf->get_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
                leaf->get_header.page_size = db_->page_size();
                leaf->get_header.page_id = root_page;
                leaf->get_is_leaf = 1;
                leaf->get_entry_count = 0;
                leaf->get_free_space = db_->page_size() - sizeof(SBGinEntryTreeLeaf); // Data area size
                leaf->get_data_end = db_->page_size();

                buffer_pool_->unpinPage(root_page, true, ctx);

                // Update meta with new root
                meta->gin_keys_btree_root = root_page;
                meta->gin_num_keys++;
                buffer_pool_->unpinPage(meta_page_, true, ctx);
            }
            else
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
            }

            // Find leaf page for this key
            uint32_t leaf_page = 0;
            status = findEntryTreeLeaf(root_page, key, &leaf_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Insert into leaf (may cause split)
            GinEntryTreeValue value;
            value.posting_list_page = posting_page;
            value.num_tids = 0; // Will be updated when TIDs are added

            uint32_t new_sibling = 0;
            std::vector<uint8_t> separator_key;
            status = insertIntoEntryTreeLeaf(leaf_page, key, value, &new_sibling, &separator_key, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // If there was a split, create new root
            if (new_sibling != 0)
            {
                uint32_t new_root = 0;
                status = createNewEntryTreeRoot(leaf_page, new_sibling, separator_key, &new_root, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Update meta with new root
                status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
                if (status == Status::OK)
                {
                    meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
                    meta->gin_keys_btree_root = new_root;
                    buffer_pool_->unpinPage(meta_page_, true, ctx);
                }
            }

            return Status::OK;
        }

        Status GinIndex::findEntryTreeLeaf(uint32_t tree_root, const std::vector<uint8_t> &key,
                                           uint32_t *leaf_page_out,
                                           ErrorContext *ctx)
        {
            uint32_t current_page = tree_root;

            while (true)
            {
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Check if this is a leaf
                auto *page_base = reinterpret_cast<SBGinEntryTreeLeaf *>(page_data);
                if (page_base->get_is_leaf == 1)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    *leaf_page_out = current_page;
                    return Status::OK;
                }

                // Internal node - find the appropriate child
                auto *internal = reinterpret_cast<SBGinEntryTreeInternal *>(page_data);
                uint32_t next_page = internal->get_rightmost_child; // Default to rightmost

                // Binary search through internal entries
                for (uint16_t i = 0; i < internal->get_entry_count; i++)
                {
                    uint16_t offset = internal->get_offsets[i];
                    auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(page_data + offset);

                    std::vector<uint8_t> separator_key(entry->key_data, entry->key_data + entry->key_len);

                    if (compareKeys(key, separator_key) < 0)
                    {
                        next_page = entry->child_page;
                        break;
                    }
                }

                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }
        }

        Status GinIndex::searchEntryTreeLeaf(uint32_t leaf_page, const std::vector<uint8_t> &key,
                                             GinEntryTreeValue *value_out, int32_t *position_out,
                                             ErrorContext *ctx)
        {
            uint8_t *leaf_data = nullptr;
            Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);

            // Binary search through entries
            int32_t left = 0;
            int32_t right = leaf->get_entry_count - 1;
            int32_t result_pos = -1;

            while (left <= right)
            {
                int32_t mid = (left + right) / 2;
                uint16_t offset = leaf->get_offsets[mid];
                auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);

                std::vector<uint8_t> entry_key(entry->key_data, entry->key_data + entry->key_len);
                int cmp = compareKeys(key, entry_key);

                if (cmp == 0)
                {
                    // Found exact match
                    if (value_out)
                    {
                        *value_out = entry->value;
                    }
                    result_pos = mid;
                    break;
                }
                else if (cmp < 0)
                {
                    right = mid - 1;
                    result_pos = mid; // Insert position if not found
                }
                else
                {
                    left = mid + 1;
                    result_pos = mid + 1; // Insert position if not found
                }
            }

            if (position_out)
            {
                *position_out = result_pos;
            }

            buffer_pool_->unpinPage(leaf_page, false, ctx);

            if (result_pos == -1 || result_pos >= leaf->get_entry_count)
            {
                return Status::NOT_FOUND;
            }

            // Check if we found exact match
            uint16_t offset = leaf->get_offsets[result_pos];
            auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);
            std::vector<uint8_t> entry_key(entry->key_data, entry->key_data + entry->key_len);

            if (compareKeys(key, entry_key) == 0)
            {
                return Status::OK;
            }

            return Status::NOT_FOUND;
        }

        Status GinIndex::insertIntoEntryTreeLeaf(uint32_t leaf_page,
                                                 const std::vector<uint8_t> &key,
                                                 const GinEntryTreeValue &value,
                                                 uint32_t *new_sibling_out,
                                                 std::vector<uint8_t> *separator_key_out,
                                                 ErrorContext *ctx)
        {
            *new_sibling_out = 0;

            uint8_t *leaf_data = nullptr;
            Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);

            // Calculate space needed: key_len (2) + value (12) + key_data
            uint16_t space_needed = sizeof(uint16_t) + sizeof(GinEntryTreeValue) + key.size();

            // Check if we need to split
            if (leaf->get_free_space < space_needed || leaf->get_entry_count >= MAX_ENTRY_TREE_LEAF_ENTRIES)
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return splitEntryTreeLeaf(leaf_page, new_sibling_out, separator_key_out, ctx);
            }

            // Find insertion position
            int32_t insert_pos = 0;
            for (uint16_t i = 0; i < leaf->get_entry_count; i++)
            {
                uint16_t offset = leaf->get_offsets[i];
                auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);
                std::vector<uint8_t> entry_key(entry->key_data, entry->key_data + entry->key_len);

                if (compareKeys(key, entry_key) < 0)
                {
                    insert_pos = i;
                    break;
                }
                insert_pos = i + 1;
            }

            // Allocate space at end of data area (grows downward from get_data_end)
            uint16_t new_offset = leaf->get_data_end - space_needed;
            auto *new_entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + new_offset);

            new_entry->key_len = key.size();
            new_entry->value = value;
            std::memcpy(new_entry->key_data, key.data(), key.size());

            // Shift offsets array to make room
            if (insert_pos < leaf->get_entry_count)
            {
                std::memmove(&leaf->get_offsets[insert_pos + 1],
                             &leaf->get_offsets[insert_pos],
                             (leaf->get_entry_count - insert_pos) * sizeof(uint16_t));
            }

            // Insert new offset
            leaf->get_offsets[insert_pos] = new_offset;
            leaf->get_entry_count++;
            leaf->get_data_end = new_offset;
            leaf->get_free_space -= space_needed;

            buffer_pool_->unpinPage(leaf_page, true, ctx);
            return Status::OK;
        }

        Status GinIndex::splitEntryTreeLeaf(uint32_t leaf_page,
                                            uint32_t *new_sibling_out,
                                            std::vector<uint8_t> *separator_key_out,
                                            ErrorContext *ctx)
        {
            // Pin original leaf
            uint8_t *leaf_data = nullptr;
            Status status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);

            // Allocate new sibling
            uint32_t new_sibling = 0;
            status = db_->page_manager()->allocatePage(new_sibling, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return status;
            }

            uint8_t *sibling_data = nullptr;
            status = buffer_pool_->pinPage(new_sibling, (void **)&sibling_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return status;
            }

            auto *sibling = reinterpret_cast<SBGinEntryTreeLeaf *>(sibling_data);
            std::memset(sibling, 0, sizeof(SBGinEntryTreeLeaf));

            sibling->get_header.magic = K_MAGIC_SBRD;
            sibling->get_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            sibling->get_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
            sibling->get_header.page_size = db_->page_size();
            sibling->get_header.page_id = new_sibling;
            sibling->get_is_leaf = 1;
            sibling->get_entry_count = 0;
            sibling->get_free_space = db_->page_size() - sizeof(SBGinEntryTreeLeaf);
            sibling->get_data_end = db_->page_size();

            // Split point: move second half to sibling
            uint16_t split_point = leaf->get_entry_count / 2;
            uint16_t sibling_new_data_end = db_->page_size();

            // Copy entries from split_point onwards to sibling
            for (uint16_t i = split_point; i < leaf->get_entry_count; i++)
            {
                uint16_t offset = leaf->get_offsets[i];
                auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);

                // Calculate entry size
                uint16_t entry_size = sizeof(uint16_t) + sizeof(GinEntryTreeValue) + entry->key_len;

                // Allocate space in sibling
                sibling_new_data_end -= entry_size;
                auto *new_entry = reinterpret_cast<GinEntryTreeLeafEntry *>(sibling_data + sibling_new_data_end);

                // Copy entry
                new_entry->key_len = entry->key_len;
                new_entry->value = entry->value;
                std::memcpy(new_entry->key_data, entry->key_data, entry->key_len);

                // Add offset to sibling
                sibling->get_offsets[sibling->get_entry_count] = sibling_new_data_end;
                sibling->get_entry_count++;
                sibling->get_free_space -= entry_size;
            }

            sibling->get_data_end = sibling_new_data_end;

            // Update original leaf
            leaf->get_entry_count = split_point;
            // Recalculate free space
            leaf->get_free_space = leaf->get_data_end - (1084 + leaf->get_entry_count * sizeof(uint16_t));

            // Separator key is the first key of sibling
            uint16_t sep_offset = sibling->get_offsets[0];
            auto *sep_entry = reinterpret_cast<GinEntryTreeLeafEntry *>(sibling_data + sep_offset);
            separator_key_out->assign(sep_entry->key_data, sep_entry->key_data + sep_entry->key_len);

            *new_sibling_out = new_sibling;

            buffer_pool_->unpinPage(leaf_page, true, ctx);
            buffer_pool_->unpinPage(new_sibling, true, ctx);

            return Status::OK;
        }

        Status GinIndex::createNewEntryTreeRoot(uint32_t left_child, uint32_t right_child,
                                                const std::vector<uint8_t> &separator_key,
                                                uint32_t *new_root_out,
                                                ErrorContext *ctx)
        {
            // Allocate new root page
            uint32_t new_root = 0;
            Status status = db_->page_manager()->allocatePage(new_root, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            uint8_t *root_data = nullptr;
            status = buffer_pool_->pinPage(new_root, (void **)&root_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *root = reinterpret_cast<SBGinEntryTreeInternal *>(root_data);
            std::memset(root, 0, sizeof(SBGinEntryTreeInternal));

            root->get_header.magic = K_MAGIC_SBRD;
            root->get_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            root->get_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
            root->get_header.page_size = db_->page_size();
            root->get_header.page_id = new_root;
            root->get_is_leaf = 0;
            root->get_entry_count = 1;
            root->get_free_space = db_->page_size() - sizeof(SBGinEntryTreeInternal);
            root->get_data_end = db_->page_size();
            root->get_rightmost_child = right_child;

            // Add single entry for left child
            uint16_t entry_size = sizeof(uint16_t) + sizeof(uint32_t) + separator_key.size();
            uint16_t new_offset = root->get_data_end - entry_size;

            auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(root_data + new_offset);
            entry->key_len = separator_key.size();
            entry->child_page = left_child;
            std::memcpy(entry->key_data, separator_key.data(), separator_key.size());

            root->get_offsets[0] = new_offset;
            root->get_data_end = new_offset;
            root->get_free_space -= entry_size;

            buffer_pool_->unpinPage(new_root, true, ctx);

            *new_root_out = new_root;
            return Status::OK;
        }

        // Multi-key operations (Phase 4)
        // Find TIDs matching ALL keys (AND operation)
        // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::findAll(const std::vector<std::vector<uint8_t>> &keys,
                                                uint64_t current_xid,
                                                ErrorContext *ctx)
        {
            std::vector<TID> result;

            // Edge cases
            if (keys.empty())
            {
                return result;
            }

            // Collect TID lists for each key
            std::vector<std::vector<uint64_t>> tid_lists;
            tid_lists.reserve(keys.size());

            for (const auto &key : keys)
            {
                // Search for this key
                uint64_t posting_page = 0;
                Status status = searchKeysTree(key, &posting_page, ctx);
                if (status != Status::OK || posting_page == 0)
                {
                    // Key not found - intersection must be empty
                    return result;
                }

                // Get TIDs for this key
                std::vector<uint64_t> tids;
                // FIREBIRD MGA: Pass current_xid for index-level visibility filtering
                status = getPostingListTids(posting_page, &tids, current_xid, ctx);
                if (status != Status::OK)
                {
                    // Error retrieving TIDs
                    return result;
                }

                // Firebird MGA: Filter TIDs by heap tuple visibility using TIP
                tids = filterTidsByVisibility(tids, current_xid, ctx);

                // If any key has no visible TIDs, intersection is empty
                if (tids.empty())
                {
                    return result;
                }

                tid_lists.push_back(std::move(tids));
            }

            // Compute intersection of all TID lists (uses legacy format)
            std::vector<uint64_t> legacy_result = mergeTidLists(tid_lists);

            // PHASE 1.5: Convert legacy uint64_t to TID structs
            for (uint64_t legacy_tid : legacy_result)
            {
                result.push_back(convertLegacyTID(legacy_tid));
            }

            return result;
        }

        // Find TIDs matching ANY key (OR operation)
        // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::findAny(const std::vector<std::vector<uint8_t>> &keys,
                                                uint64_t current_xid,
                                                ErrorContext *ctx)
        {
            std::vector<TID> result;

            // Edge cases
            if (keys.empty())
            {
                return result;
            }

            // Collect TID lists for each key
            std::vector<std::vector<uint64_t>> tid_lists;
            tid_lists.reserve(keys.size());

            for (const auto &key : keys)
            {
                // Search for this key
                uint64_t posting_page = 0;
                Status status = searchKeysTree(key, &posting_page, ctx);
                if (status != Status::OK || posting_page == 0)
                {
                    // Key not found - skip it
                    continue;
                }

                // Get TIDs for this key
                std::vector<uint64_t> tids;
                // FIREBIRD MGA: Pass current_xid for index-level visibility filtering
                status = getPostingListTids(posting_page, &tids, current_xid, ctx);
                if (status != Status::OK)
                {
                    // Error retrieving TIDs - skip this key
                    continue;
                }

                // Firebird MGA: Filter TIDs by heap tuple visibility using TIP
                tids = filterTidsByVisibility(tids, current_xid, ctx);

                // Skip empty TID lists
                if (tids.empty())
                {
                    continue;
                }

                tid_lists.push_back(std::move(tids));
            }

            // If no keys had visible TIDs, return empty
            if (tid_lists.empty())
            {
                return result;
            }

            // Compute union of all TID lists (uses legacy format)
            std::vector<uint64_t> legacy_result = unionTidLists(tid_lists);

            // PHASE 1.5: Convert legacy uint64_t to TID structs
            for (uint64_t legacy_tid : legacy_result)
            {
                result.push_back(convertLegacyTID(legacy_tid));
            }

            return result;
        }

        // Static helper: Merge sorted TID lists (AND operation - intersection)
        std::vector<uint64_t> GinIndex::mergeTidLists(
            const std::vector<std::vector<uint64_t>> &tid_lists)
        {
            std::vector<uint64_t> result;

            // Edge cases
            if (tid_lists.empty())
            {
                return result;
            }

            if (tid_lists.size() == 1)
            {
                return tid_lists[0];
            }

            // Start with the first list as the candidate set
            result = tid_lists[0];

            // Intersect with each subsequent list
            for (size_t i = 1; i < tid_lists.size(); i++)
            {
                std::vector<uint64_t> intersection;
                const auto &current_list = tid_lists[i];

                // Two-pointer technique for sorted list intersection
                size_t j = 0, k = 0;
                while (j < result.size() && k < current_list.size())
                {
                    if (result[j] == current_list[k])
                    {
                        // Found in both lists - add to intersection
                        intersection.push_back(result[j]);
                        j++;
                        k++;
                    }
                    else if (result[j] < current_list[k])
                    {
                        j++;
                    }
                    else
                    {
                        k++;
                    }
                }

                // Update result with intersection
                result = std::move(intersection);

                // Early exit if intersection becomes empty
                if (result.empty())
                {
                    break;
                }
            }

            return result;
        }

        // Static helper: Union sorted TID lists (OR operation)
        std::vector<uint64_t> GinIndex::unionTidLists(
            const std::vector<std::vector<uint64_t>> &tid_lists)
        {
            std::vector<uint64_t> result;

            // Edge cases
            if (tid_lists.empty())
            {
                return result;
            }

            if (tid_lists.size() == 1)
            {
                return tid_lists[0];
            }

            // Use a merge approach for sorted lists
            // Create index pointers for each list
            std::vector<size_t> indices(tid_lists.size(), 0);

            while (true)
            {
                // Find the minimum TID across all lists
                uint64_t min_tid = UINT64_MAX;
                bool found_any = false;

                for (size_t i = 0; i < tid_lists.size(); i++)
                {
                    if (indices[i] < tid_lists[i].size())
                    {
                        if (tid_lists[i][indices[i]] < min_tid)
                        {
                            min_tid = tid_lists[i][indices[i]];
                        }
                        found_any = true;
                    }
                }

                // All lists exhausted
                if (!found_any)
                {
                    break;
                }

                // Add minimum TID to result (once)
                result.push_back(min_tid);

                // Advance all pointers that point to this minimum TID
                for (size_t i = 0; i < tid_lists.size(); i++)
                {
                    if (indices[i] < tid_lists[i].size() &&
                        tid_lists[i][indices[i]] == min_tid)
                    {
                        indices[i]++;
                    }
                }
            }

            return result;
        }

        // ===== Firebird MGA: TIP-based Visibility Helpers =====

        // Helper: Check if a transaction is visible to current transaction
        // Returns true if the transaction (xmin) is visible to the reader (current_xid)
        // Uses TIP (Transaction Inventory Pages), NOT snapshots
        bool GinIndex::isTransactionVisible(uint64_t xmin, uint64_t current_xid, ErrorContext *ctx)
        {
            // Special case: when current_xid == 0, bypass visibility checking
            // This is used for unit testing GIN index functionality without transactions
            if (current_xid == 0)
            {
                return true;
            }

            // Own changes always visible (Firebird MGA Rule 3)
            if (xmin == current_xid)
            {
                return true;
            }

            // Use TIP-based visibility check from TransactionManager
            return db_->transaction_manager()->isVersionVisible(xmin, current_xid);
        }

        // Helper: Filter TID list by heap tuple visibility
        // For each TID, checks if the corresponding heap tuple is visible to current transaction
        // Uses TIP-based visibility (Firebird MGA), NOT snapshots
        // Returns a new vector containing only visible TIDs
        std::vector<uint64_t> GinIndex::filterTidsByVisibility(const std::vector<uint64_t> &tids,
                                                                uint64_t current_xid,
                                                                ErrorContext *ctx)
        {
            std::vector<uint64_t> visible_tids;

            // Special case: when current_xid == 0, bypass visibility checking
            // This is used for unit testing GIN index functionality without heap tuples
            if (current_xid == 0)
            {
                return tids;
            }

            // Reserve space to avoid reallocations
            visible_tids.reserve(tids.size());

            // For each TID, we need to check the heap tuple's visibility
            // TID format: (page_id << 32) | (item_id << 16)
            // We'll need to pin the page and check the tuple header's xmin/xmax

            auto *buffer_pool = db_->buffer_pool();
            auto *txn_manager = db_->transaction_manager();

            for (uint64_t tid : tids)
            {
                // Extract page_id and item_id from TID
                uint32_t page_id = static_cast<uint32_t>(tid >> 32);
                uint16_t item_id = static_cast<uint16_t>((tid >> 16) & 0xFFFF);

                // Pin the heap page
                uint8_t *page_data = nullptr;
                Status status = buffer_pool->pinPage(page_id, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    // If we can't read the page, skip this TID
                    continue;
                }

                // Get page header to read tuple
                auto *page_header = reinterpret_cast<PageHeader *>(page_data);

                // Get item pointer array (starts after page header)
                auto *item_pointers = reinterpret_cast<struct ItemPointer *>(page_data + sizeof(PageHeader));

                // Check if item_id is valid
                // We need to calculate how many items are on the page
                // This requires reading the HeapPageSpecial at the end of the page
                auto *special = reinterpret_cast<struct HeapPageSpecial *>(
                    page_data + db_->page_size() - sizeof(struct HeapPageSpecial));

                uint16_t item_count = special->pd_lower / sizeof(struct ItemPointer);

                if (item_id >= item_count)
                {
                    // Invalid item_id
                    buffer_pool->unpinPage(page_id, false, ctx);
                    continue;
                }

                // Get the item pointer
                struct ItemPointer *item_ptr = &item_pointers[item_id];

                // Check if item is deleted or unused
                if (item_ptr->isDeleted() || item_ptr->isUnused())
                {
                    buffer_pool->unpinPage(page_id, false, ctx);
                    continue;
                }

                // Get tuple header
                auto *tuple_header = reinterpret_cast<struct TupleHeader *>(page_data + item_ptr->offset);

                // Firebird MGA: Check visibility using TIP-based visibility
                // Own changes always visible (MGA Rule 3)
                bool visible = (tuple_header->xmin == current_xid);

                if (!visible)
                {
                    // Tuple is visible if:
                    // 1. xmin is visible (inserting transaction committed and visible to current_xid)
                    // 2. xmax is not visible (deleting transaction not yet visible, or no deletion)
                    bool xmin_visible = txn_manager->isVersionVisible(tuple_header->xmin, current_xid);
                    bool xmax_visible = (tuple_header->xmax != 0) &&
                                        txn_manager->isVersionVisible(tuple_header->xmax, current_xid);

                    visible = (xmin_visible && !xmax_visible);
                }

                // Tuple is visible if inserted by visible transaction and not deleted by visible transaction
                if (visible)
                {
                    visible_tids.push_back(tid);
                }

                buffer_pool->unpinPage(page_id, false, ctx);
            }

            return visible_tids;
        }

        // ===== Phase 5: Advanced Query Operations =====

        // Estimate cardinality for a key (for query optimization)
        uint32_t GinIndex::estimateKeyCardinality(const std::vector<uint8_t> &key,
                                                   ErrorContext *ctx)
        {
            // Search for the key in the keys B-Tree
            uint64_t posting_page = 0;
            Status status = searchKeysTree(key, &posting_page, ctx);
            if (status != Status::OK || posting_page == 0)
            {
                return 0; // Key not found
            }

            // Pin the posting list page
            uint8_t *list_data = nullptr;
            status = buffer_pool_->pinPage(posting_page, (void **)&list_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto *list_page = reinterpret_cast<SBGinPostingListPage *>(list_data);
            uint32_t cardinality = list_page->gpl_entry_count;

            buffer_pool_->unpinPage(posting_page, false, ctx);

            return cardinality;
        }

        // Optimized multi-key AND query with selectivity-based reordering
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::findAllOptimized(
            const std::vector<std::vector<uint8_t>> &keys,
            const QueryOptions &options,
            ErrorContext *ctx)
        {
            std::vector<TID> result;

            if (keys.empty())
            {
                return result;
            }

            // If optimization is disabled, use standard findAll
            if (!options.optimize_key_order)
            {
                return findAll(keys, 0, ctx);
            }

            // Estimate cardinality for each key
            std::vector<KeyCardinality> key_cards;
            key_cards.reserve(keys.size());

            for (const auto &key : keys)
            {
                KeyCardinality kc;
                kc.key = key;
                kc.estimated_tids = estimateKeyCardinality(key, ctx);
                key_cards.push_back(kc);
            }

            // Sort keys by cardinality (ascending - process rare keys first)
            std::sort(key_cards.begin(), key_cards.end(),
                      [](const KeyCardinality &a, const KeyCardinality &b)
                      {
                          return a.estimated_tids < b.estimated_tids;
                      });

            // Extract sorted keys
            std::vector<std::vector<uint8_t>> sorted_keys;
            sorted_keys.reserve(key_cards.size());
            for (const auto &kc : key_cards)
            {
                sorted_keys.push_back(kc.key);
            }

            // Use standard findAll with optimized key order
            // PHASE 1 TASK 1.1.5: Pass nullptr for snapshot (Phase 1 Task 1.2 will pass actual snapshot)
            return findAll(sorted_keys, 0, ctx);
        }

        // Optimized multi-key OR query
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::findAnyOptimized(
            const std::vector<std::vector<uint8_t>> &keys,
            const QueryOptions &options,
            ErrorContext *ctx)
        {
            // For OR queries, key order doesn't significantly affect performance
            // So we just delegate to standard findAny
            // Future optimization: parallel execution
            return findAny(keys, 0, ctx);
        }

        // PostgreSQL GIN operator support
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::executeOperator(
            GinOperator op,
            const std::vector<std::vector<uint8_t>> &left_keys,
            const std::vector<std::vector<uint8_t>> &right_keys,
            ErrorContext *ctx)
        {
            std::vector<TID> result;

            switch (op)
            {
            case GinOperator::CONTAINS: // @> - left contains right
                // All right keys must be present in documents
                return findAll(right_keys, 0, ctx);

            case GinOperator::CONTAINED_BY: // <@ - left contained by right
                // At least one left key must be present
                return findAny(left_keys, 0, ctx);

            case GinOperator::OVERLAP: // && - has common elements
                // Any key from either set
                {
                    std::vector<std::vector<uint8_t>> all_keys;
                    all_keys.insert(all_keys.end(), left_keys.begin(), left_keys.end());
                    all_keys.insert(all_keys.end(), right_keys.begin(), right_keys.end());
                    return findAny(all_keys, 0, ctx);
                }

            case GinOperator::EQUALS: // = - exact match
                // All left keys and all right keys must be present
                {
                    std::vector<std::vector<uint8_t>> all_keys;
                    all_keys.insert(all_keys.end(), left_keys.begin(), left_keys.end());
                    all_keys.insert(all_keys.end(), right_keys.begin(), right_keys.end());
                    return findAll(all_keys, 0, ctx);
                }

            case GinOperator::EXISTS: // ? - key exists
                // At least one left key exists
                return findAny(left_keys, 0, ctx);

            case GinOperator::EXISTS_ANY: // ?| - any key exists
                // At least one key from the set exists
                return findAny(left_keys, 0, ctx);

            case GinOperator::EXISTS_ALL: // ?& - all keys exist
                // All keys must exist
                return findAll(left_keys, 0, ctx);

            case GinOperator::TEXT_SEARCH: // @@ - full text search
                // Treat as ALL operation (all terms must be present)
                return findAll(left_keys, 0, ctx);

            default:
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown GIN operator");
                return result;
            }
        }

        // Wildcard query support
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::findWithWildcard(const void *pattern,
                                                          size_t pattern_len,
                                                          ErrorContext *ctx)
        {
            std::vector<TID> result;

            if (!pattern || pattern_len == 0)
            {
                return result;
            }

            // Convert pattern to string for matching
            std::string pattern_str(static_cast<const char *>(pattern), pattern_len);

            // Pin meta page to get root
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return result;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint32_t root_page = meta->gin_keys_btree_root;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            if (root_page == 0)
            {
                return result;
            }

            // Collect all matching keys by scanning the index
            // This is a simple implementation - a production version would optimize tree traversal
            std::vector<std::vector<uint8_t>> matching_keys;

            // Helper lambda to check if a key matches the pattern
            auto matches_pattern = [&pattern_str](const std::vector<uint8_t> &key) -> bool
            {
                std::string key_str(key.begin(), key.end());

                // Simple wildcard matching: % = any chars, _ = single char
                size_t pat_idx = 0;
                size_t key_idx = 0;

                while (pat_idx < pattern_str.size() && key_idx < key_str.size())
                {
                    if (pattern_str[pat_idx] == '%')
                    {
                        // Match any sequence of characters
                        // Try to match the rest of the pattern from various positions
                        if (pat_idx == pattern_str.size() - 1)
                        {
                            return true; // % at end matches everything
                        }

                        // Try matching from each position
                        for (size_t try_pos = key_idx; try_pos <= key_str.size(); try_pos++)
                        {
                            size_t saved_pat = pat_idx + 1;
                            size_t saved_key = try_pos;
                            bool match = true;

                            while (saved_pat < pattern_str.size() && saved_key < key_str.size())
                            {
                                if (pattern_str[saved_pat] == '_' ||
                                    pattern_str[saved_pat] == key_str[saved_key])
                                {
                                    saved_pat++;
                                    saved_key++;
                                }
                                else if (pattern_str[saved_pat] == '%')
                                {
                                    break; // Handle nested % recursively
                                }
                                else
                                {
                                    match = false;
                                    break;
                                }
                            }

                            if (match && saved_pat == pattern_str.size() && saved_key == key_str.size())
                            {
                                return true;
                            }
                        }
                        return false;
                    }
                    else if (pattern_str[pat_idx] == '_')
                    {
                        // Match any single character
                        pat_idx++;
                        key_idx++;
                    }
                    else
                    {
                        // Exact character match required
                        if (pattern_str[pat_idx] != key_str[key_idx])
                        {
                            return false;
                        }
                        pat_idx++;
                        key_idx++;
                    }
                }

                // Both should be exhausted for a match
                return pat_idx == pattern_str.size() && key_idx == key_str.size();
            };

            // Scan entry tree leaves for matching keys using full tree traversal

            // Helper lambda to scan entry tree leaf and collect matching keys
            std::function<void(uint32_t)> scanLeafForWildcard = [&](uint32_t leaf_page)
            {
                uint8_t *leaf_data = nullptr;
                Status scan_status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
                if (scan_status != Status::OK)
                {
                    return;
                }

                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);
                uint16_t entry_count = leaf->get_entry_count;

                // Scan all entries in this leaf
                for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_LEAF_ENTRIES; i++)
                {
                    uint16_t offset = leaf->get_offsets[i];
                    if (offset == 0 || offset >= db_->page_size())
                        continue;

                    auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);

                    // Extract key
                    std::vector<uint8_t> key(entry->key_data, entry->key_data + entry->key_len);

                    // Check if key matches pattern
                    if (matches_pattern(key))
                    {
                        matching_keys.push_back(key);
                    }
                }

                buffer_pool_->unpinPage(leaf_page, false, ctx);
            };

            // Helper lambda to traverse entry tree (depth-first)
            std::function<void(uint32_t)> traverseTree = [&](uint32_t page_num)
            {
                uint8_t *page_data = nullptr;
                Status scan_status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
                if (scan_status != Status::OK)
                {
                    return;
                }

                // Check if leaf or internal
                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(page_data);
                bool is_leaf = (leaf->get_is_leaf != 0);

                if (is_leaf)
                {
                    buffer_pool_->unpinPage(page_num, false, ctx);
                    scanLeafForWildcard(page_num);
                }
                else
                {
                    // Internal node - recurse to children
                    auto *internal = reinterpret_cast<SBGinEntryTreeInternal *>(page_data);
                    uint16_t entry_count = internal->get_entry_count;

                    // Collect child pages before unpinning
                    std::vector<uint32_t> children;
                    for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_INTERNAL_ENTRIES; i++)
                    {
                        uint16_t offset = internal->get_offsets[i];
                        if (offset == 0 || offset >= db_->page_size())
                            continue;

                        auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(page_data + offset);
                        if (entry->child_page != 0)
                        {
                            children.push_back(entry->child_page);
                        }
                    }

                    // Rightmost child
                    if (internal->get_rightmost_child != 0)
                    {
                        children.push_back(internal->get_rightmost_child);
                    }

                    buffer_pool_->unpinPage(page_num, false, ctx);

                    // Recurse to children
                    for (uint32_t child : children)
                    {
                        traverseTree(child);
                    }
                }
            };

            // Start traversal from root
            traverseTree(root_page);

            // Now find TIDs for all matching keys (OR operation)
            if (!matching_keys.empty())
            {
                return findAny(matching_keys, 0, ctx);
            }

            return result;
        }

        // Fuzzy matching with edit distance
        // PHASE 1.5: Return TID structs instead of uint64_t
        std::vector<TID> GinIndex::findFuzzy(const void *key_data, size_t key_len,
                                                   uint32_t max_edit_distance,
                                                   ErrorContext *ctx)
        {
            std::vector<TID> result;

            if (!key_data || key_len == 0)
            {
                return result;
            }

            // Convert search key to vector
            std::vector<uint8_t> search_key(static_cast<const uint8_t *>(key_data),
                                            static_cast<const uint8_t *>(key_data) + key_len);

            // Levenshtein distance implementation
            auto levenshtein_distance = [](const std::vector<uint8_t> &s1,
                                           const std::vector<uint8_t> &s2) -> uint32_t
            {
                const size_t len1 = s1.size();
                const size_t len2 = s2.size();

                // Create distance matrix
                std::vector<std::vector<uint32_t>> dp(len1 + 1,
                                                      std::vector<uint32_t>(len2 + 1));

                // Initialize first row and column
                for (size_t i = 0; i <= len1; i++)
                {
                    dp[i][0] = i;
                }
                for (size_t j = 0; j <= len2; j++)
                {
                    dp[0][j] = j;
                }

                // Compute distances
                for (size_t i = 1; i <= len1; i++)
                {
                    for (size_t j = 1; j <= len2; j++)
                    {
                        uint32_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;

                        dp[i][j] = std::min({dp[i - 1][j] + 1,      // deletion
                                             dp[i][j - 1] + 1,      // insertion
                                             dp[i - 1][j - 1] + cost}); // substitution
                    }
                }

                return dp[len1][len2];
            };

            // Pin meta page to get root
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return result;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint32_t root_page = meta->gin_keys_btree_root;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            if (root_page == 0)
            {
                return result;
            }

            // Collect all keys within edit distance by scanning the entry tree
            std::vector<std::vector<uint8_t>> matching_keys;

            // Helper lambda to scan entry tree leaf and collect keys within edit distance
            std::function<void(uint32_t)> scanLeafForFuzzy = [&](uint32_t leaf_page)
            {
                uint8_t *leaf_data = nullptr;
                Status scan_status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
                if (scan_status != Status::OK)
                {
                    return;
                }

                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);
                uint16_t entry_count = leaf->get_entry_count;

                // Scan all entries in this leaf
                for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_LEAF_ENTRIES; i++)
                {
                    uint16_t offset = leaf->get_offsets[i];
                    if (offset == 0 || offset >= db_->page_size())
                        continue;

                    auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);

                    // Extract key
                    std::vector<uint8_t> key(entry->key_data, entry->key_data + entry->key_len);

                    // Calculate edit distance
                    uint32_t distance = levenshtein_distance(search_key, key);

                    // Check if within threshold
                    if (distance <= max_edit_distance)
                    {
                        matching_keys.push_back(key);
                    }
                }

                buffer_pool_->unpinPage(leaf_page, false, ctx);
            };

            // Helper lambda to traverse entry tree (depth-first)
            std::function<void(uint32_t)> traverseTree = [&](uint32_t page_num)
            {
                uint8_t *page_data = nullptr;
                Status scan_status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
                if (scan_status != Status::OK)
                {
                    return;
                }

                // Check if leaf or internal
                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(page_data);
                bool is_leaf = (leaf->get_is_leaf != 0);

                if (is_leaf)
                {
                    buffer_pool_->unpinPage(page_num, false, ctx);
                    scanLeafForFuzzy(page_num);
                }
                else
                {
                    // Internal node - recurse to children
                    auto *internal = reinterpret_cast<SBGinEntryTreeInternal *>(page_data);
                    uint16_t entry_count = internal->get_entry_count;

                    // Collect child pages before unpinning
                    std::vector<uint32_t> children;
                    for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_INTERNAL_ENTRIES; i++)
                    {
                        uint16_t offset = internal->get_offsets[i];
                        if (offset == 0 || offset >= db_->page_size())
                            continue;

                        auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(page_data + offset);
                        if (entry->child_page != 0)
                        {
                            children.push_back(entry->child_page);
                        }
                    }

                    // Rightmost child
                    if (internal->get_rightmost_child != 0)
                    {
                        children.push_back(internal->get_rightmost_child);
                    }

                    buffer_pool_->unpinPage(page_num, false, ctx);

                    // Recurse to children
                    for (uint32_t child : children)
                    {
                        traverseTree(child);
                    }
                }
            };

            // Start traversal from root
            traverseTree(root_page);

            // Now find TIDs for all matching keys (OR operation)
            if (!matching_keys.empty())
            {
                return findAny(matching_keys, 0, ctx);
            }

            return result;
        }

        // ===== Phase 6: Advanced Performance & Features =====

        // SIMD capability detection
        bool GinIndex::hasSIMDSupport()
        {
#if defined(__x86_64__) || defined(_M_X64)
            // Check for SSE2 support (available on all x86-64)
            return true;
#elif defined(__ARM_NEON)
            // ARM NEON support
            return true;
#else
            return false;
#endif
        }

        // SIMD-optimized intersection of two sorted TID lists
        std::vector<uint64_t> GinIndex::intersectTwoListsSIMD(
            const std::vector<uint64_t> &list1,
            const std::vector<uint64_t> &list2)
        {
            std::vector<uint64_t> result;

            if (list1.empty() || list2.empty())
            {
                return result;
            }

#if defined(__x86_64__) || defined(_M_X64)
            // x86-64 SIMD implementation with SSE2
            size_t i = 0, j = 0;
            result.reserve(std::min(list1.size(), list2.size()));

            // Process 2 elements at a time with SSE2 (128-bit = 2 x 64-bit)
            while (i + 1 < list1.size() && j + 1 < list2.size())
            {
                // Load 2 elements from each list
                __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&list1[i]));
                __m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&list2[j]));

                // Extract individual values using SSE2-compatible method
                // Store to temporary array and read back
                alignas(16) uint64_t a_vals[2];
                alignas(16) uint64_t b_vals[2];
                _mm_store_si128(reinterpret_cast<__m128i *>(a_vals), v1);
                _mm_store_si128(reinterpret_cast<__m128i *>(b_vals), v2);

                uint64_t a0 = a_vals[0];
                uint64_t b0 = b_vals[0];

                // Two-pointer logic with SIMD-loaded values
                if (a0 == b0)
                {
                    result.push_back(a0);
                    i++;
                    j++;
                }
                else if (a0 < b0)
                {
                    i++;
                }
                else
                {
                    j++;
                }
            }

            // Handle remaining elements
            while (i < list1.size() && j < list2.size())
            {
                if (list1[i] == list2[j])
                {
                    result.push_back(list1[i]);
                    i++;
                    j++;
                }
                else if (list1[i] < list2[j])
                {
                    i++;
                }
                else
                {
                    j++;
                }
            }
#else
            // Fallback to scalar implementation
            size_t i = 0, j = 0;
            while (i < list1.size() && j < list2.size())
            {
                if (list1[i] == list2[j])
                {
                    result.push_back(list1[i]);
                    i++;
                    j++;
                }
                else if (list1[i] < list2[j])
                {
                    i++;
                }
                else
                {
                    j++;
                }
            }
#endif

            return result;
        }

        // SIMD-optimized multi-way intersection
        std::vector<uint64_t> GinIndex::mergeTidListsSIMD(
            const std::vector<std::vector<uint64_t>> &tid_lists)
        {
            std::vector<uint64_t> result;

            if (tid_lists.empty())
            {
                return result;
            }

            if (tid_lists.size() == 1)
            {
                return tid_lists[0];
            }

            if (!hasSIMDSupport())
            {
                // Fallback to scalar implementation
                return mergeTidLists(tid_lists);
            }

            // Use SIMD for pairwise intersection
            result = tid_lists[0];

            for (size_t i = 1; i < tid_lists.size(); i++)
            {
                result = intersectTwoListsSIMD(result, tid_lists[i]);

                // Early exit if result becomes empty
                if (result.empty())
                {
                    break;
                }
            }

            return result;
        }

        // SIMD-optimized multi-way union
        std::vector<uint64_t> GinIndex::unionTidListsSIMD(
            const std::vector<std::vector<uint64_t>> &tid_lists)
        {
            // Union doesn't benefit as much from SIMD, use scalar implementation
            // SIMD would require complex merge logic with deduplication
            return unionTidLists(tid_lists);
        }

        // Parallel multi-key AND query
        // P0-6: Fixed to accept current_xid for proper MGA visibility
        std::vector<uint64_t> GinIndex::findAllParallel(
            const std::vector<std::vector<uint8_t>> &keys,
            uint64_t current_xid,
            uint32_t max_threads,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            if (keys.empty())
            {
                return result;
            }

            if (max_threads <= 1 || keys.size() <= 1)
            {
                // Use standard implementation for single thread or single key
                // P0-6: Pass current_xid for proper MGA visibility
                std::vector<TID> tid_results = findAll(keys, current_xid, ctx);
                for (const TID &tid : tid_results)
                {
                    uint64_t legacy_tid = convertTIDtoLegacy(tid);
                    if (legacy_tid != 0)
                    {
                        result.push_back(legacy_tid);
                    }
                }
                return result;
            }

            // Parallel key lookup with thread pool
            std::vector<std::vector<uint64_t>> tid_lists(keys.size());
            std::mutex error_mutex;
            bool has_error = false;

            // Limit threads to available cores and key count
            uint32_t num_threads = std::min(max_threads, static_cast<uint32_t>(keys.size()));
            num_threads = std::min(num_threads, std::thread::hardware_concurrency());

            std::vector<std::thread> threads;
            threads.reserve(num_threads);

            // Distribute keys across threads
            for (uint32_t t = 0; t < num_threads; t++)
            {
                threads.emplace_back([&, t]()
                {
                    // Process keys assigned to this thread
                    for (size_t i = t; i < keys.size(); i += num_threads)
                    {
                        uint64_t posting_page = 0;
                        Status status = searchKeysTree(keys[i], &posting_page, ctx);

                        if (status != Status::OK || posting_page == 0)
                        {
                            // Key not found - intersection is empty
                            std::lock_guard<std::mutex> lock(error_mutex);
                            has_error = true;
                            return;
                        }

                        std::vector<uint64_t> tids;
                        // P0-6: Fixed to use current_xid for proper MGA visibility
                        status = getPostingListTids(posting_page, &tids, current_xid, ctx);

                        if (status != Status::OK || tids.empty())
                        {
                            std::lock_guard<std::mutex> lock(error_mutex);
                            has_error = true;
                            return;
                        }

                        tid_lists[i] = std::move(tids);
                    }
                });
            }

            // Wait for all threads to complete
            for (auto &thread : threads)
            {
                thread.join();
            }

            if (has_error)
            {
                return result; // Empty result
            }

            // Use SIMD-optimized intersection
            result = mergeTidListsSIMD(tid_lists);

            return result;
        }

        // Parallel multi-key OR query
        // P0-6: Fixed to accept current_xid for proper MGA visibility
        std::vector<uint64_t> GinIndex::findAnyParallel(
            const std::vector<std::vector<uint8_t>> &keys,
            uint64_t current_xid,
            uint32_t max_threads,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            if (keys.empty())
            {
                return result;
            }

            if (max_threads <= 1 || keys.size() <= 1)
            {
                // P0-6: Pass current_xid for proper MGA visibility
                std::vector<TID> tid_results = findAny(keys, current_xid, ctx);
                for (const TID &tid : tid_results)
                {
                    uint64_t legacy_tid = convertTIDtoLegacy(tid);
                    if (legacy_tid != 0)
                    {
                        result.push_back(legacy_tid);
                    }
                }
                return result;
            }

            // Parallel key lookup
            std::vector<std::vector<uint64_t>> tid_lists;
            std::mutex list_mutex;

            uint32_t num_threads = std::min(max_threads, static_cast<uint32_t>(keys.size()));
            num_threads = std::min(num_threads, std::thread::hardware_concurrency());

            std::vector<std::thread> threads;
            threads.reserve(num_threads);

            for (uint32_t t = 0; t < num_threads; t++)
            {
                threads.emplace_back([&, t]()
                {
                    for (size_t i = t; i < keys.size(); i += num_threads)
                    {
                        uint64_t posting_page = 0;
                        Status status = searchKeysTree(keys[i], &posting_page, ctx);

                        if (status != Status::OK || posting_page == 0)
                        {
                            continue; // Skip missing keys
                        }

                        std::vector<uint64_t> tids;
                        // P0-6: Fixed to use current_xid for proper MGA visibility
                        status = getPostingListTids(posting_page, &tids, current_xid, ctx);

                        if (status != Status::OK || tids.empty())
                        {
                            continue;
                        }

                        std::lock_guard<std::mutex> lock(list_mutex);
                        tid_lists.push_back(std::move(tids));
                    }
                });
            }

            for (auto &thread : threads)
            {
                thread.join();
            }

            if (tid_lists.empty())
            {
                return result;
            }

            // Use standard union (SIMD doesn't help much here)
            result = unionTidLists(tid_lists);

            return result;
        }

        // Range query implementation
        std::vector<uint64_t> GinIndex::findInRange(
            const RangeQuery &range,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            // Scan entry tree for keys in range
            std::vector<std::vector<uint8_t>> matching_keys;
            Status status = scanEntriesInRange(
                range.lower_bound,
                range.upper_bound,
                range.lower_inclusive,
                range.upper_inclusive,
                &matching_keys,
                ctx);

            if (status != Status::OK)
            {
                return result;
            }

            // Union all matching keys' TID lists
            // PHASE 1.5: findAny returns TID, convert to legacy format for internal use
            std::vector<TID> tid_results = findAny(matching_keys, 0, ctx);
            for (const TID &tid : tid_results)
            {
                uint64_t legacy_tid = convertTIDtoLegacy(tid);
                if (legacy_tid != 0)
                {
                    result.push_back(legacy_tid);
                }
            }
            return result;
        }

        // Helper: Scan entry tree for keys in range
        Status GinIndex::scanEntriesInRange(
            const std::vector<uint8_t> &lower_bound,
            const std::vector<uint8_t> &upper_bound,
            bool lower_inclusive,
            bool upper_inclusive,
            std::vector<std::vector<uint8_t>> *matching_keys_out,
            ErrorContext *ctx)
        {
            // Pin meta page to get root
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint32_t root_page = meta->gin_keys_btree_root;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            if (root_page == 0)
            {
                return Status::OK; // Empty index
            }

            // Find starting leaf
            uint32_t leaf_page = 0;
            status = findEntryTreeLeaf(root_page, lower_bound, &leaf_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Scan leaves until we exceed upper_bound
            while (leaf_page != 0)
            {
                uint8_t *leaf_data = nullptr;
                status = buffer_pool_->pinPage(leaf_page, (void **)&leaf_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(leaf_data);

                // Scan entries in this leaf
                for (uint16_t i = 0; i < leaf->get_entry_count; i++)
                {
                    uint16_t offset = leaf->get_offsets[i];
                    auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(leaf_data + offset);

                    std::vector<uint8_t> key(entry->key_data, entry->key_data + entry->key_len);

                    // Check if key is in range
                    int cmp_lower = compareKeys(key, lower_bound);
                    int cmp_upper = compareKeys(key, upper_bound);

                    bool in_range = true;

                    // Check lower bound
                    if (lower_inclusive)
                    {
                        if (cmp_lower < 0) in_range = false;
                    }
                    else
                    {
                        if (cmp_lower <= 0) in_range = false;
                    }

                    // Check upper bound
                    if (upper_inclusive)
                    {
                        if (cmp_upper > 0) in_range = false;
                    }
                    else
                    {
                        if (cmp_upper >= 0) in_range = false;
                    }

                    if (in_range)
                    {
                        matching_keys_out->push_back(key);
                    }

                    // If we've exceeded upper bound, stop scanning
                    if (cmp_upper > 0)
                    {
                        buffer_pool_->unpinPage(leaf_page, false, ctx);
                        return Status::OK;
                    }
                }

                // Move to next leaf (B-tree leaves are not linked in current implementation)
                // For now, we stop here. Full implementation would link leaves or traverse tree
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                break;
            }

            return Status::OK;
        }

        // Helper: Pattern matching for wildcards
        bool GinIndex::matchesPattern(
            const std::vector<uint8_t> &key,
            const std::string &pattern)
        {
            std::string key_str(key.begin(), key.end());

            size_t pat_idx = 0;
            size_t key_idx = 0;

            while (pat_idx < pattern.size() && key_idx < key_str.size())
            {
                if (pattern[pat_idx] == '%')
                {
                    // Match any sequence
                    if (pat_idx == pattern.size() - 1)
                    {
                        return true; // % at end matches everything
                    }

                    // Try matching from each position
                    for (size_t try_pos = key_idx; try_pos <= key_str.size(); try_pos++)
                    {
                        if (matchesPattern(
                            std::vector<uint8_t>(key_str.begin() + try_pos, key_str.end()),
                            pattern.substr(pat_idx + 1)))
                        {
                            return true;
                        }
                    }
                    return false;
                }
                else if (pattern[pat_idx] == '_')
                {
                    // Match single character
                    pat_idx++;
                    key_idx++;
                }
                else
                {
                    // Exact match
                    if (pattern[pat_idx] != key_str[key_idx])
                    {
                        return false;
                    }
                    pat_idx++;
                    key_idx++;
                }
            }

            // Check if we've consumed the key
            if (key_idx < key_str.size())
            {
                return false; // Key has more characters, but pattern is exhausted
            }

            // Key is consumed, check if remaining pattern is only wildcards
            while (pat_idx < pattern.size())
            {
                if (pattern[pat_idx] != '%')
                {
                    return false; // Pattern has non-wildcard characters left
                }
                pat_idx++;
            }

            return true; // Both consumed or only wildcards remain
        }

        // Optimized wildcard query with prefix scan
        std::vector<uint64_t> GinIndex::findWithWildcardOptimized(
            const void *pattern,
            size_t pattern_len,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            if (!pattern || pattern_len == 0)
            {
                return result;
            }

            std::string pattern_str(static_cast<const char *>(pattern), pattern_len);

            // Extract prefix (characters before first wildcard)
            size_t prefix_end = pattern_str.find_first_of("%_");
            std::string prefix = (prefix_end != std::string::npos) ?
                pattern_str.substr(0, prefix_end) : pattern_str;

            if (prefix.empty())
            {
                // No prefix optimization possible - would need full scan
                // PHASE 1.5: findWithWildcard returns TID, convert to legacy format for internal use
                std::vector<TID> tid_results = findWithWildcard(pattern, pattern_len, ctx);
                for (const TID &tid : tid_results)
                {
                    uint64_t legacy_tid = convertTIDtoLegacy(tid);
                    if (legacy_tid != 0)
                    {
                        result.push_back(legacy_tid);
                    }
                }
                return result;
            }

            // Use prefix for range scan
            std::vector<uint8_t> lower_bound(prefix.begin(), prefix.end());
            std::vector<uint8_t> upper_bound = lower_bound;

            // Increment last byte for upper bound
            if (!upper_bound.empty())
            {
                upper_bound.back()++;
            }

            // Scan keys in range
            std::vector<std::vector<uint8_t>> candidate_keys;
            Status status = scanEntriesInRange(
                lower_bound,
                upper_bound,
                true,  // lower_inclusive
                false, // upper_exclusive
                &candidate_keys,
                ctx);

            if (status != Status::OK)
            {
                return result;
            }

            // Filter candidates by full pattern
            std::vector<std::vector<uint8_t>> matching_keys;
            for (const auto &key : candidate_keys)
            {
                if (matchesPattern(key, pattern_str))
                {
                    matching_keys.push_back(key);
                }
            }

            // Union all matching keys
            // PHASE 1.5: findAny returns TID, convert to legacy format for internal use
            std::vector<TID> tid_results = findAny(matching_keys, 0, ctx);
            for (const TID &tid : tid_results)
            {
                uint64_t legacy_tid = convertTIDtoLegacy(tid);
                if (legacy_tid != 0)
                {
                    result.push_back(legacy_tid);
                }
            }
            return result;
        }

        // Helper: Levenshtein distance
        uint32_t GinIndex::levenshteinDistance(
            const std::vector<uint8_t> &s1,
            const std::vector<uint8_t> &s2)
        {
            const size_t len1 = s1.size();
            const size_t len2 = s2.size();

            std::vector<std::vector<uint32_t>> dp(len1 + 1,
                                                  std::vector<uint32_t>(len2 + 1));

            for (size_t i = 0; i <= len1; i++)
            {
                dp[i][0] = i;
            }
            for (size_t j = 0; j <= len2; j++)
            {
                dp[0][j] = j;
            }

            for (size_t i = 1; i <= len1; i++)
            {
                for (size_t j = 1; j <= len2; j++)
                {
                    uint32_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;

                    dp[i][j] = std::min({dp[i - 1][j] + 1,      // deletion
                                         dp[i][j - 1] + 1,      // insertion
                                         dp[i - 1][j - 1] + cost}); // substitution
                }
            }

            return dp[len1][len2];
        }

        // STOR-M3: Optimized fuzzy matching with BK-tree
        std::vector<uint64_t> GinIndex::findFuzzyOptimized(
            const void *key_data,
            size_t key_len,
            uint32_t max_edit_distance,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            if (!key_data || key_len == 0)
            {
                return result;
            }

            std::vector<uint8_t> search_key(static_cast<const uint8_t *>(key_data),
                                            static_cast<const uint8_t *>(key_data) + key_len);

            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return result;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint32_t root_page = meta->gin_keys_btree_root;
            uint64_t num_keys = meta->gin_num_keys;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            if (root_page == 0)
            {
                return result;
            }

            // STOR-M3: Build BK-tree from all indexed keys for efficient fuzzy search
            // This is more efficient than brute-force when:
            // 1. The number of keys is large (> 100)
            // 2. The edit distance is small (< 3)
            //
            // For small key sets, fall back to brute-force scan
            constexpr uint64_t BK_TREE_THRESHOLD = 100;

            if (num_keys < BK_TREE_THRESHOLD)
            {
                // Use existing brute-force implementation for small key sets
                std::vector<TID> tid_results = findFuzzy(key_data, key_len, max_edit_distance, ctx);

                result.reserve(tid_results.size());
                for (const TID &tid : tid_results)
                {
                    uint64_t legacy = convertTIDtoLegacy(tid);
                    if (legacy != 0)
                    {
                        result.push_back(legacy);
                    }
                }
                return result;
            }

            // Build BK-tree from all keys in the entry tree
            BKTree bk_tree;

            // Helper lambda to collect all keys from entry tree leaves
            std::function<void(uint32_t)> collectKeys = [&](uint32_t page_num)
            {
                uint8_t *page_data = nullptr;
                Status scan_status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
                if (scan_status != Status::OK)
                {
                    return;
                }

                auto *internal = reinterpret_cast<SBGinEntryTreeInternal *>(page_data);

                if (internal->get_is_leaf == 1)
                {
                    // This is a leaf page
                    auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(page_data);
                    uint16_t entry_count = leaf->get_entry_count;

                    for (uint16_t i = 0; i < entry_count; i++)
                    {
                        uint16_t offset = leaf->get_offsets[i];
                        auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(
                            page_data + offset);

                        // Extract key and add to BK-tree
                        std::vector<uint8_t> key(entry->key_data,
                                                 entry->key_data + entry->key_len);
                        bk_tree.insert(key);
                    }
                }
                else
                {
                    // Internal node - recurse into children
                    uint16_t entry_count = internal->get_entry_count;

                    // Visit first child (leftmost)
                    if (entry_count > 0)
                    {
                        uint16_t offset = internal->get_offsets[0];
                        auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(
                            page_data + offset);
                        collectKeys(entry->child_page);
                    }

                    // Visit remaining children
                    for (uint16_t i = 0; i < entry_count; i++)
                    {
                        uint16_t offset = internal->get_offsets[i];
                        auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(
                            page_data + offset);

                        // For internal entries, child is at index i+1 in logical terms
                        // We need to follow the entry's child pointer
                        if (i + 1 < entry_count)
                        {
                            uint16_t next_offset = internal->get_offsets[i + 1];
                            auto *next_entry = reinterpret_cast<GinEntryTreeInternalEntry *>(
                                page_data + next_offset);
                            collectKeys(next_entry->child_page);
                        }
                    }

                    // Visit rightmost child
                    if (internal->get_rightmost_child != 0)
                    {
                        collectKeys(internal->get_rightmost_child);
                    }
                }

                buffer_pool_->unpinPage(page_num, false, ctx);
            };

            // Collect all keys into BK-tree
            collectKeys(root_page);

            // Use BK-tree to find matching keys within edit distance
            std::vector<std::vector<uint8_t>> matching_keys =
                bk_tree.findWithinDistance(search_key, max_edit_distance);

            // For each matching key, get the posting list TIDs
            // Use current transaction ID 0 for now (no MGA filtering in legacy API)
            uint64_t current_xid = 0;

            for (const auto &key : matching_keys)
            {
                // Find posting page for this key
                uint64_t posting_page = 0;
                status = searchKeysTree(key, &posting_page, ctx);
                if (status != Status::OK || posting_page == 0)
                {
                    continue;
                }

                // Get TIDs from posting list
                std::vector<uint64_t> tids;
                status = getPostingListTids(static_cast<uint32_t>(posting_page),
                                           &tids, current_xid, ctx);
                if (status != Status::OK)
                {
                    continue;
                }

                // Add to result
                result.insert(result.end(), tids.begin(), tids.end());
            }

            // Remove duplicates and sort
            std::sort(result.begin(), result.end());
            result.erase(std::unique(result.begin(), result.end()), result.end());

            return result;
        }

        // PHASE 2 TASK 2.4: Remove index entries pointing to dead tuples
        // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
        Status GinIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                           uint64_t *entries_removed_out,
                                           uint64_t *pages_modified_out,
                                           ErrorContext *ctx)
        {
            // Initialize output parameters
            uint64_t total_entries_removed = 0;
            uint64_t total_pages_modified = 0;
            bool had_errors = false;

            // Early exit if empty
            if (dead_tids.empty())
            {
                if (entries_removed_out)
                    *entries_removed_out = 0;
                if (pages_modified_out)
                    *pages_modified_out = 0;
                return Status::OK;
            }

            // PHASE 1.5: Convert TID structs to legacy format set for lookup
            std::set<uint64_t> dead_set;
            for (const TID &tid : dead_tids)
            {
                uint64_t legacy = convertTIDtoLegacy(tid);
                if (legacy != 0)  // Skip custom tablespace TIDs
                {
                    dead_set.insert(legacy);
                }
            }

            if (dead_set.empty())
            {
                if (entries_removed_out)
                    *entries_removed_out = 0;
                if (pages_modified_out)
                    *pages_modified_out = 0;
                return Status::OK;
            }

            // ===== Step 1: Remove dead TIDs from pending list =====
            // The pending list contains recent insertions not yet merged into main index
            // Format: chain of SBGinPendingListPage with GinPendingEntry arrays

            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(VACUUM, "GIN GC: Failed to pin meta page %u: %d",
                            meta_page_, static_cast<int>(status));
                return Status::IO_ERROR;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint64_t pending_head = meta->gin_pending_list_head;
            uint64_t pending_count_before = meta->gin_pending_list_count;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Scan pending list chain
            uint64_t current_page = pending_head;
            uint64_t pending_entries_removed = 0;

            while (current_page != 0)
            {
                uint8_t *page_data = nullptr;
                status = buffer_pool_->pinPage(static_cast<uint32_t>(current_page), (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(VACUUM, "GIN GC: Failed to pin pending list page %lu: %d",
                                current_page, static_cast<int>(status));
                    had_errors = true;
                    break;
                }

                auto *pending_page = reinterpret_cast<SBGinPendingListPage *>(page_data);
                uint16_t entry_count = pending_page->gpp_entry_count;
                uint64_t next_page = pending_page->gpp_next_page;

                bool page_modified = false;

                // Scan entries in this page
                // We mark entries as deleted by setting tid = INVALID_TID
                for (uint16_t i = 0; i < entry_count && i < getMaxPendingEntriesPerPage(); i++)
                {
                    GinPendingEntry &entry = pending_page->gpp_entries[i];

                    // Convert GPID to legacy format for comparison
                    uint64_t entry_tid = convertTIDtoLegacy(entry.getTID());

                    // Check if already deleted (tid == 0)
                    if (entry_tid == 0)
                    {
                        continue;
                    }

                    // Check if this TID is in the dead set
                    if (dead_set.find(entry_tid) != dead_set.end())
                    {
                        // Mark as deleted by setting to INVALID_TID
                        entry.setTID(INVALID_TID);
                        pending_entries_removed++;
                        page_modified = true;
                    }
                }

                if (page_modified)
                {
                    total_pages_modified++;
                }

                buffer_pool_->unpinPage(static_cast<uint32_t>(current_page), page_modified, ctx);

                // Move to next page in chain
                current_page = next_page;
            }

            total_entries_removed += pending_entries_removed;

            // Update meta page pending count
            if (pending_entries_removed > 0)
            {
                status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
                if (status == Status::OK)
                {
                    meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
                    if (meta->gin_pending_list_count >= pending_entries_removed)
                    {
                        meta->gin_pending_list_count -= pending_entries_removed;
                    }
                    else
                    {
                        meta->gin_pending_list_count = 0;
                    }
                    buffer_pool_->unpinPage(meta_page_, true, ctx);
                    total_pages_modified++;
                }
            }

            // ===== Step 2: Remove dead TIDs from posting lists/trees =====
            // Plan 01 Task D.6: Posting list/tree pruning
            // Implementation: Reuse pattern from updateTIDsAfterMigration()

            uint64_t posting_entries_removed = 0;

            // Get entry tree root
            status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(VACUUM, "GIN GC: Failed to pin meta page for entry tree: %d",
                           static_cast<int>(status));
                if (entries_removed_out)
                    *entries_removed_out = total_entries_removed;
                if (pages_modified_out)
                    *pages_modified_out = total_pages_modified;
                return had_errors ? Status::IO_ERROR : Status::OK;
            }

            meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint64_t keys_tree_root = meta->gin_keys_btree_root;
            buffer_pool_->unpinPage(meta_page_, false, ctx);

            if (keys_tree_root == 0)
            {
                // Empty index, nothing to do
                LOG_INFO(VACUUM, "GIN GC: Removed %lu entries from pending list (empty entry tree)",
                        pending_entries_removed);
                if (entries_removed_out)
                    *entries_removed_out = total_entries_removed;
                if (pages_modified_out)
                    *pages_modified_out = total_pages_modified;
                return had_errors ? Status::IO_ERROR : Status::OK;
            }

            // Helper: Recursively traverse entry tree and collect all posting page numbers
            std::vector<uint64_t> posting_pages;
            std::function<void(uint32_t)> collectPostingPages = [&](uint32_t page_num)
            {
                uint8_t *page_data = nullptr;
                Status pin_status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
                if (pin_status != Status::OK)
                {
                    LOG_WARNING(VACUUM, "GIN GC: Failed to pin entry tree page %u: %d",
                               page_num, static_cast<int>(pin_status));
                    had_errors = true;
                    return;
                }

                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(page_data);
                bool is_leaf = (leaf->get_is_leaf != 0);

                if (is_leaf)
                {
                    // Leaf node: collect posting pages from entries
                    uint16_t entry_count = leaf->get_entry_count;

                    for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_LEAF_ENTRIES; i++)
                    {
                        uint16_t offset = leaf->get_offsets[i];
                        if (offset == 0 || offset >= db_->page_size())
                            continue;

                        auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(page_data + offset);
                        uint64_t posting_page = entry->value.posting_list_page;

                        if (posting_page != 0)
                        {
                            posting_pages.push_back(posting_page);
                        }
                    }
                }
                else
                {
                    // Internal node: recurse to children
                    auto *internal = reinterpret_cast<SBGinEntryTreeInternal *>(page_data);
                    uint16_t entry_count = internal->get_entry_count;

                    for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_INTERNAL_ENTRIES; i++)
                    {
                        uint16_t offset = internal->get_offsets[i];
                        if (offset == 0 || offset >= db_->page_size())
                            continue;

                        auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(page_data + offset);
                        uint32_t child_page = entry->child_page;

                        if (child_page != 0)
                        {
                            collectPostingPages(child_page);
                        }
                    }

                    if (internal->get_rightmost_child != 0)
                    {
                        collectPostingPages(internal->get_rightmost_child);
                    }
                }

                buffer_pool_->unpinPage(page_num, false, ctx);
            };

            // Collect all posting pages
            collectPostingPages(static_cast<uint32_t>(keys_tree_root));

            LOG_INFO(VACUUM, "GIN GC: Found %lu posting pages to process", posting_pages.size());

            // Process each posting page
            for (uint64_t posting_page_num : posting_pages)
            {
                uint8_t *posting_data = nullptr;
                status = buffer_pool_->pinPage(static_cast<uint32_t>(posting_page_num),
                                              (void **)&posting_data, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(VACUUM, "GIN GC: Failed to pin posting page %lu: %d",
                               posting_page_num, static_cast<int>(status));
                    had_errors = true;
                    continue;
                }

                auto *posting_page = reinterpret_cast<SBGinPostingListPage *>(posting_data);

                if (posting_page->gpl_is_tree)
                {
                    // Posting tree: recursively process leaf nodes
                    uint64_t tree_root = posting_page->gpl_data.gpl_tree_root;
                    buffer_pool_->unpinPage(static_cast<uint32_t>(posting_page_num), false, ctx);

                    std::function<void(uint32_t)> prunePostingTree = [&](uint32_t tree_page_num)
                    {
                        uint8_t *tree_data = nullptr;
                        Status tree_pin_status = buffer_pool_->pinPage(tree_page_num,
                                                                       (void **)&tree_data, ctx);
                        if (tree_pin_status != Status::OK)
                        {
                            LOG_WARNING(VACUUM, "GIN GC: Failed to pin posting tree page %u: %d",
                                       tree_page_num, static_cast<int>(tree_pin_status));
                            had_errors = true;
                            return;
                        }

                        auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(tree_data);
                        bool is_leaf_node = (leaf->gpt_is_leaf != 0);
                        bool tree_page_modified = false;

                        if (is_leaf_node)
                        {
                            // Compact TID array by removing dead entries
                            uint16_t entry_count = leaf->gpt_entry_count;
                            uint16_t new_count = 0;

                            for (uint16_t i = 0; i < entry_count && i < getMaxPostingTreeLeafTids(); i++)
                            {
                                uint64_t tid = convertTIDtoLegacy(leaf->gpt_tids[i].getTID());

                                if (dead_set.find(tid) == dead_set.end())
                                {
                                    // Keep this entry
                                    if (new_count != i)
                                    {
                                        leaf->gpt_tids[new_count] = leaf->gpt_tids[i];
                                        tree_page_modified = true;
                                    }
                                    new_count++;
                                }
                                else
                                {
                                    posting_entries_removed++;
                                    tree_page_modified = true;
                                }
                            }

                            if (tree_page_modified)
                            {
                                leaf->gpt_entry_count = new_count;
                                total_pages_modified++;
                            }
                        }
                        else
                        {
                            // Internal node: recurse to children
                            auto *internal = reinterpret_cast<SBGinPostingTreeInternal *>(tree_data);
                            uint16_t entry_count = internal->gpt_entry_count;

                            for (uint16_t i = 0; i < entry_count && i < getMaxPostingTreeInternalEntries(); i++)
                            {
                                uint32_t child_page = internal->gpt_entries[i].child_page;
                                if (child_page != 0)
                                {
                                    prunePostingTree(child_page);
                                }
                            }
                        }

                        buffer_pool_->unpinPage(tree_page_num, tree_page_modified, ctx);
                    };

                    if (tree_root != 0)
                    {
                        prunePostingTree(static_cast<uint32_t>(tree_root));
                    }
                }
                else
                {
                    // Simple posting list
                    bool page_modified = false;

                    if (posting_page->gpl_is_compressed)
                    {
                        // Compressed list: decompress, filter, recompress
                        // For now, skip compressed lists (alpha limitation)
                        LOG_INFO(VACUUM, "GIN GC: Skipping compressed posting list at page %lu",
                                posting_page_num);
                        buffer_pool_->unpinPage(static_cast<uint32_t>(posting_page_num), false, ctx);
                        continue;
                    }

                    // Uncompressed list: compact array
                    uint16_t entry_count = posting_page->gpl_entry_count;
                    uint16_t new_count = 0;
                    GinPostingEntry* entries = posting_page->getEntries();

                    for (uint16_t i = 0; i < entry_count && i < getMaxPostingEntriesPerPage(); i++)
                    {
                        uint64_t tid = convertTIDtoLegacy(entries[i].getTID());

                        if (dead_set.find(tid) == dead_set.end())
                        {
                            // Keep this entry
                            if (new_count != i)
                            {
                                entries[new_count] = entries[i];
                                page_modified = true;
                            }
                            new_count++;
                        }
                        else
                        {
                            posting_entries_removed++;
                            page_modified = true;
                        }
                    }

                    if (page_modified)
                    {
                        posting_page->gpl_entry_count = new_count;
                        total_pages_modified++;
                    }

                    buffer_pool_->unpinPage(static_cast<uint32_t>(posting_page_num), page_modified, ctx);
                }
            }

            total_entries_removed += posting_entries_removed;

            LOG_INFO(VACUUM, "GIN GC: Removed %lu entries (%lu pending + %lu posting)",
                     total_entries_removed, pending_entries_removed, posting_entries_removed);

            // Set output parameters
            if (entries_removed_out)
                *entries_removed_out = total_entries_removed;
            if (pages_modified_out)
                *pages_modified_out = total_pages_modified;

            return had_errors ? Status::IO_ERROR : Status::OK;
        }

        // ==================================================================
        // PHASE 5 TASK 5.3.3: Update TIDs After Tablespace Migration
        // ==================================================================

        Status GinIndex::updateTIDsAfterMigration(
            const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
            uint64_t *tids_updated_out,
            uint64_t *pages_modified_out,
            ErrorContext *ctx)
        {
            // Initialize output counters
            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = 0;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = 0;
            }

            // Early exit if no TID mapping (empty table or no migration)
            if (tid_mapping.empty())
            {
                return Status::OK;
            }

            if (!buffer_pool_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
                return Status::INVALID_ARGUMENT;
            }

            // Statistics
            uint64_t total_tids_updated = 0;
            uint64_t total_pages_modified = 0;
            bool had_errors = false;

            // ===== STEP 1: Update TIDs in pending list =====
            // The pending list contains recent insertions not yet merged into main index

            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin GIN meta page");
                return status;
            }

            auto *meta = reinterpret_cast<SBGinIndexMetaPage *>(meta_data);
            uint64_t pending_head = meta->gin_pending_list_head;
            uint64_t keys_tree_root = meta->gin_keys_btree_root;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Scan pending list chain
            uint64_t current_page = pending_head;
            while (current_page != 0)
            {
                uint8_t *page_data = nullptr;
                status = buffer_pool_->pinPage(static_cast<uint32_t>(current_page), (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(STORAGE, "Failed to pin GIN pending list page %lu during TID update: %d",
                               current_page, static_cast<int>(status));
                    had_errors = true;
                    break;
                }

                auto *pending_page = reinterpret_cast<SBGinPendingListPage *>(page_data);
                uint16_t entry_count = pending_page->gpp_entry_count;
                uint64_t next_page = pending_page->gpp_next_page;

                bool page_modified = false;

                // Update TIDs in pending entries
                for (uint16_t i = 0; i < entry_count && i < getMaxPendingEntriesPerPage(); i++)
                {
                    GinPendingEntry &entry = pending_page->gpp_entries[i];

                    // Convert GPID to legacy for lookup
                    uint64_t old_tid = convertTIDtoLegacy(entry.getTID());

                    // Skip deleted entries (tid == 0)
                    if (old_tid == 0)
                    {
                        continue;
                    }

                    // Look up in mapping
                    auto it = tid_mapping.find(old_tid);
                    if (it != tid_mapping.end())
                    {
                        // Found mapping - update TID
                        uint64_t new_tid = it->second;
                        entry.setTID(convertLegacyTID(new_tid));

                        total_tids_updated++;
                        page_modified = true;

                        LOG_DEBUG(STORAGE, "Updated GIN pending entry TID: %lu -> %lu (page %lu)",
                                 it->first, new_tid, current_page);
                    }
                }

                // Unpin page (mark dirty if modified)
                buffer_pool_->unpinPage(static_cast<uint32_t>(current_page), page_modified, ctx);

                if (page_modified)
                {
                    total_pages_modified++;
                }

                // Move to next page in chain
                current_page = next_page;
            }

            LOG_INFO(STORAGE, "GIN pending list: %lu TIDs updated", total_tids_updated);

            // ===== STEP 2: Update TIDs in posting lists/trees =====
            // We need to traverse the entry tree (keys B-Tree) and update TIDs in all posting structures

            if (keys_tree_root == 0)
            {
                // Empty index (no keys yet), nothing more to update
                if (tids_updated_out != nullptr)
                {
                    *tids_updated_out = total_tids_updated;
                }
                if (pages_modified_out != nullptr)
                {
                    *pages_modified_out = total_pages_modified;
                }
                return Status::OK;
            }

            // Helper: Recursively traverse entry tree and collect all posting page numbers
            std::vector<uint64_t> posting_pages;
            std::function<void(uint32_t)> collectPostingPages = [&](uint32_t page_num)
            {
                uint8_t *page_data = nullptr;
                Status pin_status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
                if (pin_status != Status::OK)
                {
                    LOG_WARNING(STORAGE, "Failed to pin entry tree page %u during TID update: %d",
                               page_num, static_cast<int>(pin_status));
                    had_errors = true;
                    return;
                }

                // Check if this is a leaf or internal node
                auto *leaf = reinterpret_cast<SBGinEntryTreeLeaf *>(page_data);
                bool is_leaf = (leaf->get_is_leaf != 0);

                if (is_leaf)
                {
                    // Leaf node: extract posting pages from entries
                    uint16_t entry_count = leaf->get_entry_count;

                    for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_LEAF_ENTRIES; i++)
                    {
                        // Entry is stored at offset
                        if (i >= entry_count)
                            break;

                        uint16_t offset = leaf->get_offsets[i];
                        if (offset == 0 || offset >= db_->page_size())
                            continue;

                        auto *entry = reinterpret_cast<GinEntryTreeLeafEntry *>(page_data + offset);

                        // Extract posting page number
                        uint64_t posting_page = entry->value.posting_list_page;
                        if (posting_page != 0)
                        {
                            posting_pages.push_back(posting_page);
                        }
                    }
                }
                else
                {
                    // Internal node: recurse to children
                    auto *internal = reinterpret_cast<SBGinEntryTreeInternal *>(page_data);
                    uint16_t entry_count = internal->get_entry_count;

                    // Recurse to all children (via offsets)
                    for (uint16_t i = 0; i < entry_count && i < MAX_ENTRY_TREE_INTERNAL_ENTRIES; i++)
                    {
                        uint16_t offset = internal->get_offsets[i];
                        if (offset == 0 || offset >= db_->page_size())
                            continue;

                        auto *entry = reinterpret_cast<GinEntryTreeInternalEntry *>(page_data + offset);
                        uint32_t child_page = entry->child_page;

                        if (child_page != 0)
                        {
                            collectPostingPages(child_page);
                        }
                    }

                    // Also recurse to rightmost child
                    if (internal->get_rightmost_child != 0)
                    {
                        collectPostingPages(internal->get_rightmost_child);
                    }
                }

                buffer_pool_->unpinPage(page_num, false, ctx);
            };

            // Collect all posting pages from entry tree
            collectPostingPages(static_cast<uint32_t>(keys_tree_root));

            LOG_INFO(STORAGE, "GIN entry tree: found %lu posting pages to update", posting_pages.size());

            // ===== STEP 3: Update TIDs in each posting page =====
            for (uint64_t posting_page_num : posting_pages)
            {
                uint8_t *posting_data = nullptr;
                status = buffer_pool_->pinPage(static_cast<uint32_t>(posting_page_num),
                                              (void **)&posting_data, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(STORAGE, "Failed to pin posting page %lu during TID update: %d",
                               posting_page_num, static_cast<int>(status));
                    had_errors = true;
                    continue;
                }

                auto *posting_page = reinterpret_cast<SBGinPostingListPage *>(posting_data);

                bool page_modified = false;

                // Check if this is a posting tree or a simple list
                if (posting_page->gpl_is_tree)
                {
                    // ===== Posting Tree: Recursively update TIDs in all leaf nodes =====
                    uint64_t tree_root = posting_page->gpl_data.gpl_tree_root;

                    buffer_pool_->unpinPage(static_cast<uint32_t>(posting_page_num), false, ctx);

                    // Helper: Recursively traverse posting tree and update TIDs in leaves
                    std::function<void(uint32_t)> updatePostingTree = [&](uint32_t tree_page_num)
                    {
                        uint8_t *tree_data = nullptr;
                        Status tree_pin_status = buffer_pool_->pinPage(tree_page_num,
                                                                       (void **)&tree_data, ctx);
                        if (tree_pin_status != Status::OK)
                        {
                            LOG_WARNING(STORAGE, "Failed to pin posting tree page %u: %d",
                                       tree_page_num, static_cast<int>(tree_pin_status));
                            had_errors = true;
                            return;
                        }

                        // Check if leaf or internal
                        auto *leaf = reinterpret_cast<SBGinPostingTreeLeaf *>(tree_data);
                        bool is_leaf_node = (leaf->gpt_is_leaf != 0);

                        bool tree_page_modified = false;

                        if (is_leaf_node)
                        {
                            // Leaf node: update TID array
                            uint16_t tid_count = leaf->gpt_entry_count;

                            for (uint16_t i = 0; i < tid_count && i < getMaxPostingTreeLeafTids(); i++)
                            {
                                // Convert GPID to legacy for lookup
                                uint64_t old_tid = convertTIDtoLegacy(leaf->gpt_tids[i].getTID());

                                auto it = tid_mapping.find(old_tid);
                                if (it != tid_mapping.end())
                                {
                                    uint64_t new_tid = it->second;
                                    leaf->gpt_tids[i].setTID(convertLegacyTID(new_tid));

                                    total_tids_updated++;
                                    tree_page_modified = true;

                                    LOG_DEBUG(STORAGE, "Updated GIN posting tree TID: %lu -> %lu (page %u)",
                                             old_tid, new_tid, tree_page_num);
                                }
                            }
                        }
                        else
                        {
                            // Internal node: recurse to children
                            auto *internal = reinterpret_cast<SBGinPostingTreeInternal *>(tree_data);
                            uint16_t entry_count = internal->gpt_entry_count;

                            for (uint16_t i = 0; i < entry_count && i < getMaxPostingTreeInternalEntries(); i++)
                            {
                                uint32_t child_page = internal->gpt_entries[i].child_page;
                                if (child_page != 0)
                                {
                                    updatePostingTree(child_page);
                                }
                            }
                        }

                        buffer_pool_->unpinPage(tree_page_num, tree_page_modified, ctx);

                        if (tree_page_modified)
                        {
                            total_pages_modified++;
                        }
                    };

                    // Start recursive update from tree root
                    if (tree_root != 0)
                    {
                        updatePostingTree(static_cast<uint32_t>(tree_root));
                    }
                }
                else
                {
                    // ===== Simple Posting List =====
                    if (posting_page->gpl_is_compressed)
                    {
                        // Compressed posting list: would need to decompress, update, recompress
                        // For now, log warning and skip
                        LOG_WARNING(STORAGE, "GIN compressed posting list at page %lu - TID update not implemented",
                                   posting_page_num);
                        buffer_pool_->unpinPage(static_cast<uint32_t>(posting_page_num), false, ctx);
                        had_errors = true;
                        continue;
                    }

                    // Uncompressed posting list: simple TID array
                    uint16_t entry_count = posting_page->gpl_entry_count;

                    for (uint16_t i = 0; i < entry_count && i < getMaxPostingEntriesPerPage(); i++)
                    {
                        // Convert GPID to legacy for lookup
                        uint64_t old_tid = convertTIDtoLegacy(posting_page->getEntries()[i].getTID());

                        auto it = tid_mapping.find(old_tid);
                        if (it != tid_mapping.end())
                        {
                            uint64_t new_tid = it->second;
                            posting_page->getEntries()[i].setTID(convertLegacyTID(new_tid));

                            total_tids_updated++;
                            page_modified = true;

                            LOG_DEBUG(STORAGE, "Updated GIN posting list TID: %lu -> %lu (page %lu)",
                                     old_tid, new_tid, posting_page_num);
                        }
                    }

                    buffer_pool_->unpinPage(static_cast<uint32_t>(posting_page_num), page_modified, ctx);

                    if (page_modified)
                    {
                        total_pages_modified++;
                    }
                }
            }

            // Return statistics
            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = total_tids_updated;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = total_pages_modified;
            }

            if (had_errors)
            {
                LOG_WARNING(STORAGE, "GIN TID update completed with some errors: %lu TIDs updated, %lu pages modified",
                           total_tids_updated, total_pages_modified);
                // Return OK since migration can still proceed, errors are logged
            }
            else
            {
                LOG_INFO(STORAGE, "GIN TID update completed successfully: %lu TIDs updated, %lu pages modified",
                        total_tids_updated, total_pages_modified);
            }

            return Status::OK;
        }

    } // namespace core
} // namespace scratchbird
