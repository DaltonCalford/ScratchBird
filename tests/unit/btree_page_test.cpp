/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "gtest/gtest.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/uuidv7.h"
#include <vector>

using namespace scratchbird::core;

class BTreePageTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        page_size_ = 8192;
        page_data_.resize(page_size_);
        btree_page_ = std::make_unique<BTreePage>(page_data_.data(), page_size_);
    }

    std::vector<uint8_t> page_data_;
    uint32_t page_size_;
    std::unique_ptr<BTreePage> btree_page_;
};

TEST_F(BTreePageTest, Initialization)
{
    ID index_uuid = generateUuidV7();
    ID table_uuid = generateUuidV7();
    uint16_t level = 0;
    uint16_t flags = static_cast<uint16_t>(BTreeFlags::LEAF);

    btree_page_->initialize(index_uuid, table_uuid, level, flags);

    EXPECT_EQ(btree_page_->get_node_count(), 0);
    EXPECT_TRUE(btree_page_->is_leaf());
    EXPECT_TRUE(btree_page_->has_sufficient_space(100));
}

TEST_F(BTreePageTest, AddNode)
{
    ID index_uuid = generateUuidV7();
    ID table_uuid = generateUuidV7();
    btree_page_->initialize(index_uuid, table_uuid, 0, static_cast<uint16_t>(BTreeFlags::LEAF));

    std::vector<uint8_t> key = {'k', 'e', 'y', '1'};
    Tuple value = {nullptr, 0, TID{12345, 0}};

    // MGA Phase 3.1: add_node now requires xmin parameter for transaction tracking
    uint64_t xmin = 100; // Test transaction ID
    Status s = btree_page_->add_node(key, value, xmin);
    ASSERT_EQ(s, Status::OK);

    EXPECT_EQ(btree_page_->get_node_count(), 1);

    SBBTreeNode *node = btree_page_->get_node(0);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->btn_key_len, key.size());
}

TEST_F(BTreePageTest, AppendSortedLeafNodeKeepsPageOrderedWithoutRebuild)
{
    ID index_uuid = generateUuidV7();
    ID table_uuid = generateUuidV7();
    btree_page_->initialize(index_uuid, table_uuid, 0, static_cast<uint16_t>(BTreeFlags::LEAF));

    const std::vector<std::vector<uint8_t>> keys = {
        {'p','r','e','f','i','x','_','0'},
        {'p','r','e','f','i','x','_','1'},
        {'p','r','e','f','i','x','_','2'},
    };

    std::vector<uint8_t> prev_key;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        Tuple value{nullptr, 0, TID{static_cast<uint32_t>(100 + i), 0}};
        ASSERT_EQ(btree_page_->append_sorted_leaf_node(keys[i], value, 200 + i, prev_key),
                  Status::OK);
        prev_key = keys[i];
    }

    ASSERT_EQ(btree_page_->get_node_count(), 3);

    for (uint16_t i = 0; i < 3; ++i)
    {
        std::vector<uint8_t> key_out;
        std::vector<TID> tids_out;
        ASSERT_EQ(BTreePage::get_node(page_data_.data(), page_size_, i, key_out, tids_out),
                  Status::OK);
        EXPECT_EQ(key_out, keys[i]);
        ASSERT_EQ(tids_out.size(), 1u);
        EXPECT_EQ(getPageNumber(tids_out[0].gpid), static_cast<uint64_t>(100 + i));
    }

    auto *first = btree_page_->get_node(0);
    auto *second = btree_page_->get_node(1);
    auto *third = btree_page_->get_node(2);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(third, nullptr);
    EXPECT_NE(first->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE), 0);
    EXPECT_EQ(first->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE), 0);
    EXPECT_GT(second->btn_prefix_len, 0);
    EXPECT_EQ(third->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::FIRST_ON_PAGE), 0);
    EXPECT_NE(third->btn_flags & static_cast<uint16_t>(BTreeNodeFlags::LAST_ON_PAGE), 0);
}
