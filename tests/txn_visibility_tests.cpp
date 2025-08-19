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
    fmap.set_base_path("/tmp", "sb_txn_vis_test");

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap), fl.page_size, layout);

    // Transaction manager over same space
    FileMap fmap2(fl);
    fmap2.set_base_path("/tmp", "sb_txn_vis_test");
    TransactionManager tm(std::move(fmap2), fl.page_size);
    tm.init_seed();

    // Begin tx, insert row, verify created_xid set
    Transaction tx = tm.begin();
    std::vector<Value> v(1);
    v[0].u64 = 1234;
    auto ins = rel.insert_txn(v, tx);

    // Fetch head and confirm value roundtrip (created_xid not directly exposed, but presence
    // implied)
    std::vector<Value> out;
    if (!rel.fetch(ins.rid, out)) {
        std::cerr << "fetch failed" << std::endl;
        return 1;
    }
    if (out.size() != 1 || out[0].u64 != 1234) {
        std::cerr << "value mismatch" << std::endl;
        return 1;
    }

    // Prior to commit, visible scan with cutoff < tx.id should not see the row
    SnapshotRC snap_before = tm.snapshot_read_committed(/*own_xid*/ 0);
    snap_before.cutoff_committed_id = (tx.id > 0) ? (tx.id - 1) : 0;
    auto scan_before = rel.open_scan_visible(
        snap_before, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    std::vector<Value> tmp;
    ods::RowId rr{};
    if (scan_before.next(tmp, &rr)) {
        std::cerr << "uncommitted row should not be visible in RC" << std::endl;
        return 1;
    }

    // Commit and ensure TIP state is committed
    tm.commit(tx);
    if (tm.read_txn_state(tx.id) != TxnState::Committed) {
        std::cerr << "txn not committed" << std::endl;
        return 1;
    }

    // After commit, fetch by RID should succeed
    std::vector<Value> out2;
    if (!rel.fetch(ins.rid, out2)) {
        std::cerr << "committed row should be visible (fetch)" << std::endl;
        return 1;
    }

    // Start a new txn and ensure read-your-writes sees own insert
    Transaction tx2 = tm.begin();
    std::vector<Value> v2(1);
    v2[0].u64 = 777;
    auto ins2 = rel.insert_txn(v2, tx2);
    SnapshotRC snap_own = tm.snapshot_read_committed(tx2.id);
    auto scan_own =
        rel.open_scan_visible(snap_own, [&](std::uint64_t xid) { return tm.read_txn_state(xid); });
    bool saw_own = false;
    std::vector<Value> tmp2;
    ods::RowId rr2{};
    while (scan_own.next(tmp2, &rr2)) {
        if (rr2.page_no == ins2.rid.page_no && rr2.slot_no == ins2.rid.slot_no) {
            saw_own = true;
            break;
        }
    }
    if (!saw_own) {
        std::cerr << "own insert should be visible in RC" << std::endl;
        return 1;
    }

    std::cout << "txn_visibility_tests ok" << std::endl;
    return 0;
}
