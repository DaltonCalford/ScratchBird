#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/ods.h"

#include <cstring>
#include <iostream>
#include <vector>

using namespace scratchbird::engine;
using namespace scratchbird::engine::ods;

int main()
{
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = FileOptions{};
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_heap_corrupt_test");

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    std::vector<Value> v(1);
    v[0].u64 = 1;
    auto ins = rel.insert(v);

    // Tamper: set slot offset beyond dir_start to trigger invariant failure
    FileMap fmap2(fl);
    fmap2.set_base_path("/tmp", "sb_heap_corrupt_test");
    std::vector<std::uint8_t> pg(fl.page_size, 0);
    fmap2.read_page(ins.rid.page_no, pg.data());
    auto hh = HeapPageCodec::read_heap_hdr(pg);
    std::uint16_t bad_off = static_cast<std::uint16_t>(hh.dir_start + 10);
    std::memcpy(pg.data() + (pg.size() - (ins.rid.slot_no + 1) * HEAP_SLOT_SIZE_BYTES), &bad_off,
                2);
    fmap2.write_page(ins.rid.page_no, pg.data());

    std::string err;
    bool ok = HeapPageCodec::check_heap_page_invariants(pg, err);
    if (ok) {
        std::cerr << "corruption not detected" << std::endl;
        return 1;
    }
    std::cout << "heap_corruption_tests ok" << std::endl;
    return 0;
}
