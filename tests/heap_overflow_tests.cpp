#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    FileMap::Layout layout{};
    layout.page_size = 4096;
    layout.pages_per_segment = 1024;
    layout.options = FileOptions{};
    FileMap fmap(layout);
    fmap.set_base_path("/tmp", "sb_heap_overflow_test");

    TupleLayout tl{{AttrMeta{AttrType::VarBytes, false}}};
    auto rel = HeapRelation::create(std::move(fmap), layout.page_size, tl);

    // Large payload to force overflow
    // Multi-chunk simulation: insert two large rows to force sequential overflow pages
    std::string payload(3000, 'X');
    std::vector<Value> v(1);
    v[0].is_null = false;
    v[0].bytes = payload;
    InsertResult res = rel.insert(v);
    // second large row
    std::string payload2(3500, 'Y');
    v[0].bytes = payload2;
    InsertResult res2 = rel.insert(v);

    std::vector<Value> out;
    if (!rel.fetch(res.rid, out)) {
        std::cerr << "fetch failed" << std::endl;
        return 1;
    }
    if (out.size() != 1 || out[0].bytes != payload) {
        std::cerr << "overflow roundtrip mismatch" << std::endl;
        return 1;
    }
    std::vector<Value> out2;
    if (!rel.fetch(res2.rid, out2)) {
        std::cerr << "fetch2 failed" << std::endl;
        return 1;
    }
    if (out2.size() != 1 || out2[0].bytes != payload2) {
        std::cerr << "overflow roundtrip mismatch 2" << std::endl;
        return 1;
    }
    std::cout << "heap_overflow_tests ok" << std::endl;
    return 0;
}
