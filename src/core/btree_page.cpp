/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/logger.h"
#include <stdexcept>
#include <cstring>

namespace scratchbird::core
{
    namespace
    {
        uint16_t calculate_prefix_length_bytes(const std::vector<uint8_t> &key1,
                                               const std::vector<uint8_t> &key2)
        {
            const size_t min_len = std::min(key1.size(), key2.size());
            uint16_t prefix_len = 0;
            while (prefix_len < min_len && key1[prefix_len] == key2[prefix_len])
            {
                ++prefix_len;
            }
            return prefix_len;
        }

        bool should_prefix_compress(uint16_t prefix_len, size_t key_len)
        {
            return prefix_len >= 4 && key_len >= 8;
        }

        std::vector<uint8_t> decompress_key_bytes(const std::vector<uint8_t> &prev_key,
                                                  const uint8_t *compressed_key_data,
                                                  uint16_t compressed_key_len,
                                                  uint16_t prefix_len)
        {
            if (prefix_len == 0 || prev_key.empty())
            {
                return std::vector<uint8_t>(compressed_key_data,
                                            compressed_key_data + compressed_key_len);
            }

            if (prefix_len > prev_key.size())
            {
                return std::vector<uint8_t>(compressed_key_data,
                                            compressed_key_data + compressed_key_len);
            }

            std::vector<uint8_t> full_key;
            full_key.reserve(prefix_len + compressed_key_len);
            full_key.insert(full_key.end(), prev_key.begin(), prev_key.begin() + prefix_len);
            full_key.insert(full_key.end(), compressed_key_data,
                            compressed_key_data + compressed_key_len);
            return full_key;
        }

        int compare_key_bytes(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
        {
            if (a == b)
            {
                return 0;
            }
            return (a < b) ? -1 : 1;
        }
    }


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
        // Default to adaptive prefix compression for new pages
        page_header_->btr_compression = static_cast<uint8_t>(BTreeCompressionType::ADAPTIVE);
        page_header_->btr_min_prefix_len = 0;
        page_header_->btr_xmin = 0; // Phase 3 Enhancement: Set from ConnectionContext::getCurrentTransactionId()
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

        struct LeafEntry
        {
            std::vector<uint8_t> key;
            std::vector<OnDiskTID> tids;
            uint16_t flags = 0;
            uint64_t xmin = 0;
            uint64_t xmax = 0;
        };

        std::vector<LeafEntry> entries;
        entries.reserve(page_header_->btr_count + 1);

        auto *offsets = reinterpret_cast<const uint16_t *>(page_data_ + sizeof(SBBTreePage));
        std::vector<uint8_t> prev_key;

        for (uint16_t i = 0; i < page_header_->btr_count; ++i)
        {
            const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data_ + offsets[i]);
            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

            std::vector<uint8_t> full_key = decompress_key_bytes(prev_key, node_key_data,
                                                                 node->btn_key_len,
                                                                 node->btn_prefix_len);
            prev_key = full_key;

            std::vector<OnDiskTID> tids;
            tids.reserve(node->btn_tuple_count);
            const auto *tuple_ids_ptr =
                reinterpret_cast<const OnDiskTID *>(node_key_data + node->btn_key_len);
            for (uint32_t j = 0; j < node->btn_tuple_count; ++j)
            {
                tids.push_back(tuple_ids_ptr[j]);
            }

            LeafEntry entry;
            entry.key = std::move(full_key);
            entry.tids = std::move(tids);
            entry.flags = node->btn_flags;
            entry.xmin = node->btn_xmin;
            entry.xmax = node->btn_xmax;
            entries.push_back(std::move(entry));
        }

        LeafEntry new_entry;
        new_entry.key = key;
        new_entry.tids = {toOnDiskTID(value.tid)};
        new_entry.flags = 0;
        new_entry.xmin = xmin;
        new_entry.xmax = 0;

        size_t insert_pos = entries.size();
        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (compare_key_bytes(new_entry.key, entries[i].key) < 0)
            {
                insert_pos = i;
                break;
            }
        }
        entries.insert(entries.begin() + static_cast<std::ptrdiff_t>(insert_pos),
                       std::move(new_entry));

        // Rebuild page with optional prefix compression
        std::vector<uint8_t> temp(page_size_, 0);
        auto new_header = *page_header_;
        new_header.btr_count = 0;
        new_header.btr_high_water = page_size_;
        new_header.btr_free_space = page_size_ - sizeof(SBBTreePage);
        new_header.btr_prefix_total = 0;
        new_header.btr_suffix_total = 0;
        new_header.btr_min_prefix_len = 0;

        std::memcpy(temp.data(), &new_header, sizeof(SBBTreePage));
        auto *new_offsets = reinterpret_cast<uint16_t *>(temp.data() + sizeof(SBBTreePage));

        std::vector<uint8_t> prev_full_key;
        uint16_t min_prefix = 0;

        for (const auto &entry : entries)
        {
            uint16_t prefix_len = 0;
            std::vector<uint8_t> stored_key = entry.key;
            const auto compression = static_cast<BTreeCompressionType>(new_header.btr_compression);
            if (compression == BTreeCompressionType::PREFIX ||
                compression == BTreeCompressionType::BOTH ||
                compression == BTreeCompressionType::ADAPTIVE)
            {
                prefix_len = calculate_prefix_length_bytes(prev_full_key, entry.key);
                if (!should_prefix_compress(prefix_len, entry.key.size()))
                {
                    prefix_len = 0;
                }
            }

            if (prefix_len > 0)
            {
                stored_key.assign(entry.key.begin() + prefix_len, entry.key.end());
                new_header.btr_prefix_total += prefix_len;
                if (min_prefix == 0 || prefix_len < min_prefix)
                {
                    min_prefix = prefix_len;
                }
            }

            const uint32_t node_size =
                sizeof(SBBTreeNode) + stored_key.size() +
                static_cast<uint32_t>(entry.tids.size() * sizeof(OnDiskTID));

            if (new_header.btr_free_space < (node_size + sizeof(uint16_t)))
            {
                return Status::PAGE_FULL;
            }

            new_header.btr_high_water -= node_size;
            auto *node = reinterpret_cast<SBBTreeNode *>(temp.data() + new_header.btr_high_water);
            node->btn_flags = entry.flags;
            node->btn_prefix_len = prefix_len;
            node->btn_suffix_trunc = 0;
            node->btn_key_len = static_cast<uint16_t>(stored_key.size());
            node->btn_tuple_count = static_cast<uint32_t>(entry.tids.size());
            node->btn_child_page = 0;
            node->btn_xmin = entry.xmin;
            node->btn_xmax = entry.xmax;

            uint8_t *key_location = reinterpret_cast<uint8_t *>(node) + sizeof(SBBTreeNode);
            if (!stored_key.empty())
            {
                std::memcpy(key_location, stored_key.data(), stored_key.size());
            }
            auto *tid_location =
                reinterpret_cast<OnDiskTID *>(key_location + stored_key.size());
            for (size_t t = 0; t < entry.tids.size(); ++t)
            {
                tid_location[t] = entry.tids[t];
            }

            new_offsets[new_header.btr_count] =
                static_cast<uint16_t>(new_header.btr_high_water);
            new_header.btr_count++;
            new_header.btr_free_space -= (node_size + sizeof(uint16_t));

            prev_full_key = entry.key;
        }

        new_header.btr_min_prefix_len = min_prefix;
        if (new_header.btr_prefix_total > 0)
        {
            new_header.btr_flags |= static_cast<uint16_t>(BTreeFlags::COMPRESSED);
        }
        else
        {
            new_header.btr_flags &= ~static_cast<uint16_t>(BTreeFlags::COMPRESSED);
        }

        std::memcpy(temp.data(), &new_header, sizeof(SBBTreePage));
        std::memcpy(page_data_, temp.data(), page_size_);

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
            node_size += node->btn_tuple_count * sizeof(OnDiskTID);
        }
        else
        {
            node_size += sizeof(uint64_t); // child pointer
        }

        // Instead of physically removing the node (which would require compacting),
        // we'll mark it as deleted. Physical compaction can happen during GC/maintenance.
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
        // page compaction/GC. This is a common approach in B-tree implementations
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
                node_size += node->btn_tuple_count * sizeof(OnDiskTID);
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
                node_size += node->btn_tuple_count * sizeof(OnDiskTID);
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
                             std::vector<uint8_t> &key_out, std::vector<TID> &tuple_ids_out)
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

        if (node_offset < sizeof(SBBTreePage) || node_offset >= page_size)
        {
            LOG_WARNING(GENERAL,
                        "BTreePage::get_node invalid offset: page_id=%u node_index=%u btr_count=%u offset=%u page_size=%u",
                        page->btr_header.page_id, node_index, page->btr_count,
                        static_cast<uint32_t>(node_offset), page_size);
            return Status::PAGE_CORRUPT;
        }

        auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + node_offset);

        if (node_offset + sizeof(SBBTreeNode) > page_size)
        {
            LOG_WARNING(GENERAL,
                        "BTreePage::get_node header overflow: page_id=%u node_index=%u btr_count=%u offset=%u page_size=%u",
                        page->btr_header.page_id, node_index, page->btr_count,
                        static_cast<uint32_t>(node_offset), page_size);
            return Status::PAGE_CORRUPT;
        }

        // Extract compressed key (suffix only if prefix compression enabled)
        const uint8_t *key_data = reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);

        // Bounds check for key data
        if (node_offset + sizeof(SBBTreeNode) + node->btn_key_len > page_size)
        {
            LOG_WARNING(GENERAL,
                        "BTreePage::get_node key overflow: page_id=%u node_index=%u btr_count=%u offset=%u key_len=%u page_size=%u",
                        page->btr_header.page_id, node_index, page->btr_count,
                        static_cast<uint32_t>(node_offset), node->btn_key_len, page_size);
            return Status::PAGE_CORRUPT;
        }

        std::vector<uint8_t> prev_key;
        key_out.clear();
        for (uint16_t i = 0; i <= node_index; ++i)
        {
            uint16_t cur_offset = offsets[i];
            auto *cur_node = reinterpret_cast<const SBBTreeNode *>(page_data + cur_offset);
            const uint8_t *cur_key_data =
                reinterpret_cast<const uint8_t *>(cur_node) + sizeof(SBBTreeNode);

            std::vector<uint8_t> full_key = decompress_key_bytes(prev_key, cur_key_data,
                                                                 cur_node->btn_key_len,
                                                                 cur_node->btn_prefix_len);
            if (i == node_index)
            {
                key_out = std::move(full_key);
                break;
            }
            prev_key = std::move(full_key);
        }

        // Check if this is a leaf or internal node
        bool is_leaf = (page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) != 0;

        tuple_ids_out.clear();

        if (is_leaf)
        {
            // Leaf node: Extract tuple IDs from after the key data
            const uint8_t *tuple_data = key_data + node->btn_key_len;
            auto *tuple_ids = reinterpret_cast<const OnDiskTID *>(tuple_data);

            tuple_ids_out.reserve(node->btn_tuple_count);

            uint64_t tuple_bytes = static_cast<uint64_t>(node->btn_tuple_count) * sizeof(OnDiskTID);
            if (node_offset + sizeof(SBBTreeNode) + node->btn_key_len + tuple_bytes > page_size)
            {
                LOG_WARNING(GENERAL,
                            "BTreePage::get_node tuple overflow: page_id=%u node_index=%u btr_count=%u offset=%u key_len=%u tuple_count=%u page_size=%u",
                            page->btr_header.page_id, node_index, page->btr_count,
                            static_cast<uint32_t>(node_offset), node->btn_key_len,
                            node->btn_tuple_count, page_size);
                return Status::PAGE_CORRUPT;
            }

            for (uint32_t i = 0; i < node->btn_tuple_count; i++)
            {
                tuple_ids_out.push_back(fromOnDiskTID(tuple_ids[i]));
            }
        }
        else
        {
            // Internal node: Extract child page pointer from node header
            // For internal nodes, btn_child_page contains the child pointer
            // There's only one child pointer per node (not stored after key)
            tuple_ids_out.push_back(TID{node->btn_child_page, 0});
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

        // Reconstruct previous full key by scanning in order
        auto *offsets = reinterpret_cast<const uint16_t *>(page_data_ + sizeof(SBBTreePage));
        std::vector<uint8_t> prev_key;
        for (uint16_t i = 0; i < node_index; ++i)
        {
            auto *node = reinterpret_cast<const SBBTreeNode *>(page_data_ + offsets[i]);
            const uint8_t *node_key_data =
                reinterpret_cast<const uint8_t *>(node) + sizeof(SBBTreeNode);
            std::vector<uint8_t> full_key = decompress_key_bytes(
                prev_key, node_key_data, node->btn_key_len, node->btn_prefix_len);
            prev_key = std::move(full_key);
        }

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
