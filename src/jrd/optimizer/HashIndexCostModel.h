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
 * 2025.07.22 - ScratchBird Hash Index Implementation - Cost Model
 */

#ifndef JRD_HASH_INDEX_COST_MODEL_H
#define JRD_HASH_INDEX_COST_MODEL_H

#include "Optimizer.h"
#include "../constants.h"
#include "../HashIndex.h"
#include "../common/classes/fb_string.h"
#include "../lls.h"

namespace Jrd {

// Forward declarations
class HashIndex;
struct index_desc;
class InversionCandidate;

//----------------------------
// Scan Types for Hash Index Cost Calculations
//----------------------------
enum ScanType
{
	SCAN_EQUALITY = 0,      // Equality lookup (=)
	SCAN_RANGE = 1,         // Range scan (<, >, >=, <=, BETWEEN)
	SCAN_STARTING = 2,      // Starting with pattern matching
	SCAN_FULL = 3           // Full table/index scan
};

//----------------------------
// Hash Index Cost Constants
//----------------------------

// Base cost constants for hash indexes
inline constexpr double DEFAULT_HASH_INDEX_COST = 1.5;		// Lower than B-Tree (3.0)
inline constexpr double HASH_EQUALITY_COST_FACTOR = 0.8;	// Hash excels at equality
inline constexpr double HASH_RANGE_COST_FACTOR = 10.0;		// Hash poor at ranges
inline constexpr double HASH_SCAN_COST_FACTOR = 2.5;		// Hash scan more expensive than B-Tree

// Collision and load factor impact
inline constexpr double HASH_OPTIMAL_LOAD_FACTOR = 0.75;	// Optimal load factor
inline constexpr double HASH_COLLISION_PENALTY = 0.2;		// Cost penalty per collision
inline constexpr double HASH_EXPANSION_COST = 5.0;			// Cost of hash table expansion

// Selectivity factors for hash indexes
inline constexpr double HASH_EQUALITY_SELECTIVITY_BOOST = 0.9;	// Hash very good at equality
inline constexpr double HASH_RANGE_SELECTIVITY_PENALTY = 0.1;	// Hash very poor at ranges

//----------------------------
// HashIndexCostModel - Cost calculation for hash indexes
//----------------------------
class HashIndexCostModel
{
public:
	// Constructor
	HashIndexCostModel();

	// Destructor
	~HashIndexCostModel();

	// Primary cost calculation methods
	static double calculateIndexScanCost(const index_desc* idx, double selectivity, 
										 double cardinality, ScanType scan_type);
	static double calculateEqualityLookupCost(const index_desc* idx, double load_factor = 0.75);
	static double calculateRangeScanCost(const index_desc* idx, double selectivity, double cardinality);
	static double calculateFullScanCost(const index_desc* idx, double cardinality);

	// Selectivity calculation methods
	static double calculateHashSelectivity(const index_desc* idx, ScanType scan_type, 
										   double base_selectivity);
	static double adjustSelectivityForCollisions(double base_selectivity, double load_factor);
	static double calculateEqualitySelectivity(const index_desc* idx);

	// Load factor and collision analysis
	static double estimateLoadFactor(const index_desc* idx, double cardinality);
	static double calculateCollisionProbability(double load_factor, ULONG bucket_count);
	static double calculateExpectedCollisions(double load_factor, ULONG bucket_count, double cardinality);

	// Cost comparison methods
	static bool isHashBetterThanBTree(const index_desc* hash_idx, const index_desc* btree_idx,
									  ScanType scan_type, double selectivity, double cardinality);
	static double getRelativeCostToBTree(const index_desc* hash_idx, ScanType scan_type, 
										 double selectivity, double cardinality);

	// Index suitability analysis
	static bool isSuitableForEquality(const index_desc* idx);
	static bool isSuitableForRange(const index_desc* idx);
	static bool isSuitableForSort(const index_desc* idx);
	static bool isSuitableForGroupBy(const index_desc* idx);

	// Performance prediction methods
	static double predictPerformanceGain(const index_desc* idx, ScanType scan_type, 
										 double expected_cardinality);
	static double calculateWorstCasePerformance(const index_desc* idx, double cardinality);
	static double calculateBestCasePerformance(const index_desc* idx);

	// Hash-specific optimization hints
	static double calculateOptimalBucketCount(double expected_cardinality);
	static double calculateRecommendedLoadFactor(ScanType primary_scan_type);
	static bool recommendHashExpansion(const index_desc* idx, double current_load_factor);

private:
	// Internal calculation helpers
	static double calculateBaseCost(ScanType scan_type);
	static double applyLoadFactorPenalty(double base_cost, double load_factor);
	static double applyCollisionPenalty(double base_cost, double collision_rate);
	static double calculateScanEfficiency(const index_desc* idx, ScanType scan_type);

	// Statistical analysis helpers
	static double estimateAverageChainLength(double load_factor);
	static double calculateProbeDistance(double load_factor, CollisionStrategy strategy);
	static double calculateMemoryAccessCost(ULONG bucket_count, double locality_factor = 0.8);

	// Index metadata analysis
	static ULONG extractBucketCount(const index_desc* idx);
	static CollisionStrategy extractCollisionStrategy(const index_desc* idx);
	static double extractCurrentLoadFactor(const index_desc* idx);
};

//----------------------------
// HashInversionCandidateAnalyzer - Analyze hash index candidates for optimizer
//----------------------------
class HashInversionCandidateAnalyzer
{
public:
	// Constructor
	explicit HashInversionCandidateAnalyzer(Optimizer* optimizer);

	// Main analysis methods
	void analyzeHashCandidate(InversionCandidate* candidate, const index_desc* idx, 
							  ScanType scan_type, double cardinality);
	void compareWithBTreeCandidate(InversionCandidate* hash_candidate, 
								   InversionCandidate* btree_candidate);
	void adjustCostForQueryPattern(InversionCandidate* candidate, const ScratchBird::Stack<BoolExprNode*>& conditions);

	// Cost adjustment methods
	void applyEqualityOptimization(InversionCandidate* candidate);
	void applyRangeScanPenalty(InversionCandidate* candidate, double range_width);
	void applySortingPenalty(InversionCandidate* candidate);
	void applyGroupByOptimization(InversionCandidate* candidate, bool has_group_by);

	// Performance prediction
	double predictExecutionTime(InversionCandidate* candidate, double expected_rows);
	double calculateIOCost(InversionCandidate* candidate);
	double calculateCPUCost(InversionCandidate* candidate);
	double calculateMemoryCost(InversionCandidate* candidate);

	// Query pattern analysis
	bool isEqualityHeavyWorkload(const ScratchBird::Stack<BoolExprNode*>& conditions);
	bool isRangeHeavyWorkload(const ScratchBird::Stack<BoolExprNode*>& conditions);
	bool isScanHeavyWorkload(const ScratchBird::Stack<BoolExprNode*>& conditions);
	double calculateConditionSelectivity(const ScratchBird::Stack<BoolExprNode*>& conditions);

private:
	Optimizer* m_optimizer;

	// Internal analysis helpers
	void analyzeConditionTypes(const ScratchBird::Stack<BoolExprNode*>& conditions, 
							   ULONG& equality_count, ULONG& range_count, ULONG& scan_count);
	double calculateCompositeCost(InversionCandidate* candidate);
	void adjustForMultipleConditions(InversionCandidate* candidate, ULONG condition_count);
};

//----------------------------
// HashOptimizerIntegration - Integration with existing optimizer
//----------------------------
class HashOptimizerIntegration
{
public:
	// Static integration methods
	static void enhanceIndexScratchForHash(IndexScratch& scratch, const index_desc* idx);
	static void adjustInversionCandidateForHash(InversionCandidate* candidate, const index_desc* idx,
												ScanType scan_type, double cardinality);
	static bool shouldPreferHashOverBTree(const index_desc* hash_idx, const index_desc* btree_idx,
										  const ScratchBird::Stack<BoolExprNode*>& conditions, double cardinality);

	// Cost model integration
	static double calculateHashIndexCost(const index_desc* idx, double selectivity, double cardinality,
										 ScanType scan_type);
	static double calculateHashSelectivityAdjustment(const index_desc* idx, ScanType scan_type,
													 double base_selectivity);

	// Query plan optimization
	static void optimizeHashIndexPlan(InversionCandidate* candidate, const ScratchBird::Stack<BoolExprNode*>& conditions);
	static void generateHashIndexHints(const index_desc* idx, ScratchBird::string& plan_hints);

	// Performance monitoring integration
	static void collectHashIndexStatistics(const index_desc* idx, double execution_time,
										   ULONG records_retrieved, ULONG buckets_scanned);
	static void updateHashIndexSelectivity(index_desc* idx, double observed_selectivity);

private:
	// Integration helpers
	static bool isHashIndexType(const index_desc* idx);
	static ScanType determineScanType(const ScratchBird::Stack<BoolExprNode*>& conditions);
	static double calculateQueryComplexityFactor(const ScratchBird::Stack<BoolExprNode*>& conditions);
};

//----------------------------
// Utility Functions
//----------------------------

// Index type detection
bool isHashIndex(const index_desc* idx);
bool isBTreeIndex(const index_desc* idx);
bool isGinIndex(const index_desc* idx);

// Cost comparison utilities
double compareCosts(double hash_cost, double btree_cost);
bool isSignificantCostDifference(double cost1, double cost2, double threshold = 0.1);
double calculateCostSavings(double old_cost, double new_cost);

// Performance prediction utilities
double predictHashPerformance(const index_desc* idx, ScanType scan_type, double cardinality);
double estimateOptimalPerformance(const index_desc* idx, double cardinality);

} // namespace Jrd

#endif // JRD_HASH_INDEX_COST_MODEL_H