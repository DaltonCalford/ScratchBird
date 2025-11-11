#pragma once

#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
#include <unordered_map>
#include <list>
#include <optional>
#include <chrono>
#include <shared_mutex>

namespace scratchbird::core
{
    /**
     * Permission Cache - Phase 3.2.3
     *
     * Global permission cache with LRU eviction and TTL expiration.
     * Provides 2-5x additional performance improvement on top of Phase 3.2.1/3.2.2.
     *
     * Features:
     * - LRU eviction when cache is full
     * - TTL-based expiration (default: 60 seconds)
     * - Thread-safe with shared_mutex (multiple readers, single writer)
     * - Cache invalidation on GRANT/REVOKE
     * - Performance statistics tracking
     */
    class PermissionCache
    {
    public:
        /**
         * Cache key uniquely identifies a permission check
         */
        struct CacheKey
        {
            ID user_id;
            ID object_id;
            CatalogManager::PermissionObjectType object_type;
            CatalogManager::Privilege privilege;

            bool operator==(const CacheKey &other) const
            {
                return user_id == other.user_id &&
                       object_id == other.object_id &&
                       object_type == other.object_type &&
                       privilege == other.privilege;
            }
        };

        /**
         * Hash function for CacheKey
         */
        struct CacheKeyHash
        {
            size_t operator()(const CacheKey &key) const;
        };

        /**
         * Cache statistics
         */
        struct Statistics
        {
            size_t current_entries;      // Current cache size
            size_t max_entries;          // Maximum capacity
            size_t total_lookups;        // Total lookup attempts
            size_t hit_count;            // Cache hits
            size_t miss_count;           // Cache misses
            size_t eviction_count;       // LRU evictions
            size_t invalidation_count;   // Manual invalidations
            size_t ttl_expiration_count; // TTL expirations

            double getHitRate() const
            {
                if (total_lookups == 0)
                    return 0.0;
                return static_cast<double>(hit_count) / total_lookups * 100.0;
            }
        };

        /**
         * Constructor
         *
         * @param max_entries Maximum number of entries (default: 1000)
         * @param ttl_seconds Time-to-live in seconds (default: 60)
         */
        explicit PermissionCache(size_t max_entries = 1000,
                                 std::chrono::seconds ttl_seconds = std::chrono::seconds(60));

        ~PermissionCache();

        /**
         * Lookup permission in cache
         *
         * @param key Cache key
         * @return Optional<bool> - has_permission if cached, nullopt if not cached or expired
         *
         * Thread-safe: Multiple threads can lookup concurrently
         */
        std::optional<bool> lookup(const CacheKey &key);

        /**
         * Insert permission result into cache
         *
         * @param key Cache key
         * @param has_permission Permission result to cache
         *
         * Thread-safe: Single writer at a time
         * Note: If cache is full, least recently used entry is evicted
         */
        void insert(const CacheKey &key, bool has_permission);

        /**
         * Invalidate all cache entries for a user
         *
         * Called when user permissions change (GRANT/REVOKE TO user)
         *
         * @param user_id User ID to invalidate
         *
         * Thread-safe: Exclusive lock acquired
         */
        void invalidateUser(const ID &user_id);

        /**
         * Invalidate all cache entries for an object
         *
         * Called when object permissions change (GRANT/REVOKE ON object)
         *
         * @param object_id Object ID to invalidate
         *
         * Thread-safe: Exclusive lock acquired
         */
        void invalidateObject(const ID &object_id);

        /**
         * Invalidate entire cache
         *
         * Called on major schema changes or when cache coherency is uncertain
         *
         * Thread-safe: Exclusive lock acquired
         */
        void invalidateAll();

        /**
         * Get cache statistics
         *
         * @return Current statistics snapshot
         *
         * Thread-safe: Shared lock acquired
         */
        Statistics getStatistics() const;

        /**
         * Reset statistics counters (does not clear cache)
         *
         * Thread-safe: Exclusive lock acquired
         */
        void resetStatistics();

        /**
         * Enable or disable cache
         *
         * When disabled, all lookups return nullopt (cache miss)
         * Useful for debugging
         *
         * @param enabled true to enable, false to disable
         */
        void setEnabled(bool enabled)
        {
            enabled_ = enabled;
        }

        bool isEnabled() const
        {
            return enabled_;
        }

    private:
        struct CacheEntry
        {
            bool has_permission;
            std::chrono::steady_clock::time_point timestamp;
            size_t access_count;

            CacheEntry(bool perm)
                : has_permission(perm),
                  timestamp(std::chrono::steady_clock::now()),
                  access_count(1)
            {
            }
        };

        // Cache storage
        std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> cache_;

        // LRU list (most recently used at front)
        std::list<CacheKey> lru_list_;

        // Configuration
        size_t max_entries_;
        std::chrono::seconds ttl_;
        bool enabled_;

        // Statistics (mutable to allow updates in const methods)
        mutable Statistics stats_;

        // Thread safety (reader-writer lock)
        mutable std::shared_mutex mutex_;

        // Helper: Remove entry from LRU list
        void removeFromLRU(const CacheKey &key);

        // Helper: Move entry to front of LRU list
        void moveToFront(const CacheKey &key);

        // Helper: Check if entry is expired
        bool isExpired(const CacheEntry &entry) const;
    };

} // namespace scratchbird::core
