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
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
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
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/server/server_session.h"
#include "test_helpers.h"

using namespace scratchbird;
using namespace scratchbird::client;
using namespace scratchbird::core;
using namespace scratchbird::protocol;
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

uint64_t fnv1a64(const std::string& value, uint64_t seed) {
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = seed;
    for (unsigned char c : value) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
    return hash;
}

std::array<uint8_t, 16> deriveBindingUuid(const std::string& database_name) {
    std::array<uint8_t, 16> out{};
    const uint64_t hi = fnv1a64(database_name, 1469598103934665603ull);
    const uint64_t lo = fnv1a64(database_name, 1099511628211ull);
    for (int i = 0; i < 8; ++i) {
        out[static_cast<size_t>(i)] = static_cast<uint8_t>((hi >> (i * 8)) & 0xFF);
        out[static_cast<size_t>(8 + i)] = static_cast<uint8_t>((lo >> (i * 8)) & 0xFF);
    }
    out[6] = static_cast<uint8_t>((out[6] & 0x0F) | 0x40);
    out[8] = static_cast<uint8_t>((out[8] & 0x3F) | 0x80);
    return out;
}

core::Status configurePolicyForUser(CatalogManager* catalog,
                                    const std::string& username,
                                    const std::string& policy_name,
                                    uint16_t allowed_method_mask,
                                    CatalogManager::ConnectionAuthMethod required_method,
                                    ErrorContext* ctx) {
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
    policy.mfa_required = false;
    policy.allow_password_fallback = false;
    policy.allowed_auth_method_mask = allowed_method_mask;
    policy.has_required_auth_method = true;
    policy.required_auth_method = required_method;
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

core::Status configureIngressRuleProfile(CatalogManager* catalog,
                                         const std::string& profile_name,
                                         CatalogManager::ConnectionRuleTransportKind allow_transport,
                                         ErrorContext* ctx) {
    CatalogManager::ConnectionRuleCatalogInfo allow{};
    allow.rule_id = generateUuidV7();
    allow.profile_scope = profile_name;
    allow.rule_order = 1;
    allow.transport_kind = allow_transport;
    allow.action = CatalogManager::ConnectionRuleAction::ALLOW;
    core::Status status = catalog->upsertConnectionRuleCatalogEntry(allow, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    CatalogManager::ConnectionRuleCatalogInfo deny{};
    deny.rule_id = generateUuidV7();
    deny.profile_scope = profile_name;
    deny.rule_order = 2;
    deny.transport_kind = allow_transport;
    deny.action = CatalogManager::ConnectionRuleAction::DENY;
    return catalog->upsertConnectionRuleCatalogEntry(deny, ctx);
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
        IPCServerConfig server_config("auth_profile_parity", IPCMethod::UNIX_SOCKET);
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
            result_.status = session.run();
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

private:
    core::Database* db_ = nullptr;
    std::string socket_path_;
    std::unique_ptr<IPCServer> server_;
    std::thread server_thread_;
    SessionThreadResult result_{};
};

struct ProtocolProfile {
    std::string name;
    std::vector<AuthMethod> preferred_methods;
    std::string client_name;
};

class AuthPolicyProtocolParityTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_path_ = makeUniqueDbPath("test_auth_policy_protocol_parity");
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

    core::Status connectWithProfile(const ProtocolProfile& profile,
                                    const std::string& socket_path,
                                    std::string* last_error_out,
                                    ErrorContext* ctx,
                                    uint16_t connect_flags = 0,
                                    const std::array<uint8_t, 16>* bound_db_uuid = nullptr,
                                    const std::string& database_name = "auth_policy_protocol_parity") {
        ConnectionConfig config;
        config.database_name = database_name;
        config.client_name = profile.client_name.empty() ? "scratchbird_client" : profile.client_name;
        config.username = "SysArch";
        config.password = "replaceme";
        config.preferred_auth_methods = profile.preferred_methods;
        config.auto_start_server = false;
        config.ipc_method = IPCMethod::UNIX_SOCKET;
        config.socket_path = socket_path;
        config.connect_client_flags = connect_flags;
        if (bound_db_uuid) {
            config.has_bound_db_uuid = true;
            config.bound_db_uuid = *bound_db_uuid;
        }

        Connection conn;
        core::Status status = conn.connect(config, ctx);
        if (last_error_out) {
            *last_error_out = conn.getLastError();
        }
        conn.disconnect();
        return status;
    }

    std::string db_path_;
    std::unique_ptr<Database> db_;
    CatalogManager* catalog_ = nullptr;
};

TEST_F(AuthPolicyProtocolParityTest, StrictScramPolicyAllowsAllProtocolProfiles) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("SCRAM_PARITY_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    const std::vector<ProtocolProfile> profiles = {
        {"native", {AuthMethod::SCRAM_SHA_256}},
        {"postgresql", {AuthMethod::SCRAM_SHA_256, AuthMethod::PASSWORD, AuthMethod::MD5}},
        {"mysql", {AuthMethod::SCRAM_SHA_256, AuthMethod::SCRAM_SHA_512, AuthMethod::PASSWORD, AuthMethod::MD5}},
        {"firebird", {AuthMethod::SCRAM_SHA_256, AuthMethod::SCRAM_SHA_512, AuthMethod::PASSWORD, AuthMethod::MD5}},
    };

    for (const auto& profile : profiles) {
        const std::string socket_path = makeUniqueSocketPath("auth_parity_scram_" + profile.name);
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        std::string last_error;
        core::Status connect_status = connectWithProfile(profile, socket_path, &last_error, &ctx);
        EXPECT_EQ(connect_status, core::Status::OK)
            << "profile=" << profile.name << " ctx=" << ctx.message << " last_error=" << last_error;

        server_thread.stop();
    }
}

TEST_F(AuthPolicyProtocolParityTest, MySqlParserBridgeAllowsPasswordCompatUnderStrictScramPolicy) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("MYSQL_BRIDGE_PASSWORD_COMPAT");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    const ProtocolProfile generic_password_only = {
        "generic_password_only",
        {AuthMethod::PASSWORD},
        "scratchbird_client",
    };
    std::string generic_last_error;
    ErrorContext generic_ctx;
    {
        ScopedEnvVar required_method_env("SCRATCHBIRD_AUTH_REQUIRED_METHODS", "PASSWORD");
        const std::string socket_path = makeUniqueSocketPath("auth_mysql_bridge_generic_password");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&generic_ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(generic_ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << generic_ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << generic_ctx.message;

        core::Status generic_status = connectWithProfile(
            generic_password_only, socket_path, &generic_last_error, &generic_ctx);
        EXPECT_NE(generic_status, core::Status::OK)
            << "ctx=" << generic_ctx.message << " last_error=" << generic_last_error;

        server_thread.stop();
    }

    const ProtocolProfile mysql_parser_bridge = {
        "mysql_parser_bridge",
        {AuthMethod::PASSWORD},
        "sb_parser_mysql",
    };
    std::string mysql_last_error;
    ErrorContext mysql_ctx;
    {
        ScopedEnvVar required_method_env("SCRATCHBIRD_AUTH_REQUIRED_METHODS", "PASSWORD");
        const std::string socket_path = makeUniqueSocketPath("auth_mysql_bridge_parser_password");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&mysql_ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(mysql_ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << mysql_ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << mysql_ctx.message;

        core::Status mysql_status = connectWithProfile(
            mysql_parser_bridge, socket_path, &mysql_last_error, &mysql_ctx);
        EXPECT_EQ(mysql_status, core::Status::OK)
            << "ctx=" << mysql_ctx.message << " last_error=" << mysql_last_error;

        server_thread.stop();
    }
}

TEST_F(AuthPolicyProtocolParityTest, TokenOnlyPolicyDeniesPasswordProfiles) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("TOKEN_ONLY_PARITY_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_TOKEN,
                                     CatalogManager::ConnectionAuthMethod::TOKEN,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    const std::vector<ProtocolProfile> profiles = {
        {"native", {AuthMethod::SCRAM_SHA_256}},
        {"postgresql", {AuthMethod::SCRAM_SHA_256, AuthMethod::PASSWORD, AuthMethod::MD5}},
        {"mysql", {AuthMethod::SCRAM_SHA_256, AuthMethod::SCRAM_SHA_512, AuthMethod::PASSWORD, AuthMethod::MD5}},
        {"firebird", {AuthMethod::SCRAM_SHA_256, AuthMethod::SCRAM_SHA_512, AuthMethod::PASSWORD, AuthMethod::MD5}},
    };

    for (const auto& profile : profiles) {
        const std::string socket_path = makeUniqueSocketPath("auth_parity_token_" + profile.name);
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        std::string last_error;
        core::Status connect_status = connectWithProfile(profile, socket_path, &last_error, &ctx);
        EXPECT_NE(connect_status, core::Status::OK)
            << "profile=" << profile.name << " unexpectedly connected";
        EXPECT_FALSE(last_error.empty()) << "profile=" << profile.name;

        server_thread.stop();
    }
}

TEST_F(AuthPolicyProtocolParityTest, IngressRuleChainAppliesTrustedProxyChecksAtConnect) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("INGRESS_PROXY_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    ASSERT_EQ(configureIngressRuleProfile(
                  catalog_,
                  "native",
                  CatalogManager::ConnectionRuleTransportKind::UNIX_SOCKET,
                  &ctx),
              core::Status::OK)
        << ctx.message;

    const ProtocolProfile profile = {"native", {AuthMethod::SCRAM_SHA_256}};

    {
        const std::string socket_path = makeUniqueSocketPath("auth_ingress_direct");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        std::string last_error;
        core::Status connect_status = connectWithProfile(profile, socket_path, &last_error, &ctx);
        EXPECT_EQ(connect_status, core::Status::OK)
            << "ctx=" << ctx.message << " last_error=" << last_error;

        server_thread.stop();
    }

    {
        const std::string socket_path = makeUniqueSocketPath("auth_ingress_manager");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        const auto bound_uuid = deriveBindingUuid("auth_policy_protocol_parity");
        std::string last_error;
        core::Status connect_status = connectWithProfile(profile,
                                                         socket_path,
                                                         &last_error,
                                                         &ctx,
                                                         CONNECT_FLAG_MANAGER_DBBT,
                                                         &bound_uuid);
        EXPECT_NE(connect_status, core::Status::OK);
        EXPECT_TRUE(last_error.find("SEC_1230") != std::string::npos ||
                    last_error.find("SEC_1231") != std::string::npos ||
                    last_error.find("SEC_1233") != std::string::npos)
            << "ctx=" << ctx.message << " last_error=" << last_error;

        server_thread.stop();
    }
}

TEST_F(AuthPolicyProtocolParityTest, ManagerBoundConnectRejectsDatabaseUuidMismatch) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("INGRESS_PROXY_UUID_MISMATCH");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar ingress_env("SCRATCHBIRD_CONNECTION_RULE_PROFILE", "native");

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    ASSERT_EQ(configureIngressRuleProfile(
                  catalog_,
                  "native",
                  CatalogManager::ConnectionRuleTransportKind::UNIX_SOCKET,
                  &ctx),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_ingress_uuid_mismatch");
    SessionThreadHarness server_thread(db_.get(), socket_path);

    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    std::array<uint8_t, 16> wrong_uuid = deriveBindingUuid("auth_policy_protocol_parity");
    wrong_uuid[0] ^= 0xFF;

    const ProtocolProfile profile = {"native", {AuthMethod::SCRAM_SHA_256}};
    std::string last_error;
    core::Status connect_status = connectWithProfile(profile,
                                                     socket_path,
                                                     &last_error,
                                                     &ctx,
                                                     CONNECT_FLAG_MANAGER_DBBT,
                                                     &wrong_uuid);
    EXPECT_NE(connect_status, core::Status::OK);
    EXPECT_NE(last_error.find("database UUID mismatch"), std::string::npos)
        << "ctx=" << ctx.message << " last_error=" << last_error;

    server_thread.stop();
}

TEST_F(AuthPolicyProtocolParityTest, BoundDatabaseUuidFlagRequiresBoundUuidPayload) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("BOUND_UUID_REQUIRED");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar ingress_env("SCRATCHBIRD_CONNECTION_RULE_PROFILE", "native");

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    ASSERT_EQ(configureIngressRuleProfile(
                  catalog_,
                  "native",
                  CatalogManager::ConnectionRuleTransportKind::UNIX_SOCKET,
                  &ctx),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_bound_uuid_required");
    SessionThreadHarness server_thread(db_.get(), socket_path);

    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    const ProtocolProfile profile = {"native", {AuthMethod::SCRAM_SHA_256}};
    std::string last_error;
    core::Status connect_status = connectWithProfile(profile,
                                                     socket_path,
                                                     &last_error,
                                                     &ctx,
                                                     CONNECT_FLAG_BOUND_DB_UUID,
                                                     nullptr);
    EXPECT_NE(connect_status, core::Status::OK);
    EXPECT_NE(last_error.find("database UUID"), std::string::npos)
        << "ctx=" << ctx.message << " last_error=" << last_error;

    server_thread.stop();
}

TEST_F(AuthPolicyProtocolParityTest, BoundDatabaseUuidFlagRejectsUuidMismatch) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("BOUND_UUID_MISMATCH");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar ingress_env("SCRATCHBIRD_CONNECTION_RULE_PROFILE", "native");

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    ASSERT_EQ(configureIngressRuleProfile(
                  catalog_,
                  "native",
                  CatalogManager::ConnectionRuleTransportKind::UNIX_SOCKET,
                  &ctx),
              core::Status::OK)
        << ctx.message;

    const std::string socket_path = makeUniqueSocketPath("auth_bound_uuid_mismatch");
    SessionThreadHarness server_thread(db_.get(), socket_path);

    core::Status start_status = server_thread.start(&ctx);
    if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
        GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
    }
    ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

    std::array<uint8_t, 16> wrong_uuid = deriveBindingUuid("auth_policy_protocol_parity");
    wrong_uuid[0] ^= 0xAA;

    const ProtocolProfile profile = {"native", {AuthMethod::SCRAM_SHA_256}};
    std::string last_error;
    core::Status connect_status = connectWithProfile(profile,
                                                     socket_path,
                                                     &last_error,
                                                     &ctx,
                                                     CONNECT_FLAG_BOUND_DB_UUID,
                                                     &wrong_uuid);
    EXPECT_NE(connect_status, core::Status::OK);
    EXPECT_NE(last_error.find("database UUID mismatch"), std::string::npos)
        << "ctx=" << ctx.message << " last_error=" << last_error;

    server_thread.stop();
}

TEST_F(AuthPolicyProtocolParityTest, ScopedAuthPolicyNameUsesEmulationDatabaseScope) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string native_policy = makeUniquePolicyName("NATIVE_SCOPE_POLICY");
    const std::string postgresql_policy = makeUniquePolicyName("PG_SCOPE_POLICY");
    ScopedEnvVar global_policy("SCRATCHBIRD_AUTH_POLICY_NAME", "");
    ScopedEnvVar native_policy_env("SCRATCHBIRD_AUTH_POLICY_NAME_NATIVE", native_policy);
    ScopedEnvVar pg_policy_env("SCRATCHBIRD_AUTH_POLICY_NAME_POSTGRESQL", postgresql_policy);

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     native_policy,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     postgresql_policy,
                                     CatalogManager::AUTH_POLICY_METHOD_TOKEN,
                                     CatalogManager::ConnectionAuthMethod::TOKEN,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    const ProtocolProfile profile = {
        "postgresql", {AuthMethod::SCRAM_SHA_256, AuthMethod::PASSWORD, AuthMethod::MD5}};
    std::string last_error;

    {
        const std::string socket_path = makeUniqueSocketPath("auth_policy_scope_profile_native");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        core::Status native_status = connectWithProfile(profile, socket_path, &last_error, &ctx);
        EXPECT_EQ(native_status, core::Status::OK)
            << "ctx=" << ctx.message << " last_error=" << last_error;
    }

    {
        const std::string socket_path = makeUniqueSocketPath("auth_policy_scope_profile_emulated");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        core::Status emulated_status = connectWithProfile(
            profile,
            socket_path,
            &last_error,
            &ctx,
            0,
            nullptr,
            "remote.emulation.postgresql.localhost.databases.auth_policy_protocol_parity");
        EXPECT_NE(emulated_status, core::Status::OK);
        EXPECT_TRUE(
            last_error.find("TOKEN authentication requires auth_token_authkey_id") !=
                std::string::npos ||
            last_error.find("AUTH_POLICY_METHOD_DENIED") != std::string::npos ||
            last_error.find("AUTH_POLICY_REQUIRED_METHOD") != std::string::npos)
            << "ctx=" << ctx.message << " last_error=" << last_error;
    }
}

TEST_F(AuthPolicyProtocolParityTest, ScopedAuthPinningUsesEmulationDatabaseScope) {
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const std::string policy_name = makeUniquePolicyName("PINNING_SCOPE_POLICY");
    ScopedEnvVar policy_env("SCRATCHBIRD_AUTH_POLICY_NAME", policy_name);
    ScopedEnvVar required_global("SCRATCHBIRD_AUTH_REQUIRED_METHODS", "");
    ScopedEnvVar required_pg("SCRATCHBIRD_AUTH_REQUIRED_METHODS_POSTGRESQL", "TOKEN");

    ErrorContext ctx;
    ASSERT_EQ(configurePolicyForUser(catalog_,
                                     "SYSARCH",
                                     policy_name,
                                     CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256,
                                     CatalogManager::ConnectionAuthMethod::SCRAM_SHA_256,
                                     &ctx),
              core::Status::OK)
        << ctx.message;

    const ProtocolProfile profile = {
        "postgresql", {AuthMethod::SCRAM_SHA_256, AuthMethod::PASSWORD, AuthMethod::MD5}};
    std::string last_error;

    {
        const std::string socket_path = makeUniqueSocketPath("auth_policy_scope_pinning_native");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        core::Status native_status = connectWithProfile(profile, socket_path, &last_error, &ctx);
        EXPECT_EQ(native_status, core::Status::OK)
            << "ctx=" << ctx.message << " last_error=" << last_error;
    }

    {
        const std::string socket_path = makeUniqueSocketPath("auth_policy_scope_pinning_emulated");
        SessionThreadHarness server_thread(db_.get(), socket_path);

        core::Status start_status = server_thread.start(&ctx);
        if (start_status != core::Status::OK && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Socket setup restricted: " << ctx.message;
        }
        ASSERT_EQ(start_status, core::Status::OK) << ctx.message;

        core::Status emulated_status = connectWithProfile(
            profile,
            socket_path,
            &last_error,
            &ctx,
            0,
            nullptr,
            "remote.emulation.postgresql.localhost.databases.auth_policy_protocol_parity");
        EXPECT_NE(emulated_status, core::Status::OK);
        EXPECT_NE(last_error.find("AUTH_CLIENT_PINNING_VIOLATION"), std::string::npos)
            << "ctx=" << ctx.message << " last_error=" << last_error;
    }
}

}  // namespace
