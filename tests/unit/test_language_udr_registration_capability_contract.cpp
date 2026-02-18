/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/vnext_error_codes.h"
#include "scratchbird/udr/language_udr_runtime.h"

namespace
{
    using scratchbird::core::ErrorContext;
    using scratchbird::core::Status;
    using scratchbird::core::generateUuidV7;
    using scratchbird::udr::LanguageUdrCompileRequest;
    using scratchbird::udr::LanguageUdrModuleStatus;
    using scratchbird::udr::LanguageUdrRegistration;
    using scratchbird::udr::LanguageUdrRegistry;
    using scratchbird::udr::LanguageUdrRuntimeBoundary;
    using scratchbird::udr::LanguageUdrSignatureStatus;

    auto makeRegistration(const std::string &profile_id,
                          const std::string &profile_version,
                          const std::string &module_semver,
                          const std::string &module_name) -> LanguageUdrRegistration
    {
        LanguageUdrRegistration reg{};
        reg.module_id = generateUuidV7();
        reg.module_name = module_name;
        reg.engine_profile_id = profile_id;
        reg.engine_profile_version = profile_version;
        reg.translation_mode = "SQL_REWRITE_TO_NATIVE";
        reg.module_semver = module_semver;
        reg.artifact_hash = "artifact_hash_" + module_name;
        reg.signature_status = LanguageUdrSignatureStatus::TRUSTED;
        reg.status = LanguageUdrModuleStatus::ACTIVE;
        return reg;
    }

    auto makeRequest(const std::string &profile_id,
                     const std::string &profile_version,
                     const std::string &feature_key) -> LanguageUdrCompileRequest
    {
        LanguageUdrCompileRequest req{};
        req.request_id = generateUuidV7();
        req.profile_id = profile_id;
        req.profile_version = profile_version;
        req.payload_format = "SQL_TEXT";
        req.payload = std::vector<uint8_t>{'s', 'e', 'l', 'e', 'c', 't'};
        req.session_option_signature = "sess_sig";
        req.principal_id = generateUuidV7();
        req.role_context_signature = "role_sig";
        req.transaction_id = 42;
        req.catalog_epoch = 10;
        req.security_epoch = 11;
        req.native_feature_key = feature_key;
        req.compile_permission_granted = true;
        return req;
    }
} // namespace

TEST(LanguageUdrRegistrationCapabilityContractTest, ResolveRejectsUnknownProfileId)
{
    LanguageUdrRegistry registry;
    LanguageUdrRegistration selected{};
    ErrorContext ctx;

    const Status status = registry.resolveActiveModule("unknown", "3.0", selected, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "UDR_1501");
    EXPECT_TRUE(scratchbird::core::statusMatchesVNextErrorCode(status, ctx.vnext_code));
}

TEST(LanguageUdrRegistrationCapabilityContractTest, ResolveRejectsModuleNotInstalledForProfileVersion)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.1", "3.1.0", "mod_native_31");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrRegistration selected{};
    const Status status = registry.resolveActiveModule("native", "3.0", selected, &ctx);
    EXPECT_EQ(status, Status::NOT_FOUND);
    EXPECT_EQ(ctx.vnext_code, "UDR_1502");
    EXPECT_TRUE(scratchbird::core::statusMatchesVNextErrorCode(status, ctx.vnext_code));
}

TEST(LanguageUdrRegistrationCapabilityContractTest, ResolveRejectsIncompatibleModuleMajorVersion)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "4.0.0", "mod_native_v4");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrRegistration selected{};
    const Status status = registry.resolveActiveModule("native", "3.0", selected, &ctx);
    EXPECT_EQ(status, Status::NOT_SUPPORTED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1503");
    EXPECT_TRUE(scratchbird::core::statusMatchesVNextErrorCode(status, ctx.vnext_code));
}

TEST(LanguageUdrRegistrationCapabilityContractTest, ResolveSelectsHighestSemverWithMatchingCapabilityHash)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration low = makeRegistration("native", "3.0", "3.1.1", "mod_low");
    LanguageUdrRegistration high = makeRegistration("native", "3.0", "3.2.0", "mod_high");
    std::vector<std::string> features{"select_projection", "vector_search"};

    ASSERT_EQ(registry.registerModule(low, features, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(registry.registerModule(high, features, &ctx), Status::OK) << ctx.message;

    LanguageUdrRegistration selected{};
    ASSERT_EQ(registry.resolveActiveModule("native", "3.0", selected, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(selected.module_id, high.module_id);
}

TEST(LanguageUdrRegistrationCapabilityContractTest, CapabilityGateRejectsDisabledFeatureDeterministically)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.5", "mod_features");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    const Status status = registry.ensureFeatureEnabled(
        "native", "3.0", "create_database", &ctx);
    EXPECT_EQ(status, Status::NOT_SUPPORTED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1504");
    EXPECT_TRUE(scratchbird::core::statusMatchesVNextErrorCode(status, ctx.vnext_code));
}

TEST(LanguageUdrRegistrationCapabilityContractTest, ModuleRevocationIsOneWay)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.1", "mod_rev");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(registry.setModuleStatus(reg.module_id, LanguageUdrModuleStatus::DISABLED, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(registry.setModuleStatus(reg.module_id, LanguageUdrModuleStatus::REVOKED, &ctx), Status::OK)
        << ctx.message;

    const Status status = registry.setModuleStatus(reg.module_id, LanguageUdrModuleStatus::ACTIVE, &ctx);
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);
    EXPECT_EQ(ctx.vnext_code, "UDR_1503");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, CompileRequestSchemaValidationRejectsMalformedEnvelope)
{
    LanguageUdrCompileRequest bad{};
    bad.request_id = generateUuidV7();
    bad.profile_id = "native";

    ErrorContext ctx;
    const Status status = LanguageUdrRuntimeBoundary::validateCompileRequest(bad, &ctx);
    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_EQ(ctx.vnext_code, "UDR_1506");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsSecurityBeforeCapabilityChecks)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.2", "mod_security");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "create_database");
    req.compile_permission_granted = false;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::PERMISSION_DENIED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1507");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightSucceedsWithValidProfileModuleAndFeature)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_ok");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection", "create_database"}, &ctx), Status::OK)
        << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "create_database");
    LanguageUdrRegistration selected{};
    ASSERT_EQ(LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(selected.module_id, reg.module_id);
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsSandboxNetworkAccessByDefaultPolicy)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_sandbox_net");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "select_projection");
    req.requires_network_access = true;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::PERMISSION_DENIED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1508");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsSandboxFilesystemWriteByDefaultPolicy)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_sandbox_fs");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "select_projection");
    req.requires_filesystem_write = true;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::PERMISSION_DENIED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1508");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsPayloadQuotaDeterministically)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_quota_payload");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "select_projection");
    req.resource_limits.max_payload_bytes = 2;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1512");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsAstQuotaDeterministically)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_quota_ast");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "select_projection");
    req.payload = std::vector<uint8_t>{'s','e','l','e','c','t',' ','a',',','b',' ','f','r','o','m',' ','t',
                                       ' ','w','h','e','r','e',' ','a','=','1',' ','a','n','d',' ','b','=','2'};
    req.resource_limits.max_ast_node_count = 2;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1512");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsNormalizationQuotaDeterministically)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_quota_norm");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "select_projection");
    req.resource_limits.max_normalization_steps = 1;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1512");
}

TEST(LanguageUdrRegistrationCapabilityContractTest, PreflightRejectsCompileWallTimeQuotaDeterministically)
{
    LanguageUdrRegistry registry;
    ErrorContext ctx;

    LanguageUdrRegistration reg = makeRegistration("native", "3.0", "3.0.9", "mod_quota_time");
    ASSERT_EQ(registry.registerModule(reg, {"select_projection"}, &ctx), Status::OK) << ctx.message;

    LanguageUdrCompileRequest req = makeRequest("native", "3.0", "select_projection");
    req.payload.assign(4096, static_cast<uint8_t>('x'));
    req.resource_limits.max_compile_wall_time_ms = 1;

    LanguageUdrRegistration selected{};
    const Status status = LanguageUdrRuntimeBoundary::preflightCompile(registry, req, selected, &ctx);
    EXPECT_EQ(status, Status::CONFIGURATION_LIMIT_EXCEEDED);
    EXPECT_EQ(ctx.vnext_code, "UDR_1512");
}
