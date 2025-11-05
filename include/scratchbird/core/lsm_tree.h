/**
 * LSM-Tree (Log-Structured Merge-Tree) Index Implementation
 *
 * CRITICAL: This implementation uses Firebird MGA (Multi-Generational Architecture)
 * See /MGA_RULES.md for compliance requirements
 *
 * Architecture:
 * - Memtable: In-memory sorted map (Red-Black Tree) for recent writes
 * - SSTables: Disk-based sorted files organized in levels (0-3)
 * - Compaction: Background merging of SSTables + garbage collection
 * - WAL: Write-Ahead Log for durability and crash recovery
 * - Bloom Filters: Probabilistic membership test to reduce read I/O
 *
 * MGA Compliance:
 * - Every entry has xmin/xmax for visibility (Firebird style)
 * - Uses TransactionId (uint64_t), NOT Snapshot
 * - TIP-based visibility via TransactionManager::isVersionVisible()
 * - Soft delete: Set xmax, physical removal via compaction
 */

#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/transaction_manager.h"
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <string>
#include <queue>
#include <cmath>

namespace scratchbird
{
namespace core
{

// Forward declarations
class Database;
class TransactionManager;

/**
 * Memtable Entry
 *
 * Represents a single key-value pair in the memtable (in-memory sorted map).
 *
 * MGA Compliance:
 * - xmin: Transaction that created this entry
 * - xmax: Transaction that deleted this entry (0 if active)
 * - Visibility determined by TransactionManager::isVersionVisible()
 *
 * Sequence number is used to maintain insertion order for entries with the same key.
 * Higher sequence = newer version.
 */
struct MemtableEntry
{
    std::vector<uint8_t> key;       // Variable-length key
    std::vector<uint8_t> value;     // Variable-length value
    uint64_t sequence_number;       // Monotonic sequence (higher = newer)
    uint8_t entry_type;             // 0 = Insert, 1 = Delete (tombstone)
    uint64_t xmin;                  // MGA: Transaction that created this entry
    uint64_t xmax;                  // MGA: Transaction that deleted this entry (0 = active)

    // Comparison operator for std::map
    // Order by: key (lexicographic) then sequence_number (descending - newest first)
    bool operator<(const MemtableEntry &other) const
    {
        if (key != other.key)
        {
            return key < other.key;
        }
        // Newer versions (higher sequence) come first
        return sequence_number > other.sequence_number;
    }

    // Calculate entry size (for memtable size tracking)
    size_t getSize() const
    {
        return key.size() + value.size() + sizeof(sequence_number) +
               sizeof(entry_type) + sizeof(xmin) + sizeof(xmax);
    }
};

/**
 * Entry type constants
 */
constexpr uint8_t ENTRY_TYPE_INSERT = 0;
constexpr uint8_t ENTRY_TYPE_DELETE = 1;

/**
 * Memtable
 *
 * In-memory sorted map using Red-Black Tree (std::map).
 * Buffers writes in memory before flushing to disk as SSTables.
 *
 * Features:
 * - O(log n) insert, search, delete
 * - Thread-safe (mutex-protected)
 * - MGA visibility filtering
 * - Size tracking (triggers flush at 4 MB default)
 * - Range scan support
 *
 * Write Pattern:
 * 1. Insert entry with current xid (xmin = current_xid, xmax = 0)
 * 2. Track size
 * 3. Return ResourceExhausted when full (triggers flush)
 *
 * Read Pattern:
 * 1. Search for key
 * 2. Apply MGA visibility filtering (newest visible version)
 * 3. Handle tombstones (entry_type = DELETE)
 */
class Memtable
{
public:
    /**
     * Constructor
     *
     * @param max_size_bytes Maximum size in bytes before flush (default 4 MB)
     */
    explicit Memtable(size_t max_size_bytes = 4 * 1024 * 1024);

    /**
     * Destructor
     */
    ~Memtable();

    /**
     * Insert or update key-value pair
     *
     * MGA: Sets xmin = current_xid, xmax = 0
     *
     * @param key Key bytes
     * @param value Value bytes
     * @param current_xid Current transaction ID (MGA)
     * @param ctx Error context
     * @return OK if successful, ResourceExhausted if full, error otherwise
     */
    Status put(const std::vector<uint8_t> &key,
               const std::vector<uint8_t> &value,
               uint64_t current_xid,
               ErrorContext *ctx = nullptr);

    /**
     * Delete key (insert tombstone)
     *
     * MGA: Sets entry_type = DELETE, xmin = current_xid, xmax = 0
     *
     * @param key Key bytes
     * @param current_xid Current transaction ID (MGA)
     * @param ctx Error context
     * @return OK if successful, ResourceExhausted if full, error otherwise
     */
    Status remove(const std::vector<uint8_t> &key,
                  uint64_t current_xid,
                  ErrorContext *ctx = nullptr);

    /**
     * Get value for key
     *
     * Returns the newest visible version according to MGA visibility rules.
     *
     * @param key Key bytes
     * @param current_xid Current transaction ID (MGA)
     * @param txn_mgr Transaction manager for visibility checks
     * @param value_out Output value (only set if found)
     * @param found Output flag (true if key found and visible)
     * @param ctx Error context
     * @return OK if successful, error otherwise
     */
    Status get(const std::vector<uint8_t> &key,
               uint64_t current_xid,
               TransactionManager *txn_mgr,
               std::vector<uint8_t> *value_out,
               bool *found,
               ErrorContext *ctx = nullptr);

    /**
     * Range scan
     *
     * Returns all visible entries in key range [start_key, end_key].
     * Results ordered by key (ascending).
     *
     * @param start_key Start of range (inclusive, nullptr = from beginning)
     * @param end_key End of range (inclusive, nullptr = to end)
     * @param current_xid Current transaction ID (MGA)
     * @param txn_mgr Transaction manager for visibility checks
     * @param entries_out Output entries (cleared first)
     * @param ctx Error context
     * @return OK if successful, error otherwise
     */
    Status scan(const std::vector<uint8_t> *start_key,
                const std::vector<uint8_t> *end_key,
                uint64_t current_xid,
                TransactionManager *txn_mgr,
                std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *entries_out,
                ErrorContext *ctx = nullptr);

    /**
     * Check if memtable is full (size >= max_size_bytes)
     *
     * @return true if full, false otherwise
     */
    bool isFull() const;

    /**
     * Get current size in bytes
     *
     * @return Current size
     */
    size_t getSize() const;

    /**
     * Get all entries (for flushing to SSTable)
     *
     * Returns all entries in sorted order (key ascending, sequence descending).
     * Used when flushing memtable to disk.
     *
     * @param entries_out Output entries (cleared first)
     * @param ctx Error context
     * @return OK if successful, error otherwise
     */
    Status getAllEntries(std::vector<MemtableEntry> *entries_out,
                        ErrorContext *ctx = nullptr);

    /**
     * Get number of entries
     *
     * @return Number of entries
     */
    size_t getNumEntries() const;

private:
    // Red-Black Tree: std::map provides O(log n) operations and sorted order
    // Key: MemtableEntry (sorted by key then sequence)
    // Value: unused (always true)
    std::map<MemtableEntry, bool> entries_;

    // Sequence number (monotonically increasing)
    uint64_t next_sequence_;

    // Current size in bytes
    size_t current_size_;

    // Maximum size in bytes
    size_t max_size_;

    // Flag indicating memtable is full (rejected an insert due to capacity)
    bool is_full_;

    // Mutex for thread-safe access
    mutable std::mutex mutex_;

    /**
     * Check if entry is visible to current transaction (MGA rules)
     *
     * FIREBIRD MGA COMPLIANCE:
     * - Own changes always visible (entry.xmin == current_xid)
     * - Committed and older changes visible (entry.xmin < current_xid, state = TX_COMMITTED)
     * - Deleted entries not visible (entry.xmax != 0 and visible)
     *
     * NO POSTGRESQL MVCC (no Snapshot usage)
     *
     * @param entry Entry to check
     * @param current_xid Current transaction ID
     * @param txn_mgr Transaction manager
     * @return true if visible, false otherwise
     */
    bool isEntryVisible(const MemtableEntry &entry,
                       uint64_t current_xid,
                       TransactionManager *txn_mgr) const;
};

// ============================================================================
// SSTable (Sorted String Table) - On-Disk Immutable Files
// ============================================================================

/**
 * SSTable Footer - Fixed-size metadata at end of file
 *
 * Layout:
 * - Magic number for file format validation
 * - Offsets to index block and bloom filter
 * - Min/max keys for quick range checks
 * - Checksum for data integrity
 */
#pragma pack(push, 1)
struct SSTableFooter
{
    static constexpr uint64_t MAGIC_NUMBER = 0x5353544142ULL; // "SSTAB"
    static constexpr uint16_t VERSION = 1;
    static constexpr size_t MAX_KEY_SIZE = 256;

    uint64_t magic;          // Magic number: 0x5353544142 ("SSTAB")
    uint16_t version;        // File format version (1)
    uint64_t index_offset;   // Byte offset of index block
    uint64_t bloom_offset;   // Byte offset of Bloom filter
    uint64_t num_entries;    // Total number of key-value pairs
    uint64_t min_key_len;    // Min key length
    uint8_t min_key[256];    // Min key (truncated if > 256 bytes)
    uint64_t max_key_len;    // Max key length
    uint8_t max_key[256];    // Max key (truncated if > 256 bytes)
    uint32_t checksum;       // CRC32 of file (except this field)
};
#pragma pack(pop)

/**
 * Index Entry - Points to data block containing keys
 *
 * Binary searchable array in SSTable index block
 */
struct IndexEntry
{
    std::vector<uint8_t> first_key; // First key in data block
    uint64_t block_offset;          // Byte offset of data block
    uint64_t block_size;            // Size of data block in bytes
};

/**
 * LSM Bloom Filter - Probabilistic membership test for variable-length keys
 *
 * Note: Different from the TID-based BloomFilter in tid_resolver.h
 * This is optimized for LSM-Tree key lookups with variable-length byte arrays
 *
 * Phase 5 implementation - basic functionality
 * 10 bits/key → ~1% false positive rate
 *
 * Uses FNV-1a hash function with multiple seeds for k hash functions
 */
class LSMBloomFilter
{
public:
    /**
     * Construct Bloom filter with expected number of keys and target false positive rate
     *
     * @param expected_keys Estimated number of keys to be added
     * @param false_positive_rate Target false positive rate (default 1%)
     */
    LSMBloomFilter(size_t expected_keys = 1000, double false_positive_rate = 0.01);
    ~LSMBloomFilter();

    /**
     * Add key to bloom filter
     *
     * @param key Variable-length key
     */
    void add(const std::vector<uint8_t> &key);

    /**
     * Check if key might be present (may return false positives)
     *
     * @param key Variable-length key
     * @return true if key might be present, false if definitely not present
     */
    bool mightContain(const std::vector<uint8_t> &key) const;

    /**
     * Serialize bloom filter to byte array
     *
     * Format: [num_bits (8 bytes)][num_hashes (8 bytes)][bit array]
     *
     * @param output Output byte array
     */
    void serialize(std::vector<uint8_t> *output) const;

    /**
     * Deserialize bloom filter from byte array
     *
     * @param data Serialized bloom filter data
     * @return Deserialized bloom filter (caller owns pointer)
     */
    static LSMBloomFilter *deserialize(const std::vector<uint8_t> &data);

    /**
     * Get size of bloom filter in bytes
     *
     * @return Size in bytes
     */
    size_t getSizeBytes() const;

private:
    std::vector<uint8_t> bits_;  // Bit array (packed into bytes)
    size_t num_bits_;            // Total number of bits
    size_t num_hashes_;          // Number of hash functions

    /**
     * Calculate optimal number of bits for Bloom filter
     *
     * Formula: m = -n * ln(p) / (ln(2)^2)
     * where n = number of keys, p = false positive rate
     *
     * @param n Number of keys
     * @param p False positive rate
     * @return Number of bits
     */
    static size_t calculateNumBits(size_t n, double p);

    /**
     * Calculate optimal number of hash functions
     *
     * Formula: k = (m / n) * ln(2)
     * where m = number of bits, n = number of keys
     *
     * @param m Number of bits
     * @param n Number of keys
     * @return Number of hash functions
     */
    static size_t calculateNumHashes(size_t m, size_t n);

    /**
     * Hash function using FNV-1a with seed
     *
     * @param key Variable-length key
     * @param seed Seed value (0 to num_hashes-1)
     * @return 64-bit hash value
     */
    uint64_t hash(const std::vector<uint8_t> &key, size_t seed) const;
};

/**
 * SSTable Writer - Flush memtable to immutable on-disk file
 *
 * Write path:
 * 1. open() - Create file
 * 2. addEntry() - Add entries in sorted order (multiple calls)
 * 3. finish() - Flush final block, write index/bloom/footer
 *
 * File format:
 * [Data Block 0][Data Block 1]...[Index Block][Bloom Filter][Footer]
 */
class SSTableWriter
{
public:
    explicit SSTableWriter(const std::string &file_path, size_t block_size = 4096);
    ~SSTableWriter();

    /**
     * Open file for writing
     */
    Status open(ErrorContext *ctx = nullptr);

    /**
     * Add entry to SSTable
     *
     * REQUIREMENT: Must be called in sorted key order!
     *
     * @param key Entry key
     * @param value Entry value
     * @param sequence_number Monotonic sequence
     * @param entry_type 0=Insert, 1=Delete
     * @param xmin MGA: Transaction that created
     * @param xmax MGA: Transaction that deleted (0 if active)
     */
    Status addEntry(const std::vector<uint8_t> &key,
                    const std::vector<uint8_t> &value,
                    uint64_t sequence_number,
                    uint8_t entry_type,
                    uint64_t xmin,
                    uint64_t xmax,
                    ErrorContext *ctx = nullptr);

    /**
     * Finish writing SSTable
     *
     * - Flushes final data block
     * - Writes index block
     * - Writes bloom filter
     * - Writes footer with metadata
     * - Closes file
     */
    Status finish(ErrorContext *ctx = nullptr);

    // Getters
    uint64_t getNumEntries() const { return num_entries_; }
    const std::vector<uint8_t> &getMinKey() const { return min_key_; }
    const std::vector<uint8_t> &getMaxKey() const { return max_key_; }

private:
    std::string file_path_;
    std::ofstream file_;
    size_t block_size_;

    // Current data block being written
    std::vector<uint8_t> current_block_;
    uint64_t current_block_offset_;

    // Index entries (one per data block)
    std::vector<IndexEntry> index_entries_;

    // Bloom filter for all keys
    LSMBloomFilter bloom_filter_;

    // Metadata
    std::vector<uint8_t> min_key_;
    std::vector<uint8_t> max_key_;
    uint64_t num_entries_;

    // Flush current block to disk
    Status flushBlock(ErrorContext *ctx);

    // Calculate CRC32 checksum
    uint32_t calculateChecksum();
};

/**
 * SSTable Reader - Read immutable on-disk sorted string tables
 *
 * Read path:
 * 1. open() - Read footer, parse index, load bloom filter
 * 2. get() - Bloom check → Binary search index → Read block → MGA filter
 * 3. scan() - Range scan with start/end keys
 *
 * Optimizations:
 * - Bloom filter reduces unnecessary I/O (1% false positive)
 * - Binary searchable index for O(log n) block location
 * - MGA visibility filtering (TIP-based)
 */
class SSTableReader
{
public:
    explicit SSTableReader(const std::string &file_path);
    ~SSTableReader();

    /**
     * Open SSTable and read metadata
     *
     * - Reads footer from end of file
     * - Validates magic number and version
     * - Loads bloom filter into memory
     * - Parses index block
     */
    Status open(ErrorContext *ctx = nullptr);

    /**
     * Get value for key (point query)
     *
     * Steps:
     * 1. Check bloom filter (skip if not present)
     * 2. Binary search index to find data block
     * 3. Read data block
     * 4. Scan entries in block for key
     * 5. Apply MGA visibility filtering
     *
     * @param key Key to search for
     * @param current_xid Current transaction ID
     * @param txn_mgr Transaction manager for visibility checks
     * @param value_out Output value (if found)
     * @param found Output flag (true if found and visible)
     * @return Status::OK on success
     */
    Status get(const std::vector<uint8_t> &key,
               uint64_t current_xid,
               TransactionManager *txn_mgr,
               std::vector<uint8_t> *value_out,
               bool *found,
               ErrorContext *ctx = nullptr);

    /**
     * Range scan with start/end keys
     *
     * @param start_key Start key (inclusive), empty for beginning
     * @param end_key End key (exclusive), empty for end
     * @param current_xid Current transaction ID
     * @param txn_mgr Transaction manager
     * @param results Output vector of (key, value) pairs
     */
    Status scan(const std::vector<uint8_t> &start_key,
                const std::vector<uint8_t> &end_key,
                uint64_t current_xid,
                TransactionManager *txn_mgr,
                std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>> *results,
                ErrorContext *ctx = nullptr);

    // Getters
    uint64_t getNumEntries() const { return footer_.num_entries; }
    std::vector<uint8_t> getMinKey() const;
    std::vector<uint8_t> getMaxKey() const;
    bool isOpen() const { return is_open_; }

private:
    std::string file_path_;
    std::ifstream file_;
    bool is_open_;

    // Metadata from footer
    SSTableFooter footer_;

    // Index entries (loaded at open)
    std::vector<IndexEntry> index_entries_;

    // Bloom filter (loaded at open)
    LSMBloomFilter *bloom_filter_;

    /**
     * Find index entry containing key using binary search
     *
     * Returns index of data block that might contain key
     * Returns -1 if key is definitely not in SSTable
     */
    int findBlockIndex(const std::vector<uint8_t> &key) const;

    /**
     * Read data block at given offset
     *
     * @param block_offset Byte offset of block
     * @param block_size Size of block in bytes
     * @param block_data Output buffer for block data
     */
    Status readBlock(uint64_t block_offset,
                     uint64_t block_size,
                     std::vector<uint8_t> *block_data,
                     ErrorContext *ctx);

    /**
     * Parse entries from data block
     *
     * Deserializes all entries in block
     */
    Status parseBlockEntries(const std::vector<uint8_t> &block_data,
                             std::vector<MemtableEntry> *entries,
                             ErrorContext *ctx);

    /**
     * Check if entry is visible to current transaction (MGA)
     *
     * Uses Firebird MGA rules (TIP-based visibility)
     */
    bool isEntryVisible(const MemtableEntry &entry,
                       uint64_t current_xid,
                       TransactionManager *txn_mgr) const;

public:
    /**
     * Load all entries from SSTable (for compaction)
     *
     * This method reads ALL entries without visibility filtering
     * Used by compaction to access invisible entries for garbage collection
     *
     * @param entries Output vector of all entries
     * @return Status::OK on success
     */
    Status loadAllEntries(std::vector<MemtableEntry> *entries, ErrorContext *ctx = nullptr);
};

/**
 * ============================================================================
 * PHASE 4: COMPACTION
 * ============================================================================
 */

/**
 * Compaction Strategy
 *
 * - LEVELED: SSTables organized in levels (0-3), merges when level is full
 * - TIERED: Time-based windows, merges when too many SSTables in window
 */
enum class CompactionStrategy
{
    LEVELED = 0,
    TIERED = 1
};

/**
 * Level Metadata
 *
 * Tracks SSTables at each level
 */
struct LevelMetadata
{
    uint32_t level;                              // Level number (0-3)
    uint64_t size_bytes;                         // Total size of SSTables at this level
    uint64_t size_limit_bytes;                   // Maximum size before triggering compaction
    std::vector<std::string> sstable_paths;      // File paths of SSTables at this level

    LevelMetadata()
        : level(0), size_bytes(0), size_limit_bytes(0) {}

    LevelMetadata(uint32_t lvl, uint64_t limit)
        : level(lvl), size_bytes(0), size_limit_bytes(limit) {}
};

/**
 * Compaction Task
 *
 * Describes a compaction operation: merge source SSTables into target level
 */
struct CompactionTask
{
    uint32_t source_level;                       // Source level (e.g., 0 for L0→L1)
    uint32_t target_level;                       // Target level (e.g., 1 for L0→L1)
    std::vector<std::string> source_sstables;    // SSTables to merge
    std::vector<std::string> overlapping_sstables; // Overlapping SSTables in target level
    uint64_t oit;                                // OIT (Oldest Interesting Transaction) for MGA garbage collection

    CompactionTask()
        : source_level(0), target_level(0), oit(0) {}
};

/**
 * Merge Entry
 *
 * Used in priority queue for K-way merge
 */
struct MergeEntry
{
    std::vector<uint8_t> key;
    std::vector<uint8_t> value;
    uint64_t sequence_number;
    uint8_t entry_type;
    uint64_t xmin;
    uint64_t xmax;
    uint32_t source_level;                       // Which level this entry came from
    size_t source_index;                         // Index in source SSTables array
    size_t entry_index;                          // Index within current data block

    // For priority queue: smallest key comes first
    bool operator>(const MergeEntry &other) const
    {
        if (key != other.key)
        {
            return key > other.key;
        }
        // If keys equal, higher level (newer) wins
        if (source_level != other.source_level)
        {
            return source_level < other.source_level;
        }
        // If same level, higher sequence number wins
        return sequence_number < other.sequence_number;
    }
};

/**
 * LSM Compaction Manager
 *
 * Handles compaction of SSTables:
 * - K-way merge of sorted files
 * - Garbage collection (MGA-compliant)
 * - Level management
 */
class LSMCompactionManager
{
public:
    explicit LSMCompactionManager(TransactionManager *txn_mgr);
    ~LSMCompactionManager();

    /**
     * Initialize level metadata
     */
    Status initialize(ErrorContext *ctx = nullptr);

    /**
     * Add SSTable to level
     */
    Status addSSTable(uint32_t level, const std::string &sstable_path, uint64_t file_size, ErrorContext *ctx = nullptr);

    /**
     * Check if compaction is needed at any level
     */
    bool needsCompaction() const;

    /**
     * Select compaction task
     */
    Status selectCompactionTask(CompactionTask *task, ErrorContext *ctx = nullptr);

    /**
     * Execute compaction task
     *
     * - K-way merge of source SSTables
     * - Garbage collection (remove old versions)
     * - Tombstone removal
     * - Atomic replacement
     */
    Status executeCompaction(const CompactionTask &task, ErrorContext *ctx = nullptr);

    /**
     * Get level metadata
     */
    const LevelMetadata &getLevel(uint32_t level) const;

    /**
     * Get statistics
     */
    void getStatistics(uint64_t *total_sstables, uint64_t *total_size_bytes) const;

private:
    TransactionManager *txn_mgr_;
    std::vector<LevelMetadata> levels_;          // Level 0-3 metadata
    mutable std::mutex mutex_;                    // Protects level metadata

    /**
     * Find overlapping SSTables in target level
     */
    void findOverlappingSSTables(uint32_t level,
                                  const std::vector<uint8_t> &min_key,
                                  const std::vector<uint8_t> &max_key,
                                  std::vector<std::string> *overlapping);

    /**
     * K-way merge implementation
     *
     * @param oit Oldest Interesting Transaction (OIT) for Firebird MGA garbage collection
     */
    Status kWayMerge(const std::vector<std::string> &source_sstables,
                     const std::string &output_path,
                     uint64_t oit,
                     ErrorContext *ctx);

    /**
     * Check if entry can be garbage collected (MGA rules)
     *
     * CRITICAL: Firebird MGA compliance
     * - Entry must be deleted (xmax != 0)
     * - Both xmin and xmax must be committed
     * - xmax < OIT (no transaction can see this version)
     *
     * Per Firebird spec: "Record versions created by transactions < OIT can be garbage collected"
     *
     * @param oit Oldest Interesting Transaction - boundary below which all transactions are committed
     */
    bool canGarbageCollect(const MergeEntry &entry, uint64_t oit) const;

    /**
     * Atomically replace old SSTables with new ones
     */
    Status replaceSSTablesAtomic(uint32_t target_level,
                                  const std::vector<std::string> &old_sstables,
                                  const std::vector<std::string> &new_sstables,
                                  ErrorContext *ctx);

    /**
     * Delete old SSTable files
     */
    void deleteOldSSTables(const std::vector<std::string> &sstable_paths);
};

} // namespace core
} // namespace scratchbird

