#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/ods.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;
using namespace scratchbird::engine::ods;

int main()
{
    const std::uint32_t page_size = 4096;
    std::vector<std::uint8_t> page(page_size, 0);
    HeapPageCodec::init_heap_data_page(page);
    std::string err;
    bool ok = HeapPageCodec::check_heap_page_invariants(page, err);
    if (!ok) {
        std::cerr << "Invariant failure on empty page: " << err << std::endl;
        return 1;
    }
    // Write a small tuple and add slot
    std::vector<std::uint8_t> tup = {'h', 'e', 'l', 'l', 'o'};
    std::uint16_t off = HeapPageCodec::write_raw_tuple(page, tup);
    HeapPageCodec::push_slot(page, off);
    ok = HeapPageCodec::check_heap_page_invariants(page, err);
    if (!ok) {
        std::cerr << "Invariant failure after insert: " << err << std::endl;
        return 1;
    }
    std::cout << "heap_page_tests ok" << std::endl;
    return 0;
}
