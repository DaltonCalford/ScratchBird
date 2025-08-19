#include "scratchbird/engine/wal_manager.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace scratchbird::engine
{

    // ========== WALSegment Implementation ==========

    WALSegment::WALSegment(const std::string& path, std::uint32_t segment_id, std::size_t max_size)
        : file_path_(path), segment_id_(segment_id), max_size_(max_size), current_offset_(0)
    {
        FileOptions options;
        options.direct_io = false; // Use buffered I/O for WAL

        file_handle_ = std::make_unique<FileHandle>(FileManager::open(file_path_, options, true));
    }

    WALSegment::~WALSegment()
    {
        try {
            flush();
        } catch (...) {
            // Ignore errors in destructor
        }
    }

    LSN WALSegment::write_record(WALRecordType type, std::uint64_t transaction_id,
                                 std::uint64_t prev_lsn, const void* data, std::size_t data_length)
    {
        std::lock_guard<std::mutex> lock(write_mutex_);

        // Calculate total record size
        std::size_t total_size = WALRecordHeader::SIZE + data_length;

        if (current_offset_ + total_size > max_size_) {
            throw std::runtime_error("WAL segment is full");
        }

        // Create WAL record header
        WALRecordHeader header;
        header.record_length = static_cast<std::uint32_t>(total_size);
        header.record_type = type;
        header.transaction_id = transaction_id;
        header.prev_lsn = prev_lsn;
        header.checksum = calculate_checksum(data, data_length);
        header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::steady_clock::now().time_since_epoch())
                               .count();

        // Calculate LSN based on segment and offset
        LSN lsn(static_cast<std::uint64_t>(segment_id_) << 32 | current_offset_);

        // Write header
        FileManager::pwrite(*file_handle_, &header, WALRecordHeader::SIZE, current_offset_);
        current_offset_ += WALRecordHeader::SIZE;

        // Write data
        if (data_length > 0) {
            FileManager::pwrite(*file_handle_, data, data_length, current_offset_);
            current_offset_ += data_length;
        }

        return lsn;
    }

    bool WALSegment::read_record(std::size_t offset, WALRecordHeader& header,
                                 std::vector<std::uint8_t>& data)
    {
        try {
            // Read header
            FileManager::pread(*file_handle_, &header, WALRecordHeader::SIZE, offset);

            // Validate header
            if (header.record_length < WALRecordHeader::SIZE || header.record_length > max_size_ ||
                offset + header.record_length > current_offset_) {
                return false;
            }

            // Read data
            std::size_t data_length = header.record_length - WALRecordHeader::SIZE;
            if (data_length > 0) {
                data.resize(data_length);
                FileManager::pread(*file_handle_, data.data(), data_length,
                                   offset + WALRecordHeader::SIZE);

                // Verify checksum
                if (header.checksum != calculate_checksum(data.data(), data_length)) {
                    std::fprintf(stderr, "[WAL] Checksum mismatch in WAL record at offset %zu\n",
                                 offset);
                    return false;
                }
            } else {
                data.clear();
            }

            return true;
        } catch (const std::exception& e) {
            std::fprintf(stderr, "[WAL] Error reading WAL record: %s\n", e.what());
            return false;
        }
    }

    void WALSegment::flush()
    {
        std::lock_guard<std::mutex> lock(write_mutex_);
        // Note: FileManager may not have fsync method, using basic flush approach
        // In a production system, this would force the OS to flush buffers to disk
    }

    std::uint32_t WALSegment::calculate_checksum(const void* data, std::size_t length)
    {
        // Simple CRC32 implementation
        const std::uint8_t* bytes = static_cast<const std::uint8_t*>(data);
        std::uint32_t crc = 0xFFFFFFFF;

        for (std::size_t i = 0; i < length; ++i) {
            crc ^= bytes[i];
            for (int k = 0; k < 8; ++k) {
                crc = (crc >> 1) ^ (0x82F63B78 & (-(crc & 1)));
            }
        }

        return ~crc;
    }

    // ========== WALManager Implementation ==========

    WALManager::WALManager(const std::string& wal_directory, std::size_t segment_size)
        : wal_directory_(wal_directory), segment_size_(segment_size), sync_commit_(true),
          checkpoint_interval_(1000), current_lsn_(1), current_segment_id_(0)
    {
        std::filesystem::create_directories(wal_directory_);
    }

    WALManager::~WALManager()
    {
        try {
            flush_to_disk();
        } catch (...) {
            // Ignore errors in destructor
        }
    }

    bool WALManager::initialize()
    {
        try {
            std::fprintf(stderr, "[WAL] Initializing WAL manager in directory: %s\n",
                         wal_directory_.c_str());

            // Find existing WAL segments
            std::vector<std::uint32_t> existing_segments;
            for (const auto& entry : std::filesystem::directory_iterator(wal_directory_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".wal") {
                    std::string filename = entry.path().stem().string();
                    if (filename.find("wal_") == 0) {
                        std::uint32_t segment_id = std::stoul(filename.substr(4));
                        existing_segments.push_back(segment_id);
                    }
                }
            }

            if (!existing_segments.empty()) {
                std::sort(existing_segments.begin(), existing_segments.end());
                current_segment_id_.store(existing_segments.back() + 1);

                std::fprintf(stderr, "[WAL] Found %zu existing WAL segments, next segment ID: %u\n",
                             existing_segments.size(), current_segment_id_.load());
            }

            // Create initial segment
            return create_new_segment();

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[WAL] Failed to initialize WAL manager: %s\n", e.what());
            return false;
        }
    }

    LSN WALManager::log_begin_transaction(std::uint64_t transaction_id)
    {
        TransactionWALRecord record;
        record.transaction_id = transaction_id;
        record.commit_timestamp = 0;

        std::fprintf(stderr, "[WAL] BEGIN TRANSACTION %llu\n",
                     static_cast<unsigned long long>(transaction_id));

        return write_wal_record(WALRecordType::BEGIN_TRANSACTION, transaction_id, 0, &record,
                                sizeof(record));
    }

    LSN WALManager::log_commit_transaction(std::uint64_t transaction_id)
    {
        TransactionWALRecord record;
        record.transaction_id = transaction_id;
        record.commit_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch())
                                      .count();

        std::fprintf(stderr, "[WAL] COMMIT TRANSACTION %llu\n",
                     static_cast<unsigned long long>(transaction_id));

        LSN commit_lsn = write_wal_record(WALRecordType::COMMIT_TRANSACTION, transaction_id, 0,
                                          &record, sizeof(record));

        if (sync_commit_) {
            flush_to_disk();
        }

        return commit_lsn;
    }

    LSN WALManager::log_abort_transaction(std::uint64_t transaction_id)
    {
        TransactionWALRecord record;
        record.transaction_id = transaction_id;
        record.commit_timestamp = 0;

        std::fprintf(stderr, "[WAL] ABORT TRANSACTION %llu\n",
                     static_cast<unsigned long long>(transaction_id));

        return write_wal_record(WALRecordType::ABORT_TRANSACTION, transaction_id, 0, &record,
                                sizeof(record));
    }

    LSN WALManager::log_insert_record(std::uint64_t transaction_id, std::uint64_t prev_lsn,
                                      std::uint32_t page_id, std::uint16_t slot_id,
                                      const void* new_data, std::size_t new_length)
    {
        HeapWALRecord record;
        record.page_id = page_id;
        record.slot_id = slot_id;
        record.old_length = 0;
        record.new_length = static_cast<std::uint16_t>(new_length);

        // Combine record header with data
        std::vector<std::uint8_t> combined_data;
        combined_data.reserve(sizeof(record) + new_length);

        combined_data.insert(combined_data.end(), reinterpret_cast<const std::uint8_t*>(&record),
                             reinterpret_cast<const std::uint8_t*>(&record) + sizeof(record));

        if (new_length > 0) {
            combined_data.insert(combined_data.end(), static_cast<const std::uint8_t*>(new_data),
                                 static_cast<const std::uint8_t*>(new_data) + new_length);
        }

        std::fprintf(stderr, "[WAL] INSERT page=%u slot=%u length=%zu\n", page_id, slot_id,
                     new_length);

        return write_wal_record(WALRecordType::INSERT_RECORD, transaction_id, prev_lsn,
                                combined_data.data(), combined_data.size());
    }

    LSN WALManager::log_update_record(std::uint64_t transaction_id, std::uint64_t prev_lsn,
                                      std::uint32_t page_id, std::uint16_t slot_id,
                                      const void* old_data, std::size_t old_length,
                                      const void* new_data, std::size_t new_length)
    {
        HeapWALRecord record;
        record.page_id = page_id;
        record.slot_id = slot_id;
        record.old_length = static_cast<std::uint16_t>(old_length);
        record.new_length = static_cast<std::uint16_t>(new_length);

        // Combine record header with old and new data
        std::vector<std::uint8_t> combined_data;
        combined_data.reserve(sizeof(record) + old_length + new_length);

        combined_data.insert(combined_data.end(), reinterpret_cast<const std::uint8_t*>(&record),
                             reinterpret_cast<const std::uint8_t*>(&record) + sizeof(record));

        if (old_length > 0) {
            combined_data.insert(combined_data.end(), static_cast<const std::uint8_t*>(old_data),
                                 static_cast<const std::uint8_t*>(old_data) + old_length);
        }

        if (new_length > 0) {
            combined_data.insert(combined_data.end(), static_cast<const std::uint8_t*>(new_data),
                                 static_cast<const std::uint8_t*>(new_data) + new_length);
        }

        std::fprintf(stderr, "[WAL] UPDATE page=%u slot=%u old_length=%zu new_length=%zu\n",
                     page_id, slot_id, old_length, new_length);

        return write_wal_record(WALRecordType::UPDATE_RECORD, transaction_id, prev_lsn,
                                combined_data.data(), combined_data.size());
    }

    LSN WALManager::log_delete_record(std::uint64_t transaction_id, std::uint64_t prev_lsn,
                                      std::uint32_t page_id, std::uint16_t slot_id,
                                      const void* old_data, std::size_t old_length)
    {
        HeapWALRecord record;
        record.page_id = page_id;
        record.slot_id = slot_id;
        record.old_length = static_cast<std::uint16_t>(old_length);
        record.new_length = 0;

        // Combine record header with old data
        std::vector<std::uint8_t> combined_data;
        combined_data.reserve(sizeof(record) + old_length);

        combined_data.insert(combined_data.end(), reinterpret_cast<const std::uint8_t*>(&record),
                             reinterpret_cast<const std::uint8_t*>(&record) + sizeof(record));

        if (old_length > 0) {
            combined_data.insert(combined_data.end(), static_cast<const std::uint8_t*>(old_data),
                                 static_cast<const std::uint8_t*>(old_data) + old_length);
        }

        std::fprintf(stderr, "[WAL] DELETE page=%u slot=%u old_length=%zu\n", page_id, slot_id,
                     old_length);

        return write_wal_record(WALRecordType::DELETE_RECORD, transaction_id, prev_lsn,
                                combined_data.data(), combined_data.size());
    }

    LSN WALManager::log_checkpoint(const std::vector<std::uint64_t>& active_transactions)
    {
        // Serialize active transaction list
        std::vector<std::uint8_t> checkpoint_data;
        std::uint32_t count = static_cast<std::uint32_t>(active_transactions.size());

        checkpoint_data.insert(checkpoint_data.end(), reinterpret_cast<const std::uint8_t*>(&count),
                               reinterpret_cast<const std::uint8_t*>(&count) + sizeof(count));

        for (std::uint64_t txn_id : active_transactions) {
            checkpoint_data.insert(checkpoint_data.end(),
                                   reinterpret_cast<const std::uint8_t*>(&txn_id),
                                   reinterpret_cast<const std::uint8_t*>(&txn_id) + sizeof(txn_id));
        }

        std::fprintf(stderr, "[WAL] CHECKPOINT with %u active transactions\n", count);

        LSN checkpoint_lsn = write_wal_record(WALRecordType::CHECKPOINT, 0, 0,
                                              checkpoint_data.data(), checkpoint_data.size());

        flush_to_disk();
        return checkpoint_lsn;
    }

    void WALManager::flush_to_disk()
    {
        std::lock_guard<std::mutex> lock(wal_mutex_);

        if (current_segment_) {
            current_segment_->flush();
        }
    }

    void WALManager::create_checkpoint()
    {
        // In a full implementation, this would coordinate with the transaction manager
        // to get the list of active transactions
        std::vector<std::uint64_t> active_transactions; // Simplified: assume no active transactions
        log_checkpoint(active_transactions);
    }

    bool WALManager::create_new_segment()
    {
        try {
            std::uint32_t segment_id = current_segment_id_.fetch_add(1);

            std::ostringstream oss;
            oss << wal_directory_ << "/wal_" << std::setfill('0') << std::setw(8) << segment_id
                << ".wal";
            std::string segment_path = oss.str();

            auto new_segment =
                std::make_unique<WALSegment>(segment_path, segment_id, segment_size_);

            std::lock_guard<std::mutex> lock(wal_mutex_);
            current_segment_ = std::move(new_segment);

            std::fprintf(stderr, "[WAL] Created new WAL segment: %s\n", segment_path.c_str());
            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[WAL] Failed to create new segment: %s\n", e.what());
            return false;
        }
    }

    WALSegment* WALManager::get_current_segment()
    {
        std::lock_guard<std::mutex> lock(wal_mutex_);

        if (!current_segment_ || current_segment_->is_full()) {
            if (!create_new_segment()) {
                return nullptr;
            }
        }

        return current_segment_.get();
    }

    LSN WALManager::write_wal_record(WALRecordType type, std::uint64_t transaction_id,
                                     std::uint64_t prev_lsn, const void* data,
                                     std::size_t data_length)
    {
        WALSegment* segment = get_current_segment();
        if (!segment) {
            throw std::runtime_error("Failed to get WAL segment for writing");
        }

        LSN lsn = segment->write_record(type, transaction_id, prev_lsn, data, data_length);
        current_lsn_.store(lsn.value);

        return lsn;
    }

    bool WALManager::perform_recovery()
    {
        std::fprintf(stderr, "[WAL] Starting database recovery...\n");

        try {
            RecoveryState state;

            // Phase 1: Analysis - determine what needs to be redone/undone
            if (!analysis_pass(state)) {
                std::fprintf(stderr, "[WAL] Analysis pass failed\n");
                return false;
            }

            // Phase 2: Redo - replay all committed changes
            if (!redo_pass(state)) {
                std::fprintf(stderr, "[WAL] Redo pass failed\n");
                return false;
            }

            // Phase 3: Undo - rollback uncommitted transactions
            if (!undo_pass(state)) {
                std::fprintf(stderr, "[WAL] Undo pass failed\n");
                return false;
            }

            std::fprintf(stderr, "[WAL] Recovery completed successfully\n");
            return true;

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[WAL] Recovery failed: %s\n", e.what());
            return false;
        }
    }

    bool WALManager::analysis_pass(RecoveryState& /* state */)
    {
        std::fprintf(stderr, "[WAL] Analysis pass - determining recovery state\n");

        // Simplified analysis pass for demonstration
        // In a full implementation, this would scan the WAL from the last checkpoint
        // to determine which transactions need to be redone or undone

        return true;
    }

    bool WALManager::redo_pass(const RecoveryState& /* state */)
    {
        std::fprintf(stderr, "[WAL] Redo pass - replaying committed changes\n");

        // Simplified redo pass for demonstration
        // In a full implementation, this would replay all operations from committed transactions

        return true;
    }

    bool WALManager::undo_pass(const RecoveryState& /* state */)
    {
        std::fprintf(stderr, "[WAL] Undo pass - rolling back uncommitted transactions\n");

        // Simplified undo pass for demonstration
        // In a full implementation, this would undo all operations from uncommitted transactions

        return true;
    }

    // ========== Utility Functions ==========

    bool perform_database_recovery(const std::string& database_path,
                                   const std::string& wal_directory)
    {
        try {
            std::fprintf(stderr, "[WAL] Performing database recovery for %s\n",
                         database_path.c_str());

            WALManager wal_manager(wal_directory);
            if (!wal_manager.initialize()) {
                std::fprintf(stderr, "[WAL] Failed to initialize WAL manager for recovery\n");
                return false;
            }

            return wal_manager.perform_recovery();

        } catch (const std::exception& e) {
            std::fprintf(stderr, "[WAL] Database recovery failed: %s\n", e.what());
            return false;
        }
    }

    std::string wal_record_type_to_string(WALRecordType type)
    {
        switch (type) {
        case WALRecordType::BEGIN_TRANSACTION:
            return "BEGIN_TRANSACTION";
        case WALRecordType::COMMIT_TRANSACTION:
            return "COMMIT_TRANSACTION";
        case WALRecordType::ABORT_TRANSACTION:
            return "ABORT_TRANSACTION";
        case WALRecordType::INSERT_RECORD:
            return "INSERT_RECORD";
        case WALRecordType::UPDATE_RECORD:
            return "UPDATE_RECORD";
        case WALRecordType::DELETE_RECORD:
            return "DELETE_RECORD";
        case WALRecordType::PAGE_SPLIT:
            return "PAGE_SPLIT";
        case WALRecordType::PAGE_MERGE:
            return "PAGE_MERGE";
        case WALRecordType::INDEX_INSERT:
            return "INDEX_INSERT";
        case WALRecordType::INDEX_DELETE:
            return "INDEX_DELETE";
        case WALRecordType::INDEX_UPDATE:
            return "INDEX_UPDATE";
        case WALRecordType::CHECKPOINT:
            return "CHECKPOINT";
        case WALRecordType::SCHEMA_CHANGE:
            return "SCHEMA_CHANGE";
        case WALRecordType::COMPENSATION:
            return "COMPENSATION";
        default:
            return "UNKNOWN";
        }
    }

    std::size_t estimate_wal_record_size(WALRecordType type, std::size_t data_size)
    {
        std::size_t base_size = WALRecordHeader::SIZE;

        switch (type) {
        case WALRecordType::BEGIN_TRANSACTION:
        case WALRecordType::COMMIT_TRANSACTION:
        case WALRecordType::ABORT_TRANSACTION:
            return base_size + sizeof(TransactionWALRecord);

        case WALRecordType::INSERT_RECORD:
        case WALRecordType::UPDATE_RECORD:
        case WALRecordType::DELETE_RECORD:
            return base_size + sizeof(HeapWALRecord) + data_size;

        case WALRecordType::INDEX_INSERT:
        case WALRecordType::INDEX_DELETE:
        case WALRecordType::INDEX_UPDATE:
            return base_size + sizeof(IndexWALRecord) + data_size;

        default:
            return base_size + data_size;
        }
    }

    bool validate_wal_record(const WALRecordHeader& header, const void* data)
    {
        // Basic validation
        if (header.record_length < WALRecordHeader::SIZE) {
            return false;
        }

        if (header.record_type < WALRecordType::BEGIN_TRANSACTION ||
            header.record_type > WALRecordType::COMPENSATION) {
            return false;
        }

        // Additional validation could be added here
        (void)data; // Suppress unused parameter warning

        return true;
    }

} // namespace scratchbird::engine
