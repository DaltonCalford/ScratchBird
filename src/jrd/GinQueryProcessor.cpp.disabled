/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinQueryProcessor.cpp  
 *	DESCRIPTION:	GIN query processor implementation for CONTAINS operations
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
 * 2025.07.22 - ScratchBird GIN Query Processor Implementation
 */

#include "scratchbird.h"
#include "../jrd/GinQueryProcessor.h"
#include "../jrd/GinIndex.h"
#include "../jrd/GinPageManager.h"
#include "../jrd/GinCompression.h"
#include "../jrd/RecordBitmap.h"
#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/constants.h"
#include "../common/gdsassert.h"
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cmath>

using namespace ScratchBird;
using namespace Jrd;
using namespace std::chrono;

namespace Jrd {

//----------------------------
// GinQueryProcessor Implementation
//----------------------------

GinQueryProcessor::GinQueryProcessor(thread_db* tdbb, GinIndex* gin_index)
	: m_tdbb(tdbb),
	  m_gin_index(gin_index),
	  m_page_manager(nullptr),
	  m_strategy(GIN_STRATEGY_HYBRID),
	  m_enable_caching(true),
	  m_max_cache_size(MAX_RESULT_CACHE_SIZE)
{
	fb_assert(tdbb != nullptr);
	fb_assert(gin_index != nullptr);
	
	// Get page manager from GIN index
	m_page_manager = gin_index->getPageManager();
	
	// Initialize statistics
	resetStatistics();
}

GinQueryProcessor::~GinQueryProcessor()
{
	// Clean up temporary bitmaps
	cleanupTempBitmaps();
	
	// Clean up result cache
	clearResultCache();
}

//----------------------------
// Main Query Interface
//----------------------------

bool GinQueryProcessor::executeContainsQuery(const dsc* search_value, RecordBitmap* result_bitmap,
											  GinQueryStats* stats)
{
	fb_assert(search_value != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	startQuery();
	
	try {
		// Extract tokens from search value
		TokenList search_tokens = m_gin_index->extractTokens(search_value);
		
		if (search_tokens.isEmpty()) {
			endQuery();
			if (stats) *stats = m_last_stats;
			return false;
		}
		
		m_last_stats.tokens_processed = search_tokens.getCount();
		
		// Check cache first
		if (m_enable_caching) {
			ScratchBird::string query_key = createQueryKey(search_tokens, GIN_QUERY_CONTAINS);
			RecordBitmap* cached_result = getCachedResult(query_key);
			
			if (cached_result) {
				incrementCacheHit();
				result_bitmap->copy(*cached_result);
				endQuery();
				if (stats) *stats = m_last_stats;
				return true;
			}
			incrementCacheMiss();
		}
		
		// Choose execution strategy
		GinExecutionStrategy strategy = chooseExecutionStrategy(search_tokens, GIN_QUERY_CONTAINS);
		
		bool success = false;
		
		switch (strategy) {
			case GIN_STRATEGY_BITMAP:
				success = executeContainsAllQuery(search_tokens, result_bitmap);
				break;
				
			case GIN_STRATEGY_SORTED_SCAN:
				success = executeSortedScanStrategy(search_tokens, result_bitmap);
				break;
				
			case GIN_STRATEGY_HASH_JOIN:
				success = executeHashJoinStrategy(search_tokens, result_bitmap);
				break;
				
			default:
				success = executeContainsAllQuery(search_tokens, result_bitmap);
				break;
		}
		
		m_last_stats.strategy = strategy;
		m_last_stats.records_returned = result_bitmap->getCount();
		
		// Cache result if successful and cacheable
		if (success && m_enable_caching && isResultCacheable(search_tokens, GIN_QUERY_CONTAINS)) {
			ScratchBird::string query_key = createQueryKey(search_tokens, GIN_QUERY_CONTAINS);
			cacheResult(query_key, result_bitmap);
		}
		
		endQuery();
		if (stats) *stats = m_last_stats;
		return success;
	}
	catch (...) {
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
}

bool GinQueryProcessor::executeContainsAnyQuery(const TokenList& search_tokens, RecordBitmap* result_bitmap,
												 GinQueryStats* stats)
{
	fb_assert(result_bitmap != nullptr);
	
	startQuery();
	
	try {
		if (search_tokens.isEmpty()) {
			endQuery();
			if (stats) *stats = m_last_stats;
			return false;
		}
		
		m_last_stats.tokens_processed = search_tokens.getCount();
		
		// Check cache
		if (m_enable_caching) {
			ScratchBird::string query_key = createQueryKey(search_tokens, GIN_QUERY_CONTAINS_ANY);
			RecordBitmap* cached_result = getCachedResult(query_key);
			
			if (cached_result) {
				incrementCacheHit();
				result_bitmap->copy(*cached_result);
				endQuery();
				if (stats) *stats = m_last_stats;
				return true;
			}
			incrementCacheMiss();
		}
		
		// Execute OR operation on all tokens
		Array<PostingList> posting_lists;
		posting_lists.grow(search_tokens.getCount());
		
		for (FB_SIZE_T i = 0; i < search_tokens.getCount(); i++) {
			PostingList list = getPostingList(search_tokens[i]);
			if (!list.isEmpty()) {
				posting_lists.add(list);
				m_last_stats.posting_lists_read++;
			}
		}
		
		if (posting_lists.isEmpty()) {
			endQuery();
			if (stats) *stats = m_last_stats;
			return false;
		}
		
		// Union all posting lists
		RecordBitmap* union_result = unionPostingLists(posting_lists);
		
		if (union_result) {
			result_bitmap->copy(*union_result);
			deallocateBitmap(union_result);
			
			m_last_stats.records_returned = result_bitmap->getCount();
			
			// Cache result
			if (m_enable_caching && isResultCacheable(search_tokens, GIN_QUERY_CONTAINS_ANY)) {
				ScratchBird::string query_key = createQueryKey(search_tokens, GIN_QUERY_CONTAINS_ANY);
				cacheResult(query_key, result_bitmap);
			}
			
			endQuery();
			if (stats) *stats = m_last_stats;
			return true;
		}
		
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
	catch (...) {
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
}

bool GinQueryProcessor::executeContainsAllQuery(const TokenList& search_tokens, RecordBitmap* result_bitmap,
												 GinQueryStats* stats)
{
	fb_assert(result_bitmap != nullptr);
	
	startQuery();
	
	try {
		if (search_tokens.isEmpty()) {
			endQuery();
			if (stats) *stats = m_last_stats;
			return false;
		}
		
		m_last_stats.tokens_processed = search_tokens.getCount();
		
		// Check cache
		if (m_enable_caching) {
			ScratchBird::string query_key = createQueryKey(search_tokens, GIN_QUERY_CONTAINS_ALL);
			RecordBitmap* cached_result = getCachedResult(query_key);
			
			if (cached_result) {
				incrementCacheHit();
				result_bitmap->copy(*cached_result);
				endQuery();
				if (stats) *stats = m_last_stats;
				return true;
			}
			incrementCacheMiss();
		}
		
		// Reorder tokens by selectivity (smallest first for efficiency)
		TokenList ordered_tokens = search_tokens;
		reorderTokensBySelectivity(ordered_tokens);
		
		// Get posting lists for all tokens
		Array<PostingList> posting_lists;
		posting_lists.grow(ordered_tokens.getCount());
		
		for (FB_SIZE_T i = 0; i < ordered_tokens.getCount(); i++) {
			PostingList list = getPostingList(ordered_tokens[i]);
			if (list.isEmpty()) {
				// If any token has no matches, result is empty
				endQuery();
				if (stats) *stats = m_last_stats;
				return true; // Empty result is valid
			}
			posting_lists.add(list);
			m_last_stats.posting_lists_read++;
		}
		
		// Intersect all posting lists
		RecordBitmap* intersection_result = intersectPostingLists(posting_lists);
		
		if (intersection_result) {
			result_bitmap->copy(*intersection_result);
			deallocateBitmap(intersection_result);
			
			m_last_stats.records_returned = result_bitmap->getCount();
			
			// Cache result
			if (m_enable_caching && isResultCacheable(search_tokens, GIN_QUERY_CONTAINS_ALL)) {
				ScratchBird::string query_key = createQueryKey(search_tokens, GIN_QUERY_CONTAINS_ALL);
				cacheResult(query_key, result_bitmap);
			}
			
			endQuery();
			if (stats) *stats = m_last_stats;
			return true;
		}
		
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
	catch (...) {
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
}

bool GinQueryProcessor::executePhraseQuery(const TokenList& phrase_tokens, USHORT max_distance,
											RecordBitmap* result_bitmap, GinQueryStats* stats)
{
	fb_assert(result_bitmap != nullptr);
	
	startQuery();
	
	try {
		if (phrase_tokens.getCount() < 2) {
			// Single token phrase - treat as simple contains
			if (phrase_tokens.getCount() == 1) {
				return executeContainsQuery(phrase_tokens, result_bitmap, stats);
			}
			endQuery();
			if (stats) *stats = m_last_stats;
			return false;
		}
		
		m_last_stats.tokens_processed = phrase_tokens.getCount();
		
		// For now, implement simplified phrase matching
		// Full implementation would require position information in posting lists
		RecordBitmap* phrase_result = matchPhrase(phrase_tokens, max_distance);
		
		if (phrase_result) {
			result_bitmap->copy(*phrase_result);
			deallocateBitmap(phrase_result);
			
			m_last_stats.records_returned = result_bitmap->getCount();
			endQuery();
			if (stats) *stats = m_last_stats;
			return true;
		}
		
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
	catch (...) {
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
}

bool GinQueryProcessor::executeSimilarityQuery(const dsc* search_value, double threshold,
												RecordBitmap* result_bitmap, GinQueryStats* stats)
{
	fb_assert(search_value != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	startQuery();
	
	try {
		// Extract tokens from search value
		TokenList search_tokens = m_gin_index->extractTokens(search_value);
		
		if (search_tokens.isEmpty()) {
			endQuery();
			if (stats) *stats = m_last_stats;
			return false;
		}
		
		m_last_stats.tokens_processed = search_tokens.getCount();
		
		// Execute similarity matching
		RecordBitmap* similarity_result = matchSimilarity(search_tokens, threshold);
		
		if (similarity_result) {
			result_bitmap->copy(*similarity_result);
			deallocateBitmap(similarity_result);
			
			m_last_stats.records_returned = result_bitmap->getCount();
			endQuery();
			if (stats) *stats = m_last_stats;
			return true;
		}
		
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
	catch (...) {
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
}

bool GinQueryProcessor::executeBooleanQuery(GinPlanNode* query_plan, RecordBitmap* result_bitmap,
											 GinQueryStats* stats)
{
	fb_assert(query_plan != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	startQuery();
	
	try {
		bool success = executeQueryPlan(query_plan, result_bitmap);
		
		if (success) {
			m_last_stats.records_returned = result_bitmap->getCount();
		}
		
		endQuery();
		if (stats) *stats = m_last_stats;
		return success;
	}
	catch (...) {
		endQuery();
		if (stats) *stats = m_last_stats;
		return false;
	}
}

//----------------------------
// Query Planning and Optimization
//----------------------------

GinPlanNode* GinQueryProcessor::createQueryPlan(const TokenList& search_tokens, GinQueryType query_type)
{
	switch (query_type) {
		case GIN_QUERY_CONTAINS:
		case GIN_QUERY_CONTAINS_ALL:
			return GinQueryBuilder::buildContainsAllQuery(search_tokens);
			
		case GIN_QUERY_CONTAINS_ANY:
			return GinQueryBuilder::buildContainsAnyQuery(search_tokens);
			
		case GIN_QUERY_PHRASE:
			return GinQueryBuilder::buildPhraseQuery(search_tokens, MAX_PHRASE_DISTANCE);
			
		default:
			return GinQueryBuilder::buildContainsQuery(search_tokens);
	}
}

void GinQueryProcessor::optimizeQueryPlan(GinPlanNode* plan)
{
	fb_assert(plan != nullptr);
	
	// Estimate costs for the plan
	estimateQueryCost(plan);
	
	// Optimize based on plan type
	switch (plan->node_type) {
		case GIN_PLAN_AND:
			// Reorder children by selectivity (most selective first)
			std::sort(plan->children.begin(), plan->children.end(),
					  [](const GinPlanNode* a, const GinPlanNode* b) {
						  return a->estimated_selectivity < b->estimated_selectivity;
					  });
			break;
			
		case GIN_PLAN_OR:
			// Reorder children by cost (cheapest first)
			std::sort(plan->children.begin(), plan->children.end(),
					  [](const GinPlanNode* a, const GinPlanNode* b) {
						  return a->estimated_cost < b->estimated_cost;
					  });
			break;
			
		default:
			break;
	}
	
	// Recursively optimize children
	for (size_t i = 0; i < plan->children.getCount(); i++) {
		optimizeQueryPlan(plan->children[i]);
	}
}

void GinQueryProcessor::estimateQueryCost(GinPlanNode* plan)
{
	fb_assert(plan != nullptr);
	
	switch (plan->node_type) {
		case GIN_PLAN_TOKEN:
			{
				ULONG posting_size = estimatePostingListSize(plan->token);
				plan->estimated_cost = posting_size;
				plan->estimated_selectivity = posting_size;
			}
			break;
			
		case GIN_PLAN_AND:
			{
				ULONG total_cost = 0;
				ULONG min_selectivity = ULONG_MAX;
				
				for (size_t i = 0; i < plan->children.getCount(); i++) {
					estimateQueryCost(plan->children[i]);
					total_cost += plan->children[i]->estimated_cost;
					min_selectivity = std::min(min_selectivity, plan->children[i]->estimated_selectivity);
				}
				
				plan->estimated_cost = total_cost;
				plan->estimated_selectivity = min_selectivity; // Intersection reduces selectivity
			}
			break;
			
		case GIN_PLAN_OR:
			{
				ULONG total_cost = 0;
				ULONG total_selectivity = 0;
				
				for (size_t i = 0; i < plan->children.getCount(); i++) {
					estimateQueryCost(plan->children[i]);
					total_cost += plan->children[i]->estimated_cost;
					total_selectivity += plan->children[i]->estimated_selectivity;
				}
				
				plan->estimated_cost = total_cost;
				plan->estimated_selectivity = total_selectivity; // Union increases selectivity
			}
			break;
			
		default:
			plan->estimated_cost = 1000; // Default cost
			plan->estimated_selectivity = 1000; // Default selectivity
			break;
	}
}

GinExecutionStrategy GinQueryProcessor::chooseExecutionStrategy(const TokenList& search_tokens, 
															   GinQueryType query_type)
{
	// Simple strategy selection based on token count and estimated sizes
	if (search_tokens.getCount() == 1) {
		return GIN_STRATEGY_BITMAP;
	}
	
	if (search_tokens.getCount() <= 3) {
		return shouldUseBitmapStrategy(search_tokens) ? GIN_STRATEGY_BITMAP : GIN_STRATEGY_SORTED_SCAN;
	}
	
	// For complex queries, use hybrid approach
	return GIN_STRATEGY_HYBRID;
}

//----------------------------
// Posting List Operations
//----------------------------

PostingList GinQueryProcessor::getPostingList(const Token& token)
{
	PostingListEntry* entry = m_gin_index->findPostingList(m_tdbb, token);
	
	if (entry) {
		return entry->decompress();
	}
	
	return PostingList(); // Empty list
}

RecordBitmap* GinQueryProcessor::intersectPostingLists(const Array<PostingList>& posting_lists)
{
	if (posting_lists.isEmpty()) {
		return allocateBitmap(0);
	}
	
	if (posting_lists.getCount() == 1) {
		return postingListToBitmap(posting_lists[0]);
	}
	
	// Convert posting lists to bitmaps
	Array<RecordBitmap*> bitmaps;
	bitmaps.grow(posting_lists.getCount());
	
	for (FB_SIZE_T i = 0; i < posting_lists.getCount(); i++) {
		RecordBitmap* bitmap = postingListToBitmap(posting_lists[i]);
		if (bitmap) {
			bitmaps.add(bitmap);
			m_temp_bitmaps.add(bitmap);
		}
	}
	
	m_last_stats.bitmap_operations += bitmaps.getCount();
	
	return intersectBitmaps(bitmaps);
}

RecordBitmap* GinQueryProcessor::unionPostingLists(const Array<PostingList>& posting_lists)
{
	if (posting_lists.isEmpty()) {
		return allocateBitmap(0);
	}
	
	if (posting_lists.getCount() == 1) {
		return postingListToBitmap(posting_lists[0]);
	}
	
	// Convert posting lists to bitmaps
	Array<RecordBitmap*> bitmaps;
	bitmaps.grow(posting_lists.getCount());
	
	for (FB_SIZE_T i = 0; i < posting_lists.getCount(); i++) {
		RecordBitmap* bitmap = postingListToBitmap(posting_lists[i]);
		if (bitmap) {
			bitmaps.add(bitmap);
			m_temp_bitmaps.add(bitmap);
		}
	}
	
	m_last_stats.bitmap_operations += bitmaps.getCount();
	
	return unionBitmaps(bitmaps);
}

RecordBitmap* GinQueryProcessor::subtractPostingLists(const PostingList& positive_list, 
													  const PostingList& negative_list)
{
	RecordBitmap* positive_bitmap = postingListToBitmap(positive_list);
	RecordBitmap* negative_bitmap = postingListToBitmap(negative_list);
	
	if (!positive_bitmap) {
		if (negative_bitmap) deallocateBitmap(negative_bitmap);
		return allocateBitmap(0);
	}
	
	if (!negative_bitmap) {
		return positive_bitmap;
	}
	
	// Subtract negative from positive
	positive_bitmap->subtract(*negative_bitmap);
	
	deallocateBitmap(negative_bitmap);
	m_last_stats.bitmap_operations++;
	
	return positive_bitmap;
}

//----------------------------
// Advanced Query Features
//----------------------------

RecordBitmap* GinQueryProcessor::matchPhrase(const TokenList& phrase_tokens, USHORT max_distance)
{
	// Simplified phrase matching - for full implementation, position information would be needed
	// For now, use AND semantics with proximity consideration
	
	if (phrase_tokens.getCount() < 2) {
		return allocateBitmap(0);
	}
	
	// Get posting lists for all tokens
	Array<PostingList> posting_lists;
	posting_lists.grow(phrase_tokens.getCount());
	
	for (FB_SIZE_T i = 0; i < phrase_tokens.getCount(); i++) {
		PostingList list = getPostingList(phrase_tokens[i]);
		if (list.isEmpty()) {
			return allocateBitmap(0); // No phrase matches if any token is missing
		}
		posting_lists.add(list);
	}
	
	// For now, intersect all posting lists (simplified phrase matching)
	return intersectPostingLists(posting_lists);
}

RecordBitmap* GinQueryProcessor::matchProximity(const Token& token1, const Token& token2, USHORT max_distance)
{
	// Simplified proximity matching - intersect the two token posting lists
	PostingList list1 = getPostingList(token1);
	PostingList list2 = getPostingList(token2);
	
	if (list1.isEmpty() || list2.isEmpty()) {
		return allocateBitmap(0);
	}
	
	Array<PostingList> lists;
	lists.add(list1);
	lists.add(list2);
	
	return intersectPostingLists(lists);
}

RecordBitmap* GinQueryProcessor::matchWildcard(const ScratchBird::string& pattern)
{
	// TODO: Implement wildcard pattern matching
	// This would involve iterating through all tokens and matching against pattern
	return allocateBitmap(0);
}

RecordBitmap* GinQueryProcessor::matchFuzzy(const Token& token, USHORT max_edit_distance)
{
	// TODO: Implement fuzzy matching with edit distance
	// This would involve generating candidate tokens within edit distance
	return allocateBitmap(0);
}

RecordBitmap* GinQueryProcessor::matchSimilarity(const TokenList& search_tokens, double threshold)
{
	// Simplified similarity matching using token overlap
	// Full implementation would use cosine similarity or other metrics
	
	if (search_tokens.isEmpty()) {
		return allocateBitmap(0);
	}
	
	// For now, use OR semantics for similarity
	Array<PostingList> posting_lists;
	posting_lists.grow(search_tokens.getCount());
	
	for (FB_SIZE_T i = 0; i < search_tokens.getCount(); i++) {
		PostingList list = getPostingList(search_tokens[i]);
		if (!list.isEmpty()) {
			posting_lists.add(list);
		}
	}
	
	return unionPostingLists(posting_lists);
}

//----------------------------
// Bitmap Operations
//----------------------------

RecordBitmap* GinQueryProcessor::postingListToBitmap(const PostingList& posting_list)
{
	if (posting_list.isEmpty()) {
		return allocateBitmap(0);
	}
	
	RecordBitmap* bitmap = allocateBitmap(posting_list.getCount());
	
	for (FB_SIZE_T i = 0; i < posting_list.getCount(); i++) {
		bitmap->set(posting_list[i]);
		m_last_stats.records_examined++;
	}
	
	return bitmap;
}

RecordBitmap* GinQueryProcessor::intersectBitmaps(const Array<RecordBitmap*>& bitmaps)
{
	if (bitmaps.isEmpty()) {
		return allocateBitmap(0);
	}
	
	if (bitmaps.getCount() == 1) {
		RecordBitmap* result = allocateBitmap(bitmaps[0]->getCount());
		result->copy(*bitmaps[0]);
		return result;
	}
	
	// Start with the first bitmap
	RecordBitmap* result = allocateBitmap(bitmaps[0]->getCount());
	result->copy(*bitmaps[0]);
	
	// Intersect with remaining bitmaps
	for (FB_SIZE_T i = 1; i < bitmaps.getCount(); i++) {
		result->intersect(*bitmaps[i]);
		m_last_stats.bitmap_operations++;
	}
	
	return result;
}

RecordBitmap* GinQueryProcessor::unionBitmaps(const Array<RecordBitmap*>& bitmaps)
{
	if (bitmaps.isEmpty()) {
		return allocateBitmap(0);
	}
	
	if (bitmaps.getCount() == 1) {
		RecordBitmap* result = allocateBitmap(bitmaps[0]->getCount());
		result->copy(*bitmaps[0]);
		return result;
	}
	
	// Start with the first bitmap
	RecordBitmap* result = allocateBitmap(bitmaps[0]->getCount());
	result->copy(*bitmaps[0]);
	
	// Union with remaining bitmaps
	for (FB_SIZE_T i = 1; i < bitmaps.getCount(); i++) {
		result->unite(*bitmaps[i]);
		m_last_stats.bitmap_operations++;
	}
	
	return result;
}

RecordBitmap* GinQueryProcessor::optimizedBitmapOperation(const Array<RecordBitmap*>& bitmaps,
														  bool is_intersection)
{
	if (is_intersection) {
		return intersectBitmaps(bitmaps);
	} else {
		return unionBitmaps(bitmaps);
	}
}

//----------------------------
// Performance and Caching
//----------------------------

void GinQueryProcessor::setExecutionStrategy(GinExecutionStrategy strategy)
{
	m_strategy = strategy;
}

void GinQueryProcessor::enableResultCaching(bool enable, ULONG max_cache_size)
{
	m_enable_caching = enable;
	m_max_cache_size = max_cache_size;
	
	if (!enable) {
		clearResultCache();
	}
}

void GinQueryProcessor::clearResultCache()
{
	for (GenericMap<Pair<ScratchBird::string, RecordBitmap*>>::iterator it = m_result_cache.begin();
		 it != m_result_cache.end(); ++it) {
		delete it->second;
	}
	m_result_cache.clear();
}

RecordBitmap* GinQueryProcessor::getCachedResult(const ScratchBird::string& query_key)
{
	GenericMap<Pair<ScratchBird::string, RecordBitmap*>>::iterator it = 
		m_result_cache.locate(Pair<ScratchBird::string, RecordBitmap*>(query_key, nullptr));
	
	return (it != m_result_cache.end()) ? it->second : nullptr;
}

void GinQueryProcessor::cacheResult(const ScratchBird::string& query_key, RecordBitmap* result)
{
	if (m_result_cache.count() >= m_max_cache_size) {
		evictOldCacheEntries();
	}
	
	// Create a copy of the result for caching
	RecordBitmap* cached_bitmap = allocateBitmap(result->getCount());
	cached_bitmap->copy(*result);
	
	m_result_cache.put(Pair<ScratchBird::string, RecordBitmap*>(query_key, cached_bitmap));
}

//----------------------------
// Statistics and Monitoring
//----------------------------

void GinQueryProcessor::resetStatistics()
{
	memset(&m_last_stats, 0, sizeof(m_last_stats));
	memset(&m_cumulative_stats, 0, sizeof(m_cumulative_stats));
}

GinQueryProcessor::TokenFrequencyStats GinQueryProcessor::getTokenFrequency(const Token& token)
{
	TokenFrequencyStats stats;
	stats.token = token;
	stats.document_frequency = 0;
	stats.total_frequency = 0;
	stats.idf_score = 0.0;
	
	// Get posting list for token
	PostingList list = getPostingList(token);
	stats.document_frequency = list.getCount();
	stats.total_frequency = list.getCount(); // Simplified - assume one occurrence per document
	
	// Calculate IDF score (inverse document frequency)
	if (stats.document_frequency > 0) {
		ULONG total_documents = m_gin_index->getTotalDocuments(); // Would need to be implemented
		if (total_documents > 0) {
			stats.idf_score = log(static_cast<double>(total_documents) / stats.document_frequency);
		}
	}
	
	return stats;
}

//----------------------------
// Private Helper Methods
//----------------------------

bool GinQueryProcessor::executeQueryPlan(GinPlanNode* plan, RecordBitmap* result_bitmap)
{
	fb_assert(plan != nullptr);
	fb_assert(result_bitmap != nullptr);
	
	switch (plan->node_type) {
		case GIN_PLAN_TOKEN:
			{
				RecordBitmap* token_result = executeTokenLookup(plan->token);
				if (token_result) {
					result_bitmap->copy(*token_result);
					deallocateBitmap(token_result);
					return true;
				}
				return false;
			}
			
		case GIN_PLAN_AND:
			{
				RecordBitmap* and_result = executeAndOperation(plan->children);
				if (and_result) {
					result_bitmap->copy(*and_result);
					deallocateBitmap(and_result);
					return true;
				}
				return false;
			}
			
		case GIN_PLAN_OR:
			{
				RecordBitmap* or_result = executeOrOperation(plan->children);
				if (or_result) {
					result_bitmap->copy(*or_result);
					deallocateBitmap(or_result);
					return true;
				}
				return false;
			}
			
		case GIN_PLAN_NOT:
			{
				if (plan->children.getCount() == 1) {
					RecordBitmap* not_result = executeNotOperation(plan->children[0]);
					if (not_result) {
						result_bitmap->copy(*not_result);
						deallocateBitmap(not_result);
						return true;
					}
				}
				return false;
			}
			
		default:
			return false;
	}
}

RecordBitmap* GinQueryProcessor::executeTokenLookup(const Token& token)
{
	PostingList list = getPostingList(token);
	m_last_stats.posting_lists_read++;
	
	return postingListToBitmap(list);
}

RecordBitmap* GinQueryProcessor::executeAndOperation(const Array<GinPlanNode*>& operands)
{
	Array<RecordBitmap*> bitmaps;
	bitmaps.grow(operands.getCount());
	
	for (FB_SIZE_T i = 0; i < operands.getCount(); i++) {
		RecordBitmap* operand_result = allocateBitmap();
		if (executeQueryPlan(operands[i], operand_result)) {
			bitmaps.add(operand_result);
			m_temp_bitmaps.add(operand_result);
		} else {
			deallocateBitmap(operand_result);
			// If any operand fails, AND result is empty
			return allocateBitmap(0);
		}
	}
	
	return intersectBitmaps(bitmaps);
}

RecordBitmap* GinQueryProcessor::executeOrOperation(const Array<GinPlanNode*>& operands)
{
	Array<RecordBitmap*> bitmaps;
	bitmaps.grow(operands.getCount());
	
	for (FB_SIZE_T i = 0; i < operands.getCount(); i++) {
		RecordBitmap* operand_result = allocateBitmap();
		if (executeQueryPlan(operands[i], operand_result)) {
			bitmaps.add(operand_result);
			m_temp_bitmaps.add(operand_result);
		} else {
			deallocateBitmap(operand_result);
		}
	}
	
	if (bitmaps.isEmpty()) {
		return allocateBitmap(0);
	}
	
	return unionBitmaps(bitmaps);
}

RecordBitmap* GinQueryProcessor::executeNotOperation(GinPlanNode* operand)
{
	// NOT operation requires universe set - simplified implementation
	RecordBitmap* operand_result = allocateBitmap();
	
	if (executeQueryPlan(operand, operand_result)) {
		// For NOT operation, we'd need to subtract from universal set
		// This is a simplified implementation
		RecordBitmap* not_result = allocateBitmap(0);
		deallocateBitmap(operand_result);
		return not_result;
	}
	
	deallocateBitmap(operand_result);
	return allocateBitmap(0);
}

void GinQueryProcessor::reorderTokensBySelectivity(TokenList& tokens)
{
	// Sort tokens by estimated selectivity (smaller posting lists first)
	std::sort(tokens.begin(), tokens.end(), [this](const Token& a, const Token& b) {
		double sel_a = calculateTokenSelectivity(a);
		double sel_b = calculateTokenSelectivity(b);
		return sel_a < sel_b;
	});
}

bool GinQueryProcessor::shouldUseBitmapStrategy(const TokenList& tokens)
{
	// Use bitmap strategy for larger posting lists
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		ULONG size = estimatePostingListSize(tokens[i]);
		if (size > BITMAP_STRATEGY_THRESHOLD) {
			return true;
		}
	}
	return false;
}

ULONG GinQueryProcessor::estimatePostingListSize(const Token& token)
{
	// For now, return a default estimate
	// Real implementation would use statistics from index
	return 1000;
}

double GinQueryProcessor::calculateTokenSelectivity(const Token& token)
{
	ULONG posting_size = estimatePostingListSize(token);
	return static_cast<double>(posting_size);
}

RecordBitmap* GinQueryProcessor::allocateBitmap(ULONG estimated_size)
{
	return FB_NEW_POOL(getDefaultMemoryPool()) RecordBitmap(estimated_size > 0 ? estimated_size : DEFAULT_BITMAP_SIZE);
}

void GinQueryProcessor::deallocateBitmap(RecordBitmap* bitmap)
{
	delete bitmap;
}

void GinQueryProcessor::cleanupTempBitmaps()
{
	for (FB_SIZE_T i = 0; i < m_temp_bitmaps.getCount(); i++) {
		delete m_temp_bitmaps[i];
	}
	m_temp_bitmaps.clear();
}

ScratchBird::string GinQueryProcessor::createQueryKey(const TokenList& tokens, GinQueryType query_type)
{
	ScratchBird::string key;
	key += static_cast<char>('0' + query_type);
	key += ":";
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		if (i > 0) key += ",";
		key += tokens[i].c_str();
	}
	
	return key;
}

bool GinQueryProcessor::isResultCacheable(const TokenList& tokens, GinQueryType query_type)
{
	// Cache results for small to medium token sets
	return tokens.getCount() <= 5;
}

void GinQueryProcessor::evictOldCacheEntries()
{
	// Simple eviction - remove half the entries
	// Real implementation would use LRU or similar
	ULONG target_size = m_max_cache_size / 2;
	ULONG removed = 0;
	
	GenericMap<Pair<ScratchBird::string, RecordBitmap*>>::iterator it = m_result_cache.begin();
	while (it != m_result_cache.end() && removed < target_size) {
		delete it->second;
		it = m_result_cache.remove(it);
		removed++;
	}
}

void GinQueryProcessor::startQuery()
{
	m_last_stats.query_start_time = high_resolution_clock::now();
	memset(&m_last_stats, 0, sizeof(m_last_stats));
}

void GinQueryProcessor::endQuery()
{
	auto end_time = high_resolution_clock::now();
	m_last_stats.query_time_ms = duration_cast<milliseconds>(end_time - m_last_stats.query_start_time).count();
	
	updateStatistics(m_last_stats);
}

void GinQueryProcessor::updateStatistics(const GinQueryStats& query_stats)
{
	m_cumulative_stats.query_time_ms += query_stats.query_time_ms;
	m_cumulative_stats.tokens_processed += query_stats.tokens_processed;
	m_cumulative_stats.posting_lists_read += query_stats.posting_lists_read;
	m_cumulative_stats.records_examined += query_stats.records_examined;
	m_cumulative_stats.records_returned += query_stats.records_returned;
	m_cumulative_stats.bitmap_operations += query_stats.bitmap_operations;
	m_cumulative_stats.cache_hits += query_stats.cache_hits;
	m_cumulative_stats.cache_misses += query_stats.cache_misses;
}

void GinQueryProcessor::incrementCacheHit()
{
	m_last_stats.cache_hits++;
}

void GinQueryProcessor::incrementCacheMiss()
{
	m_last_stats.cache_misses++;
}

//----------------------------
// GinQueryResultIterator Implementation
//----------------------------

GinQueryResultIterator::GinQueryResultIterator(RecordBitmap* bitmap)
	: m_bitmap(bitmap),
	  m_current_position(0),
	  m_current_record(0),
	  m_has_next(false)
{
	fb_assert(bitmap != nullptr);
	advanceToNext();
}

GinQueryResultIterator::~GinQueryResultIterator()
{
}

bool GinQueryResultIterator::next()
{
	if (!m_has_next) {
		return false;
	}
	
	m_current_position++;
	advanceToNext();
	return m_has_next;
}

bool GinQueryResultIterator::hasNext() const
{
	return m_has_next;
}

RecordNumber GinQueryResultIterator::getCurrentRecord() const
{
	return m_current_record;
}

ULONG GinQueryResultIterator::getCurrentPosition() const
{
	return m_current_position;
}

ULONG GinQueryResultIterator::getTotalResults() const
{
	return m_bitmap->getCount();
}

ULONG GinQueryResultIterator::getProcessedResults() const
{
	return m_current_position;
}

void GinQueryResultIterator::advanceToNext()
{
	m_has_next = m_bitmap->getNext(m_current_record);
}

//----------------------------
// GinQueryBuilder Implementation
//----------------------------

GinPlanNode* GinQueryBuilder::buildContainsQuery(const TokenList& tokens)
{
	return buildContainsAllQuery(tokens);
}

GinPlanNode* GinQueryBuilder::buildContainsAnyQuery(const TokenList& tokens)
{
	if (tokens.isEmpty()) {
		return nullptr;
	}
	
	if (tokens.getCount() == 1) {
		return createTokenNode(tokens[0]);
	}
	
	Array<GinPlanNode*> token_nodes;
	token_nodes.grow(tokens.getCount());
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		token_nodes.add(createTokenNode(tokens[i]));
	}
	
	return createOrNode(token_nodes);
}

GinPlanNode* GinQueryBuilder::buildContainsAllQuery(const TokenList& tokens)
{
	if (tokens.isEmpty()) {
		return nullptr;
	}
	
	if (tokens.getCount() == 1) {
		return createTokenNode(tokens[0]);
	}
	
	Array<GinPlanNode*> token_nodes;
	token_nodes.grow(tokens.getCount());
	
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		token_nodes.add(createTokenNode(tokens[i]));
	}
	
	return createAndNode(token_nodes);
}

GinPlanNode* GinQueryBuilder::buildPhraseQuery(const TokenList& tokens, USHORT max_distance)
{
	return createPhraseNode(tokens, max_distance);
}

GinPlanNode* GinQueryBuilder::buildBooleanQuery(const ScratchBird::string& boolean_expression)
{
	// TODO: Implement boolean expression parsing
	return nullptr;
}

GinPlanNode* GinQueryBuilder::createAndNode(const Array<GinPlanNode*>& children)
{
	GinPlanNode* node = FB_NEW_POOL(getDefaultMemoryPool()) GinPlanNode(GIN_PLAN_AND);
	for (FB_SIZE_T i = 0; i < children.getCount(); i++) {
		node->children.add(children[i]);
	}
	return node;
}

GinPlanNode* GinQueryBuilder::createOrNode(const Array<GinPlanNode*>& children)
{
	GinPlanNode* node = FB_NEW_POOL(getDefaultMemoryPool()) GinPlanNode(GIN_PLAN_OR);
	for (FB_SIZE_T i = 0; i < children.getCount(); i++) {
		node->children.add(children[i]);
	}
	return node;
}

GinPlanNode* GinQueryBuilder::createNotNode(GinPlanNode* operand)
{
	GinPlanNode* node = FB_NEW_POOL(getDefaultMemoryPool()) GinPlanNode(GIN_PLAN_NOT);
	node->children.add(operand);
	return node;
}

GinPlanNode* GinQueryBuilder::createTokenNode(const Token& token)
{
	GinPlanNode* node = FB_NEW_POOL(getDefaultMemoryPool()) GinPlanNode(GIN_PLAN_TOKEN);
	node->token = token;
	return node;
}

GinPlanNode* GinQueryBuilder::createPhraseNode(const TokenList& tokens, USHORT max_distance)
{
	GinPlanNode* node = FB_NEW_POOL(getDefaultMemoryPool()) GinPlanNode(GIN_PLAN_PHRASE);
	node->proximity_distance = max_distance;
	
	// Add token children
	for (FB_SIZE_T i = 0; i < tokens.getCount(); i++) {
		node->children.add(createTokenNode(tokens[i]));
	}
	
	return node;
}

} // namespace Jrd