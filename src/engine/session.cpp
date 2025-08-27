#include "scratchbird/engine/session.h"

#include "scratchbird/engine/network_server.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace scratchbird::engine
{

    Session::Session(std::uint64_t session_id, std::unique_ptr<TcpConnection> connection,
                     CatalogManager* catalog)
        : session_id_(session_id), connection_(std::move(connection)), catalog_(catalog),
          state_(SessionState::Created), database_attached_(false),
          connect_time_(get_current_time_ms()), queries_executed_(0)
    {
        last_activity_time_ = connect_time_;

        // TODO: Initialize engine for this session when available
    }

    Session::~Session()
    {
        shutdown();
    }

    bool Session::initialize()
    {
        if (!connection_ || !connection_->is_connected()) {
            state_ = SessionState::Error;
            return false;
        }

        // Configure connection
        connection_->set_tcp_nodelay(true);
        connection_->set_keepalive(true);
        connection_->set_timeout(300); // 5 minute timeout

        state_ = SessionState::Authenticating;
        return true;
    }

    void Session::run()
    {
        try {
            while (is_connection_alive() && state_.load() != SessionState::Disconnected) {
                update_activity_time();

                // Handle authentication phase
                if (state_.load() == SessionState::Authenticating) {
                    if (!handle_authentication()) {
                        break;
                    }
                }

                // Process protocol messages
                if (state_.load() == SessionState::Authenticated ||
                    state_.load() == SessionState::Connected) {
                    if (!process_messages()) {
                        break;
                    }
                }

                // Brief pause to prevent busy loop
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } catch (const std::exception& e) {
            std::cerr << "Session " << session_id_ << " error: " << e.what() << std::endl;
            state_ = SessionState::Error;
        }

        shutdown();
    }

    void Session::shutdown()
    {
        std::lock_guard<std::mutex> lock(session_mutex_);

        if (state_.load() == SessionState::Disconnected) {
            return;
        }

        // Detach database if attached
        if (database_attached_) {
            detach_database();
        }

        // Close connection
        if (connection_) {
            connection_->close();
        }

        state_ = SessionState::Disconnected;
    }

    SessionState Session::get_state() const
    {
        return state_.load();
    }

    std::string Session::get_client_address() const
    {
        if (!connection_) {
            return "";
        }
        return connection_->get_peer_address();
    }

    std::uint16_t Session::get_client_port() const
    {
        if (!connection_) {
            return 0;
        }
        return connection_->get_peer_port();
    }

    std::int64_t Session::get_last_activity_time() const
    {
        return last_activity_time_.load();
    }

    bool Session::is_authenticated() const
    {
        return auth_context_.is_authenticated;
    }

    bool Session::attach_database(const std::string& database_path)
    {
        std::lock_guard<std::mutex> lock(session_mutex_);

        if (database_attached_) {
            return false;
        }

        try {
            // TODO: Initialize engine with database when engine is available
            // For now, just simulate successful attachment
            database_name_ = database_path;
            database_attached_ = true;
            state_ = SessionState::Connected;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Database attach error: " << e.what() << std::endl;
            return false;
        }
    }

    void Session::detach_database()
    {
        std::lock_guard<std::mutex> lock(session_mutex_);

        if (database_attached_) {
            // TODO: Shutdown engine when available
            database_name_.clear();
            database_attached_ = false;
        }
    }

    bool Session::is_idle_timeout_exceeded(std::uint32_t timeout_seconds) const
    {
        std::int64_t current_time = get_current_time_ms();
        std::int64_t last_activity = last_activity_time_.load();
        return (current_time - last_activity) > (timeout_seconds * 1000);
    }

    bool Session::is_connection_alive() const
    {
        return connection_ && connection_->is_connected();
    }

    void Session::set_keepalive(bool enable)
    {
        if (connection_) {
            connection_->set_keepalive(enable);
        }
    }

    bool Session::handle_authentication()
    {
        // TODO: Implement proper authentication protocol
        // For now, simulate successful authentication

        auth_context_.username = "anonymous";
        auth_context_.client_address = get_client_address();
        auth_context_.auth_method = "none";
        auth_context_.is_authenticated = true;
        auth_context_.requires_2fa = false;
        auth_context_.role_name = "public";

        state_ = SessionState::Authenticated;
        return true;
    }

    bool Session::process_messages()
    {
        // TODO: Implement protocol message processing
        // For now, just handle basic connection keep-alive

        std::vector<std::uint8_t> data;

        // Try to receive data with non-blocking check
        if (connection_->receive_data(data, 1024)) {
            if (!data.empty()) {
                // Process received data
                update_activity_time();

                // Echo back for now (simple test protocol)
                connection_->send_data(data);

                queries_executed_++;
            }
            return true;
        }

        return false; // Connection error
    }

    void Session::handle_protocol_error()
    {
        std::cerr << "Protocol error in session " << session_id_ << std::endl;
        state_ = SessionState::Error;
    }

    void Session::update_activity_time()
    {
        last_activity_time_ = get_current_time_ms();
    }

    std::int64_t Session::get_current_time_ms() const
    {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
            .count();
    }

} // namespace scratchbird::engine
