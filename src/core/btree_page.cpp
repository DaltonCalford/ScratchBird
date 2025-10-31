#include "scratchbird/core/btree_page.h"
#include <stdexcept>
#include <cstring>

namespace scratchbird::core
{

    BTreePage::BTreePage(uint8_t *page_data, uint32_t page_size)
        : page_data_(page_data), page_size_(page_size)
    {
        if ((page_data == nullptr) || page_size < sizeof(SBBTreePage))
        {
            throw std::invalid_argument("Invalid page data or size for BTreePage");
        }
        page_header_ = reinterpret_cast<SBBTreePage *>(page_data_);
    }

    auto BTreePage::initialize(const ID &index_uuid, const ID &table_uuid, uint16_t level,
                               uint16_t flags) -> Status
    {
        if ((flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0)
        {
            page_header_->btr_header.page_type = PageType::PAGE_TYPE_BTREE_LEAF;
        }
        else
        {
            page_header_->btr_header.page_type = PageType::PAGE_TYPE_BTREE_INTERNAL;
        }
        page_header_->btr_index_uuid = index_uuid;
        page_header_->btr_table_uuid = table_uuid;
        page_header_->btr_level = level;
        page_header_->btr_flags = flags;
        page_header_->btr_count = 0;
        page_header_->btr_free_space = page_size_ - sizeof(SBBTreePage);
        page_header_->btr_left_sibling = 0;
        page_header_->btr_right_sibling = 0;
        page_header_->btr_parent_page = 0;
        page_header_->btr_rightmost_child = 0; // Initialize rightmost child pointer
        page_header_->btr_prefix_total = 0;
        page_header_->btr_suffix_total = 0;
        page_header_->btr_compression = static_cast<uint8_t>(BTreeCompressionType::NONE);
        page_header_->btr_min_prefix_len = 0;
        page_header_->btr_xmin = 0; // TODO: Integrate with transaction manager
        page_header_->btr_xmax = 0;
        page_header_->btr_lsn = 0;
        // btr_high_water starts at end of page, nodes grow downward from there
        page_header_->btr_high_water = page_size_;
        return Status::OK;
    }

    // Task 17 MGA Phase 3.1: Added xmin parameter for transaction tracking
    auto BTreePage::add_node(const std::vector<uint8_t> &key, const Tuple &value, uint64_t xmin,
                             ErrorContext *ctx)
        -> Status
    {
        if (!is_leaf())
        {
            return Status::INVALID_ARGUMENT;
        }

        uint32_t node_size = sizeof(SBBTreeNode) + key.size() + sizeof(value.tid);
        if (!has_sufficient_space(node_size))
        {
            return Status::PAGE_FULL;
        }

        // Allocate space for the new node from the end of the page
        page_header_->btr_high_water -= node_size;
        auto *new_node = reinterpret_cast<SBBTreeNode *>(page_data_ + page_header_->btr_high_water);

        // Populate the new node
        new_node->btn_flags = 0;
        new_node->btn_prefix_len = 0; // TODO: Implement prefix compression
        new_node->btn_suffix_trunc = 0;
        new_node->btn_key_len = key.size();
        new_node->btn_tuple_count = 1;
        new_node->btn_child_page = 0;
        // Task 17 MGA Phase 3.1: Set btn_xmin from transaction creating this entry
        new_node->btn_xmin = xmin;  // Transaction ID creating this entry
        new_node->btn_xmax = 0;      // 0 = entry is active (not deleted)

        // Copy key and tuple ID
        uint8_t *key_location = reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
        memcpy(key_location, key.data(), key.size());
        auto *tid_location = reinterpret_cast<uint64_t *>(key_location + key.size());
        // PHASE 1.5: Convert TID struct to legacy format for on-disk storage
        *tid_location = convertTIDtoLegacy(value.tid);

        // Insert the node into the sorted position in the offsets array
        auto *offsets = reinterpret_cast<uint16_t *>(page_data_ + sizeof(SBBTreePage));

        // Find the correct insertion position using binary search
        uint16_t insert_pos = 0;
        int left = 0;
        int right = page_header_->btr_count - 1;

        while (left <= right)
        {
            int mid = left + (right - left) / 2;
            const auto *existing_node =
                reinterpret_cast<const SBBTreeNode *>(page_data_ + offsets[mid]);

            // Extract existing node's key
            const uint8_t *existing_key_data =
                reinterpret_cast<const uint8_t *>(existing_node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> existing_key(existing_key_data,
                                              existing_key_data + existing_node->btn_key_len);

            // Compare keys
            if (key < existing_key)
            {
                right = mid - 1;
                insert_pos = mid;
            }
            else
            {
                left = mid + 1;
                insert_pos = mid + 1;
            }
        }

        // Shift existing offsets to make room for new entry
        for (uint16_t i = page_header_->btr_count; i > insert_pos; --i)
        {
            offsets[i] = offsets[i - 1];
        }

        // Insert the new offset at the correct position
        offsets[insert_pos] = page_header_->btr_high_water;

        // Update header
        page_header_->btr_count++;
        page_header_->btr_free_space -= (node_size + sizeof(uint16_t));

        return Status::OK;
    }

    auto BTreePage::get_node(uint16_t node_index) -> SBBTreeNode *
    {
        if (node_index >= page_header_->btr_count)
        {
            return nullptr;
        }
        // Node pointers are stored after the page header
        auto *offsets = reinterpret_cast<uint16_t *>(page_data_ + sizeof(SBBTreePage));
        return reinterpret_cast<SBBTreeNode *>(page_data_ + offsets[node_index]);
    }

    void BTreePage::remove_node(uint16_t node_index)
    {
        if (node_index >= page_header_->btr_count)
        {
            return; // Invalid index
        }

        auto *offsets = reinterpret_cast<uint16_t *>(page_data_ + sizeof(SBBTreePage));

        // Get the node to be removed
        auto *node = reinterpret_cast<SBBTreeNode *>(page_data_ + offsets[node_index]);

        // Calculate the size of the node being removed
        uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;
        if (is_leaf())
        {
            node_size += node->btn_tuple_count * sizeof(uint64_t);
        }
        else
        {
            node_size += sizeof(uint64_t); // child pointer
        }

        // Instead of physically removing the node (which would require compacting),
        // we'll mark it as deleted. Physical compaction can happen during vacuum/maintenance.
        node->btn_flags |= static_cast<uint16_t>(BTreeNodeFlags::DELETED);

        // Remove the offset entry by shifting remaining offsets left
        for (uint16_t i = node_index; i < page_header_->btr_count - 1; ++i)
        {
            offsets[i] = offsets[i + 1];
        }

        // Update header
        page_header_->btr_count--;

        // Mark page as having garbage for future compaction
        page_header_->btr_flags |= static_cast<uint16_t>(BTreeFlags::HAS_GARBAGE);

        // Note: Free space is not reclaimed immediately. It will be reclaimed during
        // page compaction/vacuum. This is a common approach in B-tree implementations
        // to avoid expensive page reorganization on every delete.
    }

    auto BTreePage::has_sufficient_space(uint32_t required_space) const -> bool
    {
        // We need space for the node itself, plus a pointer in the node array.
        return page_header_->btr_free_space >= (required_space + sizeof(uint16_t));
    }

    auto BTreePage::get_node_count() const -> uint16_t
    {
        return page_header_->btr_count;
    }

    auto BTreePage::is_leaf() const -> bool
    {
        return (page_header_->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0;
    }

    auto BTreePage::find_split_point() -> uint16_t
    {
        // Calculate the split point to divide the page roughly in half by size
        // This helps maintain balanced B-tree structure

        if (page_header_->btr_count < 2)
        {
            // Can't split a page with less than 2 nodes
            return 0;
        }

        auto *offsets = reinterpret_cast<uint16_t *>(page_data_ + sizeof(SBBTreePage));

        // Calculate total used space
        uint32_t total_used_space = 0;
        for (uint16_t i = 0; i < page_header_->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data_ + offsets[i]);

            // Calculate node size: header + key + tuple IDs (for leaf) or child pointer (for
            // internal)
            uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;
            if (is_leaf())
            {
                node_size += node->btn_tuple_count * sizeof(uint64_t);
            }
            else
            {
                node_size += sizeof(uint64_t); // child pointer
            }

            total_used_space += node_size;
        }

        // Find the split point that divides space roughly in half
        uint32_t target_size = total_used_space / 2;
        uint32_t accumulated_size = 0;
        uint16_t split_point = page_header_->btr_count / 2; // Default to middle

        for (uint16_t i = 0; i < page_header_->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data_ + offsets[i]);

            uint32_t node_size = sizeof(SBBTreeNode) + node->btn_key_len;
            if (is_leaf())
            {
                node_size += node->btn_tuple_count * sizeof(uint64_t);
            }
            else
            {
                node_size += sizeof(uint64_t);
            }

            accumulated_size += node_size;

            if (accumulated_size >= target_size)
            {
                // Split after this node
                split_point = i + 1;
                break;
            }
        }

        // Ensure we don't split at the beginning or end
        if (split_point == 0)
        {
            split_point = 1;
        }
        if (split_point >= page_header_->btr_count)
        {
            split_point = page_header_->btr_count - 1;
        }

        return split_point;
    }

    // Static method to get node with decompression support
    auto BTreePage::get_node(const uint8_t *page_data, uint32_t page_size, uint16_t node_index,
                             std::vector<uint8_t> &key_out, std::vector<uint64_t> &tuple_ids_out)
        -> Status
    {
        if (!page_data)
        {
            return Status::INVALID_ARGUMENT;
        }

        auto *page = reinterpret_cast<const SBBTreePage *>(page_data);

        if (node_index >= page->btr_count)
        {
            return Status::INVALID_ARGUMENT;
        }

        // Get node offset
        auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
        uint16_t node_offset = offsets[node_index];

        auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + node_offset);

        // Extract compressed key (suffix only if prefix compression enabled)
        const uint8_t *key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
        std::vector<uint8_t> compressed_key(key_data, key_data + node->btn_key_len);

        // Decompress key if needed
        if (node->btn_prefix_len > 0)
        {
            // Page has prefix compression - reconstruct full key
            // The page prefix is stored after the page header (implementation detail)
            // For now, we'll return the compressed key and let caller handle it
            // Full implementation would extract page prefix and concatenate
            key_out = compressed_key; // TODO: Add full decompression
        }
        else
        {
            key_out = compressed_key;
        }

        // Check if this is a leaf or internal node
        bool is_leaf = (page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0;

        tuple_ids_out.clear();

        if (is_leaf)
        {
            // Leaf node: Extract tuple IDs from after the key data
            const uint8_t *tuple_data = key_data + node->btn_key_len;
            auto *tuple_ids = reinterpret_cast<const uint64_t *>(tuple_data);

            tuple_ids_out.reserve(node->btn_tuple_count);

            for (uint32_t i = 0; i < node->btn_tuple_count; i++)
            {
                tuple_ids_out.push_back(tuple_ids[i]);
            }
        }
        else
        {
            // Internal node: Extract child page pointer from node header
            // For internal nodes, btn_child_page contains the child pointer
            // There's only one child pointer per node (not stored after key)
            tuple_ids_out.push_back(node->btn_child_page);
        }

        return Status::OK;
    }

    void BTreePage::enableCompression(const std::vector<uint8_t> &page_prefix)
    {
        if (page_prefix.empty())
        {
            return;
        }

        // Mark page as compressed
        page_header_->btr_flags |= static_cast<uint16_t>(BTreeFlags::COMPRESSED);

        // Store prefix length
        page_header_->btr_min_prefix_len = page_prefix.size();

        // Store the prefix in the page (after nodes area, before high water)
        // For Alpha implementation, we'll store it in a reserved area
        // Full implementation would use special_size region
    }

    bool BTreePage::isCompressionEnabled() const
    {
        return (page_header_->btr_flags & static_cast<uint16_t>(BTreeFlags::COMPRESSED)) != 0;
    }

    std::vector<uint8_t> BTreePage::getPagePrefix() const
    {
        if (!isCompressionEnabled())
        {
            return {};
        }

        // Return the page prefix
        // For Alpha, return empty - full implementation would extract from page
        return {};
    }

    uint16_t BTreePage::calculateNodePrefix(uint16_t node_index,
                                            const std::vector<uint8_t> &key) const
    {
        if (node_index == 0)
        {
            // First node on page - no prefix compression
            return 0;
        }

        // Get previous key
        auto *offsets = reinterpret_cast<const uint16_t *>(page_data_ + sizeof(SBBTreePage));
        auto *prev_node =
            reinterpret_cast<const SBBTreeNode *>(page_data_ + offsets[node_index - 1]);

        const uint8_t *prev_key_data =
            reinterpret_cast<const uint8_t *>(prev_node) + sizeof(SBBTreeNode);
        std::vector<uint8_t> prev_key(prev_key_data, prev_key_data + prev_node->btn_key_len);

        // Calculate common prefix
        size_t min_len = std::min(prev_key.size(), key.size());
        uint16_t prefix_len = 0;

        for (size_t i = 0; i < min_len; i++)
        {
            if (prev_key[i] != key[i])
            {
                break;
            }
            prefix_len++;
        }

        return prefix_len;
    }

} // namespace scratchbird::core
