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

#include <memory>

#include "test_helpers.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"

namespace scratchbird::core
{
    TEST(CatalogSessionEpochPinningTest, SessionPinsPersistAndValidateWithReplanOrReject)
    {
        using scratchbird::testing::TestDatabaseFile;

        TestDatabaseFile db_file("catalog_session_epoch_pinning");
        ASSERT_EQ(Database::create(db_file.path(), 16384), Status::OK);

        Database db;
        ASSERT_EQ(db.open(db_file.path()), Status::OK);

        std::unique_ptr<ConnectionContext> conn;
        ASSERT_EQ(db.connect(conn), Status::OK);
        ConnectionContext::setCurrent(conn.get());

        CatalogManager* catalog = db.catalog_manager();
        ASSERT_NE(catalog, nullptr);

        ErrorContext ctx;
        const ID system_user_id = catalog->getSystemUserId(&ctx);
        ASSERT_NE(system_user_id, ID{}) << ctx.message;

        CatalogManager::SessionInfo session{};
        ASSERT_EQ(catalog->createSession(system_user_id, ID{}, "native", session, &ctx), Status::OK)
            << ctx.message;
        EXPECT_EQ(session.cluster_config_epoch, db.cluster_config_epoch());
        EXPECT_EQ(session.schema_epoch, 0u);
        EXPECT_EQ(session.security_epoch, session.policy_epoch_global);

        constexpr uint64_t kClusterEpoch = 12;
        constexpr uint64_t kSchemaEpoch = 34;
        constexpr uint64_t kSecurityEpoch = 56;
        ASSERT_EQ(catalog->setSessionEpochPins(session.session_id, kClusterEpoch, kSchemaEpoch, kSecurityEpoch, &ctx),
                  Status::OK) << ctx.message;

        CatalogManager::SessionInfo pinned{};
        ASSERT_EQ(catalog->getSession(session.session_id, pinned, &ctx), Status::OK) << ctx.message;
        EXPECT_EQ(pinned.cluster_config_epoch, kClusterEpoch);
        EXPECT_EQ(pinned.schema_epoch, kSchemaEpoch);
        EXPECT_EQ(pinned.security_epoch, kSecurityEpoch);

        CatalogManager::SessionEpochValidation validation{};
        EXPECT_EQ(catalog->validateSessionEpochPins(session.session_id,
                                                    kClusterEpoch,
                                                    kSchemaEpoch,
                                                    kSecurityEpoch,
                                                    true,
                                                    validation,
                                                    &ctx),
                  Status::OK);
        EXPECT_TRUE(validation.valid);

        EXPECT_EQ(catalog->validateSessionEpochPins(session.session_id,
                                                    kClusterEpoch,
                                                    kSchemaEpoch + 1,
                                                    kSecurityEpoch,
                                                    false,
                                                    validation,
                                                    &ctx),
                  Status::OK);
        EXPECT_FALSE(validation.valid);
        EXPECT_TRUE(validation.requires_replan);
        EXPECT_EQ(validation.reason_code, "schema_epoch_mismatch");

        EXPECT_EQ(catalog->validateSessionEpochPins(session.session_id,
                                                    kClusterEpoch,
                                                    kSchemaEpoch + 1,
                                                    kSecurityEpoch,
                                                    true,
                                                    validation,
                                                    &ctx),
                  Status::INVALID_TRANSACTION_STATE);

        ConnectionContext::setCurrent(nullptr);
        conn.reset();
        db.close();
    }
} // namespace scratchbird::core
