// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/performance_config.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace ScratchBird
{

    PerformanceConfigurationManager::PerformanceConfigurationManager(
        const PerformanceConfiguration& config)
        : config_(config), shutdown_requested_(false)
    {
        load_default_configuration();
    }

    PerformanceConfigurationManager::~PerformanceConfigurationManager()
    {
        shutdown();
    }

    std::error_code PerformanceConfigurationManager::initialize()
    {
        // Start background monitoring threads
        if (config_.enable_performance_monitoring) {
            monitoring_thread_ =
                std::thread(&PerformanceConfigurationManager::monitoring_loop, this);
        }

        if (config_.enable_performance_alerts) {
            alerting_thread_ = std::thread(&PerformanceConfigurationManager::alerting_loop, this);
        }

        if (config_.enable_auto_tuning) {
            auto_tuning_thread_ =
                std::thread(&PerformanceConfigurationManager::auto_tuning_loop, this);
        }

        return {};
    }

    void PerformanceConfigurationManager::shutdown()
    {
        shutdown_requested_ = true;

        // Join background threads
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
        if (alerting_thread_.joinable()) {
            alerting_thread_.join();
        }
        if (auto_tuning_thread_.joinable()) {
            auto_tuning_thread_.join();
        }
    }

    PerformanceConfiguration PerformanceConfigurationManager::get_configuration() const
    {
        std::shared_lock<std::shared_mutex> lock(config_mutex_);
        return config_;
    }

    std::error_code PerformanceConfigurationManager::update_configuration(
        const PerformanceConfiguration& new_config, bool force_update)
    {
        // Validate the new configuration
        auto validation_result = validate_configuration(new_config);
        if (!validation_result.is_valid) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        std::unique_lock<std::shared_mutex> lock(config_mutex_);
        PerformanceConfiguration old_config = config_;
        config_ = new_config;
        lock.unlock();

        // Notify components of configuration changes
        auto error = notify_component_config_changes(old_config, new_config);
        if (error) {
            // Rollback on error
            std::unique_lock<std::shared_mutex> rollback_lock(config_mutex_);
            config_ = old_config;
            return error;
        }

        return {};
    }

    ConfigurationValidationResult PerformanceConfigurationManager::validate_configuration(
        const PerformanceConfiguration& config) const
    {
        ConfigurationValidationResult result;

        // Validate TCP optimizer configuration
        validate_component_config("tcp", result);

        // Validate connection pool configuration
        validate_component_config("connection_pool", result);

        // Validate network buffer configuration
        validate_component_config("network_buffer", result);

        // Validate buffer pool configuration
        validate_component_config("buffer_pool", result);

        // Validate performance monitoring settings
        if (config.metrics_collection_interval.count() < 1) {
            result.add_error("Metrics collection interval must be at least 1 second");
        }

        if (config.alert_check_interval.count() < 1) {
            result.add_error("Alert check interval must be at least 1 second");
        }

        if (config.auto_tuning_aggressiveness < 0.0 || config.auto_tuning_aggressiveness > 1.0) {
            result.add_error("Auto-tuning aggressiveness must be between 0.0 and 1.0");
        }

        return result;
    }

    PerformanceMetrics PerformanceConfigurationManager::get_performance_metrics() const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        return current_metrics_;
    }

    void
    PerformanceConfigurationManager::update_performance_metrics(const PerformanceMetrics& metrics)
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_metrics_ = metrics;
        current_metrics_.last_update_time = std::chrono::steady_clock::now();

        // Add to history
        metrics_history_.push_back(current_metrics_);

        // Clean up old history if needed
        cleanup_metrics_history();
    }

    std::vector<PerformanceMetrics>
    PerformanceConfigurationManager::get_metrics_history(std::chrono::seconds duration) const
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        std::vector<PerformanceMetrics> result;

        auto cutoff_time = std::chrono::steady_clock::now() - duration;

        for (const auto& metrics : metrics_history_) {
            if (metrics.last_update_time >= cutoff_time) {
                result.push_back(metrics);
            }
        }

        return result;
    }

    void PerformanceConfigurationManager::reset_performance_metrics()
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        current_metrics_.reset();
        metrics_history_.clear();
    }

    void PerformanceConfigurationManager::register_alert_callback(PerformanceAlertCallback callback)
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        alert_callbacks_.push_back(callback);
    }

    std::vector<PerformanceAlert>
    PerformanceConfigurationManager::get_pending_alerts(bool clear_alerts)
    {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        auto alerts = pending_alerts_;

        if (clear_alerts) {
            pending_alerts_.clear();
        }

        return alerts;
    }

    std::error_code PerformanceConfigurationManager::update_alert_thresholds(
        const PerformanceThresholds& thresholds)
    {
        std::unique_lock<std::shared_mutex> lock(config_mutex_);
        config_.alert_thresholds = thresholds;
        return {};
    }

    std::vector<AutoTuningRecommendation>
    PerformanceConfigurationManager::get_auto_tuning_recommendations() const
    {
        return generate_tuning_recommendations();
    }

    void PerformanceConfigurationManager::set_auto_tuning_enabled(bool enabled)
    {
        std::unique_lock<std::shared_mutex> lock(config_mutex_);
        config_.enable_auto_tuning = enabled;
    }

    void PerformanceConfigurationManager::register_config_change_callback(
        const std::string& component, ConfigurationChangeCallback callback)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        component_callbacks_[component] = callback;
    }

    std::string
    PerformanceConfigurationManager::get_component_configuration(const std::string& component) const
    {
        std::shared_lock<std::shared_mutex> lock(config_mutex_);

        // Convert component configuration to JSON string
        // This is a simplified implementation - in practice would use a proper JSON library
        if (component == "tcp") {
            return "{}"; // Placeholder for TCP config JSON
        } else if (component == "connection_pool") {
            return "{}"; // Placeholder for connection pool config JSON
        } else if (component == "network_buffer") {
            return "{}"; // Placeholder for network buffer config JSON
        } else if (component == "buffer_pool") {
            return "{}"; // Placeholder for buffer pool config JSON
        }

        return "{}";
    }

    std::string
    PerformanceConfigurationManager::generate_performance_report(bool include_history) const
    {
        std::ostringstream report;

        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
        std::lock_guard<std::mutex> alerts_lock(alerts_mutex_);

        report << "ScratchBird Performance Report\n";
        report << "==============================\n\n";

        // Current metrics
        report << "Current Performance Metrics:\n";
        report << "CPU Usage: " << current_metrics_.cpu_usage_percent.load() << "%\n";
        report << "Memory Usage: " << (current_metrics_.memory_usage_bytes.load() / 1024 / 1024)
               << " MB\n";
        report << "Active Connections: " << current_metrics_.network_connections_active.load()
               << "\n";
        report << "Queries Per Second: " << current_metrics_.queries_per_second.load() << "\n";
        report << "Average Query Time: " << current_metrics_.avg_query_time_ms.load() << " ms\n";
        report << "Buffer Hit Ratio: " << current_metrics_.buffer_hit_ratio.load() << "%\n\n";

        // Alert status
        report << "Alert Status:\n";
        report << "Pending Alerts: " << pending_alerts_.size() << "\n\n";

        // Configuration summary
        report << "Configuration Status:\n";
        report << "Performance Monitoring: "
               << (config_.enable_performance_monitoring ? "Enabled" : "Disabled") << "\n";
        report << "Performance Alerts: "
               << (config_.enable_performance_alerts ? "Enabled" : "Disabled") << "\n";
        report << "Auto-Tuning: " << (config_.enable_auto_tuning ? "Enabled" : "Disabled")
               << "\n\n";

        if (include_history) {
            report << "Historical Data Points: " << metrics_history_.size() << "\n";
        }

        return report.str();
    }

    std::vector<std::string> PerformanceConfigurationManager::run_performance_diagnostics() const
    {
        std::vector<std::string> diagnostics;

        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);

        // CPU diagnostics
        auto cpu_usage = current_metrics_.cpu_usage_percent.load();
        if (cpu_usage > config_.alert_thresholds.cpu_usage_critical_percent) {
            diagnostics.push_back("CRITICAL: CPU usage is extremely high (" +
                                  std::to_string(cpu_usage) + "%)");
        } else if (cpu_usage > config_.alert_thresholds.cpu_usage_warning_percent) {
            diagnostics.push_back("WARNING: CPU usage is high (" + std::to_string(cpu_usage) +
                                  "%)");
        }

        // Memory diagnostics
        auto memory_usage_mb = current_metrics_.memory_usage_bytes.load() / 1024 / 1024;
        auto memory_available_mb = current_metrics_.memory_available_bytes.load() / 1024 / 1024;
        auto memory_usage_percent =
            (double)memory_usage_mb / (memory_usage_mb + memory_available_mb) * 100.0;

        if (memory_usage_percent > config_.alert_thresholds.memory_usage_critical_percent) {
            diagnostics.push_back("CRITICAL: Memory usage is extremely high (" +
                                  std::to_string(memory_usage_percent) + "%)");
        } else if (memory_usage_percent > config_.alert_thresholds.memory_usage_warning_percent) {
            diagnostics.push_back("WARNING: Memory usage is high (" +
                                  std::to_string(memory_usage_percent) + "%)");
        }

        // Buffer hit ratio diagnostics
        auto hit_ratio = current_metrics_.buffer_hit_ratio.load();
        if (hit_ratio < config_.alert_thresholds.buffer_hit_ratio_critical) {
            diagnostics.push_back("CRITICAL: Buffer hit ratio is very low (" +
                                  std::to_string(hit_ratio) + "%)");
        } else if (hit_ratio < config_.alert_thresholds.buffer_hit_ratio_warning) {
            diagnostics.push_back("WARNING: Buffer hit ratio is low (" + std::to_string(hit_ratio) +
                                  "%)");
        }

        // Query performance diagnostics
        auto avg_query_time = current_metrics_.avg_query_time_ms.load();
        if (avg_query_time > config_.alert_thresholds.avg_query_time_critical_ms) {
            diagnostics.push_back("CRITICAL: Average query time is very high (" +
                                  std::to_string(avg_query_time) + " ms)");
        } else if (avg_query_time > config_.alert_thresholds.avg_query_time_warning_ms) {
            diagnostics.push_back("WARNING: Average query time is high (" +
                                  std::to_string(avg_query_time) + " ms)");
        }

        if (diagnostics.empty()) {
            diagnostics.push_back("INFO: All performance metrics are within normal ranges");
        }

        return diagnostics;
    }

    // Private methods implementation

    void PerformanceConfigurationManager::monitoring_loop()
    {
        while (!shutdown_requested_) {
            collect_system_metrics();

            std::shared_lock<std::shared_mutex> lock(config_mutex_);
            auto interval = config_.metrics_collection_interval;
            lock.unlock();

            std::this_thread::sleep_for(interval);
        }
    }

    void PerformanceConfigurationManager::alerting_loop()
    {
        while (!shutdown_requested_) {
            check_performance_thresholds();

            std::shared_lock<std::shared_mutex> lock(config_mutex_);
            auto interval = config_.alert_check_interval;
            lock.unlock();

            std::this_thread::sleep_for(interval);
        }
    }

    void PerformanceConfigurationManager::auto_tuning_loop()
    {
        while (!shutdown_requested_) {
            auto recommendations = generate_tuning_recommendations();

            // Apply critical recommendations automatically
            for (const auto& rec : recommendations) {
                if (rec.is_critical && rec.confidence_score > 0.8) {
                    apply_auto_tuning_recommendation(rec);
                }
            }

            std::shared_lock<std::shared_mutex> lock(config_mutex_);
            auto interval = config_.auto_tuning_interval;
            lock.unlock();

            std::this_thread::sleep_for(interval);
        }
    }

    void PerformanceConfigurationManager::check_performance_thresholds()
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
        std::lock_guard<std::mutex> alerts_lock(alerts_mutex_);

        auto& thresholds = config_.alert_thresholds;

        // Check CPU usage
        auto cpu_usage = current_metrics_.cpu_usage_percent.load();
        if (cpu_usage > thresholds.cpu_usage_critical_percent) {
            pending_alerts_.emplace_back(PerformanceAlertType::HIGH_CPU_USAGE,
                                         "CPU usage is critically high", cpu_usage,
                                         thresholds.cpu_usage_critical_percent, "system");
        }

        // Check memory usage
        auto memory_usage_mb = current_metrics_.memory_usage_bytes.load() / 1024 / 1024;
        auto memory_available_mb = current_metrics_.memory_available_bytes.load() / 1024 / 1024;
        auto memory_usage_percent =
            (double)memory_usage_mb / (memory_usage_mb + memory_available_mb) * 100.0;

        if (memory_usage_percent > thresholds.memory_usage_critical_percent) {
            pending_alerts_.emplace_back(PerformanceAlertType::HIGH_MEMORY_USAGE,
                                         "Memory usage is critically high", memory_usage_percent,
                                         thresholds.memory_usage_critical_percent, "system");
        }

        // Check buffer hit ratio
        auto hit_ratio = current_metrics_.buffer_hit_ratio.load();
        if (hit_ratio < thresholds.buffer_hit_ratio_critical) {
            pending_alerts_.emplace_back(PerformanceAlertType::LOW_BUFFER_HIT_RATIO,
                                         "Buffer hit ratio is critically low", hit_ratio,
                                         thresholds.buffer_hit_ratio_critical, "buffer_pool");
        }

        // Check query performance
        auto avg_query_time = current_metrics_.avg_query_time_ms.load();
        if (avg_query_time > thresholds.avg_query_time_critical_ms) {
            pending_alerts_.emplace_back(PerformanceAlertType::HIGH_QUERY_LATENCY,
                                         "Average query time is critically high", avg_query_time,
                                         thresholds.avg_query_time_critical_ms, "executor");
        }

        // Limit number of alerts per type
        if (pending_alerts_.size() > config_.max_alerts_per_type) {
            pending_alerts_.resize(config_.max_alerts_per_type);
        }

        // Notify alert callbacks
        for (const auto& callback : alert_callbacks_) {
            for (const auto& alert : pending_alerts_) {
                callback(alert);
            }
        }
    }

    std::vector<AutoTuningRecommendation>
    PerformanceConfigurationManager::generate_tuning_recommendations() const
    {
        std::vector<AutoTuningRecommendation> recommendations;

        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);

        // Generate recommendations based on current metrics
        auto hit_ratio = current_metrics_.buffer_hit_ratio.load();
        if (hit_ratio < config_.alert_thresholds.buffer_hit_ratio_warning) {
            AutoTuningRecommendation rec;
            rec.component = "buffer_pool";
            rec.parameter = "buffer_pool_size";
            rec.current_value = std::to_string(config_.buffer_pool_config.buffer_count);
            rec.recommended_value = std::to_string(config_.buffer_pool_config.buffer_count * 2);
            rec.reasoning = "Low buffer hit ratio indicates insufficient buffer pool size";
            rec.confidence_score = 0.8;
            rec.is_critical = hit_ratio < config_.alert_thresholds.buffer_hit_ratio_critical;
            recommendations.push_back(rec);
        }

        auto avg_query_time = current_metrics_.avg_query_time_ms.load();
        if (avg_query_time > config_.alert_thresholds.avg_query_time_warning_ms) {
            AutoTuningRecommendation rec;
            rec.component = "executor";
            rec.parameter = "work_mem";
            rec.current_value = "4MB";
            rec.recommended_value = "8MB";
            rec.reasoning = "High query latency may benefit from increased work memory";
            rec.confidence_score = 0.6;
            rec.is_critical = avg_query_time > config_.alert_thresholds.avg_query_time_critical_ms;
            recommendations.push_back(rec);
        }

        return recommendations;
    }

    void PerformanceConfigurationManager::collect_system_metrics()
    {
        // This is a placeholder implementation
        // In a real implementation, this would collect actual system metrics

        std::lock_guard<std::mutex> lock(metrics_mutex_);

        // Simulate some metrics for testing
        current_metrics_.cpu_usage_percent.store(25.0);
        current_metrics_.memory_usage_bytes.store(512 * 1024 * 1024);      // 512 MB
        current_metrics_.memory_available_bytes.store(1536 * 1024 * 1024); // 1536 MB
        current_metrics_.network_connections_active.store(50);
        current_metrics_.queries_per_second.store(100);
        current_metrics_.avg_query_time_ms.store(150.0);
        current_metrics_.buffer_hit_ratio.store(85.0);

        current_metrics_.last_update_time = std::chrono::steady_clock::now();
    }

    void PerformanceConfigurationManager::validate_component_config(
        const std::string& component, ConfigurationValidationResult& result) const
    {
        // Component-specific validation
        if (component == "tcp") {
            // Validate TCP configuration
            if (config_.tcp_config.socket_receive_buffer_size < 4096) {
                result.add_warning("TCP receive buffer size is very small");
            }
        } else if (component == "buffer_pool") {
            // Validate buffer pool configuration
            if (config_.buffer_pool_config.buffer_count < 64) {
                result.add_warning("Buffer pool size is very small");
            }
        }
        // Add more component validations as needed
    }

    std::error_code PerformanceConfigurationManager::notify_component_config_changes(
        const PerformanceConfiguration& old_config, const PerformanceConfiguration& new_config)
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);

        for (const auto& [component, callback] : component_callbacks_) {
            auto error = callback(new_config);
            if (error) {
                return error;
            }
        }

        return {};
    }

    void PerformanceConfigurationManager::load_default_configuration()
    {
        // Load default values for all configuration components
        // This would typically load from a configuration file or use compiled defaults

        current_metrics_.reset();
    }

    void PerformanceConfigurationManager::cleanup_metrics_history()
    {
        if (metrics_history_.size() <= 3600) { // Keep 1 hour of data at 10-second intervals
            return;
        }

        auto cutoff_time = std::chrono::steady_clock::now() - config_.metrics_retention_period;

        metrics_history_.erase(std::remove_if(metrics_history_.begin(), metrics_history_.end(),
                                              [cutoff_time](const PerformanceMetrics& metrics) {
                                                  return metrics.last_update_time < cutoff_time;
                                              }),
                               metrics_history_.end());
    }

    std::error_code PerformanceConfigurationManager::apply_auto_tuning_recommendation(
        const AutoTuningRecommendation& recommendation)
    {
        // Apply the tuning recommendation
        // This would update the specific configuration parameter

        if (recommendation.component == "buffer_pool" &&
            recommendation.parameter == "buffer_pool_size") {
            std::unique_lock<std::shared_mutex> lock(config_mutex_);
            try {
                auto new_size = std::stoul(recommendation.recommended_value);
                config_.buffer_pool_config.buffer_count = new_size;
            } catch (const std::exception&) {
                return std::make_error_code(std::errc::invalid_argument);
            }
        }

        return {};
    }

} // namespace ScratchBird
