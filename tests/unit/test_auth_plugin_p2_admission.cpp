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

#include "scratchbird/security/auth_plugin_manager.h"

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
    std::vector<uint8_t> encoded(((bytes.size() + 2) / 3) * 4);
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

class ScopedTempDir {
public:
    ScopedTempDir() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("sb_auth_plugin_p2_test_" + std::to_string(stamp));
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

void writePluginArtifacts(const std::filesystem::path& root,
                          const std::string& plugin_id,
                          const std::string& method_id,
                          const std::string& jws_kid,
                          const std::string& policy_signer) {
    const auto plugin_dir = root / plugin_id;
    const auto module_dir = plugin_dir / "module";
    std::filesystem::create_directories(module_dir);

    const auto module_path = module_dir / "libp2.so";
    {
        std::ofstream module_out(module_path, std::ios::binary);
        module_out << "dummy module content";
    }

    nlohmann::json manifest{
        {"schema_version", "1"},
        {"plugin_id", plugin_id},
        {"plugin_version", "2.0.0"},
        {"abi_major", 1},
        {"abi_minor", 0},
        {"module_path", "module/libp2.so"},
        {"module_sha256", sha256Hex(module_path)},
        {"supported_method_ids", nlohmann::json::array({method_id})},
        {"signing", {{"kid", jws_kid}}}
    };
    {
        std::ofstream manifest_out(plugin_dir / "manifest.json");
        manifest_out << manifest.dump(2);
    }

    const std::string canonical_manifest = canonicalizeJson(manifest).dump();
    const std::string payload_b64 = base64UrlEncode(
        std::vector<uint8_t>(canonical_manifest.begin(), canonical_manifest.end()));
    const std::string signature_b64 = base64UrlEncode(std::vector<uint8_t>{1, 2, 3, 4});

    nlohmann::json jws{
        {"payload", payload_b64},
        {"signatures", nlohmann::json::array({
             {{"header", {{"kid", jws_kid}, {"alg", "EdDSA"}}},
              {"signature", signature_b64}}
        })}
    };
    {
        std::ofstream jws_out(plugin_dir / "manifest.jws");
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
                  {"allowed_signers", nlohmann::json::array({policy_signer})},
                  {"allowed_method_ids", nlohmann::json::array({method_id})}
              }}
         }}
    };
    {
        std::ofstream policy_out(root / "auth_plugins.policy.json");
        policy_out << policy.dump(2);
    }
}

}  // namespace

TEST(AuthPluginP2AdmissionTest, RejectsUntrustedSignerForEnterpriseMethod) {
    ScopedTempDir temp;
    const std::string plugin_id = "scratchbird.auth.ldap.external";
    writePluginArtifacts(temp.path(),
                         plugin_id,
                         "scratchbird.auth.ldap_bind",
                         "unknown-kid",
                         "sb-enterprise-kid-2026");

    scratchbird::security::AuthPluginManager manager;
    scratchbird::security::AuthPluginManagerConfig config =
        scratchbird::security::AuthPluginManagerConfig::defaults();
    config.truststore_path =
        (repoRoot() / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.policy_path = (temp.path() / "auth_plugins.policy.json").string();
    config.plugin_root = temp.path().string();

    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(manager.initialize(config, &ctx), scratchbird::core::Status::OK) << ctx.message;

    bool saw_untrusted = false;
    for (const auto& issue : manager.admissionIssues()) {
        if (issue.plugin_id == plugin_id &&
            issue.reason == scratchbird::security::AuthPluginRejectReason::AUTH_PLUGIN_SIGNER_UNTRUSTED) {
            saw_untrusted = true;
        }
    }
    EXPECT_TRUE(saw_untrusted);
}

TEST(AuthPluginP2AdmissionTest, RejectsPolicySignerMismatchForEnterpriseMethod) {
    ScopedTempDir temp;
    const std::string plugin_id = "scratchbird.auth.kerberos.external";
    writePluginArtifacts(temp.path(),
                         plugin_id,
                         "scratchbird.auth.kerberos_gssapi",
                         "sb-enterprise-kid-2026",
                         "sb-security-kid-2026");

    scratchbird::security::AuthPluginManager manager;
    scratchbird::security::AuthPluginManagerConfig config =
        scratchbird::security::AuthPluginManagerConfig::defaults();
    config.truststore_path =
        (repoRoot() / "etc" / "auth" / "auth_plugin_truststore.jwks.json.example").string();
    config.policy_path = (temp.path() / "auth_plugins.policy.json").string();
    config.plugin_root = temp.path().string();

    scratchbird::core::ErrorContext ctx;
    ASSERT_EQ(manager.initialize(config, &ctx), scratchbird::core::Status::OK) << ctx.message;

    bool saw_policy_denied = false;
    for (const auto& issue : manager.admissionIssues()) {
        if (issue.plugin_id == plugin_id &&
            issue.reason == scratchbird::security::AuthPluginRejectReason::AUTH_PLUGIN_POLICY_DENIED) {
            saw_policy_denied = true;
        }
    }
    EXPECT_TRUE(saw_policy_denied);
}
