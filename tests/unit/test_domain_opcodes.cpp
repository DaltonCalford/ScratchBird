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
#include <chrono>
#include <cstdlib>
#include <functional>
#include <vector>
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "test_helpers.h"
#include "test_user_helpers.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace {
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
    appendByte(out, static_cast<uint8_t>(scratchbird::sblr::Opcode::EXTENDED_OPCODE));
    appendInt16(out, static_cast<uint16_t>(opcode));
}

void appendLiteralString(std::vector<uint8_t>& out, const std::string& value)
{
    appendByte(out, static_cast<uint8_t>(scratchbird::sblr::Opcode::LITERAL_STRING));
    appendString(out, value);
}

void appendLiteralNull(std::vector<uint8_t>& out)
{
    appendByte(out, static_cast<uint8_t>(scratchbird::sblr::Opcode::LITERAL_NULL));
}

void appendLiteralInt32(std::vector<uint8_t>& out, int32_t value)
{
    appendByte(out, static_cast<uint8_t>(scratchbird::sblr::Opcode::LITERAL_INT32));
    appendInt32(out, static_cast<uint32_t>(value));
}

std::vector<uint8_t> buildSelectBytecode(
    uint32_t select_count,
    const std::function<void(std::vector<uint8_t>&)>& emit)
{
    std::vector<uint8_t> bytecode;
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    appendByte(bytecode, SBLR_VERSION);
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::SELECT));
    appendByte(bytecode, 0); // flags
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::BEGIN_LIST));
    appendUVarint(bytecode, select_count);
    emit(bytecode);
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::END_LIST));
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::BEGIN_LIST));
    appendUVarint(bytecode, 0);
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::END_LIST));
    return bytecode;
}

std::vector<uint8_t> buildConstantReturnFunction(const std::string& value)
{
    std::vector<uint8_t> expr;
    appendLiteralString(expr, value);

    std::vector<uint8_t> bytecode;
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    appendByte(bytecode, SBLR_VERSION);
    appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
    appendByte(bytecode, 1);
    appendInt32(bytecode, static_cast<uint32_t>(expr.size()));
    bytecode.insert(bytecode.end(), expr.begin(), expr.end());
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::END));
    return bytecode;
}

std::vector<uint8_t> buildBooleanReturnFunction(const std::string& param_name,
                                                bool return_true)
{
    std::vector<uint8_t> expr;
    appendExtendedOpcode(expr, ExtendedOpcode::EXT_VAR_LOAD);
    appendString(expr, param_name);
    appendExtendedOpcode(expr, ExtendedOpcode::EXT_VAR_LOAD);
    appendString(expr, param_name);
    appendByte(expr, static_cast<uint8_t>(return_true
                                              ? scratchbird::sblr::Opcode::EXPR_EQ
                                              : scratchbird::sblr::Opcode::EXPR_NE));

    std::vector<uint8_t> bytecode;
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::VERSION));
    appendByte(bytecode, SBLR_VERSION);
    appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
    appendByte(bytecode, 1);
    appendInt32(bytecode, static_cast<uint32_t>(expr.size()));
    bytecode.insert(bytecode.end(), expr.begin(), expr.end());
    appendByte(bytecode, static_cast<uint8_t>(scratchbird::sblr::Opcode::END));
    return bytecode;
}

void registerFunction(CatalogManager* catalog,
                      const ID& schema_id,
                      const ID& owner_id,
                      const std::string& name,
                      DataType return_type,
                      const std::vector<CatalogManager::ParameterInfo>& params,
                      const std::vector<uint8_t>& bytecode)
{
    CatalogManager::FunctionInfo info;
    info.function_id = generateUuidV7();
    info.schema_id = schema_id;
    info.name = name;
    info.owner_id = owner_id;
    info.parameters = params;
    info.return_type = return_type;
    info.bytecode = bytecode;
    auto now = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
    info.created_time = now;
    info.modified_time = now;

    ErrorContext ctx;
    Status status = catalog->registerFunction(info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
}
} // namespace

class DomainOpcodeTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (const char* existing = std::getenv("SCRATCHBIRD_ENABLE_LEGACY_EXECUTE"))
        {
            legacy_env_had_prev_ = true;
            legacy_env_prev_ = existing;
        }
        setenv("SCRATCHBIRD_ENABLE_LEGACY_EXECUTE", "1", 1);

        db_file_ = std::make_unique<TestDatabaseFile>("domain_opcodes", ".sbdb");
        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);
        domain_mgr_ = db_.domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

        ASSERT_EQ(db_.connect(conn_, &ctx), Status::OK) << ctx.message;
        executor_ = std::make_unique<Executor>(&db_);
        executor_->setConnectionContext(conn_.get());

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", schema_info, &ctx), Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        ID domain_id;
        ASSERT_EQ(domain_mgr_->createBasicDomain(schema_id_, "opcode_domain",
                                                 DataType::VARCHAR, 0, 0,
                                                 false, "", {},
                                                 domain_id, &ctx),
                  Status::OK) << ctx.message;
        domain_id_ = domain_id;

        DomainSecurity security;
        security.masking_config.type = MaskingType::FULL;
        security.masking_config.full_mask_char = "*";
        security.encryption_enabled = true;
        security.encryption_algorithm = EncryptionAlgorithm::AES256_GCM;
        security.audit_enabled = true;
        security.required_privilege_for_unmasked = "SELECT";
        ASSERT_EQ(domain_mgr_->setSecurityOptions(domain_id_, security, &ctx),
                  Status::OK) << ctx.message;

        DomainIntegrity integrity;
        integrity.uniqueness_check = true;
        integrity.normalization_enabled = true;
        integrity.normalization_function = "LOWERCASE";
        ASSERT_EQ(domain_mgr_->setIntegrityOptions(domain_id_, integrity, &ctx),
                  Status::OK) << ctx.message;

        EnsureUser(catalog_, "masked_user", schema_id_);
        CatalogManager::UserInfo user_info;
        ASSERT_EQ(catalog_->getUserByName("masked_user", user_info, &ctx), Status::OK)
            << ctx.message;
        user_id_ = user_info.user_id;

        table_id_ = generateUuidV7();
        column_id_ = generateUuidV7();

        CatalogManager::ParameterInfo param;
        param.name = "value";
        param.type = DataType::VARCHAR;
        param.mode = CatalogManager::ParameterMode::IN;

        ID owner_id = catalog_->getSystemUserId(&ctx);
        registerFunction(catalog_, schema_id_, owner_id, "always_valid",
                         DataType::BOOLEAN, {param},
                         buildBooleanReturnFunction("value", true));
        registerFunction(catalog_, schema_id_, owner_id, "always_invalid",
                         DataType::BOOLEAN, {param},
                         buildBooleanReturnFunction("value", false));
        registerFunction(catalog_, schema_id_, owner_id, "parse_const",
                         DataType::TEXT, {param},
                         buildConstantReturnFunction("parsed"));
        registerFunction(catalog_, schema_id_, owner_id, "standardize_const",
                         DataType::TEXT, {param},
                         buildConstantReturnFunction("standardized"));
        registerFunction(catalog_, schema_id_, owner_id, "enrich_const",
                         DataType::TEXT, {param},
                         buildConstantReturnFunction(
                             "{\"value\":\"enriched\",\"metadata\":{\"source\":\"unit\"}}"));
    }

    void TearDown() override
    {
        if (legacy_env_had_prev_)
        {
            setenv("SCRATCHBIRD_ENABLE_LEGACY_EXECUTE",
                   legacy_env_prev_.c_str(),
                   1);
        }
        else
        {
            unsetenv("SCRATCHBIRD_ENABLE_LEGACY_EXECUTE");
        }
    }

    ExecutionResult executeBytecode(const std::vector<uint8_t>& bytecode)
    {
        return executor_->execute(bytecode);
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<ConnectionContext> conn_;
    std::unique_ptr<Executor> executor_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_{};
    ID domain_id_{};
    ID user_id_{};
    ID table_id_{};
    ID column_id_{};
    bool legacy_env_had_prev_ = false;
    std::string legacy_env_prev_;
};

TEST_F(DomainOpcodeTest, CheckDomainConstraintPasses)
{
    auto bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "ok");
        appendString(out, "");

        appendLiteralString(out, "ok");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_CHECK_DOMAIN_CONSTRAINT);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_TRUE(rs->getValue(0, 1).getBool());
}

TEST_F(DomainOpcodeTest, CheckDomainConstraintFailsOnNull)
{
    auto bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralNull(out);
        appendString(out, "");

        appendLiteralNull(out);
        appendExtendedOpcode(out, ExtendedOpcode::EXT_CHECK_DOMAIN_CONSTRAINT);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    ASSERT_TRUE(result.hasResultSet());
    auto* rs = result.resultSet();
    ASSERT_EQ(rs->rowCount(), 1u);
    EXPECT_FALSE(rs->getValue(0, 1).getBool());
}

TEST_F(DomainOpcodeTest, ApplyDomainMasking)
{
    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "secret");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_APPLY_DOMAIN_MASKING);
        appendId(out, domain_id_);
        appendId(out, user_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "******");
}

TEST_F(DomainOpcodeTest, CheckDomainPrivilege)
{
    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendExtendedOpcode(out, ExtendedOpcode::EXT_CHECK_DOMAIN_PRIVILEGE);
        appendId(out, domain_id_);
        appendId(out, user_id_);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_FALSE(result.resultSet()->getValue(0, 0).getBool());

    ErrorContext ctx;
    ID grantor_id = catalog_->getSystemUserId(&ctx);
    ASSERT_EQ(catalog_->grantPermission(domain_id_,
                                        CatalogManager::PermissionObjectType::DOMAIN,
                                        user_id_,
                                        CatalogManager::GranteeType::USER,
                                        static_cast<uint32_t>(CatalogManager::Privilege::SELECT),
                                        false,
                                        grantor_id,
                                        &ctx),
              Status::OK) << ctx.message;

    result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_TRUE(result.resultSet()->getValue(0, 0).getBool());
}

TEST_F(DomainOpcodeTest, EncryptAndDecryptDomainValue)
{
    auto bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "secret");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_ENCRYPT_DOMAIN_VALUE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");

        appendLiteralString(out, "secret");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_ENCRYPT_DOMAIN_VALUE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendExtendedOpcode(out, ExtendedOpcode::EXT_DECRYPT_DOMAIN_VALUE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_TRUE(rs->getValue(0, 0).isEncrypted());
    EXPECT_EQ(rs->getValue(0, 1).toString(), "secret");
}

TEST_F(DomainOpcodeTest, NormalizeDomainValue)
{
    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "MiXeD");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_NORMALIZE_DOMAIN_VALUE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "mixed");
}

TEST_F(DomainOpcodeTest, ValidateDomainValueFalse)
{
    DomainValidationConfig validation;
    validation.validation_function = "always_invalid";
    ErrorContext ctx;
    ASSERT_EQ(domain_mgr_->setValidationOptions(domain_id_, validation, &ctx),
              Status::OK) << ctx.message;

    auto bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "value");
        appendString(out, "");

        appendLiteralString(out, "value");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_VALIDATE_DOMAIN_VALUE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_FALSE(rs->getValue(0, 1).getBool());
}

TEST_F(DomainOpcodeTest, ValidateDomainValueTrue)
{
    DomainValidationConfig validation;
    validation.validation_function = "always_valid";
    ErrorContext ctx;
    ASSERT_EQ(domain_mgr_->setValidationOptions(domain_id_, validation, &ctx),
              Status::OK) << ctx.message;

    auto bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "value");
        appendString(out, "");

        appendLiteralString(out, "value");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_VALIDATE_DOMAIN_VALUE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_TRUE(rs->getValue(0, 1).getBool());
}

TEST_F(DomainOpcodeTest, ApplyQualityPipeline)
{
    DomainQuality quality;
    quality.parse_function = "parse_const";
    quality.standardize_function = "standardize_const";
    quality.enrich_function = "enrich_const";
    ErrorContext ctx;
    ASSERT_EQ(domain_mgr_->setQualityOptions(domain_id_, quality, &ctx),
              Status::OK) << ctx.message;

    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "raw");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_APPLY_QUALITY_PIPELINE);
        appendId(out, domain_id_);
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->getValue(0, 0).toString(), "enriched");
}

TEST_F(DomainOpcodeTest, CheckGlobalUniqueness)
{
    Value existing = Value::makeVarchar("dup");
    ErrorContext ctx;
    uint64_t tx_id = conn_->getCurrentXid();
    TID row_tid(0, 1, 1);
    ASSERT_EQ(domain_mgr_->registerUniqueValue(domain_id_, table_id_, column_id_,
                                               row_tid, existing, tx_id, &ctx),
              Status::OK) << ctx.message;

    auto bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "dup");
        appendString(out, "");

        appendLiteralString(out, "dup");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_CHECK_GLOBAL_UNIQUENESS);
        appendId(out, domain_id_);
        appendId(out, table_id_);
        appendId(out, column_id_);
        appendId(out, generateUuidV7());
        appendInt16(out, 0);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    auto* rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_FALSE(rs->getValue(0, 1).getBool());

    auto unique_bytecode = buildSelectBytecode(2, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "unique");
        appendString(out, "");

        appendLiteralString(out, "unique");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_CHECK_GLOBAL_UNIQUENESS);
        appendId(out, domain_id_);
        appendId(out, table_id_);
        appendId(out, column_id_);
        appendId(out, generateUuidV7());
        appendInt16(out, 0);
        appendString(out, "");
    });

    result = executeBytecode(unique_bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    rs = result.resultSet();
    ASSERT_NE(rs, nullptr);
    EXPECT_TRUE(rs->getValue(0, 1).getBool());
}

TEST_F(DomainOpcodeTest, AuditDomainAccessLogged)
{
    auto* audit_logger = db_.audit_logger();
    ASSERT_NE(audit_logger, nullptr);
    uint64_t before = audit_logger->getTotalEventCount();

    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendLiteralInt32(out, 1);
        appendExtendedOpcode(out, ExtendedOpcode::EXT_AUDIT_DOMAIN_ACCESS);
        appendId(out, domain_id_);
        appendId(out, user_id_);
        appendId(out, table_id_);
        appendId(out, column_id_);
        appendString(out, "");
    });

    auto result = executeBytecode(bytecode);
    ASSERT_TRUE(result.success()) << result.error();
    EXPECT_GE(audit_logger->getTotalEventCount(), before + 1);
}
