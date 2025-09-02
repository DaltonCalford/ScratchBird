#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include <cstdint>
#include <vector>

namespace scratchbird {
namespace core {

// Forward declarations
struct ErrorContext;

// Heap page item pointer (line pointer)
// Points to actual tuple data within the page
#pragma pack(push, 1)
struct ItemPointer {
    uint16_t offset;     // Offset from start of page
    uint16_t length : 15; // Length of tuple (max ~32KB)
    uint16_t flags : 1;   // 0 = normal, 1 = deleted
    
    static constexpr uint16_t FLAG_DELETED = 0x8000;
    
    bool is_deleted() const { return flags & 1; }
    void set_deleted(bool deleted) { flags = deleted ? 1 : 0; }
};
#pragma pack(pop)

// Tuple header - metadata for each tuple
#pragma pack(push, 1)
struct TupleHeader {
    uint64_t xmin;       // Transaction ID that inserted this tuple
    uint64_t xmax;       // Transaction ID that deleted this tuple (or 0)
    uint16_t flags;      // Various flags
    uint16_t null_bitmap_offset; // Offset to null bitmap (0 if no nulls)
    
    static constexpr uint16_t FLAG_HAS_NULLS = 0x0001;
    static constexpr uint16_t FLAG_DELETED = 0x0002;
    
    bool has_nulls() const { return flags & FLAG_HAS_NULLS; }
    bool is_deleted() const { return flags & FLAG_DELETED; }
};
#pragma pack(pop)

// Special area at the end of heap pages
#pragma pack(push, 1)
struct HeapPageSpecial {
    uint16_t pd_flags;      // Page flags
    uint16_t pd_lower;      // Offset to start of free space
    uint16_t pd_upper;      // Offset to end of free space
    uint16_t pd_special;    // Offset to start of special area
    uint64_t pd_prune_xid;  // Oldest XID for pruning
};
#pragma pack(pop)

// Heap page class - manages tuple storage within a page
class HeapPage {
public:
    // Constructor wraps an existing page buffer
    explicit HeapPage(uint8_t* page_data, uint32_t page_size);
    
    // Initialize a new heap page
    Status initialize(uint32_t page_id, ErrorContext* ctx = nullptr);
    
    // Insert a tuple into the page
    // Returns the item ID (slot number) on success
    Status insert_tuple(const uint8_t* tuple_data, uint32_t tuple_size,
                       uint64_t xmin, uint16_t* item_id_out,
                       ErrorContext* ctx = nullptr);
    
    // Get tuple data by item ID
    Status get_tuple(uint16_t item_id, const uint8_t** data_out,
                    uint32_t* size_out, ErrorContext* ctx = nullptr);
    
    // Mark tuple as deleted
    Status delete_tuple(uint16_t item_id, uint64_t xmax,
                       ErrorContext* ctx = nullptr);
    
    // Check if there's enough space for a tuple
    bool has_free_space(uint32_t tuple_size) const;
    
    // Get number of tuples (including deleted)
    uint16_t get_item_count() const;
    
    // Get free space available
    uint32_t get_free_space() const;
    
    // Validate page structure
    Status validate(ErrorContext* ctx = nullptr) const;
    
    // Get page header
    PageHeader* header() { return reinterpret_cast<PageHeader*>(page_data_); }
    const PageHeader* header() const { return reinterpret_cast<const PageHeader*>(page_data_); }
    
private:
    uint8_t* page_data_;
    uint32_t page_size_;
    
    // Get pointer to item array (starts after PageHeader)
    ItemPointer* get_item_array() {
        return reinterpret_cast<ItemPointer*>(page_data_ + sizeof(PageHeader));
    }
    const ItemPointer* get_item_array() const {
        return reinterpret_cast<const ItemPointer*>(page_data_ + sizeof(PageHeader));
    }
    
    // Get pointer to special area
    HeapPageSpecial* get_special() {
        return reinterpret_cast<HeapPageSpecial*>(
            page_data_ + page_size_ - sizeof(HeapPageSpecial));
    }
    const HeapPageSpecial* get_special() const {
        return reinterpret_cast<const HeapPageSpecial*>(
            page_data_ + page_size_ - sizeof(HeapPageSpecial));
    }
    
    // Update page header after modifications
    void update_header_stats();
};

} // namespace core
} // namespace scratchbird