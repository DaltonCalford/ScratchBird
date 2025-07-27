/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		HashIndexPartitioner.h
 *	DESCRIPTION:	Hash index partitioning for very large datasets
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
 * 2025.07.24 - ScratchBird Hash Index Partitioning Implementation
 */

#ifndef JRD_HASH_INDEX_PARTITIONER_H
#define JRD_HASH_INDEX_PARTITIONER_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/ThreadData.h"
#include "PersistentHashStorage.h"
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class Database;
struct index_desc;
class PersistentHashStorage;
struct hash_entry;

//----------------------------
// Partitioning Constants
//----------------------------

inline constexpr ULONG DEFAULT_PARTITION_COUNT = 16;         // Default number of partitions
inline constexpr ULONG MAX_PARTITION_COUNT = 1024;           // Maximum partitions per index
inline constexpr ULONG MIN_PARTITION_SIZE_MB = 64;           // Minimum partition size
inline constexpr ULONG MAX_PARTITION_SIZE_MB = 4096;         // Maximum partition size
inline constexpr ULONG PARTITION_REBALANCE_THRESHOLD = 80;   // Rebalance when utilization exceeds %
inline constexpr ULONG MIN_ENTRIES_PER_PARTITION = 100000;   // Minimum entries to justify partitioning

//----------------------------
// Partitioning Strategy
//----------------------------

enum PartitioningStrategy : UCHAR
{
    PARTITION_STRATEGY_HASH_BASED = 0,      // Hash-based partitioning
    PARTITION_STRATEGY_RANGE_BASED = 1,     // Range-based partitioning
    PARTITION_STRATEGY_DIRECTORY_BASED = 2,  // Directory-based partitioning
    PARTITION_STRATEGY_CONSISTENT_HASH = 3,  // Consistent hashing
    PARTITION_STRATEGY_ADAPTIVE = 4,         // Adaptive partitioning
    PARTITION_STRATEGY_TEMPORAL = 5,         // Time-based partitioning
    PARTITION_STRATEGY_SIZE_BASED = 6,       // Size-based partitioning
    PARTITION_STRATEGY_HYBRID = 7            // Hybrid strategy
};

//----------------------------
// Partition Distribution Mode
//----------------------------

enum PartitionDistributionMode : UCHAR
{
    DISTRIBUTION_UNIFORM = 0,               // Even distribution across partitions
    DISTRIBUTION_WEIGHTED = 1,              // Weighted distribution based on capacity
    DISTRIBUTION_LOCALITY_AWARE = 2,        // Distribution aware of data locality
    DISTRIBUTION_PERFORMANCE_OPTIMIZED = 3, // Optimized for query performance
    DISTRIBUTION_LOAD_BALANCED = 4          // Dynamic load balancing
};

//----------------------------
// Partition Metadata
//----------------------------

struct PartitionInfo
{
    ULONG partition_id;                     // Unique partition identifier
    ScratchBird::string partition_name;     // Human-readable partition name
    
    // Storage information
    ULONG storage_page_start;               // Starting page for this partition
    ULONG storage_page_count;               // Number of pages allocated
    ULONG storage_used_bytes;               // Bytes currently used
    ULONG storage_total_bytes;              // Total bytes allocated
    
    // Entry statistics
    ULONG entry_count;                      // Number of entries in partition
    ULONG bucket_count;                     // Number of hash buckets
    ULONG max_entries_per_bucket;           // Maximum entries per bucket
    double average_entries_per_bucket;      // Average entries per bucket
    
    // Hash range information
    ULONG hash_range_start;                 // Starting hash value (inclusive)
    ULONG hash_range_end;                   // Ending hash value (exclusive)
    double hash_range_utilization;          // How much of hash range is used
    
    // Performance metrics
    ULONG lookup_operations;                // Number of lookup operations
    ULONG insert_operations;                // Number of insert operations  
    ULONG delete_operations;                // Number of delete operations
    double average_operation_time_ms;       // Average operation response time
    GDS_TIMESTAMP last_access_time;         // Last access timestamp
    
    // Load balancing information
    double load_factor;                     // Current load factor (0.0-1.0)
    bool is_hot_partition;                  // True if receives heavy traffic
    bool needs_rebalancing;                 // True if requires rebalancing
    ULONG rebalance_priority;               // Priority for rebalancing (higher = more urgent)
    
    // Maintenance information
    GDS_TIMESTAMP creation_time;            // When partition was created
    GDS_TIMESTAMP last_maintenance_time;    // Last maintenance operation
    ULONG maintenance_operations_count;     // Number of maintenance operations
    bool is_online;                         // True if partition is online
    bool is_read_only;                      // True if partition is read-only
    
    PartitionInfo()
        : partition_id(0), storage_page_start(0), storage_page_count(0),
          storage_used_bytes(0), storage_total_bytes(0), entry_count(0),
          bucket_count(0), max_entries_per_bucket(0), average_entries_per_bucket(0.0),
          hash_range_start(0), hash_range_end(0), hash_range_utilization(0.0),
          lookup_operations(0), insert_operations(0), delete_operations(0),
          average_operation_time_ms(0.0), last_access_time(0), load_factor(0.0),
          is_hot_partition(false), needs_rebalancing(false), rebalance_priority(0),
          creation_time(0), last_maintenance_time(0), maintenance_operations_count(0),
          is_online(true), is_read_only(false)
    {
    }
    
    // Utility methods
    double getUtilizationPercentage() const {
        return storage_total_bytes > 0 ? 
               (static_cast<double>(storage_used_bytes) / storage_total_bytes) * 100.0 : 0.0;
    }
    
    bool isOverloaded() const {
        return getUtilizationPercentage() > PARTITION_REBALANCE_THRESHOLD;
    }
    
    bool canAcceptMoreEntries() const {
        return is_online && !is_read_only && getUtilizationPercentage() < 95.0;
    }
    
    ULONG getHashRangeSize() const {
        return hash_range_end - hash_range_start;
    }
};

//----------------------------
// Partitioning Configuration
//----------------------------

struct PartitioningConfiguration
{
    // Basic partitioning settings
    PartitioningStrategy strategy;          // Partitioning strategy to use
    PartitionDistributionMode distribution_mode; // How to distribute data
    ULONG target_partition_count;           // Desired number of partitions
    ULONG max_partition_count;              // Maximum allowed partitions
    
    // Size thresholds
    ULONG min_partition_size_mb;            // Minimum partition size
    ULONG max_partition_size_mb;            // Maximum partition size
    ULONG target_entries_per_partition;     // Target entries per partition
    ULONG max_entries_per_partition;        // Maximum entries per partition
    
    // Performance tuning
    double rebalance_threshold;             // Rebalance when utilization exceeds this
    ULONG rebalance_check_interval_ms;      // How often to check for rebalancing
    bool enable_automatic_rebalancing;      // Enable automatic rebalancing
    bool enable_partition_merging;          // Enable merging of small partitions
    bool enable_partition_splitting;        // Enable splitting of large partitions
    
    // Load balancing
    bool enable_load_balancing;             // Enable load balancing
    double load_imbalance_threshold;        // Trigger load balancing at this imbalance
    ULONG hot_partition_threshold;          // Operations/sec to consider partition "hot"
    bool enable_adaptive_partitioning;     // Adapt partitioning based on access patterns
    
    // Concurrency settings
    bool enable_parallel_operations;        // Enable parallel partition operations
    ULONG max_concurrent_operations;        // Maximum concurrent operations per partition
    bool enable_partition_locking;          // Enable fine-grained partition locking
    ULONG operation_timeout_ms;             // Timeout for partition operations
    
    // Maintenance settings
    bool enable_background_maintenance;     // Enable background maintenance
    ULONG maintenance_check_interval_ms;    // Maintenance check interval
    ULONG partition_reorganize_threshold;   // Reorganize when fragmentation exceeds this
    bool enable_partition_compression;      // Enable partition-level compression
    
    PartitioningConfiguration()
        : strategy(PARTITION_STRATEGY_HASH_BASED),
          distribution_mode(DISTRIBUTION_UNIFORM),
          target_partition_count(DEFAULT_PARTITION_COUNT),
          max_partition_count(MAX_PARTITION_COUNT),
          min_partition_size_mb(MIN_PARTITION_SIZE_MB),
          max_partition_size_mb(MAX_PARTITION_SIZE_MB),
          target_entries_per_partition(1000000),
          max_entries_per_partition(5000000),
          rebalance_threshold(0.8),
          rebalance_check_interval_ms(60000),
          enable_automatic_rebalancing(true),
          enable_partition_merging(true),
          enable_partition_splitting(true),
          enable_load_balancing(true),
          load_imbalance_threshold(0.3),
          hot_partition_threshold(1000),
          enable_adaptive_partitioning(true),
          enable_parallel_operations(true),
          max_concurrent_operations(4),
          enable_partition_locking(true),
          operation_timeout_ms(30000),
          enable_background_maintenance(true),
          maintenance_check_interval_ms(300000),
          partition_reorganize_threshold(30),
          enable_partition_compression(false)
    {
    }
};

//----------------------------
// Partition Key Router
//----------------------------

/**
 * Routes keys to appropriate partitions based on partitioning strategy
 */
class PartitionKeyRouter
{
public:
    explicit PartitionKeyRouter(MemoryPool* pool);
    ~PartitionKeyRouter();

    // Routing methods
    ULONG routeKeyToPartition(const UCHAR* key_data, USHORT key_length, 
                             const std::vector<PartitionInfo>& partitions) const;
    
    std::vector<ULONG> routeKeyToPartitions(const UCHAR* key_data, USHORT key_length,
                                           const std::vector<PartitionInfo>& partitions,
                                           ULONG max_partitions = 1) const;
    
    // Range routing for range queries
    std::vector<ULONG> routeRangeToPartitions(const UCHAR* start_key, USHORT start_key_length,
                                             const UCHAR* end_key, USHORT end_key_length,
                                             const std::vector<PartitionInfo>& partitions) const;
    
    // Strategy-specific routing
    ULONG routeWithHashStrategy(const UCHAR* key_data, USHORT key_length,
                               const std::vector<PartitionInfo>& partitions) const;
    
    ULONG routeWithRangeStrategy(const UCHAR* key_data, USHORT key_length,
                                const std::vector<PartitionInfo>& partitions) const;
    
    ULONG routeWithConsistentHashStrategy(const UCHAR* key_data, USHORT key_length,
                                         const std::vector<PartitionInfo>& partitions) const;
    
    ULONG routeWithDirectoryStrategy(const UCHAR* key_data, USHORT key_length,
                                    const std::vector<PartitionInfo>& partitions) const;
    
    // Configuration
    void setPartitioningStrategy(PartitioningStrategy strategy);
    PartitioningStrategy getPartitioningStrategy() const;
    
    // Statistics and optimization
    struct RoutingStatistics {
        ULONG total_routing_operations;      // Total routing operations
        ULONG single_partition_routes;       // Routes to single partition
        ULONG multi_partition_routes;        // Routes to multiple partitions
        double average_partitions_per_route; // Average partitions per route
        std::map<ULONG, ULONG> partition_access_counts; // Access count per partition
        
        RoutingStatistics()
            : total_routing_operations(0), single_partition_routes(0),
              multi_partition_routes(0), average_partitions_per_route(1.0)
        {
        }
    };
    
    RoutingStatistics getRoutingStatistics() const;
    void resetStatistics();

private:
    MemoryPool* m_pool;
    PartitioningStrategy m_strategy;
    
    // Statistics
    mutable RoutingStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Consistent hashing ring for consistent hash strategy
    struct ConsistentHashNode {
        ULONG hash_value;
        ULONG partition_id;
        
        ConsistentHashNode(ULONG hash, ULONG partition) 
            : hash_value(hash), partition_id(partition) {}
        
        bool operator<(const ConsistentHashNode& other) const {
            return hash_value < other.hash_value;
        }
    };
    
    mutable std::vector<ConsistentHashNode> m_consistent_hash_ring;
    mutable std::mutex m_ring_mutex;
    
    // Directory for directory-based strategy
    mutable std::map<ULONG, ULONG> m_key_directory;
    mutable std::mutex m_directory_mutex;
    
    // Helper methods
    ULONG calculateKeyHash(const UCHAR* key_data, USHORT key_length) const;
    void updateConsistentHashRing(const std::vector<PartitionInfo>& partitions) const;
    void updateStatistics(ULONG partition_count) const;
    
    // Range calculation helpers
    bool isKeyInRange(const UCHAR* key_data, USHORT key_length,
                     const UCHAR* start_key, USHORT start_key_length,
                     const UCHAR* end_key, USHORT end_key_length) const;
};

//----------------------------
// Partition Manager
//----------------------------

/**
 * Manages the lifecycle and operations of hash index partitions
 */
class HashIndexPartitionManager
{
public:
    explicit HashIndexPartitionManager(MemoryPool* pool);
    ~HashIndexPartitionManager();

    // Partition lifecycle
    bool initializePartitioning(thread_db* tdbb, const index_desc* idx,
                               const PartitioningConfiguration& config);
    
    void shutdownPartitioning(thread_db* tdbb, const index_desc* idx);
    
    // Partition management
    ULONG createPartition(thread_db* tdbb, const index_desc* idx,
                         ULONG hash_range_start, ULONG hash_range_end);
    
    bool dropPartition(thread_db* tdbb, const index_desc* idx, ULONG partition_id);
    
    bool mergePartitions(thread_db* tdbb, const index_desc* idx,
                        const std::vector<ULONG>& partition_ids, ULONG target_partition_id);
    
    bool splitPartition(thread_db* tdbb, const index_desc* idx, ULONG partition_id,
                       ULONG split_point);
    
    // Partition access
    std::vector<PartitionInfo> getPartitions(const index_desc* idx) const;
    PartitionInfo* getPartition(const index_desc* idx, ULONG partition_id);
    const PartitionInfo* getPartition(const index_desc* idx, ULONG partition_id) const;
    
    // Data operations across partitions
    bool insertEntry(thread_db* tdbb, const index_desc* idx, hash_entry* entry);
    bool deleteEntry(thread_db* tdbb, const index_desc* idx, const UCHAR* key_data, USHORT key_length);
    hash_entry* lookupEntry(thread_db* tdbb, const index_desc* idx, const UCHAR* key_data, USHORT key_length);
    
    // Batch operations
    std::vector<bool> insertEntries(thread_db* tdbb, const index_desc* idx,
                                   const std::vector<hash_entry*>& entries);
    
    std::vector<hash_entry*> lookupEntries(thread_db* tdbb, const index_desc* idx,
                                          const std::vector<std::pair<const UCHAR*, USHORT>>& keys);
    
    // Range operations
    std::vector<hash_entry*> scanRange(thread_db* tdbb, const index_desc* idx,
                                      const UCHAR* start_key, USHORT start_key_length,
                                      const UCHAR* end_key, USHORT end_key_length);
    
    // Load balancing and rebalancing
    bool performRebalancing(thread_db* tdbb, const index_desc* idx);
    bool isRebalancingNeeded(const index_desc* idx) const;
    
    void performLoadBalancing(thread_db* tdbb, const index_desc* idx);
    
    // Configuration management
    void setPartitioningConfiguration(const index_desc* idx, const PartitioningConfiguration& config);
    PartitioningConfiguration getPartitioningConfiguration(const index_desc* idx) const;
    
    // Statistics and monitoring
    struct PartitioningStatistics {
        ULONG total_partitions;              // Total number of partitions
        ULONG active_partitions;             // Number of active partitions
        ULONG total_entries;                 // Total entries across all partitions
        ULONG total_storage_bytes;           // Total storage used
        double average_utilization;          // Average partition utilization
        double load_imbalance_factor;        // Load imbalance across partitions
        ULONG rebalancing_operations;        // Number of rebalancing operations
        ULONG partition_splits;              // Number of partition splits
        ULONG partition_merges;              // Number of partition merges
        
        PartitioningStatistics()
            : total_partitions(0), active_partitions(0), total_entries(0),
              total_storage_bytes(0), average_utilization(0.0),
              load_imbalance_factor(0.0), rebalancing_operations(0),
              partition_splits(0), partition_merges(0)
        {
        }
    };
    
    PartitioningStatistics getPartitioningStatistics(const index_desc* idx) const;
    
    // Health monitoring
    bool isPartitioningHealthy(const index_desc* idx) const;
    ScratchBird::string getPartitioningHealthReport(const index_desc* idx) const;

private:
    MemoryPool* m_pool;
    
    // Per-index partition information
    struct IndexPartitionInfo {
        const index_desc* index;
        std::vector<PartitionInfo> partitions;
        PartitioningConfiguration configuration;
        std::unique_ptr<PartitionKeyRouter> key_router;
        
        // Statistics
        PartitioningStatistics statistics;
        GDS_TIMESTAMP last_rebalancing_time;
        GDS_TIMESTAMP last_maintenance_time;
        
        // Synchronization
        mutable std::mutex partition_mutex;
        
        IndexPartitionInfo(const index_desc* idx, MemoryPool* pool)
            : index(idx), key_router(std::make_unique<PartitionKeyRouter>(pool)),
              last_rebalancing_time(0), last_maintenance_time(0)
        {
        }
    };
    
    std::map<USHORT, std::unique_ptr<IndexPartitionInfo>> m_index_partitions;
    mutable std::mutex m_partitions_mutex;
    
    // Background maintenance
    std::atomic<bool> m_maintenance_active;
    void maintenanceThreadProc();
    
    // Partition operations implementation
    bool createInitialPartitions(thread_db* tdbb, IndexPartitionInfo* partition_info);
    
    bool redistributeEntries(thread_db* tdbb, IndexPartitionInfo* partition_info,
                            const std::vector<ULONG>& affected_partitions);
    
    // Rebalancing algorithms
    bool performHashBasedRebalancing(thread_db* tdbb, IndexPartitionInfo* partition_info);
    bool performSizeBasedRebalancing(thread_db* tdbb, IndexPartitionInfo* partition_info);
    bool performLoadBasedRebalancing(thread_db* tdbb, IndexPartitionInfo* partition_info);
    
    // Partition storage management
    PersistentHashStorage* getPartitionStorage(const index_desc* idx, ULONG partition_id);
    bool allocatePartitionStorage(thread_db* tdbb, const index_desc* idx, ULONG partition_id,
                                 ULONG initial_size_mb);
    
    // Statistics update
    void updatePartitioningStatistics(IndexPartitionInfo* partition_info);
    void updatePartitionStatistics(PartitionInfo* partition, const hash_entry* entry, bool is_insert);
    
    // Helper methods
    IndexPartitionInfo* getIndexPartitionInfo(const index_desc* idx);
    const IndexPartitionInfo* getIndexPartitionInfo(const index_desc* idx) const;
    
    ULONG generatePartitionId() const;
    bool validatePartitioningConfiguration(const PartitioningConfiguration& config) const;
    
    // Load balancing helpers
    double calculateLoadImbalance(const std::vector<PartitionInfo>& partitions) const;
    std::vector<ULONG> identifyOverloadedPartitions(const std::vector<PartitionInfo>& partitions) const;
    std::vector<ULONG> identifyUnderloadedPartitions(const std::vector<PartitionInfo>& partitions) const;
    
    // Maintenance operations
    void performPartitionMaintenance(thread_db* tdbb, IndexPartitionInfo* partition_info);
    bool shouldMergePartitions(const PartitionInfo& partition1, const PartitionInfo& partition2) const;
    bool shouldSplitPartition(const PartitionInfo& partition) const;
};

//----------------------------
// Cross-Partition Query Coordinator
//----------------------------

/**
 * Coordinates queries that span multiple partitions
 */
class CrossPartitionQueryCoordinator
{
public:
    explicit CrossPartitionQueryCoordinator(MemoryPool* pool);
    ~CrossPartitionQueryCoordinator();

    // Query coordination
    struct QueryResult {
        std::vector<hash_entry*> entries;    // Result entries
        ULONG total_entries_examined;        // Total entries examined
        ULONG partitions_accessed;           // Number of partitions accessed
        double query_execution_time_ms;      // Total execution time
        bool was_optimized;                  // True if query was optimized
        
        QueryResult()
            : total_entries_examined(0), partitions_accessed(0),
              query_execution_time_ms(0.0), was_optimized(false)
        {
        }
    };
    
    // Point queries
    QueryResult executePointQuery(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                                 const index_desc* idx, const UCHAR* key_data, USHORT key_length);
    
    // Range queries
    QueryResult executeRangeQuery(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                                 const index_desc* idx,
                                 const UCHAR* start_key, USHORT start_key_length,
                                 const UCHAR* end_key, USHORT end_key_length);
    
    // Batch queries
    QueryResult executeBatchQuery(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                                 const index_desc* idx,
                                 const std::vector<std::pair<const UCHAR*, USHORT>>& keys);
    
    // Aggregation queries
    struct AggregationResult {
        ULONG total_count;                   // Total number of matching entries
        double sum_value;                    // Sum of numeric values (if applicable)
        double avg_value;                    // Average value
        double min_value;                    // Minimum value
        double max_value;                    // Maximum value
        ULONG partitions_involved;           // Number of partitions involved
        
        AggregationResult()
            : total_count(0), sum_value(0.0), avg_value(0.0),
              min_value(0.0), max_value(0.0), partitions_involved(0)
        {
        }
    };
    
    AggregationResult executeAggregationQuery(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                                             const index_desc* idx,
                                             const std::function<bool(const hash_entry*)>& predicate,
                                             const std::function<double(const hash_entry*)>& value_extractor);
    
    // Parallel query execution
    void setParallelExecutionEnabled(bool enabled);
    void setMaxParallelQueries(ULONG max_parallel);
    
    // Query optimization
    void enableQueryOptimization(bool enabled);
    void setQueryOptimizationLevel(ULONG level); // 1=basic, 2=medium, 3=aggressive
    
    // Statistics
    struct CoordinatorStatistics {
        ULONG total_queries_executed;        // Total queries executed
        ULONG single_partition_queries;      // Queries hitting single partition
        ULONG multi_partition_queries;       // Queries hitting multiple partitions
        double average_partitions_per_query; // Average partitions accessed per query
        double average_query_time_ms;        // Average query execution time
        ULONG optimized_queries;             // Number of optimized queries
        
        CoordinatorStatistics()
            : total_queries_executed(0), single_partition_queries(0),
              multi_partition_queries(0), average_partitions_per_query(1.0),
              average_query_time_ms(0.0), optimized_queries(0)
        {
        }
    };
    
    CoordinatorStatistics getStatistics() const;
    void resetStatistics();

private:
    MemoryPool* m_pool;
    
    // Configuration
    bool m_parallel_execution_enabled;
    ULONG m_max_parallel_queries;
    bool m_query_optimization_enabled;
    ULONG m_optimization_level;
    
    // Statistics
    mutable CoordinatorStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Parallel execution
    struct ParallelQueryTask {
        ULONG partition_id;
        std::function<std::vector<hash_entry*>()> query_function;
        std::vector<hash_entry*> result;
        bool completed;
        std::string error_message;
        
        ParallelQueryTask(ULONG pid, std::function<std::vector<hash_entry*>()> func)
            : partition_id(pid), query_function(func), completed(false)
        {
        }
    };
    
    // Query execution helpers
    std::vector<hash_entry*> executeParallelPartitionQueries(
        const std::vector<std::unique_ptr<ParallelQueryTask>>& tasks);
    
    std::vector<hash_entry*> executeSequentialPartitionQueries(
        const std::vector<std::unique_ptr<ParallelQueryTask>>& tasks);
    
    // Query optimization
    std::vector<ULONG> optimizePartitionAccessOrder(const std::vector<ULONG>& partition_ids,
                                                    const std::vector<PartitionInfo>& partitions) const;
    
    bool canOptimizeQuery(const std::vector<ULONG>& partition_ids) const;
    
    // Result aggregation
    std::vector<hash_entry*> mergeQueryResults(const std::vector<std::vector<hash_entry*>>& results) const;
    AggregationResult aggregateResults(const std::vector<std::vector<hash_entry*>>& results,
                                      const std::function<double(const hash_entry*)>& value_extractor) const;
    
    // Statistics update
    void updateQueryStatistics(ULONG partitions_accessed, double execution_time_ms, bool was_optimized);
};

//----------------------------
// Partition Migration Manager
//----------------------------

/**
 * Manages migration of data between partitions during rebalancing
 */
class PartitionMigrationManager
{
public:
    explicit PartitionMigrationManager(MemoryPool* pool);
    ~PartitionMigrationManager();

    // Migration operations
    struct MigrationPlan {
        struct MigrationTask {
            ULONG source_partition_id;       // Source partition
            ULONG target_partition_id;       // Target partition
            ULONG estimated_entries;         // Estimated entries to migrate
            ULONG estimated_size_mb;         // Estimated data size to migrate
            ULONG priority;                  // Migration priority
            
            MigrationTask()
                : source_partition_id(0), target_partition_id(0),
                  estimated_entries(0), estimated_size_mb(0), priority(0)
            {
            }
        };
        
        std::vector<MigrationTask> tasks;    // Migration tasks
        ULONG total_estimated_time_ms;       // Total estimated migration time
        bool requires_index_rebuild;        // True if requires index rebuild
        
        MigrationPlan() : total_estimated_time_ms(0), requires_index_rebuild(false) {}
    };
    
    MigrationPlan createMigrationPlan(thread_db* tdbb, const index_desc* idx,
                                     const std::vector<PartitionInfo>& current_partitions,
                                     const std::vector<PartitionInfo>& target_partitions);
    
    bool executeMigrationPlan(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                             const index_desc* idx, const MigrationPlan& plan);
    
    // Individual migration operations
    bool migratePartitionData(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                             const index_desc* idx, ULONG source_partition_id, ULONG target_partition_id,
                             const std::function<bool(const hash_entry*)>& should_migrate);
    
    bool balancePartitionLoad(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                             const index_desc* idx, const std::vector<ULONG>& overloaded_partitions,
                             const std::vector<ULONG>& underloaded_partitions);
    
    // Migration monitoring
    struct MigrationStatistics {
        ULONG migration_operations_count;    // Number of migration operations
        ULONG entries_migrated;              // Total entries migrated
        ULONG bytes_migrated;                // Total bytes migrated
        double average_migration_time_ms;    // Average migration time
        ULONG failed_migrations;             // Number of failed migrations
        double success_rate;                 // Migration success rate
        
        MigrationStatistics()
            : migration_operations_count(0), entries_migrated(0), bytes_migrated(0),
              average_migration_time_ms(0.0), failed_migrations(0), success_rate(1.0)
        {
        }
    };
    
    MigrationStatistics getMigrationStatistics() const;
    void resetStatistics();
    
    // Configuration
    void setMaxConcurrentMigrations(ULONG max_concurrent);
    void setMigrationBatchSize(ULONG batch_size);
    void setMigrationTimeout(ULONG timeout_ms);

private:
    MemoryPool* m_pool;
    
    // Configuration
    ULONG m_max_concurrent_migrations;
    ULONG m_migration_batch_size;
    ULONG m_migration_timeout_ms;
    
    // Statistics
    mutable MigrationStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Migration state tracking
    struct MigrationSession {
        ULONG session_id;
        const index_desc* index;
        MigrationPlan plan;
        std::atomic<ULONG> completed_tasks;
        std::atomic<ULONG> failed_tasks;
        std::atomic<bool> is_active;
        GDS_TIMESTAMP start_time;
        
        MigrationSession(const index_desc* idx) 
            : session_id(0), index(idx), completed_tasks(0), failed_tasks(0),
              is_active(true), start_time(0)
        {
        }
    };
    
    std::map<ULONG, std::unique_ptr<MigrationSession>> m_active_migrations;
    mutable std::mutex m_migrations_mutex;
    ULONG m_next_session_id;
    
    // Helper methods
    ULONG createMigrationSession(const index_desc* idx, const MigrationPlan& plan);
    void completeMigrationSession(ULONG session_id, bool success);
    
    bool executeMigrationTask(thread_db* tdbb, HashIndexPartitionManager* partition_manager,
                             const MigrationPlan::MigrationTask& task, MigrationSession* session);
    
    ULONG estimateMigrationTime(const MigrationPlan::MigrationTask& task) const;
    bool validateMigrationPlan(const MigrationPlan& plan) const;
    
    void updateMigrationStatistics(ULONG entries_migrated, ULONG bytes_migrated,
                                  double migration_time_ms, bool success);
};

//----------------------------
// Global Partition Manager
//----------------------------

/**
 * Global manager for hash index partitioning across all databases
 */
class GlobalHashPartitionManager
{
public:
    static GlobalHashPartitionManager* getInstance();
    
    // Global management
    void registerDatabase(Database* database);
    void unregisterDatabase(Database* database);
    
    HashIndexPartitionManager* getPartitionManagerForDatabase(Database* database);
    
    // Global operations
    void performGlobalRebalancing();
    void optimizeGlobalPartitioning();
    
    // Global configuration
    void setGlobalPartitioningConfig(const PartitioningConfiguration& config);
    PartitioningConfiguration getGlobalPartitioningConfig() const;
    
    // Global statistics
    struct GlobalPartitioningStatistics {
        ULONG total_databases;               // Number of registered databases
        ULONG total_partitioned_indexes;     // Total partitioned indexes
        ULONG total_partitions;              // Total partitions across all databases
        ULONG total_entries;                 // Total entries across all partitions
        double global_average_utilization;   // Global average partition utilization
        ULONG global_rebalancing_operations; // Global rebalancing operations
        
        GlobalPartitioningStatistics()
            : total_databases(0), total_partitioned_indexes(0), total_partitions(0),
              total_entries(0), global_average_utilization(0.0),
              global_rebalancing_operations(0)
        {
        }
    };
    
    GlobalPartitioningStatistics getGlobalStatistics() const;
    
    // Resource management
    ULONG getGlobalPartitionCount() const;
    ULONG getMaxGlobalPartitions() const;
    void setMaxGlobalPartitions(ULONG max_partitions);

private:
    GlobalHashPartitionManager();
    ~GlobalHashPartitionManager();
    
    static GlobalHashPartitionManager* s_instance;
    static std::mutex s_instance_mutex;
    
    struct DatabasePartitionManager {
        Database* database;
        std::unique_ptr<HashIndexPartitionManager> partition_manager;
        std::unique_ptr<CrossPartitionQueryCoordinator> query_coordinator;
        std::unique_ptr<PartitionMigrationManager> migration_manager;
        
        DatabasePartitionManager(Database* db, MemoryPool* pool)
            : database(db),
              partition_manager(std::make_unique<HashIndexPartitionManager>(pool)),
              query_coordinator(std::make_unique<CrossPartitionQueryCoordinator>(pool)),
              migration_manager(std::make_unique<PartitionMigrationManager>(pool))
        {
        }
    };
    
    std::vector<DatabasePartitionManager> m_database_managers;
    mutable std::mutex m_managers_mutex;
    
    PartitioningConfiguration m_global_config;
    ULONG m_max_global_partitions;
    
    // Global monitoring
    void globalMonitoringThreadProc();
    bool m_global_monitoring_active;
    
    DatabasePartitionManager* findManagerForDatabase(Database* database);
    void cleanupUnusedManagers();
    void updateGlobalStatistics();
};

//----------------------------
// Integration with Hash Index System
//----------------------------

/**
 * Integration layer for hash index partitioning
 */
class HashPartitioningIntegration
{
public:
    // System initialization
    static void initializePartitioning();
    static void shutdownPartitioning();
    
    // Index integration
    static bool enablePartitioning(thread_db* tdbb, const index_desc* idx,
                                  const PartitioningConfiguration& config = PartitioningConfiguration());
    
    static bool disablePartitioning(thread_db* tdbb, const index_desc* idx);
    static bool isPartitioningEnabled(const index_desc* idx);
    
    // Operation hooks
    static bool shouldUsePartitioning(const index_desc* idx, ULONG estimated_size_mb);
    
    // Partitioned operations
    static bool partitionedInsert(thread_db* tdbb, const index_desc* idx, hash_entry* entry);
    static bool partitionedDelete(thread_db* tdbb, const index_desc* idx, const UCHAR* key_data, USHORT key_length);
    static hash_entry* partitionedLookup(thread_db* tdbb, const index_desc* idx, const UCHAR* key_data, USHORT key_length);
    
    // Maintenance hooks
    static void onIndexMaintenanceRequired(thread_db* tdbb, const index_desc* idx);
    static void onSystemResourcesLow();
    static void onSystemResourcesAvailable();

private:
    static std::map<USHORT, PartitioningConfiguration> s_index_partitioning_configs;
    static std::set<USHORT> s_partitioned_indexes;
    static std::mutex s_integration_mutex;
    static bool s_partitioning_initialized;
};

//----------------------------
// Utility Functions
//----------------------------

// Partitioning decision utilities
bool shouldPartitionIndex(const index_desc* idx, ULONG current_size_mb, ULONG entry_count);

ULONG calculateOptimalPartitionCount(ULONG total_entries, ULONG target_entries_per_partition,
                                    ULONG max_partitions = MAX_PARTITION_COUNT);

PartitioningStrategy selectOptimalPartitioningStrategy(ULONG entry_count, ULONG key_size,
                                                      bool has_range_queries, bool has_temporal_patterns);

// Configuration optimization
PartitioningConfiguration optimizePartitioningConfiguration(const index_desc* idx,
                                                           ULONG estimated_entries,
                                                           const std::vector<ULONG>& access_patterns);

// Health monitoring utilities
bool isPartitioningHealthy(const std::vector<PartitionInfo>& partitions);
double calculatePartitioningEfficiency(const std::vector<PartitionInfo>& partitions);
ScratchBird::string generatePartitioningHealthReport(const std::vector<PartitionInfo>& partitions);

// Load analysis utilities
double calculateLoadImbalanceFactor(const std::vector<PartitionInfo>& partitions);
std::vector<ULONG> identifyHotPartitions(const std::vector<PartitionInfo>& partitions, ULONG threshold);
std::vector<ULONG> identifyUnderUtilizedPartitions(const std::vector<PartitionInfo>& partitions, double threshold);

// Migration planning utilities
ULONG estimatePartitionMigrationTime(const PartitionInfo& source, const PartitionInfo& target, ULONG entries_to_migrate);
bool canPartitionsBeMerged(const PartitionInfo& partition1, const PartitionInfo& partition2, ULONG max_partition_size);
std::pair<ULONG, ULONG> calculateOptimalSplitPoint(const PartitionInfo& partition);

} // namespace Jrd

#endif // JRD_HASH_INDEX_PARTITIONER_H