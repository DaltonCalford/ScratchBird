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
    fmap.set_base_path("/tmp", "sb_heap_null_test");

    TupleLayout layout{
        {AttrMeta{AttrType::Int64, 8, true, true}, AttrMeta{AttrType::VarBytes, 0, false, true}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    // All-null row
    std::vector<Value> v(2);
    v[0].is_null = true;
    v[1].is_null = true;
    InsertResult ins = rel.insert(v);

    std::vector<Value> out;
    if (!rel.fetch(ins.rid, out)) {
        std::cerr << "fetch failed" << std::endl;
        return 1;
    }
    if (!(out.size() == 2 && out[0].is_null && out[1].is_null)) {
        std::cerr << "all-null roundtrip mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_rel_null_tests ok" << std::endl;
    return 0;
}
