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

namespace scratchbird::core
{
    TEST(SessionEpochPinsTest, MatchingEpochsPassValidation)
    {
        SessionEpochPins pinned{};
        pinned.cluster_config_epoch = 10;
        pinned.schema_epoch = 20;
        pinned.security_epoch = 30;

        SessionEpochValidationResult result =
            validateSessionEpochPins(pinned, pinned, SessionEpochMismatchPolicy::REJECT);

        EXPECT_TRUE(result.valid);
        EXPECT_EQ(result.status, Status::OK);
        EXPECT_EQ(result.reason, SessionEpochReason::NONE);
        EXPECT_EQ(result.action, SessionEpochAction::NONE);
    }

    TEST(SessionEpochPinsTest, MismatchCanForceReplanInsteadOfReject)
    {
        SessionEpochPins pinned{};
        pinned.cluster_config_epoch = 100;
        pinned.schema_epoch = 200;
        pinned.security_epoch = 300;

        SessionEpochPins current = pinned;
        current.cluster_config_epoch = 101;

        SessionEpochValidationResult replan_result =
            validateSessionEpochPins(pinned, current, SessionEpochMismatchPolicy::REPLAN);
        EXPECT_FALSE(replan_result.valid);
        EXPECT_EQ(replan_result.status, Status::OK);
        EXPECT_EQ(replan_result.reason, SessionEpochReason::CLUSTER_CONFIG_EPOCH_MISMATCH);
        EXPECT_EQ(replan_result.action, SessionEpochAction::REPLAN);

        current = pinned;
        current.security_epoch = 301;
        SessionEpochValidationResult reject_result =
            validateSessionEpochPins(pinned, current, SessionEpochMismatchPolicy::REJECT);
        EXPECT_FALSE(reject_result.valid);
        EXPECT_EQ(reject_result.status, Status::INVALID_TRANSACTION_STATE);
        EXPECT_EQ(reject_result.reason, SessionEpochReason::SECURITY_EPOCH_MISMATCH);
        EXPECT_EQ(reject_result.action, SessionEpochAction::REJECT);
    }
} // namespace scratchbird::core
