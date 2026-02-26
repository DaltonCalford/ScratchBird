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
 * Protocol Translation Cache Implementation
 */

#include "scratchbird/protocol/translation_cache.h"
#include "scratchbird/core/telemetry.h"

namespace scratchbird::protocol
{

TranslationCache::TranslationCache(const TranslationCacheConfig& config)
    : config_(config), enabled_(config.enabled)
{
}

bool TranslationCache::isExpired(const CacheEntry& entry,
                                 const std::chrono::steady_clock::time_point& now) const
{
    if (config_.ttl.count() <= 0) {
        return false;
    }
    return (now - entry.created_at) > config_.ttl;
}

size_t TranslationCache::estimateSize(const CacheKey& key, const CacheEntry& entry) const
{
    return sizeof(CacheEntry) + sizeof(CacheKey) + key.dialect.size() + key.sql.size() +
           key.privilege_signature.size() + entry.bytecode.size();
}

void TranslationCache::touch(LruIter it)
{
    if (it == lru_.begin()) {
        return;
    }
    lru_.splice(lru_.begin(), lru_, it);
}

void TranslationCache::evictOne()
{
    if (lru_.empty()) {
        return;
    }
    auto it = std::prev(lru_.end());
    current_bytes_ -= it->second.size_bytes;
    cache_.erase(it->first);
    lru_.erase(it);
    ++stats_.evictions;
    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();
    if (metrics.translation_cache_evictions_total) {
        metrics.translation_cache_evictions_total->inc();
    }
}

bool TranslationCache::get(const std::string& dialect,
                           const std::string& sql,
                           uint64_t schema_version,
                           const std::string& privilege_signature,
                           std::vector<uint8_t>& bytecode_out)
{
    if (!enabled_) {
        return false;
    }

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    CacheKey key{dialect, sql, schema_version, privilege_signature};
    auto now = std::chrono::steady_clock::now();

    // get() updates access metadata and LRU ordering, so it requires an exclusive lock.
    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        ++stats_.misses;
        if (metrics.translation_cache_misses_total) {
            metrics.translation_cache_misses_total->inc();
        }
        return false;
    }

    if (isExpired(it->second->second, now)) {
        current_bytes_ -= it->second->second.size_bytes;
        lru_.erase(it->second);
        cache_.erase(it);
        ++stats_.evictions;
        if (metrics.translation_cache_evictions_total) {
            metrics.translation_cache_evictions_total->inc();
        }
        ++stats_.misses;
        if (metrics.translation_cache_misses_total) {
            metrics.translation_cache_misses_total->inc();
        }
        return false;
    }

    it->second->second.last_access = now;
    bytecode_out = it->second->second.bytecode;
    touch(it->second);
    ++stats_.hits;
    if (metrics.translation_cache_hits_total) {
        metrics.translation_cache_hits_total->inc();
    }
    return true;
}

void TranslationCache::put(const std::string& dialect,
                           const std::string& sql,
                           uint64_t schema_version,
                           const std::string& privilege_signature,
                           std::vector<uint8_t> bytecode)
{
    if (!enabled_) {
        return;
    }

    CacheKey key{dialect, sql, schema_version, privilege_signature};
    CacheEntry entry;
    entry.bytecode = std::move(bytecode);
    entry.created_at = std::chrono::steady_clock::now();
    entry.last_access = entry.created_at;
    entry.size_bytes = estimateSize(key, entry);

    if (config_.max_bytes > 0 && entry.size_bytes > config_.max_bytes) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);
    auto existing = cache_.find(key);
    if (existing != cache_.end()) {
        current_bytes_ -= existing->second->second.size_bytes;
        lru_.erase(existing->second);
        cache_.erase(existing);
    }

    while (config_.max_entries > 0 && cache_.size() >= config_.max_entries) {
        evictOne();
    }
    while (config_.max_bytes > 0 && current_bytes_ + entry.size_bytes > config_.max_bytes) {
        evictOne();
        if (lru_.empty()) {
            break;
        }
    }

    lru_.push_front({key, std::move(entry)});
    cache_[lru_.front().first] = lru_.begin();
    current_bytes_ += lru_.front().second.size_bytes;
}

void TranslationCache::invalidateAll()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    cache_.clear();
    lru_.clear();
    current_bytes_ = 0;
}

TranslationCacheStats TranslationCache::stats() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    TranslationCacheStats stats = stats_;
    stats.current_entries = cache_.size();
    stats.current_bytes = current_bytes_;
    return stats;
}

void TranslationCache::resetStats()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);
    stats_ = TranslationCacheStats{};
}

TranslationCache& TranslationCacheManager::getInstance()
{
    static TranslationCache instance;
    return instance;
}

} // namespace scratchbird::protocol
