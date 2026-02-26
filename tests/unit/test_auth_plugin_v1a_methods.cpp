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

#include <cstdlib>
#include <filesystem>
#include <string>

#include "scratchbird/security/auth_manager.h"
#include "scratchbird/security/auth_plugin_manager.h"

namespace {

std::filesystem::path repoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* key, const std::string& value)
        : key_(key), had_original_(false) {
        if (const char* current = std::getenv(key_)) {
            had_original_ = true;
            original_value_ = current;
        }
        set(value);
    }

    ~ScopedEnvVar() {
        if (had_original_) {
            set(original_value_);
        } else {
            unset();
        }
    }

private:
    void set(const std::string& value) {
#ifdef _WIN32
        _putenv_s(key_, value.c_str());
#else
        setenv(key_, value.c_str(), 1);
#endif
    }

    void unset() {
#ifdef _WIN32
        _putenv_s(key_, "");
#else
        unsetenv(key_);
#endif
    }

    const char* key_;
    bool had_original_;
    std::string original_value_;
};

}  // namespace

TEST(AuthPluginV1AMethodsTest, TokenAuthTypeMapsToAuthKeyMethodId) {
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

    std::string method_id;
    ASSERT_TRUE(manager.resolveMethodIdForAuthType(scratchbird::security::AuthType::TOKEN, method_id));
    EXPECT_EQ(method_id, "scratchbird.auth.authkey_token");
    EXPECT_TRUE(manager.isMethodAvailable(method_id));
}

TEST(AuthPluginV1AMethodsTest, EnterpriseP2AuthTypesMapToRegisteredMethods) {
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

    struct Mapping {
        scratchbird::security::AuthType auth_type;
        const char* method_id;
    };
    const Mapping mappings[] = {
        {scratchbird::security::AuthType::LDAP, "scratchbird.auth.ldap_bind"},
        {scratchbird::security::AuthType::KERBEROS, "scratchbird.auth.kerberos_gssapi"},
        {scratchbird::security::AuthType::IDENT, "scratchbird.auth.ident_rfc1413"},
        {scratchbird::security::AuthType::RADIUS, "scratchbird.auth.radius_pap"},
        {scratchbird::security::AuthType::PAM, "scratchbird.auth.pam_conversation"},
    };

    for (const auto& mapping : mappings) {
        std::string method_id;
        ASSERT_TRUE(manager.resolveMethodIdForAuthType(mapping.auth_type, method_id));
        EXPECT_EQ(method_id, mapping.method_id);
        EXPECT_TRUE(manager.isMethodAvailable(method_id)) << method_id;
    }
}

TEST(AuthPluginV1AMethodsTest, AuthManagerRejectsPinningViolation) {
    scratchbird::security::AuthManager manager;
    scratchbird::security::AuthManagerConfig config;
    config.hba_enabled = false;
    config.rate_limit_enabled = false;
    config.audit_enabled = false;
    config.auth_plugin_registry_enabled = false;
    config.default_auth_type = scratchbird::security::AuthType::TRUST;

    scratchbird::core::ErrorContext init_ctx;
    ASSERT_EQ(manager.initialize(config, &init_ctx), scratchbird::core::Status::OK)
        << init_ctx.message;

    scratchbird::security::AuthContext auth_ctx;
    scratchbird::security::ConnectionInfo conn;
    conn.protocol = "native";
    conn.database_name = "sb";
    conn.client_address = "127.0.0.1";
    auth_ctx.setConnectionInfo(conn);
    auth_ctx.setUsername("alice");

    ScopedEnvVar required("SCRATCHBIRD_AUTH_REQUIRED_METHODS", "SCRAM_SHA_256");
    scratchbird::security::AuthResult result = manager.startAuthentication(auth_ctx);
    EXPECT_EQ(result.state, scratchbird::security::AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, scratchbird::security::AuthFailReason::NOT_ALLOWED);
    EXPECT_NE(result.failure_message.find("AUTH_CLIENT_PINNING_VIOLATION"), std::string::npos);
}

TEST(AuthPluginV1AMethodsTest, AuthManagerRejectsNoLoginDirectWithoutProxyAssertion) {
    scratchbird::security::AuthManager manager;
    scratchbird::security::AuthManagerConfig config;
    config.hba_enabled = false;
    config.rate_limit_enabled = false;
    config.audit_enabled = false;
    config.auth_plugin_registry_enabled = false;
    config.default_auth_type = scratchbird::security::AuthType::TRUST;

    scratchbird::core::ErrorContext init_ctx;
    ASSERT_EQ(manager.initialize(config, &init_ctx), scratchbird::core::Status::OK)
        << init_ctx.message;

    scratchbird::security::AuthContext auth_ctx;
    scratchbird::security::ConnectionInfo conn;
    conn.protocol = "native";
    conn.database_name = "sb";
    conn.client_address = "127.0.0.1";
    auth_ctx.setConnectionInfo(conn);
    auth_ctx.setUsername("alice");

    ScopedEnvVar no_login("SCRATCHBIRD_AUTH_NO_LOGIN_DIRECT", "1");
    auth_ctx.setSessionProperty("auth.proxy_assertion_verified", "0");
    scratchbird::security::AuthResult result = manager.startAuthentication(auth_ctx);
    EXPECT_EQ(result.state, scratchbird::security::AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, scratchbird::security::AuthFailReason::NOT_ALLOWED);
    EXPECT_NE(result.failure_message.find("AUTH_NO_LOGIN_DIRECT"), std::string::npos);
}
