#include "scratchbird/core/btree_page.h"
#include <stdexcept>
#include <cstring>

namespace scratchbird {
namespace core {

BTreePage::BTreePage(uint8_t* page_data, uint32_t page_size)
    : page_data_(page_data), page_size_(page_size) {
    if (!page_data || page_size < sizeof(SBBTreePage)) {
        throw std::invalid_argument("Invalid page data or size for BTreePage");
    }
    page_header_ = reinterpret_cast<SBBTreePage*>(page_data_);
}

void BTreePage::initialize(const UuidV7Bytes& index_uuid, const UuidV7Bytes& table_uuid, uint16_t level, uint16_t flags) {
    if (flags & BTR_FLAG_LEAF) {
        page_header_->btr_header.page_type = PageType::PAGE_TYPE_BTREE_LEAF;
    } else {
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
    page_header_->btr_compression = BTR_COMPRESS_NONE;
    page_header_->btr_min_prefix_len = 0;
    page_header_->btr_xmin = 0; // TODO: Integrate with transaction manager
    page_header_->btr_xmax = 0;
    page_header_->btr_lsn = 0;
    page_header_->btr_high_water = sizeof(SBBTreePage);
}

Status BTreePage::add_node(const std::vector<uint8_t>& key, const Tuple& value, ErrorContext* ctx) {
    if (!is_leaf()) {
        return Status::InvalidArgument;
    }

    uint32_t node_size = sizeof(SBBTreeNode) + key.size() + sizeof(value.tid);
    if (!has_sufficient_space(node_size)) {
        return Status::PageFull;
    }

    // Allocate space for the new node from the end of the page
    page_header_->btr_high_water -= node_size;
    SBBTreeNode* new_node = reinterpret_cast<SBBTreeNode*>(page_data_ + page_header_->btr_high_water);

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
    uint8_t* key_location = reinterpret_cast<uint8_t*>(new_node) + sizeof(SBBTreeNode);
    memcpy(key_location, key.data(), key.size());
    uint64_t* tid_location = reinterpret_cast<uint64_t*>(key_location + key.size());
    *tid_location = value.tid;

    // TODO: Insert the node into the sorted position in the offsets array.
    // For now, just append to the end.
    uint16_t* offsets = reinterpret_cast<uint16_t*>(page_data_ + sizeof(SBBTreePage));
    offsets[page_header_->btr_count] = page_header_->btr_high_water;

    // Update header
    page_header_->btr_count++;
    page_header_->btr_free_space -= (node_size + sizeof(uint16_t));

    return Status::Ok;
}

SBBTreeNode* BTreePage::get_node(uint16_t node_index) {
    if (node_index >= page_header_->btr_count) {
        return nullptr;
    }
    // Node pointers are stored after the page header
    uint16_t* offsets = reinterpret_cast<uint16_t*>(page_data_ + sizeof(SBBTreePage));
    return reinterpret_cast<SBBTreeNode*>(page_data_ + offsets[node_index]);
}

void BTreePage::remove_node(uint16_t node_index) {
    // TODO: Implement node removal
}

bool BTreePage::has_sufficient_space(uint32_t required_space) const {
    // We need space for the node itself, plus a pointer in the node array.
    return page_header_->btr_free_space >= (required_space + sizeof(uint16_t));
}

uint16_t BTreePage::get_node_count() const {
    return page_header_->btr_count;
}

bool BTreePage::is_leaf() const {
    return page_header_->btr_flags & BTR_FLAG_LEAF;
}

uint16_t BTreePage::find_split_point() const {
    // TODO: Implement split point calculation
    return 0;
}

} // namespace core
} // namespace scratchbird