#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace core {

HeapPage::HeapPage(uint8_t* page_data, uint32_t page_size)
    : page_data_(page_data), page_size_(page_size) {}

Status HeapPage::initialize(uint32_t page_id, ErrorContext* ctx) {
    // Validate page size
    if (!is_valid_alpha_page_size(page_size_)) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Invalid page size for heap page");
        return Status::InvalidArgument;
    }
    
    // Initialize page header
    PageHeader* hdr = header();
    
    // Only initialize if this is a new page
    if (hdr->magic != kMagicSBRD) {
        memset(page_data_, 0, page_size_);
        
        hdr->magic = kMagicSBRD;
        hdr->version = 1;
        hdr->page_type = PAGE_TYPE_HEAP;
        hdr->page_size = page_size_;
        hdr->page_id = page_id;
        hdr->item_count = 0;
        hdr->free_space = 0;  // Will be calculated
        hdr->free_offset = sizeof(PageHeader);
        hdr->special_size = sizeof(HeapPageSpecial);
    } else {
        // Page already initialized - validate and correct page size if needed
        if (hdr->page_size != page_size_) {
            // Correct the mismatch - the buffer size is authoritative
            hdr->page_size = page_size_;
        }
    }
    
    // Initialize special area
    HeapPageSpecial* special = get_special();
    special->pd_flags = 0;
    special->pd_lower = sizeof(PageHeader);  // After header
    special->pd_upper = page_size_ - sizeof(HeapPageSpecial);  // Before special
    special->pd_special = page_size_ - sizeof(HeapPageSpecial);
    special->pd_prune_xid = 0;
    
    update_header_stats();
    
    return Status::Ok;
}

Status HeapPage::insert_tuple(const uint8_t* tuple_data, uint32_t tuple_size,
                             uint64_t xmin, uint16_t* item_id_out,
                             ErrorContext* ctx) {
    // Validate input: tuple_size must include space for TupleHeader
    if (tuple_size < sizeof(TupleHeader)) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                         "Tuple size must be at least sizeof(TupleHeader)");
        return Status::InvalidArgument;
    }
    
    // Check if we have space
    if (!has_free_space(tuple_size + sizeof(ItemPointer))) {
        SET_ERROR_CONTEXT(ctx, Status::PageFull, "No space for tuple");
        return Status::PageFull;
    }
    
    HeapPageSpecial* special = get_special();
    ItemPointer* items = get_item_array();
    
    // Find a free slot (reuse deleted slots if possible)
    uint16_t item_id = header()->item_count;
    for (uint16_t i = 0; i < header()->item_count; i++) {
        if (items[i].is_deleted() && items[i].length >= tuple_size) {
            item_id = i;
            break;
        }
    }
    
    // Allocate space for tuple from upper area
    uint32_t tuple_offset = special->pd_upper - tuple_size;
    
    // Add tuple header
    TupleHeader tuple_hdr;
    tuple_hdr.xmin = xmin;
    tuple_hdr.xmax = 0;
    tuple_hdr.flags = 0;
    tuple_hdr.null_bitmap_offset = 0;
    
    // Copy tuple header and data
    memcpy(page_data_ + tuple_offset, &tuple_hdr, sizeof(TupleHeader));
    
    // Calculate actual data size after header
    uint32_t data_size = tuple_size - sizeof(TupleHeader);
    if (data_size > 0) {
        memcpy(page_data_ + tuple_offset + sizeof(TupleHeader), 
               tuple_data, data_size);
    }
    
    // Update item pointer
    if (item_id == header()->item_count) {
        // New slot - advance lower boundary
        special->pd_lower += sizeof(ItemPointer);
        header()->item_count++;
    }
    
    items[item_id].offset = tuple_offset;
    items[item_id].length = tuple_size;
    items[item_id].set_deleted(false);
    
    // Update upper boundary
    special->pd_upper = tuple_offset;
    
    update_header_stats();
    
    if (item_id_out) {
        *item_id_out = item_id;
    }
    
    return Status::Ok;
}

Status HeapPage::get_tuple(uint16_t item_id, const uint8_t** data_out,
                          uint32_t* size_out, ErrorContext* ctx) {
    if (item_id >= header()->item_count) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid item ID");
        return Status::InvalidArgument;
    }
    
    ItemPointer* items = get_item_array();
    
    if (items[item_id].is_deleted()) {
        SET_ERROR_CONTEXT(ctx, Status::NotFound, "Tuple is deleted");
        return Status::NotFound;
    }
    
    // Validate item pointer is within page bounds
    uint32_t offset = items[item_id].offset;
    uint32_t length = items[item_id].length;
    
    if (offset >= page_size_ || offset + length > page_size_) {
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Item pointer extends beyond page boundary");
        return Status::PageCorrupt;
    }
    
    if (data_out) {
        *data_out = page_data_ + offset;
    }
    if (size_out) {
        *size_out = length;
    }
    
    return Status::Ok;
}

Status HeapPage::delete_tuple(uint16_t item_id, uint64_t xmax, ErrorContext* ctx) {
    if (item_id >= header()->item_count) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid item ID");
        return Status::InvalidArgument;
    }
    
    ItemPointer* items = get_item_array();
    
    if (items[item_id].is_deleted()) {
        SET_ERROR_CONTEXT(ctx, Status::NotFound, "Tuple already deleted");
        return Status::NotFound;
    }
    
    // Mark item as deleted
    items[item_id].set_deleted(true);
    
    // Update tuple header with xmax
    TupleHeader* tuple_hdr = reinterpret_cast<TupleHeader*>(
        page_data_ + items[item_id].offset);
    tuple_hdr->xmax = xmax;
    tuple_hdr->flags |= TupleHeader::FLAG_DELETED;
    
    update_header_stats();
    
    return Status::Ok;
}

bool HeapPage::has_free_space(uint32_t tuple_size) const {
    const HeapPageSpecial* special = get_special();
    uint32_t free_space = special->pd_upper - special->pd_lower;
    
    // Need space for tuple and potentially a new item pointer
    uint32_t needed = tuple_size;
    
    // Check if we need a new item slot
    bool found_deleted_slot = false;
    const ItemPointer* items = get_item_array();
    for (uint16_t i = 0; i < header()->item_count; i++) {
        if (items[i].is_deleted() && items[i].length >= tuple_size) {
            found_deleted_slot = true;
            break;
        }
    }
    
    if (!found_deleted_slot) {
        needed += sizeof(ItemPointer);
    }
    
    return free_space >= needed;
}

uint16_t HeapPage::get_item_count() const {
    return header()->item_count;
}

uint32_t HeapPage::get_free_space() const {
    const HeapPageSpecial* special = get_special();
    return special->pd_upper - special->pd_lower;
}

Status HeapPage::validate(ErrorContext* ctx) const {
    const PageHeader* hdr = header();
    
    // Validate header
    if (hdr->magic != kMagicSBRD) {
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid magic");
        return Status::PageCorrupt;
    }
    
    if (hdr->page_type != PAGE_TYPE_HEAP) {
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Not a heap page");
        return Status::PageCorrupt;
    }
    
    // Validate special area
    const HeapPageSpecial* special = get_special();
    
    if (special->pd_lower < sizeof(PageHeader) ||
        special->pd_lower > special->pd_upper ||
        special->pd_upper > special->pd_special ||
        special->pd_special != page_size_ - sizeof(HeapPageSpecial)) {
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid page boundaries");
        return Status::PageCorrupt;
    }
    
    // Validate item pointers
    const ItemPointer* items = get_item_array();
    for (uint16_t i = 0; i < hdr->item_count; i++) {
        if (!items[i].is_deleted()) {
            if (items[i].offset < special->pd_upper ||
                items[i].offset + items[i].length > special->pd_special) {
                SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid item pointer");
                return Status::PageCorrupt;
            }
        }
    }
    
    return Status::Ok;
}

void HeapPage::update_header_stats() {
    PageHeader* hdr = header();
    HeapPageSpecial* special = get_special();
    
    hdr->free_space = special->pd_upper - special->pd_lower;
    hdr->free_offset = special->pd_lower;
}

} // namespace core
} // namespace scratchbird