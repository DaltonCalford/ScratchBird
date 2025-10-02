#include <utility>

#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"


    namespace scratchbird::core
    {

        BTree::BTree(Database *db, SBBTreeIndex index_info)
            : db_(db), index_info_(std::move(index_info))
        {
            // Constructor implementation
        }

        BTree::~BTree()
        {
            // Destructor implementation
        }

        auto BTree::insert(const std::vector<uint8_t> &key, uint64_t tuple_id, ErrorContext *ctx) -> Status
        {
            // Find the appropriate leaf page for this key
            uint64_t leaf_page_num;
            Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            BufferPool *bp = db_->buffer_pool();
            void *page_data_ptr;
            status = bp->pinPage(leaf_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Wrap the page in a BTreePage helper
            auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
            uint32_t page_size = page->btr_header.page_size;

            // Create a Tuple for the add_node call
            Tuple tuple;
            tuple.tid = tuple_id;
            tuple.page_id = 0;    // Not used by add_node
            tuple.item_id = 0;    // Not used by add_node
            tuple.data = nullptr; // Not used by add_node
            tuple.data_size = 0;  // Not used by add_node

            try
            {
                BTreePage btree_page(reinterpret_cast<uint8_t *>(page_data_ptr), page_size);

                status = btree_page.add_node(key, tuple, ctx);
                if (status == Status::PAGE_FULL)
                {
                    // Page is full - would need to split
                    // For now, return an error indicating split is needed
                    bp->unpinPage(leaf_page_num, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                                      "B-tree page is full, split not yet implemented");
                    return Status::PAGE_FULL;
                }
                else if (status != Status::OK)
                {
                    bp->unpinPage(leaf_page_num, false, ctx);
                    return status;
                }

                // Mark page as dirty since we modified it
                bp->unpinPage(leaf_page_num, true, ctx);
                return Status::OK;
            }
            catch (const std::exception &e)
            {
                bp->unpinPage(leaf_page_num, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, e.what());
                return Status::INVALID_ARGUMENT;
            }
        }

        // Searches for a key within a single B-Tree page using binary search.
        static auto searchPage(const SBBTreePage *page, const std::vector<uint8_t> &key,
                                std::vector<uint64_t> *tuple_ids_out) -> bool
        {
            const auto *page_data = reinterpret_cast<const uint8_t *>(page);

            // Get the node offsets array
            const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

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
                    for (int i = 0; i < page->btr_count; ++i)
                    {
                        const auto *n = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);
                        if ((n->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                        {
                            continue;
                        }

                        const uint8_t *node_key_data = reinterpret_cast<const uint8_t *>(n) + sizeof(SBBTreeNode);
                        std::vector<uint8_t> node_key(node_key_data, node_key_data + n->btn_key_len);

                        if (key == node_key)
                        {
                            found_index = i;
                            break;
                        }
                    }
                    break;
                }

                // Extract the node's key
                const uint8_t *node_key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

                // Compare keys
                if (key == node_key)
                {
                    found_index = mid;
                    break;
                }
                else if (key < node_key)
                {
                    right = mid - 1;
                }
                else
                {
                    left = mid + 1;
                }
            }

            // If found, collect the tuple IDs
            if (found_index >= 0)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[found_index]);
                const uint8_t *node_key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

                const auto *tuple_ids_ptr = reinterpret_cast<const uint64_t *>(
                    node_key_data + node->btn_key_len);

                for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                {
                    tuple_ids_out->push_back(tuple_ids_ptr[j]);
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

            while (true)
            {
                void *page_data_ptr;
                Status status = bp->pinPage(current_page_num, &page_data_ptr, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                const auto *page = reinterpret_cast<const SBBTreePage *>(page_data_ptr);

                if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0)
                {
                    *page_num_out = current_page_num;
                    bp->unpinPage(current_page_num, false, ctx);
                    return Status::OK;
                }

                // This is an internal page, find the correct child to descend to
                const auto *page_data = reinterpret_cast<const uint8_t *>(page);
                uint64_t next_page_num = 0;

                // Get the node offsets array
                const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));

                // Linear search through nodes to find the correct child
                // Each internal node has a key and a child pointer to the left of that key
                for (uint16_t i = 0; i < page->btr_count; ++i)
                {
                    const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);

                    // Extract the node's key
                    const uint8_t *node_key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                    std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

                    // If our search key is less than this node's key, go to this node's left child
                    if (key < node_key)
                    {
                        next_page_num = node->btn_child_page;
                        break;
                    }
                }

                // If we didn't find a suitable child (key >= all keys), use the rightmost child
                // In a proper B-tree, this would be stored separately or as the last entry
                if (next_page_num == 0)
                {
                    // Use the last node's child page as fallback
                    if (page->btr_count > 0)
                    {
                        const auto *last_node = reinterpret_cast<const SBBTreeNode *>(
                            page_data + offsets[page->btr_count - 1]);
                        next_page_num = last_node->btn_child_page;
                    }
                    else
                    {
                        // Empty internal page - this shouldn't happen
                        bp->unpinPage(current_page_num, false, ctx);
                        return Status::PAGE_CORRUPT;
                    }
                }

                bp->unpinPage(current_page_num, false, ctx);
                current_page_num = next_page_num;
            }
        }

        auto BTree::search(const std::vector<uint8_t> &key, std::vector<uint64_t> *tuple_ids_out,
                             ErrorContext *ctx) -> Status
        {
            uint64_t leaf_page_num;
            Status status = find_leaf_page(key, &leaf_page_num, false, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            BufferPool *bp = db_->buffer_pool();
            void *page_data_ptr;
            status = bp->pinPage(leaf_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            const auto *page = reinterpret_cast<const SBBTreePage *>(page_data_ptr);

            bool found = searchPage(page, key, tuple_ids_out);

            bp->unpinPage(leaf_page_num, false, ctx);

            if (found)
            {
                return Status::OK;
            }
            
                            return Status::NOT_FOUND;
           
        }

        auto BTree::remove(const std::vector<uint8_t> &key, uint64_t tuple_id, ErrorContext *ctx) -> Status
        {
            // Find the appropriate leaf page for this key
            uint64_t leaf_page_num;
            Status status = find_leaf_page(key, &leaf_page_num, true, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            BufferPool *bp = db_->buffer_pool();
            void *page_data_ptr;
            status = bp->pinPage(leaf_page_num, &page_data_ptr, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_data_ptr);
            const auto *page_data = reinterpret_cast<const uint8_t *>(page);

            // Get the node offsets array
            auto *offsets = reinterpret_cast<uint16_t *>(
                reinterpret_cast<uint8_t *>(page_data_ptr) + sizeof(SBBTreePage));

            // Search for the key and matching tuple_id
            bool found = false;
            uint16_t node_to_remove = 0;

            for (uint16_t i = 0; i < page->btr_count; ++i)
            {
                const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[i]);

                // Extract the node's key
                const uint8_t *node_key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
                std::vector<uint8_t> node_key(node_key_data, node_key_data + node->btn_key_len);

                if (key == node_key)
                {
                    // Check if the tuple_id matches
                    const auto *tuple_ids_ptr = reinterpret_cast<const uint64_t *>(
                        node_key_data + node->btn_key_len);

                    for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                    {
                        if (tuple_ids_ptr[j] == tuple_id)
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
            }

            if (!found)
            {
                bp->unpinPage(leaf_page_num, false, ctx);
                return Status::NOT_FOUND;
            }

            // For now, mark the node as deleted rather than physically removing it
            // Physical removal would require compacting the page
            auto *node_to_mark = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(page_data_ptr) + offsets[node_to_remove]);
            node_to_mark->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);

            // Mark page as dirty since we modified it
            bp->unpinPage(leaf_page_num, true, ctx);
            return Status::OK;
        }

    } // namespace scratchbird::core
