#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 4096;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_many_rows_test");

    TupleLayout layout{
        {AttrMeta{AttrType::Int64, 8, true, false}, AttrMeta{AttrType::VarBytes, 0, false, true}}};
    HeapOptions opts{};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout, opts);

    const int N = 10000;
    std::uint64_t expect_sum = 0;
    for (int i = 0; i < N; ++i) {
        std::vector<Value> v(2);
        v[0].u64 = static_cast<std::uint64_t>(i);
        if ((i % 5) == 0) {
            v[1].is_null = true;
        } else {
            v[1].bytes = std::string((i % 13) + 1, 'a' + (i % 26));
        }
        rel.insert(v);
        expect_sum += static_cast<std::uint64_t>(i);
    }

    // Scan and verify count and sum of the first column
    auto scan = rel.open_scan();
    int count = 0;
    std::uint64_t sum = 0;
    std::vector<Value> row;
    ods::RowId rid{};
    while (scan.next(row, &rid)) {
        ++count;
        sum += row[0].u64;
    }
    if (count != N) {
        std::cerr << "count mismatch: got " << count << " expect " << N << std::endl;
        return 1;
    }
    if (sum != expect_sum) {
        std::cerr << "sum mismatch: got " << sum << " expect " << expect_sum << std::endl;
        return 1;
    }
    std::cout << "heap_many_rows_tests ok" << std::endl;
    return 0;
}
