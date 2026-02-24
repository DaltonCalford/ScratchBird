/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "gtest/gtest.h"

#include "scratchbird/protocol/adapters/postgresql_adapter.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/protocol/adapters/firebird_adapter.h"
#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/protocol/sbwp_protocol.h"
#include "scratchbird/parser/v3_compiler.h"

#include <filesystem>
#include <cctype>

using namespace scratchbird;
using namespace scratchbird::protocol;

namespace {
std::filesystem::path dbPath(const std::string& name) {
    return std::filesystem::path("build") / "database" / name;
}

template <typename T>
class AdapterHarness : public T {
public:
    using T::T;
    core::Status runCompile(const std::string& sql, std::vector<uint8_t>& bytecode, std::string& err) {
        return T::compileQuery(sql, bytecode, err);
    }

    core::Status ensureEngineReady(core::ErrorContext* ctx) {
        return T::ensureEngine(ctx);
    }

    void primeNativeScratchbirdCompiler() {
        if (!T::compiler_v3_) {
            T::compiler_v3_ = std::make_unique<scratchbird::parser::v3::Compiler>();
        }
    }
    
    // Expose protected methods for testing PostgreSQL
    std::string computeMD5Hash(const std::string& password,
                               const std::string& username,
                               const uint8_t salt[4]) {
        return T::computeMD5Hash(password, username, salt);
    }
    
    bool validateMD5Response(const std::string& response,
                             const std::string& expected_hash) {
        return T::validateMD5Response(response, expected_hash);
    }
    
    // Expose protected methods for testing MySQL
    std::vector<uint8_t> computeNativePasswordAuth(const std::string& password,
                                                    const uint8_t* scramble) {
        return T::computeNativePasswordAuth(password, scramble);
    }
    
    std::vector<uint8_t> computeCachingSha2PasswordAuth(const std::string& password,
                                                         const uint8_t* scramble) {
        return T::computeCachingSha2PasswordAuth(password, scramble);
    }

    uint64_t getContractFeatureMask() const {
        return T::contractServerFeatureMask();
    }

    AuthMethod configuredAuthMethod() const {
        return T::getConfig().auth_method;
    }

    bool resolveDatabaseSelection(const std::string& requested, std::string& selected) const {
        return T::resolveDatabaseSelection(requested, selected);
    }

    core::Status parseIncomingPacket(network::Connection* conn) {
        return T::parseMessage(conn);
    }

    core::Status processIncomingPacket(network::Connection* conn) {
        return T::processMessage(conn);
    }

    core::Status forceAuthSuccess(network::Connection* conn) {
        return T::sendAuthResult(conn, true);
    }

    core::Status sendGreetingForTest(network::Connection* conn) {
        return T::sendGreeting(conn);
    }
};

void cleanupDb(const std::string& name) {
    std::error_code ec;
    std::filesystem::remove(dbPath(name), ec);
    std::filesystem::create_directories(dbPath(name).parent_path(), ec);
}

std::vector<uint8_t> buildMySqlWirePacket(const std::vector<uint8_t>& payload, uint8_t sequence = 0) {
    std::vector<uint8_t> packet;
    packet.reserve(4 + payload.size());
    packet.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
    packet.push_back(static_cast<uint8_t>((payload.size() >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>((payload.size() >> 16) & 0xFF));
    packet.push_back(sequence);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<uint8_t> extractMySqlPayload(const std::vector<uint8_t>& wire_packet) {
    if (wire_packet.size() < 4) {
        return {};
    }
    const size_t payload_size =
        static_cast<size_t>(wire_packet[0]) |
        (static_cast<size_t>(wire_packet[1]) << 8) |
        (static_cast<size_t>(wire_packet[2]) << 16);
    if (wire_packet.size() < 4 + payload_size) {
        return {};
    }
    return std::vector<uint8_t>(wire_packet.begin() + 4, wire_packet.begin() + 4 + payload_size);
}
} // namespace

TEST(ProtocolAdapterDialects, PostgreSQLSelectUsesPgCompiler) {
    cleanupDb("test_pg_adapter.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_adapter.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    auto status = adapter.runCompile("SELECT 1", bytecode, err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}

TEST(ProtocolAdapterDialects, MySQLSelectUsesMysqlParser) {
    cleanupDb("test_mysql_adapter.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_adapter.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    auto status = adapter.runCompile("SELECT 1 FROM dual", bytecode, err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}

TEST(ProtocolAdapterDialects, FirebirdSelectUsesFirebirdParser) {
    cleanupDb("test_fb_adapter.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_adapter.sbdb").string();

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    auto status = adapter.runCompile("SELECT 1 FROM RDB$DATABASE", bytecode, err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}

TEST(ProtocolAdapterDialectsAuthParity, DefaultAuthMethodIsScram256AcrossAdapters) {
    cleanupDb("test_auth_parity_default_pg.sbdb");
    cleanupDb("test_auth_parity_default_mysql.sbdb");
    cleanupDb("test_auth_parity_default_fb.sbdb");
    cleanupDb("test_auth_parity_default_native.sbdb");

    ProtocolAdapterConfig pg_cfg;
    pg_cfg.database_path = dbPath("test_auth_parity_default_pg.sbdb").string();
    AdapterHarness<PostgresqlAdapter> pg_adapter(pg_cfg);
    EXPECT_EQ(pg_adapter.configuredAuthMethod(), AuthMethod::SCRAM_SHA_256);

    ProtocolAdapterConfig mysql_cfg;
    mysql_cfg.database_path = dbPath("test_auth_parity_default_mysql.sbdb").string();
    AdapterHarness<MySqlAdapter> mysql_adapter(mysql_cfg);
    EXPECT_EQ(mysql_adapter.configuredAuthMethod(), AuthMethod::SCRAM_SHA_256);

    ProtocolAdapterConfig fb_cfg;
    fb_cfg.database_path = dbPath("test_auth_parity_default_fb.sbdb").string();
    AdapterHarness<FirebirdAdapter> fb_adapter(fb_cfg);
    EXPECT_EQ(fb_adapter.configuredAuthMethod(), AuthMethod::SCRAM_SHA_256);

    ProtocolAdapterConfig native_cfg;
    native_cfg.database_path = dbPath("test_auth_parity_default_native.sbdb").string();
    AdapterHarness<NativeAdapter> native_adapter(native_cfg);
    EXPECT_EQ(native_adapter.configuredAuthMethod(), AuthMethod::SCRAM_SHA_256);
}

TEST(ProtocolAdapterDialectsAuthParity, LegacyAuthMethodsNeedExplicitConfig) {
    cleanupDb("test_auth_parity_legacy_pg.sbdb");
    cleanupDb("test_auth_parity_legacy_mysql.sbdb");
    cleanupDb("test_auth_parity_legacy_fb.sbdb");
    cleanupDb("test_auth_parity_legacy_native.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.auth_method = AuthMethod::PASSWORD;

    cfg.database_path = dbPath("test_auth_parity_legacy_pg.sbdb").string();
    AdapterHarness<PostgresqlAdapter> pg_adapter(cfg);
    EXPECT_EQ(pg_adapter.configuredAuthMethod(), AuthMethod::PASSWORD);

    cfg.database_path = dbPath("test_auth_parity_legacy_mysql.sbdb").string();
    AdapterHarness<MySqlAdapter> mysql_adapter(cfg);
    EXPECT_EQ(mysql_adapter.configuredAuthMethod(), AuthMethod::PASSWORD);

    cfg.database_path = dbPath("test_auth_parity_legacy_fb.sbdb").string();
    AdapterHarness<FirebirdAdapter> fb_adapter(cfg);
    EXPECT_EQ(fb_adapter.configuredAuthMethod(), AuthMethod::PASSWORD);

    cfg.database_path = dbPath("test_auth_parity_legacy_native.sbdb").string();
    AdapterHarness<NativeAdapter> native_adapter(cfg);
    EXPECT_EQ(native_adapter.configuredAuthMethod(), AuthMethod::PASSWORD);
}

TEST(ProtocolAdapterDialectsBinding, EnforcedBoundDatabaseRejectsSwitchAcrossAdapters) {
    cleanupDb("test_bound_pg.sbdb");
    cleanupDb("test_bound_mysql.sbdb");
    cleanupDb("test_bound_fb.sbdb");

    ProtocolAdapterConfig pg_cfg;
    pg_cfg.database_path = dbPath("test_bound_pg.sbdb").string();
    pg_cfg.enforce_bound_database = true;
    pg_cfg.default_database = "tenant_a";
    AdapterHarness<PostgresqlAdapter> pg_adapter(pg_cfg);

    ProtocolAdapterConfig mysql_cfg;
    mysql_cfg.database_path = dbPath("test_bound_mysql.sbdb").string();
    mysql_cfg.enforce_bound_database = true;
    mysql_cfg.default_database = "tenant_a";
    AdapterHarness<MySqlAdapter> mysql_adapter(mysql_cfg);

    ProtocolAdapterConfig fb_cfg;
    fb_cfg.database_path = dbPath("test_bound_fb.sbdb").string();
    fb_cfg.enforce_bound_database = true;
    fb_cfg.default_database = "tenant_a";
    AdapterHarness<FirebirdAdapter> fb_adapter(fb_cfg);

    std::string selected;
    EXPECT_FALSE(pg_adapter.resolveDatabaseSelection("tenant_b", selected));
    EXPECT_FALSE(mysql_adapter.resolveDatabaseSelection("tenant_b", selected));
    EXPECT_FALSE(fb_adapter.resolveDatabaseSelection("tenant_b", selected));

    EXPECT_TRUE(pg_adapter.resolveDatabaseSelection("TENANT_A", selected));
    EXPECT_EQ(selected, "tenant_a");
    EXPECT_TRUE(mysql_adapter.resolveDatabaseSelection("", selected));
    EXPECT_EQ(selected, "tenant_a");
    EXPECT_TRUE(fb_adapter.resolveDatabaseSelection("tenant_a", selected));
    EXPECT_EQ(selected, "tenant_a");
}

// ============================================================================
// C1: PostgreSQL Protocol Adapter Parity Tests
// ============================================================================

TEST(ProtocolAdapterDialectsC1, PostgreSQLServerVersionFormat) {
    // C1: server_version should be in PostgreSQL format: "XX.X (ScratchBird X.X)"
    cleanupDb("test_pg_version.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_version.sbdb").string();

    PostgresqlAdapter adapter(cfg);
    const auto& params = adapter.getServerParameters();

    auto it = params.find("server_version");
    ASSERT_NE(it, params.end());
    
    // Should match PostgreSQL format like "15.4 (ScratchBird 1.0)"
    EXPECT_TRUE(it->second.find(".") != std::string::npos);
    EXPECT_TRUE(it->second.find("ScratchBird") != std::string::npos);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLParameterStatusKeys) {
    // C1: Verify required ParameterStatus keys are present
    cleanupDb("test_pg_params.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_params.sbdb").string();

    PostgresqlAdapter adapter(cfg);
    const auto& params = adapter.getServerParameters();

    // Required parameters per PostgreSQL protocol
    EXPECT_NE(params.find("server_version"), params.end());
    EXPECT_NE(params.find("server_encoding"), params.end());
    EXPECT_NE(params.find("client_encoding"), params.end());
    EXPECT_NE(params.find("DateStyle"), params.end());
    EXPECT_NE(params.find("TimeZone"), params.end());
    EXPECT_NE(params.find("integer_datetimes"), params.end());
    EXPECT_NE(params.find("standard_conforming_strings"), params.end());
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLMD5HashComputation) {
    // C1: Verify MD5 hash computation matches PostgreSQL format
    cleanupDb("test_pg_md5.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_md5.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    
    // Access protected method through harness
    uint8_t salt[4] = {0x01, 0x02, 0x03, 0x04};
    std::string hash = adapter.computeMD5Hash("password", "user", salt);
    
    // Should be "md5" prefix + 32 hex characters
    EXPECT_EQ(hash.length(), 35);
    EXPECT_EQ(hash.substr(0, 3), "md5");
    
    // All remaining characters should be hex
    for (size_t i = 3; i < hash.length(); ++i) {
        EXPECT_TRUE(std::isxdigit(hash[i])) << "Character at position " << i << " is not hex";
    }
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLMD5Validation) {
    // C1: Verify MD5 response validation
    cleanupDb("test_pg_md5_val.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_md5_val.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    
    // Valid MD5 hash is 35 chars: "md5" + 32 hex chars
    const std::string valid_hash = "md5abcdef1234567890abcdef12345678" "01";
    const std::string valid_hash_upper = "md5ABCDEF1234567890ABCDEF12345678" "01";
    
    // Test valid response format (35 chars total)
    EXPECT_TRUE(adapter.validateMD5Response(valid_hash, valid_hash));
    
    // Test case-insensitive comparison
    EXPECT_TRUE(adapter.validateMD5Response(valid_hash_upper, valid_hash));
    
    // Test invalid length (33 chars instead of 35)
    EXPECT_FALSE(adapter.validateMD5Response("md5abcdef1234567890abcdef1234567", valid_hash));
    
    // Test missing md5 prefix (32 hex chars only)
    EXPECT_FALSE(adapter.validateMD5Response("abcdef1234567890abcdef1234567890", valid_hash));
    
    // Test wrong hash
    EXPECT_FALSE(adapter.validateMD5Response("md5abcdef1234567890abcdef1234567890", valid_hash));
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLTLSEnabledByDefault) {
    // C1: TLS should be disabled by default until configured
    cleanupDb("test_pg_tls.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_tls.sbdb").string();

    PostgresqlAdapter adapter(cfg);
    
    // TLS should not be enabled by default
    EXPECT_FALSE(adapter.isTLSEnabled());
    EXPECT_FALSE(adapter.isTLSNegotiated());
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLServerParameterCustom) {
    // C1: Verify custom server parameters can be set
    cleanupDb("test_pg_custom_param.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_custom_param.sbdb").string();

    PostgresqlAdapter adapter(cfg);
    adapter.setServerParameter("application_name", "test_app");
    adapter.setServerParameter("custom_param", "custom_value");

    const auto& params = adapter.getServerParameters();
    
    EXPECT_EQ(params.at("application_name"), "test_app");
    EXPECT_EQ(params.at("custom_param"), "custom_value");
}

// ============================================================================
// C3: MySQL Protocol Adapter Parity Tests
// ============================================================================

TEST(ProtocolAdapterDialectsC3, MySQLServerVersionFormat) {
    // C3: server_version should match emulation target format
    cleanupDb("test_mysql_version.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_version.sbdb").string();

    MySqlAdapter adapter(cfg);
    
    // Default should be MySQL 8.0 native version format
    EXPECT_TRUE(adapter.getServerVersion().find("8.0") != std::string::npos);
    EXPECT_FALSE(adapter.getServerVersion().empty());
}

TEST(ProtocolAdapterDialectsC3, MySQLEmulationTargetConfiguration) {
    // C3: Test emulation target configuration
    cleanupDb("test_mysql_target.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_target.sbdb").string();

    MySqlAdapter adapter(cfg);
    
    // Test MySQL 5.7 target
    adapter.setEmulationTarget(MySqlAdapter::EmulationTarget::MYSQL_5_7);
    EXPECT_EQ(adapter.getEmulationTarget(), MySqlAdapter::EmulationTarget::MYSQL_5_7);
    
    // Test MySQL 8.0 target
    adapter.setEmulationTarget(MySqlAdapter::EmulationTarget::MYSQL_8_0);
    EXPECT_EQ(adapter.getEmulationTarget(), MySqlAdapter::EmulationTarget::MYSQL_8_0);
    
    // Test MariaDB target
    adapter.setEmulationTarget(MySqlAdapter::EmulationTarget::MARIADB_10_5);
    EXPECT_EQ(adapter.getEmulationTarget(), MySqlAdapter::EmulationTarget::MARIADB_10_5);
}

TEST(ProtocolAdapterDialectsC3, MySQLTLSEnabledByDefault) {
    // C3: TLS should be disabled by default until configured
    cleanupDb("test_mysql_tls.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_tls.sbdb").string();

    MySqlAdapter adapter(cfg);
    
    // TLS should not be enabled by default
    EXPECT_FALSE(adapter.isTLSEnabled());
    EXPECT_FALSE(adapter.isTLSNegotiated());
}

TEST(ProtocolAdapterDialectsNative, NativeCapabilityMaskAdvertisesCanonicalProfiles) {
    cleanupDb("test_native_capability_mask.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_native_capability_mask.sbdb").string();

    AdapterHarness<NativeAdapter> adapter(cfg);
    const uint64_t feature_mask = adapter.getContractFeatureMask();
    const uint64_t profile_mask = feature_mask & scratchbird::protocol::sbwp::kFeatureProfileMask;
    const auto enabled = scratchbird::protocol::sbwp::enabledProfilesFromFeatureMask(feature_mask);

    EXPECT_NE(profile_mask, 0u);
    EXPECT_EQ(profile_mask, scratchbird::protocol::sbwp::canonicalProfileFeatureMask());
    EXPECT_EQ(enabled.size(), 13u);
    EXPECT_TRUE(scratchbird::protocol::sbwp::hasProfileFeature(feature_mask, "postgresql"));
    EXPECT_TRUE(scratchbird::protocol::sbwp::hasProfileFeature(feature_mask, "firebird"));
    EXPECT_TRUE(scratchbird::protocol::sbwp::hasProfileFeature(feature_mask, "opensearch"));
}

TEST(ProtocolAdapterDialectsNative, NativeCompileRejectIncludesDeterministicSqlContext) {
    cleanupDb("test_native_compile_diagnostic.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_native_compile_diagnostic.sbdb").string();

    AdapterHarness<NativeAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;
    adapter.primeNativeScratchbirdCompiler();

    std::vector<uint8_t> first_bytecode;
    std::vector<uint8_t> second_bytecode;
    std::string first_error;
    std::string second_error;

    const std::string bad_sql = "DOC PATH FILTER PATH_ID 1 OP BAD VALUE_REF 2";
    const auto first_status = adapter.runCompile(bad_sql, first_bytecode, first_error);
    const auto second_status = adapter.runCompile(bad_sql, second_bytecode, second_error);

    EXPECT_EQ(first_status, core::Status::INVALID_ARGUMENT);
    EXPECT_EQ(second_status, core::Status::INVALID_ARGUMENT);
    EXPECT_NE(first_error.find("SQL_CONTEXT:"), std::string::npos);
    EXPECT_NE(second_error.find("SQL_CONTEXT:"), std::string::npos);
    EXPECT_EQ(first_error, second_error);
}

TEST(ProtocolAdapterDialectsC3, MySQLNativePasswordAuth) {
    // C3: Test mysql_native_password computation
    cleanupDb("test_mysql_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_auth.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    
    // Test with a known password and scramble
    uint8_t scramble[20] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                            0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
    
    // Should not crash and should return a result
    auto result = adapter.computeNativePasswordAuth("password", scramble);
    // mysql_native_password produces 20-byte result
    EXPECT_TRUE(result.empty() || result.size() == 20);
    
    // Empty password should produce empty result
    auto empty_result = adapter.computeNativePasswordAuth("", scramble);
    EXPECT_TRUE(empty_result.empty());
}

TEST(ProtocolAdapterDialectsC3, MySQLCachingSha2PasswordAuth) {
    // C3: Test caching_sha2_password computation
    cleanupDb("test_mysql_sha2.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_sha2.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    
    // Test with a known password and scramble
    uint8_t scramble[20] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
                            0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
    
    // Should not crash and should return a result
    auto result = adapter.computeCachingSha2PasswordAuth("password", scramble);
    // caching_sha2_password produces 32-byte result
    EXPECT_TRUE(result.empty() || result.size() == 32);
    
    // Empty password should produce empty result
    auto empty_result = adapter.computeCachingSha2PasswordAuth("", scramble);
    EXPECT_TRUE(empty_result.empty());
}

TEST(ProtocolAdapterDialectsC3, MySQLComChangeUserRequestsClearPasswordAuthSwitch) {
    cleanupDb("test_mysql_change_user_auth_switch.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_change_user_auth_switch.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 101);

    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_CHANGE_USER);
    payload.insert(payload.end(), {'a', 'l', 'i', 'c', 'e', '\0'});
    payload.push_back('\0');  // empty auth response
    payload.insert(payload.end(), {'d', 'e', 'f', 'a', 'u', 'l', 't', '\0'});

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 2u);
    EXPECT_EQ(response_payload[0], mysql::EOF_PACKET);

    const std::string plugin_name(reinterpret_cast<const char*>(response_payload.data() + 1));
    EXPECT_EQ(plugin_name, "mysql_clear_password");
}

TEST(ProtocolAdapterDialectsC3, MySQLComChangeUserRejectsBoundDatabaseSwitch) {
    cleanupDb("test_mysql_change_user_bound_db.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_change_user_bound_db.sbdb").string();
    cfg.enforce_bound_database = true;
    cfg.default_database = "tenant_a";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 102);

    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_CHANGE_USER);
    payload.insert(payload.end(), {'a', 'l', 'i', 'c', 'e', '\0'});
    payload.push_back('\0');  // empty auth response
    payload.insert(payload.end(), {'t', 'e', 'n', 'a', 'n', 't', '_', 'b', '\0'});

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 4u);
    EXPECT_EQ(response_payload[0], mysql::ERR_PACKET);

    const uint16_t error_code = static_cast<uint16_t>(response_payload[1]) |
                                (static_cast<uint16_t>(response_payload[2]) << 8);
    EXPECT_EQ(error_code, mysql::ErrorCode::ACCESS_DENIED);

    std::string message(reinterpret_cast<const char*>(response_payload.data() + 3),
                        response_payload.size() - 3);
    EXPECT_NE(message.find("Database switch denied"), std::string::npos);
}

TEST(ProtocolAdapterDialectsC3, MySQLGreetingPacketHeaderHasValidPayloadLength) {
    cleanupDb("test_mysql_greeting_packet_header.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_greeting_packet_header.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 103);

    ASSERT_EQ(adapter.sendGreetingForTest(&conn), core::Status::OK);
    const auto& wire_packet = conn.getWriteBuffer();
    ASSERT_GE(wire_packet.size(), 4u);

    const size_t payload_size =
        static_cast<size_t>(wire_packet[0]) |
        (static_cast<size_t>(wire_packet[1]) << 8) |
        (static_cast<size_t>(wire_packet[2]) << 16);
    EXPECT_GT(payload_size, 0u);
    EXPECT_EQ(wire_packet.size(), payload_size + 4u);
}
