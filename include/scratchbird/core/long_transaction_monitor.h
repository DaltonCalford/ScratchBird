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
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class ProcArray;

    // Long transaction action policy
    enum class LongTransactionPolicy : uint8_t
    {
        LOG = 0,                 // Just log warnings
        ROLLBACK_READONLY = 1,   // Rollback read-only long transactions
        ROLLBACK_ALL = 2,        // Rollback any long transaction
        TERMINATE_CONNECTION = 3 // Force disconnect long transactions
    };

    // Statistics for long transaction monitoring
    struct LongTransactionStatistics
    {
        uint64_t warnings_logged;           // Total warnings logged
        uint64_t readonly_rolled_back;      // Read-only transactions rolled back
        uint64_t readwrite_rolled_back;     // Read-write transactions rolled back
        uint64_t connections_terminated;    // Connections terminated
        uint64_t last_check_time;           // Timestamp of last check (microseconds)
        uint32_t current_long_transactions; // Current number of long transactions

        LongTransactionStatistics()
            : warnings_logged(0), readonly_rolled_back(0), readwrite_rolled_back(0),
              connections_terminated(0), last_check_time(0), current_long_transactions(0)
        {
        }
    };

    // Long Transaction Monitor
    // Monitors active transactions and takes action on long-running transactions
    class LongTransactionMonitor
    {
    public:
        // Constructor - does not take ownership of Database
        explicit LongTransactionMonitor(Database *db);

        // Destructor - stops monitoring thread if running
        ~LongTransactionMonitor();

        // Initialize long transaction monitor
        Status initialize(ErrorContext *ctx = nullptr);

        // Start/stop monitoring thread
        Status startMonitoring(ErrorContext *ctx = nullptr);
        Status stopMonitoring(ErrorContext *ctx = nullptr);
        bool isMonitoring() const;

        // Enable/disable monitoring
        void enable();
        void disable();
        bool isEnabled() const;

        // Configuration
        void setWarningThreshold(uint32_t seconds);
        void setCriticalThreshold(uint32_t seconds);
        void setCheckInterval(uint32_t seconds);
        void setPolicy(LongTransactionPolicy policy);

        uint32_t getWarningThreshold() const
        {
            return warning_threshold_seconds_;
        }
        uint32_t getCriticalThreshold() const
        {
            return critical_threshold_seconds_;
        }
        uint32_t getCheckInterval() const
        {
            return check_interval_seconds_;
        }
        LongTransactionPolicy getPolicy() const
        {
            return policy_;
        }

        // Statistics
        LongTransactionStatistics getStatistics() const;

        // Manual check for long transactions (can be called directly)
        uint32_t checkLongTransactions(ErrorContext *ctx = nullptr);

    private:
        Database *db_;

        // Configuration
        std::atomic<bool> enabled_;
        std::atomic<uint32_t> warning_threshold_seconds_;  // Warn after this many seconds
        std::atomic<uint32_t> critical_threshold_seconds_; // Take action after this many seconds
        std::atomic<uint32_t> check_interval_seconds_;     // Check every N seconds
        LongTransactionPolicy policy_;                     // Action policy

        // Monitoring thread
        std::thread monitor_thread_;
        std::atomic<bool> monitoring_;
        std::atomic<bool> shutdown_requested_;

        // Wake mechanism for monitoring thread
        std::mutex wake_mutex_;
        std::condition_variable wake_cv_;

        // Statistics
        mutable std::mutex stats_mutex_;
        LongTransactionStatistics stats_;

        // Internal methods
        void monitoringLoop();
        void checkAndActOnTransaction(uint32_t proc_id, uint64_t xid, uint64_t age_seconds,
                                      bool is_read_only, ErrorContext *ctx);
        void readConfiguration();
    };

} // namespace scratchbird::core
