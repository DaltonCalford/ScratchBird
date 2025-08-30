// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/performance_config.h"

#include <chrono>
#include <gtest/gtest.h>
#include <thread>

using namespace ScratchBird;

class PerformanceConfigTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create a test configuration
        test_config_.enable_performance_monitoring = true;
        test_config_.metrics_collection_interval = std::chrono::seconds(1);
        test_config_.enable_performance_alerts = true;
        test_config_.alert_check_interval = std::chrono::seconds(1);
        test_config_.enable_auto_tuning = false; // Disable for testing
    }

    void TearDown() override
    {
        if (config_manager_) {
            config_manager_->shutdown();
            config_manager_.reset();
        }
    }

    PerformanceConfiguration test_config_;
    std::unique_ptr<PerformanceConfigurationManager> config_manager_;
};

TEST_F(PerformanceConfigTest, ConstructorAndInitialization)
{
    // Test constructor with default configuration
    EXPECT_NO_THROW({ config_manager_ = std::make_unique<PerformanceConfigurationManager>(); });

    // Test initialization
    EXPECT_FALSE(config_manager_->initialize());

    // Test getting default configuration
    auto config = config_manager_->get_configuration();
    EXPECT_TRUE(config.enable_performance_monitoring);
}

TEST_F(PerformanceConfigTest, ConfigurationValidation)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);

    // Test valid configuration
    auto result = config_manager_->validate_configuration(test_config_);
    EXPECT_TRUE(result.is_valid);
    EXPECT_FALSE(result.has_errors());

    // Test invalid configuration - negative interval
    PerformanceConfiguration invalid_config = test_config_;
    invalid_config.metrics_collection_interval = std::chrono::seconds(-1);

    result = config_manager_->validate_configuration(invalid_config);
    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(result.has_errors());
}

TEST_F(PerformanceConfigTest, ConfigurationUpdate)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Test valid configuration update
    PerformanceConfiguration new_config = test_config_;
    new_config.metrics_collection_interval = std::chrono::seconds(5);

    auto error = config_manager_->update_configuration(new_config);
    EXPECT_FALSE(error);

    // Verify configuration was updated
    auto updated_config = config_manager_->get_configuration();
    EXPECT_EQ(updated_config.metrics_collection_interval.count(), 5);
}

TEST_F(PerformanceConfigTest, MetricsCollection)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Test initial metrics
    auto initial_metrics = config_manager_->get_performance_metrics();
    EXPECT_EQ(initial_metrics.cpu_usage_percent.load(), 0.0);

    // Test metrics update
    PerformanceMetrics test_metrics;
    test_metrics.cpu_usage_percent = 75.0;
    test_metrics.memory_usage_bytes = 1024 * 1024 * 1024; // 1 GB
    test_metrics.queries_per_second = 500;
    test_metrics.buffer_hit_ratio = 90.0;

    config_manager_->update_performance_metrics(test_metrics);

    auto updated_metrics = config_manager_->get_performance_metrics();
    EXPECT_EQ(updated_metrics.cpu_usage_percent.load(), 75.0);
    EXPECT_EQ(updated_metrics.memory_usage_bytes.load(), 1024 * 1024 * 1024);
    EXPECT_EQ(updated_metrics.queries_per_second.load(), 500);
    EXPECT_EQ(updated_metrics.buffer_hit_ratio.load(), 90.0);
}

TEST_F(PerformanceConfigTest, MetricsHistory)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Add multiple metrics entries
    for (int i = 0; i < 5; ++i) {
        PerformanceMetrics metrics;
        metrics.cpu_usage_percent = 50.0 + i * 10;
        metrics.queries_per_second = 100 + i * 50;

        config_manager_->update_performance_metrics(metrics);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Get recent history
    auto history = config_manager_->get_metrics_history(std::chrono::seconds(10));
    EXPECT_EQ(history.size(), 5);

    // Verify history ordering and values
    EXPECT_EQ(history[0].cpu_usage_percent.load(), 50.0);
    EXPECT_EQ(history[4].cpu_usage_percent.load(), 90.0);
}

TEST_F(PerformanceConfigTest, AlertSystem)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);

    // Set low thresholds for testing
    PerformanceThresholds test_thresholds;
    test_thresholds.cpu_usage_critical_percent = 60.0;
    test_thresholds.memory_usage_critical_percent = 60.0;
    test_thresholds.buffer_hit_ratio_critical = 50.0;

    config_manager_->update_alert_thresholds(test_thresholds);
    config_manager_->initialize();

    // Set up alert callback
    std::vector<PerformanceAlert> received_alerts;
    config_manager_->register_alert_callback(
        [&received_alerts](const PerformanceAlert& alert) { received_alerts.push_back(alert); });

    // Trigger alerts with high metrics
    PerformanceMetrics high_metrics;
    high_metrics.cpu_usage_percent = 75.0;                    // Above critical threshold
    high_metrics.memory_usage_bytes = 2048 * 1024 * 1024;     // 2 GB
    high_metrics.memory_available_bytes = 1024 * 1024 * 1024; // 1 GB (67% usage)
    high_metrics.buffer_hit_ratio = 40.0;                     // Below critical threshold

    config_manager_->update_performance_metrics(high_metrics);

    // Allow some time for alert processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check for pending alerts
    auto pending_alerts = config_manager_->get_pending_alerts(false);
    EXPECT_GT(pending_alerts.size(), 0);

    // Verify alert types
    bool found_cpu_alert = false;
    bool found_buffer_alert = false;

    for (const auto& alert : pending_alerts) {
        if (alert.type == PerformanceAlertType::HIGH_CPU_USAGE) {
            found_cpu_alert = true;
            EXPECT_EQ(alert.current_value, 75.0);
        }
        if (alert.type == PerformanceAlertType::LOW_BUFFER_HIT_RATIO) {
            found_buffer_alert = true;
            EXPECT_EQ(alert.current_value, 40.0);
        }
    }

    EXPECT_TRUE(found_cpu_alert);
    EXPECT_TRUE(found_buffer_alert);
}

TEST_F(PerformanceConfigTest, AutoTuningRecommendations)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Set metrics that should trigger tuning recommendations
    PerformanceMetrics poor_metrics;
    poor_metrics.buffer_hit_ratio = 45.0;    // Low hit ratio
    poor_metrics.avg_query_time_ms = 2000.0; // High query time

    config_manager_->update_performance_metrics(poor_metrics);

    // Get tuning recommendations
    auto recommendations = config_manager_->get_auto_tuning_recommendations();
    EXPECT_GT(recommendations.size(), 0);

    // Verify recommendation content
    bool found_buffer_pool_rec = false;
    bool found_executor_rec = false;

    for (const auto& rec : recommendations) {
        if (rec.component == "buffer_pool") {
            found_buffer_pool_rec = true;
            EXPECT_EQ(rec.parameter, "buffer_pool_size");
            EXPECT_GT(rec.confidence_score, 0.0);
            EXPECT_LE(rec.confidence_score, 1.0);
        }
        if (rec.component == "executor") {
            found_executor_rec = true;
            EXPECT_EQ(rec.parameter, "work_mem");
        }
    }

    EXPECT_TRUE(found_buffer_pool_rec);
    EXPECT_TRUE(found_executor_rec);
}

TEST_F(PerformanceConfigTest, ComponentConfigurationManagement)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);

    // Test component configuration callback
    bool callback_called = false;
    std::string callback_component;

    config_manager_->register_config_change_callback(
        "tcp", [&callback_called, &callback_component](const PerformanceConfiguration& config) {
            callback_called = true;
            callback_component = "tcp";
            return std::error_code{};
        });

    config_manager_->initialize();

    // Update configuration to trigger callback
    PerformanceConfiguration new_config = test_config_;
    new_config.metrics_collection_interval = std::chrono::seconds(15);

    config_manager_->update_configuration(new_config);

    EXPECT_TRUE(callback_called);
    EXPECT_EQ(callback_component, "tcp");

    // Test getting component configuration
    auto tcp_config_json = config_manager_->get_component_configuration("tcp");
    EXPECT_FALSE(tcp_config_json.empty());
    EXPECT_EQ(tcp_config_json, "{}"); // Placeholder implementation returns empty JSON
}

TEST_F(PerformanceConfigTest, PerformanceReport)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Set some test metrics
    PerformanceMetrics metrics;
    metrics.cpu_usage_percent = 65.0;
    metrics.memory_usage_bytes = 800 * 1024 * 1024; // 800 MB
    metrics.queries_per_second = 250;
    metrics.avg_query_time_ms = 120.0;
    metrics.buffer_hit_ratio = 88.5;

    config_manager_->update_performance_metrics(metrics);

    // Generate performance report
    auto report = config_manager_->generate_performance_report(false);

    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("ScratchBird Performance Report"), std::string::npos);
    EXPECT_NE(report.find("CPU Usage: 65"), std::string::npos);
    EXPECT_NE(report.find("Buffer Hit Ratio: 88.5"), std::string::npos);

    // Test report with history
    auto detailed_report = config_manager_->generate_performance_report(true);
    EXPECT_GT(detailed_report.length(), report.length());
}

TEST_F(PerformanceConfigTest, DiagnosticsSystem)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Test normal metrics - should report all normal
    PerformanceMetrics normal_metrics;
    normal_metrics.cpu_usage_percent = 30.0;
    normal_metrics.memory_usage_bytes = 400 * 1024 * 1024;      // 400 MB
    normal_metrics.memory_available_bytes = 1600 * 1024 * 1024; // 1600 MB (20% usage)
    normal_metrics.buffer_hit_ratio = 92.0;
    normal_metrics.avg_query_time_ms = 80.0;

    config_manager_->update_performance_metrics(normal_metrics);

    auto diagnostics = config_manager_->run_performance_diagnostics();
    EXPECT_EQ(diagnostics.size(), 1);
    EXPECT_NE(diagnostics[0].find("normal ranges"), std::string::npos);

    // Test problematic metrics - should report issues
    PerformanceMetrics problem_metrics;
    problem_metrics.cpu_usage_percent = 98.0;                   // Critical
    problem_metrics.memory_usage_bytes = 1800 * 1024 * 1024;    // 1800 MB
    problem_metrics.memory_available_bytes = 200 * 1024 * 1024; // 200 MB (90% usage)
    problem_metrics.buffer_hit_ratio = 35.0;                    // Critical
    problem_metrics.avg_query_time_ms = 8000.0;                 // Critical

    config_manager_->update_performance_metrics(problem_metrics);

    diagnostics = config_manager_->run_performance_diagnostics();
    EXPECT_GT(diagnostics.size(), 1);

    // Look for specific diagnostic messages
    bool found_cpu_critical = false;
    bool found_buffer_critical = false;
    bool found_query_critical = false;

    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.find("CRITICAL: CPU usage") != std::string::npos) {
            found_cpu_critical = true;
        }
        if (diagnostic.find("CRITICAL: Buffer hit ratio") != std::string::npos) {
            found_buffer_critical = true;
        }
        if (diagnostic.find("CRITICAL: Average query time") != std::string::npos) {
            found_query_critical = true;
        }
    }

    EXPECT_TRUE(found_cpu_critical);
    EXPECT_TRUE(found_buffer_critical);
    EXPECT_TRUE(found_query_critical);
}

TEST_F(PerformanceConfigTest, MetricsReset)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Set some test metrics
    PerformanceMetrics metrics;
    metrics.cpu_usage_percent = 75.0;
    metrics.queries_per_second = 300;

    config_manager_->update_performance_metrics(metrics);

    // Verify metrics are set
    auto current_metrics = config_manager_->get_performance_metrics();
    EXPECT_EQ(current_metrics.cpu_usage_percent.load(), 75.0);
    EXPECT_EQ(current_metrics.queries_per_second.load(), 300);

    // Reset metrics
    config_manager_->reset_performance_metrics();

    // Verify metrics are reset
    auto reset_metrics = config_manager_->get_performance_metrics();
    EXPECT_EQ(reset_metrics.cpu_usage_percent.load(), 0.0);
    EXPECT_EQ(reset_metrics.queries_per_second.load(), 0);

    // Verify history is cleared
    auto history = config_manager_->get_metrics_history(std::chrono::seconds(10));
    EXPECT_EQ(history.size(), 0);
}

TEST_F(PerformanceConfigTest, AutoTuningEnableDisable)
{
    config_manager_ = std::make_unique<PerformanceConfigurationManager>(test_config_);
    config_manager_->initialize();

    // Initially disabled
    auto config = config_manager_->get_configuration();
    EXPECT_FALSE(config.enable_auto_tuning);

    // Enable auto-tuning
    config_manager_->set_auto_tuning_enabled(true);

    config = config_manager_->get_configuration();
    EXPECT_TRUE(config.enable_auto_tuning);

    // Disable auto-tuning
    config_manager_->set_auto_tuning_enabled(false);

    config = config_manager_->get_configuration();
    EXPECT_FALSE(config.enable_auto_tuning);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
