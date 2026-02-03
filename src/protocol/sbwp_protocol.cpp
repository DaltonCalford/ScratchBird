#include "scratchbird/protocol/sbwp_protocol.h"

#include <cstring>

namespace scratchbird::protocol::sbwp {
namespace {
const uint8_t kMagicBytes[4] = {'S', 'B', 'W', 'P'};

void setError(core::ErrorContext* ctx, const char* msg) {
    if (ctx) {
        ctx->message = msg ? msg : "";
    }
}

void writeU16(std::vector<uint8_t>& buf, size_t offset, uint16_t value) {
    buf[offset] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeU32(std::vector<uint8_t>& buf, size_t offset, uint32_t value) {
    buf[offset] = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

void writeU64(std::vector<uint8_t>& buf, size_t offset, uint64_t value) {
    for (size_t i = 0; i < 8; ++i) {
        buf[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
        (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readU32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
        (static_cast<uint32_t>(data[1]) << 8) |
        (static_cast<uint32_t>(data[2]) << 16) |
        (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t readU64(const uint8_t* data) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(data[i]) << (8 * i);
    }
    return value;
}

std::vector<uint8_t> buildParamList(const std::map<std::string, std::string>& params) {
    std::vector<uint8_t> buf;
    buf.reserve(128);
    for (const auto& entry : params) {
        buf.insert(buf.end(), entry.first.begin(), entry.first.end());
        buf.push_back(0);
        buf.insert(buf.end(), entry.second.begin(), entry.second.end());
        buf.push_back(0);
    }
    buf.push_back(0);
    return buf;
}

} // namespace

std::vector<uint8_t> encodeMessage(const MessageHeader& header,
                                   const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> buf;
    buf.resize(kHeaderSize + payload.size());
    std::memcpy(buf.data(), kMagicBytes, sizeof(kMagicBytes));
    buf[4] = kProtocolMajor;
    buf[5] = kProtocolMinor;
    buf[6] = static_cast<uint8_t>(header.type);
    buf[7] = header.flags;
    writeU32(buf, 8, static_cast<uint32_t>(payload.size()));
    writeU32(buf, 12, header.sequence);
    std::memcpy(buf.data() + 16, header.attachment_id.data(), header.attachment_id.size());
    writeU64(buf, 32, header.txn_id);
    if (!payload.empty()) {
        std::memcpy(buf.data() + kHeaderSize, payload.data(), payload.size());
    }
    return buf;
}

core::Status decodeHeader(const std::vector<uint8_t>& header_bytes,
                          MessageHeader& header,
                          core::ErrorContext* ctx) {
    if (header_bytes.size() != kHeaderSize) {
        setError(ctx, "Invalid header length");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (std::memcmp(header_bytes.data(), kMagicBytes, sizeof(kMagicBytes)) != 0) {
        setError(ctx, "Invalid protocol magic");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (header_bytes[4] != kProtocolMajor || header_bytes[5] != kProtocolMinor) {
        setError(ctx, "Unsupported protocol version");
        return core::Status::PROTOCOL_VIOLATION;
    }
    uint32_t length = readU32(header_bytes.data() + 8);
    if (length > kMaxMessageSize) {
        setError(ctx, "Payload too large");
        return core::Status::PROTOCOL_VIOLATION;
    }
    header.type = static_cast<MessageType>(header_bytes[6]);
    header.flags = header_bytes[7];
    header.length = length;
    header.sequence = readU32(header_bytes.data() + 12);
    std::memcpy(header.attachment_id.data(), header_bytes.data() + 16, header.attachment_id.size());
    header.txn_id = readU64(header_bytes.data() + 32);
    return core::Status::OK;
}

std::vector<uint8_t> buildStartupPayload(uint64_t features,
                                         const std::map<std::string, std::string>& params) {
    auto param_bytes = buildParamList(params);
    std::vector<uint8_t> payload(2 + 2 + 8 + param_bytes.size());
    payload[0] = kProtocolMajor;
    payload[1] = kProtocolMinor;
    writeU16(payload, 2, 0);
    writeU64(payload, 4, features);
    if (!param_bytes.empty()) {
        std::memcpy(payload.data() + 12, param_bytes.data(), param_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildQueryPayload(const std::string& query,
                                       uint32_t flags,
                                       uint32_t max_rows,
                                       uint32_t timeout_ms) {
    std::vector<uint8_t> query_bytes(query.begin(), query.end());
    query_bytes.push_back(0);
    std::vector<uint8_t> payload(4 + 4 + 4 + query_bytes.size());
    writeU32(payload, 0, flags);
    writeU32(payload, 4, max_rows);
    writeU32(payload, 8, timeout_ms);
    std::memcpy(payload.data() + 12, query_bytes.data(), query_bytes.size());
    return payload;
}

std::vector<uint8_t> buildParsePayload(const std::string& statement_name,
                                       const std::string& query,
                                       const std::vector<uint32_t>& param_types) {
    std::vector<uint8_t> name_bytes(statement_name.begin(), statement_name.end());
    std::vector<uint8_t> query_bytes(query.begin(), query.end());
    size_t payload_len = 4 + name_bytes.size() + 4 + query_bytes.size() + 2 + 2 + param_types.size() * 4;
    std::vector<uint8_t> payload(payload_len);
    size_t offset = 0;
    writeU32(payload, offset, static_cast<uint32_t>(name_bytes.size()));
    offset += 4;
    if (!name_bytes.empty()) {
        std::memcpy(payload.data() + offset, name_bytes.data(), name_bytes.size());
        offset += name_bytes.size();
    }
    writeU32(payload, offset, static_cast<uint32_t>(query_bytes.size()));
    offset += 4;
    if (!query_bytes.empty()) {
        std::memcpy(payload.data() + offset, query_bytes.data(), query_bytes.size());
        offset += query_bytes.size();
    }
    writeU16(payload, offset, static_cast<uint16_t>(param_types.size()));
    offset += 2;
    writeU16(payload, offset, 0);
    offset += 2;
    for (uint32_t oid : param_types) {
        writeU32(payload, offset, oid);
        offset += 4;
    }
    return payload;
}

std::vector<uint8_t> buildBindPayload(const std::string& portal_name,
                                      const std::string& statement_name,
                                      const std::vector<ParamValue>& params,
                                      const std::vector<uint16_t>& result_formats) {
    std::vector<uint8_t> portal_bytes(portal_name.begin(), portal_name.end());
    std::vector<uint8_t> stmt_bytes(statement_name.begin(), statement_name.end());
    std::vector<uint16_t> param_formats;
    param_formats.reserve(params.size());
    for (const auto& param : params) {
        param_formats.push_back(param.format);
    }
    size_t payload_len = 4 + portal_bytes.size() + 4 + stmt_bytes.size();
    payload_len += 2 + param_formats.size() * 2;
    payload_len += 2 + 2;
    for (const auto& param : params) {
        payload_len += 4;
        if (!param.is_null) {
            payload_len += param.data.size();
        }
    }
    payload_len += 2 + result_formats.size() * 2;

    std::vector<uint8_t> payload(payload_len);
    size_t offset = 0;
    writeU32(payload, offset, static_cast<uint32_t>(portal_bytes.size()));
    offset += 4;
    if (!portal_bytes.empty()) {
        std::memcpy(payload.data() + offset, portal_bytes.data(), portal_bytes.size());
        offset += portal_bytes.size();
    }
    writeU32(payload, offset, static_cast<uint32_t>(stmt_bytes.size()));
    offset += 4;
    if (!stmt_bytes.empty()) {
        std::memcpy(payload.data() + offset, stmt_bytes.data(), stmt_bytes.size());
        offset += stmt_bytes.size();
    }
    writeU16(payload, offset, static_cast<uint16_t>(param_formats.size()));
    offset += 2;
    for (uint16_t fmt : param_formats) {
        writeU16(payload, offset, fmt);
        offset += 2;
    }
    writeU16(payload, offset, static_cast<uint16_t>(params.size()));
    offset += 2;
    writeU16(payload, offset, 0);
    offset += 2;
    for (const auto& param : params) {
        if (param.is_null) {
            writeU32(payload, offset, 0xFFFFFFFFu);
            offset += 4;
            continue;
        }
        writeU32(payload, offset, static_cast<uint32_t>(param.data.size()));
        offset += 4;
        if (!param.data.empty()) {
            std::memcpy(payload.data() + offset, param.data.data(), param.data.size());
            offset += param.data.size();
        }
    }
    writeU16(payload, offset, static_cast<uint16_t>(result_formats.size()));
    offset += 2;
    for (uint16_t fmt : result_formats) {
        writeU16(payload, offset, fmt);
        offset += 2;
    }
    return payload;
}

std::vector<uint8_t> buildDescribePayload(uint8_t describe_type, const std::string& name) {
    std::vector<uint8_t> name_bytes(name.begin(), name.end());
    std::vector<uint8_t> payload(4 + 4 + name_bytes.size());
    payload[0] = describe_type;
    writeU32(payload, 4, static_cast<uint32_t>(name_bytes.size()));
    if (!name_bytes.empty()) {
        std::memcpy(payload.data() + 8, name_bytes.data(), name_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildExecutePayload(const std::string& portal_name, uint32_t max_rows) {
    std::vector<uint8_t> portal_bytes(portal_name.begin(), portal_name.end());
    std::vector<uint8_t> payload(4 + portal_bytes.size() + 4);
    writeU32(payload, 0, static_cast<uint32_t>(portal_bytes.size()));
    if (!portal_bytes.empty()) {
        std::memcpy(payload.data() + 4, portal_bytes.data(), portal_bytes.size());
    }
    writeU32(payload, 4 + portal_bytes.size(), max_rows);
    return payload;
}

std::vector<uint8_t> buildSblrExecutePayload(uint64_t sblr_hash,
                                             const std::vector<uint8_t>& bytecode,
                                             const std::vector<ParamValue>& params) {
    size_t payload_len = 8 + 4 + 2 + 2 + bytecode.size();
    for (const auto& param : params) {
        payload_len += 4;
        if (!param.is_null) {
            payload_len += param.data.size();
        }
    }
    std::vector<uint8_t> payload(payload_len);
    size_t offset = 0;
    writeU64(payload, offset, sblr_hash);
    offset += 8;
    writeU32(payload, offset, static_cast<uint32_t>(bytecode.size()));
    offset += 4;
    writeU16(payload, offset, static_cast<uint16_t>(params.size()));
    offset += 2;
    writeU16(payload, offset, 0);
    offset += 2;
    if (!bytecode.empty()) {
        std::memcpy(payload.data() + offset, bytecode.data(), bytecode.size());
        offset += bytecode.size();
    }
    for (const auto& param : params) {
        if (param.is_null) {
            writeU32(payload, offset, 0xFFFFFFFFu);
            offset += 4;
            continue;
        }
        writeU32(payload, offset, static_cast<uint32_t>(param.data.size()));
        offset += 4;
        if (!param.data.empty()) {
            std::memcpy(payload.data() + offset, param.data.data(), param.data.size());
            offset += param.data.size();
        }
    }
    return payload;
}

std::vector<uint8_t> buildSubscribePayload(uint8_t subscribe_type,
                                           const std::string& channel,
                                           const std::string& filter) {
    std::vector<uint8_t> channel_bytes(channel.begin(), channel.end());
    std::vector<uint8_t> filter_bytes(filter.begin(), filter.end());
    size_t payload_len = 4 + 4 + channel_bytes.size() + 4 + filter_bytes.size();
    std::vector<uint8_t> payload(payload_len);
    payload[0] = subscribe_type;
    size_t offset = 4;
    writeU32(payload, offset, static_cast<uint32_t>(channel_bytes.size()));
    offset += 4;
    if (!channel_bytes.empty()) {
        std::memcpy(payload.data() + offset, channel_bytes.data(), channel_bytes.size());
        offset += channel_bytes.size();
    }
    writeU32(payload, offset, static_cast<uint32_t>(filter_bytes.size()));
    offset += 4;
    if (!filter_bytes.empty()) {
        std::memcpy(payload.data() + offset, filter_bytes.data(), filter_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildUnsubscribePayload(const std::string& channel) {
    std::vector<uint8_t> channel_bytes(channel.begin(), channel.end());
    std::vector<uint8_t> payload(4 + channel_bytes.size());
    writeU32(payload, 0, static_cast<uint32_t>(channel_bytes.size()));
    if (!channel_bytes.empty()) {
        std::memcpy(payload.data() + 4, channel_bytes.data(), channel_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildTxnBeginPayload(uint16_t flags,
                                          uint8_t conflict_action,
                                          uint8_t autocommit_mode,
                                          uint8_t isolation_level,
                                          uint8_t access_mode,
                                          uint8_t deferrable,
                                          uint8_t wait_mode,
                                          uint32_t timeout_ms) {
    std::vector<uint8_t> payload(2 + 1 + 1 + 1 + 1 + 1 + 1 + 4);
    writeU16(payload, 0, flags);
    payload[2] = conflict_action;
    payload[3] = autocommit_mode;
    payload[4] = isolation_level;
    payload[5] = access_mode;
    payload[6] = deferrable;
    payload[7] = wait_mode;
    writeU32(payload, 8, timeout_ms);
    return payload;
}

std::vector<uint8_t> buildTxnCommitPayload(uint8_t flags) {
    std::vector<uint8_t> payload(4);
    payload[0] = flags;
    return payload;
}

std::vector<uint8_t> buildTxnRollbackPayload(uint8_t flags) {
    std::vector<uint8_t> payload(4);
    payload[0] = flags;
    return payload;
}

std::vector<uint8_t> buildTxnSavepointPayload(const std::string& name) {
    std::vector<uint8_t> name_bytes(name.begin(), name.end());
    std::vector<uint8_t> payload(4 + name_bytes.size());
    writeU32(payload, 0, static_cast<uint32_t>(name_bytes.size()));
    if (!name_bytes.empty()) {
        std::memcpy(payload.data() + 4, name_bytes.data(), name_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildTxnReleasePayload(const std::string& name) {
    return buildTxnSavepointPayload(name);
}

std::vector<uint8_t> buildTxnRollbackToPayload(const std::string& name) {
    return buildTxnSavepointPayload(name);
}

std::vector<uint8_t> buildSetOptionPayload(const std::string& name, const std::string& value) {
    std::vector<uint8_t> name_bytes(name.begin(), name.end());
    std::vector<uint8_t> value_bytes(value.begin(), value.end());
    std::vector<uint8_t> payload(4 + name_bytes.size() + 4 + value_bytes.size());
    writeU32(payload, 0, static_cast<uint32_t>(name_bytes.size()));
    if (!name_bytes.empty()) {
        std::memcpy(payload.data() + 4, name_bytes.data(), name_bytes.size());
    }
    size_t offset = 4 + name_bytes.size();
    writeU32(payload, offset, static_cast<uint32_t>(value_bytes.size()));
    offset += 4;
    if (!value_bytes.empty()) {
        std::memcpy(payload.data() + offset, value_bytes.data(), value_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildStreamControlPayload(uint8_t control_type,
                                               uint32_t window_size,
                                               uint32_t timeout_ms) {
    std::vector<uint8_t> payload(12);
    payload[0] = control_type;
    writeU32(payload, 4, window_size);
    writeU32(payload, 8, timeout_ms);
    return payload;
}

std::vector<uint8_t> buildAttachCreatePayload(const std::string& mode,
                                              const std::string& db_name) {
    std::vector<uint8_t> mode_bytes(mode.begin(), mode.end());
    std::vector<uint8_t> db_bytes(db_name.begin(), db_name.end());
    std::vector<uint8_t> payload(4 + mode_bytes.size() + 4 + db_bytes.size());
    writeU32(payload, 0, static_cast<uint32_t>(mode_bytes.size()));
    if (!mode_bytes.empty()) {
        std::memcpy(payload.data() + 4, mode_bytes.data(), mode_bytes.size());
    }
    size_t offset = 4 + mode_bytes.size();
    writeU32(payload, offset, static_cast<uint32_t>(db_bytes.size()));
    offset += 4;
    if (!db_bytes.empty()) {
        std::memcpy(payload.data() + offset, db_bytes.data(), db_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildAttachDetachPayload() {
    return {};
}

std::vector<uint8_t> buildAttachListPayload() {
    return {};
}

std::vector<uint8_t> buildClosePayload(uint8_t close_type, const std::string& name) {
    std::vector<uint8_t> name_bytes(name.begin(), name.end());
    std::vector<uint8_t> payload(4 + 4 + name_bytes.size());
    payload[0] = close_type;
    writeU32(payload, 4, static_cast<uint32_t>(name_bytes.size()));
    if (!name_bytes.empty()) {
        std::memcpy(payload.data() + 8, name_bytes.data(), name_bytes.size());
    }
    return payload;
}

std::vector<uint8_t> buildCancelPayload(uint32_t cancel_type, uint32_t target_seq) {
    std::vector<uint8_t> payload(8);
    writeU32(payload, 0, cancel_type);
    writeU32(payload, 4, target_seq);
    return payload;
}

core::Status parseAuthRequest(const std::vector<uint8_t>& payload,
                              AuthMethod& method,
                              std::vector<uint8_t>& data,
                              core::ErrorContext* ctx) {
    if (payload.size() < 4) {
        setError(ctx, "Auth request truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    method = static_cast<AuthMethod>(payload[0]);
    data.assign(payload.begin() + 4, payload.end());
    return core::Status::OK;
}

core::Status parseAuthContinue(const std::vector<uint8_t>& payload,
                               AuthMethod& method,
                               uint8_t& stage,
                               std::vector<uint8_t>& data,
                               core::ErrorContext* ctx) {
    if (payload.size() < 8) {
        setError(ctx, "Auth continue truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    method = static_cast<AuthMethod>(payload[0]);
    stage = payload[1];
    uint32_t data_len = readU32(payload.data() + 4);
    if (8u + data_len > payload.size()) {
        setError(ctx, "Auth continue truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    data.assign(payload.begin() + 8, payload.begin() + 8 + data_len);
    return core::Status::OK;
}

core::Status parseAuthOk(const std::vector<uint8_t>& payload,
                         std::vector<uint8_t>& session_id,
                         std::vector<uint8_t>& info,
                         core::ErrorContext* ctx) {
    if (payload.size() < 20) {
        setError(ctx, "Auth ok truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    session_id.assign(payload.begin(), payload.begin() + 16);
    uint32_t info_len = readU32(payload.data() + 16);
    if (20u + info_len > payload.size()) {
        setError(ctx, "Auth ok truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    info.assign(payload.begin() + 20, payload.begin() + 20 + info_len);
    return core::Status::OK;
}

core::Status parseReady(const std::vector<uint8_t>& payload,
                        uint8_t& status,
                        uint64_t& txn_id,
                        uint64_t& epoch,
                        core::ErrorContext* ctx) {
    if (payload.size() < 20) {
        setError(ctx, "Ready truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    status = payload[0];
    txn_id = readU64(payload.data() + 4);
    epoch = readU64(payload.data() + 12);
    return core::Status::OK;
}

core::Status parseParameterStatus(const std::vector<uint8_t>& payload,
                                  std::string& name,
                                  std::string& value,
                                  core::ErrorContext* ctx) {
    if (payload.size() < 8) {
        setError(ctx, "Parameter status truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    size_t offset = 0;
    uint32_t name_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + name_len + 4 > payload.size()) {
        setError(ctx, "Parameter status truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    name.assign(reinterpret_cast<const char*>(payload.data() + offset), name_len);
    offset += name_len;
    uint32_t value_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + value_len > payload.size()) {
        setError(ctx, "Parameter status truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    value.assign(reinterpret_cast<const char*>(payload.data() + offset), value_len);
    return core::Status::OK;
}

core::Status parseParameterDescription(const std::vector<uint8_t>& payload,
                                       std::vector<uint32_t>& param_types,
                                       core::ErrorContext* ctx) {
    if (payload.size() < 4) {
        setError(ctx, "Parameter description truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    uint16_t count = readU16(payload.data());
    size_t offset = 4;
    param_types.clear();
    param_types.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        if (offset + 4 > payload.size()) {
            setError(ctx, "Parameter description truncated");
            return core::Status::PROTOCOL_VIOLATION;
        }
        param_types.push_back(readU32(payload.data() + offset));
        offset += 4;
    }
    return core::Status::OK;
}

core::Status parseRowDescription(const std::vector<uint8_t>& payload,
                                 std::vector<ColumnInfo>& columns,
                                 core::ErrorContext* ctx) {
    if (payload.size() < 4) {
        setError(ctx, "Row description truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    uint16_t count = readU16(payload.data());
    size_t offset = 4;
    columns.clear();
    columns.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        if (offset + 4 > payload.size()) {
            setError(ctx, "Row description truncated");
            return core::Status::PROTOCOL_VIOLATION;
        }
        uint32_t name_len = readU32(payload.data() + offset);
        offset += 4;
        if (offset + name_len + 4 + 2 + 4 + 2 + 4 + 1 + 1 + 2 > payload.size()) {
            setError(ctx, "Row description truncated");
            return core::Status::PROTOCOL_VIOLATION;
        }
        ColumnInfo info;
        info.name.assign(reinterpret_cast<const char*>(payload.data() + offset), name_len);
        offset += name_len;
        info.table_oid = readU32(payload.data() + offset);
        offset += 4;
        info.column_index = readU16(payload.data() + offset);
        offset += 2;
        info.type_oid = readU32(payload.data() + offset);
        offset += 4;
        info.type_size = static_cast<int16_t>(readU16(payload.data() + offset));
        offset += 2;
        info.type_modifier = static_cast<int32_t>(readU32(payload.data() + offset));
        offset += 4;
        info.format = payload[offset++];
        info.nullable = payload[offset++] == 1;
        offset += 2;
        columns.push_back(std::move(info));
    }
    return core::Status::OK;
}

core::Status parseDataRow(const std::vector<uint8_t>& payload,
                          size_t column_count,
                          std::vector<ColumnValue>& values,
                          core::ErrorContext* ctx) {
    if (payload.size() < 4) {
        setError(ctx, "Row data truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    size_t offset = 0;
    uint16_t count = readU16(payload.data());
    offset += 2;
    uint16_t null_bytes = readU16(payload.data() + offset);
    offset += 2;
    if (count != column_count) {
        setError(ctx, "Row data column count mismatch");
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (offset + null_bytes > payload.size()) {
        setError(ctx, "Row data truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    const uint8_t* null_bitmap = payload.data() + offset;
    offset += null_bytes;
    values.clear();
    values.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        size_t byte_index = i / 8;
        uint8_t bit_index = static_cast<uint8_t>(i % 8);
        bool is_null = byte_index < null_bytes && (null_bitmap[byte_index] & (1u << bit_index)) != 0;
        if (is_null) {
            values.push_back(ColumnValue{{}, true});
            continue;
        }
        if (offset + 4 > payload.size()) {
            setError(ctx, "Row data truncated");
            return core::Status::PROTOCOL_VIOLATION;
        }
        uint32_t length = readU32(payload.data() + offset);
        offset += 4;
        if (length == 0xFFFFFFFFu) {
            values.push_back(ColumnValue{{}, true});
            continue;
        }
        if (offset + length > payload.size()) {
            setError(ctx, "Row data truncated");
            return core::Status::PROTOCOL_VIOLATION;
        }
        ColumnValue value;
        value.is_null = false;
        value.data.assign(payload.begin() + offset, payload.begin() + offset + length);
        offset += length;
        values.push_back(std::move(value));
    }
    return core::Status::OK;
}

core::Status parseCommandComplete(const std::vector<uint8_t>& payload,
                                  uint8_t& command_type,
                                  uint64_t& rows,
                                  uint64_t& last_id,
                                  std::string& tag,
                                  core::ErrorContext* ctx) {
    if (payload.size() < 20) {
        setError(ctx, "Command complete truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    command_type = payload[0];
    rows = readU64(payload.data() + 4);
    last_id = readU64(payload.data() + 12);
    tag.assign(reinterpret_cast<const char*>(payload.data() + 20), payload.size() - 20);
    for (size_t i = 0; i < tag.size(); ++i) {
        if (tag[i] == '\0') {
            tag.resize(i);
            break;
        }
    }
    return core::Status::OK;
}

core::Status parseNotification(const std::vector<uint8_t>& payload,
                               Notification& notice,
                               core::ErrorContext* ctx) {
    if (payload.size() < 12) {
        setError(ctx, "Notification truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    size_t offset = 0;
    notice.process_id = readU32(payload.data());
    offset += 4;
    uint32_t channel_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + channel_len + 4 > payload.size()) {
        setError(ctx, "Notification truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    notice.channel.assign(reinterpret_cast<const char*>(payload.data() + offset), channel_len);
    offset += channel_len;
    uint32_t payload_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + payload_len > payload.size()) {
        setError(ctx, "Notification truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    notice.payload.assign(payload.begin() + offset, payload.begin() + offset + payload_len);
    offset += payload_len;
    notice.change_type = 0;
    notice.row_id = 0;
    notice.has_row = false;
    if (offset < payload.size()) {
        notice.change_type = payload[offset++];
        if (offset + 8 <= payload.size()) {
            notice.row_id = readU64(payload.data() + offset);
            notice.has_row = true;
        }
    }
    return core::Status::OK;
}

core::Status parseQueryPlan(const std::vector<uint8_t>& payload,
                            QueryPlan& plan,
                            core::ErrorContext* ctx) {
    if (payload.size() < 32) {
        setError(ctx, "Query plan truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    plan.format = readU32(payload.data());
    uint32_t plan_len = readU32(payload.data() + 4);
    plan.planning_time = readU64(payload.data() + 8);
    plan.estimated_rows = readU64(payload.data() + 16);
    plan.estimated_cost = readU64(payload.data() + 24);
    if (32u + plan_len > payload.size()) {
        setError(ctx, "Query plan truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    plan.plan.assign(payload.begin() + 32, payload.begin() + 32 + plan_len);
    return core::Status::OK;
}

core::Status parseSblrCompiled(const std::vector<uint8_t>& payload,
                               SblrCompiled& compiled,
                               core::ErrorContext* ctx) {
    if (payload.size() < 16) {
        setError(ctx, "SBLR compiled truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    compiled.hash = readU64(payload.data());
    compiled.version = readU32(payload.data() + 8);
    uint32_t length = readU32(payload.data() + 12);
    if (16u + length > payload.size()) {
        setError(ctx, "SBLR compiled truncated");
        return core::Status::PROTOCOL_VIOLATION;
    }
    compiled.bytecode.assign(payload.begin() + 16, payload.begin() + 16 + length);
    return core::Status::OK;
}

core::Status parseErrorMessage(const std::vector<uint8_t>& payload,
                               std::string& severity,
                               std::string& sqlstate,
                               std::string& message,
                               std::string& detail,
                               std::string& hint,
                               core::ErrorContext* ctx) {
    severity.clear();
    sqlstate.clear();
    message.clear();
    detail.clear();
    hint.clear();
    size_t offset = 0;
    while (offset < payload.size()) {
        uint8_t field = payload[offset++];
        if (field == 0) {
            break;
        }
        size_t start = offset;
        while (offset < payload.size() && payload[offset] != 0) {
            ++offset;
        }
        if (offset >= payload.size()) {
            setError(ctx, "Error message truncated");
            return core::Status::PROTOCOL_VIOLATION;
        }
        std::string value(reinterpret_cast<const char*>(payload.data() + start), offset - start);
        ++offset;
        switch (field) {
            case 'S': severity = value; break;
            case 'C': sqlstate = value; break;
            case 'M': message = value; break;
            case 'D': detail = value; break;
            case 'H': hint = value; break;
            default: break;
        }
    }
    return core::Status::OK;
}

} // namespace scratchbird::protocol::sbwp
