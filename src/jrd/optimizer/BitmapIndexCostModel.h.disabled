/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapIndexCostModel.h
 *	DESCRIPTION:	Cost model for bitmap index operations and query optimization
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
 * 2025.07.23 - ScratchBird Bitmap Index Cost Model Implementation
 */

#ifndef JRD_BITMAP_INDEX_COST_MODEL_H
#define JRD_BITMAP_INDEX_COST_MODEL_H

#include "../jrd/constants.h"
#include "../jrd/recsrc/BitmapIndexTableScan.h"
#include "../jrd/IndexType.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"

namespace Jrd {

// Forward declarations
class thread_db;
struct index_desc;
class jrd_rel;
class dsc;
class BitmapIndex;
class CompilerScratch;
class ValueExprNode;
class BoolExprNode;

//----------------------------
// Bitmap Index Cost Factors
//----------------------------
struct BitmapIndexCostFactors
{
	// Base costs
	double bitmap_page_read_cost;		// Cost to read a bitmap page
	double bitmap_creation_cost;		// Cost to create a bitmap from index
	double bitmap_intersection_cost;	// Cost to intersect two bitmaps
	double bitmap_union_cost;			// Cost to union two bitmaps
	double bitmap_compression_cost;		// Cost to compress/decompress bitmaps
	
	// Cardinality-related costs
	double low_cardinality_bonus;		// Bonus for low cardinality (< 10%)
	double high_cardinality_penalty;	// Penalty for high cardinality (> 50%)
	double cardinality_threshold_low;	// Low cardinality threshold (0.1 = 10%)
	double cardinality_threshold_high;	// High cardinality threshold (0.5 = 50%)
	
	// Operation-specific costs
	double equality_scan_cost;			// Cost for equality scans
	double in_list_scan_cost_base;		// Base cost for IN list scans
	double in_list_scan_cost_per_value;	// Additional cost per value in IN list
	double null_scan_cost;				// Cost for NULL/NOT NULL scans
	double range_scan_cost;				// Cost for range scans (if supported)
	
	// Memory and compression costs
	double memory_allocation_cost;		// Cost to allocate bitmap memory
	double compression_cpu_cost;		// CPU cost for compression/decompression
	double compression_io_savings;		// IO cost savings from compression
	double bitmap_cache_hit_cost;		// Cost when bitmap is cached
	double bitmap_cache_miss_cost;		// Cost when bitmap must be loaded
	
	BitmapIndexCostFactors()
	{
		// Initialize with realistic cost estimates
		bitmap_page_read_cost = 1.0;
		bitmap_creation_cost = 0.5;
		bitmap_intersection_cost = 0.1;
		bitmap_union_cost = 0.1;
		bitmap_compression_cost = 0.2;
		
		low_cardinality_bonus = 0.5;		// 50% cost reduction for low cardinality
		high_cardinality_penalty = 2.0;	// 2x cost increase for high cardinality
		cardinality_threshold_low = 0.1;	// 10%
		cardinality_threshold_high = 0.5;	// 50%
		
		equality_scan_cost = 1.0;
		in_list_scan_cost_base = 1.2;
		in_list_scan_cost_per_value = 0.1;
		null_scan_cost = 0.8;
		range_scan_cost = 3.0;				// Expensive for bitmap indexes
		
		memory_allocation_cost = 0.1;
		compression_cpu_cost = 0.05;
		compression_io_savings = 0.3;
		bitmap_cache_hit_cost = 0.1;
		bitmap_cache_miss_cost = 1.5;
	}
};

//----------------------------
// Bitmap Index Statistics for Cost Calculation
//----------------------------
struct BitmapIndexStatistics
{
	ULONG total_records;				// Total records in relation
	ULONG distinct_values;				// Number of distinct values in indexed column
	double cardinality_ratio;			// Distinct values / total records
	ULONG average_bitmap_size;			// Average compressed bitmap size
	ULONG largest_bitmap_size;			// Largest bitmap size
	double compression_ratio;			// Average compression ratio achieved
	ULONG cache_hit_rate;				// Cache hit rate percentage (0-100)
	
	// Value distribution statistics
	ULONG null_count;					// Number of NULL values
	ULONG most_frequent_value_count;	// Count of most frequent value
	ULONG least_frequent_value_count;	// Count of least frequent value
	double value_distribution_skew;		// Measure of value distribution skewness
	
	BitmapIndexStatistics()
	{
		memset(this, 0, sizeof(BitmapIndexStatistics));
		compression_ratio = 0.7;		// Default 70% compression
		cache_hit_rate = 80;			// Default 80% cache hit rate
	}
};

//----------------------------
// Query Cost Estimation Result
//----------------------------
struct BitmapQueryCostEstimate
{
	double total_cost;					// Total estimated cost
	double io_cost;						// I/O related costs
	double cpu_cost;					// CPU processing costs
	double memory_cost;					// Memory allocation costs
	
	// Cost breakdown
	double bitmap_access_cost;			// Cost to access bitmaps
	double bitmap_operation_cost;		// Cost for bitmap operations (AND/OR/NOT)
	double record_retrieval_cost;		// Cost to retrieve actual records
	double filtering_cost;				// Cost for additional filtering
	
	// Selectivity and result estimates
	double estimated_selectivity;		// Estimated query selectivity
	ULONG estimated_result_count;		// Estimated number of result records
	ULONG bitmaps_to_process;			// Number of bitmaps to process
	
	// Optimization recommendations
	BitmapExecutionStrategy recommended_strategy;
	bool recommend_compression;
	bool recommend_caching;
	ULONG recommended_chunk_size;
	
	BitmapQueryCostEstimate()
	{
		memset(this, 0, sizeof(BitmapQueryCostEstimate));
		recommended_strategy = BITMAP_STRATEGY_HYBRID;
		recommend_compression = true;
		recommend_caching = true;
		recommended_chunk_size = 8192;
	}
};

//----------------------------
// Main Bitmap Index Cost Model Class
//----------------------------
class BitmapIndexCostModel
{
public:
	BitmapIndexCostModel(MemoryPool* pool);
	~BitmapIndexCostModel();
	
	//----------------------------
	// Cost estimation methods
	//----------------------------
	
	// Main cost estimation entry point
	BitmapQueryCostEstimate estimateQueryCost(thread_db* tdbb,
											  const index_desc* bitmap_index,
											  BitmapQueryType query_type,
											  const dsc* search_value = nullptr,
											  const ScratchBird::HalfStaticArray<dsc, 8>* search_values = nullptr) const;
	
	// Specific query type cost estimations
	double estimateEqualityScanCost(thread_db* tdbb, const index_desc* bitmap_index,
								   const dsc* search_value) const;
	
	double estimateInListScanCost(thread_db* tdbb, const index_desc* bitmap_index,
								 const ScratchBird::HalfStaticArray<dsc, 8>& search_values) const;
	
	double estimateNullScanCost(thread_db* tdbb, const index_desc* bitmap_index) const;
	
	double estimateRangeScanCost(thread_db* tdbb, const index_desc* bitmap_index,
								const dsc* min_value, const dsc* max_value) const;
	
	//----------------------------
	// Selectivity estimation
	//----------------------------
	
	double estimateSelectivity(thread_db* tdbb, const index_desc* bitmap_index,
							  BitmapQueryType query_type, const dsc* search_value) const;
	
	double estimateInListSelectivity(thread_db* tdbb, const index_desc* bitmap_index,
									const ScratchBird::HalfStaticArray<dsc, 8>& search_values) const;
	
	double estimateNullSelectivity(thread_db* tdbb, const index_desc* bitmap_index) const;
	
	//----------------------------
	// Index suitability assessment
	//----------------------------
	
	bool isBitmapIndexSuitable(thread_db* tdbb, const index_desc* bitmap_index,
							  BitmapQueryType query_type, ULONG expected_result_count) const;
	
	bool shouldUseBitmapIndex(thread_db* tdbb, const index_desc* bitmap_index,
							 const index_desc* alternative_index, BitmapQueryType query_type) const;
	
	//----------------------------
	// Comparison with other index types
	//----------------------------
	
	double compareToBTreeIndex(thread_db* tdbb, const index_desc* bitmap_index,
							  const index_desc* btree_index, BitmapQueryType query_type) const;
	
	double compareToHashIndex(thread_db* tdbb, const index_desc* bitmap_index,
							 const index_desc* hash_index, BitmapQueryType query_type) const;
	
	double compareToTableScan(thread_db* tdbb, const index_desc* bitmap_index,
							 const jrd_rel* relation, BitmapQueryType query_type) const;
	
	//----------------------------
	// Strategy recommendations
	//----------------------------
	
	BitmapExecutionStrategy recommendExecutionStrategy(thread_db* tdbb,
													   const index_desc* bitmap_index,
													   ULONG expected_result_count) const;
	
	bool recommendCompression(thread_db* tdbb, const index_desc* bitmap_index,
							 ULONG expected_bitmap_size) const;
	
	ULONG recommendChunkSize(thread_db* tdbb, const index_desc* bitmap_index,
							ULONG available_memory) const;
	
	//----------------------------
	// Statistics and configuration
	//----------------------------
	
	void updateCostFactors(const BitmapIndexCostFactors& factors);
	BitmapIndexCostFactors getCostFactors() const;
	
	void updateIndexStatistics(const index_desc* bitmap_index,
							  const BitmapIndexStatistics& stats);
	
	BitmapIndexStatistics getIndexStatistics(thread_db* tdbb,
											 const index_desc* bitmap_index) const;

private:
	//----------------------------
	// Private implementation methods
	//----------------------------
	
	// Statistics collection
	BitmapIndexStatistics collectIndexStatistics(thread_db* tdbb,
												 const index_desc* bitmap_index) const;
	
	void estimateValueDistribution(thread_db* tdbb, const index_desc* bitmap_index,
								  BitmapIndexStatistics& stats) const;
	
	// Cost calculation helpers
	double calculateBitmapAccessCost(const BitmapIndexStatistics& stats,
									ULONG bitmaps_to_access) const;
	
	double calculateBitmapOperationCost(BitmapOperation operation,
									   ULONG bitmap_count, ULONG average_bitmap_size) const;
	
	double calculateMemoryCost(ULONG total_bitmap_size, bool compression_enabled) const;
	
	double calculateRecordRetrievalCost(ULONG estimated_result_count) const;
	
	// Cardinality analysis
	double calculateCardinalityFactor(double cardinality_ratio) const;
	bool isLowCardinality(double cardinality_ratio) const;
	bool isHighCardinality(double cardinality_ratio) const;
	
	// Value frequency estimation
	ULONG estimateValueFrequency(thread_db* tdbb, const index_desc* bitmap_index,
								const dsc* value) const;
	
	double estimateValueSelectivity(ULONG value_frequency, ULONG total_records) const;
	
	// Cache and compression modeling
	double calculateCacheEffectiveness(const BitmapIndexStatistics& stats) const;
	double calculateCompressionBenefit(const BitmapIndexStatistics& stats) const;
	
	// Optimization strategy analysis
	BitmapExecutionStrategy analyzeOptimalStrategy(const BitmapIndexStatistics& stats,
												  ULONG expected_result_count) const;
	
	// Error handling and validation
	void validateIndexDescriptor(const index_desc* bitmap_index) const;
	void validateQueryParameters(BitmapQueryType query_type, const dsc* search_value) const;

private:
	//----------------------------
	// Member variables
	//----------------------------
	
	MemoryPool* m_pool;
	BitmapIndexCostFactors m_cost_factors;
	
	// Cached statistics for performance
	mutable ScratchBird::ObjectsArray<BitmapIndexStatistics> m_statistics_cache;
	mutable ScratchBird::ObjectsArray<ULONG> m_index_ids;
	
	// Configuration
	static const ULONG MAX_CACHED_STATISTICS = 100;
	static const double DEFAULT_CACHE_HIT_RATIO = 0.8;
	static const double DEFAULT_COMPRESSION_RATIO = 0.7;
	static const ULONG PARALLEL_PROCESSING_THRESHOLD = 10000;
};

//----------------------------
// Bitmap Index Cost Model Factory
//----------------------------
class BitmapIndexCostModelFactory
{
public:
	// Create cost model instances
	static BitmapIndexCostModel* createCostModel(MemoryPool* pool);
	static BitmapIndexCostModel* createOptimizedCostModel(MemoryPool* pool,
														  const BitmapIndexCostFactors& factors);
	
	// Create pre-configured cost models for different scenarios
	static BitmapIndexCostModel* createOLTPCostModel(MemoryPool* pool);
	static BitmapIndexCostModel* createOLAPCostModel(MemoryPool* pool);
	static BitmapIndexCostModel* createMemoryConstrainedCostModel(MemoryPool* pool);
	
	// Utility methods
	static BitmapIndexCostFactors getDefaultCostFactors();
	static BitmapIndexCostFactors getOLTPCostFactors();
	static BitmapIndexCostFactors getOLAPCostFactors();
	static BitmapIndexCostFactors getMemoryConstrainedCostFactors();
};

//----------------------------
// Integration with Query Optimizer
//----------------------------
class BitmapIndexOptimizerIntegration
{
public:
	// Integration with existing optimizer
	static void registerBitmapIndexCostModel(BitmapIndexCostModel* cost_model);
	static BitmapIndexCostModel* getBitmapIndexCostModel();
	
	// Optimizer hooks
	static double calculateBitmapIndexCost(thread_db* tdbb, CompilerScratch* csb,
										  const index_desc* bitmap_index,
										  BoolExprNode* condition);
	
	static bool shouldPreferBitmapIndex(thread_db* tdbb, CompilerScratch* csb,
									   const index_desc* bitmap_index,
									   const index_desc* alternative_index,
									   BoolExprNode* condition);
	
	// Plan generation support
	static ScratchBird::string generateBitmapIndexPlan(thread_db* tdbb,
													   const index_desc* bitmap_index,
													   const BitmapQueryCostEstimate& cost_estimate);

private:
	static BitmapIndexCostModel* s_global_cost_model;
};

} // namespace Jrd

#endif // JRD_BITMAP_INDEX_COST_MODEL_H