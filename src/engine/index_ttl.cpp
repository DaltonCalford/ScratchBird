#include "scratchbird/engine/index_ttl.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

namespace scratchbird::engine
{

    TTLIndex::TTLIndex(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique)
    {
        last_cleanup_ = std::chrono::system_clock::now();
    }

    void TTLIndex::create_empty()
    {
        root_page_ = 1;
        meta_page_ = 2;
        entries_.clear();
        entries_count_ = 0;
        expired_count_ = 0;
        cleanup_runs_ = 0;

        // Initialize with default configuration
        config_.expire_column = "expires_at";
        config_.interval = std::chrono::seconds(3600); // 1 hour default
        config_.auto_cleanup = true;
        config_.cleanup_frequency = std::chrono::seconds(3600);

        schedule_next_cleanup();
        save_to_disk();
    }

    bool TTLIndex::open_existing(std::uint32_t root_page)
    {
        root_page_ = root_page;
        meta_page_ = root_page + 1;
        return load_from_disk();
    }

    bool TTLIndex::configure_ttl(const TTLConfiguration& config)
    {
        config_ = config;
        return save_to_disk();
    }

    bool TTLIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        return insert_with_payload(key, row_id, "", err);
    }

    bool TTLIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                       const std::string& payload, std::string& err)
    {
        // For basic insert, use current time + configured interval as expiry
        auto expiry = std::chrono::system_clock::now() + config_.interval;
        return insert_with_ttl(key, row_id, expiry, payload, err);
    }

    bool TTLIndex::insert_with_ttl(const std::string& key, std::uint64_t row_id,
                                   const std::chrono::system_clock::time_point& expiry,
                                   const std::string& payload, std::string& err)
    {
        // Check for uniqueness if required
        if (unique_) {
            auto it =
                std::find_if(entries_.begin(), entries_.end(), [&key, this](const TTLEntry& entry) {
                    return entry.key == key && !is_expired(entry);
                });
            if (it != entries_.end()) {
                err = "Duplicate key violation: " + key;
                return false;
            }
        }

        // Create new entry
        TTLEntry entry;
        entry.key = key;
        entry.row_id = row_id;
        entry.expiry_time = expiry;
        entry.payload = payload;

        entries_.push_back(entry);
        entries_count_++;

        // Trigger cleanup if needed
        if (config_.auto_cleanup && should_run_cleanup()) {
            cleanup_expired();
        }

        return save_to_disk();
    }

    void TTLIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        out.clear();

        // Clean up expired entries first if auto-cleanup is enabled
        if (config_.auto_cleanup && should_run_cleanup()) {
            const_cast<TTLIndex*>(this)->cleanup_expired();
        }

        for (const auto& entry : entries_) {
            if (entry.key == key && !is_expired(entry)) {
                out.push_back(entry.row_id);
                if (unique_)
                    break; // Only one result for unique indexes
            }
        }
    }

    void TTLIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        out.clear();

        // Clean up expired entries first if auto-cleanup is enabled
        if (config_.auto_cleanup && should_run_cleanup()) {
            const_cast<TTLIndex*>(this)->cleanup_expired();
        }

        for (const auto& entry : entries_) {
            if (entry.key == key && !is_expired(entry)) {
                out.emplace_back(entry.row_id, entry.payload);
                if (unique_)
                    break; // Only one result for unique indexes
            }
        }
    }

    void TTLIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                bool hi_incl,
                                std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        // TTL indexes don't support efficient range queries
        // This is a linear scan implementation for completeness
        out.clear();

        if (config_.auto_cleanup && should_run_cleanup()) {
            const_cast<TTLIndex*>(this)->cleanup_expired();
        }

        for (const auto& entry : entries_) {
            if (is_expired(entry))
                continue;

            bool matches = true;

            // Check lower bound
            if (lo_incl) {
                matches = matches && (entry.key >= lo);
            } else {
                matches = matches && (entry.key > lo);
            }

            // Check upper bound
            if (hi_incl) {
                matches = matches && (entry.key <= hi);
            } else {
                matches = matches && (entry.key < hi);
            }

            if (matches) {
                out.emplace_back(entry.key, entry.row_id);
            }
        }

        // Sort results by key
        std::sort(out.begin(), out.end());
    }

    std::size_t TTLIndex::erase_equal(const std::string& key, std::string& /*err*/)
    {
        // Clean up expired entries first
        if (config_.auto_cleanup && should_run_cleanup()) {
            cleanup_expired();
        }

        std::size_t removed_count = 0;
        auto it = entries_.begin();

        while (it != entries_.end()) {
            if (it->key == key && !is_expired(*it)) {
                it = entries_.erase(it);
                removed_count++;
                entries_count_--;
                if (unique_)
                    break; // Only remove one for unique indexes
            } else {
                ++it;
            }
        }

        if (removed_count > 0) {
            save_to_disk();
        }

        return removed_count;
    }

    bool TTLIndex::validate(std::string& err) const
    {
        // Basic validation checks
        if (entries_count_ != entries_.size()) {
            err = "Entry count mismatch in TTL index";
            return false;
        }

        // Check for duplicates in unique index
        if (unique_) {
            std::vector<std::string> keys;
            for (const auto& entry : entries_) {
                if (!is_expired(entry)) {
                    if (std::find(keys.begin(), keys.end(), entry.key) != keys.end()) {
                        err = "Duplicate key found in unique TTL index: " + entry.key;
                        return false;
                    }
                    keys.push_back(entry.key);
                }
            }
        }

        return true;
    }

    void TTLIndex::cleanup_expired()
    {
        auto now = std::chrono::system_clock::now();
        std::size_t original_size = entries_.size();

        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [this](const TTLEntry& entry) { return is_expired(entry); }),
                       entries_.end());

        std::size_t removed = original_size - entries_.size();
        expired_count_ += removed;
        entries_count_ -= removed;
        cleanup_runs_++;
        last_cleanup_ = now;

        if (removed > 0) {
            save_to_disk();
        }
    }

    std::vector<TTLIndex::TTLEntry> TTLIndex::get_expired_entries() const
    {
        std::vector<TTLEntry> expired;
        for (const auto& entry : entries_) {
            if (is_expired(entry)) {
                expired.push_back(entry);
            }
        }
        return expired;
    }

    void TTLIndex::force_cleanup()
    {
        cleanup_expired();
    }

    void TTLIndex::rebuild_offline()
    {
        // For TTL index, rebuilding means cleaning up expired entries and reorganizing
        cleanup_expired();

        // Sort entries by expiry time for better access patterns
        std::sort(entries_.begin(), entries_.end(), [](const TTLEntry& a, const TTLEntry& b) {
            return a.expiry_time < b.expiry_time;
        });

        save_to_disk();
    }

    void TTLIndex::compact_index()
    {
        // Compact by removing expired entries and optimizing storage
        cleanup_expired();

        // Shrink vector capacity to fit current size
        entries_.shrink_to_fit();

        save_to_disk();
    }

    std::string TTLIndex::collect_statistics() const
    {
        update_statistics();

        std::ostringstream ss;
        ss << "TTL Index Statistics:\n";
        ss << "  Total entries: " << entries_count_ << "\n";
        ss << "  Expired entries: " << expired_count_ << "\n";
        ss << "  Cleanup runs: " << cleanup_runs_ << "\n";
        ss << "  TTL interval: " << config_.interval.count() << " seconds\n";
        ss << "  Auto cleanup: " << (config_.auto_cleanup ? "enabled" : "disabled") << "\n";
        ss << "  Cleanup frequency: " << config_.cleanup_frequency.count() << " seconds\n";
        ss << "  Last cleanup: "
           << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() -
                                                               last_cleanup_)
                  .count()
           << " seconds ago\n";

        return ss.str();
    }

    double TTLIndex::estimate_search_cost(const std::string& /*key*/) const
    {
        // TTL index requires linear scan for search
        return static_cast<double>(entries_.size()) * 0.1; // Cost per entry examination
    }

    double TTLIndex::estimate_range_cost(const std::string& /*lo*/, const std::string& /*hi*/) const
    {
        // Range queries require full table scan
        return static_cast<double>(entries_.size()) * 0.5; // Higher cost for range scan
    }

    double TTLIndex::estimate_maintenance_cost() const
    {
        // Cost includes periodic cleanup
        return static_cast<double>(entries_.size()) * 0.05; // Cleanup cost per entry
    }

    bool TTLIndex::is_expired(const TTLEntry& entry) const
    {
        return std::chrono::system_clock::now() > entry.expiry_time;
    }

    void TTLIndex::update_statistics() const
    {
        // Count active (non-expired) entries
        std::size_t active = 0;
        std::size_t expired = 0;

        for (const auto& entry : entries_) {
            if (is_expired(entry)) {
                expired++;
            } else {
                active++;
            }
        }

        // Update counts if they've changed
        entries_count_ = active;
        expired_count_ = expired;
    }

    bool TTLIndex::should_run_cleanup() const
    {
        if (!config_.auto_cleanup)
            return false;

        auto now = std::chrono::system_clock::now();
        auto time_since_cleanup = now - last_cleanup_;

        return time_since_cleanup >= config_.cleanup_frequency;
    }

    void TTLIndex::schedule_next_cleanup()
    {
        last_cleanup_ = std::chrono::system_clock::now();
    }

    bool TTLIndex::load_from_disk()
    {
        // Simplified load implementation - in real system would read from pages
        entries_.clear();
        entries_count_ = 0;
        expired_count_ = 0;
        cleanup_runs_ = 0;
        last_cleanup_ = std::chrono::system_clock::now();

        return true;
    }

    bool TTLIndex::save_to_disk()
    {
        // Simplified save implementation - in real system would write to pages
        // For now, this is a placeholder that always succeeds
        return true;
    }

    // TTL Index Scan Implementation
    bool TTLIndexScan::init(const std::string& key_condition)
    {
        reset();

        if (!index_)
            return false;

        // Collect all non-expired entries
        active_entries_.clear();

        for (const auto& entry : index_->entries_) {
            if (!index_->is_expired(entry)) {
                // Apply key condition if provided
                if (key_condition.empty() || entry.key == key_condition) {
                    active_entries_.push_back(entry);
                }
            }
        }

        pages_accessed_ = 1; // Simplified - assume one page access for scan setup
        return !active_entries_.empty();
    }

    bool TTLIndexScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        if (current_position_ >= active_entries_.size()) {
            finished_ = true;
            return false;
        }

        const auto& entry = active_entries_[current_position_++];
        row_id = entry.row_id;
        key = entry.key;
        payload = entry.payload;
        rows_scanned_++;

        return true;
    }

    void TTLIndexScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        current_position_ = 0;
        active_entries_.clear();
    }

    bool TTLIndexScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t TTLIndexScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t TTLIndexScan::pages_accessed() const
    {
        return pages_accessed_;
    }

} // namespace scratchbird::engine
