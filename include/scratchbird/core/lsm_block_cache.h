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

/**
 * LSM Block Cache - LRU cache for SSTable blocks
 *
 * Purpose: Cache frequently accessed SSTable blocks in memory to reduce disk I/O
 *
 * Design:
 * - LRU eviction policy (least recently used)
 * - Thread-safe (mutex-protected)
 * - Configurable cache size (default: 64 MB)
 * - Cache key: <file_path, block_offset>
 * - Cache value: block data (vector<uint8_t>)
 *
 * Performance:
 * - O(1) lookup (hash map)
 * - O(1) insertion (hash map + doubly-linked list)
 * - O(1) eviction (LRU at tail of list)
 *
 * November 22, 2025
 */

#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <cstdint>
#include <string>

namespace scratchbird
{
namespace core
{

/**
 * LSMBlockCache - LRU cache for SSTable blocks
 *
 * Thread-safe LRU cache that stores recently accessed SSTable blocks
 * to reduce disk I/O during reads.
 *
 * Example usage:
 *   LSMBlockCache cache(64 * 1024 * 1024);  // 64 MB cache
 *
 *   // Try to get from cache
 *   std::vector<uint8_t> block;
 *   if (cache.get(file_path, offset, &block)) {
 *       // Cache hit - use cached data
 *   } else {
 *       // Cache miss - read from disk
 *       block = read_from_disk(file_path, offset);
 *       cache.put(file_path, offset, block);
 *   }
 */
class LSMBlockCache
{
public:
    /**
     * Constructor
     * @param max_size_bytes Maximum cache size in bytes (default: 64 MB)
     */
    explicit LSMBlockCache(size_t max_size_bytes = 64 * 1024 * 1024);

    ~LSMBlockCache() = default;

    /**
     * Get block from cache
     * @param file_path SSTable file path
     * @param block_offset Block offset within file
     * @param block_out Output buffer for block data
     * @return true if found in cache (cache hit), false otherwise (cache miss)
     */
    bool get(const std::string& file_path,
             uint64_t block_offset,
             std::vector<uint8_t>* block_out);

    /**
     * Put block into cache
     * @param file_path SSTable file path
     * @param block_offset Block offset within file
     * @param block Block data to cache
     */
    void put(const std::string& file_path,
             uint64_t block_offset,
             const std::vector<uint8_t>& block);

    /**
     * Invalidate (remove) all blocks for a given file
     * Called when an SSTable is deleted during compaction
     * @param file_path SSTable file path
     */
    void invalidate(const std::string& file_path);

    /**
     * Clear entire cache
     */
    void clear();

    /**
     * Get cache statistics
     * @param hits_out Number of cache hits
     * @param misses_out Number of cache misses
     * @param size_bytes_out Current cache size in bytes
     * @param num_entries_out Number of cached blocks
     */
    void getStatistics(uint64_t* hits_out,
                      uint64_t* misses_out,
                      size_t* size_bytes_out,
                      size_t* num_entries_out);

private:
    // Cache key: <file_path, block_offset>
    struct CacheKey
    {
        std::string file_path;
        uint64_t block_offset;

        bool operator==(const CacheKey& other) const
        {
            return file_path == other.file_path && block_offset == other.block_offset;
        }
    };

    // Hash function for CacheKey
    struct CacheKeyHash
    {
        size_t operator()(const CacheKey& key) const
        {
            // Combine file_path hash and block_offset
            size_t h1 = std::hash<std::string>{}(key.file_path);
            size_t h2 = std::hash<uint64_t>{}(key.block_offset);
            return h1 ^ (h2 << 1);
        }
    };

    // Cache entry: block data + size
    struct CacheEntry
    {
        std::vector<uint8_t> block;
        size_t size_bytes;

        CacheEntry() : size_bytes(0) {}
        CacheEntry(const std::vector<uint8_t>& b)
            : block(b), size_bytes(b.size()) {}
    };

    // LRU list node: cache key (most recently used at front, LRU at back)
    using LRUList = std::list<CacheKey>;
    using LRUIterator = LRUList::iterator;

    // Hash map: cache key -> (cache entry, LRU list iterator)
    using CacheMap = std::unordered_map<CacheKey, std::pair<CacheEntry, LRUIterator>, CacheKeyHash>;

    size_t max_size_bytes_;          // Maximum cache size
    size_t current_size_bytes_;      // Current cache size
    LRUList lru_list_;               // LRU list (front = MRU, back = LRU)
    CacheMap cache_map_;             // Hash map for O(1) lookup
    mutable std::mutex mutex_;       // Thread safety

    // Statistics
    uint64_t cache_hits_;
    uint64_t cache_misses_;

    // Helper: Evict LRU blocks until size <= max_size
    void evictToFit(size_t incoming_size);

    // Helper: Move key to front of LRU list (mark as most recently used)
    void touchKey(const CacheKey& key, LRUIterator it);
};

} // namespace core
} // namespace scratchbird
