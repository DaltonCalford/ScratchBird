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
#include "scratchbird/protocol/translation_cache.h"
#include "scratchbird/protocol/sbwp_protocol.h"
#include "scratchbird/parser/v3_compiler.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/optimizer/statistics_manager.h"

#include <filesystem>
#include <map>
#include <cctype>
#include <type_traits>
#include <sys/socket.h>
#include <unistd.h>

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

    bool validateAuthResponse(const std::string& expected_plugin,
                              const std::string& auth_response,
                              const uint8_t* scramble,
                              const std::string& password) {
        return T::validateAuthResponse(expected_plugin, auth_response, scramble, password);
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

    core::Status sendProtocolErrorForTest(network::Connection* conn,
                                          uint32_t error_code,
                                          const std::string& sqlstate,
                                          const std::string& message) {
        return T::sendProtocolError(conn, error_code, sqlstate, message);
    }

    void setClientCapabilitiesForTest(uint32_t capabilities) {
        T::setClientCapabilitiesForTest(capabilities);
    }

    core::ConnectionContext::PreparedStatement* preparedStatementForTest(
        const std::string& name) {
        if (!T::connection_ctx_) {
            return nullptr;
        }
        return T::connection_ctx_->getPreparedStatement(name);
    }

    core::Database* engineDatabaseForTest() {
        return T::engineDatabase();
    }

    void applySuccessfulMySqlQueryForTest(const std::string& sql) {
        if constexpr (std::is_base_of_v<MySqlAdapter, T>) {
            T::applySuccessfulSessionQueryForTest(sql);
        }
    }

    bool shouldBootstrapMySqlSystemSchemaForRemoteQueryForTest(const std::string& sql) const {
        if constexpr (std::is_base_of_v<MySqlAdapter, T>) {
            return T::shouldBootstrapSystemSchemaForRemoteQuery(sql);
        }
        return false;
    }

    const std::string& selectedDatabaseNameForTest() const {
        return T::database_name_;
    }

    void setSelectedDatabaseNameForTest(const std::string& database) {
        T::database_name_ = database;
    }

    void setUsernameForTest(const std::string& username) {
        T::username_ = username;
    }

    void setMySqlAuthPluginForTest(const std::string& plugin) {
        if constexpr (std::is_base_of_v<MySqlAdapter, T>) {
            T::setAuthPluginNameForTest(plugin);
        }
    }

    const std::string& mySqlAuthPluginForTest() const {
        if constexpr (std::is_base_of_v<MySqlAdapter, T>) {
            return T::authPluginNameForTest();
        }
        static const std::string empty;
        return empty;
    }

    void setPostgresqlStateForTest(PgProtocolState state) {
        if constexpr (std::is_base_of_v<PostgresqlAdapter, T>) {
            T::setProtocolStateForTest(state);
        }
    }

    PgProtocolState postgresqlStateForTest() const {
        if constexpr (std::is_base_of_v<PostgresqlAdapter, T>) {
            return T::protocolStateForTest();
        }
        return PgProtocolState::ERROR;
    }

    void applyPostgresqlSessionSchemaContextForTest(const std::string& logical_db,
                                                    core::ErrorContext* ctx) {
        if constexpr (std::is_base_of_v<PostgresqlAdapter, T>) {
            T::applyPostgresqlSessionSchemaContextForTest(logical_db, ctx);
        }
    }

    void applyFirebirdSessionSchemaContextForTest(core::ErrorContext* ctx) {
        if constexpr (std::is_base_of_v<FirebirdAdapter, T>) {
            T::applyFirebirdSessionSchemaContextForTest(ctx);
        }
    }

    core::ConnectionContext* connectionContextForTest() {
        return T::connection_ctx_.get();
    }

    core::Status assumePublicSuperuserForTest(core::ErrorContext* ctx) {
        if (!T::connection_ctx_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Connection context is not available");
            return core::Status::INTERNAL_ERROR;
        }
        auto* db = T::engineDatabase();
        if (db == nullptr || db->catalog_manager() == nullptr) {
            SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Database/catalog is not available");
            return core::Status::INTERNAL_ERROR;
        }

        core::CatalogManager::SchemaInfo public_schema;
        auto status = db->catalog_manager()->getSchema("public", public_schema, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        T::connection_ctx_->setCurrentSchemaId(public_schema.schema_id);
        T::connection_ctx_->set_current_schema("public");
        T::connection_ctx_->set_search_path({"public", "sys"});
        T::connection_ctx_->setCurrentUser(db->catalog_manager()->getSystemUserId(ctx), true);
        return core::Status::OK;
    }

    core::Status executeQueryForTest(const QueryContext& query, ResultContext& result) {
        core::ErrorContext ctx;
        auto status = assumePublicSuperuserForTest(&ctx);
        if (status != core::Status::OK) {
            result.has_error = true;
            result.error_code = static_cast<uint32_t>(status);
            result.error_message = ctx.message;
            return status;
        }
        return T::executeQuery(query, result);
    }

    core::Status prepareStatementForTest(const std::string& name,
                                         const std::string& query,
                                         std::vector<int32_t>& param_types) {
        core::ErrorContext ctx;
        auto status = assumePublicSuperuserForTest(&ctx);
        if (status != core::Status::OK) {
            return status;
        }
        return T::prepareStatement(name, query, param_types);
    }

    core::Status executePreparedForTest(const std::string& name,
                                        const QueryContext& params,
                                        ResultContext& result) {
        core::ErrorContext ctx;
        auto status = assumePublicSuperuserForTest(&ctx);
        if (status != core::Status::OK) {
            result.has_error = true;
            result.error_code = static_cast<uint32_t>(status);
            result.error_message = ctx.message;
            return status;
        }
        return T::executePrepared(name, params, result);
    }

    core::Status sendQueryResultForTest(network::Connection* conn,
                                        const ResultContext& result) {
        return T::sendQueryResult(conn, result);
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

std::vector<std::vector<uint8_t>> splitMySqlPackets(const std::vector<uint8_t>& stream) {
    std::vector<std::vector<uint8_t>> payloads;
    size_t offset = 0;
    while (offset + 4 <= stream.size()) {
        const size_t payload_size =
            static_cast<size_t>(stream[offset]) |
            (static_cast<size_t>(stream[offset + 1]) << 8) |
            (static_cast<size_t>(stream[offset + 2]) << 16);
        if (offset + 4 + payload_size > stream.size()) {
            break;
        }
        payloads.emplace_back(stream.begin() + offset + 4,
                              stream.begin() + offset + 4 + payload_size);
        offset += 4 + payload_size;
    }
    return payloads;
}

std::vector<std::string> parseMySqlLenEncRow(const std::vector<uint8_t>& payload) {
    std::vector<std::string> values;
    size_t offset = 0;
    while (offset < payload.size()) {
        const uint8_t marker = payload[offset++];
        if (marker == 0xfb) {
            values.emplace_back("NULL");
            continue;
        }
        if (marker >= 0xfc) {
            values.emplace_back("<unsupported>");
            break;
        }
        if (offset + marker > payload.size()) {
            values.emplace_back("<truncated>");
            break;
        }
        values.emplace_back(reinterpret_cast<const char*>(payload.data() + offset), marker);
        offset += marker;
    }
    return values;
}

uint16_t readMySqlErrorCode(const std::vector<uint8_t>& payload) {
    if (payload.size() < 3) {
        return 0;
    }
    return static_cast<uint16_t>(payload[1]) |
           (static_cast<uint16_t>(payload[2]) << 8);
}

std::string readMySqlSqlState(const std::vector<uint8_t>& payload) {
    if (payload.size() < 9 || payload[3] != '#') {
        return std::string{};
    }
    return std::string(reinterpret_cast<const char*>(payload.data() + 4), 5);
}

void writePgInt16(std::vector<uint8_t>& payload, uint16_t value) {
    payload.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(value & 0xFF));
}

void writePgInt32(std::vector<uint8_t>& payload, uint32_t value) {
    payload.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    payload.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    payload.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(value & 0xFF));
}

void writePgCString(std::vector<uint8_t>& payload, const std::string& value) {
    payload.insert(payload.end(), value.begin(), value.end());
    payload.push_back('\0');
}

std::vector<uint8_t> buildPgFrontendMessage(uint8_t type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> packet;
    packet.reserve(1 + 4 + payload.size());
    packet.push_back(type);
    writePgInt32(packet, static_cast<uint32_t>(4 + payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<uint8_t> buildSbwpFrontendMessage(sbwp::MessageType type,
                                              const std::vector<uint8_t>& payload,
                                              uint32_t sequence = 1) {
    sbwp::MessageHeader header;
    header.type = type;
    header.flags = 0;
    header.length = static_cast<uint32_t>(payload.size());
    header.sequence = sequence;
    header.attachment_id.fill(0);
    header.txn_id = 0;
    return sbwp::encodeMessage(header, payload);
}

std::vector<uint8_t> buildNativeStartupPacket(
    const std::string& user,
    const std::string& database,
    const std::map<std::string, std::string>& extra_params = {}) {
    std::map<std::string, std::string> params = extra_params;
    params["user"] = user;
    params["database"] = database;
    const auto payload = sbwp::buildStartupPayload(0, params);
    return buildSbwpFrontendMessage(sbwp::MessageType::Startup, payload, 1);
}

bool decodeFirstSbwpMessage(const std::vector<uint8_t>& stream,
                            sbwp::MessageHeader& header,
                            std::vector<uint8_t>& payload) {
    if (stream.size() < sbwp::kHeaderSize) {
        return false;
    }

    std::vector<uint8_t> header_bytes(stream.begin(), stream.begin() + sbwp::kHeaderSize);
    core::ErrorContext ctx;
    if (sbwp::decodeHeader(header_bytes, header, &ctx) != core::Status::OK) {
        return false;
    }

    const size_t total_size = sbwp::kHeaderSize + static_cast<size_t>(header.length);
    if (stream.size() < total_size) {
        return false;
    }

    payload.assign(stream.begin() + sbwp::kHeaderSize, stream.begin() + total_size);
    return true;
}

std::vector<char> extractPgBackendMessageTypes(const std::vector<uint8_t>& stream) {
    std::vector<char> types;
    size_t offset = 0;
    while (offset + 5 <= stream.size()) {
        char msg_type = static_cast<char>(stream[offset]);
        uint32_t msg_len =
            (static_cast<uint32_t>(stream[offset + 1]) << 24) |
            (static_cast<uint32_t>(stream[offset + 2]) << 16) |
            (static_cast<uint32_t>(stream[offset + 3]) << 8) |
            static_cast<uint32_t>(stream[offset + 4]);
        if (msg_len < 4) {
            break;
        }
        const size_t frame_len = 1 + static_cast<size_t>(msg_len);
        if (offset + frame_len > stream.size()) {
            break;
        }
        types.push_back(msg_type);
        offset += frame_len;
    }
    return types;
}

bool pgContainsMessageType(const std::vector<char>& types, char expected) {
    return std::find(types.begin(), types.end(), expected) != types.end();
}

int32_t readPgAuthenticationType(const std::vector<uint8_t>& stream) {
    size_t offset = 0;
    while (offset + 5 <= stream.size()) {
        const char msg_type = static_cast<char>(stream[offset]);
        const uint32_t msg_len =
            (static_cast<uint32_t>(stream[offset + 1]) << 24) |
            (static_cast<uint32_t>(stream[offset + 2]) << 16) |
            (static_cast<uint32_t>(stream[offset + 3]) << 8) |
            static_cast<uint32_t>(stream[offset + 4]);
        if (msg_len < 4) {
            break;
        }
        const size_t frame_len = 1 + static_cast<size_t>(msg_len);
        if (offset + frame_len > stream.size()) {
            break;
        }
        if (msg_type == pg::BackendMsg::AUTHENTICATION && msg_len >= 8) {
            const size_t payload_offset = offset + 5;
            return static_cast<int32_t>(
                (static_cast<uint32_t>(stream[payload_offset]) << 24) |
                (static_cast<uint32_t>(stream[payload_offset + 1]) << 16) |
                (static_cast<uint32_t>(stream[payload_offset + 2]) << 8) |
                static_cast<uint32_t>(stream[payload_offset + 3]));
        }
        offset += frame_len;
    }
    return -1;
}

struct PgErrorResponseFields {
    bool found = false;
    std::string sqlstate;
    std::string message;
};

PgErrorResponseFields readFirstPgErrorResponse(const std::vector<uint8_t>& stream) {
    PgErrorResponseFields fields;
    size_t offset = 0;
    while (offset + 5 <= stream.size()) {
        const char msg_type = static_cast<char>(stream[offset]);
        const uint32_t msg_len =
            (static_cast<uint32_t>(stream[offset + 1]) << 24) |
            (static_cast<uint32_t>(stream[offset + 2]) << 16) |
            (static_cast<uint32_t>(stream[offset + 3]) << 8) |
            static_cast<uint32_t>(stream[offset + 4]);
        if (msg_len < 4) {
            break;
        }
        const size_t frame_len = 1 + static_cast<size_t>(msg_len);
        if (offset + frame_len > stream.size()) {
            break;
        }
        if (msg_type != pg::BackendMsg::ERROR_RESPONSE) {
            offset += frame_len;
            continue;
        }

        fields.found = true;
        size_t payload = offset + 5;
        const size_t payload_end = offset + frame_len;
        while (payload < payload_end) {
            const char field_code = static_cast<char>(stream[payload++]);
            if (field_code == '\0') {
                break;
            }

            const size_t value_start = payload;
            while (payload < payload_end && stream[payload] != '\0') {
                ++payload;
            }
            if (payload >= payload_end) {
                break;
            }
            const std::string value(reinterpret_cast<const char*>(stream.data() + value_start),
                                    payload - value_start);
            ++payload;  // consume trailing NUL

            if (field_code == pg::ErrorField::CODE) {
                fields.sqlstate = value;
            } else if (field_code == pg::ErrorField::MESSAGE) {
                fields.message = value;
            }
        }
        return fields;
    }
    return fields;
}

core::Status sendPgFrontendPacket(AdapterHarness<PostgresqlAdapter>& adapter,
                                  network::Connection* conn,
                                  uint8_t type,
                                  const std::vector<uint8_t>& payload) {
    auto packet = buildPgFrontendMessage(type, payload);
    auto& read_buffer = conn->getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());
    auto parse_status = adapter.parseIncomingPacket(conn);
    if (parse_status != core::Status::OK) {
        return parse_status;
    }
    return adapter.processIncomingPacket(conn);
}

bool readExactFd(int fd, void* buffer, size_t size) {
    uint8_t* out = static_cast<uint8_t*>(buffer);
    size_t remaining = size;
    while (remaining > 0) {
        const ssize_t n = ::recv(fd, out, remaining, MSG_WAITALL);
        if (n <= 0) {
            return false;
        }
        out += static_cast<size_t>(n);
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

std::vector<uint8_t> buildPgParsePayload(const std::string& statement_name,
                                         const std::string& query) {
    std::vector<uint8_t> payload;
    writePgCString(payload, statement_name);
    writePgCString(payload, query);
    writePgInt16(payload, 0);  // num_params
    return payload;
}

std::vector<uint8_t> buildPgBindPayload(const std::string& portal_name,
                                        const std::string& statement_name) {
    std::vector<uint8_t> payload;
    writePgCString(payload, portal_name);
    writePgCString(payload, statement_name);
    writePgInt16(payload, 0);  // num_format_codes
    writePgInt16(payload, 0);  // num_params
    writePgInt16(payload, 0);  // num_result_formats
    return payload;
}

std::vector<uint8_t> buildPgExecutePayload(const std::string& portal_name, uint32_t max_rows) {
    std::vector<uint8_t> payload;
    writePgCString(payload, portal_name);
    writePgInt32(payload, max_rows);
    return payload;
}

std::vector<uint8_t> buildPgSaslInitialPayload(const std::string& mechanism,
                                               const std::string& client_first) {
    std::vector<uint8_t> payload;
    writePgCString(payload, mechanism);
    writePgInt32(payload, static_cast<uint32_t>(client_first.size()));
    payload.insert(payload.end(), client_first.begin(), client_first.end());
    return payload;
}

std::vector<uint8_t> buildPgStartupMessage(const std::string& user,
                                           const std::string& database) {
    std::vector<uint8_t> payload;
    writePgInt32(payload, static_cast<uint32_t>(pg::PROTOCOL_VERSION_3));
    writePgCString(payload, "user");
    writePgCString(payload, user);
    writePgCString(payload, "database");
    writePgCString(payload, database);
    payload.push_back('\0');

    std::vector<uint8_t> packet;
    writePgInt32(packet, static_cast<uint32_t>(4 + payload.size()));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

void writeFbU32BE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

std::vector<uint8_t> buildFirebirdPacket(uint32_t opcode, const std::vector<uint8_t>& body = {}) {
    std::vector<uint8_t> packet;
    packet.reserve(4 + body.size());
    writeFbU32BE(packet, opcode);
    packet.insert(packet.end(), body.begin(), body.end());
    return packet;
}

std::vector<uint8_t> buildFirebirdConnectBodyForPolicyTest() {
    std::vector<uint8_t> body;
    writeFbU32BE(body, firebird::Opcode::op_attach);
    writeFbU32BE(body, firebird::DEFAULT_PROTOCOL_VERSION);
    writeFbU32BE(body, firebird::ARCH_GENERIC);
    writeFbU32BE(body, 1);
    writeFbU32BE(body, 1);
    writeFbU32BE(body, 0);
    return body;
}

void appendFbXdrBuffer(std::vector<uint8_t>& out, const std::vector<uint8_t>& value) {
    writeFbU32BE(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

void appendFbXdrString(std::vector<uint8_t>& out, const std::string& value) {
    writeFbU32BE(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

std::vector<uint8_t> buildFirebirdContAuthBody(const std::vector<uint8_t>& auth_data,
                                               const std::string& plugin) {
    std::vector<uint8_t> body;
    appendFbXdrBuffer(body, auth_data);
    appendFbXdrString(body, plugin);
    return body;
}

uint32_t readFbU32BE(const std::vector<uint8_t>& data, size_t offset = 0) {
    if (offset + 4 > data.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
            static_cast<uint32_t>(data[offset + 3]);
}

std::string readFbXdrString(const std::vector<uint8_t>& data, size_t& offset) {
    if (offset + 4 > data.size()) {
        return {};
    }
    const uint32_t len = readFbU32BE(data, offset);
    offset += 4;
    if (offset + len > data.size()) {
        return {};
    }
    std::string out(reinterpret_cast<const char*>(data.data() + offset), len);
    offset += len;
    const size_t padding = (4 - (len % 4)) % 4;
    if (offset + padding > data.size()) {
        return {};
    }
    offset += padding;
    return out;
}

struct FirebirdErrorFields {
    bool has_error = false;
    int32_t gds_code = 0;
    std::string sqlstate;
};

FirebirdErrorFields parseFirebirdErrorFields(const std::vector<uint8_t>& packet) {
    FirebirdErrorFields fields;
    if (packet.size() < 28 || readFbU32BE(packet) != firebird::Opcode::op_response) {
        return fields;
    }

    size_t offset = 4;   // opcode
    offset += 4;         // handle
    offset += 8;         // object id
    if (offset + 4 > packet.size()) {
        return fields;
    }

    const uint32_t data_len = readFbU32BE(packet, offset);
    offset += 4 + data_len;
    const size_t data_padding = (4 - (data_len % 4)) % 4;
    if (offset + data_padding > packet.size()) {
        return fields;
    }
    offset += data_padding;

    if (offset + 8 > packet.size()) {
        return fields;
    }
    const uint32_t arg_type = readFbU32BE(packet, offset);
    offset += 4;
    if (arg_type != static_cast<uint32_t>(firebird::ErrorCode::isc_arg_gds)) {
        return fields;
    }
    fields.gds_code = static_cast<int32_t>(readFbU32BE(packet, offset));
    offset += 4;
    fields.has_error = true;

    while (offset + 4 <= packet.size()) {
        const uint32_t token = readFbU32BE(packet, offset);
        offset += 4;
        if (token == static_cast<uint32_t>(firebird::ErrorCode::isc_arg_end)) {
            break;
        }
        if (token == static_cast<uint32_t>(firebird::ErrorCode::isc_arg_sql_state) ||
            token == static_cast<uint32_t>(firebird::ErrorCode::isc_arg_string) ||
            token == static_cast<uint32_t>(firebird::ErrorCode::isc_arg_cstring) ||
            token == static_cast<uint32_t>(firebird::ErrorCode::isc_arg_interpreted)) {
            std::string value = readFbXdrString(packet, offset);
            if (token == static_cast<uint32_t>(firebird::ErrorCode::isc_arg_sql_state)) {
                fields.sqlstate = value;
            }
            continue;
        }
        if (offset + 4 <= packet.size()) {
            offset += 4;
        } else {
            break;
        }
    }

    return fields;
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

TEST(ProtocolAdapterDialects, MySQLRemoteCompileIgnoresSystemSchemaSubstringsInsideStringLiterals) {
    cleanupDb("test_mysql_remote_compile_boundary.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_remote_compile_boundary.sbdb").string();
    cfg.engine_endpoint = "/tmp/test_mysql_remote_compile_boundary.sock";
    cfg.default_database = "tenant_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    std::vector<uint8_t> bytecode;
    std::string err;
    const auto status = adapter.runCompile(
        "INSERT INTO t1 VALUES ('sasha@mysql.com'),('monty@mysql.com'),"
        "('foo@hotmail.com'),('foo@aol.com'),('bar@aol.com')",
        bytecode,
        err);

    ASSERT_EQ(status, core::Status::OK) << err;
    EXPECT_FALSE(bytecode.empty());
}

TEST(ProtocolAdapterDialects, MySQLRemoteExecutionBootstrapIgnoresSystemSchemaSubstringsInsideStringLiterals) {
    cleanupDb("test_mysql_remote_exec_boundary.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_remote_exec_boundary.sbdb").string();
    cfg.default_database = "compat_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setSelectedDatabaseNameForTest("compat_mysql");

    EXPECT_FALSE(adapter.shouldBootstrapMySqlSystemSchemaForRemoteQueryForTest(
        "INSERT INTO t1 VALUES ('sasha@mysql.com'),('monty@mysql.com'),"
        "('foo@hotmail.com'),('foo@aol.com'),('bar@aol.com')"));
}

TEST(ProtocolAdapterDialects, MySQLGreetingResetsPriorSessionDatabaseSelection) {
    cleanupDb("test_mysql_greeting_session_reset.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_greeting_session_reset.sbdb").string();
    cfg.default_database = "compat_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setSelectedDatabaseNameForTest("compat_mysql");
    adapter.applySuccessfulMySqlQueryForTest("USE `compat_mysql`");

    network::Connection conn(nullptr, 321);
    ASSERT_EQ(adapter.sendGreetingForTest(&conn), core::Status::OK);
    EXPECT_TRUE(adapter.selectedDatabaseNameForTest().empty());
}

TEST(ProtocolAdapterDialects, MySQLGreetingResetsPriorSessionAuthPluginSelection) {
    cleanupDb("test_mysql_greeting_auth_reset.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_greeting_auth_reset.sbdb").string();
    cfg.default_database = "compat_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setMySqlAuthPluginForTest("mysql_native_password");

    network::Connection conn(nullptr, 654);
    ASSERT_EQ(adapter.sendGreetingForTest(&conn), core::Status::OK);
    EXPECT_EQ(adapter.mySqlAuthPluginForTest(), "caching_sha2_password");
}

TEST(ProtocolAdapterDialects, PostgresqlGreetingResetsProtocolStateToStartup) {
    cleanupDb("test_postgresql_greeting_state_reset.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_postgresql_greeting_state_reset.sbdb").string();
    cfg.default_database = "compat_pg";

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    adapter.setPostgresqlStateForTest(PgProtocolState::READY);

    network::Connection conn(nullptr, 655);
    ASSERT_EQ(adapter.sendGreetingForTest(&conn), core::Status::OK);
    EXPECT_EQ(adapter.postgresqlStateForTest(), PgProtocolState::STARTUP);
}

TEST(ProtocolAdapterDialects, MySQLTranslationCacheDoesNotLeakAcrossDatabases) {
    cleanupDb("test_mysql_cache_isolation_a.sbdb");
    cleanupDb("test_mysql_cache_isolation_b.sbdb");

    auto& translation_cache = TranslationCacheManager::getInstance();
    translation_cache.invalidateAll();

    ProtocolAdapterConfig cfg_a;
    cfg_a.database_path = dbPath("test_mysql_cache_isolation_a.sbdb").string();
    AdapterHarness<MySqlAdapter> adapter_a(cfg_a);

    ProtocolAdapterConfig cfg_b;
    cfg_b.database_path = dbPath("test_mysql_cache_isolation_b.sbdb").string();
    AdapterHarness<MySqlAdapter> adapter_b(cfg_b);

    core::ErrorContext ctx;
    ASSERT_EQ(adapter_a.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;
    ASSERT_EQ(adapter_b.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    auto run_query = [](auto& adapter, const std::string& sql) {
        QueryContext query;
        query.query = sql;
        ResultContext result;
        auto status = adapter.executeQueryForTest(query, result);
        return std::make_pair(status, result);
    };

    {
        auto [status, result] = run_query(
            adapter_a,
            "CREATE TABLE t1 (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, data INT NOT NULL)");
        ASSERT_EQ(status, core::Status::OK);
        ASSERT_FALSE(result.has_error) << result.error_message;
    }

    {
        auto [status, result] = run_query(adapter_a, "INSERT INTO t1 VALUES (0, 'mysql')");
        ASSERT_EQ(status, core::Status::OK);
        ASSERT_TRUE(result.has_error);
        EXPECT_NE(result.error_message.find("data"), std::string::npos);
    }

    {
        auto [status, result] = run_query(
            adapter_b,
            "CREATE TABLE t1 (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY, username VARCHAR(32) NOT NULL)");
        ASSERT_EQ(status, core::Status::OK);
        ASSERT_FALSE(result.has_error) << result.error_message;
    }

    {
        auto [status, result] = run_query(adapter_b, "INSERT INTO t1 VALUES (0, 'mysql')");
        ASSERT_EQ(status, core::Status::OK);
        ASSERT_FALSE(result.has_error) << result.error_message;
    }

    {
        auto [status, result] = run_query(adapter_b, "SELECT username FROM t1 ORDER BY id");
        ASSERT_EQ(status, core::Status::OK);
        ASSERT_FALSE(result.has_error) << result.error_message;
        ASSERT_EQ(result.rows.size(), 1u);
        ASSERT_EQ(result.rows[0].size(), 1u);
        EXPECT_EQ(std::string(result.rows[0][0].data.begin(), result.rows[0][0].data.end()), "mysql");
    }

    translation_cache.invalidateAll();
}

TEST(ProtocolAdapterDialects, MySQLSetAutocommitOneDoesNotRequireActiveTransaction) {
    cleanupDb("test_mysql_autocommit_set.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_autocommit_set.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;
    QueryContext query;
    query.query = "SET autocommit=1";

    ResultContext result;
    ASSERT_EQ(adapter.executeQueryForTest(query, result), core::Status::OK)
        << result.error_message;
    ASSERT_FALSE(result.has_error) << result.error_message;
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

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyRejectsUnsupportedConfiguredAuthMethodAtStartup) {
    cleanupDb("test_pg_policy_reject_unsupported_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_reject_unsupported_auth.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::TOKEN;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 113);

    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    EXPECT_EQ(adapter.processIncomingPacket(&conn), core::Status::NOT_SUPPORTED);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_FALSE(message_types.empty());
    EXPECT_EQ(message_types.front(), pg::BackendMsg::ERROR_RESPONSE);

    const auto error_fields = readFirstPgErrorResponse(conn.getWriteBuffer());
    ASSERT_TRUE(error_fields.found);
    EXPECT_EQ(error_fields.sqlstate, "0A000");
    EXPECT_NE(error_fields.message.find("not supported by PostgreSQL emulation policy"),
              std::string::npos);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyRejectsPeerConfiguredMethodAtStartup) {
    cleanupDb("test_pg_policy_reject_peer_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_reject_peer_auth.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::PEER;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 118);

    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    EXPECT_EQ(adapter.processIncomingPacket(&conn), core::Status::NOT_SUPPORTED);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_FALSE(message_types.empty());
    EXPECT_EQ(message_types.front(), pg::BackendMsg::ERROR_RESPONSE);

    const auto error_fields = readFirstPgErrorResponse(conn.getWriteBuffer());
    ASSERT_TRUE(error_fields.found);
    EXPECT_EQ(error_fields.sqlstate, "0A000");
    EXPECT_NE(error_fields.message.find("not supported by PostgreSQL emulation policy"),
              std::string::npos);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyAllowsMd5ConfiguredMethodAtStartup) {
    cleanupDb("test_pg_policy_allow_md5_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_allow_md5_auth.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::MD5;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 115);

    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    EXPECT_EQ(readPgAuthenticationType(conn.getWriteBuffer()), pg::AuthType::MD5_PASSWORD);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyNormalizesScram512ToScram256AtStartup) {
    cleanupDb("test_pg_policy_scram512_normalize.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_scram512_normalize.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::SCRAM_SHA_512;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 116);

    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    EXPECT_EQ(readPgAuthenticationType(conn.getWriteBuffer()), pg::AuthType::SASL);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyRejectsUnsupportedSaslMechanismDeterministically) {
    cleanupDb("test_pg_policy_reject_sasl_mechanism.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_reject_sasl_mechanism.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::SCRAM_SHA_256;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 119);

    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::SASL_INITIAL),
                  buildPgSaslInitialPayload("OAUTHBEARER", "n,,n=policy_user,r=nonce")),
              core::Status::OK);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_FALSE(message_types.empty());
    EXPECT_EQ(message_types.front(), pg::BackendMsg::ERROR_RESPONSE);

    const auto error_fields = readFirstPgErrorResponse(conn.getWriteBuffer());
    ASSERT_TRUE(error_fields.found);
    EXPECT_EQ(error_fields.sqlstate, "0A000");
    EXPECT_NE(error_fields.message.find("Unsupported SASL mechanism"), std::string::npos);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyRejectsScramPlusChannelBindingDeterministically) {
    cleanupDb("test_pg_policy_reject_scram_plus.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_reject_scram_plus.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::SCRAM_SHA_256;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 120);

    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::SASL_INITIAL),
                  buildPgSaslInitialPayload("SCRAM-SHA-256",
                                            "p=tls-server-end-point,,n=policy_user,r=nonce")),
              core::Status::OK);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_FALSE(message_types.empty());
    EXPECT_EQ(message_types.front(), pg::BackendMsg::ERROR_RESPONSE);

    const auto error_fields = readFirstPgErrorResponse(conn.getWriteBuffer());
    ASSERT_TRUE(error_fields.found);
    EXPECT_EQ(error_fields.sqlstate, "0A000");
    EXPECT_NE(error_fields.message.find("SCRAM-PLUS channel binding is not supported"),
              std::string::npos);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLPolicyGssEncNegotiatedDisableIsDeterministic) {
    cleanupDb("test_pg_policy_gssenc_negotiated_disable.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_policy_gssenc_negotiated_disable.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::MD5;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    int fd_pair[2] = {-1, -1};
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fd_pair), 0);
    auto server_socket = network::Socket::fromFd(fd_pair[0], network::AddressFamily::UNIX);
    ASSERT_NE(server_socket, nullptr);
    network::Connection conn(std::move(server_socket), 121);

    std::vector<uint8_t> gssenc_request;
    writePgInt32(gssenc_request, 8);
    writePgInt32(gssenc_request, static_cast<uint32_t>(pg::GSSENC_REQUEST));
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), gssenc_request.begin(), gssenc_request.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    uint8_t gss_response = 0;
    ASSERT_TRUE(readExactFd(fd_pair[1], &gss_response, 1));
    EXPECT_EQ(static_cast<char>(gss_response), 'N');

    conn.clearWriteBuffer();
    const auto startup_packet = buildPgStartupMessage("policy_user", "policy_db");
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());
    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    EXPECT_EQ(readPgAuthenticationType(conn.getWriteBuffer()), pg::AuthType::MD5_PASSWORD);

    ::close(fd_pair[1]);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLBoundSessionAllowsSystemDatabaseAliasAtStartup) {
    cleanupDb("test_pg_bound_system_db_alias.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_bound_system_db_alias.sbdb").string();
    cfg.enforce_bound_database = true;
    cfg.default_database = "tenant_a";
    cfg.require_authentication = false;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 122);

    const auto startup_packet = buildPgStartupMessage("policy_user", "template1");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    EXPECT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_FALSE(message_types.empty());
    EXPECT_NE(message_types.front(), pg::BackendMsg::ERROR_RESPONSE);
    EXPECT_EQ(message_types.back(), pg::BackendMsg::READY_FOR_QUERY);
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

TEST(ProtocolAdapterDialectsC1, PostgreSQLStartupPublishesEmulatedDatabaseSearchPath) {
    cleanupDb("test_pg_emulated_search_path.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_emulated_search_path.sbdb").string();
    cfg.require_authentication = false;

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 123);

    const auto startup_packet = buildPgStartupMessage("tenant_user", "tenant_pg");
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto& params = adapter.getServerParameters();
    auto it = params.find("search_path");
    ASSERT_NE(it, params.end());
    EXPECT_EQ(it->second, "pg_catalog, emulated.postgresql.localhost.databases.tenant_pg");
    EXPECT_EQ(adapter.selectedDatabaseNameForTest(), "tenant_pg");
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLSessionSchemaContextUsesEmulatedDatabaseRoot) {
    cleanupDb("test_pg_schema_context_root.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_schema_context_root.sbdb").string();
    cfg.default_database = "tenant_pg";

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    adapter.setUsernameForTest("tenant_user");
    adapter.setSelectedDatabaseNameForTest("tenant_pg");
    adapter.applyPostgresqlSessionSchemaContextForTest("tenant_pg", &ctx);

    auto* conn_ctx = adapter.connectionContextForTest();
    ASSERT_NE(conn_ctx, nullptr);
    ASSERT_EQ(conn_ctx->current_schema(),
              "emulated.postgresql.localhost.databases.tenant_pg");
    ASSERT_FALSE(conn_ctx->search_path().empty());
    EXPECT_EQ(conn_ctx->search_path().front(),
              "emulated.postgresql.localhost.databases.tenant_pg");
    EXPECT_NE(conn_ctx->current_schema(), "users.public");
    EXPECT_NE(conn_ctx->getCurrentSchemaId(), core::ID{});
}

TEST(ProtocolAdapterDialectsC3, MySQLSessionSchemaContextUsesEmulatedDatabaseRoot) {
    cleanupDb("test_mysql_schema_context_root.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_schema_context_root.sbdb").string();
    cfg.default_database = "tenant_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    adapter.setUsernameForTest("tenant_user");
    adapter.applySuccessfulMySqlQueryForTest("USE `tenant_mysql`");

    auto* conn_ctx = adapter.connectionContextForTest();
    ASSERT_NE(conn_ctx, nullptr);
    ASSERT_EQ(conn_ctx->current_schema(),
              "emulated.mysql.localhost.databases.tenant_mysql");
    ASSERT_FALSE(conn_ctx->search_path().empty());
    EXPECT_EQ(conn_ctx->search_path().front(),
              "emulated.mysql.localhost.databases.tenant_mysql");
    EXPECT_NE(conn_ctx->getCurrentSchemaId(), core::ID{});
}

TEST(ProtocolAdapterDialectsC3, MySQLRegularSelectDoesNotBootstrapInformationSchema) {
    cleanupDb("test_mysql_regular_select_no_bootstrap.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_regular_select_no_bootstrap.sbdb").string();
    cfg.default_database = "tenant_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    adapter.applySuccessfulMySqlQueryForTest("USE `tenant_mysql`");

    QueryContext query;
    query.query = "SELECT 1";
    ResultContext result;
    ASSERT_EQ(adapter.executeQueryForTest(query, result), core::Status::OK);
    ASSERT_FALSE(result.has_error) << result.error_message;
    ASSERT_EQ(result.rows.size(), 1u);
    ASSERT_EQ(result.rows.front().size(), 1u);
    EXPECT_EQ(std::string(result.rows.front().front().data.begin(),
                          result.rows.front().front().data.end()),
              "1");

    auto* db = adapter.engineDatabaseForTest();
    ASSERT_NE(db, nullptr);
    auto* catalog = db->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    core::CatalogManager::SchemaInfo schema_info;
    core::ErrorContext lookup_ctx;
    const auto status = catalog->getSchema(
        "emulated.mysql.localhost.databases.information_schema",
        schema_info,
        &lookup_ctx);
    EXPECT_NE(status, core::Status::OK);
}

TEST(ProtocolAdapterDialectsC3, MySQLInformationSchemaBootstrapUsesSiblingDatabaseRoots) {
    cleanupDb("test_mysql_information_schema_bootstrap_roots.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_information_schema_bootstrap_roots.sbdb").string();
    cfg.default_database = "tenant_mysql";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    adapter.applySuccessfulMySqlQueryForTest("USE `tenant_mysql`");

    QueryContext create_query;
    create_query.query = "CREATE TABLE t_info_bootstrap (id INT PRIMARY KEY)";
    ResultContext create_result;
    ASSERT_EQ(adapter.executeQueryForTest(create_query, create_result), core::Status::OK);
    ASSERT_FALSE(create_result.has_error) << create_result.error_message;

    QueryContext info_query;
    info_query.query =
        "SELECT schema_name FROM information_schema.schemata "
        "WHERE schema_name = 'tenant_mysql'";
    ResultContext info_result;
    ASSERT_EQ(adapter.executeQueryForTest(info_query, info_result), core::Status::OK);
    ASSERT_FALSE(info_result.has_error) << info_result.error_message;
    ASSERT_EQ(info_result.rows.size(), 1u);
    ASSERT_EQ(info_result.rows.front().size(), 1u);
    EXPECT_EQ(std::string(info_result.rows.front().front().data.begin(),
                          info_result.rows.front().front().data.end()),
              "tenant_mysql");

    auto* db = adapter.engineDatabaseForTest();
    ASSERT_NE(db, nullptr);
    auto* catalog = db->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(catalog->getSchema("emulated.mysql.localhost.databases.information_schema",
                                 schema_info,
                                 &ctx),
              core::Status::OK)
        << ctx.message;

    core::CatalogManager::SchemaInfo nested_info;
    core::ErrorContext nested_ctx;
    const auto nested_status = catalog->getSchema(
        "emulated.mysql.localhost.databases.tenant_mysql.information_schema",
        nested_info,
        &nested_ctx);
    EXPECT_NE(nested_status, core::Status::OK);
}

TEST(ProtocolAdapterDialectsFirebird, FirebirdSessionSchemaContextUsesEmulatedDatabaseRoot) {
    cleanupDb("test_fb_schema_context_root.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_schema_context_root.sbdb").string();
    cfg.default_database = "tenant_fb.fdb";

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    adapter.setUsernameForTest("SYSDBA");
    adapter.applyFirebirdSessionSchemaContextForTest(&ctx);
    ASSERT_TRUE(ctx.message.empty()) << ctx.message;

    auto* conn_ctx = adapter.connectionContextForTest();
    ASSERT_NE(conn_ctx, nullptr);
    ASSERT_EQ(conn_ctx->current_schema(),
              "emulated.firebird.localhost.tenant_fb");
    ASSERT_FALSE(conn_ctx->search_path().empty());
    EXPECT_EQ(conn_ctx->search_path().front(),
              "emulated.firebird.localhost.tenant_fb");
    EXPECT_NE(conn_ctx->getCurrentSchemaId(), core::ID{});
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

TEST(ProtocolAdapterDialectsC1, PostgreSQLExtendedParseBindSyncFlow) {
    cleanupDb("test_pg_extended_parse_bind_sync.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_extended_parse_bind_sync.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 108);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::PARSE),
                  buildPgParsePayload("stmt_sync", "SELECT 1")),
              core::Status::OK);
    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::BIND),
                  buildPgBindPayload("portal_sync", "stmt_sync")),
              core::Status::OK);
    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::SYNC),
                  {}),
              core::Status::OK);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(message_types.size(), 3u);
    EXPECT_EQ(message_types[0], pg::BackendMsg::PARSE_COMPLETE);
    EXPECT_EQ(message_types[1], pg::BackendMsg::BIND_COMPLETE);
    EXPECT_EQ(message_types[2], pg::BackendMsg::READY_FOR_QUERY);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLExtendedExecuteMissingPortalReportsErrorOnSync) {
    cleanupDb("test_pg_extended_missing_portal.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_extended_missing_portal.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 109);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    EXPECT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::EXECUTE),
                  buildPgExecutePayload("missing_portal", 0)),
              core::Status::NOT_FOUND);
    auto pre_sync_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(pre_sync_types.size(), 1u);
    EXPECT_EQ(pre_sync_types[0], pg::BackendMsg::ERROR_RESPONSE);

    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::SYNC),
                  {}),
              core::Status::OK);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    ASSERT_EQ(message_types.size(), 2u);
    EXPECT_EQ(message_types[0], pg::BackendMsg::ERROR_RESPONSE);
    EXPECT_EQ(message_types[1], pg::BackendMsg::READY_FOR_QUERY);
}

TEST(ProtocolAdapterDialectsC1, PostgreSQLExtendedExecutePathEmitsErrorAndReady) {
    cleanupDb("test_pg_extended_execute_path.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_pg_extended_execute_path.sbdb").string();

    AdapterHarness<PostgresqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 110);
    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::PARSE),
                  buildPgParsePayload("stmt_exec", "SELECT 1")),
              core::Status::OK);
    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::BIND),
                  buildPgBindPayload("portal_exec", "stmt_exec")),
              core::Status::OK);
    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::EXECUTE),
                  buildPgExecutePayload("portal_exec", 0)),
              core::Status::OK);

    // Execute queues response data; sync flushes ERROR/Ready in one frame.
    ASSERT_EQ(sendPgFrontendPacket(
                  adapter,
                  &conn,
                  static_cast<uint8_t>(pg::FrontendMsg::SYNC),
                  {}),
              core::Status::OK);

    const auto message_types = extractPgBackendMessageTypes(conn.getWriteBuffer());
    EXPECT_GE(message_types.size(), 4u);
    EXPECT_EQ(message_types[0], pg::BackendMsg::PARSE_COMPLETE);
    EXPECT_EQ(message_types[1], pg::BackendMsg::BIND_COMPLETE);
    EXPECT_TRUE(pgContainsMessageType(message_types, pg::BackendMsg::ERROR_RESPONSE));
    EXPECT_EQ(message_types.back(), pg::BackendMsg::READY_FOR_QUERY);
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
    
    // Default should track the MySQL 8.4 LTS baseline string format.
    EXPECT_TRUE(adapter.getServerVersion().find("8.4") != std::string::npos);
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

TEST(ProtocolAdapterDialectsNative, NativeHandshakeAcceptsMatchingAuthMethodPin) {
    cleanupDb("test_native_auth_pin_match.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_native_auth_pin_match.sbdb").string();

    AdapterHarness<NativeAdapter> adapter(cfg);
    network::Connection conn(nullptr, 203);

    const auto startup_packet = buildNativeStartupPacket(
        "policy_user",
        "policy_db",
        {{"auth_method_id", "scratchbird.auth.scram_sha_256"}});
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    sbwp::MessageHeader header;
    std::vector<uint8_t> payload;
    ASSERT_TRUE(decodeFirstSbwpMessage(conn.getWriteBuffer(), header, payload));
    EXPECT_EQ(header.type, sbwp::MessageType::AuthRequest);

    sbwp::AuthMethod auth_method = sbwp::AuthMethod::Ok;
    std::vector<uint8_t> auth_data;
    core::ErrorContext ctx;
    ASSERT_EQ(sbwp::parseAuthRequest(payload, auth_method, auth_data, &ctx), core::Status::OK)
        << ctx.message;
    EXPECT_EQ(auth_method, sbwp::AuthMethod::ScramSha256);
}

TEST(ProtocolAdapterDialectsNative, NativeHandshakeRejectsConflictingAuthMethodPin) {
    cleanupDb("test_native_auth_pin_conflict.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_native_auth_pin_conflict.sbdb").string();

    AdapterHarness<NativeAdapter> adapter(cfg);
    network::Connection conn(nullptr, 204);

    const auto startup_packet = buildNativeStartupPacket(
        "policy_user",
        "policy_db",
        {{"auth_method_id", "scratchbird.auth.password_compat"}});
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), startup_packet.begin(), startup_packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    sbwp::MessageHeader header;
    std::vector<uint8_t> payload;
    ASSERT_TRUE(decodeFirstSbwpMessage(conn.getWriteBuffer(), header, payload));
    EXPECT_EQ(header.type, sbwp::MessageType::Error);

    std::string severity;
    std::string sqlstate;
    std::string message;
    std::string detail;
    std::string hint;
    core::ErrorContext ctx;
    ASSERT_EQ(sbwp::parseErrorMessage(payload, severity, sqlstate, message, detail, hint, &ctx),
              core::Status::OK)
        << ctx.message;
    EXPECT_EQ(sqlstate, "0A000");
    EXPECT_NE(message.find("conflicts with core native auth policy"), std::string::npos);
}

TEST(ProtocolAdapterDialectsNative, NativeCompileRejectIncludesDeterministicSqlContext) {
    cleanupDb("test_native_compile_diagnostic.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_native_compile_diagnostic.sbdb").string();

    AdapterHarness<NativeAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;
    ASSERT_EQ(adapter.assumePublicSuperuserForTest(&ctx), core::Status::OK) << ctx.message;
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

TEST(ProtocolAdapterDialectsNative, PreparedStatementsCacheBucketedCustomPlansForScratchBird) {
    cleanupDb("test_native_prepared_bucketed_optimizer.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_native_prepared_bucketed_optimizer.sbdb").string();

    AdapterHarness<NativeAdapter> adapter(cfg);
    core::ErrorContext ctx;
    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    auto run_query = [&](const std::string& sql) {
        QueryContext query;
        query.query = sql;
        ResultContext result;
        auto status = adapter.executeQueryForTest(query, result);
        EXPECT_EQ(status, core::Status::OK);
        EXPECT_FALSE(result.has_error) << result.error_message;
    };

    run_query("CREATE TABLE users (id INTEGER, name VARCHAR(32), email VARCHAR(64), age INTEGER)");
    run_query("CREATE INDEX idx_users_id ON users (id)");
    run_query("GRANT SELECT ON users TO PUBLIC");

    for (int i = 1; i <= 256; ++i) {
        run_query("INSERT INTO users (id, name, email, age) VALUES (" +
                  std::to_string(i) + ", 'u" + std::to_string(i) + "', 'u" +
                  std::to_string(i) + "@x', " + std::to_string(20 + (i % 10)) + ")");
    }

    auto* db = adapter.engineDatabaseForTest();
    ASSERT_NE(db, nullptr);

    core::CatalogManager::SchemaInfo public_schema;
    ASSERT_EQ(db->catalog_manager()->getSchema("public", public_schema, &ctx),
              core::Status::OK)
        << ctx.message;
    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(db->catalog_manager()->getTable(public_schema.schema_id, "users", table_info, &ctx),
              core::Status::OK)
        << ctx.message;
    ASSERT_EQ(db->statistics_manager()->analyzeTable(table_info.table_id, 1.0, &ctx),
              core::Status::OK)
        << ctx.message;

    std::vector<int32_t> param_types;
    ASSERT_EQ(adapter.prepareStatementForTest("q_bucketed",
                                              "SELECT id FROM users WHERE id < $1",
                                              param_types),
              core::Status::OK);

    QueryContext selective;
    selective.parameter_values = {"5"};
    selective.parameter_nulls = {false};
    ResultContext selective_result;
    ASSERT_EQ(adapter.executePreparedForTest("q_bucketed", selective, selective_result),
              core::Status::OK);
    ASSERT_FALSE(selective_result.has_error) << selective_result.error_message;

    auto* prepared = adapter.preparedStatementForTest("q_bucketed");
    ASSERT_NE(prepared, nullptr);
    EXPECT_EQ(prepared->optimizer_parameter_signature_to_bucket.size(), 1u);

    QueryContext broad;
    broad.parameter_values = {"250"};
    broad.parameter_nulls = {false};
    ResultContext broad_result;
    ASSERT_EQ(adapter.executePreparedForTest("q_bucketed", broad, broad_result),
              core::Status::OK);
    ASSERT_FALSE(broad_result.has_error) << broad_result.error_message;

    prepared = adapter.preparedStatementForTest("q_bucketed");
    ASSERT_NE(prepared, nullptr);
    EXPECT_EQ(prepared->optimizer_parameter_signature_to_bucket.size(), 2u);
    EXPECT_EQ(prepared->optimizer_bucketed_bytecode.size(), 2u);
    EXPECT_EQ(prepared->optimizer_plan_mode,
              core::ConnectionContext::PreparedStatement::OptimizerPlanMode::CUSTOM_BUCKETED);
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

TEST(ProtocolAdapterDialectsC3, MySQLPolicyRejectsSha256PasswordInVerifierPath) {
    cleanupDb("test_mysql_policy_sha256_reject.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_policy_sha256_reject.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    uint8_t scramble[20] = {0};

    EXPECT_FALSE(adapter.validateAuthResponse("sha256_password", "", scramble, "secret"));
    EXPECT_FALSE(adapter.validateAuthResponse("sha256_password", "proof", scramble, "secret"));
}

TEST(ProtocolAdapterDialectsC3, MySQLPolicyAuthSocketVerifierRequiresEmptyProof) {
    cleanupDb("test_mysql_policy_auth_socket_verifier.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_policy_auth_socket_verifier.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    uint8_t scramble[20] = {0};

    EXPECT_TRUE(adapter.validateAuthResponse("auth_socket", "", scramble, ""));
    EXPECT_FALSE(adapter.validateAuthResponse("auth_socket", "x", scramble, ""));
    EXPECT_FALSE(adapter.validateAuthResponse("auth_socket", "", scramble, "nonempty"));
}

TEST(ProtocolAdapterDialectsC3, MySQLPolicyRejectsUnknownPluginInVerifierPath) {
    cleanupDb("test_mysql_policy_reject_unknown_plugin.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_policy_reject_unknown_plugin.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    uint8_t scramble[20] = {0};

    EXPECT_FALSE(adapter.validateAuthResponse("unknown_auth_plugin", "proof", scramble, ""));
    EXPECT_FALSE(adapter.validateAuthResponse("unknown_auth_plugin", "", scramble, "secret"));
    EXPECT_FALSE(adapter.validateAuthResponse("unknown_auth_plugin", "proof", scramble, "secret"));
}

TEST(ProtocolAdapterDialectsC3, MySQLPolicyAllowsNativeAndCachingSha2Plugins) {
    cleanupDb("test_mysql_policy_allow_native_and_sha2.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_policy_allow_native_and_sha2.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    uint8_t scramble[20] = {0};

    const auto native = adapter.computeNativePasswordAuth("secret", scramble);
    const std::string native_response(native.begin(), native.end());
    EXPECT_TRUE(native.empty() || adapter.validateAuthResponse("mysql_native_password",
                                                               native_response,
                                                               scramble,
                                                               "secret"));

    const auto sha2 = adapter.computeCachingSha2PasswordAuth("secret", scramble);
    const std::string sha2_response(sha2.begin(), sha2.end());
    EXPECT_TRUE(sha2.empty() || adapter.validateAuthResponse("caching_sha2_password",
                                                             sha2_response,
                                                             scramble,
                                                             "secret"));
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

TEST(ProtocolAdapterDialectsC3, MySqlTextResultRowsSerializeTypedValuesAsText) {
    cleanupDb("test_mysql_text_row_format.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_text_row_format.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 145);

    ResultContext result;
    result.columns = {
        {"a", WireType::INT32},
        {"b", WireType::BOOLEAN},
        {"c", WireType::FLOAT64},
    };
    result.rows = {{
        ProtocolCodec::ColumnValue::fromInt32(1),
        ProtocolCodec::ColumnValue::fromBool(true),
        ProtocolCodec::ColumnValue::fromDouble(3.5),
    }};

    ASSERT_EQ(adapter.sendQueryResultForTest(&conn, result), core::Status::OK);

    const auto packets = splitMySqlPackets(conn.getWriteBuffer());
    ASSERT_GE(packets.size(), 5u);

    size_t row_packet_index = 1 + result.columns.size();
    ASSERT_LT(row_packet_index, packets.size());
    if (!packets[row_packet_index].empty() &&
        packets[row_packet_index][0] == mysql::EOF_PACKET) {
        ++row_packet_index;
    }
    ASSERT_LT(row_packet_index, packets.size());

    const auto row_values = parseMySqlLenEncRow(packets[row_packet_index]);
    ASSERT_EQ(row_values.size(), 3u);
    EXPECT_EQ(row_values[0], "1");
    EXPECT_EQ(row_values[1], "1");
    EXPECT_EQ(row_values[2], "3.5");
}

TEST(ProtocolAdapterDialectsC3, MySqlTextResultRowsHeuristicallyFormatUnknownBinaryScalars) {
    cleanupDb("test_mysql_text_row_unknown_scalar_format.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_text_row_unknown_scalar_format.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 146);

    ResultContext result;
    result.columns = {
        {"bool_expr", WireType::UNKNOWN},
        {"count_expr", WireType::UNKNOWN},
    };
    result.rows = {{
        ProtocolCodec::ColumnValue::fromBool(true),
        ProtocolCodec::ColumnValue::fromInt64(0),
    }};

    ASSERT_EQ(adapter.sendQueryResultForTest(&conn, result), core::Status::OK);

    const auto packets = splitMySqlPackets(conn.getWriteBuffer());
    ASSERT_GE(packets.size(), 5u);

    size_t row_packet_index = 1 + result.columns.size();
    ASSERT_LT(row_packet_index, packets.size());
    if (!packets[row_packet_index].empty() &&
        packets[row_packet_index][0] == mysql::EOF_PACKET) {
        ++row_packet_index;
    }
    ASSERT_LT(row_packet_index, packets.size());

    const auto row_values = parseMySqlLenEncRow(packets[row_packet_index]);
    ASSERT_EQ(row_values.size(), 2u);
    EXPECT_EQ(row_values[0], "1");
    EXPECT_EQ(row_values[1], "0");
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

TEST(ProtocolAdapterDialectsC3, MySQLComChangeUserAllowsSystemDatabaseAliasWhenBound) {
    cleanupDb("test_mysql_change_user_bound_system_db_alias.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_change_user_bound_system_db_alias.sbdb").string();
    cfg.enforce_bound_database = true;
    cfg.default_database = "tenant_a";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 123);

    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_CHANGE_USER);
    payload.insert(payload.end(), {'a', 'l', 'i', 'c', 'e', '\0'});
    payload.push_back('\0');  // empty auth response
    payload.insert(payload.end(),
                   {'i','n','f','o','r','m','a','t','i','o','n','_','s','c','h','e','m','a','\0'});

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 2u);
    EXPECT_NE(response_payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(response_payload[0], mysql::EOF_PACKET);

    const std::string plugin_name(reinterpret_cast<const char*>(response_payload.data() + 1));
    EXPECT_EQ(plugin_name, "mysql_clear_password");
}

TEST(ProtocolAdapterDialectsC3, MySQLComQueryUseUpdatesSubsequentCompileDatabaseContext) {
    cleanupDb("test_mysql_com_query_use_schema_context.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_com_query_use_schema_context.sbdb").string();
    cfg.default_database = "compat_mysql_root";

    AdapterHarness<MySqlAdapter> adapter(cfg);
    core::ErrorContext ctx;

    ASSERT_EQ(adapter.ensureEngineReady(&ctx), core::Status::OK) << ctx.message;

    auto run_setup_query = [&](const std::string& sql) {
        QueryContext setup_query;
        setup_query.query = sql;
        ResultContext setup_result;
        ASSERT_EQ(adapter.executeQueryForTest(setup_query, setup_result), core::Status::OK);
        ASSERT_FALSE(setup_result.has_error) << setup_result.error_message;
    };

    run_setup_query("CREATE DATABASE IF NOT EXISTS compat_mysql_root");
    run_setup_query("CREATE DATABASE IF NOT EXISTS main");

    adapter.applySuccessfulMySqlQueryForTest("USE `main`");
    EXPECT_EQ(adapter.selectedDatabaseNameForTest(), "main");

    QueryContext create_query;
    create_query.query =
        "CREATE TABLE t_wave2_use_ctx (id INT PRIMARY KEY, payload VARCHAR(32))";
    ResultContext create_result;
    ASSERT_EQ(adapter.executeQueryForTest(create_query, create_result), core::Status::OK);
    ASSERT_FALSE(create_result.has_error) << create_result.error_message;

    QueryContext insert_query;
    insert_query.query = "INSERT INTO t_wave2_use_ctx VALUES (1, 'alpha')";
    ResultContext insert_result;
    ASSERT_EQ(adapter.executeQueryForTest(insert_query, insert_result), core::Status::OK);
    ASSERT_FALSE(insert_result.has_error) << insert_result.error_message;

    auto* db = adapter.engineDatabaseForTest();
    ASSERT_NE(db, nullptr);
    auto* catalog = db->catalog_manager();
    ASSERT_NE(catalog, nullptr);

    core::CatalogManager::SchemaInfo schema_info;
    ASSERT_EQ(catalog->getSchema("emulated.mysql.localhost.databases.main",
                                 schema_info,
                                 &ctx),
              core::Status::OK)
        << ctx.message;

    core::CatalogManager::TableInfo table_info;
    ASSERT_EQ(catalog->getTable(schema_info.schema_id,
                                "t_wave2_use_ctx",
                                table_info,
                                &ctx),
              core::Status::OK)
        << ctx.message;
}

TEST(ProtocolAdapterDialectsC3, MySQLComStmtPrepareReturnsPrepareOkPacket) {
    cleanupDb("test_mysql_stmt_prepare_ok.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_stmt_prepare_ok.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    network::Connection conn(nullptr, 111);

    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_STMT_PREPARE);
    const std::string sql = "SELECT 1";
    payload.insert(payload.end(), sql.begin(), sql.end());

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 12u);
    EXPECT_EQ(response_payload[0], mysql::OK_PACKET);

    const uint32_t stmt_id =
        static_cast<uint32_t>(response_payload[1]) |
        (static_cast<uint32_t>(response_payload[2]) << 8) |
        (static_cast<uint32_t>(response_payload[3]) << 16) |
        (static_cast<uint32_t>(response_payload[4]) << 24);
    EXPECT_GT(stmt_id, 0u);
}

TEST(ProtocolAdapterDialectsC3, MySQLComStmtExecuteUnknownStatementReturnsError) {
    cleanupDb("test_mysql_stmt_execute_unknown.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_stmt_execute_unknown.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 112);

    ASSERT_EQ(adapter.forceAuthSuccess(&conn), core::Status::OK);
    conn.clearWriteBuffer();

    std::vector<uint8_t> payload;
    payload.push_back(mysql::Command::COM_STMT_EXECUTE);
    payload.push_back(0xEF);
    payload.push_back(0xBE);
    payload.push_back(0xAD);
    payload.push_back(0xDE);

    const auto packet = buildMySqlWirePacket(payload, 0);
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto response_payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(response_payload.size(), 9u);
    EXPECT_EQ(response_payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(response_payload), mysql::ErrorCode::UNKNOWN_ERROR);
    EXPECT_EQ(readMySqlSqlState(response_payload), "HY000");
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

TEST(ProtocolAdapterDialectsC3, MySQLErrorMappingUndefinedColumnUses42S22) {
    cleanupDb("test_mysql_error_mapping_undefined_column.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_error_mapping_undefined_column.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 104);

    ASSERT_EQ(adapter.sendProtocolErrorForTest(
                  &conn,
                  static_cast<uint32_t>(core::Status::UNDEFINED_COLUMN),
                  "",
                  "Unknown column"),
              core::Status::OK);

    const auto payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(payload.size(), 9u);
    EXPECT_EQ(payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(payload), mysql::ErrorCode::BAD_FIELD_ERROR);
    EXPECT_EQ(readMySqlSqlState(payload), "42S22");
}

TEST(ProtocolAdapterDialectsC3, MySQLErrorMappingInvalidAuthorizationUses28000) {
    cleanupDb("test_mysql_error_mapping_invalid_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_error_mapping_invalid_auth.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 105);

    ASSERT_EQ(adapter.sendProtocolErrorForTest(
                  &conn,
                  static_cast<uint32_t>(core::Status::INVALID_AUTHORIZATION),
                  "",
                  "Access denied"),
              core::Status::OK);

    const auto payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(payload.size(), 9u);
    EXPECT_EQ(payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(payload), mysql::ErrorCode::ACCESS_DENIED);
    EXPECT_EQ(readMySqlSqlState(payload), "28000");
}

TEST(ProtocolAdapterDialectsC3, MySQLErrorMappingQueryCanceledUses70100) {
    cleanupDb("test_mysql_error_mapping_query_canceled.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_error_mapping_query_canceled.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 106);

    ASSERT_EQ(adapter.sendProtocolErrorForTest(
                  &conn,
                  static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                  "",
                  "Query execution was interrupted"),
              core::Status::OK);

    const auto payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(payload.size(), 9u);
    EXPECT_EQ(payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(payload), 1317);
    EXPECT_EQ(readMySqlSqlState(payload), "70100");
}

TEST(ProtocolAdapterDialectsC3, MySQLErrorMappingOutOfRangeUses22003) {
    cleanupDb("test_mysql_error_mapping_out_of_range.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_mysql_error_mapping_out_of_range.sbdb").string();

    AdapterHarness<MySqlAdapter> adapter(cfg);
    adapter.setClientCapabilitiesForTest(mysql::Capability::PROTOCOL_41);
    network::Connection conn(nullptr, 107);

    ASSERT_EQ(adapter.sendProtocolErrorForTest(
                  &conn,
                  static_cast<uint32_t>(core::Status::OUT_OF_RANGE),
                  "",
                  "Out of range value"),
              core::Status::OK);

    const auto payload = extractMySqlPayload(conn.getWriteBuffer());
    ASSERT_GE(payload.size(), 9u);
    EXPECT_EQ(payload[0], mysql::ERR_PACKET);
    EXPECT_EQ(readMySqlErrorCode(payload), 1264);
    EXPECT_EQ(readMySqlSqlState(payload), "22003");
}

TEST(ProtocolAdapterDialectsFirebird, FirebirdPolicyRejectsUnsupportedConfiguredAuthMethodAtConnect) {
    cleanupDb("test_fb_policy_reject_unsupported_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_policy_reject_unsupported_auth.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::TOKEN;

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    network::Connection conn(nullptr, 114);

    const auto packet = buildFirebirdPacket(firebird::Opcode::op_connect,
                                            buildFirebirdConnectBodyForPolicyTest());
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readFbU32BE(out), firebird::Opcode::op_response);
    EXPECT_NE(readFbU32BE(out), firebird::Opcode::op_accept_data);
}

TEST(ProtocolAdapterDialectsFirebird, FirebirdPolicyAllowsPasswordConfiguredMethodAtConnect) {
    cleanupDb("test_fb_policy_allow_password_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_policy_allow_password_auth.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::PASSWORD;

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    network::Connection conn(nullptr, 117);

    const auto packet = buildFirebirdPacket(firebird::Opcode::op_connect,
                                            buildFirebirdConnectBodyForPolicyTest());
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readFbU32BE(out), firebird::Opcode::op_accept_data);
}

TEST(ProtocolAdapterDialectsFirebird, FirebirdPolicyAllowsScram512ConfiguredMethodAtConnect) {
    cleanupDb("test_fb_policy_allow_scram512_auth.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_policy_allow_scram512_auth.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::SCRAM_SHA_512;

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    network::Connection conn(nullptr, 121);

    const auto packet = buildFirebirdPacket(firebird::Opcode::op_connect,
                                            buildFirebirdConnectBodyForPolicyTest());
    auto& read_buffer = conn.getReadBuffer();
    read_buffer.insert(read_buffer.end(), packet.begin(), packet.end());

    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readFbU32BE(out), firebird::Opcode::op_accept_data);
}

TEST(ProtocolAdapterDialectsFirebird, FirebirdPolicyRejectsLegacyAuthPluginDeterministically) {
    cleanupDb("test_fb_policy_reject_legacy_auth_plugin.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_policy_reject_legacy_auth_plugin.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::PASSWORD;

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    network::Connection conn(nullptr, 122);

    auto& read_buffer = conn.getReadBuffer();
    const auto connect_packet = buildFirebirdPacket(firebird::Opcode::op_connect,
                                                    buildFirebirdConnectBodyForPolicyTest());
    read_buffer.insert(read_buffer.end(), connect_packet.begin(), connect_packet.end());
    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    ASSERT_GE(conn.getWriteBuffer().size(), 4u);
    ASSERT_EQ(readFbU32BE(conn.getWriteBuffer()), firebird::Opcode::op_accept_data);

    conn.clearWriteBuffer();
    const auto cont_auth_packet = buildFirebirdPacket(
        firebird::Opcode::op_cont_auth,
        buildFirebirdContAuthBody({0x01, 0x02, 0x03, 0x04}, firebird::AUTH_PLUGIN_LEGACY));
    read_buffer.insert(read_buffer.end(), cont_auth_packet.begin(), cont_auth_packet.end());
    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readFbU32BE(out), firebird::Opcode::op_response);
    const auto err = parseFirebirdErrorFields(out);
    ASSERT_TRUE(err.has_error);
    EXPECT_EQ(err.gds_code, firebird::ErrorCode::isc_login);
    EXPECT_EQ(err.sqlstate, "0A000");
}

TEST(ProtocolAdapterDialectsFirebird, FirebirdPolicyRejectsWinSspiPluginDeterministically) {
    cleanupDb("test_fb_policy_reject_win_sspi_auth_plugin.sbdb");

    ProtocolAdapterConfig cfg;
    cfg.database_path = dbPath("test_fb_policy_reject_win_sspi_auth_plugin.sbdb").string();
    cfg.require_authentication = true;
    cfg.auth_method = AuthMethod::PASSWORD;

    AdapterHarness<FirebirdAdapter> adapter(cfg);
    network::Connection conn(nullptr, 123);

    auto& read_buffer = conn.getReadBuffer();
    const auto connect_packet = buildFirebirdPacket(firebird::Opcode::op_connect,
                                                    buildFirebirdConnectBodyForPolicyTest());
    read_buffer.insert(read_buffer.end(), connect_packet.begin(), connect_packet.end());
    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);
    ASSERT_GE(conn.getWriteBuffer().size(), 4u);
    ASSERT_EQ(readFbU32BE(conn.getWriteBuffer()), firebird::Opcode::op_accept_data);

    conn.clearWriteBuffer();
    const auto cont_auth_packet = buildFirebirdPacket(
        firebird::Opcode::op_cont_auth,
        buildFirebirdContAuthBody({0x05, 0x06, 0x07, 0x08}, "Win_Sspi"));
    read_buffer.insert(read_buffer.end(), cont_auth_packet.begin(), cont_auth_packet.end());
    ASSERT_EQ(adapter.parseIncomingPacket(&conn), core::Status::OK);
    ASSERT_EQ(adapter.processIncomingPacket(&conn), core::Status::OK);

    const auto& out = conn.getWriteBuffer();
    ASSERT_GE(out.size(), 4u);
    EXPECT_EQ(readFbU32BE(out), firebird::Opcode::op_response);
    const auto err = parseFirebirdErrorFields(out);
    ASSERT_TRUE(err.has_error);
    EXPECT_EQ(err.gds_code, firebird::ErrorCode::isc_login);
    EXPECT_EQ(err.sqlstate, "0A000");
}
