#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_varshort_test");

    TupleLayout layout{{AttrMeta{AttrType::VarBytes, 0, false, false}}};
    HeapOptions opts{};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout, opts);

    std::string payload = "hello varlen";
    std::vector<Value> v(1);
    v[0].bytes = payload;
    InsertResult ins = rel.insert(v);

    std::vector<Value> out;
    if (!rel.fetch(ins.rid, out)) {
        std::cerr << "fetch failed" << std::endl;
        return 1;
    }
    if (out.size() != 1 || out[0].bytes != payload) {
        std::cerr << "varlen short roundtrip mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_varlen_short_tests ok" << std::endl;
    return 0;
}
