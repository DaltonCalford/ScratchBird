/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		GinTableScan.h  
 *	DESCRIPTION:	GIN index table scan record source for full-text queries
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
 * 2025.07.22 - ScratchBird GIN Table Scan Implementation
 */

#ifndef JRD_GIN_TABLE_SCAN_H
#define JRD_GIN_TABLE_SCAN_H

#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/GinIndex.h"
#include "../jrd/GinQueryProcessor.h"
#include "../jrd/RecordBitmap.h"
#include "../jrd/Database.h"
#include "../jrd/req.h"
#include "../jrd/jrd.h"
#include "../jrd/constants.h"
#include "../dsql/BoolNodes.h"
#include "../dsql/Nodes.h"
#include "../common/classes/array.h"

namespace Jrd {

// Forward declarations
class GinIndex;
class GinQueryProcessor;
class RecordBitmap;
class BoolExprNode;
class ValueExprNode;
class InversionNode;
struct index_desc;

//----------------------------
// GIN Query Context for Record Source
//----------------------------
struct GinScanContext {
	// Query parameters
	GinQueryType query_type;			// Type of full-text query
	ScratchBird::string search_text;	// Text to search for
	TokenList search_tokens;			// Extracted search tokens
	double similarity_threshold;		// For similarity queries
	USHORT proximity_distance;			// For phrase/proximity queries
	
	// Execution parameters
	GinExecutionStrategy strategy;		// Execution strategy
	bool enable_optimization;			// Enable query optimization
	bool enable_caching;				// Enable result caching
	
	// Statistics
	GinQueryStats query_stats;			// Query execution statistics
	
	GinScanContext() :
		query_type(GIN_QUERY_CONTAINS),
		similarity_threshold(0.7),
		proximity_distance(5),
		strategy(GIN_STRATEGY_HYBRID),
		enable_optimization(true),
		enable_caching(true)
	{
		memset(&query_stats, 0, sizeof(query_stats));
	}
};

//----------------------------
// GIN Table Scan - Record source for full-text queries using GIN indexes
//----------------------------
class GinTableScan final : public RecordStream
{
	struct Impure : public RecordSource::Impure
	{
		RecordBitmap* irsb_gin_bitmap;			// Bitmap of matching records
		RecordBitmap::iterator* irsb_gin_iterator; // Iterator for bitmap traversal
		GinQueryResultIterator* irsb_result_iterator; // GIN-specific result iterator
		GinScanContext* irsb_scan_context;		// GIN scan context
		ULONG irsb_records_read;				// Number of records read
		ULONG irsb_records_rejected;			// Number of records rejected by filter
		bool irsb_eof_reached;					// End of file reached
		bool irsb_bitmap_computed;				// Bitmap has been computed
	};

public:
	//----------------------------
	// Constructor
	//----------------------------
	GinTableScan(CompilerScratch* csb, const ScratchBird::string& alias,
				 StreamType stream, jrd_rel* relation,
				 const index_desc* gin_index, InversionNode* inversion,
				 BoolExprNode* contains_condition, double selectivity);

	//----------------------------
	// RecordSource interface
	//----------------------------
	void close(thread_db* tdbb) const override;
	void getLegacyPlan(thread_db* tdbb, ScratchBird::string& plan, unsigned level) const override;

protected:
	void internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const override;
	void internalOpen(thread_db* tdbb) const override;
	bool internalGetRecord(thread_db* tdbb) const override;

public:
	//----------------------------
	// GIN-specific interface
	//----------------------------
	
	// Set full-text search parameters
	void setSearchText(const ScratchBird::string& search_text);
	void setQueryType(GinQueryType query_type);
	void setSimilarityThreshold(double threshold);
	void setProximityDistance(USHORT distance);
	void setExecutionStrategy(GinExecutionStrategy strategy);
	
	// Enable/disable features
	void enableQueryOptimization(bool enable);
	void enableResultCaching(bool enable);
	
	// Statistics and monitoring
	GinQueryStats getLastQueryStats() const;
	ULONG getRecordsRead() const;
	ULONG getRecordsRejected() const;
	double getQuerySelectivity() const;
	
	// Index information
	const index_desc* getGinIndex() const { return m_gin_index; }
	ScratchBird::string getIndexName() const;

private:
	//----------------------------
	// Private methods
	//----------------------------
	
	// Query execution
	bool executeFullTextQuery(thread_db* tdbb, Impure* impure) const;
	bool initializeGinContext(thread_db* tdbb, Impure* impure) const;
	bool computeResultBitmap(thread_db* tdbb, GinScanContext* context) const;
	
	// Record navigation
	bool fetchNextRecord(thread_db* tdbb, Impure* impure) const;
	bool positionOnRecord(thread_db* tdbb, RecordNumber record_number) const;
	bool validateRecord(thread_db* tdbb) const;
	
	// Query parsing and preparation
	TokenList extractSearchTokens(const ScratchBird::string& search_text) const;
	GinQueryType inferQueryType(const BoolExprNode* condition) const;
	ScratchBird::string extractSearchValue(const BoolExprNode* condition) const;
	
	// Optimization and planning
	GinExecutionStrategy chooseOptimalStrategy(const TokenList& tokens) const;
	double estimateQuerySelectivity(const TokenList& tokens) const;
	ULONG estimateResultSize(const TokenList& tokens) const;
	
	// Integration with Firebird's bitmap system
	RecordBitmap* createFirebirdBitmap(thread_db* tdbb, const TokenList& tokens) const;
	void convertGinBitmapToFirebird(thread_db* tdbb, RecordBitmap* gin_bitmap, RecordBitmap* fb_bitmap) const;
	
	// Error handling and cleanup
	void handleQueryError(thread_db* tdbb, const ScratchBird::Exception& ex) const;
	void cleanupGinContext(Impure* impure) const;
	void cleanupBitmaps(Impure* impure) const;
	
	// Statistics collection
	void updateQueryStatistics(Impure* impure, const GinQueryStats& stats) const;
	void recordCacheHit() const;
	void recordCacheMiss() const;

private:
	//----------------------------
	// Member variables
	//----------------------------
	const ScratchBird::string m_alias;			// Table alias
	jrd_rel* const m_relation;					// Relation being scanned
	const index_desc* const m_gin_index;		// GIN index descriptor
	NestConst<InversionNode> const m_inversion;	// Index inversion (optional)
	NestConst<BoolExprNode> const m_contains_condition; // CONTAINS condition
	
	// Query parameters (mutable for lazy initialization)
	mutable GinIndex* m_gin_index_impl;			// GIN index implementation
	mutable GinQueryProcessor* m_query_processor; // Query processor
	mutable GinTokenizer* m_tokenizer;			// Text tokenizer
	
	// Configuration
	mutable GinQueryType m_default_query_type;	// Default query type
	mutable GinExecutionStrategy m_default_strategy; // Default execution strategy
	mutable double m_default_similarity_threshold; // Default similarity threshold
	mutable USHORT m_default_proximity_distance; // Default proximity distance
	
	// Statistics (mutable for const methods)
	mutable ULONG m_total_queries;				// Total queries executed
	mutable ULONG m_cache_hits;					// Cache hits
	mutable ULONG m_cache_misses;				// Cache misses
	mutable double m_average_selectivity;		// Average query selectivity
	mutable ULONG m_total_records_scanned;		// Total records scanned
	mutable ULONG m_total_records_returned;		// Total records returned
	
	// Performance monitoring
	mutable ULONG m_query_time_ms;				// Last query execution time
	mutable ULONG m_bitmap_size;				// Last bitmap size
	mutable ULONG m_tokens_processed;			// Tokens processed in last query
};

//----------------------------
// GIN Full-Text Condition Node
//----------------------------
class GinContainsNode : public BoolExprNode
{
public:
	GinContainsNode(MemoryPool& pool, ValueExprNode* field, ValueExprNode* search_value, 
					GinQueryType query_type = GIN_QUERY_CONTAINS);

	// BoolExprNode interface
	BoolExprNode* pass2(thread_db* tdbb, CompilerScratch* csb) override;
	bool execute(thread_db* tdbb, Request* request) const override;
	void getLegacyPlan(thread_db* tdbb, ScratchBird::string& plan, unsigned level) const override;
	
	// GIN-specific interface
	ValueExprNode* getField() const { return m_field; }
	ValueExprNode* getSearchValue() const { return m_search_value; }
	GinQueryType getQueryType() const { return m_query_type; }
	void setQueryType(GinQueryType query_type) { m_query_type = query_type; }
	
	// Index selection support
	bool canUseGinIndex(const index_desc* idx) const;
	double estimateSelectivity(const index_desc* idx) const;

protected:
	void internalGetPlan(thread_db* tdbb, PlanEntry& planEntry, unsigned level, bool recurse) const override;

private:
	NestConst<ValueExprNode> m_field;			// Field being searched
	NestConst<ValueExprNode> m_search_value;	// Search value/text
	GinQueryType m_query_type;					// Type of full-text query
	
	// Cached analysis
	mutable const index_desc* m_cached_index;	// Cached GIN index for this field
	mutable double m_cached_selectivity;		// Cached selectivity estimate
	mutable bool m_analysis_done;				// Analysis has been performed
};

//----------------------------
// GIN Scan Optimizer Interface
//----------------------------
class GinScanOptimizer
{
public:
	// Index selection
	static const index_desc* findBestGinIndex(jrd_rel* relation, const BoolExprNode* condition);
	static bool canUseGinIndex(const index_desc* idx, const BoolExprNode* condition);
	static double estimateGinSelectivity(const index_desc* idx, const BoolExprNode* condition);
	
	// Query optimization
	static GinExecutionStrategy chooseExecutionStrategy(const TokenList& tokens, 
														const index_desc* idx);
	static ULONG estimateQueryCost(const TokenList& tokens, const index_desc* idx);
	static bool shouldUseGinIndex(const BoolExprNode* condition, double table_cardinality);
	
	// Integration with Firebird optimizer
	static RecordSource* createGinTableScan(CompilerScratch* csb, 
											 StreamType stream,
											 jrd_rel* relation,
											 const BoolExprNode* condition,
											 const ScratchBird::string& alias);
	
	// Performance analysis
	static void analyzeGinPerformance(const index_desc* idx, 
									  const GinQueryStats& stats);
	static void updateOptimizerStatistics(const index_desc* idx, 
										   double actual_selectivity);

private:
	// Helper methods
	static TokenList extractTokensFromCondition(const BoolExprNode* condition);
	static GinQueryType inferQueryTypeFromCondition(const BoolExprNode* condition);
	static bool isTextSearchCondition(const BoolExprNode* condition);
	static const index_desc* findGinIndexForField(jrd_rel* relation, USHORT field_id);
};

} // namespace Jrd

#endif // JRD_GIN_TABLE_SCAN_H