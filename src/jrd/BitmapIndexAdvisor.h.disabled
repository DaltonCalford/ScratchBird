/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapIndexAdvisor.h
 *	DESCRIPTION:	Bitmap index advisor with recommendation engine
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
 * 2025.07.23 - ScratchBird Bitmap Index Advisor Implementation
 */

#ifndef JRD_BITMAP_INDEX_ADVISOR_H
#define JRD_BITMAP_INDEX_ADVISOR_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "BitmapIndex.h"
#include <vector>
#include <memory>
#include <map>
#include <set>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class Database;
struct index_desc;
class jrd_rel;
class BoolExprNode;
class ValueExprNode;
class CompilerScratch;

//----------------------------
// Recommendation Types
//----------------------------

enum RecommendationType : UCHAR
{
    RECOMMENDATION_CREATE_INDEX = 0,        // Create new bitmap index
    RECOMMENDATION_DROP_INDEX = 1,          // Drop unused/inefficient index
    RECOMMENDATION_MODIFY_INDEX = 2,        // Modify existing index
    RECOMMENDATION_MERGE_INDEXES = 3,       // Merge multiple indexes
    RECOMMENDATION_SPLIT_INDEX = 4,         // Split large index
    RECOMMENDATION_CHANGE_TYPE = 5,         // Change to different index type
    RECOMMENDATION_OPTIMIZE = 6,            // Optimize existing index
    RECOMMENDATION_PARTITION = 7,           // Partition large index
    RECOMMENDATION_COMPRESS = 8,            // Apply compression
    RECOMMENDATION_MAINTENANCE = 9          // Perform maintenance
};

//----------------------------
// Recommendation Confidence Levels
//----------------------------

enum RecommendationConfidence : UCHAR
{
    CONFIDENCE_VERY_LOW = 0,               // <20% confidence
    CONFIDENCE_LOW = 1,                    // 20-40% confidence
    CONFIDENCE_MEDIUM = 2,                 // 40-60% confidence
    CONFIDENCE_HIGH = 3,                   // 60-80% confidence
    CONFIDENCE_VERY_HIGH = 4               // >80% confidence
};

//----------------------------
// Analysis Scope
//----------------------------

enum AnalysisScope : UCHAR
{
    SCOPE_TABLE = 0,                       // Single table analysis
    SCOPE_DATABASE = 1,                    // Database-wide analysis
    SCOPE_WORKLOAD = 2,                    // Workload-based analysis
    SCOPE_QUERY_SPECIFIC = 3,              // Specific query optimization
    SCOPE_PERFORMANCE_ISSUE = 4            // Performance problem analysis
};

//----------------------------
// Workload Pattern Analysis
//----------------------------

struct WorkloadPattern
{
    // Query patterns
    ULONG total_queries;                   // Total queries analyzed
    ULONG select_queries;                  // Number of SELECT queries
    ULONG insert_queries;                  // Number of INSERT queries
    ULONG update_queries;                  // Number of UPDATE queries
    ULONG delete_queries;                  // Number of DELETE queries
    
    // Query characteristics
    double average_selectivity;            // Average query selectivity
    ULONG complex_where_clauses;           // Queries with complex WHERE clauses
    ULONG range_queries;                   // Range-based queries
    ULONG equality_queries;                // Equality-based queries
    ULONG in_list_queries;                 // IN list queries
    ULONG null_checks;                     // NULL check queries
    
    // Join patterns
    ULONG join_queries;                    // Queries with joins
    ULONG multi_table_joins;               // Multi-table join queries
    ULONG star_schema_queries;             // Star schema pattern queries
    ULONG snowflake_queries;               // Snowflake pattern queries
    
    // Update patterns
    double update_frequency;               // Updates per hour
    std::map<USHORT, ULONG> field_update_frequency; // Per-field update frequency
    std::vector<std::pair<ULONG, ULONG>> hot_spots; // Frequently updated record ranges
    
    // Performance characteristics
    double average_query_time_ms;          // Average query execution time
    ULONG slow_queries;                    // Queries exceeding threshold
    ULONG queries_using_table_scan;        // Queries requiring table scan
    ULONG queries_using_indexes;           // Queries using existing indexes
    
    WorkloadPattern()
        : total_queries(0), select_queries(0), insert_queries(0),
          update_queries(0), delete_queries(0), average_selectivity(1.0),
          complex_where_clauses(0), range_queries(0), equality_queries(0),
          in_list_queries(0), null_checks(0), join_queries(0),
          multi_table_joins(0), star_schema_queries(0), snowflake_queries(0),
          update_frequency(0.0), average_query_time_ms(0.0), slow_queries(0),
          queries_using_table_scan(0), queries_using_indexes(0)
    {
    }
};

//----------------------------
// Index Recommendation
//----------------------------

struct IndexRecommendation
{
    ULONG recommendation_id;               // Unique recommendation ID
    RecommendationType type;               // Type of recommendation
    RecommendationConfidence confidence;   // Confidence level
    
    // Target information
    jrd_rel* target_relation;              // Target table
    std::vector<USHORT> field_ids;         // Fields involved
    const index_desc* existing_index;     // Existing index (if applicable)
    
    // Recommendation details
    ScratchBird::string recommendation_text; // Human-readable recommendation
    ScratchBird::string rationale;         // Why this recommendation is made
    ScratchBird::string implementation_sql; // SQL to implement recommendation
    
    // Impact analysis
    struct ImpactAnalysis {
        // Performance impact
        double expected_query_improvement;  // Expected query performance improvement
        double expected_insert_impact;     // Expected impact on inserts
        double expected_update_impact;     // Expected impact on updates
        double expected_delete_impact;     // Expected impact on deletes
        
        // Resource impact
        ULONG estimated_storage_mb;        // Estimated storage requirement
        ULONG estimated_memory_mb;         // Estimated memory requirement
        ULONG estimated_cpu_overhead;      // Estimated CPU overhead percentage
        
        // Maintenance impact
        ULONG estimated_maintenance_time;  // Estimated maintenance time per week
        double fragmentation_tendency;     // Tendency to fragment (0-1)
        
        ImpactAnalysis()
            : expected_query_improvement(0.0), expected_insert_impact(0.0),
              expected_update_impact(0.0), expected_delete_impact(0.0),
              estimated_storage_mb(0), estimated_memory_mb(0),
              estimated_cpu_overhead(0), estimated_maintenance_time(0),
              fragmentation_tendency(0.0)
        {
        }
    } impact;
    
    // Affected queries
    std::vector<ScratchBird::string> affected_queries; // Queries that would benefit
    std::vector<double> query_improvements;   // Per-query improvement estimates
    
    // Implementation details
    ULONG estimated_build_time_minutes;   // Time to build index
    bool requires_exclusive_access;       // True if needs exclusive table access
    bool can_build_online;                // True if can build with concurrent access
    ULONG implementation_complexity;      // Implementation complexity (1-5)
    
    // Risk assessment
    struct RiskAssessment {
        double implementation_risk;        // Risk of implementation failure
        double performance_risk;           // Risk of performance degradation
        double maintenance_risk;           // Risk of high maintenance overhead
        ScratchBird::string risk_factors;  // Description of risk factors
        
        RiskAssessment()
            : implementation_risk(0.0), performance_risk(0.0), maintenance_risk(0.0)
        {
        }
    } risk;
    
    // Priority and scheduling
    ULONG priority_score;                 // Priority score (higher = more important)
    GDS_TIMESTAMP recommended_implementation_time; // When to implement
    std::vector<ULONG> prerequisite_recommendations; // Must be done first
    
    IndexRecommendation()
        : recommendation_id(0), type(RECOMMENDATION_CREATE_INDEX),
          confidence(CONFIDENCE_MEDIUM), target_relation(nullptr),
          existing_index(nullptr), estimated_build_time_minutes(0),
          requires_exclusive_access(false), can_build_online(true),
          implementation_complexity(1), priority_score(0),
          recommended_implementation_time(0)
    {
    }
};

//----------------------------
// Query Analysis Results
//----------------------------

struct QueryAnalysis
{
    ScratchBird::string query_text;       // Original query text
    jrd_rel* primary_table;               // Primary table being queried
    std::vector<jrd_rel*> joined_tables;  // Tables involved in joins
    
    // Predicate analysis
    struct PredicateInfo {
        USHORT field_id;                  // Field involved in predicate
        enum PredicateType {
            EQUALITY = 0,
            RANGE = 1,
            IN_LIST = 2,
            NULL_CHECK = 3,
            LIKE = 4,
            COMPLEX = 5
        } type;
        
        double selectivity;               // Estimated selectivity
        bool uses_index;                  // True if currently uses index
        const index_desc* used_index;     // Index currently used (if any)
        
        PredicateInfo() : field_id(0), type(EQUALITY), selectivity(1.0),
                         uses_index(false), used_index(nullptr) {}
    };
    
    std::vector<PredicateInfo> predicates; // Predicates in WHERE clause
    
    // Performance metrics
    double execution_time_ms;             // Current execution time
    double cpu_time_ms;                   // CPU time consumed
    ULONG disk_io_operations;             // Disk I/O operations
    ULONG records_examined;               // Records examined
    ULONG records_returned;               // Records returned
    double selectivity;                   // Overall query selectivity
    
    // Optimization opportunities
    std::vector<USHORT> missing_indexes;  // Fields that could benefit from indexes
    std::vector<ScratchBird::string> optimization_notes; // Optimization suggestions
    
    QueryAnalysis()
        : primary_table(nullptr), execution_time_ms(0.0), cpu_time_ms(0.0),
          disk_io_operations(0), records_examined(0), records_returned(0),
          selectivity(1.0)
    {
    }
};

//----------------------------
// Table Analysis Results
//----------------------------

struct TableAnalysis
{
    jrd_rel* relation;                    // Table being analyzed
    
    // Table characteristics
    ULONG total_records;                  // Total number of records
    ULONG table_size_mb;                  // Table size in MB
    double growth_rate;                   // Records added per day
    
    // Field characteristics
    struct FieldInfo {
        USHORT field_id;                  // Field identifier
        ScratchBird::string field_name;   // Field name
        UCHAR data_type;                  // Field data type
        ULONG distinct_values;            // Number of distinct values
        double selectivity;               // Average selectivity
        bool has_nulls;                   // True if field contains NULLs
        double null_percentage;           // Percentage of NULL values
        bool frequently_queried;          // True if often used in WHERE clauses
        bool frequently_updated;          // True if often updated
        
        FieldInfo() : field_id(0), data_type(0), distinct_values(0),
                     selectivity(1.0), has_nulls(false), null_percentage(0.0),
                     frequently_queried(false), frequently_updated(false) {}
    };
    
    std::vector<FieldInfo> fields;        // Field analysis results
    
    // Current indexes
    std::vector<const index_desc*> existing_indexes; // Current indexes on table
    std::vector<double> index_usage_rates; // Usage rate for each index
    std::vector<double> index_effectiveness; // Effectiveness of each index
    
    // Query patterns
    WorkloadPattern workload;             // Workload analysis for this table
    
    TableAnalysis() : relation(nullptr), total_records(0), table_size_mb(0), growth_rate(0.0) {}
};

//----------------------------
// Bitmap Index Advisor Engine
//----------------------------

/**
 * Main advisor engine for bitmap index recommendations
 */
class BitmapIndexAdvisor
{
public:
    explicit BitmapIndexAdvisor(MemoryPool* pool);
    ~BitmapIndexAdvisor();

    // Main analysis interface
    std::vector<IndexRecommendation> analyzeAndRecommend(thread_db* tdbb, Database* database,
                                                        AnalysisScope scope = SCOPE_DATABASE);
    
    // Specific analysis methods
    std::vector<IndexRecommendation> analyzeTable(thread_db* tdbb, jrd_rel* relation);
    std::vector<IndexRecommendation> analyzeWorkload(thread_db* tdbb, Database* database,
                                                    const std::vector<ScratchBird::string>& queries);
    std::vector<IndexRecommendation> analyzeQuery(thread_db* tdbb, const ScratchBird::string& query);
    std::vector<IndexRecommendation> analyzePerformanceIssue(thread_db* tdbb, jrd_rel* relation,
                                                             const ScratchBird::string& problem_description);
    
    // Workload analysis
    WorkloadPattern analyzeWorkloadPattern(thread_db* tdbb, Database* database,
                                          GDS_TIMESTAMP start_time, GDS_TIMESTAMP end_time);
    
    QueryAnalysis analyzeQueryPattern(thread_db* tdbb, CompilerScratch* csb,
                                     const ScratchBird::string& query);
    
    TableAnalysis analyzeTableCharacteristics(thread_db* tdbb, jrd_rel* relation);
    
    // Index suitability analysis
    bool isBitmapIndexSuitable(thread_db* tdbb, jrd_rel* relation, USHORT field_id) const;
    
    double calculateBitmapIndexBenefit(thread_db* tdbb, jrd_rel* relation,
                                      const std::vector<USHORT>& field_ids,
                                      const WorkloadPattern& workload) const;
    
    // Recommendation refinement
    std::vector<IndexRecommendation> rankRecommendations(const std::vector<IndexRecommendation>& recommendations) const;
    
    std::vector<IndexRecommendation> filterRecommendations(const std::vector<IndexRecommendation>& recommendations,
                                                          RecommendationConfidence min_confidence = CONFIDENCE_MEDIUM) const;
    
    void optimizeRecommendationSet(std::vector<IndexRecommendation>& recommendations) const;
    
    // Cost-benefit analysis
    struct CostBenefitAnalysis {
        // Costs
        double implementation_cost;        // Cost to implement (time, resources)
        double maintenance_cost;           // Ongoing maintenance cost
        double storage_cost;               // Storage cost
        double update_overhead_cost;       // Cost of slower updates
        
        // Benefits
        double query_performance_benefit;  // Query performance improvement value
        double system_efficiency_benefit;  // Overall system efficiency improvement
        double user_experience_benefit;    // User experience improvement value
        
        // Net analysis
        double net_benefit;                // Total benefit minus total cost
        double roi_percentage;             // Return on investment percentage
        double payback_period_days;        // Time to recover implementation cost
        
        CostBenefitAnalysis()
            : implementation_cost(0.0), maintenance_cost(0.0), storage_cost(0.0),
              update_overhead_cost(0.0), query_performance_benefit(0.0),
              system_efficiency_benefit(0.0), user_experience_benefit(0.0),
              net_benefit(0.0), roi_percentage(0.0), payback_period_days(0.0)
        {
        }
    };
    
    CostBenefitAnalysis calculateCostBenefit(thread_db* tdbb, const IndexRecommendation& recommendation) const;
    
    // Configuration and tuning
    struct AdvisorConfiguration {
        // Analysis parameters
        double min_selectivity_threshold;  // Minimum selectivity for index consideration
        ULONG min_table_size_records;      // Minimum table size for analysis
        double min_confidence_threshold;   // Minimum confidence for recommendations
        
        // Resource constraints
        ULONG max_storage_overhead_mb;     // Maximum acceptable storage overhead
        double max_update_impact_percentage; // Maximum acceptable update impact
        ULONG max_concurrent_builds;       // Maximum concurrent index builds
        
        // Recommendation preferences
        bool prefer_single_column_indexes; // Prefer single-column over multi-column
        bool prefer_online_builds;         // Prefer online index builds
        bool conservative_recommendations; // Make conservative recommendations
        
        AdvisorConfiguration()
            : min_selectivity_threshold(0.1), min_table_size_records(1000),
              min_confidence_threshold(0.6), max_storage_overhead_mb(1024),
              max_update_impact_percentage(10.0), max_concurrent_builds(2),
              prefer_single_column_indexes(false), prefer_online_builds(true),
              conservative_recommendations(false)
        {
        }
    };
    
    void setConfiguration(const AdvisorConfiguration& config);
    AdvisorConfiguration getConfiguration() const;
    
    // Learning and adaptation
    void recordRecommendationOutcome(ULONG recommendation_id, bool was_implemented,
                                   double actual_benefit, const ScratchBird::string& feedback);
    
    void updatePredictionModel(const std::vector<IndexRecommendation>& recommendations);
    
    // Reporting and visualization
    ScratchBird::string generateRecommendationReport(const std::vector<IndexRecommendation>& recommendations) const;
    
    ScratchBird::string generateExecutiveSummary(const std::vector<IndexRecommendation>& recommendations) const;
    
    ScratchBird::string generateImplementationPlan(const std::vector<IndexRecommendation>& recommendations) const;

private:
    MemoryPool* m_pool;
    AdvisorConfiguration m_config;
    
    // Analysis engines
    std::unique_ptr<class WorkloadAnalyzer> m_workload_analyzer;
    std::unique_ptr<class QueryAnalyzer> m_query_analyzer;
    std::unique_ptr<class TableAnalyzer> m_table_analyzer;
    std::unique_ptr<class IndexSuitabilityAnalyzer> m_suitability_analyzer;
    std::unique_ptr<class CostBenefitCalculator> m_cost_benefit_calculator;
    
    // Machine learning model
    std::unique_ptr<class RecommendationPredictor> m_predictor;
    
    // Recommendation generation
    IndexRecommendation createIndexRecommendation(thread_db* tdbb, jrd_rel* relation,
                                                 const std::vector<USHORT>& field_ids,
                                                 const WorkloadPattern& workload) const;
    
    IndexRecommendation createDropRecommendation(thread_db* tdbb, const index_desc* idx,
                                                const WorkloadPattern& workload) const;
    
    IndexRecommendation createModifyRecommendation(thread_db* tdbb, const index_desc* idx,
                                                  const WorkloadPattern& workload) const;
    
    IndexRecommendation createOptimizationRecommendation(thread_db* tdbb, const index_desc* idx) const;
    
    // Analysis helpers
    std::vector<USHORT> identifyIndexCandidateFields(thread_db* tdbb, jrd_rel* relation,
                                                     const WorkloadPattern& workload) const;
    
    std::vector<const index_desc*> identifyUnderutilizedIndexes(thread_db* tdbb, jrd_rel* relation,
                                                               const WorkloadPattern& workload) const;
    
    double calculateFieldSelectivity(thread_db* tdbb, jrd_rel* relation, USHORT field_id) const;
    
    ULONG estimateDistinctValues(thread_db* tdbb, jrd_rel* relation, USHORT field_id) const;
    
    // Impact estimation
    double estimateQueryPerformanceImpact(thread_db* tdbb, const IndexRecommendation& recommendation) const;
    
    double estimateUpdatePerformanceImpact(thread_db* tdbb, const IndexRecommendation& recommendation) const;
    
    ULONG estimateStorageRequirement(thread_db* tdbb, const IndexRecommendation& recommendation) const;
    
    // Confidence calculation
    RecommendationConfidence calculateConfidence(thread_db* tdbb, const IndexRecommendation& recommendation) const;
    
    double calculateStatisticalConfidence(const std::vector<double>& sample_data) const;
    
    // Risk assessment
    IndexRecommendation::RiskAssessment assessRisks(thread_db* tdbb, const IndexRecommendation& recommendation) const;
    
    // Priority calculation
    ULONG calculatePriority(const IndexRecommendation& recommendation, const CostBenefitAnalysis& analysis) const;
    
    // Recommendation optimization
    void eliminateRedundantRecommendations(std::vector<IndexRecommendation>& recommendations) const;
    
    void resolveConflictingRecommendations(std::vector<IndexRecommendation>& recommendations) const;
    
    void optimizeImplementationOrder(std::vector<IndexRecommendation>& recommendations) const;
};

//----------------------------
// Specialized Analyzers
//----------------------------

/**
 * Workload analyzer for understanding query patterns
 */
class WorkloadAnalyzer
{
public:
    explicit WorkloadAnalyzer(MemoryPool* pool);
    ~WorkloadAnalyzer();
    
    WorkloadPattern analyzeWorkload(thread_db* tdbb, Database* database,
                                   GDS_TIMESTAMP start_time, GDS_TIMESTAMP end_time);
    
    void recordQuery(const ScratchBird::string& query, double execution_time_ms,
                    ULONG records_examined, ULONG records_returned);
    
    std::vector<ScratchBird::string> identifySlowQueries(double time_threshold_ms = 1000.0) const;
    
    std::map<jrd_rel*, WorkloadPattern> getPerTableWorkloads() const;

private:
    MemoryPool* m_pool;
    
    struct QueryRecord {
        ScratchBird::string query_text;
        GDS_TIMESTAMP timestamp;
        double execution_time_ms;
        ULONG records_examined;
        ULONG records_returned;
        
        QueryRecord() : timestamp(0), execution_time_ms(0.0),
                       records_examined(0), records_returned(0) {}
    };
    
    std::vector<QueryRecord> m_query_history;
    mutable ScratchBird::Mutex m_history_mutex;
    
    void analyzeQueryPatterns(const std::vector<QueryRecord>& queries, WorkloadPattern& pattern);
    void extractTableReferences(const ScratchBird::string& query, std::vector<jrd_rel*>& tables);
    void extractFieldReferences(const ScratchBird::string& query, std::vector<USHORT>& fields);
};

//----------------------------
// Index Recommendation Manager
//----------------------------

/**
 * Manager for tracking and implementing index recommendations
 */
class IndexRecommendationManager
{
public:
    static IndexRecommendationManager* getInstance();
    
    // Recommendation lifecycle
    ULONG addRecommendation(const IndexRecommendation& recommendation);
    bool updateRecommendation(ULONG recommendation_id, const IndexRecommendation& updated_recommendation);
    bool removeRecommendation(ULONG recommendation_id);
    
    // Recommendation retrieval
    IndexRecommendation* getRecommendation(ULONG recommendation_id);
    std::vector<IndexRecommendation> getRecommendationsForTable(jrd_rel* relation) const;
    std::vector<IndexRecommendation> getRecommendationsForDatabase(Database* database) const;
    std::vector<IndexRecommendation> getPendingRecommendations() const;
    
    // Implementation tracking
    bool markRecommendationImplemented(ULONG recommendation_id, bool success,
                                      const ScratchBird::string& notes = "");
    
    bool scheduleRecommendationImplementation(ULONG recommendation_id, GDS_TIMESTAMP scheduled_time);
    
    // Statistics and reporting
    struct RecommendationStatistics {
        ULONG total_recommendations;        // Total recommendations made
        ULONG implemented_recommendations;  // Successfully implemented
        ULONG rejected_recommendations;     // Rejected by user/system
        ULONG pending_recommendations;      // Still pending
        double average_implementation_time; // Average time to implement
        double success_rate;               // Implementation success rate
        
        RecommendationStatistics()
            : total_recommendations(0), implemented_recommendations(0),
              rejected_recommendations(0), pending_recommendations(0),
              average_implementation_time(0.0), success_rate(0.0)
        {
        }
    };
    
    RecommendationStatistics getStatistics() const;
    
    // Persistence
    bool saveRecommendations(Database* database);
    bool loadRecommendations(Database* database);

private:
    IndexRecommendationManager();
    ~IndexRecommendationManager();
    
    static IndexRecommendationManager* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    std::map<ULONG, IndexRecommendation> m_recommendations;
    ULONG m_next_recommendation_id;
    mutable ScratchBird::Mutex m_recommendations_mutex;
    
    RecommendationStatistics m_statistics;
    mutable ScratchBird::Mutex m_statistics_mutex;
    
    void updateStatistics();
};

//----------------------------
// Integration with Query Optimizer
//----------------------------

/**
 * Integration layer for bitmap index advisor
 */
class BitmapIndexAdvisorIntegration
{
public:
    // System hooks
    static void registerAdvisor();
    static void unregisterAdvisor();
    
    // Query analysis hooks
    static void onQueryExecution(thread_db* tdbb, const ScratchBird::string& query,
                                double execution_time_ms, ULONG records_examined);
    
    static void onSlowQuery(thread_db* tdbb, const ScratchBird::string& query,
                           double execution_time_ms);
    
    // Index event hooks
    static void onIndexCreated(thread_db* tdbb, const index_desc* idx);
    static void onIndexDropped(thread_db* tdbb, const index_desc* idx);
    static void onIndexUsed(thread_db* tdbb, const index_desc* idx, double selectivity);
    
    // Recommendation triggers
    static void triggerPerformanceAnalysis(thread_db* tdbb, jrd_rel* relation);
    static void triggerWorkloadAnalysis(thread_db* tdbb, Database* database);
    
    // Advisory reporting
    static std::vector<IndexRecommendation> getRecommendationsForCurrentSession(thread_db* tdbb);
    static ScratchBird::string generateAdvisoryReport(thread_db* tdbb, Database* database);

private:
    static std::unique_ptr<BitmapIndexAdvisor> s_global_advisor;
    static bool s_advisor_enabled;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Recommendation utilities
bool isRecommendationApplicable(thread_db* tdbb, const IndexRecommendation& recommendation);
double calculateRecommendationValue(const IndexRecommendation& recommendation);
std::vector<IndexRecommendation> mergeRecommendationSets(const std::vector<IndexRecommendation>& set1,
                                                         const std::vector<IndexRecommendation>& set2);

// Analysis utilities
double calculateFieldCardinality(thread_db* tdbb, jrd_rel* relation, USHORT field_id);
bool isFieldSuitableForBitmapIndex(thread_db* tdbb, jrd_rel* relation, USHORT field_id);
std::vector<USHORT> findCorrelatedFields(thread_db* tdbb, jrd_rel* relation, USHORT primary_field_id);

// Workload analysis utilities
WorkloadPattern mergeWorkloadPatterns(const WorkloadPattern& pattern1, const WorkloadPattern& pattern2);
double calculateWorkloadComplexity(const WorkloadPattern& pattern);
bool isWorkloadBitmapIndexFriendly(const WorkloadPattern& pattern);

// Report generation utilities
ScratchBird::string formatRecommendationAsHtml(const IndexRecommendation& recommendation);
ScratchBird::string formatRecommendationAsXml(const IndexRecommendation& recommendation);
ScratchBird::string formatRecommendationAsJson(const IndexRecommendation& recommendation);

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_ADVISOR_H