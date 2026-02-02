/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/index_gc_interface.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <functional>

namespace scratchbird
{
namespace core
{

// Forward declarations
class TransactionManager;
class Database;
struct UuidV7Bytes;  // ID typedef
using ID = UuidV7Bytes;

// ============================================================================
// Entry Types
// ============================================================================

constexpr uint8_t ENTRY_TYPE_INSERT = 0;
constexpr uint8_t ENTRY_TYPE_DELETE = 1;

// ============================================================================
// MemtableEntry - Entry in memtable or SSTable
// ============================================================================

struct MemtableEntry
{
    std::vector<uint8_t> key;
    std::vector<uint8_t> value;
    uint64_t sequence_number;
    uint8_t entry_type;  // ENTRY_TYPE_INSERT or ENTRY_TYPE_DELETE

    // MGA compliance (Firebird)
    uint64_t xmin;  // Transaction that created this entry
    uint64_t xmax;  // Transaction that deleted (0 if active)

    MemtableEntry() : sequence_number(0), entry_type(ENTRY_TYPE_INSERT), xmin(0), xmax(0) {}
};

// ============================================================================
// Memtable - In-memory write buffer (Red-Black Tree)
// ============================================================================

class Memtable
{
public:
    explicit Memtable(size_t max_size_bytes = 4 * 1024 * 1024);
    ~Memtable() = default;

    // Insert or update key-value pair
    Status put(const std::vector<uint8_t> &key,
               const std::vector<uint8_t> &value,
               uint64_t xmin,
               ErrorContext *ctx = nullptr);

    // Mark key as deleted (tombstone)
    Status remove(const std::vector<uint8_t> &key,
                  uint64_t xmax,
                  ErrorContext *ctx = nullptr);

    // Lookup key (returns latest visible version)
    Status get(const std::vector<uint8_t> &key,
               uint64_t current_xid,
               TransactionManager *txn_mgr,
               std::vector<uint8_t> *value_out,
               bool *found,
               ErrorContext *ctx = nullptr);

    // Range scan (returns entries in key order, supports nullptr for open-ended ranges)
    Status scan(const std::vector<uint8_t> *start_key,
                const std::vector<uint8_t> *end_key,
                uint64_t current_xid,
                TransactionManager *txn_mgr,
                std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *entries_out,
                ErrorContext *ctx = nullptr);

    // Get all entries (for flushing to SSTable)
    Status getAllEntries(std::vector<MemtableEntry> *entries_out,
                        ErrorContext *ctx = nullptr);

    // Update TIDs after tablespace migration (GPID remap)
    Status updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                    uint64_t *entries_updated_out = nullptr,
                                    ErrorContext *ctx = nullptr);

    // Remove entries that match predicate (used by GC)
    Status removeEntriesIf(const std::function<bool(const MemtableEntry &)> &predicate,
                           uint64_t *entries_removed_out = nullptr,
                           ErrorContext *ctx = nullptr);

    // Check if memtable is full
    bool isFull() const { return current_size_ >= max_size_; }

    // Get approximate size in bytes
    size_t getSize() const { return current_size_; }

    // Get number of entries
    size_t getNumEntries() const { return entries_.size(); }

private:
    std::map<std::vector<uint8_t>, std::vector<MemtableEntry>> entries_;  // key -> versions
    size_t max_size_;
    size_t current_size_;
    uint64_t sequence_;  // Monotonic sequence number
    mutable std::mutex mutex_;
};

// Forward declarations
class LSMBloomFilter;
class Compressor;
class LSMThreadPool;
enum class CompressionType : uint8_t;

// ============================================================================
// SSTableWriter - Writes sorted string table to disk
// ============================================================================

class SSTableWriter
{
public:
    SSTableWriter(const std::string &file_path,
                  size_t block_size = 4096,
                  CompressionType compression = static_cast<CompressionType>(0));
    ~SSTableWriter();

    // Open file for writing
    Status open(ErrorContext *ctx = nullptr);

    // Add entry (must be in sorted order by key)
    Status addEntry(const std::vector<uint8_t> &key,
                    const std::vector<uint8_t> &value,
                    uint64_t sequence_number,
                    uint8_t entry_type,
                    uint64_t xmin,
                    uint64_t xmax,
                    ErrorContext *ctx = nullptr);

    // Finish writing and flush metadata
    Status finish(ErrorContext *ctx = nullptr);

    // Close file
    Status close(ErrorContext *ctx = nullptr);

private:
    std::string file_path_;
    size_t block_size_;
    int fd_;

    std::vector<uint8_t> min_key_;
    std::vector<uint8_t> max_key_;
    uint64_t num_entries_;
    uint64_t data_offset_;

    std::vector<uint8_t> current_block_;
    std::vector<std::pair<std::vector<uint8_t>, uint64_t>> index_;  // key -> offset

    // Bloom filter for read optimization
    std::unique_ptr<LSMBloomFilter> bloom_filter_;

    // TID metadata for GC targeting
    TID min_tid_{INVALID_TID};
    TID max_tid_{INVALID_TID};
    uint64_t tid_entries_ = 0;
    std::unique_ptr<LSMBloomFilter> tid_bloom_filter_;

    // Compression
    CompressionType compression_type_;
    std::unique_ptr<Compressor> compressor_;
};

// ============================================================================
// SSTableReader - Reads sorted string table from disk
// ============================================================================

class SSTableReader
{
public:
    // Iterator for sequential SSTable scanning (for compaction)
    class Iterator
    {
    public:
        virtual ~Iterator() = default;
        virtual bool isValid() const = 0;
        virtual void next() = 0;
        virtual const std::vector<uint8_t>& key() const = 0;
        virtual const std::vector<uint8_t>& value() const = 0;
        virtual uint64_t sequenceNumber() const = 0;
        virtual uint8_t entryType() const = 0;
        virtual uint64_t xmin() const = 0;
        virtual uint64_t xmax() const = 0;
    };

    SSTableReader(const std::string &file_path, size_t block_size = 4096);
    ~SSTableReader();

    // Open file for reading
    Status open(ErrorContext *ctx = nullptr);

    // Lookup key
    Status get(const std::vector<uint8_t> &key,
               uint64_t current_xid,
               TransactionManager *txn_mgr,
               std::vector<uint8_t> *value_out,
               bool *found,
               ErrorContext *ctx = nullptr);

    // Range scan
    Status scan(const std::vector<uint8_t> &start_key,
                const std::vector<uint8_t> &end_key,
                uint64_t current_xid,
                TransactionManager *txn_mgr,
                std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *entries_out,
                ErrorContext *ctx = nullptr);

    // Create iterator for sequential scanning (for compaction)
    std::unique_ptr<Iterator> createIterator();

    // Get min/max keys
    std::vector<uint8_t> getMinKey() const { return min_key_; }
    std::vector<uint8_t> getMaxKey() const { return max_key_; }
    bool hasTidMetadata() const { return has_tid_metadata_; }
    TID getMinTid() const { return min_tid_; }
    TID getMaxTid() const { return max_tid_; }
    uint64_t getTidCount() const { return tid_count_; }

    // Get file size
    uint64_t getFileSize() const { return file_size_; }

    // Get file path
    const std::string& getFilePath() const { return file_path_; }

    // Get compression type
    CompressionType compressionType() const { return compression_type_; }

    // Check if open
    bool isOpen() const { return fd_ >= 0; }

    // Close file
    Status close(ErrorContext *ctx = nullptr);

private:
    std::string file_path_;
    size_t block_size_;
    int fd_;
    uint64_t file_size_;
    uint64_t data_end_offset_;

    std::vector<uint8_t> min_key_;
    std::vector<uint8_t> max_key_;
    uint64_t num_entries_;

    // Index: key -> file offset
    std::map<std::vector<uint8_t>, uint64_t> index_;

    // Bloom filter for read optimization (loaded from SSTable footer)
    std::unique_ptr<LSMBloomFilter> bloom_filter_;

    // TID metadata for GC targeting
    bool has_tid_metadata_ = false;
    TID min_tid_{INVALID_TID};
    TID max_tid_{INVALID_TID};
    uint64_t tid_count_ = 0;
    std::unique_ptr<LSMBloomFilter> tid_bloom_filter_;

    // Compression (loaded from SSTable footer)
    CompressionType compression_type_;
    std::unique_ptr<Compressor> compressor_;
};

// ============================================================================
// Compaction Structures
// ============================================================================

struct LevelMetadata
{
    uint32_t level;
    uint64_t size_limit_bytes;
    uint64_t size_bytes;
    std::vector<std::string> sstable_paths;

    LevelMetadata() : level(0), size_limit_bytes(0), size_bytes(0) {}
    LevelMetadata(uint32_t l, uint64_t limit) : level(l), size_limit_bytes(limit), size_bytes(0) {}
};

struct CompactionTask
{
    uint32_t source_level;
    uint32_t target_level;
    std::vector<std::string> source_sstables;
    std::vector<std::string> overlapping_sstables;
    uint64_t oit;  // Oldest Interesting Transaction (for garbage collection)

    CompactionTask() : source_level(0), target_level(0), oit(0) {}
};

// ============================================================================
// LSMCompactionManager - Manages background compaction
// ============================================================================

class LSMCompactionManager
{
public:
    explicit LSMCompactionManager(TransactionManager *txn_mgr, bool enable_parallel = true);
    ~LSMCompactionManager();

    // Initialize
    Status initialize(ErrorContext *ctx = nullptr);

    // Add SSTable to level
    Status addSSTable(uint32_t level,
                      const std::string &sstable_path,
                      uint64_t file_size,
                      ErrorContext *ctx = nullptr);

    // Check if compaction is needed
    bool needsCompaction();

    // Select compaction task
    Status selectCompactionTask(CompactionTask *task_out,
                               ErrorContext *ctx = nullptr);

    // Execute compaction (single-threaded)
    Status executeCompaction(const CompactionTask &task,
                            ErrorContext *ctx = nullptr);

    // Execute compaction in parallel (uses thread pool)
    Status executeCompactionParallel(const CompactionTask &task,
                                    ErrorContext *ctx = nullptr);

    // Get statistics
    void getStatistics(uint64_t *total_sstables_out,
                      uint64_t *total_size_out);

    // Enable/disable parallel compaction
    void setParallelCompaction(bool enable);
    bool isParallelEnabled() const { return parallel_enabled_; }

    // Configuration setters
    void setIndexPath(const std::string &path) { index_path_ = path; }
    void setBlockSize(size_t block_size) { block_size_ = block_size; }

    // Recalculate level sizes from current SSTable paths (post-rewrite GC)
    void recalculateLevelSizes();

private:
    TransactionManager *txn_mgr_;
    std::vector<LevelMetadata> levels_;
    std::mutex mutex_;

    // Configuration
    std::string index_path_;  // Base path for SSTable files
    size_t block_size_;       // SSTable block size (default 4096)

    // Parallel compaction
    bool parallel_enabled_;
    std::unique_ptr<LSMThreadPool> thread_pool_;

    // Helper methods
    void findOverlappingSSTables(uint32_t level,
                                 const std::vector<uint8_t> &min_key,
                                 const std::vector<uint8_t> &max_key,
                                 std::vector<std::string> *overlapping_out);

    Status kWayMerge(const std::vector<std::string> &input_sstables,
                     const std::string &output_sstable,
                     uint64_t oit,
                     ErrorContext *ctx = nullptr);

    Status replaceSSTablesAtomic(uint32_t level,
                                 const std::vector<std::string> &old_sstables,
                                 const std::vector<std::string> &new_sstables,
                                 ErrorContext *ctx = nullptr);
};

// ============================================================================
// LSMTreeIndex Statistics
// ============================================================================

struct Statistics
{
    size_t active_memtable_entries;
    size_t active_memtable_size;
    size_t immutable_memtable_entries;
    size_t immutable_memtable_size;
    size_t level0_sstables;
    size_t level1_sstables;
    size_t level2_sstables;
    size_t level3_sstables;
    uint64_t total_size_bytes;

    Statistics()
        : active_memtable_entries(0),
          active_memtable_size(0),
          immutable_memtable_entries(0),
          immutable_memtable_size(0),
          level0_sstables(0),
          level1_sstables(0),
          level2_sstables(0),
          level3_sstables(0),
          total_size_bytes(0)
    {
    }
};

// ============================================================================
// LSMTreeIndex - Full LSM-Tree with disk persistence
// ============================================================================

/**
 * LSMTreeIndex - Complete LSM-Tree implementation
 *
 * Architecture:
 * - Memtable: In-memory write buffer (4MB default)
 * - Immutable Memtable: Being flushed to disk
 * - Level 0-3: Tiered SSTables on disk
 * - Background compaction: Automatic merging and garbage collection
 *
 * Write path: memtable → immutable → Level 0 → Level 1 → Level 2 → Level 3
 * Read path: memtable → immutable → L0 → L1 → L2 → L3
 *
 * MGA Compliance:
 * - All entries have xmin/xmax for transaction visibility
 * - Uses TransactionManager for TIP-based visibility checks
 * - Garbage collection based on OIT (Oldest Interesting Transaction)
 */
class LSMTreeIndex : public IndexGCInterface
{
public:
    /**
     * Constructor
     * @param db Database instance (for page size, etc.)
     * @param index_path Directory to store index files
     * @param txn_mgr Transaction manager for visibility checks
     * @param memtable_size_mb Maximum memtable size in MB (default: 4MB)
     */
    LSMTreeIndex(Database *db,
                 const std::string &index_path,
                 TransactionManager *txn_mgr,
                 size_t memtable_size_mb = 4);

    ~LSMTreeIndex();

    // ========================================================================
    // Lifecycle Methods
    // ========================================================================

    /**
     * Create new LSM-Tree index (creates directory structure)
     */
    Status create(ErrorContext *ctx = nullptr);

    /**
     * Open existing LSM-Tree index (loads SSTables, starts compaction thread)
     */
    Status open(ErrorContext *ctx = nullptr);

    /**
     * Open an existing LSM-Tree index (static factory for executor template)
     * Constructs index path from database and index UUID
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param root_page Ignored for LSM-Tree (file-based, not page-based)
     * @param ctx Error context
     * @return Unique pointer to opened index, or nullptr on error
     */
    static std::unique_ptr<LSMTreeIndex> open(Database* db,
                                               const ID& index_uuid,
                                               uint32_t root_page,
                                               ErrorContext* ctx = nullptr);

    /**
     * Close index (stops compaction, flushes memtable)
     */
    Status close(ErrorContext *ctx = nullptr);

    // ========================================================================
    // Data Operations
    // ========================================================================

    /**
     * Insert key-value pair
     * @param key Search key
     * @param value Data to store
     * @param xid Transaction ID
     */
    Status put(const std::vector<uint8_t> &key,
               const std::vector<uint8_t> &value,
               uint64_t xid,
               ErrorContext *ctx = nullptr);

    /**
     * Lookup key
     * @param key Search key
     * @param xid Current transaction ID (for visibility)
     * @param value_out Output value
     * @param found Output flag (true if key found and visible)
     */
    Status get(const std::vector<uint8_t> &key,
               uint64_t xid,
               std::vector<uint8_t> *value_out,
               bool *found,
               ErrorContext *ctx = nullptr);

    /**
     * Remove key (inserts tombstone)
     * @param key Search key
     * @param xid Transaction ID
     */
    Status remove(const std::vector<uint8_t> &key,
                  uint64_t xid,
                  ErrorContext *ctx = nullptr);

    /**
     * Range scan
     * @param start_key Start of range (empty for unbounded)
     * @param end_key End of range (empty for unbounded)
     * @param xid Current transaction ID (for visibility)
     * @param entries_out Output entries
     */
    Status scan(const std::vector<uint8_t> &start_key,
                const std::vector<uint8_t> &end_key,
                uint64_t xid,
                std::vector<MemtableEntry> *entries_out,
                ErrorContext *ctx = nullptr);

    // ========================================================================
    // Maintenance Operations
    // ========================================================================

    /**
     * Flush active memtable to disk (makes it immutable, creates new active)
     */
    Status flush(ErrorContext *ctx = nullptr);

    /**
     * Get index statistics
     */
    Status getStatistics(Statistics *stats_out,
                        ErrorContext *ctx = nullptr);

    // Update TIDs after tablespace migration (GPID remap)
    Status updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                    uint64_t *tids_updated_out = nullptr,
                                    uint64_t *files_modified_out = nullptr,
                                    ErrorContext *ctx = nullptr);

    // IndexGCInterface
    Status removeDeadEntries(const std::vector<TID> &dead_tids,
                             uint64_t *entries_removed_out = nullptr,
                             uint64_t *pages_modified_out = nullptr,
                             ErrorContext *ctx = nullptr) override;
    const char *indexTypeName() const override { return "LSM"; }

private:
    // Index configuration
    Database *db_;  // Database reference (for page size, etc.)
    std::string index_path_;
    TransactionManager *txn_mgr_;
    size_t memtable_max_size_;
    size_t block_size_;  // SSTable block size (typically same as DB page size)

    // Memtables
    std::unique_ptr<Memtable> active_memtable_;
    std::unique_ptr<Memtable> immutable_memtable_;
    std::mutex memtable_mutex_;

    // SSTables (4 levels)
    std::vector<std::vector<std::unique_ptr<SSTableReader>>> sstables_;
    std::mutex sstables_mutex_;

    // Compaction
    std::unique_ptr<LSMCompactionManager> compaction_mgr_;
    std::thread compaction_thread_;
    std::atomic<bool> compaction_shutdown_;
    std::atomic<bool> gc_in_progress_;

    // Helper methods
    Status flushImmutableMemtable(ErrorContext *ctx);
    Status loadExistingSSTables(ErrorContext *ctx);
    std::string generateSSTablePath(uint32_t level);
    void compactionThreadFunc();
};

} // namespace core
} // namespace scratchbird
