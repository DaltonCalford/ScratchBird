/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include "scratchbird/core/cluster_write_safety.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    TEST(GtxidOrderingTest, AllocationAndCommitOrderingIsMonotonicPerShard)
    {
        ShardTxnOrderBook order_book;
        const ID shard_a = generateUuidV7();
        const ID shard_b = generateUuidV7();

        GTXID a1{};
        GTXID a2{};
        GTXID b1{};
        ASSERT_EQ(order_book.allocateNext(shard_a, a1), Status::OK);
        ASSERT_EQ(order_book.allocateNext(shard_a, a2), Status::OK);
        ASSERT_EQ(order_book.allocateNext(shard_b, b1), Status::OK);

        EXPECT_EQ(a1.local_txn_id, 1u);
        EXPECT_EQ(a2.local_txn_id, 2u);
        EXPECT_EQ(b1.local_txn_id, 1u);

        TxnOrderingResult c1 = order_book.recordCommitted(a1);
        TxnOrderingResult c2 = order_book.recordCommitted(a2);
        EXPECT_TRUE(c1.accepted);
        EXPECT_TRUE(c2.accepted);
        EXPECT_EQ(order_book.lastCommitted(shard_a), 2u);

        TxnOrderingResult duplicate = order_book.recordCommitted(a2);
        EXPECT_FALSE(duplicate.accepted);
        EXPECT_EQ(duplicate.reason, TxnOrderingReason::STALE_OR_DUPLICATE);
    }

    TEST(GtxidOrderingTest, FollowerApplyRejectsGapsAndDuplicates)
    {
        ShardTxnOrderBook order_book;
        const ID shard = generateUuidV7();

        GTXID g1{shard, 1};
        GTXID g2{shard, 2};
        GTXID g3{shard, 3};

        TxnOrderingResult apply1 = order_book.recordFollowerApply(g1);
        EXPECT_TRUE(apply1.accepted);

        TxnOrderingResult gap = order_book.recordFollowerApply(g3);
        EXPECT_FALSE(gap.accepted);
        EXPECT_EQ(gap.reason, TxnOrderingReason::OUT_OF_ORDER);
        EXPECT_EQ(gap.expected_next_local_txn_id, 2u);

        TxnOrderingResult apply2 = order_book.recordFollowerApply(g2);
        EXPECT_TRUE(apply2.accepted);

        TxnOrderingResult duplicate = order_book.recordFollowerApply(g2);
        EXPECT_FALSE(duplicate.accepted);
        EXPECT_EQ(duplicate.reason, TxnOrderingReason::STALE_OR_DUPLICATE);
    }
} // namespace scratchbird::core
