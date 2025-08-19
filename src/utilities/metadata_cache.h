#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <condition_variable>
#include <set>
#include <shared_mutex>

namespace SBEnhanced {
    
    // Metadata Object Types
    enum class MetadataType {
        TABLE,
        VIEW,
        PROCEDURE,
        FUNCTION,
        TRIGGER,
        DOMAIN,
        EXCEPTION,
        GENERATOR,
        ROLE,
        INDEX,
        CONSTRAINT,
        FIELD,
        PARAMETER,
        VARIABLE,
        COLLATION,
        CHARACTER_SET,
        FILTER,
        PACKAGE,
        SCHEMA,
        USER,
        GRANT,
        DATABASE_LINK,
        UNKNOWN
    };
    
    // Cache Entry
    struct CacheEntry {
        std::string key;
        std::string value;
        MetadataType type;
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point last_accessed_time;
        std::chrono::steady_clock::time_point last_modified_time;
        std::atomic<uint64_t> access_count{0};
        std::atomic<uint64_t> hit_count{0};
        std::atomic<uint64_t> miss_count{0};
        std::atomic<bool> is_valid{true};
        std::atomic<bool> is_dirty{false};
        std::atomic<bool> is_loading{false};
        std::size_t size_bytes = 0;
        uint32_t version = 1;
        std::string checksum;
        std::vector<std::string> dependencies;
        std::vector<std::string> dependents;
        std::map<std::string, std::string> attributes;
        std::function<std::string()> loader_function;
        std::function<bool(const std::string&)> validator_function;
    };
    
    // Cache Configuration
    struct CacheConfig {
        std::size_t max_size_bytes = 100 * 1024 * 1024; // 100MB
        std::size_t max_entries = 10000;
        std::chrono::seconds default_ttl{3600}; // 1 hour
        std::chrono::seconds cleanup_interval{300}; // 5 minutes
        std::chrono::seconds refresh_interval{1800}; // 30 minutes
        double eviction_threshold = 0.8; // Start evicting at 80% full
        bool enable_compression = true;
        bool enable_persistence = false;
        bool enable_clustering = false;
        bool enable_statistics = true;
        bool enable_background_refresh = true;
        bool enable_dependency_tracking = true;
        std::string persistence_file;
        std::string compression_algorithm = "lz4";
        std::map<MetadataType, std::chrono::seconds> type_specific_ttl;
        std::map<MetadataType, uint32_t> type_specific_priority;
    };
    
    // Cache Statistics
    struct CacheStats {
        std::atomic<uint64_t> total_requests{0};
        std::atomic<uint64_t> cache_hits{0};
        std::atomic<uint64_t> cache_misses{0};
        std::atomic<uint64_t> cache_evictions{0};
        std::atomic<uint64_t> cache_expirations{0};
        std::atomic<uint64_t> cache_invalidations{0};
        std::atomic<uint64_t> cache_loads{0};
        std::atomic<uint64_t> cache_stores{0};
        std::atomic<uint64_t> cache_removes{0};
        std::atomic<uint64_t> cache_clears{0};
        std::atomic<uint64_t> entries_count{0};
        std::atomic<uint64_t> memory_usage{0};
        std::atomic<uint64_t> peak_memory_usage{0};
        std::atomic<uint64_t> compressed_size{0};
        std::atomic<uint64_t> uncompressed_size{0};
        std::atomic<double> hit_ratio{0.0};
        std::atomic<double> compression_ratio{0.0};
        std::atomic<std::chrono::microseconds> average_load_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> average_access_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> total_load_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> total_access_time{std::chrono::microseconds::zero()};
        std::map<MetadataType, std::atomic<uint64_t>> type_counts;
        std::map<MetadataType, std::atomic<uint64_t>> type_hits;
        std::map<MetadataType, std::atomic<uint64_t>> type_misses;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_reset_time;
    };
    
    // Cache Event
    enum class CacheEvent {
        HIT,
        MISS,
        LOAD,
        STORE,
        EVICT,
        EXPIRE,
        INVALIDATE,
        CLEAR
    };
    
    // Cache Listener
    using CacheListener = std::function<void(CacheEvent, const std::string&, MetadataType)>;
    
    // Eviction Policy
    enum class EvictionPolicy {
        LRU,     // Least Recently Used
        LFU,     // Least Frequently Used
        FIFO,    // First In, First Out
        RANDOM,  // Random eviction
        TTL,     // Time To Live based
        SIZE,    // Largest entries first
        PRIORITY // Priority based
    };
    
    // Cache Query
    struct CacheQuery {
        std::string pattern;
        MetadataType type = MetadataType::UNKNOWN;
        std::string schema_name;
        std::chrono::steady_clock::time_point min_created_time;
        std::chrono::steady_clock::time_point max_created_time;
        std::chrono::steady_clock::time_point min_accessed_time;
        std::chrono::steady_clock::time_point max_accessed_time;
        uint64_t min_access_count = 0;
        uint64_t max_access_count = UINT64_MAX;
        bool include_invalid = false;
        bool include_dirty = false;
        bool include_loading = false;
        std::map<std::string, std::string> attribute_filters;
        std::function<bool(const CacheEntry&)> custom_filter;
    };
    
} // namespace SBEnhanced

// Metadata Cache Class
class MetadataCache {
private:
    // Cache storage
    std::unordered_map<std::string, std::unique_ptr<SBEnhanced::CacheEntry>> cache_map;
    mutable std::shared_mutex cache_mutex;
    
    // Type-specific caches
    std::map<SBEnhanced::MetadataType, std::unordered_map<std::string, std::string>> type_caches;
    mutable std::shared_mutex type_mutex;
    
    // Access order tracking (for LRU)
    std::list<std::string> access_order;
    std::unordered_map<std::string, std::list<std::string>::iterator> access_map;
    mutable std::mutex access_mutex;
    
    // Frequency tracking (for LFU)
    std::map<uint64_t, std::set<std::string>> frequency_buckets;
    std::unordered_map<std::string, uint64_t> frequency_map;
    mutable std::mutex frequency_mutex;
    
    // Configuration
    SBEnhanced::CacheConfig config;
    
    // Statistics
    mutable SBEnhanced::CacheStats stats;
    
    // Background maintenance
    std::thread maintenance_thread;
    std::atomic<bool> maintenance_running{false};
    std::condition_variable maintenance_condition;
    std::mutex maintenance_mutex;
    
    // Event listeners
    std::vector<SBEnhanced::CacheListener> listeners;
    mutable std::mutex listeners_mutex;
    
    // Dependency tracking
    std::unordered_map<std::string, std::set<std::string>> dependencies;
    std::unordered_map<std::string, std::set<std::string>> dependents;
    mutable std::shared_mutex dependency_mutex;
    
    // Background refresh
    std::thread refresh_thread;
    std::atomic<bool> refresh_running{false};
    std::queue<std::string> refresh_queue;
    std::condition_variable refresh_condition;
    std::mutex refresh_mutex;
    
    // Compression
    std::function<std::string(const std::string&)> compress_function;
    std::function<std::string(const std::string&)> decompress_function;
    
    // Persistence
    std::string persistence_file;
    std::atomic<bool> persistence_dirty{false};
    std::thread persistence_thread;
    std::atomic<bool> persistence_running{false};
    
    // Error handling
    mutable std::vector<std::string> error_log;
    mutable std::mutex error_mutex;
    std::atomic<uint64_t> error_count{0};
    
public:
    MetadataCache();
    ~MetadataCache();
    
    // Initialization
    bool initialize(const SBEnhanced::CacheConfig& config);
    bool shutdown();
    
    // Basic cache operations
    bool get(const std::string& key, std::string& value);
    bool get(const std::string& key, std::string& value, SBEnhanced::MetadataType& type);
    bool put(const std::string& key, const std::string& value, SBEnhanced::MetadataType type);
    bool put(const std::string& key, const std::string& value, SBEnhanced::MetadataType type,
             const std::chrono::seconds& ttl);
    bool remove(const std::string& key);
    bool contains(const std::string& key) const;
    bool clear();
    
    // Advanced cache operations
    bool putIfAbsent(const std::string& key, const std::string& value, SBEnhanced::MetadataType type);
    bool replace(const std::string& key, const std::string& value);
    bool replace(const std::string& key, const std::string& old_value, const std::string& new_value);
    bool getAndPut(const std::string& key, const std::string& value, std::string& old_value);
    bool getAndRemove(const std::string& key, std::string& value);
    
    // Batch operations
    bool putAll(const std::map<std::string, std::string>& entries, SBEnhanced::MetadataType type);
    bool getAll(const std::vector<std::string>& keys, std::map<std::string, std::string>& values);
    bool removeAll(const std::vector<std::string>& keys);
    
    // Type-specific operations
    bool getByType(SBEnhanced::MetadataType type, std::map<std::string, std::string>& entries);
    bool removeByType(SBEnhanced::MetadataType type);
    bool clearByType(SBEnhanced::MetadataType type);
    std::vector<std::string> getKeysByType(SBEnhanced::MetadataType type) const;
    
    // Pattern matching
    std::vector<std::string> getKeysMatching(const std::string& pattern) const;
    bool getMatching(const std::string& pattern, std::map<std::string, std::string>& entries);
    bool removeMatching(const std::string& pattern);
    
    // Query operations
    std::vector<std::string> query(const SBEnhanced::CacheQuery& query) const;
    bool queryEntries(const SBEnhanced::CacheQuery& query, 
                     std::vector<std::shared_ptr<SBEnhanced::CacheEntry>>& entries) const;
    
    // Lazy loading
    bool getOrLoad(const std::string& key, std::string& value, 
                   std::function<std::string()> loader);
    bool getOrLoad(const std::string& key, std::string& value, SBEnhanced::MetadataType type,
                   std::function<std::string()> loader);
    bool refresh(const std::string& key);
    bool refreshAll();
    bool refreshByType(SBEnhanced::MetadataType type);
    
    // Dependency management
    bool addDependency(const std::string& key, const std::string& depends_on);
    bool removeDependency(const std::string& key, const std::string& depends_on);
    bool invalidateDependents(const std::string& key);
    std::vector<std::string> getDependencies(const std::string& key) const;
    std::vector<std::string> getDependents(const std::string& key) const;
    
    // Cache validation
    bool validate(const std::string& key);
    bool validateAll();
    bool isValid(const std::string& key) const;
    bool isDirty(const std::string& key) const;
    bool markDirty(const std::string& key);
    bool markClean(const std::string& key);
    
    // Cache maintenance
    bool evict(uint64_t count = 1);
    bool expire();
    bool compact();
    bool optimize();
    
    // Statistics
    SBEnhanced::CacheStats getStatistics() const;
    bool resetStatistics();
    double getHitRatio() const;
    double getCompressionRatio() const;
    std::size_t getSize() const;
    std::size_t getMemoryUsage() const;
    
    // Configuration
    SBEnhanced::CacheConfig getConfiguration() const;
    bool updateConfiguration(const SBEnhanced::CacheConfig& config);
    
    // Event handling
    bool addListener(const SBEnhanced::CacheListener& listener);
    bool removeListener(const SBEnhanced::CacheListener& listener);
    void notifyListeners(SBEnhanced::CacheEvent event, const std::string& key, 
                        SBEnhanced::MetadataType type);
    
    // Persistence
    bool save();
    bool load();
    bool saveTo(const std::string& filename);
    bool loadFrom(const std::string& filename);
    
    // Error handling
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    uint64_t getErrorCount() const;
    std::string getLastError() const;
    
    // Utility methods
    bool isInitialized() const;
    std::vector<std::string> getAllKeys() const;
    std::vector<SBEnhanced::MetadataType> getAllTypes() const;
    std::string toString() const;
    
private:
    // Internal cache management
    std::string generateKey(const std::string& base_key, SBEnhanced::MetadataType type) const;
    bool evictLRU(uint64_t count);
    bool evictLFU(uint64_t count);
    bool evictFIFO(uint64_t count);
    bool evictRandom(uint64_t count);
    bool evictByTTL(uint64_t count);
    bool evictBySize(uint64_t count);
    bool evictByPriority(uint64_t count);
    
    // Access tracking
    void updateAccessOrder(const std::string& key);
    void updateAccessFrequency(const std::string& key);
    
    // Background maintenance
    void maintenanceLoop();
    void refreshLoop();
    void persistenceLoop();
    
    // Compression helpers
    std::string compressValue(const std::string& value);
    std::string decompressValue(const std::string& compressed_value);
    
    // Persistence helpers
    bool serializeCache(std::ostream& output);
    bool deserializeCache(std::istream& input);
    
    // Error handling helpers
    void logError(const std::string& operation, const std::string& error) const;
    
    // Statistics helpers
    void updateStats(const std::string& operation, std::chrono::microseconds duration) const;
    void incrementCounter(const std::string& counter_name) const;
    
    // Utility helpers
    bool isExpired(const SBEnhanced::CacheEntry* entry) const;
    bool shouldEvict() const;
    uint32_t getTypePriority(SBEnhanced::MetadataType type) const;
    std::chrono::seconds getTypeTTL(SBEnhanced::MetadataType type) const;
    std::string formatCacheEntry(const SBEnhanced::CacheEntry* entry) const;
    SBEnhanced::MetadataType stringToMetadataType(const std::string& type_str) const;
    std::string metadataTypeToString(SBEnhanced::MetadataType type) const;
};