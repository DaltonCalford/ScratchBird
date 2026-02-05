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
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/uuidv7.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <vector>
#include <list>
#include <condition_variable>

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

    // ===========================================================================================
    // TRANSACTION MANAGER - LOCKING CONTRACT
    // ===========================================================================================
    //
    // ISSUE 3.9 FIX: Document mutex usage patterns for all methods
    // CRITICAL FIX (CRITICAL-3): Document complete lock ordering hierarchy to prevent deadlocks
    //
    // Mutex hierarchy (MUST be acquired in this order to prevent deadlock):
    //   1. mutex_ - Protects transaction state (next_xid_, oldest_xid_, cache, etc.)
    //   2. ProcArray::array_lock (pthread_rwlock) - Protects process control blocks
    //   3. group_commit_mutex_ - Protects group commit queue (independent of mutex_)
    //
    // Lock ordering rules:
    //   - If acquiring both mutex_ and ProcArray::array_lock:
    //     ALWAYS acquire mutex_ FIRST, then ProcArray::array_lock
    //   - If acquiring both ProcArray::alloc_lock and array_lock:
    //     ALWAYS acquire alloc_lock FIRST (per ProcArray's internal contract)
    //   - NEVER acquire mutex_ while holding ProcArray::array_lock (deadlock risk!)
    //   - group_commit_mutex_ is independent - can be acquired in any order relative to mutex_
    //
    // Locking conventions:
    //   - PUBLIC methods acquire locks internally (thread-safe)
    //   - PRIVATE helper methods may require caller to hold locks (documented per-method)
    //   - Never hold mutex_ during I/O operations (release before disk writes)
    //   - Group commit uses separate mutex to avoid blocking regular operations
    //
    // Examples of correct lock ordering:
    //   updateTransactionMarkers():  mutex_ → ProcArray::array_lock (rdlock) ✓ CORRECT
    //   isVersionVisible():          mutex_ → (no ProcArray lock needed) ✓ CORRECT
    //   beginTransaction():          mutex_ → ProcArray::array_lock (wrlock via setTransactionId) ✓ CORRECT
    //
    // ===========================================================================================

    // Transaction Manager - handles transaction lifecycle and visibility
    class TransactionManager
    {
    public:
        explicit TransactionManager(Database *db);
        ~TransactionManager();

        // ===========================================================================================
        // INITIALIZATION AND LOADING
        // ===========================================================================================

        // Initialize transaction subsystem
        // LOCKING: Called from load() which holds mutex_. Does NOT acquire mutex_ internally.
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Load existing transaction state from disk
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto load(ErrorContext *ctx = nullptr) -> Status;

        // ===========================================================================================
        // TRANSACTION LIFECYCLE
        // ===========================================================================================

        // Begin a new transaction
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext *ctx = nullptr)
            -> Status;

        // Commit a transaction
        // LOCKING: Thread-safe. Acquires mutex_ for pre-commit work, releases before I/O,
        //          then uses group_commit_mutex_ for group commit coordination.
        auto commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr)
            -> Status;

        // Rollback a transaction
        // LOCKING: Thread-safe. Acquires mutex_ for pre-rollback work, releases before I/O,
        //          then uses group_commit_mutex_ for group commit coordination.
        auto rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr)
            -> Status;

        // Prepare a transaction for 2PC (limbo state)
        // LOCKING: Thread-safe. Acquires mutex_ for pre-prepare work, releases before I/O.
        auto prepareTransaction(uint32_t proc_id, uint64_t xid, const std::string& gid,
                                const ID& owner_id, ErrorContext *ctx = nullptr) -> Status;

        // Commit a prepared (2PC) transaction
        // LOCKING: Thread-safe. Acquires mutex_ for state updates, releases before I/O.
        auto commitPreparedTransaction(const std::string& gid,
                                       ErrorContext *ctx = nullptr) -> Status;

        // Roll back a prepared (2PC) transaction
        // LOCKING: Thread-safe. Acquires mutex_ for state updates, releases before I/O.
        auto rollbackPreparedTransaction(const std::string& gid,
                                         ErrorContext *ctx = nullptr) -> Status;

        // ===========================================================================================
        // TRANSACTION STATE QUERIES
        // ===========================================================================================

        // Get transaction state
        // CRITICAL FIX (CRITICAL-2): Removed const because this method modifies transaction_cache_
        // Even though cache is mutable, removing const makes the API clearer and prevents misuse
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto getTransactionState(uint64_t xid, TransactionState &state_out,
                                 ErrorContext *ctx = nullptr) -> Status;

        // Check if a transaction is visible to another transaction (READ COMMITTED semantics)
        // CRITICAL FIX (CRITICAL-2 side-effect): Removed const because this calls getTransactionState()
        // which modifies the cache. This is part of the cache consistency fix.
        // LOCKING: Thread-safe. Acquires mutex_ internally via isXidInRange() and getTransactionState().
        auto isTransactionVisible(uint64_t xid, uint64_t current_xid) -> bool;

        // Validate XID is structurally valid (not INVALID_XID)
        // LOCKING: No locks required (static method, no shared state access).
        static auto isValidXid(uint64_t xid) -> bool;

        // Validate XID is in valid range for current database state
        // LOCKING: Thread-safe. Acquires mutex_ internally for range checks.
        auto isXidInRange(uint64_t xid) const -> bool;

        // ===========================================================================================
        // TRANSACTION ID AND MARKER QUERIES
        // ===========================================================================================

        // Get current transaction ID (for read-only operations)
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto getCurrentXid() const -> uint64_t
        {
            // Note: Lock not strictly needed for atomic read, but kept for consistency.
            // For non-transactional callers (no ConnectionContext), return the last
            // allocated XID (next_xid - 1) so visibility checks consider it in-range.
            std::lock_guard<std::mutex> lock(mutex_);
            uint64_t next = next_xid_.load(std::memory_order_acquire);
            if (next <= config::DEFAULT_INITIAL_XID)
            {
                return config::DEFAULT_INITIAL_XID;
            }
            return next - 1;
        }

        // Get oldest valid XID (OIT - for GC and XID validation)
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto getOldestXid() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return oldest_xid_;
        }

        // Get oldest active transaction (OAT)
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto getOldestActiveXid() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return oldest_active_xid_;
        }

        // Get oldest snapshot transaction (OST)
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto getOldestSnapshot() const -> uint64_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return oldest_snapshot_;
        }

        // Update oldest XID after GC/sweep completes
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto setOldestXid(uint64_t xid, ErrorContext *ctx = nullptr) -> Status;

        // Update transaction markers (called during transaction lifecycle)
        // LOCKING: Thread-safe. Acquires mutex_ internally, then acquires ProcArray read lock.
        //          Lock order: mutex_ → ProcArray::array_lock (rwlock read).
        auto updateTransactionMarkers(ErrorContext *ctx = nullptr) -> Status;

        // Check if approaching XID wraparound
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto isApproachingWraparound() const -> bool
        {
            // Note: Lock not strictly needed for atomic read, but kept for consistency
            std::lock_guard<std::mutex> lock(mutex_);
            return next_xid_.load(std::memory_order_acquire) > MAX_SAFE_XID;
        }

        // Get active transaction for a specific backend
        // LOCKING: Thread-safe. Uses ProcArrayManager API which handles locking internally.
        auto getBackendXid(uint32_t proc_id) const -> uint64_t;

        // ===========================================================================================
        // FIREBIRD MGA VISIBILITY API
        // ===========================================================================================

        /**
         * Check if a tuple version is visible to a transaction (Firebird MGA).
         * Uses TIP (Transaction Inventory Page) lookup, NOT snapshots.
         *
         * Per MGA_RULES.md Rule 3:
         * - Own changes always visible (version_xid == reader_xid)
         * - Otherwise: version must be COMMITTED and older than reader
         *
         * This is the CORE of Firebird MGA visibility semantics.
         *
         * @param version_xid Transaction ID that created the version
         * @param reader_xid Transaction ID of the reader
         * @return true if version is visible to reader
         *
         * LOCKING: Thread-safe. Acquires mutex_ internally via getTransactionState().
         */
        auto isVersionVisible(uint64_t version_xid, uint64_t reader_xid) -> bool;

        // ===========================================================================================
        // STATISTICS AND CONFIGURATION
        // ===========================================================================================

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

        // Get transaction statistics
        // LOCKING: Thread-safe. Acquires mutex_ internally.
        auto getStats() const -> Stats
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return stats_;
        }

        // Group commit control
        // LOCKING: Thread-safe. Uses atomic operations (no locks required).
        void enableGroupCommit(bool enabled)
        {
            group_commit_enabled_.store(enabled, std::memory_order_release);
        }

        // Set group commit timeout in microseconds
        // LOCKING: No locks required (simple assignment to non-shared variable).
        //          Note: Not thread-safe for concurrent updates, but safe for read/write from
        //          single configuration thread.
        void setGroupCommitTimeout(uint64_t timeout_us)
        {
            group_commit_timeout_us_ = timeout_us;
        }

        // Set group commit batch size
        // LOCKING: No locks required (simple assignment to non-shared variable).
        //          Note: Not thread-safe for concurrent updates, but safe for read/write from
        //          single configuration thread.
        void setGroupCommitBatchSize(uint32_t batch_size)
        {
            group_commit_batch_size_ = batch_size;
        }

        // Get group commit statistics
        // LOCKING: Thread-safe. Uses atomic operations (no locks required).
        auto getGroupCommitStats() const -> std::pair<uint64_t, uint64_t>
        {
            return {group_commits_performed_.load(std::memory_order_acquire),
                    group_commit_total_xids_.load(std::memory_order_acquire)};
        }

    private:
        // Group commit waiter structure
        struct CommitWaiter
        {
            uint64_t xid;
            TransactionState state;
            Status result;
            std::condition_variable cv;
            std::mutex cv_mutex;
            bool completed;

            CommitWaiter(uint64_t xid_, TransactionState state_)
                : xid(xid_), state(state_), result(Status::OK), completed(false)
            {
            }
        };
        Database *db_;
        BufferPool *buffer_pool_;
        PageManager *page_manager_;

        // Transaction state
        std::atomic<uint64_t> next_xid_{config::DEFAULT_INITIAL_XID}; // Next XID to allocate (NEXT) - ATOMIC for thread safety
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

        // Prepared transaction XIDs (2PC limbo); used to keep OAT pinned.
        std::unordered_set<uint64_t> prepared_xids_;

        // TIP page location cache (Issue 3.1 optimization)
        // Maps XID -> TIP page ID to avoid scanning entire chain
        // Marked mutable since caching is an internal optimization
        mutable std::unordered_map<uint64_t, uint32_t> tip_location_cache_;
        static constexpr uint32_t MAX_TIP_LOCATION_CACHE_SIZE = 1000; // Limit cache size
        mutable std::mutex tip_cache_mutex_; // Protects tip_location_cache_ only

        mutable std::mutex mutex_; // Thread safety for future

        // Group commit infrastructure
        std::mutex group_commit_mutex_;                  // Protects group commit queue
        std::vector<CommitWaiter *> commit_queue_;       // Queue of waiting commits
        bool group_commit_in_progress_{false};           // True if leader is processing
        std::atomic<bool> group_commit_enabled_{true};   // Configuration flag
        uint64_t group_commit_timeout_us_{10000};        // Wait up to 10ms for batch (configurable)
        uint32_t group_commit_batch_size_{32};           // Max batch size (configurable)

        // Group commit statistics
        std::atomic<uint64_t> group_commits_performed_{0}; // Total group commits performed
        std::atomic<uint64_t> group_commit_total_xids_{0}; // Total XIDs committed via group commit

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

        // ===========================================================================================
        // PRIVATE HELPER METHODS - LOCKING REQUIREMENTS
        // ===========================================================================================

        // TIP page management - calculate based on actual page size
        // LOCKING: No locks required (only reads page_size_ which is immutable after construction).
        [[nodiscard]] auto getTipEntriesPerPage() const -> uint32_t;

        // Helper methods for TIP management
        // LOCKING: No locks required (called from load() which holds mutex_).
        auto loadTipPage(uint32_t page_id, ErrorContext *ctx) -> Status;

        // LOCKING: No locks required (allocates page and writes header, no shared state).
        auto allocateTipPage(uint32_t &page_id_out, ErrorContext *ctx) -> Status;

        // LOCKING: No locks required internally. Updates TIP pages (disk I/O).
        //          May update tip_location_cache_ but doesn't require mutex_ (cache is best-effort).
        auto writeTipEntry(uint64_t xid, TransactionState state, ErrorContext *ctx) -> Status;

        // LOCKING: No locks required (reads TIP pages from disk via buffer pool).
        auto findTipEntry(uint64_t xid, TIPEntry &entry_out, ErrorContext *ctx) -> Status;

        // LOCKING: No locks required (performs fsync via Database API).
        auto flushTransactionState(ErrorContext *ctx) -> Status;

        // Check for XID wraparound based on transaction age
        // LOCKING: Requires mutex_ held by caller.
        // Returns Status::OK if safe, Status::PAGE_FULL if critical, Status::PAGE_CORRUPT if blocked
        auto checkXIDWraparound(ErrorContext *ctx) -> Status;

        // Group commit methods
        // LOCKING: No locks required (performs batch TIP writes via writeTipEntry()).
        auto writeTipEntriesBatch(const std::vector<std::pair<uint64_t, TransactionState>> &batch,
                                  ErrorContext *ctx) -> Status;

        // LOCKING: Acquires group_commit_mutex_ internally to collect waiters.
        //          Does NOT hold mutex_ (called after mutex_ released in commit/rollback).
        auto performGroupCommit(CommitWaiter *leader_waiter, ErrorContext *ctx) -> Status;

        // ===========================================================================================
        // LRU CACHE MANAGEMENT (PRIVATE HELPERS)
        // ===========================================================================================
        // Note: These methods are marked const because they only modify mutable cache state,
        // which doesn't affect logical const-ness. The cache is an implementation detail
        // for performance optimization and doesn't change the observable behavior.
        //
        // LOCKING REQUIREMENT: Caller MUST hold mutex_ before calling these methods.
        // These methods manipulate shared cache data structures and are NOT thread-safe on their own.
        // ===========================================================================================

        // Move entry to front of LRU (most recently used)
        // LOCKING: Requires mutex_ held by caller.
        void touchCacheEntry(uint64_t xid) const;

        // Remove least recently used entry from cache
        // LOCKING: Requires mutex_ held by caller.
        void evictOldestCacheEntry() const;

        // Add entry to cache with LRU tracking
        // LOCKING: Requires mutex_ held by caller.
        void addToCacheLRU(uint64_t xid, TransactionState state) const;

        // Remove entry from cache with LRU cleanup
        // LOCKING: Requires mutex_ held by caller.
        void removeFromCacheLRU(uint64_t xid) const;

        // P1-7: Binary search for XID in TIP entries array
        // Assumes entries are sorted by XID (which they should be due to monotonic allocation)
        // Returns index if found, or -1 if not found
        // LOCKING: No locks required (operates on pinned page buffer).
        static int32_t binarySearchTIPEntries(const TIPEntry *entries, uint32_t count, uint64_t xid);
    };

} // namespace scratchbird::core
