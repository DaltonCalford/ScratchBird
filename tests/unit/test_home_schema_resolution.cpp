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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

#include <cstring>

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    bool isZeroUuid(const ID& id)
    {
        for (auto byte : id.bytes)
        {
            if (byte != 0)
            {
                return false;
            }
        }
        return true;
    }

    int compareUuid(const ID& lhs, const ID& rhs)
    {
        return std::memcmp(lhs.bytes.data(), rhs.bytes.data(), lhs.bytes.size());
    }
} // namespace

class HomeSchemaResolutionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("home_schema_resolution");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("public", schema_info, &ctx), Status::OK) << ctx.message;
        public_schema_id_ = schema_info.schema_id;

        system_user_id_ = catalog_->getSystemUserId(&ctx);
        ASSERT_FALSE(isZeroUuid(system_user_id_));

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
        executor_ = std::make_unique<Executor>(db_.get());

        ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        conn_ctx_->setCurrentUser(system_user_id_, true);
        conn_ctx_->setCurrentSchemaId(public_schema_id_);
        executor_->setConnectionContext(conn_ctx_.get());
        ConnectionContext::setCurrent(conn_ctx_.get());
    }

    void TearDown() override
    {
        ConnectionContext::setCurrent(nullptr);
        executor_.reset();
        compiler_.reset();
        conn_ctx_.reset();
        db_.reset();
        db_file_.reset();
    }

    CatalogManager::SessionInfo createSessionForUser(const ID& user_id)
    {
        ErrorContext ctx;
        CatalogManager::SessionInfo session_info;
        auto status = catalog_->createSession(user_id, ID{}, "", session_info, &ctx);
        EXPECT_EQ(status, Status::OK) << ctx.message;
        return session_info;
    }

protected:
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_{};
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    ID public_schema_id_{};
    ID system_user_id_{};
};

TEST_F(HomeSchemaResolutionTest, SessionUsesGroupHomeWhenUserDefaultMissing)
{
    ErrorContext ctx;

    ID group_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("users.group_home",
                                         CatalogManager::SchemaType::USER_HOME,
                                         group_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID group_id;
    ASSERT_EQ(catalog_->createGroup("developers", CatalogManager::GroupType::LOCAL,
                                    "", group_schema_id, group_id, &ctx),
              Status::OK) << ctx.message;

    ID user_id;
    ASSERT_EQ(catalog_->createUser("bob", "", public_schema_id_, false, user_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->updateUser(user_id, "", ID{}, true, false, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->addGroupMember(group_id, user_id, false, system_user_id_, &ctx),
              Status::OK) << ctx.message;

    auto session_info = createSessionForUser(user_id);
    EXPECT_EQ(session_info.current_schema_id, group_schema_id);
}

TEST_F(HomeSchemaResolutionTest, SessionUsesRoleHomeBeforeGroupHome)
{
    ErrorContext ctx;

    ID role_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("users.roles.admin_role_home",
                                         CatalogManager::SchemaType::USER_HOME,
                                         role_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID group_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("users.groups.developers_home",
                                         CatalogManager::SchemaType::USER_HOME,
                                         group_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID role_id;
    ASSERT_EQ(catalog_->createRole("admin_role", system_user_id_,
                                   role_schema_id, role_id, &ctx),
              Status::OK) << ctx.message;

    ID group_id;
    ASSERT_EQ(catalog_->createGroup("developers", CatalogManager::GroupType::LOCAL,
                                    "", group_schema_id, group_id, &ctx),
              Status::OK) << ctx.message;

    ID user_id;
    ASSERT_EQ(catalog_->createUser("carol", "", public_schema_id_, false, user_id, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->updateUser(user_id, "", ID{}, true, false, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->grantRole(role_id, user_id, system_user_id_, false, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->addGroupMember(group_id, user_id, false, system_user_id_, &ctx),
              Status::OK) << ctx.message;

    auto session_info = createSessionForUser(user_id);
    EXPECT_EQ(session_info.current_schema_id, role_schema_id);
}

TEST_F(HomeSchemaResolutionTest, SessionGroupTieBreakUsesGroupUuidAscending)
{
    ErrorContext ctx;

    ID schema_a;
    ASSERT_EQ(catalog_->createSchemaPath("users.groups.g_a_home",
                                         CatalogManager::SchemaType::USER_HOME,
                                         schema_a, &ctx),
              Status::OK) << ctx.message;

    ID schema_b;
    ASSERT_EQ(catalog_->createSchemaPath("users.groups.g_b_home",
                                         CatalogManager::SchemaType::USER_HOME,
                                         schema_b, &ctx),
              Status::OK) << ctx.message;

    ID group_a;
    ASSERT_EQ(catalog_->createGroup("z_group", CatalogManager::GroupType::LOCAL,
                                    "", schema_a, group_a, &ctx),
              Status::OK) << ctx.message;

    ID group_b;
    ASSERT_EQ(catalog_->createGroup("a_group", CatalogManager::GroupType::LOCAL,
                                    "", schema_b, group_b, &ctx),
              Status::OK) << ctx.message;

    ID user_id;
    ASSERT_EQ(catalog_->createUser("dave", "", public_schema_id_, false, user_id, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->updateUser(user_id, "", ID{}, true, false, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->addGroupMember(group_a, user_id, false, system_user_id_, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->addGroupMember(group_b, user_id, false, system_user_id_, &ctx),
              Status::OK) << ctx.message;

    auto session_info = createSessionForUser(user_id);

    ID expected_schema = compareUuid(group_a, group_b) < 0 ? schema_a : schema_b;
    EXPECT_EQ(session_info.current_schema_id, expected_schema);
}

TEST_F(HomeSchemaResolutionTest, SessionSearchPathUsesPersistedProfile)
{
    ErrorContext ctx;

    ID user_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("users.eve",
                                         CatalogManager::SchemaType::USER_HOME,
                                         user_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID user_id;
    ASSERT_EQ(catalog_->createUser("eve", "", user_schema_id, false, user_id, &ctx),
              Status::OK) << ctx.message;

    auto session_1 = createSessionForUser(user_id);
    ASSERT_FALSE(session_1.search_path.empty());
    ASSERT_FALSE(isZeroUuid(session_1.search_path_profile_id));

    std::string expected_user_path;
    ASSERT_EQ(catalog_->getSchemaPath(user_schema_id, expected_user_path, &ctx), Status::OK)
        << ctx.message;
    ASSERT_FALSE(expected_user_path.empty());

    EXPECT_EQ(session_1.search_path.front(), expected_user_path);
    EXPECT_FALSE(session_1.search_path_schema_ids.empty());
    EXPECT_EQ(session_1.search_path_schema_ids.front(), user_schema_id);

    auto session_2 = createSessionForUser(user_id);
    EXPECT_EQ(session_2.search_path_profile_id, session_1.search_path_profile_id);
    EXPECT_EQ(session_2.search_path_schema_ids, session_1.search_path_schema_ids);
    EXPECT_EQ(session_2.search_path, session_1.search_path);
}

TEST_F(HomeSchemaResolutionTest, SetRoleSwitchesSchemaToRoleHome)
{
    ErrorContext ctx;

    ID user_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("users.alice",
                                         CatalogManager::SchemaType::USER_HOME,
                                         user_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID role_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("app.role_home",
                                         CatalogManager::SchemaType::APPLICATION,
                                         role_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID user_id;
    ASSERT_EQ(catalog_->createUser("alice", "", user_schema_id, false, user_id, &ctx),
              Status::OK) << ctx.message;

    ID role_id;
    ASSERT_EQ(catalog_->createRole("admin_role", system_user_id_,
                                   role_schema_id, role_id, &ctx),
              Status::OK) << ctx.message;

    ASSERT_EQ(catalog_->grantRole(role_id, user_id, system_user_id_, false, &ctx),
              Status::OK) << ctx.message;

    auto session_info = createSessionForUser(user_id);
    conn_ctx_.reset();
    ASSERT_EQ(db_->connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
    conn_ctx_->setCurrentUser(user_id, false);
    conn_ctx_->setCurrentSchemaId(session_info.current_schema_id);
    conn_ctx_->setSessionContext(session_info.session_id, ID{},
                                 session_info.emulation_mode,
                                 session_info.policy_epoch_global,
                                 session_info.policy_epoch_table);
    ConnectionContext::setCurrent(conn_ctx_.get());
    executor_->setConnectionContext(conn_ctx_.get());
    compiler_->setCurrentSchema(session_info.current_schema_id);

    auto set_role_result = compiler_->compile("SET ROLE admin_role");
    ASSERT_TRUE(set_role_result.success());
    auto exec_result = executor_->execute(set_role_result.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentSchemaId(), role_schema_id);

    auto reset_role_result = compiler_->compile("RESET ROLE");
    ASSERT_TRUE(reset_role_result.success());
    exec_result = executor_->execute(reset_role_result.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentSchemaId(), user_schema_id);

    auto set_role_none_result = compiler_->compile("SET ROLE NONE");
    ASSERT_TRUE(set_role_none_result.success());
    exec_result = executor_->execute(set_role_none_result.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentSchemaId(), user_schema_id);
}

TEST_F(HomeSchemaResolutionTest, SetSessionAuthorizationSwitchesAndResetsUserContext)
{
    ErrorContext ctx;
    const std::string switch_user_name = "v3_sa_switch_user";

    ID app_schema_id;
    ASSERT_EQ(catalog_->createSchemaPath("users.app_user",
                                         CatalogManager::SchemaType::USER_HOME,
                                         app_schema_id, &ctx),
              Status::OK) << ctx.message;

    ID app_user_id;
    auto create_status =
        catalog_->createUser(switch_user_name, "", app_schema_id, true, app_user_id, &ctx);
    if (create_status == Status::FILE_EXISTS)
    {
        CatalogManager::UserInfo existing_user;
        ASSERT_EQ(catalog_->getUserByName(switch_user_name, existing_user, &ctx),
                  Status::OK) << ctx.message;
        app_user_id = existing_user.user_id;
    }
    else
    {
        ASSERT_EQ(create_status, Status::OK) << ctx.message;
    }

    CatalogManager::UserInfo resolved_user;
    ASSERT_EQ(catalog_->getUserByName(switch_user_name, resolved_user, &ctx),
              Status::OK) << ctx.message;
    if (!resolved_user.is_superuser)
    {
        ASSERT_EQ(catalog_->updateUser(resolved_user.user_id, "", resolved_user.default_schema_id,
                                       resolved_user.is_active, true, &ctx),
                  Status::OK) << ctx.message;
        ASSERT_EQ(catalog_->getUserByName(switch_user_name, resolved_user, &ctx),
                  Status::OK) << ctx.message;
        ASSERT_TRUE(resolved_user.is_superuser);
    }

    auto set_session_auth = compiler_->compile("SET SESSION AUTHORIZATION v3_sa_switch_user");
    ASSERT_TRUE(set_session_auth.success());
    auto exec_result = executor_->execute(set_session_auth.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentUserId(), resolved_user.user_id);

    auto reset_session_auth = compiler_->compile("RESET SESSION AUTHORIZATION");
    ASSERT_TRUE(reset_session_auth.success());
    exec_result = executor_->execute(reset_session_auth.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentUserId(), system_user_id_);

    auto set_session_auth_default = compiler_->compile("SET SESSION AUTHORIZATION DEFAULT");
    ASSERT_TRUE(set_session_auth_default.success());
    exec_result = executor_->execute(set_session_auth_default.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentUserId(), system_user_id_);
}
