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
#include <functional>
#include <memory>
#include <vector>
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    void appendByte(std::vector<uint8_t>& out, uint8_t value)
    {
        out.push_back(value);
    }

    void appendInt16(std::vector<uint8_t>& out, uint16_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint16_t));
        scratchbird::sblr::writeInt16(&out[offset], value);
    }

    void appendInt32(std::vector<uint8_t>& out, uint32_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint32_t));
        scratchbird::sblr::writeInt32(&out[offset], value);
    }

    void appendUVarint(std::vector<uint8_t>& out, uint64_t value)
    {
        size_t offset = out.size();
        out.resize(offset + 10);
        size_t count = scratchbird::sblr::writeUVarint(&out[offset], value);
        out.resize(offset + count);
    }

    void appendString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendUVarint(out, static_cast<uint64_t>(value.size()));
        out.insert(out.end(), value.begin(), value.end());
    }

    void appendId(std::vector<uint8_t>& out, const ID& id)
    {
        out.insert(out.end(), id.bytes.begin(), id.bytes.end());
    }

    void appendExtendedOpcode(std::vector<uint8_t>& out, ExtendedOpcode opcode)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
        appendInt16(out, static_cast<uint16_t>(opcode));
    }

    void appendLiteralString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::LITERAL_STRING));
        appendString(out, value);
    }

    std::vector<uint8_t> buildSelectBytecode(
        uint32_t select_count,
        const std::function<void(std::vector<uint8_t>&)>& emit)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::SELECT));
        appendByte(bytecode, 0); // flags
        appendByte(bytecode, static_cast<uint8_t>(Opcode::BEGIN_LIST));
        appendUVarint(bytecode, select_count);
        emit(bytecode);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END_LIST));
        appendByte(bytecode, static_cast<uint8_t>(Opcode::BEGIN_LIST));
        appendUVarint(bytecode, 0);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END_LIST));
        return bytecode;
    }
}

class DomainSecurityIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("domain_security", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        domain_mgr_ = db_.domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", schema_info, &ctx), Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        compiler_ = std::make_unique<QueryCompilerV3>(&db_);
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(&db_);
        executor_->setCurrentSchema(schema_id_);
        Status conn_status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(conn_status, Status::OK) << ctx.message;
        connection_ctx_->setCurrentUser(catalog_->getSystemUserId(&ctx), true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        auto create_domain = compileAndExecute(
            "CREATE DOMAIN secure_ssn AS TEXT WITH SECURITY ("
            "MASKING = PARTIAL, MASK_PATTERN = 'XXX-XX-####', "
            "AUDIT_ACCESS = TRUE, REQUIRE_PRIVILEGE = SELECT)");
        ASSERT_TRUE(create_domain.success()) << create_domain.error();

        DomainInfo domain_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "secure_ssn", domain_info, &ctx),
                  Status::OK) << ctx.message;
        domain_id_ = domain_info.domain_id;

        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.ordinal = 0;

        CatalogManager::ColumnInfo ssn_col;
        ssn_col.column_id = generateUuidV7();
        ssn_col.column_name = "ssn";
        ssn_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        ssn_col.nullable = false;
        ssn_col.ordinal = 1;
        ssn_col.domain_id = domain_id_;
        column_id_ = ssn_col.column_id;

        std::vector<CatalogManager::ColumnInfo> columns{id_col, ssn_col};
        ASSERT_EQ(catalog_->createTable(schema_id_, "secure_people", columns,
                                        table_id_, 0, &ctx),
                  Status::OK) << ctx.message;

        Status status = catalog_->createUser("masked_user", "", schema_id_, false, user_id_, &ctx);
        if (status == Status::FILE_EXISTS)
        {
            CatalogManager::UserInfo user_info;
            ASSERT_EQ(catalog_->getUserByName("masked_user", user_info, &ctx),
                      Status::OK) << ctx.message;
            user_id_ = user_info.user_id;
        }
        else
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        db_file_.reset();
    }

    ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_{};
    ID domain_id_{};
    ID table_id_{};
    ID column_id_{};
    ID user_id_{};
    std::unique_ptr<QueryCompilerV3> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(DomainSecurityIntegrationTest, MasksWithoutPrivilege)
{
    TypedValue value = TypedValue::makeText("123-45-6789");
    TypedValue masked;
    ErrorContext ctx;

    ASSERT_EQ(domain_mgr_->applyMasking(domain_id_, user_id_, value, masked, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(masked.toString(), "***-**-6789");
}

TEST_F(DomainSecurityIntegrationTest, BypassesMaskingWithPrivilege)
{
    ErrorContext ctx;
    ID grantor_id = catalog_->getSystemUserId(&ctx);
    uint32_t privileges = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
    ASSERT_EQ(catalog_->grantPermission(domain_id_, CatalogManager::PermissionObjectType::DOMAIN,
                                        user_id_, CatalogManager::GranteeType::USER,
                                        privileges, false, grantor_id, &ctx),
              Status::OK) << ctx.message;

    bool has_privilege = false;
    ASSERT_EQ(domain_mgr_->checkMaskingPrivilege(domain_id_, user_id_, has_privilege, &ctx),
              Status::OK) << ctx.message;
    ASSERT_TRUE(has_privilege);

    TypedValue value = TypedValue::makeText("123-45-6789");
    TypedValue masked;
    ASSERT_EQ(domain_mgr_->applyMasking(domain_id_, user_id_, value, masked, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(masked.toString(), "123-45-6789");
}

TEST_F(DomainSecurityIntegrationTest, AuditsDomainAccess)
{
    auto* audit_logger = db_.audit_logger();
    ASSERT_NE(audit_logger, nullptr);

    uint64_t before = audit_logger->getTotalEventCount();

    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "audit");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_AUDIT_DOMAIN_ACCESS);
        appendId(out, domain_id_);
        appendId(out, user_id_);
        appendId(out, table_id_);
        appendId(out, column_id_);
        appendString(out, "");
    });

    auto result = executor_->execute(bytecode);
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t after = audit_logger->getTotalEventCount();
    ASSERT_GT(after, before);

    AuditQuery query;
    query.event_type = AuditEventType::DOMAIN_ACCESS;
    query.object_name = "secure_ssn";

    std::vector<AuditEvent> events;
    ErrorContext ctx;
    ASSERT_EQ(audit_logger->queryAuditLog(query, events, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().object_name, "secure_ssn");
}
