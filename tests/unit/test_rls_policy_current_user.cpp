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

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/native_sql_renderer.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::core;
using namespace scratchbird::sblr;

namespace
{
std::vector<uint8_t> decodeHexBytes(const std::string& hex_str)
{
    std::vector<uint8_t> bytes;
    size_t start = 0;
    if (hex_str.size() >= 2 && hex_str[0] == '0' && hex_str[1] == 'x')
    {
        start = 2;
    }
    if (((hex_str.size() - start) % 2) != 0)
    {
        return {};
    }
    bytes.reserve((hex_str.size() - start) / 2);
    auto hex_to_nibble = [](char c) -> int {
        if (c >= '0' && c <= '9')
        {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f')
        {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F')
        {
            return c - 'A' + 10;
        }
        return -1;
    };
    for (size_t i = start; i < hex_str.size(); i += 2)
    {
        const int hi = hex_to_nibble(hex_str[i]);
        const int lo = hex_to_nibble(hex_str[i + 1]);
        if (hi < 0 || lo < 0)
        {
            return {};
        }
        bytes.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return bytes;
}

std::string opcodeNameOrUnknown(uint16_t opcode)
{
    const char* name = scratchbird::sblr::v3::opcodeName(opcode);
    return name == nullptr ? std::string("<unknown>") : std::string(name);
}

const scratchbird::sblr::v3::Instruction* findNestedInstruction(
    const scratchbird::sblr::v3::Instruction& parent,
    const char* field_name)
{
    const auto* obj =
        std::get_if<scratchbird::sblr::v3::Value::Object>(&parent.payload.data);
    if (obj == nullptr)
    {
        return nullptr;
    }
    auto it = obj->find(field_name);
    if (it == obj->end())
    {
        return nullptr;
    }
    const auto* ptr =
        std::get_if<scratchbird::sblr::v3::Value::InstrPtr>(&it->second.data);
    if (ptr == nullptr || !*ptr)
    {
        return nullptr;
    }
    return ptr->get();
}

class RlsPolicyCurrentUserTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_path_ = scratchbird::testing::uniqueTestDbPath("test_rls_policy_current_user");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        CatalogManager::SchemaInfo public_schema;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", public_schema, &ctx), Status::OK)
            << ctx.message;
        public_schema_id_ = public_schema.schema_id;

        ASSERT_EQ(db_.connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        conn_ctx_->setCurrentSchemaId(public_schema_id_);
        conn_ctx_->setCurrentUser(catalog_->getSystemUserId(&ctx), true);
        ConnectionContext::setCurrent(conn_ctx_.get());

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        compiler_->setCurrentSchema(public_schema_id_);

        executor_ = std::make_unique<Executor>(&db_);
        executor_->setCurrentSchema(public_schema_id_);
        executor_->setConnectionContext(conn_ctx_.get());
    }

    void TearDown() override
    {
        compiler_.reset();
        executor_.reset();
        ConnectionContext::setCurrent(nullptr);
        conn_ctx_.reset();
        db_.close();
        std::filesystem::remove(db_path_);
        std::filesystem::remove(db_path_ + "-lock");
    }

    ExecutionResult executeSql(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                if (!errors.empty())
                {
                    errors += '\n';
                }
                errors += err;
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    CatalogManager::TableInfo getTable(const std::string& table_name)
    {
        CatalogManager::TableInfo info;
        ErrorContext ctx;
        EXPECT_EQ(catalog_->getTable(public_schema_id_, table_name, info, &ctx), Status::OK)
            << ctx.message;
        return info;
    }

    std::string db_path_;
    Database db_;
    CatalogManager* catalog_ = nullptr;
    ID public_schema_id_;
    std::unique_ptr<ConnectionContext> conn_ctx_;
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
};
} // namespace

TEST_F(RlsPolicyCurrentUserTest,
       StoredCurrentUserPolicyEvaluatesForOwnedRowAndFiltersSelect)
{
    ExecutionResult current_user_result = executeSql("SELECT CURRENT_USER");
    ASSERT_TRUE(current_user_result.success()) << current_user_result.error();
    ASSERT_NE(current_user_result.resultSet(), nullptr);
    ASSERT_EQ(current_user_result.resultSet()->rowCount(), 1u);
    const std::string current_user =
        current_user_result.resultSet()->getValue(0, 0).toString();
    ASSERT_FALSE(current_user.empty());

    ASSERT_TRUE(executeSql(
                    "CREATE TABLE docs_rls_current_user ("
                    "id INTEGER PRIMARY KEY, "
                    "owner_name VARCHAR(64), "
                    "payload VARCHAR(32))")
                    .success());
    ASSERT_TRUE(executeSql(
                    "INSERT INTO docs_rls_current_user (id, owner_name, payload) "
                    "VALUES (1, CURRENT_USER, 'mine')")
                    .success());
    ASSERT_TRUE(executeSql(
                    "INSERT INTO docs_rls_current_user (id, owner_name, payload) "
                    "VALUES (2, 'someone_else', 'other')")
                    .success());
    ASSERT_TRUE(
        executeSql("ALTER TABLE docs_rls_current_user ENABLE ROW LEVEL SECURITY").success());
    ASSERT_TRUE(
        executeSql("ALTER TABLE docs_rls_current_user FORCE ROW LEVEL SECURITY").success());
    ASSERT_TRUE(executeSql(
                    "CREATE POLICY docs_rls_current_user_policy ON docs_rls_current_user "
                    "FOR SELECT TO PUBLIC USING (owner_name = CURRENT_USER)")
                    .success());

    CatalogManager::TableInfo table_info = getTable("docs_rls_current_user");

    CatalogManager::PolicyInfo policy_info;
    ErrorContext ctx;
    ASSERT_EQ(catalog_->getPolicy(table_info.table_id,
                                  "docs_rls_current_user_policy",
                                  policy_info,
                                  &ctx),
              Status::OK)
        << ctx.message;
    ASSERT_FALSE(policy_info.using_expr.empty());

    std::vector<uint8_t> expr_bytes = decodeHexBytes(policy_info.using_expr);
    ASSERT_FALSE(expr_bytes.empty());

    scratchbird::sblr::v3::Instruction decoded;
    scratchbird::sblr::v3::DecodeError derr;
    size_t offset = 0;
    ASSERT_TRUE(scratchbird::sblr::v3::decodeInstructionWithSchema(
                    expr_bytes.data(), expr_bytes.size(), offset, decoded, derr))
        << derr.message;
    ASSERT_EQ(offset, expr_bytes.size());

    EXPECT_EQ(decoded.opcode,
              static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_EXPR_EQ))
        << opcodeNameOrUnknown(decoded.opcode);

    const auto* lhs = findNestedInstruction(decoded, "lhs");
    const auto* rhs = findNestedInstruction(decoded, "rhs");
    ASSERT_NE(lhs, nullptr);
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(lhs->opcode,
              static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_COLUMN_REF))
        << opcodeNameOrUnknown(lhs->opcode);
    EXPECT_TRUE(
        rhs->opcode ==
            static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_FUNC_CURRENT_USER) ||
        rhs->opcode ==
            static_cast<uint16_t>(scratchbird::sblr::v3::Opcode::SBLR3_EXPR_FUNCTION_CALL))
        << opcodeNameOrUnknown(rhs->opcode);

    ExecutionResult direct_probe = executeSql(
        "SELECT id, owner_name, owner_name = CURRENT_USER "
        "FROM docs_rls_current_user WHERE id = 1");
    ASSERT_TRUE(direct_probe.success()) << direct_probe.error();
    ASSERT_NE(direct_probe.resultSet(), nullptr);
    ASSERT_EQ(direct_probe.resultSet()->rowCount(), 1u);
    EXPECT_EQ(direct_probe.resultSet()->getValue(0, 1).toString(), current_user);
    EXPECT_TRUE(direct_probe.resultSet()->getValue(0, 2).toBoolean())
        << opcodeNameOrUnknown(rhs->opcode);

    ExecutionResult visible = executeSql(
        "SELECT id, owner_name, payload FROM docs_rls_current_user ORDER BY id");
    ASSERT_TRUE(visible.success()) << visible.error();
    ASSERT_NE(visible.resultSet(), nullptr);
    ASSERT_EQ(visible.resultSet()->rowCount(), 1u);
    EXPECT_EQ(visible.resultSet()->getValue(0, 0).toInt32(), 1);
    EXPECT_EQ(visible.resultSet()->getValue(0, 1).toString(), current_user);
    EXPECT_EQ(visible.resultSet()->getValue(0, 2).toString(), "mine");

    ExecutionResult counted = executeSql(
        "SELECT COUNT(*) FROM docs_rls_current_user");
    ASSERT_TRUE(counted.success()) << counted.error();
    ASSERT_NE(counted.resultSet(), nullptr);
    ASSERT_EQ(counted.resultSet()->rowCount(), 1u);
    EXPECT_EQ(counted.resultSet()->getValue(0, 0).toInt64(), 1);

    ExecutionResult tagged_count = executeSql(
        "SELECT 'ASSERT|sec_rls|with_rls_count|' || CAST(COUNT(*) AS VARCHAR(20)) "
        "FROM docs_rls_current_user");
    ASSERT_TRUE(tagged_count.success()) << tagged_count.error();
    ASSERT_NE(tagged_count.resultSet(), nullptr);
    ASSERT_EQ(tagged_count.resultSet()->rowCount(), 1u);
    EXPECT_EQ(tagged_count.resultSet()->getValue(0, 0).toString(),
              "ASSERT|sec_rls|with_rls_count|1");
}
