/*
 * The contents of this file are subject to the Initial
 * Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
 * http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 * Software distributed under the License is distributed AS IS,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the License for the specific language governing rights
 * and limitations under the License.
 *
 * The Original Code was created for the ScratchBird Open Source 
 * RDBMS project.
 *
 * Copyright (c) 2025 ScratchBird Project
 * and all contributors signed below.
 *
 * All Rights Reserved.
 * Contributor(s): ______________________________________.
 *
 * 2025.07.24 - ScratchBird Partial Hash Index Implementation
 */

#ifndef JRD_PARTIAL_HASH_INDEX_H
#define JRD_PARTIAL_HASH_INDEX_H

#include "HashIndex.h"
#include "IndexType.h"
#include "constants.h"
#include "ods.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"
#include "../common/classes/fb_pair.h"

namespace Jrd {

// Forward declaration for statistics
class PartialHashIndexPerformanceMonitor;

// Forward declarations
class Database;
class jrd_rel;
class index_desc;
class thread_db;
class jrd_tra;
class IndexRetrieval;
class IndexCondition;
struct dsc;
class Record;

//----------------------------
// Partial Hash Index Constants
//----------------------------
const USHORT PARTIAL_HASH_DEFAULT_BUCKETS = 32;        // Default buckets for partial indexes
const USHORT PARTIAL_HASH_MIN_BUCKETS = 8;             // Minimum buckets for partial indexes
const USHORT PARTIAL_HASH_MAX_CONDITION_LENGTH = 1024; // Max WHERE clause length
const float PARTIAL_HASH_LOAD_FACTOR = 0.6f;           // Lower load factor for better performance

// Maintenance operation types
const USHORT PARTIAL_HASH_MAINT_INTEGRITY_CHECK = 1;   // Basic integrity check
const USHORT PARTIAL_HASH_MAINT_DEFRAGMENT = 2;        // Index defragmentation
const USHORT PARTIAL_HASH_MAINT_RECALC_STATS = 3;      // Statistics recalculation
const USHORT PARTIAL_HASH_MAINT_OPTIMIZE_CACHE = 4;    // Cache optimization
const USHORT PARTIAL_HASH_MAINT_FULL_REBUILD = 5;      // Full index rebuild
const USHORT PARTIAL_HASH_MAINT_CLEANUP_ORPHANS = 6;   // Orphan cleanup

//----------------------------
// Partial Hash Statistics
//----------------------------
struct PartialHashStatistics
{
    ULONG total_records_evaluated;      // Total records checked against condition
    ULONG records_included;             // Records that passed condition
    ULONG records_excluded;             // Records that failed condition
    ULONG condition_evaluation_errors;  // Errors during condition evaluation
    double inclusion_ratio;             // Ratio of included/total records
    double average_evaluation_time;     // Average time to evaluate condition (microseconds)
    ULONG cache_hits;                   // Condition evaluation cache hits
    ULONG cache_misses;                 // Condition evaluation cache misses
};

//----------------------------
// Partial Hash Key Entry - Extended with condition validation
//----------------------------
struct PartialHashKeyEntry : public HashKeyEntry
{
    UCHAR condition_validated;          // Flag indicating condition was validated
    RecordNumber validation_transaction; // Transaction that performed validation
    // Inherited: key_length, record_number, key_data[1]
};

//----------------------------
// PartialHashIndex - Hash index with WHERE clause filtering
//----------------------------
class PartialHashIndex : public HashIndex
{
public:
    // Constructor and destructor
    PartialHashIndex(thread_db* tdbb, Database* database, jrd_rel* relation, 
                     const index_desc* desc);
    virtual ~PartialHashIndex();

    // IndexType interface implementation (overridden for partial logic)
    virtual index_error_t insert(thread_db* tdbb, const dsc* key, 
                                  RecordNumber record, jrd_tra* transaction) override;
    virtual bool lookup(thread_db* tdbb, const dsc* key, IndexRetrieval* retrieval) override;
    virtual index_error_t remove(thread_db* tdbb, const dsc* key, 
                                 RecordNumber record, jrd_tra* transaction) override;
    virtual const char* getTypeName() const override { return IDX_TYPE_NAME_PARTIAL_HASH; }

    // Partial index specific operations
    bool initializePartialCondition(thread_db* tdbb);
    bool evaluateCondition(thread_db* tdbb, Record* record) const;
    bool shouldIncludeRecord(thread_db* tdbb, Record* record, jrd_tra* transaction) const;
    
    // Condition management
    bool updateCondition(thread_db* tdbb, BoolExprNode* new_condition);
    bool validateConditionIntegrity(thread_db* tdbb);
    const BoolExprNode* getCondition() const;
    
    // Partial index maintenance
    bool rebuildPartialIndex(thread_db* tdbb);
    bool validatePartialIndex(thread_db* tdbb);
    ULONG getIncludedRecordCount() const;
    double getInclusionRatio() const;
    
    // Advanced maintenance operations
    bool performMaintenance(thread_db* tdbb, USHORT maintenance_type);
    bool defragmentIndex(thread_db* tdbb);
    bool recalculateStatistics(thread_db* tdbb);
    bool optimizeCache(thread_db* tdbb);
    bool cleanupOrphanedEntries(thread_db* tdbb);
    
    // Transaction and change notification support
    bool onTransactionCommit(thread_db* tdbb, jrd_tra* transaction);
    bool onTransactionRollback(thread_db* tdbb, jrd_tra* transaction);
    bool onTableStructureChange(thread_db* tdbb, USHORT change_type);
    
    // Performance monitoring and adaptive optimization
    bool shouldTriggerMaintenance() const;
    USHORT recommendMaintenanceType() const;
    
    // Performance optimization
    bool optimizeForConditionPattern(thread_db* tdbb);
    bool enableConditionCaching(bool enable);
    void clearConditionCache(thread_db* tdbb);
    
    // Statistics and monitoring
    const PartialHashStatistics& getPartialStatistics() const;
    void resetPartialStatistics();
    bool generatePartialIndexReport(thread_db* tdbb, ScratchBird::string& report) const;
    
    // Performance monitoring integration
    PartialHashIndexPerformanceMonitor* getPerformanceMonitor() const;
    bool enablePerformanceMonitoring(bool enable = true);
    bool isPerformanceMonitoringEnabled() const;

protected:
    // Internal condition evaluation
    bool internalEvaluateCondition(thread_db* tdbb, Record* record, 
                                   bool* evaluation_error = nullptr) const;
    bool validateConditionSyntax(thread_db* tdbb, const BoolExprNode* condition) const;
    
    // Condition caching system
    struct ConditionCacheEntry
    {
        RecordNumber record_number;
        bool condition_result;
        SLONG evaluation_time;
        jrd_tra* transaction;
    };
    
    typedef GenericMap<Pair<RecordNumber, ConditionCacheEntry>> ConditionCache;
    mutable ConditionCache m_condition_cache;
    mutable ULONG m_cache_size_limit;
    mutable bool m_caching_enabled;
    
    // Condition evaluation helpers
    bool getCachedConditionResult(RecordNumber record, bool& result) const;
    void setCachedConditionResult(RecordNumber record, bool result) const;
    void evictOldCacheEntries() const;
    
    // Partial hash bucket management (override parent methods)
    virtual bool insertIntoBucket(thread_db* tdbb, HashBucket* bucket, 
                                  const dsc* key, RecordNumber record) override;
    virtual bool removeFromBucket(thread_db* tdbb, HashBucket* bucket, 
                                  const dsc* key, RecordNumber record) override;
    virtual bool findInBucket(const HashBucket* bucket, const dsc* key, 
                              RecordNumber* record) const override;
    
    // Record validation before index operations
    bool validateRecordForInclusion(thread_db* tdbb, RecordNumber record, 
                                    jrd_tra* transaction) const;
    Record* fetchRecordForEvaluation(thread_db* tdbb, RecordNumber record) const;
    
    // Partial index integrity checks
    bool verifyIndexConsistency(thread_db* tdbb) const;
    bool detectOrphanedEntries(thread_db* tdbb) const;
    bool detectMissingEntries(thread_db* tdbb) const;

private:
    // Partial index specific members
    IndexCondition* m_condition_evaluator;  // Condition evaluator instance
    ScratchBird::string m_condition_text;   // Original WHERE clause text
    bool m_condition_valid;                 // Whether condition is valid
    ULONG m_condition_complexity;           // Estimated condition complexity score
    
    // Performance tracking
    mutable PartialHashStatistics m_partial_stats;
    mutable SLONG m_last_stats_reset;       // Timestamp of last statistics reset
    
    // Configuration
    bool m_strict_condition_mode;           // Fail on condition evaluation errors
    bool m_lazy_evaluation_mode;            // Defer condition evaluation when possible
    ULONG m_max_evaluation_time;            // Maximum time to spend on condition evaluation
    
    // Optimization settings
    bool m_condition_indexing_enabled;      // Whether to create sub-indexes on condition fields
    bool m_adaptive_bucket_sizing;          // Adjust bucket count based on inclusion ratio
    double m_target_inclusion_ratio;        // Target ratio for optimization
    
    // Transaction management
    mutable ScratchBird::GenericMap<ScratchBird::Pair<ScratchBird::NonPooled<jrd_tra*, ULONG>>> m_transaction_counters; // Track per-transaction operations
    
    // Performance monitoring
    mutable PartialHashIndexPerformanceMonitor* m_performance_monitor;
    bool m_monitoring_enabled;
    
    // Utility methods
    void initializePartialStatistics();
    void updatePartialStatistics(bool included, SLONG evaluation_time) const;
    ULONG calculateConditionComplexity(const BoolExprNode* condition) const;
    bool isConditionCacheable(const BoolExprNode* condition) const;
    void logPartialIndexOperation(thread_db* tdbb, const char* operation, 
                                  ULONG identifier, bool success) const;
};

//----------------------------
// PartialHashIndexFactory - Factory for creating PartialHashIndex instances
//----------------------------
class PartialHashIndexFactory : public IndexTypeFactory
{
public:
    virtual IndexType* createIndex(thread_db* tdbb, Database* database,
                                   jrd_rel* relation, const index_desc* desc) override
    {
        return new PartialHashIndex(tdbb, database, relation, desc);
    }

    virtual const char* getTypeName() const override
    {
        return IDX_TYPE_NAME_PARTIAL_HASH;
    }

    virtual int getTypeId() const override
    {
        return IDX_TYPE_PARTIAL_HASH;
    }
    
    virtual bool supportsPartialIndexes() const override
    {
        return true;
    }
    
    virtual bool requiresCondition() const override
    {
        return true;
    }
};

//----------------------------
// PartialHashIndexValidator - Validation utilities for partial hash indexes
//----------------------------
class PartialHashIndexValidator
{
public:
    // Validation methods
    static bool validateConditionExpression(thread_db* tdbb, const BoolExprNode* condition,
                                            jrd_rel* relation, ScratchBird::string& error_message);
    static bool validateIndexConsistency(thread_db* tdbb, PartialHashIndex* index,
                                         ScratchBird::string& report);
    static bool validatePerformanceCharacteristics(thread_db* tdbb, PartialHashIndex* index,
                                                   double min_inclusion_ratio = 0.1);
    
    // Analysis methods
    static double estimateInclusionRatio(thread_db* tdbb, const BoolExprNode* condition,
                                         jrd_rel* relation, ULONG sample_size = 1000);
    static ULONG estimateConditionComplexity(const BoolExprNode* condition);
    static bool recommendsIndexCreation(thread_db* tdbb, const BoolExprNode* condition,
                                        jrd_rel* relation, double& confidence_score);
    
    // Optimization recommendations
    static void generateOptimizationRecommendations(thread_db* tdbb, PartialHashIndex* index,
                                                    ScratchBird::string& recommendations);
    static bool recommendsBucketCountAdjustment(PartialHashIndex* index, ULONG& recommended_count);
    static bool recommendsConditionSimplification(const BoolExprNode* condition,
                                                  ScratchBird::string& simplified_suggestion);

private:
    // Internal validation helpers
    static bool checkConditionReferences(const BoolExprNode* condition, jrd_rel* relation);
    static bool checkConditionDeterminism(const BoolExprNode* condition);
    static bool checkConditionPerformance(thread_db* tdbb, const BoolExprNode* condition,
                                          jrd_rel* relation, double& avg_evaluation_time);
};

} // namespace Jrd

#endif // JRD_PARTIAL_HASH_INDEX_H