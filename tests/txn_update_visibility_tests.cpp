#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/txn.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;

static TupleLayout make_layout()
{
    TupleLayout l{};
    l.attrs.push_back({AttrType::Int64, 8, true, false});
    l.attrs.push_back({AttrType::VarBytes, 0, false, true});
    return l;
}

TEST(txn_update_visibility, concurrent_update_rc)
{
    // temp file map
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap_rel(fl);
    std::string base = "/tmp";
    const char* suffix = "sb_txn_update_vis";
    fmap_rel.set_base_path(base.c_str(), suffix);

    // Seed TIP using a separate FileMap bound to same base path
    FileMap fmap_tm(fl);
    fmap_tm.set_base_path(base.c_str(), suffix);
    TransactionManager tm(std::move(fmap_tm), fl.page_size);
    tm.init_seed();

    // Create relation
    TupleLayout layout = make_layout();
    HeapRelation rel = HeapRelation::create(std::move(fmap_rel), fl.page_size, layout);

    // Begin T1 inserts row R0
    auto t1 = tm.begin();
    Value v0i;
    v0i.is_null = false;
    v0i.u64 = 1;
    Value v0s;
    v0s.is_null = true;
    ods::RowId r0;
    {
        auto ir = rel.insert_txn({v0i, v0s}, t1);
        r0 = ir.rid;
    }

    // T2 scans: should not see uncommitted R0
    auto t2 = tm.begin();
    SnapshotRC s2 = tm.snapshot_read_committed(t2.id);
    auto scan2 =
        rel.open_scan_visible(s2, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    std::vector<Value> out;
    ods::RowId rid_out{};
    ASSERT_FALSE(scan2.next(out, &rid_out));

    // Commit T1
    tm.commit(t1);

    // T2 scans again: should see committed R0
    s2 = tm.snapshot_read_committed(t2.id);
    scan2 = rel.open_scan_visible(s2, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    ASSERT_TRUE(scan2.next(out, &rid_out));
    ASSERT_EQ(out[0].u64, 1u);

    // T3 starts an update on R0 to R1 while T2 holds RC snapshot
    auto t3 = tm.begin();
    Value v1i;
    v1i.is_null = false;
    v1i.u64 = 2;
    ods::RowId r1{};
    ASSERT_TRUE(rel.update_txn(rid_out, {v1i, v0s}, t3, &r1));

    // T2 with old snapshot should still see old value
    scan2 = rel.open_scan_visible(s2, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    out.clear();
    ods::RowId rid_again{};
    ASSERT_TRUE(scan2.next(out, &rid_again));
    ASSERT_EQ(out[0].u64, 1u);

    // T3 (updater) should see new value and not see deleted old
    SnapshotRC s3 = tm.snapshot_read_committed(t3.id);
    auto scan3 =
        rel.open_scan_visible(s3, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    out.clear();
    ods::RowId rid3{};
    ASSERT_TRUE(scan3.next(out, &rid3));
    ASSERT_EQ(out[0].u64, 2u);
    ASSERT_FALSE(scan3.next(out, &rid3));

    // Commit T3
    tm.commit(t3);

    // New snapshot for T2 after commit sees only new value
    s2 = tm.snapshot_read_committed(t2.id);
    scan2 = rel.open_scan_visible(s2, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    out.clear();
    ASSERT_TRUE(scan2.next(out, &rid_out));
    ASSERT_EQ(out[0].u64, 2u);
    ASSERT_FALSE(scan2.next(out, &rid_out));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
