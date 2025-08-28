#include "scratchbird/engine/connection_pool.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#include <process.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace ScratchBird
{

    // PooledConnection implementation
    PooledConnection::PooledConnection(int socket_fd, pid_t worker_pid, ConnectionPool* pool)
        : socket_fd_(socket_fd), worker_pid_(worker_pid), pool_(pool),
          state_(ConnectionState::IDLE), failure_count_(0),
          last_used_(std::chrono::steady_clock::now()),
          created_at_(std::chrono::steady_clock::now())
    {
    }

    PooledConnection::~PooledConnection()
    {
        if (socket_fd_ >= 0) {
#ifdef _WIN32
            closesocket(socket_fd_);
#else
            close(socket_fd_);
#endif
        }

        // Clean up worker process
        if (worker_pid_ > 0) {
#ifdef _WIN32
            // Windows process termination
            HANDLE process_handle = OpenProcess(PROCESS_TERMINATE, FALSE, worker_pid_);
            if (process_handle != NULL) {
                TerminateProcess(process_handle, 0);
                CloseHandle(process_handle);
            }
#else
            // Send SIGTERM first, then SIGKILL if necessary
            kill(worker_pid_, SIGTERM);

            // Wait briefly for graceful shutdown
            int status;
            pid_t result = waitpid(worker_pid_, &status, WNOHANG);
            if (result == 0) {
                // Process still running, force kill
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                kill(worker_pid_, SIGKILL);
                waitpid(worker_pid_, &status, 0);
            }
#endif
        }
    }

    PooledConnection::PooledConnection(PooledConnection&& other) noexcept
        : socket_fd_(other.socket_fd_), worker_pid_(other.worker_pid_), pool_(other.pool_),
          state_(other.state_), failure_count_(other.failure_count_), last_used_(other.last_used_),
          created_at_(other.created_at_)
    {
        other.socket_fd_ = -1;
        other.worker_pid_ = -1;
        other.pool_ = nullptr;
    }

    PooledConnection& PooledConnection::operator=(PooledConnection&& other) noexcept
    {
        if (this != &other) {
            // Cleanup current resources
            if (socket_fd_ >= 0) {
#ifdef _WIN32
                closesocket(socket_fd_);
#else
                close(socket_fd_);
#endif
            }

            // Move from other
            socket_fd_ = other.socket_fd_;
            worker_pid_ = other.worker_pid_;
            pool_ = other.pool_;
            state_ = other.state_;
            failure_count_ = other.failure_count_;
            last_used_ = other.last_used_;
            created_at_ = other.created_at_;

            // Reset other
            other.socket_fd_ = -1;
            other.worker_pid_ = -1;
            other.pool_ = nullptr;
        }
        return *this;
    }

    ConnectionState PooledConnection::get_state() const
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return state_;
    }

    void PooledConnection::set_state(ConnectionState state)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = state;
    }

    bool PooledConnection::is_healthy() const
    {
        if (socket_fd_ < 0 || worker_pid_ <= 0) {
            return false;
        }

        ConnectionState current_state = get_state();
        if (current_state == ConnectionState::DEAD ||
            current_state == ConnectionState::TERMINATING) {
            return false;
        }

        // Check if worker process is still alive
#ifdef _WIN32
        HANDLE process_handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, worker_pid_);
        if (process_handle == NULL) {
            return false;
        }

        DWORD exit_code;
        bool is_running =
            GetExitCodeProcess(process_handle, &exit_code) && (exit_code == STILL_ACTIVE);
        CloseHandle(process_handle);
        return is_running;
#else
        // Send signal 0 to check if process exists
        return kill(worker_pid_, 0) == 0;
#endif
    }

    std::error_code PooledConnection::perform_health_check()
    {
        if (!is_healthy()) {
            failure_count_++;
            set_state(ConnectionState::DEAD);
            return std::make_error_code(std::errc::connection_aborted);
        }

        // Simple socket health check - try to get socket options
        int error = 0;
        socklen_t len = sizeof(error);

#ifdef _WIN32
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) !=
            0) {
#else
        if (getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
#endif
            failure_count_++;
            return std::make_error_code(static_cast<std::errc>(errno));
        }

        if (error != 0) {
            failure_count_++;
            return std::make_error_code(static_cast<std::errc>(error));
        }

        // Health check passed
        reset_failure_count();
        return {};
    }

    void PooledConnection::return_to_pool()
    {
        if (pool_) {
            update_last_used();
            set_state(ConnectionState::IDLE);
            // Note: Actual return to pool handled by ConnectionPool::return_connection
        }
    }

    // ConnectionFactory implementation
    ConnectionFactory::ConnectionFactory(const ConnectionPoolConfig& config) : config_(config) {}

    ConnectionFactory::~ConnectionFactory() = default;

    std::unique_ptr<PooledConnection> ConnectionFactory::create_connection(ConnectionPool* pool)
    {
        std::lock_guard<std::mutex> lock(creation_mutex_);

        auto start_time = std::chrono::steady_clock::now();

        try {
            auto [socket_fd, worker_pid] = spawn_worker_process();

            if (socket_fd < 0 || worker_pid <= 0) {
                stats.creation_failures++;
                return nullptr;
            }

            // Set up IPC mechanisms
            if (auto ec = setup_ipc(socket_fd, worker_pid); ec) {
                stats.creation_failures++;
#ifdef _WIN32
                closesocket(socket_fd);
#else
                close(socket_fd);
                kill(worker_pid, SIGKILL);
#endif
                return nullptr;
            }

            auto connection = std::make_unique<PooledConnection>(socket_fd, worker_pid, pool);

            // Update statistics
            stats.connections_created++;
            auto creation_time = std::chrono::steady_clock::now() - start_time;

            if (stats.connections_created == 1) {
                stats.avg_creation_time = creation_time;
            } else {
                stats.avg_creation_time = std::chrono::nanoseconds(
                    (stats.avg_creation_time.count() * 7 + creation_time.count()) / 8);
            }

            return connection;

        } catch (const std::exception& e) {
            stats.creation_failures++;
            return nullptr;
        }
    }

    std::error_code ConnectionFactory::validate_config() const
    {
        if (config_.worker_executable.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        if (config_.min_pool_size > config_.max_pool_size) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        if (config_.initial_pool_size > config_.max_pool_size) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        return {};
    }

    std::pair<int, pid_t> ConnectionFactory::spawn_worker_process()
    {
#ifdef _WIN32
        // Windows implementation - simplified for now
        // In a real implementation, this would use CreateProcess
        return {-1, -1};
#else
        // Create a socket pair for parent-worker communication
        int socket_pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair) != 0) {
            return {-1, -1};
        }

        pid_t worker_pid = fork();

        if (worker_pid == -1) {
            // Fork failed
            close(socket_pair[0]);
            close(socket_pair[1]);
            return {-1, -1};
        }

        if (worker_pid == 0) {
            // Child process - become the worker
            close(socket_pair[0]); // Close parent end

            // Set up worker process environment
            // In a real implementation, this would exec the worker executable
            // For now, we'll simulate a worker that just waits

            // Redirect stdin/stdout to the socket
            dup2(socket_pair[1], STDIN_FILENO);
            dup2(socket_pair[1], STDOUT_FILENO);

            // Simple worker loop - wait for commands
            char buffer[1024];
            while (true) {
                ssize_t bytes_read = read(socket_pair[1], buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break; // Parent closed connection
                }

                // Echo back for basic communication test
                write(socket_pair[1], "OK\n", 3);
            }

            close(socket_pair[1]);
            _exit(0);
        } else {
            // Parent process
            close(socket_pair[1]); // Close worker end
            return {socket_pair[0], worker_pid};
        }
#endif
    }

    std::error_code ConnectionFactory::setup_ipc(int socket_fd, pid_t /*worker_pid*/)
    {
        // Basic socket configuration for IPC
        if (socket_fd < 0) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        // Set socket to non-blocking mode for async operations
#ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(socket_fd, FIONBIO, &mode) != 0) {
            return std::make_error_code(std::errc::io_error);
        }
#else
        int flags = fcntl(socket_fd, F_GETFL, 0);
        if (flags == -1 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            return std::make_error_code(std::errc::io_error);
        }
#endif

        return {};
    }

    // ConnectionPool implementation
    ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
        : config_(config), factory_(std::make_unique<ConnectionFactory>(config)),
          shared_memory_ptr_(nullptr), shared_memory_size_(0)
    {
    }

    ConnectionPool::~ConnectionPool()
    {
        shutdown();
    }

    std::error_code ConnectionPool::initialize()
    {
        // Validate configuration
        if (auto validation_error = validate_config(config_); !validation_error.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        // Validate factory
        if (auto ec = factory_->validate_config(); ec) {
            return ec;
        }

        // Set up shared memory for process coordination
        if (auto ec = setup_shared_memory(); ec) {
            return ec;
        }

        // Create initial connections
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            for (size_t i = 0; i < config_.initial_pool_size; ++i) {
                auto connection = factory_->create_connection(this);
                if (connection) {
                    all_connections_.push_back(std::move(connection));
                    idle_connections_.push(std::move(all_connections_.back()));
                    total_connections_count_++;
                    idle_connections_count_++;
                }
            }
        }

        // Start background threads
        shutdown_requested_ = false;

        if (config_.enable_health_monitoring) {
            health_monitor_thread_ = std::thread(&ConnectionPool::health_monitor_loop, this);
        }

        pool_manager_thread_ = std::thread(&ConnectionPool::pool_manager_loop, this);

        return {};
    }

    void ConnectionPool::shutdown()
    {
        // Signal shutdown to background threads
        shutdown_requested_ = true;
        pool_condition_.notify_all();

        // Wait for background threads to finish
        if (health_monitor_thread_.joinable()) {
            health_monitor_thread_.join();
        }

        if (pool_manager_thread_.joinable()) {
            pool_manager_thread_.join();
        }

        // Clean up connections
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);

            // Clear idle connections queue
            while (!idle_connections_.empty()) {
                idle_connections_.pop();
            }

            // Clear active connections
            active_connections_.clear();

            // Clear all connections (this will trigger destructors)
            all_connections_.clear();
        }

        // Clean up shared memory
        cleanup_shared_memory();
    }

    std::unique_ptr<PooledConnection>
    ConnectionPool::get_connection(std::chrono::milliseconds timeout)
    {
        auto start_time = std::chrono::steady_clock::now();

        std::unique_lock<std::mutex> lock(pool_mutex_);

        // Record the request
        connection_requests_count_++;
        pending_requests_.push(start_time);

        // Wait for available connection or timeout
        bool connection_available = pool_condition_.wait_for(
            lock, timeout, [this] { return !idle_connections_.empty() || shutdown_requested_; });

        // Remove request from pending queue
        if (!pending_requests_.empty()) {
            pending_requests_.pop();
        }

        if (shutdown_requested_) {
            return nullptr;
        }

        if (!connection_available || idle_connections_.empty()) {
            // Timeout occurred
            connection_timeouts_count_++;
            return nullptr;
        }

        // Get connection from idle queue
        auto connection = std::move(idle_connections_.front());
        idle_connections_.pop();

        // Move to active connections
        int socket_fd = connection->get_socket();
        connection->set_state(ConnectionState::ACTIVE);
        connection->update_last_used();

        active_connections_[socket_fd] = std::move(connection);

        // Update statistics
        idle_connections_count_--;
        active_connections_count_++;

        auto wait_time = std::chrono::steady_clock::now() - start_time;
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            if (connection_requests_count_ == 1) {
                avg_request_wait_time_ = wait_time;
            } else {
                avg_request_wait_time_ = std::chrono::nanoseconds(
                    (avg_request_wait_time_.count() * 7 + wait_time.count()) / 8);
            }
        }

        return std::move(active_connections_[socket_fd]);
    }

    void ConnectionPool::return_connection(std::unique_ptr<PooledConnection> connection)
    {
        if (!connection) {
            return;
        }

        std::lock_guard<std::mutex> lock(pool_mutex_);

        int socket_fd = connection->get_socket();

        // Remove from active connections
        active_connections_.erase(socket_fd);
        active_connections_count_--;

        // Check if connection is still healthy
        if (connection->is_healthy()) {
            connection->set_state(ConnectionState::IDLE);
            idle_connections_.push(std::move(connection));
            idle_connections_count_++;
            pool_condition_.notify_one();
        } else {
            // Connection is dead, remove it from all_connections_
            connection->set_state(ConnectionState::DEAD);
            auto it = std::find_if(
                all_connections_.begin(), all_connections_.end(),
                [socket_fd](const auto& conn) { return conn && conn->get_socket() == socket_fd; });
            if (it != all_connections_.end()) {
                all_connections_.erase(it);
            }
            total_connections_count_--;
            failed_connections_count_++;
        }
    }

    void ConnectionPool::refresh_pool()
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        // Mark all idle connections for replacement
        std::queue<std::unique_ptr<PooledConnection>> new_idle_queue;

        while (!idle_connections_.empty()) {
            auto connection = std::move(idle_connections_.front());
            idle_connections_.pop();
            connection->set_state(ConnectionState::TERMINATING);
            idle_connections_count_--;
            total_connections_count_--;
        }

        // Create new connections to replace them
        ensure_minimum_connections();
    }

    ConnectionPoolStats ConnectionPool::get_stats() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        // Copy atomic values to the result struct
        ConnectionPoolStats result;
        result.total_connections = total_connections_count_.load();
        result.active_connections = active_connections_count_.load();
        result.idle_connections = idle_connections_count_.load();
        result.failed_connections = failed_connections_count_.load();
        result.connection_requests = connection_requests_count_.load();
        result.connection_timeouts = connection_timeouts_count_.load();
        result.health_check_failures = health_check_failures_count_.load();
        result.avg_connection_time = avg_connection_time_;
        result.avg_request_wait_time = avg_request_wait_time_;

        return result;
    }

    std::error_code ConnectionPool::update_config(const ConnectionPoolConfig& new_config)
    {
        if (auto validation_error = validate_config(new_config); !validation_error.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        std::lock_guard<std::mutex> lock(pool_mutex_);
        config_ = new_config;

        // Update factory config
        factory_ = std::make_unique<ConnectionFactory>(config_);

        return {};
    }

    std::string ConnectionPool::validate_config(const ConnectionPoolConfig& config)
    {
        std::ostringstream errors;

        if (config.min_pool_size == 0) {
            errors << "min_pool_size must be greater than 0; ";
        }

        if (config.max_pool_size == 0) {
            errors << "max_pool_size must be greater than 0; ";
        }

        if (config.min_pool_size > config.max_pool_size) {
            errors << "min_pool_size cannot be greater than max_pool_size; ";
        }

        if (config.initial_pool_size > config.max_pool_size) {
            errors << "initial_pool_size cannot be greater than max_pool_size; ";
        }

        if (config.connection_timeout.count() <= 0) {
            errors << "connection_timeout must be positive; ";
        }

        if (config.idle_timeout.count() <= 0) {
            errors << "idle_timeout must be positive; ";
        }

        if (config.worker_executable.empty()) {
            errors << "worker_executable cannot be empty; ";
        }

        return errors.str();
    }

    bool ConnectionPool::is_healthy() const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        // Pool is healthy if:
        // 1. Not shutting down
        // 2. Has some connections available
        // 3. Success rate is reasonable
        if (shutdown_requested_) {
            return false;
        }

        if (total_connections_count_ == 0) {
            return false;
        }

        // Calculate success rate locally
        uint64_t requests = connection_requests_count_.load();
        uint64_t failures = failed_connections_count_.load() + connection_timeouts_count_.load();
        double success_rate =
            requests > 0 ? static_cast<double>(requests - failures) / requests : 1.0;
        return success_rate >= 0.8; // 80% success rate threshold
    }

    ConnectionPool::PoolStatus ConnectionPool::get_status() const
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);

        PoolStatus status;
        status.total_connections = total_connections_count_.load();
        status.idle_connections = idle_connections_.size();
        status.active_connections = active_connections_.size();
        status.dead_connections = failed_connections_count_.load();
        status.queued_requests = pending_requests_.size();
        status.is_healthy = is_healthy();

        return status;
    }

    void ConnectionPool::health_monitor_loop()
    {
        while (!shutdown_requested_) {
            std::this_thread::sleep_for(config_.health_check_interval);

            if (shutdown_requested_)
                break;

            std::lock_guard<std::mutex> lock(pool_mutex_);

            // Check health of all connections
            for (auto& connection : all_connections_) {
                if (!connection)
                    continue;

                if (auto ec = connection->perform_health_check(); ec) {
                    health_check_failures_count_++;

                    if (connection->get_failure_count() >= config_.max_connection_failures) {
                        connection->set_state(ConnectionState::DEAD);
                    }
                }
            }

            // Remove dead connections and create new ones if needed
            cleanup_connections();
            ensure_minimum_connections();
        }
    }

    void ConnectionPool::pool_manager_loop()
    {
        while (!shutdown_requested_) {
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Check every 10 seconds

            if (shutdown_requested_)
                break;

            std::lock_guard<std::mutex> lock(pool_mutex_);

            // Clean up idle connections that have timed out
            auto now = std::chrono::steady_clock::now();
            std::queue<std::unique_ptr<PooledConnection>> fresh_idle_queue;

            while (!idle_connections_.empty()) {
                auto connection = std::move(idle_connections_.front());
                idle_connections_.pop();

                auto idle_time = now - connection->get_last_used();
                if (idle_time > config_.idle_timeout) {
                    // Connection has been idle too long, remove it
                    connection->set_state(ConnectionState::TERMINATING);
                    idle_connections_count_--;
                    total_connections_count_--;
                } else {
                    // Keep this connection
                    fresh_idle_queue.push(std::move(connection));
                }
            }

            idle_connections_ = std::move(fresh_idle_queue);

            // Ensure minimum connections are maintained
            ensure_minimum_connections();

            // Update statistics
            update_stats();
        }
    }

    void ConnectionPool::ensure_minimum_connections()
    {
        size_t current_total = total_connections_count_.load();

        while (current_total < config_.min_pool_size && current_total < config_.max_pool_size) {
            auto connection = factory_->create_connection(this);
            if (!connection) {
                break; // Failed to create connection
            }

            all_connections_.push_back(std::move(connection));
            idle_connections_.push(std::move(all_connections_.back()));

            total_connections_count_++;
            idle_connections_count_++;
            current_total++;
        }

        if (!idle_connections_.empty()) {
            pool_condition_.notify_all();
        }
    }

    void ConnectionPool::cleanup_connections()
    {
        // Remove dead connections from all_connections_
        all_connections_.erase(std::remove_if(all_connections_.begin(), all_connections_.end(),
                                              [](const auto& conn) {
                                                  return !conn ||
                                                         conn->get_state() == ConnectionState::DEAD;
                                              }),
                               all_connections_.end());
    }

    std::error_code ConnectionPool::setup_shared_memory()
    {
        if (!config_.use_process_pool) {
            return {}; // No shared memory needed for thread-based pool
        }

#ifdef _WIN32
        // Windows shared memory implementation would go here
        return {};
#else
        // Linux/Unix shared memory using shm_open
        shared_memory_size_ = 4096; // 4KB should be enough for coordination

        int shm_fd =
            shm_open(config_.shared_memory_name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if (shm_fd == -1) {
            return std::make_error_code(static_cast<std::errc>(errno));
        }

        // Set the size of shared memory
        if (ftruncate(shm_fd, shared_memory_size_) == -1) {
            close(shm_fd);
            return std::make_error_code(static_cast<std::errc>(errno));
        }

        // Map shared memory
        shared_memory_ptr_ =
            mmap(nullptr, shared_memory_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

        close(shm_fd); // File descriptor no longer needed after mmap

        if (shared_memory_ptr_ == MAP_FAILED) {
            return std::make_error_code(static_cast<std::errc>(errno));
        }

        // Initialize shared memory structure
        std::memset(shared_memory_ptr_, 0, shared_memory_size_);

        return {};
#endif
    }

    void ConnectionPool::cleanup_shared_memory()
    {
        if (!shared_memory_ptr_) {
            return;
        }

#ifdef _WIN32
        // Windows cleanup would go here
#else
        // Unmap shared memory
        if (munmap(shared_memory_ptr_, shared_memory_size_) == -1) {
            // Log error but continue cleanup
        }

        // Remove shared memory object
        shm_unlink(config_.shared_memory_name.c_str());
#endif

        shared_memory_ptr_ = nullptr;
        shared_memory_size_ = 0;
    }

    void ConnectionPool::update_stats()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);

        // Statistics are mostly updated in real-time by other methods
        // This method can be used for periodic calculations or cleanup

        // Calculate current utilization
        uint64_t total = total_connections_count_.load();
        [[maybe_unused]] uint64_t active = active_connections_count_.load();

        if (total == 0) {
            // Reset averages if no connections
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            avg_connection_time_ = std::chrono::nanoseconds(0);
            avg_request_wait_time_ = std::chrono::nanoseconds(0);
        }
    }

    void ConnectionPool::handle_request_timeout()
    {
        // This method can be called when a connection request times out
        // to perform any necessary cleanup or logging
        connection_timeouts_count_++;
    }

} // namespace ScratchBird
