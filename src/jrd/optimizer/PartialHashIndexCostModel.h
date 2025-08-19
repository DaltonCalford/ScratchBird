/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created for the ScratchBird Open Source 
 *  RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Project
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 *
 * 2025.07.24 - ScratchBird Partial Hash Index Optimizer Integration
 */

#ifndef JRD_PARTIAL_HASH_INDEX_COST_MODEL_H
#define JRD_PARTIAL_HASH_INDEX_COST_MODEL_H

#include "HashIndexCostModel.h"
#include "Optimizer.h"
#include "../PartialHashIndex.h"
#include "../constants.h"
#include "../common/classes/fb_string.h"
#include "../dsql/BoolNodes.h"

namespace Jrd {

// Forward declarations
class PartialHashIndex;
class BoolExprNode;
struct PartialHashStatistics;

//----------------------------
// Partial Hash Index Cost Constants
//----------------------------

// Partial index specific cost factors
inline constexpr double PARTIAL_HASH_BASE_COST_FACTOR = 0.7;        // Better than regular hash due to smaller size
inline constexpr double PARTIAL_HASH_CONDITION_EVAL_COST = 0.1;     // Cost per condition evaluation
inline constexpr double PARTIAL_HASH_INCLUSION_BENEFIT = 0.5;       // Benefit from reduced index size
inline constexpr double PARTIAL_HASH_CACHE_HIT_BENEFIT = 0.05;      // Benefit from condition cache hits

// Condition complexity impact on cost
inline constexpr double CONDITION_SIMPLE_FACTOR = 1.0;              // Simple conditions (field = value)
inline constexpr double CONDITION_MODERATE_FACTOR = 1.5;            // Moderate conditions (field > value AND field < value2)
inline constexpr double CONDITION_COMPLEX_FACTOR = 3.0;             // Complex conditions (UDFs, subqueries)

// Selectivity thresholds for partial index effectiveness
inline constexpr double PARTIAL_INDEX_MIN_SELECTIVITY = 0.01;       // Minimum useful inclusion ratio (1%)
inline constexpr double PARTIAL_INDEX_OPTIMAL_SELECTIVITY = 0.3;    // Optimal inclusion ratio (30%)
inline constexpr double PARTIAL_INDEX_MAX_SELECTIVITY = 0.8;        // Maximum before full index is better (80%)

//----------------------------
// PartialHashIndexCostModel - Cost calculation for partial hash indexes
//----------------------------
class PartialHashIndexCostModel : public HashIndexCostModel
{
public:
    // Constructor
    PartialHashIndexCostModel();
    
    // Destructor
    ~PartialHashIndexCostModel();

    // Primary cost calculation methods for partial indexes
    static double calculatePartialIndexScanCost(const index_desc* idx, const BoolExprNode* condition,
                                                 double selectivity, double cardinality, ScanType scan_type);
    static double calculatePartialEqualityLookupCost(const index_desc* idx, const BoolExprNode* condition,
                                                      double inclusion_ratio, double cache_hit_ratio = 0.8);
    static double calculateConditionEvaluationCost(const BoolExprNode* condition, double complexity_factor);
    static double calculateInclusionRatioBenefit(double inclusion_ratio, double base_cost);

    // Condition analysis and optimization
    static double analyzeConditionComplexity(const BoolExprNode* condition);
    static double estimateConditionSelectivity(const BoolExprNode* condition, const jrd_rel* relation);
    static double calculateConditionCacheEffectiveness(const BoolExprNode* condition, double cardinality);
    static bool isConditionSuitableForPartialIndex(const BoolExprNode* condition, double& estimated_selectivity);

    // Partial index specific selectivity calculations
    static double calculatePartialHashSelectivity(const index_desc* idx, const BoolExprNode* condition,
                                                   ScanType scan_type, double base_selectivity);
    static double adjustSelectivityForInclusion(double base_selectivity, double inclusion_ratio);
    static double calculateEffectiveSelectivity(double index_selectivity, double condition_selectivity);

    // Cost comparison with other index types
    static bool isPartialHashBetterThanFullHash(const index_desc* partial_idx, const index_desc* full_idx,
                                                const BoolExprNode* condition, double selectivity, double cardinality);
    static bool isPartialHashBetterThanBTree(const index_desc* partial_idx, const index_desc* btree_idx,
                                             const BoolExprNode* condition, double selectivity, double cardinality);
    static double getPartialHashAdvantageRatio(const index_desc* idx, const BoolExprNode* condition,
                                               double inclusion_ratio, ScanType scan_type);

    // Performance prediction for partial indexes
    static double predictPartialIndexPerformance(const index_desc* idx, const BoolExprNode* condition,
                                                  ScanType scan_type, double expected_cardinality);
    static double calculateWorstCasePartialPerformance(const index_desc* idx, const BoolExprNode* condition);
    static double calculateBestCasePartialPerformance(const index_desc* idx, const BoolExprNode* condition);

    // Partial index optimization recommendations
    static double calculateOptimalInclusionRatio(ScanType primary_scan_type, double cardinality);
    static bool recommendPartialIndexCreation(const BoolExprNode* condition, const jrd_rel* relation,
                                               double expected_cardinality, double& confidence_score);
    static void generatePartialIndexOptimizationHints(const index_desc* idx, const BoolExprNode* condition,
                                                      ScratchBird::string& hints);

    // Maintenance cost calculations
    static double calculateMaintenanceCost(const index_desc* idx, const BoolExprNode* condition,
                                           double update_frequency, double inclusion_ratio);
    static double calculateConditionCacheCost(double cache_size, double hit_ratio, double evaluation_cost);
    static double calculateIndexRebuildCost(const index_desc* idx, double inclusion_ratio);

    // Statistics-based cost adjustments
    static void adjustCostFromStatistics(double& cost, const PartialHashStatistics& stats);
    static double calculateDynamicCostAdjustment(const PartialHashStatistics& stats, ScanType scan_type);
    static bool shouldTriggerCostRecalculation(const PartialHashStatistics& stats);

private:
    // Internal calculation helpers
    static double calculateBasePartialCost(ScanType scan_type, double inclusion_ratio);
    static double applyConditionComplexityPenalty(double base_cost, double complexity_factor);
    static double applyInclusionRatioBenefit(double base_cost, double inclusion_ratio);
    static double applyCacheHitBenefit(double base_cost, double cache_hit_ratio);

    // Condition analysis helpers
    static ULONG countConditionNodes(const BoolExprNode* condition);
    static ULONG countFieldReferences(const BoolExprNode* condition);
    static ULONG countFunctionCalls(const BoolExprNode* condition);
    static bool hasSubqueries(const BoolExprNode* condition);
    static bool hasNonDeterministicFunctions(const BoolExprNode* condition);

    // Cost model internal state
    static bool isPartialIndexType(const index_desc* idx);
    static double getPartialIndexInclusionRatio(const index_desc* idx);
    static double getConditionCacheHitRatio(const index_desc* idx);
};

//----------------------------
// PartialHashInversionCandidateAnalyzer - Enhanced analysis for partial hash indexes
//----------------------------
class PartialHashInversionCandidateAnalyzer : public HashInversionCandidateAnalyzer
{
public:
    // Constructor
    explicit PartialHashInversionCandidateAnalyzer(Optimizer* optimizer);

    // Partial index specific analysis methods
    void analyzePartialHashCandidate(InversionCandidate* candidate, const index_desc* idx,
                                     const BoolExprNode* condition, ScanType scan_type, double cardinality);
    void comparePartialWithFullIndex(InversionCandidate* partial_candidate, InversionCandidate* full_candidate,
                                     const BoolExprNode* condition);
    void adjustCostForConditionPattern(InversionCandidate* candidate, const BoolExprNode* condition);

    // Condition matching and optimization
    bool doesConditionMatchPartialIndex(const BoolExprNode* query_condition, const BoolExprNode* index_condition);
    double calculateConditionOverlap(const BoolExprNode* query_condition, const BoolExprNode* index_condition);
    void optimizeForConditionReuse(InversionCandidate* candidate, const BoolExprNode* condition);

    // Partial index suitability analysis
    bool isQuerySuitableForPartialIndex(const BoolExprNode* query_condition, const BoolExprNode* index_condition,
                                        double& suitability_score);
    double calculatePartialIndexEffectiveness(InversionCandidate* candidate, const BoolExprNode* condition);
    void applyPartialIndexPenalties(InversionCandidate* candidate, const BoolExprNode* condition);

    // Advanced performance prediction
    double predictPartialIndexExecutionTime(InversionCandidate* candidate, const BoolExprNode* condition,
                                            double expected_rows);
    double calculateConditionEvaluationOverhead(InversionCandidate* candidate, const BoolExprNode* condition);
    double calculateCacheUtilizationBenefit(InversionCandidate* candidate, const BoolExprNode* condition);

    // Query pattern analysis for partial indexes
    bool isPartialIndexOptimalForQuery(const BoolExprNode* query_condition, const BoolExprNode* index_condition,
                                       double cardinality);
    double calculateQueryPartialIndexAffinity(const ScratchBird::Stack<BoolExprNode*>& query_conditions,
                                              const BoolExprNode* index_condition);
    void adjustForMultiplePartialConditions(InversionCandidate* candidate, 
                                           const ScratchBird::Stack<BoolExprNode*>& conditions);

private:
    // Internal analysis helpers
    void analyzeConditionCompatibility(const BoolExprNode* query_condition, const BoolExprNode* index_condition,
                                      double& compatibility_score, bool& requires_additional_filtering);
    double calculatePartialIndexSavings(InversionCandidate* candidate, const BoolExprNode* condition);
    void adjustForConditionComplexity(InversionCandidate* candidate, double complexity_factor);
    
    // Condition evaluation optimization
    bool canReuseConditionEvaluation(const BoolExprNode* condition1, const BoolExprNode* condition2);
    double estimateConditionCacheEfficiency(const BoolExprNode* condition, double cardinality);
    void optimizeConditionEvaluationOrder(InversionCandidate* candidate, 
                                          const ScratchBird::Stack<BoolExprNode*>& conditions);
};

//----------------------------
// PartialHashOptimizerIntegration - Integration with ScratchBird optimizer
//----------------------------
class PartialHashOptimizerIntegration
{
public:
    // Static integration methods for partial hash indexes
    static void enhanceIndexScratchForPartialHash(IndexScratch& scratch, const index_desc* idx,
                                                  const BoolExprNode* condition);
    static void adjustInversionCandidateForPartialHash(InversionCandidate* candidate, const index_desc* idx,
                                                       const BoolExprNode* condition, ScanType scan_type, 
                                                       double cardinality);
    static bool shouldPreferPartialHashOverOthers(const index_desc* partial_idx, 
                                                  const ScratchBird::Array<index_desc*>& other_indexes,
                                                  const BoolExprNode* query_condition, double cardinality);

    // Query condition analysis and matching
    static bool canUsePartialIndex(const BoolExprNode* query_condition, const BoolExprNode* index_condition,
                                  double& match_quality);
    static void analyzeQueryConditionForPartialIndexes(const ScratchBird::Stack<BoolExprNode*>& query_conditions,
                                                       ScratchBird::Array<const index_desc*>& suitable_indexes,
                                                       ScratchBird::Array<double>& match_scores);
    static void optimizePartialIndexSelection(ScratchBird::Array<InversionCandidate*>& candidates,
                                             const ScratchBird::Stack<BoolExprNode*>& query_conditions);

    // Cost model integration for optimizer
    static double calculatePartialHashIndexCost(const index_desc* idx, const BoolExprNode* condition,
                                                double selectivity, double cardinality, ScanType scan_type);
    static double calculatePartialHashSelectivityAdjustment(const index_desc* idx, const BoolExprNode* condition,
                                                           ScanType scan_type, double base_selectivity);
    static void generatePartialHashIndexPlan(InversionCandidate* candidate, const BoolExprNode* condition,
                                            ScratchBird::string& plan_text);

    // Performance monitoring and statistics integration
    static void collectPartialHashIndexStatistics(const index_desc* idx, const BoolExprNode* condition,
                                                  double execution_time, ULONG records_retrieved,
                                                  ULONG records_evaluated, ULONG condition_cache_hits);
    static void updatePartialHashIndexSelectivity(index_desc* idx, const BoolExprNode* condition,
                                                  double observed_selectivity, double observed_inclusion_ratio);
    static bool shouldRecommendPartialIndexMaintenance(const index_desc* idx, const PartialHashStatistics& stats);

    // Adaptive optimization based on runtime statistics
    static void adaptCostModelFromStatistics(const index_desc* idx, const PartialHashStatistics& stats);
    static void recommendPartialIndexTuning(const index_desc* idx, const PartialHashStatistics& stats,
                                           ScratchBird::string& recommendations);
    static double calculateDynamicCostAdjustment(const index_desc* idx, const PartialHashStatistics& stats);

private:
    // Integration helpers
    static bool isPartialHashIndexType(const index_desc* idx);
    static const BoolExprNode* extractPartialIndexCondition(const index_desc* idx);
    static ScanType determineScanTypeForPartialIndex(const ScratchBird::Stack<BoolExprNode*>& conditions,
                                                    const BoolExprNode* index_condition);
    static double calculateQueryComplexityFactorForPartial(const ScratchBird::Stack<BoolExprNode*>& conditions,
                                                          const BoolExprNode* index_condition);
    
    // Statistics integration helpers
    static void extractPartialIndexStatistics(const index_desc* idx, PartialHashStatistics& stats);
    static bool areStatisticsReliable(const PartialHashStatistics& stats);
    static double calculateStatisticalConfidence(const PartialHashStatistics& stats);
};

//----------------------------
// Utility Functions for Partial Hash Index Optimization
//----------------------------

// Condition analysis utilities
bool areConditionsEquivalent(const BoolExprNode* condition1, const BoolExprNode* condition2);
bool isConditionSubset(const BoolExprNode* subset_condition, const BoolExprNode* superset_condition);
double calculateConditionSimilarity(const BoolExprNode* condition1, const BoolExprNode* condition2);

// Cost comparison utilities for partial indexes
double comparePartialIndexCosts(double partial_cost, double full_cost, double condition_overhead);
bool isSignificantPartialIndexBenefit(double partial_cost, double alternative_cost, double threshold = 0.2);
double calculatePartialIndexROI(double cost_savings, double maintenance_overhead);

// Performance prediction utilities
double predictPartialHashPerformance(const index_desc* idx, const BoolExprNode* condition, 
                                     ScanType scan_type, double cardinality);
double estimateOptimalPartialPerformance(const index_desc* idx, const BoolExprNode* condition, double cardinality);
bool shouldUsePartialHashIndex(const index_desc* idx, const BoolExprNode* query_condition, 
                              const BoolExprNode* index_condition, double cardinality);

} // namespace Jrd

#endif // JRD_PARTIAL_HASH_INDEX_COST_MODEL_H