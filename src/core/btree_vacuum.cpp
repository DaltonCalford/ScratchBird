#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include <cstring>
#include <vector>

namespace scratchbird::core
{

    auto BTree::vacuum(VacuumStats *stats_out, ErrorContext *ctx) -> Status
    {
        VacuumStats stats{};
        stats.pages_visited = 0;
        stats.pages_vacuumed = 0;
        stats.nodes_removed = 0;
        stats.bytes_reclaimed = 0;
        stats.pages_merged = 0;

        // Start from root and traverse all leaf pages via sibling pointers
        uint32_t current_page = index_info_.idx_root_page;

        // Navigate to leftmost leaf
        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(current_page, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);

        // Walk down to leftmost leaf
        while ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) == 0)
        {
            // Internal node - get first child
            auto *offsets = reinterpret_cast<uint16_t *>(reinterpret_cast<uint8_t *>(page_buffer) +
                                                         sizeof(SBBTreePage));

            if (page->btr_count == 0)
            {
                bp->unpinPage(current_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Empty internal node");
                return Status::PAGE_CORRUPT;
            }

            auto *first_node = reinterpret_cast<SBBTreeNode *>(
                reinterpret_cast<uint8_t *>(page_buffer) + offsets[0]);

            uint32_t next_page = static_cast<uint32_t>(first_node->btn_child_page);
            bp->unpinPage(current_page, false, ctx);

            if (next_page == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid child pointer");
                return Status::PAGE_CORRUPT;
            }

            current_page = next_page;
            status = bp->pinPage(current_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            page = reinterpret_cast<SBBTreePage *>(page_buffer);
        }

        bp->unpinPage(current_page, false, ctx);

        // Now current_page is leftmost leaf - traverse all leaves
        while (current_page != 0)
        {
            // Vacuum this page
            status = vacuumPage(current_page, stats, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                // Continue on errors - vacuum is best-effort
            }

            // Get next sibling
            status = bp->pinPage(current_page, &page_buffer, ctx);
            if (status != Status::OK)
            {
                break;
            }

            page = reinterpret_cast<SBBTreePage *>(page_buffer);
            uint32_t next_page = static_cast<uint32_t>(page->btr_right_sibling);

            bp->unpinPage(current_page, false, ctx);

            current_page = next_page;
        }

        // Update index statistics
        index_info_.idx_deleted_count = 0; // All deleted nodes removed

        if (stats_out)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

    auto BTree::vacuumPage(uint32_t page_id, VacuumStats &stats, ErrorContext *ctx) -> Status
    {
        stats.pages_visited++;

        BufferPool *bp = db_->buffer_pool();
        void *page_buffer;
        Status status = bp->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        auto *page = reinterpret_cast<SBBTreePage *>(page_data);

        // Check if page needs vacuuming
        bool has_deleted = (page->btr_flags & static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE)) != 0;

        if (!has_deleted)
        {
            // Quick scan for deleted nodes
            auto *offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

            for (uint16_t i = 0; i < page->btr_count; i++)
            {
                auto *node = reinterpret_cast<SBBTreeNode *>(page_data + offsets[i]);
                if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
                {
                    has_deleted = true;
                    page->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);
                    break;
                }
            }
        }

        if (!has_deleted)
        {
            // No deleted nodes - nothing to do
            bp->unpinPage(page_id, false, ctx);
            return Status::OK;
        }

        // Compact the page to remove deleted nodes
        status = compactPage(page_data, db_->page_size(), stats);

        // Clear HAS_GARBAGE flag
        page->btr_flags &= ~static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);

        stats.pages_vacuumed++;

        // Unpin with dirty flag
        bp->unpinPage(page_id, true, ctx);

        return Status::OK;
    }

    auto BTree::compactPage(uint8_t *page_data, uint32_t page_size, VacuumStats &stats) -> Status
    {
        auto *page = reinterpret_cast<SBBTreePage *>(page_data);
        auto *old_offsets = reinterpret_cast<uint16_t *>(page_data + sizeof(SBBTreePage));

        // Build list of non-deleted nodes
        struct NodeInfo
        {
            uint16_t old_offset;
            uint32_t size;
            std::vector<uint8_t> key;
            std::vector<uint64_t> tuple_ids;
        };

        std::vector<NodeInfo> live_nodes;
        live_nodes.reserve(page->btr_count);

        uint32_t bytes_before = 0;
        uint32_t bytes_after = 0;

        for (uint16_t i = 0; i < page->btr_count; i++)
        {
            auto *node = reinterpret_cast<SBBTreeNode *>(page_data + old_offsets[i]);

            // Calculate node size
            uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;
            if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0)
            {
                node_size += node->btn_tuple_count * sizeof(uint64_t);
            }
            else
            {
                node_size += sizeof(uint64_t); // child pointer
            }

            bytes_before += node_size;

            // Check if deleted
            if ((node->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::DELETED)) != 0)
            {
                stats.nodes_removed++;
                continue; // Skip deleted nodes
            }

            // Extract node data
            NodeInfo info;
            info.old_offset = old_offsets[i];
            info.size = node_size;

            // Extract key
            const uint8_t *key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
            info.key.assign(key_data, key_data + node->btn_key_len);

            // Extract tuple IDs or child pointer
            const uint8_t *tuple_data = key_data + node->btn_key_len;
            if ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0)
            {
                auto *tuple_ids = reinterpret_cast<const uint64_t *>(tuple_data);
                info.tuple_ids.assign(tuple_ids, tuple_ids + node->btn_tuple_count);
            }
            else
            {
                auto *child_ptr = reinterpret_cast<const uint64_t *>(tuple_data);
                info.tuple_ids.push_back(*child_ptr);
            }

            bytes_after += node_size;
            live_nodes.push_back(info);
        }

        stats.bytes_reclaimed += (bytes_before - bytes_after);

        if (live_nodes.size() == page->btr_count)
        {
            // No deleted nodes found - nothing to compact
            return Status::OK;
        }

        // Rebuild page with only live nodes
        // Reset high water mark
        uint16_t new_high_water = page_size;

        for (size_t i = 0; i < live_nodes.size(); i++)
        {
            const auto &node_info = live_nodes[i];

            // Allocate space from end
            new_high_water -= node_info.size;

            auto *new_node = reinterpret_cast<SBBTreeNode *>(page_data + new_high_water);

            // Copy node header
            auto *old_node = reinterpret_cast<SBBTreeNode *>(page_data + node_info.old_offset);
            new_node->btn_flags = old_node->btn_flags;
            new_node->btn_prefix_len = old_node->btn_prefix_len;
            new_node->btn_suffix_trunc = old_node->btn_suffix_trunc;
            new_node->btn_key_len = old_node->btn_key_len;
            new_node->btn_tuple_count = old_node->btn_tuple_count;
            new_node->btn_child_page = old_node->btn_child_page;
            new_node->btn_xmin = old_node->btn_xmin;
            new_node->btn_xmax = old_node->btn_xmax;

            // Copy key
            uint8_t *key_location = reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
            memcpy(key_location, node_info.key.data(), node_info.key.size());

            // Copy tuple IDs or child pointer
            uint8_t *tuple_location = key_location + node_info.key.size();
            memcpy(tuple_location, node_info.tuple_ids.data(),
                   node_info.tuple_ids.size() * sizeof(uint64_t));

            // Update offset array
            old_offsets[i] = new_high_water;
        }

        // Update page header
        page->btr_count = live_nodes.size();
        page->btr_high_water = new_high_water;

        // Recalculate free space
        uint32_t offsets_size = page->btr_count * sizeof(uint16_t);
        uint32_t used_space = sizeof(SBBTreePage) + offsets_size + (page_size - new_high_water);
        page->btr_free_space = page_size - used_space;

        return Status::OK;
    }

    bool BTree::shouldMergePages(const SBBTreePage *page1, const SBBTreePage *page2) const
    {
        if (!page1 || !page2)
        {
            return false;
        }

        // Only merge leaf pages with same parent
        if ((page1->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) == 0)
        {
            return false;
        }

        if ((page2->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) == 0)
        {
            return false;
        }

        if (page1->btr_parent_page != page2->btr_parent_page)
        {
            return false;
        }

        // Calculate total space needed
        uint32_t page1_used = db_->page_size() - page1->btr_free_space;
        uint32_t page2_used = db_->page_size() - page2->btr_free_space;
        uint32_t total_used = page1_used + page2_used;

        // Can merge if combined data fits in one page with some headroom
        uint32_t merge_threshold = (db_->page_size() * 3) / 4; // 75% full

        return total_used < merge_threshold;
    }

    auto BTree::mergePages(uint32_t left_page, uint32_t right_page, VacuumStats &stats,
                           ErrorContext *ctx) -> Status
    {
        // TODO: Implement page merging
        // This is complex as it requires:
        // 1. Moving all entries from right page to left page
        // 2. Updating parent to remove separator key
        // 3. Updating sibling pointers
        // 4. Freeing right page
        // 5. Potentially recursive merge if parent becomes underfull

        // For Alpha release, we'll skip merging and only do compaction
        return Status::NOT_IMPLEMENTED;
    }

} // namespace scratchbird::core
