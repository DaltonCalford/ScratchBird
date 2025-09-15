#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace core {

HeapPage::HeapPage(uint8_t* page_data, uint32_t page_size)
    : page_data_(page_data), page_size_(page_size), 
      toast_mgr_(nullptr), db_(nullptr) {}

HeapPage::HeapPage(uint8_t* page_data, uint32_t page_size,
                   ToastManager* toast_mgr, Database* db, const UuidV7Bytes& table_id)
    : page_data_(page_data), page_size_(page_size),
      toast_mgr_(toast_mgr), db_(db), table_id_(table_id) {}

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
        
        // Initialize special area only for new pages
        HeapPageSpecial* special = get_special();
        special->pd_flags = 0;
        special->pd_lower = sizeof(PageHeader);  // After header
        special->pd_upper = page_size_ - sizeof(HeapPageSpecial);  // Before special
        special->pd_special = page_size_ - sizeof(HeapPageSpecial);
        special->pd_prune_xid = 0;
    } else {
        // Page already initialized - validate and correct page size if needed
        if (hdr->page_size != page_size_) {
            // Correct the mismatch - the buffer size is authoritative
            hdr->page_size = page_size_;
        }
        
        // Validate special area is sane
        HeapPageSpecial* special = get_special();
        bool special_valid = (special->pd_lower >= sizeof(PageHeader) &&
                             special->pd_upper <= page_size_ - sizeof(HeapPageSpecial) &&
                             special->pd_lower <= special->pd_upper &&
                             special->pd_special == page_size_ - sizeof(HeapPageSpecial));
        
        if (!special_valid) {
            // Special area is corrupt - reinitialize it
            special->pd_flags = 0;
            special->pd_lower = sizeof(PageHeader);
            special->pd_upper = page_size_ - sizeof(HeapPageSpecial);
            special->pd_special = page_size_ - sizeof(HeapPageSpecial);
            special->pd_prune_xid = 0;
        }
    }
    
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
    
    // Check if we need to TOAST this tuple
    uint32_t actual_tuple_size = tuple_size;
    std::vector<uint8_t> toasted_data;
    const uint8_t* data_to_insert = tuple_data;
    
    if (toast_mgr_ && db_ && ToastManager::should_toast(tuple_size, page_size_)) {
        // Create a temporary buffer for the toasted tuple
        toasted_data.resize(sizeof(TupleHeader) + sizeof(ToastPointer));
        
        // Copy the tuple header
        TupleHeader* new_hdr = reinterpret_cast<TupleHeader*>(toasted_data.data());
        new_hdr->xmin = xmin;
        new_hdr->xmax = 0;
        new_hdr->flags = 0;
        new_hdr->null_bitmap_offset = 0;
        
        // TOAST the data portion (after TupleHeader)
        ToastPointer toast_ptr;
        // Use EXTERNAL strategy for automatic compression when available
        Status s = toast_mgr_->toast_value(tuple_data, tuple_size - sizeof(TupleHeader),
                                          ToastStrategy::EXTERNAL, xmin, &toast_ptr, ctx);
        if (s != Status::Ok) {
            return s;
        }
        
        // Copy the TOAST pointer after the header
        memcpy(toasted_data.data() + sizeof(TupleHeader), &toast_ptr, sizeof(ToastPointer));
        
        // Update pointers for insertion
        data_to_insert = toasted_data.data();
        actual_tuple_size = toasted_data.size();
    }
    
    // Check if we have space for the (possibly toasted) tuple
    if (!has_free_space(actual_tuple_size + sizeof(ItemPointer))) {
        SET_ERROR_CONTEXT(ctx, Status::PageFull, "No space for tuple");
        return Status::PageFull;
    }
    
    HeapPageSpecial* special = get_special();
    ItemPointer* items = get_item_array();
    
    // Find a free slot (reuse deleted slots if possible)
    uint16_t item_id = header()->item_count;
    for (uint16_t i = 0; i < header()->item_count; i++) {
        if (items[i].is_deleted() && items[i].length >= actual_tuple_size) {
            item_id = i;
            break;
        }
    }
    
    // Allocate space for tuple from upper area
    uint32_t tuple_offset = special->pd_upper - actual_tuple_size;
    
    // If not using TOAST, prepare the tuple header
    if (data_to_insert == tuple_data) {
        // Add tuple header for non-toasted data
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
    } else {
        // Copy the already prepared toasted tuple
        memcpy(page_data_ + tuple_offset, data_to_insert, actual_tuple_size);
    }
    
    // Update item pointer
    if (item_id == header()->item_count) {
        // New slot - advance lower boundary
        special->pd_lower += sizeof(ItemPointer);
        header()->item_count++;
    }
    
    items[item_id].offset = tuple_offset;
    items[item_id].length = actual_tuple_size;
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

Status HeapPage::get_tuple_detoasted(uint16_t item_id, std::vector<uint8_t>* buffer,
                                    uint64_t xmin, ErrorContext* ctx) {
    // First get the raw tuple data
    const uint8_t* raw_data;
    uint32_t raw_size;
    Status s = get_tuple(item_id, &raw_data, &raw_size, ctx);
    if (s != Status::Ok) {
        return s;
    }
    
    // Check if we have a TOAST pointer
    if (raw_size >= sizeof(TupleHeader) + sizeof(ToastPointer)) {
        const uint8_t* data_ptr = raw_data + sizeof(TupleHeader);
        
        // Check if this is a TOAST pointer
        if (is_toast_pointer(data_ptr)) {
            // We have a TOAST pointer, need to detoast
            if (!toast_mgr_ || !db_) {
                SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, 
                                 "TOAST manager not available for detoasting");
                return Status::InvalidArgument;
            }
            
            const ToastPointer* toast_ptr = reinterpret_cast<const ToastPointer*>(data_ptr);
            
            // Detoast the value
            std::vector<uint8_t> detoasted_data;
            s = toast_mgr_->detoast_value(toast_ptr, &detoasted_data, xmin, ctx);
            if (s != Status::Ok) {
                return s;
            }
            
            // Reconstruct the full tuple with detoasted data
            try {
                buffer->resize(sizeof(TupleHeader) + detoasted_data.size());
            } catch (const std::bad_alloc&) {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for detoasted tuple");
                return Status::OOM;
            }
            
            // Copy tuple header
            memcpy(buffer->data(), raw_data, sizeof(TupleHeader));
            
            // Copy detoasted data
            memcpy(buffer->data() + sizeof(TupleHeader), 
                   detoasted_data.data(), detoasted_data.size());
            
            return Status::Ok;
        }
    }
    
    // Not a TOAST pointer, just copy the raw data
    try {
        buffer->resize(raw_size);
    } catch (const std::bad_alloc&) {
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for tuple");
        return Status::OOM;
    }
    memcpy(buffer->data(), raw_data, raw_size);
    
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
    
    // Check if we need to delete TOAST data
    if (toast_mgr_ && db_) {
        // Get the tuple to check for TOAST pointers
        uint32_t offset = items[item_id].offset;
        uint32_t length = items[item_id].length;
        
        if (length >= sizeof(TupleHeader) + sizeof(ToastPointer)) {
            const uint8_t* data_ptr = page_data_ + offset + sizeof(TupleHeader);
            
            // Check if this is a TOAST pointer
            if (is_toast_pointer(data_ptr)) {
                const ToastPointer* toast_ptr = reinterpret_cast<const ToastPointer*>(data_ptr);
                
                // Delete the TOAST data
                Status s = toast_mgr_->delete_toast_value(toast_ptr->va_valueid, xmax, ctx);
                if (s != Status::Ok && s != Status::NotFound) {
                    return s;
                }
            }
        }
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
    
    // Sanity check - if pd_upper < pd_lower, page is corrupt
    if (special->pd_upper < special->pd_lower) {
        return false;
    }
    
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