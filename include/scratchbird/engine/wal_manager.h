#ifndef SCRATCHBIRD_ENGINE_WAL_MANAGER_H
#define SCRATCHBIRD_ENGINE_WAL_MANAGER_H

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

    // WAL (Write-Ahead Logging) for transaction durability and crash recovery

    // Log Sequence Number - unique identifier for each log record
    struct LSN {
        std::uint64_t value{0};

        LSN() = default;
        explicit LSN(std::uint64_t v) : value(v) {}

        bool operator<(const LSN& other) const
        {
            return value < other.value;
        }
        bool operator<=(const LSN& other) const
        {
            return value <= other.value;
        }
        bool operator>(const LSN& other) const
        {
            return value > other.value;
        }
        bool operator>=(const LSN& other) const
        {
            return value >= other.value;
        }
        bool operator==(const LSN& other) const
        {
            return value == other.value;
        }
        bool operator!=(const LSN& other) const
        {
            return value != other.value;
        }

        std::string to_string() const
        {
            return std::to_string(value);
        }
    };

    // WAL record types for different operations
    enum class WALRecordType : std::uint8_t {
        // Transaction lifecycle
        BEGIN_TRANSACTION = 1,
        COMMIT_TRANSACTION = 2,
        ABORT_TRANSACTION = 3,

        // Data modification
        INSERT_RECORD = 10,
        UPDATE_RECORD = 11,
        DELETE_RECORD = 12,

        // Page operations
        PAGE_SPLIT = 20,
        PAGE_MERGE = 21,

        // Index operations
        INDEX_INSERT = 30,
        INDEX_DELETE = 31,
        INDEX_UPDATE = 32,

        // Checkpoint and system
        CHECKPOINT = 40,
        SCHEMA_CHANGE = 41,

        // Compensation records for undo
        COMPENSATION = 50
    };

    // WAL record header
    struct WALRecordHeader {
        std::uint32_t record_length;  // Total length including header and data
        WALRecordType record_type;    // Type of operation
        std::uint64_t transaction_id; // Transaction that generated this record
        std::uint64_t prev_lsn;       // Previous LSN for this transaction (for undo chain)
        std::uint32_t checksum;       // CRC32 checksum of record data
        std::uint64_t timestamp;      // When the record was created

        static constexpr std::size_t SIZE = 40; // Fixed header size
    };

    // WAL record for heap operations
    struct HeapWALRecord {
        std::uint32_t page_id;    // Page being modified
        std::uint16_t slot_id;    // Slot within the page
        std::uint16_t old_length; // Length of old data
        std::uint16_t new_length; // Length of new data
        // Followed by old_data[old_length] and new_data[new_length]
    };

    // WAL record for transaction operations
    struct TransactionWALRecord {
        std::uint64_t transaction_id;
        std::uint64_t commit_timestamp;
    };

    // WAL record for index operations
    struct IndexWALRecord {
        std::uint32_t index_id;     // Index being modified
        std::uint32_t page_id;      // Index page
        std::uint16_t key_length;   // Length of key data
        std::uint16_t value_length; // Length of value data
        // Followed by key_data[key_length] and value_data[value_length]
    };

    // WAL segment file management
    class WALSegment
    {
      public:
        WALSegment(const std::string& path, std::uint32_t segment_id, std::size_t max_size);
        ~WALSegment();

        // Write a WAL record to this segment
        LSN write_record(WALRecordType type, std::uint64_t transaction_id, std::uint64_t prev_lsn,
                         const void* data, std::size_t data_length);

        // Read a WAL record at specific offset
        bool read_record(std::size_t offset, WALRecordHeader& header,
                         std::vector<std::uint8_t>& data);

        // Flush buffered writes to disk
        void flush();

        // Check if segment is full
        bool is_full() const
        {
            return current_offset_ >= max_size_;
        }

        // Get current size
        std::size_t get_size() const
        {
            return current_offset_;
        }

        // Get segment ID
        std::uint32_t get_segment_id() const
        {
            return segment_id_;
        }

      private:
        std::string file_path_;
        std::uint32_t segment_id_;
        std::size_t max_size_;
        std::size_t current_offset_;
        std::unique_ptr<FileHandle> file_handle_;
        std::mutex write_mutex_;

        std::uint32_t calculate_checksum(const void* data, std::size_t length);
    };

    // Main WAL Manager
    class WALManager
    {
      public:
        WALManager(const std::string& wal_directory,
                   std::size_t segment_size = 64 * 1024 * 1024); // 64MB default
        ~WALManager();

        // Initialize WAL system
        bool initialize();

        // Write WAL records
        LSN log_begin_transaction(std::uint64_t transaction_id);
        LSN log_commit_transaction(std::uint64_t transaction_id);
        LSN log_abort_transaction(std::uint64_t transaction_id);

        LSN log_insert_record(std::uint64_t transaction_id, std::uint64_t prev_lsn,
                              std::uint32_t page_id, std::uint16_t slot_id, const void* new_data,
                              std::size_t new_length);

        LSN log_update_record(std::uint64_t transaction_id, std::uint64_t prev_lsn,
                              std::uint32_t page_id, std::uint16_t slot_id, const void* old_data,
                              std::size_t old_length, const void* new_data, std::size_t new_length);

        LSN log_delete_record(std::uint64_t transaction_id, std::uint64_t prev_lsn,
                              std::uint32_t page_id, std::uint16_t slot_id, const void* old_data,
                              std::size_t old_length);

        LSN log_checkpoint(const std::vector<std::uint64_t>& active_transactions);

        // Force WAL buffers to disk (for transaction commit)
        void flush_to_disk();

        // Recovery operations
        bool perform_recovery();
        LSN get_last_checkpoint_lsn();

        // WAL maintenance
        void create_checkpoint();
        void truncate_wal_before(LSN lsn);

        // Get current LSN
        LSN get_current_lsn() const
        {
            return LSN(current_lsn_.load());
        }

        // Configuration
        void set_sync_commit(bool sync)
        {
            sync_commit_ = sync;
        }
        void set_checkpoint_interval(std::size_t interval)
        {
            checkpoint_interval_ = interval;
        }

      private:
        std::string wal_directory_;
        std::size_t segment_size_;
        bool sync_commit_;
        std::size_t checkpoint_interval_;

        std::atomic<std::uint64_t> current_lsn_;
        std::atomic<std::uint32_t> current_segment_id_;

        std::unique_ptr<WALSegment> current_segment_;
        std::vector<std::unique_ptr<WALSegment>> segments_;

        mutable std::mutex wal_mutex_;

        // Recovery state
        struct RecoveryState {
            LSN checkpoint_lsn;
            std::vector<std::uint64_t> active_transactions;
            std::unordered_map<std::uint64_t, LSN> transaction_last_lsn;
        };

        // Internal methods
        bool create_new_segment();
        WALSegment* get_current_segment();
        bool read_wal_record(LSN lsn, WALRecordHeader& header, std::vector<std::uint8_t>& data);

        // Recovery phases
        bool analysis_pass(RecoveryState& state);
        bool redo_pass(const RecoveryState& state);
        bool undo_pass(const RecoveryState& state);

        // WAL record construction helpers
        LSN write_wal_record(WALRecordType type, std::uint64_t transaction_id,
                             std::uint64_t prev_lsn, const void* data, std::size_t data_length);
    };

    // ARIES-style recovery context
    struct RecoveryContext {
        WALManager* wal_manager;
        std::string database_path;

        // Recovery statistics
        std::size_t records_analyzed{0};
        std::size_t records_redone{0};
        std::size_t records_undone{0};
        std::size_t transactions_recovered{0};
        std::size_t transactions_aborted{0};
    };

    // High-level recovery interface
    bool perform_database_recovery(const std::string& database_path,
                                   const std::string& wal_directory);

    // WAL utility functions
    std::string wal_record_type_to_string(WALRecordType type);
    std::size_t estimate_wal_record_size(WALRecordType type, std::size_t data_size);
    bool validate_wal_record(const WALRecordHeader& header, const void* data);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_WAL_MANAGER_H
