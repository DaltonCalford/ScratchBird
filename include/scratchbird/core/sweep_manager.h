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
#include <cstdint>
#include <mutex>
#include <atomic>
#include <chrono>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class TransactionManager;
    class BufferPool;

    // Sweep statistics for monitoring
    struct SweepStatistics
    {
        uint64_t sweep_count = 0;              // Total sweeps executed
        uint64_t last_sweep_time = 0;          // Timestamp of last sweep (microseconds since epoch)
        uint64_t last_sweep_duration_ms = 0;   // Duration in milliseconds
        uint64_t last_oit_before = 0;          // OIT before last sweep
        uint64_t last_oit_after = 0;           // OIT after last sweep
        uint64_t total_transactions_swept = 0; // Cumulative count of transactions swept
        bool sweep_in_progress = false;        // Is sweep currently running?
    };

    // SweepManager - Manages database sweep operations
    //
    // Sweep advances the Oldest Interesting Transaction (OIT) marker by:
    // 1. Scanning Transaction Inventory Pages (TIP) to find committed/aborted transactions
    // 2. Finding the first uncommitted transaction (becomes new OIT)
    // 3. Updating the database header with new OIT
    // 4. Optionally reclaiming space from old tuple versions (foreground mode)
    //
    // Sweep is triggered when: (OST - OIT) > sweep_interval
    // Where OST = Oldest Snapshot Transaction, OIT = Oldest Interesting Transaction
    class SweepManager
    {
    public:
        SweepManager(Database *db);
        ~SweepManager();

        // Explicitly delete copy operations
        SweepManager(const SweepManager &) = delete;
        SweepManager &operator=(const SweepManager &) = delete;

        // Initialize sweep manager
        Status initialize(ErrorContext *ctx = nullptr);

        // Check if sweep should be triggered based on transaction gap
        // Called after transaction commit
        // Returns true if sweep was triggered
        bool checkSweepTrigger(ErrorContext *ctx = nullptr);

        // Execute sweep process
        // foreground: true = full sweep with space reclamation, false = background (OIT advancement
        // only) Returns Status::OK on success
        Status executeSweep(bool foreground, ErrorContext *ctx = nullptr);

        // Get current sweep statistics
        SweepStatistics getStatistics() const;

        // Check if sweep is currently running
        bool isSweepInProgress() const
        {
            return sweep_in_progress_.load(std::memory_order_acquire);
        }

    private:
        Database *db_;
        TransactionManager *txn_manager_;
        BufferPool *buffer_pool_;

        // Sweep statistics (protected by mutex)
        mutable std::mutex stats_mutex_;
        SweepStatistics stats_;

        // Sweep in progress flag (atomic for lock-free check)
        std::atomic<bool> sweep_in_progress_{false};

        // Helper methods

        // Scan TIP pages to find first uncommitted transaction
        // Returns new OIT (or 0 if no change needed)
        uint64_t findFirstUncommittedTransaction(ErrorContext *ctx) const;

        // Reclaim space from old tuple versions (foreground sweep only)
        // Removes versions with xmax < new_oit
        Status reclaimSpace(uint64_t new_oit, ErrorContext *ctx);

        // Update sweep statistics
        void updateStatistics(uint64_t oit_before, uint64_t oit_after, uint64_t duration_ms);
    };

} // namespace scratchbird::core
