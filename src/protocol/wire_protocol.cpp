/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Wire Protocol Implementation
 *
 * Local Server Architecture - Phase 2
 */

#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/protocol/sbwp_protocol.h"
#include "scratchbird/server/ipc_server.h"

#include <cstring>
#include <random>
#include <chrono>
#include <algorithm>

namespace scratchbird {
namespace protocol {

// ============================================================================
// Message Class Implementation
// ============================================================================

Message::Message() {
    std::memset(&header_, 0, sizeof(header_));
    header_.magic = PROTOCOL_MAGIC;
    header_.version = PROTOCOL_VERSION;
}

Message::Message(MessageType type) : Message() {
    header_.type = static_cast<uint8_t>(type);
}

Message::~Message() = default;

Message::Message(Message&& other) noexcept
    : header_(other.header_)
    , payload_(std::move(other.payload_))
    , read_offset_(other.read_offset_)
{
    std::memset(&other.header_, 0, sizeof(other.header_));
    other.read_offset_ = 0;
}

Message& Message::operator=(Message&& other) noexcept {
    if (this != &other) {
        header_ = other.header_;
        payload_ = std::move(other.payload_);
        read_offset_ = other.read_offset_;
        std::memset(&other.header_, 0, sizeof(other.header_));
        other.read_offset_ = 0;
    }
    return *this;
}

void Message::clearPayload() {
    payload_.clear();
    read_offset_ = 0;
    updatePayloadLength();
}

void Message::reservePayload(size_t size) {
    payload_.reserve(size);
}

// Write methods
void Message::writeUInt8(uint8_t value) {
    payload_.push_back(value);
    updatePayloadLength();
}

void Message::writeUInt16(uint16_t value) {
    payload_.push_back(static_cast<uint8_t>(value & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    updatePayloadLength();
}

void Message::writeUInt32(uint32_t value) {
    payload_.push_back(static_cast<uint8_t>(value & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    payload_.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    updatePayloadLength();
}

void Message::writeUInt64(uint64_t value) {
    for (int i = 0; i < 8; i++) {
        payload_.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
    updatePayloadLength();
}

void Message::writeInt32(int32_t value) {
    writeUInt32(static_cast<uint32_t>(value));
}

void Message::writeInt64(int64_t value) {
    writeUInt64(static_cast<uint64_t>(value));
}

void Message::writeFloat(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUInt32(bits);
}

void Message::writeDouble(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    writeUInt64(bits);
}

void Message::writeBytes(const void* data, size_t length) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    payload_.insert(payload_.end(), ptr, ptr + length);
    updatePayloadLength();
}

void Message::writeString(const std::string& str) {
    writeBytes(str.data(), str.size());
}

void Message::writeLengthPrefixedString(const std::string& str) {
    writeUInt32(static_cast<uint32_t>(str.size()));
    writeString(str);
}

void Message::writeNullTerminatedString(const std::string& str, size_t max_length) {
    size_t copy_len = std::min(str.size(), max_length - 1);
    writeBytes(str.data(), copy_len);
    // Pad with zeros to max_length
    for (size_t i = copy_len; i < max_length; i++) {
        writeUInt8(0);
    }
}

// Read methods
bool Message::readUInt8(uint8_t& value) {
    if (read_offset_ + 1 > payload_.size()) return false;
    value = payload_[read_offset_++];
    return true;
}

bool Message::readUInt16(uint16_t& value) {
    if (read_offset_ + 2 > payload_.size()) return false;
    value = static_cast<uint16_t>(payload_[read_offset_]) |
            (static_cast<uint16_t>(payload_[read_offset_ + 1]) << 8);
    read_offset_ += 2;
    return true;
}

bool Message::readUInt32(uint32_t& value) {
    if (read_offset_ + 4 > payload_.size()) return false;
    value = static_cast<uint32_t>(payload_[read_offset_]) |
            (static_cast<uint32_t>(payload_[read_offset_ + 1]) << 8) |
            (static_cast<uint32_t>(payload_[read_offset_ + 2]) << 16) |
            (static_cast<uint32_t>(payload_[read_offset_ + 3]) << 24);
    read_offset_ += 4;
    return true;
}

bool Message::readUInt64(uint64_t& value) {
    if (read_offset_ + 8 > payload_.size()) return false;
    value = 0;
    for (int i = 0; i < 8; i++) {
        value |= static_cast<uint64_t>(payload_[read_offset_ + i]) << (i * 8);
    }
    read_offset_ += 8;
    return true;
}

bool Message::readInt32(int32_t& value) {
    uint32_t u;
    if (!readUInt32(u)) return false;
    value = static_cast<int32_t>(u);
    return true;
}

bool Message::readInt64(int64_t& value) {
    uint64_t u;
    if (!readUInt64(u)) return false;
    value = static_cast<int64_t>(u);
    return true;
}

bool Message::readFloat(float& value) {
    uint32_t bits;
    if (!readUInt32(bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool Message::readDouble(double& value) {
    uint64_t bits;
    if (!readUInt64(bits)) return false;
    std::memcpy(&value, &bits, sizeof(value));
    return true;
}

bool Message::readBytes(void* buffer, size_t length) {
    if (read_offset_ + length > payload_.size()) return false;
    std::memcpy(buffer, payload_.data() + read_offset_, length);
    read_offset_ += length;
    return true;
}

bool Message::readString(std::string& str, size_t length) {
    if (read_offset_ + length > payload_.size()) return false;
    str.assign(reinterpret_cast<const char*>(payload_.data() + read_offset_), length);
    read_offset_ += length;
    return true;
}

bool Message::readLengthPrefixedString(std::string& str) {
    uint32_t length;
    if (!readUInt32(length)) return false;
    if (length > MAX_MESSAGE_SIZE) return false;
    return readString(str, length);
}

bool Message::readNullTerminatedString(std::string& str, size_t max_length) {
    if (read_offset_ + max_length > payload_.size()) return false;

    const char* start = reinterpret_cast<const char*>(payload_.data() + read_offset_);
    size_t len = strnlen(start, max_length);
    str.assign(start, len);
    read_offset_ += max_length;  // Always advance by max_length
    return true;
}

// Serialization
core::Status Message::serialize(std::vector<uint8_t>& buffer) const {
    buffer.resize(sizeof(MessageHeader) + payload_.size());

    // Serialize header
    std::memcpy(buffer.data(), &header_, sizeof(MessageHeader));

    // Copy payload
    if (!payload_.empty()) {
        std::memcpy(buffer.data() + sizeof(MessageHeader), payload_.data(), payload_.size());
    }

    return core::Status::OK;
}

core::Status Message::serializeHeader(uint8_t* buffer) const {
    std::memcpy(buffer, &header_, sizeof(MessageHeader));
    return core::Status::OK;
}

// Deserialization
core::Status Message::parseHeader(const uint8_t* data, MessageHeader& header,
                                  core::ErrorContext* ctx) {
    std::memcpy(&header, data, sizeof(MessageHeader));

    if (!validateHeader(header)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid message header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

core::Status Message::setPayload(const uint8_t* data, size_t length) {
    payload_.assign(data, data + length);
    header_.payload_length = static_cast<uint32_t>(length);
    read_offset_ = 0;
    return core::Status::OK;
}

// Validation
bool Message::isValid() const {
    return validateHeader(header_);
}

bool Message::validateHeader(const MessageHeader& header) {
    if (header.magic != PROTOCOL_MAGIC) {
        return false;
    }

    if (header.payload_length > MAX_MESSAGE_SIZE) {
        return false;
    }

    return true;
}

// ============================================================================
// ProtocolCodec Implementation
// ============================================================================

// Connection Messages

Message ProtocolCodec::buildConnectRequest(const std::string& database,
                                           const std::string& client_name,
                                           uint32_t client_pid,
                                           uint16_t client_flags) {
    Message msg(MessageType::CONNECT_REQUEST);
    msg.reservePayload(sizeof(ConnectRequestPayload));

    msg.writeUInt16(PROTOCOL_VERSION);
    msg.writeUInt16(client_flags);
    msg.writeUInt32(client_pid);
    msg.writeNullTerminatedString(database, 256);
    msg.writeNullTerminatedString(client_name, 64);
    msg.writeNullTerminatedString("1.0.0", 32);  // client_version

    return msg;
}

core::Status ProtocolCodec::parseConnectRequest(const Message& msg,
                                                std::string& database,
                                                std::string& client_name,
                                                uint32_t& client_pid,
                                                uint16_t* client_flags_out,
                                                core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint16_t version = 0;
    uint16_t flags = 0;
    if (!m.readUInt16(version) || !m.readUInt16(flags) || !m.readUInt32(client_pid)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated CONNECT_REQUEST");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (!m.readNullTerminatedString(database, 256) ||
        !m.readNullTerminatedString(client_name, 64)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid CONNECT_REQUEST strings");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (client_flags_out) {
        *client_flags_out = flags;
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildConnectResponse(bool success,
                                            const uint8_t session_id[16],
                                            const std::string& error_message,
                                            uint16_t server_flags) {
    Message msg(MessageType::CONNECT_RESPONSE);
    msg.reservePayload(sizeof(ConnectResponsePayload));

    msg.writeUInt8(success ? 0 : 1);
    msg.writeUInt16(PROTOCOL_VERSION);
    msg.writeUInt16(server_flags);
    msg.writeBytes(session_id, 16);
    msg.writeNullTerminatedString("ScratchBird", 64);
    msg.writeNullTerminatedString("1.0.0-alpha1", 32);

    if (!success && !error_message.empty()) {
        msg.writeLengthPrefixedString(error_message);
    }

    return msg;
}

core::Status ProtocolCodec::parseConnectResponse(const Message& msg,
                                                 bool& success,
                                                 uint8_t session_id[16],
                                                 std::string& error_message,
                                                 uint16_t* server_flags_out,
                                                 core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint8_t status;
    uint16_t version, flags;

    if (!m.readUInt8(status) || !m.readUInt16(version) || !m.readUInt16(flags) ||
        !m.readBytes(session_id, 16)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated CONNECT_RESPONSE");
        return core::Status::PROTOCOL_VIOLATION;
    }

    success = (status == 0);
    if (server_flags_out) {
        *server_flags_out = flags;
    }

    // Skip server name and version
    std::string server_name, server_version;
    m.readNullTerminatedString(server_name, 64);
    m.readNullTerminatedString(server_version, 32);

    // Read error message if present
    if (!success && m.getRemainingBytes() > 0) {
        m.readLengthPrefixedString(error_message);
    }

    return core::Status::OK;
}

// Authentication Messages

Message ProtocolCodec::buildAuthRequest(const uint8_t session_id[16],
                                        const std::string& username,
                                        const std::string& password) {
    std::vector<uint8_t> payload(password.begin(), password.end());
    return buildAuthRequest(session_id, username, AuthMethod::PASSWORD, payload);
}

core::Status ProtocolCodec::parseAuthRequest(const Message& msg,
                                             uint8_t session_id[16],
                                             std::string& username,
                                             std::string& password,
                                             core::ErrorContext* ctx) {
    AuthMethod auth_method = AuthMethod::PASSWORD;
    std::vector<uint8_t> payload;
    auto status = parseAuthRequest(msg, session_id, username, auth_method, payload, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    password.assign(reinterpret_cast<const char*>(payload.data()), payload.size());
    return core::Status::OK;
}

Message ProtocolCodec::buildAuthRequest(const uint8_t session_id[16],
                                        const std::string& username,
                                        AuthMethod auth_method,
                                        const std::vector<uint8_t>& payload) {
    Message msg(MessageType::AUTH_REQUEST);

    msg.writeBytes(session_id, 16);
    msg.writeNullTerminatedString(username, 64);
    msg.writeUInt8(static_cast<uint8_t>(auth_method));
    msg.writeUInt16(static_cast<uint16_t>(payload.size()));
    if (!payload.empty()) {
        msg.writeBytes(payload.data(), payload.size());
    }

    return msg;
}

core::Status ProtocolCodec::parseAuthRequest(const Message& msg,
                                             uint8_t session_id[16],
                                             std::string& username,
                                             AuthMethod& auth_method,
                                             std::vector<uint8_t>& payload,
                                             core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readBytes(session_id, 16)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated AUTH_REQUEST");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (!m.readNullTerminatedString(username, 64)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid AUTH_REQUEST username");
        return core::Status::PROTOCOL_VIOLATION;
    }

    uint8_t auth_method_byte;
    uint16_t cred_length;
    if (!m.readUInt8(auth_method_byte) || !m.readUInt16(cred_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid AUTH_REQUEST credentials header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    payload.resize(cred_length);
    if (cred_length > 0 && !m.readBytes(payload.data(), cred_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid AUTH_REQUEST password");
        return core::Status::PROTOCOL_VIOLATION;
    }

    auth_method = static_cast<AuthMethod>(auth_method_byte);
    return core::Status::OK;
}

Message ProtocolCodec::buildAuthResponse(bool success,
                                         uint32_t user_id,
                                         const std::string& error_message) {
    return buildAuthResponse(success ? AuthStatus::OK : AuthStatus::ERROR,
                             user_id,
                             error_message);
}

core::Status ProtocolCodec::parseAuthResponse(const Message& msg,
                                              bool& success,
                                              uint32_t& user_id,
                                              std::string& error_message,
                                              core::ErrorContext* ctx) {
    AuthStatus status = AuthStatus::ERROR;
    auto parse_status = parseAuthResponse(msg, status, user_id, error_message, nullptr, ctx);
    if (parse_status != core::Status::OK) {
        return parse_status;
    }

    success = (status == AuthStatus::OK);
    return core::Status::OK;
}

Message ProtocolCodec::buildAuthResponse(AuthStatus status,
                                         uint32_t user_id,
                                         const std::string& error_message,
                                         const std::vector<uint8_t>& data) {
    Message msg(MessageType::AUTH_RESPONSE);

    msg.writeUInt8(static_cast<uint8_t>(status));
    msg.writeUInt32(user_id);
    msg.writeNullTerminatedString(error_message, 256);
    if (!data.empty()) {
        msg.writeBytes(data.data(), data.size());
    }

    return msg;
}

core::Status ProtocolCodec::parseAuthResponse(const Message& msg,
                                              AuthStatus& status,
                                              uint32_t& user_id,
                                              std::string& error_message,
                                              std::vector<uint8_t>* data,
                                              core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint8_t status_byte;
    if (!m.readUInt8(status_byte) || !m.readUInt32(user_id)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated AUTH_RESPONSE");
        return core::Status::PROTOCOL_VIOLATION;
    }

    status = static_cast<AuthStatus>(status_byte);

    m.readNullTerminatedString(error_message, 256);

    if (data) {
        size_t remaining = m.getRemainingBytes();
        data->clear();
        if (remaining > 0) {
            data->resize(remaining);
            if (!m.readBytes(data->data(), remaining)) {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Truncated AUTH_RESPONSE payload");
                return core::Status::PROTOCOL_VIOLATION;
            }
        }
    }

    return core::Status::OK;
}

// Query Messages

Message ProtocolCodec::buildQuery(const uint8_t session_id[16],
                                  const std::string& query,
                                  uint8_t flags) {
    Message msg(MessageType::QUERY);

    msg.writeBytes(session_id, 16);
    msg.writeUInt32(static_cast<uint32_t>(query.size()));
    msg.writeUInt8(flags);
    msg.writeString(query);

    return msg;
}

Message ProtocolCodec::buildQueryCancel() {
    return Message(MessageType::QUERY_CANCEL);
}

core::Status ProtocolCodec::parseQuery(const Message& msg,
                                       uint8_t session_id[16],
                                       std::string& query,
                                       uint8_t& flags,
                                       std::vector<uint8_t>* bytecode_out,
                                       core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readBytes(session_id, 16)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY session_id");
        return core::Status::PROTOCOL_VIOLATION;
    }

    uint32_t query_length;
    if (!m.readUInt32(query_length) || !m.readUInt8(flags)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (query_length > MAX_QUERY_LENGTH) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Query too long");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (flags & static_cast<uint8_t>(QueryFlags::BYTECODE)) {
        if (!bytecode_out) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Missing bytecode output for QUERY");
            return core::Status::PROTOCOL_VIOLATION;
        }
        bytecode_out->clear();
        if (query_length > 0) {
            bytecode_out->resize(query_length);
            if (!m.readBytes(bytecode_out->data(), query_length)) {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Truncated QUERY bytecode");
                return core::Status::PROTOCOL_VIOLATION;
            }
        }
        if (flags & static_cast<uint8_t>(QueryFlags::BYTECODE_HAS_SQL)) {
            uint32_t sql_length;
            if (!m.readUInt32(sql_length)) {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Truncated QUERY SQL length");
                return core::Status::PROTOCOL_VIOLATION;
            }
            if (sql_length > MAX_QUERY_LENGTH) {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Query SQL too long");
                return core::Status::PROTOCOL_VIOLATION;
            }
            if (!m.readString(query, sql_length)) {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Truncated QUERY SQL text");
                return core::Status::PROTOCOL_VIOLATION;
            }
        } else {
            query.clear();
        }
        return core::Status::OK;
    }

    if (!m.readString(query, query_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY text");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

core::Status ProtocolCodec::parseQuery(const Message& msg,
                                       uint8_t session_id[16],
                                       std::string& query,
                                       uint8_t& flags,
                                       core::ErrorContext* ctx) {
    return parseQuery(msg, session_id, query, flags, nullptr, ctx);
}

Message ProtocolCodec::buildQueryBytecode(const uint8_t session_id[16],
                                          const std::vector<uint8_t>& bytecode,
                                          const std::string& sql,
                                          uint8_t flags) {
    Message msg(MessageType::QUERY);
    uint8_t out_flags = flags | static_cast<uint8_t>(QueryFlags::BYTECODE);
    if (!sql.empty()) {
        out_flags |= static_cast<uint8_t>(QueryFlags::BYTECODE_HAS_SQL);
    }

    msg.writeBytes(session_id, 16);
    msg.writeUInt32(static_cast<uint32_t>(bytecode.size()));
    msg.writeUInt8(out_flags);
    if (!bytecode.empty()) {
        msg.writeBytes(bytecode.data(), bytecode.size());
    }
    if (!sql.empty()) {
        msg.writeUInt32(static_cast<uint32_t>(sql.size()));
        msg.writeString(sql);
    }

    return msg;
}

Message ProtocolCodec::buildQueryBytecode(const uint8_t session_id[16],
                                          const std::vector<uint8_t>& bytecode,
                                          uint8_t flags) {
    return buildQueryBytecode(session_id, bytecode, std::string(), flags);
}
Message ProtocolCodec::buildQueryError(uint32_t error_code,
                                       const std::string& sqlstate,
                                       const std::string& message,
                                       const std::string& detail,
                                       const std::string& hint) {
    Message msg(MessageType::QUERY_ERROR);

    msg.writeUInt32(error_code);

    // SQLSTATE: 5 chars + null
    char sqlstate_buf[6] = {0};
    std::strncpy(sqlstate_buf, sqlstate.c_str(), 5);
    msg.writeBytes(sqlstate_buf, 6);

    msg.writeUInt16(static_cast<uint16_t>(std::min(message.size(), static_cast<size_t>(MAX_ERROR_MESSAGE_LENGTH))));
    msg.writeUInt16(static_cast<uint16_t>(std::min(detail.size(), static_cast<size_t>(MAX_ERROR_MESSAGE_LENGTH))));
    msg.writeUInt16(static_cast<uint16_t>(std::min(hint.size(), static_cast<size_t>(MAX_ERROR_MESSAGE_LENGTH))));

    msg.writeString(message.substr(0, MAX_ERROR_MESSAGE_LENGTH));
    msg.writeString(detail.substr(0, MAX_ERROR_MESSAGE_LENGTH));
    msg.writeString(hint.substr(0, MAX_ERROR_MESSAGE_LENGTH));

    return msg;
}

core::Status ProtocolCodec::parseQueryError(const Message& msg,
                                            uint32_t& error_code,
                                            std::string& sqlstate,
                                            std::string& message,
                                            std::string& detail,
                                            std::string& hint,
                                            core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readUInt32(error_code)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY_ERROR");
        return core::Status::PROTOCOL_VIOLATION;
    }

    char sqlstate_buf[6];
    if (!m.readBytes(sqlstate_buf, 6)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY_ERROR sqlstate");
        return core::Status::PROTOCOL_VIOLATION;
    }
    sqlstate.assign(sqlstate_buf, strnlen(sqlstate_buf, 5));

    uint16_t msg_len, detail_len, hint_len;
    if (!m.readUInt16(msg_len) || !m.readUInt16(detail_len) || !m.readUInt16(hint_len)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY_ERROR lengths");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (!m.readString(message, msg_len) ||
        !m.readString(detail, detail_len) ||
        !m.readString(hint, hint_len)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY_ERROR strings");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

// Result Messages

Message ProtocolCodec::buildRowDescription(const std::vector<ColumnInfo>& columns) {
    Message msg(MessageType::ROW_DESCRIPTION);

    auto mapWireTypeToOid = [](WireType type) -> uint32_t {
        switch (type) {
            case WireType::NULL_TYPE: return 0;
            case WireType::BOOLEAN: return sbwp::kOidBool;
            case WireType::INT16: return sbwp::kOidInt2;
            case WireType::INT32: return sbwp::kOidInt4;
            case WireType::INT64: return sbwp::kOidInt8;
            case WireType::FLOAT32: return sbwp::kOidFloat4;
            case WireType::FLOAT64: return sbwp::kOidFloat8;
            case WireType::DECIMAL: return sbwp::kOidNumeric;
            case WireType::VARCHAR: return sbwp::kOidVarchar;
            case WireType::CHAR: return sbwp::kOidChar;
            case WireType::BYTEA: return sbwp::kOidBytea;
            case WireType::DATE: return sbwp::kOidDate;
            case WireType::TIME: return sbwp::kOidTime;
            case WireType::TIMESTAMP: return sbwp::kOidTimestamp;
            case WireType::TIMESTAMPTZ: return sbwp::kOidTimestamptz;
            case WireType::INTERVAL: return sbwp::kOidInterval;
            case WireType::UUID: return sbwp::kOidUuid;
            case WireType::JSON: return sbwp::kOidJson;
            case WireType::JSONB: return sbwp::kOidJsonb;
            case WireType::XML: return sbwp::kOidXml;
            case WireType::INET: return sbwp::kOidInet;
            case WireType::CIDR: return sbwp::kOidCidr;
            case WireType::MACADDR: return sbwp::kOidMacaddr;
            case WireType::TSVECTOR: return sbwp::kOidTsVector;
            case WireType::TSQUERY: return sbwp::kOidTsQuery;
            case WireType::VECTOR: return sbwp::kOidSbVector;
            case WireType::MONEY: return sbwp::kOidMoney;
            case WireType::ARRAY:
            case WireType::COMPOSITE:
            case WireType::RANGE:
            case WireType::GEOMETRY:
                return sbwp::kOidRecord;
            default:
                return 0;
        }
    };

    auto mapWireTypeSize = [](WireType type) -> int16_t {
        switch (type) {
            case WireType::BOOLEAN: return 1;
            case WireType::INT16: return 2;
            case WireType::INT32: return 4;
            case WireType::INT64: return 8;
            case WireType::FLOAT32: return 4;
            case WireType::FLOAT64: return 8;
            case WireType::DATE: return 4;
            case WireType::TIME: return 8;
            case WireType::TIMESTAMP: return 8;
            case WireType::TIMESTAMPTZ: return 8;
            case WireType::UUID: return 16;
            case WireType::MONEY: return 8;
            default:
                return -1;
        }
    };

    msg.writeUInt16(static_cast<uint16_t>(columns.size()));
    msg.writeUInt16(0);

    for (const auto& col : columns) {
        msg.writeUInt32(static_cast<uint32_t>(col.name.size()));
        msg.writeString(col.name);
        msg.writeUInt32(col.table_oid);
        msg.writeUInt16(col.column_index);
        uint32_t type_oid = col.type_oid != 0 ? col.type_oid : mapWireTypeToOid(col.type);
        msg.writeUInt32(type_oid);
        int16_t type_size = col.type_size != 0 ? col.type_size : mapWireTypeSize(col.type);
        msg.writeUInt16(static_cast<uint16_t>(type_size));
        msg.writeUInt32(col.type_modifier);
        msg.writeUInt8(col.format);
        msg.writeUInt8(col.nullable ? 1 : 0);
        msg.writeUInt16(0);
    }

    return msg;
}

core::Status ProtocolCodec::parseRowDescription(const Message& msg,
                                                std::vector<ColumnInfo>& columns,
                                                core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    auto mapOidToWireType = [](uint32_t oid) -> WireType {
        switch (oid) {
            case sbwp::kOidBool: return WireType::BOOLEAN;
            case sbwp::kOidInt2: return WireType::INT16;
            case sbwp::kOidInt4: return WireType::INT32;
            case sbwp::kOidInt8: return WireType::INT64;
            case sbwp::kOidFloat4: return WireType::FLOAT32;
            case sbwp::kOidFloat8: return WireType::FLOAT64;
            case sbwp::kOidNumeric: return WireType::DECIMAL;
            case sbwp::kOidVarchar: return WireType::VARCHAR;
            case sbwp::kOidChar: return WireType::CHAR;
            case sbwp::kOidText: return WireType::VARCHAR;
            case sbwp::kOidBytea: return WireType::BYTEA;
            case sbwp::kOidDate: return WireType::DATE;
            case sbwp::kOidTime: return WireType::TIME;
            case sbwp::kOidTimestamp: return WireType::TIMESTAMP;
            case sbwp::kOidTimestamptz: return WireType::TIMESTAMPTZ;
            case sbwp::kOidInterval: return WireType::INTERVAL;
            case sbwp::kOidUuid: return WireType::UUID;
            case sbwp::kOidJson: return WireType::JSON;
            case sbwp::kOidJsonb: return WireType::JSONB;
            case sbwp::kOidXml: return WireType::XML;
            case sbwp::kOidInet: return WireType::INET;
            case sbwp::kOidCidr: return WireType::CIDR;
            case sbwp::kOidMacaddr: return WireType::MACADDR;
            case sbwp::kOidTsVector: return WireType::TSVECTOR;
            case sbwp::kOidTsQuery: return WireType::TSQUERY;
            case sbwp::kOidSbVector: return WireType::VECTOR;
            case sbwp::kOidMoney: return WireType::MONEY;
            case sbwp::kOidRecord: return WireType::COMPOSITE;
            default:
                return WireType::UNKNOWN;
        }
    };

    uint16_t column_count;
    uint16_t reserved;
    if (!m.readUInt16(column_count) || !m.readUInt16(reserved)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated ROW_DESCRIPTION");
        return core::Status::PROTOCOL_VIOLATION;
    }

    columns.clear();
    columns.reserve(column_count);

    for (uint16_t i = 0; i < column_count; i++) {
        ColumnInfo col;

        uint32_t name_length;
        if (!m.readUInt32(name_length) || !m.readString(col.name, name_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DESCRIPTION column name");
            return core::Status::PROTOCOL_VIOLATION;
        }

        uint16_t type_size = 0;
        uint16_t reserved_col = 0;
        uint8_t nullable = 0;
        if (!m.readUInt32(col.table_oid) ||
            !m.readUInt16(col.column_index) ||
            !m.readUInt32(col.type_oid) ||
            !m.readUInt16(type_size) ||
            !m.readUInt32(col.type_modifier) ||
            !m.readUInt8(col.format) ||
            !m.readUInt8(nullable) ||
            !m.readUInt16(reserved_col)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DESCRIPTION column metadata");
            return core::Status::PROTOCOL_VIOLATION;
        }

        col.type_size = static_cast<int16_t>(type_size);
        col.nullable = nullable != 0;
        col.type = mapOidToWireType(col.type_oid);
        columns.push_back(std::move(col));
    }

    return core::Status::OK;
}

// ColumnValue helpers
ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromInt32(int32_t value) {
    ColumnValue cv;
    cv.is_null = false;
    cv.data.resize(4);
    std::memcpy(cv.data.data(), &value, 4);
    return cv;
}

ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromInt64(int64_t value) {
    ColumnValue cv;
    cv.is_null = false;
    cv.data.resize(8);
    std::memcpy(cv.data.data(), &value, 8);
    return cv;
}

ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromDouble(double value) {
    ColumnValue cv;
    cv.is_null = false;
    cv.data.resize(8);
    std::memcpy(cv.data.data(), &value, 8);
    return cv;
}

ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromString(const std::string& value) {
    ColumnValue cv;
    cv.is_null = false;
    cv.data.assign(value.begin(), value.end());
    return cv;
}

ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromBool(bool value) {
    ColumnValue cv;
    cv.is_null = false;
    cv.data.resize(1);
    cv.data[0] = value ? 1 : 0;
    return cv;
}

ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromBytes(const uint8_t* data, size_t length) {
    ColumnValue cv;
    cv.is_null = false;
    if (data && length > 0) {
        cv.data.assign(data, data + length);
    }
    return cv;
}

ProtocolCodec::ColumnValue ProtocolCodec::ColumnValue::fromStream(uint64_t stream_id,
                                                                  uint64_t stream_length,
                                                                  const uint8_t* data,
                                                                  size_t length) {
    ColumnValue cv;
    cv.is_null = false;
    cv.is_stream = true;
    cv.stream_id = stream_id;
    cv.stream_length = stream_length;
    if (data && length > 0) {
        cv.data.assign(data, data + length);
    }
    return cv;
}

Message ProtocolCodec::buildRowData(const std::vector<ColumnValue>& values) {
    Message msg(MessageType::ROW_DATA);

    uint16_t column_count = static_cast<uint16_t>(values.size());
    uint16_t null_bytes = static_cast<uint16_t>((column_count + 7) / 8);
    msg.writeUInt16(column_count);
    msg.writeUInt16(null_bytes);

    std::vector<uint8_t> null_bitmap(null_bytes, 0);
    for (size_t i = 0; i < values.size(); ++i) {
        if (!values[i].is_null) {
            continue;
        }
        size_t byte_index = i / 8;
        uint8_t bit_index = static_cast<uint8_t>(i % 8);
        null_bitmap[byte_index] |= static_cast<uint8_t>(1u << bit_index);
    }
    if (!null_bitmap.empty()) {
        msg.writeBytes(null_bitmap.data(), null_bitmap.size());
    }

    for (const auto& val : values) {
        if (val.is_null) {
            continue;
        }
        if (val.is_stream) {
            msg.writeInt32(-2);
            msg.writeUInt64(val.stream_id);
            msg.writeUInt64(val.stream_length);
        } else {
            msg.writeInt32(static_cast<int32_t>(val.data.size()));
            if (!val.data.empty()) {
                msg.writeBytes(val.data.data(), val.data.size());
            }
        }
    }

    return msg;
}

core::Status ProtocolCodec::parseRowData(const Message& msg,
                                         std::vector<ColumnValue>& values,
                                         core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint16_t column_count;
    uint16_t null_bytes = 0;
    if (!m.readUInt16(column_count) || !m.readUInt16(null_bytes)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated ROW_DATA");
        return core::Status::PROTOCOL_VIOLATION;
    }

    values.clear();
    values.reserve(column_count);

    std::vector<uint8_t> null_bitmap;
    if (null_bytes > 0) {
        null_bitmap.resize(null_bytes);
        if (!m.readBytes(null_bitmap.data(), null_bytes)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DATA null bitmap");
            return core::Status::PROTOCOL_VIOLATION;
        }
    }

    for (uint16_t i = 0; i < column_count; i++) {
        size_t byte_index = i / 8;
        uint8_t bit_index = static_cast<uint8_t>(i % 8);
        bool is_null = byte_index < null_bitmap.size() &&
                       (null_bitmap[byte_index] & (1u << bit_index)) != 0;
        if (is_null) {
            ColumnValue val;
            val.is_null = true;
            values.push_back(std::move(val));
            continue;
        }
        int32_t length;
        if (!m.readInt32(length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DATA column length");
            return core::Status::PROTOCOL_VIOLATION;
        }

        ColumnValue val;
        if (length < 0) {
            if (length == -1) {
                val.is_null = true;
            } else if (length == -2) {
                uint64_t stream_id = 0;
                uint64_t stream_length = 0;
                if (!m.readUInt64(stream_id) || !m.readUInt64(stream_length)) {
                    SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                      "Truncated ROW_DATA stream reference");
                    return core::Status::PROTOCOL_VIOLATION;
                }
                val.is_null = false;
                val.is_stream = true;
                val.stream_id = stream_id;
                val.stream_length = stream_length;
            } else {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Invalid ROW_DATA column length");
                return core::Status::PROTOCOL_VIOLATION;
            }
        } else {
            val.is_null = false;
            val.data.resize(static_cast<size_t>(length));
            if (length > 0 && !m.readBytes(val.data.data(), static_cast<size_t>(length))) {
                SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                                  "Truncated ROW_DATA column value");
                return core::Status::PROTOCOL_VIOLATION;
            }
        }

        values.push_back(std::move(val));
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildEndOfResults() {
    return Message(MessageType::END_OF_RESULTS);
}

Message ProtocolCodec::buildCommandComplete(const std::string& command_tag,
                                            int64_t rows_affected) {
    Message msg(MessageType::COMMAND_COMPLETE);

    msg.writeNullTerminatedString(command_tag, 64);
    msg.writeInt64(rows_affected);

    return msg;
}

Message ProtocolCodec::buildSubscribe(uint8_t subscribe_type,
                                      const std::string& channel,
                                      const std::string& filter) {
    Message msg(MessageType::SUBSCRIBE);
    msg.writeUInt8(subscribe_type);
    msg.writeUInt8(0);
    msg.writeUInt8(0);
    msg.writeUInt8(0);
    msg.writeUInt32(static_cast<uint32_t>(channel.size()));
    if (!channel.empty()) {
        msg.writeBytes(channel.data(), channel.size());
    }
    msg.writeUInt32(static_cast<uint32_t>(filter.size()));
    if (!filter.empty()) {
        msg.writeBytes(filter.data(), filter.size());
    }
    return msg;
}

core::Status ProtocolCodec::parseSubscribe(const Message& msg,
                                           uint8_t& subscribe_type,
                                           std::string& channel,
                                           std::string& filter,
                                           core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint8_t reserved[3] = {0, 0, 0};
    uint32_t channel_length = 0;
    uint32_t filter_length = 0;
    if (!m.readUInt8(subscribe_type) ||
        !m.readBytes(reserved, sizeof(reserved)) ||
        !m.readUInt32(channel_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated SUBSCRIBE");
        return core::Status::PROTOCOL_VIOLATION;
    }

    channel.clear();
    if (channel_length > 0) {
        channel.resize(channel_length);
        if (!m.readBytes(channel.data(), channel_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated SUBSCRIBE channel");
            return core::Status::PROTOCOL_VIOLATION;
        }
    }

    if (!m.readUInt32(filter_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated SUBSCRIBE filter length");
        return core::Status::PROTOCOL_VIOLATION;
    }

    filter.clear();
    if (filter_length > 0) {
        filter.resize(filter_length);
        if (!m.readBytes(filter.data(), filter_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated SUBSCRIBE filter");
            return core::Status::PROTOCOL_VIOLATION;
        }
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildUnsubscribe(const std::string& channel) {
    Message msg(MessageType::UNSUBSCRIBE);
    msg.writeUInt32(static_cast<uint32_t>(channel.size()));
    if (!channel.empty()) {
        msg.writeBytes(channel.data(), channel.size());
    }
    return msg;
}

core::Status ProtocolCodec::parseUnsubscribe(const Message& msg,
                                             std::string& channel,
                                             core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint32_t channel_length = 0;
    if (!m.readUInt32(channel_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated UNSUBSCRIBE");
        return core::Status::PROTOCOL_VIOLATION;
    }

    channel.clear();
    if (channel_length > 0) {
        channel.resize(channel_length);
        if (!m.readBytes(channel.data(), channel_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated UNSUBSCRIBE channel");
            return core::Status::PROTOCOL_VIOLATION;
        }
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildNotification(uint32_t process_id,
                                         const std::string& channel,
                                         const std::vector<uint8_t>& payload,
                                         uint8_t change_type,
                                         uint64_t row_id) {
    Message msg(MessageType::NOTIFICATION);
    msg.writeUInt32(process_id);
    msg.writeUInt32(static_cast<uint32_t>(channel.size()));
    if (!channel.empty()) {
        msg.writeBytes(channel.data(), channel.size());
    }
    msg.writeUInt32(static_cast<uint32_t>(payload.size()));
    if (!payload.empty()) {
        msg.writeBytes(payload.data(), payload.size());
    }
    msg.writeUInt8(change_type);
    msg.writeUInt64(row_id);
    return msg;
}

core::Status ProtocolCodec::parseNotification(const Message& msg,
                                              uint32_t& process_id,
                                              std::string& channel,
                                              std::vector<uint8_t>& payload,
                                              uint8_t& change_type,
                                              uint64_t& row_id,
                                              core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint32_t channel_length = 0;
    uint32_t payload_length = 0;
    if (!m.readUInt32(process_id) ||
        !m.readUInt32(channel_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated NOTIFICATION header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    channel.clear();
    if (channel_length > 0) {
        channel.resize(channel_length);
        if (!m.readBytes(channel.data(), channel_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated NOTIFICATION channel");
            return core::Status::PROTOCOL_VIOLATION;
        }
    }

    if (!m.readUInt32(payload_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated NOTIFICATION payload length");
        return core::Status::PROTOCOL_VIOLATION;
    }

    payload.clear();
    if (payload_length > 0) {
        payload.resize(payload_length);
        if (!m.readBytes(payload.data(), payload_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated NOTIFICATION payload");
            return core::Status::PROTOCOL_VIOLATION;
        }
    }

    if (!m.readUInt8(change_type) || !m.readUInt64(row_id)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated NOTIFICATION tail");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildQueryProgress(uint64_t rows_processed,
                                          uint64_t bytes_processed) {
    Message msg(MessageType::QUERY_PROGRESS);
    msg.reservePayload(sizeof(QueryProgressPayload));
    msg.writeUInt64(rows_processed);
    msg.writeUInt64(bytes_processed);
    return msg;
}

core::Status ProtocolCodec::parseCommandComplete(const Message& msg,
                                                 std::string& command_tag,
                                                 int64_t& rows_affected,
                                                 core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readNullTerminatedString(command_tag, 64) ||
        !m.readInt64(rows_affected)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated COMMAND_COMPLETE");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

core::Status ProtocolCodec::parseQueryProgress(const Message& msg,
                                               uint64_t& rows_processed,
                                               uint64_t& bytes_processed,
                                               core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readUInt64(rows_processed) || !m.readUInt64(bytes_processed)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY_PROGRESS");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}
// COPY Messages

Message ProtocolCodec::buildCopyInResponse(CopyFormat format,
                                           const std::vector<uint16_t>& column_formats) {
    Message msg(MessageType::COPY_IN_RESPONSE);

    msg.writeUInt8(static_cast<uint8_t>(format));
    msg.writeUInt8(0);
    msg.writeUInt16(static_cast<uint16_t>(column_formats.size()));
    for (uint16_t fmt : column_formats) {
        msg.writeUInt16(fmt);
    }

    return msg;
}

Message ProtocolCodec::buildCopyOutResponse(CopyFormat format,
                                            const std::vector<uint16_t>& column_formats) {
    Message msg(MessageType::COPY_OUT_RESPONSE);

    msg.writeUInt8(static_cast<uint8_t>(format));
    msg.writeUInt8(0);
    msg.writeUInt16(static_cast<uint16_t>(column_formats.size()));
    for (uint16_t fmt : column_formats) {
        msg.writeUInt16(fmt);
    }

    return msg;
}

Message ProtocolCodec::buildCopyBothResponse(CopyFormat format,
                                             const std::vector<uint16_t>& column_formats) {
    Message msg(MessageType::COPY_BOTH_RESPONSE);

    msg.writeUInt8(static_cast<uint8_t>(format));
    msg.writeUInt8(0);
    msg.writeUInt16(static_cast<uint16_t>(column_formats.size()));
    for (uint16_t fmt : column_formats) {
        msg.writeUInt16(fmt);
    }

    return msg;
}

Message ProtocolCodec::buildCopyData(const uint8_t* data, size_t length) {
    Message msg(MessageType::COPY_DATA);
    if (data && length > 0) {
        msg.writeBytes(data, length);
    }
    return msg;
}

core::Status ProtocolCodec::parseCopyData(const Message& msg,
                                          const uint8_t** data,
                                          size_t* length,
                                          core::ErrorContext* ctx) {
    if (!data || !length) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Missing output buffers for COPY_DATA");
        return core::Status::INVALID_ARGUMENT;
    }
    *data = msg.getPayloadData();
    *length = msg.getPayloadSize();
    return core::Status::OK;
}

Message ProtocolCodec::buildCopyDone() {
    return Message(MessageType::COPY_DONE);
}

Message ProtocolCodec::buildCopyFail(const std::string& error_message) {
    Message msg(MessageType::COPY_FAIL);
    msg.writeUInt32(static_cast<uint32_t>(error_message.size()));
    if (!error_message.empty()) {
        msg.writeString(error_message);
    }
    return msg;
}

core::Status ProtocolCodec::parseCopyFail(const Message& msg,
                                          std::string& error_message,
                                          core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint32_t length = 0;
    if (!m.readUInt32(length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated COPY_FAIL length");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (length > m.getRemainingBytes()) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated COPY_FAIL message");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (!m.readString(error_message, length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated COPY_FAIL message");
        return core::Status::PROTOCOL_VIOLATION;
    }
    return core::Status::OK;
}

// Streaming Messages

Message ProtocolCodec::buildStreamControl(StreamControlType control_type,
                                          uint32_t window_size,
                                          uint32_t timeout_ms) {
    Message msg(MessageType::STREAM_CONTROL);
    msg.writeUInt8(static_cast<uint8_t>(control_type));
    msg.writeUInt8(0);
    msg.writeUInt8(0);
    msg.writeUInt8(0);
    msg.writeUInt32(window_size);
    msg.writeUInt32(timeout_ms);
    return msg;
}

core::Status ProtocolCodec::parseStreamControl(const Message& msg,
                                               StreamControlType& control_type,
                                               uint32_t& window_size,
                                               uint32_t& timeout_ms,
                                               core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint8_t type = 0;
    uint8_t reserved = 0;
    if (!m.readUInt8(type) ||
        !m.readUInt8(reserved) ||
        !m.readUInt8(reserved) ||
        !m.readUInt8(reserved) ||
        !m.readUInt32(window_size) ||
        !m.readUInt32(timeout_ms)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated STREAM_CONTROL");
        return core::Status::PROTOCOL_VIOLATION;
    }
    control_type = static_cast<StreamControlType>(type);
    return core::Status::OK;
}

Message ProtocolCodec::buildStreamReady(uint64_t stream_id,
                                        uint64_t total_rows,
                                        uint64_t estimated_bytes) {
    Message msg(MessageType::STREAM_READY);
    msg.writeUInt64(stream_id);
    msg.writeUInt64(total_rows);
    msg.writeUInt64(estimated_bytes);
    return msg;
}

core::Status ProtocolCodec::parseStreamReady(const Message& msg,
                                             uint64_t& stream_id,
                                             uint64_t& total_rows,
                                             uint64_t& estimated_bytes,
                                             core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readUInt64(stream_id) ||
        !m.readUInt64(total_rows) ||
        !m.readUInt64(estimated_bytes)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated STREAM_READY");
        return core::Status::PROTOCOL_VIOLATION;
    }
    return core::Status::OK;
}

Message ProtocolCodec::buildStreamData(uint64_t stream_id,
                                       uint32_t chunk_rows,
                                       const uint8_t* data,
                                       size_t length) {
    Message msg(MessageType::STREAM_DATA);
    msg.writeUInt64(stream_id);
    msg.writeUInt32(chunk_rows);
    msg.writeUInt32(static_cast<uint32_t>(length));
    if (data && length > 0) {
        msg.writeBytes(data, length);
    }
    return msg;
}

core::Status ProtocolCodec::parseStreamData(const Message& msg,
                                            uint64_t& stream_id,
                                            uint32_t& chunk_rows,
                                            const uint8_t** data,
                                            size_t* length,
                                            core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint32_t chunk_bytes = 0;
    if (!m.readUInt64(stream_id) ||
        !m.readUInt32(chunk_rows) ||
        !m.readUInt32(chunk_bytes)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated STREAM_DATA header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (!data || !length) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                          "Missing output buffers for STREAM_DATA");
        return core::Status::INVALID_ARGUMENT;
    }

    if (chunk_bytes > m.getRemainingBytes()) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated STREAM_DATA payload");
        return core::Status::PROTOCOL_VIOLATION;
    }

    *data = m.getPayloadData() + m.getReadOffset();
    *length = chunk_bytes;
    return core::Status::OK;
}

Message ProtocolCodec::buildStreamEnd(uint64_t stream_id,
                                      uint64_t total_rows,
                                      uint64_t total_bytes) {
    Message msg(MessageType::STREAM_END);
    msg.writeUInt64(stream_id);
    msg.writeUInt64(total_rows);
    msg.writeUInt64(total_bytes);
    return msg;
}

core::Status ProtocolCodec::parseStreamEnd(const Message& msg,
                                           uint64_t& stream_id,
                                           uint64_t& total_rows,
                                           uint64_t& total_bytes,
                                           core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readUInt64(stream_id) ||
        !m.readUInt64(total_rows) ||
        !m.readUInt64(total_bytes)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated STREAM_END");
        return core::Status::PROTOCOL_VIOLATION;
    }
    return core::Status::OK;
}

// Transaction Messages

Message ProtocolCodec::buildBeginTransaction(const uint8_t session_id[16],
                                             uint8_t isolation_level,
                                             bool read_only) {
    Message msg(MessageType::BEGIN_TRANSACTION);

    msg.writeBytes(session_id, 16);
    msg.writeUInt8(isolation_level);
    msg.writeUInt8(read_only ? 1 : 0);

    return msg;
}

Message ProtocolCodec::buildCommit(const uint8_t session_id[16]) {
    Message msg(MessageType::COMMIT);
    msg.writeBytes(session_id, 16);
    return msg;
}

Message ProtocolCodec::buildRollback(const uint8_t session_id[16]) {
    Message msg(MessageType::ROLLBACK);
    msg.writeBytes(session_id, 16);
    return msg;
}

Message ProtocolCodec::buildSavepoint(const uint8_t session_id[16],
                                      const std::string& name) {
    Message msg(MessageType::SAVEPOINT);
    msg.writeBytes(session_id, 16);
    msg.writeNullTerminatedString(name, 64);
    return msg;
}

Message ProtocolCodec::buildReleaseSavepoint(const uint8_t session_id[16],
                                             const std::string& name) {
    Message msg(MessageType::RELEASE_SAVEPOINT);
    msg.writeBytes(session_id, 16);
    msg.writeNullTerminatedString(name, 64);
    return msg;
}

Message ProtocolCodec::buildRollbackTo(const uint8_t session_id[16],
                                       const std::string& name) {
    Message msg(MessageType::ROLLBACK_TO);
    msg.writeBytes(session_id, 16);
    msg.writeNullTerminatedString(name, 64);
    return msg;
}

Message ProtocolCodec::buildTransactionStatus(uint8_t status, uint64_t xid) {
    Message msg(MessageType::TRANSACTION_STATUS);
    msg.writeUInt8(status);
    msg.writeUInt64(xid);
    return msg;
}

// Administrative Messages

Message ProtocolCodec::buildPing(uint64_t timestamp, uint32_t sequence) {
    Message msg(MessageType::PING);
    msg.writeUInt64(timestamp);
    msg.writeUInt32(sequence);
    return msg;
}

Message ProtocolCodec::buildPong(uint64_t timestamp, uint32_t sequence) {
    Message msg(MessageType::PONG);
    msg.writeUInt64(timestamp);
    msg.writeUInt32(sequence);
    return msg;
}

core::Status ProtocolCodec::parsePing(const Message& msg,
                                      uint64_t& timestamp,
                                      uint32_t& sequence,
                                      core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    if (!m.readUInt64(timestamp) || !m.readUInt32(sequence)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated PING");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildDisconnect() {
    return Message(MessageType::DISCONNECT);
}

Message ProtocolCodec::buildShutdown() {
    return Message(MessageType::SHUTDOWN);
}

Message ProtocolCodec::buildProtocolError(const std::string& message) {
    Message msg(MessageType::PROTOCOL_ERROR);
    msg.writeLengthPrefixedString(message);
    return msg;
}

// ============================================================================
// ProtocolSession Implementation
// ============================================================================

ProtocolSession::ProtocolSession(scratchbird::server::IPCConnection* connection)
    : connection_(connection) {
}

ProtocolSession::~ProtocolSession() = default;

core::Status ProtocolSession::sendMessage(const Message& msg, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connection_ || !connection_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                          "Connection is closed");
        return core::Status::CONNECTION_FAILURE;
    }

    std::vector<uint8_t> buffer;
    core::Status status = msg.serialize(buffer);
    if (status != core::Status::OK) {
        return status;
    }

    status = connection_->writeExact(buffer.data(), buffer.size(), ctx);
    if (status == core::Status::OK) {
        messages_sent_++;
        bytes_sent_ += buffer.size();
    }

    return status;
}

core::Status ProtocolSession::receiveMessage(Message& msg, core::ErrorContext* ctx) {
    if (!connection_ || !connection_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                          "Connection is closed");
        return core::Status::CONNECTION_FAILURE;
    }

    // Read header
    uint8_t header_buf[sizeof(MessageHeader)];
    core::Status status = connection_->readExact(header_buf, sizeof(header_buf), ctx);
    if (status != core::Status::OK) {
        return status;
    }

    MessageHeader header;
    status = Message::parseHeader(header_buf, header, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    // Create message with header
    msg = Message(static_cast<MessageType>(header.type));
    msg.setFlags(header.flags);

    // Read payload
    if (header.payload_length > 0) {
        std::vector<uint8_t> payload_buf(header.payload_length);
        status = connection_->readExact(payload_buf.data(), header.payload_length, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        msg.setPayload(payload_buf.data(), header.payload_length);
    }

    messages_received_++;
    bytes_received_ += sizeof(MessageHeader) + header.payload_length;

    return core::Status::OK;
}

bool ProtocolSession::isConnected() const {
    return connection_ && connection_->isOpen();
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* messageTypeToString(MessageType type) {
    switch (type) {
        case MessageType::CONNECT_REQUEST:    return "CONNECT_REQUEST";
        case MessageType::CONNECT_RESPONSE:   return "CONNECT_RESPONSE";
        case MessageType::DISCONNECT:         return "DISCONNECT";
        case MessageType::AUTH_REQUEST:       return "AUTH_REQUEST";
        case MessageType::AUTH_RESPONSE:      return "AUTH_RESPONSE";
        case MessageType::AUTH_CHALLENGE:     return "AUTH_CHALLENGE";
        case MessageType::SUBSCRIBE:          return "SUBSCRIBE";
        case MessageType::UNSUBSCRIBE:        return "UNSUBSCRIBE";
        case MessageType::QUERY:              return "QUERY";
        case MessageType::QUERY_RESULT:       return "QUERY_RESULT";
        case MessageType::QUERY_ERROR:        return "QUERY_ERROR";
        case MessageType::QUERY_CANCEL:       return "QUERY_CANCEL";
        case MessageType::PREPARE:            return "PREPARE";
        case MessageType::PREPARE_RESPONSE:   return "PREPARE_RESPONSE";
        case MessageType::EXECUTE:            return "EXECUTE";
        case MessageType::CLOSE_STATEMENT:    return "CLOSE_STATEMENT";
        case MessageType::DESCRIBE:           return "DESCRIBE";
        case MessageType::DESCRIBE_RESPONSE:  return "DESCRIBE_RESPONSE";
        case MessageType::BEGIN_TRANSACTION:  return "BEGIN_TRANSACTION";
        case MessageType::COMMIT:             return "COMMIT";
        case MessageType::ROLLBACK:           return "ROLLBACK";
        case MessageType::SAVEPOINT:          return "SAVEPOINT";
        case MessageType::RELEASE_SAVEPOINT:  return "RELEASE_SAVEPOINT";
        case MessageType::ROLLBACK_TO:        return "ROLLBACK_TO";
        case MessageType::TRANSACTION_STATUS: return "TRANSACTION_STATUS";
        case MessageType::ROW_DESCRIPTION:    return "ROW_DESCRIPTION";
        case MessageType::ROW_DATA:           return "ROW_DATA";
        case MessageType::END_OF_RESULTS:     return "END_OF_RESULTS";
        case MessageType::COMMAND_COMPLETE:   return "COMMAND_COMPLETE";
        case MessageType::PORTAL_SUSPENDED:   return "PORTAL_SUSPENDED";
        case MessageType::NOTIFICATION:       return "NOTIFICATION";
        case MessageType::QUERY_PROGRESS:     return "QUERY_PROGRESS";
        case MessageType::SHUTDOWN:           return "SHUTDOWN";
        case MessageType::PING:               return "PING";
        case MessageType::PONG:               return "PONG";
        case MessageType::STATUS_REQUEST:     return "STATUS_REQUEST";
        case MessageType::STATUS_RESPONSE:    return "STATUS_RESPONSE";
        case MessageType::COPY_DATA:          return "COPY_DATA";
        case MessageType::COPY_DONE:          return "COPY_DONE";
        case MessageType::COPY_FAIL:          return "COPY_FAIL";
        case MessageType::COPY_IN_RESPONSE:   return "COPY_IN_RESPONSE";
        case MessageType::COPY_OUT_RESPONSE:  return "COPY_OUT_RESPONSE";
        case MessageType::COPY_BOTH_RESPONSE: return "COPY_BOTH_RESPONSE";
        case MessageType::STREAM_CONTROL:     return "STREAM_CONTROL";
        case MessageType::STREAM_READY:       return "STREAM_READY";
        case MessageType::STREAM_DATA:        return "STREAM_DATA";
        case MessageType::STREAM_END:         return "STREAM_END";
        case MessageType::DEBUG_MESSAGE:      return "DEBUG_MESSAGE";
        case MessageType::PROTOCOL_ERROR:     return "PROTOCOL_ERROR";
        default:                              return "UNKNOWN";
    }
}

const char* wireTypeToString(WireType type) {
    switch (type) {
        case WireType::NULL_TYPE:   return "NULL";
        case WireType::BOOLEAN:     return "BOOLEAN";
        case WireType::INT16:       return "INT16";
        case WireType::INT32:       return "INT32";
        case WireType::INT64:       return "INT64";
        case WireType::FLOAT32:     return "FLOAT32";
        case WireType::FLOAT64:     return "FLOAT64";
        case WireType::DECIMAL:     return "DECIMAL";
        case WireType::VARCHAR:     return "VARCHAR";
        case WireType::CHAR:        return "CHAR";
        case WireType::BYTEA:       return "BYTEA";
        case WireType::DATE:        return "DATE";
        case WireType::TIME:        return "TIME";
        case WireType::TIMESTAMP:   return "TIMESTAMP";
        case WireType::TIMESTAMPTZ: return "TIMESTAMPTZ";
        case WireType::INTERVAL:    return "INTERVAL";
        case WireType::UUID:        return "UUID";
        case WireType::JSON:        return "JSON";
        case WireType::JSONB:       return "JSONB";
        case WireType::ARRAY:       return "ARRAY";
        case WireType::COMPOSITE:   return "COMPOSITE";
        case WireType::GEOMETRY:    return "GEOMETRY";
        case WireType::VECTOR:      return "VECTOR";
        case WireType::MONEY:       return "MONEY";
        case WireType::XML:         return "XML";
        case WireType::INET:        return "INET";
        case WireType::CIDR:        return "CIDR";
        case WireType::MACADDR:     return "MACADDR";
        case WireType::TSVECTOR:    return "TSVECTOR";
        case WireType::TSQUERY:     return "TSQUERY";
        case WireType::RANGE:       return "RANGE";
        case WireType::UNKNOWN:     return "UNKNOWN";
        default:                    return "UNKNOWN";
    }
}

void generateSessionId(uint8_t session_id[16]) {
    // Use random_device + mt19937 for UUID v4
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);

    // Generate 16 random bytes
    for (int i = 0; i < 16; i++) {
        session_id[i] = dis(gen);
    }

    // Set version (4) at byte 6: clear high nibble, set to 0100xxxx
    session_id[6] = (session_id[6] & 0x0F) | 0x40;

    // Set variant (RFC 4122) at byte 8: set to 10xxxxxx
    session_id[8] = (session_id[8] & 0x3F) | 0x80;
}

std::string sessionIdToString(const uint8_t session_id[16]) {
    static const char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);

    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            result += '-';
        }
        result += hex[(session_id[i] >> 4) & 0xF];
        result += hex[session_id[i] & 0xF];
    }

    return result;
}

} // namespace protocol
} // namespace scratchbird
