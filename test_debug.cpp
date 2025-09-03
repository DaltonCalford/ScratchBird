#include <iostream>
#include <vector>
#include <cstring>
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird::core;

int main() {
    ErrorContext ctx;
    
    // Test with 8KB page
    const uint32_t page_size = 8192;
    std::vector<uint8_t> buffer(page_size, 0);
    
    // Create heap page
    HeapPage heap_page(buffer.data(), page_size);
    
    // Initialize it
    Status status = heap_page.initialize(1, &ctx);
    std::cout << "Initialize status: " << (int)status << std::endl;
    
    // Check free space
    uint32_t free_space = heap_page.get_free_space();
    std::cout << "Free space after init: " << free_space << std::endl;
    
    // Create a 500-byte tuple
    std::vector<uint8_t> tuple(500);
    TupleHeader* hdr = reinterpret_cast<TupleHeader*>(tuple.data());
    hdr->xmin = 1;
    hdr->xmax = 0;
    hdr->flags = 0;
    hdr->null_bitmap_offset = 0;
    memset(tuple.data() + sizeof(TupleHeader), 'T', tuple.size() - sizeof(TupleHeader));
    
    // Try to insert
    uint16_t item_id;
    status = heap_page.insert_tuple(tuple.data(), tuple.size(), 1, &item_id, &ctx);
    std::cout << "Insert status: " << (int)status << std::endl;
    if (status != Status::Ok) {
        std::cout << "Error: " << ctx.message << std::endl;
    }
    
    // Check special area values
    PageHeader* page_hdr = reinterpret_cast<PageHeader*>(buffer.data());
    std::cout << "Header free_space: " << page_hdr->free_space << std::endl;
    std::cout << "Header free_offset: " << page_hdr->free_offset << std::endl;
    
    return 0;
}