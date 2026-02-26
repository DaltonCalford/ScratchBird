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

#include "scratchbird/security/auth_plugin_manager.h"

namespace {

std::filesystem::path repoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

}  // namespace

TEST(AuthPluginPamTest, PolicyRegistersMethod) {
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

    EXPECT_TRUE(manager.hasPlugin("scratchbird.auth.pam"));
    EXPECT_TRUE(manager.isMethodAvailable("scratchbird.auth.pam_conversation"));
}
