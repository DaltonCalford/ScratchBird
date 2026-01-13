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

    network::SSLMode ssl_mode{network::SSLMode::REQUIRE};
    std::string ssl_cert;
    std::string ssl_key;
    std::string ssl_root_cert;
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

} // namespace client
} // namespace scratchbird
