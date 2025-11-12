#include "scratchbird/core/permission_cache.h"
#include "scratchbird/core/logger.h"
#include <algorithm>

namespace scratchbird::core
{

    // ============================================================================
    // CacheKeyHash Implementation
    // ============================================================================

    size_t PermissionCache::CacheKeyHash::operator()(const CacheKey &key) const
    {
        // Combine hashes of all fields using XOR and bit shifts
        // This provides good distribution for typical UUID patterns

        // Hash user_id (16 bytes)
        size_t h1 = 0;
        const uint8_t *user_bytes = reinterpret_cast<const uint8_t *>(&key.user_id);
        for (size_t i = 0; i < sizeof(ID); ++i)
        {
            h1 = (h1 * 31) + user_bytes[i];
        }

        // Hash object_id (16 bytes)
        size_t h2 = 0;
        const uint8_t *obj_bytes = reinterpret_cast<const uint8_t *>(&key.object_id);
        for (size_t i = 0; i < sizeof(ID); ++i)
        {
            h2 = (h2 * 31) + obj_bytes[i];
        }

        // Hash object_type and privilege
        size_t h3 = static_cast<size_t>(key.object_type);
        size_t h4 = static_cast<size_t>(key.privilege);

        // Combine all hashes
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }

    // ============================================================================
    // PermissionCache Implementation
    // ============================================================================

    PermissionCache::PermissionCache(size_t max_entries, std::chrono::seconds ttl_seconds)
        : max_entries_(max_entries),
          ttl_(ttl_seconds),
          enabled_(true)
    {
        stats_.current_entries = 0;
        stats_.max_entries = max_entries;
        stats_.total_lookups = 0;
        stats_.hit_count = 0;
        stats_.miss_count = 0;
        stats_.eviction_count = 0;
        stats_.invalidation_count = 0;
        stats_.ttl_expiration_count = 0;

        LOG_INFO(GENERAL, "Permission cache initialized: max_entries=%zu, ttl=%llds",
                 max_entries_, ttl_.count());
    }

    PermissionCache::~PermissionCache()
    {
        LOG_INFO(GENERAL, "Permission cache destroyed: entries=%zu, hit_rate=%.2f%%",
                 stats_.current_entries, stats_.getHitRate());
    }

    std::optional<bool> PermissionCache::lookup(const CacheKey &key)
    {
        if (!enabled_)
        {
            return std::nullopt;
        }

        // Shared lock for reading
        std::shared_lock<std::shared_mutex> lock(mutex_);

        ++stats_.total_lookups;

        auto it = cache_.find(key);
        if (it == cache_.end())
        {
            // Cache miss
            ++stats_.miss_count;
            return std::nullopt;
        }

        // Check if entry is expired
        if (isExpired(it->second))
        {
            // Entry expired - need to remove it
            // Release shared lock and acquire exclusive lock
            lock.unlock();
            {
                std::unique_lock<std::shared_mutex> write_lock(mutex_);

                // Re-check in case another thread removed it
                auto it2 = cache_.find(key);
                if (it2 != cache_.end() && isExpired(it2->second))
                {
                    removeFromLRU(key);
                    cache_.erase(it2);
                    --stats_.current_entries;
                    ++stats_.ttl_expiration_count;
                }
            }

            ++stats_.miss_count;
            return std::nullopt;
        }

        // Cache hit!
        ++stats_.hit_count;

        // Update access count (const_cast is safe here since we have the lock)
        ++const_cast<CacheEntry &>(it->second).access_count;

        // Move to front of LRU (need exclusive lock)
        // For performance, we skip LRU update on lookups and only update on inserts
        // This reduces lock contention significantly

        return it->second.has_permission;
    }

    void PermissionCache::insert(const CacheKey &key, bool has_permission)
    {
        if (!enabled_)
        {
            return;
        }

        // Exclusive lock for writing
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Check if entry already exists (update case)
        auto it = cache_.find(key);
        if (it != cache_.end())
        {
            // Update existing entry
            it->second.has_permission = has_permission;
            it->second.timestamp = std::chrono::steady_clock::now();
            ++it->second.access_count;

            // Move to front of LRU
            moveToFront(key);
            return;
        }

        // Check if cache is full
        if (cache_.size() >= max_entries_)
        {
            // Evict least recently used entry
            const CacheKey &lru_key = lru_list_.back();
            cache_.erase(lru_key);
            lru_list_.pop_back();

            ++stats_.eviction_count;
            --stats_.current_entries;

            LOG_DEBUG(GENERAL, "Permission cache LRU eviction: cache_size=%zu", cache_.size());
        }

        // Insert new entry
        CacheEntry entry(has_permission);
        cache_.emplace(key, entry);
        lru_list_.push_front(key);
        ++stats_.current_entries;

        LOG_DEBUG(GENERAL, "Permission cache insert: has_perm=%d, cache_size=%zu",
                  has_permission, cache_.size());
    }

    void PermissionCache::invalidateUser(const ID &user_id)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        size_t removed_count = 0;
        auto it = cache_.begin();
        while (it != cache_.end())
        {
            if (it->first.user_id == user_id)
            {
                removeFromLRU(it->first);
                it = cache_.erase(it);
                ++removed_count;
            }
            else
            {
                ++it;
            }
        }

        stats_.current_entries = cache_.size();
        stats_.invalidation_count += removed_count;

        LOG_INFO(GENERAL, "Permission cache invalidated for user: removed=%zu entries", removed_count);
    }

    void PermissionCache::invalidateObject(const ID &object_id)
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        size_t removed_count = 0;
        auto it = cache_.begin();
        while (it != cache_.end())
        {
            if (it->first.object_id == object_id)
            {
                removeFromLRU(it->first);
                it = cache_.erase(it);
                ++removed_count;
            }
            else
            {
                ++it;
            }
        }

        stats_.current_entries = cache_.size();
        stats_.invalidation_count += removed_count;

        LOG_INFO(GENERAL, "Permission cache invalidated for object: removed=%zu entries", removed_count);
    }

    void PermissionCache::invalidateAll()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        size_t removed_count = cache_.size();

        cache_.clear();
        lru_list_.clear();
        stats_.current_entries = 0;
        stats_.invalidation_count += removed_count;

        LOG_INFO(GENERAL, "Permission cache completely invalidated: removed=%zu entries", removed_count);
    }

    PermissionCache::Statistics PermissionCache::getStatistics() const
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        stats_.current_entries = cache_.size();
        return stats_;
    }

    void PermissionCache::resetStatistics()
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        stats_.total_lookups = 0;
        stats_.hit_count = 0;
        stats_.miss_count = 0;
        stats_.eviction_count = 0;
        stats_.invalidation_count = 0;
        stats_.ttl_expiration_count = 0;

        LOG_INFO(GENERAL, "Permission cache statistics reset");
    }

    // ============================================================================
    // Helper Methods
    // ============================================================================

    void PermissionCache::removeFromLRU(const CacheKey &key)
    {
        // Remove key from LRU list
        // Note: This is O(N) but cache is typically small (1000 entries)
        auto it = std::find(lru_list_.begin(), lru_list_.end(), key);
        if (it != lru_list_.end())
        {
            lru_list_.erase(it);
        }
    }

    void PermissionCache::moveToFront(const CacheKey &key)
    {
        // Remove from current position and add to front
        removeFromLRU(key);
        lru_list_.push_front(key);
    }

    bool PermissionCache::isExpired(const CacheEntry &entry) const
    {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry.timestamp);
        return age > ttl_;
    }

} // namespace scratchbird::core
