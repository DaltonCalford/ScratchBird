#include "scratchbird/engine/multi_segment_manager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

namespace scratchbird::engine
{

    // ========== SegmentInfo Implementation ==========

    double SegmentInfo::utilization() const
    {
        if (max_pages == 0)
            return 0.0;
        return static_cast<double>(current_pages) / static_cast<double>(max_pages);
    }

    bool SegmentInfo::is_full() const
    {
        return utilization() >= 0.9; // 90% utilization threshold
    }

    bool SegmentInfo::needs_compaction() const
    {
        return fragmentation_ratio > 0.3; // 30% fragmentation threshold
    }

    // ========== MultiSegmentManager Implementation ==========

    MultiSegmentManager::MultiSegmentManager(const std::string& base_path,
                                             const SegmentPolicy& policy)
        : base_path_(base_path), policy_(policy), page_size_(4096), next_segment_id_(1),
          allocation_counter_(0)
    {
        std::filesystem::create_directories(base_path_);
    }

    MultiSegmentManager::~MultiSegmentManager()
    {
        shutdown();
    }

    bool MultiSegmentManager::initialize()
    {
        try {
            std::fprintf(stderr, "[MultiSegment] Initializing multi-segment manager at: %s\n",
                         base_path_.c_str());

            // Load existing segments
            if (!load_existing_segments()) {
                std::fprintf(stderr, "[MultiSegment] Failed to load existing segments\n");
                return false;
            }

            // Create initial segment if none exist
            if (segments_.empty()) {
                std::uint32_t initial_segment =
                    create_segment(policy_.max_segment_size / page_size_);
                if (initial_segment == 0) {
                    std::fprintf(stderr, "[MultiSegment] Failed to create initial segment\n");
                    return false;
                }
                std::fprintf(stderr, "[MultiSegment] Created initial segment %u\n",
                             initial_segment);
            }

            std::fprintf(stderr, "[MultiSegment] Initialized with %zu segments\n",
                         segments_.size());
            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Initialization failed: %s\n", e.what());
            return false;
        }
    }

    void MultiSegmentManager::shutdown()
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        // Close all segment files
        for (auto& [segment_id, file_handle] : segment_files_) {
            try {
                // File handles are automatically closed by destructor
            } catch (...) {
                // Ignore errors during shutdown
            }
        }

        segment_files_.clear();
        segments_.clear();

        std::fprintf(stderr, "[MultiSegment] Shutdown completed\n");
    }

    std::uint32_t MultiSegmentManager::create_segment(std::uint64_t max_pages)
    {
        try {
            std::uint32_t segment_id = next_segment_id_.fetch_add(1);

            if (max_pages == 0) {
                max_pages = policy_.max_segment_size / page_size_;
            }

            std::string segment_path = get_segment_file_path(segment_id);

            // Create segment file
            FileOptions options;
            options.direct_io = false;
            auto file_handle =
                std::make_unique<FileHandle>(FileManager::open(segment_path, options, true));

            // Create segment info
            auto segment_info = std::make_unique<SegmentInfo>();
            segment_info->segment_id = segment_id;
            segment_info->file_path = segment_path;
            segment_info->max_pages = max_pages;
            segment_info->current_pages = 0;
            segment_info->free_pages = max_pages;
            segment_info->page_size = page_size_;
            segment_info->is_active = true;
            segment_info->created_time =
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            segment_info->last_accessed = segment_info->created_time;

            std::lock_guard<std::mutex> lock(segments_mutex_);
            segments_[segment_id] = std::move(segment_info);
            segment_files_[segment_id] = std::move(file_handle);

            std::fprintf(stderr, "[MultiSegment] Created segment %u with %llu max pages at %s\n",
                         segment_id, static_cast<unsigned long long>(max_pages),
                         segment_path.c_str());

            return segment_id;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Failed to create segment: %s\n", e.what());
            return 0;
        }
    }

    bool MultiSegmentManager::open_segment(std::uint32_t segment_id)
    {
        try {
            std::lock_guard<std::mutex> lock(segments_mutex_);

            auto it = segments_.find(segment_id);
            if (it == segments_.end()) {
                std::fprintf(stderr, "[MultiSegment] Segment %u not found\n", segment_id);
                return false;
            }

            // Check if already open
            if (segment_files_.find(segment_id) != segment_files_.end()) {
                return true; // Already open
            }

            // Open segment file
            FileOptions options;
            options.direct_io = false;
            auto file_handle = std::make_unique<FileHandle>(
                FileManager::open(it->second->file_path, options, false));

            segment_files_[segment_id] = std::move(file_handle);
            it->second->last_accessed =
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

            std::fprintf(stderr, "[MultiSegment] Opened segment %u\n", segment_id);
            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Failed to open segment %u: %s\n", segment_id,
                         e.what());
            return false;
        }
    }

    bool MultiSegmentManager::close_segment(std::uint32_t segment_id)
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        auto it = segment_files_.find(segment_id);
        if (it != segment_files_.end()) {
            segment_files_.erase(it);
            std::fprintf(stderr, "[MultiSegment] Closed segment %u\n", segment_id);
            return true;
        }

        return false;
    }

    std::uint32_t MultiSegmentManager::allocate_page(std::uint32_t preferred_segment)
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        std::uint32_t target_segment = 0;

        // Try preferred segment first
        if (preferred_segment != 0) {
            auto it = segments_.find(preferred_segment);
            if (it != segments_.end() && it->second->is_active && it->second->free_pages > 0) {
                target_segment = preferred_segment;
            }
        }

        // Use allocation strategy if no preferred segment or preferred is unavailable
        if (target_segment == 0) {
            target_segment = find_best_segment_for_allocation();
        }

        // Create new segment if needed and allowed
        if (target_segment == 0 && policy_.auto_create_segments &&
            segments_.size() < policy_.max_segments) {
            // Temporarily release lock to create segment
            segments_mutex_.unlock();
            target_segment = create_segment();
            segments_mutex_.lock();
        }

        if (target_segment == 0) {
            std::fprintf(stderr, "[MultiSegment] No available segment for page allocation\n");
            return 0;
        }

        // Allocate page in target segment
        auto& segment = segments_[target_segment];
        if (segment->free_pages == 0) {
            std::fprintf(stderr, "[MultiSegment] Segment %u has no free pages\n", target_segment);
            return 0;
        }

        std::uint32_t local_page = segment->current_pages;
        std::uint32_t global_page_id = make_page_id(target_segment, local_page);

        segment->current_pages++;
        segment->free_pages--;
        segment->total_allocations++;
        segment->last_accessed =
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        std::fprintf(stderr, "[MultiSegment] Allocated page %u in segment %u (local page %u)\n",
                     global_page_id, target_segment, local_page);

        return global_page_id;
    }

    bool MultiSegmentManager::deallocate_page(std::uint32_t page_id)
    {
        std::uint32_t segment_id = page_id_to_segment_id(page_id);
        std::uint32_t local_page = page_id_to_local_page(page_id);

        std::lock_guard<std::mutex> lock(segments_mutex_);

        auto it = segments_.find(segment_id);
        if (it == segments_.end()) {
            std::fprintf(stderr, "[MultiSegment] Cannot deallocate page %u: segment %u not found\n",
                         page_id, segment_id);
            return false;
        }

        auto& segment = it->second;
        if (local_page >= segment->current_pages) {
            std::fprintf(stderr,
                         "[MultiSegment] Cannot deallocate page %u: local page %u out of range\n",
                         page_id, local_page);
            return false;
        }

        segment->free_pages++;
        segment->total_deallocations++;
        segment->last_accessed =
            std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

        // Update fragmentation (simplified calculation)
        if (segment->total_allocations > 0) {
            segment->fragmentation_ratio = static_cast<double>(segment->total_deallocations) /
                                           static_cast<double>(segment->total_allocations);
        }

        std::fprintf(stderr, "[MultiSegment] Deallocated page %u from segment %u\n", page_id,
                     segment_id);
        return true;
    }

    bool MultiSegmentManager::read_page(std::uint32_t page_id, void* buffer,
                                        std::size_t buffer_size)
    {
        std::uint32_t segment_id = page_id_to_segment_id(page_id);
        std::uint32_t local_page = page_id_to_local_page(page_id);

        if (buffer_size < page_size_) {
            std::fprintf(stderr, "[MultiSegment] Buffer too small for page read\n");
            return false;
        }

        std::lock_guard<std::mutex> lock(segments_mutex_);

        // Ensure segment is open
        if (segment_files_.find(segment_id) == segment_files_.end()) {
            if (!const_cast<MultiSegmentManager*>(this)->open_segment(segment_id)) {
                return false;
            }
        }

        auto file_it = segment_files_.find(segment_id);
        if (file_it == segment_files_.end()) {
            std::fprintf(stderr, "[MultiSegment] Segment %u file not open\n", segment_id);
            return false;
        }

        try {
            std::uint64_t offset = static_cast<std::uint64_t>(local_page) * page_size_;
            FileManager::pread(*file_it->second, buffer, page_size_, offset);

            // Update access time
            auto segment_it = segments_.find(segment_id);
            if (segment_it != segments_.end()) {
                segment_it->second->last_accessed =
                    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            }

            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Failed to read page %u: %s\n", page_id, e.what());
            return false;
        }
    }

    bool MultiSegmentManager::write_page(std::uint32_t page_id, const void* buffer,
                                         std::size_t buffer_size)
    {
        std::uint32_t segment_id = page_id_to_segment_id(page_id);
        std::uint32_t local_page = page_id_to_local_page(page_id);

        if (buffer_size != page_size_) {
            std::fprintf(stderr, "[MultiSegment] Buffer size mismatch for page write\n");
            return false;
        }

        std::lock_guard<std::mutex> lock(segments_mutex_);

        // Ensure segment is open
        if (segment_files_.find(segment_id) == segment_files_.end()) {
            if (!const_cast<MultiSegmentManager*>(this)->open_segment(segment_id)) {
                return false;
            }
        }

        auto file_it = segment_files_.find(segment_id);
        if (file_it == segment_files_.end()) {
            std::fprintf(stderr, "[MultiSegment] Segment %u file not open\n", segment_id);
            return false;
        }

        try {
            std::uint64_t offset = static_cast<std::uint64_t>(local_page) * page_size_;
            FileManager::pwrite(*file_it->second, buffer, page_size_, offset);

            // Update access time
            auto segment_it = segments_.find(segment_id);
            if (segment_it != segments_.end()) {
                segment_it->second->last_accessed =
                    std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            }

            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Failed to write page %u: %s\n", page_id, e.what());
            return false;
        }
    }

    std::vector<SegmentInfo> MultiSegmentManager::get_segment_info() const
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        std::vector<SegmentInfo> info_list;
        info_list.reserve(segments_.size());

        for (const auto& [segment_id, segment] : segments_) {
            info_list.push_back(*segment);
        }

        return info_list;
    }

    SegmentInfo MultiSegmentManager::get_segment_info(std::uint32_t segment_id) const
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        auto it = segments_.find(segment_id);
        if (it != segments_.end()) {
            return *it->second;
        }

        return SegmentInfo{}; // Return empty info if not found
    }

    std::uint32_t MultiSegmentManager::get_segment_count() const
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);
        return static_cast<std::uint32_t>(segments_.size());
    }

    std::uint64_t MultiSegmentManager::get_total_pages() const
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        std::uint64_t total = 0;
        for (const auto& [segment_id, segment] : segments_) {
            total += segment->max_pages;
        }
        return total;
    }

    std::uint64_t MultiSegmentManager::get_free_pages() const
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        std::uint64_t free = 0;
        for (const auto& [segment_id, segment] : segments_) {
            free += segment->free_pages;
        }
        return free;
    }

    std::uint64_t MultiSegmentManager::get_allocated_pages() const
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);

        std::uint64_t allocated = 0;
        for (const auto& [segment_id, segment] : segments_) {
            allocated += segment->current_pages;
        }
        return allocated;
    }

    double MultiSegmentManager::get_overall_utilization() const
    {
        std::uint64_t total = get_total_pages();
        if (total == 0)
            return 0.0;

        std::uint64_t allocated = get_allocated_pages();
        return static_cast<double>(allocated) / static_cast<double>(total);
    }

    // ========== Private Helper Methods ==========

    std::uint32_t MultiSegmentManager::find_best_segment_for_allocation()
    {
        switch (policy_.allocation_strategy) {
        case SegmentAllocationStrategy::ROUND_ROBIN:
            return allocate_round_robin();
        case SegmentAllocationStrategy::LEAST_UTILIZED:
            return allocate_least_utilized();
        case SegmentAllocationStrategy::BEST_FIT:
            return allocate_best_fit();
        case SegmentAllocationStrategy::SEQUENTIAL:
            return allocate_sequential();
        default:
            return allocate_least_utilized();
        }
    }

    bool MultiSegmentManager::load_existing_segments()
    {
        try {
            std::vector<std::string> segment_files;

            for (const auto& entry : std::filesystem::directory_iterator(base_path_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".seg") {
                    segment_files.push_back(entry.path().string());
                }
            }

            std::sort(segment_files.begin(), segment_files.end());

            for (const std::string& file_path : segment_files) {
                // Extract segment ID from filename
                std::string filename = std::filesystem::path(file_path).stem().string();
                if (filename.find("segment_") == 0) {
                    std::uint32_t segment_id = std::stoul(filename.substr(8));

                    // Create segment info (in a production system, this would be loaded from
                    // metadata)
                    auto segment_info = std::make_unique<SegmentInfo>();
                    segment_info->segment_id = segment_id;
                    segment_info->file_path = file_path;
                    segment_info->max_pages = policy_.max_segment_size / page_size_;
                    segment_info->current_pages = 0; // Would be loaded from metadata
                    segment_info->free_pages = segment_info->max_pages; // Would be calculated
                    segment_info->page_size = page_size_;
                    segment_info->is_active = true;

                    segments_[segment_id] = std::move(segment_info);

                    if (segment_id >= next_segment_id_.load()) {
                        next_segment_id_.store(segment_id + 1);
                    }
                }
            }

            std::fprintf(stderr, "[MultiSegment] Loaded %zu existing segments\n", segments_.size());
            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Failed to load existing segments: %s\n", e.what());
            return false;
        }
    }

    std::string MultiSegmentManager::get_segment_file_path(std::uint32_t segment_id) const
    {
        std::ostringstream oss;
        oss << base_path_ << "/segment_" << std::setfill('0') << std::setw(8) << segment_id
            << ".seg";
        return oss.str();
    }

    std::uint32_t MultiSegmentManager::allocate_round_robin()
    {
        if (segments_.empty())
            return 0;

        std::uint32_t start_counter = allocation_counter_.fetch_add(1);
        std::uint32_t attempts = 0;

        for (const auto& [segment_id, segment] : segments_) {
            if (attempts++ >= segments_.size())
                break;

            std::uint32_t index = (start_counter + attempts) % segments_.size();
            // Find segment by index (simplified approach)
            auto it = segments_.begin();
            std::advance(it, index);

            if (it->second->is_active && it->second->free_pages > 0) {
                return it->first;
            }
        }

        return 0;
    }

    std::uint32_t MultiSegmentManager::allocate_least_utilized()
    {
        std::uint32_t best_segment = 0;
        double lowest_utilization = 1.0;

        for (const auto& [segment_id, segment] : segments_) {
            if (segment->is_active && segment->free_pages > 0) {
                double util = segment->utilization();
                if (util < lowest_utilization) {
                    lowest_utilization = util;
                    best_segment = segment_id;
                }
            }
        }

        return best_segment;
    }

    std::uint32_t MultiSegmentManager::allocate_best_fit()
    {
        // For simplicity, this implementation just finds the segment with the most free space
        std::uint32_t best_segment = 0;
        std::uint64_t most_free = 0;

        for (const auto& [segment_id, segment] : segments_) {
            if (segment->is_active && segment->free_pages > most_free) {
                most_free = segment->free_pages;
                best_segment = segment_id;
            }
        }

        return best_segment;
    }

    std::uint32_t MultiSegmentManager::allocate_sequential()
    {
        // Find the first segment with available space
        for (const auto& [segment_id, segment] : segments_) {
            if (segment->is_active && segment->free_pages > 0) {
                return segment_id;
            }
        }

        return 0;
    }

    std::uint32_t MultiSegmentManager::page_id_to_segment_id(std::uint32_t page_id) const
    {
        // Simple encoding: upper 16 bits = segment ID, lower 16 bits = local page
        return (page_id >> 16) & 0xFFFF;
    }

    std::uint32_t MultiSegmentManager::page_id_to_local_page(std::uint32_t page_id) const
    {
        // Simple encoding: lower 16 bits = local page
        return page_id & 0xFFFF;
    }

    std::uint32_t MultiSegmentManager::make_page_id(std::uint32_t segment_id,
                                                    std::uint32_t local_page) const
    {
        // Simple encoding: upper 16 bits = segment ID, lower 16 bits = local page
        return (segment_id << 16) | (local_page & 0xFFFF);
    }

    void MultiSegmentManager::set_allocation_strategy(SegmentAllocationStrategy strategy)
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);
        policy_.allocation_strategy = strategy;
        std::fprintf(stderr, "[MultiSegment] Allocation strategy changed\n");
    }

    void MultiSegmentManager::set_auto_create_segments(bool enabled)
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);
        policy_.auto_create_segments = enabled;
    }

    void MultiSegmentManager::set_fragmentation_threshold(double threshold)
    {
        std::lock_guard<std::mutex> lock(segments_mutex_);
        policy_.fragmentation_threshold = threshold;
    }

    // ========== Utility Functions ==========

    bool migrate_single_segment_to_multi_segment(const std::string& single_segment_path,
                                                 const std::string& multi_segment_base_path)
    {
        try {
            std::fprintf(stderr, "[MultiSegment] Migrating single segment %s to multi-segment %s\n",
                         single_segment_path.c_str(), multi_segment_base_path.c_str());

            // Create multi-segment manager
            SegmentPolicy policy;
            MultiSegmentManager manager(multi_segment_base_path, policy);
            if (!manager.initialize()) {
                return false;
            }

            // Read source file and copy pages
            std::ifstream source(single_segment_path, std::ios::binary);
            if (!source.is_open()) {
                std::fprintf(stderr, "[MultiSegment] Cannot open source file %s\n",
                             single_segment_path.c_str());
                return false;
            }

            const std::size_t page_size = 4096;
            std::vector<char> page_buffer(page_size);
            std::uint32_t page_count = 0;

            while (source.read(page_buffer.data(), page_size)) {
                std::uint32_t page_id = manager.allocate_page();
                if (page_id == 0) {
                    std::fprintf(stderr,
                                 "[MultiSegment] Failed to allocate page during migration\n");
                    return false;
                }

                if (!manager.write_page(page_id, page_buffer.data(), page_size)) {
                    std::fprintf(stderr, "[MultiSegment] Failed to write page during migration\n");
                    return false;
                }

                page_count++;
            }

            std::fprintf(stderr, "[MultiSegment] Successfully migrated %u pages\n", page_count);
            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[MultiSegment] Migration failed: %s\n", e.what());
            return false;
        }
    }

} // namespace scratchbird::engine
