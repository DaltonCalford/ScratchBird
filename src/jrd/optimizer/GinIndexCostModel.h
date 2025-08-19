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
 * 2025.07.22 - ScratchBird GIN Index Implementation - Cost Model
 */

#ifndef JRD_GIN_INDEX_COST_MODEL_H
#define JRD_GIN_INDEX_COST_MODEL_H

#include "Optimizer.h"
#include "../constants.h"
#include "../GinIndex.h"
#include "../common/classes/fb_string.h"
#include "../lls.h"

namespace Jrd {

// Forward declarations
class GinIndex;
struct index_desc;
class InversionCandidate;
class BoolExprNode;

//----------------------------
// GIN Query Types for Cost Calculations
//----------------------------
enum GinQueryType
{
	GIN_QUERY_CONTAINS = 0,     // Basic CONTAINS query
	GIN_QUERY_CONTAINS_ANY = 1, // CONTAINS ANY (OR semantics)
	GIN_QUERY_CONTAINS_ALL = 2, // CONTAINS ALL (AND semantics)
	GIN_QUERY_PHRASE = 3,       // Phrase/proximity search
	GIN_QUERY_SIMILAR = 4,      // Similarity/fuzzy search
	GIN_QUERY_SCAN = 5          // Full index scan
};

//----------------------------
// GIN Index Cost Constants
//----------------------------

// Base cost constants for GIN indexes
inline constexpr double DEFAULT_GIN_INDEX_COST = 4.0;		// Higher than B-Tree (3.0) and Hash (1.5)
inline constexpr double GIN_CONTAINS_COST_FACTOR = 1.2;	// GIN good at contains
inline constexpr double GIN_PHRASE_COST_FACTOR = 2.0;		// Phrase queries more expensive
inline constexpr double GIN_SIMILARITY_COST_FACTOR = 3.5;	// Similarity most expensive
inline constexpr double GIN_FULL_SCAN_COST_FACTOR = 8.0;	// Full scan very expensive

// Posting list and token processing costs
inline constexpr double GIN_TOKEN_PROCESSING_COST = 0.1;	// Cost per token processed
inline constexpr double GIN_POSTING_LIST_COST = 0.2;		// Cost per posting list read
inline constexpr double GIN_BITMAP_OPERATION_COST = 0.05;	// Cost per bitmap operation
inline constexpr double GIN_DECOMPRESSION_COST = 0.3;		// Cost of decompressing posting lists

// Selectivity factors for GIN indexes
inline constexpr double GIN_SINGLE_TOKEN_SELECTIVITY = 0.01;	// Selectivity for single token
inline constexpr double GIN_MULTIPLE_TOKEN_SELECTIVITY = 0.001;	// Selectivity for multiple tokens
inline constexpr double GIN_PHRASE_SELECTIVITY_BOOST = 0.1;	// Phrase queries more selective
inline constexpr double GIN_SIMILARITY_SELECTIVITY = 0.05;	// Similarity less selective

// Performance tuning constants
inline constexpr double GIN_CACHE_HIT_RATIO = 0.8;			// Expected cache hit ratio
inline constexpr double GIN_PARALLEL_PROCESSING_FACTOR = 0.7;	// Benefit of parallel processing
inline constexpr ULONG GIN_OPTIMAL_TOKEN_COUNT = 3;		// Optimal number of tokens for performance

//----------------------------
// GinIndexCostModel - Cost calculation for GIN indexes
//----------------------------
class GinIndexCostModel
{
public:
	// Constructor
	GinIndexCostModel();

	// Destructor
	~GinIndexCostModel();

	// Primary cost calculation methods
	static double calculateIndexScanCost(const index_desc* idx, GinQueryType query_type, 
										 const TokenList& tokens, double cardinality);
	static double calculateContainsQueryCost(const index_desc* idx, const TokenList& tokens);
	static double calculateContainsAnyCost(const index_desc* idx, const TokenList& tokens);
	static double calculateContainsAllCost(const index_desc* idx, const TokenList& tokens);
	static double calculatePhraseQueryCost(const index_desc* idx, const TokenList& tokens, 
											USHORT proximity_distance);
	static double calculateSimilarityQueryCost(const index_desc* idx, const ScratchBird::string& query_text,
												double similarity_threshold);
	static double calculateFullScanCost(const index_desc* idx, double cardinality);

	// Selectivity calculation methods
	static double calculateGinSelectivity(const index_desc* idx, GinQueryType query_type, 
										  const TokenList& tokens);
	static double calculateTokenSelectivity(const index_desc* idx, const ScratchBird::string& token);
	static double calculateMultiTokenSelectivity(const index_desc* idx, const TokenList& tokens,
												  GinQueryType query_type);
	static double adjustSelectivityForTokenFrequency(double base_selectivity, 
													 const TokenList& tokens);

	// Token and posting list analysis
	static ULONG estimatePostingListSize(const index_desc* idx, const ScratchBird::string& token);
	static double calculateTokenProcessingCost(const TokenList& tokens);
	static double calculatePostingListIntersectionCost(const TokenList& tokens, 
														double avg_posting_list_size);
	static double calculatePostingListUnionCost(const TokenList& tokens, 
												 double avg_posting_list_size);

	// Cost comparison methods
	static bool isGinBetterThanBTree(const index_desc* gin_idx, const index_desc* btree_idx,
									 GinQueryType query_type, const TokenList& tokens, double cardinality);
	static double getRelativeCostToBTree(const index_desc* gin_idx, GinQueryType query_type, 
										 const TokenList& tokens, double cardinality);
	static bool isGinBetterThanFullScan(const index_desc* gin_idx, GinQueryType query_type,
										const TokenList& tokens, double table_cardinality);

	// Index suitability analysis
	static bool isSuitableForFullText(const index_desc* idx);
	static bool isSuitableForTokenSearch(const index_desc* idx, const TokenList& tokens);
	static bool isSuitableForPhraseSearch(const index_desc* idx);
	static bool isSuitableForSimilaritySearch(const index_desc* idx);

	// Performance prediction methods
	static double predictQueryPerformance(const index_desc* idx, GinQueryType query_type, 
										  const TokenList& tokens, double expected_cardinality);
	static double calculateWorstCasePerformance(const index_desc* idx, GinQueryType query_type,
												double cardinality);
	static double calculateBestCasePerformance(const index_desc* idx, GinQueryType query_type,
											   const TokenList& tokens);

	// GIN-specific optimization hints
	static ULONG calculateOptimalTokenCacheSize(double expected_query_frequency);
	static double calculateRecommendedCompressionLevel(double posting_list_size);
	static bool recommendParallelProcessing(const TokenList& tokens, double cardinality);
	static GinExecutionStrategy recommendExecutionStrategy(const TokenList& tokens, double cardinality);

private:
	// Internal calculation helpers
	static double calculateBaseCost(GinQueryType query_type);
	static double applyTokenCountPenalty(double base_cost, ULONG token_count);
	static double applyCompressionBenefit(double base_cost, double compression_ratio);
	static double calculateQueryComplexity(GinQueryType query_type, const TokenList& tokens);

	// Posting list processing costs
	static double calculatePostingListReadCost(ULONG posting_list_size, bool compressed);
	static double calculateBitmapOperationCost(ULONG bitmap_size, GinQueryType query_type);
	static double calculateTokenIntersectionCost(ULONG list1_size, ULONG list2_size);
	static double calculateTokenUnionCost(ULONG list1_size, ULONG list2_size);

	// Cache and memory analysis
	static double calculateCacheEfficiency(const TokenList& tokens, double cache_size);
	static double calculateMemoryAccessCost(ULONG data_size, double locality_factor = 0.6);
	static double applyParallelProcessingBenefit(double base_cost, const TokenList& tokens);

	// Index metadata analysis
	static ULONG extractPostingListCount(const index_desc* idx);
	static double extractCompressionRatio(const index_desc* idx);
	static double extractAverageTokenLength(const index_desc* idx);
	static ULONG extractUniqueTokenCount(const index_desc* idx);
};

//----------------------------
// GinInversionCandidateAnalyzer - Analyze GIN index candidates for optimizer
//----------------------------
class GinInversionCandidateAnalyzer
{
public:
	// Constructor
	explicit GinInversionCandidateAnalyzer(Optimizer* optimizer);

	// Main analysis methods
	void analyzeGinCandidate(InversionCandidate* candidate, const index_desc* idx, 
							 GinQueryType query_type, const TokenList& tokens, double cardinality);
	void compareWithBTreeCandidate(InversionCandidate* gin_candidate, 
								   InversionCandidate* btree_candidate);
	void adjustCostForQueryPattern(InversionCandidate* candidate, const BoolExprNode* condition);

	// Cost adjustment methods
	static void applyFullTextOptimization(InversionCandidate* candidate, const TokenList& tokens);
	static void applyPhraseSearchOptimization(InversionCandidate* candidate, USHORT proximity_distance);
	static void applySimilaritySearchPenalty(InversionCandidate* candidate, double similarity_threshold);
	static void applyTokenFrequencyAdjustment(InversionCandidate* candidate, const TokenList& tokens);

	// Performance prediction
	double predictExecutionTime(InversionCandidate* candidate, const TokenList& tokens, 
								double expected_rows);
	double calculateIOCost(InversionCandidate* candidate, const TokenList& tokens);
	double calculateCPUCost(InversionCandidate* candidate, const TokenList& tokens);
	double calculateMemoryCost(InversionCandidate* candidate, const TokenList& tokens);

	// Query pattern analysis
	static bool isFullTextQuery(const BoolExprNode* condition);
	static bool isPhraseQuery(const BoolExprNode* condition);
	static bool isSimilarityQuery(const BoolExprNode* condition);
	static GinQueryType extractQueryType(const BoolExprNode* condition);
	static TokenList extractTokensFromCondition(const BoolExprNode* condition);

private:
	Optimizer* m_optimizer;

	// Internal analysis helpers
	void analyzeTokenDistribution(const TokenList& tokens, ULONG& common_tokens, 
								  ULONG& rare_tokens, ULONG& stop_words);
	double calculateCompositeCost(InversionCandidate* candidate, const TokenList& tokens);
	void adjustForTokenComplexity(InversionCandidate* candidate, const TokenList& tokens);
	double estimateResultSetSize(const index_desc* idx, const TokenList& tokens, GinQueryType query_type);
};

//----------------------------
// GinOptimizerIntegration - Integration with existing optimizer
//----------------------------
class GinOptimizerIntegration
{
public:
	// Static integration methods
	static void enhanceIndexScratchForGin(IndexScratch& scratch, const index_desc* idx);
	static void adjustInversionCandidateForGin(InversionCandidate* candidate, const index_desc* idx,
											   const BoolExprNode* condition, double cardinality);
	static bool shouldPreferGinOverBTree(const index_desc* gin_idx, const index_desc* btree_idx,
										 const BoolExprNode* condition, double cardinality);

	// Cost model integration
	static double calculateGinIndexCost(const index_desc* idx, const BoolExprNode* condition, 
										double cardinality);
	static double calculateGinSelectivityAdjustment(const index_desc* idx, const BoolExprNode* condition,
													double base_selectivity);

	// Query plan optimization
	static void optimizeGinIndexPlan(InversionCandidate* candidate, const BoolExprNode* condition);
	static void generateGinIndexHints(const index_desc* idx, const BoolExprNode* condition, 
									  ScratchBird::string& plan_hints);

	// Performance monitoring integration
	static void collectGinIndexStatistics(const index_desc* idx, double execution_time,
										  ULONG tokens_processed, ULONG posting_lists_read,
										  ULONG records_retrieved);
	static void updateGinIndexSelectivity(index_desc* idx, const TokenList& tokens, 
										  double observed_selectivity);

	// GIN-specific optimizer enhancements
	static void enableGinParallelProcessing(InversionCandidate* candidate, const TokenList& tokens);
	static void optimizeGinCaching(InversionCandidate* candidate, const TokenList& tokens);
	static void adjustGinCompressionSettings(InversionCandidate* candidate, 
											 const index_desc* idx);

private:
	// Integration helpers
	static bool isGinIndexType(const index_desc* idx);
	static GinQueryType determineGinQueryType(const BoolExprNode* condition);
	static double calculateQueryComplexityFactor(const BoolExprNode* condition);
	static TokenList extractQueryTokens(const BoolExprNode* condition);
	
	// Advanced analysis helpers
	static double analyzeTokenDistribution(const index_desc* idx, const TokenList& tokens);
	static bool shouldUseCompression(const index_desc* idx, const TokenList& tokens);
	static GinExecutionStrategy selectOptimalStrategy(const TokenList& tokens, double cardinality);
};

//----------------------------
// Utility Functions
//----------------------------

// Index type detection  
bool isGinIndex(const index_desc* idx);
bool isFullTextSearchCapable(const index_desc* idx);

// GIN cost comparison utilities
double compareGinCosts(double gin_cost, double btree_cost);
bool isSignificantGinCostDifference(double cost1, double cost2, double threshold = 0.15);
double calculateGinCostSavings(double old_cost, double new_cost);

// GIN performance prediction utilities
double predictGinPerformance(const index_desc* idx, GinQueryType query_type, 
							 const TokenList& tokens, double cardinality);
double estimateOptimalGinPerformance(const index_desc* idx, const TokenList& tokens, 
									 double cardinality);

// Token analysis utilities
TokenList tokenizeQueryString(const ScratchBird::string& query_text, GinTokenizer* tokenizer = nullptr);
double calculateTokenFrequency(const index_desc* idx, const ScratchBird::string& token);
bool isStopWord(const ScratchBird::string& token);
bool isCommonWord(const ScratchBird::string& token, double frequency_threshold = 0.1);

} // namespace Jrd

#endif // JRD_GIN_INDEX_COST_MODEL_H