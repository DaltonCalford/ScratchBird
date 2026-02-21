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

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "scratchbird/client/connection.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/security/mfa_auth.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/server/server_session.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::client;
using namespace scratchbird::core;
using namespace scratchbird::protocol;
using namespace scratchbird::security;
using namespace scratchbird::server;

namespace {

bool isNetworkRestrictedError(const ErrorContext& ctx) {
    return ctx.message.find("Operation not permitted") != std::string::npos ||
           ctx.message.find("Permission denied") != std::string::npos;
}

std::string toUpperAscii(std::string value) {
    for (char& ch : value) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - ('a' - 'A'));
        }
    }
    return value;
}

bool equalsIgnoreCaseAscii(const std::string& lhs, const std::string& rhs) {
    return toUpperAscii(lhs) == toUpperAscii(rhs);
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

std::string makeUniqueDbPath(const std::string& prefix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "/tmp/" + prefix + "_" + std::to_string(getpid()) + "_" +
           std::to_string(now) + ".db";
}

std::string makeUniqueSocketPath(const std::string& prefix) {
    static std::atomic<uint32_t> counter{0};
    return scratchbird::testing::uniqueTestSocketPath(
        prefix + "_" + std::to_string(getpid()) + "_" + std::to_string(counter.fetch_add(1)));
}

std::string makeUniquePolicyName(const std::string& prefix) {
    static std::atomic<uint32_t> counter{0};
    return prefix + "_" + std::to_string(getpid()) + "_" + std::to_string(counter.fetch_add(1));
}

std::string makeCurrentTotpCode(const std::string& secret_base32) {
    const std::vector<uint8_t> secret = decodeBase32(secret_base32);
    if (secret.empty()) {
        return {};
    }
    return generateTotp(secret);
}

core::Status configurePolicyForUser(CatalogManager* catalog,
                                    const std::string& username,
                                    const std::string& policy_name,
                                    uint16_t allowed_method_mask,
                                    CatalogManager::ConnectionAuthMethod required_method,
                                    ErrorContext* ctx,
                                    bool attach_mfa_profile = false,
                                    uint32_t step_up_ttl_ms = 0) {
    ID mfa_policy_id{};
    if (attach_mfa_profile) {
        CatalogManager::MfaPolicyCatalogInfo mfa_policy{};
        mfa_policy.mfa_policy_id = generateUuidV7();
        mfa_policy.policy_name = policy_name + "_mfa";
        mfa_policy.primary_factor = CatalogManager::MfaFactorType::TOTP;
        mfa_policy.allow_recovery_codes = true;
        mfa_policy.allow_break_glass = true;
        mfa_policy.max_challenge_attempts = 3;
        mfa_policy.challenge_ttl_ms = 60000;
        mfa_policy.step_up_ttl_ms = step_up_ttl_ms == 0 ? 900000 : step_up_ttl_ms;
        core::Status mfa_status = catalog->upsertMfaPolicyCatalogEntry(mfa_policy, ctx);
        if (mfa_status != core::Status::OK) {
            return mfa_status;
        }
        mfa_policy_id = mfa_policy.mfa_policy_id;
    }

    CatalogManager::AuthProviderCatalogInfo provider{};
    provider.provider_id = generateUuidV7();
    provider.provider_name = policy_name + "_provider";
    provider.provider_kind = CatalogManager::AuthProviderKind::INTERNAL_SCRAM_SHA256;
    provider.provider_state = CatalogManager::AuthProviderState::ENABLED;
    provider.priority_rank = 1;
    provider.fail_mode = CatalogManager::AuthProviderFailMode::TRY_NEXT;
    core::Status status = catalog->upsertAuthProviderCatalogEntry(provider, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    CatalogManager::AuthPolicyCatalogInfo policy{};
    policy.policy_id = generateUuidV7();
    policy.policy_name = policy_name;
    policy.provider_chain = {provider.provider_id};
    policy.mfa_required = true;
    policy.allow_password_fallback = false;
    policy.allowed_auth_method_mask = allowed_method_mask;
    policy.has_required_auth_method = true;
    policy.required_auth_method = required_method;
    policy.has_mfa_policy = attach_mfa_profile;
    policy.mfa_policy_id = mfa_policy_id;
    policy.allowed_transport_mask = CatalogManager::AUTH_POLICY_TRANSPORT_IPC;
    policy.peer_mode = CatalogManager::AuthPeerMode::DISABLED;
    status = catalog->upsertAuthPolicyCatalogEntry(policy, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    std::vector<CatalogManager::PrincipalAccountCatalogInfo> accounts;
    status = catalog->listPrincipalAccountCatalogEntries(accounts, ctx);
    if (status != core::Status::OK && status != core::Status::NOT_FOUND) {
        return status;
    }

    CatalogManager::PrincipalAccountCatalogInfo account{};
    bool found_existing = false;
    for (const auto& row : accounts) {
        if (!equalsIgnoreCaseAscii(row.principal_name, username)) {
            continue;
        }
        if (row.source_scope_kind != CatalogManager::SourceScopeKind::ANY) {
            continue;
        }
        if (row.has_source_scope_value || !row.source_scope_value.empty()) {
            continue;
        }
        account = row;
        found_existing = true;
        break;
    }

    if (!found_existing) {
        account.account_id = generateUuidV7();
        account.principal_name = username;
        account.principal_kind = CatalogManager::PrincipalKind::USER;
        account.source_scope_kind = CatalogManager::SourceScopeKind::ANY;
        account.has_source_scope_value = false;
    }
    account.auth_policy_id = policy.policy_id;
    account.is_login_enabled = true;
    account.is_locked = false;
    account.has_locked_reason = false;
    return catalog->upsertPrincipalAccountCatalogEntry(account, ctx);
}

core::Status createTokenForUser(CatalogManager* catalog,
                                const CatalogManager::UserInfo& user,
                                const std::string& token_secret,
                                ID& authkey_id_out,
                                std::vector<uint8_t>& binding_out,
                                ErrorContext* ctx) {
    CatalogManager::AuthKeyInfo authkey{};
    authkey.issuer = "mfa_challenge_contract";
    authkey.status = CatalogManager::AuthKeyStatus::ACTIVE;
    authkey.usage_type = CatalogManager::AuthKeyUsage::LIMITED;
    authkey.usage_limit = 5;
    authkey.scope = CatalogManager::AuthKeyScope::API_TOKEN;
    authkey.binding_kind = CatalogManager::AuthKeyBindingKind::CLIENT_NONCE;
    authkey.binding_value = token_secret;

    core::Status status = catalog->createAuthKey(authkey, authkey_id_out, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    binding_out.assign(user.user_id.bytes.begin(), user.user_id.bytes.end());
    return core::Status::OK;
}

struct SessionThreadResult {
    core::Status status = core::Status::OK;
    std::string error_message;
};

class SessionThreadHarness {
public:
    SessionThreadHarness(core::Database* db, std::string socket_path)
        : db_(db), socket_path_(std::move(socket_path)) {}

    core::Status start(ErrorContext* ctx) {
        IPCServerConfig server_config("mfa_challenge_contract", IPCMethod::UNIX_SOCKET);
        server_config.socket_path = socket_path_;
        server_config.accept_timeout_ms = 5000;
        server_config.read_timeout_ms = 1000;
        server_config.write_timeout_ms = 1000;
        server_ = IPCServer::create(server_config, ctx);
        if (!server_) {
            return core::Status::CONNECTION_FAILURE;
        }

        core::Status status = server_->listen(ctx);
        if (status != core::Status::OK) {
            return status;
        }

        server_thread_ = std::thread([this]() {
            ErrorContext accept_ctx;
            auto conn = server_->accept(&accept_ctx);
            if (!conn) {
                result_.status = core::Status::CONNECTION_FAILURE;
                result_.error_message = accept_ctx.message.empty()
                    ? "accept failed"
                    : accept_ctx.message;
                return;
            }

            uint8_t server_session_id[16];
            generateSessionId(server_session_id);
            ServerSession session(conn.get(), db_, server_session_id);
            ErrorContext run_ctx;
            result_.status = session.run();
            if (result_.status != core::Status::OK && !run_ctx.message.empty()) {
                result_.error_message = run_ctx.message;
            }
        });
        return core::Status::OK;
    }

    void stop() {
        if (server_) {
            server_->close();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        server_.reset();
    }

    ~SessionThreadHarness() {
        stop();
    }

    const SessionThreadResult& result() const { return result_; }

private:
    core::Database* db_ = nullptr;
    std::string socket_path_;
    std::unique_ptr<IPCServer> server_;
    std::thread server_thread_;
    SessionThreadResult result_{};
};

class AuthMfaChallengeFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = makeUniqueDbPath("test_auth_mfa_challenge_flow");
        std::remove(db_path_.c_str());

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_path_, 16384, &ctx), core::Status::OK) << ctx.message;

        db_ = std::make_unique<Database>();
        ASSERT_EQ(db_->open(db_path_, &ctx), core::Status::OK) << ctx.message;
        catalog_ = db_->catalog_manager();
        ASSERT_NE(catalog_, nullptr);
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

TEST_F(AuthMfaChallengeFlowTest, TokenAuthWithMfaChallengeSucceeds) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    static const std::string kSecretBase32 = "JBSWY3DPEHPK3PXP";
    const std::string mfa_code = makeCurrentTotpCode(kSecretBase32);
    ASSERT_FALSE(mfa_code.empty());

    const std::string policy_name = makeUniquePolicyName("MFA_TOKEN_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar secret_env("SCRATCHBIRD_MFA_TOTP_SECRET_SYSARCH", kSecretBase32);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_TOKEN,
                                     CatalogManager::ConnectionAuthMethod::TOKEN,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    CatalogManager::UserInfo sysarch{};
    ASSERT_EQ(catalog_->getUserByName("SYSARCH", sysarch, &ctx), core::Status::OK) << ctx.message;

    ID authkey_id{};
    std::vector<uint8_t> token_binding;
    const std::string token_secret = "mfa-token-secret-001";
    ASSERT_EQ(createTokenForUser(catalog_, sysarch, token_secret, authkey_id, token_binding, &ctx),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_mfa_token");
    SessionThreadHarness server_thread(db_.get(), socket_path);
    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    ConnectionConfig config;
    config.database_name = "mfa_token_test";
    config.username = "SYSARCH";
    config.mfa_code = mfa_code;
    config.auth_token_authkey_id.assign(authkey_id.bytes.begin(), authkey_id.bytes.end());
    config.auth_token_secret = token_secret;
    config.auth_token_binding = token_binding;
    config.preferred_auth_methods = {protocol::AuthMethod::TOKEN};
    config.auto_start_server = false;
    config.ipc_method = IPCMethod::UNIX_SOCKET;
    config.socket_path = socket_path;

    Connection conn;
    core::Status connect_status = conn.connect(config, &ctx);
    EXPECT_EQ(connect_status, core::Status::OK) << ctx.message << " / " << conn.getLastError();
    if (connect_status == core::Status::OK) {
        EXPECT_TRUE(conn.isConnected());
    }

    conn.disconnect();
    server_thread.stop();
}

TEST_F(AuthMfaChallengeFlowTest, TokenAuthWithMfaPolicyFailsWhenCodeMissing) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    static const std::string kSecretBase32 = "JBSWY3DPEHPK3PXP";
    const std::string policy_name = makeUniquePolicyName("MFA_TOKEN_REQUIRED_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar secret_env("SCRATCHBIRD_MFA_TOTP_SECRET_SYSARCH", kSecretBase32);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_TOKEN,
                                     CatalogManager::ConnectionAuthMethod::TOKEN,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    CatalogManager::UserInfo sysarch{};
    ASSERT_EQ(catalog_->getUserByName("SYSARCH", sysarch, &ctx), core::Status::OK) << ctx.message;

    ID authkey_id{};
    std::vector<uint8_t> token_binding;
    const std::string token_secret = "mfa-token-secret-002";
    ASSERT_EQ(createTokenForUser(catalog_, sysarch, token_secret, authkey_id, token_binding, &ctx),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_mfa_token_missing_code");
    SessionThreadHarness server_thread(db_.get(), socket_path);
    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    ConnectionConfig config;
    config.database_name = "mfa_token_missing_code_test";
    config.username = "SYSARCH";
    config.auth_token_authkey_id.assign(authkey_id.bytes.begin(), authkey_id.bytes.end());
    config.auth_token_secret = token_secret;
    config.auth_token_binding = token_binding;
    config.preferred_auth_methods = {protocol::AuthMethod::TOKEN};
    config.auto_start_server = false;
    config.ipc_method = IPCMethod::UNIX_SOCKET;
    config.socket_path = socket_path;

    Connection conn;
    core::Status connect_status = conn.connect(config, &ctx);
    EXPECT_NE(connect_status, core::Status::OK);
    EXPECT_NE(conn.getLastError().find("no mfa_code"), std::string::npos);

    conn.disconnect();
    server_thread.stop();
}

TEST_F(AuthMfaChallengeFlowTest, ScramAuthWithMfaContinuationSucceeds) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    static const std::string kSecretBase32 = "JBSWY3DPEHPK3PXP";
    const std::string mfa_code = makeCurrentTotpCode(kSecretBase32);
    ASSERT_FALSE(mfa_code.empty());

    const std::string policy_name = makeUniquePolicyName("MFA_SCRAM_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar secret_env("SCRATCHBIRD_MFA_TOTP_SECRET_SYSARCH", kSecretBase32);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_mfa_scram");
    SessionThreadHarness server_thread(db_.get(), socket_path);
    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    ConnectionConfig config;
    config.database_name = "mfa_scram_test";
    config.username = "SYSARCH";
    config.password = "ScratchBirdBeta1!";
    config.mfa_code = mfa_code;
    config.preferred_auth_methods = {protocol::AuthMethod::SCRAM_SHA_256};
    config.auto_start_server = false;
    config.ipc_method = IPCMethod::UNIX_SOCKET;
    config.socket_path = socket_path;

    Connection conn;
    core::Status connect_status = conn.connect(config, &ctx);
    EXPECT_EQ(connect_status, core::Status::OK) << ctx.message << " / " << conn.getLastError();
    if (connect_status == core::Status::OK) {
        EXPECT_TRUE(conn.isConnected());
    }

    conn.disconnect();
    server_thread.stop();
}

TEST_F(AuthMfaChallengeFlowTest, PrivilegedQueryDeniedWhenStepUpExpired) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    static const std::string kSecretBase32 = "JBSWY3DPEHPK3PXP";
    const std::string mfa_code = makeCurrentTotpCode(kSecretBase32);
    ASSERT_FALSE(mfa_code.empty());

    const std::string policy_name = makeUniquePolicyName("MFA_STEP_UP_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar secret_env("SCRATCHBIRD_MFA_TOTP_SECRET_SYSARCH", kSecretBase32);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx,
                                     true,
                                     1),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_mfa_step_up");
    SessionThreadHarness server_thread(db_.get(), socket_path);
    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    ConnectionConfig config;
    config.database_name = "mfa_step_up_test";
    config.username = "SYSARCH";
    config.password = "ScratchBirdBeta1!";
    config.mfa_code = mfa_code;
    config.preferred_auth_methods = {protocol::AuthMethod::SCRAM_SHA_256};
    config.auto_start_server = false;
    config.ipc_method = IPCMethod::UNIX_SOCKET;
    config.socket_path = socket_path;

    Connection conn;
    core::Status connect_status = conn.connect(config, &ctx);
    ASSERT_EQ(connect_status, core::Status::OK) << ctx.message << " / " << conn.getLastError();
    ASSERT_TRUE(conn.isConnected());

    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    ResultSet privileged_results;
    const std::vector<uint8_t> invalid_bytecode = {0x00};
    core::Status privileged_status = conn.executeBytecode(
        invalid_bytecode, "SET ROLE analyst", &privileged_results, &ctx);
    EXPECT_NE(privileged_status, core::Status::OK);
    EXPECT_NE(conn.getLastError().find("MFA step-up required"), std::string::npos);

    ResultSet regular_results;
    core::Status regular_status = conn.executeBytecode(
        invalid_bytecode, "SELECT 1", &regular_results, &ctx);
    EXPECT_NE(regular_status, core::Status::OK);
    EXPECT_EQ(conn.getLastError().find("MFA step-up required"), std::string::npos);

    conn.disconnect();
    server_thread.stop();
}

}  // namespace
