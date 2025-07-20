#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <stack>
#include <unordered_set>

// Forward declarations
extern "C" {
    #include <ibase.h>
}

namespace SBEnhanced {
    
    // Transaction Isolation Levels
    enum class IsolationLevel {
        READ_UNCOMMITTED,
        READ_COMMITTED,
        REPEATABLE_READ,
        SERIALIZABLE,
        READ_COMMITTED_RECORD_VERSION,
        READ_COMMITTED_NO_RECORD_VERSION
    };
    
    // Transaction Access Modes
    enum class AccessMode {
        READ_WRITE,
        READ_ONLY
    };
    
    // Transaction Lock Resolution
    enum class LockResolution {
        WAIT,
        NO_WAIT,
        CANCEL
    };
    
    // Transaction Configuration
    struct TransactionConfig {
        IsolationLevel isolation_level = IsolationLevel::READ_COMMITTED;
        AccessMode access_mode = AccessMode::READ_WRITE;
        LockResolution lock_resolution = LockResolution::WAIT;
        std::chrono::seconds lock_timeout{30};
        bool auto_commit = false;
        bool read_only = false;
        bool ignore_limbo = false;
        bool no_auto_undo = false;
        std::vector<std::string> table_reservation;
        std::map<std::string, std::string> custom_parameters;
    };
    
    // Transaction State
    enum class TransactionState {
        INACTIVE,
        ACTIVE,
        PREPARING,
        PREPARED,
        COMMITTING,
        COMMITTED,
        ROLLING_BACK,
        ROLLED_BACK,
        LIMBO,
        FAILED
    };
    
    // Transaction Info
    struct TransactionInfo {
        std::string transaction_id;
        std::string name;
        isc_tr_handle tr_handle = 0;
        isc_db_handle db_handle = 0;
        
        // Configuration
        TransactionConfig config;
        
        // State
        TransactionState state = TransactionState::INACTIVE;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_activity_time;
        std::atomic<bool> is_active{false};
        std::atomic<bool> has_changes{false};
        std::atomic<bool> is_read_only{false};
        
        // Statistics
        std::atomic<uint64_t> query_count{0};
        std::atomic<uint64_t> update_count{0};
        std::atomic<uint64_t> insert_count{0};
        std::atomic<uint64_t> delete_count{0};
        std::atomic<uint64_t> select_count{0};
        std::atomic<uint64_t> bytes_read{0};
        std::atomic<uint64_t> bytes_written{0};
        std::atomic<uint64_t> records_affected{0};
        std::atomic<uint64_t> pages_read{0};
        std::atomic<uint64_t> pages_written{0};
        std::atomic<uint64_t> cache_hits{0};
        std::atomic<uint64_t> cache_misses{0};
        
        // Savepoints
        std::stack<std::string> savepoints;
        std::map<std::string, std::chrono::steady_clock::time_point> savepoint_times;
        
        // Locking
        std::unordered_set<std::string> locked_tables;
        std::unordered_set<std::string> locked_records;
        std::atomic<uint64_t> lock_count{0};
        std::atomic<uint64_t> lock_waits{0};
        std::atomic<uint64_t> lock_timeouts{0};
        std::atomic<uint64_t> deadlocks{0};
        
        // Error handling
        std::vector<std::string> warnings;
        std::string last_error;
        std::atomic<uint64_t> error_count{0};
        
        // Thread info
        std::thread::id owner_thread;
        std::string owner_connection;
        
        // Callback functions
        std::function<void(const TransactionInfo&)> on_commit;
        std::function<void(const TransactionInfo&)> on_rollback;
        std::function<void(const TransactionInfo&, const std::string&)> on_savepoint;
        std::function<void(const TransactionInfo&, const std::string&)> on_error;
    };
    
    // Transaction Statistics
    struct TransactionStats {
        std::atomic<uint64_t> total_transactions{0};
        std::atomic<uint64_t> active_transactions{0};
        std::atomic<uint64_t> committed_transactions{0};
        std::atomic<uint64_t> rolled_back_transactions{0};
        std::atomic<uint64_t> prepared_transactions{0};
        std::atomic<uint64_t> limbo_transactions{0};
        std::atomic<uint64_t> failed_transactions{0};
        std::atomic<uint64_t> read_only_transactions{0};
        std::atomic<uint64_t> read_write_transactions{0};
        std::atomic<uint64_t> auto_commit_transactions{0};
        std::atomic<uint64_t> explicit_transactions{0};
        std::atomic<uint64_t> savepoint_count{0};
        std::atomic<uint64_t> deadlock_count{0};
        std::atomic<uint64_t> timeout_count{0};
        std::atomic<std::chrono::microseconds> average_transaction_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> average_commit_time{std::chrono::microseconds::zero()};
        std::atomic<std::chrono::microseconds> average_rollback_time{std::chrono::microseconds::zero()};
        std::atomic<uint64_t> longest_transaction_time{0};
        std::atomic<uint64_t> shortest_transaction_time{UINT64_MAX};
    };
    
    // Distributed Transaction Support
    struct DistributedTransactionInfo {
        std::string global_transaction_id;
        std::vector<std::string> participating_databases;
        std::vector<std::string> participating_transactions;
        TransactionState state = TransactionState::INACTIVE;
        std::chrono::steady_clock::time_point start_time;
        std::map<std::string, TransactionState> database_states;
        std::function<void(const DistributedTransactionInfo&)> on_prepare;
        std::function<void(const DistributedTransactionInfo&)> on_commit;
        std::function<void(const DistributedTransactionInfo&)> on_rollback;
    };
    
} // namespace SBEnhanced

// Transaction Manager Class
class TransactionManager {
private:
    // Transaction registry
    std::map<std::string, std::unique_ptr<SBEnhanced::TransactionInfo>> transactions;
    std::mutex transactions_mutex;
    
    // Current transaction tracking
    thread_local std::string current_transaction_id;
    thread_local std::stack<std::string> transaction_stack;
    
    // Configuration
    SBEnhanced::TransactionConfig default_config;
    
    // Statistics
    mutable SBEnhanced::TransactionStats stats;
    
    // Distributed transactions
    std::map<std::string, std::unique_ptr<SBEnhanced::DistributedTransactionInfo>> distributed_transactions;
    std::mutex distributed_mutex;
    
    // Background monitoring
    std::atomic<bool> monitoring_enabled{false};
    std::thread monitoring_thread;
    std::condition_variable monitoring_condition;
    std::mutex monitoring_mutex;
    
    // Error handling
    mutable std::vector<std::string> error_log;
    mutable std::mutex error_mutex;
    std::atomic<uint64_t> error_count{0};
    
    // Performance monitoring
    std::atomic<bool> performance_monitoring_enabled{false};
    std::map<std::string, std::atomic<uint64_t>> performance_counters;
    mutable std::mutex performance_mutex;
    
public:
    TransactionManager();
    ~TransactionManager();
    
    // Initialization
    bool initialize(const SBEnhanced::TransactionConfig& default_config);
    bool shutdown();
    
    // Transaction lifecycle
    std::string beginTransaction(isc_db_handle db_handle, const std::string& name = "",
                                const SBEnhanced::TransactionConfig& config = {});
    bool commitTransaction(const std::string& transaction_id = "");
    bool rollbackTransaction(const std::string& transaction_id = "");
    bool prepareTransaction(const std::string& transaction_id);
    bool commitPreparedTransaction(const std::string& transaction_id);
    bool rollbackPreparedTransaction(const std::string& transaction_id);
    
    // Savepoint management
    bool createSavepoint(const std::string& savepoint_name, const std::string& transaction_id = "");
    bool rollbackToSavepoint(const std::string& savepoint_name, const std::string& transaction_id = "");
    bool releaseSavepoint(const std::string& savepoint_name, const std::string& transaction_id = "");
    std::vector<std::string> getSavepoints(const std::string& transaction_id = "") const;
    
    // Transaction queries
    bool isTransactionActive(const std::string& transaction_id) const;
    SBEnhanced::TransactionState getTransactionState(const std::string& transaction_id) const;
    std::shared_ptr<SBEnhanced::TransactionInfo> getTransactionInfo(const std::string& transaction_id) const;
    std::vector<std::string> getActiveTransactions() const;
    std::vector<std::string> getAllTransactions() const;
    
    // Current transaction management
    std::string getCurrentTransaction() const;
    bool setCurrentTransaction(const std::string& transaction_id);
    std::string pushTransaction(const std::string& transaction_id);
    std::string popTransaction();
    
    // Transaction handle access
    isc_tr_handle getTransactionHandle(const std::string& transaction_id) const;
    bool setTransactionHandle(const std::string& transaction_id, isc_tr_handle handle);
    
    // Transaction monitoring
    bool enableTransactionMonitoring(bool enable = true);
    bool killTransaction(const std::string& transaction_id);
    bool killAllTransactions();
    std::vector<std::string> getLongRunningTransactions(std::chrono::seconds threshold) const;
    std::vector<std::string> getIdleTransactions(std::chrono::seconds threshold) const;
    
    // Lock management
    bool getLockInfo(const std::string& transaction_id, std::map<std::string, std::string>& lock_info) const;
    bool detectDeadlocks(std::vector<std::string>& deadlocked_transactions) const;
    bool resolveLockConflict(const std::string& transaction_id, const std::string& resolution);
    
    // Distributed transaction support
    std::string beginDistributedTransaction(const std::vector<std::string>& database_connections,
                                           const std::string& global_id = "");
    bool prepareDistributedTransaction(const std::string& global_id);
    bool commitDistributedTransaction(const std::string& global_id);
    bool rollbackDistributedTransaction(const std::string& global_id);
    bool recoverDistributedTransactions(std::vector<std::string>& recovered_transactions);
    
    // Statistics
    SBEnhanced::TransactionStats getStatistics() const;
    bool resetStatistics();
    
    // Configuration
    SBEnhanced::TransactionConfig getDefaultConfiguration() const;
    bool setDefaultConfiguration(const SBEnhanced::TransactionConfig& config);
    
    // Error handling
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    uint64_t getErrorCount() const;
    std::string getLastError() const;
    
    // Performance monitoring
    bool enablePerformanceMonitoring(bool enable = true);
    std::map<std::string, uint64_t> getPerformanceCounters() const;
    bool resetPerformanceCounters();
    
    // Utility methods
    bool isInitialized() const;
    std::string generateTransactionId() const;
    std::string generateGlobalTransactionId() const;
    
private:
    // Internal transaction management
    bool createTransactionInternal(const std::string& transaction_id, isc_db_handle db_handle,
                                  const std::string& name, const SBEnhanced::TransactionConfig& config);
    bool destroyTransactionInternal(const std::string& transaction_id);
    bool startTransactionInternal(SBEnhanced::TransactionInfo* transaction);
    bool commitTransactionInternal(SBEnhanced::TransactionInfo* transaction);
    bool rollbackTransactionInternal(SBEnhanced::TransactionInfo* transaction);
    
    // Transaction Parameter Block (TPB) building
    std::vector<char> buildTPB(const SBEnhanced::TransactionConfig& config);
    
    // Monitoring
    void monitoringLoop();
    bool checkTransactionHealth();
    bool cleanupInactiveTransactions();
    
    // Error handling helpers
    void logError(const std::string& operation, const std::string& error) const;
    void logPerformance(const std::string& operation, std::chrono::microseconds duration) const;
    std::string formatISCError(const ISC_STATUS* status_vector) const;
    
    // Statistics helpers
    void updateStats(const std::string& operation, std::chrono::microseconds duration) const;
    void incrementCounter(const std::string& counter_name) const;
    
    // Utility helpers
    bool isTransactionExpired(const SBEnhanced::TransactionInfo* transaction) const;
    bool isTransactionIdle(const SBEnhanced::TransactionInfo* transaction) const;
    std::string formatTransactionInfo(const SBEnhanced::TransactionInfo* transaction) const;
};