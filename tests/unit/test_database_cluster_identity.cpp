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

#include "test_helpers.h"
#include "scratchbird/core/database.h"

namespace scratchbird::core
{
    using scratchbird::testing::TestDatabaseFile;

    TEST(DatabaseClusterIdentityTest, DefaultsToStandaloneIdentity)
    {
        TestDatabaseFile db_file("database_cluster_identity_default");
        ASSERT_EQ(Database::create(db_file.path(), 16384), Status::OK);

        Database db;
        ASSERT_EQ(db.open(db_file.path()), Status::OK);
        EXPECT_EQ(db.cluster_id(), ID{});
        EXPECT_EQ(db.node_id(), ID{});
        EXPECT_EQ(db.cluster_config_epoch(), 0u);
        db.close();
    }

    TEST(DatabaseClusterIdentityTest, PersistsClusterIdentityAcrossRestart)
    {
        TestDatabaseFile db_file("database_cluster_identity_persist");
        ASSERT_EQ(Database::create(db_file.path(), 16384), Status::OK);

        const ID cluster_id = generateUuidV7();
        const ID node_id = generateUuidV7();
        constexpr uint64_t kClusterEpoch = 44;

        {
            Database db;
            ASSERT_EQ(db.open(db_file.path()), Status::OK);
            ASSERT_EQ(db.set_cluster_identity(cluster_id, node_id, kClusterEpoch), Status::OK);
            db.close();
        }

        {
            Database reopened;
            ASSERT_EQ(reopened.open(db_file.path()), Status::OK);
            EXPECT_EQ(reopened.cluster_id(), cluster_id);
            EXPECT_EQ(reopened.node_id(), node_id);
            EXPECT_EQ(reopened.cluster_config_epoch(), kClusterEpoch);
            reopened.close();
        }
    }
} // namespace scratchbird::core
