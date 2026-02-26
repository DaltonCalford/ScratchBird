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
    TEST(DeterministicShardRouterTest, SameInputsProduceSameRouteDecision)
    {
        DeterministicShardRouter router;
        RoutingPlan plan{};
        plan.table_id = generateUuidV7();
        plan.routing_epoch = 17;
        plan.targets = {
            RoutingTarget{generateUuidV7(), generateUuidV7(), "node-a:7610", 1},
            RoutingTarget{generateUuidV7(), generateUuidV7(), "node-b:7610", 2},
            RoutingTarget{generateUuidV7(), generateUuidV7(), "node-c:7610", 3},
        };
        ASSERT_EQ(router.upsertPlan(plan), Status::OK);

        RoutingRequest req{};
        req.table_id = plan.table_id;
        req.shard_key = "tenant:alpha";
        req.has_expected_routing_epoch = true;
        req.expected_routing_epoch = 17;

        RoutingDecision first = router.route(req);
        RoutingDecision second = router.route(req);
        ASSERT_TRUE(first.routed);
        ASSERT_TRUE(second.routed);
        EXPECT_EQ(first.status, Status::OK);
        EXPECT_EQ(second.status, Status::OK);
        EXPECT_EQ(first.target.shard_id, second.target.shard_id);
        EXPECT_EQ(first.target.leader_node_id, second.target.leader_node_id);
        EXPECT_EQ(first.routing_epoch, 17u);
    }

    TEST(DeterministicShardRouterTest, RejectsStaleRoutingEpoch)
    {
        DeterministicShardRouter router;
        RoutingPlan plan{};
        plan.table_id = generateUuidV7();
        plan.routing_epoch = 99;
        plan.targets = {
            RoutingTarget{generateUuidV7(), generateUuidV7(), "node-a:7610", 1},
            RoutingTarget{generateUuidV7(), generateUuidV7(), "node-b:7610", 1},
        };
        ASSERT_EQ(router.upsertPlan(plan), Status::OK);

        RoutingRequest stale{};
        stale.table_id = plan.table_id;
        stale.shard_key = "user:42";
        stale.has_expected_routing_epoch = true;
        stale.expected_routing_epoch = 98;

        RoutingDecision stale_decision = router.route(stale);
        EXPECT_FALSE(stale_decision.routed);
        EXPECT_EQ(stale_decision.status, Status::INVALID_TRANSACTION_STATE);
        EXPECT_EQ(stale_decision.reason, RoutingDecisionReason::STALE_ROUTING_EPOCH);
        EXPECT_EQ(stale_decision.routing_epoch, 99u);
    }
} // namespace scratchbird::core
