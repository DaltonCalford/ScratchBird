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

#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/security_quorum.h"
#include <unordered_map>
#include <list>
#include <optional>
#include <chrono>
#include <shared_mutex>
#include <functional>

namespace scratchbird::core
{
    /**
     * Permission check mode for cache behavior
     *
     * SECURITY ENHANCEMENT (MEDIUM-1): Verify mode for security-critical operations
     */
    enum class PermissionCheckMode
    {
        CACHED = 0,    // Use cache if available (default, fast path)
        VERIFIED = 1   // Always check database (security-critical operations)
    };

    /**
     * Permission Cache - Phase 3.2.3
     *
     * Global permission cache with LRU eviction and TTL expiration.
     *
     * P2-4: TTL reduced from 60s to 10s to minimize TOCTOU (time-of-check to time-of-use)
     * race window for permission changes.
     * Provides 2-5x additional performance improvement on top of Phase 3.2.1/3.2.2.
     *
     * Features:
     * - LRU eviction when cache is full
     * - TTL-based expiration (default: 10 seconds - reduced from 60s for security)
     * - Thread-safe with shared_mutex (multiple readers, single writer)
     * - Cache invalidation on GRANT/REVOKE
     * - Performance statistics tracking
     * - VERIFIED mode for security-critical operations (bypasses cache)
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
         * @param ttl_seconds Time-to-live in seconds (default: 10)
         *
         * P2-4: Reduced TTL from 60s to 10s to minimize TOCTOU race window.
         * Permission changes now propagate to all sessions within 10 seconds.
         */
        explicit PermissionCache(size_t max_entries = 1000,
                                 std::chrono::seconds ttl_seconds = std::chrono::seconds(10));

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
        void insert(const CacheKey &key, bool has_permission,
                    uint64_t policy_epoch_global = 0,
                    uint64_t policy_epoch_table = 0);

        /**
         * Check permission with optional cache bypass for security-critical operations
         *
         * SECURITY ENHANCEMENT (MEDIUM-1): Supports VERIFIED mode for security-critical operations
         *
         * @param catalog Catalog manager for database lookup
         * @param key Cache key
         * @param mode Permission check mode (CACHED or VERIFIED)
         * @return Permission result (always fresh in VERIFIED mode)
         *
         * Behavior:
         * - CACHED mode: Use cache if available, query database on miss, cache result
         * - VERIFIED mode: Always query database, update cache with fresh value
         *
         * Use VERIFIED mode for:
         * - DROP operations (irreversible)
         * - DELETE operations (data loss)
         * - GRANT/REVOKE operations (security changes)
         * - ALTER operations on security-sensitive objects
         *
         * Thread-safe: Shared lock for reads, exclusive lock for writes
         */
        bool checkPermission(CatalogManager *catalog,
                           const CacheKey &key,
                           PermissionCheckMode mode,
                           ErrorContext *ctx);

        /**
         * Configure security quorum behavior for cache usage
         */
        void configureQuorum(const SecurityQuorumConfig &config);

        /**
         * Provide a quorum status provider (returns true/false or nullopt if unknown)
         */
        void setQuorumStatusProvider(std::function<std::optional<bool>()> provider);

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
            uint64_t policy_epoch_global;
            uint64_t policy_epoch_table;

            CacheEntry(bool perm, uint64_t epoch_global, uint64_t epoch_table)
                : has_permission(perm),
                  timestamp(std::chrono::steady_clock::now()),
                  access_count(1),
                  policy_epoch_global(epoch_global),
                  policy_epoch_table(epoch_table)
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

        // Quorum gate for cache usage
        SecurityQuorum quorum_;

        // Helper: Remove entry from LRU list
        void removeFromLRU(const CacheKey &key);

        // Helper: Move entry to front of LRU list
        void moveToFront(const CacheKey &key);

        // Helper: Check if entry is expired
        bool isExpired(const CacheEntry &entry) const;

        // Helper: Check cached entry with policy epochs
        std::optional<bool> lookupWithEpoch(const CacheKey &key,
                                            uint64_t policy_epoch_global,
                                            uint64_t policy_epoch_table);

        bool isEpochMismatch(const CacheEntry &entry,
                             uint64_t policy_epoch_global,
                             uint64_t policy_epoch_table) const;
    };

} // namespace scratchbird::core
