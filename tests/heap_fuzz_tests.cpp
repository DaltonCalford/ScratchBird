#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"

#include <cstdint>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace scratchbird::engine;

static std::uint64_t fnv1a64_update(std::uint64_t h, const void* data, std::size_t len)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

int main()
{
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int> boold(0, 1);
    std::uniform_int_distribution<int> typed(0, 1); // 0=int64,1=varbytes
    std::uniform_int_distribution<int> lenD(0, 64);

    // Random schema with 1..3 cols
    int ncols = 1 + (rng() % 3);
    TupleLayout layout{};
    for (int i = 0; i < ncols; ++i) {
        if (typed(rng) == 0) {
            layout.attrs.push_back(AttrMeta{AttrType::Int64, 8, true, boold(rng) != 0});
        } else {
            layout.attrs.push_back(AttrMeta{AttrType::VarBytes, 0, false, boold(rng) != 0});
        }
    }

    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 2048;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_fuzz_test");

    HeapOptions opts{};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout, opts);

    // Generate and insert rows
    struct Row {
        std::vector<Value> vals;
    };
    std::vector<Row> rows;
    std::vector<ods::RowId> rids;
    const int N = 200;
    for (int i = 0; i < N; ++i) {
        Row row{};
        row.vals.resize(layout.attrs.size());
        for (std::size_t c = 0; c < layout.attrs.size(); ++c) {
            bool make_null = boold(rng) != 0 && layout.attrs[c].nullable;
            row.vals[c].is_null = make_null;
            if (make_null)
                continue;
            if (layout.attrs[c].type == AttrType::Int64) {
                row.vals[c].u64 = static_cast<std::uint64_t>(rng());
            } else {
                int l = lenD(rng);
                row.vals[c].bytes.assign(l, static_cast<char>('a' + (rng() % 26)));
            }
        }
        auto res = rel.insert(row.vals);
        rows.push_back(std::move(row));
        rids.push_back(res.rid);
    }

    // Compute expected checksum
    std::uint64_t expect = 1469598103934665603ull; // FNV-1a 64 offset
    for (const auto& r : rows) {
        for (std::size_t c = 0; c < r.vals.size(); ++c) {
            const auto& v = r.vals[c];
            if (v.is_null) {
                std::uint8_t z = 0xFF;
                expect = fnv1a64_update(expect, &z, 1);
                continue;
            }
            // include type tag to avoid collisions
            std::uint8_t tag = (layout.attrs[c].type == AttrType::Int64) ? 1 : 2;
            expect = fnv1a64_update(expect, &tag, 1);
            if (tag == 1) {
                expect = fnv1a64_update(expect, &v.u64, sizeof v.u64);
            } else {
                std::uint32_t l = static_cast<std::uint32_t>(r.vals[c].bytes.size());
                expect = fnv1a64_update(expect, &l, 4);
                if (l)
                    expect = fnv1a64_update(expect, r.vals[c].bytes.data(), l);
            }
        }
    }

    // Scan and compute checksum
    auto scan = rel.open_scan();
    std::uint64_t got = 1469598103934665603ull;
    std::vector<Value> row;
    ods::RowId rid{};
    while (scan.next(row, &rid)) {
        for (std::size_t c = 0; c < row.size(); ++c) {
            const auto& v = row[c];
            if (v.is_null) {
                std::uint8_t z = 0xFF;
                got = fnv1a64_update(got, &z, 1);
                continue;
            }
            std::uint8_t tag = (layout.attrs[c].type == AttrType::Int64) ? 1 : 2;
            got = fnv1a64_update(got, &tag, 1);
            if (tag == 1) {
                got = fnv1a64_update(got, &v.u64, sizeof v.u64);
            } else {
                std::uint32_t l = static_cast<std::uint32_t>(row[c].bytes.size());
                got = fnv1a64_update(got, &l, 4);
                if (l)
                    got = fnv1a64_update(got, row[c].bytes.data(), l);
            }
        }
    }

    if (got != expect) {
        std::cerr << "heap_fuzz_tests checksum mismatch" << std::endl;
        return 1;
    }
    std::cout << "heap_fuzz_tests ok" << std::endl;
    return 0;
}
