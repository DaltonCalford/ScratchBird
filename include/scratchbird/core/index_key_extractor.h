/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/toast.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace scratchbird::core
{

// Forward declarations
class HeapTuple;
class ToastManager;
struct ErrorContext;

/**
 * IndexKeyExtractor - Helper class for extracting index-ready keys from heap tuples
 *
 * Purpose:
 *   - Automatically detects and detoasts TOAST pointers in indexed columns
 *   - Provides "index-ready" keys to index insert/update operations
 *   - Caches detoasted values to avoid repeated detoasting for multiple indexes
 *   - Keeps index code simple and TOAST-unaware
 *
 * Architecture (Firebird MGA compliant):
 *   - Indexes ALWAYS store actual detoasted values, NEVER TOAST pointer bytes
 *   - Indexes ALWAYS point to heap tuple TIDs, NEVER to TOAST chunk TIDs
 *   - Detoasting happens in storage layer, NOT in index layer
 *   - This class is the clean interface between storage and indexes
 *
 * Usage:
 *   IndexKeyExtractor extractor;
 *   std::vector<uint8_t> key;
 *   extractor.extractKey(tuple, column_indices, toast_mgr, xid, &key, ctx);
 *   index->insert(key, tid, xid);  // Index receives detoasted value
 *
 * Reference: /docs/analysis/TOAST_INDEX_INTEGRATION_ANALYSIS.md
 */
class IndexKeyExtractor
{
public:
    IndexKeyExtractor();
    ~IndexKeyExtractor();

    /**
     * Extract index key from heap tuple with automatic detoasting
     *
     * @param tuple_data Raw tuple data
     * @param tuple_size Size of tuple data
     * @param column_offsets Byte offsets of each column in tuple
     * @param column_sizes Size of each column in tuple
     * @param column_indices Column indices to extract for index key
     * @param toast_mgr TOAST manager for detoasting (can be nullptr if no TOAST)
     * @param xid Transaction ID for visibility checks during detoasting
     * @param key_out Output buffer for extracted key
     * @param ctx Error context
     * @return Status::OK on success
     *
     * Behavior:
     *   - Extracts columns specified by column_indices
     *   - For each column:
     *     * If column is TOAST pointer (18 bytes with magic): detoast it
     *     * If column is inline data: use as-is
     *   - Concatenates all column values into key_out
     *   - Caches detoasted values for reuse
     */
    auto extractKey(
        const uint8_t* tuple_data,
        size_t tuple_size,
        const std::vector<size_t>& column_offsets,
        const std::vector<size_t>& column_sizes,
        const std::vector<uint16_t>& column_indices,
        ToastManager* toast_mgr,
        uint64_t xid,
        std::vector<uint8_t>* key_out,
        ErrorContext* ctx) -> Status;

    /**
     * Extract index keys for update operation (old and new keys)
     *
     * @param old_tuple_data Old tuple data (before update)
     * @param old_tuple_size Size of old tuple
     * @param old_column_offsets Column offsets in old tuple
     * @param old_column_sizes Column sizes in old tuple
     * @param new_tuple_data New tuple data (after update)
     * @param new_tuple_size Size of new tuple
     * @param new_column_offsets Column offsets in new tuple
     * @param new_column_sizes Column sizes in new tuple
     * @param column_indices Column indices to extract
     * @param toast_mgr TOAST manager
     * @param xid Transaction ID
     * @param old_key_out Output for old key (for index delete)
     * @param new_key_out Output for new key (for index insert)
     * @param ctx Error context
     * @return Status::OK on success
     *
     * Behavior:
     *   - Extracts both old and new keys
     *   - Detoasts TOAST pointers in both tuples
     *   - Caches detoasted values to avoid repeated work
     *   - Used for index updates when indexed column changes
     */
    auto extractKeyForUpdate(
        const uint8_t* old_tuple_data,
        size_t old_tuple_size,
        const std::vector<size_t>& old_column_offsets,
        const std::vector<size_t>& old_column_sizes,
        const uint8_t* new_tuple_data,
        size_t new_tuple_size,
        const std::vector<size_t>& new_column_offsets,
        const std::vector<size_t>& new_column_sizes,
        const std::vector<uint16_t>& column_indices,
        ToastManager* toast_mgr,
        uint64_t xid,
        std::vector<uint8_t>* old_key_out,
        std::vector<uint8_t>* new_key_out,
        ErrorContext* ctx) -> Status;

    /**
     * Clear detoasted value cache
     *
     * Call this after completing an insert/update operation to free memory.
     * Cache is per-operation to avoid repeated detoasting for multiple indexes.
     */
    void clearCache();

private:
    // Cache for detoasted values (column_index -> detoasted_value)
    // Avoids repeated detoasting when multiple indexes use same column
    std::unordered_map<uint16_t, std::vector<uint8_t>> detoast_cache_;

    /**
     * Get column value, detoasting if needed
     *
     * @param column_data Pointer to column data
     * @param column_size Size of column data
     * @param column_index Column index (for caching)
     * @param toast_mgr TOAST manager
     * @param xid Transaction ID
     * @param value_out Output buffer for column value
     * @param ctx Error context
     * @return Status::OK on success
     *
     * Checks cache first, detoasts only if not cached.
     */
    auto getColumnValue(
        const uint8_t* column_data,
        size_t column_size,
        uint16_t column_index,
        ToastManager* toast_mgr,
        uint64_t xid,
        std::vector<uint8_t>* value_out,
        ErrorContext* ctx) -> Status;
};

} // namespace scratchbird::core
