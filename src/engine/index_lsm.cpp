#include "scratchbird/engine/index_lsm.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace scratchbird::engine
{

    // MemTable implementation
    MemTable::MemTable(std::size_t max_size) : max_size_(max_size)
    {
        entries_.reserve(max_size / 64); // Rough estimate of entries
    }

    bool MemTable::insert(const std::string& key, std::uint64_t row_id, const std::string& payload)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (current_size_ >= max_size_) {
            return false; // MemTable is full
        }

        // Check for existing key
        auto it = std::find_if(entries_.begin(), entries_.end(),
                               [&key](const Entry& e) { return e.key == key; });

        if (it != entries_.end()) {
            // Update existing entry
            it->row_id = row_id;
            it->payload = payload;
            it->deleted = false;
        } else {
            // Add new entry
            entries_.push_back({key, row_id, payload, false});
            current_size_ += key.size() + payload.size() + sizeof(std::uint64_t);
        }

        // Keep entries sorted for efficient range queries
        std::sort(entries_.begin(), entries_.end(),
                  [](const Entry& a, const Entry& b) { return a.key < b.key; });

        return true;
    }

    bool MemTable::search(const std::string& key, std::vector<std::uint64_t>& row_ids) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = std::lower_bound(entries_.begin(), entries_.end(), key,
                                   [](const Entry& e, const std::string& k) { return e.key < k; });

        if (it != entries_.end() && it->key == key && !it->deleted) {
            row_ids.push_back(it->row_id);
            return true;
        }

        return false;
    }

    void MemTable::search_range(const std::string& lo, const std::string& hi,
                                std::vector<std::pair<std::string, std::uint64_t>>& results) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto start =
            std::lower_bound(entries_.begin(), entries_.end(), lo,
                             [](const Entry& e, const std::string& k) { return e.key < k; });

        auto end = std::upper_bound(entries_.begin(), entries_.end(), hi,
                                    [](const std::string& k, const Entry& e) { return k < e.key; });

        for (auto it = start; it != end; ++it) {
            if (!it->deleted) {
                results.emplace_back(it->key, it->row_id);
            }
        }
    }

    bool MemTable::is_full() const
    {
        return current_size_ >= max_size_;
    }

    std::size_t MemTable::size() const
    {
        return current_size_;
    }

    void MemTable::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
        current_size_ = 0;
    }

    MemTable::Iterator MemTable::begin() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (entries_.empty()) {
            return {"", 0, "", false};
        }
        return {entries_[0].key, entries_[0].row_id, entries_[0].payload, true};
    }

    MemTable::Iterator MemTable::next(const Iterator& it) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        auto entry_it = std::find_if(entries_.begin(), entries_.end(),
                                     [&it](const Entry& e) { return e.key == it.key; });

        if (entry_it == entries_.end() || ++entry_it == entries_.end()) {
            return {"", 0, "", false};
        }

        return {entry_it->key, entry_it->row_id, entry_it->payload, true};
    }

    // SSTable implementation
    SSTable::SSTable(FileMap& fmap, std::uint32_t sstable_id) : fmap_(fmap), sstable_id_(sstable_id)
    {
        info_.sstable_id = sstable_id;
        info_.level = 0;
        info_.file_size = 0;
        info_.key_count = 0;
        info_.creation_time = std::chrono::duration_cast<std::chrono::seconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
    }

    bool SSTable::create_from_memtable(const MemTable& memtable)
    {
        // Simplified SSTable creation - would use proper file format in production
        auto it = memtable.begin();
        std::uint64_t entries_written = 0;

        // Simple Bloom filter build: fixed-size 8192 bits, 3 hash functions
        const std::size_t bloom_bits = 8192;
        std::vector<std::uint8_t> bloom((bloom_bits + 7) / 8, 0);

        auto bloom_set = [&](const std::string& k) {
            auto h1 = std::hash<std::string>{}(k);
            std::uint64_t h2 = 1469598103934665603ull; // FNV-like
            for (unsigned char c : k)
                h2 = (h2 ^ c) * 1099511628211ull;
            auto h3 = static_cast<std::uint64_t>(std::accumulate(k.begin(), k.end(), 0u));
            auto set_bit = [&](std::uint64_t h) {
                std::size_t idx = static_cast<std::size_t>(h % bloom_bits);
                bloom[idx / 8] |= static_cast<std::uint8_t>(1u << (idx % 8));
            };
            set_bit(h1);
            set_bit(h2);
            set_bit(h3);
        };

        while (it.valid) {
            if (write_entry(it.key, it.row_id, it.payload)) {
                entries_written++;
                if (entries_written == 1) {
                    info_.min_key = it.key;
                }
                info_.max_key = it.key;
                bloom_set(it.key);
            }
            it = memtable.next(it);
        }

        info_.key_count = entries_written;
        info_.bloom_bits = std::move(bloom);
        return entries_written > 0;
    }

    bool SSTable::search(const std::string& key, std::vector<std::uint64_t>& row_ids) const
    {
        // Key range pre-check
        if (key < info_.min_key || key > info_.max_key) {
            return false;
        }

        // Bloom filter pre-check (best-effort)
        if (!info_.bloom_bits.empty()) {
            auto bits = info_.bloom_bits.size() * 8;
            auto h1 = std::hash<std::string>{}(key);
            std::uint64_t h2 = 1469598103934665603ull;
            for (unsigned char c : key)
                h2 = (h2 ^ c) * 1099511628211ull;
            auto h3 = static_cast<std::uint64_t>(std::accumulate(key.begin(), key.end(), 0u));
            auto test_bit = [&](std::uint64_t h) {
                std::size_t idx = static_cast<std::size_t>(h % bits);
                return (info_.bloom_bits[idx / 8] >> (idx % 8)) & 1u;
            };
            if (!(test_bit(h1) && test_bit(h2) && test_bit(h3))) {
                return false; // Definitely not present
            }
        }

        // Simulate SSTable search by iterating through entries
        // In a real implementation, this would use binary search on disk blocks
        for (auto it = begin(); it.valid; it = next(it)) {
            if (it.key == key) {
                row_ids.push_back(it.row_id);
                return true;
            }
            if (it.key > key) {
                break; // Keys are sorted, so we can stop here
            }
        }

        return !row_ids.empty();
    }

    void SSTable::search_range(const std::string& lo, const std::string& hi,
                               std::vector<std::pair<std::string, std::uint64_t>>& results) const
    {
        // Simplified range search implementation
        if (hi < info_.min_key || lo > info_.max_key) {
            return; // No overlap with this SSTable
        }

        // Iterate through SSTable entries in the given range
        for (auto it = begin(); it.valid; it = next(it)) {
            if (it.key >= lo && it.key <= hi) {
                results.emplace_back(it.key, it.row_id);
            }
            if (it.key > hi) {
                break; // Keys are sorted, so we can stop here
            }
        }
    }

    SSTableInfo SSTable::get_info() const
    {
        return info_;
    }

    std::uint64_t SSTable::get_file_size() const
    {
        return info_.file_size;
    }

    SSTable::Iterator SSTable::begin() const
    {
        // Start iteration from the beginning of the SSTable
        Iterator it;
        it.valid = false;

        // In a real implementation, this would read from disk
        // For now, return the first valid iterator position
        if (info_.key_count > 0) {
            // Simulate reading first entry - in real implementation would read from file
            it.key = info_.min_key;
            it.row_id = 1; // Placeholder row_id
            it.payload = "";
            it.valid = true;
        }

        return it;
    }

    SSTable::Iterator SSTable::next(const Iterator& it) const
    {
        // Advance to the next entry in the SSTable
        Iterator next_it;
        next_it.valid = false;

        if (!it.valid) {
            return next_it; // Invalid iterator
        }

        // In a real implementation, this would read the next entry from disk
        // For now, implement a simplified iteration
        if (it.key < info_.max_key) {
            // Simulate advancing to next entry
            next_it.key = it.key + "_next"; // Simple progression for testing
            next_it.row_id = it.row_id + 1;
            next_it.payload = it.payload;
            next_it.valid = true;

            // Check if we've gone beyond the max key
            if (next_it.key > info_.max_key) {
                next_it.valid = false;
            }
        }

        return next_it;
    }

    bool SSTable::write_entry(const std::string& key, std::uint64_t row_id,
                              const std::string& payload)
    {
        // Simplified write - would use proper file format
        info_.file_size += key.size() + payload.size() + sizeof(std::uint64_t);
        return true;
    }

    bool SSTable::read_entry(std::uint32_t offset, std::string& key, std::uint64_t& row_id,
                             std::string& payload) const
    {
        // Simplified read - would use proper file format
        return false;
    }

    // CompactionManager implementation
    CompactionManager::CompactionManager(FileMap& fmap, CompactionStrategy strategy)
        : fmap_(fmap), strategy_(strategy)
    {
    }

    void CompactionManager::add_sstable(std::uint32_t sstable_id, const SSTableInfo& info)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sstables_.push_back(info);
    }

    bool CompactionManager::needs_compaction() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (strategy_ == CompactionStrategy::SizeTiered) {
            // Size-tiered: compact when we have too many SSTables of similar size
            std::map<std::uint32_t, std::uint32_t> level_counts;
            for (const auto& ss : sstables_) {
                level_counts[ss.level]++;
            }

            for (const auto& [level, count] : level_counts) {
                if (count >= 4) { // Threshold for compaction
                    return true;
                }
            }
        } else if (strategy_ == CompactionStrategy::Leveled) {
            // Leveled: compact when level exceeds capacity
            std::map<std::uint32_t, std::uint64_t> level_sizes;
            for (const auto& ss : sstables_) {
                level_sizes[ss.level] += ss.file_size;
            }

            const std::uint64_t base_size = 10 * 1024 * 1024; // 10MB
            for (const auto& [level, size] : level_sizes) {
                std::uint64_t capacity = base_size * (1ULL << (level * 3)); // 10x per level
                if (size > capacity) {
                    return true;
                }
            }
        }

        return false;
    }

    void CompactionManager::trigger_compaction()
    {
        if (strategy_ == CompactionStrategy::SizeTiered) {
            size_tiered_compaction();
        } else {
            leveled_compaction();
        }
    }

    std::vector<SSTableInfo> CompactionManager::get_sstables_for_level(std::uint32_t level) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<SSTableInfo> result;
        for (const auto& ss : sstables_) {
            if (ss.level == level) {
                result.push_back(ss);
            }
        }

        return result;
    }

    void CompactionManager::compact_level(std::uint32_t level)
    {
        auto sstables_to_compact = get_sstables_for_level(level);
        if (sstables_to_compact.size() < 2) {
            return; // Need at least 2 SSTables to compact
        }

        std::vector<std::uint32_t> sstable_ids;
        for (const auto& ss : sstables_to_compact) {
            sstable_ids.push_back(ss.sstable_id);
        }

        merge_sstables(sstable_ids, level + 1);
    }

    void CompactionManager::size_tiered_compaction()
    {
        // Group SSTables by size tier and compact within tiers
        std::map<std::uint32_t, std::vector<std::uint32_t>> size_tiers;

        for (const auto& ss : sstables_) {
            std::uint32_t tier =
                static_cast<std::uint32_t>(std::log2(ss.file_size / (1024 * 1024)) + 1);
            size_tiers[tier].push_back(ss.sstable_id);
        }

        // Parallelize merges per tier (simple thread-per-merge demo)
        std::vector<std::thread> workers;
        for (auto& [tier, sstable_ids] : size_tiers) {
            if (sstable_ids.size() >= 4) {
                // Copy IDs for thread safety
                auto ids = sstable_ids;
                workers.emplace_back([this, ids]() {
                    merge_sstables(ids, 0);
                });
            }
        }
        for (auto& t : workers) {
            if (t.joinable())
                t.join();
        }
    }

    void CompactionManager::leveled_compaction()
    {
        // Compact level by level
        for (std::uint32_t level = 0; level < 7; ++level) { // Max 7 levels
            if (get_sstables_for_level(level).size() > (level == 0 ? 4 : 10)) {
                compact_level(level);
                break; // Compact one level at a time
            }
        }
    }

    std::uint32_t CompactionManager::merge_sstables(const std::vector<std::uint32_t>& sstable_ids,
                                                    std::uint32_t target_level)
    {
        if (sstable_ids.empty()) {
            return 0;
        }

        // Generate new SSTable ID
        std::uint32_t new_sstable_id = sstable_ids[0] + 10000;

        // Perform merge sort of SSTables using pointers to avoid copy issues
        std::vector<std::unique_ptr<SSTable>> sstables_to_merge;
        std::vector<SSTable::Iterator> iterators;

        // Initialize iterators for each SSTable
        for (std::uint32_t id : sstable_ids) {
            auto sstable = std::make_unique<SSTable>(fmap_, id);
            auto it = sstable->begin();
            if (it.valid) {
                iterators.push_back(it);
            }
            sstables_to_merge.push_back(std::move(sstable));
        }

        // Create new merged SSTable
        SSTable merged_sstable(fmap_, new_sstable_id);
        std::uint64_t total_file_size = 0;
        std::uint64_t total_key_count = 0;
        std::string min_key, max_key;
        bool first_entry = true;

        // Merge sort entries from all SSTables
        while (!iterators.empty()) {
            // Find iterator with smallest key
            auto min_it =
                std::min_element(iterators.begin(), iterators.end(),
                                 [](const SSTable::Iterator& a, const SSTable::Iterator& b) {
                                     return a.key < b.key;
                                 });

            // Process the entry with smallest key
            if (first_entry || min_it->key < min_key) {
                min_key = min_it->key;
                first_entry = false;
            }
            if (min_it->key > max_key) {
                max_key = min_it->key;
            }

            // Write entry to merged SSTable
            merged_sstable.write_entry(min_it->key, min_it->row_id, min_it->payload);
            total_key_count++;

            // Advance the iterator
            std::size_t idx = std::distance(iterators.begin(), min_it);
            auto next_it = sstables_to_merge[idx]->next(*min_it);

            if (next_it.valid) {
                *min_it = next_it;
            } else {
                // Remove exhausted iterator and SSTable
                iterators.erase(min_it);
                sstables_to_merge.erase(sstables_to_merge.begin() + idx);
            }
        }

        // Remove old SSTables from tracking
        std::lock_guard<std::mutex> lock(mutex_);
        sstables_.erase(std::remove_if(sstables_.begin(), sstables_.end(),
                                       [&sstable_ids](const SSTableInfo& ss) {
                                           return std::find(sstable_ids.begin(), sstable_ids.end(),
                                                            ss.sstable_id) != sstable_ids.end();
                                       }),
                        sstables_.end());

        // Add new merged SSTable
        SSTableInfo new_info;
        new_info.sstable_id = new_sstable_id;
        new_info.level = target_level;
        new_info.file_size = total_file_size;
        new_info.key_count = total_key_count;
        new_info.min_key = min_key;
        new_info.max_key = max_key;
        new_info.creation_time = std::chrono::duration_cast<std::chrono::seconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();

        sstables_.push_back(new_info);

        return new_sstable_id;
    }

    // LSMTreeIndex implementation
    LSMTreeIndex::LSMTreeIndex(FileMap&& fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique), meta_page_(0),
          compaction_strategy_(CompactionStrategy::SizeTiered),
          memtable_max_size_(64 * 1024 * 1024) // 64MB
    {
        memtable_ = std::make_unique<MemTable>(memtable_max_size_);
        compaction_manager_ = std::make_unique<CompactionManager>(fmap_, compaction_strategy_);
    }

    LSMTreeIndex::~LSMTreeIndex()
    {
        // Ensure any pending operations complete
    }

    bool LSMTreeIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        return insert_with_payload(key, row_id, "", err);
    }

    bool LSMTreeIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                           const std::string& payload, std::string& err)
    {
        std::lock_guard<std::mutex> lock(operation_mutex_);

        total_writes_++;

        // Check if memtable is full
        if (memtable_->is_full()) {
            flush_memtable();
        }

        // Insert into current memtable
        if (!memtable_->insert(key, row_id, payload)) {
            err = "Failed to insert into memtable";
            return false;
        }

        // Trigger compaction if needed
        if (compaction_manager_->needs_compaction() && !compaction_in_progress_) {
            background_compaction();
        }

        return true;
    }

    void LSMTreeIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        std::lock_guard<std::mutex> lock(operation_mutex_);

        total_reads_++;

        // Search in memtable first
        if (memtable_->search(key, out)) {
            return;
        }

        // Search in immutable memtable if it exists
        if (immutable_memtable_ && immutable_memtable_->search(key, out)) {
            return;
        }

        // Search in SSTables (from newest to oldest)
        auto sstables = compaction_manager_->get_sstables_for_level(0);
        for (const auto& ss_info : sstables) {
            SSTable sstable(const_cast<FileMap&>(fmap_), ss_info.sstable_id);
            if (sstable.search(key, out)) {
                return;
            }
        }
    }

    void LSMTreeIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        std::vector<std::uint64_t> row_ids;
        search_equal(key, row_ids);

        // Convert to payload format - simplified implementation
        for (auto row_id : row_ids) {
            out.emplace_back(row_id, ""); // Would retrieve actual payload
        }
    }

    void LSMTreeIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                    bool hi_incl,
                                    std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        std::lock_guard<std::mutex> lock(operation_mutex_);

        total_reads_++;

        // Search in memtable
        memtable_->search_range(lo, hi, out);

        // Search in immutable memtable
        if (immutable_memtable_) {
            immutable_memtable_->search_range(lo, hi, out);
        }

        // Search in SSTables
        auto sstables = compaction_manager_->get_sstables_for_level(0);
        for (const auto& ss_info : sstables) {
            SSTable sstable(const_cast<FileMap&>(fmap_), ss_info.sstable_id);
            sstable.search_range(lo, hi, out);
        }

        // Remove duplicates and sort
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
    }

    std::size_t LSMTreeIndex::erase_equal(const std::string& key, std::string& err)
    {
        // LSM-Trees typically use tombstone markers for deletion
        return insert_with_payload(key, 0, "__TOMBSTONE__", err) ? 1 : 0;
    }

    bool LSMTreeIndex::validate(std::string& error) const
    {
        // Basic validation - would be more comprehensive in production
        if (memtable_->size() > memtable_max_size_) {
            error = "MemTable size exceeds maximum";
            return false;
        }

        return true;
    }

    void LSMTreeIndex::rebuild_offline()
    {
        // Force flush and compaction
        flush_memtable();
        force_compaction();
    }

    std::string LSMTreeIndex::collect_statistics() const
    {
        std::ostringstream stats;
        stats << "LSM-Tree Index Statistics:\n";
        stats << "  Total writes: " << total_writes_ << "\n";
        stats << "  Total reads: " << total_reads_ << "\n";
        stats << "  Total compactions: " << total_compactions_ << "\n";
        stats << "  MemTable size: " << memtable_->size() << " / " << memtable_max_size_ << "\n";
        stats << "  Write amplification: " << get_write_amplification() << "\n";
        stats << "  Read amplification: " << get_read_amplification() << "\n";
        stats << "  Space amplification: " << get_space_amplification() << "\n";
        stats << "  Compaction strategy: "
              << (compaction_strategy_ == CompactionStrategy::SizeTiered ? "Size-Tiered"
                                                                         : "Leveled");

        return stats.str();
    }

    void LSMTreeIndex::compact_index()
    {
        force_compaction();
    }

    bool LSMTreeIndex::open_existing(std::uint32_t root_page)
    {
        meta_page_ = root_page;
        std::string err;
        return load_metadata(err);
    }

    void LSMTreeIndex::create_empty()
    {
        meta_page_ = allocate_page();
        std::string err;
        save_metadata(err);
    }

    double LSMTreeIndex::estimate_search_cost(const std::string& key) const
    {
        // LSM-Tree search cost depends on number of levels
        auto sstables = compaction_manager_->get_sstables_for_level(0);
        double cost = 1.0;             // MemTable search
        // Lower cost if Bloom filters are present
        double per_sstable = 0.5;
        for (const auto& ss : sstables) {
            per_sstable += ss.bloom_bits.empty() ? 0.0 : -0.2;
        }
        cost += sstables.size() * std::max(0.2, per_sstable);
        return cost;
    }

    double LSMTreeIndex::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        // Range queries are more expensive in LSM-Trees
        auto sstables = compaction_manager_->get_sstables_for_level(0);
        double cost = 2.0;             // MemTable range scan
        cost += sstables.size() * 2.0; // SSTable range scans
        return cost;
    }

    double LSMTreeIndex::estimate_maintenance_cost() const
    {
        return get_write_amplification() * 1.5; // Factor in compaction overhead
    }

    void LSMTreeIndex::set_compaction_strategy(CompactionStrategy strategy)
    {
        compaction_strategy_ = strategy;
        compaction_manager_ = std::make_unique<CompactionManager>(fmap_, strategy);
    }

    void LSMTreeIndex::set_memtable_size(std::size_t size)
    {
        memtable_max_size_ = size;
    }

    void LSMTreeIndex::force_flush()
    {
        std::lock_guard<std::mutex> lock(operation_mutex_);
        flush_memtable();
    }

    void LSMTreeIndex::force_compaction()
    {
        if (!compaction_in_progress_) {
            background_compaction();
        }
    }

    std::uint64_t LSMTreeIndex::get_write_amplification() const
    {
        // Simplified calculation - would be more accurate in production
        return total_compactions_ > 0 ? (total_writes_ * 2) / total_compactions_ : 1;
    }

    std::uint64_t LSMTreeIndex::get_read_amplification() const
    {
        // Read amplification = average SSTables touched per read
        auto sstables = compaction_manager_->get_sstables_for_level(0);
        return std::max(static_cast<std::uint64_t>(1), static_cast<std::uint64_t>(sstables.size()));
    }

    std::uint64_t LSMTreeIndex::get_space_amplification() const
    {
        // Space amplification = total size / live data size
        return 2; // Simplified - would calculate actual sizes
    }

    bool LSMTreeIndex::load_metadata(std::string& /* err */)
    {
        // Load LSM-Tree metadata from meta page
        // Simplified implementation
        return true;
    }

    bool LSMTreeIndex::save_metadata(std::string& /* err */)
    {
        // Save LSM-Tree metadata to meta page
        // Simplified implementation
        return true;
    }

    void LSMTreeIndex::flush_memtable()
    {
        if (memtable_->size() == 0) {
            return;
        }

        // Move current memtable to immutable
        immutable_memtable_ = std::move(memtable_);
        memtable_ = std::make_unique<MemTable>(memtable_max_size_);

        // Create new SSTable from immutable memtable
        std::uint32_t new_sstable_id = allocate_sstable_id();
        SSTable new_sstable(fmap_, new_sstable_id);

        if (new_sstable.create_from_memtable(*immutable_memtable_)) {
            compaction_manager_->add_sstable(new_sstable_id, new_sstable.get_info());
        }

        // Clear immutable memtable
        immutable_memtable_.reset();
    }

    void LSMTreeIndex::background_compaction()
    {
        compaction_in_progress_ = true;
        compaction_manager_->trigger_compaction();
        total_compactions_++;
        compaction_in_progress_ = false;
    }

    std::uint32_t LSMTreeIndex::allocate_sstable_id()
    {
        static std::atomic<std::uint32_t> next_id{1000};
        return next_id++;
    }

    std::uint32_t LSMTreeIndex::allocate_page()
    {
        static std::atomic<std::uint32_t> next_page{100};
        return next_page++;
    }

    // LSMTreeScan implementation
    bool LSMTreeScan::init(const std::string& key_condition)
    {
        reset();
        target_key_ = key_condition;

        if (index_) {
            index_->search_equal(target_key_, results_);
            pages_accessed_++;
        }

        return !results_.empty();
    }

    bool LSMTreeScan::next(std::uint64_t& row_id, std::string& key, std::string& payload)
    {
        if (result_position_ >= results_.size()) {
            finished_ = true;
            return false;
        }

        row_id = results_[result_position_++];
        key = target_key_;
        payload.clear(); // Would retrieve actual payload
        rows_scanned_++;

        return true;
    }

    void LSMTreeScan::reset()
    {
        finished_ = false;
        rows_scanned_ = 0;
        pages_accessed_ = 0;
        result_position_ = 0;
        results_.clear();
    }

    bool LSMTreeScan::is_finished() const
    {
        return finished_;
    }

    std::uint64_t LSMTreeScan::rows_scanned() const
    {
        return rows_scanned_;
    }

    std::uint64_t LSMTreeScan::pages_accessed() const
    {
        return pages_accessed_;
    }

} // namespace scratchbird::engine
