/**
 * ScratchBird Wire Protocol Implementation
 *
 * Local Server Architecture - Phase 2
 */

#include "scratchbird/protocol/wire_protocol.h"
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
                                           uint32_t client_pid) {
    Message msg(MessageType::CONNECT_REQUEST);
    msg.reservePayload(sizeof(ConnectRequestPayload));

    msg.writeUInt16(PROTOCOL_VERSION);
    msg.writeUInt16(0);  // client_flags (reserved)
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
                                                core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint16_t version, flags;
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

    return core::Status::OK;
}

Message ProtocolCodec::buildConnectResponse(bool success,
                                            const uint8_t session_id[16],
                                            const std::string& error_message) {
    Message msg(MessageType::CONNECT_RESPONSE);
    msg.reservePayload(sizeof(ConnectResponsePayload));

    msg.writeUInt8(success ? 0 : 1);
    msg.writeUInt16(PROTOCOL_VERSION);
    msg.writeUInt16(0);  // server_flags
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
    Message msg(MessageType::AUTH_REQUEST);

    msg.writeBytes(session_id, 16);
    msg.writeNullTerminatedString(username, 64);
    msg.writeUInt8(0);  // auth_method = password
    msg.writeUInt16(static_cast<uint16_t>(password.size()));
    msg.writeString(password);

    return msg;
}

core::Status ProtocolCodec::parseAuthRequest(const Message& msg,
                                             uint8_t session_id[16],
                                             std::string& username,
                                             std::string& password,
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

    uint8_t auth_method;
    uint16_t cred_length;
    if (!m.readUInt8(auth_method) || !m.readUInt16(cred_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid AUTH_REQUEST credentials header");
        return core::Status::PROTOCOL_VIOLATION;
    }

    if (!m.readString(password, cred_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Invalid AUTH_REQUEST password");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

Message ProtocolCodec::buildAuthResponse(bool success,
                                         uint32_t user_id,
                                         const std::string& error_message) {
    Message msg(MessageType::AUTH_RESPONSE);

    msg.writeUInt8(success ? 0 : 1);
    msg.writeUInt32(user_id);
    msg.writeNullTerminatedString(error_message, 256);

    return msg;
}

core::Status ProtocolCodec::parseAuthResponse(const Message& msg,
                                              bool& success,
                                              uint32_t& user_id,
                                              std::string& error_message,
                                              core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint8_t status;
    if (!m.readUInt8(status) || !m.readUInt32(user_id)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated AUTH_RESPONSE");
        return core::Status::PROTOCOL_VIOLATION;
    }

    success = (status == 0);

    m.readNullTerminatedString(error_message, 256);

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

core::Status ProtocolCodec::parseQuery(const Message& msg,
                                       uint8_t session_id[16],
                                       std::string& query,
                                       uint8_t& flags,
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

    if (!m.readString(query, query_length)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated QUERY text");
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
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

    msg.writeUInt16(static_cast<uint16_t>(columns.size()));

    for (const auto& col : columns) {
        msg.writeUInt16(static_cast<uint16_t>(col.name.size()));
        msg.writeString(col.name);
        msg.writeUInt8(static_cast<uint8_t>(col.type));
        msg.writeUInt32(col.type_modifier);
        msg.writeUInt16(1);  // binary format
    }

    return msg;
}

core::Status ProtocolCodec::parseRowDescription(const Message& msg,
                                                std::vector<ColumnInfo>& columns,
                                                core::ErrorContext* ctx) {
    Message& m = const_cast<Message&>(msg);
    m.resetReadOffset();

    uint16_t column_count;
    if (!m.readUInt16(column_count)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated ROW_DESCRIPTION");
        return core::Status::PROTOCOL_VIOLATION;
    }

    columns.clear();
    columns.reserve(column_count);

    for (uint16_t i = 0; i < column_count; i++) {
        ColumnInfo col;

        uint16_t name_length;
        if (!m.readUInt16(name_length) || !m.readString(col.name, name_length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DESCRIPTION column name");
            return core::Status::PROTOCOL_VIOLATION;
        }

        uint8_t type_byte;
        uint16_t format;
        if (!m.readUInt8(type_byte) || !m.readUInt32(col.type_modifier) ||
            !m.readUInt16(format)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DESCRIPTION column metadata");
            return core::Status::PROTOCOL_VIOLATION;
        }

        col.type = static_cast<WireType>(type_byte);
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
    cv.data.assign(data, data + length);
    return cv;
}

Message ProtocolCodec::buildRowData(const std::vector<ColumnValue>& values) {
    Message msg(MessageType::ROW_DATA);

    msg.writeUInt16(static_cast<uint16_t>(values.size()));

    for (const auto& val : values) {
        if (val.is_null) {
            msg.writeInt32(-1);  // NULL indicator
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
    if (!m.readUInt16(column_count)) {
        SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                          "Truncated ROW_DATA");
        return core::Status::PROTOCOL_VIOLATION;
    }

    values.clear();
    values.reserve(column_count);

    for (uint16_t i = 0; i < column_count; i++) {
        int32_t length;
        if (!m.readInt32(length)) {
            SET_ERROR_CONTEXT(ctx, core::Status::PROTOCOL_VIOLATION,
                              "Truncated ROW_DATA column length");
            return core::Status::PROTOCOL_VIOLATION;
        }

        ColumnValue val;
        if (length < 0) {
            val.is_null = true;
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
        case MessageType::SHUTDOWN:           return "SHUTDOWN";
        case MessageType::PING:               return "PING";
        case MessageType::PONG:               return "PONG";
        case MessageType::STATUS_REQUEST:     return "STATUS_REQUEST";
        case MessageType::STATUS_RESPONSE:    return "STATUS_RESPONSE";
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
