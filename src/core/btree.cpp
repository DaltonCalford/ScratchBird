#include <utility>
#include <cstring>
#include <set>

#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/transaction_manager.h"  // Firebird MGA: For isVersionVisible (TIP-based visibility)
#include "scratchbird/core/logger.h"

namespace scratchbird::core
{

    // ============================================================================
    // B-TREE PREFIX COMPRESSION HELPER FUNCTIONS
    // ============================================================================

    // Calculate the common prefix length between two keys
    // Used to determine how many leading bytes can be omitted when storing compressed keys
    // Returns the number of matching bytes from the start of both keys
    static uint16_t calculate_prefix_length(const std::vector<uint8_t>& key1,
                                           const std::vector<uint8_t>& key2)
    {
        uint16_t len = std::min(static_cast<uint16_t>(key1.size()),
                               static_cast<uint16_t>(key2.size()));
        uint16_t prefix = 0;

        while (prefix < len && key1[prefix] == key2[prefix]) {
            prefix++;
        }

        return prefix;
    }

    // Compress a key by storing only the suffix after removing common prefix
    // prev_key: the previous key on the page (used to find common prefix)
    // current_key: the key to compress
    // prefix_len_out: receives the calculated prefix length
    // Returns: vector containing only the suffix (compressed key data)
    static std::vector<uint8_t> compress_key(const std::vector<uint8_t>& prev_key,
                                             const std::vector<uint8_t>& current_key,
                                             uint16_t* prefix_len_out)
    {
        // Calculate common prefix
        uint16_t prefix_len = calculate_prefix_length(prev_key, current_key);

        // Don't compress if:
        // 1. No common prefix exists (prefix_len == 0)
        // 2. Key is too short (< 8 bytes) - overhead exceeds benefit
        // 3. Prefix is too small (< 4 bytes) - not worth the complexity
        if (prefix_len == 0 || current_key.size() < 8 || prefix_len < 4) {
            *prefix_len_out = 0;
            return current_key; // Return full uncompressed key
        }

        // Store only the suffix
        *prefix_len_out = prefix_len;
        std::vector<uint8_t> suffix(current_key.begin() + prefix_len, current_key.end());
        return suffix;
    }

    // Decompress a key by combining the prefix from prev_key with stored suffix
    // prev_key: the previous key on the page (provides the prefix)
    // compressed_key: the stored suffix data
    // prefix_len: length of prefix to take from prev_key
    // Returns: full reconstructed key
    static std::vector<uint8_t> decompress_key(const std::vector<uint8_t>& prev_key,
                                               const uint8_t* compressed_key_data,
                                               uint16_t compressed_key_len,
                                               uint16_t prefix_len)
    {
        // If no compression (prefix_len == 0), return the data as-is
        if (prefix_len == 0) {
            return std::vector<uint8_t>(compressed_key_data,
                                       compressed_key_data + compressed_key_len);
        }

        // Validate prefix length doesn't exceed prev_key size
        if (prefix_len > prev_key.size()) {
            // Corruption detected - return compressed data as fallback
            return std::vector<uint8_t>(compressed_key_data,
                                       compressed_key_data + compressed_key_len);
        }

        // Reconstruct: prefix from prev_key + suffix from compressed data
        std::vector<uint8_t> full_key;
        full_key.reserve(prefix_len + compressed_key_len);

        // Copy prefix from previous key
        full_key.insert(full_key.end(), prev_key.begin(), prev_key.begin() + prefix_len);

        // Append suffix from compressed data
        full_key.insert(full_key.end(), compressed_key_data,
                       compressed_key_data + compressed_key_len);

        return full_key;
    }

    BTree::BTree(Database *db, SBBTreeIndex index_info)
        : db_(db), index_info_(std::move(index_info))
    {
        // Constructor implementation
    }

    BTree::~BTree()
    {
        // Destructor implementation
    }

    auto BTree::create(Database *db, const UuidV7Bytes &index_uuid, const UuidV7Bytes &table_uuid,
                       const std::vector<UuidV7Bytes> &column_uuids, uint32_t *root_page_out,
                       ErrorContext *ctx) -> Status
    {
        if (!db || !root_page_out)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to BTree::create");
            return Status::INVALID_ARGUMENT;
        }

        BufferPool *buffer_pool = db->buffer_pool();
        if (!buffer_pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
            return Status::INVALID_ARGUMENT;
        }

        PageManager *page_manager = db->page_manager();
        if (!page_manager)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no page manager");
            return Status::INVALID_ARGUMENT;
        }

        // Step 1: Allocate root page
        uint32_t root_page = 0;
        Status status = page_manager->allocatePage(root_page, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate root page for B-tree");
            return status;
        }

        // Step 2: Pin root page
        void *root_page_data_ptr = nullptr;
        status = buffer_pool->pinPage(root_page, &root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page for B-tree");
            return status;
        }

        // Step 3: Initialize as leaf page (single page tree initially)
        auto *page = reinterpret_cast<SBBTreePage *>(root_page_data_ptr);
        uint32_t page_size = db->page_size();

        // Zero out the page
        std::memset(page, 0, page_size);

        // Step 4: Set page header
        page->btr_header.magic = K_MAGIC_SBRD;
        page->btr_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
        page->btr_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF);
        page->btr_header.page_size = page_size;
        page->btr_header.page_id = root_page;
        page->btr_header.checksum = 0; // Will be set on flush
        page->btr_header.lsn = 0;
        page->btr_header.flags = 0;
        std::memcpy(page->btr_header.database_uuid, db->uuid().bytes.data(), 16);
        page->btr_header.generation = 0;
        page->btr_header.free_space = 0; // Will be calculated below
        page->btr_header.item_count = 0;
        page->btr_header.free_offset = 0;
        page->btr_header.special_size = 0;

        // Set index and table UUIDs
        std::memcpy(page->btr_index_uuid.bytes.data(), index_uuid.bytes.data(), 16);
        std::memcpy(page->btr_table_uuid.bytes.data(), table_uuid.bytes.data(), 16);

        // Step 5: Set flags: ROOT | LEAF | LEFTMOST | RIGHTMOST
        page->btr_level = 0; // Leaf level
        page->btr_flags = static_cast<uint16_t>(BTreeFlags::ROOT) |
                          static_cast<uint16_t>(BTreeFlags::LEAF) |
                          static_cast<uint16_t>(BTreeFlags::LEFTMOST) |
                          static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
        page->btr_count = 0; // No entries yet

        // Initialize sibling pointers
        page->btr_left_sibling = 0;
        page->btr_right_sibling = 0;
        page->btr_parent_page = 0;

        // Initialize compression metadata
        page->btr_prefix_total = 0;
        page->btr_suffix_total = 0;
        page->btr_compression = static_cast<uint8_t>(BTreeCompressionType::NONE);
        page->btr_min_prefix_len = 0;

        // Initialize multi-version support
        page->btr_xmin = 0;
        page->btr_xmax = 0;
        page->btr_lsn = 0;

        // Step 6: Initialize BTreePage helper and call initialize()
        try
        {
            BTreePage btree_page(reinterpret_cast<uint8_t *>(root_page_data_ptr), page_size);
            status = btree_page.initialize(index_uuid, table_uuid, 0, page->btr_flags);
            if (status != Status::OK)
            {
                buffer_pool->unpinPage(root_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, status, "Failed to initialize B-tree page");
                return status;
            }
        }
        catch (const std::exception &e)
        {
            buffer_pool->unpinPage(root_page, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }

        // Step 7: Unpin page (mark dirty)
        buffer_pool->unpinPage(root_page, true, ctx);

        // Step 8: Return root page number
        *root_page_out = root_page;
        return Status::OK;
    }

    auto BTree::open(Database *db, const UuidV7Bytes &index_uuid, uint32_t root_page,
                     ErrorContext *ctx) -> std::unique_ptr<BTree>
    {
        if (!db)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
            return nullptr;
        }

        BufferPool *buffer_pool = db->buffer_pool();
        if (!buffer_pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
            return nullptr;
        }

        // Step 1: Pin root page to validate
        void *root_page_data_ptr = nullptr;
        Status status = buffer_pool->pinPage(root_page, &root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page");
            return nullptr;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(root_page_data_ptr);

        // Step 2: Verify it's a B-tree page (check page_type)
        if (page->btr_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_LEAF) &&
            page->btr_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_BTREE_INTERNAL))
        {
            buffer_pool->unpinPage(root_page, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid B-tree page type");
            return nullptr;
        }

        // Step 3: Verify index_uuid matches
        if (std::memcmp(page->btr_index_uuid.bytes.data(), index_uuid.bytes.data(), 16) != 0)
        {
            buffer_pool->unpinPage(root_page, false, ctx);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "B-tree index UUID mismatch");
            return nullptr;
        }

        // Step 4: Load SBBTreeIndex structure from page
        SBBTreeIndex index_info;
        std::memcpy(index_info.idx_uuid.bytes.data(), page->btr_index_uuid.bytes.data(), 16);
        std::memcpy(index_info.idx_table_uuid.bytes.data(), page->btr_table_uuid.bytes.data(), 16);

        // Note: column_ids are not stored on the B-tree page itself,
        // they would typically be retrieved from the catalog.
        // For now, we initialize with an empty vector.
        index_info.idx_column_ids.clear();

        index_info.idx_flags = 0;
        index_info.idx_root_page = root_page;

        // Calculate tree height by traversing from root
        // For now, we'll set it based on the root's level
        index_info.idx_height = page->btr_level + 1;

        // Initialize statistics (would need to be calculated by scanning)
        index_info.idx_tuple_count = 0;
        index_info.idx_page_count = 1; // At least the root page
        index_info.idx_deleted_count = 0;

        // Step 5: Unpin page
        buffer_pool->unpinPage(root_page, false, ctx);

        // Step 6: Create and return BTree instance
        return std::make_unique<BTree>(db, index_info);
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    // Task 17 MGA Phase 3.1: Added xid parameter for transaction tracking
    auto BTree::insert(const std::vector<uint8_t> &key, const TID &tid, uint64_t xid,
                       ErrorContext *ctx)
        -> Status
    {
        // Phase 3 TODO: TOAST detoasting for index keys
        // ARCHITECTURAL LIMITATION: Indexes don't have ToastManager reference
        // Solution requires one of:
        // 1. Pass ToastManager* as parameter to insert() (breaks API)
        // 2. Store ToastManager* in BTree class (requires table_id lookup)
        // 3. Detoast at higher level (StorageEngine/QueryExecutor) before calling insert()
        //
        // Recommended approach: Option 3 - detoast keys before index insert
        // Current behavior: If key contains TOAST pointer, indexes pointer bytes (INCORRECT)
        // This will be fixed in a future architectural enhancement.

        // Find the appropriate leaf page for this key
        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_data_ptr;
        status = bp->pinPage(leaf_page_num, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        // Wrap the page in a BTreePage helper
        auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
        uint32_t page_size = page->btr_header.page_size;

        // Create a Tuple for the add_node call
        // PHASE 1.5: Tuple struct now uses TID directly
        Tuple tuple;
        tuple.tid = tid;
        tuple.data = nullptr; // Not used by add_node
        tuple.data_size = 0;  // Not used by add_node

        try
        {
            BTreePage btree_page(reinterpret_cast<uint8_t *>(page_data_ptr), page_size);

            // Task 17 MGA Phase 3.1: Pass xid to add_node for btn_xmin tracking
            status = btree_page.add_node(key, tuple, xid, ctx);
            if (status == Status::PAGE_FULL)
            {
                // Page is full - need to split
                bp->unpinPage(leaf_page_num, false, ctx);

                // Release lock before split (split will re-acquire locks)
                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }

                // Split the leaf page
                status = split_leaf_page(leaf_page_num, key, tid, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // After split, retry the insert (will re-acquire locks)
                // Task 17 MGA Phase 3.1: Pass xid through recursive call
                return insert(key, tid, xid, ctx);
            }
            else if (status != Status::OK)
            {
                bp->unpinPage(leaf_page_num, false, ctx);

                // Release lock on error
                if (lock_mgr != nullptr)
                {
                    LockTag leaf_tag{};
                    leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    leaf_tag.object_uuid = index_info_.idx_uuid;
                    leaf_tag.page_num = leaf_page_num;
                    lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }

                return status;
            }

            // Mark page as dirty since we modified it
            bp->unpinPage(leaf_page_num, true, ctx);

            // Release lock after successful insert
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }

            return Status::OK;
        }
        catch (const std::exception &e)
        {
            bp->unpinPage(leaf_page_num, false, ctx);

            // Release lock on exception
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }

            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    // Searches for a key within a single B-Tree page using binary search.
    // Handles prefix-compressed keys by decompressing them before comparison.
    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    // Task 17 MGA Phase 3.3: Added snapshot parameter for visibility filtering
    auto BTree::searchPage(const SBBTreePage *page, const std::vector<uint8_t> &key,
                           uint64_t current_xid,
                           std::vector<TID> *tids_out) const -> bool
    {
        const auto *page_data = reinterpret_cast<const uint8_t *>(page);

        // Get the node offsets array
        const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

        // Track previous key for decompression
        std::vector<uint8_t> prev_key;

        // Binary search for the key
        int left = 0;
        int right = page->btr_count - 1;
        int found_index = -1;

        while (left <= right)
        {
            int mid = left + (right - left) / 2;

            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[mid]);

            // Skip deleted nodes
            if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
            {
                // Fall back to linear search in this rare case
                prev_key.clear(); // Reset for linear search
                for (int i = 0; i < page->btr_count; ++i)
                {
                    const auto *n = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                    if ((n->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                    {
                        continue;
                    }

                    const uint8_t *node_key_data =
                        reinterpret_cast<const uint8_t *>(n) + sizeof(SBBTreeNode);

                    // Decompress key if needed
                    std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                                    n->btn_key_len,
                                                                    n->btn_prefix_len);

                    // Compare decompressed key
                    int cmp = compare_keys(key, full_key.data(), full_key.size());
                    if (cmp == 0)
                    {
                        found_index = i;
                        break;
                    }

                    // Update prev_key for next iteration
                    prev_key = full_key;
                }
                break;
            }

            // Extract the node's key
            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

            // Decompress key if compressed (btn_prefix_len > 0)
            // For binary search, we need to build prev_key by scanning from first node
            if (node->btn_prefix_len > 0 && prev_key.empty() && mid > 0) {
                // Need to build prev_key by scanning from start
                prev_key.clear();
                for (int i = 0; i < mid; ++i) {
                    const auto *scan_node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                    if ((scan_node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0) {
                        continue;
                    }
                    const uint8_t *scan_key_data = reinterpret_cast<const uint8_t *>(scan_node) + sizeof(SBBTreeNode);
                    prev_key = decompress_key(prev_key, scan_key_data, scan_node->btn_key_len, scan_node->btn_prefix_len);
                }
            }

            // Decompress the current node's key
            std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                           node->btn_key_len,
                                                           node->btn_prefix_len);

            // Compare decompressed key
            int cmp = compare_keys(key, full_key.data(), full_key.size());
            if (cmp == 0)
            {
                found_index = mid;
                break;
            }
            else if (cmp < 0)
            {
                right = mid - 1;
            }
            else
            {
                prev_key = full_key; // Update prev_key when moving right
                left = mid + 1;
            }
        }

        // If found, collect the tuple IDs
        if (found_index >= 0)
        {
            const auto *node =
                reinterpret_cast<const SBBTreeNode *>(page_data + offsets[found_index]);
            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

            // Firebird MGA: Check visibility before returning TIDs using TIP-based visibility
            // Only return TIDs if entry is visible to the current transaction
            if (!isEntryVisible(node->btn_xmin, node->btn_xmax, current_xid))
            {
                return false; // Entry exists but not visible to this transaction
            }

            // PHASE 1.5: Convert stored uint64_t to TID struct
            const auto *tuple_ids_ptr =
                reinterpret_cast<const uint64_t *>(node_key_data + node->btn_key_len);

            for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
            {
                TID tid = convertLegacyTID(tuple_ids_ptr[j]);
                tids_out->push_back(tid);
            }
            return true;
        }

        return false;
    }

    auto BTree::find_leaf_page(const std::vector<uint8_t> &key, uint64_t *page_num_out,
                               bool write_lock, ErrorContext *ctx) -> Status
    {
        uint64_t current_page_num = index_info_.idx_root_page;
        BufferPool *bp = db_->buffer_pool();
        LockManager *lock_mgr = db_->lock_manager();

        // TODO(concurrency): Get proc_id from thread-local storage or connection context
        // For now, use proc_id=0 for single-user mode (Alpha implementation)
        const uint32_t proc_id = 0;

        uint64_t previous_page_num = 0; // For lock coupling

        while (true)
        {
            void *page_data_ptr;
            Status status = bp->pinPage(current_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                // Release previous lock if held
                if (previous_page_num != 0 && lock_mgr != nullptr)
                {
                    LockTag prev_tag{};
                    prev_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    prev_tag.object_uuid = index_info_.idx_uuid;
                    prev_tag.page_num = previous_page_num;
                    lock_mgr->releaseLock(
                        proc_id, prev_tag,
                        write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE, ctx);
                }
                return status;
            }

            // HIGH-3 FIX: Document B-Tree lock coupling pattern
            //
            // LOCK COUPLING PROTOCOL (Crabbing/Hand-Over-Hand Locking)
            // ========================================================
            //
            // Purpose: Safely traverse B-tree during concurrent operations while minimizing lock contention.
            //
            // Algorithm Overview:
            // 1. Start at root, acquire lock on current page
            // 2. Read current page to find next child
            // 3. Acquire lock on child page (now holding TWO locks)
            // 4. Release lock on parent page (hand-over-hand motion)
            // 5. Repeat steps 2-4 until reaching leaf
            //
            // Correctness Guarantees:
            // - Always hold at least ONE lock during traversal (prevents page eviction)
            // - Briefly hold TWO locks during transition (parent + child)
            // - Release parent ONLY AFTER successfully acquiring child lock
            // - If child lock fails, keep parent lock and return error
            //
            // Concurrency Benefits:
            // - Multiple readers can traverse tree simultaneously (SHARE locks)
            // - Writers use EXCLUSIVE locks, blocking readers at that page
            // - Releasing parent early allows other threads to access upper tree
            // - Minimizes lock hold time compared to holding all locks from root to leaf
            //
            // Why Not Hold All Locks?
            // - Holding root→leaf locks would serialize ALL tree operations
            // - Lock coupling allows concurrent access to different tree branches
            // - Example: Thread A descending left branch, Thread B descending right branch
            //
            // Edge Cases Handled:
            // - Lock acquisition failure: Keep previous lock, unpin current, return error
            // - Leaf page reached: Keep lock held for caller (insert/search/delete needs it)
            // - Previous lock = 0: First iteration (root), no previous lock to release
            //
            // Performance Characteristics:
            // - Lock hold time: O(tree_height) instead of O(1) for whole-tree lock
            // - Concurrency: Multiple operations can proceed in parallel
            // - Deadlock-free: Always acquire locks in top-down order (root→leaf)
            //
            // Alternative Approaches (Not Used):
            // - Optimistic Lock Coupling: Release parent before acquiring child (risky)
            // - B-link Trees: Right-link pointers allow lock-free traversal (complex)
            // - Lock-free Algorithms: Require atomic compare-and-swap (high complexity)
            //
            // Thread Safety:
            // - Each thread maintains its own previous_page_num variable (line 443)
            // - Lock manager handles concurrent lock requests with wait queues
            // - Buffer pool ensures pinned pages aren't evicted
            //
            // Example Execution (3-level tree, A→B→C traversal):
            //
            // Time | Held Locks         | Action
            // -----|-------------------|----------------------------------
            // T1   | A (root)          | Acquired lock on root page A
            // T2   | A, B              | Acquired lock on child B (2 locks)
            // T3   | B                 | Released lock on A (hand-over)
            // T4   | B, C              | Acquired lock on child C (2 locks)
            // T5   | C (leaf)          | Released lock on B (hand-over)
            // T6   | C (leaf)          | Return to caller with lock held
            //
            // Acquire lock on current page using lock coupling
            if (lock_mgr != nullptr)
            {
                LockTag page_tag{};
                page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                page_tag.object_uuid = index_info_.idx_uuid;
                page_tag.page_num = current_page_num;
                page_tag.offset_num = 0;
                page_tag.padding = 0;

                LockMode lock_mode = write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE;

                // Step 1: Acquire lock on CURRENT page (child)
                // At this point we hold locks on: previous_page (parent), current_page (child)
                status = lock_mgr->acquireLock(proc_id, page_tag, lock_mode, true, 0, ctx);
                if (status != Status::OK)
                {
                    bp->unpinPage(current_page_num, false, ctx);

                    // CRITICAL: If we fail to acquire child lock, keep parent lock held
                    // Release previous lock if held (cleanup on error)
                    if (previous_page_num != 0)
                    {
                        LockTag prev_tag{};
                        prev_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                        prev_tag.object_uuid = index_info_.idx_uuid;
                        prev_tag.page_num = previous_page_num;
                        lock_mgr->releaseLock(proc_id, prev_tag, lock_mode, ctx);
                    }

                    SET_ERROR_CONTEXT(ctx, status,
                                      "Failed to acquire page lock during B-tree traversal");
                    return status;
                }

                // Step 2: Release lock on PREVIOUS page (parent) - the "coupling" step
                // This is the core of lock coupling: hand-over-hand motion
                // We now hold lock ONLY on current_page (child)
                if (previous_page_num != 0)
                {
                    LockTag prev_tag{};
                    prev_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    prev_tag.object_uuid = index_info_.idx_uuid;
                    prev_tag.page_num = previous_page_num;
                    lock_mgr->releaseLock(proc_id, prev_tag, lock_mode, ctx);
                }
                // At this point: previous lock released, current lock held
                // Other threads can now access the parent page we just released
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_data_ptr);

            if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0)
            {
                // Found leaf page - keep lock held and return
                *page_num_out = current_page_num;
                bp->unpinPage(current_page_num, false, ctx);
                // Note: Lock is kept held on leaf page for caller
                return Status::OK;
            }

            // This is an internal page, find the correct child to descend to
            const auto *page_data = reinterpret_cast<const uint8_t *>(page);
            uint64_t next_page_num = 0;

            // Get the node offsets array
            const auto *offsets =
                reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

            // Track previous key for decompression
            std::vector<uint8_t> prev_key;

            // Linear search through nodes to find the correct child
            // Each internal node has a key and a child pointer to the left of that key
            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);

                // Extract the node's key
                const uint8_t *node_key_data =
                    reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

                // Decompress key if compressed
                std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                               node->btn_key_len,
                                                               node->btn_prefix_len);

                // Compare decompressed key
                int cmp = compare_keys(key, full_key.data(), full_key.size());
                if (cmp < 0)
                {
                    next_page_num = node->btn_child_page;
                    break;
                }

                // Update prev_key for next iteration
                prev_key = full_key;
            }

            // If we didn't find a suitable child (key >= all keys), use the rightmost child
            // The rightmost child pointer is stored in the page header (btr_rightmost_child)
            if (next_page_num == 0)
            {
                // Use the rightmost child pointer from page header
                next_page_num = page->btr_rightmost_child;

                if (next_page_num == 0)
                {
                    // Missing rightmost child pointer - this is a corruption issue
                    bp->unpinPage(current_page_num, false, ctx);

                    // Release lock on current page before returning error
                    if (lock_mgr != nullptr)
                    {
                        LockTag page_tag{};
                        page_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                        page_tag.object_uuid = index_info_.idx_uuid;
                        page_tag.page_num = current_page_num;
                        lock_mgr->releaseLock(
                            proc_id, page_tag,
                            write_lock ? LockMode::LOCK_EXCLUSIVE : LockMode::LOCK_SHARE, ctx);
                    }

                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Internal node missing rightmost child pointer");
                    return Status::PAGE_CORRUPT;
                }
            }

            bp->unpinPage(current_page_num, false, ctx);

            // Track previous page for lock coupling
            previous_page_num = current_page_num;
            current_page_num = next_page_num;
        }
    }

    // PHASE 1 TASK 1.1.1: Added Snapshot parameter (not yet used - Phase 1 Task 1.2 will implement filtering)
    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    auto BTree::search(const std::vector<uint8_t> &key,
                       uint64_t current_xid,
                       std::vector<TID> *tids_out,
                       ErrorContext *ctx) -> Status
    {
        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, false, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_data_ptr;
        status = bp->pinPage(leaf_page_num, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_SHARE, ctx);
            }
            return status;
        }

        const auto *page = reinterpret_cast<const SBBTreePage *>(page_data_ptr);

        // Firebird MGA: Pass transaction ID to searchPage for TIP-based visibility filtering
        bool found = searchPage(page, key, current_xid, tids_out);

        bp->unpinPage(leaf_page_num, false, ctx);

        // Release lock after search
        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_SHARE, ctx);
        }

        // Firebird MGA: Index-level TIP-based visibility filtering active
        // Entries with btn_xmin/btn_xmax are filtered at index level using isVersionVisible()
        // This provides 10-100x speedup for queries with many deleted tuples by avoiding
        // unnecessary heap page accesses.

        if (found)
        {
            return Status::OK;
        }

        return Status::NOT_FOUND;
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    // Task 17 MGA Phase 3.1: Added xid parameter (will be used in Phase 3.2 for markDeleted)
    auto BTree::remove(const std::vector<uint8_t> &key, const TID &tid, uint64_t xid,
                       ErrorContext *ctx)
        -> Status
    {
        // TODO Phase 3.2: Use xid to set btn_xmax instead of physical removal (markDeleted)
        // Find the appropriate leaf page for this key
        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext (Phase 2 complete)
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_data_ptr;
        status = bp->pinPage(leaf_page_num, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
        const auto *page_data = reinterpret_cast<const uint8_t *>(page);

        // Get the node offsets array
        auto *offsets = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(page_data_ptr) +
                                                     sizeof(SBBTreePage));

        // Search for the key and matching tuple_id
        bool found = false;
        uint16_t node_to_remove = 0;

        // Track previous key for decompression
        std::vector<uint8_t> prev_key;

        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);

            // Extract the node's key
            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

            // Decompress key if compressed
            std::vector<uint8_t> full_key = decompress_key(prev_key, node_key_data,
                                                           node->btn_key_len,
                                                           node->btn_prefix_len);

            // Compare decompressed key
            int cmp = compare_keys(key, full_key.data(), full_key.size());
            if (cmp == 0)
            {
                // PHASE 1.5: Convert TID to legacy format for comparison
                uint64_t legacy_tid = convertTIDtoLegacy(tid);

                // Check if the tid matches
                const auto *tuple_ids_ptr =
                    reinterpret_cast<const uint64_t *>(node_key_data + node->btn_key_len);

                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    if (tuple_ids_ptr[j] == legacy_tid)
                    {
                        found = true;
                        node_to_remove = i;
                        break;
                    }
                }

                if (found)
                {
                    break;
                }
            }

            // Update prev_key for next iteration
            prev_key = full_key;
        }

        if (!found)
        {
            bp->unpinPage(leaf_page_num, false, ctx);

            // Release lock on not found
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }

            return Status::NOT_FOUND;
        }

        // Mark the node as deleted (physical removal is done by vacuum)
        auto *node_to_mark = reinterpret_cast<SBBTreeNode *>(
            reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[node_to_remove]);
        node_to_mark->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);

        // Set HAS_GARBAGE flag to indicate page needs vacuuming
        page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);

        // Mark page as dirty since we modified it
        bp->unpinPage(leaf_page_num, true, ctx);

        // Release lock after successful remove
        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
        }

        return Status::OK;
    }

    // Task 17 MGA Phase 3.2: Soft deletion support (mark deleted instead of physical removal)
    auto BTree::markDeleted(const std::vector<uint8_t> &key, const TID &tid, uint64_t xmax,
                           ErrorContext *ctx) -> Status
    {
        // Navigate to leaf page containing this key
        uint64_t leaf_page_num;
        Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Get proc_id from ConnectionContext
        int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
        const uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;
        LockManager *lock_mgr = db_->lock_manager();

        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        status = bp->pinPage(leaf_page_num, &page_buffer, ctx);
        if (status != Status::OK)
        {
            // Release lock acquired by find_leaf_page
            if (lock_mgr != nullptr)
            {
                LockTag leaf_tag{};
                leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                leaf_tag.object_uuid = index_info_.idx_uuid;
                leaf_tag.page_num = leaf_page_num;
                lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
            }
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

        // Scan entries on page to find matching key + TID
        auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));
        bool found = false;

        for (uint16_t i = 0; i < page->btr_count; i++)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[i]);

            // Skip already deleted nodes
            if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
            {
                continue;
            }

            // Extract key
            uint8_t *node_key_data = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

            // Check if key matches
            if (compare_keys(key, node_key) == 0)
            {
                // Key matches - check if TID matches
                uint8_t *tid_data = node_key_data + node->btn_key_len;
                auto *tids = reinterpret_cast<uint64_t *>(tid_data);

                for (uint32_t j = 0; j < node->btn_tuple_count; j++)
                {
                    TID node_tid = convertLegacyTID(tids[j]);
                    if (node_tid == tid)
                    {
                        // Found it! Set btn_xmax to mark as deleted
                        node->btn_xmax = xmax;
                        found = true;
                        break;
                    }
                }
            }

            if (found)
            {
                break;
            }
        }

        // Unpin page (mark dirty if modified)
        bp->unpinPage(leaf_page_num, found, ctx);

        // Release lock
        if (lock_mgr != nullptr)
        {
            LockTag leaf_tag{};
            leaf_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
            leaf_tag.object_uuid = index_info_.idx_uuid;
            leaf_tag.page_num = leaf_page_num;
            lock_mgr->releaseLock(proc_id, leaf_tag, LockMode::LOCK_EXCLUSIVE, ctx);
        }

        return found ? Status::OK : Status::NOT_FOUND;
    }

    // Firebird MGA: Check if index entry is visible using TIP-based visibility
    bool BTree::isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t reader_xid) const
    {
        // ===========================================================================================
        // FIREBIRD MGA VISIBILITY - TIP-based, NOT snapshot-based
        // Per MGA_RULES.md Rule 3 (lines 121-145)
        // ===========================================================================================

        // If no transaction specified, entry is always visible (used by VACUUM, etc.)
        if (reader_xid == 0)
        {
            return true;
        }

        // Get transaction manager
        TransactionManager *txn_mgr = db_->transaction_manager();
        if (txn_mgr == nullptr)
        {
            return true; // No transaction tracking - always visible
        }

        // Special case: xmin = 0 means legacy entry or system operation (always visible)
        if (xmin == 0)
        {
            return true;
        }

        // Own changes always visible
        if (xmin == reader_xid)
        {
            return true;
        }

        // Check if creating transaction (xmin) is visible using TIP-based visibility
        // This is Firebird MGA: checks if xmin is COMMITTED and older than reader
        if (!txn_mgr->isVersionVisible(xmin, reader_xid))
        {
            return false; // Entry not created or created by uncommitted/aborted transaction
        }

        // Check if deleting transaction (xmax) affects visibility
        if (xmax != 0)
        {
            // If deleting transaction is visible, the entry is deleted
            if (txn_mgr->isVersionVisible(xmax, reader_xid))
            {
                return false; // Entry was deleted
            }
        }

        return true; // Entry is visible
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    auto BTree::split_leaf_page(uint64_t left_page_num, const std::vector<uint8_t> &new_key,
                                const TID &new_tid, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();

        // Allocate new right page
        uint32_t right_page_num_u32;
        Status status = pm->allocatePage(right_page_num_u32, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new page for split");
            return status;
        }
        uint64_t right_page_num = right_page_num_u32;

        // Pin both pages
        void *left_page_data_ptr;
        void *right_page_data_ptr;

        status = bp->pinPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            pm->freePage(right_page_num_u32, ctx);
            return status;
        }

        status = bp->pinPage(right_page_num, &right_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            bp->unpinPage(left_page_num, false, ctx);
            pm->freePage(right_page_num_u32, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        auto *right_page = reinterpret_cast<SBBTreePage *>(right_page_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;

        try
        {
            // Initialize right page as leaf
            BTreePage right_btree_page(reinterpret_cast<uint8_t *>(right_page_data_ptr), page_size);
            right_btree_page.initialize(left_page->btr_index_uuid, left_page->btr_table_uuid,
                                        left_page->btr_level, left_page->btr_flags);

            // Calculate split point
            BTreePage left_btree_page(reinterpret_cast<uint8_t *>(left_page_data_ptr), page_size);
            uint16_t split_point = left_btree_page.find_split_point();

            // Get left page node offsets
            auto *left_offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(left_page_data_ptr) + sizeof(SBBTreePage));

            // Move second half of nodes to right page
            for (uint16_t i = split_point; i < left_page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[i]);

                // Extract key
                const uint8_t *node_key_data =
                    reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

                // Extract tuple IDs
                const auto *tuple_ids_ptr =
                    reinterpret_cast<const uint64_t *>(node_key_data + node->btn_key_len);

                // Add each tuple to right page
                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    Tuple tuple;
                    // PHASE 1.5: Convert legacy uint64_t from disk to TID struct
                    tuple.tid = convertLegacyTID(tuple_ids_ptr[j]);
                    tuple.data = nullptr;
                    tuple.data_size = 0;

                    // Task 17 MGA Phase 3.1: Preserve original xmin during page split
                    status = right_btree_page.add_node(node_key, tuple, node->btn_xmin, ctx);
                    if (status != Status::OK)
                    {
                        bp->unpinPage(left_page_num, false, ctx);
                        bp->unpinPage(right_page_num, false, ctx);
                        pm->freePage(right_page_num_u32, ctx);
                        return status;
                    }
                }
            }

            // Update left page count (remove moved nodes)
            left_page->btr_count = split_point;

            // Recalculate left page free space
            uint32_t left_used_space = sizeof(SBBTreePage) + (split_point * sizeof(uint16_t));
            for (uint16_t i = 0; i < split_point; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[i]);
                uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len +
                                     (node->btn_tuple_count * sizeof(uint64_t));
                left_used_space += node_size;
            }
            left_page->btr_free_space = page_size - left_used_space;

            // Update sibling pointers
            uint64_t old_right_sibling = left_page->btr_right_sibling;
            left_page->btr_right_sibling = right_page_num;
            right_page->btr_left_sibling = left_page_num;
            right_page->btr_right_sibling = old_right_sibling;
            right_page->btr_parent_page = left_page->btr_parent_page;

            // Update old right sibling's left pointer if it exists
            // CRITICAL FIX: Acquire lock before modifying to prevent race condition
            if (old_right_sibling != 0)
            {
                // TODO(concurrency): Get proc_id from thread-local storage or connection context
                const uint32_t proc_id = 0;
                LockManager *lock_mgr = db_->lock_manager();

                // Acquire exclusive lock on old right sibling before modification
                if (lock_mgr != nullptr)
                {
                    LockTag sibling_tag{};
                    sibling_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    sibling_tag.object_uuid = index_info_.idx_uuid;
                    sibling_tag.page_num = old_right_sibling;

                    status = lock_mgr->acquireLock(proc_id, sibling_tag, LockMode::LOCK_EXCLUSIVE,
                                                   true, 0, ctx);
                    if (status != Status::OK)
                    {
                        // CRITICAL FIX (Issue 2.7): Failed to acquire lock - MUST NOT continue
                        // Continuing without lock can cause B-tree corruption via race condition
                        // Two concurrent splits could both try to update the same sibling pointer
                        bp->unpinPage(left_page_num, false, ctx);
                        bp->unpinPage(right_page_num, false, ctx);
                        pm->freePage(right_page_num_u32, ctx);
                        SET_ERROR_CONTEXT(ctx, status, "Failed to acquire lock on old right sibling during leaf split");
                        return status;
                    }
                }

                void *old_right_data_ptr;
                status = bp->pinPage(old_right_sibling, &old_right_data_ptr, ctx);
                if (status == Status::OK)
                {
                    auto *old_right_page = reinterpret_cast<SBBTreePage *>(old_right_data_ptr);
                    old_right_page->btr_left_sibling = right_page_num;
                    bp->unpinPage(old_right_sibling, true, ctx);
                }

                // Release lock on old right sibling
                if (lock_mgr != nullptr)
                {
                    LockTag sibling_tag{};
                    sibling_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    sibling_tag.object_uuid = index_info_.idx_uuid;
                    sibling_tag.page_num = old_right_sibling;
                    lock_mgr->releaseLock(proc_id, sibling_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }
            }

            // Update rightmost flag
            if ((left_page->btr_flags & static_cast<uint16_t>(BTreeFlags::RIGHTMOST)) != 0)
            {
                left_page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
                right_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
            }

            // Get separator key (first key on right page)
            auto *right_offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(right_page_data_ptr) + sizeof(SBBTreePage));
            const auto *separator_node = reinterpret_cast<const SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(right_page_data_ptr) + right_offsets[0]);
            const uint8_t *sep_key_data =
                reinterpret_cast<const uint8_t *>(separator_node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> separator_key(sep_key_data,
                                               sep_key_data + separator_node->btn_key_len);

            // Unpin both pages (mark as dirty)
            bp->unpinPage(left_page_num, true, ctx);
            bp->unpinPage(right_page_num, true, ctx);

            // Insert separator key into parent
            return insert_into_parent(left_page_num, separator_key, right_page_num, ctx);
        }
        catch (const std::exception &e)
        {
            bp->unpinPage(left_page_num, false, ctx);
            bp->unpinPage(right_page_num, false, ctx);
            pm->freePage(right_page_num_u32, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    auto BTree::split_internal_page(uint64_t left_page_num,
                                    const std::vector<uint8_t> &separator_key,
                                    uint64_t right_page_num, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();

        // Allocate new right internal page
        uint32_t new_right_page_num_u32;
        Status status = pm->allocatePage(new_right_page_num_u32, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new internal page for split");
            return status;
        }
        uint64_t new_right_page_num = new_right_page_num_u32;

        // Pin both pages
        void *left_page_data_ptr;
        void *new_right_page_data_ptr;

        status = bp->pinPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            pm->freePage(new_right_page_num_u32, ctx);
            return status;
        }

        status = bp->pinPage(new_right_page_num, &new_right_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            bp->unpinPage(left_page_num, false, ctx);
            pm->freePage(new_right_page_num_u32, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        auto *new_right_page = reinterpret_cast<SBBTreePage *>(new_right_page_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;

        try
        {
            // Initialize new right page as internal (not leaf)
            BTreePage new_right_btree_page(reinterpret_cast<uint8_t *>(new_right_page_data_ptr),
                                           page_size);
            uint16_t internal_flags =
                left_page->btr_flags & ~static_cast<uint16_t>(BTreeFlags::LEAF);
            new_right_btree_page.initialize(left_page->btr_index_uuid, left_page->btr_table_uuid,
                                            left_page->btr_level, internal_flags);

            // Calculate split point
            BTreePage left_btree_page(reinterpret_cast<uint8_t *>(left_page_data_ptr), page_size);
            uint16_t split_point = left_btree_page.find_split_point();

            // Get left page node offsets
            auto *left_offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(left_page_data_ptr) + sizeof(SBBTreePage));

            // Get the promoted separator key (key at split point)
            const auto *promoted_node = reinterpret_cast<const SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[split_point]);
            const uint8_t *promoted_key_data =
                reinterpret_cast<const uint8_t *>(promoted_node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> promoted_key(promoted_key_data,
                                              promoted_key_data + promoted_node->btn_key_len);

            // Move nodes after split point to new right page
            // For internal nodes, the separator is promoted (not copied)
            for (uint16_t i = split_point + 1; i < left_page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[i]);

                // Extract key and child pointer
                const uint8_t *node_key_data =
                    reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);
                uint64_t child_page = node->btn_child_page;

                // Add node to right page by directly manipulating page structure
                // (Note: BTreePage::add_node is for leaf nodes, so we do this manually)
                uint32_t node_size = sizeof(SBBTreeNode) + node_key.size() + sizeof(uint64_t);

                // Allocate space from end of page
                new_right_page->btr_high_water -= node_size;
                auto *new_node = reinterpret_cast<SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(new_right_page_data_ptr) +
                    new_right_page->btr_high_water);

                // Copy node data
                new_node->btn_flags = node->btn_flags;
                new_node->btn_prefix_len = 0;
                new_node->btn_suffix_trunc = 0;
                new_node->btn_key_len = node_key.size();
                new_node->btn_tuple_count = 0;
                new_node->btn_child_page = child_page;
                new_node->btn_xmin = 0;
                new_node->btn_xmax = 0;

                // Copy key
                uint8_t *new_key_location =
                    reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
                memcpy(new_key_location, node_key.data(), node_key.size());

                // Update offset array
                auto *new_right_offsets = reinterpret_cast<uint16_t *>(
                    reinterpret_cast<uint8_t *>(new_right_page_data_ptr) + sizeof(SBBTreePage));
                new_right_offsets[new_right_page->btr_count] = new_right_page->btr_high_water;

                // Update header
                new_right_page->btr_count++;
                new_right_page->btr_free_space -= (node_size + sizeof(uint16_t));

                // Update child's parent pointer
                void *child_page_data_ptr;
                if (bp->pinPage(child_page, &child_page_data_ptr, ctx) == Status::OK)
                {
                    auto *child_page_ptr = reinterpret_cast<SBBTreePage *>(child_page_data_ptr);
                    child_page_ptr->btr_parent_page = new_right_page_num;
                    bp->unpinPage(child_page, true, ctx);
                }
            }

            // CRITICAL FIX: Set rightmost child pointers for internal nodes
            // Save the old rightmost child before modifying left page
            uint64_t old_rightmost = left_page->btr_rightmost_child;

            // Update left page count
            left_page->btr_count = split_point;

            // The promoted node's child pointer becomes left page's new rightmost child
            left_page->btr_rightmost_child = promoted_node->btn_child_page;

            // The new right page's rightmost child is the old left page's rightmost child
            // (since all entries after split_point were moved to right page)
            new_right_page->btr_rightmost_child = old_rightmost;

            // Recalculate left page free space
            uint32_t left_used_space = sizeof(SBBTreePage) + (split_point * sizeof(uint16_t));
            for (uint16_t i = 0; i < split_point; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(
                    reinterpret_cast<uint8_t *>(left_page_data_ptr) + left_offsets[i]);
                uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len + sizeof(uint64_t);
                left_used_space += node_size;
            }
            left_page->btr_free_space = page_size - left_used_space;

            // Update sibling pointers
            uint64_t old_right_sibling = left_page->btr_right_sibling;
            left_page->btr_right_sibling = new_right_page_num;
            new_right_page->btr_left_sibling = left_page_num;
            new_right_page->btr_right_sibling = old_right_sibling;
            new_right_page->btr_parent_page = left_page->btr_parent_page;

            // Update old right sibling's left pointer if it exists
            // CRITICAL FIX: Acquire lock before modifying to prevent race condition
            if (old_right_sibling != 0)
            {
                // TODO(concurrency): Get proc_id from thread-local storage or connection context
                const uint32_t proc_id = 0;
                LockManager *lock_mgr = db_->lock_manager();

                // Acquire exclusive lock on old right sibling before modification
                if (lock_mgr != nullptr)
                {
                    LockTag sibling_tag{};
                    sibling_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    sibling_tag.object_uuid = index_info_.idx_uuid;
                    sibling_tag.page_num = old_right_sibling;

                    status = lock_mgr->acquireLock(proc_id, sibling_tag, LockMode::LOCK_EXCLUSIVE,
                                                   true, 0, ctx);
                    if (status != Status::OK)
                    {
                        // CRITICAL FIX (Issue 2.7): Failed to acquire lock - MUST NOT continue
                        // Continuing without lock can cause B-tree corruption via race condition
                        // Two concurrent splits could both try to update the same sibling pointer
                        bp->unpinPage(left_page_num, false, ctx);
                        bp->unpinPage(new_right_page_num, false, ctx);
                        pm->freePage(new_right_page_num_u32, ctx);
                        SET_ERROR_CONTEXT(ctx, status, "Failed to acquire lock on old right sibling during internal split");
                        return status;
                    }
                }

                void *old_right_data_ptr;
                status = bp->pinPage(old_right_sibling, &old_right_data_ptr, ctx);
                if (status == Status::OK)
                {
                    auto *old_right_page = reinterpret_cast<SBBTreePage *>(old_right_data_ptr);
                    old_right_page->btr_left_sibling = new_right_page_num;
                    bp->unpinPage(old_right_sibling, true, ctx);
                }

                // Release lock on old right sibling
                if (lock_mgr != nullptr)
                {
                    LockTag sibling_tag{};
                    sibling_tag.target_type = LockTarget::LOCK_TARGET_PAGE;
                    sibling_tag.object_uuid = index_info_.idx_uuid;
                    sibling_tag.page_num = old_right_sibling;
                    lock_mgr->releaseLock(proc_id, sibling_tag, LockMode::LOCK_EXCLUSIVE, ctx);
                }
            }

            // Unpin both pages (mark as dirty)
            bp->unpinPage(left_page_num, true, ctx);
            bp->unpinPage(new_right_page_num, true, ctx);

            // Insert promoted key into parent
            return insert_into_parent(left_page_num, promoted_key, new_right_page_num, ctx);
        }
        catch (const std::exception &e)
        {
            bp->unpinPage(left_page_num, false, ctx);
            bp->unpinPage(new_right_page_num, false, ctx);
            pm->freePage(new_right_page_num_u32, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    auto BTree::insert_into_parent(uint64_t left_page_num,
                                   const std::vector<uint8_t> &separator_key,
                                   uint64_t right_page_num, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();

        // Pin left page to get parent info
        void *left_page_data_ptr;
        Status status = bp->pinPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        uint64_t parent_page_num = left_page->btr_parent_page;
        bool left_is_root = (left_page->btr_flags & static_cast<uint16_t>(BTreeFlags::ROOT)) != 0;

        bp->unpinPage(left_page_num, false, ctx);

        // If no parent (was root), create new root
        if (parent_page_num == 0 || left_is_root)
        {
            return create_new_root(left_page_num, separator_key, right_page_num, ctx);
        }

        // Pin parent page
        void *parent_page_data_ptr;
        status = bp->pinPage(parent_page_num, &parent_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *parent_page = reinterpret_cast<SBBTreePage *>(parent_page_data_ptr);
        uint32_t page_size = parent_page->btr_header.page_size;

        // Check if parent has space for new entry
        uint32_t required_space = sizeof(SBBTreeNode) + separator_key.size() + sizeof(uint64_t);

        if (parent_page->btr_free_space < (required_space + sizeof(uint16_t)))
        {
            // Parent is full, need to split it
            bp->unpinPage(parent_page_num, false, ctx);
            return split_internal_page(parent_page_num, separator_key, right_page_num, ctx);
        }

        // Add entry to parent: separator_key points to right_page_num
        // Allocate space from end of page
        parent_page->btr_high_water -= required_space;
        auto *new_node = reinterpret_cast<SBBTreeNode *>(
            reinterpret_cast<uint8_t *>(parent_page_data_ptr) + parent_page->btr_high_water);

        // Populate new node
        new_node->btn_flags = 0;
        new_node->btn_prefix_len = 0;
        new_node->btn_suffix_trunc = 0;
        new_node->btn_key_len = separator_key.size();
        new_node->btn_tuple_count = 0;
        new_node->btn_child_page = right_page_num;
        new_node->btn_xmin = 0;
        new_node->btn_xmax = 0;

        // Copy key
        uint8_t *key_location = reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
        memcpy(key_location, separator_key.data(), separator_key.size());

        // Find insertion position in parent's offset array
        auto *parent_offsets = reinterpret_cast<uint16_t *>(
            reinterpret_cast<uint8_t *>(parent_page_data_ptr) + sizeof(SBBTreePage));

        // Track previous key for decompression
        std::vector<uint8_t> prev_key;

        uint16_t insert_pos = 0;
        for (uint16_t i = 0; i < parent_page->btr_count; ++i)
        {
            const auto *existing_node = reinterpret_cast<const SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(parent_page_data_ptr) + parent_offsets[i]);
            const uint8_t *existing_key_data =
                reinterpret_cast<const uint8_t *>(existing_node) + sizeof(SBBTreeNode);

            // Decompress existing key if compressed
            std::vector<uint8_t> full_existing_key = decompress_key(prev_key, existing_key_data,
                                                                     existing_node->btn_key_len,
                                                                     existing_node->btn_prefix_len);

            // Compare with separator key
            int cmp = compare_keys(separator_key, full_existing_key.data(), full_existing_key.size());
            if (cmp < 0)
            {
                insert_pos = i;
                break;
            }
            insert_pos = i + 1;

            // Update prev_key for next iteration
            prev_key = full_existing_key;
        }

        // Shift existing offsets to make room
        for (uint16_t i = parent_page->btr_count; i > insert_pos; --i)
        {
            parent_offsets[i] = parent_offsets[i - 1];
        }

        // Insert new offset
        parent_offsets[insert_pos] = parent_page->btr_high_water;

        // Update parent header
        parent_page->btr_count++;
        parent_page->btr_free_space -= (required_space + sizeof(uint16_t));

        // Unpin parent (mark as dirty)
        bp->unpinPage(parent_page_num, true, ctx);

        return Status::OK;
    }

    auto BTree::create_new_root(uint64_t left_page_num, const std::vector<uint8_t> &separator_key,
                                uint64_t right_page_num, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();

        // Allocate new root page
        uint32_t new_root_page_num_u32;
        Status status = pm->allocatePage(new_root_page_num_u32, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new root page");
            return status;
        }
        uint64_t new_root_page_num = new_root_page_num_u32;

        // Pin all three pages
        void *left_page_data_ptr;
        void *right_page_data_ptr;
        void *new_root_page_data_ptr;

        status = bp->pinPage(left_page_num, &left_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            pm->freePage(new_root_page_num_u32, ctx);
            return status;
        }

        status = bp->pinPage(right_page_num, &right_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            bp->unpinPage(left_page_num, false, ctx);
            pm->freePage(new_root_page_num_u32, ctx);
            return status;
        }

        status = bp->pinPage(new_root_page_num, &new_root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            bp->unpinPage(left_page_num, false, ctx);
            bp->unpinPage(right_page_num, false, ctx);
            pm->freePage(new_root_page_num_u32, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_page_data_ptr);
        auto *right_page = reinterpret_cast<SBBTreePage *>(right_page_data_ptr);
        auto *new_root_page = reinterpret_cast<SBBTreePage *>(new_root_page_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;

        try
        {
            // Initialize new root as internal page at level+1
            BTreePage new_root_btree_page(reinterpret_cast<uint8_t *>(new_root_page_data_ptr),
                                          page_size);
            uint16_t root_flags = static_cast<uint16_t>(BTreeFlags::ROOT) |
                                  static_cast<uint16_t>(BTreeFlags::LEFTMOST) |
                                  static_cast<uint16_t>(BTreeFlags::RIGHTMOST);
            new_root_btree_page.initialize(left_page->btr_index_uuid, left_page->btr_table_uuid,
                                           left_page->btr_level + 1, root_flags);

            // Add single entry: left_child (implicitly first), separator_key -> right_child
            uint32_t node_size = sizeof(SBBTreeNode) + separator_key.size() + sizeof(uint64_t);

            // For the first (leftmost) child, we typically store a dummy entry or special handling
            // In this implementation, we'll store the separator key with right_child pointer
            // The leftmost child is implicitly handled through tree traversal

            // Allocate space from end of page
            new_root_page->btr_high_water -= node_size;
            auto *new_node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(new_root_page_data_ptr) +
                new_root_page->btr_high_water);

            // Populate node
            new_node->btn_flags = 0;
            new_node->btn_prefix_len = 0;
            new_node->btn_suffix_trunc = 0;
            new_node->btn_key_len = separator_key.size();
            new_node->btn_tuple_count = 0;
            new_node->btn_child_page = left_page_num; // Left child for this key
            new_node->btn_xmin = 0;
            new_node->btn_xmax = 0;

            // Copy key
            uint8_t *key_location = reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
            memcpy(key_location, separator_key.data(), separator_key.size());

            // Update offset array
            auto *root_offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(new_root_page_data_ptr) + sizeof(SBBTreePage));
            root_offsets[0] = new_root_page->btr_high_water;

            // Update root header
            new_root_page->btr_count = 1;
            new_root_page->btr_free_space -= (node_size + sizeof(uint16_t));

            // CRITICAL FIX: Set the rightmost child pointer to right_page_num
            // The root now has one separator key that points left to left_page_num (stored in
            // btn_child_page) and the rightmost child is right_page_num
            new_root_page->btr_rightmost_child = right_page_num;

            // Remove ROOT flag from left page, update parent
            left_page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::ROOT);
            left_page->btr_parent_page = new_root_page_num;

            // Update right page parent
            right_page->btr_parent_page = new_root_page_num;

            // Update index info with new root page
            index_info_.idx_root_page = new_root_page_num;
            index_info_.idx_height++;

            // Unpin all pages (mark as dirty)
            bp->unpinPage(left_page_num, true, ctx);
            bp->unpinPage(right_page_num, true, ctx);
            bp->unpinPage(new_root_page_num, true, ctx);

            return Status::OK;
        }
        catch (const std::exception &e)
        {
            bp->unpinPage(left_page_num, false, ctx);
            bp->unpinPage(right_page_num, false, ctx);
            bp->unpinPage(new_root_page_num, false, ctx);
            pm->freePage(new_root_page_num_u32, ctx);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
            return Status::INVALID_ARGUMENT;
        }
    }

    // ============================================================================
    // VACUUM OPERATIONS
    // ============================================================================

    auto BTree::vacuum(VacuumStats *stats_out, ErrorContext *ctx) -> Status
    {
        VacuumStats stats = {};

        if (!db_ || !db_->buffer_pool() || !db_->page_manager())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database state for vacuum");
            return Status::INVALID_ARGUMENT;
        }

        BufferPool *bp = db_->buffer_pool();

        // Start from root and traverse all pages
        uint32_t root_page = static_cast<uint32_t>(index_info_.idx_root_page);

        // Pin root page
        void *root_page_data_ptr = nullptr;
        Status status = bp->pinPage(root_page, &root_page_data_ptr, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page for vacuum");
            return status;
        }

        auto *root = reinterpret_cast<SBBTreePage *>(root_page_data_ptr);
        uint16_t tree_height = root->btr_level + 1;

        bp->unpinPage(root_page, false, ctx);

        // Vacuum pages level by level, bottom-up
        // This allows us to merge pages and update parent pointers correctly
        for (int16_t level = 0; level < tree_height; ++level)
        {
            // Find all pages at this level by traversing siblings
            std::vector<uint32_t> pages_at_level;

            // Start from leftmost page at this level
            uint32_t current_page = root_page;

            // Navigate down to the correct level
            for (int16_t l = tree_height - 1; l > level; --l)
            {
                void *page_data_ptr = nullptr;
                status = bp->pinPage(current_page, &page_data_ptr, ctx);
                if (status != Status::OK)
                {
                    continue;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);

                // For internal nodes, follow the leftmost child
                if (page->btr_count > 0)
                {
                    auto *offsets = reinterpret_cast<uint16_t *>(
                        reinterpret_cast<uint8_t *>(page_data_ptr) + sizeof(SBBTreePage));
                    auto *first_node = reinterpret_cast<SBBTreeNode *>(
                        reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[0]);
                    current_page = static_cast<uint32_t>(first_node->btn_child_page);
                }
                else
                {
                    // Use rightmost child if no keys
                    current_page = static_cast<uint32_t>(page->btr_rightmost_child);
                }

                bp->unpinPage(static_cast<uint32_t>(page->btr_header.page_id), false, ctx);
            }

            // Now traverse all siblings at this level
            while (current_page != 0)
            {
                pages_at_level.push_back(current_page);

                void *page_data_ptr = nullptr;
                status = bp->pinPage(current_page, &page_data_ptr, ctx);
                if (status != Status::OK)
                {
                    break;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
                uint64_t next_page = page->btr_right_sibling;

                bp->unpinPage(current_page, false, ctx);

                current_page = static_cast<uint32_t>(next_page);
            }

            // Vacuum each page at this level
            for (uint32_t page_id : pages_at_level)
            {
                stats.pages_visited++;
                status = vacuumPage(page_id, stats, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    // Continue vacuuming even if one page fails
                    continue;
                }
            }

            // Attempt to merge adjacent pages at this level
            for (size_t i = 0; i + 1 < pages_at_level.size(); ++i)
            {
                uint32_t left_page = pages_at_level[i];
                uint32_t right_page = pages_at_level[i + 1];

                // Check if pages should be merged
                void *left_data_ptr = nullptr;
                void *right_data_ptr = nullptr;

                status = bp->pinPage(left_page, &left_data_ptr, ctx);
                if (status != Status::OK)
                    continue;

                status = bp->pinPage(right_page, &right_data_ptr, ctx);
                if (status != Status::OK)
                {
                    bp->unpinPage(left_page, false, ctx);
                    continue;
                }

                auto *left_page_hdr = reinterpret_cast<SBBTreePage *>(left_data_ptr);
                auto *right_page_hdr = reinterpret_cast<SBBTreePage *>(right_data_ptr);

                bool should_merge = shouldMergePages(left_page_hdr, right_page_hdr);

                bp->unpinPage(left_page, false, ctx);
                bp->unpinPage(right_page, false, ctx);

                if (should_merge)
                {
                    status = mergePages(left_page, right_page, stats, ctx);
                    if (status == Status::OK)
                    {
                        // Page was merged, skip right_page in future iterations
                        pages_at_level.erase(pages_at_level.begin() + i + 1);
                    }
                }
            }
        }

        if (stats_out)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

    auto BTree::vacuumPage(uint32_t page_id, VacuumStats &stats, ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();

        void *page_data_ptr = nullptr;
        Status status = bp->pinPage(page_id, &page_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
        uint32_t page_size = page->btr_header.page_size;

        // Check if page has garbage
        if (!(page->btr_flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE)))
        {
            bp->unpinPage(page_id, false, ctx);
            return Status::OK;
        }

        // Count deleted nodes
        auto *offsets = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(page_data_ptr) +
                                                     sizeof(SBBTreePage));

        uint16_t deleted_count = 0;
        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[i]);

            if (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED))
            {
                deleted_count++;
            }
        }

        if (deleted_count == 0)
        {
            // Clear the HAS_GARBAGE flag
            page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
            bp->unpinPage(page_id, true, ctx);
            return Status::OK;
        }

        // Compact the page
        status = compactPage(reinterpret_cast<uint8_t *>(page_data_ptr), page_size, stats);

        if (status == Status::OK)
        {
            stats.pages_vacuumed++;
            stats.nodes_removed += deleted_count;
            bp->unpinPage(page_id, true, ctx);
        }
        else
        {
            bp->unpinPage(page_id, false, ctx);
        }

        return status;
    }

    auto BTree::compactPage(uint8_t *page_data, uint32_t page_size, VacuumStats &stats) -> Status
    {
        auto *page = reinterpret_cast<SBBTreePage *>(page_data);
        auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

        // Create a temporary buffer for compaction
        std::vector<uint8_t> temp_buffer(page_size);
        uint8_t *temp_data = temp_buffer.data();

        // Copy page header
        std::memcpy(temp_data, page_data, sizeof(SBBTreePage));
        auto *temp_page = reinterpret_cast<SBBTreePage *>(temp_data);

        // Initialize new offset array
        auto *temp_offsets = reinterpret_cast<uint16_t *>(temp_data + sizeof(SBBTreePage));

        // Reset high water mark
        temp_page->btr_high_water = page_size;
        temp_page->btr_count = 0;
        temp_page->btr_free_space = page_size - sizeof(SBBTreePage);
        temp_page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);

        // Copy non-deleted nodes
        uint64_t bytes_reclaimed = 0;

        for (uint16_t i = 0; i < page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[i]);

            // Skip deleted nodes
            if (node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED))
            {
                // Calculate size of deleted node
                uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;

                if (page->btr_level == 0)
                {
                    // Leaf node: has tuple IDs
                    node_size += node->btn_tuple_count * sizeof(uint64_t);
                }
                else
                {
                    // Internal node: has child pointer (already in btn_child_page)
                    // No additional space needed
                }

                bytes_reclaimed += node_size;
                continue;
            }

            // Calculate node size
            uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;

            if (page->btr_level == 0)
            {
                // Leaf node
                node_size += node->btn_tuple_count * sizeof(uint64_t);
            }

            // Allocate space from end of temp page
            temp_page->btr_high_water -= node_size;

            // Copy node data
            uint8_t *dest_node = temp_data + temp_page->btr_high_water;
            std::memcpy(dest_node, node, node_size);

            // Update offset array
            temp_offsets[temp_page->btr_count] = temp_page->btr_high_water;
            temp_page->btr_count++;

            // Update free space accounting
            temp_page->btr_free_space -= (node_size + sizeof(uint16_t));
        }

        // Copy compacted data back to original page
        std::memcpy(page_data, temp_data, page_size);

        stats.bytes_reclaimed += bytes_reclaimed;

        return Status::OK;
    }

    bool BTree::shouldMergePages(const SBBTreePage *page1, const SBBTreePage *page2) const
    {
        if (!page1 || !page2)
        {
            return false;
        }

        // Don't merge if they're not siblings
        if (page1->btr_right_sibling != page2->btr_header.page_id)
        {
            return false;
        }

        // Don't merge if they're at different levels
        if (page1->btr_level != page2->btr_level)
        {
            return false;
        }

        // Don't merge if they belong to different indices
        if (std::memcmp(page1->btr_index_uuid.bytes.data(), page2->btr_index_uuid.bytes.data(),
                        16) != 0)
        {
            return false;
        }

        // Calculate total space needed if merged
        uint32_t page_size = page1->btr_header.page_size;

        // Space needed for all nodes from both pages
        uint32_t total_nodes = page1->btr_count + page2->btr_count;
        uint32_t total_offset_space = total_nodes * sizeof(uint16_t);

        // Calculate approximate data size (conservative estimate)
        uint32_t page1_data_size = page_size - page1->btr_free_space - sizeof(SBBTreePage);
        uint32_t page2_data_size = page_size - page2->btr_free_space - sizeof(SBBTreePage);
        uint32_t total_data_size = page1_data_size + page2_data_size;

        uint32_t required_space = sizeof(SBBTreePage) + total_offset_space + total_data_size;

        // Only merge if combined data fits in one page with some margin (80% threshold)
        uint32_t threshold = (page_size * 80) / 100;

        return required_space <= threshold;
    }

    auto BTree::mergePages(uint32_t left_page_id, uint32_t right_page_id, VacuumStats &stats,
                           ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();
        PageManager *pm = db_->page_manager();

        // Pin both pages
        void *left_data_ptr = nullptr;
        void *right_data_ptr = nullptr;

        Status status = bp->pinPage(left_page_id, &left_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = bp->pinPage(right_page_id, &right_data_ptr, ctx);
        if (status != Status::OK)
        {
            bp->unpinPage(left_page_id, false, ctx);
            return status;
        }

        auto *left_page = reinterpret_cast<SBBTreePage *>(left_data_ptr);
        auto *right_page = reinterpret_cast<SBBTreePage *>(right_data_ptr);
        uint32_t page_size = left_page->btr_header.page_size;

        // Get offset arrays
        auto *left_offsets = reinterpret_cast<uint16_t *>(
            reinterpret_cast<uint8_t *>(left_data_ptr) + sizeof(SBBTreePage));
        auto *right_offsets = reinterpret_cast<uint16_t *>(
            reinterpret_cast<uint8_t *>(right_data_ptr) + sizeof(SBBTreePage));

        // Copy all nodes from right page to left page
        for (uint16_t i = 0; i < right_page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(right_data_ptr) + right_offsets[i]);

            // Calculate node size
            uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;

            if (right_page->btr_level == 0)
            {
                // Leaf node
                node_size += node->btn_tuple_count * sizeof(uint64_t);
            }

            // Check if there's enough space
            if (left_page->btr_free_space < node_size + sizeof(uint16_t))
            {
                // Not enough space, abort merge
                bp->unpinPage(left_page_id, false, ctx);
                bp->unpinPage(right_page_id, false, ctx);
                return Status::PAGE_FULL;
            }

            // Allocate space from end of left page
            left_page->btr_high_water -= node_size;

            // Copy node data
            uint8_t *dest_node =
                reinterpret_cast<uint8_t *>(left_data_ptr) + left_page->btr_high_water;
            std::memcpy(dest_node, node, node_size);

            // Update offset array
            left_offsets[left_page->btr_count] = left_page->btr_high_water;
            left_page->btr_count++;

            // Update free space
            left_page->btr_free_space -= (node_size + sizeof(uint16_t));
        }

        // Update sibling pointers
        left_page->btr_right_sibling = right_page->btr_right_sibling;

        // Update right sibling's left pointer if it exists
        if (right_page->btr_right_sibling != 0)
        {
            void *right_sibling_data_ptr = nullptr;
            status = bp->pinPage(static_cast<uint32_t>(right_page->btr_right_sibling),
                                 &right_sibling_data_ptr, ctx);
            if (status == Status::OK)
            {
                auto *right_sibling = reinterpret_cast<SBBTreePage *>(right_sibling_data_ptr);
                right_sibling->btr_left_sibling = left_page_id;
                bp->unpinPage(static_cast<uint32_t>(right_page->btr_right_sibling), true, ctx);
            }
        }

        // CRITICAL FIX (Issue 2.11): Update parent to remove separator key for right page
        // When pages are merged, the separator key in the parent that points to the right page
        // must be removed to maintain B-tree structure integrity
        uint64_t parent_page_num = right_page->btr_parent_page;

        // Mark pages as dirty and unpin before updating parent
        bp->unpinPage(left_page_id, true, ctx);
        bp->unpinPage(right_page_id, false, ctx);

        // Free the right page
        pm->freePage(right_page_id, ctx);

        // Update parent if it exists (not root)
        if (parent_page_num != 0)
        {
            status = removeFromParent(parent_page_num, right_page_id, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                // Log warning but don't fail the merge - pages are already merged
                // The parent inconsistency will be detected during validation
                // TODO: Consider implementing parent merge if parent becomes underutilized
            }
        }

        stats.pages_merged++;

        return Status::OK;
    }

    auto BTree::removeFromParent(uint64_t parent_page_num, uint64_t child_page_id,
                                 ErrorContext *ctx) -> Status
    {
        BufferPool *bp = db_->buffer_pool();

        // Pin parent page
        void *parent_data_ptr = nullptr;
        Status status = bp->pinPage(parent_page_num, &parent_data_ptr, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *parent_page = reinterpret_cast<SBBTreePage *>(parent_data_ptr);
        auto *parent_offsets = reinterpret_cast<uint16_t *>(
            reinterpret_cast<uint8_t *>(parent_data_ptr) + sizeof(SBBTreePage));

        // Find the entry that points to child_page_id
        int16_t entry_to_remove = -1;
        for (uint16_t i = 0; i < parent_page->btr_count; ++i)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(parent_data_ptr) + parent_offsets[i]);

            // Check if this node points to the child page we're removing
            if (node->btn_child_page == child_page_id)
            {
                entry_to_remove = i;
                break;
            }
        }

        if (entry_to_remove < 0)
        {
            // Child not found in parent - might be rightmost child or corruption
            // Check if child is the rightmost child
            if (parent_page->btr_rightmost_child == child_page_id)
            {
                // Need to promote the last entry's child to be the new rightmost child
                if (parent_page->btr_count > 0)
                {
                    auto *last_node = reinterpret_cast<SBBTreeNode *>(
                        reinterpret_cast<uint8_t *>(parent_data_ptr) +
                        parent_offsets[parent_page->btr_count - 1]);
                    parent_page->btr_rightmost_child = last_node->btn_child_page;

                    // Remove the last entry since its child became the rightmost
                    parent_page->btr_count--;

                    // Recalculate free space
                    uint32_t node_size = sizeof(SBBTreeNode) + last_node->btn_key_len;
                    parent_page->btr_free_space += (node_size + sizeof(uint16_t));

                    bp->unpinPage(parent_page_num, true, ctx);
                    return Status::OK;
                }
            }

            bp->unpinPage(parent_page_num, false, ctx);
            return Status::NOT_FOUND;
        }

        // Mark the node as deleted (physical removal done by compaction during next vacuum)
        auto *node_to_remove = reinterpret_cast<SBBTreeNode *>(
            reinterpret_cast<uint8_t *>(parent_data_ptr) + parent_offsets[entry_to_remove]);
        node_to_remove->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);

        // Set HAS_GARBAGE flag to indicate page needs vacuuming
        parent_page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);

        bp->unpinPage(parent_page_num, true, ctx);

        return Status::OK;
    }

    // PHASE 2 TASK 2.2: IndexGCInterface implementation
    // Remove index entries pointing to dead tuples
    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    Status BTree::removeDeadEntries(const std::vector<TID> &dead_tids,
                                    uint64_t *entries_removed_out,
                                    uint64_t *pages_modified_out,
                                    ErrorContext *ctx)
    {
        // Initialize output counters
        if (entries_removed_out != nullptr)
        {
            *entries_removed_out = 0;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = 0;
        }

        // Early exit if no dead TIDs
        if (dead_tids.empty())
        {
            return Status::OK;
        }

        // PHASE 1.5: Convert TID structs to legacy format for lookup
        // On-disk format still stores uint64_t, so we build a set of legacy TIDs
        std::set<uint64_t> dead_set;
        for (const TID &tid : dead_tids)
        {
            uint64_t legacy = convertTIDtoLegacy(tid);
            if (legacy != 0)  // Skip custom tablespace TIDs (not supported in ALPHA)
            {
                dead_set.insert(legacy);
            }
        }

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
            return Status::INVALID_ARGUMENT;
        }

        // Statistics
        uint64_t total_entries_removed = 0;
        uint64_t total_pages_modified = 0;
        bool had_errors = false;

        // Find the leftmost leaf page
        // Strategy: Start from root and go left at each level until we hit a leaf
        uint64_t current_page_num = index_info_.idx_root_page;

        // Navigate to leftmost leaf
        while (true)
        {
            void *page_buffer = nullptr;
            Status pin_status = bp->pinPage(current_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, pin_status, "Failed to pin page during GC navigation");
                return pin_status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint16_t level = page->btr_level;

            if (level == 0)
            {
                // We've reached a leaf, unpin and break
                bp->unpinPage(current_page_num, false, ctx);
                break;
            }

            // Internal node - go to leftmost child
            if (page->btr_count == 0)
            {
                // Empty internal node - shouldn't happen, but handle gracefully
                bp->unpinPage(current_page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED, "Empty internal node encountered");
                return Status::INDEX_CORRUPTED;
            }

            // Get the first node to find its left child
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);
            auto *first_node = reinterpret_cast<SBBTreeNode *>(
                page_data + sizeof(SBBTreePage));

            uint64_t next_page = first_node->btn_child_page;
            bp->unpinPage(current_page_num, false, ctx);

            current_page_num = next_page;
        }

        // Now scan all leaf pages left-to-right using sibling pointers
        uint64_t leaf_page_num = current_page_num;

        while (leaf_page_num != 0)
        {
            void *page_buffer = nullptr;
            Status pin_status = bp->pinPage(leaf_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                LOG_WARNING(VACUUM, "B-Tree GC: Failed to pin leaf page %lu: %d",
                            leaf_page_num, static_cast<int>(pin_status));
                had_errors = true;
                break; // Stop scanning on error
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

            // Verify this is a leaf page
            if (page->btr_level != 0)
            {
                LOG_WARNING(VACUUM, "B-Tree GC: Expected leaf page but got level %u at page %lu",
                            page->btr_level, leaf_page_num);
                bp->unpinPage(leaf_page_num, false, ctx);
                had_errors = true;
                break;
            }

            uint16_t entry_count = page->btr_count;
            uint64_t entries_removed_this_page = 0;
            bool page_modified = false;

            // Scan all entries on this leaf page
            // We'll mark entries as deleted by setting the DELETED flag
            uint8_t *current_offset = page_data + sizeof(SBBTreePage);

            for (uint16_t slot = 0; slot < entry_count; slot++)
            {
                auto *node = reinterpret_cast<SBBTreeNode *>(current_offset);

                // Skip already deleted entries
                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    // Move to next node (skip key data and tuple IDs)
                    uint16_t key_len = node->btn_key_len;
                    uint32_t tuple_count = node->btn_tuple_count;
                    current_offset += sizeof(SBBTreeNode) + key_len +
                                      (tuple_count * sizeof(uint64_t));
                    continue;
                }

                // Get tuple IDs for this entry
                uint16_t key_len = node->btn_key_len;
                uint32_t tuple_count = node->btn_tuple_count;
                uint8_t *tuple_ids_ptr = current_offset + sizeof(SBBTreeNode) + key_len;
                auto *tuple_ids = reinterpret_cast<uint64_t *>(tuple_ids_ptr);

                // Check if any of this entry's tuple IDs are in the dead set
                bool has_dead_tuples = false;
                for (uint32_t i = 0; i < tuple_count; i++)
                {
                    if (dead_set.find(tuple_ids[i]) != dead_set.end())
                    {
                        has_dead_tuples = true;
                        break;
                    }
                }

                if (has_dead_tuples)
                {
                    // Mark entry as deleted
                    node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);
                    entries_removed_this_page++;
                    page_modified = true;
                }

                // Move to next node
                current_offset += sizeof(SBBTreeNode) + key_len +
                                  (tuple_count * sizeof(uint64_t));
            }

            // If we modified this page, set the HAS_GARBAGE flag
            if (page_modified)
            {
                page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
                total_pages_modified++;
                total_entries_removed += entries_removed_this_page;
            }

            // Get next leaf page via right sibling pointer
            uint64_t next_leaf = page->btr_right_sibling;

            // Unpin current page
            bp->unpinPage(leaf_page_num, page_modified, ctx);

            // Move to next leaf
            leaf_page_num = next_leaf;
        }

        // Update output counters
        if (entries_removed_out != nullptr)
        {
            *entries_removed_out = total_entries_removed;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = total_pages_modified;
        }

        // Return appropriate status
        if (had_errors)
        {
            // Had some errors but may have removed some entries
            // Caller can check entries_removed_out to see progress
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    // ============================================================================
    // PHASE 5 TASK 5.2: B-TREE TID UPDATES FOR TABLESPACE MIGRATION
    // ============================================================================

    /**
     * updateTIDsAfterMigration - Update TIDs in B-Tree leaf nodes after table migration
     *
     * This method is called when a table has been migrated to a different tablespace.
     * It traverses all leaf nodes in the B-Tree and updates TIDs that reference
     * migrated heap pages.
     *
     * Algorithm:
     * 1. Navigate to leftmost leaf page (same as removeDeadEntries)
     * 2. Scan all leaf pages left-to-right using sibling pointers
     * 3. For each leaf page:
     *    a. Scan all index entries (nodes)
     *    b. For each entry, scan all tuple IDs (TIDs stored as uint64_t)
     *    c. Extract GPID from TID (upper 64 bits of legacy format)
     *    d. Check if GPID is in tid_mapping (old GPID -> new GPID)
     *    e. If found, update TID with new GPID
     *    f. Mark page as dirty if any TIDs updated
     * 4. Continue to next leaf via btr_right_sibling pointer
     * 5. Return total TIDs updated and pages modified
     *
     * Note: TIDs are stored on-disk as uint64_t in legacy format:
     *       (32-bit page_id << 32) | (16-bit item_id << 16)
     *       The tid_mapping maps GPIDs (full 64-bit GPID values)
     *
     * @param tid_mapping Map of old GPID -> new GPID for migrated pages
     * @param tids_updated_out Output: Total number of TIDs updated
     * @param pages_modified_out Output: Total number of leaf pages modified
     * @param ctx Error context
     * @return Status::OK on success, error status otherwise
     */
    Status BTree::updateTIDsAfterMigration(const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
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

        BufferPool *bp = db_->buffer_pool();
        if (!bp)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
            return Status::INVALID_ARGUMENT;
        }

        // Statistics
        uint64_t total_tids_updated = 0;
        uint64_t total_pages_modified = 0;
        bool had_errors = false;

        // ===== STEP 1: Find the leftmost leaf page =====
        // Strategy: Start from root and go left at each level until we hit a leaf
        uint64_t current_page_num = index_info_.idx_root_page;

        // Navigate to leftmost leaf
        while (true)
        {
            void *page_buffer = nullptr;
            Status pin_status = bp->pinPage(current_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, pin_status,
                                "Failed to pin page during TID update navigation");
                return pin_status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint16_t level = page->btr_level;

            if (level == 0)
            {
                // We've reached a leaf, unpin and break
                bp->unpinPage(current_page_num, false, ctx);
                break;
            }

            // Internal node - go to leftmost child
            if (page->btr_count == 0)
            {
                // Empty internal node - shouldn't happen, but handle gracefully
                bp->unpinPage(current_page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED,
                                "Empty internal node encountered during TID update");
                return Status::INDEX_CORRUPTED;
            }

            // Get the first node to find its left child
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);
            auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));
            auto *first_node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[0]);

            uint64_t next_page = first_node->btn_child_page;
            bp->unpinPage(current_page_num, false, ctx);

            current_page_num = next_page;
        }

        // ===== STEP 2: Scan all leaf pages left-to-right using sibling pointers =====
        uint64_t leaf_page_num = current_page_num;

        while (leaf_page_num != 0)
        {
            void *page_buffer = nullptr;
            Status pin_status = bp->pinPage(leaf_page_num, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                LOG_WARNING(CATALOG,
                           "B-Tree TID update: Failed to pin leaf page %lu: %d",
                           leaf_page_num, static_cast<int>(pin_status));
                had_errors = true;
                break; // Stop scanning on error
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

            // Verify this is a leaf page
            if (page->btr_level != 0)
            {
                LOG_WARNING(CATALOG,
                           "B-Tree TID update: Expected leaf page but got level %u at page %lu",
                           page->btr_level, leaf_page_num);
                bp->unpinPage(leaf_page_num, false, ctx);
                had_errors = true;
                break;
            }

            uint16_t entry_count = page->btr_count;
            uint64_t tids_updated_this_page = 0;
            bool page_modified = false;

            // ===== STEP 3: Scan all entries on this leaf page =====
            auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

            for (uint16_t slot = 0; slot < entry_count; slot++)
            {
                auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[slot]);

                // Skip deleted entries (from vacuum operations)
                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    continue;
                }

                // Get tuple IDs for this entry
                uint16_t key_len = node->btn_key_len;
                uint32_t tuple_count = node->btn_tuple_count;

                // Tuple IDs are stored right after the key data
                uint8_t *tuple_ids_ptr = reinterpret_cast<uint8_t *>(node) +
                                        sizeof(SBBTreeNode) + key_len;
                auto *tuple_ids = reinterpret_cast<uint64_t *>(tuple_ids_ptr);

                // ===== STEP 4: Update each tuple ID if its GPID is in tid_mapping =====
                for (uint32_t i = 0; i < tuple_count; i++)
                {
                    uint64_t legacy_tid = tuple_ids[i];

                    // Extract GPID from legacy TID format
                    // Legacy format: (32-bit page_id << 32) | (16-bit item_id << 16)
                    // GPID is the upper 64 bits when interpreted as full TID
                    // For legacy format, GPID = makeGPID(0, page_id)
                    uint32_t page_id = static_cast<uint32_t>(legacy_tid >> 32);
                    uint16_t item_id = static_cast<uint16_t>((legacy_tid >> 16) & 0xFFFF);
                    GPID old_gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));

                    // Check if this GPID was migrated
                    auto it = tid_mapping.find(old_gpid);
                    if (it != tid_mapping.end())
                    {
                        // Found! Update TID with new GPID
                        GPID new_gpid = it->second;

                        // Extract new page number from new GPID
                        uint64_t new_page_number = getPageNumber(TID(new_gpid, 0));
                        uint32_t new_page_id = static_cast<uint32_t>(new_page_number);

                        // Construct new legacy TID with updated page_id
                        uint64_t new_legacy_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                                                 (static_cast<uint64_t>(item_id) << 16);

                        // Update the TID in-place
                        tuple_ids[i] = new_legacy_tid;
                        tids_updated_this_page++;
                        page_modified = true;
                    }
                }
            }

            // If we modified this page, update statistics
            if (page_modified)
            {
                total_pages_modified++;
                total_tids_updated += tids_updated_this_page;
            }

            // Get next leaf page via right sibling pointer
            uint64_t next_leaf = page->btr_right_sibling;

            // Unpin current page (mark dirty if modified)
            bp->unpinPage(leaf_page_num, page_modified, ctx);

            // Move to next leaf
            leaf_page_num = next_leaf;
        }

        // Update output counters
        if (tids_updated_out != nullptr)
        {
            *tids_updated_out = total_tids_updated;
        }
        if (pages_modified_out != nullptr)
        {
            *pages_modified_out = total_pages_modified;
        }

        // Return appropriate status
        if (had_errors)
        {
            // Had some errors but may have updated some TIDs
            // Caller can check tids_updated_out to see progress
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                            "Errors encountered during B-Tree TID update");
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

} // namespace scratchbird::core
