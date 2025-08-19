#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/txn.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;

TEST(txn_deadlock, simple_cycle_detection)
{
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap_rel(fl);
    fmap_rel.set_base_path("/tmp", "sb_txn_deadlock");
    FileMap fmap_tm(fl);
    fmap_tm.set_base_path("/tmp", "sb_txn_deadlock");
    TransactionManager tm(std::move(fmap_tm), fl.page_size);
    tm.init_seed();

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap_rel), fl.page_size, layout);

    // Insert two rows
    auto t0 = tm.begin();
    Value v;
    v.is_null = false;
    v.u64 = 1;
    auto rA = rel.insert_txn({v}, t0).rid;
    v.u64 = 2;
    auto rB = rel.insert_txn({v}, t0).rid;
    tm.commit(t0);

    auto t1 = tm.begin();
    auto t2 = tm.begin();

    // T1 locks A, T2 locks B
    ASSERT_TRUE(LockManager::acquire_write_lock(rA, t1.id));
    ASSERT_TRUE(LockManager::acquire_write_lock(rB, t2.id));
    // Now T1 waits on B, T2 waits on A -> cycle
    ASSERT_FALSE(LockManager::acquire_write_lock(rB, t1.id));
    ASSERT_FALSE(LockManager::acquire_write_lock(rA, t2.id));
}
