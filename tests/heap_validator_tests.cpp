#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap.h"
#include "scratchbird/engine/heap_rel.h"

#include <cstring>
#include <iostream>
#include <vector>

using namespace scratchbird::engine;
using namespace scratchbird::engine::ods;

int main()
{
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 2048;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_validator_test");

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    // Insert a couple rows
    for (int i = 0; i < 2; ++i) {
        std::vector<Value> v(1);
        v[0].u64 = 100 + i;
        rel.insert(v);
    }

    // Corrupt slot 0: set offset outside bounds
    FileMap fmap2(fl);
    fmap2.set_base_path("/tmp", "sb_heap_validator_test");
    std::vector<std::uint8_t> pg(fl.page_size, 0);
    fmap2.read_page(1001, pg.data());
    auto hh = HeapPageCodec::read_heap_hdr(pg);
    std::uint16_t bad_off = static_cast<std::uint16_t>(hh.dir_start + 100);
    std::memcpy(pg.data() + (pg.size() - 1 * HEAP_SLOT_SIZE_BYTES), &bad_off, 2);
    fmap2.write_page(1001, pg.data());

    std::string err;
    bool ok = HeapPageCodec::check_heap_page_invariants(pg, err);
    if (ok) {
        std::cerr << "validator did not catch corruption" << std::endl;
        return 1;
    }
    std::cout << "heap_validator_tests ok" << std::endl;
    return 0;
}
