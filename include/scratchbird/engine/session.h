#pragma once

#include "scratchbird/engine/authentication.h"
#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/network_server.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace scratchbird::engine
{

    /// Forward declarations
    class TcpConnection;
    class ProtocolHandler;
    class Engine;

    /// Session state enumeration
    enum class SessionState {
        Created,
        Authenticating,
        Authenticated,
        Connected,
        Disconnected,
        Error
    };

    // Use the comprehensive authentication system from ScratchBird namespace
    using AuthenticationContext = ScratchBird::AuthenticationContext;
    using SecurityContext = ScratchBird::SecurityContext;
    using AuthenticationManager = ScratchBird::AuthenticationManager;

    /// Session implementation
    class Session
    {
      public:
        Session(std::uint64_t session_id, std::unique_ptr<TcpConnection> connection,
                CatalogManager* catalog, AuthenticationManager* auth_manager = nullptr);
        ~Session();

        // Non-copyable, non-movable
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        /// Session lifecycle
        bool initialize();
        void run();
        void shutdown();

        /// Session information
        std::uint64_t get_session_id() const
        {
            return session_id_;
        }
        SessionState get_state() const;
        std::string get_client_address() const;
        std::uint16_t get_client_port() const;
        std::int64_t get_connect_time() const
        {
            return connect_time_;
        }
        std::int64_t get_last_activity_time() const;

        /// Authentication
        bool is_authenticated() const;
        const AuthenticationContext& get_auth_context() const
        {
            return auth_context_;
        }
        std::string get_username() const
        {
            return auth_context_.get_username();
        }

        /// Enhanced authentication methods
        bool authenticate_user(const std::string& username, const std::string& password);
        bool authenticate_with_challenge(const std::string& challenge_response);
        bool require_two_factor() const;
        SecurityContext* get_security_context() const
        {
            return security_context_.get();
        }

        /// Database operations
        bool attach_database(const std::string& database_path);
        void detach_database();
        bool is_database_attached() const
        {
            return database_attached_;
        }
        std::string get_database_name() const
        {
            return database_name_;
        }

        /// Statistics
        std::uint64_t get_queries_executed() const
        {
            return queries_executed_;
        }
        bool is_idle_timeout_exceeded(std::uint32_t timeout_seconds) const;

        /// Connection management
        bool is_connection_alive() const;
        void set_keepalive(bool enable);

      private:
        std::uint64_t session_id_;
        std::unique_ptr<TcpConnection> connection_;
        CatalogManager* catalog_;
        AuthenticationManager* auth_manager_;
        // TODO: Add engine and protocol handler when available
        // std::unique_ptr<Engine> engine_;
        // std::unique_ptr<ProtocolHandler> protocol_handler_;

        mutable std::mutex session_mutex_;
        std::atomic<SessionState> state_;
        AuthenticationContext auth_context_;
        std::unique_ptr<SecurityContext> security_context_;
        std::unique_ptr<ScratchBird::AuthenticationChallenge> active_challenge_;

        std::string database_name_;
        bool database_attached_;

        std::int64_t connect_time_;
        std::atomic<std::int64_t> last_activity_time_;
        std::atomic<std::uint64_t> queries_executed_;

        bool handle_authentication();
        bool process_messages();
        void handle_protocol_error();
        void update_activity_time();
        std::int64_t get_current_time_ms() const;
    };

} // namespace scratchbird::engine
