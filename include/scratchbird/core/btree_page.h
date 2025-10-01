#pragma once

#include "scratchbird/core/btree.h"
#include "scratchbird/core/status.h"

namespace scratchbird
{
    namespace core
    {

        class BTreePage
        {
        public:
            // Constructor wraps an existing page buffer
            explicit BTreePage(uint8_t *page_data, uint32_t page_size);

            // Initialize a new B-Tree page
            auto initialize(const ID &index_uuid, const ID &table_uuid, uint16_t level,
                            uint16_t flags) -> Status;

            // Node management
            Status add_node(const std::vector<uint8_t> &key, const Tuple &value,
                            ErrorContext *ctx = nullptr);
            SBBTreeNode *get_node(uint16_t node_index);
            void remove_node(uint16_t node_index);

            // Page properties
            bool has_sufficient_space(uint32_t required_space) const;
            uint16_t get_node_count() const;
            bool is_leaf() const;

            // Split logic
            static uint16_t find_split_point() ;

        private:
            uint8_t *page_data_;
            uint32_t page_size_;
            SBBTreePage *page_header_;
        };

    } // namespace core
} // namespace scratchbird
