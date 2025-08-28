#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <system_error>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

namespace ScratchBird
{

    /**
     * Network buffer configuration parameters
     */
    struct NetworkBufferConfig {
        // Default buffer sizes (64KB as specified in requirements)
        size_t default_recv_buffer_size = 65536;    // 64KB receive buffer
        size_t default_send_buffer_size = 65536;    // 64KB send buffer
        
        // Buffer size limits
        size_t min_buffer_size = 4096;              // 4KB minimum
        size_t max_buffer_size = 16777216;          // 16MB maximum
        
        // Auto-tuning parameters
        bool enable_auto_tuning = true;             // Enable adaptive buffer sizing
        std::chrono::seconds tuning_interval{30};   // Auto-tuning check interval
        double utilization_threshold = 0.8;         // Buffer utilization threshold for tuning
        double growth_factor = 1.5;                 // Factor to grow buffers when needed
        double shrink_factor = 0.75;                // Factor to shrink buffers when underutilized
        
        // Monitoring parameters
        bool enable_monitoring = true;              // Enable buffer monitoring
        std::chrono::seconds stats_collection_interval{10}; // Statistics collection frequency
        size_t overflow_alert_threshold = 5;        // Number of overflows before alert
        double underutilization_threshold = 0.2;    // Utilization below which to alert
    };

    /**
     * Network buffer statistics for a single connection
     */
    struct NetworkBufferStats {
        // Buffer sizes
        size_t recv_buffer_size = 0;
        size_t send_buffer_size = 0;
        
        // Usage statistics
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> recv_buffer_full_events{0};
        std::atomic<uint64_t> send_buffer_full_events{0};
        
        // Efficiency metrics
        std::atomic<uint64_t> recv_operations{0};
        std::atomic<uint64_t> send_operations{0};
        std::chrono::nanoseconds avg_recv_latency{0};
        std::chrono::nanoseconds avg_send_latency{0};
        
        // Timestamps
        std::chrono::steady_clock::time_point last_tuning_time;
        std::chrono::steady_clock::time_point created_time;
        
        // Calculate buffer utilization rates
        double get_recv_utilization() const {
            if (recv_buffer_size == 0) return 0.0;
            return static_cast<double>(bytes_received.load()) / recv_buffer_size;
        }
        
        double get_send_utilization() const {
            if (send_buffer_size == 0) return 0.0;
            return static_cast<double>(bytes_sent.load()) / send_buffer_size;
        }
        
        // Calculate efficiency ratios
        double get_recv_efficiency() const {
            uint64_t ops = recv_operations.load();
            uint64_t bytes = bytes_received.load();
            return ops > 0 ? static_cast<double>(bytes) / ops : 0.0;
        }
        
        double get_send_efficiency() const {
            uint64_t ops = send_operations.load();
            uint64_t bytes = bytes_sent.load();
            return ops > 0 ? static_cast<double>(bytes) / ops : 0.0;
        }
    };

    /**
     * Aggregated network buffer statistics for monitoring
     */
    struct AggregatedBufferStats {
        uint64_t total_connections = 0;
        uint64_t total_recv_buffers_allocated = 0;
        uint64_t total_send_buffers_allocated = 0;
        uint64_t total_bytes_received = 0;
        uint64_t total_bytes_sent = 0;
        uint64_t total_overflow_events = 0;
        uint64_t connections_with_auto_tuning = 0;
        uint64_t tuning_operations_performed = 0;
        
        double avg_recv_buffer_size = 0.0;
        double avg_send_buffer_size = 0.0;
        double avg_recv_utilization = 0.0;
        double avg_send_utilization = 0.0;
        double overall_efficiency = 0.0;
        
        std::chrono::steady_clock::time_point last_update_time;
    };

    /**
     * Network buffer alerts and notifications
     */
    enum class BufferAlertType {
        OVERFLOW_DETECTED,           // Buffer overflow events detected
        UNDERUTILIZATION,           // Buffer significantly underutilized
        TUNING_FAILED,              // Auto-tuning operation failed
        BUFFER_SIZE_LIMIT_REACHED,  // Buffer hit size limits
        HIGH_LATENCY_DETECTED       // High send/recv latency detected
    };

    struct BufferAlert {
        BufferAlertType type;
        int socket_fd;
        std::string message;
        std::chrono::steady_clock::time_point timestamp;
        
        // Additional context data
        size_t buffer_size_before = 0;
        size_t buffer_size_after = 0;
        double utilization_rate = 0.0;
        uint64_t event_count = 0;
    };

    /**
     * Network buffer manager for configurable buffer optimization
     * 
     * Features:
     * - Configurable socket buffer sizes (SO_RCVBUF, SO_SNDBUF)
     * - Auto-tuning based on connection patterns and utilization
     * - Real-time monitoring and statistics collection
     * - Alert generation for buffer issues
     * - Per-connection and aggregated metrics
     */
    class NetworkBufferManager
    {
      public:
        explicit NetworkBufferManager(const NetworkBufferConfig& config = {});
        ~NetworkBufferManager();

        // Non-copyable, non-moveable (singleton-like resource manager)
        NetworkBufferManager(const NetworkBufferManager&) = delete;
        NetworkBufferManager& operator=(const NetworkBufferManager&) = delete;
        NetworkBufferManager(NetworkBufferManager&&) = delete;
        NetworkBufferManager& operator=(NetworkBufferManager&&) = delete;

        /**
         * Initialize buffer manager and start monitoring threads
         */
        std::error_code initialize();

        /**
         * Shutdown buffer manager and cleanup resources
         */
        void shutdown();

        /**
         * Configure network buffers for a socket
         * @param socket_fd Socket file descriptor
         * @param recv_size Receive buffer size (0 for default)
         * @param send_size Send buffer size (0 for default)
         * @return Error code, empty on success
         */
        std::error_code configure_socket_buffers(int socket_fd, 
                                               size_t recv_size = 0, 
                                               size_t send_size = 0);

        /**
         * Register a socket for monitoring and auto-tuning
         * @param socket_fd Socket file descriptor
         * @return Error code, empty on success
         */
        std::error_code register_socket(int socket_fd);

        /**
         * Unregister a socket from monitoring
         * @param socket_fd Socket file descriptor
         */
        void unregister_socket(int socket_fd);

        /**
         * Record network I/O operation for statistics
         * @param socket_fd Socket file descriptor
         * @param bytes_transferred Number of bytes transferred
         * @param is_send True for send operation, false for receive
         * @param operation_latency Time taken for the operation
         */
        void record_io_operation(int socket_fd, 
                                size_t bytes_transferred, 
                                bool is_send, 
                                std::chrono::nanoseconds operation_latency);

        /**
         * Record buffer overflow event
         * @param socket_fd Socket file descriptor
         * @param is_send_buffer True for send buffer overflow, false for receive
         */
        void record_buffer_overflow(int socket_fd, bool is_send_buffer);

        /**
         * Manually trigger buffer tuning for a socket
         * @param socket_fd Socket file descriptor
         * @return New buffer sizes or error
         */
        std::error_code tune_socket_buffers(int socket_fd);

        /**
         * Get buffer statistics for a specific socket
         * @param socket_fd Socket file descriptor
         * @return Buffer statistics or nullptr if not found
         */
        std::shared_ptr<NetworkBufferStats> get_socket_stats(int socket_fd) const;

        /**
         * Get aggregated buffer statistics for all sockets
         * @return Aggregated statistics
         */
        AggregatedBufferStats get_aggregated_stats() const;

        /**
         * Get pending alerts
         * @param clear_alerts Whether to clear alerts after retrieval
         * @return Vector of pending alerts
         */
        std::vector<BufferAlert> get_pending_alerts(bool clear_alerts = true);

        /**
         * Update buffer manager configuration
         * @param new_config New configuration
         * @return Error code, empty on success
         */
        std::error_code update_config(const NetworkBufferConfig& new_config);

        /**
         * Get current configuration
         */
        const NetworkBufferConfig& get_config() const { return config_; }

        /**
         * Validate buffer configuration
         */
        static std::string validate_config(const NetworkBufferConfig& config);

        /**
         * Get optimal buffer size recommendation based on connection characteristics
         * @param socket_fd Socket file descriptor
         * @param is_send_buffer True for send buffer, false for receive buffer
         * @return Recommended buffer size
         */
        size_t get_recommended_buffer_size(int socket_fd, bool is_send_buffer) const;

      private:
        NetworkBufferConfig config_;
        mutable std::mutex manager_mutex_;
        std::atomic<bool> shutdown_requested_{false};
        
        // Socket tracking
        std::unordered_map<int, std::shared_ptr<NetworkBufferStats>> socket_stats_;
        mutable std::mutex socket_stats_mutex_;
        
        // Background monitoring
        std::thread monitoring_thread_;
        std::thread tuning_thread_;
        
        // Alert management
        std::vector<BufferAlert> pending_alerts_;
        mutable std::mutex alerts_mutex_;
        
        // Aggregated statistics
        mutable AggregatedBufferStats aggregated_stats_;
        mutable std::mutex aggregated_stats_mutex_;

        /**
         * Background thread for monitoring buffer utilization
         */
        void monitoring_loop();

        /**
         * Background thread for auto-tuning buffer sizes
         */
        void tuning_loop();

        /**
         * Apply socket buffer configuration
         */
        std::error_code apply_socket_buffer_config(int socket_fd, 
                                                 size_t recv_size, 
                                                 size_t send_size);

        /**
         * Get current socket buffer sizes
         */
        std::pair<size_t, size_t> get_current_socket_buffer_sizes(int socket_fd) const;

        /**
         * Calculate optimal buffer size based on usage patterns
         */
        size_t calculate_optimal_buffer_size(const NetworkBufferStats& stats, 
                                           bool is_send_buffer) const;

        /**
         * Generate alert for buffer issue
         */
        void generate_alert(BufferAlertType type, 
                          int socket_fd, 
                          const std::string& message,
                          const NetworkBufferStats& stats);

        /**
         * Update aggregated statistics
         */
        void update_aggregated_stats();

        /**
         * Check if socket needs buffer tuning
         */
        bool needs_buffer_tuning(const NetworkBufferStats& stats) const;

        /**
         * Perform buffer tuning for a socket
         */
        std::error_code perform_buffer_tuning(int socket_fd, NetworkBufferStats& stats);
    };

    /**
     * RAII wrapper for managed network socket with automatic buffer optimization
     */
    class ManagedNetworkSocket
    {
      public:
        ManagedNetworkSocket(int socket_fd, NetworkBufferManager& manager);
        ~ManagedNetworkSocket();

        // Non-copyable, moveable
        ManagedNetworkSocket(const ManagedNetworkSocket&) = delete;
        ManagedNetworkSocket& operator=(const ManagedNetworkSocket&) = delete;
        ManagedNetworkSocket(ManagedNetworkSocket&& other) noexcept;
        ManagedNetworkSocket& operator=(ManagedNetworkSocket&& other) noexcept;

        /**
         * Get socket file descriptor
         */
        int get_socket() const { return socket_fd_; }

        /**
         * Record a send operation
         */
        void record_send(size_t bytes_sent, std::chrono::nanoseconds latency);

        /**
         * Record a receive operation
         */
        void record_receive(size_t bytes_received, std::chrono::nanoseconds latency);

        /**
         * Record buffer overflow
         */
        void record_overflow(bool is_send_buffer);

        /**
         * Get socket statistics
         */
        std::shared_ptr<NetworkBufferStats> get_stats() const;

        /**
         * Manually trigger buffer optimization
         */
        std::error_code optimize_buffers();

        /**
         * Check if socket is valid and managed
         */
        bool is_valid() const { return socket_fd_ >= 0 && manager_ != nullptr; }

      private:
        int socket_fd_;
        NetworkBufferManager* manager_;
    };

} // namespace ScratchBird