#include "scratchbird/engine/file.h"
#include "scratchbird/engine/heap_rel.h"
#include "scratchbird/engine/txn.h"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

using namespace scratchbird::engine;

class EnhancedDeadlockTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset to default policy before each test
        LockManager::set_deadlock_victim_policy(DeadlockVictimPolicy::YoungTransaction);
    }
};

TEST_F(EnhancedDeadlockTest, VictimSelectionPolicies)
{
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap_rel(fl);
    fmap_rel.set_base_path("/tmp", "sb_enhanced_deadlock");
    FileMap fmap_tm(fl);
    fmap_tm.set_base_path("/tmp", "sb_enhanced_deadlock");
    TransactionManager tm(std::move(fmap_tm), fl.page_size);
    tm.init_seed();

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap_rel), fl.page_size, layout);

    // Insert test rows
    auto t0 = tm.begin();
    Value v;
    v.is_null = false;
    v.u64 = 1;
    auto rA = rel.insert_txn({v}, t0).rid;
    v.u64 = 2;
    auto rB = rel.insert_txn({v}, t0).rid;
    tm.commit(t0);

    // Test 1: YoungTransaction policy (default)
    {
        LockManager::set_deadlock_victim_policy(DeadlockVictimPolicy::YoungTransaction);
        ASSERT_EQ(LockManager::get_deadlock_victim_policy(), DeadlockVictimPolicy::YoungTransaction);
        
        auto t1 = tm.begin();
        auto t2 = tm.begin();
        
        // T1 locks A first
        ASSERT_TRUE(LockManager::acquire_write_lock(rA, t1.id));
        
        // Small delay to ensure T2 starts after T1
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        
        // T2 locks B
        ASSERT_TRUE(LockManager::acquire_write_lock(rB, t2.id));
        
        // Create deadlock: T1 waits on B, T2 waits on A
        // With YoungTransaction policy, T2 (newer) should be victim
        ASSERT_FALSE(LockManager::acquire_write_lock(rB, t1.id));  // T1 waits
        ASSERT_FALSE(LockManager::acquire_write_lock(rA, t2.id));  // T2 becomes victim
        
        // Clean up
        LockManager::release_write_lock(rA, t1.id);
        LockManager::release_write_lock(rB, t2.id);
    }
    
    // Test 2: OldTransaction policy
    {
        LockManager::set_deadlock_victim_policy(DeadlockVictimPolicy::OldTransaction);
        ASSERT_EQ(LockManager::get_deadlock_victim_policy(), DeadlockVictimPolicy::OldTransaction);
        
        // Policy changed successfully - deadlock detection should now prefer different victims
        // (Actual victim selection testing would require more complex scenarios)
    }
    
    // Test 3: FewestLocks policy
    {
        LockManager::set_deadlock_victim_policy(DeadlockVictimPolicy::FewestLocks);
        ASSERT_EQ(LockManager::get_deadlock_victim_policy(), DeadlockVictimPolicy::FewestLocks);
        
        // Policy changed successfully
    }
    
    // Test 4: LowestCost policy  
    {
        LockManager::set_deadlock_victim_policy(DeadlockVictimPolicy::LowestCost);
        ASSERT_EQ(LockManager::get_deadlock_victim_policy(), DeadlockVictimPolicy::LowestCost);
        
        // Policy changed successfully
    }
}

TEST_F(EnhancedDeadlockTest, CycleDetectionAndReconstruction)
{
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap_rel(fl);
    fmap_rel.set_base_path("/tmp", "sb_cycle_test");
    FileMap fmap_tm(fl);
    fmap_tm.set_base_path("/tmp", "sb_cycle_test");
    TransactionManager tm(std::move(fmap_tm), fl.page_size);
    tm.init_seed();

    TupleLayout layout{{AttrMeta{AttrType::Int64, 8, true, false}}};
    auto rel = HeapRelation::create(std::move(fmap_rel), fl.page_size, layout);

    // Insert test rows
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

    // Create simple 2-transaction cycle by establishing wait-for relationships
    ASSERT_TRUE(LockManager::acquire_write_lock(rA, t1.id));  // T1 locks A
    ASSERT_TRUE(LockManager::acquire_write_lock(rB, t2.id));  // T2 locks B
    
    // Now create the deadlock by having each transaction wait for the other's lock
    ASSERT_FALSE(LockManager::acquire_write_lock(rB, t1.id)); // T1 tries to lock B (fails, waits)
    ASSERT_FALSE(LockManager::acquire_write_lock(rA, t2.id)); // T2 tries to lock A (fails, creates cycle)
    
    // Test that cycle detection works after the deadlock is established
    auto cycle = LockManager::find_deadlock_cycle(t2.id, t1.id);
    ASSERT_FALSE(cycle.empty());
    
    // Test victim selection
    auto victim = LockManager::choose_deadlock_victim(cycle);
    ASSERT_TRUE(victim == t1.id || victim == t2.id);
    
    // Clean up
    LockManager::release_write_lock(rA, t1.id);
    LockManager::release_write_lock(rB, t2.id);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}