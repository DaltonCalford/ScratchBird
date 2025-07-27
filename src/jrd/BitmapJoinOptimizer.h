/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		BitmapJoinOptimizer.h
 *	DESCRIPTION:	Bitmap join optimization algorithms for multi-table queries
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
 * 2025.07.23 - ScratchBird Bitmap Join Optimization
 */

#ifndef JRD_BITMAP_JOIN_OPTIMIZER_H
#define JRD_BITMAP_JOIN_OPTIMIZER_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "BitmapIndex.h"
#include "../jrd/optimizer/opt_proto.h"
#include <vector>
#include <memory>
#include <map>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class CompilerScratch;
class jrd_rel;
struct index_desc;
class BoolExprNode;
class ValueExprNode;
class RecordSource;
class CompressedBitmap;

//----------------------------
// Bitmap Join Strategy Types
//----------------------------

enum BitmapJoinStrategy : UCHAR
{
    BITMAP_JOIN_STAR = 0,           // Star join (fact table + dimension tables)
    BITMAP_JOIN_CHAIN = 1,          // Chain join (sequential table joins)
    BITMAP_JOIN_SNOWFLAKE = 2,      // Snowflake join (hierarchical dimensions)
    BITMAP_JOIN_HASH = 3,           // Hash-based bitmap join
    BITMAP_JOIN_NESTED_LOOP = 4,    // Nested loop with bitmap filtering
    BITMAP_JOIN_MERGE = 5,          // Merge join with bitmap acceleration
    BITMAP_JOIN_PARALLEL = 6        // Parallel bitmap join processing
};

//----------------------------
// Bitmap Join Node Types
//----------------------------

enum BitmapJoinNodeType : UCHAR
{
    BITMAP_NODE_FACT = 0,           // Fact table (large, many-to-one joins)
    BITMAP_NODE_DIMENSION = 1,      // Dimension table (small, one-to-many joins)
    BITMAP_NODE_BRIDGE = 2,         // Bridge table (many-to-many relationships)
    BITMAP_NODE_LOOKUP = 3          // Lookup table (reference data)
};

//----------------------------
// Join Condition Analysis
//----------------------------

struct BitmapJoinCondition
{
    jrd_rel* left_relation;         // Left table in join
    jrd_rel* right_relation;        // Right table in join
    USHORT left_field_id;           // Left join field
    USHORT right_field_id;          // Right join field
    const index_desc* left_index;   // Left table bitmap index (nullable)
    const index_desc* right_index;  // Right table bitmap index (nullable)
    double selectivity;             // Estimated join selectivity
    ULONG estimated_result_size;    // Estimated result cardinality
    bool is_bitmap_eligible;        // True if bitmap optimization applicable
    
    BitmapJoinCondition()
        : left_relation(nullptr), right_relation(nullptr),
          left_field_id(0), right_field_id(0),
          left_index(nullptr), right_index(nullptr),
          selectivity(1.0), estimated_result_size(0),
          is_bitmap_eligible(false)
    {
    }
};

//----------------------------
// Bitmap Join Plan Node
//----------------------------

struct BitmapJoinPlanNode
{
    jrd_rel* relation;              // Table for this node
    BitmapJoinNodeType node_type;   // Node type classification
    std::vector<const index_desc*> bitmap_indexes; // Available bitmap indexes
    std::vector<BitmapJoinCondition> join_conditions; // Join conditions
    CompressedBitmap* filter_bitmap; // Pre-computed filter bitmap
    double table_selectivity;       // Overall table selectivity
    ULONG estimated_cardinality;    // Estimated result cardinality
    bool requires_materialization;  // True if temp results needed
    
    BitmapJoinPlanNode()
        : relation(nullptr), node_type(BITMAP_NODE_FACT),
          filter_bitmap(nullptr), table_selectivity(1.0),
          estimated_cardinality(0), requires_materialization(false)
    {
    }
};

//----------------------------
// Bitmap Join Execution Plan
//----------------------------

struct BitmapJoinExecutionPlan
{
    BitmapJoinStrategy strategy;            // Selected join strategy
    std::vector<BitmapJoinPlanNode> nodes;  // Ordered execution nodes
    std::vector<ULONG> execution_order;     // Order of node execution
    double estimated_cost;                  // Total estimated cost
    ULONG estimated_result_size;            // Final result size estimate
    bool uses_parallel_processing;          // True if parallel execution
    ULONG parallel_worker_count;            // Number of parallel workers
    
    BitmapJoinExecutionPlan()
        : strategy(BITMAP_JOIN_STAR), estimated_cost(0.0),
          estimated_result_size(0), uses_parallel_processing(false),
          parallel_worker_count(1)
    {
    }
};

//----------------------------
// Join Statistics and Metrics
//----------------------------

struct BitmapJoinMetrics
{
    ULONG bitmap_and_operations;       // Number of bitmap AND operations
    ULONG bitmap_or_operations;        // Number of bitmap OR operations
    ULONG bitmap_intersections;        // Number of bitmap intersections
    ULONG records_filtered;            // Records eliminated by bitmap filtering
    ULONG records_processed;           // Total records processed
    double bitmap_selectivity;         // Overall bitmap selectivity
    ULONG execution_time_ms;           // Total execution time
    ULONG bitmap_processing_time_ms;   // Time spent on bitmap operations
    ULONG disk_io_operations;          // Disk I/O operations performed
    ULONG memory_usage_kb;             // Peak memory usage
    
    BitmapJoinMetrics()
        : bitmap_and_operations(0), bitmap_or_operations(0),
          bitmap_intersections(0), records_filtered(0),
          records_processed(0), bitmap_selectivity(1.0),
          execution_time_ms(0), bitmap_processing_time_ms(0),
          disk_io_operations(0), memory_usage_kb(0)
    {
    }
};

//----------------------------
// Bitmap Join Optimizer Engine
//----------------------------

/**
 * Main optimizer for bitmap-based join operations
 */
class BitmapJoinOptimizer
{
public:
    explicit BitmapJoinOptimizer(MemoryPool* pool);
    ~BitmapJoinOptimizer();

    // Main optimization interface
    BitmapJoinExecutionPlan optimizeJoin(thread_db* tdbb, CompilerScratch* csb,
                                        const std::vector<jrd_rel*>& relations,
                                        const std::vector<BoolExprNode*>& conditions);
    
    // Join strategy analysis
    BitmapJoinStrategy analyzeBestStrategy(const std::vector<jrd_rel*>& relations,
                                         const std::vector<BitmapJoinCondition>& conditions) const;
    
    // Node classification and analysis
    BitmapJoinNodeType classifyRelation(thread_db* tdbb, jrd_rel* relation,
                                       const std::vector<BitmapJoinCondition>& conditions) const;
    
    std::vector<BitmapJoinPlanNode> createJoinPlan(thread_db* tdbb,
                                                   const std::vector<jrd_rel*>& relations,
                                                   const std::vector<BitmapJoinCondition>& conditions,
                                                   BitmapJoinStrategy strategy) const;
    
    // Cost estimation
    double estimateJoinCost(thread_db* tdbb, const BitmapJoinExecutionPlan& plan) const;
    
    double estimateBitmapJoinCost(const BitmapJoinPlanNode& left_node,
                                 const BitmapJoinPlanNode& right_node,
                                 const BitmapJoinCondition& condition) const;
    
    // Execution plan optimization
    std::vector<ULONG> optimizeExecutionOrder(const std::vector<BitmapJoinPlanNode>& nodes) const;
    
    void optimizeParallelExecution(BitmapJoinExecutionPlan& plan) const;
    
    // Join condition analysis
    std::vector<BitmapJoinCondition> analyzeJoinConditions(thread_db* tdbb,
                                                          const std::vector<jrd_rel*>& relations,
                                                          const std::vector<BoolExprNode*>& conditions) const;
    
    bool isBitmapJoinEligible(thread_db* tdbb, const BitmapJoinCondition& condition) const;
    
    // Bitmap index selection
    const index_desc* selectBestBitmapIndex(thread_db* tdbb, jrd_rel* relation,
                                           USHORT field_id,
                                           const BitmapJoinCondition& condition) const;
    
    std::vector<const index_desc*> findBitmapIndexes(thread_db* tdbb, jrd_rel* relation) const;

private:
    MemoryPool* m_pool;
    
    // Strategy-specific optimizers
    BitmapJoinExecutionPlan optimizeStarJoin(thread_db* tdbb,
                                            const std::vector<jrd_rel*>& relations,
                                            const std::vector<BitmapJoinCondition>& conditions) const;
    
    BitmapJoinExecutionPlan optimizeChainJoin(thread_db* tdbb,
                                             const std::vector<jrd_rel*>& relations,
                                             const std::vector<BitmapJoinCondition>& conditions) const;
    
    BitmapJoinExecutionPlan optimizeSnowflakeJoin(thread_db* tdbb,
                                                 const std::vector<jrd_rel*>& relations,
                                                 const std::vector<BitmapJoinCondition>& conditions) const;
    
    BitmapJoinExecutionPlan optimizeHashJoin(thread_db* tdbb,
                                            const std::vector<jrd_rel*>& relations,
                                            const std::vector<BitmapJoinCondition>& conditions) const;
    
    // Cost model helpers
    double calculateBitmapIntersectionCost(ULONG left_cardinality, ULONG right_cardinality) const;
    double calculateBitmapUnionCost(ULONG left_cardinality, ULONG right_cardinality) const;
    double calculateMaterializationCost(ULONG cardinality) const;
    double calculateParallelProcessingBonus(ULONG cardinality, ULONG worker_count) const;
    
    // Relation analysis helpers
    ULONG estimateRelationCardinality(thread_db* tdbb, jrd_rel* relation) const;
    double estimateFieldSelectivity(thread_db* tdbb, jrd_rel* relation, USHORT field_id) const;
    bool isFactTable(thread_db* tdbb, jrd_rel* relation,
                    const std::vector<BitmapJoinCondition>& conditions) const;
    bool isDimensionTable(thread_db* tdbb, jrd_rel* relation,
                         const std::vector<BitmapJoinCondition>& conditions) const;
    
    // Join graph analysis
    struct JoinGraph
    {
        std::vector<jrd_rel*> nodes;                    // Relations (graph nodes)
        std::vector<std::vector<ULONG>> adjacency;      // Adjacency matrix
        std::vector<BitmapJoinCondition> edges;         // Join conditions (graph edges)
        
        bool isStarPattern() const;
        bool isChainPattern() const;
        bool isSnowflakePattern() const;
        jrd_rel* findCentralNode() const;
        std::vector<jrd_rel*> findLeafNodes() const;
    };
    
    JoinGraph buildJoinGraph(const std::vector<jrd_rel*>& relations,
                            const std::vector<BitmapJoinCondition>& conditions) const;
    
    BitmapJoinStrategy determineStrategyFromGraph(const JoinGraph& graph) const;
};

//----------------------------
// Bitmap Join Executor
//----------------------------

/**
 * Executes optimized bitmap join plans
 */
class BitmapJoinExecutor
{
public:
    explicit BitmapJoinExecutor(MemoryPool* pool);
    ~BitmapJoinExecutor();

    // Main execution interface
    RecordSource* executeJoinPlan(thread_db* tdbb, const BitmapJoinExecutionPlan& plan,
                                 CompilerScratch* csb);
    
    // Strategy-specific execution
    RecordSource* executeStarJoin(thread_db* tdbb, const BitmapJoinExecutionPlan& plan,
                                 CompilerScratch* csb);
    
    RecordSource* executeChainJoin(thread_db* tdbb, const BitmapJoinExecutionPlan& plan,
                                  CompilerScratch* csb);
    
    RecordSource* executeSnowflakeJoin(thread_db* tdbb, const BitmapJoinExecutionPlan& plan,
                                      CompilerScratch* csb);
    
    RecordSource* executeHashJoin(thread_db* tdbb, const BitmapJoinExecutionPlan& plan,
                                 CompilerScratch* csb);
    
    // Bitmap filtering operations
    CompressedBitmap* createFilterBitmap(thread_db* tdbb, const BitmapJoinPlanNode& node,
                                        const std::vector<BoolExprNode*>& conditions);
    
    CompressedBitmap* intersectBitmaps(const std::vector<CompressedBitmap*>& bitmaps);
    
    CompressedBitmap* unionBitmaps(const std::vector<CompressedBitmap*>& bitmaps);
    
    // Parallel execution support
    RecordSource* executeParallelJoin(thread_db* tdbb, const BitmapJoinExecutionPlan& plan,
                                     CompilerScratch* csb);
    
    struct ParallelWorkerContext
    {
        thread_db* worker_tdbb;
        const BitmapJoinExecutionPlan* plan;
        CompilerScratch* csb;
        ULONG start_partition;
        ULONG end_partition;
        CompressedBitmap* result_bitmap;
        BitmapJoinMetrics metrics;
        
        ParallelWorkerContext()
            : worker_tdbb(nullptr), plan(nullptr), csb(nullptr),
              start_partition(0), end_partition(0), result_bitmap(nullptr)
        {
        }
    };
    
    static void parallelWorkerThread(ParallelWorkerContext* context);
    
    // Metrics collection
    BitmapJoinMetrics getExecutionMetrics() const;
    void resetMetrics();

private:
    MemoryPool* m_pool;
    BitmapJoinMetrics m_execution_metrics;
    mutable ScratchBird::Mutex m_metrics_mutex;
    
    // Bitmap management
    std::vector<CompressedBitmap*> m_temp_bitmaps;  // Temporary bitmaps for cleanup
    
    // Execution helpers
    RecordSource* createBitmapFilteredTableScan(thread_db* tdbb, jrd_rel* relation,
                                               CompressedBitmap* filter_bitmap,
                                               CompilerScratch* csb);
    
    RecordSource* createBitmapJoinRecordSource(thread_db* tdbb,
                                              const BitmapJoinPlanNode& left_node,
                                              const BitmapJoinPlanNode& right_node,
                                              const BitmapJoinCondition& condition,
                                              CompilerScratch* csb);
    
    // Bitmap operation implementations
    CompressedBitmap* performBitmapJoin(thread_db* tdbb,
                                       CompressedBitmap* left_bitmap,
                                       CompressedBitmap* right_bitmap,
                                       const BitmapJoinCondition& condition);
    
    void applyBitmapFilters(thread_db* tdbb, BitmapJoinPlanNode& node,
                           const std::vector<BoolExprNode*>& conditions);
    
    // Memory management
    void cleanupTempBitmaps();
    void addTempBitmap(CompressedBitmap* bitmap);
    
    // Performance monitoring
    void startOperationTimer();
    ULONG stopOperationTimer();
    void recordBitmapOperation(ULONG operation_count, ULONG processing_time_ms);
    
    GDS_TIMESTAMP m_operation_start_time;
};

//----------------------------
// Bitmap Join Record Source
//----------------------------

/**
 * Record source that implements bitmap-optimized joins
 */
class BitmapJoinRecordSource : public RecordSource
{
public:
    BitmapJoinRecordSource(CompilerScratch* csb, const BitmapJoinExecutionPlan& plan);
    virtual ~BitmapJoinRecordSource();

    // RecordSource interface
    virtual void open(thread_db* tdbb) override;
    virtual void close(thread_db* tdbb) override;
    virtual bool getRecord(thread_db* tdbb) override;
    virtual bool refetchRecord(thread_db* tdbb) override;
    virtual WriteLockResult lockRecord(thread_db* tdbb, bool skipLocked = false) override;
    virtual void markRecursive() override;
    virtual void invalidateRecords(jrd_req* request) override;
    
    // Cost and statistics
    virtual double getCost() const override;
    virtual ULONG getCardinality() const override;
    
    // Bitmap join specific methods
    void setBitmapFilters(const std::vector<CompressedBitmap*>& filters);
    CompressedBitmap* getCurrentResultBitmap() const;
    BitmapJoinMetrics getJoinMetrics() const;

private:
    CompilerScratch* m_csb;
    BitmapJoinExecutionPlan m_plan;
    std::unique_ptr<BitmapJoinExecutor> m_executor;
    
    // Execution state
    std::vector<RecordSource*> m_child_sources;
    std::vector<CompressedBitmap*> m_filter_bitmaps;
    CompressedBitmap* m_current_result_bitmap;
    bool m_is_open;
    ULONG m_current_position;
    
    // Performance tracking
    BitmapJoinMetrics m_join_metrics;
    
    // Internal helpers
    void initializeChildSources(thread_db* tdbb);
    void applyBitmapFiltering(thread_db* tdbb);
    bool fetchNextRecord(thread_db* tdbb);
    void updateJoinMetrics();
};

//----------------------------
// Join Plan Cache
//----------------------------

/**
 * Cache for reusing optimized bitmap join plans
 */
class BitmapJoinPlanCache
{
public:
    static BitmapJoinPlanCache* getInstance();
    
    // Plan caching
    void cachePlan(const ScratchBird::string& query_signature,
                   const BitmapJoinExecutionPlan& plan);
    
    BitmapJoinExecutionPlan* findCachedPlan(const ScratchBird::string& query_signature);
    
    void invalidatePlansForRelation(jrd_rel* relation);
    void clearCache();
    
    // Statistics
    struct CacheStatistics
    {
        ULONG total_lookups;
        ULONG cache_hits;
        ULONG cache_misses;
        ULONG plan_invalidations;
        ULONG cached_plan_count;
        
        double getHitRatio() const
        {
            return total_lookups > 0 ? static_cast<double>(cache_hits) / total_lookups : 0.0;
        }
        
        CacheStatistics()
            : total_lookups(0), cache_hits(0), cache_misses(0),
              plan_invalidations(0), cached_plan_count(0)
        {
        }
    };
    
    CacheStatistics getCacheStatistics() const;
    void resetStatistics();

private:
    BitmapJoinPlanCache();
    ~BitmapJoinPlanCache();
    
    static BitmapJoinPlanCache* s_instance;
    static ScratchBird::Mutex s_instance_mutex;
    
    struct CacheEntry
    {
        ScratchBird::string query_signature;
        BitmapJoinExecutionPlan plan;
        std::vector<jrd_rel*> referenced_relations;
        GDS_TIMESTAMP creation_time;
        GDS_TIMESTAMP last_access_time;
        ULONG access_count;
        
        CacheEntry() : creation_time(0), last_access_time(0), access_count(0) {}
    };
    
    std::vector<CacheEntry> m_cached_plans;
    mutable ScratchBird::Mutex m_cache_mutex;
    mutable CacheStatistics m_statistics;
    
    static constexpr ULONG MAX_CACHED_PLANS = 1000;
    static constexpr ULONG CACHE_CLEANUP_THRESHOLD = 1200;
    
    // Cache management
    void evictOldEntries();
    void evictLeastUsedEntries();
    ScratchBird::string generateQuerySignature(const std::vector<jrd_rel*>& relations,
                                              const std::vector<BoolExprNode*>& conditions) const;
};

//----------------------------
// Integration with Query Optimizer
//----------------------------

/**
 * Integration points with ScratchBird's query optimizer
 */
class BitmapJoinOptimizerIntegration
{
public:
    // Optimizer hook registration
    static void registerBitmapJoinOptimizer();
    static void unregisterBitmapJoinOptimizer();
    
    // Cost model integration
    static double calculateBitmapJoinCost(thread_db* tdbb, CompilerScratch* csb,
                                         const std::vector<jrd_rel*>& relations,
                                         const std::vector<BoolExprNode*>& conditions);
    
    static bool shouldUseBitmapJoin(thread_db* tdbb, CompilerScratch* csb,
                                   const std::vector<jrd_rel*>& relations,
                                   const std::vector<BoolExprNode*>& conditions);
    
    // Plan generation hooks
    static RecordSource* createBitmapJoinRecordSource(thread_db* tdbb, CompilerScratch* csb,
                                                     const std::vector<jrd_rel*>& relations,
                                                     const std::vector<BoolExprNode*>& conditions);
    
    // Statistics collection
    static void updateJoinStatistics(const BitmapJoinMetrics& metrics);
    static BitmapJoinMetrics getGlobalJoinStatistics();

private:
    static std::unique_ptr<BitmapJoinOptimizer> s_global_optimizer;
    static BitmapJoinMetrics s_global_metrics;
    static ScratchBird::Mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Bitmap join eligibility testing
bool canUseBitmapJoin(thread_db* tdbb, const std::vector<jrd_rel*>& relations,
                     const std::vector<BoolExprNode*>& conditions);

// Join pattern detection
BitmapJoinStrategy detectJoinPattern(const std::vector<jrd_rel*>& relations,
                                   const std::vector<BitmapJoinCondition>& conditions);

// Cardinality estimation helpers
ULONG estimateJoinCardinality(thread_db* tdbb, const BitmapJoinCondition& condition);
double estimateJoinSelectivity(thread_db* tdbb, const BitmapJoinCondition& condition);

// Index analysis utilities
std::vector<const index_desc*> findJoinEligibleBitmapIndexes(thread_db* tdbb, jrd_rel* relation);
bool isBitmapIndexSuitableForJoin(thread_db* tdbb, const index_desc* index,
                                 const BitmapJoinCondition& condition);

} // namespace Jrd

#endif // JRD_BITMAP_JOIN_OPTIMIZER_H