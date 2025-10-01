#include <utility>

#include "scratchbird/core/btree.h"
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
            // TODO: Implement B-Tree insertion logic
            return Status::INVALID_ARGUMENT;
        }

        // Searches for a key within a single B-Tree page.
        // For now, this is a simple linear scan. A binary search would be more efficient.
        static auto searchPage(const SBBTreePage *page, const std::vector<uint8_t> &key,
                                std::vector<uint64_t> *tuple_ids_out) -> bool
        {
            // TODO: Implement a more efficient search than linear scan
            const auto *page_data = reinterpret_cast<const uint8_t *>(page);
            uint16_t current_offset = page->btr_high_water;

            for (int i = 0; i < page->btr_count; ++i)
            {
                const auto *node =
                    reinterpret_cast<const SBBTreeNode *>(page_data + current_offset);

                // This is a simplified key comparison. A real implementation would need to handle
                // different key types and prefix/suffix compression.
                std::vector<uint8_t> node_key(page_data + current_offset + sizeof(SBBTreeNode),
                                              page_data + current_offset + sizeof(SBBTreeNode) +
                                                  node->btn_key_len);

                if (key == node_key)
                {
                    // Found the key. Now, collect the tuple IDs.
                    // This part is also simplified. A real implementation would need to handle
                    // duplicate keys that might span multiple nodes or overflow pages.
                    const auto *tuple_ids_ptr = reinterpret_cast<const uint64_t *>(
                        page_data + current_offset + sizeof(SBBTreeNode) + node->btn_key_len);
                    for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
                    {
                        tuple_ids_out->push_back(tuple_ids_ptr[j]);
                    }
                    return true;
                }

                // Move to the next node. The layout on page is not specified yet, assuming they are
                // contiguous for now. This is a placeholder and will need to be updated with the
                // actual on-disk layout.
                current_offset += sizeof(SBBTreeNode) + node->btn_key_len +
                                  (node->btn_tuple_count * sizeof(uint64_t));
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

                // This is an internal page, so we need to find the child to descend to.
                // The logic here is highly simplified. A real implementation would need to
                // iterate through the nodes on the page, compare keys, and find the correct
                // child pointer.
                // For now, we'll just descend to the first child as a placeholder.
                const auto *page_data = reinterpret_cast<const uint8_t *>(page);
                const auto *first_node =
                    reinterpret_cast<const SBBTreeNode *>(page_data + page->btr_high_water);
                uint64_t next_page_num = first_node->btn_child_page;

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
            // TODO: Implement B-Tree removal logic
            return Status::INVALID_ARGUMENT;
        }

    } // namespace scratchbird::core
