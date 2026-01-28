#include "scratchbird/core/tid_resolver.h"
#include "scratchbird/core/catalog_manager.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace scratchbird::core
{

// ============================================================================
// BloomFilter Implementation
// ============================================================================

TIDBloomFilter::TIDBloomFilter(uint64_t expected_items, float false_positive_rate)
{
    // Calculate optimal bloom filter size
    // m = -(n * ln(p)) / (ln(2)^2)
    // where n = expected_items, p = false_positive_rate, m = size in bits
    double ln2_squared = 0.4804530139182014; // (ln 2)^2
    size_bits_ = static_cast<uint64_t>(
        -(static_cast<double>(expected_items) * std::log(false_positive_rate)) / ln2_squared);

    // Round up to nearest byte
    size_bits_ = ((size_bits_ + 7) / 8) * 8;

    // Calculate optimal number of hash functions
    // k = (m/n) * ln(2)
    num_hashes_ = static_cast<uint32_t>(
        (static_cast<double>(size_bits_) / static_cast<double>(expected_items)) * 0.693147180559945);

    // Clamp to reasonable range [1, 10]
    num_hashes_ = std::max(1u, std::min(10u, num_hashes_));

    // Allocate bit array
    uint64_t size_bytes = size_bits_ / 8;
    bits_ = new uint8_t[size_bytes];
    std::memset(bits_, 0, size_bytes);
}

TIDBloomFilter::~TIDBloomFilter()
{
    delete[] bits_;
}

void TIDBloomFilter::insert(uint64_t item)
{
    for (uint32_t i = 0; i < num_hashes_; ++i) {
        uint64_t hash;
        if (i == 0) {
            hash = hash1(item);
        } else if (i == 1) {
            hash = hash2(item);
        } else {
            hash = hash3(item + i);
        }

        uint64_t bit_index = hash % size_bits_;
        uint64_t byte_index = bit_index / 8;
        uint64_t bit_offset = bit_index % 8;

        bits_[byte_index] |= (1u << bit_offset);
    }
}

bool TIDBloomFilter::contains(uint64_t item) const
{
    for (uint32_t i = 0; i < num_hashes_; ++i) {
        uint64_t hash;
        if (i == 0) {
            hash = hash1(item);
        } else if (i == 1) {
            hash = hash2(item);
        } else {
            hash = hash3(item + i);
        }

        uint64_t bit_index = hash % size_bits_;
        uint64_t byte_index = bit_index / 8;
        uint64_t bit_offset = bit_index % 8;

        if ((bits_[byte_index] & (1u << bit_offset)) == 0) {
            return false; // Definitely not present
        }
    }

    return true; // Might be present (or false positive)
}

void TIDBloomFilter::clear()
{
    uint64_t size_bytes = size_bits_ / 8;
    std::memset(bits_, 0, size_bytes);
}

// MurmurHash3-style hash functions
uint64_t TIDBloomFilter::hash1(uint64_t item) const
{
    // MurmurHash3 64-bit finalizer
    item ^= item >> 33;
    item *= 0xff51afd7ed558ccdULL;
    item ^= item >> 33;
    item *= 0xc4ceb9fe1a85ec53ULL;
    item ^= item >> 33;
    return item;
}

uint64_t TIDBloomFilter::hash2(uint64_t item) const
{
    // Different seed/constants
    item ^= item >> 32;
    item *= 0xd6e8feb86659fd93ULL;
    item ^= item >> 32;
    item *= 0xc4ceb9fe1a85ec53ULL;
    item ^= item >> 32;
    return item;
}

uint64_t TIDBloomFilter::hash3(uint64_t item) const
{
    // Third variation
    item ^= item >> 31;
    item *= 0x7fb5d329728ea185ULL;
    item ^= item >> 27;
    item *= 0x81dadef4bc2dd44dULL;
    item ^= item >> 33;
    return item;
}

// ============================================================================
// QueryTIDCache Implementation
// ============================================================================

std::optional<uint16_t> QueryTIDCache::lookup(const TID &tid) const
{
    uint64_t key = tidToU64(tid);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        hit_count_++;
        return it->second;
    }
    miss_count_++;
    return std::nullopt;
}

void QueryTIDCache::cache(const TID &tid, uint16_t tablespace_id)
{
    uint64_t key = tidToU64(tid);
    cache_[key] = tablespace_id;
}

void QueryTIDCache::clear()
{
    cache_.clear();
    hit_count_ = 0;
    miss_count_ = 0;
}

// ============================================================================
// TIDResolver Implementation
// ============================================================================

TIDResolver::TIDResolver()
{
}

TIDResolver::~TIDResolver()
{
}

Status TIDResolver::recordMigration(
    const ID &table_id,
    const TID &source_tid,
    const TID &target_tid,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Get or create table migration data
    auto it = table_data_.find(table_id);
    if (it == table_data_.end()) {
        // First migration record for this table - create bloom filter
        // Estimate 1M tuples per table for bloom filter sizing
        constexpr uint64_t ESTIMATED_TUPLES = 1000000;
        constexpr float FALSE_POSITIVE_RATE = 0.01f; // 1%

        TableMigrationData data;
        data.table_id = table_id;
        data.bloom = std::make_unique<TIDBloomFilter>(ESTIMATED_TUPLES, FALSE_POSITIVE_RATE);

        table_data_[table_id] = std::move(data);
        it = table_data_.find(table_id);
    }

    TableMigrationData &data = it->second;

    uint64_t source_hash = hashTID(source_tid);

    // Insert into bloom filter
    data.bloom->insert(source_hash);

    // Store exact mapping
    data.tid_mapping[source_tid] = target_tid;

    return Status::OK;
}

uint16_t TIDResolver::resolveTablespace(
    const TID &tid,
    const CatalogManager::TableInfo &table_info,
    QueryTIDCache *cache,
    ErrorContext *ctx)
{
    // Fast path: no migration in progress
    if (!table_info.migration_in_progress) {
        return table_info.tablespace_id;
    }

    // Check query cache first
    if (cache) {
        auto cached = cache->lookup(tid);
        if (cached.has_value()) {
            return *cached;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Get migration data for this table
    auto it = table_data_.find(table_info.table_id);
    if (it == table_data_.end()) {
        // No migration data yet - tuple must be in source
        uint16_t result = table_info.tablespace_id;
        if (cache) {
            cache->cache(tid, result);
        }
        return result;
    }

    TableMigrationData &data = it->second;

    uint64_t tid_hash = hashTID(tid);

    // Step 1: Bloom filter check (very fast, ~1-2 ns)
    if (!data.bloom->contains(tid_hash)) {
        // Definitely NOT migrated → source tablespace
        bloom_true_negatives_++;
        uint16_t result = table_info.tablespace_id; // Source
        if (cache) {
            cache->cache(tid, result);
        }
        return result;
    }

    // Step 2: Bloom says "probably migrated" → check exact mapping
    auto mapping_it = data.tid_mapping.find(tid);
    if (mapping_it != data.tid_mapping.end()) {
        // Confirmed migrated → target tablespace
        bloom_true_positives_++;
        uint16_t result = table_info.migration_target_ts; // Target
        if (cache) {
            cache->cache(tid, result);
        }
        return result;
    }

    // Step 3: Bloom false positive → actually not migrated
    bloom_false_positives_++;
    uint16_t result = table_info.tablespace_id; // Source
    if (cache) {
        cache->cache(tid, result);
    }
    return result;
}

std::unordered_map<TID, TID> TIDResolver::getAllMappings(
    const ID &table_id,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = table_data_.find(table_id);
    if (it == table_data_.end()) {
        return {}; // No mappings
    }

    return it->second.tid_mapping;
}

Status TIDResolver::clearMigration(
    const ID &table_id,
    ErrorContext *ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = table_data_.find(table_id);
    if (it == table_data_.end()) {
        return Status::OK; // Already cleared
    }

    table_data_.erase(it);
    return Status::OK;
}

} // namespace scratchbird::core
