/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/database.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/udr/language_udr_sql_render_endpoint.h"
#include "test_helpers.h"

using scratchbird::core::Database;
using scratchbird::core::ErrorContext;
using scratchbird::core::ID;
using scratchbird::core::Status;
using scratchbird::core::generateUuidV7;
using scratchbird::sblr::QueryCompilerV3;
using scratchbird::udr::LanguageUdrModuleStatus;
using scratchbird::udr::LanguageUdrRegistration;
using scratchbird::udr::LanguageUdrRegistry;
using scratchbird::udr::LanguageUdrSblrSqlRenderRequest;
using scratchbird::udr::LanguageUdrSblrSqlRenderResponse;
using scratchbird::udr::LanguageUdrSignatureStatus;
using scratchbird::udr::renderSblrToNativeSqlEndpoint;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    auto registerModule(LanguageUdrRegistry &registry,
                        bool include_render_feature,
                        const std::string &profile_version = "1.0",
                        LanguageUdrModuleStatus status = LanguageUdrModuleStatus::ACTIVE) -> ID
    {
        LanguageUdrRegistration registration{};
        registration.module_id = generateUuidV7();
        registration.module_name = "udr_render_native";
        registration.engine_profile_id = "native";
        registration.engine_profile_version = profile_version;
        registration.translation_mode = "SQL_REWRITE_TO_NATIVE";
        registration.module_semver = "1.0.0";
        registration.artifact_hash = "artifact_native_render";
        registration.signature_status = LanguageUdrSignatureStatus::TRUSTED;
        registration.status = status;

        std::vector<std::string> features;
        if (include_render_feature)
        {
            features.push_back("sblr_to_native_sql_render");
        }
        else
        {
            features.push_back("compile_route");
        }

        ErrorContext ctx;
        const Status register_status = registry.registerModule(registration, features, &ctx);
        if (register_status != Status::OK)
        {
            ADD_FAILURE() << "registerModule failed: " << ctx.message;
            return ID{};
        }
        return registration.module_id;
    }
}

class LanguageUdrSblrSqlRenderEndpointContractTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("language_udr_sblr_sql_render_endpoint");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        compiler_ = std::make_unique<QueryCompilerV3>(db_.get());
    }

    auto compileRootInstruction(const std::string &sql) -> scratchbird::sblr::v3::Instruction
    {
        scratchbird::sblr::v3::Instruction root{};
        auto compiled = compiler_->compile(sql);
        if (!compiled.success())
        {
            for (const auto &error : compiled.errors())
            {
                ADD_FAILURE() << "compile error: " << error << " sql=" << sql;
            }
            return root;
        }

        scratchbird::sblr::v3::Container container;
        std::string decode_error;
        if (!scratchbird::sblr::v3::decodeContainer(
                compiled.bytecode().data(),
                compiled.bytecode().size(),
                container,
                decode_error))
        {
            ADD_FAILURE() << "decodeContainer failed: " << decode_error;
            return root;
        }

        size_t offset = 0;
        scratchbird::sblr::v3::DecodeError err;
        scratchbird::sblr::v3::Instruction version{};
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
                container.bytecode_stream.data(),
                container.bytecode_stream.size(),
                offset,
                version,
                err))
        {
            ADD_FAILURE() << "decode version instruction failed: " << err.message;
            return root;
        }
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
                container.bytecode_stream.data(),
                container.bytecode_stream.size(),
                offset,
                root,
                err))
        {
            ADD_FAILURE() << "decode root instruction failed: " << err.message;
            return root;
        }
        return root;
    }

    auto makeRequest(const scratchbird::sblr::v3::Instruction &instruction) const
        -> LanguageUdrSblrSqlRenderRequest
    {
        LanguageUdrSblrSqlRenderRequest request{};
        request.request_id = generateUuidV7();
        request.profile_id = "native";
        request.profile_version = "1.0";
        request.native_feature_key = "sblr_to_native_sql_render";
        request.principal_id = generateUuidV7();
        request.role_context_signature = "role_sig";
        request.render_permission_granted = true;
        request.root_instruction = instruction;
        return request;
    }

    std::unique_ptr<TestDatabaseFile> db_file_;
    std::unique_ptr<Database> db_;
    std::unique_ptr<QueryCompilerV3> compiler_;
};

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRendersDeterministicSqlForKnownOpcode)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true);
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");

    auto request = makeRequest(root);
    LanguageUdrSblrSqlRenderResponse first{};
    LanguageUdrSblrSqlRenderResponse second{};
    ErrorContext ctx;

    ASSERT_EQ(renderSblrToNativeSqlEndpoint(registry, request, first, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(first.success);
    EXPECT_EQ(first.contract_id, std::string("NRSQL-006-UDR-COMPILE"));
    EXPECT_EQ(first.canonical_opcode_symbol, std::string("OP_COMPAT_OP_UDR_COMPILE_DISPATCH"));
    EXPECT_FALSE(first.sql_text.empty());

    ASSERT_EQ(renderSblrToNativeSqlEndpoint(registry, request, second, &ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(second.success);
    EXPECT_EQ(first.sql_text, second.sql_text);
    EXPECT_EQ(first.contract_id, second.contract_id);
    EXPECT_EQ(first.canonical_opcode_symbol, second.canonical_opcode_symbol);
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsWhenFeatureNotEnabled)
{
    LanguageUdrRegistry registry;
    registerModule(registry, false);
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");
    auto request = makeRequest(root);

    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::NOT_SUPPORTED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1504");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1504");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsWithoutRenderPermission)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true);
    auto root = compileRootInstruction(
        "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_a SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_t");
    auto request = makeRequest(root);
    request.render_permission_granted = false;

    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::PERMISSION_DENIED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1507");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1507");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsUnrenderableInstructionWithUdr1510)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true);
    scratchbird::sblr::v3::Instruction invalid{};
    invalid.opcode = 0xFFFF;
    invalid.flags = 0;
    invalid.payload = scratchbird::sblr::v3::Value(
        scratchbird::sblr::v3::Value::Bytes{});

    auto request = makeRequest(invalid);
    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::SYNTAX_ERROR);
    EXPECT_EQ(ctx.vnext_code, "UDR_1510");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1510");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsMalformedEnvelopeWithUdr1506)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true);
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");
    auto request = makeRequest(root);
    request.request_id = ID{};

    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "UDR_1506");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1506");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsUnknownProfileWithUdr1501)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true);
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");
    auto request = makeRequest(root);
    request.profile_id = "unknown_profile";

    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "UDR_1501");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1501");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsMissingProfileVersionModuleWithUdr1502)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true, "1.0");
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");
    auto request = makeRequest(root);
    request.profile_version = "9.0";

    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
    EXPECT_EQ(ctx.vnext_code, "UDR_1502");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1502");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsMalformedProfileVersionWithUdr1506)
{
    LanguageUdrRegistry registry;
    registerModule(registry, true, "bad_version");
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");
    auto request = makeRequest(root);
    request.profile_version = "bad_version";

    LanguageUdrSblrSqlRenderResponse response{};
    ErrorContext ctx;
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "UDR_1506");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1506");
}

TEST_F(LanguageUdrSblrSqlRenderEndpointContractTest, EndpointRejectsDisabledModuleWithUdr1502)
{
    LanguageUdrRegistry registry;
    const ID module_id = registerModule(registry, true);
    auto root = compileRootInstruction(
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_x SESSION_SIGNATURE sig_x");
    auto request = makeRequest(root);

    ErrorContext ctx;
    ASSERT_EQ(registry.setModuleStatus(module_id, LanguageUdrModuleStatus::DISABLED, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrSblrSqlRenderResponse response{};
    const Status status = renderSblrToNativeSqlEndpoint(registry, request, response, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
    EXPECT_EQ(ctx.vnext_code, "UDR_1502");
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1502");
}
