/**
 * LSM Block Cache Implementation
 *
 * LRU cache for SSTable blocks with O(1) lookup, insertion, and eviction.
 *
 * Data structures:
 * - Hash map: O(1) lookup by cache key
 * - Doubly-linked list: O(1) LRU tracking and eviction
 *
 * Thread safety: All operations are mutex-protected
 *
 * November 22, 2025
 */

#include "scratchbird/core/lsm_block_cache.h"
#include <algorithm>

namespace scratchbird
{
namespace core
{

// ============================================================================
// Constructor
// ============================================================================

LSMBlockCache::LSMBlockCache(size_t max_size_bytes)
    : max_size_bytes_(max_size_bytes),
      current_size_bytes_(0),
      cache_hits_(0),
      cache_misses_(0)
{
}

// ============================================================================
// Cache Operations
// ============================================================================

bool LSMBlockCache::get(const std::string& file_path,
                       uint64_t block_offset,
                       std::vector<uint8_t>* block_out)
{
    if (!block_out)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    CacheKey key{file_path, block_offset};
    auto it = cache_map_.find(key);

    if (it == cache_map_.end())
    {
        // Cache miss
        cache_misses_++;
        return false;
    }

    // Cache hit - copy data and move to front of LRU list
    *block_out = it->second.first.block;
    touchKey(key, it->second.second);
    cache_hits_++;

    return true;
}

void LSMBlockCache::put(const std::string& file_path,
                       uint64_t block_offset,
                       const std::vector<uint8_t>& block)
{
    if (block.empty())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    CacheKey key{file_path, block_offset};
    size_t block_size = block.size();

    // Check if key already exists
    auto it = cache_map_.find(key);
    if (it != cache_map_.end())
    {
        // Update existing entry
        size_t old_size = it->second.first.size_bytes;
        it->second.first = CacheEntry(block);

        // Update size
        current_size_bytes_ = current_size_bytes_ - old_size + block_size;

        // Move to front of LRU list
        touchKey(key, it->second.second);

        return;
    }

    // New entry - evict if necessary
    evictToFit(block_size);

    // Add to front of LRU list
    lru_list_.push_front(key);
    LRUIterator lru_it = lru_list_.begin();

    // Add to hash map
    cache_map_[key] = std::make_pair(CacheEntry(block), lru_it);

    // Update size
    current_size_bytes_ += block_size;
}

void LSMBlockCache::invalidate(const std::string& file_path)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove all entries with matching file_path
    auto it = cache_map_.begin();
    while (it != cache_map_.end())
    {
        if (it->first.file_path == file_path)
        {
            // Remove from LRU list
            lru_list_.erase(it->second.second);

            // Update size
            current_size_bytes_ -= it->second.first.size_bytes;

            // Remove from map
            it = cache_map_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void LSMBlockCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);

    cache_map_.clear();
    lru_list_.clear();
    current_size_bytes_ = 0;
}

void LSMBlockCache::getStatistics(uint64_t* hits_out,
                                 uint64_t* misses_out,
                                 size_t* size_bytes_out,
                                 size_t* num_entries_out)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (hits_out)
    {
        *hits_out = cache_hits_;
    }

    if (misses_out)
    {
        *misses_out = cache_misses_;
    }

    if (size_bytes_out)
    {
        *size_bytes_out = current_size_bytes_;
    }

    if (num_entries_out)
    {
        *num_entries_out = cache_map_.size();
    }
}

// ============================================================================
// Helper Methods (Private)
// ============================================================================

void LSMBlockCache::evictToFit(size_t incoming_size)
{
    // Evict LRU blocks until we have enough space
    while (current_size_bytes_ + incoming_size > max_size_bytes_ && !lru_list_.empty())
    {
        // Get LRU key (back of list)
        CacheKey lru_key = lru_list_.back();

        // Find in cache map
        auto it = cache_map_.find(lru_key);
        if (it != cache_map_.end())
        {
            // Update size
            current_size_bytes_ -= it->second.first.size_bytes;

            // Remove from map
            cache_map_.erase(it);
        }

        // Remove from LRU list
        lru_list_.pop_back();
    }
}

void LSMBlockCache::touchKey(const CacheKey& key, LRUIterator it)
{
    // Move key to front of LRU list (mark as most recently used)
    lru_list_.erase(it);
    lru_list_.push_front(key);

    // Update iterator in cache map
    cache_map_[key].second = lru_list_.begin();
}

} // namespace core
} // namespace scratchbird
