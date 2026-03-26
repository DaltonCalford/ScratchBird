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

    enum class LongTransactionReason : uint8_t
    {
        NONE = 0,
        AGE_THRESHOLD = 1 << 0,
        GC_HORIZON_PIN = 1 << 1,
        SNAPSHOT_HORIZON_PIN = 1 << 2,
    };

    constexpr inline auto operator|(LongTransactionReason lhs, LongTransactionReason rhs)
        -> LongTransactionReason
    {
        return static_cast<LongTransactionReason>(static_cast<uint8_t>(lhs) |
                                                  static_cast<uint8_t>(rhs));
    }

    constexpr inline auto hasLongTransactionReason(LongTransactionReason mask,
                                                   LongTransactionReason bit) -> bool
    {
        return (static_cast<uint8_t>(mask) & static_cast<uint8_t>(bit)) != 0;
    }

    // Long transaction action policy
    enum class LongTransactionPolicy : uint8_t
    {
        LOG = 0,                 // Just log warnings
        ROLLBACK_READONLY = 1,   // Request backend-owned rollback for read-only transactions
        ROLLBACK_ALL = 2,        // Request backend-owned rollback for any long transaction
        TERMINATE_CONNECTION = 3 // Force disconnect long transactions
    };

    // Statistics for long transaction monitoring
    struct LongTransactionStatistics
    {
        uint64_t warnings_logged;           // Total warnings logged
        uint64_t notices_emitted;           // Attachment-visible notices queued
        uint64_t readonly_rolled_back;      // Read-only rollback directives issued
        uint64_t readwrite_rolled_back;     // Read-write rollback directives issued
        uint64_t connections_terminated;    // Connection termination directives issued
        uint64_t gc_horizon_flags;          // GC horizon pin detections
        uint64_t snapshot_horizon_flags;    // Snapshot horizon pin detections
        uint64_t last_check_time;           // Timestamp of last check (microseconds)
        uint32_t current_long_transactions; // Current number of long transactions
        uint32_t last_proc_id;              // Last governed backend
        uint64_t last_xid;                  // Last governed xid
        uint64_t last_age_seconds;          // Last governed age
        uint64_t last_xid_lag;              // Last OAT lag at governance time
        uint64_t last_snapshot_lag;         // Last OST lag at governance time
        uint8_t last_reason_mask;           // LongTransactionReason bits
        uint8_t last_policy_action;         // LongTransactionPolicy value

        LongTransactionStatistics()
            : warnings_logged(0), notices_emitted(0), readonly_rolled_back(0),
              readwrite_rolled_back(0), connections_terminated(0), gc_horizon_flags(0),
              snapshot_horizon_flags(0), last_check_time(0), current_long_transactions(0),
              last_proc_id(0), last_xid(0), last_age_seconds(0), last_xid_lag(0),
              last_snapshot_lag(0), last_reason_mask(0), last_policy_action(0)
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
        void setWarningXidLagThreshold(uint64_t xid_lag);
        void setCriticalXidLagThreshold(uint64_t xid_lag);
        void setCheckInterval(uint32_t seconds);
        void setPolicy(LongTransactionPolicy policy);
        void setClientNoticeEnabled(bool enabled);

        uint32_t getWarningThreshold() const
        {
            return warning_threshold_seconds_.load(std::memory_order_acquire);
        }
        uint32_t getCriticalThreshold() const
        {
            return critical_threshold_seconds_.load(std::memory_order_acquire);
        }
        uint32_t getCheckInterval() const
        {
            return check_interval_seconds_.load(std::memory_order_acquire);
        }
        uint64_t getWarningXidLagThreshold() const
        {
            return warning_xid_lag_threshold_.load(std::memory_order_acquire);
        }
        uint64_t getCriticalXidLagThreshold() const
        {
            return critical_xid_lag_threshold_.load(std::memory_order_acquire);
        }
        LongTransactionPolicy getPolicy() const
        {
            return policy_;
        }
        bool clientNoticeEnabled() const
        {
            return client_notice_enabled_.load(std::memory_order_acquire);
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
        std::atomic<uint64_t> warning_xid_lag_threshold_;  // Warn if OAT/OST lag reaches this
        std::atomic<uint64_t> critical_xid_lag_threshold_; // Act if OAT/OST lag reaches this
        std::atomic<uint32_t> check_interval_seconds_;     // Check every N seconds
        std::atomic<bool> client_notice_enabled_;          // Queue attachment notices on breach
        LongTransactionPolicy policy_;                     // Action policy

        // Monitoring thread
        std::thread monitor_thread_;
        bool monitoring_ = false;         // Guarded by wake_mutex_
        bool shutdown_requested_ = false; // Guarded by wake_mutex_

        // Wake mechanism for monitoring thread
        mutable std::mutex wake_mutex_;
        std::condition_variable wake_cv_;

        // Statistics
        mutable std::mutex stats_mutex_;
        LongTransactionStatistics stats_;

        // Internal methods
        void monitoringLoop();
        void checkAndActOnTransaction(uint32_t proc_id, uint64_t xid, uint64_t age_seconds,
                                      uint64_t xid_lag, uint64_t snapshot_lag,
                                      LongTransactionReason reason_mask,
                                      bool is_read_only,
                                      ErrorContext *ctx);
        void readConfiguration();
    };

} // namespace scratchbird::core
