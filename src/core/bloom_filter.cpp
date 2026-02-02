/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/bloom_filter.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/hash_functions.h"
#include <cmath>
#include <cstring>
#include <random>
#include <chrono>

namespace scratchbird::core
{

namespace
{
    uint8_t clampBitsPerKey(double value)
    {
        if (value < 1.0)
        {
            return 1;
        }
        if (value > 255.0)
        {
            return 255;
        }
        return static_cast<uint8_t>(std::ceil(value));
    }

    uint8_t clampNumHashes(double value)
    {
        if (value < 1.0)
        {
            return 1;
        }
        if (value > 255.0)
        {
            return 255;
        }
        return static_cast<uint8_t>(std::round(value));
    }

    double computeActualFpr(uint64_t num_keys, uint64_t num_bits, uint32_t num_hashes)
    {
        if (num_keys == 0 || num_bits == 0 || num_hashes == 0)
        {
            return 0.0;
        }
        double m = static_cast<double>(num_bits);
        double n = static_cast<double>(num_keys);
        double k = static_cast<double>(num_hashes);
        double exp_term = std::exp(-(k * n) / m);
        return std::pow(1.0 - exp_term, k);
    }
} // namespace

#pragma pack(push, 1)
struct SBBloomFilterMetaPage
{
    PageHeader bf_header;
    uint8_t bf_uuid[16];
    uint64_t bf_num_keys;
    uint32_t bf_num_bits;
    uint16_t bf_num_hashes;
    uint16_t bf_bits_per_key;

    uint64_t bf_first_data_page;
    uint32_t bf_num_data_pages;
    uint32_t bf_hash_seed;

    uint64_t bf_false_positives;
    uint64_t bf_true_negatives;
    uint64_t bf_total_queries;
    uint64_t bf_last_rebuild_time;

    uint8_t bf_reserved[48];
};

struct SBBloomFilterDataPage
{
    PageHeader bf_header;
    uint64_t bf_next_page;
    uint8_t bf_bits[1];
};
#pragma pack(pop)

BloomFilter::BloomFilter(Database *db, GPID meta_gpid)
    : db_(db),
      meta_gpid_(meta_gpid),
      tablespace_id_(getTablespaceID(meta_gpid))
{
}

BloomFilter::~BloomFilter()
{
    flushCache(nullptr);
}

Status BloomFilter::create(Database *db,
                           const UuidV7Bytes &index_uuid,
                           const BloomFilterConfig &config,
                           uint64_t estimated_keys,
                           uint16_t tablespace_id,
                           GPID *meta_gpid_out,
                           ErrorContext *ctx)
{
    if (!db || !meta_gpid_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid BloomFilter::create arguments");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t page_size = db->page_size();
    if (!isValidAlphaPageSize(page_size))
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid page size for Bloom filter");
        return Status::INVALID_ARGUMENT;
    }

    BloomFilterConfig resolved = config;
    if (resolved.bits_per_key == 0)
    {
        double bits_per_key = -std::log(resolved.target_fpr) / (std::log(2.0) * std::log(2.0));
        resolved.bits_per_key = clampBitsPerKey(bits_per_key);
    }
    if (resolved.num_hashes == 0)
    {
        double num_hashes = static_cast<double>(resolved.bits_per_key) * std::log(2.0);
        resolved.num_hashes = clampNumHashes(num_hashes);
    }

    if (estimated_keys == 0)
    {
        estimated_keys = 1;
    }

    uint32_t bytes_per_page = page_size - static_cast<uint32_t>(sizeof(PageHeader) + sizeof(uint64_t));
    uint32_t bits_per_page = bytes_per_page * 8U;
    uint64_t total_bits = static_cast<uint64_t>(resolved.bits_per_key) * estimated_keys;
    uint32_t num_pages = static_cast<uint32_t>((total_bits + bits_per_page - 1) / bits_per_page);
    if (num_pages == 0)
    {
        num_pages = 1;
    }

    PageManager *pm = db->page_manager();
    if (!pm)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::create missing page manager");
        return Status::INVALID_ARGUMENT;
    }

    GPID meta_gpid = 0;
    Status status = pm->allocatePageInTablespace(tablespace_id, &meta_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<GPID> data_pages;
    data_pages.reserve(num_pages);
    for (uint32_t i = 0; i < num_pages; ++i)
    {
        GPID data_gpid = 0;
        status = pm->allocatePageInTablespace(tablespace_id, &data_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        data_pages.push_back(data_gpid);
    }

    BufferPool *bp = db->buffer_pool();
    if (!bp)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::create missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t *meta_data = nullptr;
    status = bp->pinPageGlobal(meta_gpid, reinterpret_cast<void **>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    auto *meta = reinterpret_cast<SBBloomFilterMetaPage *>(meta_data);
    std::memset(meta, 0, sizeof(SBBloomFilterMetaPage));
    meta->bf_header.magic = K_MAGIC_SBRD;
    meta->bf_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
    meta->bf_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BLOOM_FILTER_META);
    meta->bf_header.page_size = page_size;
    meta->bf_header.page_id = static_cast<uint32_t>(getPageNumber(meta_gpid));

    std::memcpy(meta->bf_uuid, index_uuid.bytes.data(), sizeof(meta->bf_uuid));
    meta->bf_num_keys = 0;
    meta->bf_num_bits = static_cast<uint32_t>(num_pages * bits_per_page);
    meta->bf_num_hashes = resolved.num_hashes;
    meta->bf_bits_per_key = resolved.bits_per_key;

    meta->bf_first_data_page = data_pages.front();
    meta->bf_num_data_pages = num_pages;

    std::random_device rd;
    meta->bf_hash_seed = rd();
    meta->bf_false_positives = 0;
    meta->bf_true_negatives = 0;
    meta->bf_total_queries = 0;
    meta->bf_last_rebuild_time = 0;

    bp->unpinPageGlobal(meta_gpid, true, ctx);

    for (uint32_t i = 0; i < data_pages.size(); ++i)
    {
        uint8_t *page_data = nullptr;
        status = bp->pinPageGlobal(data_pages[i], reinterpret_cast<void **>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        auto *data_page = reinterpret_cast<SBBloomFilterDataPage *>(page_data);
        std::memset(page_data, 0, page_size);
        data_page->bf_header.magic = K_MAGIC_SBRD;
        data_page->bf_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
        data_page->bf_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BLOOM_FILTER_DATA);
        data_page->bf_header.page_size = page_size;
        data_page->bf_header.page_id = static_cast<uint32_t>(getPageNumber(data_pages[i]));
        data_page->bf_next_page = (i + 1 < data_pages.size()) ? data_pages[i + 1] : 0;
        bp->unpinPageGlobal(data_pages[i], true, ctx);
    }

    *meta_gpid_out = meta_gpid;
    return Status::OK;
}

std::unique_ptr<BloomFilter> BloomFilter::open(Database *db, GPID meta_gpid, ErrorContext *ctx)
{
    if (!db || meta_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid BloomFilter::open arguments");
        return nullptr;
    }

    BufferPool *bp = db->buffer_pool();
    if (!bp)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::open missing buffer pool");
        return nullptr;
    }

    uint8_t *meta_data = nullptr;
    Status status = bp->pinPageGlobal(meta_gpid, reinterpret_cast<void **>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return nullptr;
    }

    auto *meta = reinterpret_cast<SBBloomFilterMetaPage *>(meta_data);
    if (meta->bf_header.magic != K_MAGIC_SBRD)
    {
        bp->unpinPageGlobal(meta_gpid, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Bloom filter meta page invalid magic");
        return nullptr;
    }

    auto filter = std::make_unique<BloomFilter>(db, meta_gpid);
    filter->config_.bits_per_key = static_cast<uint8_t>(meta->bf_bits_per_key);
    filter->config_.num_hashes = static_cast<uint8_t>(meta->bf_num_hashes);
    filter->num_keys_ = meta->bf_num_keys;
    filter->num_bits_ = meta->bf_num_bits;
    filter->num_pages_ = meta->bf_num_data_pages;
    filter->hash_seed_ = meta->bf_hash_seed;
    filter->false_positives_ = meta->bf_false_positives;
    filter->true_negatives_ = meta->bf_true_negatives;
    filter->total_queries_ = meta->bf_total_queries;
    filter->last_rebuild_time_ = meta->bf_last_rebuild_time;

    GPID current_page = meta->bf_first_data_page;
    for (uint32_t i = 0; i < filter->num_pages_ && current_page != 0; ++i)
    {
        filter->data_pages_.push_back(current_page);
        uint8_t *page_data = nullptr;
        Status page_status = bp->pinPageGlobal(current_page, reinterpret_cast<void **>(&page_data), ctx);
        if (page_status != Status::OK)
        {
            bp->unpinPageGlobal(meta_gpid, false, ctx);
            return nullptr;
        }
        auto *data_page = reinterpret_cast<SBBloomFilterDataPage *>(page_data);
        current_page = data_page->bf_next_page;
        bp->unpinPageGlobal(filter->data_pages_.back(), false, ctx);
    }

    bp->unpinPageGlobal(meta_gpid, false, ctx);
    return filter;
}

Status BloomFilter::insert(const void *key_data, size_t key_len, ErrorContext *ctx)
{
    if (!key_data || key_len == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::insert invalid key");
        return Status::INVALID_ARGUMENT;
    }

    for (uint32_t i = 0; i < config_.num_hashes; ++i)
    {
        uint64_t bit_index = hashKey(key_data, key_len, i) % num_bits_;
        Status status = setBit(bit_index, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    ++num_keys_;
    return writeMeta(ctx);
}

bool BloomFilter::test(const void *key_data, size_t key_len, ErrorContext *ctx)
{
    if (!key_data || key_len == 0)
    {
        return false;
    }

    ++total_queries_;
    for (uint32_t i = 0; i < config_.num_hashes; ++i)
    {
        uint64_t bit_index = hashKey(key_data, key_len, i) % num_bits_;
        if (!getBit(bit_index, ctx))
        {
            ++true_negatives_;
            writeMeta(ctx);
            return false;
        }
    }

    ++false_positives_;
    writeMeta(ctx);
    return true;
}

Status BloomFilter::clear(ErrorContext *ctx)
{
    BufferPool *bp = db_->buffer_pool();
    if (!bp)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::clear missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    cache_dirty_ = false;
    cached_page_index_ = UINT32_MAX;
    bit_cache_.clear();

    uint32_t page_size = getPageSize();
    for (GPID page : data_pages_)
    {
        uint8_t *page_data = nullptr;
        Status status = bp->pinPageGlobal(page, reinterpret_cast<void **>(&page_data), ctx);
        if (status != Status::OK)
        {
            return status;
        }
        std::memset(page_data + sizeof(PageHeader) + sizeof(uint64_t), 0, page_size - sizeof(PageHeader) - sizeof(uint64_t));
        bp->unpinPageGlobal(page, true, ctx);
    }

    num_keys_ = 0;
    total_queries_ = 0;
    true_negatives_ = 0;
    false_positives_ = 0;
    last_rebuild_time_ =
        static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    return writeMeta(ctx);
}

Status BloomFilter::rebuild(ErrorContext *ctx)
{
    return clear(ctx);
}

Status BloomFilter::drop(ErrorContext *ctx)
{
    Status status = flushCache(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    PageManager *pm = db_->page_manager();
    if (!pm)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::drop missing page manager");
        return Status::INVALID_ARGUMENT;
    }

    for (GPID page : data_pages_)
    {
        status = pm->freePageGlobal(page, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    status = pm->freePageGlobal(meta_gpid_, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    data_pages_.clear();
    meta_gpid_ = 0;
    return Status::OK;
}

BloomFilterStatistics BloomFilter::getStatistics() const
{
    BloomFilterStatistics stats;
    stats.num_keys = num_keys_;
    stats.num_bits = num_bits_;
    stats.num_pages = num_pages_;
    stats.total_queries = total_queries_;
    stats.true_negatives = true_negatives_;
    stats.false_positives = false_positives_;
    stats.page_size = getPageSize();
    stats.bits_per_page = getBitsPerPage();
    stats.actual_fpr = computeActualFpr(num_keys_, num_bits_, config_.num_hashes);
    if (num_keys_ > 0)
    {
        stats.space_efficiency = static_cast<double>(num_pages_ * stats.page_size) /
                                 static_cast<double>(num_keys_);
    }
    return stats;
}

uint32_t BloomFilter::getPageSize() const
{
    return db_->page_size();
}

uint32_t BloomFilter::getBitsPerPage() const
{
    return getBytesPerPage() * 8U;
}

uint32_t BloomFilter::getBytesPerPage() const
{
    return getPageSize() - static_cast<uint32_t>(sizeof(PageHeader) + sizeof(uint64_t));
}

uint32_t BloomFilter::calculatePagesNeeded(uint64_t num_keys) const
{
    uint64_t total_bits = num_keys * config_.bits_per_key;
    uint32_t bits_per_page = getBitsPerPage();
    return static_cast<uint32_t>((total_bits + bits_per_page - 1) / bits_per_page);
}

bool BloomFilter::loadCachePage(uint32_t page_index, ErrorContext *ctx)
{
    if (page_index >= data_pages_.size())
    {
        return false;
    }

    if (cached_page_index_ == page_index)
    {
        return true;
    }

    if (flushCache(ctx) != Status::OK)
    {
        return false;
    }

    BufferPool *bp = db_->buffer_pool();
    if (!bp)
    {
        return false;
    }

    uint32_t bytes_per_page = getBytesPerPage();
    bit_cache_.resize(bytes_per_page);

    uint8_t *page_data = nullptr;
    Status status = bp->pinPageGlobal(data_pages_[page_index], reinterpret_cast<void **>(&page_data), ctx);
    if (status != Status::OK)
    {
        return false;
    }

    std::memcpy(bit_cache_.data(),
                page_data + sizeof(PageHeader) + sizeof(uint64_t),
                bytes_per_page);

    bp->unpinPageGlobal(data_pages_[page_index], false, ctx);
    cached_page_index_ = page_index;
    cache_dirty_ = false;
    return true;
}

Status BloomFilter::flushCache(ErrorContext *ctx)
{
    if (!cache_dirty_ || cached_page_index_ == UINT32_MAX)
    {
        return Status::OK;
    }

    BufferPool *bp = db_->buffer_pool();
    if (!bp)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::flushCache missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t *page_data = nullptr;
    Status status = bp->pinPageGlobal(data_pages_[cached_page_index_],
                                      reinterpret_cast<void **>(&page_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memcpy(page_data + sizeof(PageHeader) + sizeof(uint64_t),
                bit_cache_.data(),
                bit_cache_.size());
    bp->unpinPageGlobal(data_pages_[cached_page_index_], true, ctx);
    cache_dirty_ = false;
    return Status::OK;
}

Status BloomFilter::writeMeta(ErrorContext *ctx)
{
    BufferPool *bp = db_->buffer_pool();
    if (!bp)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "BloomFilter::writeMeta missing buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint8_t *meta_data = nullptr;
    Status status = bp->pinPageGlobal(meta_gpid_, reinterpret_cast<void **>(&meta_data), ctx);
    if (status != Status::OK)
    {
        return status;
    }

    auto *meta = reinterpret_cast<SBBloomFilterMetaPage *>(meta_data);
    meta->bf_num_keys = num_keys_;
    meta->bf_num_bits = static_cast<uint32_t>(num_bits_);
    meta->bf_num_hashes = config_.num_hashes;
    meta->bf_bits_per_key = config_.bits_per_key;
    meta->bf_num_data_pages = num_pages_;
    meta->bf_hash_seed = hash_seed_;
    meta->bf_false_positives = false_positives_;
    meta->bf_true_negatives = true_negatives_;
    meta->bf_total_queries = total_queries_;
    meta->bf_last_rebuild_time = last_rebuild_time_;

    bp->unpinPageGlobal(meta_gpid_, true, ctx);
    return Status::OK;
}

uint64_t BloomFilter::hashKey(uint64_t hash, uint32_t i) const
{
    uint64_t h2 = MurmurHash64(&hash, sizeof(hash), hash_seed_ ^ 0x9e3779b97f4a7c15ULL);
    return hash + static_cast<uint64_t>(i) * h2;
}

uint64_t BloomFilter::hashKey(const void *key_data, size_t key_len, uint32_t i) const
{
    uint64_t h1 = MurmurHash64(key_data, key_len, hash_seed_);
    return hashKey(h1, i);
}

Status BloomFilter::setBit(uint64_t bit_index, ErrorContext *ctx)
{
    uint32_t bits_per_page = getBitsPerPage();
    uint32_t page_index = static_cast<uint32_t>(bit_index / bits_per_page);
    uint32_t bit_in_page = static_cast<uint32_t>(bit_index % bits_per_page);
    uint32_t byte_offset = bit_in_page / 8U;
    uint8_t bit_mask = static_cast<uint8_t>(1U << (bit_in_page % 8U));

    if (!loadCachePage(page_index, ctx))
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "BloomFilter::setBit cache load failed");
        return Status::IO_ERROR;
    }

    bit_cache_[byte_offset] |= bit_mask;
    cache_dirty_ = true;
    return Status::OK;
}

bool BloomFilter::getBit(uint64_t bit_index, ErrorContext *ctx)
{
    uint32_t bits_per_page = getBitsPerPage();
    uint32_t page_index = static_cast<uint32_t>(bit_index / bits_per_page);
    uint32_t bit_in_page = static_cast<uint32_t>(bit_index % bits_per_page);
    uint32_t byte_offset = bit_in_page / 8U;
    uint8_t bit_mask = static_cast<uint8_t>(1U << (bit_in_page % 8U));

    if (!loadCachePage(page_index, ctx))
    {
        return false;
    }

    return (bit_cache_[byte_offset] & bit_mask) != 0;
}

} // namespace scratchbird::core
