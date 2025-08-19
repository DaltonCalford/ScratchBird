#include "scratchbird/engine/wal.h"

#include "scratchbird/engine/file.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace scratchbird::engine
{
    WalManager::WalManager(const WalConfig& config)
        : config_(config), buffer_offset_(0), current_lsn_(1), current_segment_(1),
          last_checkpoint_lsn_(0), initialized_(false), open_(false), total_records_(0),
          total_bytes_(0)
    {
        write_buffer_.resize(config_.buffer_size_kb * 1024);
        std::cout << "[WAL] Initialized with buffer size: " << config_.buffer_size_kb << "KB"
                  << std::endl;
    }

    WalManager::~WalManager()
    {
        if (open_) {
            close();
        }
    }

    bool WalManager::initialize()
    {
        std::lock_guard<std::mutex> lock(wal_mutex_);

        if (initialized_) {
            return true;
        }

        try {
            // Create WAL directory if it doesn't exist
            if (!std::filesystem::exists(config_.wal_dir)) {
                std::filesystem::create_directories(config_.wal_dir);
                std::cout << "[WAL] Created WAL directory: " << config_.wal_dir << std::endl;
            }

            // Generate first WAL segment filename
            current_segment_path_ =
                config_.wal_dir + "/" + wal_util::generate_segment_filename(current_segment_);

            // Create and open first WAL segment
            FileOptions opts{};
            opts.direct_io = false;
            opts.preallocate_bytes = config_.segment_size_mb * 1024 * 1024;

            current_segment_file_ =
                std::make_unique<FileHandle>(FileManager::open(current_segment_path_, opts, true));
            if (!current_segment_file_->valid()) {
                std::cerr << "[WAL] Failed to create WAL segment: " << current_segment_path_
                          << std::endl;
                return false;
            }

            std::cout << "[WAL] Created WAL segment: " << current_segment_path_ << std::endl;
            initialized_ = true;
            open_ = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[WAL] Initialize failed: " << e.what() << std::endl;
            return false;
        }
    }

    bool WalManager::open()
    {
        std::lock_guard<std::mutex> lock(wal_mutex_);

        if (open_) {
            return true;
        }

        try {
            // Find existing WAL segments
            auto segments = wal_util::find_wal_segments(config_.wal_dir);
            if (segments.empty()) {
                return initialize();
            }

            // Sort and open the latest segment
            std::sort(segments.begin(), segments.end());
            current_segment_path_ = config_.wal_dir + "/" + segments.back();
            current_segment_ = wal_util::parse_segment_number(segments.back());

            FileOptions opts{};
            opts.direct_io = false;

            current_segment_file_ =
                std::make_unique<FileHandle>(FileManager::open(current_segment_path_, opts, false));
            if (!current_segment_file_->valid()) {
                std::cerr << "[WAL] Failed to open WAL segment: " << current_segment_path_
                          << std::endl;
                return false;
            }

            std::cout << "[WAL] Opened existing WAL segment: " << current_segment_path_
                      << std::endl;

            // TODO: Scan to find current LSN position from file
            open_ = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[WAL] Open failed: " << e.what() << std::endl;
            return false;
        }
    }

    void WalManager::close()
    {
        std::lock_guard<std::mutex> lock(wal_mutex_);

        if (!open_) {
            return;
        }

        try {
            flush();
            if (current_segment_file_) {
                current_segment_file_.reset(); // FileHandle destructor handles close
            }

            std::cout << "[WAL] Closed WAL manager" << std::endl;
            open_ = false;
        } catch (const std::exception& e) {
            std::cerr << "[WAL] Close failed: " << e.what() << std::endl;
        }
    }

    std::uint64_t WalManager::log_begin(std::uint64_t xid, std::uint32_t isolation_level,
                                        std::uint64_t snapshot_xid)
    {
        WalBeginRecord record{};
        record.header.record_size = sizeof(WalBeginRecord);
        record.header.type = WalRecordType::Begin;
        record.header.flags = 0;
        record.header.xid = xid;
        record.header.lsn = allocate_lsn();
        record.header.prev_lsn = 0; // First record for this transaction
        record.header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();
        record.isolation_level = isolation_level;
        record.snapshot_xid = snapshot_xid;
        record.header.checksum = calculate_checksum(
            &record.isolation_level, sizeof(record) - offsetof(WalBeginRecord, isolation_level));

        return write_record(&record, sizeof(record));
    }

    std::uint64_t WalManager::log_commit(std::uint64_t xid, std::uint64_t prev_lsn)
    {
        WalCommitRecord record{};
        record.header.record_size = sizeof(WalCommitRecord);
        record.header.type = WalRecordType::Commit;
        record.header.flags = 0;
        record.header.xid = xid;
        record.header.lsn = allocate_lsn();
        record.header.prev_lsn = prev_lsn;
        record.header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();
        record.commit_timestamp = record.header.timestamp_us;
        record.header.checksum = calculate_checksum(
            &record.commit_timestamp, sizeof(record) - offsetof(WalCommitRecord, commit_timestamp));

        return write_record(&record, sizeof(record));
    }

    std::uint64_t WalManager::log_rollback(std::uint64_t xid, std::uint64_t prev_lsn)
    {
        WalRollbackRecord record{};
        record.header.record_size = sizeof(WalRollbackRecord);
        record.header.type = WalRecordType::Rollback;
        record.header.flags = 0;
        record.header.xid = xid;
        record.header.lsn = allocate_lsn();
        record.header.prev_lsn = prev_lsn;
        record.header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                         std::chrono::system_clock::now().time_since_epoch())
                                         .count();
        record.rollback_timestamp = record.header.timestamp_us;
        record.header.checksum =
            calculate_checksum(&record.rollback_timestamp,
                               sizeof(record) - offsetof(WalRollbackRecord, rollback_timestamp));

        return write_record(&record, sizeof(record));
    }
    std::uint64_t WalManager::log_heap_insert(std::uint64_t xid, std::uint64_t prev_lsn,
                                              std::uint32_t space_id, std::uint32_t page_no,
                                              std::uint16_t slot_no, const void* tuple_data,
                                              std::uint32_t tuple_size)
    {
        std::uint32_t record_size = sizeof(WalHeapInsertRecord) + tuple_size;
        std::vector<std::uint8_t> buffer(record_size);

        auto* record = reinterpret_cast<WalHeapInsertRecord*>(buffer.data());
        record->header.record_size = record_size;
        record->header.type = WalRecordType::HeapInsert;
        record->header.flags = 0;
        record->header.xid = xid;
        record->header.lsn = allocate_lsn();
        record->header.prev_lsn = prev_lsn;
        record->header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
        record->space_id = space_id;
        record->page_no = page_no;
        record->slot_no = slot_no;
        record->tuple_size = tuple_size;

        // Copy tuple data
        std::memcpy(buffer.data() + sizeof(WalHeapInsertRecord), tuple_data, tuple_size);

        record->header.checksum =
            calculate_checksum(buffer.data() + offsetof(WalHeapInsertRecord, space_id),
                               record_size - offsetof(WalHeapInsertRecord, space_id));

        return write_record(buffer.data(), record_size);
    }

    std::uint64_t WalManager::log_heap_update(std::uint64_t xid, std::uint64_t prev_lsn,
                                              std::uint32_t space_id, std::uint32_t page_no,
                                              std::uint16_t slot_no, const void* old_tuple_data,
                                              std::uint32_t old_tuple_size,
                                              const void* new_tuple_data,
                                              std::uint32_t new_tuple_size)
    {
        std::uint32_t record_size = sizeof(WalHeapUpdateRecord) + old_tuple_size + new_tuple_size;
        std::vector<std::uint8_t> buffer(record_size);

        auto* record = reinterpret_cast<WalHeapUpdateRecord*>(buffer.data());
        record->header.record_size = record_size;
        record->header.type = WalRecordType::HeapUpdate;
        record->header.flags = 0;
        record->header.xid = xid;
        record->header.lsn = allocate_lsn();
        record->header.prev_lsn = prev_lsn;
        record->header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
        record->space_id = space_id;
        record->page_no = page_no;
        record->slot_no = slot_no;
        record->old_tuple_size = old_tuple_size;
        record->new_tuple_size = new_tuple_size;

        // Copy tuple data
        std::uint8_t* data_ptr = buffer.data() + sizeof(WalHeapUpdateRecord);
        std::memcpy(data_ptr, old_tuple_data, old_tuple_size);
        data_ptr += old_tuple_size;
        std::memcpy(data_ptr, new_tuple_data, new_tuple_size);

        record->header.checksum =
            calculate_checksum(buffer.data() + offsetof(WalHeapUpdateRecord, space_id),
                               record_size - offsetof(WalHeapUpdateRecord, space_id));

        return write_record(buffer.data(), record_size);
    }

    std::uint64_t WalManager::log_heap_delete(std::uint64_t xid, std::uint64_t prev_lsn,
                                              std::uint32_t space_id, std::uint32_t page_no,
                                              std::uint16_t slot_no, const void* tuple_data,
                                              std::uint32_t tuple_size)
    {
        std::uint32_t record_size = sizeof(WalHeapDeleteRecord) + tuple_size;
        std::vector<std::uint8_t> buffer(record_size);

        auto* record = reinterpret_cast<WalHeapDeleteRecord*>(buffer.data());
        record->header.record_size = record_size;
        record->header.type = WalRecordType::HeapDelete;
        record->header.flags = 0;
        record->header.xid = xid;
        record->header.lsn = allocate_lsn();
        record->header.prev_lsn = prev_lsn;
        record->header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
        record->space_id = space_id;
        record->page_no = page_no;
        record->slot_no = slot_no;
        record->tuple_size = tuple_size;

        // Copy tuple data
        std::memcpy(buffer.data() + sizeof(WalHeapDeleteRecord), tuple_data, tuple_size);

        record->header.checksum =
            calculate_checksum(buffer.data() + offsetof(WalHeapDeleteRecord, space_id),
                               record_size - offsetof(WalHeapDeleteRecord, space_id));

        return write_record(buffer.data(), record_size);
    }
    std::uint64_t WalManager::log_page_write(std::uint64_t xid, std::uint64_t prev_lsn,
                                             std::uint32_t space_id, std::uint32_t page_no,
                                             std::uint32_t offset, const void* data,
                                             std::uint32_t length)
    {
        std::uint32_t record_size = sizeof(WalPageWriteRecord) + length;
        std::vector<std::uint8_t> buffer(record_size);

        auto* record = reinterpret_cast<WalPageWriteRecord*>(buffer.data());
        record->header.record_size = record_size;
        record->header.type = WalRecordType::PageWrite;
        record->header.flags = 0;
        record->header.xid = xid;
        record->header.lsn = allocate_lsn();
        record->header.prev_lsn = prev_lsn;
        record->header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
        record->space_id = space_id;
        record->page_no = page_no;
        record->offset = offset;
        record->length = length;

        // Copy page data
        std::memcpy(buffer.data() + sizeof(WalPageWriteRecord), data, length);

        record->header.checksum =
            calculate_checksum(buffer.data() + offsetof(WalPageWriteRecord, space_id),
                               record_size - offsetof(WalPageWriteRecord, space_id));

        return write_record(buffer.data(), record_size);
    }

    std::uint64_t WalManager::log_checkpoint(
        const std::vector<std::pair<std::uint32_t, std::uint32_t>>& dirty_pages)
    {
        std::uint32_t record_size =
            sizeof(WalCheckpointRecord) +
            dirty_pages.size() * sizeof(std::pair<std::uint32_t, std::uint32_t>);
        std::vector<std::uint8_t> buffer(record_size);

        auto* record = reinterpret_cast<WalCheckpointRecord*>(buffer.data());
        record->header.record_size = record_size;
        record->header.type = WalRecordType::Checkpoint;
        record->header.flags = 0;
        record->header.xid = 0; // System transaction
        record->header.lsn = allocate_lsn();
        record->header.prev_lsn = 0;
        record->header.timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count();
        record->checkpoint_lsn = record->header.lsn;
        record->redo_lsn = last_checkpoint_lsn_.load();
        record->num_dirty_pages = static_cast<std::uint32_t>(dirty_pages.size());

        // Copy dirty pages list
        std::memcpy(buffer.data() + sizeof(WalCheckpointRecord), dirty_pages.data(),
                    dirty_pages.size() * sizeof(std::pair<std::uint32_t, std::uint32_t>));

        record->header.checksum =
            calculate_checksum(buffer.data() + offsetof(WalCheckpointRecord, checkpoint_lsn),
                               record_size - offsetof(WalCheckpointRecord, checkpoint_lsn));

        auto lsn = write_record(buffer.data(), record_size);
        last_checkpoint_lsn_.store(lsn);
        return lsn;
    }

    bool WalManager::perform_checkpoint()
    {
        // TODO: Implement full checkpoint logic
        // 1. Flush all dirty pages
        // 2. Write checkpoint record
        // 3. Truncate old WAL segments
        std::cout << "[WAL] Performing checkpoint (stub)" << std::endl;
        return true;
    }

    bool WalManager::recover_database(FileMap& fmap)
    {
        // TODO: Implement full crash recovery
        // 1. Scan WAL segments from last checkpoint
        // 2. Apply REDO for committed transactions
        // 3. Apply UNDO for uncommitted transactions
        std::cout << "[WAL] Recovering database (stub)" << std::endl;
        (void)fmap; // Suppress warning
        return true;
    }

    std::uint64_t WalManager::get_last_checkpoint_lsn() const
    {
        return last_checkpoint_lsn_.load();
    }

    std::uint64_t WalManager::get_current_lsn() const
    {
        return current_lsn_.load();
    }

    bool WalManager::truncate_wal(std::uint64_t target_lsn)
    {
        // TODO: Implement WAL truncation after checkpoint
        std::cout << "[WAL] Truncating WAL to LSN " << target_lsn << " (stub)" << std::endl;
        return true;
    }

    void WalManager::flush()
    {
        if (!open_ || buffer_offset_ == 0) {
            return;
        }

        if (current_segment_file_) {
            FileManager::pwrite(*current_segment_file_, write_buffer_.data(), buffer_offset_, 0);
            if (config_.fsync_enabled) {
                FileManager::flush(*current_segment_file_);
            }
        }

        buffer_offset_ = 0;
    }

    void WalManager::fsync()
    {
        if (current_segment_file_) {
            FileManager::flush(*current_segment_file_);
        }
    }

    WalManager::WalStats WalManager::get_stats() const
    {
        WalStats stats{};
        stats.total_records = total_records_.load();
        stats.total_bytes_written = total_bytes_.load();
        stats.current_segment = current_segment_.load();
        stats.current_lsn = current_lsn_.load();
        stats.active_segments = 1; // TODO: Calculate actual active segments
        return stats;
    }

    // Private helper methods implementation
    std::uint64_t WalManager::allocate_lsn()
    {
        return current_lsn_++;
    }

    std::uint64_t WalManager::write_record(const void* record, std::uint32_t size)
    {
        std::lock_guard<std::mutex> lock(wal_mutex_);

        if (!ensure_space(size)) {
            return 0; // Failed to write
        }

        // Copy to buffer
        std::memcpy(write_buffer_.data() + buffer_offset_, record, size);
        buffer_offset_ += size;

        total_records_++;
        total_bytes_ += size;

        auto* header = reinterpret_cast<const WalRecordHeader*>(record);

        // Flush if buffer is getting full or if this is a commit/rollback
        if (buffer_offset_ > (write_buffer_.size() * 3 / 4) ||
            header->type == WalRecordType::Commit || header->type == WalRecordType::Rollback) {
            flush();
        }

        return header->lsn;
    }

    bool WalManager::ensure_space(std::uint32_t required_bytes)
    {
        if (buffer_offset_ + required_bytes <= write_buffer_.size()) {
            return true;
        }

        // Flush current buffer
        flush();

        // Check if we need to rotate segment
        // TODO: Check actual segment file size and rotate if needed
        return true;
    }

    std::uint16_t WalManager::calculate_checksum(const void* data, std::uint32_t size)
    {
        // Simple CRC16 implementation
        std::uint16_t crc = 0xFFFF;
        const std::uint8_t* bytes = reinterpret_cast<const std::uint8_t*>(data);

        for (std::uint32_t i = 0; i < size; ++i) {
            crc ^= bytes[i];
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc = crc >> 1;
                }
            }
        }

        return crc;
    }

    // Utility functions implementation
    namespace wal_util
    {

        std::string generate_segment_filename(std::uint64_t segment_no)
        {
            std::ostringstream oss;
            oss << "wal_" << std::setfill('0') << std::setw(16) << std::hex << segment_no << ".log";
            return oss.str();
        }

        std::uint64_t parse_segment_number(const std::string& filename)
        {
            if (filename.length() < 20 || filename.compare(0, 4, "wal_") != 0 ||
                filename.compare(filename.length() - 4, 4, ".log") != 0) {
                return 0;
            }

            std::string hex_part = filename.substr(4, 16);
            return std::stoull(hex_part, nullptr, 16);
        }

        std::vector<std::string> find_wal_segments(const std::string& wal_dir)
        {
            std::vector<std::string> segments;

            if (!std::filesystem::exists(wal_dir)) {
                return segments;
            }

            for (const auto& entry : std::filesystem::directory_iterator(wal_dir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.compare(0, 4, "wal_") == 0 &&
                        filename.compare(filename.length() - 4, 4, ".log") == 0) {
                        segments.push_back(filename);
                    }
                }
            }

            return segments;
        }

        bool validate_wal_record(const WalRecordHeader* header, const void* data)
        {
            if (!header || !data) {
                return false;
            }

            // Basic validation
            if (header->record_size < sizeof(WalRecordHeader)) {
                return false;
            }

            if (header->type == WalRecordType::Invalid) {
                return false;
            }

            // TODO: Validate checksum
            return true;
        }

    } // namespace wal_util

} // namespace scratchbird::engine
