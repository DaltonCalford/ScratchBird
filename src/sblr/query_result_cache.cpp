// =================================================================================================
// ScratchBird Database Engine
// Copyright (C) 2025 ScratchBird Development Team
// =================================================================================================
//
// Query Result Cache Implementation
//
// P2-19: Query Result Caching
// LRU cache for SELECT query results with automatic table-based invalidation.
//
// November 25, 2025

#include "scratchbird/sblr/query_result_cache.h"
#include <algorithm>
#include <cstring>

// Simple SHA-256 implementation (minimal, for cache key generation only)
// In production, consider using OpenSSL or another crypto library
namespace {

// SHA-256 constants
constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t Sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t Sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t sigma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t sigma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

void sha256(const uint8_t* data, size_t len, uint8_t* hash_out)
{
    uint32_t H[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Padding
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    std::vector<uint8_t> padded(padded_len, 0);
    std::memcpy(padded.data(), data, len);
    padded[len] = 0x80;

    // Length in bits (big-endian)
    uint64_t bit_len = len * 8;
    for (int i = 0; i < 8; ++i) {
        padded[padded_len - 1 - i] = static_cast<uint8_t>(bit_len >> (i * 8));
    }

    // Process blocks
    for (size_t block = 0; block < padded_len / 64; ++block) {
        uint32_t W[64];
        const uint8_t* blk = padded.data() + block * 64;

        // Initialize W[0..15]
        for (int i = 0; i < 16; ++i) {
            W[i] = (static_cast<uint32_t>(blk[i * 4]) << 24) |
                   (static_cast<uint32_t>(blk[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(blk[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(blk[i * 4 + 3]);
        }

        // Extend W[16..63]
        for (int i = 16; i < 64; ++i) {
            W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
        }

        // Initialize working variables
        uint32_t a = H[0], b = H[1], c = H[2], d = H[3];
        uint32_t e = H[4], f = H[5], g = H[6], h = H[7];

        // Main loop
        for (int i = 0; i < 64; ++i) {
            uint32_t T1 = h + Sigma1(e) + Ch(e, f, g) + K[i] + W[i];
            uint32_t T2 = Sigma0(a) + Maj(a, b, c);
            h = g; g = f; f = e;
            e = d + T1;
            d = c; c = b; b = a;
            a = T1 + T2;
        }

        // Update hash
        H[0] += a; H[1] += b; H[2] += c; H[3] += d;
        H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    // Output hash (big-endian)
    for (int i = 0; i < 8; ++i) {
        hash_out[i * 4 + 0] = static_cast<uint8_t>(H[i] >> 24);
        hash_out[i * 4 + 1] = static_cast<uint8_t>(H[i] >> 16);
        hash_out[i * 4 + 2] = static_cast<uint8_t>(H[i] >> 8);
        hash_out[i * 4 + 3] = static_cast<uint8_t>(H[i]);
    }
}

} // anonymous namespace

namespace scratchbird {
namespace sblr {

// =================================================================================================
// QueryResultCache Implementation
// =================================================================================================

QueryResultCache::QueryResultCache(size_t max_entries, size_t max_memory_bytes)
    : max_entries_(max_entries)
    , max_memory_bytes_(max_memory_bytes)
    , current_memory_bytes_(0)
    , enabled_(true)
    , hits_(0)
    , misses_(0)
    , evictions_(0)
    , invalidations_(0)
    , insertions_(0)
{
}

QueryResultCache::~QueryResultCache()
{
    // LRU list cleanup is automatic (smart pointers/RAII)
}

QueryHash QueryResultCache::computeHash(const std::string& sql_text)
{
    QueryHash hash;
    sha256(reinterpret_cast<const uint8_t*>(sql_text.data()), sql_text.size(), hash.data());
    return hash;
}

QueryHash QueryResultCache::computeHash(const std::vector<uint8_t>& bytecode)
{
    QueryHash hash;
    sha256(bytecode.data(), bytecode.size(), hash.data());
    return hash;
}

CachedResultSet* QueryResultCache::get(const QueryHash& hash)
{
    if (!enabled_) {
        ++misses_;
        return nullptr;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = cache_map_.find(hash);
    if (it == cache_map_.end()) {
        ++misses_;
        return nullptr;
    }

    // Update access metadata
    auto& entry = it->second->second;
    entry.last_accessed = std::chrono::steady_clock::now();
    ++entry.access_count;

    // Move to front of LRU list (most recently used)
    moveToFront(it->second);

    ++hits_;
    return &it->second->second;
}

void QueryResultCache::put(const QueryHash& hash, CachedResultSet result)
{
    if (!enabled_) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if already exists
    auto existing = cache_map_.find(hash);
    if (existing != cache_map_.end()) {
        // Update existing entry
        current_memory_bytes_ -= existing->second->second.estimated_size_bytes;
        removeEntry(existing->second);
    }

    // Estimate size of new entry
    result.estimated_size_bytes = estimateResultSize(result);
    size_t entry_size = result.estimated_size_bytes;

    // Evict if we're over entry limit
    while (!lru_list_.empty() && cache_map_.size() >= max_entries_) {
        evictLRU();
    }

    // Evict if we're over memory limit
    while (!lru_list_.empty() && current_memory_bytes_ + entry_size > max_memory_bytes_) {
        evictLRU();
    }

    // If single entry is too large, don't cache it
    if (entry_size > max_memory_bytes_) {
        return;
    }

    // Insert at front of LRU list
    lru_list_.push_front({hash, std::move(result)});
    auto it = lru_list_.begin();
    cache_map_[hash] = it;

    // Update table reverse index
    for (const auto& table_id : it->second.table_ids) {
        table_to_queries_[table_id].insert(hash);
    }

    current_memory_bytes_ += entry_size;
    ++insertions_;
}

void QueryResultCache::invalidateTable(const core::ID& table_id)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = table_to_queries_.find(table_id);
    if (it == table_to_queries_.end()) {
        return;  // No cached queries reference this table
    }

    // Copy the set since we'll be modifying it
    std::unordered_set<QueryHash, QueryHashHash> queries_to_remove = it->second;

    for (const auto& hash : queries_to_remove) {
        auto cache_it = cache_map_.find(hash);
        if (cache_it != cache_map_.end()) {
            removeEntry(cache_it->second);
            ++invalidations_;
        }
    }

    // Remove table from reverse index
    table_to_queries_.erase(it);
}

void QueryResultCache::invalidateAll()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    invalidations_ += cache_map_.size();
    cache_map_.clear();
    lru_list_.clear();
    table_to_queries_.clear();
    current_memory_bytes_ = 0;
}

void QueryResultCache::remove(const QueryHash& hash)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = cache_map_.find(hash);
    if (it != cache_map_.end()) {
        removeEntry(it->second);
    }
}

void QueryResultCache::setMaxEntries(size_t max_entries)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    max_entries_ = max_entries;

    // Evict if over new limit
    while (!lru_list_.empty() && cache_map_.size() > max_entries_) {
        evictLRU();
    }
}

void QueryResultCache::setMaxMemory(size_t max_bytes)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    max_memory_bytes_ = max_bytes;

    // Evict if over new limit
    while (!lru_list_.empty() && current_memory_bytes_ > max_memory_bytes_) {
        evictLRU();
    }
}

QueryResultCache::Statistics QueryResultCache::getStatistics() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    Statistics stats;
    stats.hits = hits_;
    stats.misses = misses_;
    stats.evictions = evictions_;
    stats.invalidations = invalidations_;
    stats.insertions = insertions_;
    stats.current_entries = cache_map_.size();
    stats.current_memory = current_memory_bytes_;
    stats.max_entries = max_entries_;
    stats.max_memory = max_memory_bytes_;
    return stats;
}

void QueryResultCache::resetStatistics()
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    hits_ = 0;
    misses_ = 0;
    evictions_ = 0;
    invalidations_ = 0;
    insertions_ = 0;
}

void QueryResultCache::evictLRU()
{
    // Must be called with lock held
    if (lru_list_.empty()) {
        return;
    }

    // Remove from back (least recently used)
    auto it = std::prev(lru_list_.end());
    removeEntry(it);
    ++evictions_;
}

void QueryResultCache::moveToFront(LRUIterator it)
{
    // Must be called with lock held
    if (it == lru_list_.begin()) {
        return;  // Already at front
    }

    // Splice to front
    lru_list_.splice(lru_list_.begin(), lru_list_, it);
}

void QueryResultCache::removeEntry(LRUIterator it)
{
    // Must be called with lock held
    const auto& hash = it->first;
    const auto& entry = it->second;

    // Update memory counter
    current_memory_bytes_ -= entry.estimated_size_bytes;

    // Remove from table reverse index
    for (const auto& table_id : entry.table_ids) {
        auto table_it = table_to_queries_.find(table_id);
        if (table_it != table_to_queries_.end()) {
            table_it->second.erase(hash);
            if (table_it->second.empty()) {
                table_to_queries_.erase(table_it);
            }
        }
    }

    // Remove from cache map
    cache_map_.erase(hash);

    // Remove from LRU list
    lru_list_.erase(it);
}

size_t QueryResultCache::estimateResultSize(const CachedResultSet& result) const
{
    size_t size = 0;

    // Column names
    for (const auto& name : result.column_names) {
        size += name.size() + sizeof(std::string);
    }

    // Column types
    size += result.column_types.size() * sizeof(core::DataType);

    // Rows
    for (const auto& row : result.rows) {
        for (const auto& value : row) {
            // Base TypedValue size
            size += sizeof(core::TypedValue);

            // Additional size for variable-length types
            if (!value.isNull()) {
                switch (value.type()) {
                    case core::DataType::VARCHAR:
                    case core::DataType::TEXT:
                    case core::DataType::CHAR:
                        size += value.getVarchar().size();
                        break;
                    case core::DataType::BINARY:
                    case core::DataType::UUID:
                        size += 16;  // Typical binary/UUID size
                        break;
                    default:
                        break;  // Fixed-size types already counted
                }
            }
        }
    }

    // Table IDs
    size += result.table_ids.size() * 16;  // UUID = 16 bytes

    // Overhead for std::vectors and struct
    size += sizeof(CachedResultSet);

    return size;
}

// =================================================================================================
// QueryResultCacheManager Implementation (Singleton)
// =================================================================================================

QueryResultCache& QueryResultCacheManager::getInstance()
{
    static QueryResultCache instance;
    return instance;
}

} // namespace sblr
} // namespace scratchbird
