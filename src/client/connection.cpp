/**
 * ScratchBird Client Library Implementation
 *
 * Local Server Architecture - Phase 4
 */

#include "scratchbird/client/connection.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/protocol/wire_protocol.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <iostream>
#include <condition_variable>
#include <unordered_map>
#include <csignal>
#include <fcntl.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace scratchbird {
namespace client {

// Helper to check if status is OK
inline bool isOk(core::Status status) {
    return status == core::Status::OK;
}

// ============================================================================
// ResultSet Implementation
// ============================================================================

class ResultSetImpl {
public:
    std::vector<ColumnMeta> columns_;
    std::vector<std::vector<protocol::ProtocolCodec::ColumnValue>> rows_;
    int64_t current_row_ = -1;
    int64_t row_count_ = -1;
    int64_t rows_affected_ = 0;
    std::string command_tag_;

    void clear() {
        columns_.clear();
        rows_.clear();
        current_row_ = -1;
        row_count_ = -1;
        rows_affected_ = 0;
        command_tag_.clear();
    }
};

ResultSet::ResultSet() : impl_(std::make_unique<ResultSetImpl>()) {}
ResultSet::~ResultSet() = default;

ResultSet::ResultSet(ResultSet&& other) noexcept = default;
ResultSet& ResultSet::operator=(ResultSet&& other) noexcept = default;

size_t ResultSet::getColumnCount() const {
    return impl_->columns_.size();
}

std::string ResultSet::getColumnName(size_t index) const {
    if (index >= impl_->columns_.size()) return "";
    return impl_->columns_[index].name;
}

int ResultSet::getColumnIndex(const std::string& name) const {
    // Case-insensitive comparison
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    for (size_t i = 0; i < impl_->columns_.size(); ++i) {
        std::string col_lower = impl_->columns_[i].name;
        std::transform(col_lower.begin(), col_lower.end(), col_lower.begin(), ::tolower);
        if (col_lower == lower_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

protocol::WireType ResultSet::getColumnType(size_t index) const {
    if (index >= impl_->columns_.size()) return protocol::WireType::UNKNOWN;
    return impl_->columns_[index].type;
}

const std::vector<ColumnMeta>& ResultSet::getColumns() const {
    return impl_->columns_;
}

int64_t ResultSet::getRowCount() const {
    return impl_->row_count_ >= 0 ? impl_->row_count_ : static_cast<int64_t>(impl_->rows_.size());
}

int64_t ResultSet::getRowsAffected() const {
    return impl_->rows_affected_;
}

bool ResultSet::isEmpty() const {
    return impl_->rows_.empty();
}

const std::vector<protocol::ProtocolCodec::ColumnValue>& ResultSet::getRowValues(size_t index) const {
    static const std::vector<protocol::ProtocolCodec::ColumnValue> kEmptyRow;
    if (index >= impl_->rows_.size()) {
        return kEmptyRow;
    }
    return impl_->rows_[index];
}

const std::string& ResultSet::getCommandTag() const {
    return impl_->command_tag_;
}

bool ResultSet::next() {
    if (impl_->current_row_ + 1 < static_cast<int64_t>(impl_->rows_.size())) {
        ++impl_->current_row_;
        return true;
    }
    return false;
}

void ResultSet::reset() {
    impl_->current_row_ = -1;
}

int64_t ResultSet::getCurrentRow() const {
    return impl_->current_row_;
}

bool ResultSet::isNull(size_t column) const {
    if (impl_->current_row_ < 0 ||
        impl_->current_row_ >= static_cast<int64_t>(impl_->rows_.size()) ||
        column >= impl_->rows_[impl_->current_row_].size()) {
        return true;
    }
    return impl_->rows_[impl_->current_row_][column].is_null;
}

bool ResultSet::getBool(size_t column) const {
    if (isNull(column)) return false;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.empty()) return false;
    return val.data[0] != 0;
}

int16_t ResultSet::getInt16(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 2) return 0;
    int16_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

int32_t ResultSet::getInt32(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 4) return 0;
    int32_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

int64_t ResultSet::getInt64(size_t column) const {
    if (isNull(column)) return 0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 8) return 0;
    int64_t result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

float ResultSet::getFloat(size_t column) const {
    if (isNull(column)) return 0.0f;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 4) return 0.0f;
    float result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

double ResultSet::getDouble(size_t column) const {
    if (isNull(column)) return 0.0;
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() < 8) return 0.0;
    double result;
    std::memcpy(&result, val.data.data(), sizeof(result));
    return result;
}

std::string ResultSet::getString(size_t column) const {
    if (isNull(column)) return "";
    const auto& val = impl_->rows_[impl_->current_row_][column];
    return std::string(reinterpret_cast<const char*>(val.data.data()), val.data.size());
}

std::vector<uint8_t> ResultSet::getBytes(size_t column) const {
    if (isNull(column)) return {};
    return impl_->rows_[impl_->current_row_][column].data;
}

int64_t ResultSet::getTimestamp(size_t column) const {
    return getInt64(column);
}

int32_t ResultSet::getDate(size_t column) const {
    return getInt32(column);
}

int64_t ResultSet::getTime(size_t column) const {
    return getInt64(column);
}

std::string ResultSet::getUUID(size_t column) const {
    if (isNull(column)) return "";
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (val.data.size() != 16) return "";

    // Format as UUID string
    char buf[37];
    snprintf(buf, sizeof(buf),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             val.data[0], val.data[1], val.data[2], val.data[3],
             val.data[4], val.data[5], val.data[6], val.data[7],
             val.data[8], val.data[9], val.data[10], val.data[11],
             val.data[12], val.data[13], val.data[14], val.data[15]);
    return std::string(buf);
}

const uint8_t* ResultSet::getRaw(size_t column, size_t* length) const {
    if (isNull(column)) {
        if (length) *length = 0;
        return nullptr;
    }
    const auto& val = impl_->rows_[impl_->current_row_][column];
    if (length) *length = val.data.size();
    return val.data.data();
}

// By-name accessors
bool ResultSet::isNull(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx < 0 || isNull(static_cast<size_t>(idx));
}

bool ResultSet::getBool(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getBool(static_cast<size_t>(idx)) : false;
}

int16_t ResultSet::getInt16(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt16(static_cast<size_t>(idx)) : 0;
}

int32_t ResultSet::getInt32(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt32(static_cast<size_t>(idx)) : 0;
}

int64_t ResultSet::getInt64(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getInt64(static_cast<size_t>(idx)) : 0;
}

float ResultSet::getFloat(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getFloat(static_cast<size_t>(idx)) : 0.0f;
}

double ResultSet::getDouble(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getDouble(static_cast<size_t>(idx)) : 0.0;
}

std::string ResultSet::getString(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getString(static_cast<size_t>(idx)) : "";
}

std::vector<uint8_t> ResultSet::getBytes(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getBytes(static_cast<size_t>(idx)) : std::vector<uint8_t>{};
}

int64_t ResultSet::getTimestamp(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getTimestamp(static_cast<size_t>(idx)) : 0;
}

int32_t ResultSet::getDate(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getDate(static_cast<size_t>(idx)) : 0;
}

int64_t ResultSet::getTime(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getTime(static_cast<size_t>(idx)) : 0;
}

std::string ResultSet::getUUID(const std::string& column) const {
    int idx = getColumnIndex(column);
    return idx >= 0 ? getUUID(static_cast<size_t>(idx)) : "";
}

// ============================================================================
// PreparedStatement Helper Functions - NET-1: Parameter Substitution
// ============================================================================

// Escape a string for safe SQL insertion (prevents SQL injection)
static std::string escapeString(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2 + 2);
    result += '\'';
    for (char c : str) {
        if (c == '\'') {
            result += "''";  // SQL standard escape for single quotes
        } else if (c == '\\') {
            result += "\\\\";  // Escape backslashes
        } else if (c == '\0') {
            // Skip null bytes (potential injection vector)
            continue;
        } else {
            result += c;
        }
    }
    result += '\'';
    return result;
}

// Convert ColumnValue to SQL literal string with proper escaping
static std::string columnValueToSqlLiteral(const protocol::ProtocolCodec::ColumnValue& val) {
    if (val.is_null) {
        return "NULL";
    }

    // Determine type based on data size and content
    // Note: ColumnValue doesn't store type info, so we infer from size
    if (val.data.size() == 1) {
        // Boolean (1 byte)
        return val.data[0] ? "TRUE" : "FALSE";
    } else if (val.data.size() == 4) {
        // Int32
        int32_t value;
        std::memcpy(&value, val.data.data(), 4);
        return std::to_string(value);
    } else if (val.data.size() == 8) {
        // Could be int64 or double - check if it looks like a reasonable double
        // We default to treating as int64 first, then check if it might be a double
        int64_t int_value;
        std::memcpy(&int_value, val.data.data(), 8);

        double dbl_value;
        std::memcpy(&dbl_value, val.data.data(), 8);

        // If the double representation is a "normal" floating point number
        // (not NaN, not infinity, and has a fractional part), use it
        if (std::isfinite(dbl_value) && dbl_value != static_cast<double>(int_value)) {
            std::ostringstream oss;
            oss.precision(17);  // Maximum precision for double
            oss << dbl_value;
            return oss.str();
        }

        return std::to_string(int_value);
    } else {
        // String or bytes - treat as string with escaping
        std::string str(val.data.begin(), val.data.end());
        return escapeString(str);
    }
}

// Substitute parameters in SQL, replacing $1, $2, etc. with escaped values
static std::string substituteParameters(
    const std::string& sql,
    const std::vector<protocol::ProtocolCodec::ColumnValue>& params) {

    std::string result;
    result.reserve(sql.size() * 2);

    size_t i = 0;
    while (i < sql.size()) {
        // Check for parameter placeholder ($1, $2, etc.)
        if (sql[i] == '$' && i + 1 < sql.size() && std::isdigit(sql[i + 1])) {
            // Parse parameter number
            size_t j = i + 1;
            int param_num = 0;
            while (j < sql.size() && std::isdigit(sql[j])) {
                param_num = param_num * 10 + (sql[j] - '0');
                ++j;
            }

            // Parameter indices are 1-based, vector is 0-based
            if (param_num > 0 && static_cast<size_t>(param_num) <= params.size()) {
                result += columnValueToSqlLiteral(params[param_num - 1]);
            } else {
                // Invalid parameter number - leave as-is (will cause SQL error)
                result += sql.substr(i, j - i);
            }
            i = j;
        } else if (sql[i] == '\'' && i + 1 < sql.size()) {
            // Skip string literals to avoid replacing $ inside strings
            result += sql[i++];
            while (i < sql.size() && sql[i] != '\'') {
                if (sql[i] == '\'' && i + 1 < sql.size() && sql[i + 1] == '\'') {
                    result += sql[i++];
                }
                result += sql[i++];
            }
            if (i < sql.size()) {
                result += sql[i++];  // Closing quote
            }
        } else if (sql[i] == '"' && i + 1 < sql.size()) {
            // Skip double-quoted identifiers
            result += sql[i++];
            while (i < sql.size() && sql[i] != '"') {
                if (sql[i] == '"' && i + 1 < sql.size() && sql[i + 1] == '"') {
                    result += sql[i++];
                }
                result += sql[i++];
            }
            if (i < sql.size()) {
                result += sql[i++];  // Closing quote
            }
        } else if (sql[i] == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
            // Skip line comments
            while (i < sql.size() && sql[i] != '\n') {
                result += sql[i++];
            }
        } else if (sql[i] == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
            // Skip block comments
            result += sql[i++];
            result += sql[i++];
            while (i + 1 < sql.size() && !(sql[i] == '*' && sql[i + 1] == '/')) {
                result += sql[i++];
            }
            if (i + 1 < sql.size()) {
                result += sql[i++];  // *
                result += sql[i++];  // /
            }
        } else {
            result += sql[i++];
        }
    }

    return result;
}

// ============================================================================
// PreparedStatement Implementation
// ============================================================================

class PreparedStatementImpl {
public:
    std::string sql_;
    uint32_t statement_id_ = 0;
    size_t param_count_ = 0;
    std::vector<protocol::ProtocolCodec::ColumnValue> params_;
    bool valid_ = false;
};

PreparedStatement::PreparedStatement() : impl_(std::make_unique<PreparedStatementImpl>()) {}
PreparedStatement::~PreparedStatement() = default;

PreparedStatement::PreparedStatement(PreparedStatement&& other) noexcept = default;
PreparedStatement& PreparedStatement::operator=(PreparedStatement&& other) noexcept = default;

const std::string& PreparedStatement::getSQL() const {
    return impl_->sql_;
}

size_t PreparedStatement::getParameterCount() const {
    return impl_->param_count_;
}

bool PreparedStatement::isValid() const {
    return impl_->valid_;
}

void PreparedStatement::setNull(size_t index) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
}

void PreparedStatement::setBool(size_t index, bool value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBool(value);
}

void PreparedStatement::setInt16(size_t index, int16_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
}

void PreparedStatement::setInt32(size_t index, int32_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
}

void PreparedStatement::setInt64(size_t index, int64_t value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt64(value);
}

void PreparedStatement::setFloat(size_t index, float value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
}

void PreparedStatement::setDouble(size_t index, double value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
}

void PreparedStatement::setString(size_t index, const std::string& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromString(value);
}

void PreparedStatement::setBytes(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
}

void PreparedStatement::setBytes(size_t index, const uint8_t* data, size_t length) {
    if (index == 0 || index > impl_->params_.size()) return;
    impl_->params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(data, length);
}

void PreparedStatement::setTimestamp(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
}

void PreparedStatement::setDate(size_t index, int32_t days) {
    setInt32(index, days);
}

void PreparedStatement::setTime(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
}

void PreparedStatement::clearParameters() {
    for (auto& p : impl_->params_) {
        p = protocol::ProtocolCodec::ColumnValue(nullptr);
    }
}

// ============================================================================
// Connection Implementation
// ============================================================================

class ConnectionImpl {
public:
    ConnectionConfig config_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    std::string last_error_;

    // IPC client and protocol session
    std::unique_ptr<server::IPCClient> ipc_client_;
    std::unique_ptr<protocol::ProtocolSession> protocol_session_;

    // Session info
    uint8_t session_id_[16] = {0};
    std::string server_version_;
    bool in_transaction_ = false;
    bool auto_commit_ = true;
    std::istream* copy_input_stream_ = nullptr;
    std::ostream* copy_output_stream_ = nullptr;

    // ============================
    // Connection helpers
    // ============================

    core::Status doConnect(core::ErrorContext* ctx) {
        // Check if server is running, with retry for race conditions
        // Two clients might both see no server and both try to start one.
        // The first one wins the database lock, the second one should connect to it.
        if (config_.auto_start_server) {
            int max_retries = 3;
            for (int retry = 0; retry < max_retries; ++retry) {
                if (server::isServerRunning(config_.database_name)) {
                    break;  // Server is running, proceed to connect
                }

                auto status = Connection::startServer(
                    config_.database_name,
                    config_.server_executable,
                    config_.auto_start_timeout_ms,
                    ctx
                );

                if (isOk(status)) {
                    break;  // Server started successfully
                }

                // Server start failed - might be because another client started one
                // Wait briefly and check if a server is now running
                std::this_thread::sleep_for(std::chrono::milliseconds(200 * (retry + 1)));

                if (server::isServerRunning(config_.database_name)) {
                    break;  // Another client started the server, we can connect to it
                }

                // Last retry - report the failure
                if (retry == max_retries - 1) {
                    last_error_ = "Failed to auto-start server (database may be locked by another process)";
                    return status;
                }
            }
        }

        // Create IPC client
        server::IPCClientConfig ipc_config;
        ipc_config.database_name = config_.database_name;
        ipc_config.method = config_.ipc_method;
        ipc_config.tcp_port = config_.tcp_port;
        ipc_config.connect_timeout_ms = config_.connect_timeout_ms;
        ipc_config.read_timeout_ms = config_.read_timeout_ms;
        ipc_config.write_timeout_ms = config_.write_timeout_ms;
        if (!config_.socket_path.empty()) {
            ipc_config.socket_path = config_.socket_path;
        }

        ipc_client_ = server::IPCClient::create(ipc_config, ctx);
        if (!ipc_client_) {
            last_error_ = "Failed to create IPC client";
            return core::Status::CONNECTION_FAILURE;
        }

        // Connect to server
        state_ = ConnectionState::CONNECTING;
        auto status = ipc_client_->connect(ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to connect to server";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        // Create protocol session
        protocol_session_ = std::make_unique<protocol::ProtocolSession>(
            ipc_client_->getConnection()
        );

        // Send CONNECT_REQUEST
        auto connect_msg = protocol::ProtocolCodec::buildConnectRequest(
            config_.database_name,
            "scratchbird_client",
            getpid()
        );

        status = protocol_session_->sendMessage(connect_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send connect request";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        // Receive CONNECT_RESPONSE
        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive connect response";
            state_ = ConnectionState::ERROR_STATE;
            return status;
        }

        if (response.getType() != protocol::MessageType::CONNECT_RESPONSE) {
            last_error_ = "Unexpected response type";
            state_ = ConnectionState::ERROR_STATE;
            return core::Status::PROTOCOL_VIOLATION;
        }

        bool success;
        std::string error_msg;
        status = protocol::ProtocolCodec::parseConnectResponse(
            response, success, session_id_, error_msg, ctx
        );
        if (!isOk(status) || !success) {
            last_error_ = error_msg.empty() ? "Connection refused" : error_msg;
            state_ = ConnectionState::ERROR_STATE;
            return core::Status::CONNECTION_FAILURE;
        }

        // Authenticate if credentials provided
        if (!config_.username.empty() && !config_.manual_auth) {
            status = doAuthenticate(ctx);
            if (!isOk(status)) {
                return status;
            }
        }

        state_ = ConnectionState::CONNECTED;
        auto_commit_ = config_.auto_commit;
        return core::Status::OK;
    }

    core::Status doAuthenticate(core::ErrorContext* ctx) {
        auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            config_.username,
            config_.password
        );

        auto status = protocol_session_->sendMessage(auth_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send auth request";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive auth response";
            return status;
        }

        if (response.getType() != protocol::MessageType::AUTH_RESPONSE) {
            last_error_ = "Unexpected response type";
            return core::Status::PROTOCOL_VIOLATION;
        }

        bool success;
        uint32_t user_id;
        std::string error_msg;
        status = protocol::ProtocolCodec::parseAuthResponse(
            response, success, user_id, error_msg, ctx
        );
        if (!isOk(status) || !success) {
            last_error_ = error_msg.empty() ? "Authentication failed" : error_msg;
            return core::Status::INVALID_PASSWORD;
        }

        return core::Status::OK;
    }

    core::Status doSendAuthRequest(protocol::AuthMethod method,
                                   const std::vector<uint8_t>& payload,
                                   Connection::AuthResponse& response,
                                   core::ErrorContext* ctx) {
        if (!protocol_session_) {
            last_error_ = "Not connected";
            return core::Status::CONNECTION_FAILURE;
        }

        auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
            session_id_,
            config_.username,
            method,
            payload
        );

        auto status = protocol_session_->sendMessage(auth_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send auth request";
            return status;
        }

        protocol::Message response_msg;
        status = protocol_session_->receiveMessage(response_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive auth response";
            return status;
        }

        if (response_msg.getType() != protocol::MessageType::AUTH_RESPONSE) {
            last_error_ = "Unexpected response type";
            return core::Status::PROTOCOL_VIOLATION;
        }

        protocol::AuthStatus auth_status = protocol::AuthStatus::ERROR;
        uint32_t user_id;
        std::string error_msg;
        std::vector<uint8_t> data;
        status = protocol::ProtocolCodec::parseAuthResponse(
            response_msg, auth_status, user_id, error_msg, &data, ctx
        );
        if (!isOk(status)) {
            last_error_ = "Failed to parse auth response";
            return status;
        }

        response.status = auth_status;
        response.user_id = user_id;
        response.error_message = error_msg;
        response.data = std::move(data);
        return core::Status::OK;
    }

    core::Status doExecuteQueryMessage(const protocol::Message& query_msg,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
        if (results) {
            results->impl_->clear();
        }

        auto status = protocol_session_->sendMessage(query_msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send query";
            return status;
        }

        const uint32_t kCopyWindow = 65536;

        auto handle_copy_out = [&]() -> core::Status {
            std::ostream* out = copy_output_stream_ ? copy_output_stream_ : &std::cout;
            uint32_t window = 0;
            bool stream_ready = false;

            while (true) {
                protocol::Message response;
                auto status = protocol_session_->receiveMessage(response, ctx);
                if (!isOk(status)) {
                    last_error_ = "Failed to receive COPY OUT response";
                    return status;
                }

                switch (response.getType()) {
                    case protocol::MessageType::STREAM_READY: {
                        stream_ready = true;
                        window = kCopyWindow;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::START, window, 0);
                        status = protocol_session_->sendMessage(ctrl, ctx);
                        if (!isOk(status)) {
                            last_error_ = "Failed to send STREAM_CONTROL";
                            return status;
                        }
                        break;
                    }
                    case protocol::MessageType::COPY_DATA: {
                        const uint8_t* data = nullptr;
                        size_t len = 0;
                        protocol::ProtocolCodec::parseCopyData(response, &data, &len, ctx);
                        if (len > 0) {
                            out->write(reinterpret_cast<const char*>(data),
                                       static_cast<std::streamsize>(len));
                            if (!(*out)) {
                                last_error_ = "COPY OUT write failed";
                                return core::Status::IO_ERROR;
                            }
                        }
                        if (window > 0) {
                            if (len >= window) {
                                window = 0;
                            } else {
                                window -= static_cast<uint32_t>(len);
                            }
                        }
                        if (stream_ready && window == 0) {
                            window = kCopyWindow;
                            auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                                protocol::StreamControlType::ACK, window, 0);
                            status = protocol_session_->sendMessage(ctrl, ctx);
                            if (!isOk(status)) {
                                last_error_ = "Failed to send STREAM_CONTROL ACK";
                                return status;
                            }
                        }
                        break;
                    }
                    case protocol::MessageType::COPY_DONE:
                        return core::Status::OK;

                    case protocol::MessageType::COPY_FAIL: {
                        std::string message;
                        protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                        last_error_ = message.empty() ? "COPY OUT failed" : message;
                        return core::Status::INTERNAL_ERROR;
                    }
                    case protocol::MessageType::QUERY_ERROR: {
                        uint32_t error_code;
                        std::string sqlstate, message, detail, hint;
                        protocol::ProtocolCodec::parseQueryError(
                            response, error_code, sqlstate, message, detail, hint, ctx
                        );
                        last_error_ = message;
                        if (!detail.empty()) last_error_ += " (" + detail + ")";
                        return static_cast<core::Status>(error_code);
                    }
                    case protocol::MessageType::STREAM_END:
                        break;

                    default:
                        break;
                }
            }
        };

        auto handle_copy_in = [&]() -> core::Status {
            std::istream* in = copy_input_stream_ ? copy_input_stream_ : &std::cin;
            uint32_t window = 0;
            bool done = false;

            while (!done) {
                if (window == 0) {
                    protocol::Message response;
                    auto status = protocol_session_->receiveMessage(response, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to receive COPY IN control";
                        return status;
                    }

                    switch (response.getType()) {
                        case protocol::MessageType::STREAM_READY:
                            break;
                        case protocol::MessageType::STREAM_CONTROL: {
                            protocol::StreamControlType control;
                            uint32_t new_window = 0;
                            uint32_t timeout_ms = 0;
                            status = protocol::ProtocolCodec::parseStreamControl(
                                response, control, new_window, timeout_ms, ctx);
                            if (!isOk(status)) {
                                last_error_ = "Malformed STREAM_CONTROL";
                                return status;
                            }
                            (void)timeout_ms;
                            if (control == protocol::StreamControlType::PAUSE) {
                                break;
                            }
                            if (control == protocol::StreamControlType::CANCEL) {
                                last_error_ = "COPY IN canceled by server";
                                return core::Status::CANCELLED;
                            }
                            window += new_window;
                            break;
                        }
                        case protocol::MessageType::COPY_FAIL: {
                            std::string message;
                            protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                            last_error_ = message.empty() ? "COPY IN failed" : message;
                            return core::Status::INTERNAL_ERROR;
                        }
                        case protocol::MessageType::QUERY_ERROR: {
                            uint32_t error_code;
                            std::string sqlstate, message, detail, hint;
                            protocol::ProtocolCodec::parseQueryError(
                                response, error_code, sqlstate, message, detail, hint, ctx
                            );
                            last_error_ = message;
                            if (!detail.empty()) last_error_ += " (" + detail + ")";
                            return static_cast<core::Status>(error_code);
                        }
                        default:
                            break;
                    }
                    if (window == 0) {
                        continue;
                    }
                }

                size_t to_read = std::min<size_t>(window, 16384);
                std::string buffer(to_read, '\0');
                in->read(buffer.data(), static_cast<std::streamsize>(to_read));
                std::streamsize got = in->gcount();

                if (got > 0) {
                    auto msg = protocol::ProtocolCodec::buildCopyData(
                        reinterpret_cast<const uint8_t*>(buffer.data()),
                        static_cast<size_t>(got));
                    auto status = protocol_session_->sendMessage(msg, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send COPY_DATA";
                        return status;
                    }
                    if (got >= static_cast<std::streamsize>(window)) {
                        window = 0;
                    } else {
                        window -= static_cast<uint32_t>(got);
                    }
                } else {
                    auto msg = protocol::ProtocolCodec::buildCopyDone();
                    auto status = protocol_session_->sendMessage(msg, ctx);
                    if (!isOk(status)) {
                        last_error_ = "Failed to send COPY_DONE";
                        return status;
                    }
                    done = true;
                }
            }
            return core::Status::OK;
        };

        while (true) {
            protocol::Message response;
            status = protocol_session_->receiveMessage(response, ctx);
            if (!isOk(status)) {
                last_error_ = "Failed to receive response";
                return status;
            }

            switch (response.getType()) {
                case protocol::MessageType::QUERY_ERROR: {
                    uint32_t error_code;
                    std::string sqlstate, message, detail, hint;
                    protocol::ProtocolCodec::parseQueryError(
                        response, error_code, sqlstate, message, detail, hint, ctx
                    );
                    last_error_ = message;
                    if (!detail.empty()) last_error_ += " (" + detail + ")";
                    return static_cast<core::Status>(error_code);
                }

                case protocol::MessageType::ROW_DESCRIPTION: {
                    if (results) {
                        std::vector<protocol::ProtocolCodec::ColumnInfo> cols;
                        protocol::ProtocolCodec::parseRowDescription(response, cols, ctx);
                        results->impl_->columns_.clear();
                        for (size_t i = 0; i < cols.size(); ++i) {
                            ColumnMeta meta;
                            meta.name = cols[i].name;
                            meta.type = cols[i].type;
                            meta.type_modifier = cols[i].type_modifier;
                            meta.index = i;
                            results->impl_->columns_.push_back(meta);
                        }
                    }
                    break;
                }

                case protocol::MessageType::ROW_DATA: {
                    if (results) {
                        std::vector<protocol::ProtocolCodec::ColumnValue> values;
                        protocol::ProtocolCodec::parseRowData(response, values, ctx);
                        results->impl_->rows_.push_back(std::move(values));
                    }
                    break;
                }

                case protocol::MessageType::COMMAND_COMPLETE: {
                    if (results) {
                        std::string tag;
                        int64_t rows_affected;
                        protocol::ProtocolCodec::parseCommandComplete(
                            response, tag, rows_affected, ctx
                        );
                        results->impl_->command_tag_ = tag;
                        results->impl_->rows_affected_ = rows_affected;
                    }
                    break;
                }

                case protocol::MessageType::END_OF_RESULTS: {
                    if (results) {
                        results->impl_->row_count_ = static_cast<int64_t>(results->impl_->rows_.size());
                    }
                    return core::Status::OK;
                }

                case protocol::MessageType::COPY_IN_RESPONSE: {
                    status = handle_copy_in();
                    if (!isOk(status)) {
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_OUT_RESPONSE: {
                    status = handle_copy_out();
                    if (!isOk(status)) {
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_FAIL: {
                    std::string message;
                    protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                    last_error_ = message.empty() ? "COPY failed" : message;
                    return core::Status::INTERNAL_ERROR;
                }
                case protocol::MessageType::STREAM_END:
                case protocol::MessageType::STREAM_READY:
                    break;

                case protocol::MessageType::TRANSACTION_STATUS: {
                    // NET-L1: Parse transaction status and update connection state
                    if (response.getPayloadSize() >= sizeof(protocol::TransactionStatusPayload)) {
                        const auto* ts_payload = reinterpret_cast<const protocol::TransactionStatusPayload*>(
                            response.getPayload());
                        // Status: 0 = idle, 1 = in transaction, 2 = failed
                        in_transaction_ = (ts_payload->status == 1);
                        if (ts_payload->status == 2) {
                            // Transaction failed - reset to idle
                            state_ = ConnectionState::CONNECTED;
                        } else if (ts_payload->status == 1) {
                            state_ = ConnectionState::IN_TRANSACTION;
                        } else {
                            state_ = ConnectionState::CONNECTED;
                        }
                    }
                    break;
                }

                default:
                    // Ignore unexpected messages
                    break;
            }
        }
    }

    core::Status doExecuteQuery(const std::string& sql, ResultSet* results,
                                core::ErrorContext* ctx) {
        auto query_msg = protocol::ProtocolCodec::buildQuery(session_id_, sql, 0);
        return doExecuteQueryMessage(query_msg, results, ctx);
    }

    core::Status doExecuteBytecode(const std::vector<uint8_t>& bytecode,
                                   const std::string& sql,
                                   ResultSet* results,
                                   core::ErrorContext* ctx) {
        auto query_msg = protocol::ProtocolCodec::buildQueryBytecode(session_id_, bytecode, sql, 0);
        return doExecuteQueryMessage(query_msg, results, ctx);
    }

    core::Status doBeginTransaction(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildBeginTransaction(session_id_, 0, false);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send BEGIN";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = true;
        state_ = ConnectionState::IN_TRANSACTION;
        return core::Status::OK;
    }

    core::Status doCommit(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildCommit(session_id_);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send COMMIT";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = false;
        state_ = ConnectionState::CONNECTED;
        return core::Status::OK;
    }

    core::Status doRollback(core::ErrorContext* ctx) {
        auto msg = protocol::ProtocolCodec::buildRollback(session_id_);
        auto status = protocol_session_->sendMessage(msg, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to send ROLLBACK";
            return status;
        }

        protocol::Message response;
        status = protocol_session_->receiveMessage(response, ctx);
        if (!isOk(status)) {
            last_error_ = "Failed to receive response";
            return status;
        }

        if (response.getType() == protocol::MessageType::QUERY_ERROR) {
            uint32_t code;
            std::string sqlstate, message, detail, hint;
            protocol::ProtocolCodec::parseQueryError(
                response, code, sqlstate, message, detail, hint, ctx
            );
            last_error_ = message;
            return static_cast<core::Status>(code);
        }

        in_transaction_ = false;
        state_ = ConnectionState::CONNECTED;
        return core::Status::OK;
    }
};

// ============================================================================
// Connection Public API
// ============================================================================

Connection::Connection() : impl_(std::make_unique<ConnectionImpl>()) {}
Connection::~Connection() {
    if (impl_) {
        disconnect();
    }
}

Connection::Connection(Connection&& other) noexcept = default;
Connection& Connection::operator=(Connection&& other) noexcept = default;

core::Status Connection::connect(const std::string& database,
                                 const std::string& username,
                                 const std::string& password,
                                 core::ErrorContext* ctx) {
    ConnectionConfig config;
    config.database_name = database;
    config.username = username;
    config.password = password;
    return connect(config, ctx);
}

core::Status Connection::connect(const ConnectionConfig& config,
                                 core::ErrorContext* ctx) {
    if (isConnected()) {
        disconnect();
    }

    impl_->config_ = config;
    return impl_->doConnect(ctx);
}

void Connection::disconnect() {
    if (!isConnected()) return;

    // Rollback any active transaction
    if (impl_->in_transaction_) {
        rollback();
    }

    // Send disconnect message
    if (impl_->protocol_session_) {
        auto msg = protocol::ProtocolCodec::buildDisconnect();
        impl_->protocol_session_->sendMessage(msg);
    }

    // Close IPC client
    if (impl_->ipc_client_) {
        impl_->ipc_client_->disconnect();
    }

    impl_->protocol_session_.reset();
    impl_->ipc_client_.reset();
    impl_->state_ = ConnectionState::DISCONNECTED;
    impl_->in_transaction_ = false;
}

bool Connection::isConnected() const {
    if (!impl_) return false;
    return impl_->state_ == ConnectionState::CONNECTED ||
           impl_->state_ == ConnectionState::IN_TRANSACTION;
}

ConnectionState Connection::getState() const {
    if (!impl_) return ConnectionState::DISCONNECTED;
    return impl_->state_;
}

std::string Connection::getLastError() const {
    return impl_->last_error_;
}

core::Status Connection::ping(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    auto msg = protocol::ProtocolCodec::buildPing(timestamp, 0);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send ping";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive pong";
        return status;
    }

    if (response.getType() != protocol::MessageType::PONG) {
        impl_->last_error_ = "Unexpected response to ping";
        return core::Status::PROTOCOL_VIOLATION;
    }

    return core::Status::OK;
}

void Connection::setCopyInputStream(std::istream* in) {
    if (!impl_) {
        return;
    }
    impl_->copy_input_stream_ = in;
}

void Connection::setCopyOutputStream(std::ostream* out) {
    if (!impl_) {
        return;
    }
    impl_->copy_output_stream_ = out;
}

core::Status Connection::sendAuthRequest(protocol::AuthMethod method,
                                         const std::vector<uint8_t>& payload,
                                         AuthResponse& response,
                                         core::ErrorContext* ctx) {
    if (!impl_) {
        return core::Status::INVALID_ARGUMENT;
    }

    if (impl_->state_ != ConnectionState::CONNECTED &&
        impl_->state_ != ConnectionState::CONNECTING) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doSendAuthRequest(method, payload, response, ctx);
}

core::Status Connection::executeQuery(const std::string& sql,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doExecuteQuery(sql, results, ctx);
}

core::Status Connection::executeBytecode(const std::vector<uint8_t>& bytecode,
                                         const std::string& sql,
                                         ResultSet* results,
                                         core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doExecuteBytecode(bytecode, sql, results, ctx);
}

core::Status Connection::executeBytecode(const std::vector<uint8_t>& bytecode,
                                         ResultSet* results,
                                         core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    return impl_->doExecuteBytecode(bytecode, std::string(), results, ctx);
}

core::Status Connection::execute(const std::string& sql,
                                  int64_t* rows_affected,
                                  core::ErrorContext* ctx) {
    ResultSet results;
    auto status = executeQuery(sql, &results, ctx);
    if (isOk(status) && rows_affected) {
        *rows_affected = results.getRowsAffected();
    }
    return status;
}

core::Status Connection::prepare(const std::string& sql,
                                  PreparedStatement* stmt,
                                  core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    // Count parameters ($1, $2, ... or ?)
    size_t param_count = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == '?') {
            ++param_count;
        } else if (sql[i] == '$' && i + 1 < sql.size() && std::isdigit(sql[i + 1])) {
            size_t num = 0;
            size_t j = i + 1;
            while (j < sql.size() && std::isdigit(sql[j])) {
                num = num * 10 + (sql[j] - '0');
                ++j;
            }
            if (num > param_count) param_count = num;
        }
    }

    stmt->impl_->sql_ = sql;
    stmt->impl_->param_count_ = param_count;
    stmt->impl_->params_.resize(param_count);
    stmt->impl_->valid_ = true;

    // Note: In a full implementation, we would send PREPARE to server
    // For now, we prepare client-side and substitute parameters

    return core::Status::OK;
}

core::Status Connection::executeQuery(PreparedStatement& stmt,
                                       ResultSet* results,
                                       core::ErrorContext* ctx) {
    if (!stmt.isValid()) {
        impl_->last_error_ = "Invalid prepared statement";
        return core::Status::INVALID_ARGUMENT;
    }

    // NET-1: Parameter substitution with proper escaping for SQL injection prevention
    // Substitute $1, $2, etc. with escaped parameter values
    std::string sql = substituteParameters(stmt.impl_->sql_, stmt.impl_->params_);

    return executeQuery(sql, results, ctx);
}

core::Status Connection::execute(PreparedStatement& stmt,
                                  int64_t* rows_affected,
                                  core::ErrorContext* ctx) {
    ResultSet results;
    auto status = executeQuery(stmt, &results, ctx);
    if (isOk(status) && rows_affected) {
        *rows_affected = results.getRowsAffected();
    }
    return status;
}

void Connection::closeStatement(PreparedStatement& stmt) {
    stmt.impl_->valid_ = false;
    stmt.impl_->params_.clear();
}

core::Status Connection::beginTransaction(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (impl_->in_transaction_) {
        impl_->last_error_ = "Already in transaction";
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    return impl_->doBeginTransaction(ctx);
}

core::Status Connection::commit(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (!impl_->in_transaction_) {
        impl_->last_error_ = "Not in transaction";
        return core::Status::INVALID_TRANSACTION_STATE;
    }

    return impl_->doCommit(ctx);
}

core::Status Connection::rollback(core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    if (!impl_->in_transaction_) {
        // Silently succeed if not in transaction
        return core::Status::OK;
    }

    return impl_->doRollback(ctx);
}

core::Status Connection::savepoint(const std::string& name,
                                    core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildSavepoint(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send SAVEPOINT";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    return core::Status::OK;
}

core::Status Connection::releaseSavepoint(const std::string& name,
                                           core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildReleaseSavepoint(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send RELEASE SAVEPOINT";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    // NET-M2: Proper response validation
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(code);
    }

    return core::Status::OK;
}

core::Status Connection::rollbackTo(const std::string& name,
                                     core::ErrorContext* ctx) {
    if (!isConnected()) {
        impl_->last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto msg = protocol::ProtocolCodec::buildRollbackTo(impl_->session_id_, name);
    auto status = impl_->protocol_session_->sendMessage(msg, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to send ROLLBACK TO";
        return status;
    }

    protocol::Message response;
    status = impl_->protocol_session_->receiveMessage(response, ctx);
    if (!isOk(status)) {
        impl_->last_error_ = "Failed to receive response";
        return status;
    }

    // NET-M3: Proper response validation
    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        uint32_t code;
        std::string sqlstate, message, detail, hint;
        protocol::ProtocolCodec::parseQueryError(
            response, code, sqlstate, message, detail, hint, ctx
        );
        impl_->last_error_ = message;
        return static_cast<core::Status>(code);
    }

    return core::Status::OK;
}

bool Connection::inTransaction() const {
    return impl_->in_transaction_;
}

void Connection::setAutoCommit(bool enabled) {
    impl_->auto_commit_ = enabled;
}

bool Connection::getAutoCommit() const {
    return impl_->auto_commit_;
}

std::string Connection::getDatabaseName() const {
    return impl_->config_.database_name;
}

std::string Connection::getUsername() const {
    return impl_->config_.username;
}

std::string Connection::getServerVersion() const {
    return impl_->server_version_;
}

const ConnectionConfig& Connection::getConfig() const {
    return impl_->config_;
}

// ============================================================================
// Static Server Control
// ============================================================================

core::Status Connection::startServer(const std::string& database_name,
                                       const std::string& server_path,
                                       uint32_t timeout_ms,
                                       core::ErrorContext* ctx) {
    if (isServerRunning(database_name)) {
        return core::Status::OK;  // Already running
    }

    // Find sb_server executable
    std::string exe_path = server_path;
    if (exe_path.empty()) {
        // Search common locations
        const char* path_env = std::getenv("PATH");
        if (path_env) {
            std::string path_str = path_env;
            std::stringstream ss(path_str);
            std::string dir;
            while (std::getline(ss, dir, ':')) {
                std::string candidate = dir + "/sb_server";
                std::ifstream f(candidate);
                if (f.good()) {
                    exe_path = candidate;
                    break;
                }
            }
        }

        // Also check relative to current directory
        if (exe_path.empty()) {
            std::ifstream f("./sb_server");
            if (f.good()) exe_path = "./sb_server";
        }
        if (exe_path.empty()) {
            std::ifstream f("./build/bin/sb_server");
            if (f.good()) exe_path = "./build/bin/sb_server";
        }
    }

    if (exe_path.empty()) {
        if (ctx) ctx->message = "sb_server not found in PATH";
        return core::Status::NOT_FOUND;
    }

#ifdef _WIN32
    // Windows implementation
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    std::string cmdline = exe_path + " --database=" + database_name + " --daemon";

    if (!CreateProcess(NULL, const_cast<char*>(cmdline.c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        if (ctx) ctx->message = "Failed to start server process";
        return core::Status::INTERNAL_ERROR;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // Unix implementation
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - redirect output to /dev/null
        int null_fd = open("/dev/null", O_WRONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDOUT_FILENO);
            dup2(null_fd, STDERR_FILENO);
            close(null_fd);
        }

        // Create new session (detach from parent)
        setsid();

        // Execute server
        execl(exe_path.c_str(), "sb_server",
              "--database", database_name.c_str(),
              "--daemon", nullptr);

        // execl failed
        _exit(1);
    } else if (pid < 0) {
        if (ctx) ctx->message = "fork() failed";
        return core::Status::INTERNAL_ERROR;
    }
#endif

    // Wait for server to start (poll PID file)
    // Note: If another client also tried to start a server for the same database,
    // our forked process will fail to acquire the database lock and exit.
    // We detect this by checking if a different server (different PID) started.
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        if (isServerRunning(database_name)) {
            // Server is running - could be ours or another client's
            // Either way, we can connect to it
            return core::Status::OK;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        if (static_cast<uint32_t>(elapsed) >= timeout_ms) {
            // Our server didn't start - check if our child process is still running
            // If not, it might have failed due to database lock conflict
            if (ctx) ctx->message = "Server failed to start within timeout (database may be locked by another process)";
            return core::Status::LOCK_TIMEOUT;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

core::Status Connection::stopServer(const std::string& database_name,
                                     core::ErrorContext* ctx) {
    int32_t pid = getServerPID(database_name);
    if (pid <= 0) {
        return core::Status::OK;  // Not running
    }

#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
#else
    kill(pid, SIGTERM);
#endif

    // Wait for server to stop
    for (int i = 0; i < 50; ++i) {
        if (!isServerRunning(database_name)) {
            return core::Status::OK;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Force kill
#ifndef _WIN32
    kill(pid, SIGKILL);
#endif

    return core::Status::OK;
}

bool Connection::isServerRunning(const std::string& database_name) {
    return server::isServerRunning(database_name);
}

int32_t Connection::getServerPID(const std::string& database_name) {
    std::string pid_path = server::getPIDFilePath(database_name);
    std::ifstream f(pid_path);
    if (!f.is_open()) return 0;

    int32_t pid = 0;
    f >> pid;
    return pid;
}

// ============================================================================
// PooledConnection Implementation
// ============================================================================

PooledConnection::PooledConnection(Connection* conn, std::function<void(Connection*)> returner)
    : connection_(conn), returner_(std::move(returner)) {}

PooledConnection::~PooledConnection() {
    if (connection_ && returner_) {
        returner_(connection_);
    }
}

PooledConnection::PooledConnection(PooledConnection&& other) noexcept
    : connection_(other.connection_), returner_(std::move(other.returner_)) {
    other.connection_ = nullptr;
}

PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept {
    if (this != &other) {
        if (connection_ && returner_) {
            returner_(connection_);
        }
        connection_ = other.connection_;
        returner_ = std::move(other.returner_);
        other.connection_ = nullptr;
    }
    return *this;
}

// ============================================================================
// ConnectionPool Implementation
// ============================================================================

class ConnectionPool::PoolImpl {
public:
    PoolConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::unique_ptr<Connection>> available_;
    std::atomic<uint32_t> in_use_{0};
    std::atomic<uint32_t> total_{0};
    std::atomic<uint64_t> acquisitions_{0};
    std::atomic<uint64_t> releases_{0};
    std::atomic<uint64_t> waits_{0};
    std::atomic<bool> shutdown_{false};

    void returnConnection(Connection* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        --in_use_;
        ++releases_;

        if (!shutdown_ && conn->isConnected()) {
            available_.push(std::unique_ptr<Connection>(conn));
            cv_.notify_one();
        } else {
            delete conn;
            --total_;
        }
    }
};

ConnectionPool::ConnectionPool(const PoolConfig& config)
    : impl_(std::make_unique<PoolImpl>()) {
    impl_->config_ = config;

    // Pre-create minimum connections
    for (uint32_t i = 0; i < config.min_connections; ++i) {
        auto conn = std::make_unique<Connection>();
        if (isOk(conn->connect(config.connection_config))) {
            impl_->available_.push(std::move(conn));
            ++impl_->total_;
        }
    }
}

ConnectionPool::~ConnectionPool() {
    shutdown();
}

PooledConnection ConnectionPool::acquire(core::ErrorContext* ctx) {
    std::unique_lock<std::mutex> lock(impl_->mutex_);
    ++impl_->acquisitions_;

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(impl_->config_.acquire_timeout_ms);

    while (impl_->available_.empty()) {
        // Can we create a new connection?
        if (impl_->total_ < impl_->config_.max_connections) {
            ++impl_->total_;
            lock.unlock();

            auto conn = std::make_unique<Connection>();
            if (isOk(conn->connect(impl_->config_.connection_config, ctx))) {
                ++impl_->in_use_;
                return PooledConnection(conn.release(),
                    [this](Connection* c) { impl_->returnConnection(c); });
            }

            lock.lock();
            --impl_->total_;
        }

        // Wait for a connection to be returned
        ++impl_->waits_;
        if (impl_->cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            if (ctx) ctx->message = "Connection pool timeout";
            return PooledConnection(nullptr, nullptr);
        }
    }

    auto conn = std::move(impl_->available_.front());
    impl_->available_.pop();
    ++impl_->in_use_;

    // Validate connection if configured
    if (impl_->config_.validate_on_acquire) {
        lock.unlock();
        if (!isOk(conn->ping())) {
            // Connection is dead, try to reconnect
            conn->disconnect();
            if (!isOk(conn->connect(impl_->config_.connection_config, ctx))) {
                lock.lock();
                --impl_->in_use_;
                --impl_->total_;
                return acquire(ctx);  // Try again
            }
        }
    } else {
        lock.unlock();
    }

    return PooledConnection(conn.release(),
        [this](Connection* c) { impl_->returnConnection(c); });
}

ConnectionPool::Stats ConnectionPool::getStats() const {
    Stats stats;
    stats.total_connections = impl_->total_;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        stats.available_connections = static_cast<uint32_t>(impl_->available_.size());
    }
    stats.in_use_connections = impl_->in_use_;
    stats.total_acquisitions = impl_->acquisitions_;
    stats.total_releases = impl_->releases_;
    stats.wait_count = impl_->waits_;
    return stats;
}

void ConnectionPool::shutdown() {
    impl_->shutdown_ = true;

    std::lock_guard<std::mutex> lock(impl_->mutex_);
    while (!impl_->available_.empty()) {
        impl_->available_.pop();
        --impl_->total_;
    }
    impl_->cv_.notify_all();
}

} // namespace client
} // namespace scratchbird
