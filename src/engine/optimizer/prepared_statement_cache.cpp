// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/prepared_statement_cache.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace scratchbird::engine
{

    // ============================================================================
    // PreparedStatementKey Implementation
    // ============================================================================

    bool PreparedStatementKey::operator==(const PreparedStatementKey& other) const
    {
        return sql_text == other.sql_text && database_name == other.database_name &&
               schema_name == other.schema_name && user_name == other.user_name &&
               user_roles == other.user_roles && case_sensitive == other.case_sensitive &&
               timeout_seconds == other.timeout_seconds;
    }

    std::size_t PreparedStatementKey::hash() const
    {
        if (hash_code == 0) {
            std::size_t h1 = std::hash<std::string>{}(sql_text);
            std::size_t h2 = std::hash<std::string>{}(database_name);
            std::size_t h3 = std::hash<std::string>{}(schema_name);
            std::size_t h4 = std::hash<std::string>{}(user_name);

            // Combine role hashes
            std::size_t h5 = 0;
            for (const auto& role : user_roles) {
                h5 ^= std::hash<std::string>{}(role) + 0x9e3779b9 + (h5 << 6) + (h5 >> 2);
            }

            std::size_t h6 = std::hash<bool>{}(case_sensitive);
            std::size_t h7 = std::hash<std::uint32_t>{}(timeout_seconds);

            // Combine all hashes
            hash_code = h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5) ^ (h7 << 6);
        }
        return hash_code;
    }

    std::string PreparedStatementKey::to_string() const
    {
        std::ostringstream ss;
        ss << "PreparedStatementKey{sql='" << sql_text << "', db='" << database_name
           << "', schema='" << schema_name << "', user='" << user_name << "'}";
        return ss.str();
    }

    std::string PreparedStatementKey::normalize_sql(const std::string& sql)
    {
        if (sql.empty())
            return sql;

        // Remove leading/trailing whitespace
        std::string normalized = sql;
        normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
        normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);

        // Replace multiple whitespace with single spaces
        std::regex whitespace_regex("\\s+");
        normalized = std::regex_replace(normalized, whitespace_regex, " ");

        // Convert keywords to uppercase (basic set)
        static const std::vector<std::pair<std::string, std::string>> keywords = {
            {"select", "SELECT"}, {"from", "FROM"},     {"where", "WHERE"},   {"insert", "INSERT"},
            {"into", "INTO"},     {"values", "VALUES"}, {"update", "UPDATE"}, {"set", "SET"},
            {"delete", "DELETE"}, {"join", "JOIN"},     {"inner", "INNER"},   {"left", "LEFT"},
            {"right", "RIGHT"},   {"on", "ON"},         {"and", "AND"},       {"or", "OR"},
            {"not", "NOT"},       {"order", "ORDER"},   {"by", "BY"},         {"group", "GROUP"},
            {"having", "HAVING"}};

        for (const auto& [lower, upper] : keywords) {
            std::regex keyword_regex("\\b" + lower + "\\b", std::regex_constants::icase);
            normalized = std::regex_replace(normalized, keyword_regex, upper);
        }

        return normalized;
    }

    // ============================================================================
    // PreparedStatementStats Implementation
    // ============================================================================

    void PreparedStatementStats::update_execution_stats(std::uint64_t execution_time_us,
                                                        std::uint64_t rows_processed, bool success)
    {
        auto prev_count = execution_count.fetch_add(1);
        total_execution_time_us.fetch_add(execution_time_us);
        total_rows_processed.fetch_add(rows_processed);

        // Update average
        avg_execution_time_us.store((total_execution_time_us.load()) / (prev_count + 1));

        // Update success ratio
        if (success) {
            auto current_ratio = success_ratio.load();
            auto new_ratio = (current_ratio * prev_count + 1.0) / (prev_count + 1);
            success_ratio.store(new_ratio);
        } else {
            auto current_ratio = success_ratio.load();
            auto new_ratio = (current_ratio * prev_count) / (prev_count + 1);
            success_ratio.store(new_ratio);
        }

        // Update timestamp
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
        last_execution_time.store(now);
    }

    void PreparedStatementStats::update_preparation_stats(std::uint64_t preparation_time_us)
    {
        total_preparation_time_us.fetch_add(preparation_time_us);

        auto count = execution_count.load();
        if (count > 0) {
            avg_preparation_time_us.store(total_preparation_time_us.load() / count);
        }
    }

    void PreparedStatementStats::record_cache_hit()
    {
        cache_hit_count.fetch_add(1);
    }

    void PreparedStatementStats::reset()
    {
        execution_count.store(0);
        total_preparation_time_us.store(0);
        avg_preparation_time_us.store(0);
        total_execution_time_us.store(0);
        avg_execution_time_us.store(0);
        total_rows_processed.store(0);
        last_execution_time.store(0);
        success_ratio.store(1.0);
        cache_hit_count.store(0);
    }

    // ============================================================================
    // CachedPreparedStatement Implementation
    // ============================================================================

    CachedPreparedStatement::CachedPreparedStatement(const PreparedStatementKey& key,
                                                     std::shared_ptr<PreparedStatement> statement,
                                                     const StatementMetadata& metadata)
        : key_(key), statement_(statement), metadata_(metadata),
          creation_time_(std::chrono::system_clock::now()),
          statement_size_bytes_(calculate_statement_size())
    {
        last_access_time_.store(creation_time_);
    }

    void CachedPreparedStatement::record_access()
    {
        access_count_.fetch_add(1);
        last_access_time_.store(std::chrono::system_clock::now());
    }

    std::size_t CachedPreparedStatement::get_memory_footprint() const
    {
        std::size_t total = 0;

        // Key size
        total += key_.sql_text.size() + key_.database_name.size() + key_.schema_name.size() +
                 key_.user_name.size();
        for (const auto& role : key_.user_roles) {
            total += role.size();
        }

        // Metadata size
        total += metadata_.statement_type.size();
        for (const auto& param : metadata_.parameters) {
            total += param.name.size() + param.type_name.size() + param.default_value.size();
        }
        for (const auto& col : metadata_.columns) {
            total += col.name.size() + col.type_name.size();
        }

        // Statement size (estimated)
        total += statement_size_bytes_;

        // Fixed overhead
        total += sizeof(*this);

        return total;
    }

    std::size_t CachedPreparedStatement::calculate_statement_size() const
    {
        // Estimate prepared statement size (would need actual PreparedStatement interface)
        return key_.sql_text.size() * 2; // Rough estimate
    }

    // ============================================================================
    // PreparedStatementCacheConfig Implementation
    // ============================================================================

    bool PreparedStatementCacheConfig::is_valid() const
    {
        return max_statements > 0 && max_memory_bytes > 0 && entry_ttl_seconds > 0 &&
               eviction_threshold > 0.0 && eviction_threshold <= 1.0 &&
               cleanup_interval_seconds > 0;
    }

    std::string PreparedStatementCacheConfig::validate() const
    {
        std::vector<std::string> errors;

        if (max_statements == 0) {
            errors.push_back("max_statements must be greater than 0");
        }
        if (max_memory_bytes == 0) {
            errors.push_back("max_memory_bytes must be greater than 0");
        }
        if (entry_ttl_seconds == 0) {
            errors.push_back("entry_ttl_seconds must be greater than 0");
        }
        if (eviction_threshold <= 0.0 || eviction_threshold > 1.0) {
            errors.push_back("eviction_threshold must be between 0.0 and 1.0");
        }
        if (cleanup_interval_seconds == 0) {
            errors.push_back("cleanup_interval_seconds must be greater than 0");
        }

        if (errors.empty()) {
            return "";
        }

        std::ostringstream ss;
        ss << "PreparedStatementCacheConfig validation errors: ";
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i > 0)
                ss << ", ";
            ss << errors[i];
        }
        return ss.str();
    }

    // ============================================================================
    // PreparedStatementCache Implementation
    // ============================================================================

    PreparedStatementCache::PreparedStatementCache(const PreparedStatementCacheConfig& config)
        : config_(config)
    {
        // Initialize LRU list
        lru_head_ = std::make_shared<LRUNode>(0, nullptr);
        lru_tail_ = std::make_shared<LRUNode>(0, nullptr);
        lru_head_->next = lru_tail_;
        lru_tail_->prev = lru_head_;
    }

    PreparedStatementCache::~PreparedStatementCache()
    {
        shutdown();
    }

    bool PreparedStatementCache::initialize()
    {
        std::unique_lock<std::shared_mutex> config_lock(config_mutex_);

        if (!config_.is_valid()) {
            return false;
        }

        // Start background cleanup thread if enabled
        if (config_.cleanup_interval_seconds > 0) {
            shutdown_requested_.store(false);
            cleanup_thread_ =
                std::make_unique<std::thread>(&PreparedStatementCache::cleanup_thread_main, this);
        }

        return true;
    }

    void PreparedStatementCache::shutdown()
    {
        shutdown_requested_.store(true);

        if (cleanup_thread_ && cleanup_thread_->joinable()) {
            cleanup_thread_->join();
            cleanup_thread_.reset();
        }

        clear();
    }

    bool PreparedStatementCache::insert_statement(const PreparedStatementKey& key,
                                                  std::shared_ptr<PreparedStatement> statement,
                                                  const StatementMetadata& metadata)
    {
        if (!config_.enabled || !statement) {
            return false;
        }

        auto start = std::chrono::high_resolution_clock::now();

        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        auto key_hash = key.hash();

        // Check if statement already exists
        auto it = cache_.find(key_hash);
        if (it != cache_.end()) {
            // Update existing entry
            lru_move_to_head(lru_map_[key_hash]);
            it->second->record_access();
            return true;
        }

        // Check if we need to evict
        if (should_evict()) {
            // Evict LRU entries
            while (should_evict() && !lru_map_.empty()) {
                auto tail_node = lru_remove_tail();
                if (tail_node && tail_node->statement) {
                    cache_.erase(tail_node->key_hash);
                    lru_map_.erase(tail_node->key_hash);
                    statistics_.cache_evictions.fetch_add(1);
                }
            }
        }

        // Create cached statement
        auto cached_stmt = std::make_shared<CachedPreparedStatement>(key, statement, metadata);

        // Insert into cache
        cache_[key_hash] = cached_stmt;

        // Insert into LRU
        auto lru_node = std::make_shared<LRUNode>(key_hash, cached_stmt);
        lru_map_[key_hash] = lru_node;
        lru_add_to_head(lru_node);

        // Update statistics
        statistics_.total_statements.fetch_add(1);
        statistics_.active_statements.fetch_add(1);
        update_memory_usage();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        update_insertion_statistics(duration);

        return true;
    }

    std::shared_ptr<CachedPreparedStatement>
    PreparedStatementCache::lookup_statement(const PreparedStatementKey& key)
    {
        if (!config_.enabled) {
            update_miss_statistics();
            return nullptr;
        }

        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        auto key_hash = key.hash();
        auto it = cache_.find(key_hash);

        if (it != cache_.end() && it->second->is_valid()) {
            // Found valid entry
            lru_move_to_head(lru_map_[key_hash]);
            it->second->record_access();
            it->second->get_statistics().record_cache_hit();
            update_hit_statistics();
            return it->second;
        }

        // Not found or invalid
        update_miss_statistics();
        return nullptr;
    }

    bool PreparedStatementCache::remove_statement(const PreparedStatementKey& key)
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        auto key_hash = key.hash();
        auto it = cache_.find(key_hash);

        if (it != cache_.end()) {
            // Remove from LRU
            auto lru_it = lru_map_.find(key_hash);
            if (lru_it != lru_map_.end()) {
                lru_remove_node(lru_it->second);
                lru_map_.erase(lru_it);
            }

            // Remove from cache
            cache_.erase(it);

            // Update statistics
            statistics_.active_statements.fetch_sub(1);
            update_memory_usage();

            return true;
        }

        return false;
    }

    std::uint32_t PreparedStatementCache::evict_statements(std::uint32_t max_evictions)
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        std::uint32_t evicted = 0;

        while (evicted < max_evictions && !lru_map_.empty()) {
            auto tail_node = lru_remove_tail();
            if (tail_node && tail_node->statement) {
                cache_.erase(tail_node->key_hash);
                lru_map_.erase(tail_node->key_hash);
                statistics_.cache_evictions.fetch_add(1);
                statistics_.active_statements.fetch_sub(1);
                evicted++;
            } else {
                break;
            }
        }

        if (evicted > 0) {
            update_memory_usage();
        }

        return evicted;
    }

    void PreparedStatementCache::clear()
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        cache_.clear();
        lru_map_.clear();

        // Reset LRU list
        lru_head_->next = lru_tail_;
        lru_tail_->prev = lru_head_;

        // Update statistics
        statistics_.active_statements.store(0);
        statistics_.memory_usage_bytes.store(0);
    }

    std::uint32_t PreparedStatementCache::invalidate_statements(const std::string& pattern)
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        std::uint32_t invalidated = 0;
        std::regex pattern_regex(pattern, std::regex_constants::icase);

        for (auto& [hash, cached_stmt] : cache_) {
            if (std::regex_search(cached_stmt->get_key().sql_text, pattern_regex)) {
                cached_stmt->invalidate();
                invalidated++;
            }
        }

        statistics_.cache_invalidations.fetch_add(invalidated);
        return invalidated;
    }

    std::uint32_t
    PreparedStatementCache::invalidate_statements_by_schema(const std::string& database_name,
                                                            const std::string& schema_name)
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);

        std::uint32_t invalidated = 0;

        for (auto& [hash, cached_stmt] : cache_) {
            const auto& key = cached_stmt->get_key();
            if ((database_name.empty() || key.database_name == database_name) &&
                (schema_name.empty() || key.schema_name == schema_name)) {
                cached_stmt->invalidate();
                invalidated++;
            }
        }

        statistics_.cache_invalidations.fetch_add(invalidated);
        return invalidated;
    }

    std::uint32_t PreparedStatementCache::cleanup_expired_statements()
    {
        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex_);
        std::unique_lock<std::mutex> lru_lock(lru_mutex_);

        auto now = std::chrono::system_clock::now();
        auto ttl = std::chrono::seconds(config_.entry_ttl_seconds);

        std::uint32_t cleaned = 0;
        std::vector<std::size_t> to_remove;

        for (const auto& [hash, cached_stmt] : cache_) {
            auto age = now - cached_stmt->get_creation_time();
            if (age > ttl || !cached_stmt->is_valid()) {
                to_remove.push_back(hash);
            }
        }

        for (auto hash : to_remove) {
            auto it = cache_.find(hash);
            if (it != cache_.end()) {
                // Remove from LRU
                auto lru_it = lru_map_.find(hash);
                if (lru_it != lru_map_.end()) {
                    lru_remove_node(lru_it->second);
                    lru_map_.erase(lru_it);
                }

                cache_.erase(it);
                cleaned++;
            }
        }

        if (cleaned > 0) {
            statistics_.expired_statements.fetch_add(cleaned);
            statistics_.active_statements.fetch_sub(cleaned);
            update_memory_usage();
        }

        return cleaned;
    }

    bool PreparedStatementCache::update_config(const PreparedStatementCacheConfig& config)
    {
        if (!config.is_valid()) {
            return false;
        }

        std::unique_lock<std::shared_mutex> config_lock(config_mutex_);
        config_ = config;
        return true;
    }

    PreparedStatementCacheConfig PreparedStatementCache::get_config() const
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);
        return config_;
    }

    PreparedStatementCacheStats PreparedStatementCache::get_statistics() const
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        return statistics_;
    }

    void PreparedStatementCache::reset_statistics()
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        statistics_.reset();
    }

    std::vector<PreparedStatementKey> PreparedStatementCache::get_all_statement_keys() const
    {
        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);

        std::vector<PreparedStatementKey> keys;
        keys.reserve(cache_.size());

        for (const auto& [hash, cached_stmt] : cache_) {
            keys.push_back(cached_stmt->get_key());
        }

        return keys;
    }

    std::shared_ptr<CachedPreparedStatement>
    PreparedStatementCache::get_statement_details(const PreparedStatementKey& key) const
    {
        std::shared_lock<std::shared_mutex> cache_lock(cache_mutex_);

        auto key_hash = key.hash();
        auto it = cache_.find(key_hash);

        if (it != cache_.end()) {
            return it->second;
        }

        return nullptr;
    }

    std::uint64_t PreparedStatementCache::get_memory_usage() const
    {
        return statistics_.memory_usage_bytes.load();
    }

    double PreparedStatementCache::get_capacity_utilization() const
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);

        auto current_statements = statistics_.active_statements.load();
        return static_cast<double>(current_statements) / config_.max_statements;
    }

    void PreparedStatementCache::garbage_collect()
    {
        cleanup_expired_statements();

        // Enforce memory limits
        enforce_memory_limits();
    }

    void PreparedStatementCache::set_monitoring_enabled(bool enabled)
    {
        std::unique_lock<std::shared_mutex> config_lock(config_mutex_);
        config_.enable_statistics = enabled;
    }

    std::string PreparedStatementCache::generate_performance_report() const
    {
        auto stats = get_statistics();
        auto config = get_config();

        std::ostringstream report;
        report << "Prepared Statement Cache Performance Report\n";
        report << "==========================================\n";
        report << "Configuration:\n";
        report << "  Max Statements: " << config.max_statements << "\n";
        report << "  Max Memory: " << (config.max_memory_bytes / (1024 * 1024)) << " MB\n";
        report << "  TTL: " << config.entry_ttl_seconds << " seconds\n";
        report << "  Enabled: " << (config.enabled ? "Yes" : "No") << "\n\n";

        report << "Statistics:\n";
        report << "  Cache Hits: " << stats.cache_hits.load() << "\n";
        report << "  Cache Misses: " << stats.cache_misses.load() << "\n";
        report << "  Hit Ratio: " << (stats.get_hit_ratio() * 100.0) << "%\n";
        report << "  Total Statements: " << stats.total_statements.load() << "\n";
        report << "  Active Statements: " << stats.active_statements.load() << "\n";
        report << "  Expired Statements: " << stats.expired_statements.load() << "\n";
        report << "  Memory Usage: " << (stats.memory_usage_bytes.load() / 1024) << " KB\n";
        report << "  Peak Memory: " << (stats.peak_memory_usage_bytes.load() / 1024) << " KB\n";
        report << "  Preparation Time Saved: " << stats.preparation_time_saved_us.load() << " μs\n";

        return report.str();
    }

    bool PreparedStatementCache::export_statistics(const std::string& file_path) const
    {
        try {
            std::ofstream file(file_path);
            if (!file.is_open()) {
                return false;
            }

            file << generate_performance_report();
            return true;
        } catch (...) {
            return false;
        }
    }

    // ============================================================================
    // Private Methods
    // ============================================================================

    void PreparedStatementCache::lru_add_to_head(std::shared_ptr<LRUNode> node)
    {
        node->prev = lru_head_;
        node->next = lru_head_->next;
        lru_head_->next->prev = node;
        lru_head_->next = node;
    }

    void PreparedStatementCache::lru_remove_node(std::shared_ptr<LRUNode> node)
    {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

    void PreparedStatementCache::lru_move_to_head(std::shared_ptr<LRUNode> node)
    {
        lru_remove_node(node);
        lru_add_to_head(node);
    }

    std::shared_ptr<PreparedStatementCache::LRUNode> PreparedStatementCache::lru_remove_tail()
    {
        auto last_node = lru_tail_->prev;
        if (last_node == lru_head_) {
            return nullptr; // List is empty
        }
        lru_remove_node(last_node);
        return last_node;
    }

    bool PreparedStatementCache::should_evict() const
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);

        auto current_statements = statistics_.active_statements.load();
        auto memory_usage = statistics_.memory_usage_bytes.load();

        return (current_statements >= config_.max_statements) ||
               (memory_usage >= config_.max_memory_bytes) ||
               (static_cast<double>(current_statements) / config_.max_statements >=
                config_.eviction_threshold);
    }

    std::vector<std::shared_ptr<CachedPreparedStatement>>
    PreparedStatementCache::select_eviction_candidates() const
    {
        // For now, LRU handles eviction. Could be extended for more sophisticated policies.
        return {};
    }

    void PreparedStatementCache::update_memory_usage()
    {
        std::uint64_t total_memory = 0;

        for (const auto& [hash, cached_stmt] : cache_) {
            total_memory += cached_stmt->get_memory_footprint();
        }

        statistics_.memory_usage_bytes.store(total_memory);

        // Update peak if necessary
        auto current_peak = statistics_.peak_memory_usage_bytes.load();
        if (total_memory > current_peak) {
            statistics_.peak_memory_usage_bytes.store(total_memory);
        }
    }

    void PreparedStatementCache::enforce_memory_limits()
    {
        std::shared_lock<std::shared_mutex> config_lock(config_mutex_);

        while (statistics_.memory_usage_bytes.load() > config_.max_memory_bytes) {
            if (evict_statements(1) == 0) {
                break; // No more statements to evict
            }
        }
    }

    void PreparedStatementCache::cleanup_thread_main()
    {
        while (!shutdown_requested_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(config_.cleanup_interval_seconds));

            if (shutdown_requested_.load()) {
                break;
            }

            // Perform cleanup tasks
            cleanup_expired_statements();
            garbage_collect();
        }
    }

    void PreparedStatementCache::update_hit_statistics()
    {
        statistics_.cache_hits.fetch_add(1);
    }

    void PreparedStatementCache::update_miss_statistics()
    {
        statistics_.cache_misses.fetch_add(1);
    }

    void PreparedStatementCache::update_eviction_statistics()
    {
        statistics_.cache_evictions.fetch_add(1);
    }

    void PreparedStatementCache::update_insertion_statistics(std::chrono::microseconds duration)
    {
        statistics_.total_insertion_time_us.fetch_add(duration.count());
    }

} // namespace scratchbird::engine
