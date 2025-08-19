#include "scratchbird/engine/heap.h"

#include <iostream>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    TupleLayout layout{{AttrMeta{AttrType::Int64, true}, AttrMeta{AttrType::Int64, false},
                        AttrMeta{AttrType::VarBytes, true}}};
    std::vector<Value> vals(2);
    vals[0].is_null = true;
    vals[1].u64 = 42;
    // add a varbytes null to test directory
    vals.push_back(Value{true, 0, std::string()});
    auto bytes = HeapTupleCodec::encode_tuple(layout, vals);

    // Place into a page and decode via offset
    std::vector<std::uint8_t> page(4096, 0);
    HeapPageCodec::init_heap_data_page(page);
    auto off = HeapPageCodec::write_raw_tuple(page, bytes);
    HeapPageCodec::push_slot(page, off);

    std::vector<Value> out;
    if (!HeapTupleCodec::decode_tuple(layout, page, off, out, {})) {
        std::cerr << "decode failed" << std::endl;
        return 1;
    }
    if (!(out.size() == 3 && out[0].is_null && !out[1].is_null && out[1].u64 == 42 &&
          out[2].is_null)) {
        std::cerr << "roundtrip mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_tuple_tests ok" << std::endl;
    return 0;
}
