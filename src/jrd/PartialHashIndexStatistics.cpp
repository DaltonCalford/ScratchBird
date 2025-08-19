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
 * 2025.07.24 - ScratchBird Partial Hash Index Statistics and Monitoring Implementation
 */

#include "scratchbird.h"
#include "PartialHashIndexStatistics.h"
#include "PartialHashIndex.h"
#include "Database.h"
#include "jrd.h"
#include "exe.h"
#include "tra.h"
#include "../common/gdsassert.h"
#include "../common/classes/timestamp.h"
#include "../common/os/os_utils.h"
#include <cmath>
#include <ctime>

using namespace Jrd;
using namespace ScratchBird;

//----------------------------
// PartialHashIndexStatistics Implementation
//----------------------------

PartialHashIndexStatistics::PartialHashIndexStatistics()
{
    reset();
}

void PartialHashIndexStatistics::reset()
{
    // Basic operation counters
    total_lookups = 0;
    successful_lookups = 0;
    failed_lookups = 0;
    total_inserts = 0;
    successful_inserts = 0;
    failed_inserts = 0;
    total_deletes = 0;
    successful_deletes = 0;
    
    // Condition evaluation statistics
    condition_evaluations = 0;
    condition_true_results = 0;
    condition_false_results = 0;
    condition_evaluation_errors = 0;
    condition_cache_hits = 0;
    condition_cache_misses = 0;
    
    // Performance metrics
    total_lookup_time = 0.0;
    total_insert_time = 0.0;
    total_delete_time = 0.0;
    total_condition_eval_time = 0.0;
    average_lookup_time = 0.0;
    average_condition_eval_time = 0.0;
    
    // Index efficiency metrics
    inclusion_ratio = 0.0;
    selectivity = 0.0;
    effectiveness_ratio = 0.0;
    bucket_count = 0;
    total_entries = 0;
    hash_collisions = 0;
    load_factor = 0.0;
    
    // Maintenance statistics
    maintenance_operations = 0;
    defragmentation_count = 0;
    rebuild_count = 0;
    cache_optimizations = 0;
    last_maintenance_time = 0.0;
    total_maintenance_time = 0.0;
    
    // Adaptive optimization metrics
    adaptive_adjustments = 0;
    bucket_expansions = 0;
    bucket_contractions = 0;
    optimization_effectiveness = 0.0;
    
    // Query pattern analysis
    equality_scans = 0;
    range_scans = 0;
    full_scans = 0;
    scan_efficiency = 0.0;
    
    // Timestamps
    creation_time = TimeStamp::getCurrentTimeStamp();
    last_update_time = creation_time;
    last_reset_time = creation_time;
}

void PartialHashIndexStatistics::calculateDerivedMetrics()
{
    // Calculate average times
    if (total_lookups > 0)
        average_lookup_time = total_lookup_time / total_lookups;
    
    if (condition_evaluations > 0)
        average_condition_eval_time = total_condition_eval_time / condition_evaluations;
    
    // Calculate inclusion ratio
    ULONG total_records_processed = successful_inserts + failed_inserts;
    if (total_records_processed > 0)
        inclusion_ratio = (double)successful_inserts / total_records_processed;
    
    // Calculate selectivity
    if (total_lookups > 0)
        selectivity = (double)successful_lookups / total_lookups;
    
    // Calculate load factor
    if (bucket_count > 0)
        load_factor = (double)total_entries / bucket_count;
    
    // Calculate effectiveness ratio (how much the partial condition helps)
    if (condition_evaluations > 0)
        effectiveness_ratio = (double)condition_true_results / condition_evaluations;
    
    // Calculate scan efficiency
    ULONG total_scans = equality_scans + range_scans + full_scans;
    if (total_scans > 0)
    {
        // Weight equality scans higher as they're most efficient for hash indexes
        double weighted_efficiency = (equality_scans * 1.0 + range_scans * 0.3 + full_scans * 0.1) / total_scans;
        scan_efficiency = weighted_efficiency;
    }
    
    last_update_time = TimeStamp::getCurrentTimeStamp();
}

string PartialHashIndexStatistics::serialize() const
{
    string result;
    
    // Use a simple key=value format for persistence
    result.printf("total_lookups=%lu\n", total_lookups);
    result.printf("successful_lookups=%lu\n", successful_lookups);
    result.printf("failed_lookups=%lu\n", failed_lookups);
    result.printf("total_inserts=%lu\n", total_inserts);
    result.printf("successful_inserts=%lu\n", successful_inserts);
    result.printf("failed_inserts=%lu\n", failed_inserts);
    result.printf("total_deletes=%lu\n", total_deletes);
    result.printf("successful_deletes=%lu\n", successful_deletes);
    
    result.printf("condition_evaluations=%lu\n", condition_evaluations);
    result.printf("condition_true_results=%lu\n", condition_true_results);
    result.printf("condition_false_results=%lu\n", condition_false_results);
    result.printf("condition_evaluation_errors=%lu\n", condition_evaluation_errors);
    result.printf("condition_cache_hits=%lu\n", condition_cache_hits);
    result.printf("condition_cache_misses=%lu\n", condition_cache_misses);
    
    result.printf("total_lookup_time=%.6f\n", total_lookup_time);
    result.printf("total_insert_time=%.6f\n", total_insert_time);
    result.printf("total_delete_time=%.6f\n", total_delete_time);
    result.printf("total_condition_eval_time=%.6f\n", total_condition_eval_time);
    result.printf("average_lookup_time=%.6f\n", average_lookup_time);
    result.printf("average_condition_eval_time=%.6f\n", average_condition_eval_time);
    
    result.printf("inclusion_ratio=%.6f\n", inclusion_ratio);
    result.printf("selectivity=%.6f\n", selectivity);
    result.printf("effectiveness_ratio=%.6f\n", effectiveness_ratio);
    result.printf("bucket_count=%lu\n", bucket_count);
    result.printf("total_entries=%lu\n", total_entries);
    result.printf("hash_collisions=%lu\n", hash_collisions);
    result.printf("load_factor=%.6f\n", load_factor);
    
    result.printf("maintenance_operations=%lu\n", maintenance_operations);
    result.printf("defragmentation_count=%lu\n", defragmentation_count);
    result.printf("rebuild_count=%lu\n", rebuild_count);
    result.printf("cache_optimizations=%lu\n", cache_optimizations);
    result.printf("last_maintenance_time=%.6f\n", last_maintenance_time);
    result.printf("total_maintenance_time=%.6f\n", total_maintenance_time);
    
    result.printf("adaptive_adjustments=%lu\n", adaptive_adjustments);
    result.printf("bucket_expansions=%lu\n", bucket_expansions);
    result.printf("bucket_contractions=%lu\n", bucket_contractions);
    result.printf("optimization_effectiveness=%.6f\n", optimization_effectiveness);
    
    result.printf("equality_scans=%lu\n", equality_scans);
    result.printf("range_scans=%lu\n", range_scans);
    result.printf("full_scans=%lu\n", full_scans);
    result.printf("scan_efficiency=%.6f\n", scan_efficiency);
    
    return result;
}

bool PartialHashIndexStatistics::deserialize(const string& data)
{
    // Simple key=value parser
    // In a production implementation, this would be more robust
    
    try {
        auto lines = data.split('\n');
        
        for (const auto& line : lines)
        {
            if (line.isEmpty())
                continue;
                
            auto parts = line.split('=');
            if (parts.getCount() != 2)
                continue;
                
            const string& key = parts[0];
            const string& value = parts[1];
            
            // Parse key-value pairs
            if (key == "total_lookups") total_lookups = value.toULong();
            else if (key == "successful_lookups") successful_lookups = value.toULong();
            else if (key == "failed_lookups") failed_lookups = value.toULong();
            else if (key == "total_inserts") total_inserts = value.toULong();
            else if (key == "successful_inserts") successful_inserts = value.toULong();
            else if (key == "failed_inserts") failed_inserts = value.toULong();
            else if (key == "total_deletes") total_deletes = value.toULong();
            else if (key == "successful_deletes") successful_deletes = value.toULong();
            else if (key == "condition_evaluations") condition_evaluations = value.toULong();
            else if (key == "condition_true_results") condition_true_results = value.toULong();
            else if (key == "condition_false_results") condition_false_results = value.toULong();
            else if (key == "condition_evaluation_errors") condition_evaluation_errors = value.toULong();
            else if (key == "condition_cache_hits") condition_cache_hits = value.toULong();
            else if (key == "condition_cache_misses") condition_cache_misses = value.toULong();
            else if (key == "total_lookup_time") total_lookup_time = value.toDouble();
            else if (key == "total_insert_time") total_insert_time = value.toDouble();
            else if (key == "total_delete_time") total_delete_time = value.toDouble();
            else if (key == "total_condition_eval_time") total_condition_eval_time = value.toDouble();
            else if (key == "average_lookup_time") average_lookup_time = value.toDouble();
            else if (key == "average_condition_eval_time") average_condition_eval_time = value.toDouble();
            else if (key == "inclusion_ratio") inclusion_ratio = value.toDouble();
            else if (key == "selectivity") selectivity = value.toDouble();
            else if (key == "effectiveness_ratio") effectiveness_ratio = value.toDouble();
            else if (key == "bucket_count") bucket_count = value.toULong();
            else if (key == "total_entries") total_entries = value.toULong();
            else if (key == "hash_collisions") hash_collisions = value.toULong();
            else if (key == "load_factor") load_factor = value.toDouble();
            else if (key == "maintenance_operations") maintenance_operations = value.toULong();
            else if (key == "defragmentation_count") defragmentation_count = value.toULong();
            else if (key == "rebuild_count") rebuild_count = value.toULong();
            else if (key == "cache_optimizations") cache_optimizations = value.toULong();
            else if (key == "last_maintenance_time") last_maintenance_time = value.toDouble();
            else if (key == "total_maintenance_time") total_maintenance_time = value.toDouble();
            else if (key == "adaptive_adjustments") adaptive_adjustments = value.toULong();
            else if (key == "bucket_expansions") bucket_expansions = value.toULong();
            else if (key == "bucket_contractions") bucket_contractions = value.toULong();
            else if (key == "optimization_effectiveness") optimization_effectiveness = value.toDouble();
            else if (key == "equality_scans") equality_scans = value.toULong();
            else if (key == "range_scans") range_scans = value.toULong();
            else if (key == "full_scans") full_scans = value.toULong();
            else if (key == "scan_efficiency") scan_efficiency = value.toDouble();
        }
        
        return true;
    }
    catch (const Exception&) {
        return false;
    }
}

//----------------------------
// PartialHashIndexPerformanceMonitor Implementation
//----------------------------

PartialHashIndexPerformanceMonitor::PartialHashIndexPerformanceMonitor(PartialHashIndex* index, Database* database)
    : m_index(index), m_database(database), m_statistics_dirty(false),
      m_last_performance_check(0), m_performance_baseline(0.0),
      m_performance_history(getPool()), m_real_time_monitoring(false)
{
    fb_assert(index && database);
    
    // Initialize performance baseline
    m_performance_baseline = calculatePerformanceScore(m_statistics);
    
    // Load existing statistics if available
    // loadStatistics(nullptr); // Would need thread_db context
}

PartialHashIndexPerformanceMonitor::~PartialHashIndexPerformanceMonitor()
{
    if (m_statistics_dirty)
    {
        // Try to save statistics on destruction
        // saveStatistics(nullptr); // Would need thread_db context
    }
}

void PartialHashIndexPerformanceMonitor::recordLookup(thread_db* tdbb, bool successful, double elapsed_time)
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    m_statistics.total_lookups++;
    m_statistics.total_lookup_time += elapsed_time;
    
    if (successful)
        m_statistics.successful_lookups++;
    else
        m_statistics.failed_lookups++;
    
    // Update derived metrics periodically
    if (m_statistics.total_lookups % 100 == 0)
    {
        updateDerivedMetrics();
        checkPerformanceThresholds();
    }
    
    m_statistics_dirty = true;
}

void PartialHashIndexPerformanceMonitor::recordInsert(thread_db* tdbb, bool successful, bool condition_passed, double elapsed_time)
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    m_statistics.total_inserts++;
    m_statistics.total_insert_time += elapsed_time;
    
    if (successful)
        m_statistics.successful_inserts++;
    else
        m_statistics.failed_inserts++;
    
    // Track condition evaluation if this was related to condition filtering
    if (!condition_passed)
    {
        m_statistics.condition_evaluations++;
        m_statistics.condition_false_results++;
    }
    else if (successful)
    {
        m_statistics.condition_evaluations++;
        m_statistics.condition_true_results++;
    }
    
    updateDerivedMetrics();
    m_statistics_dirty = true;
}

void PartialHashIndexPerformanceMonitor::recordDelete(thread_db* tdbb, bool successful, double elapsed_time)
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    m_statistics.total_deletes++;
    m_statistics.total_delete_time += elapsed_time;
    
    if (successful)
        m_statistics.successful_deletes++;
    
    updateDerivedMetrics();
    m_statistics_dirty = true;
}

void PartialHashIndexPerformanceMonitor::recordConditionEvaluation(thread_db* tdbb, bool result, bool cache_hit, double elapsed_time)
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    m_statistics.condition_evaluations++;
    m_statistics.total_condition_eval_time += elapsed_time;
    
    if (result)
        m_statistics.condition_true_results++;
    else
        m_statistics.condition_false_results++;
    
    if (cache_hit)
        m_statistics.condition_cache_hits++;
    else
        m_statistics.condition_cache_misses++;
    
    updateDerivedMetrics();
    m_statistics_dirty = true;
}

void PartialHashIndexPerformanceMonitor::recordMaintenance(thread_db* tdbb, USHORT maintenance_type, double elapsed_time)
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    m_statistics.maintenance_operations++;
    m_statistics.total_maintenance_time += elapsed_time;
    m_statistics.last_maintenance_time = elapsed_time;
    
    switch (maintenance_type)
    {
        case PARTIAL_HASH_MAINT_DEFRAGMENT:
            m_statistics.defragmentation_count++;
            break;
        case PARTIAL_HASH_MAINT_FULL_REBUILD:
            m_statistics.rebuild_count++;
            break;
        case PARTIAL_HASH_MAINT_OPTIMIZE_CACHE:
            m_statistics.cache_optimizations++;
            break;
        default:
            break;
    }
    
    updateDerivedMetrics();
    m_statistics_dirty = true;
}

double PartialHashIndexPerformanceMonitor::calculateCurrentPerformanceScore() const
{
    return calculatePerformanceScore(m_statistics);
}

double PartialHashIndexPerformanceMonitor::calculateTrendAnalysis() const
{
    if (m_performance_history.getCount() < 2)
        return 0.0;
    
    // Calculate trend based on recent performance history
    double trend = 0.0;
    const ULONG count = m_performance_history.getCount();
    const ULONG recent_points = MIN(count, 10); // Use last 10 data points
    
    for (ULONG i = count - recent_points; i < count - 1; i++)
    {
        trend += (m_performance_history[i + 1] - m_performance_history[i]);
    }
    
    return trend / (recent_points - 1);
}

bool PartialHashIndexPerformanceMonitor::detectPerformanceDegradation() const
{
    double current_score = calculateCurrentPerformanceScore();
    double trend = calculateTrendAnalysis();
    
    // Detect degradation if:
    // 1. Current score is significantly below baseline
    // 2. Trend is negative
    return (current_score < m_performance_baseline * 0.8) || (trend < -0.1);
}

bool PartialHashIndexPerformanceMonitor::recommendMaintenance() const
{
    return detectPerformanceDegradation() || 
           shouldTriggerBucketResize() || 
           shouldTriggerDefragmentation();
}

USHORT PartialHashIndexPerformanceMonitor::getRecommendedMaintenanceType() const
{
    if (shouldTriggerBucketResize())
        return PARTIAL_HASH_MAINT_FULL_REBUILD;
    
    if (shouldTriggerDefragmentation())
        return PARTIAL_HASH_MAINT_DEFRAGMENT;
    
    if (m_statistics.condition_cache_hits < m_statistics.condition_cache_misses)
        return PARTIAL_HASH_MAINT_OPTIMIZE_CACHE;
    
    if (detectPerformanceDegradation())
        return PARTIAL_HASH_MAINT_RECALC_STATS;
    
    return PARTIAL_HASH_MAINT_INTEGRITY_CHECK;
}

bool PartialHashIndexPerformanceMonitor::shouldTriggerBucketResize() const
{
    // Trigger resize if load factor is too high or too low
    return (m_statistics.load_factor > 0.8) || (m_statistics.load_factor < 0.3 && m_statistics.bucket_count > 16);
}

ULONG PartialHashIndexPerformanceMonitor::calculateOptimalBucketCount() const
{
    if (m_statistics.total_entries == 0)
        return PARTIAL_HASH_DEFAULT_BUCKETS;
    
    // Target load factor of 0.6
    ULONG optimal_count = (ULONG)(m_statistics.total_entries / 0.6);
    
    // Round to next power of 2 for better hash distribution
    ULONG power_of_two = 1;
    while (power_of_two < optimal_count)
        power_of_two <<= 1;
    
    return MAX(power_of_two, PARTIAL_HASH_MIN_BUCKETS);
}

bool PartialHashIndexPerformanceMonitor::shouldEnableConditionalCaching() const
{
    // Enable caching if condition evaluation is expensive and cache would help
    return (m_statistics.average_condition_eval_time > 100.0) && // > 100 microseconds
           (m_statistics.condition_cache_misses > m_statistics.condition_cache_hits * 2);
}

bool PartialHashIndexPerformanceMonitor::shouldTriggerDefragmentation() const
{
    // Trigger defragmentation if there are many hash collisions
    double collision_rate = m_statistics.total_entries > 0 ? 
                           (double)m_statistics.hash_collisions / m_statistics.total_entries : 0.0;
    
    return collision_rate > 0.3; // More than 30% collision rate
}

const PartialHashIndexStatistics& PartialHashIndexPerformanceMonitor::getStatistics() const
{
    return m_statistics;
}

void PartialHashIndexPerformanceMonitor::resetStatistics()
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    m_statistics.reset();
    m_performance_history.clear();
    m_performance_baseline = calculatePerformanceScore(m_statistics);
    m_statistics_dirty = true;
}

void PartialHashIndexPerformanceMonitor::exportStatistics(string& report) const
{
    ScratchBird::MutexLockGuard guard(m_statistics_mutex, FB_FUNCTION);
    
    report.printf("=== Partial Hash Index Performance Report ===\n\n");
    
    report.printf("Operation Statistics:\n");
    report.printf("  Total Lookups: %lu (Success: %lu, Failed: %lu)\n", 
                  m_statistics.total_lookups, m_statistics.successful_lookups, m_statistics.failed_lookups);
    report.printf("  Total Inserts: %lu (Success: %lu, Failed: %lu)\n", 
                  m_statistics.total_inserts, m_statistics.successful_inserts, m_statistics.failed_inserts);
    report.printf("  Total Deletes: %lu (Success: %lu)\n", 
                  m_statistics.total_deletes, m_statistics.successful_deletes);
    
    report.printf("\nCondition Evaluation:\n");
    report.printf("  Total Evaluations: %lu (True: %lu, False: %lu, Errors: %lu)\n",
                  m_statistics.condition_evaluations, m_statistics.condition_true_results,
                  m_statistics.condition_false_results, m_statistics.condition_evaluation_errors);
    report.printf("  Cache Performance: %lu hits, %lu misses (%.2f%% hit ratio)\n",
                  m_statistics.condition_cache_hits, m_statistics.condition_cache_misses,
                  m_statistics.condition_cache_hits + m_statistics.condition_cache_misses > 0 ?
                  (double)m_statistics.condition_cache_hits / (m_statistics.condition_cache_hits + m_statistics.condition_cache_misses) * 100.0 : 0.0);
    
    report.printf("\nPerformance Metrics:\n");
    report.printf("  Average Lookup Time: %.2f microseconds\n", m_statistics.average_lookup_time);
    report.printf("  Average Condition Evaluation Time: %.2f microseconds\n", m_statistics.average_condition_eval_time);
    report.printf("  Inclusion Ratio: %.2f%%\n", m_statistics.inclusion_ratio * 100.0);
    report.printf("  Selectivity: %.2f%%\n", m_statistics.selectivity * 100.0);
    report.printf("  Effectiveness Ratio: %.2f%%\n", m_statistics.effectiveness_ratio * 100.0);
    
    report.printf("\nIndex Structure:\n");
    report.printf("  Bucket Count: %lu\n", m_statistics.bucket_count);
    report.printf("  Total Entries: %lu\n", m_statistics.total_entries);
    report.printf("  Load Factor: %.2f\n", m_statistics.load_factor);
    report.printf("  Hash Collisions: %lu (%.2f%% collision rate)\n", 
                  m_statistics.hash_collisions,
                  m_statistics.total_entries > 0 ? (double)m_statistics.hash_collisions / m_statistics.total_entries * 100.0 : 0.0);
    
    report.printf("\nMaintenance History:\n");
    report.printf("  Maintenance Operations: %lu\n", m_statistics.maintenance_operations);
    report.printf("  Defragmentations: %lu\n", m_statistics.defragmentation_count);
    report.printf("  Rebuilds: %lu\n", m_statistics.rebuild_count);
    report.printf("  Cache Optimizations: %lu\n", m_statistics.cache_optimizations);
    report.printf("  Total Maintenance Time: %.2f seconds\n", m_statistics.total_maintenance_time / 1000000.0);
    
    report.printf("\nQuery Pattern Analysis:\n");
    report.printf("  Equality Scans: %lu\n", m_statistics.equality_scans);
    report.printf("  Range Scans: %lu\n", m_statistics.range_scans);
    report.printf("  Full Scans: %lu\n", m_statistics.full_scans);
    report.printf("  Scan Efficiency: %.2f%%\n", m_statistics.scan_efficiency * 100.0);
    
    double current_score = calculateCurrentPerformanceScore();
    double trend = calculateTrendAnalysis();
    
    report.printf("\nPerformance Analysis:\n");
    report.printf("  Current Performance Score: %.2f\n", current_score);
    report.printf("  Performance Trend: %.2f\n", trend);
    report.printf("  Performance Degradation: %s\n", detectPerformanceDegradation() ? "Yes" : "No");
    
    if (recommendMaintenance())
    {
        report.printf("\nRecommended Maintenance: %s\n", 
                      getRecommendedMaintenanceType() == PARTIAL_HASH_MAINT_DEFRAGMENT ? "Defragmentation" :
                      getRecommendedMaintenanceType() == PARTIAL_HASH_MAINT_FULL_REBUILD ? "Full Rebuild" :
                      getRecommendedMaintenanceType() == PARTIAL_HASH_MAINT_OPTIMIZE_CACHE ? "Cache Optimization" :
                      getRecommendedMaintenanceType() == PARTIAL_HASH_MAINT_RECALC_STATS ? "Statistics Recalculation" :
                      "Integrity Check");
    }
}

bool PartialHashIndexPerformanceMonitor::startRealTimeMonitoring()
{
    m_real_time_monitoring = true;
    return true;
}

void PartialHashIndexPerformanceMonitor::stopRealTimeMonitoring()
{
    m_real_time_monitoring = false;
}

bool PartialHashIndexPerformanceMonitor::isRealTimeMonitoringEnabled() const
{
    return m_real_time_monitoring;
}

// Private methods

void PartialHashIndexPerformanceMonitor::updateDerivedMetrics()
{
    m_statistics.calculateDerivedMetrics();
}

void PartialHashIndexPerformanceMonitor::checkPerformanceThresholds()
{
    double current_score = calculatePerformanceScore(m_statistics);
    addPerformanceDataPoint(current_score);
    
    // Check for performance alerts
    PartialHashIndexAlertSystem::checkAlertConditions(m_index, m_statistics);
}

void PartialHashIndexPerformanceMonitor::triggerAdaptiveOptimization()
{
    // Placeholder for adaptive optimization logic
    // This would trigger automatic maintenance based on performance metrics
}

double PartialHashIndexPerformanceMonitor::calculatePerformanceScore(const PartialHashIndexStatistics& stats) const
{
    double score = 1.0; // Start with perfect score
    
    // Factor in lookup performance
    if (stats.average_lookup_time > 1000.0) // > 1ms is considered slow
        score *= 0.8;
    
    // Factor in condition evaluation performance
    if (stats.average_condition_eval_time > 500.0) // > 0.5ms is slow for conditions
        score *= 0.9;
    
    // Factor in inclusion efficiency
    if (stats.inclusion_ratio < 0.1) // Less than 10% inclusion is inefficient
        score *= 0.7;
    
    // Factor in cache efficiency
    double cache_hit_ratio = stats.condition_cache_hits + stats.condition_cache_misses > 0 ?
                            (double)stats.condition_cache_hits / (stats.condition_cache_hits + stats.condition_cache_misses) : 1.0;
    if (cache_hit_ratio < 0.5) // Less than 50% cache hit rate
        score *= 0.8;
    
    // Factor in load factor
    if (stats.load_factor > 0.8 || stats.load_factor < 0.3)
        score *= 0.85;
    
    return score;
}

void PartialHashIndexPerformanceMonitor::addPerformanceDataPoint(double score)
{
    m_performance_history.add(score);
    
    // Keep only last 100 data points
    if (m_performance_history.getCount() > 100)
        m_performance_history.shrink(1);
}

void PartialHashIndexPerformanceMonitor::persistStatisticsIfNeeded(thread_db* tdbb)
{
    if (m_statistics_dirty && tdbb)
    {
        // saveStatistics(tdbb);
        m_statistics_dirty = false;
    }
}

//----------------------------
// Alert System Static Variables
//----------------------------

ScratchBird::Array<PartialHashIndexAlertSystem::Alert> PartialHashIndexAlertSystem::s_pending_alerts;
ScratchBird::Mutex PartialHashIndexAlertSystem::s_alerts_mutex;
double PartialHashIndexAlertSystem::s_performance_threshold = 0.7;
double PartialHashIndexAlertSystem::s_inclusion_ratio_threshold = 0.1;
double PartialHashIndexAlertSystem::s_collision_rate_threshold = 0.3;
void (*PartialHashIndexAlertSystem::s_alert_callback)(const Alert& alert) = nullptr;

//----------------------------
// PartialHashIndexAlertSystem Implementation
//----------------------------

void PartialHashIndexAlertSystem::generateAlert(AlertLevel level, AlertType type, PartialHashIndex* index,
                                               const string& message, const string& recommendation)
{
    ScratchBird::MutexLockGuard guard(s_alerts_mutex, FB_FUNCTION);
    
    Alert alert;
    alert.level = level;
    alert.type = type;
    alert.index = index;
    alert.message = message;
    alert.timestamp = TimeStamp::getCurrentTimeStamp();
    alert.acknowledged = false;
    alert.recommendation = recommendation;
    
    s_pending_alerts.add(alert);
    
    // Call callback if registered
    if (s_alert_callback)
        s_alert_callback(alert);
}

void PartialHashIndexAlertSystem::checkAlertConditions(PartialHashIndex* index, const PartialHashIndexStatistics& stats)
{
    // Check performance degradation
    if (stats.selectivity < s_performance_threshold)
    {
        string message;
        message.printf("Index selectivity (%.2f%%) is below threshold (%.2f%%)", 
                      stats.selectivity * 100.0, s_performance_threshold * 100.0);
        generateAlert(ALERT_WARNING, ALERT_PERFORMANCE_DEGRADATION, index, message,
                     "Consider rebuilding the index or adjusting the partial condition");
    }
    
    // Check inclusion ratio
    if (stats.inclusion_ratio < s_inclusion_ratio_threshold)
    {
        string message;
        message.printf("Inclusion ratio (%.2f%%) is below threshold (%.2f%%)", 
                      stats.inclusion_ratio * 100.0, s_inclusion_ratio_threshold * 100.0);
        generateAlert(ALERT_WARNING, ALERT_LOW_INCLUSION_RATIO, index, message,
                     "Consider reviewing the partial index condition for better efficiency");
    }
    
    // Check collision rate
    double collision_rate = stats.total_entries > 0 ? 
                           (double)stats.hash_collisions / stats.total_entries : 0.0;
    if (collision_rate > s_collision_rate_threshold)
    {
        string message;
        message.printf("Hash collision rate (%.2f%%) is above threshold (%.2f%%)", 
                      collision_rate * 100.0, s_collision_rate_threshold * 100.0);
        generateAlert(ALERT_WARNING, ALERT_HIGH_COLLISION_RATE, index, message,
                     "Consider increasing bucket count or rebuilding the index");
    }
    
    // Check condition evaluation time
    if (stats.average_condition_eval_time > 1000.0) // > 1ms
    {
        string message;
        message.printf("Average condition evaluation time (%.2f ms) is high", 
                      stats.average_condition_eval_time / 1000.0);
        generateAlert(ALERT_INFO, ALERT_HIGH_CONDITION_EVALUATION_TIME, index, message,
                     "Consider simplifying the partial index condition or enabling caching");
    }
}

ScratchBird::Array<PartialHashIndexAlertSystem::Alert> PartialHashIndexAlertSystem::getPendingAlerts()
{
    ScratchBird::MutexLockGuard guard(s_alerts_mutex, FB_FUNCTION);
    return s_pending_alerts;
}

void PartialHashIndexAlertSystem::acknowledgeAlert(ULONG alert_id)
{
    ScratchBird::MutexLockGuard guard(s_alerts_mutex, FB_FUNCTION);
    
    if (alert_id < s_pending_alerts.getCount())
        s_pending_alerts[alert_id].acknowledged = true;
}

void PartialHashIndexAlertSystem::clearAcknowledgedAlerts()
{
    ScratchBird::MutexLockGuard guard(s_alerts_mutex, FB_FUNCTION);
    
    for (ULONG i = s_pending_alerts.getCount(); i > 0; i--)
    {
        if (s_pending_alerts[i-1].acknowledged)
            s_pending_alerts.shrink(i-1);
    }
}

void PartialHashIndexAlertSystem::setAlertThresholds(double performance_threshold, double inclusion_ratio_threshold,
                                                    double collision_rate_threshold)
{
    s_performance_threshold = performance_threshold;
    s_inclusion_ratio_threshold = inclusion_ratio_threshold;
    s_collision_rate_threshold = collision_rate_threshold;
}

void PartialHashIndexAlertSystem::enableAlertType(AlertType type, bool enable)
{
    // Implementation would maintain a map of enabled alert types
    // For now, all alerts are enabled
}

void PartialHashIndexAlertSystem::setAlertCallback(void (*callback)(const Alert& alert))
{
    s_alert_callback = callback;
}