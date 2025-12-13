/**
 * @file result_cache.cpp
 * @brief Result Set Cache Implementation
 *
 * Implements query result caching with:
 * - Automatic invalidation on data changes
 * - Multiple eviction policies (LRU, LFU, SIZE, TTL)
 * - Memory-bounded caching
 * - Partial result support for large result sets
 *
 * Part of Phase 3.6: Connection Pooling
 */

#include "scratchbird/pool/result_cache.h"
#include <algorithm>
#include <functional>
#include <iomanip>
#include <regex>
#include <sstream>

namespace scratchbird {
namespace pool {

// =============================================================================
// CachedResult Implementation
// =============================================================================

CachedResult::CachedResult(const std::string& sql, const ResultMetadata& metadata)
    : metadata_(metadata)
    , state_(ResultCacheEntryState::VALID) {
    metadata_.sql = sql;
    stats_.created_at = std::chrono::system_clock::now();
    stats_.last_accessed = stats_.created_at;
    stats_.expires_at = stats_.created_at + ttl_;
}

bool CachedResult::is_expired() const {
    auto now = std::chrono::system_clock::now();
    return now >= stats_.expires_at;
}

void CachedResult::add_row(CachedRow row) {
    rows_.push_back(std::move(row));
    metadata_.row_count = rows_.size();
    update_memory_usage();
}

void CachedResult::set_rows(std::vector<CachedRow> rows) {
    rows_ = std::move(rows);
    metadata_.row_count = rows_.size();
    update_memory_usage();
}

void CachedResult::clear_rows() {
    rows_.clear();
    rows_.shrink_to_fit();
    metadata_.row_count = 0;
    update_memory_usage();
}

void CachedResult::record_hit() {
    stats_.hit_count++;
    stats_.last_accessed = std::chrono::system_clock::now();
}

void CachedResult::record_rows_read(uint64_t count) {
    stats_.read_count += count;
    stats_.time_saved += metadata_.execution_time;
}

void CachedResult::update_memory_usage() {
    uint64_t bytes = sizeof(CachedResult);

    // SQL and fingerprint
    bytes += metadata_.sql.size();
    bytes += metadata_.fingerprint.size();

    // Table references
    for (const auto& table : metadata_.referenced_tables) {
        bytes += table.size();
    }
    for (const auto& schema : metadata_.referenced_schemas) {
        bytes += schema.size();
    }

    // Column info
    for (const auto& col : metadata_.columns) {
        bytes += col.name.size() + sizeof(CachedColumnInfo);
    }

    // Parameters
    for (const auto& param : metadata_.parameters) {
        bytes += result_cache_utils::value_memory(param);
    }

    // Rows
    for (const auto& row : rows_) {
        for (const auto& val : row) {
            bytes += result_cache_utils::value_memory(val);
        }
    }

    // Compressed data
    bytes += compressed_data_.size();

    metadata_.memory_bytes = bytes;
}

void CachedResult::set_ttl(std::chrono::seconds ttl) {
    ttl_ = ttl;
    stats_.expires_at = stats_.created_at + ttl_;
}

std::chrono::seconds CachedResult::remaining_ttl() const {
    auto now = std::chrono::system_clock::now();
    if (now >= stats_.expires_at) {
        return std::chrono::seconds{0};
    }
    return std::chrono::duration_cast<std::chrono::seconds>(stats_.expires_at - now);
}

void CachedResult::refresh_ttl() {
    stats_.expires_at = std::chrono::system_clock::now() + ttl_;
}

bool CachedResult::compress() {
    if (metadata_.is_compressed || rows_.empty()) {
        return false;
    }

    // Simple serialization for compression
    // In production, would use actual compression (LZ4, zstd, etc.)
    std::ostringstream ss;
    ss << rows_.size() << "\n";
    for (const auto& row : rows_) {
        ss << row.size() << "\n";
        for (const auto& val : row) {
            ss << result_cache_utils::format_value(val) << "\n";
        }
    }

    std::string serialized = ss.str();
    metadata_.uncompressed_bytes = serialized.size();

    // Store as "compressed" (not actually compressed in this implementation)
    compressed_data_.assign(serialized.begin(), serialized.end());

    // Clear rows to free memory
    rows_.clear();
    rows_.shrink_to_fit();

    metadata_.is_compressed = true;
    update_memory_usage();

    return true;
}

bool CachedResult::decompress() {
    if (!metadata_.is_compressed || compressed_data_.empty()) {
        return false;
    }

    // Deserialize
    std::string serialized(compressed_data_.begin(), compressed_data_.end());
    std::istringstream ss(serialized);

    size_t row_count;
    ss >> row_count;

    rows_.clear();
    rows_.reserve(row_count);

    for (size_t i = 0; i < row_count; ++i) {
        size_t col_count;
        ss >> col_count;
        ss.ignore();  // Skip newline

        CachedRow row;
        row.reserve(col_count);

        for (size_t j = 0; j < col_count; ++j) {
            std::string val_str;
            std::getline(ss, val_str);

            // Simple deserialization - just store as string
            row.push_back(val_str);
        }

        rows_.push_back(std::move(row));
    }

    compressed_data_.clear();
    compressed_data_.shrink_to_fit();

    metadata_.is_compressed = false;
    metadata_.row_count = rows_.size();
    update_memory_usage();

    return true;
}

// =============================================================================
// ResultCacheKeyGenerator Implementation
// =============================================================================

std::string ResultCacheKeyGenerator::generate_key(
    std::string_view sql,
    const std::vector<CachedValue>& parameters) const {

    std::ostringstream ss;
    ss << sql;

    if (!parameters.empty()) {
        ss << "|";
        ss << serialize_parameters(parameters);
    }

    return ss.str();
}

uint64_t ResultCacheKeyGenerator::hash(std::string_view key) const {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : key) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::vector<std::string> ResultCacheKeyGenerator::extract_tables(std::string_view sql) const {
    std::vector<std::string> tables;
    std::string sql_str(sql);

    // FROM clause
    std::regex from_pattern(R"(\bFROM\s+([a-zA-Z_][a-zA-Z0-9_.]*)\b)", std::regex::icase);
    std::smatch match;
    std::string::const_iterator search_start(sql_str.cbegin());

    while (std::regex_search(search_start, sql_str.cend(), match, from_pattern)) {
        tables.push_back(match[1].str());
        search_start = match.suffix().first;
    }

    // JOIN clause
    std::regex join_pattern(R"(\bJOIN\s+([a-zA-Z_][a-zA-Z0-9_.]*)\b)", std::regex::icase);
    search_start = sql_str.cbegin();
    while (std::regex_search(search_start, sql_str.cend(), match, join_pattern)) {
        tables.push_back(match[1].str());
        search_start = match.suffix().first;
    }

    // Remove duplicates
    std::sort(tables.begin(), tables.end());
    tables.erase(std::unique(tables.begin(), tables.end()), tables.end());

    return tables;
}

std::string ResultCacheKeyGenerator::serialize_parameters(const std::vector<CachedValue>& params) const {
    std::ostringstream ss;

    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) ss << ",";
        ss << result_cache_utils::format_value(params[i]);
    }

    return ss.str();
}

// =============================================================================
// DatabaseResultCache Implementation
// =============================================================================

DatabaseResultCache::DatabaseResultCache(const std::string& database_name)
    : database_name_(database_name)
    , config_()
    , stats_() {
    stats_.created_at = std::chrono::system_clock::now();
}

DatabaseResultCache::DatabaseResultCache(const std::string& database_name, const ResultCacheConfig& config)
    : database_name_(database_name)
    , config_(config)
    , stats_() {
    stats_.created_at = std::chrono::system_clock::now();
}

DatabaseResultCache::~DatabaseResultCache() {
    clear();
}

std::shared_ptr<CachedResult> DatabaseResultCache::get(
    std::string_view sql,
    const std::vector<CachedValue>& parameters) {

    std::string key = key_generator_.generate_key(sql, parameters);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        update_statistics_on_get(false);
        return nullptr;
    }

    auto& result = it->second;

    // Check expiration
    if (result->is_expired()) {
        // Remove expired entry
        uint64_t hash = key_generator_.hash(key);
        hash_to_key_.erase(hash);

        // Remove from table index
        for (const auto& table : result->metadata().referenced_tables) {
            table_to_keys_[table].erase(key);
        }

        // Remove from LRU
        auto lru_it = lru_map_.find(key);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }

        stats_.memory_bytes -= result->memory_usage();
        cache_.erase(it);

        stats_.expiration_count++;
        update_statistics_on_get(false);
        return nullptr;
    }

    // Check if stale
    if (result->state() == ResultCacheEntryState::STALE ||
        result->state() == ResultCacheEntryState::INVALID) {
        update_statistics_on_get(false);
        return nullptr;
    }

    // Update eviction tracking
    switch (config_.eviction_policy) {
        case ResultEvictionPolicy::LRU: {
            auto lru_it = lru_map_.find(key);
            if (lru_it != lru_map_.end()) {
                lru_list_.erase(lru_it->second);
                lru_list_.push_front(key);
                lru_it->second = lru_list_.begin();
            }
            break;
        }
        case ResultEvictionPolicy::LFU:
            frequency_map_[key]++;
            break;
        default:
            break;
    }

    result->record_hit();
    update_statistics_on_get(true);

    return result;
}

std::shared_ptr<CachedResult> DatabaseResultCache::get_by_hash(uint64_t key_hash) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = hash_to_key_.find(key_hash);
    if (it == hash_to_key_.end()) {
        return nullptr;
    }

    std::string key = it->second;
    lock.unlock();

    return get(key, {});
}

bool DatabaseResultCache::put(std::shared_ptr<CachedResult> result) {
    if (!result || !should_cache(*result)) {
        return false;
    }

    std::string key = key_generator_.generate_key(
        result->metadata().sql,
        result->metadata().parameters);
    uint64_t hash = key_generator_.hash(key);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if already exists
    if (cache_.find(key) != cache_.end()) {
        return false;
    }

    // Ensure capacity
    ensure_capacity(result->memory_usage());

    // Check entry count
    if (cache_.size() >= config_.max_entries) {
        // Evict one entry
        std::string evicted;
        switch (config_.eviction_policy) {
            case ResultEvictionPolicy::LRU:
                evicted = evict_one_lru();
                break;
            case ResultEvictionPolicy::LFU:
                evicted = evict_one_lfu();
                break;
            case ResultEvictionPolicy::SIZE:
                evicted = evict_one_by_size();
                break;
            case ResultEvictionPolicy::TTL:
                evicted = evict_one_by_ttl();
                break;
        }

        if (!evicted.empty()) {
            stats_.eviction_count++;
        }
    }

    // Insert
    cache_[key] = result;
    hash_to_key_[hash] = key;

    // Add to eviction tracking
    switch (config_.eviction_policy) {
        case ResultEvictionPolicy::LRU:
        case ResultEvictionPolicy::TTL:
            lru_list_.push_front(key);
            lru_map_[key] = lru_list_.begin();
            break;
        case ResultEvictionPolicy::LFU:
            frequency_map_[key] = 1;
            break;
        default:
            break;
    }

    // Add to table index
    for (const auto& table : result->metadata().referenced_tables) {
        table_to_keys_[table].insert(key);
    }

    update_statistics_on_put(*result);

    return true;
}

bool DatabaseResultCache::remove(
    std::string_view sql,
    const std::vector<CachedValue>& parameters) {

    std::string key = key_generator_.generate_key(sql, parameters);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return false;
    }

    auto& result = it->second;
    uint64_t hash = key_generator_.hash(key);

    // Remove from hash index
    hash_to_key_.erase(hash);

    // Remove from eviction structures
    auto lru_it = lru_map_.find(key);
    if (lru_it != lru_map_.end()) {
        lru_list_.erase(lru_it->second);
        lru_map_.erase(lru_it);
    }
    frequency_map_.erase(key);

    // Remove from table index
    for (const auto& table : result->metadata().referenced_tables) {
        table_to_keys_[table].erase(key);
    }

    stats_.entry_count--;
    stats_.memory_bytes -= result->memory_usage();
    stats_.total_rows -= result->row_count();

    cache_.erase(it);

    return true;
}

bool DatabaseResultCache::contains(
    std::string_view sql,
    const std::vector<CachedValue>& parameters) const {

    std::string key = key_generator_.generate_key(sql, parameters);

    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cache_.find(key) != cache_.end();
}

void DatabaseResultCache::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    cache_.clear();
    hash_to_key_.clear();
    table_to_keys_.clear();

    lru_list_.clear();
    lru_map_.clear();
    frequency_map_.clear();

    pending_invalidation_tables_.clear();

    stats_.entry_count = 0;
    stats_.total_rows = 0;
    stats_.memory_bytes = 0;
}

uint64_t DatabaseResultCache::invalidate_by_table(const std::string& table_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = table_to_keys_.find(table_name);
    if (it == table_to_keys_.end()) {
        return 0;
    }

    uint64_t count = 0;
    for (const auto& key : it->second) {
        auto cache_it = cache_.find(key);
        if (cache_it != cache_.end()) {
            cache_it->second->set_state(ResultCacheEntryState::INVALID);
            count++;
        }
    }

    update_statistics_on_invalidate(table_name);
    stats_.invalidation_count += count;
    stats_.last_invalidation = std::chrono::system_clock::now();

    return count;
}

uint64_t DatabaseResultCache::invalidate_by_tables(const std::vector<std::string>& table_names) {
    uint64_t total = 0;
    for (const auto& table : table_names) {
        total += invalidate_by_table(table);
    }
    return total;
}

uint64_t DatabaseResultCache::invalidate_all() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t count = cache_.size();
    for (auto& pair : cache_) {
        pair.second->set_state(ResultCacheEntryState::INVALID);
    }

    stats_.invalidation_count += count;
    stats_.last_invalidation = std::chrono::system_clock::now();

    return count;
}

void DatabaseResultCache::mark_table_modified(const std::string& table_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    pending_invalidation_tables_.insert(table_name);
}

uint64_t DatabaseResultCache::commit_invalidations() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;
    for (const auto& table : pending_invalidation_tables_) {
        auto it = table_to_keys_.find(table);
        if (it != table_to_keys_.end()) {
            for (const auto& key : it->second) {
                auto cache_it = cache_.find(key);
                if (cache_it != cache_.end()) {
                    cache_it->second->set_state(ResultCacheEntryState::INVALID);
                    total++;
                }
            }
        }
    }

    pending_invalidation_tables_.clear();

    stats_.invalidation_count += total;
    stats_.last_invalidation = std::chrono::system_clock::now();

    return total;
}

void DatabaseResultCache::rollback_invalidations() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    pending_invalidation_tables_.clear();
}

uint64_t DatabaseResultCache::evict_to_memory(uint64_t target_bytes) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t freed = 0;
    while (stats_.memory_bytes > target_bytes && !cache_.empty()) {
        std::string evicted;
        switch (config_.eviction_policy) {
            case ResultEvictionPolicy::LRU:
                evicted = evict_one_lru();
                break;
            case ResultEvictionPolicy::LFU:
                evicted = evict_one_lfu();
                break;
            case ResultEvictionPolicy::SIZE:
                evicted = evict_one_by_size();
                break;
            case ResultEvictionPolicy::TTL:
                evicted = evict_one_by_ttl();
                break;
        }

        if (evicted.empty()) {
            break;
        }

        auto it = cache_.find(evicted);
        if (it != cache_.end()) {
            freed += it->second->memory_usage();
        }

        stats_.eviction_count++;
    }

    stats_.last_eviction = std::chrono::system_clock::now();

    return freed;
}

uint64_t DatabaseResultCache::evict_expired() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t evicted = 0;

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second->is_expired()) {
            std::string key = it->first;
            uint64_t hash = key_generator_.hash(key);

            // Remove from hash index
            hash_to_key_.erase(hash);

            // Remove from eviction structures
            auto lru_it = lru_map_.find(key);
            if (lru_it != lru_map_.end()) {
                lru_list_.erase(lru_it->second);
                lru_map_.erase(lru_it);
            }
            frequency_map_.erase(key);

            // Remove from table index
            for (const auto& table : it->second->metadata().referenced_tables) {
                table_to_keys_[table].erase(key);
            }

            stats_.total_rows -= it->second->row_count();
            stats_.memory_bytes -= it->second->memory_usage();

            it = cache_.erase(it);
            evicted++;
        } else {
            ++it;
        }
    }

    stats_.expiration_count += evicted;
    stats_.entry_count = cache_.size();

    return evicted;
}

void DatabaseResultCache::reset_statistics() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    stats_.total_hits = 0;
    stats_.total_misses = 0;
    stats_.hit_ratio = 0.0;
    stats_.eviction_count = 0;
    stats_.invalidation_count = 0;
    stats_.expiration_count = 0;
    stats_.total_time_saved = std::chrono::microseconds{0};
    stats_.total_rows_served = 0;
    stats_.queries_served_from_cache = 0;
    stats_.hits_by_table.clear();
    stats_.invalidations_by_table.clear();
}

void DatabaseResultCache::update_config(const ResultCacheConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    config_ = config;
}

uint64_t DatabaseResultCache::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cache_.size();
}

uint64_t DatabaseResultCache::memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_.memory_bytes;
}

std::string DatabaseResultCache::evict_one_lru() {
    if (lru_list_.empty()) {
        return "";
    }

    std::string key = lru_list_.back();
    lru_list_.pop_back();
    lru_map_.erase(key);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        uint64_t hash = key_generator_.hash(key);
        hash_to_key_.erase(hash);

        for (const auto& table : it->second->metadata().referenced_tables) {
            table_to_keys_[table].erase(key);
        }

        stats_.total_rows -= it->second->row_count();
        stats_.memory_bytes -= it->second->memory_usage();
        stats_.entry_count--;

        cache_.erase(it);
    }

    return key;
}

std::string DatabaseResultCache::evict_one_lfu() {
    if (frequency_map_.empty()) {
        return "";
    }

    // Find entry with lowest frequency
    auto min_it = std::min_element(frequency_map_.begin(), frequency_map_.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    std::string key = min_it->first;
    frequency_map_.erase(min_it);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        uint64_t hash = key_generator_.hash(key);
        hash_to_key_.erase(hash);

        for (const auto& table : it->second->metadata().referenced_tables) {
            table_to_keys_[table].erase(key);
        }

        // Remove from LRU if present
        auto lru_it = lru_map_.find(key);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }

        stats_.total_rows -= it->second->row_count();
        stats_.memory_bytes -= it->second->memory_usage();
        stats_.entry_count--;

        cache_.erase(it);
    }

    return key;
}

std::string DatabaseResultCache::evict_one_by_size() {
    if (cache_.empty()) {
        return "";
    }

    // Find largest entry
    auto max_it = std::max_element(cache_.begin(), cache_.end(),
        [](const auto& a, const auto& b) {
            return a.second->memory_usage() < b.second->memory_usage();
        });

    std::string key = max_it->first;
    uint64_t hash = key_generator_.hash(key);

    hash_to_key_.erase(hash);

    for (const auto& table : max_it->second->metadata().referenced_tables) {
        table_to_keys_[table].erase(key);
    }

    // Remove from other structures
    auto lru_it = lru_map_.find(key);
    if (lru_it != lru_map_.end()) {
        lru_list_.erase(lru_it->second);
        lru_map_.erase(lru_it);
    }
    frequency_map_.erase(key);

    stats_.total_rows -= max_it->second->row_count();
    stats_.memory_bytes -= max_it->second->memory_usage();
    stats_.entry_count--;

    cache_.erase(max_it);

    return key;
}

std::string DatabaseResultCache::evict_one_by_ttl() {
    if (cache_.empty()) {
        return "";
    }

    // Find entry with shortest remaining TTL
    auto min_it = std::min_element(cache_.begin(), cache_.end(),
        [](const auto& a, const auto& b) {
            return a.second->remaining_ttl() < b.second->remaining_ttl();
        });

    std::string key = min_it->first;
    uint64_t hash = key_generator_.hash(key);

    hash_to_key_.erase(hash);

    for (const auto& table : min_it->second->metadata().referenced_tables) {
        table_to_keys_[table].erase(key);
    }

    // Remove from other structures
    auto lru_it = lru_map_.find(key);
    if (lru_it != lru_map_.end()) {
        lru_list_.erase(lru_it->second);
        lru_map_.erase(lru_it);
    }
    frequency_map_.erase(key);

    stats_.total_rows -= min_it->second->row_count();
    stats_.memory_bytes -= min_it->second->memory_usage();
    stats_.entry_count--;

    cache_.erase(min_it);

    return key;
}

bool DatabaseResultCache::should_cache(const CachedResult& result) const {
    // Check row count
    if (result.row_count() < config_.min_rows_to_cache) {
        return false;
    }
    if (result.row_count() > config_.max_rows_to_cache) {
        return false;
    }

    // Check memory
    if (result.memory_usage() > config_.max_result_size) {
        return false;
    }

    // Check empty results
    if (result.row_count() == 0 && !config_.cache_empty_results) {
        return false;
    }

    // Check excluded tables
    for (const auto& table : result.metadata().referenced_tables) {
        if (std::find(config_.excluded_tables.begin(), config_.excluded_tables.end(), table) !=
            config_.excluded_tables.end()) {
            return false;
        }
    }

    return true;
}

void DatabaseResultCache::ensure_capacity(uint64_t needed_bytes) {
    while (stats_.memory_bytes + needed_bytes > config_.max_memory_bytes && !cache_.empty()) {
        std::string evicted;
        switch (config_.eviction_policy) {
            case ResultEvictionPolicy::LRU:
                evicted = evict_one_lru();
                break;
            case ResultEvictionPolicy::LFU:
                evicted = evict_one_lfu();
                break;
            case ResultEvictionPolicy::SIZE:
                evicted = evict_one_by_size();
                break;
            case ResultEvictionPolicy::TTL:
                evicted = evict_one_by_ttl();
                break;
        }

        if (evicted.empty()) {
            break;
        }

        stats_.eviction_count++;
    }
}

void DatabaseResultCache::update_statistics_on_get(bool hit) {
    if (hit) {
        stats_.total_hits++;
        stats_.queries_served_from_cache++;
    } else {
        stats_.total_misses++;
    }

    uint64_t total = stats_.total_hits + stats_.total_misses;
    if (total > 0) {
        stats_.hit_ratio = static_cast<double>(stats_.total_hits) / total;
    }
}

void DatabaseResultCache::update_statistics_on_put(const CachedResult& result) {
    stats_.entry_count++;
    stats_.total_rows += result.row_count();
    stats_.memory_bytes += result.memory_usage();
}

void DatabaseResultCache::update_statistics_on_evict() {
    // Updated in individual evict methods
}

void DatabaseResultCache::update_statistics_on_invalidate(const std::string& table_name) {
    stats_.invalidations_by_table[table_name]++;
}

// =============================================================================
// ResultCacheManager Implementation
// =============================================================================

ResultCacheManager& ResultCacheManager::instance() {
    static ResultCacheManager instance;
    return instance;
}

void ResultCacheManager::initialize(const ResultCacheConfig& default_config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    default_config_ = default_config;
    initialized_ = true;
}

void ResultCacheManager::shutdown() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    caches_.clear();
    initialized_ = false;
}

std::shared_ptr<DatabaseResultCache> ResultCacheManager::get_cache(const std::string& database_name) {
    std::shared_lock<std::shared_mutex> read_lock(mutex_);

    auto it = caches_.find(database_name);
    if (it != caches_.end()) {
        return it->second;
    }

    read_lock.unlock();

    std::unique_lock<std::shared_mutex> write_lock(mutex_);

    // Double-check
    it = caches_.find(database_name);
    if (it != caches_.end()) {
        return it->second;
    }

    auto cache = std::make_shared<DatabaseResultCache>(database_name, default_config_);
    caches_[database_name] = cache;

    return cache;
}

std::shared_ptr<DatabaseResultCache> ResultCacheManager::get_cache(
    const std::string& database_name,
    const ResultCacheConfig& config) {

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = caches_.find(database_name);
    if (it != caches_.end()) {
        return it->second;
    }

    auto cache = std::make_shared<DatabaseResultCache>(database_name, config);
    caches_[database_name] = cache;

    return cache;
}

bool ResultCacheManager::remove_cache(const std::string& database_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = caches_.find(database_name);
    if (it == caches_.end()) {
        return false;
    }

    caches_.erase(it);
    return true;
}

bool ResultCacheManager::has_cache(const std::string& database_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return caches_.find(database_name) != caches_.end();
}

std::vector<std::string> ResultCacheManager::get_database_names() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(caches_.size());

    for (const auto& pair : caches_) {
        names.push_back(pair.first);
    }

    return names;
}

void ResultCacheManager::clear_all() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& pair : caches_) {
        pair.second->clear();
    }
}

uint64_t ResultCacheManager::invalidate_table(const std::string& database_name, const std::string& table_name) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;

    if (database_name.empty()) {
        for (auto& pair : caches_) {
            total += pair.second->invalidate_by_table(table_name);
        }
    } else {
        auto it = caches_.find(database_name);
        if (it != caches_.end()) {
            total = it->second->invalidate_by_table(table_name);
        }
    }

    return total;
}

ResultCacheStatistics ResultCacheManager::get_aggregate_statistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    ResultCacheStatistics aggregate{};
    aggregate.created_at = std::chrono::system_clock::now();

    for (const auto& pair : caches_) {
        const auto& stats = pair.second->statistics();

        aggregate.entry_count += stats.entry_count;
        aggregate.total_rows += stats.total_rows;
        aggregate.memory_bytes += stats.memory_bytes;
        aggregate.total_hits += stats.total_hits;
        aggregate.total_misses += stats.total_misses;
        aggregate.eviction_count += stats.eviction_count;
        aggregate.invalidation_count += stats.invalidation_count;
        aggregate.expiration_count += stats.expiration_count;
        aggregate.total_time_saved += stats.total_time_saved;
        aggregate.total_rows_served += stats.total_rows_served;
        aggregate.queries_served_from_cache += stats.queries_served_from_cache;
    }

    uint64_t total = aggregate.total_hits + aggregate.total_misses;
    if (total > 0) {
        aggregate.hit_ratio = static_cast<double>(aggregate.total_hits) / total;
    }

    return aggregate;
}

uint64_t ResultCacheManager::total_memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;
    for (const auto& pair : caches_) {
        total += pair.second->memory_usage();
    }
    return total;
}

uint64_t ResultCacheManager::total_entry_count() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;
    for (const auto& pair : caches_) {
        total += pair.second->size();
    }
    return total;
}

uint64_t ResultCacheManager::apply_global_memory_limit(uint64_t max_bytes) {
    uint64_t current = total_memory_usage();
    if (current <= max_bytes) {
        return 0;
    }

    uint64_t to_free = current - max_bytes;
    uint64_t freed = 0;

    std::shared_lock<std::shared_mutex> lock(mutex_);

    // Evict from caches in round-robin fashion
    while (freed < to_free) {
        bool any_evicted = false;

        for (auto& pair : caches_) {
            if (pair.second->size() > 0) {
                pair.second->evict_expired();

                if (pair.second->memory_usage() > 0) {
                    uint64_t before = pair.second->memory_usage();
                    pair.second->evict_to_memory(pair.second->memory_usage() - 1024);
                    uint64_t after = pair.second->memory_usage();
                    freed += (before - after);
                    any_evicted = true;
                }
            }
        }

        if (!any_evicted) {
            break;
        }
    }

    return freed;
}

// =============================================================================
// CachedResultGuard Implementation
// =============================================================================

CachedResultGuard::CachedResultGuard(std::shared_ptr<CachedResult> result)
    : result_(std::move(result)) {
}

CachedResultGuard::~CachedResultGuard() {
    // Nothing special to do
}

// =============================================================================
// CachedResultIterator Implementation
// =============================================================================

CachedResultIterator::CachedResultIterator(const CachedResult* result, uint64_t index)
    : result_(result)
    , index_(index) {
}

CachedResultIterator::reference CachedResultIterator::operator*() const {
    return result_->row(index_);
}

CachedResultIterator::pointer CachedResultIterator::operator->() const {
    return &result_->row(index_);
}

CachedResultIterator& CachedResultIterator::operator++() {
    ++index_;
    return *this;
}

CachedResultIterator CachedResultIterator::operator++(int) {
    CachedResultIterator tmp = *this;
    ++index_;
    return tmp;
}

bool CachedResultIterator::operator==(const CachedResultIterator& other) const {
    return result_ == other.result_ && index_ == other.index_;
}

bool CachedResultIterator::operator!=(const CachedResultIterator& other) const {
    return !(*this == other);
}

// =============================================================================
// Utility Functions
// =============================================================================

namespace result_cache_utils {

std::string format_statistics(const ResultCacheStatistics& stats) {
    std::ostringstream ss;

    ss << "Result Cache Statistics:\n"
       << "  Entries: " << stats.entry_count << "\n"
       << "  Total Rows: " << stats.total_rows << "\n"
       << "  Memory: " << (stats.memory_bytes / 1024 / 1024) << " MB\n"
       << "  Hit Ratio: " << (stats.hit_ratio * 100.0) << "%\n"
       << "  Hits: " << stats.total_hits << "\n"
       << "  Misses: " << stats.total_misses << "\n"
       << "  Evictions: " << stats.eviction_count << "\n"
       << "  Invalidations: " << stats.invalidation_count << "\n"
       << "  Expirations: " << stats.expiration_count << "\n"
       << "  Queries from Cache: " << stats.queries_served_from_cache << "\n"
       << "  Rows Served: " << stats.total_rows_served << "\n"
       << "  Time Saved: " << stats.total_time_saved.count() << " us";

    return ss.str();
}

uint64_t estimate_result_memory(const std::vector<CachedRow>& rows) {
    uint64_t bytes = sizeof(std::vector<CachedRow>);

    for (const auto& row : rows) {
        bytes += sizeof(CachedRow);
        for (const auto& val : row) {
            bytes += value_memory(val);
        }
    }

    return bytes;
}

uint64_t value_memory(const CachedValue& value) {
    return std::visit([](auto&& arg) -> uint64_t {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return sizeof(std::monostate);
        } else if constexpr (std::is_same_v<T, bool>) {
            return sizeof(bool);
        } else if constexpr (std::is_same_v<T, int16_t>) {
            return sizeof(int16_t);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return sizeof(int32_t);
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return sizeof(int64_t);
        } else if constexpr (std::is_same_v<T, float>) {
            return sizeof(float);
        } else if constexpr (std::is_same_v<T, double>) {
            return sizeof(double);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return sizeof(std::string) + arg.size();
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            return sizeof(std::vector<uint8_t>) + arg.size();
        } else {
            return sizeof(T);
        }
    }, value);
}

bool should_cache_result(const ResultCacheConfig& config, uint64_t row_count, uint64_t memory_bytes) {
    if (row_count < config.min_rows_to_cache) {
        return false;
    }
    if (row_count > config.max_rows_to_cache) {
        return false;
    }
    if (memory_bytes > config.max_result_size) {
        return false;
    }
    return true;
}

std::string format_value(const CachedValue& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::monostate>) {
            return "NULL";
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int16_t> ||
                           std::is_same_v<T, int32_t> ||
                           std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, float> ||
                           std::is_same_v<T, double>) {
            std::ostringstream ss;
            ss << arg;
            return ss.str();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return "'" + arg + "'";
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            std::ostringstream ss;
            ss << "\\x";
            for (uint8_t b : arg) {
                ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
            }
            return ss.str();
        } else {
            return "<unknown>";
        }
    }, value);
}

}  // namespace result_cache_utils

}  // namespace pool
}  // namespace scratchbird
