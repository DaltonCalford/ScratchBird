#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

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

        compiler_ = std::make_unique<QueryCompilerV2>(db_.get());
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
    std::unique_ptr<QueryCompilerV2> compiler_;
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

    auto reset_role_result = compiler_->compile("SET ROLE NONE");
    ASSERT_TRUE(reset_role_result.success());
    exec_result = executor_->execute(reset_role_result.bytecode());
    ASSERT_TRUE(exec_result.success()) << exec_result.error();
    EXPECT_EQ(conn_ctx_->getCurrentSchemaId(), user_schema_id);
}
