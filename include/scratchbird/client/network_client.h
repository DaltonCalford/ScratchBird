#pragma once

/**
 * ScratchBird Network Client (libscratchbird)
 *
 * Alpha: Native protocol over network listener (parser bridge required).
 */

#include <cstdint>
#include <string>
#include <vector>
#include <istream>
#include <ostream>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/network/network.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "scratchbird/security/tls_config.h"

namespace scratchbird {
namespace client {

struct NetworkClientConfig {
    std::string host{"127.0.0.1"};
    uint16_t port{network::DEFAULT_NATIVE_PORT};
    std::string database;
    std::string username;
    std::string password;
    std::string application_name{"scratchbird_odbc"};

    uint32_t connect_timeout_ms{network::DEFAULT_CONNECT_TIMEOUT_MS};
    uint32_t read_timeout_ms{network::DEFAULT_READ_TIMEOUT_MS};
    uint32_t write_timeout_ms{network::DEFAULT_WRITE_TIMEOUT_MS};
    uint32_t copy_window_bytes{65536};
    uint32_t copy_chunk_bytes{16384};

    network::SSLMode ssl_mode{network::SSLMode::REQUIRE};
    std::string ssl_cert;
    std::string ssl_key;
    std::string ssl_root_cert;

    protocol::AuthMethod auth_method{protocol::AuthMethod::SCRAM_SHA_256};
    bool allow_password_fallback{false};
};

struct NetworkColumn {
    std::string name;
    protocol::WireType type{protocol::WireType::UNKNOWN};
    uint32_t type_modifier{0};
};

struct NetworkResultSet {
    std::vector<NetworkColumn> columns;
    std::vector<std::vector<protocol::ProtocolCodec::ColumnValue>> rows;
    int64_t rows_affected{0};
    std::string command_tag;
};

class NetworkPreparedStatement {
public:
    NetworkPreparedStatement();
    ~NetworkPreparedStatement();

    NetworkPreparedStatement(NetworkPreparedStatement&& other) noexcept;
    NetworkPreparedStatement& operator=(NetworkPreparedStatement&& other) noexcept;
    NetworkPreparedStatement(const NetworkPreparedStatement&) = delete;
    NetworkPreparedStatement& operator=(const NetworkPreparedStatement&) = delete;

    size_t getParameterCount() const;
    bool isValid() const;
    void clearParameters();

    void setNull(size_t index);
    void setBool(size_t index, bool value);
    void setInt16(size_t index, int16_t value);
    void setInt32(size_t index, int32_t value);
    void setInt64(size_t index, int64_t value);
    void setFloat(size_t index, float value);
    void setDouble(size_t index, double value);
    void setString(size_t index, const std::string& value);
    void setBytes(size_t index, const std::vector<uint8_t>& value);
    void setBytes(size_t index, const uint8_t* data, size_t length);
    void setTimestamp(size_t index, int64_t microseconds);
    void setDate(size_t index, int32_t days);
    void setTime(size_t index, int64_t microseconds);
    void setUUID(size_t index, const std::vector<uint8_t>& value);
    void setUUID(size_t index, const std::string& value);
    void setNull(size_t index, protocol::WireType type);

private:
    friend class NetworkClient;
    std::string sql_;
    size_t param_count_{0};
    std::vector<protocol::ProtocolCodec::ColumnValue> params_;
    std::vector<protocol::WireType> param_types_;
    bool valid_{false};
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    core::Status connect(const NetworkClientConfig& config,
                         core::ErrorContext* ctx = nullptr);
    void disconnect();

    bool isConnected() const;
    const std::string& lastError() const { return last_error_; }

    core::Status executeQuery(const std::string& sql,
                              NetworkResultSet& results,
                              core::ErrorContext* ctx = nullptr);
    core::Status prepare(const std::string& sql,
                         NetworkPreparedStatement& stmt,
                         core::ErrorContext* ctx = nullptr);
    core::Status executePrepared(NetworkPreparedStatement& stmt,
                                 NetworkResultSet& results,
                                 core::ErrorContext* ctx = nullptr);

    core::Status beginTransaction(core::ErrorContext* ctx = nullptr);
    core::Status commit(core::ErrorContext* ctx = nullptr);
    core::Status rollback(core::ErrorContext* ctx = nullptr);

    void setCopyInputStream(std::istream* in) { copy_input_stream_ = in; }
    void setCopyOutputStream(std::ostream* out) { copy_output_stream_ = out; }

private:
    core::Status sendMessage(const protocol::Message& msg,
                             core::ErrorContext* ctx = nullptr);
    core::Status receiveMessage(protocol::Message& msg,
                                core::ErrorContext* ctx = nullptr);
    core::Status readExactWithTimeout(void* buffer, size_t size,
                                      core::ErrorContext* ctx = nullptr);

    NetworkClientConfig config_{};
    std::unique_ptr<network::Socket> socket_{};
    std::unique_ptr<security::TLSContext> tls_ctx_{};
    std::unique_ptr<security::TLSConnection> tls_conn_{};
    bool tls_active_{false};
    uint8_t session_id_[protocol::SESSION_ID_SIZE] = {0};

    bool connected_{false};
    bool in_transaction_{false};
    std::string last_error_;

    std::istream* copy_input_stream_{nullptr};
    std::ostream* copy_output_stream_{nullptr};
};

void applyDriverDefaultsFromEnv(NetworkClientConfig& config);

} // namespace client
} // namespace scratchbird
