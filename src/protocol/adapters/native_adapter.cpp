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
 * Native ScratchBird Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements the native ScratchBird wire protocol.
 */

#include "scratchbird/protocol/adapters/native_adapter.h"
#include "scratchbird/client/sql_helpers.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/lsm_compression.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/server/ipc_server.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <process.h>
#else
#include "scratchbird/core/posix_compat.h"
#endif

namespace scratchbird {
namespace protocol {

namespace {

uint64_t estimateRowBytes(const std::vector<ProtocolCodec::ColumnValue>& values) {
    uint64_t total = 0;
    for (const auto& value : values) {
        if (value.is_null) {
            continue;
        }
        if (value.is_stream) {
            total += value.stream_length;
        } else {
            total += value.data.size();
        }
    }
    return total;
}

uint64_t nowMicros() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

uint32_t getProcessId() {
#ifdef _WIN32
    return static_cast<uint32_t>(::_getpid());
#else
    return static_cast<uint32_t>(::getpid());
#endif
}

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendU64(std::vector<uint8_t>& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
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

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string trimAscii(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool isTruthyEnv(const char* value) {
    if (!value || value[0] == '\0') {
        return false;
    }
    const std::string normalized = toUpperAscii(value);
    return normalized != "0" &&
           normalized != "FALSE" &&
           normalized != "NO" &&
           normalized != "OFF";
}

bool preferPasswordAuthForNativeAdapter() {
    if (isTruthyEnv(std::getenv("SCRATCHBIRD_NATIVE_FORCE_PASSWORD_AUTH"))) {
        return true;
    }
    return isTruthyEnv(std::getenv("SCRATCHBIRD_EMULATION_RELAXED_PASSWORD_POLICY"));
}

std::string decodeNativePasswordPayload(const std::vector<uint8_t>& payload) {
    if (payload.empty()) {
        return {};
    }

    size_t end = payload.size();
    for (size_t i = 0; i < payload.size(); ++i) {
        if (payload[i] == '\0') {
            end = i;
            break;
        }
    }
    return std::string(reinterpret_cast<const char*>(payload.data()), end);
}

bool mapAuthMethod(sbwp::AuthMethod method, protocol::AuthMethod& method_out) {
    switch (method) {
        case sbwp::AuthMethod::Password:
            method_out = protocol::AuthMethod::PASSWORD;
            return true;
        case sbwp::AuthMethod::Md5:
            method_out = protocol::AuthMethod::MD5;
            return true;
        case sbwp::AuthMethod::ScramSha256:
            method_out = protocol::AuthMethod::SCRAM_SHA_256;
            return true;
        default:
            return false;
    }
}

bool containsAuthMethod(const std::vector<sbwp::AuthMethod>& methods, sbwp::AuthMethod method) {
    return std::find(methods.begin(), methods.end(), method) != methods.end();
}

enum class StartupAuthTokenParseResult {
    SUPPORTED,
    UNSUPPORTED_NATIVE,
    INVALID
};

StartupAuthTokenParseResult parseStartupAuthToken(const std::string& token,
                                                  sbwp::AuthMethod& method_out) {
    const std::string normalized = toLowerAscii(trimAscii(token));
    if (normalized.empty()) {
        return StartupAuthTokenParseResult::INVALID;
    }

    if (normalized == "scratchbird.auth.password_compat" ||
        normalized == "password" ||
        normalized == "cleartext" ||
        normalized == "mysql_clear_password") {
        method_out = sbwp::AuthMethod::Password;
        return StartupAuthTokenParseResult::SUPPORTED;
    }

    if (normalized == "scratchbird.auth.scram_sha_256" ||
        normalized == "scram_sha_256" ||
        normalized == "scram-sha-256" ||
        normalized == "scram256" ||
        normalized == "scram") {
        method_out = sbwp::AuthMethod::ScramSha256;
        return StartupAuthTokenParseResult::SUPPORTED;
    }

    if (normalized.rfind("scratchbird.auth.", 0) == 0 ||
        normalized == "md5" ||
        normalized == "scram_sha_512" ||
        normalized == "scram-sha-512") {
        return StartupAuthTokenParseResult::UNSUPPORTED_NATIVE;
    }

    return StartupAuthTokenParseResult::INVALID;
}

std::vector<std::string> splitCsvTokens(const std::string& csv) {
    std::vector<std::string> out;
    std::istringstream stream(csv);
    std::string token;
    while (std::getline(stream, token, ',')) {
        token = trimAscii(token);
        if (!token.empty()) {
            out.push_back(std::move(token));
        }
    }
    return out;
}

bool resolveNativeStartupAuthMethod(const std::map<std::string, std::string>& params,
                                    sbwp::AuthMethod default_method,
                                    sbwp::AuthMethod& selected_method_out,
                                    std::string& error_out) {
    error_out.clear();
    selected_method_out = default_method;

    auto parse_list = [&](const char* key,
                          std::vector<sbwp::AuthMethod>& out_methods) -> bool {
        const auto it = params.find(key);
        if (it == params.end() || trimAscii(it->second).empty()) {
            return true;
        }
        const auto tokens = splitCsvTokens(it->second);
        for (const auto& token : tokens) {
            sbwp::AuthMethod parsed = sbwp::AuthMethod::Password;
            const auto parse_result = parseStartupAuthToken(token, parsed);
            if (parse_result == StartupAuthTokenParseResult::SUPPORTED) {
                if (!containsAuthMethod(out_methods, parsed)) {
                    out_methods.push_back(parsed);
                }
                continue;
            }
            if (parse_result == StartupAuthTokenParseResult::UNSUPPORTED_NATIVE) {
                error_out = std::string("Startup auth policy token is not supported on native wire lane: ") +
                    token;
                return false;
            }
            error_out = std::string("Invalid startup auth policy token: ") + token;
            return false;
        }
        return true;
    };

    const auto method_id_it = params.find("auth_method_id");
    if (method_id_it != params.end() && !trimAscii(method_id_it->second).empty()) {
        sbwp::AuthMethod parsed = sbwp::AuthMethod::Password;
        const auto parse_result = parseStartupAuthToken(method_id_it->second, parsed);
        if (parse_result == StartupAuthTokenParseResult::SUPPORTED) {
            if (parsed != selected_method_out) {
                error_out =
                    "auth_method_id conflicts with core native auth policy for this engine lane";
                return false;
            }
        } else if (parse_result == StartupAuthTokenParseResult::UNSUPPORTED_NATIVE) {
            error_out = std::string("auth_method_id is not supported on native wire lane: ") +
                trimAscii(method_id_it->second);
            return false;
        } else {
            error_out = std::string("Invalid auth_method_id token: ") + trimAscii(method_id_it->second);
            return false;
        }
    }

    std::vector<sbwp::AuthMethod> required_methods;
    if (!parse_list("auth_required_methods", required_methods)) {
        return false;
    }
    if (!required_methods.empty() &&
        !containsAuthMethod(required_methods, selected_method_out)) {
        error_out = "Core native auth policy method violates auth_required_methods pinning";
        return false;
    }

    std::vector<sbwp::AuthMethod> forbidden_methods;
    if (!parse_list("auth_forbidden_methods", forbidden_methods)) {
        return false;
    }
    for (auto required_method : required_methods) {
        if (containsAuthMethod(forbidden_methods, required_method)) {
            error_out = "auth_required_methods overlaps auth_forbidden_methods";
            return false;
        }
    }
    if (containsAuthMethod(forbidden_methods, selected_method_out)) {
        error_out = "Core native auth policy method violates auth_forbidden_methods pinning";
        return false;
    }

    const auto channel_binding_it = params.find("auth_require_channel_binding");
    if (channel_binding_it != params.end() && isTruthyEnv(channel_binding_it->second.c_str())) {
        if (selected_method_out != sbwp::AuthMethod::ScramSha256) {
            error_out = "Core native auth policy does not satisfy auth_require_channel_binding";
            return false;
        }
    }

    return true;
}

std::string formatUuid(const uint8_t* bytes, size_t length) {
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(length * 2 + 4);
    for (size_t i = 0; i < length; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out.push_back('-');
        }
        out.push_back(kHex[(bytes[i] >> 4) & 0x0F]);
        out.push_back(kHex[bytes[i] & 0x0F]);
    }
    return out;
}

uint32_t mapWireTypeToOid(WireType type) {
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
}

int16_t mapWireTypeSize(WireType type) {
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
}

uint8_t commandTypeFromTag(const std::string& tag) {
    if (tag.empty()) {
        return 0;
    }
    std::string token;
    token.reserve(tag.size());
    for (char c : tag) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
        token.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (token == "SELECT") return 1;
    if (token == "INSERT") return 2;
    if (token == "UPDATE") return 3;
    if (token == "DELETE") return 4;
    if (token == "CREATE") return 5;
    if (token == "DROP") return 6;
    if (token == "ALTER") return 7;
    if (token == "GRANT") return 8;
    if (token == "REVOKE") return 9;
    if (token == "BEGIN") return 10;
    if (token == "COMMIT") return 11;
    if (token == "ROLLBACK") return 12;
    if (token == "COPY") return 13;
    if (token == "SET") return 14;
    if (token == "SHOW") return 15;
    if (token == "EXPLAIN") return 16;
    if (token == "VACUUM" || token == "SWEEP") return 17;
    if (token == "TRUNCATE") return 18;
    if (token == "MERGE") return 19;
    return 0;
}

bool parseNotifyStatement(const std::string& sql,
                          std::string& channel,
                          std::vector<uint8_t>& payload,
                          std::string& error) {
    auto skip_space = [&](size_t& pos) {
        while (pos < sql.size() && std::isspace(static_cast<unsigned char>(sql[pos]))) {
            ++pos;
        }
    };

    auto match_keyword = [&](const char* keyword, size_t& pos_out) -> bool {
        size_t pos = 0;
        skip_space(pos);
        size_t len = std::strlen(keyword);
        if (pos + len > sql.size()) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            if (std::toupper(static_cast<unsigned char>(sql[pos + i])) !=
                std::toupper(static_cast<unsigned char>(keyword[i]))) {
                return false;
            }
        }
        size_t next = pos + len;
        if (next < sql.size() && !std::isspace(static_cast<unsigned char>(sql[next])) &&
            sql[next] != '(') {
            return false;
        }
        pos_out = next;
        return true;
    };

    auto parse_string = [&](size_t& pos, std::string& out) -> bool {
        skip_space(pos);
        if (pos >= sql.size()) {
            return false;
        }
        char quote = sql[pos];
        if (quote != '\'' && quote != '"') {
            return false;
        }
        ++pos;
        std::string value;
        while (pos < sql.size() && sql[pos] != quote) {
            value.push_back(sql[pos++]);
        }
        if (pos >= sql.size()) {
            return false;
        }
        ++pos;
        out = std::move(value);
        return true;
    };

    auto parse_identifier = [&](size_t& pos, std::string& out) -> bool {
        skip_space(pos);
        if (pos >= sql.size()) {
            return false;
        }
        if (sql[pos] == '"' || sql[pos] == '\'') {
            return parse_string(pos, out);
        }
        size_t start = pos;
        while (pos < sql.size()) {
            char c = sql[pos];
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '$') {
                break;
            }
            ++pos;
        }
        if (pos == start) {
            return false;
        }
        out.assign(sql.begin() + static_cast<std::ptrdiff_t>(start),
                   sql.begin() + static_cast<std::ptrdiff_t>(pos));
        return true;
    };

    size_t pos = 0;
    bool is_notify = match_keyword("NOTIFY", pos);
    bool is_post_event = false;
    if (!is_notify) {
        is_post_event = match_keyword("POST_EVENT", pos);
        if (!is_post_event) {
            return false;
        }
    }

    if (!parse_identifier(pos, channel)) {
        error = "Missing notification channel";
        return true;
    }

    payload.clear();
    skip_space(pos);
    if (is_notify) {
        if (pos < sql.size() && sql[pos] == ',') {
            ++pos;
            std::string payload_text;
            if (!parse_string(pos, payload_text)) {
                error = "Invalid notification payload";
                return true;
            }
            payload.assign(payload_text.begin(), payload_text.end());
        }
    }

    if (payload.size() > 8000) {
        error = "Notification payload too large";
        return true;
    }

    return true;
}

class CopyOutStreambuf : public std::streambuf {
public:
    using WriteFn = std::function<bool(const uint8_t*, size_t, std::string&)>;

    CopyOutStreambuf(size_t buffer_size, WriteFn write_fn)
        : write_fn_(std::move(write_fn)) {
        if (buffer_size == 0) {
            buffer_size = 65536;
        }
        buffer_.resize(buffer_size);
        setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

    ~CopyOutStreambuf() override {
        sync();
    }

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        if (!flushBuffer()) {
            return traits_type::eof();
        }
        return ch;
    }

    int sync() override {
        return flushBuffer() ? 0 : -1;
    }

private:
    bool flushBuffer() {
        size_t len = static_cast<size_t>(pptr() - pbase());
        if (len == 0) {
            return true;
        }
        std::string error;
        bool ok = write_fn_(reinterpret_cast<const uint8_t*>(buffer_.data()), len, error);
        if (!ok) {
            throw std::runtime_error(error.empty() ? "COPY OUT stream error" : error);
        }
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        return true;
    }

    std::vector<char> buffer_;
    WriteFn write_fn_;
};

class CopyInStreambuf : public std::streambuf {
public:
    using ReadFn = std::function<bool(std::string&, bool&, std::string&)>;

    explicit CopyInStreambuf(ReadFn read_fn)
        : read_fn_(std::move(read_fn)) {}

protected:
    int_type underflow() override {
        if (done_) {
            return traits_type::eof();
        }

        std::string chunk;
        std::string error;
        bool ok = read_fn_(chunk, done_, error);
        if (!ok) {
            throw std::runtime_error(error.empty() ? "COPY IN stream error" : error);
        }
        if (chunk.empty()) {
            return traits_type::eof();
        }

        buffer_ = std::move(chunk);
        setg(buffer_.data(), buffer_.data(), buffer_.data() + buffer_.size());
        return traits_type::to_int_type(*gptr());
    }

private:
    ReadFn read_fn_;
    std::string buffer_;
    bool done_ = false;
};

size_t countParameterPlaceholders(const std::string& sql) {
    size_t max_index = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] != '$') {
            continue;
        }
        size_t j = i + 1;
        if (j >= sql.size() || !std::isdigit(static_cast<unsigned char>(sql[j]))) {
            continue;
        }
        size_t value = 0;
        while (j < sql.size() && std::isdigit(static_cast<unsigned char>(sql[j]))) {
            value = value * 10 + static_cast<size_t>(sql[j] - '0');
            ++j;
        }
        if (value > max_index) {
            max_index = value;
        }
        i = j;
    }
    return max_index;
}

} // namespace

// ============================================================================
// Constructor/Destructor
// ============================================================================

NativeAdapter::NativeAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {
    // Generate session ID
    generateSessionId(session_id_);
}

NativeAdapter::~NativeAdapter() = default;

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status NativeAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();
    if (buffer.size() < sbwp::kHeaderSize) {
        return core::Status::IO_ERROR;  // Need more data
    }

    std::vector<uint8_t> header_bytes(buffer.begin(), buffer.begin() + sbwp::kHeaderSize);
    sbwp::MessageHeader header;
    core::ErrorContext ctx;
    auto status = sbwp::decodeHeader(header_bytes, header, &ctx);
    if (status != core::Status::OK) {
        return status;
    }

    if (header.length > config_.max_message_size) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (header.flags & sbwp::kFlagCompressed) {
        return core::Status::NOT_SUPPORTED;
    }

    size_t total_length = sbwp::kHeaderSize + header.length;
    if (buffer.size() < total_length) {
        return core::Status::IO_ERROR;
    }

    current_message_.header = header;
    current_message_.body.assign(buffer.begin() + sbwp::kHeaderSize,
                                 buffer.begin() + total_length);
    current_sequence_ = header.sequence;
    conn->consumeReadBuffer(total_length);
    return core::Status::OK;
}

core::Status NativeAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += sbwp::kHeaderSize + current_message_.body.size();

    auto type = current_message_.header.type;

    switch (type) {
        case sbwp::MessageType::Startup:
            return handleConnectRequest(conn);

        case sbwp::MessageType::Terminate:
            return handleDisconnect(conn);

        case sbwp::MessageType::AuthResponse:
            return handleAuthRequest(conn);

        case sbwp::MessageType::Subscribe:
            return handleSubscribe(conn);

        case sbwp::MessageType::Unsubscribe:
            return handleUnsubscribe(conn);

        case sbwp::MessageType::Query:
            return handleQuery(conn);

        case sbwp::MessageType::Cancel:
            return handleQueryCancel(conn);

        case sbwp::MessageType::Parse:
            return handlePrepare(conn);

        case sbwp::MessageType::Bind:
            return handleBind(conn);

        case sbwp::MessageType::Execute:
            return handleExecute(conn);

        case sbwp::MessageType::Close:
            return handleCloseStatement(conn);

        case sbwp::MessageType::Describe:
            return handleDescribe(conn);

        case sbwp::MessageType::TxnBegin:
            return handleBeginTransaction(conn);

        case sbwp::MessageType::TxnCommit:
            return handleCommit(conn);

        case sbwp::MessageType::TxnRollback:
            return handleRollback(conn);

        case sbwp::MessageType::TxnSavepoint:
            return handleSavepoint(conn);

        case sbwp::MessageType::TxnRelease:
            return handleReleaseSavepoint(conn);

        case sbwp::MessageType::TxnRollbackTo:
            return handleRollbackTo(conn);

        case sbwp::MessageType::Ping:
            return handlePing(conn);

        case sbwp::MessageType::Sync:
            sendReady(conn);
            return sendBuffer(conn);

        case sbwp::MessageType::CopyData:
            return handleCopyData(conn);

        case sbwp::MessageType::CopyDone:
            return handleCopyDone(conn);

        case sbwp::MessageType::CopyFail:
            return handleCopyFail(conn);

        default:
            sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_SUPPORTED),
                          "0A000", "Unsupported message type");
            return sendBuffer(conn);
    }
}

core::Status NativeAdapter::sendGreeting(network::Connection* /*conn*/) {
    // Native protocol: server waits for client CONNECT_REQUEST
    return core::Status::OK;
}

core::Status NativeAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication handled in handleAuthRequest
    return core::Status::OK;
}

core::Status NativeAdapter::sendAuthResult(network::Connection* conn,
                                            bool success,
                                            const std::string& error_msg) {
    if (!success) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_PASSWORD),
                       "28000", error_msg.empty() ? "Authentication failed" : error_msg);
        return sendBuffer(conn);
    }
    sendAuthOk(conn, {});
    sendParameterStatus(conn, "attachment_id", formatUuid(session_id_, sizeof(session_id_)));
    sendParameterStatus(conn, "current_txn_id", std::to_string(transaction_id_));
    sendCapabilityStatus(conn);
    sendReady(conn);
    native_state_ = NativeProtocolState::READY;
    return sendBuffer(conn);
}

core::Status NativeAdapter::sendQueryResult(network::Connection* conn,
                                             const ResultContext& result) {
    if (result.has_error) {
        sendQueryError(conn, result.error_code, result.sqlstate, result.error_message);
        sendReady(conn);
        return core::Status::OK;
    }

    if (!result.columns.empty()) {
        // Send row description
        sendRowDescription(conn, result.columns);
        auto flush_status = flushWriteBuffer(conn);
        if (flush_status != core::Status::OK) {
            return flush_status;
        }
        pollCancel(conn);
        if (cancel_requested_ &&
            (cancel_target_sequence_ == 0 || cancel_target_sequence_ == current_sequence_)) {
            cancel_requested_ = false;
            cancel_target_sequence_ = 0;
            sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                           "57014", "Query canceled");
            sendReady(conn);
            return core::Status::OK;
        }

        for (const auto& row : result.rows) {
            if (cancel_requested_ &&
                (cancel_target_sequence_ == 0 || cancel_target_sequence_ == current_sequence_)) {
                cancel_requested_ = false;
                cancel_target_sequence_ = 0;
                sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                               "57014", "Query canceled");
                sendReady(conn);
                return core::Status::OK;
            }
            sendRowData(conn, row);
            flush_status = flushWriteBuffer(conn);
            if (flush_status != core::Status::OK) {
                return flush_status;
            }
            pollCancel(conn);
            if (cancel_requested_ &&
                (cancel_target_sequence_ == 0 || cancel_target_sequence_ == current_sequence_)) {
                cancel_requested_ = false;
                cancel_target_sequence_ = 0;
                sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                               "57014", "Query canceled");
                sendReady(conn);
                return core::Status::OK;
            }
        }
    }

    // Send command complete
    sendCommandComplete(conn, result.command_tag, result.rows_affected);
    sendReady(conn);

    return core::Status::OK;
}

core::Status NativeAdapter::sendProtocolError(network::Connection* conn,
                                               uint32_t error_code,
                                               const std::string& sqlstate,
                                               const std::string& message,
                                               const std::string& /*detail*/,
                                               const std::string& /*hint*/) {
    sendQueryError(conn, error_code, sqlstate, message);
    sendReady(conn);
    return core::Status::OK;
}

// ============================================================================
// Message Handling
// ============================================================================

core::Status NativeAdapter::handleConnectRequest(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 12) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "08000", "Startup payload truncated");
        return sendBuffer(conn);
    }

    client_features_ = readU64(payload.data() + 4);

    std::map<std::string, std::string> params;
    size_t offset = 12;
    while (offset < payload.size()) {
        size_t key_start = offset;
        while (offset < payload.size() && payload[offset] != 0) {
            ++offset;
        }
        if (offset >= payload.size()) {
            break;
        }
        if (offset == key_start) {
            ++offset;
            break;
        }
        std::string key(reinterpret_cast<const char*>(payload.data() + key_start),
                        offset - key_start);
        ++offset;
        size_t val_start = offset;
        while (offset < payload.size() && payload[offset] != 0) {
            ++offset;
        }
        if (offset >= payload.size()) {
            break;
        }
        std::string value(reinterpret_cast<const char*>(payload.data() + val_start),
                          offset - val_start);
        ++offset;
        params.emplace(std::move(key), std::move(value));
    }

    auto get_param = [&](const char* name) -> std::string {
        auto it = params.find(name);
        if (it == params.end()) {
            return {};
        }
        return it->second;
    };

    auto equalsDatabaseName = [](const std::string& lhs, const std::string& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                std::tolower(static_cast<unsigned char>(rhs[i]))) {
                return false;
            }
        }
        return true;
    };

    const std::string requested_database = get_param("database");
    if (config_.enforce_bound_database &&
        !config_.default_database.empty() &&
        !requested_database.empty() &&
        !equalsDatabaseName(requested_database, config_.default_database)) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_AUTHORIZATION),
                      "28000", "Database switch denied by manager binding context");
        return sendBuffer(conn);
    }

    database_name_ = config_.enforce_bound_database
        ? config_.default_database
        : requested_database;
    if (database_name_.empty()) {
        database_name_ = config_.default_database;
    }
    if (database_name_.empty()) {
        database_name_ = "default";
    }
    username_ = get_param("user");
    conn->setUsername(username_);
    conn->setDatabase(database_name_);
    remote_password_.clear();

    std::string app_name = get_param("application_name");
    if (!app_name.empty()) {
        conn->setApplicationName(app_name);
    }

    const sbwp::AuthMethod default_auth_method = preferPasswordAuthForNativeAdapter()
        ? sbwp::AuthMethod::Password
        : sbwp::AuthMethod::ScramSha256;
    std::string auth_selection_error;
    if (!resolveNativeStartupAuthMethod(params,
                                        default_auth_method,
                                        auth_method_,
                                        auth_selection_error)) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_SUPPORTED),
                      "0A000",
                      auth_selection_error.empty() ? "Native auth method selection failed"
                                                   : auth_selection_error);
        return sendBuffer(conn);
    }

    auth_in_progress_ = true;
    scram_pending_ = false;
    sendAuthRequest(conn, auth_method_);
    native_state_ = NativeProtocolState::AUTHENTICATING;
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleDisconnect(network::Connection* conn) {
    native_state_ = NativeProtocolState::CLOSING;
    subscribed_channels_.clear();
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

core::Status NativeAdapter::handleAuthRequest(network::Connection* conn) {
    std::vector<uint8_t> payload = current_message_.body;
    if (!auth_in_progress_) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "08004", "Authentication not expected");
        return sendBuffer(conn);
    }

    if (config_.engine_endpoint.empty()) {
        auth_in_progress_ = false;
        return sendAuthResult(conn, true);
    }

    core::ErrorContext ctx;

    if (auth_method_ == sbwp::AuthMethod::Password) {
        remote_password_ = decodeNativePasswordPayload(payload);
        if (client_) {
            client_->disconnect();
            client_.reset();
        }

        auto status = ensureRemoteClient(&ctx);
        if (status != core::Status::OK) {
            sendQueryError(conn, static_cast<uint32_t>(status),
                          "28000", ctx.message.empty() ? "Authentication failed" : ctx.message);
            return sendBuffer(conn);
        }

        auth_in_progress_ = false;
        scram_pending_ = false;
        sendAuthOk(conn, {});
        sendParameterStatus(conn, "attachment_id", formatUuid(session_id_, sizeof(session_id_)));
        sendParameterStatus(conn, "current_txn_id", std::to_string(transaction_id_));
        sendCapabilityStatus(conn);
        sendReady(conn);
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    auto status = ensureRemoteClient(&ctx);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status),
                      "08001", ctx.message.empty() ? "Engine connection failed" : ctx.message);
        return sendBuffer(conn);
    }

    protocol::AuthMethod internal_method = protocol::AuthMethod::SCRAM_SHA_256;
    if (!mapAuthMethod(auth_method_, internal_method)) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::NOT_SUPPORTED),
                       "0A000",
                       "Unsupported native authentication method");
        return sendBuffer(conn);
    }
    client::Connection::AuthResponse auth_response;
    status = client_->sendAuthRequest(internal_method, payload, auth_response, &ctx);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status),
                      "28000", ctx.message.empty() ? "Authentication failed" : ctx.message);
        return sendBuffer(conn);
    }

    if (auth_response.status == AuthStatus::CONTINUE) {
        scram_pending_ = true;
        sendAuthContinue(conn, auth_method_, 0, auth_response.data);
        return sendBuffer(conn);
    }

    if (auth_response.status == AuthStatus::OK) {
        user_id_ = auth_response.user_id;
        auth_in_progress_ = false;
        scram_pending_ = false;
        sendAuthOk(conn, auth_response.data);
        sendParameterStatus(conn, "attachment_id", formatUuid(session_id_, sizeof(session_id_)));
        sendParameterStatus(conn, "current_txn_id", std::to_string(transaction_id_));
        sendCapabilityStatus(conn);
        sendReady(conn);
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_PASSWORD),
                  "28000", "Authentication failed");
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleQuery(network::Connection* conn) {
    native_state_ = NativeProtocolState::QUERY_PROCESSING;
    cancel_requested_ = false;
    cancel_target_sequence_ = 0;
    const auto& payload = current_message_.body;
    if (payload.size() < 12) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                       "42000",
                       "Invalid QUERY payload");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }
    uint32_t flags = readU32(payload.data());
    (void)flags;
    uint32_t max_rows = readU32(payload.data() + 4);
    uint32_t timeout_ms = readU32(payload.data() + 8);
    (void)timeout_ms;
    std::string query;
    size_t offset = 12;
    while (offset < payload.size() && payload[offset] != 0) {
        query.push_back(static_cast<char>(payload[offset]));
        ++offset;
    }

    bool from_stdin = false;
    bool to_stdout = false;
    CopyFormat copy_format = CopyFormat::TEXT;
    if (parseCopyQuery(query, from_stdin, to_stdout, &copy_format)) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_SUPPORTED),
                      "0A000", "COPY is not yet supported over SBWP");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    // Execute query
    QueryContext query_ctx;
    query_ctx.query = query;

    ResultContext result;
    if (!config_.engine_endpoint.empty()) {
        auto exec_status = executeRemoteQuery(query, nullptr, result);
        if (exec_status != core::Status::OK) {
            native_state_ = NativeProtocolState::READY;
            auto send_status = sendQueryResult(conn, result);
            if (send_status != core::Status::OK) {
                return send_status;
            }
            return sendBuffer(conn);
        }
    } else {
        executeQuery(query_ctx, result);
    }

    if (max_rows > 0 && result.rows.size() > max_rows) {
        PortalState portal;
        portal.statement_name.clear();
        portal.bound = true;
        portal.columns = result.columns;
        portal.rows = result.rows;
        portal.command_tag = result.command_tag;
        portal.rows_affected = result.rows_affected;
        portal.completed = false;
        portal.fetch_pos = 0;
        portal.rows_sent = 0;
        portal.bytes_sent = 0;
        portals_[""] = std::move(portal);

        auto& portal_ref = portals_[""];
        auto status = sendPortalResults(conn, portal_ref, max_rows, false);
        if (portal_ref.completed) {
            portals_.erase("");
        }
        native_state_ = NativeProtocolState::READY;
        if (status != core::Status::OK) {
            return status;
        }
        return sendBuffer(conn);
    }

    auto send_status = sendQueryResult(conn, result);
    native_state_ = NativeProtocolState::READY;

    if (send_status != core::Status::OK) {
        return send_status;
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleQueryCancel(network::Connection* conn) {
    (void)conn;
    if (current_message_.body.size() >= 8) {
        uint32_t cancel_type = readU32(current_message_.body.data());
        uint32_t target_seq = readU32(current_message_.body.data() + 4);
        (void)cancel_type;
        cancel_target_sequence_ = target_seq;
    }
    cancel_requested_ = true;
    if (cancel_target_sequence_ != 0 && cancel_target_sequence_ != current_sequence_) {
        return core::Status::OK;
    }

    bool has_active_portal = false;
    for (auto& entry : portals_) {
        if (!entry.second.completed) {
            entry.second.completed = true;
            has_active_portal = true;
        }
    }
    if (has_active_portal) {
        portals_.clear();
        cancel_requested_ = false;
        cancel_target_sequence_ = 0;
        sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                       "57014", "Query canceled");
        sendReady(conn);
        return sendBuffer(conn);
    }
    cancel_requested_ = false;
    cancel_target_sequence_ = 0;
    return core::Status::OK;
}

core::Status NativeAdapter::handlePrepare(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 8) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid PARSE payload");
        return sendBuffer(conn);
    }
    size_t offset = 0;
    uint32_t name_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + name_len + 4 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid PARSE payload");
        return sendBuffer(conn);
    }
    std::string stmt_name(reinterpret_cast<const char*>(payload.data() + offset), name_len);
    offset += name_len;
    uint32_t query_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + query_len + 4 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid PARSE payload");
        return sendBuffer(conn);
    }
    std::string query(reinterpret_cast<const char*>(payload.data() + offset), query_len);
    offset += query_len;
    uint16_t param_count = 0;
    if (offset + 2 <= payload.size()) {
        param_count = readU16(payload.data() + offset);
    }
    (void)param_count;

    std::vector<int32_t> param_types;
    auto status = prepareStatement(stmt_name, query, param_types);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status),
                      "42000", "Failed to prepare statement");
        return sendBuffer(conn);
    }

    sendParseComplete(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleBind(network::Connection* conn) {
    const auto& payload = current_message_.body;
    size_t offset = 0;
    if (payload.size() < 8) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }

    uint32_t portal_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + portal_len + 4 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }
    std::string portal_name(reinterpret_cast<const char*>(payload.data() + offset), portal_len);
    offset += portal_len;

    uint32_t stmt_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + stmt_len + 2 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }
    std::string stmt_name(reinterpret_cast<const char*>(payload.data() + offset), stmt_len);
    offset += stmt_len;

    if (offset + 2 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }
    uint16_t format_count = readU16(payload.data() + offset);
    offset += 2;
    std::vector<uint16_t> format_codes;
    format_codes.reserve(format_count);
    for (uint16_t i = 0; i < format_count; ++i) {
        if (offset + 2 > payload.size()) {
            sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                          "42000", "Invalid BIND payload");
            return sendBuffer(conn);
        }
        format_codes.push_back(readU16(payload.data() + offset));
        offset += 2;
    }

    if (offset + 4 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }
    uint16_t param_count = readU16(payload.data() + offset);
    offset += 2;
    offset += 2; // reserved

    std::vector<std::string> param_values;
    std::vector<bool> param_nulls;
    param_values.reserve(param_count);
    param_nulls.reserve(param_count);

    auto decode_param = [&](const uint8_t* data, size_t len, uint16_t format) -> std::string {
        if (format == sbwp::kFormatBinary) {
            if (len == sizeof(int16_t)) {
                int16_t v = 0;
                std::memcpy(&v, data, sizeof(int16_t));
                return std::to_string(v);
            }
            if (len == sizeof(int32_t)) {
                int32_t v = 0;
                std::memcpy(&v, data, sizeof(int32_t));
                return std::to_string(v);
            }
            if (len == sizeof(int64_t)) {
                int64_t v = 0;
                std::memcpy(&v, data, sizeof(int64_t));
                return std::to_string(v);
            }
        }
        return std::string(reinterpret_cast<const char*>(data), len);
    };

    for (uint16_t i = 0; i < param_count; ++i) {
        if (offset + 4 > payload.size()) {
            sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                          "42000", "Invalid BIND payload");
            return sendBuffer(conn);
        }
        uint32_t len = readU32(payload.data() + offset);
        offset += 4;
        if (len == 0xFFFFFFFFu) {
            param_values.emplace_back();
            param_nulls.push_back(true);
            continue;
        }
        if (offset + len > payload.size()) {
            sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                          "42000", "Invalid BIND payload");
            return sendBuffer(conn);
        }
        uint16_t format = sbwp::kFormatText;
        if (!format_codes.empty()) {
            format = (format_codes.size() == 1) ? format_codes[0] : format_codes[i];
        }
        param_values.push_back(decode_param(payload.data() + offset, len, format));
        param_nulls.push_back(false);
        offset += len;
    }

    if (offset + 2 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }
    uint16_t result_format_count = readU16(payload.data() + offset);
    offset += 2;
    offset += static_cast<size_t>(result_format_count) * 2;
    if (offset > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid BIND payload");
        return sendBuffer(conn);
    }

    auto it = prepared_statements_.find(stmt_name);
    if (it == prepared_statements_.end()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                      "26000", "Prepared statement not found");
        return sendBuffer(conn);
    }

    size_t expected = countParameterPlaceholders(it->second);
    if (expected != param_count) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                      "07001", "Parameter count mismatch");
        return sendBuffer(conn);
    }

    PortalState portal;
    portal.statement_name = stmt_name;
    portal.param_values = std::move(param_values);
    portal.param_nulls = std::move(param_nulls);
    portal.bound = true;
    portals_[portal_name] = std::move(portal);

    sendBindComplete(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleExecute(network::Connection* conn) {
    native_state_ = NativeProtocolState::QUERY_PROCESSING;
    cancel_requested_ = false;
    cancel_target_sequence_ = 0;
    const auto& payload = current_message_.body;
    if (payload.size() < 8) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid EXECUTE payload");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    size_t offset = 0;
    uint32_t portal_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + portal_len + 4 > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid EXECUTE payload");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }
    std::string portal_name(reinterpret_cast<const char*>(payload.data() + offset), portal_len);
    offset += portal_len;
    uint32_t max_rows = readU32(payload.data() + offset);

    if (cancel_requested_ &&
        (cancel_target_sequence_ == 0 || cancel_target_sequence_ == current_sequence_)) {
        cancel_requested_ = false;
        cancel_target_sequence_ = 0;
        sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                      "57014", "Query canceled");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    auto portal_it = portals_.find(portal_name);
    if (portal_it == portals_.end()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                      "26000", "Portal not found");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    PortalState& portal = portal_it->second;
    if (!portal.bound) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                      "26000", "Portal not bound");
        native_state_ = NativeProtocolState::READY;
        return sendBuffer(conn);
    }

    if (portal.rows.empty() && !portal.completed) {
        auto stmt_it = prepared_statements_.find(portal.statement_name);
        if (stmt_it == prepared_statements_.end()) {
            sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                          "26000", "Prepared statement not found");
            native_state_ = NativeProtocolState::READY;
            return sendBuffer(conn);
        }

        QueryContext ctx;
        ctx.query = stmt_it->second;
        ctx.statement_name = portal.statement_name;
        ctx.parameter_values = portal.param_values;
        ctx.parameter_nulls = portal.param_nulls;

        ResultContext result;
        if (!config_.engine_endpoint.empty()) {
            std::vector<ProtocolCodec::ColumnValue> param_values;
            param_values.reserve(ctx.parameter_values.size());
            for (const auto& value : ctx.parameter_values) {
                param_values.push_back(ProtocolCodec::ColumnValue::fromString(value));
            }
            std::string exec_query = client::substituteParameters(ctx.query, param_values);
            auto exec_status = executeRemoteQuery(exec_query, nullptr, result);
            if (exec_status != core::Status::OK) {
                native_state_ = NativeProtocolState::READY;
                auto send_status = sendQueryResult(conn, result);
                if (send_status != core::Status::OK) {
                    return send_status;
                }
                return sendBuffer(conn);
            }
        } else {
            auto exec_status = executePrepared(ctx.statement_name, ctx, result);
            if (exec_status != core::Status::OK) {
                native_state_ = NativeProtocolState::READY;
                return sendBuffer(conn);
            }
        }

        portal.columns = result.columns;
        portal.rows = result.rows;
        portal.command_tag = result.command_tag;
        portal.rows_affected = result.rows_affected;
        portal.completed = false;
        portal.fetch_pos = 0;
        portal.rows_sent = 0;
        portal.bytes_sent = 0;
    }

    auto status = sendPortalResults(conn, portal, max_rows, false);
    if (portal.completed) {
        portals_.erase(portal_it);
    }
    native_state_ = NativeProtocolState::READY;
    if (status != core::Status::OK) {
        return status;
    }
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleCloseStatement(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 8) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid CLOSE payload");
        return sendBuffer(conn);
    }
    uint8_t close_type = payload[0];
    uint32_t name_len = readU32(payload.data() + 4);
    if (8u + name_len > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid CLOSE payload");
        return sendBuffer(conn);
    }
    std::string name(reinterpret_cast<const char*>(payload.data() + 8), name_len);

    if (close_type == 'S') {
        prepared_statements_.erase(name);
        closePrepared(name);
        for (auto it = portals_.begin(); it != portals_.end();) {
            if (it->second.statement_name == name) {
                it = portals_.erase(it);
            } else {
                ++it;
            }
        }
    } else if (close_type == 'P') {
        portals_.erase(name);
    } else {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                      "42000", "Invalid CLOSE type");
        return sendBuffer(conn);
    }

    sendCloseComplete(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleDescribe(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 8) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid DESCRIBE payload");
        return sendBuffer(conn);
    }
    uint8_t describe_type = payload[0];
    uint32_t name_len = readU32(payload.data() + 4);
    if (8u + name_len > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", "Invalid DESCRIBE payload");
        return sendBuffer(conn);
    }
    std::string name(reinterpret_cast<const char*>(payload.data() + 8), name_len);

    if (describe_type == 'S') {
        auto it = prepared_statements_.find(name);
        if (it == prepared_statements_.end()) {
            sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                          "26000", "Prepared statement not found");
            return sendBuffer(conn);
        }
        size_t param_count = countParameterPlaceholders(it->second);
        std::vector<uint32_t> param_types(param_count, 0);
        sendParameterDescription(conn, param_types);
        sendNoData(conn);
        return sendBuffer(conn);
    }

    if (describe_type == 'P') {
        auto portal_it = portals_.find(name);
        if (portal_it == portals_.end()) {
            sendQueryError(conn, static_cast<uint32_t>(core::Status::NOT_FOUND),
                          "26000", "Portal not found");
            return sendBuffer(conn);
        }
        if (!portal_it->second.columns.empty()) {
            sendRowDescription(conn, portal_it->second.columns);
        } else {
            sendNoData(conn);
        }
        return sendBuffer(conn);
    }

    sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                  "42000", "Invalid DESCRIBE type");
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleBeginTransaction(network::Connection* conn) {
    auto status = beginTransaction();
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "25000",
                      "Failed to begin transaction");
    } else {
        sendTransactionStatus(conn, true);
    }
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleCommit(network::Connection* conn) {
    auto status = commitTransaction();
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "25000",
                      "Failed to commit transaction");
    } else {
        sendTransactionStatus(conn, false);
    }
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleRollback(network::Connection* conn) {
    auto status = rollbackTransaction();
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "25000",
                      "Failed to rollback transaction");
    } else {
        sendTransactionStatus(conn, false);
    }
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleSavepoint(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 4) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "3B000", "Invalid SAVEPOINT payload");
        return sendBuffer(conn);
    }
    uint32_t name_len = readU32(payload.data());
    if (4u + name_len > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "3B000", "Invalid SAVEPOINT payload");
        return sendBuffer(conn);
    }
    std::string name(reinterpret_cast<const char*>(payload.data() + 4), name_len);

    auto status = savepoint(name);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "3B000",
                      "Failed to create savepoint");
    } else {
        sendTransactionStatus(conn, true);
    }
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleReleaseSavepoint(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 4) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "3B000", "Invalid RELEASE SAVEPOINT payload");
        return sendBuffer(conn);
    }
    uint32_t name_len = readU32(payload.data());
    if (4u + name_len > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "3B000", "Invalid RELEASE SAVEPOINT payload");
        return sendBuffer(conn);
    }
    std::string name(reinterpret_cast<const char*>(payload.data() + 4), name_len);

    auto status = releaseSavepoint(name);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "3B000",
                      "Failed to release savepoint");
    } else {
        sendTransactionStatus(conn, true);
    }
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleRollbackTo(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 4) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "3B000", "Invalid ROLLBACK TO payload");
        return sendBuffer(conn);
    }
    uint32_t name_len = readU32(payload.data());
    if (4u + name_len > payload.size()) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "3B000", "Invalid ROLLBACK TO payload");
        return sendBuffer(conn);
    }
    std::string name(reinterpret_cast<const char*>(payload.data() + 4), name_len);

    auto status = rollbackToSavepoint(name);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "3B000",
                      "Failed to rollback to savepoint");
    } else {
        sendTransactionStatus(conn, true);
    }
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handlePing(network::Connection* conn) {
    uint64_t client_time = 0;
    if (current_message_.body.size() >= 8) {
        client_time = readU64(current_message_.body.data());
    }
    (void)client_time;
    sendPong(conn, nowMicros(), current_sequence_);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleStatusRequest(network::Connection* conn) {
    sendStatusResponse(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleSubscribe(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 12) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                       "42000",
                       "Invalid SUBSCRIBE payload");
        return sendBuffer(conn);
    }
    uint8_t subscribe_type = payload[0];
    uint32_t channel_len = readU32(payload.data() + 4);
    if (8u + channel_len + 4 > payload.size()) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                       "42000",
                       "Invalid SUBSCRIBE payload");
        return sendBuffer(conn);
    }
    std::string channel(reinterpret_cast<const char*>(payload.data() + 8), channel_len);
    size_t offset = 8 + channel_len;
    uint32_t filter_len = readU32(payload.data() + offset);
    offset += 4;
    if (offset + filter_len > payload.size()) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                       "42000",
                       "Invalid SUBSCRIBE payload");
        return sendBuffer(conn);
    }
    std::string filter(reinterpret_cast<const char*>(payload.data() + offset), filter_len);

    if (connection_ctx_ && !connection_ctx_->isSuperuser()) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PERMISSION_DENIED),
                       "42501",
                       "Permission denied for SUBSCRIBE");
        return sendBuffer(conn);
    }

    if (subscribe_type != 0) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::NOT_SUPPORTED),
                       "0A000",
                       "Unsupported subscription type");
        return sendBuffer(conn);
    }

    if (channel.empty()) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                       "22023",
                       "SUBSCRIBE requires a channel");
        return sendBuffer(conn);
    }

    (void)filter;
    subscribed_channels_.insert(channel);
    sendCommandComplete(conn, "SUBSCRIBE", 0);
    sendReady(conn);
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleUnsubscribe(network::Connection* conn) {
    const auto& payload = current_message_.body;
    if (payload.size() < 4) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                       "42000",
                       "Invalid UNSUBSCRIBE payload");
        return sendBuffer(conn);
    }
    uint32_t channel_len = readU32(payload.data());
    if (4u + channel_len > payload.size()) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                       "42000",
                       "Invalid UNSUBSCRIBE payload");
        return sendBuffer(conn);
    }
    std::string channel(reinterpret_cast<const char*>(payload.data() + 4), channel_len);

    if (connection_ctx_ && !connection_ctx_->isSuperuser()) {
        sendQueryError(conn,
                       static_cast<uint32_t>(core::Status::PERMISSION_DENIED),
                       "42501",
                       "Permission denied for UNSUBSCRIBE");
        return sendBuffer(conn);
    }

    if (!channel.empty()) {
        subscribed_channels_.erase(channel);
    }
    sendCommandComplete(conn, "UNSUBSCRIBE", 0);
    sendReady(conn);
    return sendBuffer(conn);
}

// ============================================================================
// Message Sending
// ============================================================================

void NativeAdapter::sendMessage(network::Connection* conn,
                                sbwp::MessageType type,
                                const std::vector<uint8_t>& payload,
                                uint8_t flags,
                                uint32_t sequence_override) {
    sbwp::MessageHeader header;
    header.type = type;
    header.flags = flags;
    header.length = static_cast<uint32_t>(payload.size());
    header.sequence = sequence_override ? sequence_override : current_sequence_;
    header.attachment_id.fill(0);
    std::memcpy(header.attachment_id.data(), session_id_, sizeof(session_id_));
    header.txn_id = transaction_id_;

    std::vector<uint8_t> encoded = sbwp::encodeMessage(header, payload);
    writeToBuffer(conn, encoded.data(), encoded.size());
}

void NativeAdapter::sendAuthRequest(network::Connection* conn,
                                    sbwp::AuthMethod method,
                                    const std::vector<uint8_t>& data) {
    std::vector<uint8_t> payload(4 + data.size(), 0);
    payload[0] = static_cast<uint8_t>(method);
    if (!data.empty()) {
        std::memcpy(payload.data() + 4, data.data(), data.size());
    }
    sendMessage(conn, sbwp::MessageType::AuthRequest, payload);
}

void NativeAdapter::sendAuthContinue(network::Connection* conn,
                                     sbwp::AuthMethod method,
                                     uint8_t stage,
                                     const std::vector<uint8_t>& data) {
    std::vector<uint8_t> payload(8 + data.size(), 0);
    payload[0] = static_cast<uint8_t>(method);
    payload[1] = stage;
    uint32_t data_len = static_cast<uint32_t>(data.size());
    std::memcpy(payload.data() + 4, &data_len, sizeof(data_len));
    if (!data.empty()) {
        std::memcpy(payload.data() + 8, data.data(), data.size());
    }
    sendMessage(conn, sbwp::MessageType::AuthContinue, payload);
}

void NativeAdapter::sendAuthOk(network::Connection* conn,
                               const std::vector<uint8_t>& info) {
    std::vector<uint8_t> payload(20 + info.size(), 0);
    std::memcpy(payload.data(), session_id_, sizeof(session_id_));
    uint32_t info_len = static_cast<uint32_t>(info.size());
    std::memcpy(payload.data() + 16, &info_len, sizeof(info_len));
    if (!info.empty()) {
        std::memcpy(payload.data() + 20, info.data(), info.size());
    }
    sendMessage(conn, sbwp::MessageType::AuthOk, payload);
}

void NativeAdapter::sendQueryError(network::Connection* conn, uint32_t /*error_code*/,
                                   const std::string& sqlstate,
                                   const std::string& message) {
    std::vector<uint8_t> payload;
    payload.reserve(message.size() + sqlstate.size() + 16);
    auto append_field = [&payload](char code, const std::string& value) {
        payload.push_back(static_cast<uint8_t>(code));
        payload.insert(payload.end(), value.begin(), value.end());
        payload.push_back(0);
    };
    append_field('S', "ERROR");
    append_field('C', sqlstate.empty() ? "HY000" : sqlstate);
    append_field('M', message.empty() ? "Error" : message);
    payload.push_back(0);
    sendMessage(conn, sbwp::MessageType::Error, payload);
}

void NativeAdapter::sendRowDescription(network::Connection* conn,
                                       const std::vector<ProtocolCodec::ColumnInfo>& columns) {
    std::vector<uint8_t> payload;
    payload.reserve(8 + columns.size() * 32);
    appendU16(payload, static_cast<uint16_t>(columns.size()));
    appendU16(payload, 0);
    for (const auto& col : columns) {
        appendU32(payload, static_cast<uint32_t>(col.name.size()));
        payload.insert(payload.end(), col.name.begin(), col.name.end());
        appendU32(payload, 0);
        appendU16(payload, 0);
        appendU32(payload, mapWireTypeToOid(col.type));
        appendU16(payload, static_cast<uint16_t>(mapWireTypeSize(col.type)));
        appendU32(payload, col.type_modifier);
        payload.push_back(sbwp::kFormatBinary);
        payload.push_back(1);
        appendU16(payload, 0);
    }
    sendMessage(conn, sbwp::MessageType::RowDescription, payload);
}

void NativeAdapter::sendRowData(network::Connection* conn,
                                const std::vector<ProtocolCodec::ColumnValue>& values) {
    uint16_t count = static_cast<uint16_t>(values.size());
    uint16_t null_bytes = static_cast<uint16_t>((count + 7) / 8);
    std::vector<uint8_t> payload;
    payload.reserve(4 + null_bytes + values.size() * 16);
    appendU16(payload, count);
    appendU16(payload, null_bytes);
    size_t null_offset = payload.size();
    payload.resize(payload.size() + null_bytes, 0);

    for (size_t i = 0; i < values.size(); ++i) {
        const auto& value = values[i];
        if (value.is_null) {
            size_t byte_index = i / 8;
            uint8_t bit_index = static_cast<uint8_t>(i % 8);
            payload[null_offset + byte_index] |= static_cast<uint8_t>(1u << bit_index);
            continue;
        }
        appendU32(payload, static_cast<uint32_t>(value.data.size()));
        if (!value.data.empty()) {
            payload.insert(payload.end(), value.data.begin(), value.data.end());
        }
    }
    sendMessage(conn, sbwp::MessageType::DataRow, payload);
}

void NativeAdapter::sendCommandComplete(network::Connection* conn, const std::string& tag,
                                        int64_t rows_affected) {
    std::vector<uint8_t> payload;
    payload.reserve(24 + tag.size() + 1);
    payload.push_back(commandTypeFromTag(tag));
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    appendU64(payload, static_cast<uint64_t>(rows_affected));
    appendU64(payload, 0);
    payload.insert(payload.end(), tag.begin(), tag.end());
    payload.push_back(0);
    sendMessage(conn, sbwp::MessageType::CommandComplete, payload);
}

void NativeAdapter::sendPortalSuspended(network::Connection* conn) {
    sendMessage(conn, sbwp::MessageType::PortalSuspended, {});
}

void NativeAdapter::sendReady(network::Connection* conn) {
    std::vector<uint8_t> payload(20, 0);
    payload[0] = in_transaction_ ? 1 : 0;
    std::memcpy(payload.data() + 4, &transaction_id_, sizeof(transaction_id_));
    uint64_t epoch = 0;
    std::memcpy(payload.data() + 12, &epoch, sizeof(epoch));
    sendMessage(conn, sbwp::MessageType::Ready, payload);
}

void NativeAdapter::sendParameterStatus(network::Connection* conn,
                                        const std::string& name,
                                        const std::string& value) {
    std::vector<uint8_t> payload;
    payload.reserve(8 + name.size() + value.size());
    appendU32(payload, static_cast<uint32_t>(name.size()));
    payload.insert(payload.end(), name.begin(), name.end());
    appendU32(payload, static_cast<uint32_t>(value.size()));
    payload.insert(payload.end(), value.begin(), value.end());
    sendMessage(conn, sbwp::MessageType::ParameterStatus, payload);
}

void NativeAdapter::sendParameterDescription(network::Connection* conn,
                                             const std::vector<uint32_t>& param_types) {
    std::vector<uint8_t> payload;
    payload.reserve(4 + param_types.size() * 4);
    appendU16(payload, static_cast<uint16_t>(param_types.size()));
    appendU16(payload, 0);
    for (uint32_t oid : param_types) {
        appendU32(payload, oid);
    }
    sendMessage(conn, sbwp::MessageType::ParameterDescription, payload);
}

void NativeAdapter::sendParseComplete(network::Connection* conn) {
    sendMessage(conn, sbwp::MessageType::ParseComplete, {});
}

void NativeAdapter::sendBindComplete(network::Connection* conn) {
    sendMessage(conn, sbwp::MessageType::BindComplete, {});
}

void NativeAdapter::sendCloseComplete(network::Connection* conn) {
    sendMessage(conn, sbwp::MessageType::CloseComplete, {});
}

void NativeAdapter::sendNoData(network::Connection* conn) {
    sendMessage(conn, sbwp::MessageType::NoData, {});
}

void NativeAdapter::sendQueryProgress(network::Connection* /*conn*/,
                                      uint64_t /*rows_processed*/,
                                      uint64_t /*bytes_processed*/) {
    // SBWP progress frames are not yet implemented for the native listener.
}

void NativeAdapter::sendNotification(network::Connection* conn,
                                     uint32_t process_id,
                                     const std::string& channel,
                                     const std::vector<uint8_t>& payload,
                                     uint8_t change_type,
                                     uint64_t row_id) {
    std::vector<uint8_t> message;
    message.reserve(12 + channel.size() + payload.size() + 9);
    appendU32(message, process_id);
    appendU32(message, static_cast<uint32_t>(channel.size()));
    message.insert(message.end(), channel.begin(), channel.end());
    appendU32(message, static_cast<uint32_t>(payload.size()));
    message.insert(message.end(), payload.begin(), payload.end());
    if (change_type != 0) {
        message.push_back(change_type);
        appendU64(message, row_id);
    }
    sendMessage(conn, sbwp::MessageType::Notification, message);
}

void NativeAdapter::sendPrepareResponse(network::Connection* conn, uint32_t /*stmt_id*/,
                                        bool success, const std::string& error_msg) {
    if (!success) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "42000", error_msg);
        return;
    }
    sendParseComplete(conn);
}

void NativeAdapter::sendDescribeResponse(network::Connection* conn, uint32_t /*stmt_id*/,
                                         const std::vector<ProtocolCodec::ColumnInfo>& columns,
                                         uint16_t param_count) {
    std::vector<uint32_t> param_types(param_count, 0);
    sendParameterDescription(conn, param_types);
    if (!columns.empty()) {
        sendRowDescription(conn, columns);
    } else {
        sendNoData(conn);
    }
}

void NativeAdapter::sendTransactionStatus(network::Connection* conn, bool in_transaction) {
    std::vector<uint8_t> payload;
    payload.reserve(32);
    payload.push_back(in_transaction ? 'T' : 'I');
    payload.push_back(1);
    payload.push_back(0);
    payload.push_back(0);
    appendU64(payload, transaction_id_);
    appendU64(payload, 0);
    appendU64(payload, 0);
    appendU32(payload, 0);
    sendMessage(conn, sbwp::MessageType::TxnStatus, payload);
}

void NativeAdapter::sendPong(network::Connection* conn, uint64_t timestamp, uint32_t /*sequence*/) {
    std::vector<uint8_t> payload(8, 0);
    std::memcpy(payload.data(), &timestamp, sizeof(timestamp));
    sendMessage(conn, sbwp::MessageType::Pong, payload);
}

void NativeAdapter::sendCopyInResponse(network::Connection* conn, uint8_t format,
                                       uint32_t window_bytes) {
    auto payload = sbwp::buildCopyInResponsePayload(format, window_bytes);
    sendMessage(conn, sbwp::MessageType::CopyInResponse, payload);
}

void NativeAdapter::sendCopyOutResponse(network::Connection* conn, uint8_t format,
                                        uint16_t column_count,
                                        const std::vector<uint32_t>& column_formats) {
    auto payload = sbwp::buildCopyOutResponsePayload(format, column_count, column_formats);
    sendMessage(conn, sbwp::MessageType::CopyOutResponse, payload);
}

void NativeAdapter::sendCopyBothResponse(network::Connection* conn, uint8_t format,
                                         uint32_t window_bytes) {
    auto payload = sbwp::buildCopyBothResponsePayload(format, window_bytes);
    sendMessage(conn, sbwp::MessageType::CopyBothResponse, payload);
}

void NativeAdapter::sendCopyData(network::Connection* conn, const uint8_t* data, size_t len) {
    auto payload = sbwp::buildCopyDataPayload(data, len);
    sendMessage(conn, sbwp::MessageType::CopyData, payload);
}

core::Status NativeAdapter::sendPortalResults(network::Connection* conn,
                                              PortalState& portal,
                                              uint32_t max_rows,
                                              bool /*backward*/) {
    if (!portal.columns.empty() && portal.fetch_pos == 0) {
        sendRowDescription(conn, portal.columns);
        auto flush_status = flushWriteBuffer(conn);
        if (flush_status != core::Status::OK) {
            return flush_status;
        }
        pollCancel(conn);
        if (cancel_requested_ &&
            (cancel_target_sequence_ == 0 || cancel_target_sequence_ == current_sequence_)) {
            cancel_requested_ = false;
            cancel_target_sequence_ = 0;
            sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                           "57014", "Query canceled");
            sendReady(conn);
            portal.completed = true;
            return core::Status::OK;
        }
    }
    size_t start = portal.fetch_pos;
    size_t end = portal.rows.size();
    if (max_rows > 0) {
        end = std::min(end, start + static_cast<size_t>(max_rows));
    }

    for (size_t i = start; i < end; ++i) {
        const auto& row = portal.rows[i];
        sendRowData(conn, row);
        auto flush_status = flushWriteBuffer(conn);
        if (flush_status != core::Status::OK) {
            return flush_status;
        }
        pollCancel(conn);
        if (cancel_requested_ &&
            (cancel_target_sequence_ == 0 || cancel_target_sequence_ == current_sequence_)) {
            cancel_requested_ = false;
            cancel_target_sequence_ = 0;
            sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                           "57014", "Query canceled");
            sendReady(conn);
            portal.completed = true;
            return core::Status::OK;
        }
        portal.rows_sent++;
        portal.bytes_sent += estimateRowBytes(row);
    }
    portal.fetch_pos = end;
    if (portal.fetch_pos < portal.rows.size()) {
        sendPortalSuspended(conn);
        portal.completed = false;
        return core::Status::OK;
    }

    sendCommandComplete(conn, portal.command_tag, portal.rows_affected);
    sendReady(conn);
    portal.completed = true;
    return core::Status::OK;
}

void NativeAdapter::sendStatusResponse(network::Connection* conn) {
    sendCapabilityStatus(conn);
    sendReady(conn);
}

void NativeAdapter::sendCapabilityStatus(network::Connection* conn) {
    const uint64_t feature_mask = serverFeatureMask();
    const uint64_t profile_mask = feature_mask & sbwp::kFeatureProfileMask;
    const auto enabled_profiles = sbwp::enabledProfilesFromFeatureMask(feature_mask);

    std::ostringstream profile_join;
    for (size_t i = 0; i < enabled_profiles.size(); ++i) {
        if (i > 0) {
            profile_join << ',';
        }
        profile_join << enabled_profiles[i];
    }

    sendParameterStatus(conn, "profile_contract_version", "16.0");
    sendParameterStatus(conn, "server_feature_mask", std::to_string(feature_mask));
    sendParameterStatus(conn, "profile_capability_mask", std::to_string(profile_mask));
    sendParameterStatus(conn, "profile_count", std::to_string(enabled_profiles.size()));
    sendParameterStatus(conn, "profile_bundle_set", profile_join.str());
}

uint64_t NativeAdapter::serverFeatureMask() const {
    constexpr uint64_t kCoreFeatures =
        sbwp::kFeatureCompression |
        sbwp::kFeatureStreaming |
        sbwp::kFeatureSblr |
        sbwp::kFeatureNotifications |
        sbwp::kFeatureQueryPlan |
        sbwp::kFeatureBatch |
        sbwp::kFeaturePipeline |
        sbwp::kFeatureSavepoints;
    return kCoreFeatures | sbwp::canonicalProfileFeatureMask();
}

core::Status NativeAdapter::ensureRemoteClient(core::ErrorContext* ctx) {
    if (client_) {
        if (client_->isConnected()) {
            return core::Status::OK;
        }
        client_.reset();
    }

    client_config_.database_name = database_name_.empty() ? "default" : database_name_;
    if (!config_.engine_endpoint.empty()) {
        client_config_.ipc_method = server::IPCMethod::AUTO;
        client_config_.socket_path = config_.engine_endpoint;
    } else {
        client_config_.ipc_method = server::IPCMethod::UNIX_SOCKET;
        client_config_.socket_path = server::getIPCPath(client_config_.database_name,
                                                        client_config_.ipc_method);
    }
    client_config_.connect_timeout_ms = config_.read_timeout_ms;
    client_config_.read_timeout_ms = config_.read_timeout_ms;
    client_config_.write_timeout_ms = config_.write_timeout_ms;
    client_config_.auto_commit = true;
    client_config_.auto_start_server = false;
    client_config_.connect_client_flags = config_.connect_client_flags;
    client_config_.has_bound_db_uuid = config_.has_bound_db_uuid;
    client_config_.bound_db_uuid = config_.bound_db_uuid;
    client_config_.manual_auth = !username_.empty() && remote_password_.empty();
    auto setPreferredAuthMethods = [&](protocol::AuthMethod primary) {
        client_config_.preferred_auth_methods.clear();
        client_config_.preferred_auth_methods.push_back(primary);
        const std::array<protocol::AuthMethod, 5> fallbacks = {
            protocol::AuthMethod::SCRAM_SHA_256,
            protocol::AuthMethod::SCRAM_SHA_512,
            protocol::AuthMethod::PEER,
            protocol::AuthMethod::PASSWORD,
            protocol::AuthMethod::MD5
        };
        for (auto method : fallbacks) {
            if (method == primary) {
                continue;
            }
            client_config_.preferred_auth_methods.push_back(method);
        }
    };
    if (!username_.empty()) {
        client_config_.username = username_;
        client_config_.password = remote_password_;
        protocol::AuthMethod preferred_method = protocol::AuthMethod::SCRAM_SHA_256;
        if (!mapAuthMethod(auth_method_, preferred_method)) {
            if (ctx) {
                ctx->set(core::Status::NOT_SUPPORTED,
                         "Unsupported native authentication method",
                         __FILE__, __LINE__, __func__);
            }
            return core::Status::NOT_SUPPORTED;
        }
        setPreferredAuthMethods(preferred_method);
    } else {
        client_config_.username = "bootstrap";
        client_config_.password.clear();
        client_config_.preferred_auth_methods.clear();
    }

    core::Status status = core::Status::OK;
    std::string last_message;
    for (int attempt = 0; attempt < 5; ++attempt) {
        client_ = std::make_unique<client::Connection>();
        status = client_->connect(client_config_, ctx);
        if (status == core::Status::OK) {
            return core::Status::OK;
        }
        const std::string client_error = client_->getLastError();
        if (!client_error.empty()) {
            last_message = client_error;
        }
        if (ctx && !ctx->message.empty()) {
            last_message = ctx->message;
        }
        client_.reset();
        if (status != core::Status::CONNECTION_FAILURE && status != core::Status::IO_ERROR) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (ctx && !last_message.empty()) {
        ctx->message = last_message;
    }
    if (ctx && ctx->message.empty()) {
        ctx->message = "Engine connect/auth failed with status " +
            std::to_string(static_cast<int>(status));
    }
    return status;
}

core::Status NativeAdapter::executeRemoteQuery(const std::string& sql,
                                               const std::vector<uint8_t>* bytecode,
                                               ResultContext& result) {
    core::ErrorContext ctx;
    auto status = ensureRemoteClient(&ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.sqlstate = "58000";
        result.error_message = ctx.message.empty() ? "Failed to connect to engine" : ctx.message;
        return status;
    }

    client::ResultSet rs;
    if (bytecode) {
        status = client_->executeBytecode(*bytecode, sql, &rs, &ctx);
    } else {
        std::vector<uint8_t> compiled_bytecode;
        std::string compile_error;
        status = compileQuery(sql, compiled_bytecode, compile_error);
        if (status != core::Status::OK) {
            result.has_error = true;
            result.error_code = static_cast<uint32_t>(status);
            result.sqlstate = "42000";
            result.error_message = compile_error.empty()
                ? "Failed to compile query before submit"
                : compile_error;
            return status;
        }
        status = client_->executeBytecode(compiled_bytecode, sql, &rs, &ctx);
    }

    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        std::string err = client_->getLastError();
        if (err.empty()) {
            err = ctx.message;
        }
        result.error_message = err.empty() ? "Query execution failed" : err;
        result.sqlstate = "42000";
        return status;
    }

    result.columns.clear();
    for (const auto& col : rs.getColumns()) {
        ProtocolCodec::ColumnInfo info;
        info.name = col.name;
        info.type = col.type;
        info.type_modifier = col.type_modifier;
        result.columns.push_back(info);
    }

    result.rows.clear();
    const auto row_count = static_cast<size_t>(rs.getRowCount());
    for (size_t i = 0; i < row_count; ++i) {
        result.rows.push_back(rs.getRowValues(i));
    }

    result.rows_affected = rs.getRowsAffected();
    result.command_tag = rs.getCommandTag();
    return core::Status::OK;
}

// ============================================================================
// COPY Helpers
// ============================================================================

core::Status NativeAdapter::handleCopyQuery(network::Connection* conn, const QueryContext& ctx,
                                 bool from_stdin, bool to_stdout, CopyFormat format) {
    copy_start_time_ = std::chrono::steady_clock::now();
    copy_format_ = format;
    copy_table_name_ = ctx.query; // Store the full query for now
    copy_buffer_.clear();
    copy_rows_processed_ = 0;
    copy_bytes_processed_ = 0;

    if (from_stdin && to_stdout) {
        // COPY BOTH mode - bidirectional streaming
        copy_direction_ = CopyDirection::BOTH;
        copy_in_window_bytes_ = kDefaultCopyWindow;
        copy_in_window_grant_ = kDefaultCopyWindow;
        native_state_ = NativeProtocolState::COPY_BOTH;
        sendCopyBothResponse(conn, static_cast<uint8_t>(format), kDefaultCopyWindow);
        return sendBuffer(conn);
    } else if (from_stdin) {
        // COPY FROM STDIN - client sends data to server
        copy_direction_ = CopyDirection::IN;
        copy_in_window_bytes_ = kDefaultCopyWindow;
        copy_in_window_grant_ = kDefaultCopyWindow;
        native_state_ = NativeProtocolState::COPY_IN;
        sendCopyInResponse(conn, static_cast<uint8_t>(format), kDefaultCopyWindow);
        return sendBuffer(conn);
    } else if (to_stdout) {
        // COPY TO STDOUT - server sends data to client
        copy_direction_ = CopyDirection::OUT;
        copy_out_window_bytes_ = kDefaultCopyWindow;
        native_state_ = NativeProtocolState::COPY_OUT;
        sendCopyOutResponse(conn, static_cast<uint8_t>(format), 0, {});
        auto status = sendBuffer(conn);
        if (status != core::Status::OK) {
            recordCopyMetrics("out", 0, 0, true, copy_start_time_);
            return status;
        }

        // Execute the COPY query and stream results
        ResultContext result;
        if (!config_.engine_endpoint.empty()) {
            status = executeRemoteQuery(ctx.query, nullptr, result);
        } else {
            QueryContext query_ctx = ctx;
            executeQuery(query_ctx, result);
            status = core::Status::OK;
        }

        if (status != core::Status::OK || result.has_error) {
            recordCopyMetrics("out", 0, 0, true, copy_start_time_);
            auto fail_payload = sbwp::buildCopyFailPayload(result.error_message.empty() ? "COPY execution failed" : result.error_message);
            sendMessage(conn, sbwp::MessageType::CopyFail, fail_payload);
            sendReady(conn);
            native_state_ = NativeProtocolState::READY;
            copy_direction_ = CopyDirection::NONE;
            return sendBuffer(conn);
        }

        // Stream the result rows as COPY data
        uint64_t total_bytes = 0;
        for (const auto& row : result.rows) {
            if (row.empty()) continue;
            const auto& value = row[0]; // COPY typically returns single column rows
            if (!value.is_null) {
                sendCopyData(conn, value.data.data(), value.data.size());
                total_bytes += value.data.size();
                copy_rows_processed_++;

                // Check for backpressure
                if (total_bytes >= copy_out_window_bytes_) {
                    auto flush_status = flushWriteBuffer(conn);
                    if (flush_status != core::Status::OK) {
                        recordCopyMetrics("out", copy_rows_processed_, total_bytes, true, copy_start_time_);
                        return flush_status;
                    }
                }
            }
        }

        copy_bytes_processed_ = total_bytes;
        recordCopyMetrics("out", copy_rows_processed_, copy_bytes_processed_, false, copy_start_time_);

        // Send CopyDone and complete
        std::vector<uint8_t> done_payload;
        sendMessage(conn, sbwp::MessageType::CopyDone, done_payload);
        sendCommandComplete(conn, "COPY", static_cast<int64_t>(copy_rows_processed_));
        sendReady(conn);
        native_state_ = NativeProtocolState::READY;
        copy_direction_ = CopyDirection::NONE;
        return sendBuffer(conn);
    }

    // Should not reach here
    sendQueryError(conn, static_cast<uint32_t>(core::Status::INVALID_ARGUMENT),
                  "42000", "Invalid COPY direction");
    native_state_ = NativeProtocolState::READY;
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleCopyData(network::Connection* conn) {
    if (native_state_ != NativeProtocolState::COPY_IN &&
        native_state_ != NativeProtocolState::COPY_BOTH) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "08P01", "CopyData received outside of COPY mode");
        native_state_ = NativeProtocolState::READY;
        copy_direction_ = CopyDirection::NONE;
        return sendBuffer(conn);
    }

    core::ErrorContext ctx;
    std::vector<uint8_t> data;
    auto status = sbwp::parseCopyData(current_message_.body, data, &ctx);
    if (status != core::Status::OK) {
        sendQueryError(conn, static_cast<uint32_t>(status), "08P01",
                      ctx.message.empty() ? "Invalid CopyData message" : ctx.message);
        recordCopyMetrics("in", copy_rows_processed_, copy_bytes_processed_, true, copy_start_time_);
        native_state_ = NativeProtocolState::READY;
        copy_direction_ = CopyDirection::NONE;
        return sendBuffer(conn);
    }

    // Append data to buffer
    if (!data.empty()) {
        copy_buffer_.insert(copy_buffer_.end(), data.begin(), data.end());
        copy_bytes_processed_ += data.size();

        // Update window tracking
        if (copy_in_window_grant_ >= data.size()) {
            copy_in_window_grant_ -= static_cast<uint32_t>(data.size());
        } else {
            copy_in_window_grant_ = 0;
        }

        // Grant more window if running low
        if (copy_in_window_grant_ < kDefaultCopyWindow / 4) {
            uint32_t grant = kDefaultCopyWindow - copy_in_window_grant_;
            copy_in_window_grant_ = kDefaultCopyWindow;
            // In a full implementation, send a window update message
            (void)grant;
        }
    }

    return core::Status::OK;
}

core::Status NativeAdapter::handleCopyDone(network::Connection* conn) {
    if (native_state_ != NativeProtocolState::COPY_IN &&
        native_state_ != NativeProtocolState::COPY_BOTH) {
        sendQueryError(conn, static_cast<uint32_t>(core::Status::PROTOCOL_VIOLATION),
                      "08P01", "CopyDone received outside of COPY mode");
        native_state_ = NativeProtocolState::READY;
        copy_direction_ = CopyDirection::NONE;
        return sendBuffer(conn);
    }

    // Process the accumulated COPY data
    // In a full implementation, this would parse the COPY data and insert into the table
    // For now, we simulate successful completion

    // Count rows (simplified - assumes newline-delimited)
    copy_rows_processed_ = 0;
    if (!copy_buffer_.empty()) {
        for (size_t i = 0; i < copy_buffer_.size(); ++i) {
            if (copy_buffer_[i] == '\n') {
                copy_rows_processed_++;
            }
        }
        // Count last row if no trailing newline
        if (copy_buffer_.back() != '\n') {
            copy_rows_processed_++;
        }
    }

    recordCopyMetrics("in", copy_rows_processed_, copy_bytes_processed_, false, copy_start_time_);

    // Execute the COPY query with the accumulated data via remote client if available
    if (!config_.engine_endpoint.empty() && client_ && client_->isConnected()) {
        // Note: In a full implementation, we would use a proper COPY API.
        // For now we execute the statement through the regular remote bytecode path.
        ResultContext copy_result;
        std::string copy_sql = copy_table_name_;
        auto exec_status = executeRemoteQuery(copy_sql, nullptr, copy_result);
        if (exec_status != core::Status::OK) {
            recordCopyMetrics("in", copy_rows_processed_, copy_bytes_processed_, true, copy_start_time_);
            sendQueryError(conn, static_cast<uint32_t>(exec_status), "58000",
                           copy_result.error_message.empty() ? "COPY execution failed" : copy_result.error_message);
            native_state_ = NativeProtocolState::READY;
            copy_direction_ = CopyDirection::NONE;
            copy_buffer_.clear();
            return sendBuffer(conn);
        }
    }

    sendCommandComplete(conn, "COPY", static_cast<int64_t>(copy_rows_processed_));
    sendReady(conn);
    native_state_ = NativeProtocolState::READY;
    copy_direction_ = CopyDirection::NONE;
    copy_buffer_.clear();
    return sendBuffer(conn);
}

core::Status NativeAdapter::handleCopyFail(network::Connection* conn) {
    core::ErrorContext ctx;
    std::string error_message;
    auto status = sbwp::parseCopyFail(current_message_.body, error_message, &ctx);
    if (status != core::Status::OK) {
        error_message = "CopyFail parsing error";
    }

    recordCopyMetrics(copy_direction_ == CopyDirection::IN ? "in" : "out",
                      copy_rows_processed_, copy_bytes_processed_, true, copy_start_time_);

    sendQueryError(conn, static_cast<uint32_t>(core::Status::QUERY_CANCELED),
                  "57014", error_message.empty() ? "COPY aborted by client" : error_message);
    sendReady(conn);
    native_state_ = NativeProtocolState::READY;
    copy_direction_ = CopyDirection::NONE;
    copy_buffer_.clear();
    return sendBuffer(conn);
}

core::Status NativeAdapter::flushWriteBuffer(network::Connection* conn,
                                             std::chrono::milliseconds max_wait) {
    auto start = std::chrono::steady_clock::now();
    auto* socket = conn ? conn->getSocket() : nullptr;
    while (conn->hasPendingWrites()) {
        auto written = conn->writeFromBuffer();
        if (written < 0) {
            return core::Status::IO_ERROR;
        }
        if (written == 0) {
            if (!socket || !socket->waitWritable(1000)) {
                return core::Status::IO_ERROR;
            }
        }
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (max_wait.count() > 0 && elapsed > max_wait) {
            return core::Status::IO_ERROR;
        }
    }
    return core::Status::OK;
}

core::Status NativeAdapter::receiveMessageBlocking(network::Connection* conn,
                                                   sbwp::ProtocolMessage& msg) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        auto status = parseMessage(conn);
        if (status == core::Status::OK) {
            msg = std::move(current_message_);
            return core::Status::OK;
        }
        if (status != core::Status::IO_ERROR) {
            return status;
        }

        if (!conn->isOpen()) {
            return core::Status::CONNECTION_FAILURE;
        }

        auto bytes = conn->readIntoBuffer();
        if (bytes < 0) {
            return core::Status::IO_ERROR;
        }
        if (bytes == 0) {
            if (conn->isReadTimedOut()) {
                return core::Status::IO_ERROR;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > std::chrono::seconds(30)) {
            return core::Status::IO_ERROR;
        }
    }
}

bool NativeAdapter::pollCancel(network::Connection* conn) {
    auto* socket = conn ? conn->getSocket() : nullptr;
    if (socket && !socket->waitReadable(0)) {
        return cancel_requested_;
    }
    auto bytes = conn->readIntoBuffer();
    if (bytes < 0) {
        return cancel_requested_;
    }

    while (true) {
        const auto& buffer = conn->getReadBuffer();
        if (buffer.size() < sbwp::kHeaderSize) {
            return cancel_requested_;
        }

        std::vector<uint8_t> header_bytes(buffer.begin(),
                                          buffer.begin() + sbwp::kHeaderSize);
        sbwp::MessageHeader header;
        core::ErrorContext ctx;
        auto status = sbwp::decodeHeader(header_bytes, header, &ctx);
        if (status != core::Status::OK) {
            return cancel_requested_;
        }

        if (header.type != sbwp::MessageType::Cancel) {
            return cancel_requested_;
        }

        size_t total_length = sbwp::kHeaderSize + header.length;
        if (buffer.size() < total_length) {
            return cancel_requested_;
        }

        status = parseMessage(conn);
        if (status != core::Status::OK) {
            return cancel_requested_;
        }

        handleQueryCancel(conn);
        if (!cancel_requested_) {
            return false;
        }
    }
}

bool NativeAdapter::parseCopyQuery(const std::string& sql, bool& from_stdin, bool& to_stdout,
                                   CopyFormat* format_out) const {
    from_stdin = false;
    to_stdout = false;
    if (format_out) {
        *format_out = CopyFormat::TEXT;
    }

    auto trim_left = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        return input.substr(pos);
    };

    std::string trimmed = trim_left(sql);
    if (trimmed.size() < 4) {
        return false;
    }

    std::string upper = trimmed;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper.rfind("COPY", 0) != 0) {
        return false;
    }
    if (upper.size() > 4) {
        char next = upper[4];
        if (!std::isspace(static_cast<unsigned char>(next)) && next != '(') {
            return false;
        }
    }

    if (upper.find("FROM STDIN") != std::string::npos) {
        from_stdin = true;
    }
    if (upper.find("TO STDOUT") != std::string::npos) {
        to_stdout = true;
    }

    if (format_out) {
        auto format_pos = upper.find("FORMAT");
        if (format_pos != std::string::npos) {
            if (upper.find("BINARY", format_pos) != std::string::npos) {
                *format_out = CopyFormat::BINARY;
            } else if (upper.find("TEXT", format_pos) != std::string::npos) {
                *format_out = CopyFormat::TEXT;
            } else if (upper.find("CSV", format_pos) != std::string::npos) {
                *format_out = CopyFormat::TEXT;
            }
        }
    }
    return from_stdin || to_stdout;
}

void NativeAdapter::recordCopyMetrics(const std::string& direction,
                                      uint64_t rows,
                                      uint64_t bytes,
                                      bool error,
                                      const std::chrono::steady_clock::time_point& start_time) const {
    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    if (metrics.copy_rows_total && rows > 0) {
        metrics.copy_rows_total->inc(static_cast<double>(rows), {direction});
    }
    if (metrics.copy_bytes_total && bytes > 0) {
        metrics.copy_bytes_total->inc(static_cast<double>(bytes), {direction});
    }
    if (metrics.copy_errors_total && error) {
        metrics.copy_errors_total->inc(1.0);
    }
    if (metrics.copy_duration_seconds) {
        auto end_time = std::chrono::steady_clock::now();
        double duration_seconds =
            std::chrono::duration_cast<std::chrono::duration<double>>(end_time - start_time).count();
        metrics.copy_duration_seconds->observe(duration_seconds, {direction});
    }
}

bool NativeAdapter::waitForCopyOutWindow(network::Connection* conn, std::string& error) {
    if (!conn || !conn->isOpen()) {
        error = "Connection closed";
        return false;
    }

    // Simple implementation: check if we should pause based on window
    if (copy_out_window_bytes_ > 0 && copy_bytes_processed_ >= copy_out_window_bytes_) {
        // In a full implementation, we would wait for a window update from the client
        // For now, we just flush and continue
        auto status = flushWriteBuffer(conn, std::chrono::milliseconds(100));
        if (status != core::Status::OK) {
            error = "Failed to flush write buffer";
            return false;
        }
        // Reset window tracking after flush
        copy_bytes_processed_ = 0;
    }
    return true;
}

bool NativeAdapter::waitForStreamWindow(network::Connection* conn, std::string& error) {
    if (!conn || !conn->isOpen()) {
        error = "Connection closed";
        return false;
    }

    if (stream_paused_) {
        // In a full implementation, we would wait for a StreamControl resume message
        // For now, we just check periodically
        auto status = flushWriteBuffer(conn, std::chrono::milliseconds(100));
        if (status != core::Status::OK) {
            error = "Failed to flush write buffer";
            return false;
        }
    }
    return true;
}

bool NativeAdapter::sendStreamPayload(network::Connection* conn, uint64_t stream_id,
                                      const uint8_t* data, size_t len, std::string& error) {
    (void)stream_id;

    if (!conn || !conn->isOpen()) {
        error = "Connection closed";
        return false;
    }

    // Check window
    if (!waitForStreamWindow(conn, error)) {
        return false;
    }

    // Send as StreamData message
    std::vector<uint8_t> payload;
    payload.reserve(8 + len);
    appendU64(payload, stream_id);
    if (len > 0 && data != nullptr) {
        payload.insert(payload.end(), data, data + len);
    }
    sendMessage(conn, sbwp::MessageType::StreamData, payload);

    auto status = flushWriteBuffer(conn, std::chrono::milliseconds(100));
    if (status != core::Status::OK) {
        error = "Failed to send stream payload";
        return false;
    }
    return true;
}

bool NativeAdapter::sendCopyOutChunk(network::Connection* conn, const uint8_t* data, size_t len,
                                     std::string& error) {
    if (!conn || !conn->isOpen()) {
        error = "Connection closed";
        return false;
    }

    if (native_state_ != NativeProtocolState::COPY_OUT &&
        native_state_ != NativeProtocolState::COPY_BOTH) {
        error = "Not in COPY OUT mode";
        return false;
    }

    // Check window
    if (!waitForCopyOutWindow(conn, error)) {
        return false;
    }

    // Send CopyData message
    sendCopyData(conn, data, len);
    copy_bytes_processed_ += len;
    copy_total_bytes_ += len;

    auto status = flushWriteBuffer(conn, std::chrono::milliseconds(100));
    if (status != core::Status::OK) {
        error = "Failed to send COPY data";
        return false;
    }
    return true;
}

core::Status NativeAdapter::grantCopyInWindow(network::Connection* conn, uint32_t window_bytes) {
    if (!conn || !conn->isOpen()) {
        return core::Status::CONNECTION_FAILURE;
    }

    if (native_state_ != NativeProtocolState::COPY_IN &&
        native_state_ != NativeProtocolState::COPY_BOTH) {
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    copy_in_window_grant_ += window_bytes;
    copy_in_window_bytes_ += window_bytes;

    // In a full implementation, we might send a window grant message to the client
    return core::Status::OK;
}

bool NativeAdapter::readCopyInChunk(network::Connection* conn, std::string& out, bool& done,
                                    std::string& error) {
    out.clear();
    done = false;

    if (!conn || !conn->isOpen()) {
        error = "Connection closed";
        return false;
    }

    if (native_state_ != NativeProtocolState::COPY_IN &&
        native_state_ != NativeProtocolState::COPY_BOTH) {
        error = "Not in COPY IN mode";
        return false;
    }

    // Check if we have buffered data
    if (!copy_buffer_.empty()) {
        // Extract a chunk from the buffer
        size_t chunk_size = std::min(copy_buffer_.size(), static_cast<size_t>(65536));
        out.assign(reinterpret_cast<const char*>(copy_buffer_.data()), chunk_size);
        copy_buffer_.erase(copy_buffer_.begin(), copy_buffer_.begin() + static_cast<std::ptrdiff_t>(chunk_size));
        return true;
    }

    // Read more data from the connection
    auto bytes = conn->readIntoBuffer();
    if (bytes < 0) {
        error = "Read error";
        return false;
    }

    // Try to parse any pending messages
    while (true) {
        const auto& buffer = conn->getReadBuffer();
        if (buffer.size() < sbwp::kHeaderSize) {
            break;
        }

        std::vector<uint8_t> header_bytes(buffer.begin(), buffer.begin() + sbwp::kHeaderSize);
        sbwp::MessageHeader header;
        core::ErrorContext ctx;
        auto status = sbwp::decodeHeader(header_bytes, header, &ctx);
        if (status != core::Status::OK) {
            break;
        }

        size_t total_length = sbwp::kHeaderSize + header.length;
        if (buffer.size() < total_length) {
            break;
        }

        // Parse the message
        status = parseMessage(conn);
        if (status != core::Status::OK) {
            break;
        }

        if (header.type == sbwp::MessageType::CopyData) {
            std::vector<uint8_t> data;
            status = sbwp::parseCopyData(current_message_.body, data, &ctx);
            if (status == core::Status::OK && !data.empty()) {
                copy_buffer_.insert(copy_buffer_.end(), data.begin(), data.end());
            }
        } else if (header.type == sbwp::MessageType::CopyDone) {
            done = true;
            return true;
        } else if (header.type == sbwp::MessageType::CopyFail) {
            std::string fail_msg;
            sbwp::parseCopyFail(current_message_.body, fail_msg, &ctx);
            error = fail_msg.empty() ? "COPY failed by client" : fail_msg;
            return false;
        }
    }

    // Return any data we extracted
    if (!copy_buffer_.empty()) {
        size_t chunk_size = std::min(copy_buffer_.size(), static_cast<size_t>(65536));
        out.assign(reinterpret_cast<const char*>(copy_buffer_.data()), chunk_size);
        copy_buffer_.erase(copy_buffer_.begin(), copy_buffer_.begin() + static_cast<std::ptrdiff_t>(chunk_size));
        return true;
    }

    // No data available yet
    return true;
}

} // namespace protocol
} // namespace scratchbird
