/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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
            // Task 17 MGA Phase 3.1: Added xmin parameter for transaction tracking
            Status add_node(const std::vector<uint8_t> &key, const Tuple &value,
                            uint64_t xmin,  // Transaction ID creating this entry
                            ErrorContext *ctx = nullptr);
            SBBTreeNode *get_node(uint16_t node_index);
            void remove_node(uint16_t node_index);

            // Page properties
            bool has_sufficient_space(uint32_t required_space) const;
            uint16_t get_node_count() const;
            bool is_leaf() const;

            // Split logic
            uint16_t find_split_point();

            // Compression support
            static Status get_node(const uint8_t *page_data, uint32_t page_size,
                                   uint16_t node_index, std::vector<uint8_t> &key_out,
                                   std::vector<TID> &tuple_ids_out);

            void enableCompression(const std::vector<uint8_t> &page_prefix);
            bool isCompressionEnabled() const;
            std::vector<uint8_t> getPagePrefix() const;

        private:
            uint8_t *page_data_;
            uint32_t page_size_;
            SBBTreePage *page_header_;

            // Helper to calculate prefix for key at position
            uint16_t calculateNodePrefix(uint16_t node_index,
                                         const std::vector<uint8_t> &key) const;
        };

    } // namespace core
} // namespace scratchbird
