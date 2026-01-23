// =================================================================================================
// ScratchBird Database Engine
// Index Cache Implementation
// November 19, 2025
// =================================================================================================

#include "scratchbird/sblr/index_cache.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/gin_index.h"
#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/rtree_index.h"
#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/columnstore_index.h"
#include "scratchbird/core/lsm_tree_index.h"
#include "scratchbird/core/uuidv7.h"

#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace sblr {

    // =================================================================================================
    // UUID Hash and Equality Functions
    // =================================================================================================

    size_t IndexCache::UUIDHash::operator()(const uint8_t* uuid) const
    {
        // Simple hash: XOR first 8 bytes with last 8 bytes
        uint64_t hash = 0;
        for (int i = 0; i < 8; i++)
        {
            hash ^= (static_cast<uint64_t>(uuid[i]) << (i * 8));
            hash ^= (static_cast<uint64_t>(uuid[i + 8]) << (i * 8));
        }
        return static_cast<size_t>(hash);
    }

    bool IndexCache::UUIDEqual::operator()(const uint8_t* lhs, const uint8_t* rhs) const
    {
        return std::memcmp(lhs, rhs, 16) == 0;
    }

    // =================================================================================================
    // IndexCache Implementation
    // =================================================================================================

    IndexCache::IndexCache(size_t max_size)
        : max_size_(max_size),
          hits_(0),
          misses_(0),
          evictions_(0),
          insertions_(0)
    {
        // Reserve space for cache map
        cache_map_.reserve(max_size);
    }

    IndexCache::~IndexCache()
    {
        clear();
    }

    void* IndexCache::get(const core::ID& index_uuid, IndexType expected_type)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(index_uuid.bytes.data());
        if (it == cache_map_.end())
        {
            // Cache miss
            misses_++;
            return nullptr;
        }

        // Cache hit
        hits_++;

        // Get the LRU list iterator
        auto lru_it = it->second;
        IndexCacheEntry& entry = lru_it->second;

        // Verify type matches
        if (entry.type != expected_type)
        {
            // Type mismatch - should not happen, but return nullptr for safety
            return nullptr;
        }

        // Update access metadata
        entry.last_access_time = getCurrentTime();
        entry.access_count++;

        // Move to front of LRU list (most recently used)
        moveToFront(lru_it);

        return entry.index_ptr;
    }

    void IndexCache::put(const core::ID& index_uuid, IndexType type, void* index_ptr)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if already in cache
        auto it = cache_map_.find(index_uuid.bytes.data());
        if (it != cache_map_.end())
        {
            // Already exists - update it
            auto lru_it = it->second;

            // Delete old instance
            deleteIndex(lru_it->second.index_ptr, lru_it->second.type);

            // Update entry
            lru_it->second.index_ptr = index_ptr;
            lru_it->second.type = type;
            lru_it->second.last_access_time = getCurrentTime();
            lru_it->second.access_count = 0;

            // Move to front
            moveToFront(lru_it);
            return;
        }

        // Evict if cache is full
        if (cache_map_.size() >= max_size_)
        {
            evictLRU();
        }

        // Allocate UUID copy for storage
        uint8_t* uuid_copy = new uint8_t[16];
        std::memcpy(uuid_copy, index_uuid.bytes.data(), 16);

        // Create entry
        IndexCacheEntry entry(index_ptr, type);
        entry.last_access_time = getCurrentTime();
        entry.access_count = 0;

        // Add to front of LRU list
        lru_list_.emplace_front(uuid_copy, entry);

        // Add to hash map
        cache_map_[uuid_copy] = lru_list_.begin();

        insertions_++;
    }

    void IndexCache::remove(const core::ID& index_uuid)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = cache_map_.find(index_uuid.bytes.data());
        if (it == cache_map_.end())
        {
            return;  // Not in cache
        }

        // Get LRU list iterator
        auto lru_it = it->second;

        // Delete the index
        deleteIndex(lru_it->second.index_ptr, lru_it->second.type);

        // Delete UUID copy
        delete[] lru_it->first;

        // Remove from list and map
        lru_list_.erase(lru_it);
        cache_map_.erase(it);
    }

    void IndexCache::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Delete all cached indexes and UUID copies
        for (auto& node : lru_list_)
        {
            deleteIndex(node.second.index_ptr, node.second.type);
            delete[] node.first;
        }

        lru_list_.clear();
        cache_map_.clear();
    }

    IndexCache::Statistics IndexCache::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        Statistics stats;
        stats.hits = hits_;
        stats.misses = misses_;
        stats.evictions = evictions_;
        stats.insertions = insertions_;
        stats.current_size = cache_map_.size();
        stats.max_size = max_size_;

        return stats;
    }

    void IndexCache::resetStatistics()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        hits_ = 0;
        misses_ = 0;
        evictions_ = 0;
        insertions_ = 0;
    }

    // =================================================================================================
    // Private Helper Methods
    // =================================================================================================

    void IndexCache::evictLRU()
    {
        // Called with mutex held

        if (lru_list_.empty())
        {
            return;
        }

        // LRU entry is at the back
        auto& lru_node = lru_list_.back();

        // Delete the index
        deleteIndex(lru_node.second.index_ptr, lru_node.second.type);

        // Remove from map
        cache_map_.erase(lru_node.first);

        // Delete UUID copy
        delete[] lru_node.first;

        // Remove from list
        lru_list_.pop_back();

        evictions_++;
    }

    void IndexCache::moveToFront(LRUIterator it)
    {
        // Called with mutex held

        if (it == lru_list_.begin())
        {
            return;  // Already at front
        }

        // Move to front (splice)
        lru_list_.splice(lru_list_.begin(), lru_list_, it);
    }

    void IndexCache::deleteIndex(void* ptr, IndexType type)
    {
        // Called with mutex held
        // Type-safe deletion using the correct destructor

        if (ptr == nullptr)
        {
            return;
        }

        switch (type)
        {
            case IndexType::BTREE:
                delete static_cast<core::BTree*>(ptr);
                break;

            case IndexType::HASH:
                delete static_cast<core::HashIndex*>(ptr);
                break;

            case IndexType::GIN:
                delete static_cast<core::GinIndex*>(ptr);
                break;

            case IndexType::GIST:
                delete static_cast<core::GiSTIndex*>(ptr);
                break;

            case IndexType::SPGIST:
                delete static_cast<core::SPGiSTIndex*>(ptr);
                break;

            case IndexType::BRIN:
                delete static_cast<core::BrinIndex*>(ptr);
                break;

            case IndexType::RTREE:
                delete static_cast<core::RTreeIndex*>(ptr);
                break;

            case IndexType::HNSW:
                delete static_cast<core::HnswIndex*>(ptr);
                break;

            case IndexType::BITMAP:
                delete static_cast<core::BitmapIndex*>(ptr);
                break;

            case IndexType::COLUMNSTORE:
                delete static_cast<core::ColumnstoreIndexSimple*>(ptr);
                break;

            case IndexType::LSM:
                delete static_cast<core::LSMTreeIndex*>(ptr);
                break;

            default:
                // Unknown type - leak rather than crash
                // This should never happen
                break;
        }
    }

    uint64_t IndexCache::getCurrentTime() const
    {
        // Get monotonic timestamp in microseconds
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }

} // namespace sblr
} // namespace scratchbird
