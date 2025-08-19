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
 *  Contributor(s): _______________________________________.
 *
 * 2025.07.22 - ScratchBird Hash Index Implementation - Cost Model
 */

#include "scratchbird.h"
#include "HashIndexCostModel.h"
#include "../HashIndex.h"
#include "../HashBucketManager.h"
#include "../exe.h"
#include "../common/gdsassert.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/ExprNodes.h"
#include "../jrd/lls.h"

using namespace ScratchBird;
using namespace Jrd;

//----------------------------
// HashIndexCostModel Implementation
//----------------------------

HashIndexCostModel::HashIndexCostModel()
{
	// Default constructor
}

HashIndexCostModel::~HashIndexCostModel()
{
	// Default destructor
}

double HashIndexCostModel::calculateIndexScanCost(const index_desc* idx, double selectivity, 
												 double cardinality, ScanType scan_type)
{
	if (!isHashIndex(idx))
		return DEFAULT_HASH_INDEX_COST;

	double base_cost = calculateBaseCost(scan_type);
	double load_factor = estimateLoadFactor(idx, cardinality);

	// Apply load factor penalty
	base_cost = applyLoadFactorPenalty(base_cost, load_factor);

	// Apply collision penalty
	double collision_rate = calculateCollisionProbability(load_factor, extractBucketCount(idx));
	base_cost = applyCollisionPenalty(base_cost, collision_rate);

	// Apply selectivity multiplier
	base_cost *= selectivity;

	return base_cost;
}

double HashIndexCostModel::calculateEqualityLookupCost(const index_desc* idx, double load_factor)
{
	if (!isHashIndex(idx))
		return DEFAULT_INDEX_COST;

	// Hash indexes excel at equality lookups - O(1) average case
	double base_cost = DEFAULT_HASH_INDEX_COST * HASH_EQUALITY_COST_FACTOR;

	// Adjust for load factor - higher load means more collisions
	if (load_factor > HASH_OPTIMAL_LOAD_FACTOR)
	{
		double penalty = (load_factor - HASH_OPTIMAL_LOAD_FACTOR) * HASH_COLLISION_PENALTY;
		base_cost += penalty;
	}

	return base_cost;
}

double HashIndexCostModel::calculateRangeScanCost(const index_desc* idx, double selectivity, double cardinality)
{
	if (!isHashIndex(idx))
		return DEFAULT_INDEX_COST * 2.0;

	// Hash indexes are very poor at range scans - must scan entire hash table
	double base_cost = DEFAULT_HASH_INDEX_COST * HASH_RANGE_COST_FACTOR;
	
	// For range scans, we essentially do a full scan with filtering
	base_cost *= cardinality * selectivity;
	
	return base_cost;
}

double HashIndexCostModel::calculateFullScanCost(const index_desc* idx, double cardinality)
{
	if (!isHashIndex(idx))
		return DEFAULT_INDEX_COST * cardinality;

	// Full scan of hash index requires visiting all buckets
	ULONG bucket_count = extractBucketCount(idx);
	double base_cost = DEFAULT_HASH_INDEX_COST * HASH_SCAN_COST_FACTOR;
	
	// Cost is proportional to number of buckets plus records
	base_cost *= (bucket_count * 0.1 + cardinality);
	
	return base_cost;
}

double HashIndexCostModel::calculateHashSelectivity(const index_desc* idx, ScanType scan_type, 
													double base_selectivity)
{
	if (!isHashIndex(idx))
		return base_selectivity;

	switch (scan_type)
	{
		case SCAN_EQUALITY:
			// Hash indexes are excellent for equality lookups
			return base_selectivity * HASH_EQUALITY_SELECTIVITY_BOOST;
			
		case SCAN_RANGE:
		case SCAN_STARTING:
			// Hash indexes are poor for range operations
			return base_selectivity * HASH_RANGE_SELECTIVITY_PENALTY;
			
		case SCAN_FULL:
			// Full scan selectivity unchanged
			return base_selectivity;
			
		default:
			return base_selectivity;
	}
}

double HashIndexCostModel::adjustSelectivityForCollisions(double base_selectivity, double load_factor)
{
	if (load_factor <= HASH_OPTIMAL_LOAD_FACTOR)
		return base_selectivity;

	// Higher load factor means more collisions, reducing effective selectivity
	double collision_penalty = (load_factor - HASH_OPTIMAL_LOAD_FACTOR) * 0.1;
	return base_selectivity * (1.0 + collision_penalty);
}

double HashIndexCostModel::calculateEqualitySelectivity(const index_desc* idx)
{
	if (!isHashIndex(idx))
		return REDUCE_SELECTIVITY_FACTOR_EQUALITY;

	// Hash indexes provide excellent equality selectivity
	return REDUCE_SELECTIVITY_FACTOR_EQUALITY * HASH_EQUALITY_SELECTIVITY_BOOST;
}

double HashIndexCostModel::estimateLoadFactor(const index_desc* idx, double cardinality)
{
	if (!isHashIndex(idx))
		return 0.75;

	ULONG bucket_count = extractBucketCount(idx);
	if (bucket_count == 0)
		bucket_count = HASH_DEFAULT_BUCKETS;

	double load_factor = cardinality / static_cast<double>(bucket_count);
	return MIN(load_factor, 1.0);
}

double HashIndexCostModel::calculateCollisionProbability(double load_factor, ULONG bucket_count)
{
	if (bucket_count == 0 || load_factor <= 0.0)
		return 0.0;

	// Using birthday paradox approximation for collision probability
	double expected_collisions = load_factor * load_factor * 0.5;
	return MIN(expected_collisions, 1.0);
}

double HashIndexCostModel::calculateExpectedCollisions(double load_factor, ULONG bucket_count, double cardinality)
{
	if (load_factor <= HASH_OPTIMAL_LOAD_FACTOR)
		return cardinality * 0.01; // Very few collisions

	// Expected collisions increases quadratically with load factor
	double excess_load = load_factor - HASH_OPTIMAL_LOAD_FACTOR;
	return cardinality * excess_load * excess_load;
}

bool HashIndexCostModel::isHashBetterThanBTree(const index_desc* hash_idx, const index_desc* btree_idx,
												ScanType scan_type, double selectivity, double cardinality)
{
	if (!isHashIndex(hash_idx) || isHashIndex(btree_idx))
		return false;

	double hash_cost = calculateIndexScanCost(hash_idx, selectivity, cardinality, scan_type);
	double btree_cost = DEFAULT_INDEX_COST * selectivity * cardinality;

	// Hash is better for equality scans, worse for range scans
	switch (scan_type)
	{
		case SCAN_EQUALITY:
			return hash_cost < btree_cost * 0.8; // Hash should be significantly better
			
		case SCAN_RANGE:
		case SCAN_STARTING:
			return false; // B-Tree is almost always better for ranges
			
		case SCAN_FULL:
			// Depends on load factor and table size
			return hash_cost < btree_cost * 1.2;
			
		default:
			return hash_cost < btree_cost;
	}
}

double HashIndexCostModel::getRelativeCostToBTree(const index_desc* hash_idx, ScanType scan_type, 
												  double selectivity, double cardinality)
{
	if (!isHashIndex(hash_idx))
		return 1.0;

	double hash_cost = calculateIndexScanCost(hash_idx, selectivity, cardinality, scan_type);
	double btree_cost = DEFAULT_INDEX_COST * selectivity * cardinality;

	return (btree_cost > 0.0) ? (hash_cost / btree_cost) : 1.0;
}

bool HashIndexCostModel::isSuitableForEquality(const index_desc* idx)
{
	return isHashIndex(idx);
}

bool HashIndexCostModel::isSuitableForRange(const index_desc* idx)
{
	// Hash indexes are not suitable for range operations
	return false;
}

bool HashIndexCostModel::isSuitableForSort(const index_desc* idx)
{
	// Hash indexes do not maintain sort order
	return false;
}

bool HashIndexCostModel::isSuitableForGroupBy(const index_desc* idx)
{
	// Hash indexes can be useful for GROUP BY on equality columns
	return isHashIndex(idx);
}

double HashIndexCostModel::predictPerformanceGain(const index_desc* idx, ScanType scan_type, 
												  double expected_cardinality)
{
	if (!isHashIndex(idx))
		return 1.0;

	switch (scan_type)
	{
		case SCAN_EQUALITY:
			// Significant performance gain for equality
			return 0.3; // 70% improvement over B-Tree
			
		case SCAN_RANGE:
			// Performance loss for ranges
			return 3.0; // 200% slower than B-Tree
			
		case SCAN_FULL:
			// Marginal performance difference
			return 1.1; // 10% slower than B-Tree
			
		default:
			return 1.0;
	}
}

double HashIndexCostModel::calculateWorstCasePerformance(const index_desc* idx, double cardinality)
{
	if (!isHashIndex(idx))
		return DEFAULT_INDEX_COST * cardinality;

	// Worst case: all keys hash to same bucket (linear search)
	return DEFAULT_HASH_INDEX_COST * cardinality * cardinality * 0.5;
}

double HashIndexCostModel::calculateBestCasePerformance(const index_desc* idx)
{
	if (!isHashIndex(idx))
		return DEFAULT_INDEX_COST;

	// Best case: perfect hash distribution, O(1) lookup
	return DEFAULT_HASH_INDEX_COST * HASH_EQUALITY_COST_FACTOR;
}

double HashIndexCostModel::calculateOptimalBucketCount(double expected_cardinality)
{
	// Optimal bucket count is typically 1.3x expected cardinality
	return expected_cardinality * 1.3;
}

double HashIndexCostModel::calculateRecommendedLoadFactor(ScanType primary_scan_type)
{
	switch (primary_scan_type)
	{
		case SCAN_EQUALITY:
			return 0.75; // Optimal for equality lookups
			
		case SCAN_FULL:
			return 0.60; // Lower load factor for scan operations
			
		case SCAN_RANGE:
			return 0.50; // Even lower for poor range performance
			
		default:
			return HASH_OPTIMAL_LOAD_FACTOR;
	}
}

bool HashIndexCostModel::recommendHashExpansion(const index_desc* idx, double current_load_factor)
{
	// Recommend expansion if load factor exceeds optimal threshold
	return current_load_factor > (HASH_OPTIMAL_LOAD_FACTOR * 1.2);
}

//----------------------------
// Private Helper Methods
//----------------------------

double HashIndexCostModel::calculateBaseCost(ScanType scan_type)
{
	switch (scan_type)
	{
		case SCAN_EQUALITY:
			return DEFAULT_HASH_INDEX_COST * HASH_EQUALITY_COST_FACTOR;
			
		case SCAN_RANGE:
		case SCAN_STARTING:
			return DEFAULT_HASH_INDEX_COST * HASH_RANGE_COST_FACTOR;
			
		case SCAN_FULL:
			return DEFAULT_HASH_INDEX_COST * HASH_SCAN_COST_FACTOR;
			
		default:
			return DEFAULT_HASH_INDEX_COST;
	}
}

double HashIndexCostModel::applyLoadFactorPenalty(double base_cost, double load_factor)
{
	if (load_factor <= HASH_OPTIMAL_LOAD_FACTOR)
		return base_cost;

	double penalty = (load_factor - HASH_OPTIMAL_LOAD_FACTOR) * 2.0;
	return base_cost * (1.0 + penalty);
}

double HashIndexCostModel::applyCollisionPenalty(double base_cost, double collision_rate)
{
	return base_cost * (1.0 + collision_rate * HASH_COLLISION_PENALTY);
}

double HashIndexCostModel::calculateScanEfficiency(const index_desc* idx, ScanType scan_type)
{
	if (!isHashIndex(idx))
		return 1.0;

	switch (scan_type)
	{
		case SCAN_EQUALITY:
			return 0.95; // Very efficient for equality
			
		case SCAN_RANGE:
		case SCAN_STARTING:
			return 0.1;  // Very inefficient for ranges
			
		case SCAN_FULL:
			return 0.7;  // Moderately efficient for full scans
			
		default:
			return 0.5;
	}
}

double HashIndexCostModel::estimateAverageChainLength(double load_factor)
{
	if (load_factor <= 0.0)
		return 1.0;

	// For chaining collision resolution, average chain length ≈ 1 + λ/2
	return 1.0 + (load_factor * 0.5);
}

double HashIndexCostModel::calculateProbeDistance(double load_factor, CollisionStrategy strategy)
{
	switch (strategy)
	{
		case COLLISION_CHAINING:
			return estimateAverageChainLength(load_factor);
			
		case COLLISION_LINEAR_PROBING:
			// Linear probing average distance ≈ (1 + 1/(1-λ)) / 2
			if (load_factor >= 1.0)
				return 10.0; // Very high cost when full
			return (1.0 + 1.0/(1.0 - load_factor)) * 0.5;
			
		case COLLISION_QUADRATIC_PROBING:
			// Quadratic probing has better clustering properties
			return calculateProbeDistance(load_factor, COLLISION_LINEAR_PROBING) * 0.8;
			
		case COLLISION_DOUBLE_HASHING:
			// Double hashing has best probe distance
			return calculateProbeDistance(load_factor, COLLISION_LINEAR_PROBING) * 0.6;
			
		default:
			return estimateAverageChainLength(load_factor);
	}
}

double HashIndexCostModel::calculateMemoryAccessCost(ULONG bucket_count, double locality_factor)
{
	// Cost increases with bucket count due to cache misses
	double base_cost = 1.0;
	
	if (bucket_count > 1000)
		base_cost += (bucket_count - 1000) * 0.0001; // Cache miss penalty
	
	return base_cost * locality_factor;
}

ULONG HashIndexCostModel::extractBucketCount(const index_desc* idx)
{
	// Extract bucket count from index metadata
	// This would need to be implemented based on actual index structure
	if (!isHashIndex(idx))
		return 0;

	// For now, return default bucket count
	// In actual implementation, this would read from idx->idx_hash_buckets or similar
	return HASH_DEFAULT_BUCKETS;
}

CollisionStrategy HashIndexCostModel::extractCollisionStrategy(const index_desc* idx)
{
	// Extract collision strategy from index metadata
	// This would need to be implemented based on actual index structure
	if (!isHashIndex(idx))
		return COLLISION_CHAINING;

	// For now, return default strategy
	// In actual implementation, this would read from idx->idx_collision_strategy or similar
	return COLLISION_CHAINING;
}

double HashIndexCostModel::extractCurrentLoadFactor(const index_desc* idx)
{
	// Extract current load factor from index statistics
	// This would need to be implemented based on actual index structure
	if (!isHashIndex(idx))
		return 0.75;

	// For now, return default load factor
	// In actual implementation, this would read from idx->idx_load_factor or similar
	return HASH_OPTIMAL_LOAD_FACTOR;
}

//----------------------------
// HashInversionCandidateAnalyzer Implementation
//----------------------------

HashInversionCandidateAnalyzer::HashInversionCandidateAnalyzer(Optimizer* optimizer)
	: m_optimizer(optimizer)
{
}

void HashInversionCandidateAnalyzer::analyzeHashCandidate(InversionCandidate* candidate, 
														  const index_desc* idx, 
														  ScanType scan_type, double cardinality)
{
	if (!isHashIndex(idx) || !candidate)
		return;

	// Calculate hash-specific cost and selectivity
	candidate->cost = HashIndexCostModel::calculateIndexScanCost(idx, candidate->selectivity, cardinality, scan_type);
	
	// Adjust selectivity for hash characteristics
	candidate->selectivity = HashIndexCostModel::calculateHashSelectivity(idx, scan_type, candidate->selectivity);
	
	// Mark if this is particularly suitable for the scan type
	if (scan_type == SCAN_EQUALITY && HashIndexCostModel::isSuitableForEquality(idx))
	{
		candidate->cost *= 0.8; // Boost for optimal usage
	}
	else if (scan_type == SCAN_RANGE || scan_type == SCAN_STARTING)
	{
		candidate->cost *= 2.5; // Penalty for poor usage
	}
}

void HashInversionCandidateAnalyzer::compareWithBTreeCandidate(InversionCandidate* hash_candidate, 
															   InversionCandidate* btree_candidate)
{
	if (!hash_candidate || !btree_candidate)
		return;

	// If hash candidate is significantly worse, mark it as less preferred
	if (hash_candidate->cost > btree_candidate->cost * 1.5)
	{
		hash_candidate->cost *= 1.2; // Additional penalty
	}
	// If hash candidate is significantly better, give it a boost
	else if (hash_candidate->cost < btree_candidate->cost * 0.7)
	{
		hash_candidate->cost *= 0.9; // Additional bonus
	}
}

void HashInversionCandidateAnalyzer::adjustCostForQueryPattern(InversionCandidate* candidate, 
															   const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	if (!candidate)
		return;

	ULONG equality_count = 0, range_count = 0, scan_count = 0;
	analyzeConditionTypes(conditions, equality_count, range_count, scan_count);

	// Adjust cost based on query pattern
	if (equality_count > range_count + scan_count)
	{
		// Equality-heavy workload - hash index advantage
		candidate->cost *= 0.8;
	}
	else if (range_count > equality_count)
	{
		// Range-heavy workload - hash index disadvantage
		candidate->cost *= 1.8;
	}
}

void HashInversionCandidateAnalyzer::applyEqualityOptimization(InversionCandidate* candidate)
{
	if (!candidate)
		return;

	// Hash indexes excel at equality operations
	candidate->cost *= HASH_EQUALITY_COST_FACTOR;
	candidate->selectivity *= HASH_EQUALITY_SELECTIVITY_BOOST;
}

void HashInversionCandidateAnalyzer::applyRangeScanPenalty(InversionCandidate* candidate, double range_width)
{
	if (!candidate)
		return;

	// Heavy penalty for range operations on hash indexes
	candidate->cost *= HASH_RANGE_COST_FACTOR * (1.0 + range_width);
	candidate->selectivity *= HASH_RANGE_SELECTIVITY_PENALTY;
}

void HashInversionCandidateAnalyzer::applySortingPenalty(InversionCandidate* candidate)
{
	if (!candidate)
		return;

	// Hash indexes provide no sort order - heavy penalty
	candidate->cost *= 5.0;
}

void HashInversionCandidateAnalyzer::applyGroupByOptimization(InversionCandidate* candidate, bool has_group_by)
{
	if (!candidate || !has_group_by)
		return;

	// Hash indexes can be useful for GROUP BY on equality columns
	candidate->cost *= 0.9;
}

double HashInversionCandidateAnalyzer::predictExecutionTime(InversionCandidate* candidate, double expected_rows)
{
	if (!candidate)
		return 1000.0; // Default high cost

	// Simple execution time prediction based on cost
	return candidate->cost * expected_rows * 0.001; // Convert to milliseconds
}

double HashInversionCandidateAnalyzer::calculateIOCost(InversionCandidate* candidate)
{
	if (!candidate)
		return 100.0;

	// Hash indexes typically have lower I/O cost for equality lookups
	return candidate->cost * 0.6;
}

double HashInversionCandidateAnalyzer::calculateCPUCost(InversionCandidate* candidate)
{
	if (!candidate)
		return 50.0;

	// Hash computation and comparison costs
	return candidate->cost * 0.4;
}

double HashInversionCandidateAnalyzer::calculateMemoryCost(InversionCandidate* candidate)
{
	if (!candidate)
		return 10.0;

	// Memory cost for hash table structures
	return candidate->cost * 0.1;
}

bool HashInversionCandidateAnalyzer::isEqualityHeavyWorkload(const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	ULONG equality_count = 0, range_count = 0, scan_count = 0;
	analyzeConditionTypes(conditions, equality_count, range_count, scan_count);

	return equality_count > (range_count + scan_count);
}

bool HashInversionCandidateAnalyzer::isRangeHeavyWorkload(const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	ULONG equality_count = 0, range_count = 0, scan_count = 0;
	analyzeConditionTypes(conditions, equality_count, range_count, scan_count);

	return range_count > equality_count;
}

bool HashInversionCandidateAnalyzer::isScanHeavyWorkload(const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	ULONG equality_count = 0, range_count = 0, scan_count = 0;
	analyzeConditionTypes(conditions, equality_count, range_count, scan_count);

	return scan_count > (equality_count + range_count);
}

double HashInversionCandidateAnalyzer::calculateConditionSelectivity(const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	if (!conditions.hasData())
		return MAXIMUM_SELECTIVITY;

	double total_selectivity = MAXIMUM_SELECTIVITY;

	for (const auto& condition : conditions)
	{
		total_selectivity *= Optimizer::getSelectivity(condition);
	}

	return total_selectivity;
}

void HashInversionCandidateAnalyzer::analyzeConditionTypes(const ScratchBird::Stack<BoolExprNode*>& conditions, 
														   ULONG& equality_count, ULONG& range_count, ULONG& scan_count)
{
	equality_count = range_count = scan_count = 0;

	for (const auto& condition : conditions)
	{
		if (const auto cmpNode = nodeAs<ComparativeBoolNode>(condition))
		{
			switch (cmpNode->blrOp)
			{
				case blr_eql:
				case blr_equiv:
					equality_count++;
					break;

				case blr_gtr:
				case blr_geq:
				case blr_lss:
				case blr_leq:
				case blr_between:
					range_count++;
					break;

				case blr_starting:
					range_count++; // STARTING WITH is a range operation
					break;

				default:
					scan_count++;
					break;
			}
		}
		else
		{
			scan_count++; // Other condition types require scanning
		}
	}
}

double HashInversionCandidateAnalyzer::calculateCompositeCost(InversionCandidate* candidate)
{
	if (!candidate)
		return DEFAULT_HASH_INDEX_COST;

	double io_cost = calculateIOCost(candidate);
	double cpu_cost = calculateCPUCost(candidate);
	double memory_cost = calculateMemoryCost(candidate);

	return io_cost + cpu_cost + memory_cost;
}

void HashInversionCandidateAnalyzer::adjustForMultipleConditions(InversionCandidate* candidate, ULONG condition_count)
{
	if (!candidate || condition_count <= 1)
		return;

	// Multiple conditions may reduce effectiveness of hash index
	double penalty = 1.0 + (condition_count - 1) * 0.1;
	candidate->cost *= penalty;
}

//----------------------------
// HashOptimizerIntegration Implementation
//----------------------------

void HashOptimizerIntegration::enhanceIndexScratchForHash(IndexScratch& scratch, const index_desc* idx)
{
	if (!isHashIndex(idx))
		return;

	// Adjust cardinality calculation for hash indexes
	// Hash indexes have different page structure than B-Tree
	double hash_factor = 0.8; // Hash pages typically more densely packed
	scratch.cardinality *= hash_factor;

	// Mark as hash index candidate
	scratch.candidate = true;
}

void HashOptimizerIntegration::adjustInversionCandidateForHash(InversionCandidate* candidate, 
															   const index_desc* idx,
															   ScanType scan_type, double cardinality)
{
	if (!isHashIndex(idx) || !candidate)
		return;

	// Use hash-specific cost calculation
	candidate->cost = HashIndexCostModel::calculateIndexScanCost(idx, candidate->selectivity, cardinality, scan_type);
	candidate->selectivity = HashIndexCostModel::calculateHashSelectivity(idx, scan_type, candidate->selectivity);
}

bool HashOptimizerIntegration::shouldPreferHashOverBTree(const index_desc* hash_idx, 
														 const index_desc* btree_idx,
														 const ScratchBird::Stack<BoolExprNode*>& conditions, 
														 double cardinality)
{
	if (!isHashIndex(hash_idx) || isHashIndex(btree_idx))
		return false;

	// Analyze condition types
	ULONG equality_count = 0, range_count = 0, scan_count = 0;
	for (const auto& condition : conditions)
	{
		if (const auto cmpNode = nodeAs<ComparativeBoolNode>(condition))
		{
			switch (cmpNode->blrOp)
			{
				case blr_eql:
				case blr_equiv:
					equality_count++;
					break;
				case blr_gtr:
				case blr_geq:
				case blr_lss:
				case blr_leq:
				case blr_between:
					range_count++;
					break;
				default:
					scan_count++;
					break;
			}
		}
	}

	// Prefer hash for equality-heavy workloads
	return (equality_count > 0 && range_count == 0);
}

double HashOptimizerIntegration::calculateHashIndexCost(const index_desc* idx, double selectivity, 
														double cardinality, ScanType scan_type)
{
	return HashIndexCostModel::calculateIndexScanCost(idx, selectivity, cardinality, scan_type);
}

double HashOptimizerIntegration::calculateHashSelectivityAdjustment(const index_desc* idx, 
																	ScanType scan_type,
																	double base_selectivity)
{
	return HashIndexCostModel::calculateHashSelectivity(idx, scan_type, base_selectivity);
}

void HashOptimizerIntegration::optimizeHashIndexPlan(InversionCandidate* candidate, 
													 const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	if (!candidate)
		return;

	HashInversionCandidateAnalyzer analyzer(nullptr);
	analyzer.adjustCostForQueryPattern(candidate, conditions);
}

void HashOptimizerIntegration::generateHashIndexHints(const index_desc* idx, ScratchBird::string& plan_hints)
{
	if (!isHashIndex(idx))
		return;

	plan_hints += " [HASH INDEX - optimize for equality conditions]";
}

void HashOptimizerIntegration::collectHashIndexStatistics(const index_desc* idx, double execution_time,
														   ULONG records_retrieved, ULONG buckets_scanned)
{
	// Implementation would update index statistics for future cost calculations
	// This would typically update database metadata tables
}

void HashOptimizerIntegration::updateHashIndexSelectivity(index_desc* idx, double observed_selectivity)
{
	// Implementation would update index selectivity statistics
	// This would typically update the idx_fraction field or similar
}

bool HashOptimizerIntegration::isHashIndexType(const index_desc* idx)
{
	return isHashIndex(idx);
}

ScanType HashOptimizerIntegration::determineScanType(const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	bool has_equality = false;
	bool has_range = false;

	for (const auto& condition : conditions)
	{
		if (const auto cmpNode = nodeAs<ComparativeBoolNode>(condition))
		{
			switch (cmpNode->blrOp)
			{
				case blr_eql:
				case blr_equiv:
					has_equality = true;
					break;
				case blr_gtr:
				case blr_geq:
				case blr_lss:
				case blr_leq:
				case blr_between:
				case blr_starting:
					has_range = true;
					break;
			}
		}
	}

	if (has_equality && !has_range)
		return SCAN_EQUALITY;
	else if (has_range)
		return SCAN_RANGE;
	else if (conditions.hasData())
		return SCAN_FULL;
	else
		return SCAN_FULL;
}

double HashOptimizerIntegration::calculateQueryComplexityFactor(const ScratchBird::Stack<BoolExprNode*>& conditions)
{
	if (!conditions.hasData())
		return 1.0;

	// More conditions generally increase complexity
	return 1.0 + (conditions.getCount() * 0.1);
}

//----------------------------
// Utility Functions Implementation
//----------------------------

bool isHashIndex(const index_desc* idx)
{
	if (!idx)
		return false;

	// Check if this is a hash index based on index type
	// This would need to check idx->idx_type or similar field
	// For now, we'll use a placeholder implementation
	return (idx->idx_type == IDX_TYPE_HASH);
}

bool isBTreeIndex(const index_desc* idx)
{
	if (!idx)
		return false;

	return (idx->idx_type == IDX_TYPE_BTREE);
}

bool isGinIndex(const index_desc* idx)
{
	if (!idx)
		return false;

	return (idx->idx_type == IDX_TYPE_GIN);
}

double compareCosts(double hash_cost, double btree_cost)
{
	if (btree_cost <= 0.0)
		return (hash_cost <= 0.0) ? 1.0 : 2.0;

	return hash_cost / btree_cost;
}

bool isSignificantCostDifference(double cost1, double cost2, double threshold)
{
	if (cost2 <= 0.0)
		return (cost1 > 0.0);

	double ratio = cost1 / cost2;
	return (ratio < (1.0 - threshold) || ratio > (1.0 + threshold));
}

double calculateCostSavings(double old_cost, double new_cost)
{
	if (old_cost <= 0.0)
		return 0.0;

	return (old_cost - new_cost) / old_cost;
}

double predictHashPerformance(const index_desc* idx, ScanType scan_type, double cardinality)
{
	return HashIndexCostModel::predictPerformanceGain(idx, scan_type, cardinality);
}

double estimateOptimalPerformance(const index_desc* idx, double cardinality)
{
	return HashIndexCostModel::calculateBestCasePerformance(idx);
}