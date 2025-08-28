#include "scratchbird/engine/buffer_pool.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace ScratchBird
{
    // BufferDescriptor implementation
    BufferDescriptor::BufferDescriptor()
        : last_access_time_(std::chrono::steady_clock::now())
    {
    }
    
    BufferTag BufferDescriptor::get_tag() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return tag_;
    }
    
    void BufferDescriptor::set_tag(const BufferTag& tag) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        tag_ = tag;
    }
    
    bool BufferDescriptor::matches_tag(const BufferTag& tag) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return tag_ == tag;
    }
    
    BufferState BufferDescriptor::get_state() const {
        return state_.load();
    }
    
    void BufferDescriptor::set_state(BufferState state) {
        state_.store(state);
    }
    
    bool BufferDescriptor::is_dirty() const {
        BufferState state = state_.load();
        return state == BufferState::DIRTY || state == BufferState::WRITING;
    }
    
    bool BufferDescriptor::is_valid() const {
        BufferState state = state_.load();
        return state == BufferState::VALID || state == BufferState::DIRTY || 
               state == BufferState::PINNED || state == BufferState::WRITING;
    }
    
    bool BufferDescriptor::is_pinned() const {
        return pin_count_.load() > 0;
    }
    
    int BufferDescriptor::get_pin_count() const {
        return pin_count_.load();
    }
    
    void BufferDescriptor::pin() {
        pin_count_.fetch_add(1);
        update_access_time();
    }
    
    void BufferDescriptor::unpin() {
        int prev_count = pin_count_.fetch_sub(1);
        if (prev_count <= 1) {
            // Last pin removed, buffer can be evicted
            pin_count_.store(0);
        }
    }
    
    bool BufferDescriptor::try_pin() {
        int current_count = pin_count_.load();
        if (current_count < 0) return false; // Invalid state
        
        if (pin_count_.compare_exchange_weak(current_count, current_count + 1)) {
            update_access_time();
            return true;
        }
        return false;
    }
    
    bool BufferDescriptor::get_usage_bit() const {
        return usage_bit_.load();
    }
    
    void BufferDescriptor::set_usage_bit(bool used) {
        usage_bit_.store(used);
    }
    
    bool BufferDescriptor::clear_usage_bit() {
        return usage_bit_.exchange(false);
    }
    
    LSN BufferDescriptor::get_lsn() const {
        return page_lsn_.load();
    }
    
    void BufferDescriptor::set_lsn(LSN lsn) {
        page_lsn_.store(lsn);
    }
    
    std::chrono::steady_clock::time_point BufferDescriptor::get_last_access_time() const {
        return last_access_time_.load();
    }
    
    void BufferDescriptor::update_access_time() {
        last_access_time_.store(std::chrono::steady_clock::now());
        access_count_.fetch_add(1);
        set_usage_bit(true);
    }
    
    uint64_t BufferDescriptor::get_access_count() const {
        return access_count_.load();
    }

    // BufferFrame implementation
    BufferFrame::BufferFrame(size_t buffer_size) : size_(buffer_size) {
        // Allocate aligned memory for buffer frame
#ifdef _WIN32
        data_ = static_cast<char*>(_aligned_malloc(buffer_size, 4096));
#else
        if (posix_memalign(reinterpret_cast<void**>(&data_), 4096, buffer_size) != 0) {
            data_ = nullptr;
        }
#endif
        
        if (data_ == nullptr) {
            throw std::bad_alloc();
        }
        
        zero_fill();
    }
    
    BufferFrame::~BufferFrame() {
        if (data_ != nullptr) {
#ifdef _WIN32
            _aligned_free(data_);
#else
            free(data_);
#endif
        }
    }
    
    void BufferFrame::zero_fill() {
        std::memset(data_, 0, size_);
    }
    
    void BufferFrame::copy_from(const char* source, size_t length) {
        if (length > size_) {
            length = size_;
        }
        std::memcpy(data_, source, length);
        if (length < size_) {
            std::memset(data_ + length, 0, size_ - length);
        }
    }
    
    void BufferFrame::copy_to(char* destination, size_t length) const {
        if (length > size_) {
            length = size_;
        }
        std::memcpy(destination, data_, length);
    }

    // BufferPoolStats implementation
    void BufferPoolStats::reset() {
        buffer_hits.store(0);
        buffer_misses.store(0);
        buffer_reads.store(0);
        buffer_writes.store(0);
        buffers_used.store(0);
        buffers_dirty.store(0);
        buffers_pinned.store(0);
        evictions_clean.store(0);
        evictions_dirty.store(0);
        clock_sweeps.store(0);
        lock_waits.store(0);
        lock_timeouts.store(0);
        avg_lookup_time_ns.store(0);
        background_writes.store(0);
        checkpoint_writes.store(0);
        last_reset_time = std::chrono::steady_clock::now();
    }

    // BufferPool implementation
    BufferPool::BufferPool(const BufferPoolConfig& config)
        : config_(config)
    {
        stats_.last_reset_time = std::chrono::steady_clock::now();
    }

    BufferPool::~BufferPool() {
        shutdown();
    }

    std::error_code BufferPool::initialize() {
        // Validate configuration
        if (auto validation_error = validate_config(config_); !validation_error.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        try {
            // Allocate buffer frames
            buffer_frames_.reserve(config_.num_buffers);
            for (size_t i = 0; i < config_.num_buffers; ++i) {
                buffer_frames_.emplace_back(std::make_unique<BufferFrame>(config_.buffer_size));
            }

            // Allocate buffer descriptors
            buffer_descriptors_.reserve(config_.num_buffers);
            for (size_t i = 0; i < config_.num_buffers; ++i) {
                buffer_descriptors_.emplace_back(std::make_unique<BufferDescriptor>());
            }

            // Initialize free buffer list
            free_buffers_.reserve(config_.num_buffers);
            for (size_t i = 0; i < config_.num_buffers; ++i) {
                free_buffers_.push_back(static_cast<int>(i));
            }

            // Reserve hash table capacity
            buffer_hash_table_.reserve(config_.hash_table_size);

            // Start background writer thread if enabled
            shutdown_requested_ = false;
            if (config_.enable_background_writer) {
                background_writer_thread_ = std::thread(&BufferPool::background_writer_loop, this);
            }

            return {};
        } catch (const std::exception&) {
            return std::make_error_code(std::errc::not_enough_memory);
        }
    }

    void BufferPool::shutdown() {
        // Signal shutdown to background thread
        shutdown_requested_ = true;
        background_writer_cv_.notify_all();

        // Wait for background thread to finish
        if (background_writer_thread_.joinable()) {
            background_writer_thread_.join();
        }

        // Flush all dirty buffers (only if initialized)
        if (!buffer_descriptors_.empty()) {
            flush_all_buffers();
        }

        // Clear data structures
        {
            std::unique_lock<std::shared_mutex> lock(hash_table_mutex_);
            buffer_hash_table_.clear();
        }

        buffer_frames_.clear();
        buffer_descriptors_.clear();
        free_buffers_.clear();
    }

    int BufferPool::get_buffer(const BufferTag& tag, bool& found) {
        auto start_time = std::chrono::high_resolution_clock::now();

        found = false;

        // First, try to find existing buffer
        {
            std::shared_lock<std::shared_mutex> hash_lock(hash_table_mutex_);
            auto it = buffer_hash_table_.find(tag);
            if (it != buffer_hash_table_.end()) {
                int buffer_id = it->second;
                auto* descriptor = buffer_descriptors_[buffer_id].get();
                
                if (descriptor->matches_tag(tag) && descriptor->is_valid()) {
                    descriptor->pin();
                    found = true;
                    update_stats(true, false, false);
                    
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
                    stats_.avg_lookup_time_ns.store(duration.count());
                    
                    return buffer_id;
                }
            }
        }

        // Buffer not found, need to allocate or find victim
        int buffer_id = allocate_buffer();
        if (buffer_id == INVALID_BUFFER_ID) {
            // No free buffers, find victim
            buffer_id = find_victim_buffer();
            if (buffer_id == INVALID_BUFFER_ID) {
                return INVALID_BUFFER_ID; // No buffers available
            }
        }

        auto* descriptor = buffer_descriptors_[buffer_id].get();
        
        // Remove old mapping if exists
        BufferTag old_tag = descriptor->get_tag();
        if (old_tag.is_valid()) {
            std::unique_lock<std::shared_mutex> hash_lock(hash_table_mutex_);
            buffer_hash_table_.erase(old_tag);
        }

        // Set new tag and state
        descriptor->set_tag(tag);
        descriptor->set_state(BufferState::READING);
        descriptor->pin();

        // Add new mapping
        {
            std::unique_lock<std::shared_mutex> hash_lock(hash_table_mutex_);
            buffer_hash_table_[tag] = buffer_id;
        }

        // TODO: Read page from disk here
        // For now, just mark as valid
        descriptor->set_state(BufferState::VALID);

        found = false; // This is a new buffer
        update_stats(false, true, false);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        stats_.avg_lookup_time_ns.store(duration.count());

        return buffer_id;
    }

    void BufferPool::release_buffer(int buffer_id, bool mark_dirty) {
        if (!is_valid_buffer_id(buffer_id)) {
            return;
        }

        auto* descriptor = buffer_descriptors_[buffer_id].get();
        
        if (mark_dirty && descriptor->get_state() == BufferState::VALID) {
            descriptor->set_state(BufferState::DIRTY);
        }
        
        descriptor->unpin();
    }

    bool BufferPool::pin_buffer(int buffer_id) {
        if (!is_valid_buffer_id(buffer_id)) {
            return false;
        }

        auto* descriptor = buffer_descriptors_[buffer_id].get();
        return descriptor->try_pin();
    }

    void BufferPool::unpin_buffer(int buffer_id) {
        if (!is_valid_buffer_id(buffer_id)) {
            return;
        }

        auto* descriptor = buffer_descriptors_[buffer_id].get();
        descriptor->unpin();
    }

    std::error_code BufferPool::flush_buffer(int buffer_id) {
        if (!is_valid_buffer_id(buffer_id)) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        return write_buffer(buffer_id);
    }

    size_t BufferPool::flush_all_buffers() {
        size_t flushed_count = 0;

        for (size_t i = 0; i < buffer_descriptors_.size() && i < config_.num_buffers; ++i) {
            auto* descriptor = buffer_descriptors_[i].get();
            if (descriptor && descriptor->is_dirty()) {
                if (write_buffer(static_cast<int>(i)) == std::error_code{}) {
                    ++flushed_count;
                }
            }
        }

        return flushed_count;
    }

    char* BufferPool::get_buffer_data(int buffer_id) {
        if (!is_valid_buffer_id(buffer_id)) {
            return nullptr;
        }
        return buffer_frames_[buffer_id]->get_data();
    }

    const char* BufferPool::get_buffer_data(int buffer_id) const {
        if (!is_valid_buffer_id(buffer_id)) {
            return nullptr;
        }
        return buffer_frames_[buffer_id]->get_data();
    }

    BufferDescriptor* BufferPool::get_buffer_descriptor(int buffer_id) {
        if (!is_valid_buffer_id(buffer_id)) {
            return nullptr;
        }
        return buffer_descriptors_[buffer_id].get();
    }

    const BufferDescriptor* BufferPool::get_buffer_descriptor(int buffer_id) const {
        if (!is_valid_buffer_id(buffer_id)) {
            return nullptr;
        }
        return buffer_descriptors_[buffer_id].get();
    }

    size_t BufferPool::invalidate_relation_buffers(RelationOid relation_oid) {
        size_t invalidated_count = 0;

        // Find all buffers for this relation
        std::vector<int> buffers_to_invalidate;
        
        {
            std::shared_lock<std::shared_mutex> hash_lock(hash_table_mutex_);
            for (const auto& [tag, buffer_id] : buffer_hash_table_) {
                if (tag.relation_oid == relation_oid) {
                    buffers_to_invalidate.push_back(buffer_id);
                }
            }
        }

        // Invalidate buffers (must be unpinned)
        for (int buffer_id : buffers_to_invalidate) {
            auto* descriptor = buffer_descriptors_[buffer_id].get();
            if (!descriptor->is_pinned()) {
                BufferTag old_tag = descriptor->get_tag();
                
                // Flush if dirty
                if (descriptor->is_dirty()) {
                    write_buffer(buffer_id);
                }

                // Remove from hash table
                {
                    std::unique_lock<std::shared_mutex> hash_lock(hash_table_mutex_);
                    buffer_hash_table_.erase(old_tag);
                }

                // Reset descriptor
                descriptor->set_state(BufferState::INVALID);
                descriptor->set_tag(BufferTag{});

                // Add to free list
                {
                    std::lock_guard<std::mutex> free_lock(free_list_mutex_);
                    free_buffers_.push_back(buffer_id);
                }

                ++invalidated_count;
            }
        }

        return invalidated_count;
    }

    BufferPoolStats BufferPool::get_stats() const {
        return stats_;
    }

    void BufferPool::reset_stats() {
        stats_.reset();
    }

    std::error_code BufferPool::update_config(const BufferPoolConfig& new_config) {
        if (auto validation_error = validate_config(new_config); !validation_error.empty()) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        // Only update parameters that can be changed at runtime
        config_.dirty_page_threshold = new_config.dirty_page_threshold;
        config_.background_write_interval = new_config.background_write_interval;
        config_.enable_statistics = new_config.enable_statistics;
        config_.stats_report_interval = new_config.stats_report_interval;
        config_.clock_hand_advance_size = new_config.clock_hand_advance_size;
        config_.use_adaptive_replacement = new_config.use_adaptive_replacement;
        config_.enable_prefetch = new_config.enable_prefetch;

        return {};
    }

    std::string BufferPool::validate_config(const BufferPoolConfig& config) {
        std::ostringstream errors;

        if (config.num_buffers == 0) {
            errors << "num_buffers must be greater than 0; ";
        }

        if (config.buffer_size == 0) {
            errors << "buffer_size must be greater than 0; ";
        }

        if (config.hash_table_size == 0) {
            errors << "hash_table_size must be greater than 0; ";
        }

        if (config.dirty_page_threshold <= 0.0 || config.dirty_page_threshold > 1.0) {
            errors << "dirty_page_threshold must be between 0.0 and 1.0; ";
        }

        if (config.clock_hand_advance_size == 0) {
            errors << "clock_hand_advance_size must be greater than 0; ";
        }

        return errors.str();
    }

    BufferPool::UsageInfo BufferPool::get_usage_info() const {
        UsageInfo info;
        info.total_buffers = config_.num_buffers;
        info.used_buffers = stats_.buffers_used.load();
        info.dirty_buffers = stats_.buffers_dirty.load();
        info.pinned_buffers = stats_.buffers_pinned.load();
        info.usage_percentage = static_cast<double>(info.used_buffers) / info.total_buffers * 100.0;
        info.hit_ratio = stats_.get_hit_ratio() * 100.0;
        return info;
    }

    // Private methods
    int BufferPool::find_victim_buffer() {
        std::lock_guard<std::mutex> clock_lock(clock_mutex_);

        size_t start_position = clock_hand_.load();
        size_t current_position = start_position;
        size_t buffers_checked = 0;

        stats_.clock_sweeps.fetch_add(1);

        while (buffers_checked < config_.num_buffers) {
            auto* descriptor = buffer_descriptors_[current_position].get();

            // Skip pinned buffers
            if (descriptor->is_pinned()) {
                current_position = (current_position + 1) % config_.num_buffers;
                ++buffers_checked;
                continue;
            }

            // Check usage bit (clock-sweep algorithm)
            if (descriptor->clear_usage_bit()) {
                // Recently used, give second chance
                current_position = (current_position + 1) % config_.num_buffers;
                ++buffers_checked;
                continue;
            }

            // Found victim
            clock_hand_.store((current_position + 1) % config_.num_buffers);

            // Evict buffer if needed
            if (descriptor->is_valid()) {
                if (evict_buffer(static_cast<int>(current_position)) != std::error_code{}) {
                    // Eviction failed, try next buffer
                    current_position = (current_position + 1) % config_.num_buffers;
                    ++buffers_checked;
                    continue;
                }
            }

            return static_cast<int>(current_position);
        }

        // No victim found
        return INVALID_BUFFER_ID;
    }

    int BufferPool::allocate_buffer() {
        std::lock_guard<std::mutex> lock(free_list_mutex_);
        
        if (free_buffers_.empty()) {
            return INVALID_BUFFER_ID;
        }

        int buffer_id = free_buffers_.back();
        free_buffers_.pop_back();
        return buffer_id;
    }

    std::error_code BufferPool::evict_buffer(int buffer_id) {
        auto* descriptor = buffer_descriptors_[buffer_id].get();

        // Write to disk if dirty
        if (descriptor->is_dirty()) {
            auto error = write_buffer(buffer_id);
            if (error != std::error_code{}) {
                return error;
            }
            stats_.evictions_dirty.fetch_add(1);
        } else {
            stats_.evictions_clean.fetch_add(1);
        }

        // Remove from hash table
        BufferTag old_tag = descriptor->get_tag();
        if (old_tag.is_valid()) {
            std::unique_lock<std::shared_mutex> hash_lock(hash_table_mutex_);
            buffer_hash_table_.erase(old_tag);
        }

        // Reset descriptor
        descriptor->set_state(BufferState::INVALID);
        descriptor->set_tag(BufferTag{});

        return {};
    }

    void BufferPool::background_writer_loop() {
        std::unique_lock<std::mutex> lock(background_writer_mutex_);

        while (!shutdown_requested_) {
            background_writer_cv_.wait_for(lock, config_.background_write_interval);

            if (shutdown_requested_) break;

            // Check if we need to write dirty buffers
            double dirty_ratio = stats_.get_dirty_ratio();
            if (dirty_ratio > config_.dirty_page_threshold) {
                size_t target_writes = static_cast<size_t>(config_.num_buffers * 0.1); // Write 10% of buffers
                size_t writes_performed = 0;

                for (size_t i = 0; i < config_.num_buffers && writes_performed < target_writes; ++i) {
                    auto* descriptor = buffer_descriptors_[i].get();
                    if (descriptor->is_dirty() && !descriptor->is_pinned()) {
                        if (write_buffer(static_cast<int>(i)) == std::error_code{}) {
                            ++writes_performed;
                            stats_.background_writes.fetch_add(1);
                        }
                    }
                }
            }
        }
    }

    std::error_code BufferPool::write_buffer(int buffer_id) {
        auto* descriptor = buffer_descriptors_[buffer_id].get();
        
        descriptor->set_state(BufferState::WRITING);

        // TODO: Actually write to disk here
        // For now, just simulate successful write
        
        descriptor->set_state(BufferState::VALID);
        update_stats(false, false, true);

        return {};
    }

    void BufferPool::update_stats(bool hit, bool read, bool write) {
        if (!config_.enable_statistics) return;

        if (hit) {
            stats_.buffer_hits.fetch_add(1);
        } else {
            stats_.buffer_misses.fetch_add(1);
        }

        if (read) {
            stats_.buffer_reads.fetch_add(1);
        }

        if (write) {
            stats_.buffer_writes.fetch_add(1);
        }

        // Update buffer utilization counts
        size_t used_count = 0, dirty_count = 0, pinned_count = 0;
        for (size_t i = 0; i < config_.num_buffers; ++i) {
            auto* descriptor = buffer_descriptors_[i].get();
            if (descriptor->is_valid()) {
                ++used_count;
                if (descriptor->is_dirty()) ++dirty_count;
                if (descriptor->is_pinned()) ++pinned_count;
            }
        }

        stats_.buffers_used.store(used_count);
        stats_.buffers_dirty.store(dirty_count);
        stats_.buffers_pinned.store(pinned_count);
    }

} // namespace ScratchBird