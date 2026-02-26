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
#include <string>
#include <vector>

#include "scratchbird/security/auth_plugin_manager.h"

namespace {

std::filesystem::path repoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

}  // namespace

TEST(AuthPluginEnterpriseMatrixTest, P2MethodsResolveInPolicyRegistrySet) {
    scratchbird::security::AuthPluginManager manager;
    scratchbird::security::AuthPluginManagerConfig config =
        scratchbird::security::AuthPluginManagerConfig::defaults();

    const std::filesystem::path root = repoRoot();
    config.truststore_path =
        (root / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.policy_path =
        (root / "etc" / "auth" / "auth_plugins.policy.json.example").string();

    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(manager.initialize(config, &ctx), scratchbird::core::Status::OK) << ctx.message;

    struct Case {
        scratchbird::security::AuthType auth_type;
        const char* plugin_id;
        const char* method_id;
    };

    const std::vector<Case> cases = {
        {scratchbird::security::AuthType::LDAP,
         "scratchbird.auth.ldap",
         "scratchbird.auth.ldap_bind"},
        {scratchbird::security::AuthType::KERBEROS,
         "scratchbird.auth.kerberos",
         "scratchbird.auth.kerberos_gssapi"},
        {scratchbird::security::AuthType::IDENT,
         "scratchbird.auth.ident",
         "scratchbird.auth.ident_rfc1413"},
        {scratchbird::security::AuthType::RADIUS,
         "scratchbird.auth.radius",
         "scratchbird.auth.radius_pap"},
        {scratchbird::security::AuthType::PAM,
         "scratchbird.auth.pam",
         "scratchbird.auth.pam_conversation"},
    };

    for (const auto& c : cases) {
        std::string method_id;
        ASSERT_TRUE(manager.resolveMethodIdForAuthType(c.auth_type, method_id));
        EXPECT_EQ(method_id, c.method_id);
        EXPECT_TRUE(manager.hasPlugin(c.plugin_id));
        EXPECT_TRUE(manager.isMethodAvailable(c.method_id));
    }
}
