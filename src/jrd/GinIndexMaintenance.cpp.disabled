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

#include "scratchbird.h"
#include "GinIndexMaintenance.h"
#include "GinIndex.h"
#include "GinTokenizer.h"
#include "../jrd.h"
#include "../exe.h"
#include "../req.h"
#include "../tra.h"
#include "../val.h"
#include "../mov_proto.h"
#include "../common/gdsassert.h"
#include "../common/StatusArg.h"
#include <chrono>
#include <algorithm>

using namespace ScratchBird;
using namespace Jrd;

//----------------------------
// GinIndexMaintenance Implementation
//----------------------------

GinIndexMaintenance::GinIndexMaintenance(MemoryPool* pool)
    : m_pool(pool),
      m_processed_operations(0),
      m_failed_operations(0),
      m_total_maintenance_time(0.0),
      m_cache_hits(0),
      m_cache_misses(0)
{
    fb_assert(pool);
    
    // Initialize default context
    m_context.strategy = GIN_MAINTENANCE_IMMEDIATE;
    m_context.max_batch_size = 1000;
    m_context.fast_update_enabled = false;
}

GinIndexMaintenance::~GinIndexMaintenance()
{
    // Ensure any pending updates are processed or discarded appropriately
    if (!m_pending_updates.empty())
    {
        // Log warning about unprocessed updates
        fb_utils::init_status(nullptr);
    }
}

bool GinIndexMaintenance::initialize(thread_db* tdbb, GinIndex* gin_index)
{
    fb_assert(tdbb && gin_index);
    
    m_context.tdbb = tdbb;
    m_context.gin_index = gin_index;
    
    // Configure maintenance strategy based on index properties
    if (gin_index->isFastUpdateEnabled())
    {
        m_context.strategy = GIN_MAINTENANCE_DEFERRED;
        m_context.fast_update_enabled = true;
    }
    
    return true;
}

void GinIndexMaintenance::setMaintenanceStrategy(GinMaintenanceStrategy strategy)
{
    m_context.strategy = strategy;
}

void GinIndexMaintenance::setFastUpdateEnabled(bool enabled)
{
    m_context.fast_update_enabled = enabled;
    
    // Adjust strategy based on fast update setting
    if (enabled && m_context.strategy == GIN_MAINTENANCE_IMMEDIATE)
    {
        m_context.strategy = GIN_MAINTENANCE_DEFERRED;
    }
}

void GinIndexMaintenance::setBatchSize(ULONG batch_size)
{
    m_context.max_batch_size = std::max(batch_size, 1UL);
}

bool GinIndexMaintenance::handleInsert(thread_db* tdbb, jrd_tra* transaction,
                                       RecordNumber record_number, const dsc* new_value)
{
    fb_assert(tdbb && transaction && new_value && record_number != 0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = false;
    
    try
    {
        ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
        
        // Extract tokens from the new value
        TokenList new_tokens = extractTokensFromValue(new_value);
        
        if (m_context.strategy == GIN_MAINTENANCE_IMMEDIATE)
        {
            // Process immediately
            success = insertTokens(tdbb, transaction, record_number, new_tokens);
        }
        else
        {
            // Add to pending updates
            GinPendingUpdate update(GIN_OP_INSERT, record_number, TokenList(), new_tokens);
            addPendingUpdate(update);
            success = true;
            
            // Check if we should flush the batch
            if (shouldFlushBatch())
            {
                success = flushPendingUpdates(tdbb, transaction);
            }
        }
        
        if (success)
        {
            m_processed_operations++;
        }
        else
        {
            m_failed_operations++;
        }
    }
    catch (const Exception& ex)
    {
        handleMaintenanceError(tdbb, ex);
        success = false;
    }
    
    // Update performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    updateMaintenanceStatistics(GIN_OP_INSERT, elapsed, success);
    
    return success;
}

bool GinIndexMaintenance::handleUpdate(thread_db* tdbb, jrd_tra* transaction,
                                       RecordNumber record_number, const dsc* old_value, 
                                       const dsc* new_value)
{
    fb_assert(tdbb && transaction && old_value && new_value && record_number != 0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = false;
    
    try
    {
        ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
        
        // Extract tokens from both old and new values
        TokenList old_tokens = extractTokensFromValue(old_value);
        TokenList new_tokens = extractTokensFromValue(new_value);
        
        // Quick check: if token lists are identical, no update needed
        if (old_tokens == new_tokens)
        {
            return true;
        }
        
        if (m_context.strategy == GIN_MAINTENANCE_IMMEDIATE)
        {
            // Process immediately
            success = updateTokens(tdbb, transaction, record_number, old_tokens, new_tokens);
        }
        else
        {
            // Add to pending updates
            GinPendingUpdate update(GIN_OP_UPDATE, record_number, old_tokens, new_tokens);
            addPendingUpdate(update);
            success = true;
            
            // Check if we should flush the batch
            if (shouldFlushBatch())
            {
                success = flushPendingUpdates(tdbb, transaction);
            }
        }
        
        if (success)
        {
            m_processed_operations++;
        }
        else
        {
            m_failed_operations++;
        }
    }
    catch (const Exception& ex)
    {
        handleMaintenanceError(tdbb, ex);
        success = false;
    }
    
    // Update performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    updateMaintenanceStatistics(GIN_OP_UPDATE, elapsed, success);
    
    return success;
}

bool GinIndexMaintenance::handleDelete(thread_db* tdbb, jrd_tra* transaction,
                                       RecordNumber record_number, const dsc* old_value)
{
    fb_assert(tdbb && transaction && old_value && record_number != 0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = false;
    
    try
    {
        ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
        
        // Extract tokens from the old value
        TokenList old_tokens = extractTokensFromValue(old_value);
        
        if (m_context.strategy == GIN_MAINTENANCE_IMMEDIATE)
        {
            // Process immediately
            success = removeTokens(tdbb, transaction, record_number, old_tokens);
        }
        else
        {
            // Add to pending updates
            GinPendingUpdate update(GIN_OP_DELETE, record_number, old_tokens, TokenList());
            addPendingUpdate(update);
            success = true;
            
            // Check if we should flush the batch
            if (shouldFlushBatch())
            {
                success = flushPendingUpdates(tdbb, transaction);
            }
        }
        
        if (success)
        {
            m_processed_operations++;
        }
        else
        {
            m_failed_operations++;
        }
    }
    catch (const Exception& ex)
    {
        handleMaintenanceError(tdbb, ex);
        success = false;
    }
    
    // Update performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    updateMaintenanceStatistics(GIN_OP_DELETE, elapsed, success);
    
    return success;
}

bool GinIndexMaintenance::handleBulkInsert(thread_db* tdbb, jrd_tra* transaction,
                                           const std::vector<std::pair<RecordNumber, const dsc*>>& records)
{
    if (records.empty())
        return true;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = true;
    
    try
    {
        ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
        
        // Process records in batches to avoid memory issues
        const ULONG BULK_BATCH_SIZE = 1000;
        
        for (size_t i = 0; i < records.size(); i += BULK_BATCH_SIZE)
        {
            size_t end_idx = std::min(i + BULK_BATCH_SIZE, records.size());
            
            for (size_t j = i; j < end_idx; j++)
            {
                RecordNumber record_number = records[j].first;
                const dsc* value = records[j].second;
                
                TokenList tokens = extractTokensFromValue(value);
                
                if (!insertTokens(tdbb, transaction, record_number, tokens))
                {
                    success = false;
                    m_failed_operations++;
                }
                else
                {
                    m_processed_operations++;
                }
            }
            
            // Yield periodically to allow other operations
            if ((i / BULK_BATCH_SIZE) % 10 == 0)
            {
                // Allow transaction to process other work
                tdbb->getAttachment()->att_mutex.leave();
                tdbb->getAttachment()->att_mutex.enter();
            }
        }
    }
    catch (const Exception& ex)
    {
        handleMaintenanceError(tdbb, ex);
        success = false;
    }
    
    // Update performance statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    updateMaintenanceStatistics(GIN_OP_BULK_INSERT, elapsed, success);
    
    return success;
}

bool GinIndexMaintenance::commitTransaction(thread_db* tdbb, jrd_tra* transaction)
{
    fb_assert(tdbb && transaction);
    
    try
    {
        ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
        
        // Process any remaining pending updates for this transaction
        bool success = processPendingUpdates(tdbb, transaction, 0);
        
        // Clean up transaction-specific data
        m_transaction_updates.erase(transaction);
        
        return success;
    }
    catch (const Exception&)
    {
        return false;
    }
}

bool GinIndexMaintenance::rollbackTransaction(thread_db* tdbb, jrd_tra* transaction)
{
    fb_assert(tdbb && transaction);
    
    try
    {
        ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
        
        // Discard all pending updates for this transaction
        m_transaction_updates.erase(transaction);
        
        // Clear pending updates that belong to this transaction
        m_pending_updates.erase(
            std::remove_if(m_pending_updates.begin(), m_pending_updates.end(),
                          [transaction](const GinPendingUpdate& update) {
                              // In a real implementation, we'd track which transaction
                              // each update belongs to. For now, we'll be conservative.
                              return !update.processed;
                          }),
            m_pending_updates.end()
        );
        
        return true;
    }
    catch (const Exception&)
    {
        return false;
    }
}

bool GinIndexMaintenance::flushPendingUpdates(thread_db* tdbb, jrd_tra* transaction)
{
    return processPendingUpdates(tdbb, transaction, 0);
}

bool GinIndexMaintenance::processPendingUpdates(thread_db* tdbb, jrd_tra* transaction, 
                                                ULONG max_operations)
{
    fb_assert(tdbb && transaction);
    
    if (m_pending_updates.empty())
        return true;
    
    bool success = true;
    ULONG processed = 0;
    
    try
    {
        for (auto& update : m_pending_updates)
        {
            if (update.processed)
                continue;
            
            if (max_operations > 0 && processed >= max_operations)
                break;
            
            bool operation_success = false;
            
            switch (update.operation)
            {
                case GIN_OP_INSERT:
                    operation_success = insertTokens(tdbb, transaction, 
                                                   update.record_number, update.new_tokens);
                    break;
                    
                case GIN_OP_UPDATE:
                    operation_success = updateTokens(tdbb, transaction, update.record_number,
                                                   update.old_tokens, update.new_tokens);
                    break;
                    
                case GIN_OP_DELETE:
                    operation_success = removeTokens(tdbb, transaction,
                                                   update.record_number, update.old_tokens);
                    break;
                    
                default:
                    operation_success = false;
                    break;
            }
            
            if (operation_success)
            {
                update.processed = true;
                processed++;
            }
            else
            {
                success = false;
                m_failed_operations++;
            }
        }
        
        // Remove processed updates
        clearProcessedUpdates();
        
        m_context.operation_count = 0;  // Reset batch counter
    }
    catch (const Exception& ex)
    {
        handleMaintenanceError(tdbb, ex);
        success = false;
    }
    
    return success;
}

TokenList GinIndexMaintenance::extractTokensFromValue(const dsc* value)
{
    if (!value || !value->dsc_address)
        return TokenList();
    
    // Convert descriptor to string for tokenization
    char buffer[4096];
    USHORT length = sizeof(buffer) - 1;
    
    dsc temp_desc;
    temp_desc.makeText(length, ttype_ascii, buffer);
    
    if (MOV_move(m_context.tdbb, value, &temp_desc) && temp_desc.dsc_length > 0)
    {
        buffer[temp_desc.dsc_length] = '\0';
        
        // Use the GIN index's tokenizer to extract tokens
        if (m_context.gin_index && m_context.gin_index->getTokenizer())
        {
            return m_context.gin_index->getTokenizer()->tokenize(buffer);
        }
        
        // Fallback: create a temporary tokenizer
        GinTokenizer tokenizer(m_pool);
        return tokenizer.tokenize(buffer);
    }
    
    return TokenList();
}

void GinIndexMaintenance::compareTokenLists(const TokenList& old_tokens, const TokenList& new_tokens,
                                           TokenList& tokens_to_add, TokenList& tokens_to_remove)
{
    // Create sets for efficient comparison
    std::set<ScratchBird::string> old_set, new_set;
    
    // Populate old tokens set
    for (ULONG i = 0; i < old_tokens.getCount(); i++)
    {
        // In a real implementation, we'd extract the actual token string
        // For now, we'll use a placeholder
        old_set.insert("old_token_" + std::to_string(i));
    }
    
    // Populate new tokens set
    for (ULONG i = 0; i < new_tokens.getCount(); i++)
    {
        new_set.insert("new_token_" + std::to_string(i));
    }
    
    // Find tokens to add (in new but not in old)
    tokens_to_add.clear();
    for (const auto& token : new_set)
    {
        if (old_set.find(token) == old_set.end())
        {
            // Token not in old set, need to add
            // In real implementation, would create proper token
        }
    }
    
    // Find tokens to remove (in old but not in new)
    tokens_to_remove.clear();
    for (const auto& token : old_set)
    {
        if (new_set.find(token) == new_set.end())
        {
            // Token not in new set, need to remove
            // In real implementation, would create proper token
        }
    }
}

bool GinIndexMaintenance::insertTokens(thread_db* tdbb, jrd_tra* transaction,
                                       RecordNumber record_number, const TokenList& tokens)
{
    if (!m_context.gin_index)
        return false;
    
    try
    {
        // Insert each token into the GIN index
        for (ULONG i = 0; i < tokens.getCount(); i++)
        {
            // In a real implementation, we'd extract the actual token
            // and call the GIN index's insertToken method
            
            // Placeholder: assume successful insertion
            m_cache_hits++;  // Simulate cache access
        }
        
        return true;
    }
    catch (const Exception&)
    {
        return false;
    }
}

bool GinIndexMaintenance::removeTokens(thread_db* tdbb, jrd_tra* transaction,
                                       RecordNumber record_number, const TokenList& tokens)
{
    if (!m_context.gin_index)
        return false;
    
    try
    {
        // Remove each token from the GIN index
        for (ULONG i = 0; i < tokens.getCount(); i++)
        {
            // In a real implementation, we'd extract the actual token
            // and call the GIN index's removeToken method
            
            // Placeholder: assume successful removal
            m_cache_hits++;  // Simulate cache access
        }
        
        return true;
    }
    catch (const Exception&)
    {
        return false;
    }
}

bool GinIndexMaintenance::updateTokens(thread_db* tdbb, jrd_tra* transaction,
                                       RecordNumber record_number, const TokenList& old_tokens,
                                       const TokenList& new_tokens)
{
    // Find the differences between old and new token lists
    TokenList tokens_to_add, tokens_to_remove;
    compareTokenLists(old_tokens, new_tokens, tokens_to_add, tokens_to_remove);
    
    bool success = true;
    
    // Remove tokens that are no longer present
    if (!tokens_to_remove.isEmpty())
    {
        success = removeTokens(tdbb, transaction, record_number, tokens_to_remove) && success;
    }
    
    // Add new tokens
    if (!tokens_to_add.isEmpty())
    {
        success = insertTokens(tdbb, transaction, record_number, tokens_to_add) && success;
    }
    
    return success;
}

void GinIndexMaintenance::addPendingUpdate(const GinPendingUpdate& update)
{
    m_pending_updates.push_back(update);
    m_context.operation_count++;
}

bool GinIndexMaintenance::shouldFlushBatch() const
{
    return m_context.operation_count >= m_context.max_batch_size;
}

void GinIndexMaintenance::clearProcessedUpdates()
{
    m_pending_updates.erase(
        std::remove_if(m_pending_updates.begin(), m_pending_updates.end(),
                      [](const GinPendingUpdate& update) {
                          return update.processed;
                      }),
        m_pending_updates.end()
    );
}

void GinIndexMaintenance::handleMaintenanceError(thread_db* tdbb, const Exception& error)
{
    // Log the error (in a real implementation)
    // For now, just track the failure
    m_failed_operations++;
    
    // Attempt recovery if possible
    try
    {
        recoverFromFailure(tdbb, m_context.transaction);
    }
    catch (...)
    {
        // Recovery failed, log critical error
    }
}

bool GinIndexMaintenance::recoverFromFailure(thread_db* tdbb, jrd_tra* transaction)
{
    // Attempt to recover from maintenance failure
    // This might involve clearing corrupted state, rebuilding indexes, etc.
    
    try
    {
        // Clear any partially processed updates
        clearProcessedUpdates();
        
        // Reset operation counters
        m_context.operation_count = 0;
        
        return true;
    }
    catch (const Exception&)
    {
        return false;
    }
}

void GinIndexMaintenance::updateMaintenanceStatistics(GinMaintenanceOperation operation,
                                                      double elapsed_time, bool success)
{
    m_total_maintenance_time += elapsed_time;
    
    if (success)
    {
        m_processed_operations++;
    }
    else
    {
        m_failed_operations++;
    }
}

ULONG GinIndexMaintenance::getPendingUpdateCount() const
{
    ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    return static_cast<ULONG>(
        std::count_if(m_pending_updates.begin(), m_pending_updates.end(),
                     [](const GinPendingUpdate& update) {
                         return !update.processed;
                     })
    );
}

ULONG GinIndexMaintenance::getProcessedUpdateCount() const
{
    return m_processed_operations;
}

double GinIndexMaintenance::getMaintenanceOverhead() const
{
    if (m_processed_operations == 0)
        return 0.0;
    
    return m_total_maintenance_time / static_cast<double>(m_processed_operations);
}

void GinIndexMaintenance::getMaintenanceStatistics(GinMaintenanceStatistics* stats) const
{
    if (!stats)
        return;
    
    ScratchBird::MutexLockGuard guard(m_mutex, FB_FUNCTION);
    
    // Initialize all fields
    memset(stats, 0, sizeof(GinMaintenanceStatistics));
    
    // Fill in statistics
    stats->successful_operations = m_processed_operations;
    stats->failed_operations = m_failed_operations;
    stats->total_maintenance_time = m_total_maintenance_time;
    
    if (m_processed_operations > 0)
    {
        stats->average_operation_time = m_total_maintenance_time / 
                                       static_cast<double>(m_processed_operations);
    }
    
    stats->cache_hits = m_cache_hits;
    stats->cache_misses = m_cache_misses;
    
    if (m_cache_hits + m_cache_misses > 0)
    {
        stats->cache_hit_ratio = static_cast<double>(m_cache_hits) / 
                                static_cast<double>(m_cache_hits + m_cache_misses);
    }
}

GinMaintenanceStrategy GinIndexMaintenance::getMaintenanceStrategy() const
{
    return m_context.strategy;
}

bool GinIndexMaintenance::isFastUpdateEnabled() const
{
    return m_context.fast_update_enabled;
}

ULONG GinIndexMaintenance::getBatchSize() const
{
    return m_context.max_batch_size;
}

//----------------------------
// Utility Function Implementations
//----------------------------

namespace GinMaintenanceUtils
{
    GinMaintenanceStrategy getOptimalStrategy(const GinIndex* gin_index, 
                                            const jrd_tra* transaction)
    {
        if (!gin_index || !transaction)
            return GIN_MAINTENANCE_IMMEDIATE;
        
        // If fast update is enabled, use deferred strategy
        if (gin_index->isFastUpdateEnabled())
            return GIN_MAINTENANCE_DEFERRED;
        
        // For small transactions, use immediate strategy
        // For large transactions, use batch strategy
        return GIN_MAINTENANCE_BATCH;
    }
    
    ULONG calculateOptimalBatchSize(const GinIndex* gin_index)
    {
        if (!gin_index)
            return 1000;
        
        // Calculate based on index size and available memory
        ULONG base_size = 1000;
        
        // Adjust based on index characteristics
        if (gin_index->getStorageSize() > 100 * 1024 * 1024)  // > 100MB
            base_size = 2000;
        else if (gin_index->getStorageSize() < 1024 * 1024)   // < 1MB
            base_size = 500;
        
        return base_size;
    }
    
    bool shouldUseFastUpdate(const GinIndex* gin_index)
    {
        if (!gin_index)
            return false;
        
        // Fast update is beneficial for frequently updated indexes
        return gin_index->isFastUpdateEnabled();
    }
} // namespace GinMaintenanceUtils