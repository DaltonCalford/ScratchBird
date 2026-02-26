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
#include <set>
#include <string>
#include <vector>

#include "scratchbird/security/auth_plugin_manager.h"

namespace {

std::filesystem::path repoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

}  // namespace

TEST(AuthPluginAdmissionTest, LoadsExpectedPhase1PluginSet) {
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

    const std::set<std::string> expected_plugins = {
        "scratchbird.auth.trust_reject",
        "scratchbird.auth.password_compat",
        "scratchbird.auth.scram",
        "scratchbird.auth.token_authkey",
        "scratchbird.auth.peer",
        "scratchbird.auth.certificate_mtls",
        "scratchbird.auth.jwt_oidc",
        "scratchbird.auth.webauthn",
        "scratchbird.auth.factor_chain",
        "scratchbird.auth.workload_identity",
        "scratchbird.auth.oauth_validator",
        "scratchbird.auth.proxy_assertion",
        "scratchbird.auth.ldap",
        "scratchbird.auth.kerberos",
        "scratchbird.auth.ident",
        "scratchbird.auth.radius",
        "scratchbird.auth.pam",
    };

    EXPECT_TRUE(manager.missingRequiredPlugins().empty());
    for (const auto& plugin_id : expected_plugins) {
        EXPECT_TRUE(manager.hasPlugin(plugin_id)) << plugin_id;
    }
}

TEST(AuthPluginAdmissionTest, Phase1PluginLayoutExists) {
    const std::filesystem::path plugin_root = repoRoot() / "src" / "security" / "plugins";
    const std::vector<std::string> plugin_dirs = {
        "trust_reject",
        "password_compat",
        "scram",
        "token_authkey",
        "peer",
        "certificate_mtls",
        "jwt_oidc",
        "webauthn",
        "factor_chain",
        "workload_identity",
        "oauth_validator",
        "proxy_assertion",
        "ldap",
        "kerberos",
        "ident",
        "radius",
        "pam",
    };

    for (const auto& dir : plugin_dirs) {
        const std::filesystem::path plugin_cpp = plugin_root / dir / (dir + "_plugin.cpp");
        EXPECT_TRUE(std::filesystem::exists(plugin_cpp)) << plugin_cpp.string();
    }
}
