#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/txn.h"

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
    fmap.set_base_path("/tmp", "sb_txn_del_vis_test");

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    // Transaction manager over same space
    FileMap fmap2(fl);
    fmap2.set_base_path("/tmp", "sb_txn_del_vis_test");
    TransactionManager tm(std::move(fmap2), fl.page_size);
    tm.init_seed();

    // Insert committed row
    Transaction txi = tm.begin();
    std::vector<Value> v(1);
    v[0].u64 = 42;
    auto ins = rel.insert_txn(v, txi);
    tm.commit(txi);

    // Ensure visible after commit
    std::vector<Value> got;
    if (!rel.fetch(ins.rid, got) || got[0].u64 != 42) {
        std::cerr << "post-insert fetch failed" << std::endl;
        return 1;
    }

    // Delete in a new txn and confirm hidden to others but visible to self
    Transaction txd = tm.begin();
    if (!rel.remove_txn(ins.rid, txd)) {
        std::cerr << "remove_txn failed" << std::endl;
        return 1;
    }

    // Snapshot as other (no own_xid) should still see the row pre-commit
    SnapshotRC snap_other = tm.snapshot_read_committed(0);
    auto scan_other = rel.open_scan_visible(
        snap_other, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    std::vector<Value> tmp;
    ods::RowId rr{};
    bool saw_other = false;
    while (scan_other.next(tmp, &rr)) {
        if (rr.page_no == ins.rid.page_no && rr.slot_no == ins.rid.slot_no) {
            saw_other = true;
            break;
        }
    }
    if (!saw_other) {
        std::cerr << "deleted row should still be visible to others pre-commit" << std::endl;
        return 1;
    }

    // Snapshot as self (own_xid) should not see the row
    SnapshotRC snap_self = tm.snapshot_read_committed(txd.id);
    auto scan_self =
        rel.open_scan_visible(snap_self, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    bool saw_self = false;
    while (scan_self.next(tmp, &rr)) {
        if (rr.page_no == ins.rid.page_no && rr.slot_no == ins.rid.slot_no) {
            saw_self = true;
            break;
        }
    }
    if (saw_self) {
        std::cerr << "own delete should hide row to self in RC" << std::endl;
        return 1;
    }

    // Commit delete; row must be hidden to all
    tm.commit(txd);
    SnapshotRC snap_after = tm.snapshot_read_committed(0);
    auto scan_after = rel.open_scan_visible(
        snap_after, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    while (scan_after.next(tmp, &rr)) {
        if (rr.page_no == ins.rid.page_no && rr.slot_no == ins.rid.slot_no) {
            std::cerr << "deleted row should not be visible after commit" << std::endl;
            return 1;
        }
    }

    std::cout << "txn_delete_visibility_tests ok" << std::endl;
    return 0;
}
