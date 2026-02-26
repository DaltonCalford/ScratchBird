/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/cluster_write_safety.h"

#include <gtest/gtest.h>

#include <unordered_map>
#include <vector>

namespace scratchbird::core
{

    TEST(SnapshotRegistryTest, TracksOldestSnapshotBoundaryPerShard)
    {
        SnapshotRegistry registry;
        const ID shard_id = generateUuidV7();
        const ID session_a = generateUuidV7();
        const ID session_b = generateUuidV7();

        SnapshotRegistryEntry a{};
        a.session_id = session_a;
        a.shard_id = shard_id;
        a.snapshot_boundary = 40;
        a.start_time_ns = 1000;
        a.last_heartbeat_ns = 1100;

        SnapshotRegistryEntry b{};
        b.session_id = session_b;
        b.shard_id = shard_id;
        b.snapshot_boundary = 25;
        b.start_time_ns = 1010;
        b.last_heartbeat_ns = 1110;

        ASSERT_EQ(registry.registerOrUpdate(a), Status::OK);
        ASSERT_EQ(registry.registerOrUpdate(b), Status::OK);
        EXPECT_EQ(registry.oldestSnapshotBoundary(shard_id), 25u);

        b.snapshot_boundary = 30;
        ASSERT_EQ(registry.registerOrUpdate(b), Status::OK);
        EXPECT_EQ(registry.oldestSnapshotBoundary(shard_id), 30u);

        ASSERT_EQ(registry.remove(session_b, shard_id), Status::OK);
        EXPECT_EQ(registry.oldestSnapshotBoundary(shard_id), 40u);

        std::vector<SnapshotRegistryEntry> rows;
        ASSERT_EQ(registry.listByShard(shard_id, rows), Status::OK);
        ASSERT_EQ(rows.size(), 1u);
        EXPECT_EQ(rows[0].session_id, session_a);
    }

    TEST(CommittedWatermarkPublisherTest, PublishesMonotonicCwmAndSnapshotVector)
    {
        CommittedWatermarkPublisher publisher;
        const ID shard_a = generateUuidV7();
        const ID shard_b = generateUuidV7();
        const ID shard_c = generateUuidV7();

        GTXID a1{};
        a1.shard_id = shard_a;
        a1.local_txn_id = 1;
        ASSERT_EQ(publisher.publishCommitted(a1), Status::OK);
        EXPECT_EQ(publisher.watermark(shard_a), 1u);

        GTXID a2 = a1;
        a2.local_txn_id = 2;
        ASSERT_EQ(publisher.publishCommitted(a2), Status::OK);
        EXPECT_EQ(publisher.watermark(shard_a), 2u);

        GTXID a_back = a1;
        ASSERT_EQ(publisher.publishCommitted(a_back), Status::INVALID_TRANSACTION_STATE);
        EXPECT_EQ(publisher.watermark(shard_a), 2u);

        GTXID b5{};
        b5.shard_id = shard_b;
        b5.local_txn_id = 5;
        ASSERT_EQ(publisher.publishCommitted(b5), Status::OK);
        EXPECT_EQ(publisher.watermark(shard_b), 5u);
        EXPECT_EQ(publisher.watermark(shard_c), 0u);

        std::unordered_map<ID, uint64_t, IDHash> snapshot_vector;
        ASSERT_EQ(publisher.snapshotVector({shard_a, shard_b, shard_c}, snapshot_vector), Status::OK);
        ASSERT_EQ(snapshot_vector.size(), 3u);
        EXPECT_EQ(snapshot_vector[shard_a], 2u);
        EXPECT_EQ(snapshot_vector[shard_b], 5u);
        EXPECT_EQ(snapshot_vector[shard_c], 0u);
    }

} // namespace scratchbird::core

