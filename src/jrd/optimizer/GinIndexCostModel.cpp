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
 * 2025.07.22 - ScratchBird GIN Index Implementation - Cost Model Implementation
 */

#include "scratchbird.h"
#include "GinIndexCostModel.h"
#include "../GinIndex.h"
#include "../jrd.h"
#include "../exe.h"
#include "../constants.h"
#include "../met_proto.h"
#include "../common/gdsassert.h"
#include <algorithm>
#include <cmath>

using namespace ScratchBird;
using namespace Jrd;

//----------------------------
// GinIndexCostModel Implementation
//----------------------------

GinIndexCostModel::GinIndexCostModel()
{
	// Constructor - initialize any static data if needed
}

GinIndexCostModel::~GinIndexCostModel()
{
	// Destructor - cleanup if needed
}

double GinIndexCostModel::calculateIndexScanCost(const index_desc* idx, GinQueryType query_type, 
												 const TokenList& tokens, double cardinality)
{
	if (!idx || !isGinIndexType(idx))
		return DEFAULT_INDEX_COST * 10; // Very high cost for non-GIN indexes

	double base_cost = calculateBaseCost(query_type);
	
	// Apply token count impact
	base_cost = applyTokenCountPenalty(base_cost, tokens.getCount());
	
	// Apply compression benefits
	double compression_ratio = extractCompressionRatio(idx);
	base_cost = applyCompressionBenefit(base_cost, compression_ratio);
	
	// Factor in cardinality
	double cardinality_factor = std::log(std::max(cardinality, 1.0)) / std::log(10.0);
	base_cost *= (1.0 + cardinality_factor * 0.1);
	
	// Apply cache efficiency
	double cache_efficiency = calculateCacheEfficiency(tokens, 1024); // Assume 1KB cache
	base_cost *= (2.0 - cache_efficiency); // Better cache = lower cost
	
	return std::max(base_cost, 0.1); // Minimum cost
}

double GinIndexCostModel::calculateContainsQueryCost(const index_desc* idx, const TokenList& tokens)
{
	if (tokens.isEmpty())
		return DEFAULT_GIN_INDEX_COST * 10; // High cost for empty token list
		
	double base_cost = DEFAULT_GIN_INDEX_COST * GIN_CONTAINS_COST_FACTOR;
	
	// Cost increases with token count (intersection operations)
	ULONG token_count = tokens.getCount();
	if (token_count > 1)
	{
		// Intersection cost grows with token count
		double intersection_cost = calculatePostingListIntersectionCost(tokens, 1000); // Assume avg 1000 entries
		base_cost += intersection_cost;
	}
	
	// Add token processing cost
	base_cost += calculateTokenProcessingCost(tokens);
	
	// Adjust for posting list sizes
	for (ULONG i = 0; i < token_count; i++)
	{
		// Simplified - in practice would get actual token from list
		ULONG posting_list_size = estimatePostingListSize(idx, "token");
		base_cost += calculatePostingListReadCost(posting_list_size, true);
	}
	
	return base_cost;
}

double GinIndexCostModel::calculateContainsAnyCost(const index_desc* idx, const TokenList& tokens)
{
	if (tokens.isEmpty())
		return DEFAULT_GIN_INDEX_COST * 10;
		
	double base_cost = DEFAULT_GIN_INDEX_COST * GIN_CONTAINS_COST_FACTOR;
	
	// CONTAINS ANY uses union operations - cheaper than intersection
	ULONG token_count = tokens.getCount();
	if (token_count > 1)
	{
		double union_cost = calculatePostingListUnionCost(tokens, 1000);
		base_cost += union_cost * 0.8; // Union is typically cheaper than intersection
	}
	
	base_cost += calculateTokenProcessingCost(tokens);
	
	return base_cost;
}

double GinIndexCostModel::calculateContainsAllCost(const index_desc* idx, const TokenList& tokens)
{
	// CONTAINS ALL is similar to basic CONTAINS but may require additional verification
	double contains_cost = calculateContainsQueryCost(idx, tokens);
	
	// Add verification overhead for ensuring all tokens match
	double verification_cost = tokens.getCount() * GIN_TOKEN_PROCESSING_COST * 0.5;
	
	return contains_cost + verification_cost;
}

double GinIndexCostModel::calculatePhraseQueryCost(const index_desc* idx, const TokenList& tokens, 
												   USHORT proximity_distance)
{
	double base_cost = calculateContainsQueryCost(idx, tokens);
	
	// Phrase queries require position information and proximity checking
	double phrase_overhead = base_cost * GIN_PHRASE_COST_FACTOR;
	
	// Cost increases with proximity distance (larger search window)
	double proximity_factor = 1.0 + (proximity_distance / 10.0);
	phrase_overhead *= proximity_factor;
	
	return base_cost + phrase_overhead;
}

double GinIndexCostModel::calculateSimilarityQueryCost(const index_desc* idx, 
													   const ScratchBird::string& query_text,
													   double similarity_threshold)
{
	// Similarity queries are the most expensive - they may require fuzzy matching
	double base_cost = DEFAULT_GIN_INDEX_COST * GIN_SIMILARITY_COST_FACTOR;
	
	// Cost increases with query length (more tokens to process)
	double length_factor = 1.0 + (query_text.length() / 100.0);
	base_cost *= length_factor;
	
	// Lower similarity thresholds = higher cost (more matches to consider)
	double similarity_factor = 2.0 - similarity_threshold; // threshold 0.5 = factor 1.5, threshold 0.9 = factor 1.1
	base_cost *= similarity_factor;
	
	return base_cost;
}

double GinIndexCostModel::calculateFullScanCost(const index_desc* idx, double cardinality)
{
	// Full scan of GIN index - very expensive
	double base_cost = DEFAULT_GIN_INDEX_COST * GIN_FULL_SCAN_COST_FACTOR;
	
	// Linear with cardinality
	base_cost *= std::max(cardinality / 1000.0, 1.0);
	
	return base_cost;
}

double GinIndexCostModel::calculateGinSelectivity(const index_desc* idx, GinQueryType query_type, 
												  const TokenList& tokens)
{
	if (tokens.isEmpty())
		return MAXIMUM_SELECTIVITY; // No filtering
		
	double selectivity = 1.0;
	
	switch (query_type)
	{
		case GIN_QUERY_CONTAINS:
		case GIN_QUERY_CONTAINS_ALL:
			// Intersection of tokens - very selective
			selectivity = calculateMultiTokenSelectivity(idx, tokens, query_type);
			break;
			
		case GIN_QUERY_CONTAINS_ANY:
			// Union of tokens - less selective
			selectivity = calculateMultiTokenSelectivity(idx, tokens, query_type);
			selectivity = std::min(selectivity * 3.0, MAXIMUM_SELECTIVITY); // ANY is less selective
			break;
			
		case GIN_QUERY_PHRASE:
			// Phrase queries are highly selective
			selectivity = calculateMultiTokenSelectivity(idx, tokens, query_type);
			selectivity *= GIN_PHRASE_SELECTIVITY_BOOST;
			break;
			
		case GIN_QUERY_SIMILAR:
			// Similarity queries are less selective
			selectivity = GIN_SIMILARITY_SELECTIVITY;
			break;
			
		default:
			selectivity = DEFAULT_SELECTIVITY;
			break;
	}
	
	return std::max(selectivity, 1.0 / 1000000.0); // Minimum selectivity
}

double GinIndexCostModel::calculateTokenSelectivity(const index_desc* idx, const ScratchBird::string& token)
{
	// Simplified selectivity calculation
	// In practice, this would use index statistics
	
	if (token.length() <= 2)
		return 0.1; // Short tokens are common
	else if (token.length() <= 4)
		return 0.05;
	else if (token.length() <= 8)
		return 0.01;
	else
		return 0.001; // Long tokens are very selective
}

double GinIndexCostModel::calculateMultiTokenSelectivity(const index_desc* idx, const TokenList& tokens,
														 GinQueryType query_type)
{
	if (tokens.isEmpty())
		return MAXIMUM_SELECTIVITY;
		
	double combined_selectivity = 1.0;
	ULONG token_count = tokens.getCount();
	
	if (query_type == GIN_QUERY_CONTAINS_ANY)
	{
		// Union - less selective than intersection
		for (ULONG i = 0; i < token_count; i++)
		{
			// Simplified - would extract actual token in practice
			double token_selectivity = calculateTokenSelectivity(idx, "token");
			combined_selectivity = combined_selectivity + token_selectivity - 
								   (combined_selectivity * token_selectivity);
		}
	}
	else
	{
		// Intersection - highly selective
		for (ULONG i = 0; i < token_count; i++)
		{
			double token_selectivity = calculateTokenSelectivity(idx, "token");
			combined_selectivity *= token_selectivity;
		}
	}
	
	return std::max(combined_selectivity, GIN_MULTIPLE_TOKEN_SELECTIVITY);
}

bool GinIndexCostModel::isGinBetterThanBTree(const index_desc* gin_idx, const index_desc* btree_idx,
											 GinQueryType query_type, const TokenList& tokens, 
											 double cardinality)
{
	if (!isGinIndexType(gin_idx))
		return false;
		
	// GIN is better for full-text searches
	if (query_type == GIN_QUERY_CONTAINS || query_type == GIN_QUERY_CONTAINS_ANY ||
		query_type == GIN_QUERY_CONTAINS_ALL || query_type == GIN_QUERY_PHRASE ||
		query_type == GIN_QUERY_SIMILAR)
	{
		return true; // GIN excels at text search
	}
	
	// For other query types, compare costs
	double gin_cost = calculateIndexScanCost(gin_idx, query_type, tokens, cardinality);
	double btree_cost = DEFAULT_INDEX_COST; // Assume standard B-tree cost
	
	return gin_cost < btree_cost * 1.2; // 20% threshold
}

bool GinIndexCostModel::isSuitableForFullText(const index_desc* idx)
{
	return isGinIndexType(idx);
}

bool GinIndexCostModel::isSuitableForTokenSearch(const index_desc* idx, const TokenList& tokens)
{
	if (!isGinIndexType(idx))
		return false;
		
	// GIN is suitable if we have tokens to search for
	return !tokens.isEmpty() && tokens.getCount() <= 50; // Reasonable upper limit
}

GinExecutionStrategy GinIndexCostModel::recommendExecutionStrategy(const TokenList& tokens, 
																   double cardinality)
{
	ULONG token_count = tokens.getCount();
	
	if (token_count == 0)
		return GIN_STRATEGY_BITMAP; // Default
	else if (token_count == 1)
		return GIN_STRATEGY_BITMAP; // Single token - bitmap is efficient
	else if (token_count <= 3)
		return GIN_STRATEGY_SORTED_SCAN; // Few tokens - sorted scan
	else if (cardinality < 10000)
		return GIN_STRATEGY_HYBRID; // Small dataset - hybrid approach
	else
		return GIN_STRATEGY_PARALLEL; // Large dataset - consider parallel
}

//----------------------------
// Private helper methods
//----------------------------

double GinIndexCostModel::calculateBaseCost(GinQueryType query_type)
{
	switch (query_type)
	{
		case GIN_QUERY_CONTAINS:
		case GIN_QUERY_CONTAINS_ALL:
			return DEFAULT_GIN_INDEX_COST * GIN_CONTAINS_COST_FACTOR;
			
		case GIN_QUERY_CONTAINS_ANY:
			return DEFAULT_GIN_INDEX_COST * GIN_CONTAINS_COST_FACTOR * 0.8; // Slightly cheaper
			
		case GIN_QUERY_PHRASE:
			return DEFAULT_GIN_INDEX_COST * GIN_PHRASE_COST_FACTOR;
			
		case GIN_QUERY_SIMILAR:
			return DEFAULT_GIN_INDEX_COST * GIN_SIMILARITY_COST_FACTOR;
			
		case GIN_QUERY_SCAN:
			return DEFAULT_GIN_INDEX_COST * GIN_FULL_SCAN_COST_FACTOR;
			
		default:
			return DEFAULT_GIN_INDEX_COST;
	}
}

double GinIndexCostModel::applyTokenCountPenalty(double base_cost, ULONG token_count)
{
	if (token_count <= 1)
		return base_cost;
		
	// Cost increases sub-linearly with token count
	double penalty = 1.0 + std::log(static_cast<double>(token_count)) * 0.2;
	return base_cost * penalty;
}

double GinIndexCostModel::applyCompressionBenefit(double base_cost, double compression_ratio)
{
	// Better compression = lower I/O cost
	double benefit = 1.0 / std::max(compression_ratio, 1.0);
	return base_cost * (0.7 + 0.3 * benefit); // 30% max benefit from compression
}

double GinIndexCostModel::calculateTokenProcessingCost(const TokenList& tokens)
{
	return tokens.getCount() * GIN_TOKEN_PROCESSING_COST;
}

double GinIndexCostModel::calculatePostingListIntersectionCost(const TokenList& tokens, 
															   double avg_posting_list_size)
{
	ULONG token_count = tokens.getCount();
	if (token_count <= 1)
		return 0.0;
		
	// Intersection cost grows with token count and list sizes
	double cost = 0.0;
	for (ULONG i = 1; i < token_count; i++)
	{
		cost += calculateTokenIntersectionCost(static_cast<ULONG>(avg_posting_list_size), 
											   static_cast<ULONG>(avg_posting_list_size));
	}
	
	return cost;
}

double GinIndexCostModel::calculatePostingListUnionCost(const TokenList& tokens, 
														double avg_posting_list_size)
{
	ULONG token_count = tokens.getCount();
	if (token_count <= 1)
		return 0.0;
		
	// Union cost - typically cheaper than intersection
	double cost = 0.0;
	for (ULONG i = 1; i < token_count; i++)
	{
		cost += calculateTokenUnionCost(static_cast<ULONG>(avg_posting_list_size), 
										static_cast<ULONG>(avg_posting_list_size));
	}
	
	return cost * 0.8; // Union is typically 20% cheaper
}

ULONG GinIndexCostModel::estimatePostingListSize(const index_desc* idx, const ScratchBird::string& token)
{
	// Simplified estimation - in practice would use index statistics
	// Common tokens have larger posting lists
	if (token.length() <= 2)
		return 10000; // Very common
	else if (token.length() <= 4)
		return 5000;  // Common
	else if (token.length() <= 8)
		return 1000;  // Moderate
	else
		return 100;   // Rare
}

double GinIndexCostModel::calculatePostingListReadCost(ULONG posting_list_size, bool compressed)
{
	double base_cost = posting_list_size * GIN_POSTING_LIST_COST;
	
	if (compressed)
	{
		// Add decompression cost but benefit from smaller I/O
		base_cost = base_cost * 0.3 + posting_list_size * GIN_DECOMPRESSION_COST;
	}
	
	return base_cost;
}

double GinIndexCostModel::calculateTokenIntersectionCost(ULONG list1_size, ULONG list2_size)
{
	// Cost of intersecting two posting lists
	ULONG min_size = std::min(list1_size, list2_size);
	return min_size * GIN_BITMAP_OPERATION_COST;
}

double GinIndexCostModel::calculateTokenUnionCost(ULONG list1_size, ULONG list2_size)
{
	// Cost of union operation
	ULONG total_size = list1_size + list2_size;
	return total_size * GIN_BITMAP_OPERATION_COST * 0.8; // Union typically cheaper
}

double GinIndexCostModel::calculateCacheEfficiency(const TokenList& tokens, double cache_size)
{
	// Simplified cache efficiency calculation
	// More tokens = lower cache efficiency (more data to cache)
	ULONG token_count = tokens.getCount();
	if (token_count == 0)
		return 1.0;
		
	double efficiency = GIN_CACHE_HIT_RATIO / (1.0 + token_count * 0.1);
	return std::max(efficiency, 0.1); // Minimum 10% efficiency
}

double GinIndexCostModel::extractCompressionRatio(const index_desc* idx)
{
	// Simplified - in practice would extract from index metadata
	return 2.5; // Assume 2.5:1 compression ratio
}

bool GinIndexCostModel::isGinIndexType(const index_desc* idx)
{
	return idx && idx->idx_type == IDX_TYPE_GIN;
}

//----------------------------
// GinInversionCandidateAnalyzer Implementation
//----------------------------

GinInversionCandidateAnalyzer::GinInversionCandidateAnalyzer(Optimizer* optimizer)
	: m_optimizer(optimizer)
{
	fb_assert(optimizer);
}

void GinInversionCandidateAnalyzer::analyzeGinCandidate(InversionCandidate* candidate, 
														const index_desc* idx,
														GinQueryType query_type, 
														const TokenList& tokens, 
														double cardinality)
{
	if (!candidate || !idx || !GinIndexCostModel::isGinIndexType(idx))
		return;
		
	// Calculate GIN-specific cost and selectivity
	candidate->cost = GinIndexCostModel::calculateIndexScanCost(idx, query_type, tokens, cardinality);
	candidate->selectivity = GinIndexCostModel::calculateGinSelectivity(idx, query_type, tokens);
	
	// Apply GIN-specific optimizations
	applyFullTextOptimization(candidate, tokens);
	
	// Adjust for query complexity
	adjustForTokenComplexity(candidate, tokens);
}

void GinInversionCandidateAnalyzer::applyFullTextOptimization(InversionCandidate* candidate, 
															  const TokenList& tokens)
{
	if (!candidate || tokens.isEmpty())
		return;
		
	// Full-text searches are highly selective
	candidate->selectivity *= 0.8; // 20% boost in selectivity
	
	// Multiple tokens make query more selective
	if (tokens.getCount() > 1)
	{
		double multi_token_boost = 1.0 / (1.0 + tokens.getCount() * 0.1);
		candidate->selectivity *= multi_token_boost;
	}
}

bool GinInversionCandidateAnalyzer::isFullTextQuery(const BoolExprNode* condition)
{
	// Simplified check - would examine node type in practice
	return condition != nullptr;
}

GinQueryType GinInversionCandidateAnalyzer::extractQueryType(const BoolExprNode* condition)
{
	// Simplified - would parse the actual condition
	return GIN_QUERY_CONTAINS;
}

TokenList GinInversionCandidateAnalyzer::extractTokensFromCondition(const BoolExprNode* condition)
{
	// Simplified - would extract actual tokens from condition
	TokenList tokens;
	return tokens;
}

void GinInversionCandidateAnalyzer::adjustForTokenComplexity(InversionCandidate* candidate, 
															 const TokenList& tokens)
{
	if (!candidate)
		return;
		
	ULONG token_count = tokens.getCount();
	
	// More tokens = higher complexity but better selectivity
	if (token_count > GIN_OPTIMAL_TOKEN_COUNT)
	{
		double complexity_penalty = 1.0 + (token_count - GIN_OPTIMAL_TOKEN_COUNT) * 0.05;
		candidate->cost *= complexity_penalty;
	}
}

//----------------------------
// Utility Functions Implementation
//----------------------------

bool isGinIndex(const index_desc* idx)
{
	return idx && idx->idx_type == IDX_TYPE_GIN;
}

bool isFullTextSearchCapable(const index_desc* idx)
{
	return isGinIndex(idx);
}

double compareGinCosts(double gin_cost, double btree_cost)
{
	if (btree_cost <= 0.0)
		return gin_cost;
		
	return gin_cost / btree_cost; // Ratio - values < 1.0 favor GIN
}

bool isSignificantGinCostDifference(double cost1, double cost2, double threshold)
{
	if (cost2 <= 0.0)
		return cost1 > 0.0;
		
	double ratio = std::abs(cost1 - cost2) / cost2;
	return ratio > threshold;
}

TokenList tokenizeQueryString(const ScratchBird::string& query_text, GinTokenizer* tokenizer)
{
	TokenList tokens;
	
	if (tokenizer)
	{
		tokens = tokenizer->tokenize(query_text.c_str());
	}
	else
	{
		// Simple fallback tokenization
		// In practice would use proper tokenizer
	}
	
	return tokens;
}

bool isStopWord(const ScratchBird::string& token)
{
	// Simplified stop word detection
	// In practice would check against stop word list
	return token.length() <= 2 || 
		   token == "the" || token == "and" || token == "or" || 
		   token == "but" || token == "in" || token == "on" || token == "at";
}

bool isCommonWord(const ScratchBird::string& token, double frequency_threshold)
{
	// Simplified common word detection
	// In practice would use frequency statistics
	return token.length() <= 3 || isStopWord(token);
}