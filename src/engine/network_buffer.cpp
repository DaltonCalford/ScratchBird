#include "scratchbird/engine/network_buffer.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#else
#include <errno.h>
#include <unistd.h>
#endif

namespace ScratchBird
{

    // NetworkBufferManager implementation
    NetworkBufferManager::NetworkBufferManager(const NetworkBufferConfig& config)
        : config_(config)
    {
        aggregated_stats_.last_update_time = std::chrono::steady_clock::now();
    }

    NetworkBufferManager::~NetworkBufferManager()
    {
        shutdown();
    }

    std::error_code NetworkBufferManager::initialize()
    {
        // Validate configuration
        if (auto validation_error = validate_config(config_); !validation_error.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        std::lock_guard<std::mutex> lock(manager_mutex_);

        // Start background threads
        shutdown_requested_ = false;
        
        if (config_.enable_monitoring) {
            monitoring_thread_ = std::thread(&NetworkBufferManager::monitoring_loop, this);
        }
        
        if (config_.enable_auto_tuning) {
            tuning_thread_ = std::thread(&NetworkBufferManager::tuning_loop, this);
        }

        return {};
    }

    void NetworkBufferManager::shutdown()
    {
        // Signal shutdown to background threads
        shutdown_requested_ = true;

        // Wait for background threads to finish
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
        
        if (tuning_thread_.joinable()) {
            tuning_thread_.join();
        }

        // Clear all tracked sockets
        {
            std::lock_guard<std::mutex> lock(socket_stats_mutex_);
            socket_stats_.clear();
        }

        // Clear pending alerts
        {
            std::lock_guard<std::mutex> lock(alerts_mutex_);
            pending_alerts_.clear();
        }
    }

    std::error_code NetworkBufferManager::configure_socket_buffers(int socket_fd, 
                                                                   size_t recv_size, 
                                                                   size_t send_size)
    {
        if (socket_fd < 0) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        // Use default sizes if not specified
        if (recv_size == 0) recv_size = config_.default_recv_buffer_size;
        if (send_size == 0) send_size = config_.default_send_buffer_size;

        // Clamp to configured limits
        recv_size = std::clamp(recv_size, config_.min_buffer_size, config_.max_buffer_size);
        send_size = std::clamp(send_size, config_.min_buffer_size, config_.max_buffer_size);

        return apply_socket_buffer_config(socket_fd, recv_size, send_size);
    }

    std::error_code NetworkBufferManager::register_socket(int socket_fd)
    {
        if (socket_fd < 0) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        auto stats = std::make_shared<NetworkBufferStats>();
        stats->created_time = std::chrono::steady_clock::now();
        stats->last_tuning_time = stats->created_time;

        // Get current buffer sizes
        auto [recv_size, send_size] = get_current_socket_buffer_sizes(socket_fd);
        stats->recv_buffer_size = recv_size;
        stats->send_buffer_size = send_size;

        {
            std::lock_guard<std::mutex> lock(socket_stats_mutex_);
            socket_stats_[socket_fd] = stats;
        }

        // Configure with default buffer sizes if not already configured
        if (recv_size == 0 || send_size == 0) {
            configure_socket_buffers(socket_fd, 
                                   config_.default_recv_buffer_size,
                                   config_.default_send_buffer_size);
        }

        return {};
    }

    void NetworkBufferManager::unregister_socket(int socket_fd)
    {
        std::lock_guard<std::mutex> lock(socket_stats_mutex_);
        socket_stats_.erase(socket_fd);
    }

    void NetworkBufferManager::record_io_operation(int socket_fd, 
                                                   size_t bytes_transferred, 
                                                   bool is_send, 
                                                   std::chrono::nanoseconds operation_latency)
    {
        std::lock_guard<std::mutex> lock(socket_stats_mutex_);
        
        auto it = socket_stats_.find(socket_fd);
        if (it == socket_stats_.end()) {
            return; // Socket not registered
        }

        auto& stats = *it->second;
        
        if (is_send) {
            stats.bytes_sent += bytes_transferred;
            stats.send_operations++;
            
            // Update average send latency using exponential moving average
            if (stats.send_operations == 1) {
                stats.avg_send_latency = operation_latency;
            } else {
                stats.avg_send_latency = std::chrono::nanoseconds(
                    (stats.avg_send_latency.count() * 7 + operation_latency.count()) / 8);
            }
        } else {
            stats.bytes_received += bytes_transferred;
            stats.recv_operations++;
            
            // Update average receive latency using exponential moving average
            if (stats.recv_operations == 1) {
                stats.avg_recv_latency = operation_latency;
            } else {
                stats.avg_recv_latency = std::chrono::nanoseconds(
                    (stats.avg_recv_latency.count() * 7 + operation_latency.count()) / 8);
            }
        }
    }

    void NetworkBufferManager::record_buffer_overflow(int socket_fd, bool is_send_buffer)
    {
        std::lock_guard<std::mutex> lock(socket_stats_mutex_);
        
        auto it = socket_stats_.find(socket_fd);
        if (it == socket_stats_.end()) {
            return; // Socket not registered
        }

        auto& stats = *it->second;
        
        if (is_send_buffer) {
            stats.send_buffer_full_events++;
        } else {
            stats.recv_buffer_full_events++;
        }

        // Check if we need to generate an alert
        uint64_t total_overflows = stats.send_buffer_full_events.load() + 
                                  stats.recv_buffer_full_events.load();
        
        if (total_overflows >= config_.overflow_alert_threshold) {
            generate_alert(BufferAlertType::OVERFLOW_DETECTED, socket_fd,
                         "Buffer overflow events detected (count: " + std::to_string(total_overflows) + ")",
                         stats);
        }
    }

    std::error_code NetworkBufferManager::tune_socket_buffers(int socket_fd)
    {
        std::lock_guard<std::mutex> lock(socket_stats_mutex_);
        
        auto it = socket_stats_.find(socket_fd);
        if (it == socket_stats_.end()) {
            return std::make_error_code(std::errc::no_such_file_or_directory);
        }

        return perform_buffer_tuning(socket_fd, *it->second);
    }

    std::shared_ptr<NetworkBufferStats> NetworkBufferManager::get_socket_stats(int socket_fd) const
    {
        std::lock_guard<std::mutex> lock(socket_stats_mutex_);
        
        auto it = socket_stats_.find(socket_fd);
        if (it == socket_stats_.end()) {
            return nullptr;
        }
        
        return it->second;
    }

    AggregatedBufferStats NetworkBufferManager::get_aggregated_stats() const
    {
        std::lock_guard<std::mutex> lock(aggregated_stats_mutex_);
        return aggregated_stats_;
    }

    std::vector<BufferAlert> NetworkBufferManager::get_pending_alerts(bool clear_alerts)
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        
        std::vector<BufferAlert> alerts = pending_alerts_;
        
        if (clear_alerts) {
            pending_alerts_.clear();
        }
        
        return alerts;
    }

    std::error_code NetworkBufferManager::update_config(const NetworkBufferConfig& new_config)
    {
        if (auto validation_error = validate_config(new_config); !validation_error.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        std::lock_guard<std::mutex> lock(manager_mutex_);
        config_ = new_config;
        
        return {};
    }

    std::string NetworkBufferManager::validate_config(const NetworkBufferConfig& config)
    {
        std::ostringstream errors;

        if (config.min_buffer_size == 0) {
            errors << "min_buffer_size must be greater than 0; ";
        }

        if (config.max_buffer_size <= config.min_buffer_size) {
            errors << "max_buffer_size must be greater than min_buffer_size; ";
        }

        if (config.default_recv_buffer_size < config.min_buffer_size ||
            config.default_recv_buffer_size > config.max_buffer_size) {
            errors << "default_recv_buffer_size must be within min/max range; ";
        }

        if (config.default_send_buffer_size < config.min_buffer_size ||
            config.default_send_buffer_size > config.max_buffer_size) {
            errors << "default_send_buffer_size must be within min/max range; ";
        }

        if (config.growth_factor <= 1.0) {
            errors << "growth_factor must be greater than 1.0; ";
        }

        if (config.shrink_factor >= 1.0 || config.shrink_factor <= 0.0) {
            errors << "shrink_factor must be between 0.0 and 1.0; ";
        }

        if (config.utilization_threshold <= 0.0 || config.utilization_threshold > 1.0) {
            errors << "utilization_threshold must be between 0.0 and 1.0; ";
        }

        if (config.underutilization_threshold <= 0.0 || 
            config.underutilization_threshold >= config.utilization_threshold) {
            errors << "underutilization_threshold must be positive and less than utilization_threshold; ";
        }

        return errors.str();
    }

    size_t NetworkBufferManager::get_recommended_buffer_size(int socket_fd, bool is_send_buffer) const
    {
        auto stats = get_socket_stats(socket_fd);
        if (!stats) {
            return is_send_buffer ? config_.default_send_buffer_size : config_.default_recv_buffer_size;
        }

        return calculate_optimal_buffer_size(*stats, is_send_buffer);
    }

    void NetworkBufferManager::monitoring_loop()
    {
        while (!shutdown_requested_) {
            std::this_thread::sleep_for(config_.stats_collection_interval);
            
            if (shutdown_requested_) break;

            // Update aggregated statistics
            update_aggregated_stats();
            
            // Check for underutilization alerts
            std::lock_guard<std::mutex> lock(socket_stats_mutex_);
            for (const auto& [socket_fd, stats] : socket_stats_) {
                double recv_util = stats->get_recv_utilization();
                double send_util = stats->get_send_utilization();
                
                if (recv_util < config_.underutilization_threshold ||
                    send_util < config_.underutilization_threshold) {
                    generate_alert(BufferAlertType::UNDERUTILIZATION, socket_fd,
                                 "Buffer underutilization detected (recv: " + 
                                 std::to_string(recv_util * 100) + "%, send: " +
                                 std::to_string(send_util * 100) + "%)", *stats);
                }

                // Check for high latency
                if (stats->avg_send_latency > std::chrono::milliseconds(100) ||
                    stats->avg_recv_latency > std::chrono::milliseconds(100)) {
                    generate_alert(BufferAlertType::HIGH_LATENCY_DETECTED, socket_fd,
                                 "High I/O latency detected", *stats);
                }
            }
        }
    }

    void NetworkBufferManager::tuning_loop()
    {
        while (!shutdown_requested_) {
            std::this_thread::sleep_for(config_.tuning_interval);
            
            if (shutdown_requested_) break;

            std::vector<int> sockets_to_tune;
            
            // Find sockets that need tuning
            {
                std::lock_guard<std::mutex> lock(socket_stats_mutex_);
                for (const auto& [socket_fd, stats] : socket_stats_) {
                    if (needs_buffer_tuning(*stats)) {
                        sockets_to_tune.push_back(socket_fd);
                    }
                }
            }

            // Perform tuning outside of lock to avoid deadlock
            for (int socket_fd : sockets_to_tune) {
                if (auto ec = tune_socket_buffers(socket_fd); ec) {
                    auto stats = get_socket_stats(socket_fd);
                    if (stats) {
                        generate_alert(BufferAlertType::TUNING_FAILED, socket_fd,
                                     "Auto-tuning failed: " + ec.message(), *stats);
                    }
                } else {
                    // Update aggregated stats for successful tuning
                    std::lock_guard<std::mutex> agg_lock(aggregated_stats_mutex_);
                    aggregated_stats_.tuning_operations_performed++;
                }
            }
        }
    }

    std::error_code NetworkBufferManager::apply_socket_buffer_config(int socket_fd, 
                                                                    size_t recv_size, 
                                                                    size_t send_size)
    {
        // Set receive buffer size
        int recv_buf = static_cast<int>(recv_size);
        if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, 
                       reinterpret_cast<const char*>(&recv_buf), sizeof(recv_buf)) < 0) {
#ifdef _WIN32
            return std::make_error_code(std::errc::io_error);
#else
            return std::make_error_code(static_cast<std::errc>(errno));
#endif
        }

        // Set send buffer size
        int send_buf = static_cast<int>(send_size);
        if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, 
                       reinterpret_cast<const char*>(&send_buf), sizeof(send_buf)) < 0) {
#ifdef _WIN32
            return std::make_error_code(std::errc::io_error);
#else
            return std::make_error_code(static_cast<std::errc>(errno));
#endif
        }

        // Update statistics if socket is registered
        {
            std::lock_guard<std::mutex> lock(socket_stats_mutex_);
            auto it = socket_stats_.find(socket_fd);
            if (it != socket_stats_.end()) {
                it->second->recv_buffer_size = recv_size;
                it->second->send_buffer_size = send_size;
            }
        }

        return {};
    }

    std::pair<size_t, size_t> NetworkBufferManager::get_current_socket_buffer_sizes(int socket_fd) const
    {
        if (socket_fd < 0) {
            return {0, 0};
        }

        // Get receive buffer size
        int recv_buf = 0;
        socklen_t recv_len = sizeof(recv_buf);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, 
                       reinterpret_cast<char*>(&recv_buf), &recv_len) != 0) {
            recv_buf = 0;
        }

        // Get send buffer size
        int send_buf = 0;
        socklen_t send_len = sizeof(send_buf);
        if (getsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, 
                       reinterpret_cast<char*>(&send_buf), &send_len) != 0) {
            send_buf = 0;
        }

        return {static_cast<size_t>(std::max(0, recv_buf)), 
                static_cast<size_t>(std::max(0, send_buf))};
    }

    size_t NetworkBufferManager::calculate_optimal_buffer_size(const NetworkBufferStats& stats, 
                                                              bool is_send_buffer) const
    {
        size_t current_size = is_send_buffer ? stats.send_buffer_size : stats.recv_buffer_size;
        double utilization = is_send_buffer ? stats.get_send_utilization() : stats.get_recv_utilization();
        uint64_t overflow_events = is_send_buffer ? stats.send_buffer_full_events.load() : 
                                                   stats.recv_buffer_full_events.load();

        // If we have overflow events, grow the buffer
        if (overflow_events > 0) {
            size_t new_size = static_cast<size_t>(current_size * config_.growth_factor);
            return std::clamp(new_size, config_.min_buffer_size, config_.max_buffer_size);
        }

        // If utilization is high, consider growing
        if (utilization > config_.utilization_threshold) {
            size_t new_size = static_cast<size_t>(current_size * config_.growth_factor);
            return std::clamp(new_size, config_.min_buffer_size, config_.max_buffer_size);
        }

        // If utilization is low, consider shrinking
        if (utilization < config_.underutilization_threshold) {
            size_t new_size = static_cast<size_t>(current_size * config_.shrink_factor);
            return std::clamp(new_size, config_.min_buffer_size, config_.max_buffer_size);
        }

        // No change needed
        return current_size;
    }

    void NetworkBufferManager::generate_alert(BufferAlertType type, 
                                             int socket_fd, 
                                             const std::string& message,
                                             const NetworkBufferStats& stats)
    {
        BufferAlert alert;
        alert.type = type;
        alert.socket_fd = socket_fd;
        alert.message = message;
        alert.timestamp = std::chrono::steady_clock::now();
        alert.buffer_size_before = stats.recv_buffer_size; // Use recv as representative
        alert.utilization_rate = stats.get_recv_utilization();
        
        uint64_t total_overflows = stats.recv_buffer_full_events.load() + 
                                  stats.send_buffer_full_events.load();
        alert.event_count = total_overflows;

        std::lock_guard<std::mutex> lock(alerts_mutex_);
        pending_alerts_.push_back(alert);
        
        // Limit alert queue size to prevent memory bloat
        if (pending_alerts_.size() > 1000) {
            pending_alerts_.erase(pending_alerts_.begin(), pending_alerts_.begin() + 100);
        }
    }

    void NetworkBufferManager::update_aggregated_stats()
    {
        std::lock_guard<std::mutex> agg_lock(aggregated_stats_mutex_);
        std::lock_guard<std::mutex> socket_lock(socket_stats_mutex_);

        // Reset aggregated stats
        aggregated_stats_ = AggregatedBufferStats{};
        aggregated_stats_.last_update_time = std::chrono::steady_clock::now();

        if (socket_stats_.empty()) {
            return;
        }

        uint64_t total_recv_buffer_size = 0;
        uint64_t total_send_buffer_size = 0;
        double total_recv_util = 0.0;
        double total_send_util = 0.0;
        uint64_t total_operations = 0;

        for (const auto& [socket_fd, stats] : socket_stats_) {
            aggregated_stats_.total_connections++;
            
            if (stats->recv_buffer_size > 0) {
                aggregated_stats_.total_recv_buffers_allocated++;
                total_recv_buffer_size += stats->recv_buffer_size;
            }
            
            if (stats->send_buffer_size > 0) {
                aggregated_stats_.total_send_buffers_allocated++;
                total_send_buffer_size += stats->send_buffer_size;
            }

            aggregated_stats_.total_bytes_received += stats->bytes_received.load();
            aggregated_stats_.total_bytes_sent += stats->bytes_sent.load();
            
            uint64_t socket_overflows = stats->recv_buffer_full_events.load() + 
                                       stats->send_buffer_full_events.load();
            aggregated_stats_.total_overflow_events += socket_overflows;

            total_recv_util += stats->get_recv_utilization();
            total_send_util += stats->get_send_utilization();
            
            uint64_t socket_ops = stats->recv_operations.load() + stats->send_operations.load();
            total_operations += socket_ops;
            
            if (config_.enable_auto_tuning) {
                aggregated_stats_.connections_with_auto_tuning++;
            }
        }

        // Calculate averages
        if (aggregated_stats_.total_recv_buffers_allocated > 0) {
            aggregated_stats_.avg_recv_buffer_size = 
                static_cast<double>(total_recv_buffer_size) / aggregated_stats_.total_recv_buffers_allocated;
        }

        if (aggregated_stats_.total_send_buffers_allocated > 0) {
            aggregated_stats_.avg_send_buffer_size = 
                static_cast<double>(total_send_buffer_size) / aggregated_stats_.total_send_buffers_allocated;
        }

        if (aggregated_stats_.total_connections > 0) {
            aggregated_stats_.avg_recv_utilization = total_recv_util / aggregated_stats_.total_connections;
            aggregated_stats_.avg_send_utilization = total_send_util / aggregated_stats_.total_connections;
        }

        // Calculate overall efficiency (bytes per operation)
        if (total_operations > 0) {
            uint64_t total_bytes = aggregated_stats_.total_bytes_received + aggregated_stats_.total_bytes_sent;
            aggregated_stats_.overall_efficiency = static_cast<double>(total_bytes) / total_operations;
        }
    }

    bool NetworkBufferManager::needs_buffer_tuning(const NetworkBufferStats& stats) const
    {
        auto now = std::chrono::steady_clock::now();
        auto time_since_last_tuning = now - stats.last_tuning_time;
        
        // Don't tune too frequently
        if (time_since_last_tuning < config_.tuning_interval) {
            return false;
        }

        // Check if we have overflow events
        if (stats.recv_buffer_full_events.load() > 0 || stats.send_buffer_full_events.load() > 0) {
            return true;
        }

        // Check utilization thresholds
        double recv_util = stats.get_recv_utilization();
        double send_util = stats.get_send_utilization();

        if (recv_util > config_.utilization_threshold || send_util > config_.utilization_threshold) {
            return true; // High utilization, consider growing
        }

        if (recv_util < config_.underutilization_threshold || 
            send_util < config_.underutilization_threshold) {
            return true; // Low utilization, consider shrinking
        }

        return false;
    }

    std::error_code NetworkBufferManager::perform_buffer_tuning(int socket_fd, NetworkBufferStats& stats)
    {
        size_t new_recv_size = calculate_optimal_buffer_size(stats, false);
        size_t new_send_size = calculate_optimal_buffer_size(stats, true);

        // Check if buffer size would hit limits
        if ((new_recv_size == config_.max_buffer_size && stats.recv_buffer_size == config_.max_buffer_size) ||
            (new_send_size == config_.max_buffer_size && stats.send_buffer_size == config_.max_buffer_size)) {
            generate_alert(BufferAlertType::BUFFER_SIZE_LIMIT_REACHED, socket_fd,
                         "Buffer size limit reached during tuning", stats);
        }

        // Apply new buffer configuration
        auto ec = apply_socket_buffer_config(socket_fd, new_recv_size, new_send_size);
        
        if (!ec) {
            // Update tuning timestamp
            stats.last_tuning_time = std::chrono::steady_clock::now();
            
            // Reset overflow counters after successful tuning
            stats.recv_buffer_full_events = 0;
            stats.send_buffer_full_events = 0;
        }

        return ec;
    }

    // ManagedNetworkSocket implementation
    ManagedNetworkSocket::ManagedNetworkSocket(int socket_fd, NetworkBufferManager& manager)
        : socket_fd_(socket_fd), manager_(&manager)
    {
        if (socket_fd_ >= 0) {
            manager_->register_socket(socket_fd_);
        }
    }

    ManagedNetworkSocket::~ManagedNetworkSocket()
    {
        if (socket_fd_ >= 0 && manager_) {
            manager_->unregister_socket(socket_fd_);
        }
    }

    ManagedNetworkSocket::ManagedNetworkSocket(ManagedNetworkSocket&& other) noexcept
        : socket_fd_(other.socket_fd_), manager_(other.manager_)
    {
        other.socket_fd_ = -1;
        other.manager_ = nullptr;
    }

    ManagedNetworkSocket& ManagedNetworkSocket::operator=(ManagedNetworkSocket&& other) noexcept
    {
        if (this != &other) {
            if (socket_fd_ >= 0 && manager_) {
                manager_->unregister_socket(socket_fd_);
            }

            socket_fd_ = other.socket_fd_;
            manager_ = other.manager_;
            
            other.socket_fd_ = -1;
            other.manager_ = nullptr;
        }
        return *this;
    }

    void ManagedNetworkSocket::record_send(size_t bytes_sent, std::chrono::nanoseconds latency)
    {
        if (manager_ && socket_fd_ >= 0) {
            manager_->record_io_operation(socket_fd_, bytes_sent, true, latency);
        }
    }

    void ManagedNetworkSocket::record_receive(size_t bytes_received, std::chrono::nanoseconds latency)
    {
        if (manager_ && socket_fd_ >= 0) {
            manager_->record_io_operation(socket_fd_, bytes_received, false, latency);
        }
    }

    void ManagedNetworkSocket::record_overflow(bool is_send_buffer)
    {
        if (manager_ && socket_fd_ >= 0) {
            manager_->record_buffer_overflow(socket_fd_, is_send_buffer);
        }
    }

    std::shared_ptr<NetworkBufferStats> ManagedNetworkSocket::get_stats() const
    {
        if (manager_ && socket_fd_ >= 0) {
            return manager_->get_socket_stats(socket_fd_);
        }
        return nullptr;
    }

    std::error_code ManagedNetworkSocket::optimize_buffers()
    {
        if (manager_ && socket_fd_ >= 0) {
            return manager_->tune_socket_buffers(socket_fd_);
        }
        return std::make_error_code(std::errc::invalid_argument);
    }

} // namespace ScratchBird