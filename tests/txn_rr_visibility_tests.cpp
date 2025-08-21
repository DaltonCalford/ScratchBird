#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/txn.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;

TEST(txn_rr_visibility, snapshot_stability)
{
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap_rel(fl);
    fmap_rel.set_base_path("/tmp", "sb_txn_rr_vis");
    FileMap fmap_tm(fl);
    fmap_tm.set_base_path("/tmp", "sb_txn_rr_vis");
    TransactionManager tm(std::move(fmap_tm), fl.page_size);
    tm.init_seed();

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap_rel), fl.page_size, layout);

    // Insert base row r0 in T1 and commit
    auto t1 = tm.begin();
    Value v;
    v.is_null = false;
    v.u64 = 1;
    auto r0 = rel.insert_txn({v}, t1).rid;
    tm.commit(t1);

    // T2 takes RR snapshot
    auto t2 = tm.begin();
    SnapshotRR rr = tm.snapshot_repeatable_read(t2.id);

    // T3 updates r0 -> r1 but does not commit yet
    auto t3 = tm.begin();
    Value v2;
    v2.is_null = false;
    v2.u64 = 2;
    ods::RowId r1{};
    ASSERT_TRUE(rel.update_txn(r0, {v2}, t3, &r1));

    // Under RR snapshot before commit, T2 should still see old value 1
    auto scan_rr =
        rel.open_scan_visible(rr, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    std::vector<Value> out;
    ods::RowId rid_out{};
    ASSERT_TRUE(scan_rr.next(out, &rid_out));
    ASSERT_EQ(out[0].u64, 1u);

    // Now commit T3 and confirm RR still sees old value with same snapshot
    tm.commit(t3);
    scan_rr = rel.open_scan_visible(rr, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    out.clear();
    ASSERT_TRUE(scan_rr.next(out, &rid_out));
    ASSERT_EQ(out[0].u64, 1u);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
