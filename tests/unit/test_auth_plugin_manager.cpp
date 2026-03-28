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

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include "scratchbird/security/auth_manager.h"
#include "scratchbird/security/auth_plugin_manager.h"

using scratchbird::core::Status;
using scratchbird::security::AuthManager;
using scratchbird::security::AuthManagerConfig;
using scratchbird::security::AuthPluginManager;
using scratchbird::security::AuthPluginManagerConfig;
using scratchbird::security::AuthPluginRejectReason;
using scratchbird::security::AuthType;

namespace {

nlohmann::ordered_json canonicalizeJson(const nlohmann::json& value) {
    if (value.is_object()) {
        nlohmann::ordered_json out = nlohmann::ordered_json::object();
        std::vector<std::string> keys;
        keys.reserve(value.size());
        for (auto it = value.begin(); it != value.end(); ++it) {
            keys.push_back(it.key());
        }
        std::sort(keys.begin(), keys.end());
        for (const auto& key : keys) {
            out[key] = canonicalizeJson(value.at(key));
        }
        return out;
    }
    if (value.is_array()) {
        nlohmann::ordered_json out = nlohmann::ordered_json::array();
        for (const auto& entry : value) {
            out.push_back(canonicalizeJson(entry));
        }
        return out;
    }
    return value;
}

std::string base64UrlEncode(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return "";
    }
    std::vector<uint8_t> encoded((((bytes.size() + 2) / 3) * 4) + 1);
    const int n = EVP_EncodeBlock(encoded.data(), bytes.data(), static_cast<int>(bytes.size()));
    if (n <= 0) {
        return "";
    }
    std::string out(reinterpret_cast<const char*>(encoded.data()), static_cast<std::size_t>(n));
    while (!out.empty() && out.back() == '=') {
        out.pop_back();
    }
    std::replace(out.begin(), out.end(), '+', '-');
    std::replace(out.begin(), out.end(), '/', '_');
    return out;
}

std::string sha256Hex(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open());
    SHA256_CTX ctx{};
    EXPECT_EQ(SHA256_Init(&ctx), 1);

    std::array<uint8_t, 4096> buf{};
    while (in.good()) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const std::streamsize n = in.gcount();
        if (n > 0) {
            EXPECT_EQ(SHA256_Update(&ctx, buf.data(), static_cast<std::size_t>(n)), 1);
        }
    }
    std::array<uint8_t, SHA256_DIGEST_LENGTH> digest{};
    EXPECT_EQ(SHA256_Final(digest.data(), &ctx), 1);

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(digest.size() * 2);
    for (std::size_t i = 0; i < digest.size(); ++i) {
        out[2 * i] = kHex[(digest[i] >> 4) & 0x0F];
        out[2 * i + 1] = kHex[digest[i] & 0x0F];
    }
    return out;
}

std::filesystem::path repoRoot() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

const char* sharedLibraryExtension() {
#if defined(_WIN32)
    return ".dll";
#elif defined(__APPLE__)
    return ".dylib";
#else
    return ".so";
#endif
}

std::filesystem::path detectRuntimePluginRoot() {
    const std::filesystem::path required_module =
        std::filesystem::path("scratchbird.auth.trust_reject") /
        ("libscratchbird_auth_trust_reject" + std::string(sharedLibraryExtension()));

    const std::vector<std::filesystem::path> candidates = {
        repoRoot() / "build" / "auth_plugins",
        std::filesystem::current_path() / "auth_plugins",
        std::filesystem::current_path().parent_path() / "auth_plugins",
        std::filesystem::current_path().parent_path().parent_path() / "auth_plugins"
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate / required_module)) {
            return candidate;
        }
    }

    return {};
}

class ScopedTempDir {
public:
    ScopedTempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("sb_auth_plugin_test_" + std::to_string(stamp));
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

TEST(AuthPluginManagerTest, LoadsExamplePolicyAndPhase1Plugins) {
    AuthPluginManager manager;
    AuthPluginManagerConfig config = AuthPluginManagerConfig::defaults();
    const auto root = repoRoot();
    config.truststore_path =
        (root / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.policy_path = (root / "etc" / "auth" / "auth_plugins.policy.json.example").string();
    config.plugin_root.clear();

    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(manager.initialize(config, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(manager.missingRequiredPlugins().empty());
    EXPECT_TRUE(manager.hasPlugin("scratchbird.auth.scram"));
    EXPECT_TRUE(manager.isMethodAvailable("scratchbird.auth.scram_sha_256"));
    EXPECT_TRUE(manager.isAuthTypeAvailable(AuthType::SCRAM_SHA_256));

    std::string method_id;
    ASSERT_TRUE(manager.resolveMethodIdForAuthType(AuthType::PEER, method_id));
    EXPECT_EQ(method_id, "scratchbird.auth.peer_uid");
}

TEST(AuthPluginManagerTest, RecordsDeterministicSignerRejection) {
    ScopedTempDir temp;

    const std::string plugin_id = "scratchbird.auth.custom";
    const auto plugin_dir = temp.path() / plugin_id;
    const auto module_dir = plugin_dir / "module";
    ASSERT_TRUE(std::filesystem::create_directories(module_dir));

    const auto module_path = module_dir / "libscratchbird_auth_custom.so";
    {
        std::ofstream module_out(module_path, std::ios::binary);
        ASSERT_TRUE(module_out.is_open());
        module_out << "dummy module content";
    }
    const std::string module_digest = sha256Hex(module_path);

    nlohmann::json manifest{
        {"schema_version", "1"},
        {"plugin_id", plugin_id},
        {"plugin_version", "1.0.0"},
        {"abi_major", 1},
        {"abi_minor", 0},
        {"module_path", "module/libscratchbird_auth_custom.so"},
        {"module_sha256", module_digest},
        {"supported_method_ids", nlohmann::json::array({"scratchbird.auth.custom_method"})},
        {"signing", {{"kid", "unknown-kid"}}}
    };
    {
        std::ofstream manifest_out(plugin_dir / "manifest.json");
        ASSERT_TRUE(manifest_out.is_open());
        manifest_out << manifest.dump(2);
    }

    const std::string canonical_manifest = canonicalizeJson(manifest).dump();
    const std::string payload_b64 = base64UrlEncode(
        std::vector<uint8_t>(canonical_manifest.begin(), canonical_manifest.end()));
    const std::string signature_b64 = base64UrlEncode(std::vector<uint8_t>{1, 2, 3, 4});

    nlohmann::json jws{
        {"payload", payload_b64},
        {"signatures", nlohmann::json::array({
             {{"header", {{"kid", "unknown-kid"}, {"alg", "EdDSA"}}},
              {"signature", signature_b64}}
        })}
    };
    {
        std::ofstream jws_out(plugin_dir / "manifest.jws");
        ASSERT_TRUE(jws_out.is_open());
        jws_out << jws.dump(2);
    }

    nlohmann::json policy{
        {"mode", "signed_only"},
        {"fail_on_unlisted_plugins", false},
        {"allowed_plugins",
         {
             {plugin_id,
              {
                  {"required", false},
                  {"allowed_signers", nlohmann::json::array({"sb-release-kid-2026"})},
                  {"allowed_method_ids",
                   nlohmann::json::array({"scratchbird.auth.custom_method"})}
              }}
         }}
    };
    const auto policy_path = temp.path() / "auth_plugins.policy.json";
    {
        std::ofstream policy_out(policy_path);
        ASSERT_TRUE(policy_out.is_open());
        policy_out << policy.dump(2);
    }

    AuthPluginManager manager;
    AuthPluginManagerConfig config = AuthPluginManagerConfig::defaults();
    config.truststore_path =
        (repoRoot() / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.policy_path = policy_path.string();
    config.plugin_root = temp.path().string();

    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(manager.initialize(config, &ctx), Status::OK) << ctx.message;

    bool saw_untrusted = false;
    for (const auto& issue : manager.admissionIssues()) {
        if (issue.plugin_id == plugin_id &&
            issue.reason == AuthPluginRejectReason::AUTH_PLUGIN_SIGNER_UNTRUSTED) {
            saw_untrusted = true;
            break;
        }
    }
    EXPECT_TRUE(saw_untrusted);
}

TEST(AuthManagerPluginRegistryTest, FailsStartupWhenRequiredPluginMissing) {
    ScopedTempDir temp;

    nlohmann::json policy{
        {"mode", "signed_only"},
        {"fail_on_unlisted_plugins", false},
        {"allowed_plugins",
         {
             {"scratchbird.auth.required_custom",
              {
                  {"required", true},
                  {"allowed_signers", nlohmann::json::array({"sb-release-kid-2026"})},
                  {"allowed_method_ids",
                   nlohmann::json::array({"scratchbird.auth.required_custom_method"})}
              }}
         }}
    };
    const auto policy_path = temp.path() / "auth_plugins.policy.json";
    {
        std::ofstream policy_out(policy_path);
        ASSERT_TRUE(policy_out.is_open());
        policy_out << policy.dump(2);
    }

    AuthManager manager;
    AuthManagerConfig config;
    config.auth_plugin_registry_enabled = true;
    config.allow_legacy_auth_fallback = false;
    config.auth_plugin_truststore_path =
        (repoRoot() / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.auth_plugin_policy_path = policy_path.string();
    config.auth_plugin_root = temp.path().string();

    scratchbird::core::ErrorContext ctx;
    EXPECT_EQ(manager.initialize(config, &ctx), Status::NOT_FOUND);
    EXPECT_NE(ctx.message.find("required plugin"), std::string::npos);
}

TEST(AuthManagerPluginRegistryTest, UsesRuntimeDispatchWhenModulesAvailable) {
    const std::filesystem::path plugin_root = detectRuntimePluginRoot();
    if (plugin_root.empty()) {
        GTEST_SKIP() << "Auth runtime plugin modules are not available for this build tree";
    }

    AuthManager manager;
    AuthManagerConfig config;
    config.hba_enabled = false;
    config.rate_limit_enabled = false;
    config.audit_enabled = false;
    config.log_connections = false;
    config.auth_plugin_registry_enabled = true;
    config.allow_legacy_auth_fallback = false;
    config.default_auth_type = AuthType::REJECT;
    config.auth_plugin_root = plugin_root.string();
    config.auth_plugin_truststore_path =
        (repoRoot() / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.auth_plugin_policy_path =
        (repoRoot() / "etc" / "auth" / "auth_plugins.policy.json.example").string();

    scratchbird::core::ErrorContext init_ctx;
    ASSERT_EQ(manager.initialize(config, &init_ctx), Status::OK) << init_ctx.message;
    ASSERT_NE(manager.authPluginManager(), nullptr);
    EXPECT_TRUE(manager.authPluginManager()->isRuntimeMethodAvailable("scratchbird.auth.reject"));

    scratchbird::security::AuthContext auth_ctx;
    scratchbird::security::ConnectionInfo conn;
    conn.protocol = "native";
    conn.database_name = "sb";
    conn.client_address = "127.0.0.1";
    auth_ctx.setConnectionInfo(conn);
    auth_ctx.setUsername("alice");

    scratchbird::security::AuthResult result = manager.startAuthentication(auth_ctx);
    EXPECT_EQ(result.state, scratchbird::security::AuthState::FAILURE);
    EXPECT_EQ(result.failure_reason, scratchbird::security::AuthFailReason::INVALID_CREDENTIALS);
    EXPECT_NE(result.failure_message.find("AUTH_TRUST_REJECT_FORCED_DENY"), std::string::npos);
    EXPECT_EQ(auth_ctx.getSessionProperty("auth.plugin_registry"), "enabled");
    EXPECT_EQ(auth_ctx.getSessionProperty("auth.plugin_method_id"), "scratchbird.auth.reject");
    EXPECT_EQ(result.failure_message.find("dispatch backend missing"), std::string::npos);
}
