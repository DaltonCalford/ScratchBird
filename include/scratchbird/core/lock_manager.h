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
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <list>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    struct ErrorContext;

    // Lock modes (from PostgreSQL)
    enum class LockMode : uint8_t
    {
        LOCK_ACCESS_SHARE = 1,           // SELECT
        LOCK_ROW_SHARE = 2,              // SELECT FOR UPDATE/SHARE
        LOCK_ROW_EXCLUSIVE = 3,          // UPDATE, DELETE, INSERT
        LOCK_SHARE_UPDATE_EXCLUSIVE = 4, // VACUUM, CREATE INDEX CONCURRENTLY
        LOCK_SHARE = 5,                  // CREATE INDEX
        LOCK_SHARE_ROW_EXCLUSIVE = 6,    // LOCK TABLE ... SHARE ROW EXCLUSIVE
        LOCK_EXCLUSIVE = 7,              // ALTER TABLE, DROP TABLE
        LOCK_ACCESS_EXCLUSIVE = 8        // ALTER TABLE, DROP TABLE, TRUNCATE
    };

    // Lock target (what is being locked)
    enum class LockTarget : uint8_t
    {
        LOCK_TARGET_DATABASE,
        LOCK_TARGET_TABLE,
        LOCK_TARGET_PAGE,
        LOCK_TARGET_TUPLE
    };

    // Lock tag (identifies a lockable object)
    struct LockTag
    {
        LockTarget target_type;
        UuidV7Bytes object_uuid; // Table/Index UUID
        uint64_t page_num;       // For page locks
        uint16_t offset_num;     // For tuple locks
        uint16_t padding;        // Alignment

        // Comparison for use in maps
        bool operator==(const LockTag &other) const
        {
            return target_type == other.target_type && object_uuid == other.object_uuid &&
                   page_num == other.page_num && offset_num == other.offset_num;
        }

        bool operator!=(const LockTag &other) const
        {
            return !(*this == other);
        }

        // Hash function
        struct Hash
        {
            size_t operator()(const LockTag &tag) const
            {
                size_t h1 = std::hash<uint8_t>{}(static_cast<uint8_t>(tag.target_type));
                size_t h2 = 0;
                for (size_t i = 0; i < 16; ++i)
                {
                    h2 ^= std::hash<uint8_t>{}(tag.object_uuid.bytes[i]) << i;
                }
                size_t h3 = std::hash<uint64_t>{}(tag.page_num);
                size_t h4 = std::hash<uint16_t>{}(tag.offset_num);
                return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
            }
        };
    };

    // Lock request (one backend requesting a lock)
    struct LockRequest
    {
        uint32_t proc_id;      // Backend requesting lock
        LockMode mode;         // Requested mode
        bool granted;          // Is lock granted?
        uint64_t request_time; // When requested (microseconds)
    };

    struct Lock;

    struct ProcLockEntry
    {
        Lock *lock = nullptr;
        LockMode mode = LockMode::LOCK_ACCESS_SHARE;
    };

    struct LockSnapshot
    {
        LockTag tag;
        LockMode mode = LockMode::LOCK_ACCESS_SHARE;
        uint32_t proc_id = 0;
        bool granted = false;
        uint64_t request_time = 0;
    };

    // Lock object (one lockable resource)
    struct Lock
    {
        LockTag tag; // What is locked

        // Granted locks (bitmask of granted modes)
        uint32_t granted_mask;      // Bit i = mode i granted
        uint32_t granted_counts[8]; // Count per mode (0-indexed from LOCK_ACCESS_SHARE-1)

        // Waiting queue (RAII-managed with unique_ptr)
        std::list<std::unique_ptr<LockRequest>> wait_queue;

        // Statistics
        uint64_t total_acquisitions;
        uint64_t total_waits;
    };

    // Lock statistics
    struct LockStats
    {
        uint64_t locks_acquired;
        uint64_t locks_released;
        uint64_t lock_waits;
        uint64_t deadlocks_detected;
        uint64_t lock_timeouts;
        uint32_t current_locks;
        uint32_t max_locks_used;

        // READ ONLY transaction optimizations (Phase 3)
        uint64_t readonly_locks_acquired; // Locks acquired by read-only transactions
        uint64_t readonly_fast_path;      // Fast-path acquisitions (no conflicts)
        uint64_t readonly_lock_waits;     // Read-only transactions that had to wait
    };

    // Deadlock detector (forward declaration)
    class DeadlockDetector;

    // Lock Manager - manages all locks in the database
    class LockManager
    {
        // Allow DeadlockDetector to access internal lock_table_ for building wait graph
        friend class DeadlockDetector;

    public:
        explicit LockManager(Database *db);
        ~LockManager();

        // Initialization
        Status initialize(ErrorContext *ctx = nullptr);

        // Shutdown
        Status shutdown(ErrorContext *ctx = nullptr);

        // Lock acquisition
        Status acquireLock(uint32_t proc_id, const LockTag &tag, LockMode mode,
                           bool wait,           // Block if conflict?
                           uint32_t timeout_ms, // Wait timeout (0 = infinite)
                           ErrorContext *ctx = nullptr);

        // Lock release
        Status releaseLock(uint32_t proc_id, const LockTag &tag, LockMode mode,
                           ErrorContext *ctx = nullptr);

        // Release all locks for a backend (on disconnect/abort)
        Status releaseAllLocks(uint32_t proc_id, ErrorContext *ctx = nullptr);

        // Check if lock conflicts
        bool checkConflict(const LockTag &tag, LockMode mode) const;

        // Get statistics
        void getStatistics(LockStats *stats_out) const;

        // Snapshot current locks (for monitoring views)
        Status listLocks(std::vector<LockSnapshot>& locks_out) const;

        // Deadlock detection (called periodically or on timeout)
        Status detectDeadlocks(ErrorContext *ctx = nullptr);

    private:
        Database *db_;

        // Lock tables (lock_table_ owns Lock objects via unique_ptr)
        std::unordered_map<LockTag, std::unique_ptr<Lock>, LockTag::Hash> lock_table_;
        std::unordered_multimap<uint32_t, ProcLockEntry> proc_locks_; // By proc_id (non-owning references)

        // Synchronization
        mutable std::mutex lock_table_mutex_;
        std::condition_variable lock_wait_cv_;

        // Deadlock detection
        std::unique_ptr<DeadlockDetector> deadlock_detector_;

        // Statistics
        LockStats stats_;

        // Configuration
        uint32_t max_locks_;
        uint32_t deadlock_timeout_ms_;

        // Lock conflict matrix [held_mode][requested_mode]
        static const bool conflict_matrix_[8][8];

        // Helper methods
        Lock *findOrCreateLock(const LockTag &tag);
        void removeLockIfUnused(const LockTag &tag);
        void grantWaitingLocks(Lock *lock);
        bool checkConflictInternal(const Lock *lock, LockMode mode, uint32_t skip_proc_id) const;

        // READ ONLY transaction optimization helpers
        bool isReadOnlyTransaction(uint32_t proc_id) const;
    };

    // Deadlock detector - detects cycles in wait-for graph
    class DeadlockDetector
    {
    public:
        explicit DeadlockDetector(LockManager *lock_mgr);
        ~DeadlockDetector();

        // Run deadlock detection
        Status detectDeadlocks(ErrorContext *ctx = nullptr);

        // Check if adding wait would create cycle
        bool wouldCreateCycle(uint32_t waiter, uint32_t holder);

    private:
        LockManager *lock_mgr_;

        // Wait-for graph (waiter -> holders)
        std::unordered_map<uint32_t, std::vector<uint32_t>> wait_graph_;

        // Build wait-for graph from current lock state
        void buildWaitGraph();

        // DFS for cycle detection
        bool hasCycle(uint32_t start_proc, std::unordered_set<uint32_t> *visited,
                      std::unordered_set<uint32_t> *rec_stack);

        // Detect all cycles
        std::vector<std::vector<uint32_t>> findAllCycles();

        // Select victim from cycle (abort youngest transaction)
        uint32_t selectVictim(const std::vector<uint32_t> &cycle);

        // Abort transaction to break deadlock
        Status abortTransaction(uint32_t proc_id, ErrorContext *ctx);
    };

} // namespace scratchbird::core
