#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <iostream>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_fixed_test");

    TupleLayout layout{
        {AttrMeta{AttrType::Int64, 8, true, false}, AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    // All fixed-width, non-null row
    std::vector<Value> v(2);
    v[0].u64 = 111;
    v[1].u64 = 222;
    InsertResult ins = rel.insert(v);

    std::vector<Value> out;
    if (!rel.fetch(ins.rid, out)) {
        std::cerr << "fetch failed" << std::endl;
        return 1;
    }
    if (!(out.size() == 2 && !out[0].is_null && !out[1].is_null && out[0].u64 == 111 &&
          out[1].u64 == 222)) {
        std::cerr << "fixed roundtrip mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_rel_fixed_tests ok" << std::endl;
    return 0;
}
