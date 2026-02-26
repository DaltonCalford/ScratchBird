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

#include <filesystem>

namespace scratchbird::core
{

    namespace
    {

        auto makeTmpGcDir() -> std::filesystem::path
        {
            std::filesystem::path path = std::filesystem::temp_directory_path() /
                ("sb_gc_horizon_" + generateUuidV7().toString());
            std::filesystem::remove_all(path);
            return path;
        }

        auto appendAndApplyRange(ShardCommitLog& log,
                                 FollowerApplyPipeline& follower,
                                 const ID& shard_id,
                                 uint64_t from_local,
                                 uint64_t to_local) -> Status
        {
            for (uint64_t i = from_local; i <= to_local; ++i)
            {
                ShardCommitLogEntry entry{};
                entry.gtxid.shard_id = shard_id;
                entry.gtxid.local_txn_id = i;
                entry.commit_timestamp_ns = 1000 + i;
                entry.payload = "payload-" + std::to_string(i);
                Status status = log.append(entry);
                if (status != Status::OK)
                {
                    return status;
                }
                status = follower.apply(shard_id, i, entry.payload);
                if (status != Status::OK)
                {
                    return status;
                }
            }
            return Status::OK;
        }

    } // namespace

    TEST(GcSafeHorizonCalculatorTest, ComputesMinOfOstAndRwmAndControlsReclaimability)
    {
        const std::filesystem::path tmp_dir = makeTmpGcDir();
        const ID shard_id = generateUuidV7();
        ShardCommitLog log(tmp_dir.string());
        FollowerApplyPipeline follower(&log);
        SnapshotRegistry snapshots;
        GcSafeHorizonCalculator calculator(&snapshots, &follower);

        ASSERT_EQ(appendAndApplyRange(log, follower, shard_id, 1, 6), Status::OK);
        EXPECT_EQ(follower.replicationWatermark(shard_id), 6u);

        SnapshotRegistryEntry s1{};
        s1.session_id = generateUuidV7();
        s1.shard_id = shard_id;
        s1.snapshot_boundary = 10;
        s1.start_time_ns = 100;
        s1.last_heartbeat_ns = 120;
        ASSERT_EQ(snapshots.registerOrUpdate(s1), Status::OK);

        SnapshotRegistryEntry s2{};
        s2.session_id = generateUuidV7();
        s2.shard_id = shard_id;
        s2.snapshot_boundary = 7;
        s2.start_time_ns = 101;
        s2.last_heartbeat_ns = 121;
        ASSERT_EQ(snapshots.registerOrUpdate(s2), Status::OK);

        GcSafeHorizonEvaluation eval{};
        ASSERT_EQ(calculator.evaluate(shard_id, eval), Status::OK);
        EXPECT_EQ(eval.oldest_snapshot_boundary, 7u);
        EXPECT_EQ(eval.replication_watermark, 6u);
        EXPECT_EQ(eval.gc_safe_horizon, 6u);

        bool reclaimable = false;
        ASSERT_EQ(calculator.canReclaimVersion(shard_id, 5, reclaimable, &eval), Status::OK);
        EXPECT_TRUE(reclaimable);
        EXPECT_EQ(eval.gc_safe_horizon, 6u);

        ASSERT_EQ(calculator.canReclaimVersion(shard_id, 6, reclaimable, &eval), Status::OK);
        EXPECT_FALSE(reclaimable);
    }

    TEST(GcSafeHorizonCalculatorTest, MissingOstOrRwmYieldsZeroSafeHorizon)
    {
        const std::filesystem::path tmp_dir = makeTmpGcDir();
        const ID shard_id = generateUuidV7();
        ShardCommitLog log(tmp_dir.string());
        FollowerApplyPipeline follower(&log);
        SnapshotRegistry snapshots;
        GcSafeHorizonCalculator calculator(&snapshots, &follower);

        GcSafeHorizonEvaluation eval{};
        ASSERT_EQ(calculator.evaluate(shard_id, eval), Status::OK);
        EXPECT_EQ(eval.gc_safe_horizon, 0u);

        bool reclaimable = true;
        ASSERT_EQ(calculator.canReclaimVersion(shard_id, 1, reclaimable, &eval), Status::OK);
        EXPECT_FALSE(reclaimable);
        EXPECT_EQ(eval.gc_safe_horizon, 0u);

        SnapshotRegistryEntry s{};
        s.session_id = generateUuidV7();
        s.shard_id = shard_id;
        s.snapshot_boundary = 5;
        s.start_time_ns = 100;
        s.last_heartbeat_ns = 120;
        ASSERT_EQ(snapshots.registerOrUpdate(s), Status::OK);

        ASSERT_EQ(calculator.evaluate(shard_id, eval), Status::OK);
        EXPECT_EQ(eval.oldest_snapshot_boundary, 5u);
        EXPECT_EQ(eval.replication_watermark, 0u);
        EXPECT_EQ(eval.gc_safe_horizon, 0u);
    }

} // namespace scratchbird::core

