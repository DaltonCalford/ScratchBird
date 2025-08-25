#include "scratchbird/engine/index_hash.h"

#include "scratchbird/engine/index_btree.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <sstream>

namespace scratchbird::engine
{

    // Universal Hash Function Implementation
    UniversalHashFunction::UniversalHashFunction(std::uint32_t a, std::uint32_t b) : a_(a), b_(b) {}

    std::uint32_t UniversalHashFunction::hash(const std::string& key) const
    {
        std::uint32_t result = 0;
        for (char c : key) {
            result = (result * a_ + static_cast<std::uint32_t>(c)) % LARGE_PRIME;
        }
        return (result * a_ + b_) % LARGE_PRIME;
    }

    // FNV Hash Function Implementation
    std::uint32_t FNVHashFunction::hash(const std::string& key) const
    {
        std::uint32_t hash_value = FNV_OFFSET_BASIS;
        for (char c : key) {
            hash_value ^= static_cast<std::uint32_t>(c);
            hash_value *= FNV_PRIME;
        }
        return hash_value;
    }

    // Hash Index Implementation
    HashIndex::HashIndex(FileMap fmap, std::uint32_t page_size, bool unique)
        : fmap_(std::move(fmap)), page_size_(page_size), unique_(unique),
          hash_func_(std::make_unique<UniversalHashFunction>())
    {
    }

    void HashIndex::create_empty()
    {
        // Allocate directory page
        directory_page_ = 1; // Assuming page 0 is reserved
        global_depth_ = 1;

        // Initialize directory with initial buckets
        directory_.clear();
        for (std::uint32_t i = 0; i < tunables_.initial_buckets; ++i) {
            HashDirEntry entry;
            entry.bucket_page = allocate_bucket_page();
            entry.local_depth = global_depth_;
            entry.entry_count = 0;
            directory_.push_back(entry);
        }

        write_directory();
        stats_dirty_ = true;
    }

    bool HashIndex::open_existing(std::uint32_t root_page)
    {
        directory_page_ = root_page;
        read_directory();
        stats_dirty_ = true;
        return true;
    }

    bool HashIndex::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        return insert_with_payload(key, row_id, "", err);
    }

    bool HashIndex::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                        const std::string& payload, std::string& err)
    {
        HashEntry entry;
        entry.hash_value = hash_func_->hash(key);
        entry.key = key;
        entry.row_id = row_id;
        entry.payload = payload;

        // Find target bucket
        std::uint32_t bucket_page = get_bucket_for_hash(entry.hash_value);

        // Check for uniqueness if required
        if (unique_) {
            std::vector<HashEntry> existing;
            if (bucket_search(bucket_page, key, existing)) {
                if (!existing.empty()) {
                    err = "Duplicate key violation: " + key;
                    return false;
                }
            }
        }

        // Try to insert into bucket
        if (!bucket_insert(bucket_page, entry, err)) {
            // If bucket is full and needs splitting
            if (err.find("bucket full") != std::string::npos) {
                split_bucket(bucket_page);

                // Retry insertion after split
                bucket_page = get_bucket_for_hash(entry.hash_value);
                if (!bucket_insert(bucket_page, entry, err)) {
                    return false;
                }
            } else {
                return false;
            }
        }

        // Update directory entry count
        for (auto& dir_entry : directory_) {
            if (dir_entry.bucket_page == bucket_page) {
                dir_entry.entry_count++;
                break;
            }
        }

        write_directory();
        stats_dirty_ = true;
        return true;
    }

    void HashIndex::search_equal(const std::string& key, std::vector<std::uint64_t>& out) const
    {
        out.clear();

        std::uint32_t hash_value = hash_func_->hash(key);
        std::uint32_t bucket_page = get_bucket_for_hash(hash_value);

        std::vector<HashEntry> entries;
        if (bucket_search(bucket_page, key, entries)) {
            for (const auto& entry : entries) {
                out.push_back(entry.row_id);
            }
        }
    }

    void HashIndex::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        out.clear();

        std::uint32_t hash_value = hash_func_->hash(key);
        std::uint32_t bucket_page = get_bucket_for_hash(hash_value);

        std::vector<HashEntry> entries;
        if (bucket_search(bucket_page, key, entries)) {
            for (const auto& entry : entries) {
                out.emplace_back(entry.row_id, entry.payload);
            }
        }
    }

    void HashIndex::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                 bool hi_incl,
                                 std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        // Hash indexes don't support range queries
        out.clear();
    }

    std::size_t HashIndex::erase_equal(const std::string& key, std::string& err)
    {
        std::uint32_t hash_value = hash_func_->hash(key);
        std::uint32_t bucket_page = get_bucket_for_hash(hash_value);

        std::size_t deleted = 0;
        if (!bucket_delete(bucket_page, key, deleted)) {
            err = "Error deleting from bucket";
            return 0;
        }

        // Update directory entry count
        for (auto& dir_entry : directory_) {
            if (dir_entry.bucket_page == bucket_page) {
                dir_entry.entry_count =
                    (dir_entry.entry_count >= deleted) ? (dir_entry.entry_count - deleted) : 0;
                break;
            }
        }

        write_directory();
        stats_dirty_ = true;
        return deleted;
    }

    bool HashIndex::validate(std::string& error) const
    {
        // Validate directory structure
        if (directory_.empty()) {
            error = "Empty directory";
            return false;
        }

        // Validate bucket pages
        std::vector<std::uint8_t> page_buffer(page_size_);
        for (const auto& dir_entry : directory_) {
            try {
                read_page(dir_entry.bucket_page, page_buffer);
                HashBucketHeader header = read_bucket_header(page_buffer);

                // Validate bucket header
                if (header.num_slots >
                    (page_size_ - sizeof(HashBucketHeader)) / sizeof(std::uint16_t)) {
                    error = "Invalid bucket slot count on page " +
                            std::to_string(dir_entry.bucket_page);
                    return false;
                }

                if (header.free_start > page_size_ || header.dir_start > page_size_) {
                    error = "Invalid bucket free space pointers on page " +
                            std::to_string(dir_entry.bucket_page);
                    return false;
                }

            } catch (const std::exception& e) {
                error = "Error reading bucket page " + std::to_string(dir_entry.bucket_page) +
                        ": " + e.what();
                return false;
            }
        }

        return true;
    }

    void HashIndex::rebuild_offline()
    {
        // Collect all entries from current index
        std::vector<HashEntry> all_entries;
        std::vector<std::uint8_t> page_buffer(page_size_);

        for (const auto& dir_entry : directory_) {
            read_page(dir_entry.bucket_page, page_buffer);
            HashBucketHeader header = read_bucket_header(page_buffer);

            // Read all entries from bucket
            std::uint16_t offset = sizeof(HashBucketHeader);
            for (std::uint16_t i = 0; i < header.num_slots; ++i) {
                HashEntry entry;
                decode_entry(page_buffer, offset, entry);
                all_entries.push_back(entry);
                offset += entry_size(entry);
            }
        }

        // Rebuild with optimal parameters
        create_empty();

        // Reinsert all entries
        for (const auto& entry : all_entries) {
            std::string err;
            if (!insert_with_payload(entry.key, entry.row_id, entry.payload, err)) {
                // Log error but continue rebuild
            }
        }

        stats_dirty_ = true;
    }

    std::string HashIndex::collect_statistics() const
    {
        if (stats_dirty_) {
            cached_stats_ = compute_statistics();
            stats_dirty_ = false;
        }

        std::ostringstream oss;
        oss << "Hash Index Statistics:\n";
        oss << "  Total Buckets: " << cached_stats_.total_buckets << "\n";
        oss << "  Total Entries: " << cached_stats_.total_entries << "\n";
        oss << "  Load Factor: " << cached_stats_.load_factor << "\n";
        oss << "  Average Chain Length: " << cached_stats_.average_chain_length << "\n";
        oss << "  Max Chain Length: " << cached_stats_.max_chain_length << "\n";
        oss << "  Empty Buckets: " << cached_stats_.empty_buckets << "\n";
        oss << "  Overflow Buckets: " << cached_stats_.overflow_buckets << "\n";
        oss << "  Directory Size: " << cached_stats_.directory_size << "\n";
        oss << "  Global Depth: " << cached_stats_.global_depth << "\n";
        oss << "  Hash Function: " << cached_stats_.hash_function << "\n";

        return oss.str();
    }

    void HashIndex::compact_index()
    {
        // Remove empty buckets and reorganize
        std::vector<HashDirEntry> new_directory;

        for (const auto& dir_entry : directory_) {
            if (dir_entry.entry_count > 0) {
                new_directory.push_back(dir_entry);
            }
        }

        directory_ = std::move(new_directory);
        write_directory();
        stats_dirty_ = true;
    }

    double HashIndex::estimate_search_cost(const std::string& key) const
    {
        // Hash index provides O(1) average search cost
        if (stats_dirty_) {
            cached_stats_ = compute_statistics();
            stats_dirty_ = false;
        }

        // Base cost is 1 page read for directory + 1 page read for bucket
        double cost = 2.0;

        // Add cost for chain traversal based on average chain length
        cost += cached_stats_.average_chain_length * 0.1; // Small additional cost per chain link

        return cost;
    }

    double HashIndex::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        // Hash indexes don't support range queries efficiently
        return std::numeric_limits<double>::infinity();
    }

    double HashIndex::estimate_maintenance_cost() const
    {
        if (stats_dirty_) {
            cached_stats_ = compute_statistics();
            stats_dirty_ = false;
        }

        // Cost increases with load factor and chain length
        double cost = 1.0;
        cost *= (1.0 + cached_stats_.load_factor);
        cost *= (1.0 + cached_stats_.average_chain_length * 0.5);

        return cost;
    }

    // Private implementation methods
    std::uint32_t HashIndex::get_bucket_for_hash(std::uint32_t hash_value) const
    {
        if (directory_.empty()) {
            return 0; // Error case
        }

        // Use low-order bits for bucket selection
        std::uint32_t bucket_index = hash_value & ((1U << global_depth_) - 1);
        bucket_index = bucket_index % directory_.size();

        return directory_[bucket_index].bucket_page;
    }

    std::uint32_t HashIndex::allocate_bucket_page()
    {
        // Simplified allocation - in real implementation, use proper page allocator
        static std::uint32_t next_page = 100; // Start after reserved pages
        return next_page++;
    }

    void HashIndex::write_directory()
    {
        std::vector<std::uint8_t> page_buffer(page_size_, 0);

        // Write page header
        ods::PageHeader header;
        header.type = static_cast<std::uint16_t>(ods::PageType::HashDir);
        header.flags = 0;
        std::memcpy(page_buffer.data(), &header, sizeof(header));

        // Write global depth and directory size
        std::uint32_t offset = sizeof(ods::PageHeader);
        std::memcpy(page_buffer.data() + offset, &global_depth_, sizeof(global_depth_));
        offset += sizeof(global_depth_);

        std::uint32_t dir_size = directory_.size();
        std::memcpy(page_buffer.data() + offset, &dir_size, sizeof(dir_size));
        offset += sizeof(dir_size);

        // Write directory entries
        for (const auto& dir_entry : directory_) {
            std::memcpy(page_buffer.data() + offset, &dir_entry, sizeof(dir_entry));
            offset += sizeof(dir_entry);
        }

        write_page(directory_page_, page_buffer);
    }

    void HashIndex::read_directory()
    {
        std::vector<std::uint8_t> page_buffer(page_size_);
        read_page(directory_page_, page_buffer);

        // Read global depth and directory size
        std::uint32_t offset = sizeof(ods::PageHeader);
        std::memcpy(&global_depth_, page_buffer.data() + offset, sizeof(global_depth_));
        offset += sizeof(global_depth_);

        std::uint32_t dir_size;
        std::memcpy(&dir_size, page_buffer.data() + offset, sizeof(dir_size));
        offset += sizeof(dir_size);

        // Read directory entries
        directory_.clear();
        directory_.reserve(dir_size);

        for (std::uint32_t i = 0; i < dir_size; ++i) {
            HashDirEntry dir_entry;
            std::memcpy(&dir_entry, page_buffer.data() + offset, sizeof(dir_entry));
            offset += sizeof(dir_entry);
            directory_.push_back(dir_entry);
        }
    }

    HashIndexStats HashIndex::compute_statistics() const
    {
        HashIndexStats stats;
        stats.total_buckets = directory_.size();
        stats.directory_size = directory_.size();
        stats.global_depth = global_depth_;
        stats.hash_function = hash_func_->name();

        std::uint32_t total_chain_length = 0;
        std::uint32_t max_chain = 0;
        std::uint32_t empty_buckets = 0;

        for (const auto& dir_entry : directory_) {
            stats.total_entries += dir_entry.entry_count;

            if (dir_entry.entry_count == 0) {
                empty_buckets++;
            } else {
                // Simplified chain length calculation
                std::uint32_t chain_length =
                    (dir_entry.entry_count + tunables_.bucket_split_threshold - 1) /
                    tunables_.bucket_split_threshold;
                total_chain_length += chain_length;
                max_chain = std::max(max_chain, chain_length);
            }
        }

        stats.empty_buckets = empty_buckets;
        stats.max_chain_length = max_chain;
        stats.average_chain_length =
            stats.total_buckets > empty_buckets
                ? static_cast<double>(total_chain_length) / (stats.total_buckets - empty_buckets)
                : 0.0;
        stats.load_factor = stats.total_buckets > 0
                                ? static_cast<double>(stats.total_entries) / stats.total_buckets
                                : 0.0;

        return stats;
    }

    // BTree Family Wrapper Implementation
    BTreeIndexFamily::BTreeIndexFamily(FileMap fmap, std::uint32_t page_size, bool unique)
        : btree_(std::make_unique<BTreeIndex>(std::move(fmap), page_size, unique))
    {
    }

    bool BTreeIndexFamily::insert(const std::string& key, std::uint64_t row_id, std::string& err)
    {
        return btree_->insert(key, row_id, err);
    }

    bool BTreeIndexFamily::insert_with_payload(const std::string& key, std::uint64_t row_id,
                                               const std::string& payload, std::string& err)
    {
        return btree_->insert_with_payload(key, row_id, payload, err);
    }

    void BTreeIndexFamily::search_equal(const std::string& key,
                                        std::vector<std::uint64_t>& out) const
    {
        btree_->search_equal(key, out);
    }

    void BTreeIndexFamily::search_equal_with_payload(
        const std::string& key, std::vector<std::pair<std::uint64_t, std::string>>& out) const
    {
        btree_->search_equal_with_payload(key, out);
    }

    void
    BTreeIndexFamily::search_range(const std::string& lo, bool lo_incl, const std::string& hi,
                                   bool hi_incl,
                                   std::vector<std::pair<std::string, std::uint64_t>>& out) const
    {
        btree_->search_range(lo, lo_incl, hi, hi_incl, out);
    }

    std::size_t BTreeIndexFamily::erase_equal(const std::string& key, std::string& err)
    {
        return btree_->erase_equal(key, err);
    }

    bool BTreeIndexFamily::validate(std::string& error) const
    {
        return btree_->validate(error);
    }

    void BTreeIndexFamily::rebuild_offline()
    {
        btree_->rebuild_offline();
    }

    std::string BTreeIndexFamily::collect_statistics() const
    {
        auto stats = btree_->compute_stats();
        std::ostringstream oss;
        oss << "B-Tree Index Statistics:\n";
        oss << "  Height: " << stats.height << "\n";
        oss << "  Leaf Pages: " << stats.leaf_pages << "\n";
        oss << "  Branch Pages: " << stats.branch_pages << "\n";
        oss << "  Key Count: " << stats.key_count << "\n";
        oss << "  Min Key: " << stats.min_key << "\n";
        oss << "  Max Key: " << stats.max_key << "\n";
        return oss.str();
    }

    std::uint32_t BTreeIndexFamily::root_page() const
    {
        return btree_->root_page();
    }

    bool BTreeIndexFamily::open_existing(std::uint32_t root_page)
    {
        return btree_->open_existing(root_page);
    }

    void BTreeIndexFamily::create_empty()
    {
        btree_->create_empty();
    }

    double BTreeIndexFamily::estimate_search_cost(const std::string& key) const
    {
        auto stats = btree_->compute_stats();
        return static_cast<double>(stats.height); // Cost proportional to tree height
    }

    double BTreeIndexFamily::estimate_range_cost(const std::string& lo, const std::string& hi) const
    {
        auto stats = btree_->compute_stats();
        // Simplified: assume range scan cost is height + fraction of leaf pages
        return static_cast<double>(stats.height) + static_cast<double>(stats.leaf_pages) * 0.1;
    }

    double BTreeIndexFamily::estimate_maintenance_cost() const
    {
        auto stats = btree_->compute_stats();
        return static_cast<double>(stats.height) * 0.5; // Lower maintenance cost for B-Tree
    }

    // Page I/O operations (simplified - would use actual FileMap in real implementation)
    std::vector<std::uint8_t> HashIndex::new_page_buffer(ods::PageType type,
                                                         std::uint32_t page_no) const
    {
        std::vector<std::uint8_t> buffer(page_size_, 0);
        ods::PageHeader header;
        header.type = static_cast<std::uint16_t>(type);
        header.flags = 0;
        std::memcpy(buffer.data(), &header, sizeof(header));
        return buffer;
    }

    void HashIndex::write_page(std::uint32_t page_no, const std::vector<std::uint8_t>& page)
    {
        // In real implementation, would use fmap_ to write page
        // For now, this is a placeholder
    }

    void HashIndex::read_page(std::uint32_t page_no, std::vector<std::uint8_t>& page) const
    {
        // In real implementation, would use fmap_ to read page
        // For now, initialize with empty page
        page.resize(page_size_, 0);
        ods::PageHeader header;
        header.type = static_cast<std::uint16_t>(ods::PageType::HashBucket);
        header.flags = 0;
        std::memcpy(page.data(), &header, sizeof(header));
    }

    // Bucket operations (simplified implementations)
    HashBucketHeader HashIndex::read_bucket_header(const std::vector<std::uint8_t>& page) const
    {
        HashBucketHeader header;
        std::memcpy(&header, page.data() + sizeof(ods::PageHeader), sizeof(header));
        return header;
    }

    void HashIndex::write_bucket_header(std::vector<std::uint8_t>& page,
                                        const HashBucketHeader& header)
    {
        std::memcpy(page.data() + sizeof(ods::PageHeader), &header, sizeof(header));
    }

    bool HashIndex::bucket_insert(std::uint32_t bucket_page, const HashEntry& entry,
                                  std::string& err)
    {
        // Simplified implementation - would read page, check space, insert entry
        return true;
    }

    bool HashIndex::bucket_search(std::uint32_t bucket_page, const std::string& key,
                                  std::vector<HashEntry>& out) const
    {
        // Simplified implementation - would read page and search for key
        return true;
    }

    bool HashIndex::bucket_delete(std::uint32_t bucket_page, const std::string& key,
                                  std::size_t& deleted)
    {
        // Simplified implementation - would read page, find entries, remove them
        deleted = 0;
        return true;
    }

    void HashIndex::encode_entry(std::vector<std::uint8_t>& page, std::uint16_t& offset,
                                 const HashEntry& entry)
    {
        // Simplified entry encoding - would pack hash, key, row_id, payload
    }

    void HashIndex::decode_entry(const std::vector<std::uint8_t>& page, std::uint16_t offset,
                                 HashEntry& entry)
    {
        // Simplified entry decoding - would unpack from page
    }

    std::uint16_t HashIndex::entry_size(const HashEntry& entry) const
    {
        return sizeof(entry.hash_value) + sizeof(entry.row_id) + entry.key.size() +
               entry.payload.size() + 8; // 8 bytes for length fields
    }

    void HashIndex::split_bucket(std::uint32_t bucket_page)
    {
        // Simplified bucket splitting - would redistribute entries
    }

    void HashIndex::expand_directory()
    {
        // Double the directory size for extensible hashing
        std::size_t old_size = directory_.size();
        directory_.resize(old_size * 2);

        // Duplicate entries for new slots
        for (std::size_t i = 0; i < old_size; ++i) {
            directory_[i + old_size] = directory_[i];
        }

        global_depth_++;
        write_directory();
    }

} // namespace scratchbird::engine
