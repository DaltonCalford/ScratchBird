#include "gtest/gtest.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/uuidv7.h"
#include <vector>

using namespace scratchbird::core;

class BTreePageTest : public ::testing::Test {
protected:
    void SetUp() override {
        page_size_ = 8192;
        page_data_.resize(page_size_);
        btree_page_ = std::make_unique<BTreePage>(page_data_.data(), page_size_);
    }

    std::vector<uint8_t> page_data_;
    uint32_t page_size_;
    std::unique_ptr<BTreePage> btree_page_;
};

TEST_F(BTreePageTest, Initialization) {
    UuidV7Bytes index_uuid = uuidv7();
    UuidV7Bytes table_uuid = uuidv7();
    uint16_t level = 0;
    uint16_t flags = BTR_FLAG_LEAF;

    btree_page_->initialize(index_uuid, table_uuid, level, flags);

    EXPECT_EQ(btree_page_->get_node_count(), 0);
    EXPECT_TRUE(btree_page_->is_leaf());
    EXPECT_TRUE(btree_page_->has_sufficient_space(100));
}

TEST_F(BTreePageTest, AddNode) {
    UuidV7Bytes index_uuid = uuidv7();
    UuidV7Bytes table_uuid = uuidv7();
    btree_page_->initialize(index_uuid, table_uuid, 0, BTR_FLAG_LEAF);

    std::vector<uint8_t> key = {'k', 'e', 'y', '1'};
    Tuple value = {nullptr, 0, 12345, 0, 0};

    Status s = btree_page_->add_node(key, value);
    ASSERT_TRUE(s.is_ok());

    EXPECT_EQ(btree_page_->get_node_count(), 1);

    SBBTreeNode* node = btree_page_->get_node(0);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->btn_key_len, key.size());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
