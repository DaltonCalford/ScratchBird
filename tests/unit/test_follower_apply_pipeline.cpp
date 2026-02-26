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

        auto makeTmpFollowerDir() -> std::filesystem::path
        {
            std::filesystem::path path = std::filesystem::temp_directory_path() /
                ("sb_follower_apply_" + generateUuidV7().toString());
            std::filesystem::remove_all(path);
            return path;
        }

        auto appendEntry(ShardCommitLog& log,
                         const ID& shard_id,
                         uint64_t local_txn_id,
                         const std::string& payload) -> Status
        {
            ShardCommitLogEntry entry{};
            entry.gtxid.shard_id = shard_id;
            entry.gtxid.local_txn_id = local_txn_id;
            entry.commit_timestamp_ns = 1000 + local_txn_id;
            entry.payload = payload;
            return log.append(entry);
        }

    } // namespace

    TEST(FollowerApplyPipelineTest, InOrderApplyUpdatesReplicationWatermark)
    {
        const std::filesystem::path tmp_dir = makeTmpFollowerDir();
        const ID shard_id = generateUuidV7();
        ShardCommitLog log(tmp_dir.string());
        ASSERT_EQ(appendEntry(log, shard_id, 1, "payload-1"), Status::OK);
        ASSERT_EQ(appendEntry(log, shard_id, 2, "payload-2"), Status::OK);

        FollowerApplyPipeline pipeline(&log);
        FollowerApplyResult result{};

        ASSERT_EQ(pipeline.apply(shard_id, 1, "payload-1", &result), Status::OK);
        EXPECT_TRUE(result.applied);
        EXPECT_FALSE(result.replayed);
        EXPECT_EQ(result.reason, FollowerApplyReason::NONE);
        EXPECT_EQ(result.replication_watermark, 1u);

        ASSERT_EQ(pipeline.apply(shard_id, 2, "payload-2", &result), Status::OK);
        EXPECT_TRUE(result.applied);
        EXPECT_EQ(result.reason, FollowerApplyReason::NONE);
        EXPECT_EQ(result.replication_watermark, 2u);
        EXPECT_EQ(pipeline.replicationWatermark(shard_id), 2u);
    }

    TEST(FollowerApplyPipelineTest, ReplayIsIdempotentAndOrderingIsEnforced)
    {
        const std::filesystem::path tmp_dir = makeTmpFollowerDir();
        const ID shard_id = generateUuidV7();
        ShardCommitLog log(tmp_dir.string());
        ASSERT_EQ(appendEntry(log, shard_id, 1, "payload-1"), Status::OK);
        ASSERT_EQ(appendEntry(log, shard_id, 2, "payload-2"), Status::OK);

        FollowerApplyPipeline pipeline(&log);
        FollowerApplyResult result{};

        ASSERT_EQ(pipeline.apply(shard_id, 1, "payload-1", &result), Status::OK);
        EXPECT_TRUE(result.applied);
        EXPECT_EQ(result.replication_watermark, 1u);

        ASSERT_EQ(pipeline.apply(shard_id, 1, "payload-1", &result), Status::OK);
        EXPECT_FALSE(result.applied);
        EXPECT_TRUE(result.replayed);
        EXPECT_EQ(result.reason, FollowerApplyReason::ALREADY_APPLIED);
        EXPECT_EQ(result.replication_watermark, 1u);

        ASSERT_EQ(pipeline.apply(shard_id, 2, "wrong-payload", &result), Status::DATA_CORRUPTED);
        EXPECT_FALSE(result.applied);
        EXPECT_FALSE(result.replayed);
        EXPECT_EQ(result.reason, FollowerApplyReason::PAYLOAD_MISMATCH);
        EXPECT_EQ(result.replication_watermark, 1u);

        ASSERT_EQ(pipeline.apply(shard_id, 4, "payload-4", &result), Status::INVALID_TRANSACTION_STATE);
        EXPECT_EQ(result.reason, FollowerApplyReason::OUT_OF_ORDER);
        EXPECT_EQ(result.expected_next_local_txn_id, 2u);

        ASSERT_EQ(pipeline.apply(shard_id, 2, "payload-2", &result), Status::OK);
        EXPECT_TRUE(result.applied);
        EXPECT_EQ(result.replication_watermark, 2u);

        ASSERT_EQ(pipeline.apply(shard_id, 3, "payload-3", &result), Status::NOT_FOUND);
        EXPECT_FALSE(result.applied);
        EXPECT_EQ(result.reason, FollowerApplyReason::LOG_ENTRY_NOT_FOUND);
        EXPECT_EQ(result.replication_watermark, 2u);
    }

} // namespace scratchbird::core

