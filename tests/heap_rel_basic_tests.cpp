#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    // Create a temporary file map for testing under /tmp
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 1024;
    layout.options = FileOptions{};
    FileMap fmap(layout);
    fmap.set_base_path("/tmp", "sb_heap_rel_test");

    TupleLayout layout_tuple{{AttrMeta{AttrType::Int64, true}, AttrMeta{AttrType::Int64, false}}};
    auto rel = HeapRelation::create(std::move(fmap), layout.page_size, layout_tuple);

    std::vector<Value> v{Value{true, 0}, Value{false, 123}};
    InsertResult res = rel.insert(v);

    std::vector<Value> out;
    if (!rel.fetch(res.rid, out)) {
        std::cerr << "fetch failed" << std::endl;
        return 1;
    }
    if (!(out.size() == 2 && out[0].is_null && !out[1].is_null && out[1].u64 == 123)) {
        std::cerr << "roundtrip mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_rel_basic_tests ok" << std::endl;
    return 0;
}
