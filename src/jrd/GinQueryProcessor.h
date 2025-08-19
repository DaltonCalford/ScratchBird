/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinQueryProcessor.h  
 *	DESCRIPTION:	GIN query processor for CONTAINS operations and full-text search
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

#ifndef JRD_GIN_QUERY_PROCESSOR_H
#define JRD_GIN_QUERY_PROCESSOR_H

#include "../jrd/jrd.h"
#include "../jrd/req.h"
#include "../jrd/exe.h"
#include "../jrd/GinIndex.h"
#include "../jrd/GinPageManager.h"
#include "../jrd/RecordBitmap.h"
#include "../common/classes/array.h"
#include "../common/classes/GenericMap.h"

namespace Jrd {

// Forward declarations
class GinIndex;
class GinPageManager;
class Database;
class jrd_req;

//----------------------------
// Query Operation Types
//----------------------------
enum GinQueryType {
	GIN_QUERY_CONTAINS = 0,		// CONTAINS operator (AND semantics)
	GIN_QUERY_CONTAINS_ANY = 1,	// CONTAINS ANY (OR semantics) 
	GIN_QUERY_CONTAINS_ALL = 2,	// CONTAINS ALL (AND semantics)
	GIN_QUERY_MATCHES = 3,		// Full-text MATCHES operator
	GIN_QUERY_SIMILAR = 4,		// SIMILAR TO with text patterns
	GIN_QUERY_PHRASE = 5,		// Phrase matching with proximity
	GIN_QUERY_BOOLEAN = 6		// Boolean query with AND/OR/NOT
};

//----------------------------
// Query Execution Strategy
//----------------------------
enum GinExecutionStrategy {
	GIN_STRATEGY_BITMAP = 0,		// Use bitmaps for result merging
	GIN_STRATEGY_SORTED_SCAN = 1,	// Use sorted posting list scans
	GIN_STRATEGY_HASH_JOIN = 2,		// Use hash-based joins
	GIN_STRATEGY_NESTED_LOOP = 3,	// Use nested loop joins
	GIN_STRATEGY_HYBRID = 4			// Adaptive strategy selection
};

//----------------------------
// Query Statistics
//----------------------------
struct GinQueryStats {
	ULONG query_time_ms;			// Total query execution time
	ULONG tokens_processed;			// Number of tokens processed
	ULONG posting_lists_read;		// Number of posting lists accessed
	ULONG records_examined;			// Total records examined
	ULONG records_returned;			// Records returned to user
	ULONG bitmap_operations;		// Number of bitmap operations
	ULONG cache_hits;				// Cache hits during query
	ULONG cache_misses;				// Cache misses during query
	double selectivity;				// Query selectivity estimate
	ULONG compressed_bytes_read;	// Bytes read from compressed posting lists
	ULONG decompression_time_ms;	// Time spent decompressing data
	GinExecutionStrategy strategy;	// Strategy used for execution
};

//----------------------------
// Query Context
//----------------------------
struct GinQueryContext {
	thread_db* tdbb;				// Thread database context
	jrd_req* request;				// Request context
	GinIndex* gin_index;			// GIN index being queried
	GinPageManager* page_manager;	// Page manager for storage access
	GinQueryType query_type;		// Type of query operation
	GinExecutionStrategy strategy;	// Execution strategy
	TokenList search_tokens;		// Tokens to search for
	RecordBitmap* result_bitmap;	// Result bitmap
	GinQueryStats* stats;			// Query execution statistics
	bool enable_caching;			// Enable result caching
	bool enable_optimization;		// Enable query optimization
	USHORT proximity_distance;		// Distance for phrase queries
	double similarity_threshold;	// Threshold for similarity queries
};

//----------------------------
// Query Plan Node Types
//----------------------------
enum GinPlanNodeType {
	GIN_PLAN_TOKEN = 0,				// Single token lookup
	GIN_PLAN_AND = 1,				// AND operation (intersection)
	GIN_PLAN_OR = 2,				// OR operation (union)
	GIN_PLAN_NOT = 3,				// NOT operation (negation)
	GIN_PLAN_PHRASE = 4,			// Phrase matching
	GIN_PLAN_PROXIMITY = 5,			// Proximity matching
	GIN_PLAN_WILDCARD = 6			// Wildcard pattern matching
};

//----------------------------
// Query Plan Node
//----------------------------
struct GinPlanNode {
	GinPlanNodeType node_type;		// Type of plan node
	Token token;					// Token for leaf nodes
	ULONG estimated_cost;			// Estimated execution cost
	ULONG estimated_selectivity;	// Estimated selectivity
	ScratchBird::Array<GinPlanNode*> children;	// Child nodes
	RecordBitmap* result_cache;		// Cached results
	bool is_negated;				// True if this is a NOT operation
	USHORT proximity_distance;		// Distance for proximity operations
	
	GinPlanNode(GinPlanNodeType type) : node_type(type), estimated_cost(0), 
		estimated_selectivity(0), result_cache(nullptr), is_negated(false), 
		proximity_distance(0) {}
		
	~GinPlanNode() {
		for (size_t i = 0; i < children.getCount(); i++) {
			delete children[i];
		}
		if (result_cache) {
			delete result_cache;
		}
	}
};

//----------------------------
// GIN Query Processor
//----------------------------
class GinQueryProcessor
{
public:
	//----------------------------
	// Constructor and Destructor
	//----------------------------
	GinQueryProcessor(thread_db* tdbb, GinIndex* gin_index);
	~GinQueryProcessor();
	
	//----------------------------
	// Main Query Interface
	//----------------------------
	
	// Execute CONTAINS query
	bool executeContainsQuery(const dsc* search_value, RecordBitmap* result_bitmap,
							  GinQueryStats* stats = nullptr);
	
	// Execute CONTAINS ANY query (OR semantics)
	bool executeContainsAnyQuery(const TokenList& search_tokens, RecordBitmap* result_bitmap,
								 GinQueryStats* stats = nullptr);
	
	// Execute CONTAINS ALL query (AND semantics) 
	bool executeContainsAllQuery(const TokenList& search_tokens, RecordBitmap* result_bitmap,
								 GinQueryStats* stats = nullptr);
	
	// Execute phrase matching query
	bool executePhraseQuery(const TokenList& phrase_tokens, USHORT max_distance,
							RecordBitmap* result_bitmap, GinQueryStats* stats = nullptr);
	
	// Execute similarity query
	bool executeSimilarityQuery(const dsc* search_value, double threshold,
								RecordBitmap* result_bitmap, GinQueryStats* stats = nullptr);
	
	// Execute boolean query with complex expressions
	bool executeBooleanQuery(GinPlanNode* query_plan, RecordBitmap* result_bitmap,
							 GinQueryStats* stats = nullptr);
	
	//----------------------------
	// Query Planning and Optimization
	//----------------------------
	
	// Create optimized query plan
	GinPlanNode* createQueryPlan(const TokenList& search_tokens, GinQueryType query_type);
	
	// Optimize query plan based on statistics
	void optimizeQueryPlan(GinPlanNode* plan);
	
	// Estimate query cost and selectivity
	void estimateQueryCost(GinPlanNode* plan);
	
	// Choose optimal execution strategy
	GinExecutionStrategy chooseExecutionStrategy(const TokenList& search_tokens, 
												 GinQueryType query_type);
	
	//----------------------------
	// Posting List Operations
	//----------------------------
	
	// Get posting list for token
	PostingList getPostingList(const Token& token);
	
	// Intersect multiple posting lists (AND operation)
	RecordBitmap* intersectPostingLists(const ScratchBird::Array<PostingList>& posting_lists);
	
	// Union multiple posting lists (OR operation)
	RecordBitmap* unionPostingLists(const ScratchBird::Array<PostingList>& posting_lists);
	
	// Subtract posting lists (NOT operation)
	RecordBitmap* subtractPostingLists(const PostingList& positive_list, 
										const PostingList& negative_list);
	
	//----------------------------
	// Advanced Query Features
	//----------------------------
	
	// Phrase matching with position information
	RecordBitmap* matchPhrase(const TokenList& phrase_tokens, USHORT max_distance);
	
	// Proximity matching between tokens
	RecordBitmap* matchProximity(const Token& token1, const Token& token2, USHORT max_distance);
	
	// Wildcard pattern matching
	RecordBitmap* matchWildcard(const ScratchBird::string& pattern);
	
	// Fuzzy matching with edit distance
	RecordBitmap* matchFuzzy(const Token& token, USHORT max_edit_distance);
	
	// N-gram based similarity matching
	RecordBitmap* matchSimilarity(const TokenList& search_tokens, double threshold);
	
	//----------------------------
	// Bitmap Operations
	//----------------------------
	
	// Convert posting list to bitmap
	RecordBitmap* postingListToBitmap(const PostingList& posting_list);
	
	// Intersect bitmaps efficiently
	RecordBitmap* intersectBitmaps(const ScratchBird::Array<RecordBitmap*>& bitmaps);
	
	// Union bitmaps efficiently  
	RecordBitmap* unionBitmaps(const ScratchBird::Array<RecordBitmap*>& bitmaps);
	
	// Optimize bitmap operations based on size
	RecordBitmap* optimizedBitmapOperation(const ScratchBird::Array<RecordBitmap*>& bitmaps,
										   bool is_intersection);
	
	//----------------------------
	// Performance and Caching
	//----------------------------
	
	// Set execution strategy
	void setExecutionStrategy(GinExecutionStrategy strategy);
	
	// Enable/disable query result caching
	void enableResultCaching(bool enable, ULONG max_cache_size = 100);
	
	// Clear query result cache
	void clearResultCache();
	
	// Get cached query result
	RecordBitmap* getCachedResult(const ScratchBird::string& query_key);
	
	// Cache query result
	void cacheResult(const ScratchBird::string& query_key, RecordBitmap* result);
	
	//----------------------------
	// Statistics and Monitoring
	//----------------------------
	
	// Get query execution statistics
	const GinQueryStats& getLastQueryStats() const { return m_last_stats; }
	
	// Get cumulative statistics
	const GinQueryStats& getCumulativeStats() const { return m_cumulative_stats; }
	
	// Reset statistics
	void resetStatistics();
	
	// Collect token frequency statistics
	struct TokenFrequencyStats {
		Token token;
		ULONG document_frequency;		// Number of documents containing token
		ULONG total_frequency;			// Total occurrences across all documents
		double idf_score;				// Inverse document frequency score
	};
	
	TokenFrequencyStats getTokenFrequency(const Token& token);
	
	//----------------------------
	// Configuration Constants
	//----------------------------
	
	static const ULONG DEFAULT_BITMAP_SIZE = 1000000;		// Default bitmap size
	static const USHORT MAX_PHRASE_DISTANCE = 10;			// Maximum phrase distance
	static const USHORT MAX_PROXIMITY_DISTANCE = 50;		// Maximum proximity distance
	static const double DEFAULT_SIMILARITY_THRESHOLD = 0.7;// Default similarity threshold
	static const ULONG MAX_RESULT_CACHE_SIZE = 100;		// Maximum cached results
	static const ULONG LARGE_POSTING_LIST_THRESHOLD = 10000;	// Large posting list threshold
	static const ULONG BITMAP_STRATEGY_THRESHOLD = 1000;	// Threshold for bitmap vs scan

private:
	//----------------------------
	// Private Member Variables
	//----------------------------
	thread_db* m_tdbb;
	GinIndex* m_gin_index;
	GinPageManager* m_page_manager;
	GinExecutionStrategy m_strategy;
	bool m_enable_caching;
	ULONG m_max_cache_size;
	
	// Statistics
	GinQueryStats m_last_stats;
	GinQueryStats m_cumulative_stats;
	
	// Query result cache
	GenericMap<Pair<ScratchBird::string, RecordBitmap*>> m_result_cache;
	
	// Temporary bitmaps for operations
	ScratchBird::Array<RecordBitmap*> m_temp_bitmaps;
	
	//----------------------------
	// Private Helper Methods
	//----------------------------
	
	// Query execution helpers
	bool executeQueryPlan(GinPlanNode* plan, RecordBitmap* result_bitmap);
	RecordBitmap* executeTokenLookup(const Token& token);
	RecordBitmap* executeAndOperation(const ScratchBird::Array<GinPlanNode*>& operands);
	RecordBitmap* executeOrOperation(const ScratchBird::Array<GinPlanNode*>& operands);
	RecordBitmap* executeNotOperation(GinPlanNode* operand);
	
	// Optimization helpers
	void reorderTokensBySelectivity(TokenList& tokens);
	bool shouldUseBitmapStrategy(const TokenList& tokens);
	ULONG estimatePostingListSize(const Token& token);
	double calculateTokenSelectivity(const Token& token);
	
	// Bitmap management helpers
	RecordBitmap* allocateBitmap(ULONG estimated_size = DEFAULT_BITMAP_SIZE);
	void deallocateBitmap(RecordBitmap* bitmap);
	void cleanupTempBitmaps();
	
	// Caching helpers
	ScratchBird::string createQueryKey(const TokenList& tokens, GinQueryType query_type);
	bool isResultCacheable(const TokenList& tokens, GinQueryType query_type);
	void evictOldCacheEntries();
	
	// Statistics helpers
	void startQuery();
	void endQuery();
	void updateStatistics(const GinQueryStats& query_stats);
	void incrementCacheHit();
	void incrementCacheMiss();
	
	// Pattern matching helpers
	bool matchesWildcardPattern(const Token& token, const ScratchBird::string& pattern);
	USHORT calculateEditDistance(const Token& token1, const Token& token2);
	double calculateCosineSimilarity(const TokenList& tokens1, const TokenList& tokens2);
	
	// Posting list helpers
	PostingList loadPostingList(const Token& token);
	bool isLargePostingList(const PostingList& posting_list);
	RecordBitmap* convertLargePostingList(const PostingList& posting_list);
	
	// Memory management
	void initializeQueryContext(GinQueryContext& context);
	void cleanupQueryContext(GinQueryContext& context);
};

//----------------------------
// Query Result Iterator
//----------------------------
class GinQueryResultIterator
{
public:
	GinQueryResultIterator(RecordBitmap* bitmap);
	~GinQueryResultIterator();
	
	// Iterator interface
	bool next();
	bool hasNext() const;
	RecordNumber getCurrentRecord() const;
	ULONG getCurrentPosition() const;
	
	// Statistics
	ULONG getTotalResults() const;
	ULONG getProcessedResults() const;
	
private:
	RecordBitmap* m_bitmap;
	ULONG m_current_position;
	RecordNumber m_current_record;
	bool m_has_next;
	
	void advanceToNext();
};

//----------------------------
// Query Builder Utility
//----------------------------
class GinQueryBuilder
{
public:
	static GinPlanNode* buildContainsQuery(const TokenList& tokens);
	static GinPlanNode* buildContainsAnyQuery(const TokenList& tokens);
	static GinPlanNode* buildContainsAllQuery(const TokenList& tokens);
	static GinPlanNode* buildPhraseQuery(const TokenList& tokens, USHORT max_distance);
	static GinPlanNode* buildBooleanQuery(const ScratchBird::string& boolean_expression);
	
	// Helper methods for building complex queries
	static GinPlanNode* createAndNode(const ScratchBird::Array<GinPlanNode*>& children);
	static GinPlanNode* createOrNode(const ScratchBird::Array<GinPlanNode*>& children);
	static GinPlanNode* createNotNode(GinPlanNode* operand);
	static GinPlanNode* createTokenNode(const Token& token);
	static GinPlanNode* createPhraseNode(const TokenList& tokens, USHORT max_distance);
};

} // namespace Jrd

#endif // JRD_GIN_QUERY_PROCESSOR_H