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

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird::core
{

class Database;

struct BloomFilterConfig
{
    bool enabled = false;
    double target_fpr = 0.01;
    uint8_t bits_per_key = 10;
    uint8_t num_hashes = 7;
    uint32_t rebuild_threshold = 100000;
};

struct BloomFilterStatistics
{
    uint64_t num_keys = 0;
    uint64_t num_bits = 0;
    uint64_t num_pages = 0;
    uint64_t total_queries = 0;
    uint64_t true_negatives = 0;
    uint64_t false_positives = 0;
    double actual_fpr = 0.0;
    double space_efficiency = 0.0;
    uint32_t page_size = 0;
    uint32_t bits_per_page = 0;
};

class BloomFilter
{
public:
    BloomFilter(Database *db, GPID meta_gpid);
    ~BloomFilter();

    static Status create(Database *db,
                         const UuidV7Bytes &index_uuid,
                         const BloomFilterConfig &config,
                         uint64_t estimated_keys,
                         uint16_t tablespace_id,
                         GPID *meta_gpid_out,
                         ErrorContext *ctx = nullptr);

    static std::unique_ptr<BloomFilter> open(Database *db,
                                             GPID meta_gpid,
                                             ErrorContext *ctx = nullptr);

    Status insert(const void *key_data, size_t key_len, ErrorContext *ctx = nullptr);
    bool test(const void *key_data, size_t key_len, ErrorContext *ctx = nullptr);

    Status clear(ErrorContext *ctx = nullptr);
    Status rebuild(ErrorContext *ctx = nullptr);
    Status drop(ErrorContext *ctx = nullptr);

    BloomFilterStatistics getStatistics() const;
    const BloomFilterConfig &getConfig() const { return config_; }
    void setTargetFpr(double target_fpr) { config_.target_fpr = target_fpr; }
    GPID getMetaPage() const { return meta_gpid_; }

private:
    Database *db_ = nullptr;
    GPID meta_gpid_ = 0;
    uint16_t tablespace_id_ = 0;
    BloomFilterConfig config_{};

    uint64_t num_keys_ = 0;
    uint64_t num_bits_ = 0;
    uint32_t num_pages_ = 0;
    uint32_t hash_seed_ = 0;
    uint64_t total_queries_ = 0;
    uint64_t true_negatives_ = 0;
    uint64_t false_positives_ = 0;
    uint64_t last_rebuild_time_ = 0;

    std::vector<GPID> data_pages_;
    std::vector<uint8_t> bit_cache_;
    uint32_t cached_page_index_ = UINT32_MAX;
    bool cache_dirty_ = false;

    uint32_t getPageSize() const;
    uint32_t getBitsPerPage() const;
    uint32_t getBytesPerPage() const;
    uint32_t calculatePagesNeeded(uint64_t num_keys) const;

    bool loadCachePage(uint32_t page_index, ErrorContext *ctx);
    Status flushCache(ErrorContext *ctx);
    Status writeMeta(ErrorContext *ctx);

    uint64_t hashKey(uint64_t hash, uint32_t i) const;
    uint64_t hashKey(const void *key_data, size_t key_len, uint32_t i) const;

    Status setBit(uint64_t bit_index, ErrorContext *ctx);
    bool getBit(uint64_t bit_index, ErrorContext *ctx);
};

} // namespace scratchbird::core
