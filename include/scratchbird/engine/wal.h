#ifndef SCRATCHBIRD_ENGINE_WAL_H
#define SCRATCHBIRD_ENGINE_WAL_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    // WAL Record Types
    enum class WalRecordType : std::uint8_t {
        Invalid = 0,
        Begin = 1,
        Commit = 2,
        Rollback = 3,
        Insert = 4,
        Update = 5,
        Delete = 6,
        PageWrite = 7,
        Checkpoint = 8,
        HeapInsert = 9,
        HeapUpdate = 10,
        HeapDelete = 11,
        IndexInsert = 12,
        IndexDelete = 13,
        IndexUpdate = 14
    };

    // WAL Record Header (common to all record types)
    struct WalRecordHeader {
        std::uint32_t record_size;  // Total size including header and data
        WalRecordType type;         // Record type
        std::uint8_t flags;         // Reserved flags
        std::uint16_t checksum;     // CRC16 of record data
        std::uint64_t xid;          // Transaction ID
        std::uint64_t lsn;          // Log Sequence Number
        std::uint64_t prev_lsn;     // Previous LSN for this transaction
        std::uint64_t timestamp_us; // Microsecond timestamp
    } __attribute__((packed));

    // Transaction Begin Record
    struct WalBeginRecord {
        WalRecordHeader header;
        std::uint32_t isolation_level; // Isolation level flags
        std::uint64_t snapshot_xid;    // Snapshot transaction ID
    } __attribute__((packed));

    // Transaction Commit Record
    struct WalCommitRecord {
        WalRecordHeader header;
        std::uint64_t commit_timestamp; // Commit timestamp
    } __attribute__((packed));

    // Transaction Rollback Record
    struct WalRollbackRecord {
        WalRecordHeader header;
        std::uint64_t rollback_timestamp; // Rollback timestamp
    } __attribute__((packed));

    // Page Write Record (for REDO)
    struct WalPageWriteRecord {
        WalRecordHeader header;
        std::uint32_t space_id; // Space ID
        std::uint32_t page_no;  // Page number
        std::uint32_t offset;   // Offset within page
        std::uint32_t length;   // Length of data
        // Followed by: uint8_t data[length]
    } __attribute__((packed));

    // Heap Insert Record
    struct WalHeapInsertRecord {
        WalRecordHeader header;
        std::uint32_t space_id;   // Space ID
        std::uint32_t page_no;    // Page number
        std::uint16_t slot_no;    // Slot number
        std::uint32_t tuple_size; // Size of tuple data
        // Followed by: uint8_t tuple_data[tuple_size]
    } __attribute__((packed));

    // Heap Update Record
    struct WalHeapUpdateRecord {
        WalRecordHeader header;
        std::uint32_t space_id;       // Space ID
        std::uint32_t page_no;        // Page number
        std::uint16_t slot_no;        // Slot number
        std::uint32_t old_tuple_size; // Size of old tuple data
        std::uint32_t new_tuple_size; // Size of new tuple data
        // Followed by: uint8_t old_tuple_data[old_tuple_size]
        // Followed by: uint8_t new_tuple_data[new_tuple_size]
    } __attribute__((packed));

    // Heap Delete Record
    struct WalHeapDeleteRecord {
        WalRecordHeader header;
        std::uint32_t space_id;   // Space ID
        std::uint32_t page_no;    // Page number
        std::uint16_t slot_no;    // Slot number
        std::uint32_t tuple_size; // Size of deleted tuple data
        // Followed by: uint8_t tuple_data[tuple_size]
    } __attribute__((packed));

    // Checkpoint Record
    struct WalCheckpointRecord {
        WalRecordHeader header;
        std::uint64_t checkpoint_lsn;  // LSN of this checkpoint
        std::uint64_t redo_lsn;        // LSN to start REDO from
        std::uint32_t num_dirty_pages; // Number of dirty pages
        // Followed by: array of dirty page descriptors
    } __attribute__((packed));

    // WAL Record Kind for listeners
    enum class WalRecKind : std::uint8_t { Insert = 1, Delete = 2, Update = 3 };

    // Generic WAL Record (for listeners)
    struct WalRecord {
        WalRecordHeader header;
        WalRecKind kind;
        std::vector<std::uint8_t> key_bytes;
        std::uint64_t row_id = 0;
        // Record data follows
    };

    // WAL Configuration
    struct WalConfig {
        std::string wal_dir;                       // WAL directory path
        std::uint32_t segment_size_mb = 64;        // WAL segment size in MB
        std::uint32_t buffer_size_kb = 1024;       // WAL buffer size in KB
        bool fsync_enabled = true;                 // Force fsync on commit
        std::uint32_t checkpoint_interval_s = 300; // Checkpoint interval in seconds
        std::uint32_t max_wal_segments = 10;       // Maximum WAL segments to keep
    };

    // WAL Manager - handles write-ahead logging for durability
    class WalManager
    {
      public:
        explicit WalManager(const WalConfig& config);
        ~WalManager();

        // Initialize WAL for new database
        bool initialize();

        // Open existing WAL
        bool open();

        // Close WAL
        void close();

        // Transaction logging
        std::uint64_t log_begin(std::uint64_t xid, std::uint32_t isolation_level,
                                std::uint64_t snapshot_xid);
        std::uint64_t log_commit(std::uint64_t xid, std::uint64_t prev_lsn);
        std::uint64_t log_rollback(std::uint64_t xid, std::uint64_t prev_lsn);

        // Data operation logging
        std::uint64_t log_heap_insert(std::uint64_t xid, std::uint64_t prev_lsn,
                                      std::uint32_t space_id, std::uint32_t page_no,
                                      std::uint16_t slot_no, const void* tuple_data,
                                      std::uint32_t tuple_size);

        std::uint64_t log_heap_update(std::uint64_t xid, std::uint64_t prev_lsn,
                                      std::uint32_t space_id, std::uint32_t page_no,
                                      std::uint16_t slot_no, const void* old_tuple_data,
                                      std::uint32_t old_tuple_size, const void* new_tuple_data,
                                      std::uint32_t new_tuple_size);

        std::uint64_t log_heap_delete(std::uint64_t xid, std::uint64_t prev_lsn,
                                      std::uint32_t space_id, std::uint32_t page_no,
                                      std::uint16_t slot_no, const void* tuple_data,
                                      std::uint32_t tuple_size);

        std::uint64_t log_page_write(std::uint64_t xid, std::uint64_t prev_lsn,
                                     std::uint32_t space_id, std::uint32_t page_no,
                                     std::uint32_t offset, const void* data, std::uint32_t length);

        // Checkpoint operations
        std::uint64_t
        log_checkpoint(const std::vector<std::pair<std::uint32_t, std::uint32_t>>& dirty_pages);
        bool perform_checkpoint();

        // Recovery operations
        bool recover_database(FileMap& fmap);
        std::uint64_t get_last_checkpoint_lsn() const;
        std::uint64_t get_current_lsn() const;

        // WAL maintenance
        bool truncate_wal(std::uint64_t checkpoint_lsn);
        void flush();
        void fsync();

        // Statistics
        struct WalStats {
            std::uint64_t total_records;
            std::uint64_t total_bytes_written;
            std::uint64_t current_segment;
            std::uint64_t current_lsn;
            std::uint32_t active_segments;
        };
        WalStats get_stats() const;

        // Additional methods needed by btree implementation
        std::uint64_t append_insert(const std::vector<std::uint8_t>& key_enc, std::uint64_t row_id,
                                    const std::string& payload)
        {
            // TODO: Implement proper WAL logging for btree insert
            (void)key_enc;
            (void)row_id;
            (void)payload; // Suppress warnings
            return get_current_lsn();
        }

        std::uint64_t next_lsn() const
        {
            return get_current_lsn() + 1;
        }

        std::uint64_t append_root_update(std::uint32_t new_root)
        {
            // TODO: Implement proper WAL logging for btree root update
            (void)new_root; // Suppress warnings
            return get_current_lsn();
        }

        std::uint64_t append_delete(const std::vector<std::uint8_t>& key_enc)
        {
            // TODO: Implement proper WAL logging for btree delete
            (void)key_enc; // Suppress warnings
            return get_current_lsn();
        }

        // Static methods for global WAL listeners (stubs for index_online.cpp)
        template <typename Func> static void register_global_listener(Func&&)
        {
            // TODO: Implement WAL global listener registry
        }

      private:
        // Internal methods
        std::uint64_t allocate_lsn();
        std::uint64_t write_record(const void* record, std::uint32_t size);
        bool write_to_segment(const void* data, std::uint32_t size);
        bool rotate_segment();
        std::uint16_t calculate_checksum(const void* data, std::uint32_t size);
        bool ensure_space(std::uint32_t required_bytes);

        // Recovery helpers
        bool replay_record(const WalRecordHeader* header, const void* data, FileMap& fmap);
        bool read_record_at_lsn(std::uint64_t lsn, std::vector<std::uint8_t>& buffer);

        WalConfig config_;
        std::string current_segment_path_;
        std::unique_ptr<FileHandle> current_segment_file_;
        std::vector<std::uint8_t> write_buffer_;
        std::uint32_t buffer_offset_;

        std::atomic<std::uint64_t> current_lsn_;
        std::atomic<std::uint64_t> current_segment_;
        std::atomic<std::uint64_t> last_checkpoint_lsn_;

        mutable std::mutex wal_mutex_;
        bool initialized_;
        bool open_;

        // Statistics
        mutable std::atomic<std::uint64_t> total_records_;
        mutable std::atomic<std::uint64_t> total_bytes_;
    };

    // WAL utilities
    namespace wal_util
    {
        std::string generate_segment_filename(std::uint64_t segment_no);
        std::uint64_t parse_segment_number(const std::string& filename);
        std::vector<std::string> find_wal_segments(const std::string& wal_dir);
        bool validate_wal_record(const WalRecordHeader* header, const void* data);
    } // namespace wal_util

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_WAL_H
