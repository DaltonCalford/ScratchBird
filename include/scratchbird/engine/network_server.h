#pragma once

#include "scratchbird/engine/catalog_manager.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// Forward declarations
    class Session;
    class ProtocolHandler;
    class ConnectionManager;
    class CatalogManager;

    /// Network server configuration
    struct NetworkServerConfig {
        std::string bind_address = "127.0.0.1";
        std::uint16_t port = 3050; // Default Firebird port
        std::string protocol = "inet";
        std::uint32_t max_connections = 1000;
        std::uint32_t connection_timeout_seconds = 300;
        std::uint32_t keepalive_interval_seconds = 60;
        bool ipv6_enabled = true;
        bool tcp_nodelay = true;
        std::uint32_t listen_backlog = 128;
        std::uint32_t worker_threads = 0; // 0 = auto-detect
        std::string log_level = "INFO";
    };

    /// Connection statistics
    struct ConnectionStats {
        std::uint64_t total_connections = 0;
        std::uint64_t active_connections = 0;
        std::uint64_t rejected_connections = 0;
        std::uint64_t bytes_sent = 0;
        std::uint64_t bytes_received = 0;
        std::uint64_t queries_processed = 0;
        double average_query_time_ms = 0.0;
    };

    /// Session information
    struct SessionInfo {
        std::uint64_t session_id;
        std::string client_address;
        std::uint16_t client_port;
        std::string protocol_version;
        std::string database_name;
        std::string username;
        std::int64_t connect_time;
        std::int64_t last_activity_time;
        std::uint64_t queries_executed = 0;
        bool is_authenticated = false;
        bool is_encrypted = false;
    };

    /// TCP connection wrapper
    class TcpConnection
    {
      public:
        explicit TcpConnection(int socket_fd);
        ~TcpConnection();

        // Non-copyable, movable
        TcpConnection(const TcpConnection&) = delete;
        TcpConnection& operator=(const TcpConnection&) = delete;
        TcpConnection(TcpConnection&& other) noexcept;
        TcpConnection& operator=(TcpConnection&& other) noexcept;

        /// Connection status
        bool is_connected() const;
        void close();

        /// Data I/O
        bool send_data(const std::vector<std::uint8_t>& data);
        bool receive_data(std::vector<std::uint8_t>& data, std::size_t max_bytes = 65536);
        bool receive_exact(std::vector<std::uint8_t>& data, std::size_t bytes);

        /// Socket configuration
        bool set_tcp_nodelay(bool enable);
        bool set_keepalive(bool enable, std::uint32_t interval_seconds = 60);
        bool set_timeout(std::uint32_t timeout_seconds);

        /// Connection info
        std::string get_peer_address() const;
        std::uint16_t get_peer_port() const;
        std::string get_local_address() const;
        std::uint16_t get_local_port() const;

        /// Socket access
        int get_socket() const
        {
            return socket_fd_;
        }

      private:
        int socket_fd_;
        mutable std::mutex socket_mutex_;
        std::atomic<bool> connected_;

        bool configure_socket();
    };

    /// TCP listener for accepting connections
    class TcpListener
    {
      public:
        explicit TcpListener(const NetworkServerConfig& config);
        ~TcpListener();

        // Non-copyable, non-movable
        TcpListener(const TcpListener&) = delete;
        TcpListener& operator=(const TcpListener&) = delete;

        /// Listener lifecycle
        bool start();
        void stop();
        bool is_running() const;

        /// Connection acceptance
        std::unique_ptr<TcpConnection> accept_connection();

        /// Configuration
        const NetworkServerConfig& get_config() const
        {
            return config_;
        }
        std::string get_bind_address() const;
        std::uint16_t get_bind_port() const;

      private:
        NetworkServerConfig config_;
        int listen_socket_ipv4_;
        int listen_socket_ipv6_;
        std::atomic<bool> running_;
        std::mutex listener_mutex_;

        bool create_listen_socket(int& socket_fd, bool ipv6);
        bool bind_and_listen(int socket_fd, bool ipv6);
        std::unique_ptr<TcpConnection> accept_from_socket(int listen_socket);
    };

    /// Connection manager for handling client sessions
    class ConnectionManager
    {
      public:
        explicit ConnectionManager(const NetworkServerConfig& config, CatalogManager* catalog);
        ~ConnectionManager();

        // Non-copyable, non-movable
        ConnectionManager(const ConnectionManager&) = delete;
        ConnectionManager& operator=(const ConnectionManager&) = delete;

        /// Connection lifecycle
        bool handle_connection(std::unique_ptr<TcpConnection> connection);
        void close_connection(std::uint64_t session_id);
        void close_all_connections();

        /// Session management
        std::vector<SessionInfo> get_active_sessions() const;
        std::uint32_t get_connection_count() const;
        bool is_connection_limit_reached() const;

        /// Statistics
        ConnectionStats get_connection_stats() const;
        void update_query_stats(double query_time_ms);

      private:
        NetworkServerConfig config_;
        CatalogManager* catalog_;
        mutable std::mutex sessions_mutex_;
        std::unordered_map<std::uint64_t, std::unique_ptr<Session>> active_sessions_;
        std::atomic<std::uint64_t> next_session_id_;
        std::atomic<std::uint64_t> total_connections_;
        std::atomic<std::uint64_t> rejected_connections_;
        std::atomic<std::uint64_t> bytes_sent_;
        std::atomic<std::uint64_t> bytes_received_;
        std::atomic<std::uint64_t> queries_processed_;
        std::atomic<double> total_query_time_ms_;

        std::uint64_t generate_session_id();
        void cleanup_inactive_sessions();
    };

    /// Main network server implementation
    class NetworkServer
    {
      public:
        explicit NetworkServer(const NetworkServerConfig& config, CatalogManager* catalog);
        ~NetworkServer();

        // Non-copyable, non-movable
        NetworkServer(const NetworkServer&) = delete;
        NetworkServer& operator=(const NetworkServer&) = delete;

        /// Server lifecycle
        bool start();
        void stop();
        bool is_running() const;

        /// Server management
        void shutdown_gracefully();
        void wait_for_shutdown();

        /// Statistics and monitoring
        ConnectionStats get_server_stats() const;
        std::vector<SessionInfo> get_active_sessions() const;
        NetworkServerConfig get_config() const
        {
            return config_;
        }

      private:
        NetworkServerConfig config_;
        CatalogManager* catalog_;

        std::unique_ptr<TcpListener> listener_;
        std::unique_ptr<ConnectionManager> connection_manager_;

        std::thread accept_thread_;
        std::vector<std::thread> worker_threads_;

        std::atomic<bool> running_;
        std::atomic<bool> shutdown_requested_;

        std::mutex server_mutex_;
        std::condition_variable shutdown_cv_;

        void accept_loop();
        void worker_loop();
        void initialize_worker_threads();
        void cleanup_threads();
    };

} // namespace scratchbird::engine
