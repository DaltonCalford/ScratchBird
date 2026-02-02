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
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/tid.h"  // Sprint 5: Needed for TID definition
#include "scratchbird/core/catalog_manager.h"  // Needed for CatalogManager::TableInfo
#include <unordered_map>
#include <memory>
#include <mutex>
#include <cstring>
#include <functional>

namespace scratchbird::core
{

using ID = UuidV7Bytes;

/**
 * TIDBloomFilter - Fast probabilistic set membership test
 *
 * Used to quickly determine if a TID has been migrated.
 * False positive rate: ~1% (configurable)
 * False negatives: Never
 *
 * Sprint 4 Task 5.4.2: Dual-Source Visibility
 */
class TIDBloomFilter
{
public:
    /**
     * Construct bloom filter
     *
     * @param expected_items Expected number of items to insert
     * @param false_positive_rate Target false positive rate (default 0.01 = 1%)
     */
    TIDBloomFilter(uint64_t expected_items, float false_positive_rate = 0.01f);

    /**
     * Destructor
     */
    ~TIDBloomFilter();

    /**
     * Insert an item into the bloom filter
     *
     * @param item Item to insert (TID as uint64_t)
     */
    void insert(uint64_t item);

    /**
     * Check if an item might be in the set
     *
     * @param item Item to check
     * @return true if item might be present (or false positive),
     *         false if item is definitely not present
     */
    bool contains(uint64_t item) const;

    /**
     * Clear all bits in the bloom filter
     */
    void clear();

    /**
     * Get size of bloom filter in bits
     */
    uint64_t size() const { return size_bits_; }

    /**
     * Get number of hash functions
     */
    uint32_t numHashes() const { return num_hashes_; }

private:
    /**
     * Hash functions for bloom filter
     */
    uint64_t hash1(uint64_t item) const;
    uint64_t hash2(uint64_t item) const;
    uint64_t hash3(uint64_t item) const;

    uint8_t *bits_;          // Bit array
    uint64_t size_bits_;     // Size in bits
    uint32_t num_hashes_;    // Number of hash functions to use
};

/**
 * QueryTIDCache - Per-query cache for TID resolution
 *
 * Caches TID resolution results for the duration of a query to avoid
 * repeated lookups for the same TID.
 *
 * Sprint 4 Task 5.4.2: Dual-Source Visibility
 */
class QueryTIDCache
{
public:
    QueryTIDCache() = default;

    /**
     * Lookup cached tablespace for a TID
     *
     * @param tid TID to lookup
     * @return Tablespace ID if cached, nullopt otherwise
     */
    std::optional<uint16_t> lookup(const TID &tid) const;

    /**
     * Cache a TID → tablespace mapping
     *
     * @param tid TID
     * @param tablespace_id Tablespace ID
     */
    void cache(const TID &tid, uint16_t tablespace_id);

    /**
     * Clear the cache
     */
    void clear();

    /**
     * Get cache hit count (for testing)
     */
    uint64_t hitCount() const { return hit_count_; }

    /**
     * Get cache miss count (for testing)
     */
    uint64_t missCount() const { return miss_count_; }

private:
    // Cache: TID (as uint64_t) → tablespace_id
    std::unordered_map<uint64_t, uint16_t> cache_;
    mutable uint64_t hit_count_ = 0;
    mutable uint64_t miss_count_ = 0;

    // Convert TID to uint64_t for hashing
    static uint64_t tidToU64(const TID &tid)
    {
        uint64_t page_num = getPageNumber(tid.gpid);
        return (page_num << 32) | tid.slot;
    }
};

/**
 * TIDResolver - Resolves which tablespace a TID belongs to during migration
 *
 * During ONLINE migration, tuples exist in two tablespaces:
 * - Source tablespace (old tuples)
 * - Target tablespace (migrated tuples and new INSERTs)
 *
 * The TID Resolver determines which tablespace to read from for a given TID.
 *
 * Sprint 4 Task 5.4.2: Dual-Source Visibility
 */
class TIDResolver
{
public:
    TIDResolver();
    ~TIDResolver();

    /**
     * Record that a TID has been migrated
     *
     * @param table_id Table being migrated
     * @param source_tid TID in source tablespace
     * @param target_tid TID in target tablespace
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status recordMigration(
        const ID &table_id,
        const TID &source_tid,
        const TID &target_tid,
        ErrorContext *ctx = nullptr);

    /**
     * Resolve which tablespace a TID belongs to
     *
     * @param tid TID to resolve
     * @param table_info Table information (contains migration state)
     * @param cache Optional query-level cache
     * @param ctx Error context
     * @return Tablespace ID (source or target)
     */
    uint16_t resolveTablespace(
        const TID &tid,
        const CatalogManager::TableInfo &table_info,
        QueryTIDCache *cache = nullptr,
        ErrorContext *ctx = nullptr);

    /**
     * Get all TID mappings for a table (for index updates during swap)
     *
     * @param table_id Table ID
     * @param ctx Error context
     * @return Map of source TID → target TID
     */
    std::unordered_map<TID, TID> getAllMappings(
        const ID &table_id,
        ErrorContext *ctx = nullptr);

    /**
     * Clear migration data for a table (after migration completes)
     *
     * @param table_id Table ID
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status clearMigration(
        const ID &table_id,
        ErrorContext *ctx = nullptr);

    /**
     * Get statistics
     */
    uint64_t bloomFalsePositives() const { return bloom_false_positives_; }
    uint64_t bloomTrueNegatives() const { return bloom_true_negatives_; }
    uint64_t bloomTruePositives() const { return bloom_true_positives_; }

private:
    struct TableMigrationData
    {
        ID table_id;
    std::unique_ptr<TIDBloomFilter> bloom;  // Fast "has TID been migrated?" check
        std::unordered_map<TID, TID> tid_mapping;  // Exact source TID → target TID
    };

    // Per-table migration data
    std::unordered_map<ID, TableMigrationData> table_data_;
    mutable std::mutex mutex_;

    // Statistics
    mutable uint64_t bloom_false_positives_ = 0;
    mutable uint64_t bloom_true_negatives_ = 0;
    mutable uint64_t bloom_true_positives_ = 0;

    // Hash TID to uint64 for bloom filter checks
    static uint64_t hashTID(const TID &tid)
    {
        return static_cast<uint64_t>(std::hash<TID>{}(tid));
    }
};

} // namespace scratchbird::core
