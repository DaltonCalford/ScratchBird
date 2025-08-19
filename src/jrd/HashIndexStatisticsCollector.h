/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		HashIndexStatisticsCollector.h
 *	DESCRIPTION:	Enhanced hash index statistics collection and analysis
 *
 * The contents of this file are subject to the Interbase Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy
 * of the License at http://www.Inprise.com/IPL.html
 *
 * Software distributed under the License is distributed on an
 * "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was created by Inprise Corporation
 * and its predecessors. Portions created by Inprise Corporation are
 * Copyright (C) Inprise Corporation.
 *
 * All Rights Reserved.
 * 2025.07.23 - ScratchBird Enhanced Hash Index Statistics Collection
 */

#ifndef JRD_HASH_INDEX_STATISTICS_COLLECTOR_H
#define JRD_HASH_INDEX_STATISTICS_COLLECTOR_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "PersistentHashStorage.h"
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <mutex>
#include <chrono>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class Database;
struct index_desc;
class PersistentHashStorage;

//----------------------------
// Statistics Collection Levels
//----------------------------

enum StatisticsCollectionLevel : UCHAR
{
    STATS_LEVEL_BASIC = 0,              // Basic counters only
    STATS_LEVEL_STANDARD = 1,           // Standard operational statistics
    STATS_LEVEL_DETAILED = 2,           // Detailed performance metrics
    STATS_LEVEL_COMPREHENSIVE = 3,      // Comprehensive analysis
    STATS_LEVEL_DIAGNOSTIC = 4          // Full diagnostic information
};

//----------------------------
// Statistics Time Windows
//----------------------------

enum StatisticsTimeWindow : UCHAR
{
    TIME_WINDOW_CURRENT = 0,            // Current session statistics
    TIME_WINDOW_HOURLY = 1,             // Last hour statistics
    TIME_WINDOW_DAILY = 2,              // Last 24 hours statistics
    TIME_WINDOW_WEEKLY = 3,             // Last week statistics
    TIME_WINDOW_MONTHLY = 4,            // Last month statistics
    TIME_WINDOW_LIFETIME = 5            // Lifetime statistics
};

//----------------------------
// Hash Distribution Analysis
//----------------------------

struct HashDistributionStatistics
{
    // Hash function quality metrics
    double hash_uniformity_score;      // Uniformity of hash distribution (0-1)
    double hash_entropy;               // Shannon entropy of hash values
    ULONG collision_count;              // Total hash collisions
    double collision_rate;              // Collisions per operation
    ULONG max_bucket_size;              // Largest bucket size
    ULONG min_bucket_size;              // Smallest bucket size
    double bucket_size_variance;       // Variance in bucket sizes
    
    // Clustering analysis
    ULONG clustering_coefficient;      // Measure of hash clustering
    std::vector<ULONG> bucket_sizes;   // Size of each bucket
    std::vector<double> load_factors;  // Load factor per bucket
    double gini_coefficient;           // Gini coefficient for distribution inequality
    
    // Hot spot identification
    std::vector<ULONG> hot_buckets;    // Frequently accessed buckets
    std::vector<ULONG> cold_buckets;   // Rarely accessed buckets
    double access_skew_factor;         // Measure of access pattern skew
    ULONG hot_spot_threshold_accesses; // Threshold for hot spot classification
    
    // Performance impact
    double distribution_efficiency;    // Overall distribution efficiency
    ULONG redistribution_recommendations; // Number of buckets needing redistribution
    double expected_performance_gain;  // Expected gain from redistribution
    
    HashDistributionStatistics()
        : hash_uniformity_score(1.0), hash_entropy(0.0), collision_count(0),
          collision_rate(0.0), max_bucket_size(0), min_bucket_size(0),
          bucket_size_variance(0.0), clustering_coefficient(0),
          gini_coefficient(0.0), access_skew_factor(1.0),
          hot_spot_threshold_accesses(100), distribution_efficiency(1.0),
          redistribution_recommendations(0), expected_performance_gain(0.0)
    {
    }
};

//----------------------------
// Performance Metrics
//----------------------------

struct HashIndexPerformanceMetrics
{
    // Operation timing statistics
    struct OperationTiming {
        ULONG operation_count;          // Number of operations
        double total_time_ms;           // Total time for all operations
        double average_time_ms;         // Average time per operation
        double min_time_ms;             // Minimum operation time
        double max_time_ms;             // Maximum operation time
        double std_deviation_ms;        // Standard deviation
        std::vector<double> percentiles; // 50th, 90th, 95th, 99th percentiles
        
        OperationTiming()
            : operation_count(0), total_time_ms(0.0), average_time_ms(0.0),
              min_time_ms(0.0), max_time_ms(0.0), std_deviation_ms(0.0)
        {
            percentiles.resize(4, 0.0); // P50, P90, P95, P99
        }
    };
    
    OperationTiming insert_timing;      // Insert operation timing
    OperationTiming lookup_timing;      // Lookup operation timing
    OperationTiming delete_timing;      // Delete operation timing
    OperationTiming update_timing;      // Update operation timing
    OperationTiming scan_timing;        // Scan operation timing
    
    // Throughput metrics
    double operations_per_second;      // Overall operations per second
    double inserts_per_second;         // Insert throughput
    double lookups_per_second;         // Lookup throughput
    double deletes_per_second;         // Delete throughput
    double peak_ops_per_second;        // Peak throughput observed
    
    // Concurrency metrics
    ULONG max_concurrent_operations;   // Maximum concurrent operations
    double average_concurrency_level;  // Average concurrency level
    ULONG lock_contention_count;       // Lock contention events
    double lock_wait_time_ms;          // Total lock wait time
    
    // I/O performance
    ULONG disk_reads;                  // Total disk reads
    ULONG disk_writes;                 // Total disk writes
    double disk_read_time_ms;          // Total disk read time
    double disk_write_time_ms;         // Total disk write time
    double average_io_time_ms;         // Average I/O operation time
    
    // Memory usage
    ULONG peak_memory_usage_kb;        // Peak memory usage
    ULONG current_memory_usage_kb;     // Current memory usage
    ULONG cache_hit_count;             // Cache hits
    ULONG cache_miss_count;            // Cache misses
    double cache_hit_ratio;            // Cache hit ratio
    
    HashIndexPerformanceMetrics()
        : operations_per_second(0.0), inserts_per_second(0.0),
          lookups_per_second(0.0), deletes_per_second(0.0),
          peak_ops_per_second(0.0), max_concurrent_operations(0),
          average_concurrency_level(0.0), lock_contention_count(0),
          lock_wait_time_ms(0.0), disk_reads(0), disk_writes(0),
          disk_read_time_ms(0.0), disk_write_time_ms(0.0),
          average_io_time_ms(0.0), peak_memory_usage_kb(0),
          current_memory_usage_kb(0), cache_hit_count(0),
          cache_miss_count(0), cache_hit_ratio(1.0)
    {
    }
};

//----------------------------
// Access Pattern Analysis
//----------------------------

struct AccessPatternStatistics
{
    // Temporal patterns
    std::map<UCHAR, ULONG> hourly_access_pattern;    // Access count by hour
    std::map<UCHAR, ULONG> daily_access_pattern;     // Access count by day of week
    std::vector<ULONG> access_frequency_histogram;   // Access frequency distribution
    
    // Spatial patterns
    std::vector<ULONG> bucket_access_counts;         // Access count per bucket
    std::vector<double> bucket_access_rates;         // Access rate per bucket
    std::map<ULONG, ULONG> key_access_frequency;     // Access frequency per key hash
    
    // Sequential access detection
    ULONG sequential_access_runs;                    // Number of sequential access patterns
    double sequential_access_percentage;            // Percentage of accesses that are sequential
    ULONG average_sequential_run_length;            // Average length of sequential runs
    ULONG max_sequential_run_length;                // Maximum sequential run length
    
    // Random access characteristics
    double access_randomness_score;                 // Measure of access randomness (0-1)
    double access_locality_score;                   // Measure of temporal/spatial locality
    ULONG working_set_size;                         // Estimated working set size
    double working_set_stability;                   // Stability of working set over time
    
    // Predictability analysis
    double access_predictability_score;            // How predictable access patterns are
    std::vector<ULONG> predicted_hot_keys;          // Keys predicted to be accessed soon
    double prediction_accuracy;                     // Accuracy of access predictions
    
    AccessPatternStatistics()
        : sequential_access_runs(0), sequential_access_percentage(0.0),
          average_sequential_run_length(0), max_sequential_run_length(0),
          access_randomness_score(0.5), access_locality_score(0.5),
          working_set_size(0), working_set_stability(0.0),
          access_predictability_score(0.0), prediction_accuracy(0.0)
    {
    }
};

//----------------------------
// Storage Efficiency Analysis
//----------------------------

struct StorageEfficiencyStatistics
{
    // Space utilization
    ULONG total_allocated_pages;       // Total pages allocated to index
    ULONG used_pages;                  // Pages actually containing data
    ULONG free_pages;                  // Completely free pages
    ULONG partially_used_pages;        // Pages with some free space
    double space_utilization_ratio;    // Used space / allocated space
    ULONG wasted_space_bytes;          // Total wasted space
    
    // Fragmentation analysis
    double internal_fragmentation;     // Fragmentation within pages
    double external_fragmentation;     // Fragmentation between pages
    ULONG fragmented_pages;            // Number of fragmented pages
    double average_page_fill_ratio;    // Average page fill percentage
    
    // Compression effectiveness
    ULONG compressed_pages;            // Number of compressed pages
    ULONG uncompressed_size_kb;        // Size before compression
    ULONG compressed_size_kb;          // Size after compression
    double compression_ratio;          // Compression effectiveness
    double compression_cpu_overhead;   // CPU cost of compression
    
    // Growth patterns
    ULONG page_allocations;            // Number of page allocations
    ULONG page_deallocations;          // Number of page deallocations
    double growth_rate_pages_per_day;  // Average growth rate
    ULONG predicted_size_in_30_days;   // Predicted size in 30 days
    
    // Reorganization recommendations
    bool needs_reorganization;         // True if reorganization recommended
    ULONG estimated_reorganization_benefit_kb; // Expected space savings
    double estimated_reorganization_time_minutes; // Expected reorganization time
    
    StorageEfficiencyStatistics()
        : total_allocated_pages(0), used_pages(0), free_pages(0),
          partially_used_pages(0), space_utilization_ratio(1.0),
          wasted_space_bytes(0), internal_fragmentation(0.0),
          external_fragmentation(0.0), fragmented_pages(0),
          average_page_fill_ratio(1.0), compressed_pages(0),
          uncompressed_size_kb(0), compressed_size_kb(0),
          compression_ratio(1.0), compression_cpu_overhead(0.0),
          page_allocations(0), page_deallocations(0),
          growth_rate_pages_per_day(0.0), predicted_size_in_30_days(0),
          needs_reorganization(false), estimated_reorganization_benefit_kb(0),
          estimated_reorganization_time_minutes(0.0)
    {
    }
};

//----------------------------
// Statistics Collection Configuration
//----------------------------

struct StatisticsCollectionConfig
{
    // Collection settings
    StatisticsCollectionLevel collection_level;    // Detail level of collection
    bool real_time_collection;                     // Enable real-time collection
    ULONG collection_interval_seconds;             // Collection interval
    bool collect_on_operations;                    // Collect stats on each operation
    
    // Sampling configuration
    bool use_sampling;                             // Use statistical sampling
    double sampling_rate;                          // Sampling rate (0.0-1.0)
    ULONG min_sample_size;                         // Minimum sample size
    ULONG max_sample_size;                         // Maximum sample size
    
    // Storage settings
    bool persist_statistics;                       // Store statistics to disk
    ULONG max_history_days;                        // Days of history to keep
    bool compress_historical_data;                 // Compress old statistics
    
    // Performance impact control
    double max_collection_overhead_percent;       // Maximum overhead allowed
    bool adaptive_collection;                     // Adapt collection based on load
    bool disable_during_high_load;                // Disable during high load
    double high_load_threshold;                   // Load threshold for disabling
    
    // Analysis settings
    bool enable_predictive_analysis;              // Enable predictive analysis
    bool enable_anomaly_detection;                // Enable anomaly detection
    bool enable_trend_analysis;                   // Enable trend analysis
    ULONG analysis_window_hours;                  // Window for analysis
    
    StatisticsCollectionConfig()
        : collection_level(STATS_LEVEL_STANDARD), real_time_collection(true),
          collection_interval_seconds(60), collect_on_operations(false),
          use_sampling(true), sampling_rate(0.1), min_sample_size(1000),
          max_sample_size(10000), persist_statistics(true),
          max_history_days(30), compress_historical_data(true),
          max_collection_overhead_percent(2.0), adaptive_collection(true),
          disable_during_high_load(true), high_load_threshold(0.8),
          enable_predictive_analysis(true), enable_anomaly_detection(true),
          enable_trend_analysis(true), analysis_window_hours(24)
    {
    }
};

//----------------------------
// Real-time Statistics Collector
//----------------------------

/**
 * Real-time statistics collection for hash indexes
 */
class RealTimeStatisticsCollector
{
public:
    explicit RealTimeStatisticsCollector(MemoryPool* pool, const index_desc* idx);
    ~RealTimeStatisticsCollector();

    // Collection lifecycle
    bool initialize(const StatisticsCollectionConfig& config);
    void shutdown();
    bool isActive() const { return m_is_active.load(); }
    
    // Operation recording
    void recordInsertOperation(ULONG bucket_id, ULONG hash_value, USHORT key_length,
                              double execution_time_ms, bool success);
    
    void recordLookupOperation(ULONG bucket_id, ULONG hash_value, USHORT key_length,
                              double execution_time_ms, bool cache_hit, bool found);
    
    void recordDeleteOperation(ULONG bucket_id, ULONG hash_value, USHORT key_length,
                              double execution_time_ms, bool success);
    
    void recordUpdateOperation(ULONG bucket_id, ULONG hash_value, USHORT key_length,
                              double execution_time_ms, bool success);
    
    void recordScanOperation(ULONG start_bucket, ULONG end_bucket, ULONG records_scanned,
                            double execution_time_ms);
    
    // Batch recording for performance
    void recordOperationBatch(const std::vector<struct OperationRecord>& operations);
    
    // Statistics retrieval
    HashIndexPerformanceMetrics getPerformanceMetrics(StatisticsTimeWindow window) const;
    HashDistributionStatistics getDistributionStatistics() const;
    AccessPatternStatistics getAccessPatternStatistics(StatisticsTimeWindow window) const;
    StorageEfficiencyStatistics getStorageStatistics() const;
    
    // Real-time monitoring
    struct RealTimeMetrics {
        double current_ops_per_second;     // Current operations per second
        ULONG concurrent_operations;       // Currently executing operations
        double current_response_time_ms;   // Current average response time
        ULONG current_memory_usage_kb;     // Current memory usage
        double current_cpu_usage_percent;  // Current CPU usage
        bool is_experiencing_contention;   // True if high contention detected
        
        RealTimeMetrics()
            : current_ops_per_second(0.0), concurrent_operations(0),
              current_response_time_ms(0.0), current_memory_usage_kb(0),
              current_cpu_usage_percent(0.0), is_experiencing_contention(false)
        {
        }
    };
    
    RealTimeMetrics getCurrentMetrics() const;
    
    // Configuration management
    void updateConfiguration(const StatisticsCollectionConfig& config);
    StatisticsCollectionConfig getConfiguration() const;
    
    // Sampling control
    void setSamplingRate(double rate);
    double getSamplingRate() const;
    bool shouldSampleOperation() const;

private:
    MemoryPool* m_pool;
    const index_desc* m_index_descriptor;
    std::atomic<bool> m_is_active;
    
    // Configuration
    StatisticsCollectionConfig m_config;
    mutable std::mutex m_config_mutex;
    
    // Operation recording structures
    struct OperationRecord {
        enum OperationType {
            OP_INSERT = 0,
            OP_LOOKUP = 1,
            OP_DELETE = 2,
            OP_UPDATE = 3,
            OP_SCAN = 4
        };
        
        OperationType operation_type;
        std::chrono::high_resolution_clock::time_point timestamp;
        ULONG bucket_id;
        ULONG hash_value;
        USHORT key_length;
        double execution_time_ms;
        bool success;
        bool cache_hit;  // For lookups
        ULONG records_affected;  // For scans
        
        OperationRecord()
            : operation_type(OP_INSERT), bucket_id(0), hash_value(0),
              key_length(0), execution_time_ms(0.0), success(false),
              cache_hit(false), records_affected(0)
        {
        }
    };
    
    // Circular buffer for operation history
    std::vector<OperationRecord> m_operation_history;
    std::atomic<ULONG> m_history_write_index;
    std::atomic<ULONG> m_history_count;
    mutable std::mutex m_history_mutex;
    
    static constexpr ULONG HISTORY_BUFFER_SIZE = 100000;
    
    // Real-time metrics
    mutable RealTimeMetrics m_current_metrics;
    std::atomic<ULONG> m_concurrent_operations;
    mutable std::mutex m_metrics_mutex;
    
    // Sampling state
    mutable std::atomic<ULONG> m_sample_counter;
    ULONG m_sample_modulus;
    
    // Performance tracking
    std::chrono::high_resolution_clock::time_point m_collection_start_time;
    std::atomic<ULONG> m_total_operations_recorded;
    std::atomic<double> m_collection_overhead_ms;
    
    // Internal methods
    void addOperationRecord(const OperationRecord& record);
    void updateRealTimeMetrics(const OperationRecord& record);
    void calculateCurrentMetrics() const;
    
    // Analysis helpers
    std::vector<OperationRecord> getOperationsInWindow(StatisticsTimeWindow window) const;
    void calculatePerformanceMetrics(const std::vector<OperationRecord>& operations,
                                   HashIndexPerformanceMetrics& metrics) const;
    void calculateAccessPatterns(const std::vector<OperationRecord>& operations,
                                AccessPatternStatistics& patterns) const;
    
    // Sampling implementation
    void updateSamplingModulus();
    bool isOperationSampled() const;
};

//----------------------------
// Historical Statistics Manager
//----------------------------

/**
 * Manages historical statistics data and trends
 */
class HistoricalStatisticsManager
{
public:
    explicit HistoricalStatisticsManager(MemoryPool* pool, Database* database);
    ~HistoricalStatisticsManager();

    // Data management
    bool storeStatisticsSnapshot(const index_desc* idx,
                                const HashIndexPerformanceMetrics& performance,
                                const HashDistributionStatistics& distribution,
                                const AccessPatternStatistics& access_patterns,
                                const StorageEfficiencyStatistics& storage);
    
    bool loadStatisticsHistory(const index_desc* idx, StatisticsTimeWindow window,
                              std::vector<struct StatisticsSnapshot>& snapshots) const;
    
    // Trend analysis
    struct TrendAnalysis {
        // Performance trends
        double performance_trend;          // Overall performance trend (-1 to 1)
        double throughput_trend;           // Throughput trend
        double response_time_trend;        // Response time trend
        double error_rate_trend;           // Error rate trend
        
        // Capacity trends
        double storage_growth_trend;       // Storage growth trend
        double memory_usage_trend;         // Memory usage trend
        ULONG projected_size_30_days;      // Projected size in 30 days
        
        // Quality trends
        double distribution_quality_trend; // Hash distribution quality trend
        double fragmentation_trend;        // Fragmentation trend
        double cache_efficiency_trend;     // Cache efficiency trend
        
        // Anomaly detection
        bool has_performance_anomalies;    // True if anomalies detected
        bool has_capacity_anomalies;       // True if capacity anomalies detected
        std::vector<ScratchBird::string> anomaly_descriptions; // Anomaly descriptions
        
        TrendAnalysis()
            : performance_trend(0.0), throughput_trend(0.0),
              response_time_trend(0.0), error_rate_trend(0.0),
              storage_growth_trend(0.0), memory_usage_trend(0.0),
              projected_size_30_days(0), distribution_quality_trend(0.0),
              fragmentation_trend(0.0), cache_efficiency_trend(0.0),
              has_performance_anomalies(false), has_capacity_anomalies(false)
        {
        }
    };
    
    TrendAnalysis analyzeTrends(const index_desc* idx, StatisticsTimeWindow window) const;
    
    // Forecasting
    HashIndexPerformanceMetrics forecastPerformance(const index_desc* idx, ULONG days_ahead) const;
    ULONG forecastStorageRequirements(const index_desc* idx, ULONG days_ahead) const;
    
    // Data maintenance
    void cleanupOldData(ULONG days_to_keep);
    void compressHistoricalData(ULONG days_old_threshold);
    ULONG getHistoricalDataSize() const;
    
    // Reporting
    ScratchBird::string generateTrendReport(const index_desc* idx, StatisticsTimeWindow window) const;
    ScratchBird::string generateCapacityPlanningReport(const index_desc* idx) const;

private:
    MemoryPool* m_pool;
    Database* m_database;
    
    struct StatisticsSnapshot {
        GDS_TIMESTAMP timestamp;
        USHORT index_id;
        HashIndexPerformanceMetrics performance;
        HashDistributionStatistics distribution;
        AccessPatternStatistics access_patterns;
        StorageEfficiencyStatistics storage;
        
        StatisticsSnapshot() : timestamp(0), index_id(0) {}
    };
    
    // Storage
    std::vector<StatisticsSnapshot> m_snapshots;
    mutable std::mutex m_snapshots_mutex;
    
    // Trend calculation helpers
    double calculateLinearTrend(const std::vector<double>& values) const;
    double calculateSeasonalTrend(const std::vector<double>& values, ULONG period) const;
    bool detectAnomalies(const std::vector<double>& values, std::vector<ULONG>& anomaly_indices) const;
    
    // Forecasting algorithms
    double simpleLinearForecast(const std::vector<double>& historical_values, ULONG periods_ahead) const;
    double exponentialSmoothingForecast(const std::vector<double>& historical_values, ULONG periods_ahead) const;
    
    // Data persistence
    bool persistSnapshot(const StatisticsSnapshot& snapshot);
    bool loadPersistedSnapshots();
};

//----------------------------
// Statistics Analysis Engine
//----------------------------

/**
 * Advanced analysis engine for hash index statistics
 */
class StatisticsAnalysisEngine
{
public:
    explicit StatisticsAnalysisEngine(MemoryPool* pool);
    ~StatisticsAnalysisEngine();

    // Comprehensive analysis
    struct ComprehensiveAnalysis {
        // Overall health score
        double overall_health_score;       // Overall index health (0-1)
        
        // Component scores
        double performance_score;          // Performance health score
        double distribution_score;         // Hash distribution quality score
        double storage_efficiency_score;   // Storage efficiency score
        double access_pattern_score;       // Access pattern efficiency score
        
        // Recommendations
        std::vector<ScratchBird::string> performance_recommendations;
        std::vector<ScratchBird::string> storage_recommendations;
        std::vector<ScratchBird::string> distribution_recommendations;
        
        // Urgency flags
        bool requires_immediate_attention; // Critical issues found
        bool optimization_recommended;     // Optimization would help
        bool maintenance_due;              // Maintenance needed
        
        // Predicted issues
        std::vector<ScratchBird::string> predicted_issues;
        ULONG days_until_critical;         // Days until critical issues
        
        ComprehensiveAnalysis()
            : overall_health_score(1.0), performance_score(1.0),
              distribution_score(1.0), storage_efficiency_score(1.0),
              access_pattern_score(1.0), requires_immediate_attention(false),
              optimization_recommended(false), maintenance_due(false),
              days_until_critical(0)
        {
        }
    };
    
    ComprehensiveAnalysis analyzeIndex(const index_desc* idx,
                                      const HashIndexPerformanceMetrics& performance,
                                      const HashDistributionStatistics& distribution,
                                      const AccessPatternStatistics& access_patterns,
                                      const StorageEfficiencyStatistics& storage,
                                      const HistoricalStatisticsManager::TrendAnalysis& trends) const;
    
    // Specific analyses
    double analyzeHashQuality(const HashDistributionStatistics& distribution) const;
    double analyzePerformanceEfficiency(const HashIndexPerformanceMetrics& performance) const;
    double analyzeStorageOptimization(const StorageEfficiencyStatistics& storage) const;
    double analyzeAccessPatternOptimality(const AccessPatternStatistics& patterns) const;
    
    // Anomaly detection
    struct AnomalyDetectionResult {
        bool has_anomalies;                // True if anomalies detected
        std::vector<ScratchBird::string> anomaly_types; // Types of anomalies found
        std::vector<double> anomaly_scores; // Severity scores for each anomaly
        ScratchBird::string root_cause_analysis; // Probable root causes
        
        AnomalyDetectionResult() : has_anomalies(false) {}
    };
    
    AnomalyDetectionResult detectAnomalies(const HashIndexPerformanceMetrics& current_performance,
                                          const HashIndexPerformanceMetrics& baseline_performance) const;
    
    // Capacity planning
    struct CapacityPlanningAnalysis {
        ULONG current_capacity_utilization_percent; // Current capacity usage
        ULONG projected_capacity_30_days;          // Projected capacity in 30 days
        ULONG projected_capacity_90_days;          // Projected capacity in 90 days
        bool capacity_expansion_needed;            // True if expansion needed
        GDS_TIMESTAMP expansion_recommended_date;   // When expansion is recommended
        ULONG recommended_expansion_size_mb;       // Recommended expansion size
        
        CapacityPlanningAnalysis()
            : current_capacity_utilization_percent(0), projected_capacity_30_days(0),
              projected_capacity_90_days(0), capacity_expansion_needed(false),
              expansion_recommended_date(0), recommended_expansion_size_mb(0)
        {
        }
    };
    
    CapacityPlanningAnalysis analyzeCapacityRequirements(const StorageEfficiencyStatistics& storage,
                                                        const HistoricalStatisticsManager::TrendAnalysis& trends) const;
    
    // Optimization recommendations
    std::vector<ScratchBird::string> generateOptimizationRecommendations(const ComprehensiveAnalysis& analysis) const;
    
    // Predictive analysis
    double predictPerformanceDegradation(const HashIndexPerformanceMetrics& performance,
                                        const HistoricalStatisticsManager::TrendAnalysis& trends) const;

private:
    MemoryPool* m_pool;
    
    // Analysis thresholds
    struct AnalysisThresholds {
        double good_performance_threshold;      // Threshold for good performance
        double acceptable_distribution_score;   // Acceptable distribution quality
        double efficient_storage_threshold;     // Efficient storage utilization
        double optimal_access_pattern_score;    // Optimal access pattern score
        
        AnalysisThresholds()
            : good_performance_threshold(0.8), acceptable_distribution_score(0.7),
              efficient_storage_threshold(0.8), optimal_access_pattern_score(0.7)
        {
        }
    } m_thresholds;
    
    // Scoring algorithms
    double calculatePerformanceScore(const HashIndexPerformanceMetrics& performance) const;
    double calculateDistributionScore(const HashDistributionStatistics& distribution) const;
    double calculateStorageScore(const StorageEfficiencyStatistics& storage) const;
    double calculateAccessPatternScore(const AccessPatternStatistics& patterns) const;
    
    // Health assessment
    double calculateOverallHealthScore(double perf_score, double dist_score, 
                                     double storage_score, double access_score) const;
    
    // Recommendation generation
    void generatePerformanceRecommendations(const HashIndexPerformanceMetrics& performance,
                                          std::vector<ScratchBird::string>& recommendations) const;
    void generateStorageRecommendations(const StorageEfficiencyStatistics& storage,
                                      std::vector<ScratchBird::string>& recommendations) const;
    void generateDistributionRecommendations(const HashDistributionStatistics& distribution,
                                           std::vector<ScratchBird::string>& recommendations) const;
    
    // Statistical analysis helpers
    double calculateZScore(double value, double mean, double std_dev) const;
    bool isStatisticallySignificant(double value1, double value2, double variance) const;
    double calculateConfidenceInterval(const std::vector<double>& values, double confidence_level) const;
};

//----------------------------
// Enhanced Hash Index Statistics Collector
//----------------------------

/**
 * Main statistics collector that coordinates all collection and analysis activities
 */
class HashIndexStatisticsCollector
{
public:
    explicit HashIndexStatisticsCollector(MemoryPool* pool, Database* database);
    ~HashIndexStatisticsCollector();

    // Collector lifecycle
    bool initialize();
    void shutdown();
    bool isRunning() const;
    
    // Index registration
    bool registerIndex(const index_desc* idx, const StatisticsCollectionConfig& config = StatisticsCollectionConfig());
    void unregisterIndex(const index_desc* idx);
    bool isIndexRegistered(const index_desc* idx) const;
    
    // Statistics collection control
    void startCollection(const index_desc* idx);
    void stopCollection(const index_desc* idx);
    void pauseCollection(const index_desc* idx);
    void resumeCollection(const index_desc* idx);
    
    // Configuration management
    void updateCollectionConfig(const index_desc* idx, const StatisticsCollectionConfig& config);
    StatisticsCollectionConfig getCollectionConfig(const index_desc* idx) const;
    
    // Statistics retrieval
    HashIndexPerformanceMetrics getPerformanceStatistics(const index_desc* idx, 
                                                         StatisticsTimeWindow window = TIME_WINDOW_CURRENT) const;
    
    HashDistributionStatistics getDistributionStatistics(const index_desc* idx) const;
    
    AccessPatternStatistics getAccessPatternStatistics(const index_desc* idx,
                                                       StatisticsTimeWindow window = TIME_WINDOW_CURRENT) const;
    
    StorageEfficiencyStatistics getStorageStatistics(const index_desc* idx) const;
    
    // Analysis and reporting
    StatisticsAnalysisEngine::ComprehensiveAnalysis getComprehensiveAnalysis(const index_desc* idx) const;
    
    HistoricalStatisticsManager::TrendAnalysis getTrendAnalysis(const index_desc* idx,
                                                               StatisticsTimeWindow window = TIME_WINDOW_WEEKLY) const;
    
    StatisticsAnalysisEngine::CapacityPlanningAnalysis getCapacityAnalysis(const index_desc* idx) const;
    
    // Reporting
    ScratchBird::string generateStatisticsReport(const index_desc* idx, StatisticsCollectionLevel detail_level) const;
    ScratchBird::string generateHealthReport(const index_desc* idx) const;
    ScratchBird::string generatePerformanceDashboard(const index_desc* idx) const;
    
    // Global operations
    std::vector<const index_desc*> getAllMonitoredIndexes() const;
    ScratchBird::string generateGlobalStatisticsReport() const;
    
    // Maintenance operations
    void performStatisticsMaintenance();
    void cleanupOldStatistics(ULONG days_to_keep = 30);
    void optimizeStatisticsStorage();
    
    // Event notifications
    void onIndexOperation(const index_desc* idx, const ScratchBird::string& operation_type,
                         double execution_time_ms, bool success);
    
    void onIndexReorganization(const index_desc* idx);
    void onIndexRebuild(const index_desc* idx);

private:
    MemoryPool* m_pool;
    Database* m_database;
    bool m_is_running;
    
    // Component managers
    std::map<USHORT, std::unique_ptr<RealTimeStatisticsCollector>> m_real_time_collectors;
    std::unique_ptr<HistoricalStatisticsManager> m_historical_manager;
    std::unique_ptr<StatisticsAnalysisEngine> m_analysis_engine;
    
    mutable std::mutex m_collectors_mutex;
    
    // Global statistics
    struct GlobalStatistics {
        ULONG total_indexes_monitored;     // Total indexes being monitored
        ULONG active_collectors;           // Active collectors
        ULONG total_operations_recorded;   // Total operations recorded
        ULONG total_statistics_size_kb;    // Total size of statistics data
        double average_collection_overhead; // Average collection overhead
        
        GlobalStatistics()
            : total_indexes_monitored(0), active_collectors(0),
              total_operations_recorded(0), total_statistics_size_kb(0),
              average_collection_overhead(0.0)
        {
        }
    } m_global_statistics;
    
    mutable std::mutex m_global_stats_mutex;
    
    // Internal helpers
    RealTimeStatisticsCollector* findCollector(const index_desc* idx);
    const RealTimeStatisticsCollector* findCollector(const index_desc* idx) const;
    
    void updateGlobalStatistics();
    void performScheduledMaintenance();
    
    // Maintenance thread
    void maintenanceThreadProc();
    std::unique_ptr<std::thread> m_maintenance_thread;
    std::atomic<bool> m_maintenance_active;
};

//----------------------------
// Global Statistics Manager
//----------------------------

/**
 * Global manager for hash index statistics across all databases
 */
class GlobalHashIndexStatisticsManager
{
public:
    static GlobalHashIndexStatisticsManager* getInstance();
    
    // Database registration
    void registerDatabase(Database* database);
    void unregisterDatabase(Database* database);
    
    // Global operations
    HashIndexStatisticsCollector* getCollectorForDatabase(Database* database);
    
    // Global reporting
    ScratchBird::string generateGlobalHealthReport() const;
    ScratchBird::string generateGlobalPerformanceReport() const;
    ScratchBird::string generateGlobalCapacityReport() const;
    
    // Global configuration
    void setGlobalCollectionConfig(const StatisticsCollectionConfig& config);
    StatisticsCollectionConfig getGlobalCollectionConfig() const;

private:
    GlobalHashIndexStatisticsManager();
    ~GlobalHashIndexStatisticsManager();
    
    static GlobalHashIndexStatisticsManager* s_instance;
    static std::mutex s_instance_mutex;
    
    struct DatabaseCollector {
        Database* database;
        std::unique_ptr<HashIndexStatisticsCollector> collector;
        
        DatabaseCollector(Database* db) : database(db) {}
    };
    
    std::vector<DatabaseCollector> m_database_collectors;
    mutable std::mutex m_collectors_mutex;
    
    StatisticsCollectionConfig m_global_config;
    
    DatabaseCollector* findCollectorForDatabase(Database* database);
};

//----------------------------
// Integration Hooks
//----------------------------

/**
 * Integration points for hash index statistics collection
 */
class HashIndexStatisticsIntegration
{
public:
    // System integration
    static void registerStatisticsCollector();
    static void unregisterStatisticsCollector();
    
    // Operation hooks
    static void onHashIndexOperation(thread_db* tdbb, const index_desc* idx,
                                    const ScratchBird::string& operation_type,
                                    double execution_time_ms, bool success);
    
    static void onHashIndexMaintenance(thread_db* tdbb, const index_desc* idx,
                                      const ScratchBird::string& maintenance_type);
    
    // Performance monitoring hooks
    static void reportPerformanceMetrics(const index_desc* idx, 
                                        const HashIndexPerformanceMetrics& metrics);
    
    // Configuration management
    static void loadStatisticsConfiguration(Database* database);
    static void saveStatisticsConfiguration(Database* database);

private:
    static bool s_integration_enabled;
    static std::mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Statistics calculation utilities
double calculatePercentile(const std::vector<double>& values, double percentile);
double calculateStandardDeviation(const std::vector<double>& values);
double calculateShannonEntropy(const std::vector<ULONG>& frequencies);
double calculateGiniCoefficient(const std::vector<ULONG>& values);

// Time series analysis utilities
std::vector<double> calculateMovingAverage(const std::vector<double>& values, ULONG window_size);
double calculateTrendSlope(const std::vector<double>& values);
bool detectSeasonality(const std::vector<double>& values, ULONG period);

// Hash quality assessment utilities
double assessHashUniformity(const std::vector<ULONG>& bucket_sizes);
double calculateCollisionProbability(ULONG total_keys, ULONG bucket_count);
double estimateOptimalBucketCount(ULONG key_count, double target_load_factor = 0.75);

// Report formatting utilities
ScratchBird::string formatStatisticsAsHtml(const HashIndexPerformanceMetrics& metrics);
ScratchBird::string formatStatisticsAsJson(const HashIndexPerformanceMetrics& metrics);
ScratchBird::string formatStatisticsAsXml(const HashIndexPerformanceMetrics& metrics);

} // namespace Jrd

#endif // JRD_HASH_INDEX_STATISTICS_COLLECTOR_H