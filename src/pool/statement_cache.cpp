/**
 * @file statement_cache.cpp
 * @brief Statement Cache Implementation
 *
 * Implements prepared statement caching with multiple eviction policies:
 * - LRU (Least Recently Used)
 * - LFU (Least Frequently Used)
 * - ARC (Adaptive Replacement Cache)
 * - FIFO (First In First Out)
 *
 * Part of Phase 3.6: Connection Pooling
 */

#include "scratchbird/pool/statement_cache.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <regex>
#include <sstream>

namespace scratchbird {
namespace pool {

// =============================================================================
// CachedStatement Implementation
// =============================================================================

CachedStatement::CachedStatement(const std::string& sql, const StatementMetadata& metadata)
    : metadata_(metadata)
    , state_(CacheEntryState::VALID) {
    metadata_.sql = sql;
    stats_.created_at = std::chrono::system_clock::now();
    stats_.last_accessed = stats_.created_at;
    stats_.last_executed = stats_.created_at;
    stats_.expires_at = stats_.created_at + ttl_;
    update_memory_usage();
}

bool CachedStatement::is_expired() const {
    auto now = std::chrono::system_clock::now();
    return now >= stats_.expires_at;
}

void CachedStatement::record_hit() {
    stats_.hit_count++;
    stats_.last_accessed = std::chrono::system_clock::now();
}

void CachedStatement::record_miss() {
    stats_.miss_count++;
}

void CachedStatement::record_execution(std::chrono::microseconds duration, bool success) {
    stats_.execution_count++;
    stats_.last_executed = std::chrono::system_clock::now();
    stats_.last_accessed = stats_.last_executed;

    if (success) {
        stats_.total_execution_time += duration;

        if (stats_.execution_count == 1) {
            stats_.min_execution_time = duration;
            stats_.max_execution_time = duration;
        } else {
            stats_.min_execution_time = std::min(stats_.min_execution_time, duration);
            stats_.max_execution_time = std::max(stats_.max_execution_time, duration);
        }

        stats_.avg_execution_time = std::chrono::microseconds(
            stats_.total_execution_time.count() / stats_.execution_count);
    } else {
        stats_.error_count++;
    }
}

uint64_t CachedStatement::memory_usage() const {
    return stats_.memory_bytes;
}

void CachedStatement::update_memory_usage() {
    // Estimate memory usage
    uint64_t bytes = sizeof(CachedStatement);
    bytes += metadata_.sql.size();
    bytes += metadata_.fingerprint.size();

    for (const auto& table : metadata_.referenced_tables) {
        bytes += table.size();
    }
    for (const auto& schema : metadata_.referenced_schemas) {
        bytes += schema.size();
    }
    for (const auto& func : metadata_.referenced_functions) {
        bytes += func.size();
    }

    bytes += metadata_.parameter_types.size() * sizeof(uint32_t);
    bytes += metadata_.privilege_signature.size();

    for (const auto& col : metadata_.result_column_names) {
        bytes += col.size();
    }
    bytes += metadata_.result_column_types.size() * sizeof(uint32_t);

    stats_.memory_bytes = bytes;
}

void CachedStatement::set_ttl(std::chrono::seconds ttl) {
    ttl_ = ttl;
    stats_.expires_at = stats_.created_at + ttl_;
}

std::chrono::seconds CachedStatement::remaining_ttl() const {
    auto now = std::chrono::system_clock::now();
    if (now >= stats_.expires_at) {
        return std::chrono::seconds{0};
    }
    return std::chrono::duration_cast<std::chrono::seconds>(stats_.expires_at - now);
}

void CachedStatement::refresh_ttl() {
    stats_.expires_at = std::chrono::system_clock::now() + ttl_;
}

// =============================================================================
// StatementFingerprinter Implementation
// =============================================================================

StatementFingerprinter::StatementFingerprinter(const StatementCacheConfig& config)
    : normalize_whitespace_(config.normalize_whitespace)
    , normalize_literals_(config.normalize_literals) {
}

std::string StatementFingerprinter::fingerprint(std::string_view sql) const {
    std::string result(sql);

    // Normalize whitespace
    if (normalize_whitespace_) {
        result = normalize_whitespace(result);
    }

    // Replace literals with placeholders
    if (normalize_literals_) {
        result = replace_literals(result);
    }

    // Normalize case (uppercase keywords)
    result = normalize_case(result);

    return result;
}

uint64_t StatementFingerprinter::hash(std::string_view fingerprint) const {
    // FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    for (char c : fingerprint) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string StatementFingerprinter::parameter_signature(const std::vector<uint32_t>& param_types) const {
    if (param_types.empty()) {
        return {};
    }

    std::ostringstream oss;
    for (size_t i = 0; i < param_types.size(); ++i) {
        if (i > 0) {
            oss << ',';
        }
        oss << param_types[i];
    }
    return oss.str();
}

std::string StatementFingerprinter::cache_key(std::string_view sql,
                                              const std::vector<uint32_t>& param_types,
                                              uint64_t schema_version_id,
                                              const std::string& privilege_signature) const {
    std::string fp = fingerprint(sql);
    return cache_key_from_fingerprint(fp, param_types, schema_version_id, privilege_signature);
}

std::string StatementFingerprinter::cache_key_from_fingerprint(
    std::string_view fingerprint,
    const std::vector<uint32_t>& param_types,
    uint64_t schema_version_id,
    const std::string& privilege_signature) const {
    std::string sig = parameter_signature(param_types);
    std::string key;
    key.reserve(fingerprint.size() + sig.size() + privilege_signature.size() + 32);
    key.append(fingerprint);
    if (!sig.empty()) {
        key.append("|P:");
        key.append(sig);
    }
    if (schema_version_id != 0) {
        key.append("|S:");
        key.append(std::to_string(schema_version_id));
    }
    if (!privilege_signature.empty()) {
        key.append("|A:");
        key.append(privilege_signature);
    }
    return key;
}

StatementType StatementFingerprinter::detect_type(std::string_view sql) const {
    // Skip leading whitespace
    size_t pos = 0;
    while (pos < sql.size() && std::isspace(sql[pos])) {
        ++pos;
    }

    if (pos >= sql.size()) {
        return StatementType::UNKNOWN;
    }

    // Get first keyword (case-insensitive)
    std::string first_word;
    while (pos < sql.size() && std::isalpha(sql[pos])) {
        first_word += std::toupper(sql[pos]);
        ++pos;
    }

    if (first_word == "SELECT") return StatementType::SELECT;
    if (first_word == "INSERT") return StatementType::INSERT;
    if (first_word == "UPDATE") return StatementType::UPDATE;
    if (first_word == "DELETE") return StatementType::DELETE;
    if (first_word == "CREATE" || first_word == "ALTER" || first_word == "DROP" ||
        first_word == "TRUNCATE") return StatementType::DDL;
    if (first_word == "GRANT" || first_word == "REVOKE") return StatementType::DCL;
    if (first_word == "BEGIN" || first_word == "COMMIT" || first_word == "ROLLBACK" ||
        first_word == "SAVEPOINT") return StatementType::TCL;
    if (first_word == "SET" || first_word == "SHOW" || first_word == "EXPLAIN" ||
        first_word == "ANALYZE") return StatementType::UTILITY;

    return StatementType::UNKNOWN;
}

std::vector<std::string> StatementFingerprinter::extract_tables(std::string_view sql) const {
    std::vector<std::string> tables;

    // Simple regex-based extraction (not full SQL parser)
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

    // INTO clause (INSERT)
    std::regex into_pattern(R"(\bINTO\s+([a-zA-Z_][a-zA-Z0-9_.]*)\b)", std::regex::icase);
    search_start = sql_str.cbegin();
    while (std::regex_search(search_start, sql_str.cend(), match, into_pattern)) {
        tables.push_back(match[1].str());
        search_start = match.suffix().first;
    }

    // UPDATE clause
    std::regex update_pattern(R"(\bUPDATE\s+([a-zA-Z_][a-zA-Z0-9_.]*)\b)", std::regex::icase);
    search_start = sql_str.cbegin();
    while (std::regex_search(search_start, sql_str.cend(), match, update_pattern)) {
        tables.push_back(match[1].str());
        search_start = match.suffix().first;
    }

    // Remove duplicates
    std::sort(tables.begin(), tables.end());
    tables.erase(std::unique(tables.begin(), tables.end()), tables.end());

    return tables;
}

std::string StatementFingerprinter::normalize_whitespace(std::string_view sql) const {
    std::string result;
    result.reserve(sql.size());

    bool in_string = false;
    char string_char = 0;
    bool last_was_space = true;  // Start true to skip leading spaces

    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];

        // Track string literals
        if (!in_string && (c == '\'' || c == '"')) {
            in_string = true;
            string_char = c;
            result += c;
            last_was_space = false;
        } else if (in_string && c == string_char) {
            // Check for escaped quote
            if (i + 1 < sql.size() && sql[i + 1] == string_char) {
                result += c;
                result += sql[++i];
            } else {
                in_string = false;
                result += c;
            }
            last_was_space = false;
        } else if (in_string) {
            result += c;
        } else if (std::isspace(c)) {
            if (!last_was_space) {
                result += ' ';
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }

    // Trim trailing space
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result;
}

std::string StatementFingerprinter::replace_literals(std::string_view sql) const {
    std::string result;
    result.reserve(sql.size());

    bool in_string = false;
    char string_char = 0;
    bool in_number = false;

    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];

        // Track string literals
        if (!in_string && (c == '\'' || c == '"')) {
            in_string = true;
            string_char = c;
            // Replace string with placeholder
            result += "?";
            // Skip to end of string
            for (++i; i < sql.size(); ++i) {
                if (sql[i] == string_char) {
                    if (i + 1 < sql.size() && sql[i + 1] == string_char) {
                        ++i;  // Skip escaped quote
                    } else {
                        break;
                    }
                }
            }
            in_string = false;
        } else if (!in_string && (std::isdigit(c) || (c == '-' && i + 1 < sql.size() && std::isdigit(sql[i + 1])))) {
            // Replace number with placeholder
            if (!in_number) {
                result += "?";
                in_number = true;
            }
            // Skip digits
            while (i + 1 < sql.size() &&
                   (std::isdigit(sql[i + 1]) || sql[i + 1] == '.' || sql[i + 1] == 'e' || sql[i + 1] == 'E')) {
                ++i;
            }
        } else {
            in_number = false;
            result += c;
        }
    }

    return result;
}

std::string StatementFingerprinter::normalize_case(std::string_view sql) const {
    std::string result;
    result.reserve(sql.size());

    bool in_string = false;
    char string_char = 0;

    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];

        if (!in_string && (c == '\'' || c == '"')) {
            in_string = true;
            string_char = c;
            result += c;
        } else if (in_string && c == string_char) {
            if (i + 1 < sql.size() && sql[i + 1] == string_char) {
                result += c;
                result += sql[++i];
            } else {
                in_string = false;
                result += c;
            }
        } else if (in_string) {
            result += c;
        } else {
            // Uppercase outside strings
            result += std::toupper(c);
        }
    }

    return result;
}

// =============================================================================
// DatabaseStatementCache Implementation
// =============================================================================

DatabaseStatementCache::DatabaseStatementCache(const std::string& database_name)
    : database_name_(database_name)
    , config_()
    , fingerprinter_(config_)
    , stats_() {
    stats_.created_at = std::chrono::system_clock::now();
}

DatabaseStatementCache::DatabaseStatementCache(const std::string& database_name, const StatementCacheConfig& config)
    : database_name_(database_name)
    , config_(config)
    , fingerprinter_(config)
    , stats_() {
    stats_.created_at = std::chrono::system_clock::now();
}

DatabaseStatementCache::~DatabaseStatementCache() {
    clear();
}

std::shared_ptr<CachedStatement> DatabaseStatementCache::get(std::string_view sql) {
    return get(sql, {});
}

std::shared_ptr<CachedStatement> DatabaseStatementCache::get(
    std::string_view sql,
    const std::vector<uint32_t>& param_types,
    uint64_t schema_version_id,
    const std::string& privilege_signature) {
    std::string fp = fingerprinter_.cache_key(sql, param_types, schema_version_id,
                                              privilege_signature);
    uint64_t hash = fingerprinter_.hash(fp);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = cache_.find(fp);
    if (it == cache_.end()) {
        update_statistics_on_get(false, StatementType::UNKNOWN);
        return nullptr;
    }

    auto& stmt = it->second;

    // Check expiration
    if (stmt->is_expired()) {
        // Remove expired entry
        cache_.erase(it);
        hash_to_fingerprint_.erase(hash);

        // Remove from eviction structures
        if (config_.eviction_policy == StatementEvictionPolicy::LRU) {
            auto lru_it = lru_map_.find(fp);
            if (lru_it != lru_map_.end()) {
                lru_list_.erase(lru_it->second);
                lru_map_.erase(lru_it);
            }
        }

        // Remove from table index
        for (const auto& table : stmt->metadata().referenced_tables) {
            auto& fps = table_to_fingerprints_[table];
            fps.erase(std::remove(fps.begin(), fps.end(), fp), fps.end());
        }

        stats_.expiration_count++;
        update_statistics_on_get(false, stmt->statement_type());
        return nullptr;
    }

    // Update eviction tracking
    switch (config_.eviction_policy) {
        case StatementEvictionPolicy::LRU:
            promote_lru(fp);
            break;
        case StatementEvictionPolicy::LFU:
            update_lfu_frequency(fp);
            break;
        case StatementEvictionPolicy::ARC:
            update_arc(fp, true);
            break;
        default:
            break;
    }

    stmt->record_hit();
    update_statistics_on_get(true, stmt->statement_type());

    return stmt;
}

std::shared_ptr<CachedStatement> DatabaseStatementCache::get_by_hash(uint64_t hash) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    auto it = hash_to_fingerprint_.find(hash);
    if (it == hash_to_fingerprint_.end()) {
        return nullptr;
    }

    lock.unlock();
    return get(it->second);
}

bool DatabaseStatementCache::put(std::shared_ptr<CachedStatement> statement) {
    if (!statement || !should_cache(*statement)) {
        return false;
    }

    auto& metadata = statement->metadata();
    std::string base_fp = metadata.fingerprint.empty()
        ? fingerprinter_.fingerprint(statement->sql())
        : metadata.fingerprint;
    std::string fp = fingerprinter_.cache_key_from_fingerprint(base_fp,
                                                               metadata.parameter_types,
                                                               metadata.schema_version_id,
                                                               metadata.privilege_signature);
    uint64_t hash = fingerprinter_.hash(fp);
    if (metadata.fingerprint != fp || metadata.fingerprint_hash != hash) {
        metadata.fingerprint = fp;
        metadata.fingerprint_hash = hash;
        statement->update_memory_usage();
    }

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Check if already exists
    if (cache_.find(fp) != cache_.end()) {
        return false;  // Already cached
    }

    // Check capacity
    if (cache_.size() >= config_.max_statements) {
        // Evict one entry
        std::string evicted;
        switch (config_.eviction_policy) {
            case StatementEvictionPolicy::LRU:
                evicted = evict_one_lru();
                break;
            case StatementEvictionPolicy::LFU:
                evicted = evict_one_lfu();
                break;
            case StatementEvictionPolicy::ARC:
                evicted = evict_one_arc();
                break;
            case StatementEvictionPolicy::FIFO:
            default:
                evicted = evict_one_lru();  // FIFO uses same structure
                break;
        }

        if (!evicted.empty()) {
            stats_.eviction_count++;
        }
    }

    // Insert
    cache_[fp] = statement;
    hash_to_fingerprint_[hash] = fp;

    // Add to eviction tracking
    switch (config_.eviction_policy) {
        case StatementEvictionPolicy::LRU:
        case StatementEvictionPolicy::FIFO:
            lru_list_.push_front(fp);
            lru_map_[fp] = lru_list_.begin();
            break;
        case StatementEvictionPolicy::LFU:
            frequency_map_[fp] = 1;
            frequency_lists_[1].push_back(fp);
            break;
        case StatementEvictionPolicy::ARC:
            arc_t1_.push_front(fp);
            break;
    }

    // Add to table index
    for (const auto& table : statement->metadata().referenced_tables) {
        table_to_fingerprints_[table].push_back(fp);
    }

    update_statistics_on_put(*statement);

    return true;
}

bool DatabaseStatementCache::remove(std::string_view sql) {
    return remove(sql, {});
}

bool DatabaseStatementCache::remove(std::string_view sql,
                                    const std::vector<uint32_t>& param_types,
                                    uint64_t schema_version_id,
                                    const std::string& privilege_signature) {
    std::string fp = fingerprinter_.cache_key(sql, param_types, schema_version_id,
                                              privilege_signature);

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = cache_.find(fp);
    if (it == cache_.end()) {
        return false;
    }

    auto& stmt = it->second;
    uint64_t hash = stmt->fingerprint_hash();

    // Remove from hash index
    hash_to_fingerprint_.erase(hash);

    // Remove from eviction structures
    if (config_.eviction_policy == StatementEvictionPolicy::LRU ||
        config_.eviction_policy == StatementEvictionPolicy::FIFO) {
        auto lru_it = lru_map_.find(fp);
        if (lru_it != lru_map_.end()) {
            lru_list_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
    } else if (config_.eviction_policy == StatementEvictionPolicy::LFU) {
        auto freq_it = frequency_map_.find(fp);
        if (freq_it != frequency_map_.end()) {
            auto& list = frequency_lists_[freq_it->second];
            list.remove(fp);
            frequency_map_.erase(freq_it);
        }
    }

    // Remove from table index
    for (const auto& table : stmt->metadata().referenced_tables) {
        auto& fps = table_to_fingerprints_[table];
        fps.erase(std::remove(fps.begin(), fps.end(), fp), fps.end());
    }

    stats_.statement_count--;
    stats_.memory_bytes -= stmt->memory_usage();

    cache_.erase(it);

    return true;
}

bool DatabaseStatementCache::contains(std::string_view sql) const {
    return contains(sql, {});
}

bool DatabaseStatementCache::contains(std::string_view sql,
                                      const std::vector<uint32_t>& param_types,
                                      uint64_t schema_version_id,
                                      const std::string& privilege_signature) const {
    std::string fp = fingerprinter_.cache_key(sql, param_types, schema_version_id,
                                              privilege_signature);

    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cache_.find(fp) != cache_.end();
}

void DatabaseStatementCache::clear() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    cache_.clear();
    hash_to_fingerprint_.clear();
    table_to_fingerprints_.clear();

    lru_list_.clear();
    lru_map_.clear();

    frequency_map_.clear();
    frequency_lists_.clear();

    arc_t1_.clear();
    arc_t2_.clear();
    arc_b1_.clear();
    arc_b2_.clear();
    arc_p_ = 0.0;

    stats_.statement_count = 0;
    stats_.memory_bytes = 0;
}

uint64_t DatabaseStatementCache::invalidate_by_table(const std::string& table_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = table_to_fingerprints_.find(table_name);
    if (it == table_to_fingerprints_.end()) {
        return 0;
    }

    uint64_t count = 0;
    for (const auto& fp : it->second) {
        auto cache_it = cache_.find(fp);
        if (cache_it != cache_.end()) {
            cache_it->second->set_state(CacheEntryState::INVALID);
            count++;
        }
    }

    stats_.invalidation_count += count;
    stats_.last_invalidation = std::chrono::system_clock::now();

    return count;
}

uint64_t DatabaseStatementCache::invalidate_by_schema(const std::string& schema_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t count = 0;
    for (auto& pair : cache_) {
        for (const auto& schema : pair.second->metadata().referenced_schemas) {
            if (schema == schema_name) {
                pair.second->set_state(CacheEntryState::INVALID);
                count++;
                break;
            }
        }
    }

    stats_.invalidation_count += count;
    stats_.last_invalidation = std::chrono::system_clock::now();

    return count;
}

uint64_t DatabaseStatementCache::invalidate_all() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t count = cache_.size();
    for (auto& pair : cache_) {
        pair.second->set_state(CacheEntryState::INVALID);
    }

    stats_.invalidation_count += count;
    stats_.last_invalidation = std::chrono::system_clock::now();

    return count;
}

uint64_t DatabaseStatementCache::evict(uint64_t target_count) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t evicted = 0;
    while (cache_.size() > target_count) {
        std::string fp;
        switch (config_.eviction_policy) {
            case StatementEvictionPolicy::LRU:
            case StatementEvictionPolicy::FIFO:
                fp = evict_one_lru();
                break;
            case StatementEvictionPolicy::LFU:
                fp = evict_one_lfu();
                break;
            case StatementEvictionPolicy::ARC:
                fp = evict_one_arc();
                break;
        }

        if (fp.empty()) {
            break;
        }
        evicted++;
    }

    stats_.eviction_count += evicted;
    stats_.last_eviction = std::chrono::system_clock::now();

    return evicted;
}

uint64_t DatabaseStatementCache::evict_expired() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    uint64_t evicted = 0;
    auto now = std::chrono::system_clock::now();

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second->is_expired()) {
            std::string fp = it->first;
            uint64_t hash = it->second->fingerprint_hash();

            // Remove from hash index
            hash_to_fingerprint_.erase(hash);

            // Remove from eviction structures
            if (config_.eviction_policy == StatementEvictionPolicy::LRU ||
                config_.eviction_policy == StatementEvictionPolicy::FIFO) {
                auto lru_it = lru_map_.find(fp);
                if (lru_it != lru_map_.end()) {
                    lru_list_.erase(lru_it->second);
                    lru_map_.erase(lru_it);
                }
            }

            // Remove from table index
            for (const auto& table : it->second->metadata().referenced_tables) {
                auto& fps = table_to_fingerprints_[table];
                fps.erase(std::remove(fps.begin(), fps.end(), fp), fps.end());
            }

            stats_.memory_bytes -= it->second->memory_usage();
            it = cache_.erase(it);
            evicted++;
        } else {
            ++it;
        }
    }

    stats_.expiration_count += evicted;
    stats_.statement_count = cache_.size();

    return evicted;
}

void DatabaseStatementCache::reset_statistics() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    stats_.total_hits = 0;
    stats_.total_misses = 0;
    stats_.hit_ratio = 0.0;
    stats_.eviction_count = 0;
    stats_.invalidation_count = 0;
    stats_.expiration_count = 0;
    stats_.count_by_type.clear();
    stats_.hits_by_type.clear();
    stats_.avg_lookup_time = std::chrono::microseconds{0};
    stats_.avg_prepare_time = std::chrono::microseconds{0};
}

void DatabaseStatementCache::update_config(const StatementCacheConfig& config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    config_ = config;
    fingerprinter_ = StatementFingerprinter(config_);
}

uint64_t DatabaseStatementCache::size() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return cache_.size();
}

uint64_t DatabaseStatementCache::memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return stats_.memory_bytes;
}

// LRU helpers
void DatabaseStatementCache::promote_lru(const std::string& fingerprint) {
    auto it = lru_map_.find(fingerprint);
    if (it != lru_map_.end()) {
        lru_list_.erase(it->second);
        lru_list_.push_front(fingerprint);
        it->second = lru_list_.begin();
    }
}

std::string DatabaseStatementCache::evict_one_lru() {
    if (lru_list_.empty()) {
        return "";
    }

    std::string fp = lru_list_.back();
    lru_list_.pop_back();
    lru_map_.erase(fp);

    auto it = cache_.find(fp);
    if (it != cache_.end()) {
        uint64_t hash = it->second->fingerprint_hash();
        hash_to_fingerprint_.erase(hash);

        for (const auto& table : it->second->metadata().referenced_tables) {
            auto& fps = table_to_fingerprints_[table];
            fps.erase(std::remove(fps.begin(), fps.end(), fp), fps.end());
        }

        stats_.memory_bytes -= it->second->memory_usage();
        stats_.statement_count--;
        cache_.erase(it);
    }

    return fp;
}

// LFU helpers
void DatabaseStatementCache::update_lfu_frequency(const std::string& fingerprint) {
    auto it = frequency_map_.find(fingerprint);
    if (it == frequency_map_.end()) {
        return;
    }

    uint64_t old_freq = it->second;
    uint64_t new_freq = old_freq + 1;

    // Remove from old frequency list
    auto& old_list = frequency_lists_[old_freq];
    old_list.remove(fingerprint);
    if (old_list.empty()) {
        frequency_lists_.erase(old_freq);
    }

    // Add to new frequency list
    frequency_lists_[new_freq].push_back(fingerprint);
    it->second = new_freq;
}

std::string DatabaseStatementCache::evict_one_lfu() {
    if (frequency_lists_.empty()) {
        return "";
    }

    // Find lowest frequency
    auto it = frequency_lists_.begin();
    if (it->second.empty()) {
        return "";
    }

    std::string fp = it->second.front();
    it->second.pop_front();
    if (it->second.empty()) {
        frequency_lists_.erase(it);
    }

    frequency_map_.erase(fp);

    auto cache_it = cache_.find(fp);
    if (cache_it != cache_.end()) {
        uint64_t hash = cache_it->second->fingerprint_hash();
        hash_to_fingerprint_.erase(hash);

        for (const auto& table : cache_it->second->metadata().referenced_tables) {
            auto& fps = table_to_fingerprints_[table];
            fps.erase(std::remove(fps.begin(), fps.end(), fp), fps.end());
        }

        stats_.memory_bytes -= cache_it->second->memory_usage();
        stats_.statement_count--;
        cache_.erase(cache_it);
    }

    return fp;
}

// ARC helpers (simplified)
void DatabaseStatementCache::update_arc(const std::string& fingerprint, bool hit) {
    // Simplified ARC - move to T2 on hit
    if (hit) {
        arc_t1_.remove(fingerprint);
        arc_t2_.push_front(fingerprint);
    }
}

std::string DatabaseStatementCache::evict_one_arc() {
    std::string fp;

    // Evict from T1 or T2 based on adaptation parameter
    if (!arc_t1_.empty() &&
        (arc_t1_.size() > static_cast<size_t>(arc_p_) || arc_t2_.empty())) {
        fp = arc_t1_.back();
        arc_t1_.pop_back();
        arc_b1_.push_front(fp);  // Move to ghost
    } else if (!arc_t2_.empty()) {
        fp = arc_t2_.back();
        arc_t2_.pop_back();
        arc_b2_.push_front(fp);  // Move to ghost
    }

    if (!fp.empty()) {
        auto cache_it = cache_.find(fp);
        if (cache_it != cache_.end()) {
            uint64_t hash = cache_it->second->fingerprint_hash();
            hash_to_fingerprint_.erase(hash);

            for (const auto& table : cache_it->second->metadata().referenced_tables) {
                auto& fps = table_to_fingerprints_[table];
                fps.erase(std::remove(fps.begin(), fps.end(), fp), fps.end());
            }

            stats_.memory_bytes -= cache_it->second->memory_usage();
            stats_.statement_count--;
            cache_.erase(cache_it);
        }
    }

    return fp;
}

bool DatabaseStatementCache::should_cache(const CachedStatement& stmt) const {
    // Check statement type
    switch (stmt.statement_type()) {
        case StatementType::SELECT:
            if (!config_.cache_select) return false;
            break;
        case StatementType::INSERT:
            if (!config_.cache_insert) return false;
            break;
        case StatementType::UPDATE:
            if (!config_.cache_update) return false;
            break;
        case StatementType::DELETE:
            if (!config_.cache_delete) return false;
            break;
        case StatementType::DDL:
            if (!config_.cache_ddl) return false;
            break;
        case StatementType::UTILITY:
            if (!config_.cache_utility) return false;
            break;
        default:
            break;
    }

    // Check size constraints
    if (stmt.sql().size() < config_.min_statement_size ||
        stmt.sql().size() > config_.max_statement_size) {
        return false;
    }

    return true;
}

void DatabaseStatementCache::update_statistics_on_get(bool hit, StatementType type) {
    if (hit) {
        stats_.total_hits++;
        stats_.hits_by_type[type]++;
    } else {
        stats_.total_misses++;
    }

    uint64_t total = stats_.total_hits + stats_.total_misses;
    if (total > 0) {
        stats_.hit_ratio = static_cast<double>(stats_.total_hits) / total;
    }
}

void DatabaseStatementCache::update_statistics_on_put(const CachedStatement& stmt) {
    stats_.statement_count++;
    stats_.memory_bytes += stmt.memory_usage();
    stats_.count_by_type[stmt.statement_type()]++;
}

void DatabaseStatementCache::update_statistics_on_evict() {
    // Updated in individual evict methods
}

// =============================================================================
// StatementCacheManager Implementation
// =============================================================================

StatementCacheManager& StatementCacheManager::instance() {
    static StatementCacheManager instance;
    return instance;
}

void StatementCacheManager::initialize(const StatementCacheConfig& default_config) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (initialized_) {
        return;
    }

    default_config_ = default_config;
    initialized_ = true;
}

void StatementCacheManager::shutdown() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    caches_.clear();
    initialized_ = false;
}

std::shared_ptr<DatabaseStatementCache> StatementCacheManager::get_cache(const std::string& database_name) {
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

    auto cache = std::make_shared<DatabaseStatementCache>(database_name, default_config_);
    caches_[database_name] = cache;

    return cache;
}

std::shared_ptr<DatabaseStatementCache> StatementCacheManager::get_cache(
    const std::string& database_name,
    const StatementCacheConfig& config) {

    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = caches_.find(database_name);
    if (it != caches_.end()) {
        return it->second;
    }

    auto cache = std::make_shared<DatabaseStatementCache>(database_name, config);
    caches_[database_name] = cache;

    return cache;
}

bool StatementCacheManager::remove_cache(const std::string& database_name) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    auto it = caches_.find(database_name);
    if (it == caches_.end()) {
        return false;
    }

    caches_.erase(it);
    return true;
}

bool StatementCacheManager::has_cache(const std::string& database_name) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return caches_.find(database_name) != caches_.end();
}

std::vector<std::string> StatementCacheManager::get_database_names() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(caches_.size());

    for (const auto& pair : caches_) {
        names.push_back(pair.first);
    }

    return names;
}

void StatementCacheManager::clear_all() {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (auto& pair : caches_) {
        pair.second->clear();
    }
}

uint64_t StatementCacheManager::invalidate_table(const std::string& database_name, const std::string& table_name) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;

    if (database_name.empty()) {
        // Invalidate across all databases
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

CacheStatistics StatementCacheManager::get_aggregate_statistics() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    CacheStatistics aggregate{};
    aggregate.created_at = std::chrono::system_clock::now();

    for (const auto& pair : caches_) {
        const auto& stats = pair.second->statistics();

        aggregate.statement_count += stats.statement_count;
        aggregate.memory_bytes += stats.memory_bytes;
        aggregate.total_hits += stats.total_hits;
        aggregate.total_misses += stats.total_misses;
        aggregate.eviction_count += stats.eviction_count;
        aggregate.invalidation_count += stats.invalidation_count;
        aggregate.expiration_count += stats.expiration_count;
    }

    uint64_t total = aggregate.total_hits + aggregate.total_misses;
    if (total > 0) {
        aggregate.hit_ratio = static_cast<double>(aggregate.total_hits) / total;
    }

    return aggregate;
}

uint64_t StatementCacheManager::total_memory_usage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;
    for (const auto& pair : caches_) {
        total += pair.second->memory_usage();
    }
    return total;
}

uint64_t StatementCacheManager::total_statement_count() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    uint64_t total = 0;
    for (const auto& pair : caches_) {
        total += pair.second->size();
    }
    return total;
}

// =============================================================================
// CachedStatementGuard Implementation
// =============================================================================

CachedStatementGuard::CachedStatementGuard(std::shared_ptr<CachedStatement> stmt)
    : stmt_(std::move(stmt)) {
}

CachedStatementGuard::~CachedStatementGuard() {
    // Nothing special to do on destruction
}

// =============================================================================
// Utility Functions
// =============================================================================

namespace statement_cache_utils {

std::string format_statistics(const CacheStatistics& stats) {
    std::ostringstream ss;

    ss << "Statement Cache Statistics:\n"
       << "  Statements: " << stats.statement_count << "\n"
       << "  Memory: " << (stats.memory_bytes / 1024) << " KB\n"
       << "  Hit Ratio: " << (stats.hit_ratio * 100.0) << "%\n"
       << "  Hits: " << stats.total_hits << "\n"
       << "  Misses: " << stats.total_misses << "\n"
       << "  Evictions: " << stats.eviction_count << "\n"
       << "  Invalidations: " << stats.invalidation_count << "\n"
       << "  Expirations: " << stats.expiration_count;

    return ss.str();
}

uint64_t estimate_statement_memory(std::string_view sql) {
    // Base overhead + SQL string
    return sizeof(CachedStatement) + sql.size() + 256;  // 256 for metadata
}

bool should_cache_statement(const StatementCacheConfig& config, StatementType type) {
    switch (type) {
        case StatementType::SELECT:
            return config.cache_select;
        case StatementType::INSERT:
            return config.cache_insert;
        case StatementType::UPDATE:
            return config.cache_update;
        case StatementType::DELETE:
            return config.cache_delete;
        case StatementType::DDL:
            return config.cache_ddl;
        case StatementType::UTILITY:
            return config.cache_utility;
        default:
            return true;
    }
}

}  // namespace statement_cache_utils

}  // namespace pool
}  // namespace scratchbird
