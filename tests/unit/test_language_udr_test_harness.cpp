/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/udr/language_udr_runtime.h"
#include "scratchbird/udr/language_udr_test_harness.h"

namespace
{
    using scratchbird::core::ErrorContext;
    using scratchbird::core::Status;
    using scratchbird::core::generateUuidV7;
    using scratchbird::udr::LanguageUdrArtifactPreference;
    using scratchbird::udr::LanguageUdrCompileRequest;
    using scratchbird::udr::LanguageUdrCompileResponse;
    using scratchbird::udr::LanguageUdrDiagnosticSeverity;
    using scratchbird::udr::LanguageUdrModuleStatus;
    using scratchbird::udr::LanguageUdrNativeArtifactStatus;
    using scratchbird::udr::LanguageUdrNativeCompileOptions;
    using scratchbird::udr::LanguageUdrRegistration;
    using scratchbird::udr::LanguageUdrRegistry;
    using scratchbird::udr::LanguageUdrResultShape;
    using scratchbird::udr::LanguageUdrSignatureStatus;
    using scratchbird::udr::LanguageUdrTestHarness;

    auto makeRegistration() -> LanguageUdrRegistration
    {
        LanguageUdrRegistration reg{};
        reg.module_id = generateUuidV7();
        reg.module_name = "test_udr_module";
        reg.engine_profile_id = "native";
        reg.engine_profile_version = "3.0";
        reg.translation_mode = "SQL_REWRITE_TO_NATIVE";
        reg.module_semver = "3.0.1";
        reg.artifact_hash = "module_hash";
        reg.signature_status = LanguageUdrSignatureStatus::TRUSTED;
        reg.status = LanguageUdrModuleStatus::ACTIVE;
        return reg;
    }

    auto makeRequest(const std::string &feature_key) -> LanguageUdrCompileRequest
    {
        LanguageUdrCompileRequest req{};
        req.request_id = generateUuidV7();
        req.profile_id = "native";
        req.profile_version = "3.0";
        req.payload_format = "SQL_TEXT";
        req.payload = std::vector<uint8_t>{'S', 'E', 'L', 'E', 'C', 'T', ' ', '1'};
        req.session_option_signature = "sess_sig";
        req.principal_id = generateUuidV7();
        req.role_context_signature = "role_sig";
        req.transaction_id = 100;
        req.catalog_epoch = 5;
        req.security_epoch = 7;
        req.native_feature_key = feature_key;
        req.compile_permission_granted = true;
        return req;
    }
} // namespace

TEST(LanguageUdrTestHarnessContractTest, CompileSingleIsDeterministic)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrCompileRequest request = makeRequest("compile_embedded_payload");

    LanguageUdrCompileResponse r1{};
    LanguageUdrCompileResponse r2{};
    ASSERT_EQ(harness.compileSingle(request, r1, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(harness.compileSingle(request, r2, &ctx), Status::OK) << ctx.message;

    EXPECT_TRUE(r1.success);
    EXPECT_EQ(r1.result_shape, LanguageUdrResultShape::UDR_RS_COMPILE_OK);
    EXPECT_EQ(r1.normalized_payload_hash, r2.normalized_payload_hash);
    EXPECT_EQ(r1.native_ast_hash, r2.native_ast_hash);
    EXPECT_EQ(r1.sblr_hash, r2.sblr_hash);
    EXPECT_EQ(r1.sblr_payload, r2.sblr_payload);
}

TEST(LanguageUdrTestHarnessContractTest, ValidateOnlyReturnsValidateShapeAndNoSblrPayload)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrCompileResponse response{};
    ASSERT_EQ(harness.validateOnly(makeRequest("compile_embedded_payload"), response, &ctx), Status::OK)
        << ctx.message;

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_VALIDATE_OK);
    EXPECT_TRUE(response.sblr_payload.empty());
}

TEST(LanguageUdrTestHarnessContractTest, CompileBatchPreservesRequestOrderAndPerItemRejects)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    std::vector<LanguageUdrCompileRequest> requests{
        makeRequest("compile_embedded_payload"),
        makeRequest("unsupported_feature")};

    std::vector<LanguageUdrCompileResponse> responses;
    ASSERT_EQ(harness.compileBatch(requests, responses, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(responses.size(), 2u);

    EXPECT_EQ(responses[0].request_id, requests[0].request_id);
    EXPECT_TRUE(responses[0].success);
    EXPECT_EQ(responses[1].request_id, requests[1].request_id);
    EXPECT_FALSE(responses[1].success);
    ASSERT_FALSE(responses[1].diagnostics.empty());
    EXPECT_EQ(responses[1].diagnostics.front().code, "UDR_1504");
}

TEST(LanguageUdrTestHarnessContractTest, NativeOptionalGeneratesSortedArtifacts)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrNativeCompileOptions options{};
    options.artifact_preference = LanguageUdrArtifactPreference::NATIVE_PREFERRED;
    options.target_triples = {
        "x86_64-pc-windows-msvc",
        "x86_64-pc-linux-gnu"};
    options.host_api_abi_version = "SB_HOST_API_V1";
    options.optimization_level = "O2";
    options.allow_interpreter_fallback = true;

    LanguageUdrCompileResponse response{};
    ASSERT_EQ(harness.compileNativeOptional(makeRequest("compile_embedded_payload"), options, response, &ctx), Status::OK)
        << ctx.message;

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_COMPILE_OK_NATIVE);
    EXPECT_EQ(response.native_artifact_status, LanguageUdrNativeArtifactStatus::GENERATED);
    ASSERT_EQ(response.native_artifacts.size(), 2u);
    EXPECT_EQ(response.native_artifacts[0].target_triple, "x86_64-pc-linux-gnu");
    EXPECT_EQ(response.native_artifacts[1].target_triple, "x86_64-pc-windows-msvc");
}

TEST(LanguageUdrTestHarnessContractTest, NativeRequiredRejectsUnsupportedTarget)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrNativeCompileOptions options{};
    options.artifact_preference = LanguageUdrArtifactPreference::NATIVE_REQUIRED;
    options.target_triples = {"aarch64-unknown-linux-gnu"};
    options.host_api_abi_version = "SB_HOST_API_V1";
    options.allow_interpreter_fallback = false;

    LanguageUdrCompileResponse response{};
    const Status status = harness.compileNativeOptional(
        makeRequest("compile_embedded_payload"), options, response, &ctx);
    EXPECT_EQ(status, Status::NOT_SUPPORTED);
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_NATIVE_EXECUTE_REJECT);
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1517");
}

TEST(LanguageUdrTestHarnessContractTest, NativePreferredFallsBackOnAbiMismatch)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrNativeCompileOptions options{};
    options.artifact_preference = LanguageUdrArtifactPreference::NATIVE_PREFERRED;
    options.target_triples = {"x86_64-pc-linux-gnu"};
    options.host_api_abi_version = "SB_HOST_API_V2";
    options.allow_interpreter_fallback = true;

    LanguageUdrCompileResponse response{};
    ASSERT_EQ(harness.compileNativeOptional(makeRequest("compile_embedded_payload"), options, response, &ctx), Status::OK)
        << ctx.message;

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_NATIVE_FALLBACK_OK);
    EXPECT_EQ(response.native_artifact_status,
              LanguageUdrNativeArtifactStatus::FALLBACK_SBLR_ONLY);
    EXPECT_EQ(response.fallback_reason_code, "NAT_FALLBACK_ABI_MISMATCH");
    EXPECT_TRUE(response.native_artifacts.empty());
}

TEST(LanguageUdrTestHarnessContractTest, CompileSingleRejectsSandboxViolationAsCompileRejectShape)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrCompileRequest request = makeRequest("compile_embedded_payload");
    request.requires_network_access = true;

    LanguageUdrCompileResponse response{};
    const Status status = harness.compileSingle(request, response, &ctx);
    EXPECT_EQ(status, Status::PERMISSION_DENIED);
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_COMPILE_REJECT);
    EXPECT_TRUE(response.sblr_payload.empty());
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1508");
    EXPECT_EQ(response.diagnostics.front().severity, LanguageUdrDiagnosticSeverity::ERROR);
}

TEST(LanguageUdrTestHarnessContractTest, CompileSingleRejectsQuotaViolationAsCompileRejectShape)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;
    ASSERT_EQ(registry.registerModule(makeRegistration(), {"compile_embedded_payload"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrTestHarness harness(registry);
    LanguageUdrCompileRequest request = makeRequest("compile_embedded_payload");
    request.resource_limits.max_payload_bytes = 2;

    LanguageUdrCompileResponse response{};
    const Status status = harness.compileSingle(request, response, &ctx);
    EXPECT_EQ(status, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.result_shape, LanguageUdrResultShape::UDR_RS_COMPILE_REJECT);
    EXPECT_TRUE(response.sblr_payload.empty());
    ASSERT_FALSE(response.diagnostics.empty());
    EXPECT_EQ(response.diagnostics.front().code, "UDR_1512");
    EXPECT_EQ(response.diagnostics.front().severity, LanguageUdrDiagnosticSeverity::ERROR);
}
