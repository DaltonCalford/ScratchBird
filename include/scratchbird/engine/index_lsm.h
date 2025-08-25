#ifndef SCRATCHBIRD_ENGINE_INDEX_LSM_H
#define SCRATCHBIRD_ENGINE_INDEX_LSM_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_family.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace scratchbird::engine
{

    /**
     * LSM-Tree compaction strategies
     */
    enum class CompactionStrategy {
        SizeTiered, // Size-tiered compaction
        Leveled     // Leveled compaction
    };

    /**
     * SSTable (Sorted String Table) metadata
     */
    struct SSTableInfo {
        std::uint32_t sstable_id;
        std::uint32_t level;
        std::uint64_t file_size;
        std::uint64_t key_count;
        std::string min_key;
        std::string max_key;
        std::uint64_t creation_time;
        bool compacting{false};
    };

    /**
     * Write buffer (MemTable) for in-memory operations
     */
    class MemTable
    {
      public:
        MemTable(std::size_t max_size);
        ~MemTable() = default;

        bool insert(const std::string& key, std::uint64_t row_id, const std::string& payload);
        bool search(const std::string& key, std::vector<std::uint64_t>& row_ids) const;
        void search_range(const std::string& lo, const std::string& hi,
                          std::vector<std::pair<std::string, std::uint64_t>>& results) const;

        bool is_full() const;
        std::size_t size() const;
        void clear();

        // Iterator for flushing to SSTable
        struct Iterator {
            std::string key;
            std::uint64_t row_id;
            std::string payload;
            bool valid;
        };

        Iterator begin() const;
        Iterator next(const Iterator& it) const;

      private:
        struct Entry {
            std::string key;
            std::uint64_t row_id;
            std::string payload;
            bool deleted{false};
        };

        mutable std::mutex mutex_;
        std::vector<Entry> entries_;
        std::size_t max_size_;
        std::atomic<std::size_t> current_size_{0};
    };

    /**
     * SSTable file reader/writer
     */
    class SSTable
    {
      public:
        SSTable(FileMap& fmap, std::uint32_t sstable_id);
        ~SSTable() = default;

        bool create_from_memtable(const MemTable& memtable);
        bool search(const std::string& key, std::vector<std::uint64_t>& row_ids) const;
        void search_range(const std::string& lo, const std::string& hi,
                          std::vector<std::pair<std::string, std::uint64_t>>& results) const;

        SSTableInfo get_info() const;
        std::uint64_t get_file_size() const;

        // Iterator for compaction
        struct Iterator {
            std::string key;
            std::uint64_t row_id;
            std::string payload;
            bool valid;
        };

        Iterator begin() const;
        Iterator next(const Iterator& it) const;

      private:
        FileMap& fmap_;
        std::uint32_t sstable_id_;
        mutable SSTableInfo info_;

        bool write_entry(const std::string& key, std::uint64_t row_id, const std::string& payload);
        bool read_entry(std::uint32_t offset, std::string& key, std::uint64_t& row_id,
                        std::string& payload) const;
    };

    /**
     * Compaction manager for LSM-Tree
     */
    class CompactionManager
    {
      public:
        CompactionManager(FileMap& fmap, CompactionStrategy strategy);
        ~CompactionManager() = default;

        void add_sstable(std::uint32_t sstable_id, const SSTableInfo& info);
        bool needs_compaction() const;
        void trigger_compaction();

        std::vector<SSTableInfo> get_sstables_for_level(std::uint32_t level) const;
        void compact_level(std::uint32_t level);

      private:
        FileMap& fmap_;
        CompactionStrategy strategy_;
        std::vector<SSTableInfo> sstables_;
        mutable std::mutex mutex_;

        void size_tiered_compaction();
        void leveled_compaction();
        std::uint32_t merge_sstables(const std::vector<std::uint32_t>& sstable_ids,
                                     std::uint32_t target_level);
    };

    /**
     * LSM-Tree Index implementation
     * Optimized for write-intensive workloads
     */
    class LSMTreeIndex : public IndexFamily
    {
      public:
        LSMTreeIndex(FileMap&& fmap, std::uint32_t page_size, bool unique = false);
        ~LSMTreeIndex() override;

        // Core index operations
        bool insert(const std::string& key, std::uint64_t row_id, std::string& err) override;
        bool insert_with_payload(const std::string& key, std::uint64_t row_id,
                                 const std::string& payload, std::string& err) override;
        void search_equal(const std::string& key, std::vector<std::uint64_t>& out) const override;
        void search_equal_with_payload(
            const std::string& key,
            std::vector<std::pair<std::uint64_t, std::string>>& out) const override;
        void search_range(const std::string& lo, bool lo_incl, const std::string& hi, bool hi_incl,
                          std::vector<std::pair<std::string, std::uint64_t>>& out) const override;
        std::size_t erase_equal(const std::string& key, std::string& err) override;

        // Index maintenance
        bool validate(std::string& error) const override;
        void rebuild_offline() override;
        std::string collect_statistics() const override;
        void compact_index() override;

        // Index metadata
        IndexMethod get_method() const override
        {
            return IndexMethod::LSMTree;
        }
        std::uint32_t root_page() const override
        {
            return meta_page_;
        }
        bool open_existing(std::uint32_t root_page) override;
        void create_empty() override;

        // Cost estimation
        double estimate_search_cost(const std::string& key) const override;
        double estimate_range_cost(const std::string& lo, const std::string& hi) const override;
        double estimate_maintenance_cost() const override;

        // LSM-Tree specific operations
        void set_compaction_strategy(CompactionStrategy strategy);
        void set_memtable_size(std::size_t size);
        void force_flush();
        void force_compaction();

        // Statistics
        std::uint64_t get_write_amplification() const;
        std::uint64_t get_read_amplification() const;
        std::uint64_t get_space_amplification() const;

      private:
        FileMap fmap_;
        std::uint32_t page_size_;
        std::uint32_t meta_page_;
        bool unique_;

        // LSM-Tree components
        std::unique_ptr<MemTable> memtable_;
        std::unique_ptr<MemTable> immutable_memtable_;
        std::unique_ptr<CompactionManager> compaction_manager_;

        // Configuration
        CompactionStrategy compaction_strategy_;
        std::size_t memtable_max_size_;

        // Threading
        mutable std::mutex operation_mutex_;
        std::atomic<bool> compaction_in_progress_{false};

        // Statistics
        mutable std::atomic<std::uint64_t> total_writes_{0};
        mutable std::atomic<std::uint64_t> total_reads_{0};
        mutable std::atomic<std::uint64_t> total_compactions_{0};

        // Internal operations
        bool load_metadata(std::string& err);
        bool save_metadata(std::string& err);
        void flush_memtable();
        void background_compaction();
        std::uint32_t allocate_sstable_id();
        std::uint32_t allocate_page();
    };

    /**
     * LSM-Tree scan implementation
     */
    class LSMTreeScan : public IndexScan
    {
      public:
        LSMTreeScan(LSMTreeIndex* index) : index_(index) {}

        bool init(const std::string& key_condition) override;
        bool next(std::uint64_t& row_id, std::string& key, std::string& payload) override;
        void reset() override;
        bool is_finished() const override;
        std::uint64_t rows_scanned() const override;
        std::uint64_t pages_accessed() const override;

      private:
        LSMTreeIndex* index_;
        bool finished_{false};
        std::uint64_t rows_scanned_{0};
        std::uint64_t pages_accessed_{0};

        // Scan state
        std::string target_key_;
        std::vector<std::uint64_t> results_;
        std::size_t result_position_{0};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_INDEX_LSM_H
