#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace scratchbird::engine;

int main()
{
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 2048;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_scan_test");
    TupleLayout layout{
        {AttrMeta{TypeId::Int64, 8, true, false}, AttrMeta{TypeId::VarBytes, 0, false, true}}};
    HeapOptions opts{};
    opts.free_space_threshold_bytes = 64;
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout, opts);

    // insert rows
    std::vector<std::string> payloads;
    for (int i = 0; i < 100; ++i) {
        std::vector<Value> v(2);
        v[0].u64 = static_cast<std::uint64_t>(i);
        v[1].is_null = (i % 3 == 0);
        if (!v[1].is_null) {
            v[1].bytes = std::string(i % 20 + 1, 'a' + (i % 10));
        }
        rel.insert(v);
        payloads.push_back(v[1].is_null ? std::string() : v[1].bytes);
    }

    // scan
    auto scan = rel.open_scan();
    std::vector<std::uint64_t> ids;
    std::vector<std::string> outs;
    std::vector<Value> row;
    ods::RowId rid{};
    while (scan.next(row, &rid)) {
        ids.push_back(row[0].u64);
        outs.push_back(row[1].is_null ? std::string() : row[1].bytes);
    }
    if (ids.size() != 100) {
        std::cerr << "scan count mismatch" << std::endl;
        return 1;
    }
    // we didn't guarantee order; sort both
    std::vector<std::pair<std::uint64_t, std::string>> pairs;
    for (size_t i = 0; i < ids.size(); ++i)
        pairs.emplace_back(ids[i], outs[i]);
    std::sort(pairs.begin(), pairs.end());
    for (int i = 0; i < 100; ++i) {
        if (pairs[i].first != static_cast<std::uint64_t>(i)) {
            std::cerr << "id mismatch" << std::endl;
            return 1;
        }
        std::string expected = payloads[i];
        if (pairs[i].second != expected) {
            std::cerr << "payload mismatch" << std::endl;
            return 1;
        }
    }
    std::cout << "heap_scan_tests ok" << std::endl;
    return 0;
}
