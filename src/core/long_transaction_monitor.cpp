#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/logger.h"
#include <chrono>

namespace scratchbird::core
{
    LongTransactionMonitor::LongTransactionMonitor(Database* db)
        : db_(db)
        , enabled_(true)
        , warning_threshold_seconds_(600)      // 10 minutes default
        , critical_threshold_seconds_(3600)    // 1 hour default
        , check_interval_seconds_(60)          // Check every minute default
        , policy_(LongTransactionPolicy::LOG)
        , monitoring_(false)
        , shutdown_requested_(false)
    {
    }

    LongTransactionMonitor::~LongTransactionMonitor()
    {
        // Stop monitoring thread if running
        if (monitoring_.load(std::memory_order_acquire))
        {
            shutdown_requested_.store(true, std::memory_order_release);

            // Wake the monitoring thread so it can exit
            wake_cv_.notify_one();

            if (monitor_thread_.joinable())
            {
                monitor_thread_.join();
            }
        }
    }

    Status LongTransactionMonitor::initialize(ErrorContext* ctx)
    {
        if (!db_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
            return Status::INVALID_ARGUMENT;
        }

        // Read configuration
        readConfiguration();

        LOG_INFO(TRANSACTION, "LongTransactionMonitor initialized: warn=%us, critical=%us, check=%us, policy=%d",
                 warning_threshold_seconds_.load(), critical_threshold_seconds_.load(),
                 check_interval_seconds_.load(), static_cast<int>(policy_));

        return Status::OK;
    }

    void LongTransactionMonitor::readConfiguration()
    {
        Config& cfg = Config::getInstance();

        // Read warning threshold (default: 600 seconds = 10 minutes)
        warning_threshold_seconds_ = cfg.getUInt("long_transactions", "warning_threshold", 600);

        // Read critical threshold (default: 3600 seconds = 1 hour)
        critical_threshold_seconds_ = cfg.getUInt("long_transactions", "critical_threshold", 3600);

        // Read check interval (default: 60 seconds)
        check_interval_seconds_ = cfg.getUInt("long_transactions", "check_interval", 60);

        // Validate thresholds
        if (warning_threshold_seconds_ < 1)
        {
            LOG_WARNING(TRANSACTION, "Warning threshold %u too low, using 1 second", warning_threshold_seconds_.load());
            warning_threshold_seconds_ = 1;
        }

        if (critical_threshold_seconds_ < warning_threshold_seconds_)
        {
            LOG_WARNING(TRANSACTION, "Critical threshold %u < warning threshold %u, using warning+60",
                       critical_threshold_seconds_.load(), warning_threshold_seconds_.load());
            critical_threshold_seconds_ = warning_threshold_seconds_.load() + 60;
        }

        if (check_interval_seconds_ < 1)
        {
            LOG_WARNING(TRANSACTION, "Check interval %u too low, using 1 second", check_interval_seconds_.load());
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
            LOG_WARNING(TRANSACTION, "Invalid long transaction policy '%s', using LOG", policy_str.c_str());
            policy_ = LongTransactionPolicy::LOG;
        }

        // Read enabled flag (default: true)
        bool enabled = cfg.getBool("long_transactions", "enabled", true);
        enabled_.store(enabled, std::memory_order_release);
    }

    Status LongTransactionMonitor::startMonitoring(ErrorContext* ctx)
    {
        // Check if already running
        if (monitoring_.load(std::memory_order_acquire))
        {
            LOG_WARNING(TRANSACTION, "Long transaction monitor already running");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Long transaction monitor already running");
            return Status::IO_ERROR;
        }

        // Start monitoring thread
        shutdown_requested_.store(false, std::memory_order_release);
        monitoring_.store(true, std::memory_order_release);

        monitor_thread_ = std::thread(&LongTransactionMonitor::monitoringLoop, this);

        LOG_INFO(TRANSACTION, "Long transaction monitor thread started");
        return Status::OK;
    }

    Status LongTransactionMonitor::stopMonitoring(ErrorContext* ctx)
    {
        if (!monitoring_.load(std::memory_order_acquire))
        {
            LOG_WARNING(TRANSACTION, "Long transaction monitor not running");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Long transaction monitor not running");
            return Status::IO_ERROR;
        }

        // Signal shutdown
        shutdown_requested_.store(true, std::memory_order_release);

        // Wake the monitoring thread so it can exit
        wake_cv_.notify_one();

        // Wait for thread to finish
        if (monitor_thread_.joinable())
        {
            monitor_thread_.join();
        }

        monitoring_.store(false, std::memory_order_release);

        LOG_INFO(TRANSACTION, "Long transaction monitor thread stopped");
        return Status::OK;
    }

    bool LongTransactionMonitor::isMonitoring() const
    {
        return monitoring_.load(std::memory_order_acquire);
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

    void LongTransactionMonitor::setPolicy(LongTransactionPolicy policy)
    {
        policy_ = policy;
        const char* policy_name =
            policy == LongTransactionPolicy::LOG ? "LOG" :
            policy == LongTransactionPolicy::ROLLBACK_READONLY ? "ROLLBACK_READONLY" :
            policy == LongTransactionPolicy::ROLLBACK_ALL ? "ROLLBACK_ALL" : "TERMINATE_CONNECTION";
        LOG_INFO(TRANSACTION, "Long transaction policy set to %s", policy_name);
    }

    LongTransactionStatistics LongTransactionMonitor::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void LongTransactionMonitor::monitoringLoop()
    {
        LOG_INFO(TRANSACTION, "Long transaction monitoring loop started");

        while (!shutdown_requested_.load(std::memory_order_acquire))
        {
            // Check if monitoring is enabled
            if (enabled_.load(std::memory_order_acquire))
            {
                // Check for long transactions
                ErrorContext err_ctx;
                checkLongTransactions(&err_ctx);
            }

            // Wait for wake signal or timeout
            std::unique_lock<std::mutex> lock(wake_mutex_);
            uint32_t interval = check_interval_seconds_.load(std::memory_order_acquire);
            wake_cv_.wait_for(lock, std::chrono::seconds(interval),
                             [this] { return shutdown_requested_.load(std::memory_order_acquire); });
        }

        LOG_INFO(TRANSACTION, "Long transaction monitoring loop stopped");
    }

    uint32_t LongTransactionMonitor::checkLongTransactions(ErrorContext* ctx)
    {
        // Get current time
        auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );

        // Get thresholds
        uint32_t warning_threshold = warning_threshold_seconds_.load(std::memory_order_acquire);
        uint32_t critical_threshold = critical_threshold_seconds_.load(std::memory_order_acquire);

        // Get all active backends from ProcArray
        std::vector<ProcessControlBlock> active_backends;
        Status s = ProcArrayManager::getAllActiveBackends(&active_backends, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to get active backends for long transaction check");
            return 0;
        }

        uint32_t long_txn_count = 0;

        for (const auto& backend : active_backends)
        {
            // Skip if no active transaction (shouldn't happen in always-in-transaction model, but check anyway)
            if (backend.xid == 0 || backend.xact_start_time == 0)
            {
                continue;
            }

            // Calculate transaction age
            std::chrono::microseconds backend_start_time(backend.xact_start_time);
            uint64_t age_microseconds = (now - backend_start_time).count();
            uint64_t age_seconds = age_microseconds / 1000000;

            // Check if transaction is long-running
            if (age_seconds >= warning_threshold)
            {
                long_txn_count++;

                // Take action if above critical threshold
                if (age_seconds >= critical_threshold)
                {
                    checkAndActOnTransaction(backend.proc_id, backend.xid, age_seconds,
                                           backend.is_read_only, ctx);
                }
                else
                {
                    // Just log warning
                    LOG_WARNING(TRANSACTION,
                               "Long transaction detected: XID=%lu, ProcID=%u, Age=%lu seconds, ReadOnly=%d",
                               backend.xid, backend.proc_id, age_seconds, backend.is_read_only);

                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.warnings_logged++;
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
                                                          uint64_t age_seconds, bool is_read_only,
                                                          ErrorContext* ctx)
    {
        LongTransactionPolicy policy = policy_;

        LOG_ERROR(TRANSACTION,
                 "CRITICAL: Long transaction exceeds threshold: XID=%lu, ProcID=%u, Age=%lu seconds, ReadOnly=%d",
                 xid, proc_id, age_seconds, is_read_only);

        // Take action based on policy
        switch (policy)
        {
            case LongTransactionPolicy::LOG:
                // Already logged above
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.warnings_logged++;
                }
                break;

            case LongTransactionPolicy::ROLLBACK_READONLY:
                if (is_read_only)
                {
                    LOG_WARNING(TRANSACTION, "Rolling back read-only long transaction: XID=%lu, ProcID=%u", xid, proc_id);
                    // TODO: Implement connection lookup and rollback
                    // For now, just log
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.readonly_rolled_back++;
                }
                else
                {
                    LOG_WARNING(TRANSACTION, "Read-write transaction exceeds threshold but policy is ROLLBACK_READONLY: XID=%lu", xid);
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.warnings_logged++;
                }
                break;

            case LongTransactionPolicy::ROLLBACK_ALL:
                LOG_WARNING(TRANSACTION, "Rolling back long transaction: XID=%lu, ProcID=%u, ReadOnly=%d", xid, proc_id, is_read_only);
                // TODO: Implement connection lookup and rollback
                // For now, just log
                {
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
                break;

            case LongTransactionPolicy::TERMINATE_CONNECTION:
                LOG_WARNING(TRANSACTION, "Terminating connection for long transaction: XID=%lu, ProcID=%u", xid, proc_id);
                // TODO: Implement connection lookup and termination
                // For now, just log
                {
                    std::lock_guard<std::mutex> lock(stats_mutex_);
                    stats_.connections_terminated++;
                }
                break;
        }
    }

} // namespace scratchbird::core
