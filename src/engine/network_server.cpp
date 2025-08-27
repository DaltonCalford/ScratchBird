#include "scratchbird/engine/network_server.h"

#include "scratchbird/engine/session.h"

#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace scratchbird::engine
{

    //=============================================================================
    // TcpConnection Implementation
    //=============================================================================

    TcpConnection::TcpConnection(int socket_fd) : socket_fd_(socket_fd), connected_(true)
    {
        if (socket_fd_ > 0) {
            configure_socket();
        }
    }

    TcpConnection::~TcpConnection()
    {
        close();
    }

    TcpConnection::TcpConnection(TcpConnection&& other) noexcept
        : socket_fd_(other.socket_fd_), connected_(other.connected_.load())
    {
        other.socket_fd_ = -1;
        other.connected_ = false;
    }

    TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept
    {
        if (this != &other) {
            close();
            socket_fd_ = other.socket_fd_;
            connected_ = other.connected_.load();
            other.socket_fd_ = -1;
            other.connected_ = false;
        }
        return *this;
    }

    bool TcpConnection::is_connected() const
    {
        return connected_.load() && socket_fd_ >= 0;
    }

    void TcpConnection::close()
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
            connected_ = false;
        }
    }

    bool TcpConnection::send_data(const std::vector<std::uint8_t>& data)
    {
        if (!is_connected() || data.empty()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(socket_mutex_);

        std::size_t bytes_sent = 0;
        const std::uint8_t* buffer = data.data();
        std::size_t remaining = data.size();

        while (remaining > 0) {
            ssize_t result = ::send(socket_fd_, buffer + bytes_sent, remaining, MSG_NOSIGNAL);
            if (result < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue; // Retry
                }
                connected_ = false;
                return false;
            }
            if (result == 0) {
                connected_ = false;
                return false;
            }
            bytes_sent += result;
            remaining -= result;
        }

        return true;
    }

    bool TcpConnection::receive_data(std::vector<std::uint8_t>& data, std::size_t max_bytes)
    {
        if (!is_connected()) {
            return false;
        }

        data.resize(max_bytes);

        std::lock_guard<std::mutex> lock(socket_mutex_);
        ssize_t result = ::recv(socket_fd_, data.data(), max_bytes, 0);

        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                data.clear();
                return true; // No data available, but connection is still valid
            }
            connected_ = false;
            return false;
        }

        if (result == 0) {
            // Connection closed by peer
            connected_ = false;
            return false;
        }

        data.resize(result);
        return true;
    }

    bool TcpConnection::receive_exact(std::vector<std::uint8_t>& data, std::size_t bytes)
    {
        if (!is_connected()) {
            return false;
        }

        data.resize(bytes);
        std::size_t bytes_received = 0;
        std::uint8_t* buffer = data.data();
        std::size_t remaining = bytes;

        std::lock_guard<std::mutex> lock(socket_mutex_);

        while (remaining > 0) {
            ssize_t result = ::recv(socket_fd_, buffer + bytes_received, remaining, 0);
            if (result < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue; // Retry
                }
                connected_ = false;
                return false;
            }
            if (result == 0) {
                connected_ = false;
                return false;
            }
            bytes_received += result;
            remaining -= result;
        }

        return true;
    }

    bool TcpConnection::set_tcp_nodelay(bool enable)
    {
        if (!is_connected()) {
            return false;
        }

        int flag = enable ? 1 : 0;
        return setsockopt(socket_fd_, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) == 0;
    }

    bool TcpConnection::set_keepalive(bool enable, std::uint32_t interval_seconds)
    {
        if (!is_connected()) {
            return false;
        }

        int flag = enable ? 1 : 0;
        if (setsockopt(socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) != 0) {
            return false;
        }

        if (enable) {
            int interval = static_cast<int>(interval_seconds);
            setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
            setsockopt(socket_fd_, IPPROTO_TCP, TCP_KEEPIDLE, &interval, sizeof(interval));
        }

        return true;
    }

    bool TcpConnection::set_timeout(std::uint32_t timeout_seconds)
    {
        if (!is_connected()) {
            return false;
        }

        struct timeval timeout;
        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;

        return setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
               setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
    }

    std::string TcpConnection::get_peer_address() const
    {
        if (!is_connected()) {
            return "";
        }

        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
            return "";
        }

        char address_str[INET6_ADDRSTRLEN];
        if (addr.ss_family == AF_INET) {
            sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(&addr);
            inet_ntop(AF_INET, &addr_in->sin_addr, address_str, INET_ADDRSTRLEN);
        } else if (addr.ss_family == AF_INET6) {
            sockaddr_in6* addr_in6 = reinterpret_cast<sockaddr_in6*>(&addr);
            inet_ntop(AF_INET6, &addr_in6->sin6_addr, address_str, INET6_ADDRSTRLEN);
        } else {
            return "";
        }

        return std::string(address_str);
    }

    std::uint16_t TcpConnection::get_peer_port() const
    {
        if (!is_connected()) {
            return 0;
        }

        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
            return 0;
        }

        if (addr.ss_family == AF_INET) {
            return ntohs(reinterpret_cast<sockaddr_in*>(&addr)->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            return ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);
        }

        return 0;
    }

    std::string TcpConnection::get_local_address() const
    {
        if (!is_connected()) {
            return "";
        }

        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
            return "";
        }

        char address_str[INET6_ADDRSTRLEN];
        if (addr.ss_family == AF_INET) {
            sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(&addr);
            inet_ntop(AF_INET, &addr_in->sin_addr, address_str, INET_ADDRSTRLEN);
        } else if (addr.ss_family == AF_INET6) {
            sockaddr_in6* addr_in6 = reinterpret_cast<sockaddr_in6*>(&addr);
            inet_ntop(AF_INET6, &addr_in6->sin6_addr, address_str, INET6_ADDRSTRLEN);
        } else {
            return "";
        }

        return std::string(address_str);
    }

    std::uint16_t TcpConnection::get_local_port() const
    {
        if (!is_connected()) {
            return 0;
        }

        sockaddr_storage addr;
        socklen_t addr_len = sizeof(addr);
        if (getsockname(socket_fd_, reinterpret_cast<sockaddr*>(&addr), &addr_len) != 0) {
            return 0;
        }

        if (addr.ss_family == AF_INET) {
            return ntohs(reinterpret_cast<sockaddr_in*>(&addr)->sin_port);
        } else if (addr.ss_family == AF_INET6) {
            return ntohs(reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port);
        }

        return 0;
    }

    bool TcpConnection::configure_socket()
    {
        if (socket_fd_ < 0) {
            return false;
        }

        // Set socket options for better performance
        int flag = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

        // Configure for non-blocking operation with appropriate timeouts
        struct timeval timeout;
        timeout.tv_sec = 30; // 30 second timeout
        timeout.tv_usec = 0;
        setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        return true;
    }

    //=============================================================================
    // TcpListener Implementation
    //=============================================================================

    TcpListener::TcpListener(const NetworkServerConfig& config)
        : config_(config), listen_socket_ipv4_(-1), listen_socket_ipv6_(-1), running_(false)
    {
    }

    TcpListener::~TcpListener()
    {
        stop();
    }

    bool TcpListener::start()
    {
        std::lock_guard<std::mutex> lock(listener_mutex_);

        if (running_) {
            return true;
        }

        // Create and bind IPv4 socket
        if (!create_listen_socket(listen_socket_ipv4_, false)) {
            return false;
        }

        if (!bind_and_listen(listen_socket_ipv4_, false)) {
            ::close(listen_socket_ipv4_);
            listen_socket_ipv4_ = -1;
            return false;
        }

        // Create and bind IPv6 socket if enabled
        if (config_.ipv6_enabled) {
            if (create_listen_socket(listen_socket_ipv6_, true)) {
                if (!bind_and_listen(listen_socket_ipv6_, true)) {
                    ::close(listen_socket_ipv6_);
                    listen_socket_ipv6_ = -1;
                    // Continue with IPv4 only
                }
            }
        }

        running_ = true;
        return true;
    }

    void TcpListener::stop()
    {
        std::lock_guard<std::mutex> lock(listener_mutex_);

        running_ = false;

        if (listen_socket_ipv4_ >= 0) {
            ::close(listen_socket_ipv4_);
            listen_socket_ipv4_ = -1;
        }

        if (listen_socket_ipv6_ >= 0) {
            ::close(listen_socket_ipv6_);
            listen_socket_ipv6_ = -1;
        }
    }

    bool TcpListener::is_running() const
    {
        return running_.load();
    }

    std::unique_ptr<TcpConnection> TcpListener::accept_connection()
    {
        if (!is_running()) {
            return nullptr;
        }

        // Use select to wait for connections on either socket
        fd_set read_fds;
        FD_ZERO(&read_fds);

        int max_fd = -1;
        if (listen_socket_ipv4_ >= 0) {
            FD_SET(listen_socket_ipv4_, &read_fds);
            max_fd = std::max(max_fd, listen_socket_ipv4_);
        }
        if (listen_socket_ipv6_ >= 0) {
            FD_SET(listen_socket_ipv6_, &read_fds);
            max_fd = std::max(max_fd, listen_socket_ipv6_);
        }

        if (max_fd < 0) {
            return nullptr;
        }

        struct timeval timeout;
        timeout.tv_sec = 1; // 1 second timeout
        timeout.tv_usec = 0;

        int result = select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
        if (result < 0) {
            return nullptr;
        }
        if (result == 0) {
            return nullptr; // Timeout
        }

        // Check which socket has a pending connection
        if (listen_socket_ipv4_ >= 0 && FD_ISSET(listen_socket_ipv4_, &read_fds)) {
            return accept_from_socket(listen_socket_ipv4_);
        }
        if (listen_socket_ipv6_ >= 0 && FD_ISSET(listen_socket_ipv6_, &read_fds)) {
            return accept_from_socket(listen_socket_ipv6_);
        }

        return nullptr;
    }

    std::string TcpListener::get_bind_address() const
    {
        return config_.bind_address;
    }

    std::uint16_t TcpListener::get_bind_port() const
    {
        return config_.port;
    }

    bool TcpListener::create_listen_socket(int& socket_fd, bool ipv6)
    {
        socket_fd = socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            return false;
        }

        // Set socket options
        int flag = 1;
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

        if (ipv6) {
            // Configure IPv6 socket for dual-stack if desired
            int ipv6_only = 0;
            setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only, sizeof(ipv6_only));
        }

        return true;
    }

    bool TcpListener::bind_and_listen(int socket_fd, bool ipv6)
    {
        if (ipv6) {
            sockaddr_in6 server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin6_family = AF_INET6;
            server_addr.sin6_port = htons(config_.port);

            if (config_.bind_address == "127.0.0.1" || config_.bind_address == "localhost") {
                server_addr.sin6_addr = in6addr_loopback;
            } else if (config_.bind_address == "0.0.0.0" || config_.bind_address.empty()) {
                server_addr.sin6_addr = in6addr_any;
            } else {
                if (inet_pton(AF_INET6, config_.bind_address.c_str(), &server_addr.sin6_addr) !=
                    1) {
                    return false;
                }
            }

            if (bind(socket_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) <
                0) {
                return false;
            }
        } else {
            sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(config_.port);

            if (config_.bind_address == "0.0.0.0" || config_.bind_address.empty()) {
                server_addr.sin_addr.s_addr = INADDR_ANY;
            } else {
                if (inet_pton(AF_INET, config_.bind_address.c_str(), &server_addr.sin_addr) != 1) {
                    return false;
                }
            }

            if (bind(socket_fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) <
                0) {
                return false;
            }
        }

        return listen(socket_fd, config_.listen_backlog) == 0;
    }

    std::unique_ptr<TcpConnection> TcpListener::accept_from_socket(int listen_socket)
    {
        sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int client_socket =
            accept(listen_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);

        if (client_socket < 0) {
            return nullptr;
        }

        return std::make_unique<TcpConnection>(client_socket);
    }

    //=============================================================================
    // ConnectionManager Implementation
    //=============================================================================

    ConnectionManager::ConnectionManager(const NetworkServerConfig& config, CatalogManager* catalog)
        : config_(config), catalog_(catalog), next_session_id_(1), total_connections_(0),
          rejected_connections_(0), bytes_sent_(0), bytes_received_(0), queries_processed_(0),
          total_query_time_ms_(0.0)
    {
    }

    ConnectionManager::~ConnectionManager()
    {
        close_all_connections();
    }

    bool ConnectionManager::handle_connection(std::unique_ptr<TcpConnection> connection)
    {
        if (!connection || !connection->is_connected()) {
            return false;
        }

        // Check connection limits
        if (is_connection_limit_reached()) {
            rejected_connections_++;
            return false;
        }

        // Create new session
        std::uint64_t session_id = generate_session_id();
        auto session = std::make_unique<Session>(session_id, std::move(connection), catalog_);

        if (!session->initialize()) {
            return false;
        }

        // Store session
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            active_sessions_[session_id] = std::move(session);
        }

        total_connections_++;
        return true;
    }

    void ConnectionManager::close_connection(std::uint64_t session_id)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            it->second->shutdown();
            active_sessions_.erase(it);
        }
    }

    void ConnectionManager::close_all_connections()
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [session_id, session] : active_sessions_) {
            session->shutdown();
        }
        active_sessions_.clear();
    }

    std::vector<SessionInfo> ConnectionManager::get_active_sessions() const
    {
        std::vector<SessionInfo> sessions;
        std::lock_guard<std::mutex> lock(sessions_mutex_);

        sessions.reserve(active_sessions_.size());
        for (const auto& [session_id, session] : active_sessions_) {
            SessionInfo info;
            info.session_id = session->get_session_id();
            info.client_address = session->get_client_address();
            info.client_port = session->get_client_port();
            info.protocol_version = "1.0"; // TODO: Get from protocol handler
            info.database_name = session->get_database_name();
            info.username = session->get_username();
            info.connect_time = session->get_connect_time();
            info.last_activity_time = session->get_last_activity_time();
            info.queries_executed = session->get_queries_executed();
            info.is_authenticated = session->is_authenticated();
            info.is_encrypted = false; // TODO: Get from TLS status
            sessions.push_back(info);
        }

        return sessions;
    }

    std::uint32_t ConnectionManager::get_connection_count() const
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        return static_cast<std::uint32_t>(active_sessions_.size());
    }

    bool ConnectionManager::is_connection_limit_reached() const
    {
        return get_connection_count() >= config_.max_connections;
    }

    ConnectionStats ConnectionManager::get_connection_stats() const
    {
        ConnectionStats stats;
        stats.total_connections = total_connections_.load();
        stats.active_connections = get_connection_count();
        stats.rejected_connections = rejected_connections_.load();
        stats.bytes_sent = bytes_sent_.load();
        stats.bytes_received = bytes_received_.load();
        stats.queries_processed = queries_processed_.load();

        if (stats.queries_processed > 0) {
            stats.average_query_time_ms = total_query_time_ms_.load() / stats.queries_processed;
        }

        return stats;
    }

    void ConnectionManager::update_query_stats(double query_time_ms)
    {
        queries_processed_++;
        // Atomic double += is not available, use load/store pattern
        double current = total_query_time_ms_.load();
        while (!total_query_time_ms_.compare_exchange_weak(current, current + query_time_ms)) {
            // Retry if another thread modified the value
        }
    }

    std::uint64_t ConnectionManager::generate_session_id()
    {
        return next_session_id_++;
    }

    void ConnectionManager::cleanup_inactive_sessions()
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = active_sessions_.begin();
        while (it != active_sessions_.end()) {
            if (!it->second->is_connection_alive() ||
                it->second->is_idle_timeout_exceeded(config_.connection_timeout_seconds)) {
                it->second->shutdown();
                it = active_sessions_.erase(it);
            } else {
                ++it;
            }
        }
    }

    //=============================================================================
    // NetworkServer Implementation
    //=============================================================================

    NetworkServer::NetworkServer(const NetworkServerConfig& config, CatalogManager* catalog)
        : config_(config), catalog_(catalog), running_(false), shutdown_requested_(false)
    {
        listener_ = std::make_unique<TcpListener>(config);
        connection_manager_ = std::make_unique<ConnectionManager>(config, catalog);
    }

    NetworkServer::~NetworkServer()
    {
        stop();
    }

    bool NetworkServer::start()
    {
        std::lock_guard<std::mutex> lock(server_mutex_);

        if (running_) {
            return true;
        }

        if (!listener_->start()) {
            return false;
        }

        running_ = true;
        shutdown_requested_ = false;

        // Start accept thread
        accept_thread_ = std::thread([this] { accept_loop(); });

        // Start worker threads
        initialize_worker_threads();

        return true;
    }

    void NetworkServer::stop()
    {
        {
            std::lock_guard<std::mutex> lock(server_mutex_);

            if (!running_) {
                return;
            }

            shutdown_requested_ = true;
            running_ = false;
        }

        // Stop listener
        if (listener_) {
            listener_->stop();
        }

        // Clean up threads
        cleanup_threads();

        // Close all connections
        if (connection_manager_) {
            connection_manager_->close_all_connections();
        }

        shutdown_cv_.notify_all();
    }

    bool NetworkServer::is_running() const
    {
        return running_.load();
    }

    void NetworkServer::shutdown_gracefully()
    {
        shutdown_requested_ = true;
        // Allow existing connections to finish
        // TODO: Implement graceful shutdown with timeout
        stop();
    }

    void NetworkServer::wait_for_shutdown()
    {
        std::unique_lock<std::mutex> lock(server_mutex_);
        shutdown_cv_.wait(lock, [this] { return !running_.load(); });
    }

    ConnectionStats NetworkServer::get_server_stats() const
    {
        return connection_manager_->get_connection_stats();
    }

    std::vector<SessionInfo> NetworkServer::get_active_sessions() const
    {
        return connection_manager_->get_active_sessions();
    }

    void NetworkServer::accept_loop()
    {
        while (running_ && !shutdown_requested_) {
            auto connection = listener_->accept_connection();
            if (connection) {
                // Handle connection in thread pool or dedicate thread
                connection_manager_->handle_connection(std::move(connection));
            }
        }
    }

    void NetworkServer::worker_loop()
    {
        while (running_ && !shutdown_requested_) {
            // TODO: Process work queue items
            // For now, just sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void NetworkServer::initialize_worker_threads()
    {
        std::uint32_t num_workers = config_.worker_threads;
        if (num_workers == 0) {
            num_workers = std::max(1u, std::thread::hardware_concurrency());
        }

        worker_threads_.reserve(num_workers);
        for (std::uint32_t i = 0; i < num_workers; ++i) {
            worker_threads_.emplace_back([this] { worker_loop(); });
        }
    }

    void NetworkServer::cleanup_threads()
    {
        // Wait for accept thread
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }

        // Wait for worker threads
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        worker_threads_.clear();
    }

} // namespace scratchbird::engine
