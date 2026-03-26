/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>
#include <openssl/hmac.h>
#include <nlohmann/json.hpp>
#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/security/scram_auth.h"

using namespace scratchbird::core;

namespace {

using Json = nlohmann::json;

double metricCounterValue(const std::string& metric_name,
                          const std::vector<std::string>& labels) {
    auto* metric = MetricsRegistry::getInstance().get(metric_name);
    if (metric == nullptr) {
        return 0.0;
    }
    auto* counter = dynamic_cast<Counter*>(metric);
    if (counter == nullptr) {
        return 0.0;
    }
    return counter->get(labels);
}

std::string makeBootstrapDbPath() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/test_bootstrap_claim_" + std::to_string(getpid()) + "_" +
           std::to_string(now) + ".db";
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

class ScopedConfigValue {
public:
    ScopedConfigValue(std::string section,
                      std::string key,
                      std::string value,
                      std::string reset_value)
        : section_(std::move(section)),
          key_(std::move(key)),
          reset_value_(std::move(reset_value)) {
        Config::getInstance().set(section_, key_, value);
    }

    ~ScopedConfigValue() {
        Config::getInstance().set(section_, key_, reset_value_);
    }

private:
    std::string section_;
    std::string key_;
    std::string reset_value_;
};

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

bool noticesContain(const std::vector<std::string>& notices, const std::string& needle) {
    for (const auto& notice : notices) {
        if (notice.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<uint8_t> buildAuthKeyTokenProof(const std::string& token_secret,
                                            const std::string& username,
                                            const ID& authkey_id,
                                            const std::vector<uint8_t>& binding) {
    static constexpr char kTokenProofPrefix[] = "SB-AUTHKEY-TOKEN-V1";

    std::vector<uint8_t> message;
    message.reserve(sizeof(kTokenProofPrefix) - 1 + username.size() +
                    authkey_id.bytes.size() + binding.size());
    message.insert(message.end(),
                   reinterpret_cast<const uint8_t*>(kTokenProofPrefix),
                   reinterpret_cast<const uint8_t*>(kTokenProofPrefix) + (sizeof(kTokenProofPrefix) - 1));
    message.insert(message.end(), username.begin(), username.end());
    message.insert(message.end(), authkey_id.bytes.begin(), authkey_id.bytes.end());
    message.insert(message.end(), binding.begin(), binding.end());

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (!HMAC(EVP_sha256(),
              token_secret.data(),
              static_cast<int>(token_secret.size()),
              message.data(),
              message.size(),
              digest,
              &digest_len)) {
        return {};
    }

    return std::vector<uint8_t>(digest, digest + digest_len);
}

std::string buildScramSha256PasswordHash(const std::string& password,
                                         uint32_t iterations) {
    std::vector<uint8_t> salt;
    std::vector<uint8_t> stored_key;
    std::vector<uint8_t> server_key;
    if (scratchbird::security::generateScramCredentials(password,
                                                        scratchbird::security::ScramAlgorithm::SHA_256,
                                                        iterations,
                                                        salt,
                                                        stored_key,
                                                        server_key) != Status::OK) {
        return {};
    }

    Json doc = Json::object();
    doc["scram"] = Json::object();
    doc["scram"]["sha256"] = Json::object({
        {"iterations", iterations},
        {"salt", scratchbird::security::base64Encode(salt)},
        {"stored_key", scratchbird::security::base64Encode(stored_key)},
        {"server_key", scratchbird::security::base64Encode(server_key)}
    });
    return doc.dump();
}

class AuthBootstrapClaimTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = makeBootstrapDbPath();
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;

        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);

        // The bootstrap claim contract applies only when catalog is still in
        // bootstrap phase A (SYSTEM-only user set). Prune any seeded users so
        // tests remain deterministic regardless of bootstrap fixtures.
        std::vector<CatalogManager::UserInfo> users;
        ASSERT_EQ(catalog_->listUsers(users, &ctx), Status::OK) << ctx.message;
        for (const auto& user : users) {
            if (toUpperAscii(user.username) == "SYSTEM") {
                continue;
            }
            ErrorContext delete_ctx;
            ASSERT_EQ(catalog_->deleteUser(user.user_id, true, &delete_ctx), Status::OK)
                << "failed deleting seeded user '" << user.username
                << "': " << delete_ctx.message;
        }

        CatalogManager::BootstrapState state = CatalogManager::BootstrapState::INITIALIZED;
        ASSERT_EQ(catalog_->getBootstrapState(state, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(state, CatalogManager::BootstrapState::UNINITIALIZED);
    }

    void TearDown() override {
        if (db_) {
            db_->close();
            db_.reset();
        }
        if (!db_path_.empty()) {
            std::remove(db_path_.c_str());
        }
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
};

TEST(AuthBootstrapClaimStandaloneTest, LocalProviderRejectsNullCatalog) {
    auto provider = AuthProviderFactory::create(AuthProviderType::LOCAL, "", nullptr, nullptr);
    EXPECT_EQ(provider, nullptr);
    EXPECT_THROW(
        {
            auto default_provider = AuthProviderFactory::createDefault(nullptr, nullptr);
            (void)default_provider;
        },
        std::invalid_argument);
}

TEST_F(AuthBootstrapClaimTest, ConcurrentBootstrapClaimsHaveSingleWinner) {
    constexpr int kThreads = 12;

    std::atomic<int> success_count{0};
    std::atomic<int> conflict_count{0};
    std::atomic<int> other_count{0};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([this, &success_count, &conflict_count, &other_count]() {
            ErrorContext ctx;
            Status status = catalog_->claimBootstrapWindow(&ctx);
            if (status == Status::OK) {
                success_count.fetch_add(1);
            } else if (status == Status::CONSTRAINT_VIOLATION) {
                conflict_count.fetch_add(1);
            } else {
                other_count.fetch_add(1);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    EXPECT_EQ(success_count.load(), 1);
    EXPECT_EQ(conflict_count.load(), kThreads - 1);
    EXPECT_EQ(other_count.load(), 0);

    CatalogManager::BootstrapState state = CatalogManager::BootstrapState::UNINITIALIZED;
    ErrorContext ctx;
    ASSERT_EQ(catalog_->getBootstrapState(state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, CatalogManager::BootstrapState::LOCKED);

    ASSERT_EQ(catalog_->releaseBootstrapWindow(&ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getBootstrapState(state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, CatalogManager::BootstrapState::UNINITIALIZED);
}

TEST_F(AuthBootstrapClaimTest, BootstrapTokenSingleUseEnforced) {
    const std::string token = "bootstrap-token-single-use";
    const std::string token_path = db_path_ + ".bootstrap.token";

    {
        std::ofstream out(token_path);
        ASSERT_TRUE(out.is_open());
        out << token << "\n";
    }
#ifndef _WIN32
    ASSERT_EQ(::chmod(token_path.c_str(), S_IRUSR | S_IWUSR), 0);
#endif

    ScopedEnvVar token_env("SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE", token_path);
    LocalAuthProvider provider(catalog_, nullptr);

    AuthUserInfo first_user{};
    std::string first_error;
    const AuthResult first_result = provider.authenticate(
        "bootstrap_single_use_user", token, first_user, first_error);
    EXPECT_EQ(first_result, AuthResult::SUCCESS) << first_error;
    EXPECT_EQ(first_user.username, "SYSTEM");

    // Token file must be consumed/revoked after first successful bootstrap login.
    std::ifstream consumed(token_path);
    EXPECT_FALSE(consumed.good());

    AuthUserInfo second_user{};
    std::string second_error;
    const AuthResult second_result = provider.authenticate(
        "bootstrap_single_use_user", token, second_user, second_error);
    EXPECT_NE(second_result, AuthResult::SUCCESS);
}

TEST_F(AuthBootstrapClaimTest, ReleasingWithoutClaimIsNoOp) {
    ErrorContext ctx;
    ASSERT_EQ(catalog_->releaseBootstrapWindow(&ctx), Status::OK) << ctx.message;

    CatalogManager::BootstrapState state = CatalogManager::BootstrapState::LOCKED;
    ASSERT_EQ(catalog_->getBootstrapState(state, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(state, CatalogManager::BootstrapState::UNINITIALIZED);
}

TEST_F(AuthBootstrapClaimTest, LegacyAuthMethodUsageUpdatesTelemetryCounters) {
    const std::string metric = "scratchbird_auth_legacy_method_total";
    const double password_before = metricCounterValue(metric, {"password"});
    const double md5_before = metricCounterValue(metric, {"md5"});

    LocalAuthProvider provider(catalog_, nullptr);
    AuthUserInfo user_info{};
    std::string error_message;

    EXPECT_NE(provider.authenticate("missing_user_metric", "invalid-token", user_info, error_message),
              AuthResult::SUCCESS);

    const uint8_t salt[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_NE(provider.authenticateMd5("missing_user_metric", salt, "md5invalid", user_info, error_message),
              AuthResult::SUCCESS);

    EXPECT_DOUBLE_EQ(metricCounterValue(metric, {"password"}), password_before + 1.0);
    EXPECT_DOUBLE_EQ(metricCounterValue(metric, {"md5"}), md5_before + 1.0);
}

TEST_F(AuthBootstrapClaimTest, AuthKeyScopeBindingAndLastUsedRoundTrip) {
    ErrorContext ctx;

    CatalogManager::AuthKeyInfo authkey{};
    authkey.issuer = "authkey_contract";
    authkey.status = CatalogManager::AuthKeyStatus::ACTIVE;
    authkey.usage_type = CatalogManager::AuthKeyUsage::LIMITED;
    authkey.usage_limit = 2;
    authkey.scope = CatalogManager::AuthKeyScope::API_TOKEN;
    authkey.binding_kind = CatalogManager::AuthKeyBindingKind::CLIENT_NONCE;
    authkey.binding_value = "nonce:contract-001";

    ID authkey_id{};
    ASSERT_EQ(catalog_->createAuthKey(authkey, authkey_id, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthKeyInfo loaded{};
    ASSERT_EQ(catalog_->getAuthKey(authkey_id, loaded, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded.scope, CatalogManager::AuthKeyScope::API_TOKEN);
    EXPECT_EQ(loaded.binding_kind, CatalogManager::AuthKeyBindingKind::CLIENT_NONCE);
    EXPECT_EQ(loaded.binding_value, "nonce:contract-001");
    EXPECT_EQ(loaded.usage_limit, 2u);
    EXPECT_EQ(loaded.usage_count, 0u);
    EXPECT_EQ(loaded.last_used_time, 0u);

    ASSERT_EQ(catalog_->consumeAuthKey(authkey_id, 1, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthKeyInfo consumed{};
    ASSERT_EQ(catalog_->getAuthKey(authkey_id, consumed, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(consumed.usage_count, 1u);
    EXPECT_GT(consumed.last_used_time, 0u);

    std::vector<CatalogManager::AuthKeyInfo> authkeys;
    ASSERT_EQ(catalog_->listAuthKeys(authkeys, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(authkeys.begin(), authkeys.end(), [&](const auto& row) {
        return row.authkey_id == authkey_id;
    });
    ASSERT_NE(it, authkeys.end());
    EXPECT_EQ(it->scope, CatalogManager::AuthKeyScope::API_TOKEN);
    EXPECT_EQ(it->binding_kind, CatalogManager::AuthKeyBindingKind::CLIENT_NONCE);
    EXPECT_EQ(it->binding_value, "nonce:contract-001");
}

TEST_F(AuthBootstrapClaimTest, AuthKeyBindingValidationRejectsMissingValue) {
    ErrorContext ctx;

    CatalogManager::AuthKeyInfo invalid{};
    invalid.issuer = "authkey_invalid_binding";
    invalid.binding_kind = CatalogManager::AuthKeyBindingKind::PEER_UID;
    invalid.binding_value.clear();

    ID authkey_id{};
    EXPECT_EQ(catalog_->createAuthKey(invalid, authkey_id, &ctx), Status::INVALID_ARGUMENT);
}

TEST_F(AuthBootstrapClaimTest, AuthKeyUsageValidationRejectsInvalidSeedCounts) {
    ErrorContext ctx;

    CatalogManager::AuthKeyInfo over_limit{};
    over_limit.issuer = "authkey_usage_over_limit";
    over_limit.usage_type = CatalogManager::AuthKeyUsage::LIMITED;
    over_limit.usage_limit = 1;
    over_limit.usage_count = 2;

    ID authkey_id{};
    EXPECT_EQ(catalog_->createAuthKey(over_limit, authkey_id, &ctx), Status::INVALID_ARGUMENT);

    CatalogManager::AuthKeyInfo invalid_single_use{};
    invalid_single_use.issuer = "authkey_single_use_invalid_limit";
    invalid_single_use.usage_type = CatalogManager::AuthKeyUsage::SINGLE_USE;
    invalid_single_use.usage_limit = 2;

    EXPECT_EQ(catalog_->createAuthKey(invalid_single_use, authkey_id, &ctx),
              Status::INVALID_ARGUMENT);
}

TEST_F(AuthBootstrapClaimTest, AuthKeyListNormalizesExpiredStatusAndPersistsIt) {
    ErrorContext ctx;

    CatalogManager::AuthKeyInfo expired{};
    expired.issuer = "authkey_expired_on_list";
    expired.status = CatalogManager::AuthKeyStatus::ACTIVE;
    expired.scope = CatalogManager::AuthKeyScope::API_TOKEN;
    expired.binding_kind = CatalogManager::AuthKeyBindingKind::CLIENT_NONCE;
    expired.binding_value = "expired_secret";
    const uint64_t now = std::chrono::system_clock::now().time_since_epoch().count();
    expired.valid_from = (now > 2000) ? (now - 2000) : 1;
    expired.valid_to = expired.valid_from + 1;

    ID authkey_id{};
    ASSERT_EQ(catalog_->createAuthKey(expired, authkey_id, &ctx), Status::OK) << ctx.message;

    std::vector<CatalogManager::AuthKeyInfo> authkeys;
    ASSERT_EQ(catalog_->listAuthKeys(authkeys, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(authkeys.begin(), authkeys.end(), [&](const auto& row) {
        return row.authkey_id == authkey_id;
    });
    ASSERT_NE(it, authkeys.end());
    EXPECT_EQ(it->status, CatalogManager::AuthKeyStatus::EXPIRED);

    CatalogManager::AuthKeyInfo reloaded{};
    ASSERT_EQ(catalog_->getAuthKey(authkey_id, reloaded, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(reloaded.status, CatalogManager::AuthKeyStatus::EXPIRED);
}

TEST_F(AuthBootstrapClaimTest, AuthTokenSuccessConsumesKey) {
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    CatalogManager::AuthKeyInfo authkey{};
    authkey.issuer = "authkey_token_success";
    authkey.status = CatalogManager::AuthKeyStatus::ACTIVE;
    authkey.usage_type = CatalogManager::AuthKeyUsage::LIMITED;
    authkey.usage_limit = 2;
    authkey.scope = CatalogManager::AuthKeyScope::API_TOKEN;
    authkey.binding_kind = CatalogManager::AuthKeyBindingKind::CLIENT_NONCE;
    authkey.binding_value = "secret-token-proof-001";

    ID authkey_id{};
    ASSERT_EQ(catalog_->createAuthKey(authkey, authkey_id, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> binding(system_user.user_id.bytes.begin(), system_user.user_id.bytes.end());
    std::vector<uint8_t> proof = buildAuthKeyTokenProof(authkey.binding_value,
                                                        system_user.username,
                                                        authkey_id,
                                                        binding);
    ASSERT_FALSE(proof.empty());

    LocalAuthProvider provider(catalog_, nullptr);
    AuthUserInfo user_info{};
    std::string error_message;
    AuthResult result = provider.authenticateToken(system_user.username,
                                                   authkey_id,
                                                   proof,
                                                   binding,
                                                   user_info,
                                                   error_message);
    EXPECT_EQ(result, AuthResult::SUCCESS) << error_message;
    EXPECT_EQ(user_info.user_id, system_user.user_id);
    EXPECT_EQ(user_info.authkey_id, authkey_id);

    CatalogManager::AuthKeyInfo consumed{};
    ASSERT_EQ(catalog_->getAuthKey(authkey_id, consumed, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(consumed.usage_count, 1u);
    EXPECT_EQ(consumed.status, CatalogManager::AuthKeyStatus::ACTIVE);
    EXPECT_GT(consumed.last_used_time, 0u);
}

TEST_F(AuthBootstrapClaimTest, AuthTokenDeniedForRevokedOrExpiredKey) {
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    LocalAuthProvider provider(catalog_, nullptr);

    auto create_token = [&](const std::string& issuer,
                            bool expired) -> ID {
        CatalogManager::AuthKeyInfo authkey{};
        authkey.issuer = issuer;
        authkey.status = CatalogManager::AuthKeyStatus::ACTIVE;
        authkey.scope = CatalogManager::AuthKeyScope::API_TOKEN;
        authkey.binding_kind = CatalogManager::AuthKeyBindingKind::CLIENT_NONCE;
        authkey.binding_value = issuer + "_secret";
        if (expired) {
            const uint64_t now = std::chrono::system_clock::now().time_since_epoch().count();
            authkey.valid_from = (now > 2000) ? (now - 2000) : 1;
            authkey.valid_to = authkey.valid_from + 1;
        }
        ID out{};
        EXPECT_EQ(catalog_->createAuthKey(authkey, out, &ctx), Status::OK) << ctx.message;
        return out;
    };

    ID revoked_id = create_token("authkey_token_revoked", false);
    ASSERT_EQ(catalog_->revokeAuthKey(revoked_id, &ctx), Status::OK) << ctx.message;

    std::vector<uint8_t> binding(system_user.user_id.bytes.begin(), system_user.user_id.bytes.end());
    std::vector<uint8_t> revoked_proof = buildAuthKeyTokenProof("authkey_token_revoked_secret",
                                                                system_user.username,
                                                                revoked_id,
                                                                binding);
    AuthUserInfo user_info{};
    std::string error_message;
    EXPECT_EQ(provider.authenticateToken(system_user.username,
                                         revoked_id,
                                         revoked_proof,
                                         binding,
                                         user_info,
                                         error_message),
              AuthResult::INVALID_CREDENTIALS);

    CatalogManager::AuthKeyInfo revoked_loaded{};
    ASSERT_EQ(catalog_->getAuthKey(revoked_id, revoked_loaded, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(revoked_loaded.status, CatalogManager::AuthKeyStatus::REVOKED);

    ID expired_id = create_token("authkey_token_expired", true);
    std::vector<uint8_t> expired_proof = buildAuthKeyTokenProof("authkey_token_expired_secret",
                                                                system_user.username,
                                                                expired_id,
                                                                binding);
    EXPECT_EQ(provider.authenticateToken(system_user.username,
                                         expired_id,
                                         expired_proof,
                                         binding,
                                         user_info,
                                         error_message),
              AuthResult::INVALID_CREDENTIALS);

    CatalogManager::AuthKeyInfo expired_loaded{};
    ASSERT_EQ(catalog_->getAuthKey(expired_id, expired_loaded, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(expired_loaded.status, CatalogManager::AuthKeyStatus::EXPIRED);
}

TEST_F(AuthBootstrapClaimTest, AuthKeyBulkRevocationByIssuerAndScope) {
    ErrorContext ctx;

    auto create_key = [this](const std::string& issuer,
                             CatalogManager::AuthKeyScope scope,
                             ID& out_id) {
        ErrorContext local_ctx;
        CatalogManager::AuthKeyInfo key{};
        key.issuer = issuer;
        key.status = CatalogManager::AuthKeyStatus::ACTIVE;
        key.scope = scope;
        key.binding_kind = CatalogManager::AuthKeyBindingKind::CLIENT_NONCE;
        key.binding_value = issuer + "_token_secret";
        return catalog_->createAuthKey(key, out_id, &local_ctx);
    };

    ID key_a{};
    ID key_b{};
    ID key_c{};
    ASSERT_EQ(create_key("principal_a", CatalogManager::AuthKeyScope::API_TOKEN, key_a), Status::OK);
    ASSERT_EQ(create_key("principal_a", CatalogManager::AuthKeyScope::LOGIN_SESSION, key_b), Status::OK);
    ASSERT_EQ(create_key("principal_b", CatalogManager::AuthKeyScope::API_TOKEN, key_c), Status::OK);

    uint32_t revoked_count = 0;
    ASSERT_EQ(catalog_->revokeAuthKeysByIssuer("principal_a", revoked_count, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(revoked_count, 2u);

    CatalogManager::AuthKeyInfo loaded_a{};
    CatalogManager::AuthKeyInfo loaded_b{};
    CatalogManager::AuthKeyInfo loaded_c{};
    ASSERT_EQ(catalog_->getAuthKey(key_a, loaded_a, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getAuthKey(key_b, loaded_b, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(catalog_->getAuthKey(key_c, loaded_c, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_a.status, CatalogManager::AuthKeyStatus::REVOKED);
    EXPECT_EQ(loaded_b.status, CatalogManager::AuthKeyStatus::REVOKED);
    EXPECT_EQ(loaded_c.status, CatalogManager::AuthKeyStatus::ACTIVE);

    ASSERT_EQ(catalog_->revokeAuthKeysByScope(CatalogManager::AuthKeyScope::API_TOKEN,
                                              revoked_count,
                                              &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(revoked_count, 1u);

    ASSERT_EQ(catalog_->getAuthKey(key_c, loaded_c, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_c.status, CatalogManager::AuthKeyStatus::REVOKED);

    ASSERT_EQ(catalog_->revokeAuthKeysByScope(CatalogManager::AuthKeyScope::API_TOKEN,
                                              revoked_count,
                                              &ctx),
              Status::OK)
        << ctx.message;
    EXPECT_EQ(revoked_count, 0u);

    EXPECT_EQ(catalog_->revokeAuthKeysByIssuer("", revoked_count, &ctx), Status::INVALID_ARGUMENT);
    EXPECT_EQ(catalog_->revokeAuthKeysByScope(static_cast<CatalogManager::AuthKeyScope>(0xFF),
                                              revoked_count,
                                              &ctx),
              Status::INVALID_ARGUMENT);
}

TEST_F(AuthBootstrapClaimTest, PeerAuthRequiresExplicitPeerMapping) {
    LocalAuthProvider provider(catalog_, nullptr);
    AuthUserInfo user_info{};
    std::string error_message;

    EXPECT_NE(provider.authenticatePeer("SYSTEM", 1001, 2001, 3001, user_info, error_message),
              AuthResult::SUCCESS);

    ErrorContext ctx;
    CatalogManager::PrincipalAccountCatalogInfo peer_account{};
    peer_account.account_id = generateUuidV7();
    peer_account.principal_name = "SYSTEM";
    peer_account.principal_kind = CatalogManager::PrincipalKind::USER;
    peer_account.source_scope_kind = CatalogManager::SourceScopeKind::PEER_UID;
    peer_account.has_source_scope_value = true;
    peer_account.source_scope_value = "1001";
    peer_account.auth_policy_id = generateUuidV7();
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(peer_account, &ctx), Status::OK)
        << ctx.message;

    ASSERT_EQ(provider.authenticatePeer("SYSTEM", 1001, 2001, 3001, user_info, error_message),
              AuthResult::SUCCESS);
    EXPECT_EQ(user_info.username, "SYSTEM");
    EXPECT_NE(user_info.authkey_id, ID{});
}

TEST_F(AuthBootstrapClaimTest, LockoutStatePersistsAcrossProviderInstances) {
    ErrorContext ctx;

    std::vector<CatalogManager::AuthPolicyCatalogInfo> policies;
    ASSERT_EQ(catalog_->listAuthPolicyCatalogEntries(policies, &ctx), Status::OK) << ctx.message;
    CatalogManager::AuthPolicyCatalogInfo policy{};
    if (policies.empty()) {
        std::vector<CatalogManager::AuthProviderCatalogInfo> providers;
        ASSERT_EQ(catalog_->listAuthProviderCatalogEntries(providers, &ctx), Status::OK) << ctx.message;

        ID provider_id{};
        if (providers.empty()) {
            CatalogManager::AuthProviderCatalogInfo provider{};
            provider.provider_id = generateUuidV7();
            provider.provider_name = "lockout_persist_provider";
            provider.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
            provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
            provider.priority_rank = 1;
            provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
            ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(provider, &ctx), Status::OK) << ctx.message;
            provider_id = provider.provider_id;
        } else {
            provider_id = providers.front().provider_id;
        }

        policy.policy_id = generateUuidV7();
        policy.policy_name = "lockout_persist_policy";
        policy.provider_chain = {provider_id};
    } else {
        policy = policies.front();
    }

    policy.lockout_threshold = 1;
    policy.lockout_window_ms = 600000;
    policy.lockout_duration_ms = 600000;
    ASSERT_EQ(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    const std::string username = "lockout_persist_user";
    const std::string password_hash = PasswordHash::hashPassword("CorrectPass!1");
    ASSERT_FALSE(password_hash.empty());

    ID user_id{};
    ASSERT_EQ(catalog_->createUser(username,
                                   password_hash,
                                   system_user.default_schema_id,
                                   false,
                                   user_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::PrincipalResolutionRequest resolve_req{};
    resolve_req.presented_principal_name = username;
    CatalogManager::PrincipalAccountCatalogInfo account{};
    Status resolve_status = catalog_->resolvePrincipalAccount(resolve_req, account, &ctx);
    if (resolve_status != Status::OK) {
        account = CatalogManager::PrincipalAccountCatalogInfo{};
        account.account_id = generateUuidV7();
        account.principal_name = username;
        account.principal_kind = CatalogManager::PrincipalKind::USER;
        account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
        account.auth_policy_id = policy.policy_id;
        ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
            << ctx.message;
    } else {
        account.auth_policy_id = policy.policy_id;
        account.is_locked = false;
        account.has_locked_reason = false;
        account.locked_reason.clear();
        ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
            << ctx.message;
    }

    LocalAuthProvider provider_a(catalog_, nullptr);
    LocalAuthProvider provider_b(catalog_, nullptr);
    AuthUserInfo user_info{};
    std::string error_message;

    EXPECT_EQ(provider_a.authenticate(username, "wrong-password", user_info, error_message),
              AuthResult::INVALID_CREDENTIALS);

    error_message.clear();
    EXPECT_EQ(provider_b.authenticate(username, "wrong-password", user_info, error_message),
              AuthResult::USER_LOCKED);

    provider_b.clearLoginAttempts(username);

    error_message.clear();
    EXPECT_EQ(provider_b.authenticate(username, "CorrectPass!1", user_info, error_message),
              AuthResult::SUCCESS);
}

TEST_F(AuthBootstrapClaimTest, LockoutScopeIsBoundToPeerIdentityAttributes) {
    ErrorContext ctx;

    std::vector<CatalogManager::AuthPolicyCatalogInfo> policies;
    ASSERT_EQ(catalog_->listAuthPolicyCatalogEntries(policies, &ctx), Status::OK) << ctx.message;
    CatalogManager::AuthPolicyCatalogInfo policy{};
    if (policies.empty()) {
        std::vector<CatalogManager::AuthProviderCatalogInfo> providers;
        ASSERT_EQ(catalog_->listAuthProviderCatalogEntries(providers, &ctx), Status::OK) << ctx.message;

        ID provider_id{};
        if (providers.empty()) {
            CatalogManager::AuthProviderCatalogInfo provider{};
            provider.provider_id = generateUuidV7();
            provider.provider_name = "lockout_scope_provider";
            provider.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
            provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
            provider.priority_rank = 1;
            provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
            ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(provider, &ctx), Status::OK) << ctx.message;
            provider_id = provider.provider_id;
        } else {
            provider_id = providers.front().provider_id;
        }

        policy.policy_id = generateUuidV7();
        policy.policy_name = "lockout_scope_policy";
        policy.provider_chain = {provider_id};
    } else {
        policy = policies.front();
    }

    policy.lockout_threshold = 1;
    policy.lockout_window_ms = 600000;
    policy.lockout_duration_ms = 600000;
    ASSERT_EQ(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    const std::string username = "lockout_scope_user";
    const std::string password_hash = PasswordHash::hashPassword("CorrectPass!1");
    ASSERT_FALSE(password_hash.empty());

    ID user_id{};
    ASSERT_EQ(catalog_->createUser(username,
                                   password_hash,
                                   system_user.default_schema_id,
                                   false,
                                   user_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::PrincipalResolutionRequest resolve_req{};
    resolve_req.presented_principal_name = username;
    CatalogManager::PrincipalAccountCatalogInfo account{};
    Status resolve_status = catalog_->resolvePrincipalAccount(resolve_req, account, &ctx);
    if (resolve_status != Status::OK) {
        account = CatalogManager::PrincipalAccountCatalogInfo{};
        account.account_id = generateUuidV7();
        account.principal_name = username;
        account.principal_kind = CatalogManager::PrincipalKind::USER;
        account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
        account.auth_policy_id = policy.policy_id;
        ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
            << ctx.message;
    } else {
        account.auth_policy_id = policy.policy_id;
        account.is_locked = false;
        account.has_locked_reason = false;
        account.locked_reason.clear();
        ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
            << ctx.message;
    }

    LocalAuthProvider provider_peer_a(catalog_, nullptr);
    LocalAuthProvider provider_peer_b(catalog_, nullptr);
    provider_peer_a.setPeerIdentityContext(true, 1001, 2001, 3001);
    provider_peer_b.setPeerIdentityContext(true, 1002, 2002, 3002);

    AuthUserInfo user_info{};
    std::string error_message;

    EXPECT_EQ(provider_peer_a.authenticate(username, "wrong-password", user_info, error_message),
              AuthResult::INVALID_CREDENTIALS);

    error_message.clear();
    EXPECT_EQ(provider_peer_a.authenticate(username, "CorrectPass!1", user_info, error_message),
              AuthResult::USER_LOCKED);

    error_message.clear();
    EXPECT_EQ(provider_peer_b.authenticate(username, "CorrectPass!1", user_info, error_message),
              AuthResult::SUCCESS);

    error_message.clear();
    EXPECT_EQ(provider_peer_a.authenticate(username, "CorrectPass!1", user_info, error_message),
              AuthResult::USER_LOCKED);
}

TEST_F(AuthBootstrapClaimTest, ScramBeginAndFinishRateLimitsAreSeparated) {
    ErrorContext ctx;

    std::vector<CatalogManager::AuthPolicyCatalogInfo> policies;
    ASSERT_EQ(catalog_->listAuthPolicyCatalogEntries(policies, &ctx), Status::OK) << ctx.message;
    CatalogManager::AuthPolicyCatalogInfo policy{};
    if (policies.empty()) {
        std::vector<CatalogManager::AuthProviderCatalogInfo> providers;
        ASSERT_EQ(catalog_->listAuthProviderCatalogEntries(providers, &ctx), Status::OK) << ctx.message;

        ID provider_id{};
        if (providers.empty()) {
            CatalogManager::AuthProviderCatalogInfo provider{};
            provider.provider_id = generateUuidV7();
            provider.provider_name = "scram_rate_scope_provider";
            provider.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
            provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
            provider.priority_rank = 1;
            provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
            ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(provider, &ctx), Status::OK) << ctx.message;
            provider_id = provider.provider_id;
        } else {
            provider_id = providers.front().provider_id;
        }

        policy.policy_id = generateUuidV7();
        policy.policy_name = "scram_rate_scope_policy";
        policy.provider_chain = {provider_id};
    } else {
        policy = policies.front();
    }

    policy.lockout_threshold = 1;
    policy.lockout_window_ms = 600000;
    policy.lockout_duration_ms = 600000;
    ASSERT_EQ(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    const std::string username = "scram_rate_scope_user";
    const std::string password_hash = PasswordHash::hashPassword("CorrectPass!1");
    ASSERT_FALSE(password_hash.empty());

    ID user_id{};
    ASSERT_EQ(catalog_->createUser(username,
                                   password_hash,
                                   system_user.default_schema_id,
                                   false,
                                   user_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::PrincipalResolutionRequest resolve_req{};
    resolve_req.presented_principal_name = username;
    CatalogManager::PrincipalAccountCatalogInfo account{};
    Status resolve_status = catalog_->resolvePrincipalAccount(resolve_req, account, &ctx);
    if (resolve_status != Status::OK) {
        account = CatalogManager::PrincipalAccountCatalogInfo{};
        account.account_id = generateUuidV7();
        account.principal_name = username;
        account.principal_kind = CatalogManager::PrincipalKind::USER;
        account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
        account.auth_policy_id = policy.policy_id;
        ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
            << ctx.message;
    } else {
        account.auth_policy_id = policy.policy_id;
        account.is_locked = false;
        account.has_locked_reason = false;
        account.locked_reason.clear();
        ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
            << ctx.message;
    }

    LocalAuthProvider provider(catalog_, nullptr);
    AuthUserInfo user_info{};
    std::string error_message;
    ScramAuthState begin_state{};
    std::string server_first;

    EXPECT_EQ(provider.beginScramAuth(username,
                                      "bad-scram-client-first",
                                      scratchbird::security::ScramAlgorithm::SHA_256,
                                      begin_state,
                                      server_first,
                                      error_message),
              AuthResult::INVALID_CREDENTIALS);

    error_message.clear();
    EXPECT_EQ(provider.beginScramAuth(username,
                                      "n,,n=scram_rate_scope_user,r=nonceA",
                                      scratchbird::security::ScramAlgorithm::SHA_256,
                                      begin_state,
                                      server_first,
                                      error_message),
              AuthResult::USER_LOCKED);

    ScramAuthState finish_state{};
    finish_state.username = username;
    std::string server_final;

    error_message.clear();
    EXPECT_EQ(provider.finishScramAuth(finish_state,
                                       "bad-scram-client-final",
                                       user_info,
                                       server_final,
                                       error_message),
              AuthResult::INVALID_CREDENTIALS);

    error_message.clear();
    EXPECT_EQ(provider.finishScramAuth(finish_state,
                                       "bad-scram-client-final",
                                       user_info,
                                       server_final,
                                       error_message),
              AuthResult::USER_LOCKED);
}

TEST_F(AuthBootstrapClaimTest, ScramPolicyRejectsWeakIterationsAndMarksUpgradeMetadata) {
    ErrorContext ctx;

    CatalogManager::AuthProviderCatalogInfo provider_row{};
    provider_row.provider_id = generateUuidV7();
    provider_row.provider_name = "weak_scram_policy_provider";
    provider_row.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
    provider_row.provider_state = CatalogManager::AuthProviderState::ENABLED;
    provider_row.priority_rank = 1;
    provider_row.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
    ASSERT_EQ(catalog_->upsertAuthProviderCatalogEntry(provider_row, &ctx), Status::OK)
        << ctx.message;

    CatalogManager::AuthPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = "weak_scram_policy";
    policy.provider_chain = {provider_row.provider_id};
    policy.allowed_auth_method_mask = CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256;
    policy.has_required_auth_method = true;
    policy.required_auth_method = CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256;
    policy.allowed_transport_mask = CatalogManager::AUTH_POLICY_TRANSPORT_LOCAL |
                                    CatalogManager::AUTH_POLICY_TRANSPORT_IPC;
    policy.min_scram_iterations = 8192;
    policy.mark_weak_scram_for_upgrade = true;
    ASSERT_EQ(catalog_->upsertAuthPolicyCatalogEntry(policy, &ctx), Status::OK) << ctx.message;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    const std::string username = "weak_scram_user";
    const std::string password_hash = buildScramSha256PasswordHash("CorrectPass!1", 4096);
    ASSERT_FALSE(password_hash.empty());

    ID user_id{};
    ASSERT_EQ(catalog_->createUser(username,
                                   password_hash,
                                   system_user.default_schema_id,
                                   false,
                                   user_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    CatalogManager::PrincipalAccountCatalogInfo account{};
    account.account_id = generateUuidV7();
    account.principal_name = username;
    account.principal_kind = CatalogManager::PrincipalKind::USER;
    account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
    account.auth_policy_id = policy.policy_id;
    ASSERT_EQ(catalog_->upsertPrincipalAccountCatalogEntry(account, &ctx), Status::OK)
        << ctx.message;

    LocalAuthProvider provider(catalog_, nullptr);
    ScramAuthState state{};
    std::string server_first;
    std::string error_message;

    EXPECT_EQ(provider.beginScramAuth(username,
                                      "n,,n=weak_scram_user,r=nonceWeak1",
                                      scratchbird::security::ScramAlgorithm::SHA_256,
                                      state,
                                      server_first,
                                      error_message),
              AuthResult::INVALID_CREDENTIALS);
    EXPECT_TRUE(server_first.empty());

    CatalogManager::UserInfo loaded_user{};
    ASSERT_EQ(catalog_->getUserByName(username, loaded_user, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(loaded_user.user_metadata.empty());

    Json metadata = Json::parse(loaded_user.user_metadata);
    ASSERT_TRUE(metadata.contains("auth"));
    ASSERT_TRUE(metadata["auth"].is_object());

    const Json& auth = metadata["auth"];
    EXPECT_TRUE(auth.value("scram_upgrade_required", false));
    EXPECT_EQ(auth.value("scram_algorithm", std::string{}), "sha256");
    EXPECT_EQ(auth.value("scram_observed_iterations", 0u), 4096u);
    EXPECT_EQ(auth.value("scram_min_required_iterations", 0u), 8192u);
    EXPECT_GT(auth.value("scram_marked_at", 0ull), 0ull);
}

TEST_F(AuthBootstrapClaimTest, DormantDetachIssuesReattachAuthKeyAndRequiresToken) {
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(db_->connect(conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(conn, nullptr);
    conn->setCurrentUser(system_user.user_id, true);
    conn->setProtocolSessionId(generateUuidV7());

    ID dormant_id{};
    ID reattach_authkey_id{};
    ASSERT_EQ(db_->detachToDormant(conn, dormant_id, &ctx, &reattach_authkey_id), Status::OK)
        << ctx.message;
    EXPECT_EQ(conn, nullptr);

    CatalogManager::AuthKeyInfo issued_key{};
    ASSERT_EQ(catalog_->getAuthKey(reattach_authkey_id, issued_key, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(issued_key.scope, CatalogManager::AuthKeyScope::REATTACH);
    EXPECT_EQ(issued_key.status, CatalogManager::AuthKeyStatus::ACTIVE);
    EXPECT_EQ(issued_key.usage_limit, 1u);

    std::unique_ptr<ConnectionContext> reattached;

    ErrorContext missing_ctx;
    EXPECT_EQ(db_->reattachDormant(dormant_id, reattached, &missing_ctx, nullptr),
              Status::INVALID_AUTHORIZATION);

    ID wrong_authkey = generateUuidV7();
    ErrorContext wrong_ctx;
    EXPECT_EQ(db_->reattachDormant(dormant_id, reattached, &wrong_ctx, &wrong_authkey),
              Status::INVALID_AUTHORIZATION);

    ErrorContext reattach_ctx;
    ASSERT_EQ(db_->reattachDormant(dormant_id, reattached, &reattach_ctx, &reattach_authkey_id),
              Status::OK)
        << reattach_ctx.message;
    ASSERT_NE(reattached, nullptr);

    CatalogManager::AuthKeyInfo consumed_key{};
    ASSERT_EQ(catalog_->getAuthKey(reattach_authkey_id, consumed_key, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(consumed_key.status, CatalogManager::AuthKeyStatus::EXPIRED);
    EXPECT_EQ(consumed_key.usage_count, 1u);
    EXPECT_GT(consumed_key.last_used_time, 0u);
}

TEST_F(AuthBootstrapClaimTest, DormantReattachRecoversReplacementTransactionAfterDatabaseClose) {
    const ScopedConfigValue restart_policy("transactions",
                                           "dormant_restart_reattach_policy",
                                           "allow_replacement",
                                           "allow_replacement");
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(db_->connect(conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(conn, nullptr);
    conn->setCurrentUser(system_user.user_id, true);
    conn->setProtocolSessionId(generateUuidV7());
    conn->setWaitForLocks(false);
    conn->setLockTimeout(9);
    const uint64_t original_xid = conn->getCurrentXid();

    ID dormant_id{};
    ID reattach_authkey_id{};
    ASSERT_EQ(db_->detachToDormant(conn, dormant_id, &ctx, &reattach_authkey_id), Status::OK)
        << ctx.message;
    EXPECT_EQ(conn, nullptr);

    db_->close();

    ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
    catalog_ = db_->catalog_manager();
    ASSERT_NE(catalog_, nullptr);

    std::unique_ptr<ConnectionContext> reattached;
    ErrorContext reattach_ctx;
    ASSERT_EQ(db_->reattachDormant(dormant_id, reattached, &reattach_ctx, &reattach_authkey_id),
              Status::OK)
        << reattach_ctx.message;
    ASSERT_NE(reattached, nullptr);
    EXPECT_NE(reattached->getCurrentXid(), 0u);
    EXPECT_NE(reattached->getCurrentXid(), original_xid);
    EXPECT_FALSE(reattached->getWaitForLocks());
    EXPECT_EQ(reattached->getLockTimeout(), 9u);
    EXPECT_TRUE(noticesContain(
        reattached->consumeNotices(),
        "Dormant transaction recovered after server restart"));

    CatalogManager::AuthKeyInfo consumed{};
    ASSERT_EQ(catalog_->getAuthKey(reattach_authkey_id, consumed, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(consumed.scope, CatalogManager::AuthKeyScope::REATTACH);
    EXPECT_EQ(consumed.status, CatalogManager::AuthKeyStatus::EXPIRED);

    std::vector<Database::DormantTransactionSnapshot> dormants;
    ASSERT_EQ(db_->snapshotDormantTransactions(dormants, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(dormants.begin(), dormants.end(), [&](const auto& row) {
        return row.dormant_id == dormant_id;
    });
    ASSERT_NE(it, dormants.end());
    EXPECT_EQ(it->state,
              static_cast<uint8_t>(CatalogManager::DormantTransactionState::REATTACHED));
}

TEST_F(AuthBootstrapClaimTest, DormantRestartPolicyCanDenyReattachAfterDatabaseClose) {
    const ScopedConfigValue restart_policy("transactions",
                                           "dormant_restart_reattach_policy",
                                           "deny_after_restart",
                                           "allow_replacement");
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(db_->connect(conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(conn, nullptr);
    conn->setCurrentUser(system_user.user_id, true);
    conn->setProtocolSessionId(generateUuidV7());

    ID dormant_id{};
    ID reattach_authkey_id{};
    ASSERT_EQ(db_->detachToDormant(conn, dormant_id, &ctx, &reattach_authkey_id), Status::OK)
        << ctx.message;

    db_->close();
    ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
    catalog_ = db_->catalog_manager();
    ASSERT_NE(catalog_, nullptr);

    std::vector<Database::DormantTransactionSnapshot> dormants;
    ASSERT_EQ(db_->snapshotDormantTransactions(dormants, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(dormants.begin(), dormants.end(), [&](const auto& row) {
        return row.dormant_id == dormant_id;
    });
    ASSERT_NE(it, dormants.end());
    EXPECT_EQ(it->state,
              static_cast<uint8_t>(CatalogManager::DormantTransactionState::EXPIRED));

    std::unique_ptr<ConnectionContext> rejected;
    ErrorContext rejected_ctx;
    EXPECT_EQ(db_->reattachDormant(dormant_id, rejected, &rejected_ctx, &reattach_authkey_id),
              Status::INVALID_AUTHORIZATION);
}

TEST_F(AuthBootstrapClaimTest, DormantCleanupPolicyRollsBackExpiredDormantAttachment) {
    const ScopedConfigValue cleanup_policy("transactions",
                                           "dormant_cleanup_policy",
                                           "rollback_expired",
                                           "rollback_expired");
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(db_->connect(conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(conn, nullptr);
    conn->setCurrentUser(system_user.user_id, true);
    conn->setProtocolSessionId(generateUuidV7());

    ID dormant_id{};
    ID reattach_authkey_id{};
    ASSERT_EQ(db_->detachToDormant(conn, dormant_id, &ctx, &reattach_authkey_id), Status::OK)
        << ctx.message;

    CatalogManager::DormantTransactionInfo info{};
    ASSERT_EQ(catalog_->getDormantTransaction(dormant_id, info, &ctx), Status::OK) << ctx.message;
    info.lease_expires_at = 1;
    ASSERT_EQ(catalog_->updateDormantTransaction(info, &ctx), Status::OK) << ctx.message;

    uint32_t normalized = 0;
    ASSERT_EQ(db_->maintainDormantTransactions(&normalized, &ctx), Status::OK) << ctx.message;
    EXPECT_GE(normalized, 1u);

    CatalogManager::DormantTransactionInfo refreshed{};
    ASSERT_EQ(catalog_->getDormantTransaction(dormant_id, refreshed, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(refreshed.state, CatalogManager::DormantTransactionState::ROLLED_BACK);

    CatalogManager::AuthKeyInfo reloaded_key{};
    ASSERT_EQ(catalog_->getAuthKey(reattach_authkey_id, reloaded_key, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(reloaded_key.status, CatalogManager::AuthKeyStatus::REVOKED);

    std::vector<Database::DormantTransactionSnapshot> dormants;
    ASSERT_EQ(db_->snapshotDormantTransactions(dormants, &ctx), Status::OK) << ctx.message;
    auto it = std::find_if(dormants.begin(), dormants.end(), [&](const auto& row) {
        return row.dormant_id == dormant_id;
    });
    ASSERT_NE(it, dormants.end());
    EXPECT_EQ(it->state,
              static_cast<uint8_t>(CatalogManager::DormantTransactionState::ROLLED_BACK));

    std::unique_ptr<ConnectionContext> rejected;
    ErrorContext rejected_ctx;
    EXPECT_EQ(db_->reattachDormant(dormant_id, rejected, &rejected_ctx, &reattach_authkey_id),
              Status::INVALID_AUTHORIZATION);
}

TEST_F(AuthBootstrapClaimTest, LocalAuthReloadsToastedUserStateAfterReopen) {
    ErrorContext ctx;

    CatalogManager::UserInfo system_user{};
    ASSERT_EQ(catalog_->getUserByName("SYSTEM", system_user, &ctx), Status::OK) << ctx.message;

    const std::string username = "reopen_toast_user";
    const std::string password_hash = PasswordHash::hashPassword("CorrectPass!1");
    ASSERT_FALSE(password_hash.empty());

    ID user_id{};
    ASSERT_EQ(catalog_->createUser(username,
                                   password_hash,
                                   system_user.default_schema_id,
                                   false,
                                   user_id,
                                   &ctx),
              Status::OK)
        << ctx.message;

    const Json metadata = Json::object({
        {"auth", Json::object({
            {"last_success", true},
            {"notes", "reopen regression"}
        })}
    });
    ASSERT_EQ(catalog_->updateUserMetadata(user_id, metadata.dump(), &ctx), Status::OK)
        << ctx.message;

    db_->close();

    ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
    catalog_ = db_->catalog_manager();
    ASSERT_NE(catalog_, nullptr);

    std::unique_ptr<ConnectionContext> conn;
    ASSERT_EQ(db_->connect(conn, &ctx), Status::OK) << ctx.message;
    ASSERT_NE(conn, nullptr);
    conn.reset();

    CatalogManager::UserInfo loaded_user{};
    ASSERT_EQ(catalog_->getUserByName(username, loaded_user, &ctx), Status::OK) << ctx.message;
    EXPECT_EQ(loaded_user.password_hash, password_hash);
    EXPECT_EQ(loaded_user.user_metadata, metadata.dump());

    LocalAuthProvider provider(catalog_, nullptr);
    AuthUserInfo user_info{};
    std::string error_message;
    EXPECT_EQ(provider.authenticate(username, "CorrectPass!1", user_info, error_message),
              AuthResult::SUCCESS)
        << error_message;
}

}  // namespace
