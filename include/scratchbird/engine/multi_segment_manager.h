#ifndef SCRATCHBIRD_ENGINE_MULTI_SEGMENT_MANAGER_H
#define SCRATCHBIRD_ENGINE_MULTI_SEGMENT_MANAGER_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    // Multi-segment database file management for scalability

    // Segment information
    struct SegmentInfo {
        std::uint32_t segment_id{0};    // Unique segment identifier
        std::string file_path;          // Path to segment file
        std::uint64_t max_pages{0};     // Maximum pages in this segment
        std::uint64_t current_pages{0}; // Current allocated pages
        std::uint64_t free_pages{0};    // Available free pages
        std::uint32_t page_size{4096};  // Page size for this segment
        bool is_active{true};           // Whether segment accepts new allocations

        // Statistics
        std::uint64_t total_allocations{0};   // Total page allocations made
        std::uint64_t total_deallocations{0}; // Total page deallocations made
        double fragmentation_ratio{0.0};      // Fragmentation percentage
        std::time_t created_time{0};          // When segment was created
        std::time_t last_accessed{0};         // Last access time

        // Utility methods
        double utilization() const;
        bool is_full() const;
        bool needs_compaction() const;
    };

    // Segment allocation strategy
    enum class SegmentAllocationStrategy {
        ROUND_ROBIN,    // Cycle through segments evenly
        LEAST_UTILIZED, // Prefer segments with lowest utilization
        BEST_FIT,       // Find segment with best size fit
        LOCALITY_AWARE, // Try to keep related data in same segment
        SEQUENTIAL      // Fill segments sequentially
    };

    // Segment load balancing policies
    struct SegmentPolicy {
        SegmentAllocationStrategy allocation_strategy{SegmentAllocationStrategy::LEAST_UTILIZED};
        std::uint64_t max_segment_size{1024 * 1024 * 1024}; // 1GB default
        std::uint32_t max_segments{1024};                   // Maximum number of segments
        double fragmentation_threshold{0.3}; // Trigger compaction at 30% fragmentation
        double utilization_threshold{0.9};   // Mark segment full at 90% utilization
        bool auto_create_segments{true};     // Automatically create new segments
        bool auto_compact_segments{false};   // Automatically compact fragmented segments
    };

    // Multi-segment file mapping and management
    class MultiSegmentManager
    {
      public:
        MultiSegmentManager(const std::string& base_path,
                            const SegmentPolicy& policy = SegmentPolicy{});
        ~MultiSegmentManager();

        // Initialization and shutdown
        bool initialize();
        void shutdown();

        // Segment lifecycle management
        std::uint32_t create_segment(std::uint64_t max_pages = 0);
        bool open_segment(std::uint32_t segment_id);
        bool close_segment(std::uint32_t segment_id);
        bool delete_segment(std::uint32_t segment_id);

        // Page allocation and deallocation
        std::uint32_t allocate_page(std::uint32_t preferred_segment = 0);
        bool deallocate_page(std::uint32_t page_id);

        // Page I/O operations
        bool read_page(std::uint32_t page_id, void* buffer, std::size_t buffer_size);
        bool write_page(std::uint32_t page_id, const void* buffer, std::size_t buffer_size);
        bool flush_page(std::uint32_t page_id);

        // Segment information and statistics
        std::vector<SegmentInfo> get_segment_info() const;
        SegmentInfo get_segment_info(std::uint32_t segment_id) const;
        std::uint32_t get_segment_count() const;

        // Space management
        std::uint64_t get_total_pages() const;
        std::uint64_t get_free_pages() const;
        std::uint64_t get_allocated_pages() const;
        double get_overall_utilization() const;

        // Maintenance operations
        bool compact_segment(std::uint32_t segment_id);
        bool validate_segment(std::uint32_t segment_id);
        void update_statistics();

        // Configuration
        void set_allocation_strategy(SegmentAllocationStrategy strategy);
        void set_auto_create_segments(bool enabled);
        void set_fragmentation_threshold(double threshold);

        // Utility methods
        std::uint32_t page_id_to_segment_id(std::uint32_t page_id) const;
        std::uint32_t page_id_to_local_page(std::uint32_t page_id) const;
        std::uint32_t make_page_id(std::uint32_t segment_id, std::uint32_t local_page) const;

      private:
        std::string base_path_;
        SegmentPolicy policy_;
        std::uint32_t page_size_;

        mutable std::mutex segments_mutex_;
        std::unordered_map<std::uint32_t, std::unique_ptr<SegmentInfo>> segments_;
        std::unordered_map<std::uint32_t, std::unique_ptr<FileHandle>> segment_files_;

        std::atomic<std::uint32_t> next_segment_id_;
        std::atomic<std::uint32_t> allocation_counter_; // For round-robin allocation

        // Internal segment management
        std::uint32_t find_best_segment_for_allocation();
        bool load_existing_segments();
        std::string get_segment_file_path(std::uint32_t segment_id) const;

        // Allocation strategies
        std::uint32_t allocate_round_robin();
        std::uint32_t allocate_least_utilized();
        std::uint32_t allocate_best_fit();
        std::uint32_t allocate_sequential();

        // Maintenance helpers
        void background_maintenance();
        bool needs_new_segment() const;
        void update_segment_statistics(std::uint32_t segment_id);
    };

    // Segment migration and compaction utilities
    class SegmentMigration
    {
      public:
        SegmentMigration(MultiSegmentManager* manager);

        // Move pages between segments
        bool migrate_page(std::uint32_t from_page_id, std::uint32_t to_segment_id);
        bool migrate_pages(const std::vector<std::uint32_t>& page_ids, std::uint32_t to_segment_id);

        // Segment consolidation
        bool consolidate_segments(const std::vector<std::uint32_t>& segment_ids,
                                  std::uint32_t target_segment_id);
        bool split_segment(std::uint32_t segment_id, std::uint32_t split_ratio = 50);

        // Defragmentation
        bool defragment_segment(std::uint32_t segment_id);
        std::vector<std::uint32_t> find_fragmented_segments(double threshold = 0.3);

      private:
        MultiSegmentManager* manager_;

        // Migration helpers
        bool copy_page_data(std::uint32_t from_page, std::uint32_t to_page);
        void update_page_references(std::uint32_t old_page, std::uint32_t new_page);
    };

    // Multi-segment statistics and monitoring
    struct MultiSegmentStats {
        std::uint32_t total_segments{0};
        std::uint32_t active_segments{0};
        std::uint64_t total_pages{0};
        std::uint64_t allocated_pages{0};
        std::uint64_t free_pages{0};

        double overall_utilization{0.0};
        double average_fragmentation{0.0};

        std::uint64_t total_allocations{0};
        std::uint64_t total_deallocations{0};
        std::uint64_t allocation_failures{0};

        // Per-segment breakdown
        std::vector<SegmentInfo> segment_details;

        // Performance metrics
        double average_allocation_time_ms{0.0};
        double average_io_time_ms{0.0};
        std::uint64_t cache_hits{0};
        std::uint64_t cache_misses{0};
    };

    // High-level multi-segment operations
    class MultiSegmentDatabase
    {
      public:
        MultiSegmentDatabase(const std::string& database_path,
                             const SegmentPolicy& policy = SegmentPolicy{});
        ~MultiSegmentDatabase();

        // Database lifecycle
        bool create_database();
        bool open_database();
        bool close_database();

        // Table space management
        std::uint32_t create_tablespace(const std::string& name, std::uint64_t initial_size = 0);
        bool drop_tablespace(const std::string& name);
        std::vector<std::string> list_tablespaces() const;

        // Advanced operations
        bool backup_to_segments(const std::string& backup_path);
        bool restore_from_segments(const std::string& backup_path);
        bool verify_database_integrity();

        // Statistics and monitoring
        MultiSegmentStats get_statistics() const;
        void generate_usage_report(const std::string& output_path) const;

      private:
        std::string database_path_;
        SegmentPolicy policy_;
        std::unique_ptr<MultiSegmentManager> segment_manager_;

        std::unordered_map<std::string, std::uint32_t> tablespace_segments_;

        bool load_tablespace_metadata();
        bool save_tablespace_metadata();
    };

    // Utility functions for multi-segment management
    bool migrate_single_segment_to_multi_segment(const std::string& single_segment_path,
                                                 const std::string& multi_segment_base_path);

    std::vector<std::string> analyze_segment_distribution(const std::string& database_path);

    bool optimize_segment_layout(const std::string& database_path, const SegmentPolicy& policy);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_MULTI_SEGMENT_MANAGER_H
