#ifndef SCRATCHBIRD_ENGINE_INDEX_TTL_H
#define SCRATCHBIRD_ENGINE_INDEX_TTL_H

#include "scratchbird/engine/index_family.h"
#include "scratchbird/engine/file.h"

#include <chrono>
#include <string>
#include <vector>
#include <memory>

namespace scratchbird::engine
{

// Forward declaration
class TTLIndexScan;

/**
 * TTL (Time-To-Live) Index Family
 * 
 * Automatically expires entries based on timestamp columns and configurable intervals.
 * Useful for session management, cache expiration, and data lifecycle management.
 */
class TTLIndex : public IndexFamily
{
    friend class TTLIndexScan;
    
public:
    struct TTLEntry {
        std::string key;
        std::uint64_t row_id;
        std::chrono::system_clock::time_point expiry_time;
        std::string payload;
    };

    struct TTLConfiguration {
        std::string expire_column;    // Column name containing expiry timestamp
        std::chrono::seconds interval; // TTL interval
        bool auto_cleanup = true;     // Automatic cleanup of expired entries
        std::chrono::seconds cleanup_frequency{3600}; // Cleanup every hour
    };

private:
    FileMap fmap_;
    std::uint32_t page_size_;
    bool unique_;
    TTLConfiguration config_;
    
    // Internal state
    std::uint32_t root_page_{0};
    std::uint32_t meta_page_{0};
    std::vector<TTLEntry> entries_;
    std::chrono::system_clock::time_point last_cleanup_;
    
    // Statistics
    mutable std::uint64_t entries_count_{0};
    mutable std::uint64_t expired_count_{0};
    mutable std::uint64_t cleanup_runs_{0};

public:
    explicit TTLIndex(FileMap fmap, std::uint32_t page_size, bool unique);
    ~TTLIndex() override = default;

    // Core IndexFamily interface
    void create_empty() override;
    bool open_existing(std::uint32_t root_page) override;

    bool insert(const std::string& key, std::uint64_t row_id, std::string& err) override;
    bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                            const std::string& payload, std::string& err) override;
    
    bool insert_with_ttl(const std::string& key, std::uint64_t row_id, 
                        const std::chrono::system_clock::time_point& expiry, 
                        const std::string& payload, std::string& err);

    void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const override;
    void search_equal_with_payload(const std::string& key,
                                  std::vector<std::pair<std::uint64_t, std::string>>& out) const override;
    void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                     std::vector<std::pair<std::string, std::uint64_t>>& out) const override;

    std::size_t erase_equal(const std::string& key, std::string& err) override;
    bool validate(std::string& err) const override;

    // TTL-specific operations
    bool configure_ttl(const TTLConfiguration& config);
    void cleanup_expired();
    std::vector<TTLEntry> get_expired_entries() const;
    void force_cleanup();

    // Index maintenance operations
    void rebuild_offline() override;
    void compact_index() override;
    
    // Statistics and monitoring
    std::string collect_statistics() const override;
    double estimate_search_cost(const std::string& key) const override;
    double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
    double estimate_maintenance_cost() const;

    // Index family identification
    IndexMethod get_method() const override { return IndexMethod::TTL; }
    std::uint32_t root_page() const override { return root_page_; }

private:
    bool is_expired(const TTLEntry& entry) const;
    void update_statistics() const;
    bool load_from_disk();
    bool save_to_disk();
    
    // Cleanup scheduling
    bool should_run_cleanup() const;
    void schedule_next_cleanup();
};

/**
 * TTL Index Scan for iterating through non-expired entries
 */
class TTLIndexScan
{
private:
    const TTLIndex* index_;
    std::vector<TTLIndex::TTLEntry> active_entries_;
    std::size_t current_position_{0};
    
    // Scan statistics
    mutable bool finished_{false};
    mutable std::uint64_t rows_scanned_{0};
    mutable std::uint64_t pages_accessed_{0};

public:
    explicit TTLIndexScan(const TTLIndex* index) : index_(index) {}

    bool init(const std::string& key_condition = "");
    bool next(std::uint64_t& row_id, std::string& key, std::string& payload);
    void reset();

    // Scan state
    bool is_finished() const;
    std::uint64_t rows_scanned() const;
    std::uint64_t pages_accessed() const;
};

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_TTL_H