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
    fmap.set_base_path("/tmp", "sb_heap_trunc_drop_test");

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    // Insert some rows across a few pages
    for (int i = 0; i < 500; ++i) {
        std::vector<Value> v(1);
        v[0].u64 = static_cast<std::uint64_t>(i);
        rel.insert(v);
    }

    // Truncate
    rel.truncate();
    // Insert a single row and ensure scan returns exactly 1
    std::vector<Value> v(1);
    v[0].u64 = 42;
    rel.insert(v);
    auto scan = rel.open_scan();
    int count = 0;
    std::vector<Value> row;
    ods::RowId rid{};
    while (scan.next(row, &rid))
        ++count;
    if (count != 1) {
        std::cerr << "truncate failed, count=" << count << std::endl;
        return 1;
    }

    // Drop (no easy observable API; just ensure it doesn't crash)
    rel.drop();
    std::cout << "heap_truncate_drop_tests ok" << std::endl;
    return 0;
}
