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
#include <vector>
#include <atomic>
#include <string>
#include <pthread.h>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    struct ErrorContext;

    // Process control block (per connection/backend)
    struct ProcessControlBlock
    {
        uint32_t proc_id;  // Process ID (slot number)
        pid_t backend_pid; // OS process ID
        bool is_active;    // Is this slot active?
        ID session_id;     // Bound session UUID (0 = none)

        // Transaction state
        uint64_t xid;             // Current transaction XID (0 = none)
        uint64_t backend_xmin;    // Snapshot horizon for this backend
        uint64_t xmin;            // Oldest XID visible to this backend
        uint8_t isolation_level;  // IsolationLevel enum value (0=READ_COMMITTED, 2=SNAPSHOT, etc.)
        bool is_snapshot_txn;     // True if using SNAPSHOT isolation
        bool is_read_only;        // True if transaction is read-only
        uint64_t xact_start_time; // Transaction start timestamp (microseconds)

        // Locking state (for future lock manager)
        uint32_t wait_lock_id;       // Lock waiting for (0 = none)
        bool deadlock_check_pending; // Needs deadlock check

        // Statistics
        uint64_t start_time;       // Backend start timestamp (microseconds)
        uint64_t query_start_time; // Current query start (0 = idle)
        uint64_t state_change_time; // Last state change timestamp (microseconds)

        // Current query text (truncated, UTF-8 bytes)
        char query_text[256];

        // Connection termination (for long transaction monitor)
        bool termination_requested; // Backend should terminate connection

        // Padding for cache line alignment (adjusted for new fields)
        uint8_t padding[8];
    };

    // Process array (shared memory structure)
    struct ProcArray
    {
        // Configuration
        uint32_t max_backends; // Maximum number of backends

        // Global transaction state
        uint64_t latest_completed_xid; // Latest completed XID
        uint64_t oldest_xmin;          // Oldest xmin across all backends

        // Free list management
        uint32_t first_free; // First free slot (linked via proc_id)
        uint32_t num_active; // Number of active backends

        // Synchronization
        pthread_rwlock_t array_lock; // Read-write lock for array
        pthread_mutex_t alloc_lock;  // Lock for slot allocation

        // Process control blocks follow this header
        // ProcessControlBlock procs[max_backends];
    };

    // ProcArray Manager - manages backend registration and transaction tracking
    class ProcArrayManager
    {
    public:
        // Initialize ProcArray in shared memory
        static auto initialize(Database *db, uint32_t max_backends, ErrorContext *ctx = nullptr)
            -> Status;

        // Shutdown and cleanup
        static auto shutdown(ErrorContext *ctx = nullptr) -> Status;

        // Backend registration
        static auto registerBackend(uint32_t *proc_id_out, ErrorContext *ctx = nullptr) -> Status;

        static auto unregisterBackend(uint32_t proc_id, ErrorContext *ctx = nullptr) -> Status;

        // Transaction tracking
        static auto setTransactionId(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr)
            -> Status;

        static auto clearTransactionId(uint32_t proc_id, ErrorContext *ctx = nullptr) -> Status;

        // Set transaction isolation level
        static auto setIsolationLevel(uint32_t proc_id, uint8_t isolation_level,
                                      ErrorContext *ctx = nullptr) -> Status;

        // Set transaction read-only flag
        static auto setTransactionReadOnly(uint32_t proc_id, bool is_read_only,
                                           ErrorContext *ctx = nullptr) -> Status;

        // Set transaction start time
        static auto setTransactionStartTime(uint32_t proc_id, uint64_t start_time,
                                            ErrorContext *ctx = nullptr) -> Status;

        // Session binding (for monitoring)
        static auto setSessionId(uint32_t proc_id, const ID& session_id,
                                 ErrorContext *ctx = nullptr) -> Status;

        // Query tracking (for monitoring)
        static auto setQueryInfo(uint32_t proc_id, uint64_t start_time,
                                 const std::string& query_text,
                                 ErrorContext *ctx = nullptr) -> Status;

        static auto clearQueryInfo(uint32_t proc_id, uint64_t state_change_time,
                                   ErrorContext *ctx = nullptr) -> Status;

        // Snapshot support
        static auto getActiveTransactions(std::vector<uint64_t> *xids_out,
                                          uint64_t *oldest_xmin_out, ErrorContext *ctx = nullptr)
            -> Status;

        // Vacuum support
        static auto getGcHorizon(uint64_t *horizon_out, ErrorContext *ctx = nullptr) -> Status;

        // Backend info queries
        static auto getBackendXmin(uint32_t proc_id, uint64_t *xmin_out,
                                   ErrorContext *ctx = nullptr) -> Status;

        static auto setBackendXmin(uint32_t proc_id, uint64_t xmin, ErrorContext *ctx = nullptr)
            -> Status;

        static auto getBackendXid(uint32_t proc_id, uint64_t *xid_out, ErrorContext *ctx = nullptr)
            -> Status;

        // Statistics
        static auto getNumActiveBackends(uint32_t *count_out, ErrorContext *ctx = nullptr)
            -> Status;

        // Get all active backends (for monitoring)
        static auto getAllActiveBackends(std::vector<ProcessControlBlock> *backends_out,
                                         ErrorContext *ctx = nullptr) -> Status;

        // Request backend termination (for long transaction monitor)
        static auto requestBackendTermination(uint32_t proc_id, ErrorContext *ctx = nullptr)
            -> Status;

        // Check if termination requested (for backend to poll)
        static auto isTerminationRequested(uint32_t proc_id, bool *requested_out,
                                           ErrorContext *ctx = nullptr) -> Status;

        // Clear termination request (after handling)
        static auto clearTerminationRequest(uint32_t proc_id, ErrorContext *ctx = nullptr)
            -> Status;

        // Get ProcArray instance (for internal use)
        static auto getInstance() -> ProcArray *;

    private:
        static std::atomic<ProcArray *> proc_array_;
        static Database *database_;

        // Helper: Get PCB by proc_id
        static auto getPCB(uint32_t proc_id) -> ProcessControlBlock *;

        // Helper: Allocate free slot
        static auto allocateSlot(uint32_t *proc_id_out) -> Status;

        // Helper: Free slot
        static auto freeSlot(uint32_t proc_id) -> Status;
    };

} // namespace scratchbird::core
