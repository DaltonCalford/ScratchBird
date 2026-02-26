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
    TEST(ClusterWriteFencingTest, StaleLeaderWritesAreRejectedByTokenAndLeaderIdentity)
    {
        ClusterWriteSafetyController controller;

        const ID shard_id = generateUuidV7();
        const ID leader_a = generateUuidV7();
        const ID leader_b = generateUuidV7();

        ShardLeaderState state{};
        state.shard_id = shard_id;
        state.leader_node_id = leader_a;
        state.leader_term = 7;
        state.routing_epoch = 44;
        ASSERT_EQ(controller.upsertShardLeaderState(state), Status::OK);

        WriteAdmissionRequest req{};
        req.shard_id = shard_id;
        req.node_id = leader_a;
        req.fencing_token.shard_id = shard_id;
        req.fencing_token.leader_term = 7;
        req.has_routing_epoch = true;
        req.routing_epoch = 44;

        WriteAdmissionResult allow = controller.validateWrite(req);
        EXPECT_TRUE(allow.allowed);
        EXPECT_EQ(allow.status, Status::OK);
        EXPECT_EQ(allow.reason, WriteAdmissionReason::NONE);

        req.fencing_token.leader_term = 6;
        WriteAdmissionResult stale_token = controller.validateWrite(req);
        EXPECT_FALSE(stale_token.allowed);
        EXPECT_EQ(stale_token.status, Status::INVALID_TRANSACTION_STATE);
        EXPECT_EQ(stale_token.reason, WriteAdmissionReason::STALE_FENCING_TOKEN);

        req.fencing_token.leader_term = 7;
        req.node_id = leader_b;
        WriteAdmissionResult wrong_leader = controller.validateWrite(req);
        EXPECT_FALSE(wrong_leader.allowed);
        EXPECT_EQ(wrong_leader.status, Status::PERMISSION_DENIED);
        EXPECT_EQ(wrong_leader.reason, WriteAdmissionReason::NOT_CURRENT_LEADER);

        // Rotate leadership and fencing term; old-leader write path must stay fenced.
        state.leader_node_id = leader_b;
        state.leader_term = 8;
        ASSERT_EQ(controller.upsertShardLeaderState(state), Status::OK);

        req.node_id = leader_b;
        req.fencing_token.leader_term = 7;
        WriteAdmissionResult rotated_stale = controller.validateWrite(req);
        EXPECT_FALSE(rotated_stale.allowed);
        EXPECT_EQ(rotated_stale.reason, WriteAdmissionReason::STALE_FENCING_TOKEN);

        req.fencing_token.leader_term = 8;
        WriteAdmissionResult rotated_allow = controller.validateWrite(req);
        EXPECT_TRUE(rotated_allow.allowed);
        EXPECT_EQ(rotated_allow.status, Status::OK);
    }

    TEST(ClusterWriteFencingTest, RoutingEpochMustMatchPinnedWritePathEpoch)
    {
        ClusterWriteSafetyController controller;
        const ID shard_id = generateUuidV7();
        const ID leader = generateUuidV7();

        ShardLeaderState state{};
        state.shard_id = shard_id;
        state.leader_node_id = leader;
        state.leader_term = 3;
        state.routing_epoch = 91;
        ASSERT_EQ(controller.upsertShardLeaderState(state), Status::OK);

        WriteAdmissionRequest req{};
        req.shard_id = shard_id;
        req.node_id = leader;
        req.fencing_token.shard_id = shard_id;
        req.fencing_token.leader_term = 3;
        req.has_routing_epoch = true;
        req.routing_epoch = 90;

        WriteAdmissionResult stale_epoch = controller.validateWrite(req);
        EXPECT_FALSE(stale_epoch.allowed);
        EXPECT_EQ(stale_epoch.status, Status::INVALID_TRANSACTION_STATE);
        EXPECT_EQ(stale_epoch.reason, WriteAdmissionReason::ROUTING_EPOCH_MISMATCH);
        EXPECT_EQ(stale_epoch.expected_routing_epoch, 91u);
    }
} // namespace scratchbird::core
