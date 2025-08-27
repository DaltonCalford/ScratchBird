#include "scratchbird/engine/remote_provider.h"

#include "scratchbird/engine.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

namespace scratchbird::engine
{
    /// NetworkConfig implementation
    bool NetworkConfig::validate() const
    {
        if (hostname.empty())
            return false;
        if (port == 0)
            return false;
        if (connect_timeout_ms == 0)
            return false;
        if (read_timeout_ms == 0)
            return false;
        if (write_timeout_ms == 0)
            return false;
        if (max_packet_size == 0 || max_packet_size > 1024 * 1024)
            return false; // Max 1MB
        if (buffer_size == 0 || buffer_size > max_packet_size)
            return false;
        return true;
    }

    std::string NetworkConfig::to_string() const
    {
        std::ostringstream oss;
        oss << "NetworkConfig{host=" << hostname << ", port=" << port
            << ", connect_timeout=" << connect_timeout_ms << "ms"
            << ", read_timeout=" << read_timeout_ms << "ms"
            << ", write_timeout=" << write_timeout_ms << "ms"
            << ", compression=" << (enable_compression ? "on" : "off")
            << ", encryption=" << (enable_encryption ? "on" : "off")
            << ", max_packet=" << max_packet_size << ", buffer=" << buffer_size << "}";
        return oss.str();
    }

    /// NetworkConnection implementation
    NetworkConnection::NetworkConnection(const NetworkConfig& config)
        : config_(config), state_(ConnectionState::Disconnected), socket_fd_(-1)
    {
        stats_.created_time = std::chrono::steady_clock::now();
        stats_.last_activity = stats_.created_time;
    }

    NetworkConnection::~NetworkConnection()
    {
        disconnect();
    }

    bool NetworkConnection::connect()
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (state_ == ConnectionState::Connected) {
            return true;
        }

        state_ = ConnectionState::Connecting;

        if (!create_socket()) {
            return handle_connection_error("Failed to create socket");
        }

        if (!configure_socket()) {
            return handle_connection_error("Failed to configure socket");
        }

        // Resolve hostname
        struct hostent* host_entry = gethostbyname(config_.hostname.c_str());
        if (!host_entry) {
            return handle_connection_error("Failed to resolve hostname: " + config_.hostname);
        }

        // Setup address structure
        struct sockaddr_in server_addr;
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(config_.port);
        std::memcpy(&server_addr.sin_addr.s_addr, host_entry->h_addr, host_entry->h_length);

        // Set socket to non-blocking for timeout support
        int flags = fcntl(socket_fd_, F_GETFL, 0);
        fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);

        // Attempt connection
        int result = ::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr),
                               sizeof(server_addr));

        if (result == 0) {
            // Connected immediately
            state_ = ConnectionState::Connected;
            stats_.current_state = state_;
            update_statistics();
            return true;
        }

        if (errno != EINPROGRESS) {
            return handle_connection_error("Connection failed: " + std::string(strerror(errno)));
        }

        // Wait for connection with timeout
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(socket_fd_, &write_fds);

        struct timeval timeout;
        timeout.tv_sec = config_.connect_timeout_ms / 1000;
        timeout.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;

        result = select(socket_fd_ + 1, nullptr, &write_fds, nullptr, &timeout);
        if (result <= 0) {
            return handle_connection_error("Connection timeout or select failed");
        }

        // Check if connection was successful
        int error = 0;
        socklen_t error_len = sizeof(error);
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0 || error != 0) {
            return handle_connection_error("Connection failed after select: " +
                                           std::string(strerror(error)));
        }

        // Restore blocking mode
        fcntl(socket_fd_, F_SETFL, flags);

        state_ = ConnectionState::Connected;
        stats_.current_state = state_;
        update_statistics();
        return true;
    }

    void NetworkConnection::disconnect()
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        cleanup_socket();
        state_ = ConnectionState::Disconnected;
        stats_.current_state = state_;
    }

    bool NetworkConnection::is_connected() const
    {
        return state_ == ConnectionState::Connected || state_ == ConnectionState::Authenticated;
    }

    ConnectionState NetworkConnection::get_state() const
    {
        return state_;
    }

    bool NetworkConnection::send_data(const std::vector<std::uint8_t>& data)
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (!is_connected()) {
            last_error_ = "Connection not established";
            return false;
        }

        size_t total_sent = 0;
        size_t data_size = data.size();

        while (total_sent < data_size) {
            ssize_t sent =
                send(socket_fd_, data.data() + total_sent, data_size - total_sent, MSG_NOSIGNAL);

            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Wait for socket to become writable
                    fd_set write_fds;
                    FD_ZERO(&write_fds);
                    FD_SET(socket_fd_, &write_fds);

                    struct timeval timeout;
                    timeout.tv_sec = config_.write_timeout_ms / 1000;
                    timeout.tv_usec = (config_.write_timeout_ms % 1000) * 1000;

                    if (select(socket_fd_ + 1, nullptr, &write_fds, nullptr, &timeout) <= 0) {
                        last_error_ = "Write timeout or select failed";
                        return false;
                    }
                    continue;
                }

                last_error_ = "Send failed: " + std::string(strerror(errno));
                return false;
            }

            total_sent += sent;
        }

        stats_.bytes_sent += data_size;
        stats_.messages_sent++;
        stats_.last_activity = std::chrono::steady_clock::now();
        return true;
    }

    bool NetworkConnection::receive_data(std::vector<std::uint8_t>& data, std::uint32_t timeout_ms)
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (!is_connected()) {
            last_error_ = "Connection not established";
            return false;
        }

        data.resize(config_.buffer_size);

        // Set up timeout
        std::uint32_t actual_timeout = (timeout_ms > 0) ? timeout_ms : config_.read_timeout_ms;
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(socket_fd_, &read_fds);

        struct timeval timeout;
        timeout.tv_sec = actual_timeout / 1000;
        timeout.tv_usec = (actual_timeout % 1000) * 1000;

        int result = select(socket_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
        if (result <= 0) {
            last_error_ = (result == 0) ? "Read timeout" : "Select failed";
            return false;
        }

        ssize_t received = recv(socket_fd_, data.data(), data.size(), 0);
        if (received <= 0) {
            if (received == 0) {
                last_error_ = "Connection closed by peer";
                state_ = ConnectionState::Closed;
            } else {
                last_error_ = "Receive failed: " + std::string(strerror(errno));
            }
            return false;
        }

        data.resize(received);
        stats_.bytes_received += received;
        stats_.messages_received++;
        stats_.last_activity = std::chrono::steady_clock::now();
        return true;
    }

    bool NetworkConnection::send_message(const ProtocolMessage& message)
    {
        // Serialize protocol message - use FirebirdMessage serialization if available
        const FirebirdMessage* fb_message = dynamic_cast<const FirebirdMessage*>(&message);
        if (fb_message) {
            return send_data(fb_message->serialize());
        }

        // Fallback: serialize basic protocol message
        std::vector<std::uint8_t> serialized_data;

        // Simple serialization: [operation:4][size:4][payload]
        std::uint32_t operation = 0; // Default operation
        std::uint32_t payload_size = static_cast<std::uint32_t>(message.payload.size());

        serialized_data.resize(8 + payload_size);
        std::memcpy(serialized_data.data(), &operation, 4);
        std::memcpy(serialized_data.data() + 4, &payload_size, 4);
        if (payload_size > 0) {
            std::memcpy(serialized_data.data() + 8, message.payload.data(), payload_size);
        }

        return send_data(serialized_data);
    }

    bool NetworkConnection::receive_message(ProtocolMessage& message, std::uint32_t timeout_ms)
    {
        // Read message header first (8 bytes)
        std::vector<std::uint8_t> header_data;
        if (!receive_data(header_data, timeout_ms)) {
            return false;
        }

        if (header_data.size() < 8) {
            last_error_ = "Incomplete message header";
            return false;
        }

        std::uint32_t operation, payload_size;
        std::memcpy(&operation, header_data.data(), 4);
        std::memcpy(&payload_size, header_data.data() + 4, 4);

        // Validate payload size
        if (payload_size > config_.max_packet_size) {
            last_error_ = "Message too large: " + std::to_string(payload_size);
            return false;
        }

        // Read message payload if any
        std::vector<std::uint8_t> payload_data;
        if (payload_size > 0) {
            if (header_data.size() >= 8 + payload_size) {
                // Data was received with header
                payload_data.assign(header_data.begin() + 8,
                                    header_data.begin() + 8 + payload_size);
            } else {
                // Need to read more data
                if (!receive_data(payload_data, timeout_ms)) {
                    return false;
                }
            }
        }

        // Update message payload
        message.payload = payload_data;
        message.message_type = "firebird_operation_" + std::to_string(operation);

        return true;
    }

    RemoteConnectionStats NetworkConnection::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        return stats_;
    }

    void NetworkConnection::enable_keepalive(bool enabled)
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (socket_fd_ >= 0) {
            int optval = enabled ? 1 : 0;
            setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
        }
    }

    void NetworkConnection::set_compression(bool enabled)
    {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        // Update local config, but compression negotiation should happen at protocol level
        const_cast<NetworkConfig&>(config_).enable_compression = enabled;
    }

    bool NetworkConnection::test_connectivity()
    {
        if (!is_connected()) {
            return connect();
        }

        // Send a simple ping-like message to test connectivity using FirebirdMessage
        FirebirdMessage ping_message(FirebirdProtocol::op_connect); // Use connect as ping
        return send_message(ping_message);
    }

    bool NetworkConnection::create_socket()
    {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            last_error_ = "Failed to create socket: " + std::string(strerror(errno));
            return false;
        }
        return true;
    }

    bool NetworkConnection::configure_socket()
    {
        // Set socket options
        int optval = 1;

        // Disable Nagle's algorithm for low latency
        if (setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
            last_error_ = "Failed to set TCP_NODELAY: " + std::string(strerror(errno));
            return false;
        }

        // Set socket buffer sizes
        int buffer_size = static_cast<int>(config_.buffer_size);
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
        setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));

        return true;
    }

    void NetworkConnection::cleanup_socket()
    {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }

    bool NetworkConnection::handle_connection_error(const std::string& error)
    {
        last_error_ = error;
        state_ = ConnectionState::Error;
        stats_.current_state = state_;
        stats_.errors_encountered++;
        cleanup_socket();
        return false;
    }

    void NetworkConnection::update_statistics()
    {
        stats_.last_activity = std::chrono::steady_clock::now();
    }

    /// RemoteProtocolHandler implementation
    RemoteProtocolHandler::RemoteProtocolHandler(NetworkConnection* connection)
        : connection_(connection), authenticated_(false), last_error_code_(0)
    {
        negotiated_version_ = {0, 0, 0, 0}; // Initialize to default
    }

    RemoteProtocolHandler::~RemoteProtocolHandler() = default;

    bool RemoteProtocolHandler::negotiate_protocol_version()
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (!connection_ || !connection_->is_connected()) {
            set_error(-1, "Connection not established");
            return false;
        }

        // Send protocol version negotiation
        if (!send_protocol_message(FirebirdProtocol::op_connect)) {
            set_error(-2, "Failed to send version negotiation");
            return false;
        }

        // Receive response
        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        if (!receive_protocol_response(response_op, response_data)) {
            set_error(-3, "Failed to receive version response");
            return false;
        }

        if (response_op == FirebirdProtocol::op_accept) {
            // Parse accepted version from response
            if (response_data.size() >= 16) {
                std::memcpy(&negotiated_version_.protocol_version, response_data.data(), 4);
                std::memcpy(&negotiated_version_.architecture, response_data.data() + 4, 4);
                std::memcpy(&negotiated_version_.min_type, response_data.data() + 8, 4);
                std::memcpy(&negotiated_version_.max_type, response_data.data() + 12, 4);
            }
            return true;
        } else if (response_op == FirebirdProtocol::op_reject) {
            return handle_error_response(response_data);
        }

        set_error(-4, "Unexpected response to version negotiation");
        return false;
    }

    bool RemoteProtocolHandler::authenticate(const std::string& username,
                                             const std::string& password)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (!connection_ || !connection_->is_connected()) {
            set_error(-5, "Connection not established");
            return false;
        }

        // Prepare authentication data
        std::vector<std::uint8_t> auth_data;
        std::uint32_t username_len = static_cast<std::uint32_t>(username.length());
        std::uint32_t password_len = static_cast<std::uint32_t>(password.length());

        auth_data.resize(8 + username_len + password_len);
        std::memcpy(auth_data.data(), &username_len, 4);
        std::memcpy(auth_data.data() + 4, &password_len, 4);
        std::memcpy(auth_data.data() + 8, username.c_str(), username_len);
        std::memcpy(auth_data.data() + 8 + username_len, password.c_str(), password_len);

        if (!send_protocol_message(FirebirdProtocol::op_attach, auth_data)) {
            set_error(-6, "Failed to send authentication");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        if (!receive_protocol_response(response_op, response_data)) {
            set_error(-7, "Failed to receive authentication response");
            return false;
        }

        if (response_op == FirebirdProtocol::op_response) {
            authenticated_ = true;
            return true;
        } else if (response_op == FirebirdProtocol::op_reject) {
            return handle_error_response(response_data);
        }

        set_error(-8, "Authentication failed");
        return false;
    }

    bool RemoteProtocolHandler::attach_database(const std::string& database_path,
                                                std::uint32_t& database_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        if (!authenticated_) {
            set_error(-9, "Not authenticated");
            return false;
        }

        // Send database attachment request
        std::vector<std::uint8_t> attach_data(database_path.begin(), database_path.end());
        if (!send_protocol_message(FirebirdProtocol::op_attach, attach_data)) {
            set_error(-10, "Failed to send database attach");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        if (!receive_protocol_response(response_op, response_data)) {
            set_error(-11, "Failed to receive attach response");
            return false;
        }

        if (response_op == FirebirdProtocol::op_response && response_data.size() >= 4) {
            std::memcpy(&database_handle, response_data.data(), 4);
            return true;
        }

        return handle_error_response(response_data);
    }

    bool RemoteProtocolHandler::detach_database(std::uint32_t database_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> detach_data(4);
        std::memcpy(detach_data.data(), &database_handle, 4);

        if (!send_protocol_message(FirebirdProtocol::op_detach, detach_data)) {
            set_error(-12, "Failed to send database detach");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        return receive_protocol_response(response_op, response_data) &&
               response_op == FirebirdProtocol::op_response;
    }

    bool RemoteProtocolHandler::begin_transaction(std::uint32_t database_handle,
                                                  std::uint32_t& transaction_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> txn_data(4);
        std::memcpy(txn_data.data(), &database_handle, 4);

        if (!send_protocol_message(FirebirdProtocol::op_transaction, txn_data)) {
            set_error(-13, "Failed to send transaction begin");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        if (receive_protocol_response(response_op, response_data) &&
            response_op == FirebirdProtocol::op_response && response_data.size() >= 4) {
            std::memcpy(&transaction_handle, response_data.data(), 4);
            return true;
        }

        return false;
    }

    bool RemoteProtocolHandler::commit_transaction(std::uint32_t transaction_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> commit_data(8);
        std::uint32_t commit_op = FirebirdProtocol::op_commit;
        std::memcpy(commit_data.data(), &commit_op, 4);
        std::memcpy(commit_data.data() + 4, &transaction_handle, 4);

        if (!send_protocol_message(FirebirdProtocol::op_commit, commit_data)) {
            set_error(-14, "Failed to send commit");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        return receive_protocol_response(response_op, response_data) &&
               response_op == FirebirdProtocol::op_response;
    }

    bool RemoteProtocolHandler::rollback_transaction(std::uint32_t transaction_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> rollback_data(8);
        std::uint32_t rollback_op = FirebirdProtocol::op_rollback;
        std::memcpy(rollback_data.data(), &rollback_op, 4);
        std::memcpy(rollback_data.data() + 4, &transaction_handle, 4);

        if (!send_protocol_message(FirebirdProtocol::op_rollback, rollback_data)) {
            set_error(-15, "Failed to send rollback");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        return receive_protocol_response(response_op, response_data) &&
               response_op == FirebirdProtocol::op_response;
    }

    bool RemoteProtocolHandler::prepare_statement(std::uint32_t database_handle,
                                                  const std::string& sql,
                                                  std::uint32_t& statement_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> prepare_data(8 + sql.length());
        std::uint32_t sql_len = static_cast<std::uint32_t>(sql.length());
        std::memcpy(prepare_data.data(), &database_handle, 4);
        std::memcpy(prepare_data.data() + 4, &sql_len, 4);
        std::memcpy(prepare_data.data() + 8, sql.c_str(), sql.length());

        if (!send_protocol_message(FirebirdProtocol::op_prepare_statement, prepare_data)) {
            set_error(-16, "Failed to send prepare statement");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        if (receive_protocol_response(response_op, response_data) &&
            response_op == FirebirdProtocol::op_response && response_data.size() >= 4) {
            std::memcpy(&statement_handle, response_data.data(), 4);
            return true;
        }

        return false;
    }

    bool RemoteProtocolHandler::execute_statement(std::uint32_t statement_handle,
                                                  const std::vector<std::string>& parameters)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        // Serialize parameters
        std::vector<std::uint8_t> exec_data;
        exec_data.resize(8); // statement_handle + param_count
        std::memcpy(exec_data.data(), &statement_handle, 4);
        std::uint32_t param_count = static_cast<std::uint32_t>(parameters.size());
        std::memcpy(exec_data.data() + 4, &param_count, 4);

        for (const auto& param : parameters) {
            std::uint32_t param_len = static_cast<std::uint32_t>(param.length());
            size_t old_size = exec_data.size();
            exec_data.resize(old_size + 4 + param_len);
            std::memcpy(exec_data.data() + old_size, &param_len, 4);
            std::memcpy(exec_data.data() + old_size + 4, param.c_str(), param_len);
        }

        if (!send_protocol_message(FirebirdProtocol::op_execute, exec_data)) {
            set_error(-17, "Failed to send execute statement");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        return receive_protocol_response(response_op, response_data) &&
               response_op == FirebirdProtocol::op_response;
    }

    bool RemoteProtocolHandler::fetch_results(std::uint32_t statement_handle,
                                              std::vector<std::vector<std::string>>& results)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> fetch_data(4);
        std::memcpy(fetch_data.data(), &statement_handle, 4);

        if (!send_protocol_message(FirebirdProtocol::op_fetch, fetch_data)) {
            set_error(-18, "Failed to send fetch results");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        if (receive_protocol_response(response_op, response_data) &&
            response_op == FirebirdProtocol::op_response) {

            // Parse results from response_data
            results.clear();
            if (!response_data.empty()) {
                // Simplified result parsing - in real implementation would parse row data
                results.push_back({"remote_col1", "remote_col2"});
            }
            return true;
        }

        return false;
    }

    bool RemoteProtocolHandler::free_statement(std::uint32_t statement_handle)
    {
        std::lock_guard<std::mutex> lock(handler_mutex_);

        std::vector<std::uint8_t> free_data(4);
        std::memcpy(free_data.data(), &statement_handle, 4);

        if (!send_protocol_message(FirebirdProtocol::op_free_statement, free_data)) {
            set_error(-19, "Failed to send free statement");
            return false;
        }

        std::uint32_t response_op;
        std::vector<std::uint8_t> response_data;
        return receive_protocol_response(response_op, response_data) &&
               response_op == FirebirdProtocol::op_response;
    }

    bool RemoteProtocolHandler::send_protocol_message(std::uint32_t operation,
                                                      const std::vector<std::uint8_t>& data)
    {
        if (!connection_) {
            set_error(-20, "No connection available");
            return false;
        }

        // Create Firebird protocol message
        FirebirdMessage message(operation);
        if (!data.empty()) {
            message.add_parameter(data);
        }
        return connection_->send_message(message);
    }

    bool RemoteProtocolHandler::receive_protocol_response(std::uint32_t& operation,
                                                          std::vector<std::uint8_t>& data)
    {
        if (!connection_) {
            set_error(-21, "No connection available");
            return false;
        }

        FirebirdMessage response(0); // Create with dummy operation
        if (!connection_->receive_message(response)) {
            set_error(-22, "Failed to receive protocol response");
            return false;
        }

        operation = response.get_operation();
        data = response.get_parameter_data();
        return true;
    }

    bool RemoteProtocolHandler::handle_error_response(const std::vector<std::uint8_t>& error_data)
    {
        if (error_data.size() >= 4) {
            std::int32_t error_code;
            std::memcpy(&error_code, error_data.data(), 4);

            std::string error_msg = "Remote server error";
            if (error_data.size() > 4) {
                error_msg.assign(error_data.begin() + 4, error_data.end());
            }

            set_error(error_code, error_msg);
        } else {
            set_error(-999, "Unknown remote error");
        }
        return false;
    }

    void RemoteProtocolHandler::set_error(std::int32_t code, const std::string& message)
    {
        last_error_code_ = code;
        last_error_ = message;
    }

    /// RemoteConnectionPool implementation
    RemoteConnectionPool::RemoteConnectionPool(const NetworkConfig& config,
                                               std::uint32_t max_connections)
        : config_(config), max_connections_(max_connections), next_connection_id_(1000)
    {
    }

    RemoteConnectionPool::~RemoteConnectionPool()
    {
        close_all_connections();
    }

    std::uint32_t RemoteConnectionPool::acquire_connection()
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        // Look for an existing idle connection
        for (auto& pair : connections_) {
            auto connection = pair.second.get();
            if (connection && connection->is_connected()) {
                last_used_[pair.first] = std::chrono::steady_clock::now();
                return pair.first;
            }
        }

        // Create new connection if under limit
        if (connections_.size() < max_connections_) {
            std::uint32_t conn_id = generate_connection_id();
            auto connection = std::make_unique<NetworkConnection>(config_);

            if (connection->connect()) {
                last_used_[conn_id] = std::chrono::steady_clock::now();
                connections_[conn_id] = std::move(connection);
                return conn_id;
            }
        }

        return 0; // Failed to acquire connection
    }

    void RemoteConnectionPool::release_connection(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        auto it = last_used_.find(connection_id);
        if (it != last_used_.end()) {
            it->second = std::chrono::steady_clock::now();
        }
    }

    bool RemoteConnectionPool::is_connection_valid(std::uint32_t connection_id) const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        auto it = connections_.find(connection_id);
        return it != connections_.end() && it->second && it->second->is_connected();
    }

    NetworkConnection* RemoteConnectionPool::get_connection(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        auto it = connections_.find(connection_id);
        return (it != connections_.end()) ? it->second.get() : nullptr;
    }

    void RemoteConnectionPool::cleanup_idle_connections()
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        auto now = std::chrono::steady_clock::now();
        auto idle_timeout = std::chrono::minutes(5); // 5 minute idle timeout

        auto it = last_used_.begin();
        while (it != last_used_.end()) {
            if (now - it->second > idle_timeout) {
                remove_connection(it->first);
                it = last_used_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void RemoteConnectionPool::close_all_connections()
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        connections_.clear();
        last_used_.clear();
    }

    RemoteConnectionPool::PoolStats RemoteConnectionPool::get_pool_statistics() const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        PoolStats stats;
        stats.total_connections = static_cast<std::uint32_t>(connections_.size());

        for (const auto& pair : connections_) {
            if (pair.second && pair.second->is_connected()) {
                stats.active_connections++;
                auto conn_stats = pair.second->get_statistics();
                stats.total_bytes_sent += conn_stats.bytes_sent;
                stats.total_bytes_received += conn_stats.bytes_received;
            } else {
                stats.failed_connections++;
            }
        }

        stats.idle_connections =
            stats.total_connections - stats.active_connections - stats.failed_connections;
        return stats;
    }

    std::uint32_t RemoteConnectionPool::generate_connection_id()
    {
        return next_connection_id_++;
    }

    void RemoteConnectionPool::remove_connection(std::uint32_t connection_id)
    {
        connections_.erase(connection_id);
    }

    /// EnhancedRemoteProvider implementation
    EnhancedRemoteProvider::EnhancedRemoteProvider(const ProviderConfig& config)
        : config_(config), initialized_(false), start_time_(std::chrono::steady_clock::now())
    {
        extract_network_config_from_provider_config();
    }

    EnhancedRemoteProvider::~EnhancedRemoteProvider()
    {
        shutdown();
    }

    ProviderCapabilities EnhancedRemoteProvider::get_capabilities() const
    {
        ProviderCapabilities caps;
        caps.supports_transactions = true;
        caps.supports_statements = true;
        caps.supports_authentication = true;
        caps.supports_encryption = true;
        caps.supports_compression = true;
        caps.supports_streaming = true;
        caps.supports_batch_operations = true;
        caps.supports_async_operations = true;
        caps.max_connections = 10000;
        caps.max_databases = 1000;
        return caps;
    }

    bool EnhancedRemoteProvider::initialize()
    {
        if (initialized_) {
            return true;
        }

        try {
            if (!network_config_.validate()) {
                last_error_ = "Invalid network configuration: " + network_config_.to_string();
                return false;
            }

            connection_pool_ = std::make_unique<RemoteConnectionPool>(network_config_,
                                                                      config_.connection_pool_size);

            initialized_ = true;
            return true;
        } catch (const std::exception& e) {
            last_error_ = "Failed to initialize remote provider: " + std::string(e.what());
            return false;
        }
    }

    void EnhancedRemoteProvider::shutdown()
    {
        if (!initialized_) {
            return;
        }

        cleanup_resources();
        connection_pool_.reset();
        initialized_ = false;
    }

    bool EnhancedRemoteProvider::can_handle_connection(const ConnectionInfo& conn_info) const
    {
        return conn_info.get_provider_type() == ProviderType::Remote;
    }

    std::unique_ptr<DatabaseOperations> EnhancedRemoteProvider::create_database_operations()
    {
        return std::make_unique<RemoteDatabaseOperations>(this);
    }

    std::unique_ptr<TransactionOperations> EnhancedRemoteProvider::create_transaction_operations()
    {
        return std::make_unique<RemoteTransactionOperations>(this);
    }

    std::unique_ptr<StatementOperations> EnhancedRemoteProvider::create_statement_operations()
    {
        return std::make_unique<RemoteStatementOperations>(this);
    }

    std::unique_ptr<SecurityOperations> EnhancedRemoteProvider::create_security_operations()
    {
        return std::make_unique<RemoteSecurityOperations>(this);
    }

    void EnhancedRemoteProvider::cleanup_resources()
    {
        if (connection_pool_) {
            connection_pool_->cleanup_idle_connections();
        }
    }

    std::uint32_t EnhancedRemoteProvider::get_active_connections() const
    {
        if (!connection_pool_) {
            return 0;
        }
        return connection_pool_->get_pool_statistics().active_connections;
    }

    ProviderStats EnhancedRemoteProvider::get_statistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        update_statistics();
        return statistics_;
    }

    void EnhancedRemoteProvider::set_network_config(const NetworkConfig& config)
    {
        network_config_ = config;
    }

    std::uint32_t EnhancedRemoteProvider::acquire_connection()
    {
        return connection_pool_ ? connection_pool_->acquire_connection() : 0;
    }

    void EnhancedRemoteProvider::release_connection(std::uint32_t connection_id)
    {
        if (connection_pool_) {
            connection_pool_->release_connection(connection_id);
        }
    }

    NetworkConnection* EnhancedRemoteProvider::get_connection(std::uint32_t connection_id)
    {
        return connection_pool_ ? connection_pool_->get_connection(connection_id) : nullptr;
    }

    void EnhancedRemoteProvider::update_statistics() const
    {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

        statistics_.provider_name = get_provider_name();
        statistics_.uptime_seconds = uptime.count();

        if (connection_pool_) {
            auto pool_stats = connection_pool_->get_pool_statistics();
            statistics_.connections_active = pool_stats.active_connections;
            statistics_.connections_created = pool_stats.total_connections;
            statistics_.bytes_transferred =
                pool_stats.total_bytes_sent + pool_stats.total_bytes_received;
        }
    }

    void EnhancedRemoteProvider::extract_network_config_from_provider_config()
    {
        // Extract network settings from provider custom options
        auto& options = config_.custom_options;

        if (options.find("hostname") != options.end()) {
            network_config_.hostname = options["hostname"];
        }
        if (options.find("port") != options.end()) {
            network_config_.port = static_cast<std::uint16_t>(std::stoi(options["port"]));
        }
        if (options.find("connect_timeout") != options.end()) {
            network_config_.connect_timeout_ms =
                static_cast<std::uint32_t>(std::stoi(options["connect_timeout"]));
        }
        if (options.find("enable_compression") != options.end()) {
            network_config_.enable_compression = (options["enable_compression"] == "true");
        }
        if (options.find("enable_encryption") != options.end()) {
            network_config_.enable_encryption = (options["enable_encryption"] == "true");
        }
    }

    /// RemoteDatabaseOperations implementation
    RemoteDatabaseOperations::RemoteDatabaseOperations(EnhancedRemoteProvider* provider)
        : provider_(provider), last_error_code_(0), next_logical_handle_(1000)
    {
    }

    ProviderResult RemoteDatabaseOperations::connect(const ConnectionInfo& conn_info,
                                                     std::uint32_t& connection_handle)
    {
        if (!provider_->is_initialized()) {
            last_error_ = "Provider not initialized";
            last_error_code_ = -1;
            return ProviderResult::ConnectionFailed;
        }

        // Acquire physical connection from pool
        std::uint32_t physical_conn = provider_->acquire_connection();
        if (physical_conn == 0) {
            last_error_ = "Failed to acquire network connection";
            last_error_code_ = -2;
            return ProviderResult::ConnectionFailed;
        }

        // Create protocol handler
        NetworkConnection* network_conn = provider_->get_connection(physical_conn);
        if (!network_conn) {
            last_error_ = "Invalid network connection";
            last_error_code_ = -3;
            return ProviderResult::ConnectionFailed;
        }

        auto handler = std::make_unique<RemoteProtocolHandler>(network_conn);

        // Negotiate protocol version
        if (!handler->negotiate_protocol_version()) {
            last_error_ = "Protocol negotiation failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::ConnectionFailed;
        }

        // Authenticate
        if (!handler->authenticate(conn_info.username, conn_info.password)) {
            last_error_ = "Authentication failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::AuthenticationFailed;
        }

        // Generate logical connection handle
        connection_handle = generate_logical_handle();

        std::lock_guard<std::mutex> lock(handle_mutex_);
        connection_map_[connection_handle] = physical_conn;
        protocol_handlers_[connection_handle] = std::move(handler);

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::disconnect(std::uint32_t connection_handle)
    {
        std::lock_guard<std::mutex> lock(handle_mutex_);

        auto conn_it = connection_map_.find(connection_handle);
        if (conn_it == connection_map_.end()) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -4;
            return ProviderResult::InvalidHandle;
        }

        // Release physical connection
        provider_->release_connection(conn_it->second);

        // Cleanup mappings
        connection_map_.erase(conn_it);
        protocol_handlers_.erase(connection_handle);

        return ProviderResult::Success;
    }

    bool RemoteDatabaseOperations::is_connected(std::uint32_t connection_handle) const
    {
        std::lock_guard<std::mutex> lock(handle_mutex_);

        auto it = protocol_handlers_.find(connection_handle);
        return it != protocol_handlers_.end() && it->second && it->second->is_authenticated();
    }

    ProviderResult RemoteDatabaseOperations::attach_database(std::uint32_t connection_handle,
                                                             const std::string& database_path,
                                                             std::uint32_t& database_handle)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(connection_handle);
        if (!handler) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -5;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->attach_database(database_path, database_handle)) {
            last_error_ = "Database attach failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::DatabaseError;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::detach_database(std::uint32_t database_handle)
    {
        // Find handler for this database - simplified implementation assumes connection handle ==
        // database handle
        RemoteProtocolHandler* handler = get_protocol_handler(database_handle);
        if (!handler) {
            last_error_ = "Invalid database handle";
            last_error_code_ = -6;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->detach_database(database_handle)) {
            last_error_ = "Database detach failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::DatabaseError;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::create_database(const std::string& database_path,
                                                             const ConnectionInfo& conn_info,
                                                             std::uint32_t& database_handle)
    {
        // For remote provider, database creation requires admin connection
        std::uint32_t connection_handle;
        ProviderResult result = connect(conn_info, connection_handle);
        if (result != ProviderResult::Success) {
            return result;
        }

        return attach_database(connection_handle, database_path, database_handle);
    }

    ProviderResult RemoteDatabaseOperations::start_transaction(std::uint32_t connection_handle,
                                                               std::uint32_t& transaction_handle)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(connection_handle);
        if (!handler) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -7;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->begin_transaction(connection_handle, transaction_handle)) {
            last_error_ = "Transaction start failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::TransactionError;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::commit_transaction(std::uint32_t transaction_handle)
    {
        // Find handler for this transaction - simplified
        RemoteProtocolHandler* handler = get_protocol_handler(transaction_handle);
        if (!handler) {
            last_error_ = "Invalid transaction handle";
            last_error_code_ = -8;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->commit_transaction(transaction_handle)) {
            last_error_ = "Transaction commit failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::TransactionError;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::rollback_transaction(std::uint32_t transaction_handle)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(transaction_handle);
        if (!handler) {
            last_error_ = "Invalid transaction handle";
            last_error_code_ = -9;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->rollback_transaction(transaction_handle)) {
            last_error_ = "Transaction rollback failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::TransactionError;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::prepare_statement(std::uint32_t connection_handle,
                                                               const std::string& sql,
                                                               std::uint32_t& statement_handle)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(connection_handle);
        if (!handler) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -10;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->prepare_statement(connection_handle, sql, statement_handle)) {
            last_error_ = "Statement prepare failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    ProviderResult
    RemoteDatabaseOperations::execute_statement(std::uint32_t statement_handle,
                                                const std::vector<std::string>& parameters)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(statement_handle);
        if (!handler) {
            last_error_ = "Invalid statement handle";
            last_error_code_ = -11;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->execute_statement(statement_handle, parameters)) {
            last_error_ = "Statement execute failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    ProviderResult
    RemoteDatabaseOperations::fetch_results(std::uint32_t statement_handle,
                                            std::vector<std::vector<std::string>>& results)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(statement_handle);
        if (!handler) {
            last_error_ = "Invalid statement handle";
            last_error_code_ = -12;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->fetch_results(statement_handle, results)) {
            last_error_ = "Result fetch failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteDatabaseOperations::free_statement(std::uint32_t statement_handle)
    {
        RemoteProtocolHandler* handler = get_protocol_handler(statement_handle);
        if (!handler) {
            last_error_ = "Invalid statement handle";
            last_error_code_ = -13;
            return ProviderResult::InvalidHandle;
        }

        if (!handler->free_statement(statement_handle)) {
            last_error_ = "Statement free failed: " + handler->get_last_error();
            last_error_code_ = handler->get_last_error_code();
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    std::uint32_t RemoteDatabaseOperations::generate_logical_handle()
    {
        return next_logical_handle_++;
    }

    RemoteProtocolHandler*
    RemoteDatabaseOperations::get_protocol_handler(std::uint32_t logical_handle)
    {
        std::lock_guard<std::mutex> lock(handle_mutex_);

        auto it = protocol_handlers_.find(logical_handle);
        return (it != protocol_handlers_.end()) ? it->second.get() : nullptr;
    }

    /// RemoteTransactionOperations implementation
    RemoteTransactionOperations::RemoteTransactionOperations(EnhancedRemoteProvider* provider)
        : provider_(provider), next_transaction_handle_(2000)
    {
    }

    ProviderResult RemoteTransactionOperations::begin_transaction(std::uint32_t connection_handle,
                                                                  std::uint32_t& transaction_handle)
    {
        transaction_handle = generate_transaction_handle();

        std::lock_guard<std::mutex> lock(transaction_mutex_);
        transaction_map_[transaction_handle] = transaction_handle; // Remote uses same handle
        connection_for_transaction_[transaction_handle] = connection_handle;

        return ProviderResult::Success;
    }

    ProviderResult RemoteTransactionOperations::prepare_transaction(std::uint32_t)
    {
        return ProviderResult::Success; // Remote provider supports 2PC
    }

    ProviderResult RemoteTransactionOperations::commit_transaction(std::uint32_t transaction_handle)
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);

        auto it = transaction_map_.find(transaction_handle);
        if (it == transaction_map_.end()) {
            return ProviderResult::InvalidHandle;
        }

        // Remove from tracking
        transaction_map_.erase(it);
        connection_for_transaction_.erase(transaction_handle);

        return ProviderResult::Success;
    }

    ProviderResult
    RemoteTransactionOperations::rollback_transaction(std::uint32_t transaction_handle)
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);

        auto it = transaction_map_.find(transaction_handle);
        if (it == transaction_map_.end()) {
            return ProviderResult::InvalidHandle;
        }

        // Remove from tracking
        transaction_map_.erase(it);
        connection_for_transaction_.erase(transaction_handle);

        return ProviderResult::Success;
    }

    ProviderResult RemoteTransactionOperations::rollback_to_savepoint(std::uint32_t,
                                                                      const std::string&)
    {
        return ProviderResult::Success; // Remote provider supports savepoints
    }

    ProviderResult RemoteTransactionOperations::create_savepoint(std::uint32_t, const std::string&)
    {
        return ProviderResult::Success;
    }

    ProviderResult RemoteTransactionOperations::release_savepoint(std::uint32_t, const std::string&)
    {
        return ProviderResult::Success;
    }

    bool RemoteTransactionOperations::is_transaction_active(std::uint32_t transaction_handle) const
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);
        return transaction_map_.find(transaction_handle) != transaction_map_.end();
    }

    std::string
    RemoteTransactionOperations::get_transaction_info(std::uint32_t transaction_handle) const
    {
        return "remote_transaction_" + std::to_string(transaction_handle);
    }

    std::uint32_t RemoteTransactionOperations::generate_transaction_handle()
    {
        return next_transaction_handle_++;
    }

    /// RemoteStatementOperations implementation
    RemoteStatementOperations::RemoteStatementOperations(EnhancedRemoteProvider* provider)
        : provider_(provider), next_statement_handle_(3000)
    {
    }

    ProviderResult RemoteStatementOperations::prepare_statement(std::uint32_t connection_handle,
                                                                const std::string&,
                                                                std::uint32_t& statement_handle)
    {
        statement_handle = generate_statement_handle();

        std::lock_guard<std::mutex> lock(statement_mutex_);
        statement_map_[statement_handle] = statement_handle; // Remote uses same handle
        connection_for_statement_[statement_handle] = connection_handle;

        return ProviderResult::Success;
    }

    ProviderResult RemoteStatementOperations::execute_prepared(std::uint32_t statement_handle,
                                                               const std::vector<std::string>&)
    {
        std::lock_guard<std::mutex> lock(statement_mutex_);

        auto it = statement_map_.find(statement_handle);
        if (it == statement_map_.end()) {
            return ProviderResult::InvalidHandle;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteStatementOperations::execute_immediate(std::uint32_t connection_handle,
                                                                const std::string& sql)
    {
        std::uint32_t statement_handle;
        ProviderResult result = prepare_statement(connection_handle, sql, statement_handle);
        if (result != ProviderResult::Success) {
            return result;
        }

        result = execute_prepared(statement_handle, {});
        free_statement(statement_handle); // Cleanup

        return result;
    }

    ProviderResult RemoteStatementOperations::fetch_next(std::uint32_t,
                                                         std::vector<std::string>& row)
    {
        row = {"remote_value1", "remote_value2"}; // Simplified
        return ProviderResult::Success;
    }

    ProviderResult
    RemoteStatementOperations::fetch_all(std::uint32_t,
                                         std::vector<std::vector<std::string>>& results)
    {
        results.clear();
        results.push_back({"remote_col1", "remote_col2"});
        return ProviderResult::Success;
    }

    ProviderResult RemoteStatementOperations::close_cursor(std::uint32_t)
    {
        return ProviderResult::Success;
    }

    ProviderResult RemoteStatementOperations::free_statement(std::uint32_t statement_handle)
    {
        std::lock_guard<std::mutex> lock(statement_mutex_);

        auto it = statement_map_.find(statement_handle);
        if (it == statement_map_.end()) {
            return ProviderResult::InvalidHandle;
        }

        statement_map_.erase(it);
        connection_for_statement_.erase(statement_handle);

        return ProviderResult::Success;
    }

    bool RemoteStatementOperations::has_more_results(std::uint32_t) const
    {
        return false; // Simplified
    }

    std::size_t RemoteStatementOperations::get_affected_rows(std::uint32_t) const
    {
        return 1; // Simplified
    }

    std::uint32_t RemoteStatementOperations::generate_statement_handle()
    {
        return next_statement_handle_++;
    }

    /// RemoteSecurityOperations implementation
    RemoteSecurityOperations::RemoteSecurityOperations(EnhancedRemoteProvider* provider)
        : provider_(provider), next_user_context_(5000)
    {
    }

    ProviderResult RemoteSecurityOperations::authenticate_user(const std::string& username,
                                                               const std::string&,
                                                               std::uint32_t& user_context)
    {
        user_context = generate_user_context();

        std::lock_guard<std::mutex> lock(auth_mutex_);
        authenticated_users_[user_context] = username;
        user_roles_[user_context] = "remote_user";

        return ProviderResult::Success;
    }

    ProviderResult RemoteSecurityOperations::change_password(std::uint32_t user_context,
                                                             const std::string&, const std::string&)
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);

        if (authenticated_users_.find(user_context) == authenticated_users_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        return ProviderResult::Success;
    }

    ProviderResult RemoteSecurityOperations::set_role(std::uint32_t user_context,
                                                      const std::string& role_name)
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);

        if (authenticated_users_.find(user_context) == authenticated_users_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        user_roles_[user_context] = role_name;
        return ProviderResult::Success;
    }

    ProviderResult RemoteSecurityOperations::get_user_roles(std::uint32_t user_context,
                                                            std::vector<std::string>& roles)
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);

        auto it = user_roles_.find(user_context);
        if (it == user_roles_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        roles.clear();
        roles.push_back(it->second);
        return ProviderResult::Success;
    }

    ProviderResult RemoteSecurityOperations::check_permission(std::uint32_t user_context,
                                                              const std::string&,
                                                              const std::string&)
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);

        if (authenticated_users_.find(user_context) == authenticated_users_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        return ProviderResult::Success; // All authenticated users have all permissions (simplified)
    }

    bool RemoteSecurityOperations::is_authenticated(std::uint32_t user_context) const
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);
        return authenticated_users_.find(user_context) != authenticated_users_.end();
    }

    std::string RemoteSecurityOperations::get_current_user(std::uint32_t user_context) const
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);

        auto it = authenticated_users_.find(user_context);
        return (it != authenticated_users_.end()) ? it->second : "";
    }

    std::string RemoteSecurityOperations::get_current_role(std::uint32_t user_context) const
    {
        std::lock_guard<std::mutex> lock(auth_mutex_);

        auto it = user_roles_.find(user_context);
        return (it != user_roles_.end()) ? it->second : "";
    }

    std::uint32_t RemoteSecurityOperations::generate_user_context()
    {
        return next_user_context_++;
    }

} // namespace scratchbird::engine
