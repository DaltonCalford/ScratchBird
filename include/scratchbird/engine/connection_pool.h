#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ScratchBird
{

    /**
     * Connection states for lifecycle management
     */
    enum class ConnectionState { IDLE, ACTIVE, TERMINATING, DEAD };

    /**
     * Connection pool configuration parameters
     */
    struct ConnectionPoolConfig {
        size_t min_pool_size = 5;      // Minimum number of connections to maintain
        size_t max_pool_size = 100;    // Maximum number of connections allowed
        size_t initial_pool_size = 10; // Initial number of connections to create

        std::chrono::seconds connection_timeout{30};    // Timeout for establishing connections
        std::chrono::seconds idle_timeout{300};         // Timeout for idle connections (5 min)
        std::chrono::seconds health_check_interval{60}; // Health check frequency (1 min)

        bool enable_health_monitoring = true; // Enable periodic health checks
        size_t max_connection_failures = 3;   // Max failures before marking connection as dead
        size_t connection_queue_size = 200;   // Max queued connection requests

        // Process-specific configuration
        bool use_process_pool = true; // Use process-based pool (PostgreSQL-style)
        std::string worker_executable = "scratchbird_worker"; // Worker process executable
        std::string shared_memory_name = "/scratchbird_pool"; // Shared memory identifier
    };

    /**
     * Statistics for connection pool monitoring
     */
    struct ConnectionPoolStats {
        uint64_t total_connections{0};
        uint64_t active_connections{0};
        uint64_t idle_connections{0};
        uint64_t failed_connections{0};
        uint64_t connection_requests{0};
        uint64_t connection_timeouts{0};
        uint64_t health_check_failures{0};

        std::chrono::nanoseconds avg_connection_time{0};
        std::chrono::nanoseconds avg_request_wait_time{0};

        double get_utilization_rate() const
        {
            return total_connections > 0
                       ? static_cast<double>(active_connections) / total_connections
                       : 0.0;
        }

        double get_success_rate() const
        {
            uint64_t failures = failed_connections + connection_timeouts;
            return connection_requests > 0
                       ? static_cast<double>(connection_requests - failures) / connection_requests
                       : 1.0;
        }
    };

    /**
     * Forward declarations
     */
    class ConnectionPool;

    /**
     * Represents a pooled database connection with lifecycle management
     */
    class PooledConnection
    {
      public:
        PooledConnection(int socket_fd, pid_t worker_pid, ConnectionPool* pool);
        ~PooledConnection();

        // Non-copyable, moveable
        PooledConnection(const PooledConnection&) = delete;
        PooledConnection& operator=(const PooledConnection&) = delete;
        PooledConnection(PooledConnection&&) noexcept;
        PooledConnection& operator=(PooledConnection&&) noexcept;

        /**
         * Get the socket file descriptor for this connection
         */
        int get_socket() const
        {
            return socket_fd_;
        }

        /**
         * Get the worker process ID
         */
        pid_t get_worker_pid() const
        {
            return worker_pid_;
        }

        /**
         * Get the current connection state
         */
        ConnectionState get_state() const;

        /**
         * Set the connection state
         */
        void set_state(ConnectionState state);

        /**
         * Check if the connection is healthy
         */
        bool is_healthy() const;

        /**
         * Perform a health check on the connection
         */
        std::error_code perform_health_check();

        /**
         * Get the number of consecutive health check failures
         */
        size_t get_failure_count() const
        {
            return failure_count_;
        }

        /**
         * Reset the failure count (e.g., after successful operation)
         */
        void reset_failure_count()
        {
            failure_count_ = 0;
        }

        /**
         * Get the time when this connection was last used
         */
        std::chrono::steady_clock::time_point get_last_used() const
        {
            return last_used_;
        }

        /**
         * Update the last used timestamp
         */
        void update_last_used()
        {
            last_used_ = std::chrono::steady_clock::now();
        }

        /**
         * Return the connection to the pool
         */
        void return_to_pool();

      private:
        int socket_fd_;
        pid_t worker_pid_;
        ConnectionPool* pool_;
        mutable std::mutex state_mutex_;
        ConnectionState state_;
        size_t failure_count_;
        std::chrono::steady_clock::time_point last_used_;
        std::chrono::steady_clock::time_point created_at_;
    };

    /**
     * Factory for creating new connections
     */
    class ConnectionFactory
    {
      public:
        ConnectionFactory(const ConnectionPoolConfig& config);
        ~ConnectionFactory();

        /**
         * Create a new connection (spawns worker process)
         */
        std::unique_ptr<PooledConnection> create_connection(ConnectionPool* pool);

        /**
         * Validate factory configuration
         */
        std::error_code validate_config() const;

        /**
         * Get factory statistics
         */
        struct FactoryStats {
            std::atomic<uint64_t> connections_created{0};
            std::atomic<uint64_t> creation_failures{0};
            std::chrono::nanoseconds avg_creation_time{0};
        } stats;

      private:
        ConnectionPoolConfig config_;
        std::mutex creation_mutex_;

        /**
         * Spawn a worker process for a new connection
         */
        std::pair<int, pid_t> spawn_worker_process();

        /**
         * Set up IPC mechanisms between parent and worker
         */
        std::error_code setup_ipc(int socket_fd, pid_t worker_pid);
    };

    /**
     * Process-based connection pool implementation (PostgreSQL-style)
     *
     * Features:
     * - Process isolation for connection safety
     * - Shared memory coordination between processes
     * - Health monitoring and automatic recovery
     * - Dynamic pool sizing based on demand
     * - Connection request queuing and timeout handling
     */
    class ConnectionPool
    {
      public:
        explicit ConnectionPool(const ConnectionPoolConfig& config = {});
        ~ConnectionPool();

        // Non-copyable, non-moveable (singleton-like resource)
        ConnectionPool(const ConnectionPool&) = delete;
        ConnectionPool& operator=(const ConnectionPool&) = delete;
        ConnectionPool(ConnectionPool&&) = delete;
        ConnectionPool& operator=(ConnectionPool&&) = delete;

        /**
         * Initialize the connection pool
         */
        std::error_code initialize();

        /**
         * Shutdown the connection pool gracefully
         */
        void shutdown();

        /**
         * Get a connection from the pool
         * @param timeout Maximum time to wait for a connection
         * @return Unique pointer to pooled connection, nullptr on timeout/error
         */
        std::unique_ptr<PooledConnection>
        get_connection(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

        /**
         * Return a connection to the pool
         */
        void return_connection(std::unique_ptr<PooledConnection> connection);

        /**
         * Force refresh of all connections in the pool
         */
        void refresh_pool();

        /**
         * Get current pool statistics
         */
        ConnectionPoolStats get_stats() const;

        /**
         * Get current pool configuration
         */
        const ConnectionPoolConfig& get_config() const
        {
            return config_;
        }

        /**
         * Update pool configuration (some changes require restart)
         */
        std::error_code update_config(const ConnectionPoolConfig& new_config);

        /**
         * Validate pool configuration
         */
        static std::string validate_config(const ConnectionPoolConfig& config);

        /**
         * Check if the pool is healthy and operational
         */
        bool is_healthy() const;

        /**
         * Get the current number of connections in various states
         */
        struct PoolStatus {
            size_t total_connections;
            size_t idle_connections;
            size_t active_connections;
            size_t dead_connections;
            size_t queued_requests;
            bool is_healthy;
        };
        PoolStatus get_status() const;

      private:
        ConnectionPoolConfig config_;
        std::unique_ptr<ConnectionFactory> factory_;
        mutable std::mutex pool_mutex_;
        std::condition_variable pool_condition_;

        // Connection management
        std::queue<std::unique_ptr<PooledConnection>> idle_connections_;
        std::unordered_map<int, std::unique_ptr<PooledConnection>> active_connections_;
        std::vector<std::unique_ptr<PooledConnection>> all_connections_;

        // Request queuing
        std::queue<std::chrono::steady_clock::time_point> pending_requests_;

        // Background threads
        std::thread health_monitor_thread_;
        std::thread pool_manager_thread_;
        std::atomic<bool> shutdown_requested_{false};

        // Statistics (using atomics for thread-safe updates)
        mutable std::atomic<uint64_t> total_connections_count_{0};
        mutable std::atomic<uint64_t> active_connections_count_{0};
        mutable std::atomic<uint64_t> idle_connections_count_{0};
        mutable std::atomic<uint64_t> failed_connections_count_{0};
        mutable std::atomic<uint64_t> connection_requests_count_{0};
        mutable std::atomic<uint64_t> connection_timeouts_count_{0};
        mutable std::atomic<uint64_t> health_check_failures_count_{0};
        mutable std::mutex stats_mutex_;
        mutable std::chrono::nanoseconds avg_connection_time_{0};
        mutable std::chrono::nanoseconds avg_request_wait_time_{0};

        // Shared memory management (for process coordination)
        void* shared_memory_ptr_;
        size_t shared_memory_size_;

        /**
         * Background thread for health monitoring
         */
        void health_monitor_loop();

        /**
         * Background thread for pool management (sizing, cleanup)
         */
        void pool_manager_loop();

        /**
         * Create new connections to meet minimum pool size
         */
        void ensure_minimum_connections();

        /**
         * Remove dead or excess connections
         */
        void cleanup_connections();

        /**
         * Setup shared memory for process coordination
         */
        std::error_code setup_shared_memory();

        /**
         * Cleanup shared memory resources
         */
        void cleanup_shared_memory();

        /**
         * Update statistics thread-safely
         */
        void update_stats();

        /**
         * Handle connection request timeout
         */
        void handle_request_timeout();
    };

} // namespace ScratchBird
