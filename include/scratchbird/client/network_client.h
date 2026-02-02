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
#include "scratchbird/core/lsm_compression.h"
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
    bool enable_compression{false};
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
    core::Status prepareServerStatement(const std::string& sql,
                                        uint32_t& stmt_id,
                                        core::ErrorContext* ctx = nullptr);
    core::Status executeServerStatement(uint32_t stmt_id,
                                        const std::vector<protocol::ProtocolCodec::ColumnValue>& params,
                                        NetworkResultSet& results,
                                        uint32_t max_rows,
                                        bool backward,
                                        bool* portal_suspended_out,
                                        core::ErrorContext* ctx = nullptr);
    core::Status closeServerStatement(uint32_t stmt_id,
                                      core::ErrorContext* ctx = nullptr);
    core::Status sendQueryCancel(core::ErrorContext* ctx = nullptr);
    core::Status subscribeNotifications(uint8_t subscribe_type,
                                        const std::string& channel,
                                        const std::string& filter,
                                        core::ErrorContext* ctx = nullptr);
    core::Status unsubscribeNotifications(const std::string& channel,
                                          core::ErrorContext* ctx = nullptr);

    struct QueryProgressSnapshot {
        uint64_t rows_processed = 0;
        uint64_t bytes_processed = 0;
        uint64_t updated_at_micros = 0;
    };

    void resetQueryProgress();
    QueryProgressSnapshot queryProgress() const { return query_progress_; }

    struct Notification {
        uint32_t process_id = 0;
        std::string channel;
        std::vector<uint8_t> payload;
        uint8_t change_type = 0;
        uint64_t row_id = 0;
    };

    void drainNotifications(std::vector<Notification>& out);

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
    QueryProgressSnapshot query_progress_{};
    std::vector<Notification> notifications_;

    bool connected_{false};
    bool in_transaction_{false};
    std::string last_error_;

    std::istream* copy_input_stream_{nullptr};
    std::ostream* copy_output_stream_{nullptr};

    bool compression_enabled_{false};
    std::unique_ptr<core::Compressor> wire_compressor_{};
};

void applyDriverDefaultsFromEnv(NetworkClientConfig& config);

} // namespace client
} // namespace scratchbird
