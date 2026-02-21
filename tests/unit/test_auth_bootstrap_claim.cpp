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
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>
#include <openssl/hmac.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/telemetry.h"

using namespace scratchbird::core;

namespace {

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

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
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

TEST_F(AuthBootstrapClaimTest, DormantReattachAuthKeyRevokedOnDatabaseClose) {
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

    db_->close();

    ASSERT_EQ(db_->open(db_path_, &ctx), Status::OK) << ctx.message;
    catalog_ = db_->catalog_manager();
    ASSERT_NE(catalog_, nullptr);

    CatalogManager::AuthKeyInfo reloaded{};
    ASSERT_EQ(catalog_->getAuthKey(reattach_authkey_id, reloaded, &ctx), Status::OK)
        << ctx.message;
    EXPECT_EQ(reloaded.scope, CatalogManager::AuthKeyScope::REATTACH);
    EXPECT_EQ(reloaded.status, CatalogManager::AuthKeyStatus::REVOKED);
}

}  // namespace
