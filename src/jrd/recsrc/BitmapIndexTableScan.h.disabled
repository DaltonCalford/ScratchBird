/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapIndexTableScan.h  
 *	DESCRIPTION:	Bitmap index table scan record source for low-cardinality queries
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
 * 2025.07.23 - ScratchBird Bitmap Index Table Scan Implementation
 */

#ifndef JRD_BITMAP_INDEX_TABLE_SCAN_H
#define JRD_BITMAP_INDEX_TABLE_SCAN_H

#include "../jrd/recsrc/RecordSource.h"
#include "../jrd/BitmapIndex.h"
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
class BitmapIndex;
class RecordBitmap;
class BoolExprNode;
class ValueExprNode;
class InversionNode;
struct index_desc;

//----------------------------
// Bitmap Query Types
//----------------------------
enum BitmapQueryType : UCHAR
{
	BITMAP_QUERY_EQUALITY = 0,		// Single value equality: column = value
	BITMAP_QUERY_IN_LIST = 1,		// IN list query: column IN (val1, val2, ...)
	BITMAP_QUERY_RANGE = 2,			// Range query: column BETWEEN val1 AND val2
	BITMAP_QUERY_NULL = 3,			// NULL query: column IS NULL
	BITMAP_QUERY_NOT_NULL = 4,		// NOT NULL query: column IS NOT NULL
	BITMAP_QUERY_MULTI_VALUE = 5	// Multiple value query with OR/AND semantics
};

//----------------------------
// Bitmap Operations for Multiple Queries
//----------------------------
enum BitmapOperation : UCHAR
{
	BITMAP_OP_AND = 0,				// Intersection of bitmaps
	BITMAP_OP_OR = 1,				// Union of bitmaps
	BITMAP_OP_NOT = 2,				// Complement of bitmap
	BITMAP_OP_XOR = 3				// Exclusive OR of bitmaps
};

//----------------------------
// Bitmap Execution Strategies
//----------------------------
enum BitmapExecutionStrategy : UCHAR
{
	BITMAP_STRATEGY_SEQUENTIAL = 0,	// Sequential bitmap processing
	BITMAP_STRATEGY_PARALLEL = 1,	// Parallel bitmap operations
	BITMAP_STRATEGY_HYBRID = 2,		// Adaptive strategy based on data characteristics
	BITMAP_STRATEGY_CHUNKED = 3		// Process bitmaps in memory-efficient chunks
};

//----------------------------
// Bitmap Query Statistics
//----------------------------
struct BitmapQueryStats
{
	ULONG bitmaps_processed;		// Number of bitmaps processed
	ULONG bits_examined;			// Total number of bits examined
	ULONG records_matched;			// Number of records matched
	ULONG compression_savings;		// Bytes saved through compression
	double processing_time;			// Query processing time in seconds
	double compression_ratio;		// Average compression ratio achieved
	ULONG cache_hits;				// Number of cache hits
	ULONG cache_misses;				// Number of cache misses
};

//----------------------------
// Bitmap Index Query Context for Record Source
//----------------------------
struct BitmapIndexScanContext {
	// Query parameters
	dsc search_value;					// Value to search for
	ScratchBird::HalfStaticArray<dsc, 8> search_values; // Multiple values for IN queries
	BitmapQueryType query_type;			// Type of bitmap query (equality, IN, range)
	
	// Bitmap operations
	BitmapOperation bitmap_operation;	// AND, OR, NOT operations on bitmaps
	bool enable_compression;			// Enable bitmap compression during operations
	bool enable_chunk_processing;		// Process bitmaps in chunks for memory efficiency
	
	// Execution parameters
	BitmapExecutionStrategy strategy;	// Execution strategy (sequential, parallel, hybrid)
	ULONG chunk_size;					// Chunk size for bitmap processing
	bool enable_optimization;			// Enable query optimization
	bool enable_caching;				// Enable result caching
	
	// Statistics
	BitmapQueryStats query_stats;		// Query execution statistics
	
	BitmapIndexScanContext() :
		query_type(BITMAP_QUERY_EQUALITY),
		bitmap_operation(BITMAP_OP_AND),
		enable_compression(true),
		enable_chunk_processing(true),
		strategy(BITMAP_STRATEGY_HYBRID),
		chunk_size(8192),
		enable_optimization(true),
		enable_caching(true)
	{
		memset(&search_value, 0, sizeof(search_value));
		memset(&query_stats, 0, sizeof(query_stats));
	}
};

//----------------------------
// Bitmap Index Table Scan - Record source for low-cardinality queries using bitmap indexes
//----------------------------
class BitmapIndexTableScan final : public RecordStream
{
	struct Impure : public RecordSource::Impure
	{
		RecordBitmap* irsb_bitmap_result;		// Final result bitmap
		RecordBitmap::iterator* irsb_bitmap_iterator; // Iterator for bitmap traversal
		ScratchBird::HalfStaticArray<RecordBitmap*, 8> irsb_temp_bitmaps; // Temporary bitmaps for operations
		BitmapIndexScanContext* irsb_scan_context; // Bitmap scan context
		ULONG irsb_records_read;				// Number of records read
		ULONG irsb_records_rejected;			// Number of records rejected by filter
		bool irsb_eof_reached;					// End of file reached
		bool irsb_bitmap_computed;				// Result bitmap has been computed
		bool irsb_compression_enabled;			// Compression is enabled for this scan
	};

public:
	//----------------------------
	// Constructor
	//----------------------------
	BitmapIndexTableScan(CompilerScratch* csb, const ScratchBird::string& alias,
						StreamType stream, jrd_rel* relation,
						const index_desc* bitmap_index, InversionNode* inversion,
						BoolExprNode* condition, double selectivity);

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
	// Bitmap-specific interface
	//----------------------------
	
	// Set bitmap query parameters
	void setSearchValue(const dsc* search_value);
	void setSearchValues(const ScratchBird::HalfStaticArray<dsc, 8>& search_values);
	void setQueryType(BitmapQueryType query_type);
	void setBitmapOperation(BitmapOperation operation);
	void setExecutionStrategy(BitmapExecutionStrategy strategy);
	void setChunkSize(ULONG chunk_size);
	
	// Enable/disable features
	void enableCompression(bool enable);
	void enableChunkProcessing(bool enable);
	void enableQueryOptimization(bool enable);
	void enableResultCaching(bool enable);
	
	// Statistics and monitoring
	BitmapQueryStats getLastQueryStats() const;
	ULONG getRecordsRead() const;
	ULONG getRecordsRejected() const;
	double getQuerySelectivity() const;
	double getCompressionRatio() const;
	
	// Index information
	const index_desc* getBitmapIndex() const { return m_bitmap_index; }
	ScratchBird::string getIndexName() const;
	ULONG getCurrentCardinality() const;
	double getCardinalityRatio() const;

private:
	//----------------------------
	// Private methods
	//----------------------------
	
	// Query execution
	void executeQuery(thread_db* tdbb, Impure* impure) const;
	void executeEqualityQuery(thread_db* tdbb, Impure* impure) const;
	void executeInListQuery(thread_db* tdbb, Impure* impure) const;
	void executeRangeQuery(thread_db* tdbb, Impure* impure) const;
	void executeNullQuery(thread_db* tdbb, Impure* impure) const;
	void executeNotNullQuery(thread_db* tdbb, Impure* impure) const;
	void executeMultiValueQuery(thread_db* tdbb, Impure* impure) const;
	
	// Bitmap operations
	RecordBitmap* getBitmapForValue(thread_db* tdbb, const dsc* value) const;
	RecordBitmap* combineBitmaps(thread_db* tdbb, 
								const ScratchBird::HalfStaticArray<RecordBitmap*, 8>& bitmaps,
								BitmapOperation operation) const;
	void optimizeBitmap(thread_db* tdbb, RecordBitmap* bitmap) const;
	
	// Compression and optimization
	void compressBitmap(thread_db* tdbb, RecordBitmap* bitmap) const;
	void applyChunkedProcessing(thread_db* tdbb, RecordBitmap* bitmap, ULONG chunk_size) const;
	bool shouldUseCompression(thread_db* tdbb, ULONG bitmap_size) const;
	BitmapExecutionStrategy selectOptimalStrategy(thread_db* tdbb, ULONG expected_results) const;
	
	// Memory management
	void allocateWorkingBitmaps(thread_db* tdbb, Impure* impure, ULONG count) const;
	void deallocateWorkingBitmaps(thread_db* tdbb, Impure* impure) const;
	void resetScanContext(thread_db* tdbb, Impure* impure) const;
	
	// Statistics collection
	void updateQueryStatistics(thread_db* tdbb, Impure* impure, 
							  const BitmapQueryStats& stats) const;
	void collectBitmapStatistics(thread_db* tdbb, const RecordBitmap* bitmap,
								BitmapQueryStats& stats) const;
	
	// Error handling
	void handleBitmapError(thread_db* tdbb, const Exception& error) const;
	void validateBitmapConsistency(thread_db* tdbb, const RecordBitmap* bitmap) const;
	
	// Index access
	BitmapIndex* getBitmapIndexInstance(thread_db* tdbb) const;
	void validateIndexSuitability(thread_db* tdbb, const dsc* search_value) const;
	ULONG estimateResultSize(thread_db* tdbb, const dsc* search_value) const;

private:
	//----------------------------
	// Member variables
	//----------------------------
	
	// Index and relation information
	const index_desc* m_bitmap_index;		// Bitmap index descriptor
	jrd_rel* m_relation;					// Target relation
	StreamType m_stream;					// Stream number
	ScratchBird::string m_alias;			// Table alias
	
	// Query condition and inversion
	InversionNode* m_inversion;				// Index inversion node
	BoolExprNode* m_condition;				// Filter condition
	double m_selectivity;					// Expected selectivity
	
	// Execution configuration
	BitmapIndexScanContext m_default_context; // Default scan context
	mutable MemoryPool* m_pool;				// Memory pool for allocations
	
	// Performance monitoring
	mutable ULONG m_total_queries;			// Total number of queries executed
	mutable ULONG m_cache_hits;				// Total cache hits
	mutable ULONG m_cache_misses;			// Total cache misses
	mutable double m_average_selectivity;	// Running average selectivity
	
	// Configuration constants
	static const ULONG DEFAULT_CHUNK_SIZE = 8192;
	static const double MIN_COMPRESSION_THRESHOLD = 0.1;
	static const ULONG MAX_WORKING_BITMAPS = 16;
	static const double PARALLEL_THRESHOLD_RECORDS = 10000.0;
};

//----------------------------
// Bitmap Index Table Scan Factory
//----------------------------
class BitmapIndexTableScanFactory
{
public:
	// Create bitmap table scan for different query types  
	static BitmapIndexTableScan* createEqualityScan(CompilerScratch* csb,
													const ScratchBird::string& alias,
													StreamType stream, jrd_rel* relation,
													const index_desc* bitmap_index,
													const dsc* search_value,
													double selectivity);
											  
	static BitmapIndexTableScan* createInListScan(CompilerScratch* csb,
												  const ScratchBird::string& alias,
												  StreamType stream, jrd_rel* relation,
												  const index_desc* bitmap_index,
												  const ScratchBird::HalfStaticArray<dsc, 8>& search_values,
												  double selectivity);
	
	static BitmapIndexTableScan* createRangeScan(CompilerScratch* csb,
												 const ScratchBird::string& alias,
												 StreamType stream, jrd_rel* relation,
												 const index_desc* bitmap_index,
												 const dsc* min_value, const dsc* max_value,
												 double selectivity);
	
	// Utility methods
	static bool isBitmapIndexSuitable(thread_db* tdbb, const index_desc* index_desc,
									 BitmapQueryType query_type, ULONG expected_cardinality);
	static double estimateBitmapSelectivity(thread_db* tdbb, const index_desc* index_desc,
										   const dsc* search_value);
	static BitmapExecutionStrategy recommendStrategy(thread_db* tdbb, 
													const index_desc* index_desc,
													ULONG expected_results);
};

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_TABLE_SCAN_H