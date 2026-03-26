/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/logger.h"
#include <chrono>
#include <functional>

namespace scratchbird::core
{
    namespace
    {
        auto toReasonMask(LongTransactionReason reasons) -> uint8_t
        {
            return static_cast<uint8_t>(reasons);
        }

        auto describeReasons(LongTransactionReason reasons) -> std::string
        {
            if (reasons == LongTransactionReason::NONE)
            {
                return "none";
            }

            std::string result;
            auto append = [&result](const char *token) {
                if (!result.empty())
                {
                    result += ",";
                }
                result += token;
            };

            if (hasLongTransactionReason(reasons, LongTransactionReason::AGE_THRESHOLD))
            {
                append("age_threshold");
            }
            if (hasLongTransactionReason(reasons, LongTransactionReason::GC_HORIZON_PIN))
            {
                append("gc_horizon_pin");
            }
            if (hasLongTransactionReason(reasons, LongTransactionReason::SNAPSHOT_HORIZON_PIN))
            {
                append("snapshot_horizon_pin");
            }

            return result;
        }

        auto formatGovernanceMessage(LongTransactionPolicy policy,
                                     uint64_t xid,
                                     uint64_t age_seconds,
                                     uint64_t xid_lag,
                                     uint64_t snapshot_lag,
                                     LongTransactionReason reasons) -> std::string
        {
            std::string action =
                policy == LongTransactionPolicy::TERMINATE_CONNECTION
                    ? "Connection termination requested by long-running transaction policy"
                : policy == LongTransactionPolicy::ROLLBACK_ALL ||
                        policy == LongTransactionPolicy::ROLLBACK_READONLY
                    ? "Transaction rollback requested by long-running transaction policy"
                    : "Long-running transaction warning";

            return action + ": xid=" + std::to_string(xid) +
                   ", age_seconds=" + std::to_string(age_seconds) +
                   ", xid_lag=" + std::to_string(xid_lag) +
                   ", snapshot_lag=" + std::to_string(snapshot_lag) +
                   ", reasons=" + describeReasons(reasons);
        }
    } // namespace

    LongTransactionMonitor::LongTransactionMonitor(Database *db)
        : db_(db), enabled_(true), warning_threshold_seconds_(600) // 10 minutes default
          ,
          critical_threshold_seconds_(3600) // 1 hour default
          ,
          warning_xid_lag_threshold_(0) // Disabled until explicitly configured
          ,
          critical_xid_lag_threshold_(0) // Disabled until explicitly configured
          ,
          check_interval_seconds_(60) // Check every minute default
          ,
          client_notice_enabled_(true), policy_(LongTransactionPolicy::LOG)
    {
    }

    LongTransactionMonitor::~LongTransactionMonitor()
    {
        // Best-effort shutdown; destructor should never throw.
        ErrorContext ctx;
        (void) stopMonitoring(&ctx);
    }

    Status LongTransactionMonitor::initialize(ErrorContext *ctx)
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        // Read configuration
        readConfiguration();

        LOG_INFO(TRANSACTION,
                 "LongTransactionMonitor initialized: warn=%us, critical=%us, warn_xid_lag=%lu, critical_xid_lag=%lu, check=%us, notices=%d, policy=%d",
                 warning_threshold_seconds_.load(), critical_threshold_seconds_.load(),
                 static_cast<unsigned long>(warning_xid_lag_threshold_.load()),
                 static_cast<unsigned long>(critical_xid_lag_threshold_.load()),
                 check_interval_seconds_.load(),
                 client_notice_enabled_.load(std::memory_order_acquire) ? 1 : 0,
                 static_cast<int>(policy_));

        return Status::OK;
    }

    void LongTransactionMonitor::readConfiguration()
    {
        Config &cfg = Config::getInstance();

        // Read warning threshold (default: 600 seconds = 10 minutes)
        warning_threshold_seconds_ = cfg.getUInt("long_transactions", "warning_threshold", 600);

        // Read critical threshold (default: 3600 seconds = 1 hour)
        critical_threshold_seconds_ = cfg.getUInt("long_transactions", "critical_threshold", 3600);

        // Read check interval (default: 60 seconds)
        check_interval_seconds_ = cfg.getUInt("long_transactions", "check_interval", 60);

        // Optional OAT/OST lag thresholds. These keep Firebird-style long
        // snapshot pressure explicit in code instead of treating age alone as
        // the only governance input.
        warning_xid_lag_threshold_ =
            static_cast<uint64_t>(cfg.getUInt("long_transactions", "warning_xid_lag_threshold", 0));
        critical_xid_lag_threshold_ =
            static_cast<uint64_t>(cfg.getUInt("long_transactions", "critical_xid_lag_threshold", 0));

        // Validate thresholds
        if (warning_threshold_seconds_ < 1)
        {
            LOG_WARNING(TRANSACTION, "Warning threshold %u too low, using 1 second",
                        warning_threshold_seconds_.load());
            warning_threshold_seconds_ = 1;
        }

        if (critical_threshold_seconds_ < warning_threshold_seconds_)
        {
            LOG_WARNING(TRANSACTION,
                        "Critical threshold %u < warning threshold %u, using warning+60",
                        critical_threshold_seconds_.load(), warning_threshold_seconds_.load());
            critical_threshold_seconds_ = warning_threshold_seconds_.load() + 60;
        }

        if (check_interval_seconds_ < 1)
        {
            LOG_WARNING(TRANSACTION, "Check interval %u too low, using 1 second",
                        check_interval_seconds_.load());
            check_interval_seconds_ = 1;
        }

        // Read policy
        std::string policy_str = cfg.getString("long_transactions", "policy", "LOG");
        if (policy_str == "LOG")
        {
            policy_ = LongTransactionPolicy::LOG;
        }
        else if (policy_str == "ROLLBACK_READONLY")
        {
            policy_ = LongTransactionPolicy::ROLLBACK_READONLY;
        }
        else if (policy_str == "ROLLBACK_ALL")
        {
            policy_ = LongTransactionPolicy::ROLLBACK_ALL;
        }
        else if (policy_str == "TERMINATE_CONNECTION")
        {
            policy_ = LongTransactionPolicy::TERMINATE_CONNECTION;
        }
        else
        {
            LOG_WARNING(TRANSACTION, "Invalid long transaction policy '%s', using LOG",
                        policy_str.c_str());
            policy_ = LongTransactionPolicy::LOG;
        }

        // Read enabled flag (default: true)
        bool enabled = cfg.getBool("long_transactions", "enabled", true);
        enabled_.store(enabled, std::memory_order_release);
        client_notice_enabled_.store(
            cfg.getBool("long_transactions", "client_notice_enabled", true),
            std::memory_order_release);
    }

    Status LongTransactionMonitor::startMonitoring(ErrorContext *ctx)
    {
        {
            std::lock_guard<std::mutex> lock(wake_mutex_);
            // Check if already running
            if (monitoring_)
            {
                LOG_WARNING(TRANSACTION, "Long transaction monitor already running");
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Long transaction monitor already running");
                return Status::IO_ERROR;
            }

            // Start monitoring thread
            shutdown_requested_ = false;
            monitoring_ = true;
        }

        monitor_thread_ = std::thread(&LongTransactionMonitor::monitoringLoop, this);

        uint64_t tid = std::hash<std::thread::id>{}(monitor_thread_.get_id());
        LOG_INFO(TRANSACTION, "Long transaction monitor thread started (tid=%lu)",
                 static_cast<unsigned long>(tid));
        return Status::OK;
    }

    Status LongTransactionMonitor::stopMonitoring(ErrorContext *ctx)
    {
        (void) ctx;

        {
            std::lock_guard<std::mutex> lock(wake_mutex_);
            if (!monitoring_)
            {
                // Normal for repeated stop calls from shutdown paths.
                return Status::OK;
            }

            // Signal shutdown while holding wake mutex.
            shutdown_requested_ = true;
            wake_cv_.notify_one();
        }

        // Wait for thread to finish
        if (monitor_thread_.joinable())
        {
            monitor_thread_.join();
        }

        {
            std::lock_guard<std::mutex> lock(wake_mutex_);
            monitoring_ = false;
            shutdown_requested_ = false;
        }

        LOG_INFO(TRANSACTION, "Long transaction monitor thread stopped");
        return Status::OK;
    }

    bool LongTransactionMonitor::isMonitoring() const
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        return monitoring_;
    }

    void LongTransactionMonitor::enable()
    {
        enabled_.store(true, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction monitoring enabled");
    }

    void LongTransactionMonitor::disable()
    {
        enabled_.store(false, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction monitoring disabled");
    }

    bool LongTransactionMonitor::isEnabled() const
    {
        return enabled_.load(std::memory_order_acquire);
    }

    void LongTransactionMonitor::setWarningThreshold(uint32_t seconds)
    {
        warning_threshold_seconds_.store(seconds, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction warning threshold set to %u seconds", seconds);
    }

    void LongTransactionMonitor::setCriticalThreshold(uint32_t seconds)
    {
        critical_threshold_seconds_.store(seconds, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction critical threshold set to %u seconds", seconds);
    }

    void LongTransactionMonitor::setCheckInterval(uint32_t seconds)
    {
        check_interval_seconds_.store(seconds, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction check interval set to %u seconds", seconds);
    }

    void LongTransactionMonitor::setWarningXidLagThreshold(uint64_t xid_lag)
    {
        warning_xid_lag_threshold_.store(xid_lag, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction warning XID lag threshold set to %lu",
                 static_cast<unsigned long>(xid_lag));
    }

    void LongTransactionMonitor::setCriticalXidLagThreshold(uint64_t xid_lag)
    {
        critical_xid_lag_threshold_.store(xid_lag, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction critical XID lag threshold set to %lu",
                 static_cast<unsigned long>(xid_lag));
    }

    void LongTransactionMonitor::setPolicy(LongTransactionPolicy policy)
    {
        policy_ = policy;
        const char *policy_name =
            policy == LongTransactionPolicy::LOG                 ? "LOG"
            : policy == LongTransactionPolicy::ROLLBACK_READONLY ? "ROLLBACK_READONLY"
            : policy == LongTransactionPolicy::ROLLBACK_ALL      ? "ROLLBACK_ALL"
                                                                 : "TERMINATE_CONNECTION";
        LOG_INFO(TRANSACTION, "Long transaction policy set to %s", policy_name);
    }

    void LongTransactionMonitor::setClientNoticeEnabled(bool enabled)
    {
        client_notice_enabled_.store(enabled, std::memory_order_release);
        LOG_INFO(TRANSACTION, "Long transaction client notice delivery %s",
                 enabled ? "enabled" : "disabled");
    }

    LongTransactionStatistics LongTransactionMonitor::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void LongTransactionMonitor::monitoringLoop()
    {
        LOG_INFO(TRANSACTION, "Long transaction monitoring loop started");

        while (true)
        {
            bool should_shutdown = false;
            {
                std::unique_lock<std::mutex> lock(wake_mutex_);
                should_shutdown = shutdown_requested_;
                if (!should_shutdown)
                {
                    uint32_t interval = check_interval_seconds_.load(std::memory_order_acquire);
                    wake_cv_.wait_for(lock, std::chrono::seconds(interval),
                                      [this] { return shutdown_requested_; });
                    should_shutdown = shutdown_requested_;
                }
            }

            if (should_shutdown)
            {
                break;
            }

            // Check if monitoring is enabled
            if (enabled_.load(std::memory_order_acquire))
            {
                // Check for long transactions
                ErrorContext err_ctx;
                checkLongTransactions(&err_ctx);
            }
        }

        LOG_INFO(TRANSACTION, "Long transaction monitoring loop stopped");
    }

    uint32_t LongTransactionMonitor::checkLongTransactions(ErrorContext *ctx)
    {
        // Get current time
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());

        // Get thresholds
        uint32_t warning_threshold = warning_threshold_seconds_.load(std::memory_order_acquire);
        uint32_t critical_threshold = critical_threshold_seconds_.load(std::memory_order_acquire);
        uint64_t warning_xid_lag =
            warning_xid_lag_threshold_.load(std::memory_order_acquire);
        uint64_t critical_xid_lag =
            critical_xid_lag_threshold_.load(std::memory_order_acquire);
        const bool notice_enabled = client_notice_enabled_.load(std::memory_order_acquire);

        uint64_t current_xid = 0;
        uint64_t oldest_active_xid = 0;
        uint64_t oldest_snapshot_xid = 0;
        if (db_ != nullptr && db_->transaction_manager() != nullptr)
        {
            current_xid = db_->transaction_manager()->getCurrentXid();
            oldest_active_xid = db_->transaction_manager()->getOldestActiveXid();
            oldest_snapshot_xid = db_->transaction_manager()->getOldestSnapshot();
        }

        // ProcArray may not be initialized yet (e.g., before first connection).
        if (!ProcArrayManager::getInstance()) {
            return 0;
        }

        // Get all active backends from ProcArray
        std::vector<ProcessControlBlock> active_backends;
        Status s = ProcArrayManager::getAllActiveBackends(&active_backends, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to get active backends for long transaction check");
            return 0;
        }

        uint32_t long_txn_count = 0;

        for (const auto &backend : active_backends)
        {
            // Skip if no active transaction (shouldn't happen in always-in-transaction model, but
            // check anyway)
            if (backend.xid == 0 || backend.xact_start_time == 0)
            {
                continue;
            }

            // Calculate transaction age
            std::chrono::microseconds backend_start_time(backend.xact_start_time);
            uint64_t age_microseconds = (now - backend_start_time).count();
            uint64_t age_seconds = age_microseconds / 1000000;
            uint64_t xid_lag = current_xid > backend.xid ? (current_xid - backend.xid) : 0;
            uint64_t snapshot_lag =
                (backend.backend_xmin != 0 && current_xid > backend.backend_xmin)
                    ? (current_xid - backend.backend_xmin)
                    : 0;

            const bool gc_pinned =
                backend.xid != 0 &&
                oldest_active_xid != 0 &&
                backend.xid <= oldest_active_xid;
            const bool snapshot_pinned =
                backend.backend_xmin != 0 &&
                oldest_snapshot_xid != 0 &&
                backend.backend_xmin <= oldest_snapshot_xid;

            LongTransactionReason reason_mask = LongTransactionReason::NONE;
            if (age_seconds >= warning_threshold)
            {
                reason_mask = reason_mask | LongTransactionReason::AGE_THRESHOLD;
            }
            if (warning_xid_lag != 0 && gc_pinned && xid_lag >= warning_xid_lag)
            {
                reason_mask = reason_mask | LongTransactionReason::GC_HORIZON_PIN;
            }
            if (warning_xid_lag != 0 && snapshot_pinned && snapshot_lag >= warning_xid_lag)
            {
                reason_mask = reason_mask | LongTransactionReason::SNAPSHOT_HORIZON_PIN;
            }

            if (reason_mask != LongTransactionReason::NONE)
            {
                long_txn_count++;
                const bool critical =
                    age_seconds >= critical_threshold ||
                    (critical_xid_lag != 0 && gc_pinned && xid_lag >= critical_xid_lag) ||
                    (critical_xid_lag != 0 && snapshot_pinned && snapshot_lag >= critical_xid_lag);

                const std::string governance_message = formatGovernanceMessage(
                    critical ? policy_ : LongTransactionPolicy::LOG,
                    backend.xid,
                    age_seconds,
                    xid_lag,
                    snapshot_lag,
                    reason_mask);

                if (!critical)
                {
                    LOG_WARNING(TRANSACTION, "%s", governance_message.c_str());

                    if (notice_enabled)
                    {
                        BackendGovernanceDirective directive;
                        directive.action = BackendGovernanceAction::NOTICE;
                        directive.reason_mask = toReasonMask(reason_mask);
                        directive.notice_pending = true;
                        directive.event_time = now.count();
                        directive.age_seconds = age_seconds;
                        directive.xid_lag = xid_lag;
                        directive.snapshot_lag = snapshot_lag;
                        directive.message = governance_message;
                        Status directive_status =
                            ProcArrayManager::requestBackendGovernanceDirective(
                                backend.proc_id, directive, ctx);
                        if (directive_status == Status::OK)
                        {
                            std::lock_guard<std::mutex> lock(stats_mutex_);
                            stats_.notices_emitted++;
                        }
                    }

                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.warnings_logged++;
                    if (hasLongTransactionReason(reason_mask, LongTransactionReason::GC_HORIZON_PIN))
                    {
                        stats_.gc_horizon_flags++;
                    }
                    if (hasLongTransactionReason(reason_mask,
                                                 LongTransactionReason::SNAPSHOT_HORIZON_PIN))
                    {
                        stats_.snapshot_horizon_flags++;
                    }
                    stats_.last_proc_id = backend.proc_id;
                    stats_.last_xid = backend.xid;
                    stats_.last_age_seconds = age_seconds;
                    stats_.last_xid_lag = xid_lag;
                    stats_.last_snapshot_lag = snapshot_lag;
                    stats_.last_reason_mask = toReasonMask(reason_mask);
                    stats_.last_policy_action = static_cast<uint8_t>(LongTransactionPolicy::LOG);
                }
                else
                {
                    checkAndActOnTransaction(backend.proc_id,
                                             backend.xid,
                                             age_seconds,
                                             xid_lag,
                                             snapshot_lag,
                                             reason_mask,
                                             backend.is_read_only,
                                             ctx);
                }
            }
        }

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.current_long_transactions = long_txn_count;
            stats_.last_check_time = now.count();
        }

        return long_txn_count;
    }

    void LongTransactionMonitor::checkAndActOnTransaction(uint32_t proc_id, uint64_t xid,
                                                          uint64_t age_seconds,
                                                          uint64_t xid_lag,
                                                          uint64_t snapshot_lag,
                                                          LongTransactionReason reason_mask,
                                                          bool is_read_only,
                                                          ErrorContext *ctx)
    {
        LongTransactionPolicy policy = policy_;
        const std::string message = formatGovernanceMessage(policy,
                                                            xid,
                                                            age_seconds,
                                                            xid_lag,
                                                            snapshot_lag,
                                                            reason_mask);
        BackendGovernanceDirective directive;
        directive.reason_mask = toReasonMask(reason_mask);
        directive.notice_pending = client_notice_enabled_.load(std::memory_order_acquire);
        directive.event_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
        directive.age_seconds = age_seconds;
        directive.xid_lag = xid_lag;
        directive.snapshot_lag = snapshot_lag;
        directive.message = message;

        LOG_ERROR(TRANSACTION,
                  "CRITICAL: %s, ProcID=%u, ReadOnly=%d",
                  message.c_str(), proc_id, is_read_only);

        auto record_stats = [&](LongTransactionPolicy action_policy, bool notice_enqueued) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (notice_enqueued)
            {
                stats_.notices_emitted++;
            }
            if (hasLongTransactionReason(reason_mask, LongTransactionReason::GC_HORIZON_PIN))
            {
                stats_.gc_horizon_flags++;
            }
            if (hasLongTransactionReason(reason_mask, LongTransactionReason::SNAPSHOT_HORIZON_PIN))
            {
                stats_.snapshot_horizon_flags++;
            }
            stats_.last_proc_id = proc_id;
            stats_.last_xid = xid;
            stats_.last_age_seconds = age_seconds;
            stats_.last_xid_lag = xid_lag;
            stats_.last_snapshot_lag = snapshot_lag;
            stats_.last_reason_mask = toReasonMask(reason_mask);
            stats_.last_policy_action = static_cast<uint8_t>(action_policy);
        };

        // Take action based on policy
        switch (policy)
        {
            case LongTransactionPolicy::LOG:
            {
                bool notice_enqueued = false;
                if (directive.notice_pending)
                {
                    directive.action = BackendGovernanceAction::NOTICE;
                    Status notice_status =
                        ProcArrayManager::requestBackendGovernanceDirective(proc_id, directive, ctx);
                    notice_enqueued = notice_status == Status::OK;
                }
                record_stats(LongTransactionPolicy::LOG, notice_enqueued);
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.warnings_logged++;
                }
                break;
            }

            case LongTransactionPolicy::ROLLBACK_READONLY:
                if (is_read_only)
                {
                    directive.action = BackendGovernanceAction::ROLLBACK_TRANSACTION;
                    directive.rollback_requested = true;
                    Status rollback_status =
                        ProcArrayManager::requestBackendGovernanceDirective(proc_id, directive, ctx);
                    if (rollback_status == Status::OK)
                    {
                        LOG_INFO(TRANSACTION,
                                 "Requested backend-owned rollback for long read-only transaction: XID=%lu, ProcID=%u",
                                 xid, proc_id);
                        record_stats(LongTransactionPolicy::ROLLBACK_READONLY,
                                     directive.notice_pending);
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.readonly_rolled_back++;
                    }
                    else
                    {
                        LOG_ERROR(TRANSACTION,
                                  "Failed to request backend rollback for long read-only transaction: XID=%lu, ProcID=%u, Status=%d",
                                  xid, proc_id, static_cast<int>(rollback_status));
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.warnings_logged++;
                    }
                }
                else
                {
                    LOG_WARNING(TRANSACTION,
                                "Read-write transaction exceeds threshold but policy is "
                                "ROLLBACK_READONLY: XID=%lu",
                                xid);
                    record_stats(LongTransactionPolicy::LOG, false);
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.warnings_logged++;
                }
                break;

            case LongTransactionPolicy::ROLLBACK_ALL:
                directive.action = BackendGovernanceAction::ROLLBACK_TRANSACTION;
                directive.rollback_requested = true;
                {
                    Status rollback_status =
                        ProcArrayManager::requestBackendGovernanceDirective(proc_id, directive, ctx);
                    if (rollback_status == Status::OK)
                    {
                        LOG_INFO(TRANSACTION,
                                 "Requested backend-owned rollback for long transaction: XID=%lu, ProcID=%u, ReadOnly=%d",
                                 xid, proc_id, is_read_only);
                        record_stats(LongTransactionPolicy::ROLLBACK_ALL,
                                     directive.notice_pending);
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        if (is_read_only)
                        {
                            stats_.readonly_rolled_back++;
                        }
                        else
                        {
                            stats_.readwrite_rolled_back++;
                        }
                    }
                    else
                    {
                        LOG_ERROR(TRANSACTION,
                                  "Failed to request backend rollback for long transaction: XID=%lu, ProcID=%u, Status=%d",
                                  xid, proc_id, static_cast<int>(rollback_status));
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.warnings_logged++;
                    }
                }
                break;

            case LongTransactionPolicy::TERMINATE_CONNECTION:
                directive.action = BackendGovernanceAction::TERMINATE_CONNECTION;
                directive.termination_requested = true;
                {
                    Status term_status =
                        ProcArrayManager::requestBackendGovernanceDirective(proc_id, directive, ctx);
                    if (term_status == Status::OK)
                    {
                        LOG_INFO(TRANSACTION,
                                 "Successfully requested termination for long transaction: XID=%lu, ProcID=%u",
                                 xid, proc_id);
                        record_stats(LongTransactionPolicy::TERMINATE_CONNECTION,
                                     directive.notice_pending);
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.connections_terminated++;
                    }
                    else
                    {
                        LOG_ERROR(TRANSACTION,
                                  "Failed to request backend termination: XID=%lu, ProcID=%u, Status=%d",
                                  xid, proc_id, static_cast<int>(term_status));
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.warnings_logged++;
                    }
                }
                break;
        }
    }

} // namespace scratchbird::core
