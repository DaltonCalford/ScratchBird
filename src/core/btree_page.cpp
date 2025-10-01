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
            page_header_->btr_prefix_total = 0;
            page_header_->btr_suffix_total = 0;
                        page_header_->btr_compression = static_cast<uint8_t>(BTreeCompressionType::NONE);
            page_header_->btr_min_prefix_len = 0;
            page_header_->btr_xmin = 0; // TODO: Integrate with transaction manager
            page_header_->btr_xmax = 0;
            page_header_->btr_lsn = 0;
            page_header_->btr_high_water = sizeof(SBBTreePage);
            return Status::OK;
        }

        auto BTreePage::add_node(const std::vector<uint8_t> &key, const Tuple &value,
                                   ErrorContext *ctx) -> Status
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
            auto *new_node =
                reinterpret_cast<SBBTreeNode *>(page_data_ + page_header_->btr_high_water);

            // Populate the new node
            new_node->btn_flags = 0;
            new_node->btn_prefix_len = 0; // TODO: Implement prefix compression
            new_node->btn_suffix_trunc = 0;
            new_node->btn_key_len = key.size();
            new_node->btn_tuple_count = 1;
            new_node->btn_child_page = 0;
            new_node->btn_xmin = 0; // TODO: Integrate with transaction manager
            new_node->btn_xmax = 0;

            // Copy key and tuple ID
            uint8_t *key_location = reinterpret_cast<uint8_t *>(new_node) + sizeof(SBBTreeNode);
            memcpy(key_location, key.data(), key.size());
            auto *tid_location = reinterpret_cast<uint64_t *>(key_location + key.size());
            *tid_location = value.tid;

            // TODO: Insert the node into the sorted position in the offsets array.
            // For now, just append to the end.
            auto *offsets = reinterpret_cast<uint16_t *>(page_data_ + sizeof(SBBTreePage));
            offsets[page_header_->btr_count] = page_header_->btr_high_water;

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
            // TODO: Implement node removal
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

        auto BTreePage::find_split_point()  -> uint16_t
        {
            // TODO: Implement split point calculation
            return 0;
        }

    } // namespace scratchbird::core
