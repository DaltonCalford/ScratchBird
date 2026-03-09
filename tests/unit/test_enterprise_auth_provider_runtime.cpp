#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"

using namespace scratchbird::core;

namespace {

std::string base64UrlEncode(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return "";
    }
    std::vector<uint8_t> encoded(((bytes.size() + 2) / 3) * 4);
    const int n = EVP_EncodeBlock(encoded.data(), bytes.data(), static_cast<int>(bytes.size()));
    if (n <= 0) {
        return "";
    }

    std::string out(reinterpret_cast<const char*>(encoded.data()), static_cast<size_t>(n));
    while (!out.empty() && out.back() == '=') {
        out.pop_back();
    }
    std::replace(out.begin(), out.end(), '+', '-');
    std::replace(out.begin(), out.end(), '/', '_');
    return out;
}

std::vector<uint8_t> hmacSha256(const std::string& text, const std::string& secret) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    HMAC(EVP_sha256(),
         secret.data(),
         static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(text.data()),
         text.size(),
         digest.data(),
         &digest_len);
    return std::vector<uint8_t>(digest.begin(), digest.begin() + digest_len);
}

std::string makeHs256Jwt(const std::string& issuer,
                         const std::string& audience,
                         const std::string& subject,
                         const std::string& preferred_username,
                         const std::string& email,
                         const std::string& secret,
                         uint64_t exp_unix) {
    const nlohmann::json header = {
        {"alg", "HS256"},
        {"typ", "JWT"},
    };
    const nlohmann::json payload = {
        {"iss", issuer},
        {"aud", audience},
        {"sub", subject},
        {"preferred_username", preferred_username},
        {"email", email},
        {"exp", exp_unix},
    };

    const std::string header_json = header.dump();
    const std::string payload_json = payload.dump();
    const std::string header_b64 = base64UrlEncode(
        std::vector<uint8_t>(header_json.begin(), header_json.end()));
    const std::string payload_b64 = base64UrlEncode(
        std::vector<uint8_t>(payload_json.begin(), payload_json.end()));
    const std::string signing_input = header_b64 + "." + payload_b64;
    const std::vector<uint8_t> signature = hmacSha256(signing_input, secret);
    return signing_input + "." + base64UrlEncode(signature);
}

struct SeededEnterpriseAuth {
    ID user_id{};
    ID policy_id{};
    ID provider_id{};
    ID account_id{};
};

class EnterpriseAuthProviderRuntimeTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = "/tmp/test_enterprise_auth_provider_runtime_" +
                   std::to_string(getpid()) + ".db";
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        ASSERT_EQ(db_->connect(conn_, &ctx), Status::OK) << ctx.message;
        ConnectionContext::setCurrent(conn_.get());
    }

    void TearDown() override {
        ConnectionContext::setCurrent(nullptr);
        conn_.reset();
        if (db_) {
            db_->close();
            db_.reset();
        }
        if (!db_path_.empty()) {
            std::remove(db_path_.c_str());
        }
    }

    SeededEnterpriseAuth seedEnterpriseAuth(const std::string& username,
                                            CatalogManager::AuthProviderKind provider_kind,
                                            CatalogManager::AuthMethod mapping_method,
                                            CatalogManager::ConnectionAuthMethod required_method,
                                            uint16_t allowed_method_mask,
                                            const std::string& provider_name,
                                            const std::string& config_payload,
                                            const std::string& external_subject,
                                            uint16_t lockout_threshold = 0) {
        ErrorContext ctx;
        SeededEnterpriseAuth seeded{};

        auto require_ok = [&](Status status, const char* action) -> bool {
            if (status == Status::OK) {
                return true;
            }
            ADD_FAILURE() << action << " failed: " << ctx.message;
            return false;
        };

        if (!require_ok(catalog_->createUser(username, "", ID{}, false, seeded.user_id, &ctx),
                        "createUser")) {
            return seeded;
        }

        CatalogManager::AuthProviderCatalogInfo provider{};
        provider.provider_id = generateUuidV7();
        provider.provider_name = provider_name;
        provider.provider_kind = provider_kind;
        provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
        provider.priority_rank = 1;
        provider.timeout_ms = 3000;
        provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
        provider.config_payload = config_payload;
        if (!require_ok(catalog_->upsertAuthProviderCatalogEntry(provider, &ctx),
                        "upsertAuthProviderCatalogEntry")) {
            return seeded;
        }
        seeded.provider_id = provider.provider_id;

        CatalogManager::AuthPolicyCatalogInfo policy{};
        policy.policy_id = generateUuidV7();
        policy.policy_name = provider_name + "_policy";
        policy.provider_chain = {provider.provider_id};
        policy.allow_password_fallback = false;
        policy.allowed_auth_method_mask = allowed_method_mask;
        policy.has_required_auth_method = true;
        policy.required_auth_method = required_method;
        policy.allowed_transport_mask = CatalogManager::AUTH_POLICY_TRANSPORT_IPC;
        policy.lockout_threshold = lockout_threshold;
        policy.lockout_window_ms = 120000;
        policy.lockout_duration_ms = 120000;
        if (!require_ok(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx),
                        "upsertAuthPolicyCatalogEntry")) {
            return seeded;
        }
        seeded.policy_id = policy.policy_id;

        CatalogManager::PrincipalAccountCatalogInfo account{};
        account.account_id = generateUuidV7();
        account.principal_name = username;
        account.principal_kind = CatalogManager::PrincipalKind::USER;
        account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
        account.auth_policy_id = policy.policy_id;
        if (!require_ok(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx),
                        "upsertPrincipalAccountCatalogEntry")) {
            return seeded;
        }
        seeded.account_id = account.account_id;

        CatalogManager::AuthMappingCatalogInfo mapping{};
        mapping.mapping_id = generateUuidV7();
        mapping.auth_method = mapping_method;
        mapping.auth_source = provider_name;
        mapping.external_subject = external_subject;
        mapping.database_id = db_->uuid();
        mapping.user_id = seeded.user_id;
        mapping.priority = 1;
        if (!require_ok(catalog_->upsertAuthMappingCatalogEntry(mapping, &ctx),
                        "upsertAuthMappingCatalogEntry")) {
            return seeded;
        }

        return seeded;
    }

    std::unique_ptr<AuthProvider> makeProvider() {
        return AuthProviderFactory::createDefault(catalog_, db_->audit_logger());
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
    std::unique_ptr<ConnectionContext> conn_;
};

TEST_F(EnterpriseAuthProviderRuntimeTest, LdapBindUsesProviderChainAndAuthMapping) {
    const std::string username = "alice";
    const std::string external_subject = "uid=alice,ou=People,dc=example,dc=com";
    const auto seeded = seedEnterpriseAuth(
        username,
        CatalogManager::AuthProviderKind::LDAP_SIMPLE_BIND,
        CatalogManager::AuthMethod::LDAP,
        CatalogManager::ConnectionAuthMethod::PASSWORD,
        CatalogManager::AUTH_POLICY_METHOD_PASSWORD,
        "corp_ldap",
        "server_uri=ldaps://ldap.example;"
        "bind_dn_template=uid={user},ou=People,dc=example,dc=com;"
        "tls_mode=LDAPS;"
        "allowed_ldap_endpoints=ldaps://ldap.example",
        external_subject);

    auto provider = makeProvider();
    ASSERT_NE(provider, nullptr);

    AuthUserInfo user_info;
    std::string error;
    const std::vector<uint8_t> payload{'s', 'e', 'c', 'r', 'e', 't'};
    EXPECT_EQ(provider->authenticatePluginPayload(
                  "scratchbird.auth.ldap_bind", username, payload, user_info, error),
              AuthResult::SUCCESS)
        << error;
    EXPECT_EQ(user_info.user_id, seeded.user_id);
    EXPECT_EQ(user_info.username, username);
    EXPECT_EQ(user_info.external_id, external_subject);
    EXPECT_NE(user_info.authkey_id, ID{});

    ErrorContext ctx;
    std::vector<CatalogManager::AuthAttemptLogCatalogInfo> attempts;
    ASSERT_EQ(catalog_->listAuthAttemptLogCatalogEntries(seeded.account_id, attempts, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(attempts.size(), 1u);
    EXPECT_EQ(attempts.back().outcome, CatalogManager::AuthAttemptOutcome::SUCCESS);
    EXPECT_TRUE(attempts.back().has_provider_id);
    EXPECT_EQ(attempts.back().provider_id, seeded.provider_id);
}

TEST_F(EnterpriseAuthProviderRuntimeTest, KerberosReplayFailsClosedAndAppliesLockout) {
    const std::string username = "alice";
    const auto seeded = seedEnterpriseAuth(
        username,
        CatalogManager::AuthProviderKind::KERBEROS_GSSAPI,
        CatalogManager::AuthMethod::KERBEROS,
        CatalogManager::ConnectionAuthMethod::TOKEN,
        CatalogManager::AUTH_POLICY_METHOD_TOKEN,
        "corp_ad_kerberos",
        "realm=EXAMPLE.COM;"
        "service_principal=postgres/host@example.com;"
        "keytab_ref=/etc/security/krb5.keytab;"
        "kdc_endpoint=kdc.example.com;"
        "allowed_kdc_endpoints=kdc.example.com;"
        "clock_skew_ms=5000",
        "alice@EXAMPLE.COM",
        1);

    auto provider = makeProvider();
    ASSERT_NE(provider, nullptr);

    AuthUserInfo user_info;
    std::string error;
    const std::string replay_payload = "principal=alice@EXAMPLE.COM;ticket=__replay__";
    EXPECT_EQ(provider->authenticatePluginPayload(
                  "scratchbird.auth.kerberos_gssapi",
                  username,
                  std::vector<uint8_t>(replay_payload.begin(), replay_payload.end()),
                  user_info,
                  error),
              AuthResult::INVALID_CREDENTIALS);
    EXPECT_FALSE(error.empty());

    error.clear();
    EXPECT_EQ(provider->authenticatePluginPayload(
                  "scratchbird.auth.kerberos_gssapi",
                  username,
                  std::vector<uint8_t>(replay_payload.begin(), replay_payload.end()),
                  user_info,
                  error),
              AuthResult::USER_LOCKED);

    ErrorContext ctx;
    CatalogManager::PrincipalAccountCatalogInfo account{};
    ASSERT_EQ(catalog_->getPrincipalAccountCatalogEntry(seeded.account_id, account, &ctx), Status::OK)
        << ctx.message;
    EXPECT_TRUE(account.is_locked);
}

TEST_F(EnterpriseAuthProviderRuntimeTest, EntraOidcIsProviderBackedAndPolicyBound) {
    const std::string username = "alice";
    const std::string issuer = "https://login.microsoftonline.com/test-tenant/v2.0";
    const std::string audience = "scratchbird";
    const std::string subject = "entra-alice-subject";
    const std::string secret = "entra-signing-secret";
    const auto seeded = seedEnterpriseAuth(
        username,
        CatalogManager::AuthProviderKind::OIDC_JWT,
        CatalogManager::AuthMethod::ACTIVE_DIRECTORY,
        CatalogManager::ConnectionAuthMethod::TOKEN,
        CatalogManager::AUTH_POLICY_METHOD_TOKEN,
        "entra_primary",
        "iss=" + issuer + ";sub=required;exp=required;aud=" + audience +
            ";client_secret=" + secret,
        subject);

    auto provider = makeProvider();
    ASSERT_NE(provider, nullptr);

    const uint64_t future_exp = static_cast<uint64_t>(std::time(nullptr)) + 3600;
    const std::string good_token = makeHs256Jwt(
        issuer, audience, subject, username, "alice@example.com", secret, future_exp);

    AuthUserInfo user_info;
    std::string error;
    EXPECT_EQ(provider->authenticatePluginPayload(
                  "scratchbird.auth.oidc_id_token",
                  username,
                  std::vector<uint8_t>(good_token.begin(), good_token.end()),
                  user_info,
                  error),
              AuthResult::SUCCESS)
        << error;
    EXPECT_EQ(user_info.user_id, seeded.user_id);
    EXPECT_EQ(user_info.external_id, subject);
    EXPECT_EQ(user_info.email, "alice@example.com");

    const std::string bad_token = makeHs256Jwt(
        "https://login.microsoftonline.com/other-tenant/v2.0",
        audience,
        subject,
        username,
        "alice@example.com",
        secret,
        future_exp);
    error.clear();
    EXPECT_EQ(provider->authenticatePluginPayload(
                  "scratchbird.auth.oidc_id_token",
                  username,
                  std::vector<uint8_t>(bad_token.begin(), bad_token.end()),
                  user_info,
                  error),
              AuthResult::INVALID_CREDENTIALS);
}

}  // namespace
