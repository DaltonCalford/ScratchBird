#include "scratchbird/engine/file.h"
#include "scratchbird/engine/txn.h"

#include <gtest/gtest.h>

using namespace scratchbird::engine;

TEST(txn_prepare_scaffold, tip_state_transitions)
{
    FileOptions fo{};
    fo.direct_io = false;
    FileMap::Layout fl{};
    fl.page_size = 4096;
    fl.pages_per_segment = 1024;
    fl.options = fo;
    FileMap fmap(fl);
    fmap.set_base_path("/tmp", "sb_txn_prepare");
    TransactionManager tm(std::move(fmap), fl.page_size);
    tm.init_seed();

    auto t = tm.begin();
    ASSERT_EQ(tm.read_txn_state(t.id), TxnState::Active);

    tm.prepare(t);
    ASSERT_EQ(tm.read_txn_state(t.id), TxnState::Prepared);

    tm.commit(t);
    ASSERT_EQ(tm.read_txn_state(t.id), TxnState::Committed);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
