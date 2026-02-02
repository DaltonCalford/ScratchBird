/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Protocol Translation Cache
 *
 * Caches SQL -> SBLR bytecode translations per dialect.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace scratchbird::protocol
{

struct TranslationCacheConfig
{
    size_t max_entries = 1024;
    size_t max_bytes = 64 * 1024 * 1024;
    std::chrono::seconds ttl = std::chrono::seconds(300);
    bool enabled = true;
};

struct TranslationCacheStats
{
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t evictions = 0;
    size_t current_entries = 0;
    size_t current_bytes = 0;
};

class TranslationCache
{
public:
    explicit TranslationCache(const TranslationCacheConfig& config = TranslationCacheConfig());

    bool get(const std::string& dialect,
             const std::string& sql,
             uint64_t schema_version,
             const std::string& privilege_signature,
             std::vector<uint8_t>& bytecode_out);
    void put(const std::string& dialect,
             const std::string& sql,
             uint64_t schema_version,
             const std::string& privilege_signature,
             std::vector<uint8_t> bytecode);
    void invalidateAll();

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool isEnabled() const { return enabled_; }
    TranslationCacheStats stats() const;
    void resetStats();

private:
    struct CacheKey
    {
        std::string dialect;
        std::string sql;
        uint64_t schema_version = 0;
        std::string privilege_signature;

        bool operator==(const CacheKey& other) const
        {
            return dialect == other.dialect && sql == other.sql &&
                   schema_version == other.schema_version &&
                   privilege_signature == other.privilege_signature;
        }
    };

    struct CacheKeyHash
    {
        size_t operator()(const CacheKey& key) const
        {
            size_t h1 = std::hash<std::string>{}(key.dialect);
            size_t h2 = std::hash<std::string>{}(key.sql);
            size_t h3 = std::hash<uint64_t>{}(key.schema_version);
            size_t h4 = std::hash<std::string>{}(key.privilege_signature);
            size_t combined = h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
            combined ^= (h3 + 0x9e3779b9 + (combined << 6) + (combined >> 2));
            combined ^= (h4 + 0x9e3779b9 + (combined << 6) + (combined >> 2));
            return combined;
        }
    };

    struct CacheEntry
    {
        std::vector<uint8_t> bytecode;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_access;
        size_t size_bytes = 0;
    };

    using LruList = std::list<std::pair<CacheKey, CacheEntry>>;
    using LruIter = LruList::iterator;

    bool isExpired(const CacheEntry& entry,
                   const std::chrono::steady_clock::time_point& now) const;
    size_t estimateSize(const CacheKey& key, const CacheEntry& entry) const;
    void touch(LruIter it);
    void evictOne();

    TranslationCacheConfig config_;
    bool enabled_ = true;

    std::unordered_map<CacheKey, LruIter, CacheKeyHash> cache_;
    LruList lru_;
    size_t current_bytes_ = 0;

    mutable std::shared_mutex mutex_;
    mutable TranslationCacheStats stats_;
};

class TranslationCacheManager
{
public:
    static TranslationCache& getInstance();

    TranslationCacheManager(const TranslationCacheManager&) = delete;
    TranslationCacheManager& operator=(const TranslationCacheManager&) = delete;

private:
    TranslationCacheManager() = default;
};

} // namespace scratchbird::protocol
