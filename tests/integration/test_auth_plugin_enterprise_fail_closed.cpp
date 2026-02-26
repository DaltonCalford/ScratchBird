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
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "scratchbird/security/auth_manager.h"

namespace {

std::filesystem::path repoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

class ScopedTempDir {
public:
    ScopedTempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("sb_auth_plugin_fail_closed_" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~ScopedTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace

TEST(AuthPluginEnterpriseFailClosedTest, RequiredUnknownEnterprisePluginFailsStartup) {
    ScopedTempDir temp;

    nlohmann::json policy{
        {"mode", "signed_only"},
        {"fail_on_unlisted_plugins", true},
        {"allowed_plugins",
         {
             {"scratchbird.auth.ldap.enterprise.required",
              {
                  {"required", true},
                  {"allowed_signers", nlohmann::json::array({"sb-enterprise-kid-2026"})},
                  {"allowed_method_ids", nlohmann::json::array({"scratchbird.auth.ldap_bind"})}
              }}
         }}
    };

    const auto policy_path = temp.path() / "auth_plugins.policy.json";
    {
        std::ofstream out(policy_path);
        out << policy.dump(2);
    }

    scratchbird::security::AuthManager manager;
    scratchbird::security::AuthManagerConfig config;
    config.auth_plugin_registry_enabled = true;
    config.allow_legacy_auth_fallback = false;
    config.auth_plugin_truststore_path =
        (repoRoot() / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.auth_plugin_policy_path = policy_path.string();
    config.auth_plugin_root = temp.path().string();

    scratchbird::core::ErrorContext ctx;
    EXPECT_EQ(manager.initialize(config, &ctx), scratchbird::core::Status::NOT_FOUND);
    EXPECT_NE(ctx.message.find("required plugin"), std::string::npos);
}
