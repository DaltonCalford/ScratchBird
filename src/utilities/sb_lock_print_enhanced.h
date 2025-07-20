#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include <set>

// Forward declarations for ScratchBird engine components
namespace jrd {
    class Attachment;
    class Database;
    class Transaction;
    class Service;
}

class SBEngineIntegration;

namespace SBEnhanced {

// Lock types
enum class LockType {
    UNKNOWN = 0,
    SHARED_READ = 1,      // Shared read lock
    PROTECTED_READ = 2,   // Protected read lock
    SHARED_WRITE = 3,     // Shared write lock
    PROTECTED_WRITE = 4,  // Protected write lock
    EXCLUSIVE = 5,        // Exclusive lock
    PAGE_LOCK = 6,        // Page-level lock
    TABLE_LOCK = 7,       // Table-level lock
    RECORD_LOCK = 8,      // Record-level lock
    BLOB_LOCK = 9,        // Blob lock
    METADATA_LOCK = 10    // Metadata lock
};

// Lock states
enum class LockState {
    UNKNOWN = 0,
    WAITING = 1,          // Lock request is waiting
    GRANTED = 2,          // Lock is granted/active
    CONVERTING = 3,       // Lock is being converted
    DEADLOCKED = 4,       // Lock is part of deadlock
    TIMEOUT = 5,          // Lock request timed out
    CANCELLED = 6         // Lock request was cancelled
};

// Lock scope
enum class LockScope {
    UNKNOWN = 0,
    SYSTEM = 1,           // System-wide lock
    DATABASE = 2,         // Database-wide lock
    TABLE = 3,            // Table lock
    RECORD = 4,           // Record lock
    PAGE = 5,             // Page lock
    BLOB = 6,             // Blob lock
    METADATA = 7          // Metadata lock
};

// Deadlock detection modes
enum class DeadlockDetectionMode {
    NONE = 0,             // No deadlock detection
    BASIC = 1,            // Basic deadlock detection
    ADVANCED = 2,         // Advanced deadlock detection with cycle analysis
    COMPREHENSIVE = 3     // Comprehensive deadlock detection with prevention
};

// Lock analysis levels
enum class LockAnalysisLevel {
    BASIC = 0,            // Basic lock information
    STANDARD = 1,         // Standard lock analysis
    DETAILED = 2,         // Detailed lock analysis with dependencies
    COMPREHENSIVE = 3,    // Comprehensive analysis with performance impact
    FORENSIC = 4          // Forensic analysis with historical data
};

// Lock monitoring modes
enum class LockMonitoringMode {
    SNAPSHOT = 0,         // Single snapshot of locks
    CONTINUOUS = 1,       // Continuous monitoring
    TRIGGERED = 2,        // Triggered by events
    SCHEDULED = 3         // Scheduled monitoring
};

// Lock information structure
struct LockInfo {
    uint64_t lock_id = 0;
    uint64_t owner_transaction_id = 0;
    uint64_t owner_attachment_id = 0;
    uint64_t owner_process_id = 0;
    std::string owner_user;
    std::string database_name;
    std::string table_name;
    std::string object_name;
    
    LockType lock_type = LockType::UNKNOWN;
    LockState lock_state = LockState::UNKNOWN;
    LockScope lock_scope = LockScope::UNKNOWN;
    
    uint64_t object_id = 0;          // Object being locked (table ID, record ID, etc.)
    uint64_t page_number = 0;        // Page number for page locks
    uint64_t record_number = 0;      // Record number for record locks
    
    std::chrono::system_clock::time_point lock_time;          // When lock was acquired/requested
    std::chrono::milliseconds wait_time{0};                   // How long waiting for lock
    std::chrono::milliseconds hold_time{0};                   // How long lock has been held
    
    uint32_t lock_level = 0;         // Lock level/depth
    uint32_t conversion_count = 0;   // Number of lock conversions
    bool is_blocking = false;        // Is this lock blocking others
    bool is_blocked = false;         // Is this lock blocked by others
    
    std::vector<uint64_t> blocked_by;    // Lock IDs that are blocking this lock
    std::vector<uint64_t> blocking;      // Lock IDs that this lock is blocking
    
    std::string lock_resource;       // Resource being locked
    std::string lock_mode_string;    // Human-readable lock mode
    std::map<std::string, std::string> additional_info;
    
    std::string getLockTypeString() const;
    std::string getLockStateString() const;
    std::string getLockScopeString() const;
    bool isDeadlocked() const { return lock_state == LockState::DEADLOCKED; }
    bool isWaiting() const { return lock_state == LockState::WAITING; }
    bool isActive() const { return lock_state == LockState::GRANTED; }
};

// Deadlock information
struct DeadlockInfo {
    uint64_t deadlock_id = 0;
    std::chrono::system_clock::time_point detection_time;
    std::vector<uint64_t> involved_transactions;
    std::vector<uint64_t> involved_locks;
    std::vector<LockInfo> lock_chain;
    
    std::string victim_transaction;
    std::string resolution_action;
    std::chrono::milliseconds resolution_time{0};
    
    std::string deadlock_description;
    std::vector<std::string> cycle_description;
    std::map<std::string, std::string> deadlock_metadata;
    
    uint32_t cycle_length = 0;
    bool was_resolved = false;
    
    std::string generateDeadlockReport() const;
};

// Lock statistics
struct LockStatistics {
    std::chrono::system_clock::time_point collection_time;
    std::string database_name;
    
    // Overall lock counts
    uint64_t total_locks = 0;
    uint64_t active_locks = 0;
    uint64_t waiting_locks = 0;
    uint64_t deadlocked_locks = 0;
    
    // Lock type counts
    std::map<LockType, uint64_t> locks_by_type;
    std::map<LockState, uint64_t> locks_by_state;
    std::map<LockScope, uint64_t> locks_by_scope;
    
    // Performance metrics
    double average_wait_time_ms = 0.0;
    double average_hold_time_ms = 0.0;
    double maximum_wait_time_ms = 0.0;
    double maximum_hold_time_ms = 0.0;
    
    // Deadlock statistics
    uint64_t total_deadlocks = 0;
    uint64_t deadlocks_resolved = 0;
    double average_deadlock_resolution_time_ms = 0.0;
    
    // Transaction statistics
    uint64_t total_transactions = 0;
    uint64_t transactions_with_locks = 0;
    uint64_t transactions_waiting = 0;
    uint64_t transactions_deadlocked = 0;
    
    // Resource contention
    std::vector<std::string> most_contended_objects;
    std::vector<std::string> most_blocking_users;
    std::map<std::string, uint64_t> contention_hotspots;
    
    // Efficiency metrics
    double lock_efficiency = 0.0;        // Ratio of granted to requested locks
    double deadlock_rate = 0.0;          // Deadlocks per unit time
    double contention_level = 0.0;       // Overall contention level
    
    std::string generateStatisticsReport() const;
};

// Lock monitoring options
struct LockMonitoringOptions {
    LockMonitoringMode monitoring_mode = LockMonitoringMode::SNAPSHOT;
    LockAnalysisLevel analysis_level = LockAnalysisLevel::STANDARD;
    DeadlockDetectionMode deadlock_detection = DeadlockDetectionMode::BASIC;
    
    std::string database_path;
    std::string database_alias;
    std::vector<std::string> target_tables;
    std::vector<std::string> target_users;
    
    // Monitoring parameters
    uint32_t monitoring_interval_seconds = 30;
    uint32_t monitoring_duration_seconds = 0;  // 0 = continuous
    uint32_t deadlock_timeout_seconds = 30;
    
    // Filtering options
    bool include_system_locks = false;
    bool include_metadata_locks = true;
    bool include_waiting_locks = true;
    bool include_granted_locks = true;
    LockType minimum_lock_type = LockType::SHARED_READ;
    std::chrono::milliseconds minimum_wait_time{0};
    
    // Analysis options
    bool analyze_lock_dependencies = true;
    bool analyze_contention_patterns = true;
    bool analyze_performance_impact = false;
    bool track_historical_data = false;
    uint32_t historical_data_retention_hours = 24;
    
    // Output options
    bool generate_reports = false;
    std::string report_output_path;
    std::string report_format = "TEXT";
    bool real_time_alerts = false;
    
    std::function<void(const std::vector<LockInfo>&)> lock_callback;
    std::function<void(const DeadlockInfo&)> deadlock_callback;
};

// Lock analysis options
struct LockAnalysisOptions {
    std::string database_path;
    LockAnalysisLevel analysis_level = LockAnalysisLevel::DETAILED;
    
    // Analysis scope
    bool analyze_current_locks = true;
    bool analyze_historical_locks = false;
    bool analyze_deadlock_patterns = true;
    bool analyze_contention_hotspots = true;
    bool analyze_performance_impact = false;
    
    // Time range for historical analysis
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Filtering
    std::vector<std::string> target_tables;
    std::vector<std::string> target_users;
    std::vector<LockType> target_lock_types;
    
    // Analysis parameters
    uint32_t top_contention_count = 10;
    uint32_t deadlock_cycle_depth = 5;
    double contention_threshold = 0.1;  // 10% contention threshold
    
    // Output options
    bool generate_detailed_report = true;
    bool generate_recommendations = true;
    std::string analysis_output_path;
    std::string analysis_output_format = "TEXT";
};

// Lock monitoring progress
struct LockMonitoringProgress {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point current_time;
    
    uint64_t snapshots_collected = 0;
    uint64_t locks_analyzed = 0;
    uint64_t deadlocks_detected = 0;
    uint64_t conflicts_resolved = 0;
    
    std::string current_database;
    std::string current_operation;
    bool monitoring_active = false;
    
    std::chrono::seconds getMonitoringDuration() const;
    double getLocksPerSecond() const;
    double getDeadlockRate() const;
};

// Lock analysis result
struct LockAnalysisResult {
    std::chrono::system_clock::time_point analysis_time;
    std::string database_name;
    bool analysis_successful = false;
    
    // Current state analysis
    LockStatistics current_statistics;
    std::vector<LockInfo> current_locks;
    std::vector<DeadlockInfo> active_deadlocks;
    
    // Historical analysis
    std::vector<LockStatistics> historical_statistics;
    std::vector<DeadlockInfo> historical_deadlocks;
    
    // Contention analysis
    std::vector<std::string> contention_hotspots;
    std::vector<std::string> frequently_blocked_objects;
    std::vector<std::string> problematic_users;
    std::map<std::string, double> table_contention_scores;
    
    // Performance analysis
    std::vector<std::string> performance_bottlenecks;
    std::vector<std::string> optimization_opportunities;
    double overall_lock_efficiency = 0.0;
    double system_contention_level = 0.0;
    
    // Deadlock analysis
    std::vector<std::string> deadlock_patterns;
    std::vector<std::string> deadlock_prevention_recommendations;
    uint32_t deadlock_frequency = 0;
    
    // Recommendations
    std::vector<std::string> performance_recommendations;
    std::vector<std::string> configuration_recommendations;
    std::vector<std::string> application_recommendations;
    
    // Issues and warnings
    std::vector<std::string> critical_issues;
    std::vector<std::string> warnings;
    std::vector<std::string> analysis_errors;
    
    std::string analysis_report_path;
    
    std::string generateAnalysisReport() const;
    std::string generateRecommendationsReport() const;
};

// Lock monitoring result
struct LockMonitoringResult {
    LockMonitoringOptions monitoring_options;
    LockMonitoringProgress monitoring_progress;
    
    bool monitoring_successful = false;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Collected data
    std::vector<LockStatistics> collected_statistics;
    std::vector<DeadlockInfo> detected_deadlocks;
    std::vector<std::string> monitoring_events;
    
    // Summary results
    uint64_t total_snapshots = 0;
    uint64_t total_locks_observed = 0;
    uint64_t total_deadlocks_detected = 0;
    uint64_t peak_concurrent_locks = 0;
    double average_lock_contention = 0.0;
    
    // Issues detected
    std::vector<std::string> performance_issues;
    std::vector<std::string> deadlock_issues;
    std::vector<std::string> contention_issues;
    
    std::vector<std::string> monitoring_errors;
    std::vector<std::string> monitoring_warnings;
    
    std::string monitoring_report_path;
    
    std::chrono::milliseconds getMonitoringDuration() const;
    std::string generateMonitoringReport() const;
};

} // namespace SBEnhanced

// Main enhanced Lock Print utility class
class LockPrintEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> lock_service;
    std::atomic<bool> monitoring_active{false};
    std::atomic<bool> shutdown_requested{false};
    
    // Monitoring thread management
    std::unique_ptr<std::thread> monitoring_thread;
    std::unique_ptr<std::thread> deadlock_detection_thread;
    std::mutex lock_data_mutex;
    std::mutex statistics_mutex;
    
    // Data storage
    std::vector<SBEnhanced::LockInfo> current_locks;
    std::vector<SBEnhanced::DeadlockInfo> detected_deadlocks;
    std::vector<SBEnhanced::LockStatistics> historical_statistics;
    
    // Internal state
    SBEnhanced::LockMonitoringProgress current_progress;
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    LockPrintEnhanced();
    ~LockPrintEnhanced();
    
    // === ORIGINAL LOCK_PRINT FUNCTIONALITY (100% Compatible) ===
    
    // Basic lock information display
    bool printLocks(const std::string& database_path = "");
    bool printLocksSummary(const std::string& database_path = "");
    bool printDeadlocks(const std::string& database_path = "");
    
    // === ENHANCED FUNCTIONALITY ===
    
    // Advanced lock monitoring
    bool startLockMonitoring(const SBEnhanced::LockMonitoringOptions& options,
                            SBEnhanced::LockMonitoringResult& result);
    
    bool stopLockMonitoring(SBEnhanced::LockMonitoringResult& result);
    
    bool performLockAnalysis(const SBEnhanced::LockAnalysisOptions& options,
                            SBEnhanced::LockAnalysisResult& result);
    
    // Lock information queries
    bool getCurrentLocks(const std::string& database_path,
                        std::vector<SBEnhanced::LockInfo>& locks);
    
    bool getWaitingLocks(const std::string& database_path,
                        std::vector<SBEnhanced::LockInfo>& waiting_locks);
    
    bool getBlockedTransactions(const std::string& database_path,
                               std::vector<uint64_t>& blocked_transactions);
    
    bool getLocksByTable(const std::string& database_path,
                        const std::string& table_name,
                        std::vector<SBEnhanced::LockInfo>& locks);
    
    bool getLocksByUser(const std::string& database_path,
                       const std::string& username,
                       std::vector<SBEnhanced::LockInfo>& locks);
    
    // Deadlock detection and analysis
    bool detectDeadlocks(const std::string& database_path,
                        std::vector<SBEnhanced::DeadlockInfo>& deadlocks);
    
    bool analyzeDeadlockCycles(const std::string& database_path,
                              std::vector<SBEnhanced::DeadlockInfo>& deadlock_cycles);
    
    bool resolveDeadlock(const std::string& database_path,
                        uint64_t deadlock_id,
                        const std::string& resolution_strategy = "auto");
    
    // Lock statistics and analysis
    bool collectLockStatistics(const std::string& database_path,
                              SBEnhanced::LockStatistics& statistics);
    
    bool analyzeLockContention(const std::string& database_path,
                              std::vector<std::string>& contention_hotspots);
    
    bool analyzePerformanceImpact(const std::string& database_path,
                                 std::vector<std::string>& performance_issues);
    
    bool generateLockEfficiencyReport(const std::string& database_path,
                                     const std::string& report_format = "TEXT",
                                     const std::string& output_path = "");
    
    // Advanced monitoring features
    bool enableRealTimeMonitoring(const std::string& database_path,
                                 uint32_t monitoring_interval_seconds = 30);
    
    bool enableDeadlockPrevention(const std::string& database_path,
                                 SBEnhanced::DeadlockDetectionMode mode);
    
    bool setLockTimeout(const std::string& database_path,
                       uint32_t timeout_seconds);
    
    // Historical analysis
    bool enableHistoricalTracking(const std::string& database_path,
                                 uint32_t retention_hours = 24);
    
    bool analyzeHistoricalTrends(const std::string& database_path,
                                const std::chrono::hours& time_window,
                                std::vector<std::string>& trend_analysis);
    
    bool compareTimeRanges(const std::string& database_path,
                          const std::chrono::system_clock::time_point& start1,
                          const std::chrono::system_clock::time_point& end1,
                          const std::chrono::system_clock::time_point& start2,
                          const std::chrono::system_clock::time_point& end2,
                          std::vector<std::string>& comparison_results);
    
    // Optimization recommendations
    bool generateOptimizationRecommendations(const std::string& database_path,
                                             std::vector<std::string>& recommendations);
    
    bool analyzeIndexEfficiency(const std::string& database_path,
                               std::vector<std::string>& index_recommendations);
    
    bool suggestQueryOptimizations(const std::string& database_path,
                                  std::vector<std::string>& query_optimizations);
    
    // Export and reporting
    bool exportLockData(const std::string& database_path,
                       const std::string& export_format,
                       const std::string& output_path);
    
    bool generateComprehensiveReport(const std::string& database_path,
                                    const std::string& report_format = "TEXT",
                                    const std::string& output_path = "");
    
    bool createLockingSummaryDashboard(const std::string& database_path,
                                      const std::string& output_path);
    
    // Progress monitoring
    SBEnhanced::LockMonitoringProgress getCurrentProgress() const;
    bool isMonitoringActive() const;
    void requestShutdown();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Configuration and validation
    bool validateDatabaseAccess(const std::string& database_path);
    bool testLockServiceConnection(const std::string& database_path);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeLockService();
    
    // Core lock data collection
    bool collectCurrentLockData(const std::string& database_path,
                               std::vector<SBEnhanced::LockInfo>& locks);
    
    bool collectLockManagerData(const std::string& database_path,
                               std::vector<SBEnhanced::LockInfo>& locks);
    
    bool collectTransactionLockData(const std::string& database_path,
                                   std::vector<SBEnhanced::LockInfo>& locks);
    
    // Deadlock detection algorithms
    bool detectDeadlockCycles(const std::vector<SBEnhanced::LockInfo>& locks,
                             std::vector<SBEnhanced::DeadlockInfo>& deadlocks);
    
    bool analyzeWaitForGraph(const std::vector<SBEnhanced::LockInfo>& locks,
                            std::vector<std::vector<uint64_t>>& cycles);
    
    bool buildDependencyGraph(const std::vector<SBEnhanced::LockInfo>& locks,
                             std::map<uint64_t, std::vector<uint64_t>>& graph);
    
    // Lock analysis algorithms
    bool calculateLockStatistics(const std::vector<SBEnhanced::LockInfo>& locks,
                                SBEnhanced::LockStatistics& statistics);
    
    bool analyzeContentionPatterns(const std::vector<SBEnhanced::LockInfo>& locks,
                                  std::vector<std::string>& patterns);
    
    bool identifyPerformanceBottlenecks(const std::vector<SBEnhanced::LockInfo>& locks,
                                       std::vector<std::string>& bottlenecks);
    
    // Monitoring loop management
    void monitoringMainLoop(const SBEnhanced::LockMonitoringOptions& options);
    void deadlockDetectionLoop(const std::string& database_path);
    
    // Data processing helpers
    bool processLockSnapshot(const std::string& database_path,
                            const SBEnhanced::LockMonitoringOptions& options);
    
    bool updateStatistics(const std::vector<SBEnhanced::LockInfo>& locks);
    bool cleanupHistoricalData();
    
    // Report generation helpers
    std::string generateLockReport(const std::vector<SBEnhanced::LockInfo>& locks,
                                  const std::string& format) const;
    
    std::string generateDeadlockReport(const std::vector<SBEnhanced::DeadlockInfo>& deadlocks,
                                      const std::string& format) const;
    
    std::string generateStatisticsReport(const SBEnhanced::LockStatistics& statistics,
                                        const std::string& format) const;
    
    // Formatting helpers
    std::string formatLockInfo(const SBEnhanced::LockInfo& lock_info) const;
    std::string formatDeadlockInfo(const SBEnhanced::DeadlockInfo& deadlock_info) const;
    std::string formatDuration(const std::chrono::milliseconds& duration) const;
    
    // Utility helpers
    SBEnhanced::LockType parseLockType(const std::string& lock_type_str);
    SBEnhanced::LockState parseLockState(const std::string& lock_state_str);
    SBEnhanced::LockScope parseLockScope(const std::string& lock_scope_str);
    
    // Progress tracking helpers
    void updateProgress(const std::string& operation, const std::string& current_database);
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    void logInfo(const std::string& operation, const std::string& info);
    
    // Database connection helpers
    bool establishDatabaseConnection(const std::string& database_path,
                                    std::unique_ptr<jrd::Attachment>& attachment);
    
    void closeDatabaseConnection(std::unique_ptr<jrd::Attachment>& attachment);
    
    // Lock manager interaction
    bool queryLockManager(jrd::Attachment* attachment,
                         std::vector<SBEnhanced::LockInfo>& locks);
    
    bool queryTransactionManager(jrd::Attachment* attachment,
                                std::vector<SBEnhanced::LockInfo>& transaction_locks);
};

// Utility functions for enhanced Lock Print
namespace SBEnhanced {

// Quick lock operations
bool quickPrintLocks(const std::string& database_path);
bool quickDetectDeadlocks(const std::string& database_path);
bool quickAnalyzeLocks(const std::string& database_path);

// Lock information helpers
std::string formatLockDuration(const std::chrono::milliseconds& duration);
std::string formatLockResource(const LockInfo& lock_info);
double calculateContentionLevel(const std::vector<LockInfo>& locks);

// Deadlock utilities
bool hasDeadlockCycle(const std::vector<LockInfo>& locks);
std::vector<uint64_t> findDeadlockVictims(const DeadlockInfo& deadlock);

// Performance analysis utilities
double calculateLockEfficiency(const LockStatistics& statistics);
std::vector<std::string> identifyContentionHotspots(const std::vector<LockInfo>& locks);

// Export utilities
bool exportToCSV(const std::vector<LockInfo>& locks, const std::string& filename);
bool exportToJSON(const std::vector<LockInfo>& locks, const std::string& filename);
bool exportToXML(const std::vector<LockInfo>& locks, const std::string& filename);

// Compatibility helpers for command-line usage
int parseLockPrintCommandLine(int argc, char* argv[],
                             std::string& database_path,
                             LockMonitoringOptions& monitor_opts);

bool executeClassicLockPrintCommand(const std::string& command_line);

} // namespace SBEnhanced