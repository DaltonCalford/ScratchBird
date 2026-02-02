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
// Index Cache - LRU cache for frequently accessed index instances
//
// This file provides an LRU (Least Recently Used) cache for index instances to improve
// performance by avoiding repeated index opens from disk.
//
// Design:
// - LRU eviction policy when cache is full
// - Thread-safe access with mutex protection
// - Configurable cache size (default: 128 indexes)
// - Cache statistics (hits, misses, evictions)
// - Supports all 11 index types
//
// November 19, 2025

#pragma once

#include "scratchbird/core/uuidv7.h"  // For UuidV7Bytes and ID type alias
#include <memory>
#include <unordered_map>
#include <list>
#include <mutex>
#include <cstdint>
#include <chrono>

// Forward declarations
namespace scratchbird {
namespace core {
    class BTree;
    class HashIndex;
    class GinIndex;
    class GiSTIndex;
    class SPGiSTIndex;  // Note: SP-GiST uses all-caps naming
    class BrinIndex;
    class RTreeIndex;
    class HnswIndex;
    class BitmapIndex;
    class ColumnstoreIndex;
    class LSMTree;
}
}

namespace scratchbird {
namespace sblr {

    // Index type enum (same as in opcodes.h)
    enum class IndexType : uint8_t;

    // Cache entry holds the index instance and metadata
    struct IndexCacheEntry
    {
        void* index_ptr;              // Type-erased index pointer
        IndexType type;               // Index type for safe casting
        uint64_t last_access_time;    // Timestamp for LRU tracking
        uint32_t access_count;        // Number of accesses (for statistics)

        IndexCacheEntry(void* ptr, IndexType t)
            : index_ptr(ptr), type(t), last_access_time(0), access_count(0) {}
    };

    // LRU cache for index instances
    class IndexCache
    {
    public:
        // Constructor with configurable cache size
        explicit IndexCache(size_t max_size = 128);

        // Destructor - cleans up cached indexes
        ~IndexCache();

        // Cache operations
        // Returns nullptr if not found in cache
        void* get(const core::ID& index_uuid, IndexType expected_type);

        // Insert index into cache (takes ownership)
        void put(const core::ID& index_uuid, IndexType type, void* index_ptr);

        // Remove specific index from cache
        void remove(const core::ID& index_uuid);

        // Clear entire cache
        void clear();

        // Cache statistics
        struct Statistics
        {
            uint64_t hits;          // Number of cache hits
            uint64_t misses;        // Number of cache misses
            uint64_t evictions;     // Number of LRU evictions
            uint64_t insertions;    // Number of insertions
            size_t current_size;    // Current number of cached indexes
            size_t max_size;        // Maximum cache capacity
        };

        Statistics getStatistics() const;
        void resetStatistics();

    private:
        // LRU list node - stores index UUID for ordering
        using LRUNode = std::pair<uint8_t*, IndexCacheEntry>;  // UUID bytes + entry
        using LRUList = std::list<LRUNode>;
        using LRUIterator = LRUList::iterator;

        // Hash map for O(1) lookup: UUID -> LRU list iterator
        struct UUIDHash
        {
            size_t operator()(const uint8_t* uuid) const;
        };

        struct UUIDEqual
        {
            bool operator()(const uint8_t* lhs, const uint8_t* rhs) const;
        };

        std::unordered_map<const uint8_t*, LRUIterator, UUIDHash, UUIDEqual> cache_map_;
        LRUList lru_list_;
        size_t max_size_;

        // Statistics
        mutable uint64_t hits_;
        mutable uint64_t misses_;
        uint64_t evictions_;
        uint64_t insertions_;

        // Thread safety
        mutable std::mutex mutex_;

        // Helper methods
        void evictLRU();  // Evict least recently used entry
        void moveToFront(LRUIterator it);  // Move entry to front (most recent)
        void deleteIndex(void* ptr, IndexType type);  // Type-safe deletion
        uint64_t getCurrentTime() const;  // Get current timestamp
    };

} // namespace sblr
} // namespace scratchbird
