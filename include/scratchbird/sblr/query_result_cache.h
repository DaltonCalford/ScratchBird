/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// Query Result Cache - LRU cache for SELECT query results
//
// P2-19: Query Result Caching
// This file provides an LRU cache for query results to improve performance
// by avoiding repeated query execution for identical queries.
//
// Design:
// - Cache key: SHA-256 hash of SQL text (or bytecode)
// - LRU eviction policy when cache is full
// - Automatic invalidation when tables are modified
// - Thread-safe access with read-write lock
// - Configurable cache size (default: 64 entries, 64MB max memory)
// - Cache statistics (hits, misses, evictions, invalidations)
//
// November 25, 2025

#pragma once

#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/types.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <string>
#include <chrono>
#include <cstdint>
#include <array>

namespace scratchbird {
namespace sblr {

    // Hash type for cache keys (SHA-256 produces 32 bytes)
    using QueryHash = std::array<uint8_t, 32>;

    // Query hash helper for unordered_map
    struct QueryHashHash
    {
        size_t operator()(const QueryHash& hash) const
        {
            // Use first 8 bytes as size_t hash
            size_t result = 0;
            for (int i = 0; i < 8; ++i) {
                result = (result << 8) | hash[i];
            }
            return result;
        }
    };

    // Cached result set entry
    struct CachedResultSet
    {
        // Column metadata
        std::vector<std::string> column_names;
        std::vector<core::DataType> column_types;

        // Row data
        std::vector<std::vector<core::TypedValue>> rows;

        // Tables involved in this query (for invalidation)
        std::unordered_set<core::ID, core::IDHash> table_ids;

        // Cache metadata
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_accessed;
        uint64_t access_count;

        // Memory estimation
        size_t estimated_size_bytes;

        CachedResultSet()
            : access_count(0), estimated_size_bytes(0)
        {
            created_at = std::chrono::steady_clock::now();
            last_accessed = created_at;
        }
    };

    // LRU cache for query results
    class QueryResultCache
    {
    public:
        // Constructor with configurable limits
        // max_entries: Maximum number of cached queries
        // max_memory_bytes: Maximum total memory usage (approximate)
        explicit QueryResultCache(size_t max_entries = 64,
                                 size_t max_memory_bytes = 64 * 1024 * 1024);

        ~QueryResultCache();

        // Disable copy (cache owns resources)
        QueryResultCache(const QueryResultCache&) = delete;
        QueryResultCache& operator=(const QueryResultCache&) = delete;

        // Cache operations

        /**
         * Compute hash of SQL text for cache lookup
         * @param sql_text SQL query string
         * @return SHA-256 hash of the SQL text
         */
        static QueryHash computeHash(const std::string& sql_text);

        /**
         * Compute hash of bytecode for cache lookup
         * @param bytecode Compiled SBLR bytecode
         * @return SHA-256 hash of the bytecode
         */
        static QueryHash computeHash(const std::vector<uint8_t>& bytecode);

        /**
         * Look up cached result by hash and copy it out.
         * @param hash Query hash
         * @param out Filled with cached result when found
         * @return True if found, false otherwise
         */
        bool get(const QueryHash& hash, CachedResultSet& out);

        /**
         * Insert result into cache
         * @param hash Query hash
         * @param result Result set to cache (moved into cache)
         */
        void put(const QueryHash& hash, CachedResultSet result);

        /**
         * Invalidate all cached results that reference a table
         * Called when INSERT/UPDATE/DELETE modifies a table
         * @param table_id Table UUID that was modified
         */
        void invalidateTable(const core::ID& table_id);

        /**
         * Invalidate all cached results (e.g., on DDL changes)
         */
        void invalidateAll();

        /**
         * Remove specific entry from cache
         * @param hash Query hash to remove
         */
        void remove(const QueryHash& hash);

        // Configuration

        /**
         * Enable or disable caching
         * @param enabled True to enable caching
         */
        void setEnabled(bool enabled) { enabled_ = enabled; }

        /**
         * Check if caching is enabled
         * @return True if caching is enabled
         */
        bool isEnabled() const { return enabled_; }

        /**
         * Set maximum cache entries
         * @param max_entries Maximum number of entries
         */
        void setMaxEntries(size_t max_entries);

        /**
         * Set maximum memory usage
         * @param max_bytes Maximum memory in bytes
         */
        void setMaxMemory(size_t max_bytes);

        // Statistics

        struct Statistics
        {
            uint64_t hits;              // Number of cache hits
            uint64_t misses;            // Number of cache misses
            uint64_t evictions;         // Number of LRU evictions
            uint64_t invalidations;     // Number of table-based invalidations
            uint64_t insertions;        // Number of insertions
            size_t current_entries;     // Current number of cached entries
            size_t current_memory;      // Current memory usage (approximate)
            size_t max_entries;         // Maximum entries limit
            size_t max_memory;          // Maximum memory limit
        };

        Statistics getStatistics() const;
        void resetStatistics();

    private:
        // LRU list: front = most recently used, back = least recently used
        using LRUList = std::list<std::pair<QueryHash, CachedResultSet>>;
        using LRUIterator = LRUList::iterator;

        // Hash map: query hash -> LRU list iterator
        std::unordered_map<QueryHash, LRUIterator, QueryHashHash> cache_map_;

        // LRU list for eviction ordering
        LRUList lru_list_;

        // Reverse index: table_id -> set of query hashes that reference it
        std::unordered_map<core::ID, std::unordered_set<QueryHash, QueryHashHash>, core::IDHash> table_to_queries_;

        // Limits
        size_t max_entries_;
        size_t max_memory_bytes_;
        size_t current_memory_bytes_;

        // Enable flag
        bool enabled_;

        // Statistics
        mutable uint64_t hits_;
        mutable uint64_t misses_;
        uint64_t evictions_;
        uint64_t invalidations_;
        uint64_t insertions_;

        // Thread safety (read-write lock for better concurrency)
        mutable std::shared_mutex mutex_;

        // Helper methods
        void evictLRU();                    // Evict least recently used entry
        void evictUntilMemoryFits();        // Evict until under memory limit
        void moveToFront(LRUIterator it);   // Move entry to front (most recent)
        void removeEntry(LRUIterator it);   // Remove entry and update all indexes
        size_t estimateResultSize(const CachedResultSet& result) const;
    };

    // Global query result cache (singleton for convenience)
    // Use getInstance() to access
    class QueryResultCacheManager
    {
    public:
        static QueryResultCache& getInstance();

        // Prevent copies
        QueryResultCacheManager(const QueryResultCacheManager&) = delete;
        QueryResultCacheManager& operator=(const QueryResultCacheManager&) = delete;

    private:
        QueryResultCacheManager() = default;
    };

} // namespace sblr
} // namespace scratchbird
