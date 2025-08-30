#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

// Include configuration types from implemented components
#include "scratchbird/engine/buffer_pool.h"
#include "scratchbird/engine/connection_pool.h"
#include "scratchbird/engine/network_buffer.h"
#include "scratchbird/engine/tcp_optimizer.h"

namespace ScratchBird
{

    /**
     * Performance monitoring metrics collection
     */
    struct PerformanceMetrics {
        // System metrics
        std::atomic<double> cpu_usage_percent{0.0};
        std::atomic<uint64_t> memory_usage_bytes{0};
        std::atomic<uint64_t> memory_available_bytes{0};

        // Network metrics
        std::atomic<uint64_t> network_bytes_sent{0};
        std::atomic<uint64_t> network_bytes_received{0};
        std::atomic<uint64_t> network_connections_active{0};
        std::atomic<uint64_t> network_connections_total{0};

        // I/O metrics
        std::atomic<uint64_t> disk_reads_total{0};
        std::atomic<uint64_t> disk_writes_total{0};
        std::atomic<uint64_t> disk_bytes_read{0};
        std::atomic<uint64_t> disk_bytes_written{0};

        // Query performance
        std::atomic<uint64_t> queries_executed{0};
        std::atomic<uint64_t> queries_per_second{0};
        std::atomic<double> avg_query_time_ms{0.0};
        std::atomic<uint64_t> slow_queries_count{0};

        // Buffer pool metrics (from BufferPool)
        std::atomic<double> buffer_hit_ratio{0.0};
        std::atomic<uint64_t> buffer_evictions{0};

        std::chrono::steady_clock::time_point last_update_time;

        void reset()
        {
            cpu_usage_percent.store(0.0);
            memory_usage_bytes.store(0);
            memory_available_bytes.store(0);
            network_bytes_sent.store(0);
            network_bytes_received.store(0);
            network_connections_active.store(0);
            network_connections_total.store(0);
            disk_reads_total.store(0);
            disk_writes_total.store(0);
            disk_bytes_read.store(0);
            disk_bytes_written.store(0);
            queries_executed.store(0);
            queries_per_second.store(0);
            avg_query_time_ms.store(0.0);
            slow_queries_count.store(0);
            buffer_hit_ratio.store(0.0);
            buffer_evictions.store(0);
            last_update_time = std::chrono::steady_clock::now();
        }

        // Copy constructor and assignment operators
        PerformanceMetrics(const PerformanceMetrics& other)
        {
            copy_from(other);
        }

        PerformanceMetrics& operator=(const PerformanceMetrics& other)
        {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }

        // Default constructor
        PerformanceMetrics() = default;

      private:
        void copy_from(const PerformanceMetrics& other)
        {
            cpu_usage_percent.store(other.cpu_usage_percent.load());
            memory_usage_bytes.store(other.memory_usage_bytes.load());
            memory_available_bytes.store(other.memory_available_bytes.load());
            network_bytes_sent.store(other.network_bytes_sent.load());
            network_bytes_received.store(other.network_bytes_received.load());
            network_connections_active.store(other.network_connections_active.load());
            network_connections_total.store(other.network_connections_total.load());
            disk_reads_total.store(other.disk_reads_total.load());
            disk_writes_total.store(other.disk_writes_total.load());
            disk_bytes_read.store(other.disk_bytes_read.load());
            disk_bytes_written.store(other.disk_bytes_written.load());
            queries_executed.store(other.queries_executed.load());
            queries_per_second.store(other.queries_per_second.load());
            avg_query_time_ms.store(other.avg_query_time_ms.load());
            slow_queries_count.store(other.slow_queries_count.load());
            buffer_hit_ratio.store(other.buffer_hit_ratio.load());
            buffer_evictions.store(other.buffer_evictions.load());
            last_update_time = other.last_update_time;
        }
    };

    /**
     * Performance alert types and thresholds
     */
    enum class PerformanceAlertType {
        HIGH_CPU_USAGE,        // CPU usage above threshold
        HIGH_MEMORY_USAGE,     // Memory usage above threshold
        LOW_BUFFER_HIT_RATIO,  // Buffer hit ratio below threshold
        HIGH_QUERY_LATENCY,    // Average query time above threshold
        HIGH_CONNECTION_COUNT, // Active connections above threshold
        LOW_DISK_SPACE,        // Available disk space below threshold
        HIGH_ERROR_RATE,       // Error rate above threshold
        SLOW_QUERY_DETECTED    // Individual query exceeded threshold
    };

    struct PerformanceAlert {
        PerformanceAlertType type;
        std::string message;
        double current_value;
        double threshold_value;
        std::chrono::steady_clock::time_point timestamp;
        std::string component; // Which component generated the alert

        // Default constructor
        PerformanceAlert() = default;

        PerformanceAlert(PerformanceAlertType t, const std::string& msg, double current,
                         double threshold, const std::string& comp)
            : type(t), message(msg), current_value(current), threshold_value(threshold),
              timestamp(std::chrono::steady_clock::now()), component(comp)
        {
        }
    };

    /**
     * Performance thresholds configuration
     */
    struct PerformanceThresholds {
        // CPU and memory thresholds
        double cpu_usage_warning_percent = 80.0;
        double cpu_usage_critical_percent = 95.0;
        double memory_usage_warning_percent = 80.0;
        double memory_usage_critical_percent = 90.0;

        // Buffer pool thresholds
        double buffer_hit_ratio_warning = 70.0;
        double buffer_hit_ratio_critical = 50.0;

        // Query performance thresholds
        double avg_query_time_warning_ms = 1000.0;  // 1 second
        double avg_query_time_critical_ms = 5000.0; // 5 seconds
        double slow_query_threshold_ms = 10000.0;   // 10 seconds

        // Connection thresholds
        uint64_t connection_count_warning = 80;  // 80% of max
        uint64_t connection_count_critical = 95; // 95% of max

        // Disk space thresholds (percentage)
        double disk_space_warning_percent = 80.0;
        double disk_space_critical_percent = 90.0;

        // Error rate thresholds (per minute)
        double error_rate_warning = 10.0;
        double error_rate_critical = 50.0;
    };

    /**
     * Comprehensive performance configuration
     * Combines all component configurations for unified management
     */
    struct PerformanceConfiguration {
        // Component configurations
        // Note: Using generic structs since actual config types may vary
        struct {
            uint32_t socket_receive_buffer_size = 65536;
            uint32_t socket_send_buffer_size = 65536;
        } tcp_config;

        struct {
            uint32_t max_connections = 100;
            uint32_t initial_pool_size = 10;
        } connection_pool_config;

        struct {
            uint32_t buffer_size = 65536;
            bool auto_tuning_enabled = true;
        } network_buffer_config;

        struct {
            uint32_t buffer_count = 1024;
            uint32_t buffer_size = 4096;
        } buffer_pool_config;

        // Global performance settings
        bool enable_performance_monitoring = true;
        std::chrono::seconds metrics_collection_interval{10};
        std::chrono::seconds metrics_retention_period{3600}; // 1 hour

        // Alerting configuration
        PerformanceThresholds alert_thresholds;
        bool enable_performance_alerts = true;
        std::chrono::seconds alert_check_interval{30};
        size_t max_alerts_per_type = 10;

        // Auto-tuning configuration
        bool enable_auto_tuning = true;
        std::chrono::minutes auto_tuning_interval{15};
        double auto_tuning_aggressiveness = 0.5; // 0.0 (conservative) to 1.0 (aggressive)

        // Logging and diagnostics
        bool enable_performance_logging = true;
        std::string performance_log_level = "INFO"; // DEBUG, INFO, WARN, ERROR
        size_t max_log_entries = 10000;

        // Hot-reload configuration
        bool allow_runtime_config_changes = true;
        std::vector<std::string> protected_settings; // Settings that cannot be changed at runtime
    };

    /**
     * Configuration validation result
     */
    struct ConfigurationValidationResult {
        bool is_valid = true;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;

        void add_error(const std::string& error)
        {
            is_valid = false;
            errors.push_back(error);
        }

        void add_warning(const std::string& warning)
        {
            warnings.push_back(warning);
        }

        bool has_errors() const
        {
            return !errors.empty();
        }
        bool has_warnings() const
        {
            return !warnings.empty();
        }
    };

    /**
     * Performance configuration change callback
     */
    using ConfigurationChangeCallback =
        std::function<std::error_code(const PerformanceConfiguration&)>;

    /**
     * Performance alert callback
     */
    using PerformanceAlertCallback = std::function<void(const PerformanceAlert&)>;

    /**
     * Auto-tuning recommendation
     */
    struct AutoTuningRecommendation {
        std::string component;
        std::string parameter;
        std::string current_value;
        std::string recommended_value;
        std::string reasoning;
        double confidence_score; // 0.0 to 1.0
        bool is_critical;        // Whether this should be applied immediately
    };

    /**
     * Performance Configuration Manager
     *
     * Central manager for all performance-related configurations across ScratchBird.
     * Provides unified configuration management, hot-reload capabilities, monitoring,
     * alerting, and auto-tuning recommendations.
     *
     * Features:
     * - Unified configuration management for all components
     * - Hot-reload configuration without restart
     * - Real-time performance monitoring and metrics collection
     * - Threshold-based alerting system
     * - Auto-tuning recommendations based on performance data
     * - Configuration validation and constraint checking
     */
    class PerformanceConfigurationManager
    {
      public:
        explicit PerformanceConfigurationManager(const PerformanceConfiguration& config = {});
        ~PerformanceConfigurationManager();

        // Non-copyable, non-moveable (singleton-like manager)
        PerformanceConfigurationManager(const PerformanceConfigurationManager&) = delete;
        PerformanceConfigurationManager& operator=(const PerformanceConfigurationManager&) = delete;
        PerformanceConfigurationManager(PerformanceConfigurationManager&&) = delete;
        PerformanceConfigurationManager& operator=(PerformanceConfigurationManager&&) = delete;

        /**
         * Initialize the performance configuration manager
         * @return Error code, empty on success
         */
        std::error_code initialize();

        /**
         * Shutdown the manager and cleanup resources
         */
        void shutdown();

        // Configuration Management

        /**
         * Get current performance configuration
         * @return Current configuration
         */
        PerformanceConfiguration get_configuration() const;

        /**
         * Update configuration with validation
         * @param new_config New configuration
         * @param force_update Skip runtime change protection
         * @return Error code, empty on success
         */
        std::error_code update_configuration(const PerformanceConfiguration& new_config,
                                             bool force_update = false);

        /**
         * Update a specific component configuration
         * @param component Component name (tcp, pool, buffer, etc.)
         * @param config_json JSON configuration string
         * @return Error code, empty on success
         */
        std::error_code update_component_config(const std::string& component,
                                                const std::string& config_json);

        /**
         * Validate configuration
         * @param config Configuration to validate
         * @return Validation result with errors/warnings
         */
        ConfigurationValidationResult
        validate_configuration(const PerformanceConfiguration& config) const;

        /**
         * Load configuration from file
         * @param file_path Path to configuration file
         * @return Error code, empty on success
         */
        std::error_code load_configuration_from_file(const std::string& file_path);

        /**
         * Save configuration to file
         * @param file_path Path to save configuration
         * @return Error code, empty on success
         */
        std::error_code save_configuration_to_file(const std::string& file_path) const;

        // Performance Monitoring

        /**
         * Get current performance metrics
         * @return Current metrics snapshot
         */
        PerformanceMetrics get_performance_metrics() const;

        /**
         * Update performance metrics
         * @param metrics New metrics values
         */
        void update_performance_metrics(const PerformanceMetrics& metrics);

        /**
         * Get performance metrics history
         * @param duration How far back to retrieve metrics
         * @return Vector of historical metrics
         */
        std::vector<PerformanceMetrics> get_metrics_history(std::chrono::seconds duration) const;

        /**
         * Reset performance metrics
         */
        void reset_performance_metrics();

        // Alerting System

        /**
         * Register performance alert callback
         * @param callback Function to call when alert is triggered
         */
        void register_alert_callback(PerformanceAlertCallback callback);

        /**
         * Get pending performance alerts
         * @param clear_alerts Whether to clear alerts after retrieval
         * @return Vector of pending alerts
         */
        std::vector<PerformanceAlert> get_pending_alerts(bool clear_alerts = true);

        /**
         * Update alert thresholds
         * @param thresholds New threshold values
         * @return Error code, empty on success
         */
        std::error_code update_alert_thresholds(const PerformanceThresholds& thresholds);

        // Auto-Tuning

        /**
         * Get auto-tuning recommendations
         * @return Vector of tuning recommendations
         */
        std::vector<AutoTuningRecommendation> get_auto_tuning_recommendations() const;

        /**
         * Apply auto-tuning recommendation
         * @param recommendation Recommendation to apply
         * @return Error code, empty on success
         */
        std::error_code
        apply_auto_tuning_recommendation(const AutoTuningRecommendation& recommendation);

        /**
         * Enable/disable auto-tuning
         * @param enabled Whether to enable auto-tuning
         */
        void set_auto_tuning_enabled(bool enabled);

        // Component Integration

        /**
         * Register configuration change callback for a component
         * @param component Component name
         * @param callback Function to call when configuration changes
         */
        void register_config_change_callback(const std::string& component,
                                             ConfigurationChangeCallback callback);

        /**
         * Get configuration for a specific component
         * @param component Component name
         * @return Component-specific configuration in JSON format
         */
        std::string get_component_configuration(const std::string& component) const;

        // Diagnostics and Utilities

        /**
         * Generate performance report
         * @param include_history Whether to include historical data
         * @return Performance report in JSON format
         */
        std::string generate_performance_report(bool include_history = false) const;

        /**
         * Run performance diagnostics
         * @return Vector of diagnostic messages
         */
        std::vector<std::string> run_performance_diagnostics() const;

        /**
         * Export configuration and metrics for external analysis
         * @param file_path Path to export file
         * @return Error code, empty on success
         */
        std::error_code export_performance_data(const std::string& file_path) const;

        /**
         * Get supported configuration parameters
         * @return Map of parameter names to descriptions
         */
        std::unordered_map<std::string, std::string> get_supported_parameters() const;

      private:
        PerformanceConfiguration config_;
        mutable std::shared_mutex config_mutex_;

        // Performance monitoring
        PerformanceMetrics current_metrics_;
        std::vector<PerformanceMetrics> metrics_history_;
        mutable std::mutex metrics_mutex_;

        // Alerting system
        std::vector<PerformanceAlert> pending_alerts_;
        std::vector<PerformanceAlertCallback> alert_callbacks_;
        mutable std::mutex alerts_mutex_;

        // Component callbacks
        std::unordered_map<std::string, ConfigurationChangeCallback> component_callbacks_;
        mutable std::mutex callbacks_mutex_;

        // Background monitoring
        std::thread monitoring_thread_;
        std::thread alerting_thread_;
        std::thread auto_tuning_thread_;
        std::atomic<bool> shutdown_requested_{false};

        /**
         * Background monitoring loop
         */
        void monitoring_loop();

        /**
         * Background alerting loop
         */
        void alerting_loop();

        /**
         * Background auto-tuning loop
         */
        void auto_tuning_loop();

        /**
         * Check performance thresholds and generate alerts
         */
        void check_performance_thresholds();

        /**
         * Generate auto-tuning recommendations based on current metrics
         */
        std::vector<AutoTuningRecommendation> generate_tuning_recommendations() const;

        /**
         * Collect system performance metrics
         */
        void collect_system_metrics();

        /**
         * Validate individual configuration component
         */
        void validate_component_config(const std::string& component,
                                       ConfigurationValidationResult& result) const;

        /**
         * Apply configuration changes to registered components
         */
        std::error_code notify_component_config_changes(const PerformanceConfiguration& old_config,
                                                        const PerformanceConfiguration& new_config);

        /**
         * Load default configuration values
         */
        void load_default_configuration();

        /**
         * Cleanup old metrics history
         */
        void cleanup_metrics_history();
    };

} // namespace ScratchBird
