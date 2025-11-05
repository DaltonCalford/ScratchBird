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
#include <cstdint>
#include <string>

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

} // namespace core
} // namespace scratchbird
