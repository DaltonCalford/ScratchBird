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

#include <vector>

#include "scratchbird/core/cluster_write_safety.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    TEST(MultiShardWriteGuardTest, RejectsCrossShardWritesWhenPolicyDisallowsThem)
    {
        const ID shard_a = generateUuidV7();
        const ID shard_b = generateUuidV7();
        std::vector<ID> write_set{shard_a, shard_b};

        MultiShardGuardPolicy policy{};
        policy.allow_cross_shard = false;
        policy.require_explicit_override = true;

        MultiShardGuardResult result = evaluateMultiShardWrite(write_set, policy, false);
        EXPECT_FALSE(result.allowed);
        EXPECT_EQ(result.status, Status::PERMISSION_DENIED);
        EXPECT_EQ(result.reason, MultiShardGuardReason::MULTI_SHARD_WRITE_NOT_ALLOWED);
        EXPECT_EQ(result.unique_shard_count, 2u);
    }

    TEST(MultiShardWriteGuardTest, ExplicitOverrideCanPermitCrossShardWrites)
    {
        const ID shard_a = generateUuidV7();
        const ID shard_b = generateUuidV7();
        std::vector<ID> write_set{shard_a, shard_b, shard_a};

        MultiShardGuardPolicy policy{};
        policy.allow_cross_shard = true;
        policy.require_explicit_override = true;

        MultiShardGuardResult no_override = evaluateMultiShardWrite(write_set, policy, false);
        EXPECT_FALSE(no_override.allowed);
        EXPECT_EQ(no_override.reason, MultiShardGuardReason::MULTI_SHARD_WRITE_REQUIRES_OVERRIDE);

        MultiShardGuardResult with_override = evaluateMultiShardWrite(write_set, policy, true);
        EXPECT_TRUE(with_override.allowed);
        EXPECT_EQ(with_override.status, Status::OK);
        EXPECT_EQ(with_override.reason, MultiShardGuardReason::NONE);
    }
} // namespace scratchbird::core
