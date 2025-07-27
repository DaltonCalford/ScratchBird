/*
 *	PROGRAM:	JRD Access Method
 *	MODULE:		ParallelHashProcessor.h
 *	DESCRIPTION:	Parallel hash operations for multi-threaded processing
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
 * 2025.07.23 - ScratchBird Parallel Hash Processing Implementation
 */

#ifndef JRD_PARALLEL_HASH_PROCESSOR_H
#define JRD_PARALLEL_HASH_PROCESSOR_H

#include "../jrd/constants.h"
#include "../common/classes/array.h"
#include "../common/classes/fb_string.h"
#include "../common/ThreadData.h"
#include "PersistentHashStorage.h"
#include <vector>
#include <memory>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

namespace Jrd {

// Forward declarations
class thread_db;
class MemoryPool;
class Database;
struct index_desc;
class PersistentHashStorage;

//----------------------------
// Parallel Processing Constants
//----------------------------

inline constexpr ULONG DEFAULT_PARALLEL_THREADS = 4;           // Default number of worker threads
inline constexpr ULONG MAX_PARALLEL_THREADS = 32;              // Maximum worker threads
inline constexpr ULONG MIN_PARALLEL_BATCH_SIZE = 1000;         // Minimum batch size for parallelization
inline constexpr ULONG DEFAULT_PARALLEL_BATCH_SIZE = 10000;    // Default batch size
inline constexpr ULONG PARALLEL_QUEUE_SIZE = 1000;             // Work queue size per thread
inline constexpr ULONG PARALLEL_TIMEOUT_MS = 30000;            // Timeout for parallel operations

//----------------------------
// Parallel Operation Types
//----------------------------

enum ParallelOperationType : UCHAR
{
    PARALLEL_INSERT = 0,            // Parallel insert operations
    PARALLEL_LOOKUP = 1,            // Parallel lookup operations
    PARALLEL_DELETE = 2,            // Parallel delete operations
    PARALLEL_UPDATE = 3,            // Parallel update operations
    PARALLEL_SCAN = 4,              // Parallel table scan
    PARALLEL_BUILD = 5,             // Parallel index build
    PARALLEL_REBUILD = 6,           // Parallel index rebuild
    PARALLEL_MERGE = 7,             // Parallel bucket merge
    PARALLEL_SPLIT = 8,             // Parallel bucket split
    PARALLEL_COMPACT = 9            // Parallel compaction
};

//----------------------------
// Work Distribution Strategy
//----------------------------

enum WorkDistributionStrategy : UCHAR
{
    DISTRIBUTE_ROUND_ROBIN = 0,     // Round-robin distribution
    DISTRIBUTE_HASH_BASED = 1,      // Hash-based distribution
    DISTRIBUTE_SIZE_BASED = 2,      // Size-based distribution
    DISTRIBUTE_LOAD_BALANCED = 3,   // Dynamic load balancing
    DISTRIBUTE_LOCALITY_AWARE = 4   // NUMA/cache locality aware
};

//----------------------------
// Parallel Work Item
//----------------------------

struct ParallelWorkItem
{
    ULONG work_id;                  // Unique work item identifier
    ParallelOperationType operation_type; // Type of operation
    
    // Operation data
    ULONG bucket_id;                // Target bucket (if applicable)
    ULONG hash_value;               // Hash value for operation
    UCHAR* key_data;                // Key data
    USHORT key_length;              // Key length
    UCHAR* value_data;              // Value data (for insert/update)
    USHORT value_length;            // Value length
    ULONG record_number;            // Record number
    
    // Range operations
    ULONG start_bucket;             // Start bucket for range operations
    ULONG end_bucket;               // End bucket for range operations
    
    // Batch operations
    std::vector<hash_entry*> batch_entries; // Batch of entries
    
    // Priority and scheduling
    ULONG priority;                 // Work item priority (higher = more urgent)
    GDS_TIMESTAMP creation_time;    // When work item was created
    GDS_TIMESTAMP deadline;         // Deadline for completion
    
    // Dependencies
    std::vector<ULONG> prerequisite_work_ids; // Work items that must complete first
    
    // Result storage
    bool success;                   // True if operation succeeded
    ScratchBird::string error_message; // Error message if failed
    UCHAR* result_data;             // Result data (for lookups)
    ULONG result_size;              // Size of result data
    
    ParallelWorkItem()
        : work_id(0), operation_type(PARALLEL_INSERT), bucket_id(0),
          hash_value(0), key_data(nullptr), key_length(0),
          value_data(nullptr), value_length(0), record_number(0),
          start_bucket(0), end_bucket(0), priority(0),
          creation_time(0), deadline(0), success(false),
          result_data(nullptr), result_size(0)
    {
    }
    
    ~ParallelWorkItem() {
        delete[] key_data;
        delete[] value_data;
        delete[] result_data;
    }
    
    // Work item comparison for priority queue
    bool operator<(const ParallelWorkItem& other) const {
        if (priority != other.priority) {
            return priority < other.priority; // Higher priority first
        }
        return creation_time > other.creation_time; // Earlier creation time first
    }
};

//----------------------------
// Worker Thread Statistics
//----------------------------

struct WorkerThreadStatistics
{
    ULONG thread_id;                // Worker thread identifier
    ULONG work_items_processed;     // Total work items processed
    ULONG successful_operations;    // Successful operations
    ULONG failed_operations;        // Failed operations
    ULONG total_processing_time_ms; // Total processing time
    double average_processing_time; // Average time per work item
    ULONG idle_time_ms;             // Time spent idle
    double cpu_utilization;         // CPU utilization percentage
    ULONG cache_hits;               // Cache hits
    ULONG cache_misses;             // Cache misses
    GDS_TIMESTAMP last_activity;    // Last activity timestamp
    
    WorkerThreadStatistics()
        : thread_id(0), work_items_processed(0), successful_operations(0),
          failed_operations(0), total_processing_time_ms(0),
          average_processing_time(0.0), idle_time_ms(0),
          cpu_utilization(0.0), cache_hits(0), cache_misses(0),
          last_activity(0)
    {
    }
    
    void updateAverageProcessingTime() {
        if (work_items_processed > 0) {
            average_processing_time = static_cast<double>(total_processing_time_ms) / work_items_processed;
        }
    }
    
    double getSuccessRate() const {
        return work_items_processed > 0 ? 
               static_cast<double>(successful_operations) / work_items_processed : 0.0;
    }
    
    double getCacheHitRatio() const {
        ULONG total_accesses = cache_hits + cache_misses;
        return total_accesses > 0 ? static_cast<double>(cache_hits) / total_accesses : 0.0;
    }
};

//----------------------------
// Parallel Processing Configuration
//----------------------------

struct ParallelProcessingConfig
{
    // Thread configuration
    ULONG max_worker_threads;       // Maximum number of worker threads
    ULONG min_worker_threads;       // Minimum number of worker threads
    bool adaptive_thread_count;     // Dynamically adjust thread count
    
    // Work distribution
    WorkDistributionStrategy distribution_strategy; // How to distribute work
    ULONG batch_size;               // Default batch size
    bool dynamic_batching;          // Adjust batch size based on load
    
    // Performance tuning
    ULONG work_queue_size;          // Size of work queue per thread
    ULONG thread_affinity_mask;     // CPU affinity mask for threads
    bool numa_aware;                // Enable NUMA awareness
    bool hyperthreading_aware;      // Consider hyperthreading in scheduling
    
    // Timeout and retry
    ULONG operation_timeout_ms;     // Timeout for individual operations
    ULONG global_timeout_ms;        // Global timeout for parallel operations
    ULONG max_retry_count;          // Maximum retry attempts for failed operations
    ULONG retry_delay_ms;           // Delay between retry attempts
    
    // Memory management
    ULONG max_memory_per_thread_mb; // Maximum memory per worker thread
    bool prealloc_work_buffers;     // Pre-allocate work buffers
    ULONG buffer_pool_size;         // Size of shared buffer pool
    
    // Load balancing
    bool enable_work_stealing;      // Enable work stealing between threads
    double load_balance_threshold;  // Threshold for load balancing (0.0-1.0)
    ULONG load_check_interval_ms;   // Interval for load checking
    
    ParallelProcessingConfig()
        : max_worker_threads(DEFAULT_PARALLEL_THREADS), min_worker_threads(1),
          adaptive_thread_count(true), distribution_strategy(DISTRIBUTE_LOAD_BALANCED),
          batch_size(DEFAULT_PARALLEL_BATCH_SIZE), dynamic_batching(true),
          work_queue_size(PARALLEL_QUEUE_SIZE), thread_affinity_mask(0),
          numa_aware(false), hyperthreading_aware(true),
          operation_timeout_ms(5000), global_timeout_ms(PARALLEL_TIMEOUT_MS),
          max_retry_count(3), retry_delay_ms(100),
          max_memory_per_thread_mb(64), prealloc_work_buffers(true),
          buffer_pool_size(1024), enable_work_stealing(true),
          load_balance_threshold(0.8), load_check_interval_ms(1000)
    {
    }
};

//----------------------------
// Parallel Worker Thread
//----------------------------

/**
 * Individual worker thread for parallel hash operations
 */
class ParallelWorkerThread
{
public:
    ParallelWorkerThread(ULONG thread_id, MemoryPool* pool, Database* database,
                        const ParallelProcessingConfig& config);
    ~ParallelWorkerThread();

    // Thread lifecycle
    bool start();
    void stop();
    void join();
    bool isRunning() const { return m_is_running.load(); }
    
    // Work management
    bool addWorkItem(std::unique_ptr<ParallelWorkItem> work_item);
    bool hasWork() const;
    ULONG getQueueSize() const;
    bool isIdle() const;
    
    // Work stealing support
    std::unique_ptr<ParallelWorkItem> stealWork();
    bool canStealWork() const;
    
    // Statistics
    WorkerThreadStatistics getStatistics() const;
    void resetStatistics();
    
    // Configuration
    void updateConfiguration(const ParallelProcessingConfig& config);

private:
    ULONG m_thread_id;
    MemoryPool* m_pool;
    Database* m_database;
    ParallelProcessingConfig m_config;
    
    // Thread management
    std::unique_ptr<std::thread> m_thread;
    std::atomic<bool> m_is_running;
    std::atomic<bool> m_should_stop;
    
    // Work queue
    std::queue<std::unique_ptr<ParallelWorkItem>> m_work_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_work_available;
    
    // Statistics
    mutable WorkerThreadStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Thread-local storage
    thread_db* m_thread_db;
    PersistentHashStorage* m_hash_storage;
    
    // Performance monitoring
    GDS_TIMESTAMP m_last_work_time;
    GDS_TIMESTAMP m_idle_start_time;
    
    // Main thread function
    void threadProc();
    
    // Work processing
    void processWorkItem(ParallelWorkItem* work_item);
    bool executeInsertOperation(ParallelWorkItem* work_item);
    bool executeLookupOperation(ParallelWorkItem* work_item);
    bool executeDeleteOperation(ParallelWorkItem* work_item);
    bool executeUpdateOperation(ParallelWorkItem* work_item);
    bool executeScanOperation(ParallelWorkItem* work_item);
    bool executeBuildOperation(ParallelWorkItem* work_item);
    bool executeRebuildOperation(ParallelWorkItem* work_item);
    bool executeMergeOperation(ParallelWorkItem* work_item);
    bool executeSplitOperation(ParallelWorkItem* work_item);
    bool executeCompactOperation(ParallelWorkItem* work_item);
    
    // Batch processing
    bool processBatchOperation(ParallelWorkItem* work_item);
    
    // Error handling and retry
    bool retryFailedOperation(ParallelWorkItem* work_item);
    void handleOperationError(ParallelWorkItem* work_item, const ScratchBird::string& error);
    
    // Performance optimization
    void optimizeThreadAffinity();
    void warmupCaches();
    void updatePerformanceMetrics(const ParallelWorkItem* work_item, ULONG processing_time_ms);
    
    // Resource management
    void initializeThreadResources();
    void cleanupThreadResources();
    bool hasAvailableMemory() const;
};

//----------------------------
// Work Distribution Manager
//----------------------------

/**
 * Manages distribution of work items across worker threads
 */
class WorkDistributionManager
{
public:
    explicit WorkDistributionManager(MemoryPool* pool);
    ~WorkDistributionManager();

    // Distribution management
    void setWorkerThreads(const std::vector<ParallelWorkerThread*>& threads);
    void setDistributionStrategy(WorkDistributionStrategy strategy);
    
    // Work distribution
    ParallelWorkerThread* selectWorkerForItem(const ParallelWorkItem* work_item);
    std::vector<ParallelWorkerThread*> selectWorkersForBatch(const std::vector<ParallelWorkItem*>& batch);
    
    // Load balancing
    void performLoadBalancing();
    bool needsLoadBalancing() const;
    
    // Work stealing coordination
    ParallelWorkerThread* findWorkerWithWork(ParallelWorkerThread* requesting_worker);
    void enableWorkStealing(bool enabled);
    
    // Statistics and monitoring
    struct DistributionStatistics {
        ULONG total_work_items_distributed;  // Total work items distributed
        ULONG load_balancing_operations;     // Load balancing operations performed
        ULONG work_stealing_operations;      // Work stealing operations
        double average_queue_length;         // Average queue length across threads
        double load_balance_efficiency;      // Load balancing efficiency
        ULONG thread_utilization_percentage; // Overall thread utilization
        
        DistributionStatistics()
            : total_work_items_distributed(0), load_balancing_operations(0),
              work_stealing_operations(0), average_queue_length(0.0),
              load_balance_efficiency(1.0), thread_utilization_percentage(0)
        {
        }
    };
    
    DistributionStatistics getStatistics() const;
    void resetStatistics();

private:
    MemoryPool* m_pool;
    std::vector<ParallelWorkerThread*> m_worker_threads;
    WorkDistributionStrategy m_strategy;
    bool m_work_stealing_enabled;
    
    // Distribution state
    std::atomic<ULONG> m_round_robin_counter;
    mutable std::mutex m_distribution_mutex;
    
    // Statistics
    mutable DistributionStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Load monitoring
    GDS_TIMESTAMP m_last_load_check;
    std::vector<double> m_thread_load_history;
    
    // Strategy implementations
    ParallelWorkerThread* selectByRoundRobin(const ParallelWorkItem* work_item);
    ParallelWorkerThread* selectByHashBased(const ParallelWorkItem* work_item);
    ParallelWorkerThread* selectBySizeBased(const ParallelWorkItem* work_item);
    ParallelWorkerThread* selectByLoadBalanced(const ParallelWorkItem* work_item);
    ParallelWorkerThread* selectByLocalityAware(const ParallelWorkItem* work_item);
    
    // Load balancing helpers
    double calculateThreadLoad(const ParallelWorkerThread* thread) const;
    std::vector<double> calculateAllThreadLoads() const;
    bool isLoadImbalanced(const std::vector<double>& loads) const;
    void redistributeWork();
    
    // Work stealing helpers
    ParallelWorkerThread* findLeastLoadedWorker() const;
    ParallelWorkerThread* findMostLoadedWorker() const;
    void updateStatistics();
};

//----------------------------
// Parallel Hash Processor
//----------------------------

/**
 * Main coordinator for parallel hash operations
 */
class ParallelHashProcessor
{
public:
    explicit ParallelHashProcessor(MemoryPool* pool, Database* database);
    ~ParallelHashProcessor();

    // Processor lifecycle
    bool initialize(const ParallelProcessingConfig& config = ParallelProcessingConfig());
    void shutdown();
    bool isInitialized() const { return m_is_initialized; }
    
    // Parallel operations interface
    bool executeParallelInserts(const std::vector<hash_entry*>& entries,
                               const index_desc* idx, std::vector<bool>& results);
    
    std::vector<hash_entry*> executeParallelLookups(const std::vector<std::pair<ULONG, std::pair<UCHAR*, USHORT>>>& keys,
                                                    const index_desc* idx);
    
    bool executeParallelDeletes(const std::vector<std::pair<ULONG, std::pair<UCHAR*, USHORT>>>& keys,
                               const index_desc* idx, std::vector<bool>& results);
    
    bool executeParallelUpdates(const std::vector<std::tuple<ULONG, std::pair<UCHAR*, USHORT>, hash_entry*>>& updates,
                               const index_desc* idx, std::vector<bool>& results);
    
    // Bulk operations
    bool executeParallelScan(const index_desc* idx, 
                            std::function<bool(const hash_entry*)> processor,
                            ULONG start_bucket = 0, ULONG end_bucket = 0);
    
    bool executeParallelBuild(const index_desc* idx, const std::vector<hash_entry*>& all_entries);
    
    bool executeParallelRebuild(const index_desc* idx);
    
    // Maintenance operations
    bool executeParallelMaintenance(const index_desc* idx, 
                                   const std::vector<ParallelOperationType>& operations);
    
    // Configuration management
    void updateConfiguration(const ParallelProcessingConfig& config);
    ParallelProcessingConfig getConfiguration() const;
    
    // Thread management
    void setThreadCount(ULONG thread_count);
    ULONG getThreadCount() const;
    
    void adjustThreadCountForWorkload(ULONG estimated_work_items);
    
    // Performance monitoring
    struct ProcessorStatistics {
        ULONG total_parallel_operations;    // Total parallel operations executed
        ULONG successful_operations;        // Successful operations
        ULONG failed_operations;            // Failed operations
        double average_operation_time_ms;   // Average operation time
        double parallel_efficiency;         // Parallelization efficiency
        ULONG max_concurrent_operations;    // Maximum concurrent operations
        double average_thread_utilization;  // Average thread utilization
        ULONG work_items_processed;         // Total work items processed
        ULONG work_stealing_events;         // Work stealing events
        
        ProcessorStatistics()
            : total_parallel_operations(0), successful_operations(0),
              failed_operations(0), average_operation_time_ms(0.0),
              parallel_efficiency(1.0), max_concurrent_operations(0),
              average_thread_utilization(0.0), work_items_processed(0),
              work_stealing_events(0)
        {
        }
    };
    
    ProcessorStatistics getStatistics() const;
    void resetStatistics();
    
    // Health monitoring
    bool isHealthy() const;
    ScratchBird::string getHealthReport() const;
    
    // Debugging and diagnostics
    std::vector<WorkerThreadStatistics> getWorkerStatistics() const;
    WorkDistributionManager::DistributionStatistics getDistributionStatistics() const;
    
    ScratchBird::string generateDiagnosticReport() const;

private:
    MemoryPool* m_pool;
    Database* m_database;
    bool m_is_initialized;
    
    // Configuration
    ParallelProcessingConfig m_config;
    mutable std::mutex m_config_mutex;
    
    // Worker thread management
    std::vector<std::unique_ptr<ParallelWorkerThread>> m_worker_threads;
    std::unique_ptr<WorkDistributionManager> m_distribution_manager;
    mutable std::mutex m_threads_mutex;
    
    // Work coordination
    std::atomic<ULONG> m_next_work_id;
    std::map<ULONG, std::unique_ptr<ParallelWorkItem>> m_pending_work;
    std::mutex m_pending_work_mutex;
    
    // Statistics
    mutable ProcessorStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Synchronization for parallel operations
    struct OperationContext {
        std::atomic<ULONG> pending_items;
        std::atomic<ULONG> completed_items;
        std::atomic<ULONG> failed_items;
        std::condition_variable completion_cv;
        std::mutex completion_mutex;
        bool operation_complete;
        
        OperationContext(ULONG total_items)
            : pending_items(total_items), completed_items(0), failed_items(0),
              operation_complete(false)
        {
        }
    };
    
    // Initialization helpers
    bool createWorkerThreads();
    void destroyWorkerThreads();
    bool initializeDistributionManager();
    
    // Work item creation
    std::unique_ptr<ParallelWorkItem> createInsertWorkItem(const hash_entry* entry, ULONG bucket_id);
    std::unique_ptr<ParallelWorkItem> createLookupWorkItem(ULONG hash_value, const UCHAR* key_data, USHORT key_length);
    std::unique_ptr<ParallelWorkItem> createDeleteWorkItem(ULONG hash_value, const UCHAR* key_data, USHORT key_length);
    std::unique_ptr<ParallelWorkItem> createUpdateWorkItem(ULONG hash_value, const UCHAR* key_data, USHORT key_length, const hash_entry* new_entry);
    
    // Batch processing helpers
    std::vector<std::vector<ParallelWorkItem*>> partitionWorkItems(const std::vector<ParallelWorkItem*>& work_items) const;
    
    bool distributeWorkItems(const std::vector<std::unique_ptr<ParallelWorkItem>>& work_items,
                            std::shared_ptr<OperationContext> context);
    
    bool waitForCompletion(std::shared_ptr<OperationContext> context, ULONG timeout_ms);
    
    // Result collection
    template<typename ResultType>
    std::vector<ResultType> collectResults(const std::shared_ptr<OperationContext>& context,
                                          const std::function<ResultType(const ParallelWorkItem*)>& extractor);
    
    // Performance optimization
    void optimizeForWorkload(ULONG work_item_count, ParallelOperationType operation_type);
    ULONG calculateOptimalThreadCount(ULONG work_item_count, ParallelOperationType operation_type) const;
    ULONG calculateOptimalBatchSize(ULONG work_item_count, ULONG thread_count) const;
    
    // Error handling
    void handleWorkerThreadError(ULONG thread_id, const ScratchBird::string& error);
    void restartFailedWorkerThread(ULONG thread_id);
    
    // Health monitoring
    void performHealthCheck();
    bool areAllThreadsHealthy() const;
    
    // Statistics update
    void updateStatistics(const std::shared_ptr<OperationContext>& context, 
                         ULONG operation_time_ms, ParallelOperationType operation_type);
};

//----------------------------
// Global Parallel Hash Manager
//----------------------------

/**
 * Global manager for parallel hash processing across all databases
 */
class GlobalParallelHashManager
{
public:
    static GlobalParallelHashManager* getInstance();
    
    // Processor management
    ParallelHashProcessor* getProcessorForDatabase(Database* database);
    void releaseProcessor(Database* database);
    
    // Global configuration
    void setGlobalConfiguration(const ParallelProcessingConfig& config);
    ParallelProcessingConfig getGlobalConfiguration() const;
    
    // Resource management
    void setGlobalThreadLimit(ULONG max_threads);
    ULONG getGlobalThreadLimit() const;
    
    ULONG getCurrentGlobalThreadCount() const;
    bool canAllocateThreads(ULONG requested_threads) const;
    
    // Global statistics
    struct GlobalStatistics {
        ULONG total_processors;           // Total parallel processors
        ULONG active_processors;          // Currently active processors
        ULONG total_worker_threads;       // Total worker threads across all processors
        ULONG global_work_items_processed; // Global work items processed
        double global_parallel_efficiency; // Global parallelization efficiency
        ULONG global_memory_usage_mb;     // Global memory usage
        
        GlobalStatistics()
            : total_processors(0), active_processors(0), total_worker_threads(0),
              global_work_items_processed(0), global_parallel_efficiency(1.0),
              global_memory_usage_mb(0)
        {
        }
    };
    
    GlobalStatistics getGlobalStatistics() const;
    
    // System integration
    void onSystemStartup();
    void onSystemShutdown();
    void onDatabaseAttach(Database* database);
    void onDatabaseDetach(Database* database);

private:
    GlobalParallelHashManager();
    ~GlobalParallelHashManager();
    
    static GlobalParallelHashManager* s_instance;
    static std::mutex s_instance_mutex;
    
    struct ProcessorInfo {
        Database* database;
        std::unique_ptr<ParallelHashProcessor> processor;
        ULONG reference_count;
        GDS_TIMESTAMP last_access;
        
        ProcessorInfo(Database* db) : database(db), reference_count(0), last_access(0) {}
    };
    
    std::vector<ProcessorInfo> m_processors;
    mutable std::mutex m_processors_mutex;
    
    // Global configuration
    ParallelProcessingConfig m_global_config;
    ULONG m_global_thread_limit;
    
    // Statistics
    mutable GlobalStatistics m_statistics;
    mutable std::mutex m_statistics_mutex;
    
    // Cleanup and maintenance
    void cleanupUnusedProcessors();
    void updateGlobalStatistics();
    
    ProcessorInfo* findProcessorInfo(Database* database);
};

//----------------------------
// Integration with Hash Index
//----------------------------

/**
 * Integration layer for parallel hash processing
 */
class ParallelHashIntegration
{
public:
    // Integration setup
    static bool enableParallelProcessing(const index_desc* idx, 
                                        const ParallelProcessingConfig& config = ParallelProcessingConfig());
    
    static bool disableParallelProcessing(const index_desc* idx);
    static bool isParallelProcessingEnabled(const index_desc* idx);
    
    // Parallel operation hooks
    static bool shouldUseParallelProcessing(const index_desc* idx, ParallelOperationType operation_type,
                                           ULONG work_item_count);
    
    // Batch operation wrappers
    static bool parallelInsertEntries(thread_db* tdbb, const index_desc* idx,
                                     const std::vector<hash_entry*>& entries);
    
    static std::vector<hash_entry*> parallelLookupEntries(thread_db* tdbb, const index_desc* idx,
                                                          const std::vector<std::pair<ULONG, std::pair<UCHAR*, USHORT>>>& keys);
    
    static bool parallelDeleteEntries(thread_db* tdbb, const index_desc* idx,
                                     const std::vector<std::pair<ULONG, std::pair<UCHAR*, USHORT>>>& keys);
    
    // Performance monitoring
    static void recordParallelOperation(const index_desc* idx, ParallelOperationType operation_type,
                                       ULONG work_items, ULONG execution_time_ms, bool success);

private:
    static std::map<USHORT, ParallelProcessingConfig> s_index_configs;
    static std::set<USHORT> s_parallel_enabled_indexes;
    static std::mutex s_integration_mutex;
};

//----------------------------
// Utility Functions
//----------------------------

// Workload analysis
ULONG calculateOptimalThreadCount(ULONG work_items, ParallelOperationType operation_type,
                                 ULONG available_cores = 0);

ULONG calculateOptimalBatchSize(ULONG total_work_items, ULONG thread_count,
                               ParallelOperationType operation_type);

// Performance estimation
double estimateParallelSpeedup(ULONG work_items, ULONG thread_count, 
                              ParallelOperationType operation_type);

bool isParallelizationBeneficial(ULONG work_items, ParallelOperationType operation_type,
                                double overhead_threshold = 0.1);

// Resource management
ULONG estimateMemoryRequirement(ULONG thread_count, ULONG batch_size,
                               ParallelOperationType operation_type);

bool hasAvailableResources(ULONG thread_count, ULONG estimated_memory_mb);

// Configuration optimization
ParallelProcessingConfig optimizeConfigurationForWorkload(ULONG typical_work_items,
                                                         ParallelOperationType primary_operation,
                                                         ULONG available_cores,
                                                         ULONG available_memory_mb);

// Work distribution utilities
WorkDistributionStrategy selectOptimalDistributionStrategy(ULONG work_items,
                                                          ULONG thread_count,
                                                          bool has_numa_topology = false);

// Monitoring and diagnostics
bool validateParallelProcessorHealth(const ParallelHashProcessor* processor);
ScratchBird::string formatParallelStatistics(const ParallelHashProcessor::ProcessorStatistics& stats);

} // namespace Jrd

#endif // JRD_PARALLEL_HASH_PROCESSOR_H