#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/connection_context.h"
#include <cstring>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
#include <emmintrin.h> // SSE2
#endif

namespace scratchbird
{
    namespace core
    {
        // Constructor
        GinIndex::GinIndex(Database *db, const UuidV7Bytes &index_uuid)
            : db_(db), buffer_pool_(db->buffer_pool()), index_uuid_(index_uuid), meta_page_(0)
        {
        }

        // Destructor
        GinIndex::~GinIndex() = default;

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
            meta->hip_header.version = DB_VERSION_ALPHA_1_0_1;
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
        Status GinIndex::insert(const void *value_data, size_t value_len, uint64_t tuple_id,
                                std::function<std::vector<std::vector<uint8_t>>(const void *, size_t)> key_extractor,
                                ErrorContext *ctx)
        {
            if (!value_data || value_len == 0 || tuple_id == 0)
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

            // Insert each key into the pending list
            for (const auto &key : keys)
            {
                Status status = insertIntoPendingList(key, tuple_id, ctx);
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
                pending->gpp_header.version = DB_VERSION_ALPHA_1_0_1;
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
            if (tail->gpp_entry_count >= MAX_PENDING_ENTRIES_PER_PAGE)
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
                new_pending->gpp_header.version = DB_VERSION_ALPHA_1_0_1;
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
            entry.tid = tuple_id;
            entry.xmin = ConnectionContext::getCurrentTransactionId(); // Record inserting transaction
            entry.key_len = std::min(static_cast<uint16_t>(key.size()), static_cast<uint16_t>(54));
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
        std::vector<uint64_t> GinIndex::find(const void *key_data, size_t key_len,
                                             ErrorContext *ctx)
        {
            std::vector<uint64_t> results;

            if (!key_data || key_len == 0)
            {
                return results;
            }

            std::vector<uint8_t> key(static_cast<const uint8_t *>(key_data),
                                     static_cast<const uint8_t *>(key_data) + key_len);

            // Search for the key in the keys B-Tree
            uint64_t posting_page = 0;
            Status status = searchKeysTree(key, &posting_page, ctx);

            // Get TIDs from posting list (if key found in main index)
            if (status == Status::OK && posting_page != 0)
            {
                status = getPostingListTids(posting_page, &results, ctx);
                if (status != Status::OK)
                {
                    results.clear();
                }
            }

            // Scan pending list for matching keys with visibility check
            // Get current snapshot for visibility checking
            ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
            const TransactionManager::Snapshot *snapshot = nullptr;
            if (conn_ctx != nullptr)
            {
                snapshot = conn_ctx->getSnapshot();
            }

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

                        // Check visibility: is this entry's transaction visible to current snapshot?
                        bool is_visible = false;
                        if (snapshot != nullptr)
                        {
                            // Use snapshot isolation
                            is_visible = db_->transaction_manager()->isSnapshotVisible(entry.xmin, snapshot);
                        }
                        else
                        {
                            // Fallback: READ COMMITTED semantics (always see committed)
                            TransactionState state;
                            Status vis_status = db_->transaction_manager()->getTransactionState(entry.xmin, state, ctx);
                            is_visible = (vis_status == Status::OK && state == TransactionState::COMMITTED);
                        }

                        // If visible and key matches, add TID to results
                        if (is_visible)
                        {
                            std::vector<uint8_t> entry_key(entry.key_data, entry.key_data + entry.key_len);
                            if (entry_key == key)
                            {
                                results.push_back(entry.tid);
                            }
                        }
                    }

                    buffer_pool_->unpinPage(pending_page, false, ctx);
                    pending_page = next_page;
                }
            }

            // Sort results to ensure consistent ordering
            std::sort(results.begin(), results.end());

            return results;
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
                    entry.tid = pending->gpp_entries[i].tid;
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

        // Vacuum operation (stub for Phase 1)
        Status GinIndex::vacuum(ErrorContext *ctx)
        {
            // TODO: Implement in Phase 4
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
                return getPostingTreeTids(tree_root, tids_out, ctx);
            }

            // It's a posting list
            uint16_t tid_count = list_page->gpl_entry_count;
            tids_out->reserve(tids_out->size() + tid_count);

            for (uint16_t i = 0; i < tid_count; i++)
            {
                tids_out->push_back(list_page->gpl_data.gpl_entries[i].tid);
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
            list_page->gpl_header.version = DB_VERSION_ALPHA_1_0_1;
            list_page->gpl_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_LIST);
            list_page->gpl_header.page_size = db_->page_size();
            list_page->gpl_header.page_id = new_posting_page;
            list_page->gpl_entry_count = 0;
            list_page->gpl_is_tree = 0;

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

            // Check for duplicate
            for (uint16_t i = 0; i < list_page->gpl_entry_count; i++)
            {
                if (list_page->gpl_data.gpl_entries[i].tid == tuple_id)
                {
                    buffer_pool_->unpinPage(posting_page, false, ctx);
                    return Status::OK; // Already exists
                }
            }

            // Check if we need to convert to tree (threshold reached)
            if (list_page->gpl_entry_count >= GIN_POSTING_LIST_THRESHOLD)
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

            // Find insertion position (maintain sorted order)
            uint16_t insert_pos = 0;
            while (insert_pos < list_page->gpl_entry_count &&
                   list_page->gpl_data.gpl_entries[insert_pos].tid < tuple_id)
            {
                insert_pos++;
            }

            // Shift elements to make room
            if (insert_pos < list_page->gpl_entry_count)
            {
                std::memmove(&list_page->gpl_data.gpl_entries[insert_pos + 1],
                             &list_page->gpl_data.gpl_entries[insert_pos],
                             (list_page->gpl_entry_count - insert_pos) * sizeof(GinPostingEntry));
            }

            list_page->gpl_data.gpl_entries[insert_pos].tid = tuple_id;
            list_page->gpl_entry_count++;

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

            // Copy TIDs from the list (already sorted)
            uint16_t tid_count = list_page->gpl_entry_count;
            std::vector<uint64_t> tids;
            tids.reserve(tid_count);
            for (uint16_t i = 0; i < tid_count; i++)
            {
                tids.push_back(list_page->gpl_data.gpl_entries[i].tid);
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
            leaf->gpt_header.version = DB_VERSION_ALPHA_1_0_1;
            leaf->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
            leaf->gpt_header.page_size = db_->page_size();
            leaf->gpt_header.page_id = leaf_page;
            leaf->gpt_is_leaf = 1;
            leaf->gpt_next_leaf = 0;
            leaf->gpt_entry_count = tid_count;

            // Copy TIDs to leaf
            for (uint16_t i = 0; i < tid_count; i++)
            {
                leaf->gpt_tids[i] = tids[i];
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
                    if (tid < internal->gpt_entries[mid].separator_tid)
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
                root->gpt_header.version = DB_VERSION_ALPHA_1_0_1;
                root->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
                root->gpt_header.page_size = db_->page_size();
                root->gpt_header.page_id = new_root;
                root->gpt_is_leaf = 0;
                root->gpt_entry_count = 2;

                root->gpt_entries[0].separator_tid = separator_tid;
                root->gpt_entries[0].child_page = leaf_page;
                root->gpt_entries[1].separator_tid = UINT64_MAX;
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

            // Check for duplicate
            for (uint16_t i = 0; i < leaf->gpt_entry_count; i++)
            {
                if (leaf->gpt_tids[i] == tid)
                {
                    buffer_pool_->unpinPage(leaf_page, false, ctx);
                    return Status::OK; // Already exists
                }
            }

            // Check if leaf is full
            if (leaf->gpt_entry_count >= MAX_POSTING_TREE_LEAF_TIDS)
            {
                buffer_pool_->unpinPage(leaf_page, false, ctx);
                return splitPostingTreeLeaf(leaf_page, new_sibling_out, separator_tid_out, ctx);
            }

            // Find insertion position (maintain sorted order)
            uint16_t insert_pos = 0;
            while (insert_pos < leaf->gpt_entry_count && leaf->gpt_tids[insert_pos] < tid)
            {
                insert_pos++;
            }

            // Shift elements to make room
            if (insert_pos < leaf->gpt_entry_count)
            {
                std::memmove(&leaf->gpt_tids[insert_pos + 1],
                             &leaf->gpt_tids[insert_pos],
                             (leaf->gpt_entry_count - insert_pos) * sizeof(uint64_t));
            }

            leaf->gpt_tids[insert_pos] = tid;
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
            sibling->gpt_header.version = DB_VERSION_ALPHA_1_0_1;
            sibling->gpt_header.page_type = static_cast<uint16_t>(PageType::GIN_POSTING_TREE);
            sibling->gpt_header.page_size = db_->page_size();
            sibling->gpt_header.page_id = new_sibling;
            sibling->gpt_is_leaf = 1;

            // Split point: move second half to sibling
            uint16_t split_point = leaf->gpt_entry_count / 2;
            uint16_t move_count = leaf->gpt_entry_count - split_point;

            std::memcpy(sibling->gpt_tids, &leaf->gpt_tids[split_point],
                        move_count * sizeof(uint64_t));
            sibling->gpt_entry_count = move_count;

            // Update original leaf
            leaf->gpt_entry_count = split_point;

            // Link siblings for range scans
            sibling->gpt_next_leaf = leaf->gpt_next_leaf;
            leaf->gpt_next_leaf = new_sibling;

            // Separator is the first key of the new sibling
            *separator_tid_out = sibling->gpt_tids[0];
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
                if (leaf->gpt_tids[mid] == tid)
                {
                    found = true;
                    break;
                }
                else if (leaf->gpt_tids[mid] < tid)
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

                // Add all TIDs from this leaf
                for (uint16_t i = 0; i < leaf->gpt_entry_count; i++)
                {
                    tids_out->push_back(leaf->gpt_tids[i]);
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
            if (internal->gpt_entry_count >= MAX_POSTING_TREE_INTERNAL_ENTRIES)
            {
                buffer_pool_->unpinPage(internal_page, false, ctx);
                return splitPostingTreeInternal(internal_page, new_sibling_out, separator_tid_out, ctx);
            }

            // Find insertion position
            uint16_t insert_pos = 0;
            while (insert_pos < internal->gpt_entry_count &&
                   internal->gpt_entries[insert_pos].separator_tid < separator_tid)
            {
                insert_pos++;
            }

            // Shift elements
            if (insert_pos < internal->gpt_entry_count)
            {
                std::memmove(&internal->gpt_entries[insert_pos + 1],
                             &internal->gpt_entries[insert_pos],
                             (internal->gpt_entry_count - insert_pos) * sizeof(GinPostingTreeInternalEntry));
            }

            internal->gpt_entries[insert_pos].separator_tid = separator_tid;
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
            sibling->gpt_header.version = DB_VERSION_ALPHA_1_0_1;
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

            // Separator is the first key of the new sibling
            *separator_tid_out = sibling->gpt_entries[0].separator_tid;
            *new_sibling_out = new_sibling;

            buffer_pool_->unpinPage(internal_page, true, ctx);
            buffer_pool_->unpinPage(new_sibling, true, ctx);

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
                leaf->get_header.version = DB_VERSION_ALPHA_1_0_1;
                leaf->get_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
                leaf->get_header.page_size = db_->page_size();
                leaf->get_header.page_id = root_page;
                leaf->get_is_leaf = 1;
                leaf->get_entry_count = 0;
                leaf->get_free_space = 8192 - 1084; // Data area size
                leaf->get_data_end = 8192;

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
            sibling->get_header.version = DB_VERSION_ALPHA_1_0_1;
            sibling->get_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
            sibling->get_header.page_size = db_->page_size();
            sibling->get_header.page_id = new_sibling;
            sibling->get_is_leaf = 1;
            sibling->get_entry_count = 0;
            sibling->get_free_space = 8192 - 1084;
            sibling->get_data_end = 8192;

            // Split point: move second half to sibling
            uint16_t split_point = leaf->get_entry_count / 2;
            uint16_t sibling_new_data_end = 8192;

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
            root->get_header.version = DB_VERSION_ALPHA_1_0_1;
            root->get_header.page_type = static_cast<uint16_t>(PageType::GIN_INDEX_META);
            root->get_header.page_size = db_->page_size();
            root->get_header.page_id = new_root;
            root->get_is_leaf = 0;
            root->get_entry_count = 1;
            root->get_free_space = 8192 - 1084;
            root->get_data_end = 8192;
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
        std::vector<uint64_t> GinIndex::findAll(const std::vector<std::vector<uint8_t>> &keys,
                                                ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

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
                status = getPostingListTids(posting_page, &tids, ctx);
                if (status != Status::OK)
                {
                    // Error retrieving TIDs
                    return result;
                }

                // If any key has no TIDs, intersection is empty
                if (tids.empty())
                {
                    return result;
                }

                tid_lists.push_back(std::move(tids));
            }

            // Compute intersection of all TID lists
            result = mergeTidLists(tid_lists);

            return result;
        }

        // Find TIDs matching ANY key (OR operation)
        std::vector<uint64_t> GinIndex::findAny(const std::vector<std::vector<uint8_t>> &keys,
                                                ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

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
                status = getPostingListTids(posting_page, &tids, ctx);
                if (status != Status::OK)
                {
                    // Error retrieving TIDs - skip this key
                    continue;
                }

                // Skip empty TID lists
                if (tids.empty())
                {
                    continue;
                }

                tid_lists.push_back(std::move(tids));
            }

            // If no keys had TIDs, return empty
            if (tid_lists.empty())
            {
                return result;
            }

            // Compute union of all TID lists
            result = unionTidLists(tid_lists);

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
        std::vector<uint64_t> GinIndex::findAllOptimized(
            const std::vector<std::vector<uint8_t>> &keys,
            const QueryOptions &options,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            if (keys.empty())
            {
                return result;
            }

            // If optimization is disabled, use standard findAll
            if (!options.optimize_key_order)
            {
                return findAll(keys, ctx);
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
            return findAll(sorted_keys, ctx);
        }

        // Optimized multi-key OR query
        std::vector<uint64_t> GinIndex::findAnyOptimized(
            const std::vector<std::vector<uint8_t>> &keys,
            const QueryOptions &options,
            ErrorContext *ctx)
        {
            // For OR queries, key order doesn't significantly affect performance
            // So we just delegate to standard findAny
            // Future optimization: parallel execution
            return findAny(keys, ctx);
        }

        // PostgreSQL GIN operator support
        std::vector<uint64_t> GinIndex::executeOperator(
            GinOperator op,
            const std::vector<std::vector<uint8_t>> &left_keys,
            const std::vector<std::vector<uint8_t>> &right_keys,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

            switch (op)
            {
            case GinOperator::CONTAINS: // @> - left contains right
                // All right keys must be present in documents
                return findAll(right_keys, ctx);

            case GinOperator::CONTAINED_BY: // <@ - left contained by right
                // At least one left key must be present
                return findAny(left_keys, ctx);

            case GinOperator::OVERLAP: // && - has common elements
                // Any key from either set
                {
                    std::vector<std::vector<uint8_t>> all_keys;
                    all_keys.insert(all_keys.end(), left_keys.begin(), left_keys.end());
                    all_keys.insert(all_keys.end(), right_keys.begin(), right_keys.end());
                    return findAny(all_keys, ctx);
                }

            case GinOperator::EQUALS: // = - exact match
                // All left keys and all right keys must be present
                {
                    std::vector<std::vector<uint8_t>> all_keys;
                    all_keys.insert(all_keys.end(), left_keys.begin(), left_keys.end());
                    all_keys.insert(all_keys.end(), right_keys.begin(), right_keys.end());
                    return findAll(all_keys, ctx);
                }

            case GinOperator::EXISTS: // ? - key exists
                // At least one left key exists
                return findAny(left_keys, ctx);

            case GinOperator::EXISTS_ANY: // ?| - any key exists
                // At least one key from the set exists
                return findAny(left_keys, ctx);

            case GinOperator::EXISTS_ALL: // ?& - all keys exist
                // All keys must exist
                return findAll(left_keys, ctx);

            case GinOperator::TEXT_SEARCH: // @@ - full text search
                // Treat as ALL operation (all terms must be present)
                return findAll(left_keys, ctx);

            default:
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown GIN operator");
                return result;
            }
        }

        // Wildcard query support
        std::vector<uint64_t> GinIndex::findWithWildcard(const void *pattern,
                                                          size_t pattern_len,
                                                          ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

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

            // Scan entry tree leaves for matching keys
            // For simplicity, we'll just use find() to test, but a real implementation
            // would optimize this with tree traversal
            // TODO: Optimize by traversing the entry tree and collecting matching keys

            // For now, return empty (needs full tree scan implementation)
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                              "Wildcard queries require full index scan - not yet implemented");
            return result;
        }

        // Fuzzy matching with edit distance
        std::vector<uint64_t> GinIndex::findFuzzy(const void *key_data, size_t key_len,
                                                   uint32_t max_edit_distance,
                                                   ErrorContext *ctx)
        {
            std::vector<uint64_t> result;

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

            // Collect all keys within edit distance
            // This requires scanning the entry tree
            // For simplicity, marking as not implemented - needs full tree scan
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                              "Fuzzy matching requires full index scan - not yet implemented");
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
        std::vector<uint64_t> GinIndex::findAllParallel(
            const std::vector<std::vector<uint8_t>> &keys,
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
                return findAll(keys, ctx);
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
                        status = getPostingListTids(posting_page, &tids, ctx);

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
        std::vector<uint64_t> GinIndex::findAnyParallel(
            const std::vector<std::vector<uint8_t>> &keys,
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
                return findAny(keys, ctx);
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
                        status = getPostingListTids(posting_page, &tids, ctx);

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
            return findAny(matching_keys, ctx);
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
                return findWithWildcard(pattern, pattern_len, ctx);
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
            return findAny(matching_keys, ctx);
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

        // Optimized fuzzy matching (placeholder - full BK-tree implementation would go here)
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

            // For now, use brute-force scan with distance calculation
            // Full BK-tree implementation would be more efficient

            // Pin meta page
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

            // This is a simplified implementation
            // A full implementation would:
            // 1. Build a BK-tree from indexed keys
            // 2. Use BK-tree for efficient distance-bounded search
            // 3. Cache the BK-tree for repeated queries

            // For now, mark as not fully optimized
            SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                              "Full BK-tree optimization not yet implemented - use findFuzzy() for basic support");
            return result;
        }

    } // namespace core
} // namespace scratchbird
