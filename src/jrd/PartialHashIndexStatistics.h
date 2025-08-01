/*
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 * Copyright (c) 2025 ScratchBird Project
 * and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2025.07.24 - ScratchBird Partial Hash Index Statistics and Monitoring
 */

#ifndef JRD_PARTIAL_HASH_INDEX_STATISTICS_H
#define JRD_PARTIAL_HASH_INDEX_STATISTICS_H

#include "scratchbird.h"
#include "jrd.h"
#include "constants.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/timestamp.h"

namespace Jrd {

// Forward declarations
class PartialHashIndex;
class Database;
class jrd_rel;
class index_desc;
class thread_db;
class jrd_tra;

//----------------------------
// Enhanced Partial Hash Statistics Structure
//----------------------------
struct PartialHashIndexStatistics
{
    // Basic operation counters
    ULONG total_lookups;                    // Total lookup operations
    ULONG successful_lookups;               // Lookups that found matches
    ULONG failed_lookups;                   // Lookups that found no matches
    ULONG total_inserts;                    // Total insert operations
    ULONG successful_inserts;               // Successful inserts
    ULONG failed_inserts;                   // Failed inserts (due to condition)
    ULONG total_deletes;                    // Total delete operations
    ULONG successful_deletes;               // Successful deletes
    
    // Condition evaluation statistics
    ULONG condition_evaluations;            // Total condition evaluations
    ULONG condition_true_results;           // Conditions that evaluated to true
    ULONG condition_false_results;          // Conditions that evaluated to false
    ULONG condition_evaluation_errors;      // Errors during condition evaluation
    ULONG condition_cache_hits;             // Cache hits for condition evaluation
    ULONG condition_cache_misses;           // Cache misses for condition evaluation
    
    // Performance metrics
    double total_lookup_time;               // Total time spent on lookups (microseconds)
    double total_insert_time;               // Total time spent on inserts (microseconds)
    double total_delete_time;               // Total time spent on deletes (microseconds)
    double total_condition_eval_time;       // Total time spent evaluating conditions (microseconds)
    double average_lookup_time;             // Average lookup time (microseconds)
    double average_condition_eval_time;     // Average condition evaluation time (microseconds)
    
    // Index efficiency metrics
    double inclusion_ratio;                 // Ratio of records included in index
    double selectivity;                     // Index selectivity (observed)
    double effectiveness_ratio;             // How effective the partial condition is
    ULONG bucket_count;                     // Current number of buckets
    ULONG total_entries;                    // Total entries in index
    ULONG hash_collisions;                  // Number of hash collisions
    double load_factor;                     // Current load factor
    
    // Maintenance statistics
    ULONG maintenance_operations;           // Number of maintenance operations performed
    ULONG defragmentation_count;            // Number of defragmentations
    ULONG rebuild_count;                    // Number of full rebuilds
    ULONG cache_optimizations;              // Number of cache optimizations
    double last_maintenance_time;           // Timestamp of last maintenance
    double total_maintenance_time;          // Total time spent on maintenance
    
    // Adaptive optimization metrics
    ULONG adaptive_adjustments;             // Number of adaptive adjustments made
    ULONG bucket_expansions;                // Number of bucket expansions
    ULONG bucket_contractions;              // Number of bucket contractions
    double optimization_effectiveness;      // Effectiveness of optimizations (0-1)
    
    // Query pattern analysis
    ULONG equality_scans;                   // Number of equality scans
    ULONG range_scans;                      // Number of range scans
    ULONG full_scans;                       // Number of full scans
    double scan_efficiency;                 // Overall scan efficiency
    
    // Timestamps for tracking
    ScratchBird::TimeStamp creation_time;   // When statistics collection started
    ScratchBird::TimeStamp last_update_time; // Last statistics update
    ScratchBird::TimeStamp last_reset_time; // Last statistics reset
    
    // Constructor
    PartialHashIndexStatistics();
    
    // Reset all statistics
    void reset();
    
    // Calculate derived metrics
    void calculateDerivedMetrics();
    
    // Serialize to string for storage
    ScratchBird::string serialize() const;
    
    // Deserialize from string
    bool deserialize(const ScratchBird::string& data);
};

//----------------------------
// Partial Hash Index Performance Monitor
//----------------------------
class PartialHashIndexPerformanceMonitor
{
public:
    // Constructor
    PartialHashIndexPerformanceMonitor(PartialHashIndex* index, Database* database);
    
    // Destructor
    ~PartialHashIndexPerformanceMonitor();
    
    // Operation tracking methods
    void recordLookup(thread_db* tdbb, bool successful, double elapsed_time);
    void recordInsert(thread_db* tdbb, bool successful, bool condition_passed, double elapsed_time);
    void recordDelete(thread_db* tdbb, bool successful, double elapsed_time);
    void recordConditionEvaluation(thread_db* tdbb, bool result, bool cache_hit, double elapsed_time);
    void recordMaintenance(thread_db* tdbb, USHORT maintenance_type, double elapsed_time);
    
    // Performance analysis
    double calculateCurrentPerformanceScore() const;
    double calculateTrendAnalysis() const;
    bool detectPerformanceDegradation() const;
    bool recommendMaintenance() const;
    USHORT getRecommendedMaintenanceType() const;
    
    // Adaptive optimization
    bool shouldTriggerBucketResize() const;
    ULONG calculateOptimalBucketCount() const;
    bool shouldEnableConditionalCaching() const;
    bool shouldTriggerDefragmentation() const;
    
    // Statistics access
    const PartialHashIndexStatistics& getStatistics() const;
    void resetStatistics();
    void exportStatistics(ScratchBird::string& report) const;
    
    // Persistence
    bool saveStatistics(thread_db* tdbb);
    bool loadStatistics(thread_db* tdbb);
    
    // Real-time monitoring
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    bool isRealTimeMonitoringEnabled() const;
    
private:
    PartialHashIndex* m_index;
    Database* m_database;
    PartialHashIndexStatistics m_statistics;
    
    // Performance tracking
    mutable bool m_statistics_dirty;
    SLONG m_last_performance_check;
    double m_performance_baseline;
    ScratchBird::Array<double> m_performance_history;
    
    // Real-time monitoring
    bool m_real_time_monitoring;
    ScratchBird::Mutex m_statistics_mutex;
    
    // Internal methods
    void updateDerivedMetrics();
    void checkPerformanceThresholds();
    void triggerAdaptiveOptimization();
    double calculatePerformanceScore(const PartialHashIndexStatistics& stats) const;
    void addPerformanceDataPoint(double score);
    void persistStatisticsIfNeeded(thread_db* tdbb);
};

//----------------------------
// Statistics Collection Manager - System-wide statistics aggregation
//----------------------------
class PartialHashIndexStatisticsManager
{
public:
    // Singleton access
    static PartialHashIndexStatisticsManager& getInstance();
    
    // Index registration
    void registerIndex(PartialHashIndex* index, PartialHashIndexPerformanceMonitor* monitor);
    void unregisterIndex(PartialHashIndex* index);
    
    // System-wide statistics
    struct SystemWideStatistics
    {
        ULONG total_partial_indexes;
        ULONG active_partial_indexes;
        ULONG total_operations;
        double average_performance_score;
        double system_efficiency_ratio;
        ULONG total_maintenance_operations;
        double total_condition_evaluation_time;
        ULONG most_effective_indexes;
        ULONG least_effective_indexes;
    };
    
    SystemWideStatistics getSystemWideStatistics() const;
    
    // Performance monitoring
    void collectSystemStatistics();
    void generateSystemReport(ScratchBird::string& report) const;
    void identifyPerformanceBottlenecks(ScratchBird::Array<PartialHashIndex*>& problematic_indexes) const;
    
    // Optimization recommendations
    void generateSystemOptimizationRecommendations(ScratchBird::string& recommendations) const;
    bool recommendGlobalMaintenance() const;
    void triggerSystemWideMaintenance(thread_db* tdbb);
    
    // Historical analysis
    void recordSystemSnapshot();
    void analyzeHistoricalTrends(ScratchBird::string& analysis) const;
    double calculateSystemPerformanceTrend() const;
    
    // Configuration
    void setPerformanceThresholds(double min_performance_score, double efficiency_threshold);
    void setMonitoringInterval(ULONG interval_seconds);
    void enableAutomaticOptimization(bool enable);
    
private:
    // Singleton constructor
    PartialHashIndexStatisticsManager();
    ~PartialHashIndexStatisticsManager();
    
    // Internal data structures
    typedef GenericMap<Pair<PartialHashIndex*, PartialHashIndexPerformanceMonitor*>> IndexMonitorMap;
    IndexMonitorMap m_registered_indexes;
    
    SystemWideStatistics m_system_stats;
    ScratchBird::Array<SystemWideStatistics> m_historical_snapshots;
    
    // Configuration
    double m_min_performance_threshold;
    double m_efficiency_threshold;
    ULONG m_monitoring_interval;
    bool m_automatic_optimization_enabled;
    
    // Thread safety
    mutable ScratchBird::Mutex m_manager_mutex;
    
    // Internal methods
    void updateSystemStatistics();
    void performAutomaticOptimization(thread_db* tdbb);
    void cleanupHistoricalData();
};

//----------------------------
// Statistics Persistence Layer
//----------------------------
class PartialHashIndexStatisticsPersistence
{
public:
    // Save/load individual index statistics
    static bool saveIndexStatistics(thread_db* tdbb, const index_desc* idx, 
                                   const PartialHashIndexStatistics& stats);
    static bool loadIndexStatistics(thread_db* tdbb, const index_desc* idx, 
                                   PartialHashIndexStatistics& stats);
    
    // System-wide statistics persistence
    static bool saveSystemStatistics(thread_db* tdbb, 
                                    const PartialHashIndexStatisticsManager::SystemWideStatistics& stats);
    static bool loadSystemStatistics(thread_db* tdbb, 
                                    PartialHashIndexStatisticsManager::SystemWideStatistics& stats);
    
    // Statistics maintenance
    static bool cleanupOldStatistics(thread_db* tdbb, ULONG days_to_keep);
    static bool backupStatistics(thread_db* tdbb, const ScratchBird::PathName& backup_path);
    static bool restoreStatistics(thread_db* tdbb, const ScratchBird::PathName& restore_path);
    
    // Export/import functionality
    static bool exportStatisticsToCSV(thread_db* tdbb, const ScratchBird::PathName& csv_path);
    static bool exportStatisticsToJSON(thread_db* tdbb, const ScratchBird::PathName& json_path);
    static bool importStatisticsFromJSON(thread_db* tdbb, const ScratchBird::PathName& json_path);
    
private:
    // Internal persistence helpers
    static ScratchBird::string serializeStatistics(const PartialHashIndexStatistics& stats);
    static bool deserializeStatistics(const ScratchBird::string& data, PartialHashIndexStatistics& stats);
    static ScratchBird::string generateStatisticsKey(const index_desc* idx);
};

//----------------------------
// Performance Alert System
//----------------------------
class PartialHashIndexAlertSystem
{
public:
    enum AlertLevel
    {
        ALERT_INFO = 1,
        ALERT_WARNING = 2,
        ALERT_ERROR = 3,
        ALERT_CRITICAL = 4
    };
    
    enum AlertType
    {
        ALERT_PERFORMANCE_DEGRADATION = 1,
        ALERT_HIGH_CONDITION_EVALUATION_TIME = 2,
        ALERT_LOW_INCLUSION_RATIO = 3,
        ALERT_HIGH_COLLISION_RATE = 4,
        ALERT_MAINTENANCE_REQUIRED = 5,
        ALERT_CACHE_INEFFICIENCY = 6,
        ALERT_BUCKET_IMBALANCE = 7
    };
    
    struct Alert
    {
        AlertLevel level;
        AlertType type;
        PartialHashIndex* index;
        ScratchBird::string message;
        ScratchBird::TimeStamp timestamp;
        bool acknowledged;
        ScratchBird::string recommendation;
    };
    
    // Alert generation
    static void generateAlert(AlertLevel level, AlertType type, PartialHashIndex* index,
                            const ScratchBird::string& message, const ScratchBird::string& recommendation = "");
    
    // Alert management
    static void checkAlertConditions(PartialHashIndex* index, const PartialHashIndexStatistics& stats);
    static ScratchBird::Array<Alert> getPendingAlerts();
    static void acknowledgeAlert(ULONG alert_id);
    static void clearAcknowledgedAlerts();
    
    // Alert configuration
    static void setAlertThresholds(double performance_threshold, double inclusion_ratio_threshold,
                                 double collision_rate_threshold);
    static void enableAlertType(AlertType type, bool enable);
    static void setAlertCallback(void (*callback)(const Alert& alert));
    
private:
    static ScratchBird::Array<Alert> s_pending_alerts;
    static ScratchBird::Mutex s_alerts_mutex;
    static double s_performance_threshold;
    static double s_inclusion_ratio_threshold;
    static double s_collision_rate_threshold;
    static void (*s_alert_callback)(const Alert& alert);
};

} // namespace Jrd

#endif // JRD_PARTIAL_HASH_INDEX_STATISTICS_H