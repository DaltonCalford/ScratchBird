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

#include "scratchbird.h"
#include "PartialHashIndexCostModel.h"
#include "../PartialHashIndex.h" 
#include "../jrd.h"
#include "../exe.h"
#include "../req.h"
#include "../btr.h"
#include "../common/isc_proto.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"

using namespace Jrd;
using namespace ScratchBird;

//----------------------------
// PartialHashIndexCostModel Implementation
//----------------------------

PartialHashIndexCostModel::PartialHashIndexCostModel()
{
    // Constructor implementation
}

PartialHashIndexCostModel::~PartialHashIndexCostModel()
{
    // Destructor implementation
}

double PartialHashIndexCostModel::calculatePartialIndexScanCost(const index_desc* idx, const BoolExprNode* condition,
                                                                double selectivity, double cardinality, ScanType scan_type)
{
    if (!idx || !condition)
        return DEFAULT_INDEX_COST * 2; // High cost for invalid input
        
    try {
        // Start with base hash index cost
        double base_cost = calculateIndexScanCost(idx, selectivity, cardinality, scan_type);
        
        // Apply partial index benefit (smaller index size)
        double inclusion_ratio = getPartialIndexInclusionRatio(idx);
        double partial_benefit = calculateInclusionRatioBenefit(inclusion_ratio, base_cost);
        base_cost *= partial_benefit;
        
        // Add condition evaluation cost
        double condition_complexity = analyzeConditionComplexity(condition);
        double condition_cost = calculateConditionEvaluationCost(condition, condition_complexity);
        
        // Apply cache hit benefit
        double cache_hit_ratio = getConditionCacheHitRatio(idx);
        double cache_benefit = 1.0 - (cache_hit_ratio * PARTIAL_HASH_CACHE_HIT_BENEFIT);
        condition_cost *= cache_benefit;
        
        // Total cost combines index access and condition evaluation
        double total_cost = base_cost + condition_cost;
        
        // Apply scan type specific adjustments
        switch (scan_type)
        {
            case SCAN_EQUALITY:
                // Partial hash excels at equality when condition matches
                total_cost *= PARTIAL_HASH_BASE_COST_FACTOR;
                break;
                
            case SCAN_RANGE:
                // Range scans are expensive for hash indexes
                total_cost *= 1.5; // Additional penalty for partial indexes
                break;
                
            case SCAN_FULL:
                // Full scans benefit significantly from smaller partial index
                total_cost *= inclusion_ratio; // Direct ratio benefit
                break;
                
            default:
                break;
        }
        
        return total_cost;
    }
    catch (const Exception&)
    {
        return DEFAULT_INDEX_COST * 2; // Conservative fallback
    }
}

double PartialHashIndexCostModel::calculatePartialEqualityLookupCost(const index_desc* idx, const BoolExprNode* condition,
                                                                     double inclusion_ratio, double cache_hit_ratio)
{
    if (!idx || !condition)
        return DEFAULT_HASH_INDEX_COST;
        
    try {
        // Base equality lookup cost for hash index
        double base_cost = calculateEqualityLookupCost(idx);
        
        // Apply inclusion ratio benefit (smaller hash table)
        base_cost *= calculateInclusionRatioBenefit(inclusion_ratio, 1.0);
        
        // Add condition evaluation overhead
        double condition_complexity = analyzeConditionComplexity(condition);
        double condition_overhead = PARTIAL_HASH_CONDITION_EVAL_COST * condition_complexity;
        
        // Apply cache hit benefit
        condition_overhead *= (1.0 - cache_hit_ratio * PARTIAL_HASH_CACHE_HIT_BENEFIT);
        
        return base_cost + condition_overhead;
    }
    catch (const Exception&)
    {
        return DEFAULT_HASH_INDEX_COST;
    }
}

double PartialHashIndexCostModel::calculateConditionEvaluationCost(const BoolExprNode* condition, double complexity_factor)
{
    if (!condition)
        return 0.0;
        
    try {
        // Base cost per condition evaluation
        double base_cost = PARTIAL_HASH_CONDITION_EVAL_COST;
        
        // Apply complexity factor
        base_cost *= complexity_factor;
        
        // Additional cost for complex expressions
        ULONG node_count = countConditionNodes(condition);
        base_cost *= (1.0 + node_count * 0.1);
        
        // Additional cost for function calls
        ULONG function_count = countFunctionCalls(condition);
        base_cost *= (1.0 + function_count * 0.3);
        
        return base_cost;
    }
    catch (const Exception&)
    {
        return PARTIAL_HASH_CONDITION_EVAL_COST * 2.0; // Conservative estimate
    }
}

double PartialHashIndexCostModel::calculateInclusionRatioBenefit(double inclusion_ratio, double base_cost)
{
    if (inclusion_ratio <= 0.0 || inclusion_ratio > 1.0)
        return 1.0; // No benefit for invalid ratios
        
    // Benefit is non-linear - smaller ratios provide greater benefit
    // Formula: benefit = 0.3 + 0.7 * inclusion_ratio
    // This gives 30% of original cost at 0% inclusion, 100% at 100% inclusion
    double benefit_factor = PARTIAL_HASH_INCLUSION_BENEFIT + 
                           (1.0 - PARTIAL_HASH_INCLUSION_BENEFIT) * inclusion_ratio;
    
    return benefit_factor;
}

double PartialHashIndexCostModel::analyzeConditionComplexity(const BoolExprNode* condition)
{
    if (!condition)
        return CONDITION_SIMPLE_FACTOR;
        
    try {
        double complexity = CONDITION_SIMPLE_FACTOR;
        
        // Analyze node structure
        ULONG node_count = countConditionNodes(condition);
        ULONG field_refs = countFieldReferences(condition);
        ULONG func_calls = countFunctionCalls(condition);
        
        // Base complexity from node count
        if (node_count <= 3)
            complexity = CONDITION_SIMPLE_FACTOR;
        else if (node_count <= 10)
            complexity = CONDITION_MODERATE_FACTOR;
        else
            complexity = CONDITION_COMPLEX_FACTOR;
        
        // Adjust for function calls
        if (func_calls > 0)
            complexity *= (1.0 + func_calls * 0.5);
            
        // Adjust for subqueries
        if (hasSubqueries(condition))
            complexity *= 2.0;
            
        // Adjust for non-deterministic functions
        if (hasNonDeterministicFunctions(condition))
            complexity *= 1.5;
            
        return complexity;
    }
    catch (const Exception&)
    {
        return CONDITION_COMPLEX_FACTOR; // Conservative estimate
    }
}

double PartialHashIndexCostModel::estimateConditionSelectivity(const BoolExprNode* condition, const jrd_rel* relation)
{
    if (!condition || !relation)
        return DEFAULT_SELECTIVITY;
        
    try {
        // Use optimizer's built-in selectivity calculation
        double base_selectivity = Optimizer::getSelectivity(condition);
        
        // Adjust based on condition complexity
        double complexity = analyzeConditionComplexity(condition);
        if (complexity > CONDITION_MODERATE_FACTOR)
        {
            // Complex conditions tend to be more selective
            base_selectivity *= 0.7;
        }
        
        // Ensure reasonable bounds
        if (base_selectivity < 0.001)
            base_selectivity = 0.001;
        if (base_selectivity > 0.9)
            base_selectivity = 0.9;
            
        return base_selectivity;
    }
    catch (const Exception&)
    {
        return DEFAULT_SELECTIVITY;
    }
}

double PartialHashIndexCostModel::calculateConditionCacheEffectiveness(const BoolExprNode* condition, double cardinality)
{
    if (!condition || cardinality <= 0)
        return 0.0;
        
    try {
        // Cache effectiveness depends on:
        // 1. Condition complexity (complex conditions benefit more from caching)
        // 2. Cardinality (higher cardinality benefits more from caching)
        // 3. Condition determinism (non-deterministic conditions can't be cached effectively)
        
        double complexity = analyzeConditionComplexity(condition);
        double base_effectiveness = complexity / CONDITION_COMPLEX_FACTOR;
        
        // Scale based on cardinality
        double cardinality_factor = MIN(cardinality / 1000.0, 1.0); // Max benefit at 1000+ records
        base_effectiveness *= cardinality_factor;
        
        // Reduce effectiveness for non-deterministic conditions
        if (hasNonDeterministicFunctions(condition))
            base_effectiveness *= 0.1;
            
        return MIN(base_effectiveness, 0.95); // Cap at 95% effectiveness
    }
    catch (const Exception&)
    {
        return 0.5; // Moderate effectiveness as fallback
    }
}

bool PartialHashIndexCostModel::isConditionSuitableForPartialIndex(const BoolExprNode* condition, double& estimated_selectivity)
{
    estimated_selectivity = DEFAULT_SELECTIVITY;
    
    if (!condition)
        return false;
        
    try {
        // Check for non-deterministic functions
        if (hasNonDeterministicFunctions(condition))
            return false;
            
        // Estimate selectivity
        estimated_selectivity = Optimizer::getSelectivity(condition);
        
        // Check if selectivity is in useful range for partial indexes
        if (estimated_selectivity < PARTIAL_INDEX_MIN_SELECTIVITY || 
            estimated_selectivity > PARTIAL_INDEX_MAX_SELECTIVITY)
            return false;
            
        // Check condition complexity - very complex conditions may not be worth it
        double complexity = analyzeConditionComplexity(condition);
        if (complexity > CONDITION_COMPLEX_FACTOR * 2.0)
            return false;
            
        return true;
    }
    catch (const Exception&)
    {
        return false;
    }
}

double PartialHashIndexCostModel::calculatePartialHashSelectivity(const index_desc* idx, const BoolExprNode* condition,
                                                                  ScanType scan_type, double base_selectivity)
{
    if (!idx || !condition)
        return base_selectivity;
        
    try {
        // Start with base hash selectivity
        double hash_selectivity = calculateHashSelectivity(idx, scan_type, base_selectivity);
        
        // Adjust for partial index characteristics
        double inclusion_ratio = getPartialIndexInclusionRatio(idx);
        double adjusted_selectivity = adjustSelectivityForInclusion(hash_selectivity, inclusion_ratio);
        
        // For equality scans, partial hash indexes can be very effective
        if (scan_type == SCAN_EQUALITY && inclusion_ratio < 0.5)
        {
            adjusted_selectivity *= 0.8; // 20% improvement in selectivity
        }
        
        return adjusted_selectivity;
    }
    catch (const Exception&)
    {
        return base_selectivity;
    }
}

double PartialHashIndexCostModel::adjustSelectivityForInclusion(double base_selectivity, double inclusion_ratio)
{
    if (inclusion_ratio <= 0.0 || inclusion_ratio > 1.0)
        return base_selectivity;
        
    // Effective selectivity is the product of index selectivity and inclusion ratio
    // But with a non-linear adjustment to account for cache effects and optimizations
    double effective_selectivity = base_selectivity * inclusion_ratio;
    
    // Apply bonus for very selective partial indexes
    if (inclusion_ratio < 0.1)
    {
        effective_selectivity *= 0.9; // 10% bonus for very selective indexes
    }
    
    return effective_selectivity;
}

double PartialHashIndexCostModel::calculateEffectiveSelectivity(double index_selectivity, double condition_selectivity)
{
    // The effective selectivity is the product, but with adjustments for interaction effects
    double base_effective = index_selectivity * condition_selectivity;
    
    // Apply slight bonus for the compound effect
    double bonus_factor = 1.0 - (1.0 - index_selectivity) * (1.0 - condition_selectivity) * 0.1;
    
    return base_effective * bonus_factor;
}

bool PartialHashIndexCostModel::isPartialHashBetterThanFullHash(const index_desc* partial_idx, const index_desc* full_idx,
                                                                const BoolExprNode* condition, double selectivity, double cardinality)
{
    if (!partial_idx || !full_idx || !condition)
        return false;
        
    try {
        // Calculate costs for both indexes
        double partial_cost = calculatePartialIndexScanCost(partial_idx, condition, selectivity, cardinality, SCAN_EQUALITY);
        double full_cost = calculateIndexScanCost(full_idx, selectivity, cardinality, SCAN_EQUALITY);
        
        // Partial is better if it's significantly cheaper (>10% improvement)
        return partial_cost < full_cost * 0.9;
    }
    catch (const Exception&)
    {
        return false;
    }
}

bool PartialHashIndexCostModel::isPartialHashBetterThanBTree(const index_desc* partial_idx, const index_desc* btree_idx,
                                                             const BoolExprNode* condition, double selectivity, double cardinality)
{
    if (!partial_idx || !btree_idx || !condition)
        return false;
        
    try {
        // Partial hash is good for equality, poor for ranges
        double partial_equality_cost = calculatePartialEqualityLookupCost(partial_idx, condition, 
                                                                         getPartialIndexInclusionRatio(partial_idx));
        double btree_cost = DEFAULT_INDEX_COST; // Standard B-Tree cost
        
        // For equality lookups, partial hash can be significantly better
        return partial_equality_cost < btree_cost * 0.8;
    }
    catch (const Exception&)
    {
        return false;
    }
}

double PartialHashIndexCostModel::getPartialHashAdvantageRatio(const index_desc* idx, const BoolExprNode* condition,
                                                               double inclusion_ratio, ScanType scan_type)
{
    if (!idx || !condition)
        return 1.0; // No advantage
        
    try {
        double base_advantage = calculateInclusionRatioBenefit(inclusion_ratio, 1.0);
        
        // Adjust based on scan type
        switch (scan_type)
        {
            case SCAN_EQUALITY:
                // Maximum advantage for equality scans
                return base_advantage * 0.7; // Up to 30% of original cost
                
            case SCAN_RANGE:
                // Less advantage for range scans
                return base_advantage * 1.2; // Limited benefit
                
            case SCAN_FULL:
                // Good advantage for full scans due to smaller size
                return base_advantage;
                
            default:
                return base_advantage;
        }
    }
    catch (const Exception&)
    {
        return 1.0;
    }
}

// Internal helper methods implementation

ULONG PartialHashIndexCostModel::countConditionNodes(const BoolExprNode* condition)
{
    if (!condition)
        return 0;
        
    // Simplified node counting - in a real implementation, this would traverse the expression tree
    // For now, estimate based on expression complexity
    return 3; // Average estimate
}

ULONG PartialHashIndexCostModel::countFieldReferences(const BoolExprNode* condition)
{
    if (!condition)
        return 0;
        
    // Simplified field reference counting
    return 2; // Average estimate
}

ULONG PartialHashIndexCostModel::countFunctionCalls(const BoolExprNode* condition)
{
    if (!condition)
        return 0;
        
    // Simplified function call counting
    return 1; // Average estimate
}

bool PartialHashIndexCostModel::hasSubqueries(const BoolExprNode* condition)
{
    if (!condition)
        return false;
        
    // Simplified subquery detection
    // Real implementation would traverse the expression tree looking for SubQueryNode
    return false; // Conservative estimate
}

bool PartialHashIndexCostModel::hasNonDeterministicFunctions(const BoolExprNode* condition)
{
    if (!condition)
        return false;
        
    // Simplified non-deterministic function detection
    // Real implementation would check for functions like RAND(), NOW(), etc.
    return false; // Conservative estimate
}

bool PartialHashIndexCostModel::isPartialIndexType(const index_desc* idx)
{
    if (!idx)
        return false;
        
    return (idx->idx_flags & idx_condition) && 
           (idx->idx_expression_request != nullptr);
}

double PartialHashIndexCostModel::getPartialIndexInclusionRatio(const index_desc* idx)
{
    if (!idx || !isPartialIndexType(idx))
        return 1.0; // Full inclusion for non-partial indexes
        
    // In a real implementation, this would extract the inclusion ratio from index statistics
    // For now, return a reasonable estimate
    return 0.3; // 30% inclusion ratio estimate
}

double PartialHashIndexCostModel::getConditionCacheHitRatio(const index_desc* idx)
{
    if (!idx || !isPartialIndexType(idx))
        return 0.0; // No cache for non-partial indexes
        
    // In a real implementation, this would extract cache statistics
    // For now, return a reasonable estimate
    return 0.8; // 80% cache hit ratio estimate
}

//----------------------------
// PartialHashInversionCandidateAnalyzer Implementation
//----------------------------

PartialHashInversionCandidateAnalyzer::PartialHashInversionCandidateAnalyzer(Optimizer* optimizer)
    : HashInversionCandidateAnalyzer(optimizer)
{
}

void PartialHashInversionCandidateAnalyzer::analyzePartialHashCandidate(InversionCandidate* candidate, const index_desc* idx,
                                                                        const BoolExprNode* condition, ScanType scan_type, double cardinality)
{
    if (!candidate || !idx || !condition)
        return;
        
    try {
        // First analyze as regular hash candidate
        analyzeHashCandidate(candidate, idx, scan_type, cardinality);
        
        // Then apply partial index specific adjustments
        double inclusion_ratio = PartialHashIndexCostModel::getPartialIndexInclusionRatio(idx);
        double condition_complexity = PartialHashIndexCostModel::analyzeConditionComplexity(condition);
        
        // Adjust cost for partial index characteristics
        double partial_cost = PartialHashIndexCostModel::calculatePartialIndexScanCost(idx, condition, 
                                                                                       candidate->selectivity, cardinality, scan_type);
        candidate->cost = partial_cost;
        
        // Adjust selectivity for partial index
        candidate->selectivity = PartialHashIndexCostModel::calculatePartialHashSelectivity(idx, condition, 
                                                                                            scan_type, candidate->selectivity);
        
        // Add condition evaluation overhead to execution time
        double condition_cost = PartialHashIndexCostModel::calculateConditionEvaluationCost(condition, condition_complexity);
        candidate->cost += condition_cost;
        
        // Update candidate metadata
        candidate->boolean = const_cast<BoolExprNode*>(condition);
        candidate->condition = const_cast<BoolExprNode*>(condition);
    }
    catch (const Exception&)
    {
        // Conservative fallback - make candidate less attractive
        candidate->cost *= 2.0;
        candidate->selectivity *= 2.0;
    }
}

bool PartialHashInversionCandidateAnalyzer::doesConditionMatchPartialIndex(const BoolExprNode* query_condition, 
                                                                           const BoolExprNode* index_condition)
{
    if (!query_condition || !index_condition)
        return false;
        
    try {
        // Simplified condition matching - in a real implementation, this would be much more sophisticated
        // For now, check if conditions are identical or query condition implies index condition
        
        // Check for exact match (simplified)
        if (query_condition == index_condition)
            return true;
            
        // Check for subset relationship (simplified)
        // Real implementation would analyze the logical relationship between conditions
        return false; // Conservative approach
    }
    catch (const Exception&)
    {
        return false;
    }
}

double PartialHashInversionCandidateAnalyzer::calculateConditionOverlap(const BoolExprNode* query_condition, 
                                                                        const BoolExprNode* index_condition)
{
    if (!query_condition || !index_condition)
        return 0.0;
        
    try {
        // Simplified overlap calculation
        // Real implementation would analyze the logical overlap between conditions
        
        if (doesConditionMatchPartialIndex(query_condition, index_condition))
            return 1.0; // Perfect overlap
            
        // For now, return a conservative estimate
        return 0.5; // 50% overlap estimate
    }
    catch (const Exception&)
    {
        return 0.0;
    }
}

//----------------------------
// PartialHashOptimizerIntegration Implementation
//----------------------------

const BoolExprNode* PartialHashOptimizerIntegration::extractPartialIndexCondition(const index_desc* idx)
{
    if (!idx || !isPartialHashIndexType(idx))
        return nullptr;
        
    // Extract the condition from the index descriptor
    // In a real implementation, this would extract the condition from idx->idx_condition
    return idx->idx_condition;
}

ScanType PartialHashOptimizerIntegration::determineScanTypeForPartialIndex(const ScratchBird::Stack<BoolExprNode*>& conditions,
                                                                          const BoolExprNode* index_condition)
{
    if (conditions.isEmpty() || !index_condition)
        return SCAN_FULL;
        
    // Analyze the query conditions to determine the scan type
    // For now, default to equality scan for hash indexes
    return SCAN_EQUALITY;
}

double PartialHashOptimizerIntegration::calculateQueryComplexityFactorForPartial(const ScratchBird::Stack<BoolExprNode*>& conditions,
                                                                                const BoolExprNode* index_condition)
{
    if (conditions.isEmpty())
        return 1.0;
        
    double complexity = 1.0;
    
    // Analyze query conditions for complexity
    for (const auto condition : conditions)
    {
        if (condition)
        {
            double conditionComplexity = PartialHashIndexCostModel::analyzeConditionComplexity(condition);
            complexity *= conditionComplexity;
        }
    }
    
    return MIN(complexity, 5.0); // Cap at 5x complexity
}

bool PartialHashOptimizerIntegration::isPartialHashIndexType(const index_desc* idx)
{
    return PartialHashIndexCostModel::isPartialIndexType(idx);
}

void PartialHashOptimizerIntegration::enhanceIndexScratchForPartialHash(IndexScratch& scratch, const index_desc* idx,
                                                                        const BoolExprNode* condition)
{
    if (!idx || !condition)
        return;
        
    try {
        // First enhance for regular hash index
        HashOptimizerIntegration::enhanceIndexScratchForHash(scratch, idx);
        
        // Then add partial index specific enhancements
        if (PartialHashIndexCostModel::isPartialIndexType(idx))
        {
            // Adjust cardinality based on inclusion ratio
            double inclusion_ratio = PartialHashIndexCostModel::getPartialIndexInclusionRatio(idx);
            scratch.cardinality *= inclusion_ratio;
            
            // Adjust selectivity
            scratch.selectivity = PartialHashIndexCostModel::calculatePartialHashSelectivity(idx, condition, 
                                                                                            SCAN_EQUALITY, scratch.selectivity);
            
            // Mark as suitable for partial index usage
            scratch.candidate = true;
        }
    }
    catch (const Exception&)
    {
        // Conservative fallback
        scratch.candidate = false;
    }
}

bool PartialHashOptimizerIntegration::canUsePartialIndex(const BoolExprNode* query_condition, const BoolExprNode* index_condition,
                                                        double& match_quality)
{
    match_quality = 0.0;
    
    if (!query_condition || !index_condition)
        return false;
        
    try {
        // Check if query condition is compatible with index condition
        bool basic_compatibility = true; // Simplified check
        
        if (basic_compatibility)
        {
            // Calculate match quality
            match_quality = PartialHashInversionCandidateAnalyzer(nullptr).calculateConditionOverlap(query_condition, index_condition);
            return match_quality > 0.1; // Require at least 10% overlap
        }
        
        return false;
    }
    catch (const Exception&)
    {
        return false;
    }
}

double PartialHashOptimizerIntegration::calculatePartialHashIndexCost(const index_desc* idx, const BoolExprNode* condition,
                                                                      double selectivity, double cardinality, ScanType scan_type)
{
    return PartialHashIndexCostModel::calculatePartialIndexScanCost(idx, condition, selectivity, cardinality, scan_type);
}

void PartialHashOptimizerIntegration::adjustInversionCandidateForPartialHash(InversionCandidate* candidate, const index_desc* idx,
                                                                             const BoolExprNode* condition, ScanType scan_type, 
                                                                             double cardinality)
{
    if (!candidate || !idx || !condition)
        return;
        
    try {
        // Calculate partial hash specific cost
        double partial_cost = calculatePartialHashIndexCost(idx, condition, candidate->selectivity, cardinality, scan_type);
        candidate->cost = partial_cost;
        
        // Adjust selectivity for partial index characteristics
        candidate->selectivity = PartialHashIndexCostModel::calculatePartialHashSelectivity(
            idx, condition, scan_type, candidate->selectivity);
        
        // Set condition reference
        candidate->condition = const_cast<BoolExprNode*>(condition);
        
        // Mark as enhanced for partial hash
        candidate->boolean = const_cast<BoolExprNode*>(condition);
    }
    catch (const Exception&) {
        // Conservative fallback - make candidate less attractive
        candidate->cost *= 1.5;
    }
}

bool PartialHashOptimizerIntegration::shouldPreferPartialHashOverOthers(const index_desc* partial_idx, 
                                                                        const ScratchBird::Array<index_desc*>& other_indexes,
                                                                        const BoolExprNode* query_condition, double cardinality)
{
    if (!partial_idx || !query_condition)
        return false;
        
    try {
        const BoolExprNode* partial_condition = extractPartialIndexCondition(partial_idx);
        if (!partial_condition)
            return false;
            
        // Check if query can use partial index
        double match_quality;
        if (!canUsePartialIndex(query_condition, partial_condition, match_quality))
            return false;
            
        // Calculate partial index cost
        double partial_cost = calculatePartialHashIndexCost(partial_idx, partial_condition,
                                                           DEFAULT_SELECTIVITY, cardinality, SCAN_EQUALITY);
        
        // Compare with other indexes
        for (const auto other_idx : other_indexes)
        {
            if (other_idx && other_idx != partial_idx)
            {
                double other_cost = DEFAULT_INDEX_COST;
                if (partial_cost >= other_cost * 0.8) // Require 20% improvement
                    return false;
            }
        }
        
        return match_quality > 0.5; // Require good match quality
    }
    catch (const Exception&) {
        return false;
    }
}

void PartialHashOptimizerIntegration::analyzeQueryConditionForPartialIndexes(const ScratchBird::Stack<BoolExprNode*>& query_conditions,
                                                                             ScratchBird::Array<const index_desc*>& suitable_indexes,
                                                                             ScratchBird::Array<double>& match_scores)
{
    suitable_indexes.clear();
    match_scores.clear();
    
    if (query_conditions.isEmpty())
        return;
        
    // For each query condition, find suitable partial indexes
    // This is a simplified implementation - real version would scan available indexes
    // and analyze their conditions against query conditions
}

void PartialHashOptimizerIntegration::optimizePartialIndexSelection(ScratchBird::Array<InversionCandidate*>& candidates,
                                                                   const ScratchBird::Stack<BoolExprNode*>& query_conditions)
{
    if (candidates.isEmpty() || query_conditions.isEmpty())
        return;
        
    // Sort candidates by cost, prioritizing partial hash indexes with good condition matches
    for (auto candidate : candidates)
    {
        if (candidate && candidate->scratch && candidate->scratch->index)
        {
            const auto idx = candidate->scratch->index;
            if (isPartialHashIndexType(idx))
            {
                const BoolExprNode* partial_condition = extractPartialIndexCondition(idx);
                if (partial_condition)
                {
                    // Boost candidate ranking for good condition matches
                    for (const auto query_condition : query_conditions)
                    {
                        double match_quality;
                        if (canUsePartialIndex(query_condition, partial_condition, match_quality))
                        {
                            candidate->cost *= (1.0 - match_quality * 0.3); // Up to 30% cost reduction
                        }
                    }
                }
            }
        }
    }
}

//----------------------------
// Utility Functions Implementation
//----------------------------

bool areConditionsEquivalent(const BoolExprNode* condition1, const BoolExprNode* condition2)
{
    if (!condition1 || !condition2)
        return (condition1 == condition2);
        
    // Simplified equivalence check
    // Real implementation would perform deep structural comparison
    return (condition1 == condition2);
}

double calculateConditionSimilarity(const BoolExprNode* condition1, const BoolExprNode* condition2)
{
    if (!condition1 || !condition2)
        return 0.0;
        
    if (areConditionsEquivalent(condition1, condition2))
        return 1.0;
        
    // Simplified similarity calculation
    // Real implementation would analyze structural and semantic similarity
    return 0.3; // Conservative estimate
}

bool shouldUsePartialHashIndex(const index_desc* idx, const BoolExprNode* query_condition, 
                              const BoolExprNode* index_condition, double cardinality)
{
    if (!idx || !query_condition || !index_condition)
        return false;
        
    try {
        // Check if partial index is suitable
        double estimated_selectivity;
        if (!PartialHashIndexCostModel::isConditionSuitableForPartialIndex(index_condition, estimated_selectivity))
            return false;
            
        // Check if query can use the partial index
        double match_quality;
        if (!PartialHashOptimizerIntegration::canUsePartialIndex(query_condition, index_condition, match_quality))
            return false;
            
        // Calculate cost benefit
        double partial_cost = PartialHashIndexCostModel::calculatePartialIndexScanCost(idx, index_condition,
                                                                                       estimated_selectivity, cardinality, SCAN_EQUALITY);
        double regular_cost = DEFAULT_INDEX_COST;
        
        return partial_cost < regular_cost * 0.8; // Require 20% improvement
    }
    catch (const Exception&)
    {
        return false;
    }
}