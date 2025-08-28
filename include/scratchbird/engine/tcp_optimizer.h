#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <mstcpip.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ScratchBird
{

    /**
     * Network configuration parameters for TCP optimization
     */
    struct NetworkConfig {
        bool tcp_nodelay = true;                         // Disable Nagle's algorithm
        std::chrono::seconds tcp_keepalive_idle{600};    // 10 minutes until first keepalive probe
        std::chrono::seconds tcp_keepalive_interval{30}; // 30 seconds between keepalive probes
        int tcp_keepalive_count = 3;                     // 3 failed probes before declaring dead
        std::chrono::seconds tcp_user_timeout{0};        // TCP user timeout (0 = system default)
        size_t socket_recv_buffer = 262144;              // 256KB receive buffer
        size_t socket_send_buffer = 262144;              // 256KB send buffer
        bool reuse_port = false;                         // SO_REUSEPORT (Linux only)

        // Platform-specific optimizations
        bool enable_fast_open = false;    // TCP Fast Open where supported
        bool enable_defer_accept = false; // TCP_DEFER_ACCEPT (Linux only)
        int listen_backlog = 128;         // Connection backlog size
    };

    /**
     * TCP socket optimizer for high-performance network connections
     *
     * Implements PostgreSQL and Firebird inspired network optimizations:
     * - TCP_NODELAY for low latency
     * - Keepalive configuration for connection health
     * - Platform-specific socket optimizations
     * - Configurable buffer sizes for throughput
     */
    class TCPOptimizer
    {
      public:
        TCPOptimizer() = default;
        ~TCPOptimizer() = default;

        // Non-copyable, moveable
        TCPOptimizer(const TCPOptimizer&) = delete;
        TCPOptimizer& operator=(const TCPOptimizer&) = delete;
        TCPOptimizer(TCPOptimizer&&) = default;
        TCPOptimizer& operator=(TCPOptimizer&&) = default;

        /**
         * Configure socket with optimal settings for database connections
         * @param sockfd Socket file descriptor to optimize
         * @param config Network configuration parameters
         * @return Error code, empty on success
         */
        std::error_code configure_socket(int sockfd, const NetworkConfig& config);

        /**
         * Configure listening socket with server-specific optimizations
         * @param sockfd Listening socket file descriptor
         * @param config Network configuration parameters
         * @return Error code, empty on success
         */
        std::error_code configure_listen_socket(int sockfd, const NetworkConfig& config);

        /**
         * Configure client socket for optimal client connections
         * @param sockfd Client socket file descriptor
         * @param config Network configuration parameters
         * @return Error code, empty on success
         */
        std::error_code configure_client_socket(int sockfd, const NetworkConfig& config);

        /**
         * Get current socket configuration
         * @param sockfd Socket file descriptor to query
         * @return Current network configuration, or default config on error
         */
        NetworkConfig get_socket_config(int sockfd) const;

        /**
         * Validate network configuration parameters
         * @param config Configuration to validate
         * @return Error message if invalid, empty string if valid
         */
        static std::string validate_config(const NetworkConfig& config);

        /**
         * Get platform-specific socket optimization capabilities
         * @return String describing available optimizations
         */
        static std::string get_platform_capabilities();

      private:
        /**
         * Set TCP_NODELAY option to disable Nagle's algorithm
         */
        std::error_code set_tcp_nodelay(int sockfd, bool enable);

        /**
         * Configure TCP keepalive parameters
         */
        std::error_code set_keepalive_options(int sockfd, const NetworkConfig& config);

        /**
         * Set socket buffer sizes
         */
        std::error_code set_buffer_sizes(int sockfd, const NetworkConfig& config);

        /**
         * Apply platform-specific optimizations
         */
        std::error_code set_platform_specific_options(int sockfd, const NetworkConfig& config);

        /**
         * Configure Linux-specific socket options
         */
        std::error_code configure_linux_options(int sockfd, const NetworkConfig& config);

        /**
         * Configure Windows-specific socket options
         */
        std::error_code configure_windows_options(int sockfd, const NetworkConfig& config);

        /**
         * Get system error code from socket operation
         */
        std::error_code get_socket_error() const;
    };

    /**
     * RAII wrapper for socket file descriptor with automatic optimization
     */
    class OptimizedSocket
    {
      public:
        explicit OptimizedSocket(int sockfd, const NetworkConfig& config = {});
        ~OptimizedSocket();

        // Non-copyable, moveable
        OptimizedSocket(const OptimizedSocket&) = delete;
        OptimizedSocket& operator=(const OptimizedSocket&) = delete;
        OptimizedSocket(OptimizedSocket&& other) noexcept;
        OptimizedSocket& operator=(OptimizedSocket&& other) noexcept;

        /**
         * Get socket file descriptor
         */
        int get() const
        {
            return sockfd_;
        }

        /**
         * Get current configuration
         */
        const NetworkConfig& config() const
        {
            return config_;
        }

        /**
         * Reconfigure socket with new parameters
         */
        std::error_code reconfigure(const NetworkConfig& new_config);

        /**
         * Release ownership of socket (caller responsible for closing)
         */
        int release();

        /**
         * Check if socket is valid
         */
        bool valid() const
        {
            return sockfd_ >= 0;
        }

      private:
        int sockfd_ = -1;
        NetworkConfig config_;
        TCPOptimizer optimizer_;
    };

    /**
     * Network statistics for monitoring TCP optimization effectiveness
     */
    struct NetworkStats {
        uint64_t connections_accepted = 0;
        uint64_t connections_failed = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t keepalive_timeouts = 0;
        uint64_t socket_errors = 0;

        std::chrono::nanoseconds avg_connection_time{0};
        std::chrono::nanoseconds avg_send_latency{0};
        std::chrono::nanoseconds avg_recv_latency{0};

        double connection_success_rate() const
        {
            uint64_t total = connections_accepted + connections_failed;
            return total > 0 ? static_cast<double>(connections_accepted) / total : 1.0;
        }
    };

    /**
     * Network statistics collector for TCP optimization monitoring
     */
    class NetworkStatsCollector
    {
      public:
        NetworkStatsCollector() = default;

        void record_connection_accepted(std::chrono::nanoseconds connection_time);
        void record_connection_failed();
        void record_bytes_sent(uint64_t bytes, std::chrono::nanoseconds latency);
        void record_bytes_received(uint64_t bytes, std::chrono::nanoseconds latency);
        void record_keepalive_timeout();
        void record_socket_error();

        NetworkStats get_stats() const;
        void reset_stats();

      private:
        mutable std::mutex stats_mutex_;
        NetworkStats stats_;
    };

} // namespace ScratchBird
