#include "scratchbird/engine/session.h"

#include "scratchbird/engine/network_server.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace scratchbird::engine
{

    Session::Session(std::uint64_t session_id, std::unique_ptr<TcpConnection> connection,
                     CatalogManager* catalog, AuthenticationManager* auth_manager)
        : session_id_(session_id), connection_(std::move(connection)), catalog_(catalog),
          auth_manager_(auth_manager), state_(SessionState::Created), database_attached_(false),
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
        return auth_context_.is_authenticated() && (state_.load() == SessionState::Authenticated ||
                                                    state_.load() == SessionState::Connected);
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
        // Use proper authentication system if available
        if (auth_manager_) {
            // Set up authentication context with connection information
            auth_context_.set_remote_address(get_client_address());
            auth_context_.set_client_info("ScratchBird Session " + std::to_string(session_id_));

            // For network protocols, authentication typically involves a handshake
            // For now, we'll require explicit authentication calls
            state_ = SessionState::Authenticating;
            return true;
        } else {
            // Fallback to anonymous authentication if no auth manager
            auth_context_.set_username("anonymous");
            auth_context_.set_remote_address(get_client_address());
            auth_context_.set_authenticated(true);

            security_context_ = std::make_unique<SecurityContext>(
                "anonymous", "", std::vector<std::string>{"public"});

            state_ = SessionState::Authenticated;
            return true;
        }
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

    // Enhanced authentication methods
    bool Session::authenticate_user(const std::string& username, const std::string& password)
    {
        if (!auth_manager_) {
            return false;
        }

        std::lock_guard<std::mutex> lock(session_mutex_);

        auth_context_.set_username(username);
        auth_context_.set_credential("password", password);

        ScratchBird::AuthenticationResult result = auth_manager_->authenticate_user(auth_context_);

        if (result == ScratchBird::AuthenticationResult::Success) {
            // Create security context
            security_context_ =
                std::make_unique<SecurityContext>(username, "", std::vector<std::string>{"user"});

            state_ = SessionState::Authenticated;
            return true;
        } else if (result == ScratchBird::AuthenticationResult::RequiresTwoFactor) {
            // Initiate 2FA challenge
            active_challenge_ = auth_manager_->initiate_challenge(
                username, ScratchBird::AuthenticationMethod::TwoFactor);
            auth_context_.set_requires_2fa(true);
            return false; // Not fully authenticated yet
        }

        return false;
    }

    bool Session::authenticate_with_challenge(const std::string& challenge_response)
    {
        if (!auth_manager_ || !active_challenge_) {
            return false;
        }

        std::lock_guard<std::mutex> lock(session_mutex_);

        active_challenge_->set_response(challenge_response);

        ScratchBird::AuthenticationResult result =
            auth_manager_->complete_challenge(*active_challenge_, auth_context_);

        if (result == ScratchBird::AuthenticationResult::Success) {
            // Create security context
            security_context_ = std::make_unique<SecurityContext>(auth_context_.get_username(), "",
                                                                  std::vector<std::string>{"user"});

            active_challenge_.reset();
            state_ = SessionState::Authenticated;
            return true;
        }

        // Clear challenge on failure
        active_challenge_.reset();
        return false;
    }

    bool Session::require_two_factor() const
    {
        return auth_context_.requires_2fa();
    }

} // namespace scratchbird::engine
