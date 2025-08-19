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
 * 2025.07.23 - ScratchBird GIN Index Implementation - DML Maintenance
 */

#ifndef JRD_GIN_INDEX_MAINTENANCE_H
#define JRD_GIN_INDEX_MAINTENANCE_H

#include "GinIndex.h"
#include "GinTokenizer.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/array.h"
#include "../common/gdsassert.h"
#include <vector>
#include <set>
#include <map>

namespace Jrd {

// Forward declarations
class thread_db;
class jrd_tra;
class record_param;
class GinIndex;

//----------------------------
// GIN Index Maintenance Operations
//----------------------------

/**
 * Types of DML operations that affect GIN indexes
 */
enum GinMaintenanceOperation : UCHAR
{
    GIN_OP_INSERT = 0,      // Insert new record
    GIN_OP_UPDATE = 1,      // Update existing record
    GIN_OP_DELETE = 2,      // Delete existing record
    GIN_OP_BULK_INSERT = 3, // Bulk insert operation
    GIN_OP_BULK_UPDATE = 4, // Bulk update operation
    GIN_OP_BULK_DELETE = 5  // Bulk delete operation
};

/**
 * Maintenance strategy for GIN index updates
 */
enum GinMaintenanceStrategy : UCHAR
{
    GIN_MAINTENANCE_IMMEDIATE = 0,    // Update index immediately
    GIN_MAINTENANCE_DEFERRED = 1,     // Defer updates to end of transaction
    GIN_MAINTENANCE_BATCH = 2,        // Batch multiple updates together
    GIN_MAINTENANCE_ASYNC = 3         // Asynchronous background updates
};

/**
 * Context information for GIN index maintenance operations
 */
struct GinMaintenanceContext
{
    thread_db* tdbb;                    // Thread database context
    jrd_tra* transaction;               // Current transaction
    GinIndex* gin_index;                // Target GIN index
    GinMaintenanceStrategy strategy;    // Maintenance strategy
    ULONG operation_count;              // Number of operations in batch
    ULONG max_batch_size;               // Maximum batch size before flush
    bool fast_update_enabled;           // FASTUPDATE option enabled
    bool transaction_cleanup_pending;   // Cleanup needed at transaction end
    
    GinMaintenanceContext()
        : tdbb(nullptr), transaction(nullptr), gin_index(nullptr),
          strategy(GIN_MAINTENANCE_IMMEDIATE), operation_count(0),
          max_batch_size(1000), fast_update_enabled(false),
          transaction_cleanup_pending(false)
    {}
};

/**
 * Represents a pending GIN index update operation
 */
struct GinPendingUpdate
{
    GinMaintenanceOperation operation;  // Type of operation
    RecordNumber record_number;         // Target record number
    TokenList old_tokens;              // Tokens from old value (for UPDATE/DELETE)
    TokenList new_tokens;              // Tokens from new value (for INSERT/UPDATE)
    ULONG timestamp;                   // Operation timestamp
    bool processed;                    // Whether operation has been processed
    
    GinPendingUpdate()
        : operation(GIN_OP_INSERT), record_number(0), timestamp(0), processed(false)
    {}
    
    GinPendingUpdate(GinMaintenanceOperation op, RecordNumber rec_num, 
                     const TokenList& old_toks, const TokenList& new_toks)
        : operation(op), record_number(rec_num), old_tokens(old_toks), 
          new_tokens(new_toks), timestamp(0), processed(false)
    {}
};

//----------------------------
// GIN Index Maintenance Engine
//----------------------------

/**
 * Main class responsible for maintaining GIN indexes during DML operations
 */
class GinIndexMaintenance
{
public:
    explicit GinIndexMaintenance(MemoryPool* pool);
    ~GinIndexMaintenance();

    // Initialization and configuration
    bool initialize(thread_db* tdbb, GinIndex* gin_index);
    void setMaintenanceStrategy(GinMaintenanceStrategy strategy);
    void setFastUpdateEnabled(bool enabled);
    void setBatchSize(ULONG batch_size);
    
    // DML operation handlers
    bool handleInsert(thread_db* tdbb, jrd_tra* transaction, 
                     RecordNumber record_number, const dsc* new_value);
                     
    bool handleUpdate(thread_db* tdbb, jrd_tra* transaction,
                     RecordNumber record_number, const dsc* old_value, const dsc* new_value);
                     
    bool handleDelete(thread_db* tdbb, jrd_tra* transaction,
                     RecordNumber record_number, const dsc* old_value);
    
    // Bulk operation handlers
    bool handleBulkInsert(thread_db* tdbb, jrd_tra* transaction,
                         const std::vector<std::pair<RecordNumber, const dsc*>>& records);
                         
    bool handleBulkUpdate(thread_db* tdbb, jrd_tra* transaction,
                         const std::vector<std::tuple<RecordNumber, const dsc*, const dsc*>>& records);
                         
    bool handleBulkDelete(thread_db* tdbb, jrd_tra* transaction,
                         const std::vector<std::pair<RecordNumber, const dsc*>>& records);
    
    // Transaction management
    bool beginTransaction(thread_db* tdbb, jrd_tra* transaction);
    bool commitTransaction(thread_db* tdbb, jrd_tra* transaction);
    bool rollbackTransaction(thread_db* tdbb, jrd_tra* transaction);
    
    // Batch processing
    bool flushPendingUpdates(thread_db* tdbb, jrd_tra* transaction);
    bool processPendingUpdates(thread_db* tdbb, jrd_tra* transaction, ULONG max_operations = 0);
    
    // Maintenance operations
    bool rebuildIndex(thread_db* tdbb, jrd_tra* transaction);
    bool optimizeIndex(thread_db* tdbb, jrd_tra* transaction);
    bool vacuumIndex(thread_db* tdbb, jrd_tra* transaction);
    
    // Statistics and monitoring
    ULONG getPendingUpdateCount() const;
    ULONG getProcessedUpdateCount() const;
    double getMaintenanceOverhead() const;
    void getMaintenanceStatistics(GinMaintenanceStatistics* stats) const;
    
    // Configuration queries
    GinMaintenanceStrategy getMaintenanceStrategy() const;
    bool isFastUpdateEnabled() const;
    ULONG getBatchSize() const;

private:
    // Internal state management
    MemoryPool* m_pool;
    GinMaintenanceContext m_context;
    std::vector<GinPendingUpdate> m_pending_updates;
    std::map<jrd_tra*, std::vector<GinPendingUpdate>> m_transaction_updates;
    
    // Performance monitoring
    ULONG m_processed_operations;
    ULONG m_failed_operations;
    double m_total_maintenance_time;
    ULONG m_cache_hits;
    ULONG m_cache_misses;
    
    // Concurrency control
    mutable ScratchBird::Mutex m_mutex;
    
    // Token processing helpers
    TokenList extractTokensFromValue(const dsc* value);
    void compareTokenLists(const TokenList& old_tokens, const TokenList& new_tokens,
                          TokenList& tokens_to_add, TokenList& tokens_to_remove);
    
    // Index update operations
    bool insertTokens(thread_db* tdbb, jrd_tra* transaction,
                     RecordNumber record_number, const TokenList& tokens);
                     
    bool removeTokens(thread_db* tdbb, jrd_tra* transaction,
                     RecordNumber record_number, const TokenList& tokens);
                     
    bool updateTokens(thread_db* tdbb, jrd_tra* transaction,
                     RecordNumber record_number, const TokenList& old_tokens, 
                     const TokenList& new_tokens);
    
    // Batch processing helpers
    void addPendingUpdate(const GinPendingUpdate& update);
    bool shouldFlushBatch() const;
    void clearProcessedUpdates();
    
    // Transaction cleanup
    void registerTransactionCleanup(jrd_tra* transaction);
    void performTransactionCleanup(thread_db* tdbb, jrd_tra* transaction);
    
    // Error handling and recovery
    void handleMaintenanceError(thread_db* tdbb, const Exception& error);
    bool recoverFromFailure(thread_db* tdbb, jrd_tra* transaction);
    
    // Performance optimization
    void optimizePendingUpdates();
    void compactPostingLists(thread_db* tdbb, jrd_tra* transaction);
    void mergeDuplicateTokens(thread_db* tdbb, jrd_tra* transaction);
    
    // Statistics helpers
    void updateMaintenanceStatistics(GinMaintenanceOperation operation, 
                                   double elapsed_time, bool success);
};

//----------------------------
// Transaction-Level GIN Maintenance
//----------------------------

/**
 * Transaction-scoped GIN index maintenance coordinator
 */
class GinTransactionMaintenance
{
public:
    explicit GinTransactionMaintenance(thread_db* tdbb, jrd_tra* transaction);
    ~GinTransactionMaintenance();
    
    // Register GIN indexes for maintenance
    void registerGinIndex(GinIndex* gin_index);
    void unregisterGinIndex(GinIndex* gin_index);
    
    // DML operation notifications
    void notifyInsert(GinIndex* gin_index, RecordNumber record_number, const dsc* value);
    void notifyUpdate(GinIndex* gin_index, RecordNumber record_number,
                     const dsc* old_value, const dsc* new_value);
    void notifyDelete(GinIndex* gin_index, RecordNumber record_number, const dsc* value);
    
    // Transaction lifecycle
    void prepareCommit();
    void commit();
    void rollback();
    
    // Statistics
    ULONG getTotalPendingOperations() const;
    ULONG getAffectedIndexCount() const;

private:
    thread_db* m_tdbb;
    jrd_tra* m_transaction;
    std::map<GinIndex*, std::unique_ptr<GinIndexMaintenance>> m_index_maintainers;
    bool m_prepared;
    bool m_committed;
};

//----------------------------
// GIN Maintenance Statistics
//----------------------------

/**
 * Statistics structure for GIN index maintenance operations
 */
struct GinMaintenanceStatistics
{
    // Operation counts
    ULONG insert_operations;
    ULONG update_operations;
    ULONG delete_operations;
    ULONG bulk_operations;
    
    // Performance metrics
    double average_operation_time;
    double total_maintenance_time;
    ULONG successful_operations;
    ULONG failed_operations;
    
    // Batch processing statistics
    ULONG batches_processed;
    ULONG average_batch_size;
    ULONG max_batch_size;
    
    // Token processing statistics
    ULONG tokens_inserted;
    ULONG tokens_removed;
    ULONG tokens_updated;
    ULONG posting_list_updates;
    
    // Cache performance
    ULONG cache_hits;
    ULONG cache_misses;
    double cache_hit_ratio;
    
    // Storage statistics
    ULONG posting_lists_compressed;
    ULONG posting_lists_expanded;
    double average_compression_ratio;
    
    GinMaintenanceStatistics()
    {
        memset(this, 0, sizeof(GinMaintenanceStatistics));
    }
};

//----------------------------
// GIN Maintenance Utilities
//----------------------------

/**
 * Utility functions for GIN index maintenance
 */
namespace GinMaintenanceUtils
{
    // Configuration helpers
    GinMaintenanceStrategy getOptimalStrategy(const GinIndex* gin_index, 
                                            const jrd_tra* transaction);
    ULONG calculateOptimalBatchSize(const GinIndex* gin_index);
    bool shouldUseFastUpdate(const GinIndex* gin_index);
    
    // Performance analysis
    double estimateMaintenanceOverhead(const GinIndex* gin_index, 
                                     GinMaintenanceOperation operation);
    bool isMaintenanceRequired(const GinIndex* gin_index);
    ULONG estimateOptimizationBenefit(const GinIndex* gin_index);
    
    // Token list utilities
    void optimizeTokenList(TokenList& tokens);
    void deduplicateTokens(TokenList& tokens);
    void sortTokensByFrequency(TokenList& tokens, const GinIndex* gin_index);
    
    // Posting list utilities
    bool shouldCompressPostingList(const GinPostingList* posting_list);
    double estimateCompressionBenefit(const GinPostingList* posting_list);
    void mergePostingLists(GinPostingList* target, const GinPostingList* source);
    
    // Error recovery
    bool canRecoverFromError(const Exception& error);
    void performEmergencyCleanup(thread_db* tdbb, GinIndex* gin_index);
    void validateIndexIntegrity(thread_db* tdbb, const GinIndex* gin_index);
}

//----------------------------
// Integration with DML Operations
//----------------------------

/**
 * Hook points for integrating GIN maintenance with existing DML operations
 */
class GinDmlIntegration
{
public:
    // Hook registration
    static void registerInsertHook(void (*hook)(thread_db*, jrd_tra*, RecordNumber, const dsc*));
    static void registerUpdateHook(void (*hook)(thread_db*, jrd_tra*, RecordNumber, 
                                               const dsc*, const dsc*));
    static void registerDeleteHook(void (*hook)(thread_db*, jrd_tra*, RecordNumber, const dsc*));
    
    // Hook execution
    static void executeInsertHooks(thread_db* tdbb, jrd_tra* transaction,
                                  RecordNumber record_number, const dsc* value);
    static void executeUpdateHooks(thread_db* tdbb, jrd_tra* transaction,
                                  RecordNumber record_number, const dsc* old_value, 
                                  const dsc* new_value);
    static void executeDeleteHooks(thread_db* tdbb, jrd_tra* transaction,
                                  RecordNumber record_number, const dsc* value);
    
    // Index discovery
    static std::vector<GinIndex*> findAffectedGinIndexes(thread_db* tdbb, 
                                                         const jrd_rel* relation,
                                                         USHORT field_id);
    
    // Maintenance coordination
    static void coordinateMaintenanceOperations(thread_db* tdbb, jrd_tra* transaction,
                                              const std::vector<GinIndex*>& indexes);
};

} // namespace Jrd

#endif // JRD_GIN_INDEX_MAINTENANCE_H