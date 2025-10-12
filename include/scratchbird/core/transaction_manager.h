#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/config.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <list>

namespace scratchbird::core
{

    // Forward declarations
    class Database;
    class BufferPool;
    class PageManager;
    struct ErrorContext;

    // Transaction states
    enum class TransactionState : uint8_t
    {
        ACTIVE = 0,
        COMMITTED = 1,
        ABORTED = 2,
        PREPARED = 3, // For future 2PC support
    };

    // Transaction information
    struct TransactionInfo
    {
        uint64_t xid;
        TransactionState state;
        uint64_t start_time; // Microseconds since epoch
        uint64_t end_time;   // Microseconds since epoch (0 if active)
    };

// Transaction Inventory Page (TIP) format
// TIP pages track transaction states for MVCC visibility
#pragma pack(push, 1)
    struct TIPPageHeader
    {
        PageHeader page_header;    // Standard page header
        uint64_t min_xid;          // Minimum XID in this page
        uint64_t max_xid;          // Maximum XID in this page
        uint32_t num_transactions; // Number of transactions in this page
        uint32_t next_tip_page;    // Next TIP page ID (0 if last)
        uint8_t reserved[20];      // Reserved for future use
    };

    // Each transaction entry in TIP page
    struct TIPEntry
    {
        uint64_t xid;         // Transaction ID
        uint8_t state;        // TransactionState
        uint8_t flags;        // Reserved flags
        uint16_t reserved;    // Alignment padding
        uint64_t commit_time; // Commit/abort timestamp
    };
#pragma pack(pop)

    // Transaction Manager - handles transaction lifecycle and visibility
    class TransactionManager
    {
    public:
        explicit TransactionManager(Database *db);
        ~TransactionManager();

        // Initialize transaction subsystem
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Load existing transaction state from disk
        auto load(ErrorContext *ctx = nullptr) -> Status;

        // Begin a new transaction
        auto beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext *ctx = nullptr)
            -> Status;

        // Commit a transaction
        auto commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr)
            -> Status;

        // Rollback a transaction
        auto rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr)
            -> Status;

        // Get transaction state
        auto getTransactionState(uint64_t xid, TransactionState &state_out,
                                 ErrorContext *ctx = nullptr) const -> Status;

        // Check if a transaction is visible to another transaction (READ COMMITTED semantics)
        auto isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) -> bool;

        // Validate XID is structurally valid (not INVALID_XID)
        static auto isValidXid(uint64_t xid) -> bool;

        // Validate XID is in valid range for current database state
        auto isXidInRange(uint64_t xid) const -> bool;

        // Get current transaction ID (for read-only operations)
        auto getCurrentXid() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return next_xid_;
        }

        // Get oldest valid XID (OIT - for VACUUM and XID validation)
        auto getOldestXid() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return oldest_xid_;
        }

        // Get oldest active transaction (OAT)
        auto getOldestActiveXid() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return oldest_active_xid_;
        }

        // Get oldest snapshot transaction (OST)
        auto getOldestSnapshot() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return oldest_snapshot_;
        }

        // Update oldest XID after VACUUM/sweep completes
        auto setOldestXid(uint64_t xid, ErrorContext *ctx = nullptr) -> Status;

        // Update transaction markers (called during transaction lifecycle)
        auto updateTransactionMarkers(ErrorContext *ctx = nullptr) -> Status;

        // Check if approaching XID wraparound
        auto isApproachingWraparound() const -> bool
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return next_xid_ > MAX_SAFE_XID;
        }

        // Get active transaction for a specific backend
        auto getBackendXid(uint32_t proc_id) const -> uint64_t;

        // Snapshot isolation support
        struct Snapshot
        {
            uint64_t xmin;                     // Oldest active XID
            uint64_t xmax;                     // Next XID to be assigned
            std::vector<uint64_t> active_xids; // Active XIDs at snapshot time

            // MVCC cross-page pin tracking
            // When following version chains across pages, we pin pages for the snapshot duration
            std::vector<uint32_t> pinned_pages; // Pages pinned for this snapshot
            BufferPool *buffer_pool =
                nullptr; // BufferPool to unpin pages (set when first pin occurs)

            // Cleanup method - unpins all pages when snapshot released
            void cleanup();

            ~Snapshot();
        };

        // Get current snapshot (for future MVCC)
        auto getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx = nullptr) -> Status;

        // Check if a transaction is visible using snapshot isolation (SNAPSHOT semantics)
        // Returns true if xid is visible according to the snapshot
        auto isSnapshotVisible(uint64_t xid, const Snapshot *snapshot) const -> bool;

        // Statistics
        struct Stats
        {
            uint64_t transactions_started = 0;   // Total transactions started
            uint64_t transactions_committed = 0; // Total transactions committed
            uint64_t transactions_aborted = 0;   // Total transactions aborted

            // READ ONLY transaction optimizations (Phase 3)
            uint64_t readonly_transactions = 0;           // Read-only transactions started
            uint64_t readonly_committed = 0;              // Read-only transactions committed
            uint64_t readonly_aborted = 0;                // Read-only transactions aborted
            uint64_t readonly_snapshots = 0;              // Snapshots created for read-only txns
            uint64_t readonly_snapshot_xids_filtered = 0; // XIDs filtered from read-only snapshots
        };

        auto getStats() const -> Stats
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return stats_;
        }

    private:
        Database *db_;
        BufferPool *buffer_pool_;
        PageManager *page_manager_;

        // Transaction state
        uint64_t next_xid_ = config::DEFAULT_INITIAL_XID; // Next XID to allocate (NEXT)
        uint64_t oldest_xid_ = FROZEN_XID + 1;            // Oldest Interesting Transaction (OIT)
        uint64_t oldest_active_xid_ = 0;                  // Oldest Active Transaction (OAT)
        uint64_t oldest_snapshot_ = 0;                    // Oldest Snapshot Transaction (OST)
        uint32_t tip_root_page_ = 0;                      // Root TIP page ID

        // In-memory cache of recent transactions (LRU cache)
        // Marked mutable since caching is an internal optimization that doesn't affect logical
        // constness
        mutable std::unordered_map<uint64_t, TransactionState> transaction_cache_;
        mutable std::list<uint64_t>
            cache_lru_list_; // LRU list: front = most recent, back = least recent
        mutable std::unordered_map<uint64_t, std::list<uint64_t>::iterator>
            cache_lru_map_;        // XID -> position in LRU list
        mutable std::mutex mutex_; // Thread safety for future

        // Statistics
        Stats stats_;

        // Special transaction IDs
        static constexpr uint64_t INVALID_XID = 0;
        static constexpr uint64_t BOOTSTRAP_XID = 1;
        static constexpr uint64_t FROZEN_XID = 2;

        // Cache limits
        static constexpr uint32_t MAX_CACHE_SIZE =
            config::DEFAULT_TRANSACTION_CACHE_SIZE; // Maximum number of cached transactions

        // XID wraparound protection
        static constexpr uint64_t XID_WRAPAROUND_THRESHOLD =
            1000000; // Trigger autovacuum when this close to UINT64_MAX
        static constexpr uint64_t MAX_SAFE_XID = UINT64_MAX - XID_WRAPAROUND_THRESHOLD;

        // TIP page management - calculate based on actual page size
        [[nodiscard]] auto getTipEntriesPerPage() const -> uint32_t;

        // Helper methods
        auto loadTipPage(uint32_t page_id, ErrorContext *ctx) -> Status;
        auto allocateTipPage(uint32_t &page_id_out, ErrorContext *ctx) -> Status;
        auto writeTipEntry(uint64_t xid, TransactionState state, ErrorContext *ctx) -> Status;
        auto findTipEntry(uint64_t xid, TIPEntry &entry_out, ErrorContext *ctx) -> Status;
        auto flushTransactionState(ErrorContext *ctx) -> Status;

        // LRU cache management (const because they modify mutable cache state)
        void touchCacheEntry(uint64_t xid) const; // Move entry to front of LRU
        void evictOldestCacheEntry() const;       // Remove least recently used entry
        void addToCacheLRU(uint64_t xid, TransactionState state) const; // Add with LRU tracking
        void removeFromCacheLRU(uint64_t xid) const;                    // Remove with LRU cleanup
    };

} // namespace scratchbird::core
