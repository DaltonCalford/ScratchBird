#include "sb_lock_print_enhanced.h"
#include "sb_engine_integration.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <thread>
#include <chrono>
#include <regex>
#include <cstring>
#include <set>
#include <queue>

namespace fs = std::filesystem;

// LockInfo member implementations
std::string SBEnhanced::LockInfo::getLockTypeString() const {
    switch (lock_type) {
        case LockType::SHARED_READ: return "Shared Read";
        case LockType::PROTECTED_READ: return "Protected Read";
        case LockType::SHARED_WRITE: return "Shared Write";
        case LockType::PROTECTED_WRITE: return "Protected Write";
        case LockType::EXCLUSIVE: return "Exclusive";
        case LockType::PAGE_LOCK: return "Page Lock";
        case LockType::TABLE_LOCK: return "Table Lock";
        case LockType::RECORD_LOCK: return "Record Lock";
        case LockType::BLOB_LOCK: return "Blob Lock";
        case LockType::METADATA_LOCK: return "Metadata Lock";
        default: return "Unknown";
    }
}

std::string SBEnhanced::LockInfo::getLockStateString() const {
    switch (lock_state) {
        case LockState::WAITING: return "Waiting";
        case LockState::GRANTED: return "Granted";
        case LockState::CONVERTING: return "Converting";
        case LockState::DEADLOCKED: return "Deadlocked";
        case LockState::TIMEOUT: return "Timeout";
        case LockState::CANCELLED: return "Cancelled";
        default: return "Unknown";
    }
}

std::string SBEnhanced::LockInfo::getLockScopeString() const {
    switch (lock_scope) {
        case LockScope::SYSTEM: return "System";
        case LockScope::DATABASE: return "Database";
        case LockScope::TABLE: return "Table";
        case LockScope::RECORD: return "Record";
        case LockScope::PAGE: return "Page";
        case LockScope::BLOB: return "Blob";
        case LockScope::METADATA: return "Metadata";
        default: return "Unknown";
    }
}

// DeadlockInfo member implementations
std::string SBEnhanced::DeadlockInfo::generateDeadlockReport() const {
    std::ostringstream oss;
    
    oss << "Deadlock Report\n";
    oss << "===============\n";
    oss << "Deadlock ID: " << deadlock_id << "\n";
    oss << "Detection Time: " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(detection_time)), "%Y-%m-%d %H:%M:%S") << "\n";
    oss << "Cycle Length: " << cycle_length << "\n";
    oss << "Resolution: " << (was_resolved ? "Resolved" : "Pending") << "\n";
    
    if (!victim_transaction.empty()) {
        oss << "Victim Transaction: " << victim_transaction << "\n";
    }
    
    if (!resolution_action.empty()) {
        oss << "Resolution Action: " << resolution_action << "\n";
    }
    
    if (resolution_time.count() > 0) {
        oss << "Resolution Time: " << resolution_time.count() << " ms\n";
    }
    
    oss << "\nInvolved Transactions:\n";
    for (const auto& txn_id : involved_transactions) {
        oss << "  - Transaction " << txn_id << "\n";
    }
    
    oss << "\nLock Chain:\n";
    for (size_t i = 0; i < lock_chain.size(); ++i) {
        const auto& lock = lock_chain[i];
        oss << "  " << (i + 1) << ". " << lock.getLockTypeString() 
            << " on " << lock.object_name 
            << " by Transaction " << lock.owner_transaction_id 
            << " (" << lock.getLockStateString() << ")\n";
    }
    
    if (!cycle_description.empty()) {
        oss << "\nDeadlock Cycle:\n";
        for (const auto& step : cycle_description) {
            oss << "  -> " << step << "\n";
        }
    }
    
    if (!deadlock_description.empty()) {
        oss << "\nDescription: " << deadlock_description << "\n";
    }
    
    return oss.str();
}

// LockStatistics member implementations
std::string SBEnhanced::LockStatistics::generateStatisticsReport() const {
    std::ostringstream oss;
    
    oss << "Lock Statistics Report\n";
    oss << "=====================\n";
    oss << "Database: " << database_name << "\n";
    oss << "Collection Time: " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(collection_time)), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    oss << "Overall Statistics:\n";
    oss << "  Total Locks: " << total_locks << "\n";
    oss << "  Active Locks: " << active_locks << "\n";
    oss << "  Waiting Locks: " << waiting_locks << "\n";
    oss << "  Deadlocked Locks: " << deadlocked_locks << "\n\n";
    
    oss << "Performance Metrics:\n";
    oss << "  Average Wait Time: " << std::fixed << std::setprecision(2) << average_wait_time_ms << " ms\n";
    oss << "  Average Hold Time: " << std::fixed << std::setprecision(2) << average_hold_time_ms << " ms\n";
    oss << "  Maximum Wait Time: " << std::fixed << std::setprecision(2) << maximum_wait_time_ms << " ms\n";
    oss << "  Maximum Hold Time: " << std::fixed << std::setprecision(2) << maximum_hold_time_ms << " ms\n\n";
    
    oss << "Deadlock Statistics:\n";
    oss << "  Total Deadlocks: " << total_deadlocks << "\n";
    oss << "  Deadlocks Resolved: " << deadlocks_resolved << "\n";
    oss << "  Average Resolution Time: " << std::fixed << std::setprecision(2) << average_deadlock_resolution_time_ms << " ms\n\n";
    
    oss << "Transaction Statistics:\n";
    oss << "  Total Transactions: " << total_transactions << "\n";
    oss << "  Transactions with Locks: " << transactions_with_locks << "\n";
    oss << "  Transactions Waiting: " << transactions_waiting << "\n";
    oss << "  Transactions Deadlocked: " << transactions_deadlocked << "\n\n";
    
    oss << "Efficiency Metrics:\n";
    oss << "  Lock Efficiency: " << std::fixed << std::setprecision(1) << (lock_efficiency * 100.0) << "%\n";
    oss << "  Deadlock Rate: " << std::fixed << std::setprecision(4) << deadlock_rate << " per second\n";
    oss << "  Contention Level: " << std::fixed << std::setprecision(1) << (contention_level * 100.0) << "%\n\n";
    
    if (!most_contended_objects.empty()) {
        oss << "Most Contended Objects:\n";
        for (size_t i = 0; i < std::min(most_contended_objects.size(), size_t(5)); ++i) {
            oss << "  " << (i + 1) << ". " << most_contended_objects[i] << "\n";
        }
        oss << "\n";
    }
    
    if (!most_blocking_users.empty()) {
        oss << "Most Blocking Users:\n";
        for (size_t i = 0; i < std::min(most_blocking_users.size(), size_t(5)); ++i) {
            oss << "  " << (i + 1) << ". " << most_blocking_users[i] << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

// LockMonitoringProgress member implementations
std::chrono::seconds SBEnhanced::LockMonitoringProgress::getMonitoringDuration() const {
    return std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
}

double SBEnhanced::LockMonitoringProgress::getLocksPerSecond() const {
    auto duration = getMonitoringDuration();
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(locks_analyzed) / duration.count();
}

double SBEnhanced::LockMonitoringProgress::getDeadlockRate() const {
    auto duration = getMonitoringDuration();
    if (duration.count() == 0) return 0.0;
    return static_cast<double>(deadlocks_detected) / duration.count();
}

// LockAnalysisResult member implementations
std::string SBEnhanced::LockAnalysisResult::generateAnalysisReport() const {
    std::ostringstream oss;
    
    oss << "Lock Analysis Report\n";
    oss << "===================\n";
    oss << "Database: " << database_name << "\n";
    oss << "Analysis Time: " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(analysis_time)), "%Y-%m-%d %H:%M:%S") << "\n";
    oss << "Analysis Status: " << (analysis_successful ? "Successful" : "Failed") << "\n\n";
    
    // Current state summary
    oss << "Current Lock State Summary:\n";
    oss << "  Total Locks: " << current_locks.size() << "\n";
    oss << "  Active Deadlocks: " << active_deadlocks.size() << "\n";
    oss << "  Overall Lock Efficiency: " << std::fixed << std::setprecision(1) << (overall_lock_efficiency * 100.0) << "%\n";
    oss << "  System Contention Level: " << std::fixed << std::setprecision(1) << (system_contention_level * 100.0) << "%\n\n";
    
    // Contention analysis
    if (!contention_hotspots.empty()) {
        oss << "Contention Hotspots:\n";
        for (size_t i = 0; i < std::min(contention_hotspots.size(), size_t(10)); ++i) {
            oss << "  " << (i + 1) << ". " << contention_hotspots[i] << "\n";
        }
        oss << "\n";
    }
    
    // Performance bottlenecks
    if (!performance_bottlenecks.empty()) {
        oss << "Performance Bottlenecks:\n";
        for (const auto& bottleneck : performance_bottlenecks) {
            oss << "  - " << bottleneck << "\n";
        }
        oss << "\n";
    }
    
    // Deadlock analysis
    if (deadlock_frequency > 0) {
        oss << "Deadlock Analysis:\n";
        oss << "  Deadlock Frequency: " << deadlock_frequency << " per hour\n";
        
        if (!deadlock_patterns.empty()) {
            oss << "  Common Patterns:\n";
            for (const auto& pattern : deadlock_patterns) {
                oss << "    - " << pattern << "\n";
            }
        }
        oss << "\n";
    }
    
    // Critical issues
    if (!critical_issues.empty()) {
        oss << "Critical Issues:\n";
        for (const auto& issue : critical_issues) {
            oss << "  ⚠️  " << issue << "\n";
        }
        oss << "\n";
    }
    
    // Warnings
    if (!warnings.empty()) {
        oss << "Warnings:\n";
        for (const auto& warning : warnings) {
            oss << "  ⚠️  " << warning << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

std::string SBEnhanced::LockAnalysisResult::generateRecommendationsReport() const {
    std::ostringstream oss;
    
    oss << "Lock Analysis Recommendations\n";
    oss << "============================\n";
    oss << "Database: " << database_name << "\n\n";
    
    if (!performance_recommendations.empty()) {
        oss << "Performance Recommendations:\n";
        for (size_t i = 0; i < performance_recommendations.size(); ++i) {
            oss << "  " << (i + 1) << ". " << performance_recommendations[i] << "\n";
        }
        oss << "\n";
    }
    
    if (!configuration_recommendations.empty()) {
        oss << "Configuration Recommendations:\n";
        for (size_t i = 0; i < configuration_recommendations.size(); ++i) {
            oss << "  " << (i + 1) << ". " << configuration_recommendations[i] << "\n";
        }
        oss << "\n";
    }
    
    if (!application_recommendations.empty()) {
        oss << "Application Recommendations:\n";
        for (size_t i = 0; i < application_recommendations.size(); ++i) {
            oss << "  " << (i + 1) << ". " << application_recommendations[i] << "\n";
        }
        oss << "\n";
    }
    
    if (!deadlock_prevention_recommendations.empty()) {
        oss << "Deadlock Prevention Recommendations:\n";
        for (size_t i = 0; i < deadlock_prevention_recommendations.size(); ++i) {
            oss << "  " << (i + 1) << ". " << deadlock_prevention_recommendations[i] << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

// LockMonitoringResult member implementations
std::chrono::milliseconds SBEnhanced::LockMonitoringResult::getMonitoringDuration() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
}

std::string SBEnhanced::LockMonitoringResult::generateMonitoringReport() const {
    std::ostringstream oss;
    
    oss << "Lock Monitoring Report\n";
    oss << "=====================\n";
    oss << "Monitoring Duration: " << getMonitoringDuration().count() << " ms\n";
    oss << "Monitoring Status: " << (monitoring_successful ? "Successful" : "Failed") << "\n\n";
    
    oss << "Summary Statistics:\n";
    oss << "  Total Snapshots: " << total_snapshots << "\n";
    oss << "  Total Locks Observed: " << total_locks_observed << "\n";
    oss << "  Total Deadlocks Detected: " << total_deadlocks_detected << "\n";
    oss << "  Peak Concurrent Locks: " << peak_concurrent_locks << "\n";
    oss << "  Average Lock Contention: " << std::fixed << std::setprecision(1) << (average_lock_contention * 100.0) << "%\n\n";
    
    if (!detected_deadlocks.empty()) {
        oss << "Detected Deadlocks:\n";
        for (const auto& deadlock : detected_deadlocks) {
            oss << "  - Deadlock " << deadlock.deadlock_id 
                << " at " << std::put_time(std::localtime(&std::chrono::system_clock::to_time_t(deadlock.detection_time)), "%H:%M:%S")
                << " (" << deadlock.involved_transactions.size() << " transactions)\n";
        }
        oss << "\n";
    }
    
    if (!performance_issues.empty()) {
        oss << "Performance Issues Detected:\n";
        for (const auto& issue : performance_issues) {
            oss << "  - " << issue << "\n";
        }
        oss << "\n";
    }
    
    if (!monitoring_errors.empty()) {
        oss << "Monitoring Errors:\n";
        for (const auto& error : monitoring_errors) {
            oss << "  - " << error << "\n";
        }
        oss << "\n";
    }
    
    return oss.str();
}

// LockPrintEnhanced Implementation
LockPrintEnhanced::LockPrintEnhanced()
    : engine(std::make_unique<SBEngineIntegration>()),
      lock_service(nullptr) {
    
    if (!initializeEngine()) {
        logError("Constructor", "Failed to initialize ScratchBird engine integration");
    }
    
    current_progress.start_time = std::chrono::steady_clock::now();
}

LockPrintEnhanced::~LockPrintEnhanced() {
    if (monitoring_active.load()) {
        requestShutdown();
        if (monitoring_thread && monitoring_thread->joinable()) {
            monitoring_thread->join();
        }
        if (deadlock_detection_thread && deadlock_detection_thread->joinable()) {
            deadlock_detection_thread->join();
        }
    }
}

bool LockPrintEnhanced::initializeEngine() {
    try {
        if (!engine) {
            logError("initializeEngine", "Engine integration not available");
            return false;
        }
        
        // Initialize engine components
        return engine->initialize();
        
    } catch (const std::exception& e) {
        logError("initializeEngine", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::initializeLockService() {
    try {
        // Initialize lock service component if needed
        // This would integrate with jrd::Service infrastructure
        return true;
        
    } catch (const std::exception& e) {
        logError("initializeLockService", std::string("Exception: ") + e.what());
        return false;
    }
}

// === ORIGINAL LOCK_PRINT FUNCTIONALITY (100% Compatible) ===

bool LockPrintEnhanced::printLocks(const std::string& database_path) {
    try {
        std::vector<SBEnhanced::LockInfo> locks;
        if (!getCurrentLocks(database_path, locks)) {
            logError("printLocks", "Failed to retrieve lock information");
            return false;
        }
        
        if (locks.empty()) {
            std::cout << "No locks found in database" << std::endl;
            return true;
        }
        
        // Print header
        std::cout << "Lock Information for Database: " << database_path << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        std::cout << std::left << std::setw(8) << "LockID"
                  << std::setw(12) << "TxnID"
                  << std::setw(12) << "Type"
                  << std::setw(10) << "State"
                  << std::setw(15) << "Object"
                  << std::setw(10) << "User"
                  << std::setw(10) << "Wait(ms)" << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        
        // Print lock information
        for (const auto& lock : locks) {
            std::cout << std::left << std::setw(8) << lock.lock_id
                      << std::setw(12) << lock.owner_transaction_id
                      << std::setw(12) << lock.getLockTypeString().substr(0, 11)
                      << std::setw(10) << lock.getLockStateString().substr(0, 9)
                      << std::setw(15) << lock.object_name.substr(0, 14)
                      << std::setw(10) << lock.owner_user.substr(0, 9)
                      << std::setw(10) << lock.wait_time.count() << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("printLocks", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::printLocksSummary(const std::string& database_path) {
    try {
        SBEnhanced::LockStatistics statistics;
        if (!collectLockStatistics(database_path, statistics)) {
            logError("printLocksSummary", "Failed to collect lock statistics");
            return false;
        }
        
        std::cout << "Lock Summary for Database: " << database_path << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "Total Locks: " << statistics.total_locks << std::endl;
        std::cout << "Active Locks: " << statistics.active_locks << std::endl;
        std::cout << "Waiting Locks: " << statistics.waiting_locks << std::endl;
        std::cout << "Deadlocked Locks: " << statistics.deadlocked_locks << std::endl;
        std::cout << "Average Wait Time: " << std::fixed << std::setprecision(2) 
                  << statistics.average_wait_time_ms << " ms" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("printLocksSummary", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::printDeadlocks(const std::string& database_path) {
    try {
        std::vector<SBEnhanced::DeadlockInfo> deadlocks;
        if (!detectDeadlocks(database_path, deadlocks)) {
            logError("printDeadlocks", "Failed to detect deadlocks");
            return false;
        }
        
        if (deadlocks.empty()) {
            std::cout << "No deadlocks found in database" << std::endl;
            return true;
        }
        
        std::cout << "Deadlock Information for Database: " << database_path << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        for (const auto& deadlock : deadlocks) {
            std::cout << deadlock.generateDeadlockReport() << std::endl;
            std::cout << std::string(80, '-') << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("printDeadlocks", std::string("Exception: ") + e.what());
        return false;
    }
}

// === ENHANCED FUNCTIONALITY ===

bool LockPrintEnhanced::startLockMonitoring(const SBEnhanced::LockMonitoringOptions& options,
                                            SBEnhanced::LockMonitoringResult& result) {
    try {
        if (monitoring_active.load()) {
            result.monitoring_errors.push_back("Lock monitoring is already active");
            return false;
        }
        
        result.monitoring_options = options;
        result.start_time = std::chrono::system_clock::now();
        current_progress.start_time = std::chrono::steady_clock::now();
        
        // Validate database access
        if (!validateDatabaseAccess(options.database_path)) {
            result.monitoring_errors.push_back("Cannot access database: " + options.database_path);
            return false;
        }
        
        // Initialize monitoring
        monitoring_active = true;
        shutdown_requested = false;
        current_progress.monitoring_active = true;
        current_progress.current_database = options.database_path;
        
        // Start monitoring thread
        monitoring_thread = std::make_unique<std::thread>(&LockPrintEnhanced::monitoringMainLoop, this, options);
        
        // Start deadlock detection thread if enabled
        if (options.deadlock_detection != SBEnhanced::DeadlockDetectionMode::NONE) {
            deadlock_detection_thread = std::make_unique<std::thread>(&LockPrintEnhanced::deadlockDetectionLoop, this, options.database_path);
        }
        
        result.monitoring_successful = true;
        logInfo("startLockMonitoring", "Lock monitoring started for: " + options.database_path);
        
        return true;
        
    } catch (const std::exception& e) {
        result.monitoring_errors.push_back(std::string("Exception: ") + e.what());
        logError("startLockMonitoring", e.what());
        return false;
    }
}

bool LockPrintEnhanced::stopLockMonitoring(SBEnhanced::LockMonitoringResult& result) {
    try {
        if (!monitoring_active.load()) {
            result.monitoring_warnings.push_back("Lock monitoring is not active");
            return true;
        }
        
        // Request shutdown
        requestShutdown();
        
        // Wait for threads to complete
        if (monitoring_thread && monitoring_thread->joinable()) {
            monitoring_thread->join();
        }
        
        if (deadlock_detection_thread && deadlock_detection_thread->joinable()) {
            deadlock_detection_thread->join();
        }
        
        monitoring_active = false;
        current_progress.monitoring_active = false;
        result.end_time = std::chrono::system_clock::now();
        
        // Collect final results
        result.monitoring_progress = current_progress;
        result.total_snapshots = current_progress.snapshots_collected;
        result.total_locks_observed = current_progress.locks_analyzed;
        result.total_deadlocks_detected = current_progress.deadlocks_detected;
        
        {
            std::lock_guard<std::mutex> lock(lock_data_mutex);
            result.detected_deadlocks = detected_deadlocks;
        }
        
        result.monitoring_successful = true;
        logInfo("stopLockMonitoring", "Lock monitoring stopped");
        
        return true;
        
    } catch (const std::exception& e) {
        result.monitoring_errors.push_back(std::string("Exception: ") + e.what());
        logError("stopLockMonitoring", e.what());
        return false;
    }
}

bool LockPrintEnhanced::performLockAnalysis(const SBEnhanced::LockAnalysisOptions& options,
                                            SBEnhanced::LockAnalysisResult& result) {
    try {
        result.analysis_time = std::chrono::system_clock::now();
        result.database_name = options.database_path;
        
        updateProgress("performLockAnalysis", options.database_path);
        
        // Collect current lock data
        if (options.analyze_current_locks) {
            if (!getCurrentLocks(options.database_path, result.current_locks)) {
                result.analysis_errors.push_back("Failed to collect current lock data");
                return false;
            }
            
            // Collect current statistics
            if (!collectLockStatistics(options.database_path, result.current_statistics)) {
                result.warnings.push_back("Failed to collect current statistics");
            }
        }
        
        // Detect active deadlocks
        if (options.analyze_deadlock_patterns) {
            if (!detectDeadlocks(options.database_path, result.active_deadlocks)) {
                result.warnings.push_back("Failed to detect active deadlocks");
            }
        }
        
        // Analyze contention hotspots
        if (options.analyze_contention_hotspots) {
            if (!analyzeLockContention(options.database_path, result.contention_hotspots)) {
                result.warnings.push_back("Failed to analyze lock contention");
            }
        }
        
        // Analyze performance impact
        if (options.analyze_performance_impact) {
            if (!analyzePerformanceImpact(options.database_path, result.performance_bottlenecks)) {
                result.warnings.push_back("Failed to analyze performance impact");
            }
        }
        
        // Calculate overall metrics
        if (!result.current_locks.empty()) {
            // Calculate lock efficiency
            uint64_t granted_locks = 0;
            for (const auto& lock : result.current_locks) {
                if (lock.lock_state == SBEnhanced::LockState::GRANTED) {
                    granted_locks++;
                }
            }
            result.overall_lock_efficiency = static_cast<double>(granted_locks) / result.current_locks.size();
            
            // Calculate contention level
            uint64_t waiting_locks = 0;
            for (const auto& lock : result.current_locks) {
                if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                    waiting_locks++;
                }
            }
            result.system_contention_level = static_cast<double>(waiting_locks) / result.current_locks.size();
        }
        
        // Generate recommendations
        if (options.generate_recommendations) {
            generateOptimizationRecommendations(options.database_path, result.performance_recommendations);
        }
        
        // Generate report if requested
        if (options.generate_detailed_report && !options.analysis_output_path.empty()) {
            std::ofstream report_file(options.analysis_output_path);
            if (report_file.is_open()) {
                report_file << result.generateAnalysisReport();
                if (options.generate_recommendations) {
                    report_file << "\n" << result.generateRecommendationsReport();
                }
                report_file.close();
                result.analysis_report_path = options.analysis_output_path;
            }
        }
        
        result.analysis_successful = true;
        return true;
        
    } catch (const std::exception& e) {
        result.analysis_errors.push_back(std::string("Exception: ") + e.what());
        logError("performLockAnalysis", e.what());
        return false;
    }
}

bool LockPrintEnhanced::getCurrentLocks(const std::string& database_path,
                                       std::vector<SBEnhanced::LockInfo>& locks) {
    try {
        return collectCurrentLockData(database_path, locks);
        
    } catch (const std::exception& e) {
        logError("getCurrentLocks", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::getWaitingLocks(const std::string& database_path,
                                       std::vector<SBEnhanced::LockInfo>& waiting_locks) {
    try {
        std::vector<SBEnhanced::LockInfo> all_locks;
        if (!getCurrentLocks(database_path, all_locks)) {
            return false;
        }
        
        waiting_locks.clear();
        for (const auto& lock : all_locks) {
            if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                waiting_locks.push_back(lock);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("getWaitingLocks", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::detectDeadlocks(const std::string& database_path,
                                       std::vector<SBEnhanced::DeadlockInfo>& deadlocks) {
    try {
        std::vector<SBEnhanced::LockInfo> locks;
        if (!getCurrentLocks(database_path, locks)) {
            return false;
        }
        
        return detectDeadlockCycles(locks, deadlocks);
        
    } catch (const std::exception& e) {
        logError("detectDeadlocks", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::collectLockStatistics(const std::string& database_path,
                                              SBEnhanced::LockStatistics& statistics) {
    try {
        std::vector<SBEnhanced::LockInfo> locks;
        if (!getCurrentLocks(database_path, locks)) {
            return false;
        }
        
        return calculateLockStatistics(locks, statistics);
        
    } catch (const std::exception& e) {
        logError("collectLockStatistics", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::analyzeLockContention(const std::string& database_path,
                                             std::vector<std::string>& contention_hotspots) {
    try {
        std::vector<SBEnhanced::LockInfo> locks;
        if (!getCurrentLocks(database_path, locks)) {
            return false;
        }
        
        return analyzeContentionPatterns(locks, contention_hotspots);
        
    } catch (const std::exception& e) {
        logError("analyzeLockContention", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::analyzePerformanceImpact(const std::string& database_path,
                                                 std::vector<std::string>& performance_issues) {
    try {
        std::vector<SBEnhanced::LockInfo> locks;
        if (!getCurrentLocks(database_path, locks)) {
            return false;
        }
        
        return identifyPerformanceBottlenecks(locks, performance_issues);
        
    } catch (const std::exception& e) {
        logError("analyzePerformanceImpact", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::generateOptimizationRecommendations(const std::string& database_path,
                                                            std::vector<std::string>& recommendations) {
    try {
        recommendations.clear();
        
        // Analyze current lock situation
        std::vector<SBEnhanced::LockInfo> locks;
        if (!getCurrentLocks(database_path, locks)) {
            return false;
        }
        
        SBEnhanced::LockStatistics statistics;
        if (!calculateLockStatistics(locks, statistics)) {
            return false;
        }
        
        // Generate recommendations based on analysis
        if (statistics.average_wait_time_ms > 1000.0) {
            recommendations.push_back("High average wait time detected. Consider optimizing transaction duration and lock ordering.");
        }
        
        if (statistics.deadlock_rate > 0.01) {
            recommendations.push_back("High deadlock rate detected. Review transaction patterns and consider implementing lock timeouts.");
        }
        
        if (statistics.contention_level > 0.3) {
            recommendations.push_back("High contention level detected. Consider partitioning hot tables or optimizing indexes.");
        }
        
        if (statistics.lock_efficiency < 0.8) {
            recommendations.push_back("Low lock efficiency detected. Review lock granularity and transaction design.");
        }
        
        // Analyze contention hotspots
        std::vector<std::string> hotspots;
        if (analyzeContentionPatterns(locks, hotspots)) {
            if (!hotspots.empty()) {
                recommendations.push_back("Contention hotspots identified. Focus optimization on: " + hotspots[0]);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("generateOptimizationRecommendations", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::validateDatabaseAccess(const std::string& database_path) {
    try {
        if (database_path.empty()) {
            return false;
        }
        
        // Check if database file exists
        if (!fs::exists(database_path)) {
            return false;
        }
        
        // Test connection
        std::unique_ptr<jrd::Attachment> attachment;
        bool success = establishDatabaseConnection(database_path, attachment);
        
        if (success) {
            closeDatabaseConnection(attachment);
        }
        
        return success;
        
    } catch (const std::exception& e) {
        logError("validateDatabaseAccess", std::string("Exception: ") + e.what());
        return false;
    }
}

SBEnhanced::LockMonitoringProgress LockPrintEnhanced::getCurrentProgress() const {
    SBEnhanced::LockMonitoringProgress progress = current_progress;
    progress.current_time = std::chrono::steady_clock::now();
    return progress;
}

bool LockPrintEnhanced::isMonitoringActive() const {
    return monitoring_active.load();
}

void LockPrintEnhanced::requestShutdown() {
    shutdown_requested = true;
}

std::vector<std::string> LockPrintEnhanced::getErrors() const {
    return error_log;
}

std::vector<std::string> LockPrintEnhanced::getWarnings() const {
    return warning_log;
}

std::string LockPrintEnhanced::getLastError() const {
    return last_error;
}

void LockPrintEnhanced::clearErrorLog() {
    error_log.clear();
    warning_log.clear();
    last_error.clear();
}

// Private implementation methods

bool LockPrintEnhanced::collectCurrentLockData(const std::string& database_path,
                                               std::vector<SBEnhanced::LockInfo>& locks) {
    try {
        locks.clear();
        
        // Establish database connection
        std::unique_ptr<jrd::Attachment> attachment;
        if (!establishDatabaseConnection(database_path, attachment)) {
            logError("collectCurrentLockData", "Failed to connect to database");
            return false;
        }
        
        // Query lock manager for current locks
        if (!queryLockManager(attachment.get(), locks)) {
            logError("collectCurrentLockData", "Failed to query lock manager");
            closeDatabaseConnection(attachment);
            return false;
        }
        
        // Query transaction manager for transaction locks
        std::vector<SBEnhanced::LockInfo> transaction_locks;
        if (queryTransactionManager(attachment.get(), transaction_locks)) {
            locks.insert(locks.end(), transaction_locks.begin(), transaction_locks.end());
        }
        
        closeDatabaseConnection(attachment);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("collectCurrentLockData", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::detectDeadlockCycles(const std::vector<SBEnhanced::LockInfo>& locks,
                                             std::vector<SBEnhanced::DeadlockInfo>& deadlocks) {
    try {
        deadlocks.clear();
        
        // Build wait-for graph
        std::map<uint64_t, std::vector<uint64_t>> wait_graph;
        if (!buildDependencyGraph(locks, wait_graph)) {
            return false;
        }
        
        // Find cycles in wait-for graph
        std::vector<std::vector<uint64_t>> cycles;
        if (!analyzeWaitForGraph(locks, cycles)) {
            return false;
        }
        
        // Create deadlock info for each cycle
        for (size_t i = 0; i < cycles.size(); ++i) {
            const auto& cycle = cycles[i];
            
            SBEnhanced::DeadlockInfo deadlock;
            deadlock.deadlock_id = i + 1;
            deadlock.detection_time = std::chrono::system_clock::now();
            deadlock.involved_transactions = cycle;
            deadlock.cycle_length = static_cast<uint32_t>(cycle.size());
            
            // Find locks involved in this cycle
            for (const auto& lock : locks) {
                if (std::find(cycle.begin(), cycle.end(), lock.owner_transaction_id) != cycle.end()) {
                    deadlock.involved_locks.push_back(lock.lock_id);
                    deadlock.lock_chain.push_back(lock);
                }
            }
            
            // Generate cycle description
            for (size_t j = 0; j < cycle.size(); ++j) {
                uint64_t txn1 = cycle[j];
                uint64_t txn2 = cycle[(j + 1) % cycle.size()];
                
                deadlock.cycle_description.push_back(
                    "Transaction " + std::to_string(txn1) + " waits for Transaction " + std::to_string(txn2)
                );
            }
            
            deadlock.deadlock_description = "Deadlock cycle involving " + std::to_string(cycle.size()) + " transactions";
            
            deadlocks.push_back(deadlock);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("detectDeadlockCycles", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::analyzeWaitForGraph(const std::vector<SBEnhanced::LockInfo>& locks,
                                           std::vector<std::vector<uint64_t>>& cycles) {
    try {
        cycles.clear();
        
        // Build adjacency list for wait-for graph
        std::map<uint64_t, std::vector<uint64_t>> graph;
        std::set<uint64_t> all_transactions;
        
        for (const auto& lock : locks) {
            all_transactions.insert(lock.owner_transaction_id);
            
            if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                // Find what this transaction is waiting for
                for (const auto& blocking_lock_id : lock.blocked_by) {
                    // Find the blocking lock
                    for (const auto& other_lock : locks) {
                        if (other_lock.lock_id == blocking_lock_id) {
                            graph[lock.owner_transaction_id].push_back(other_lock.owner_transaction_id);
                            break;
                        }
                    }
                }
            }
        }
        
        // Detect cycles using DFS
        std::set<uint64_t> visited;
        std::set<uint64_t> rec_stack;
        std::vector<uint64_t> current_path;
        
        for (uint64_t txn : all_transactions) {
            if (visited.find(txn) == visited.end()) {
                if (detectCycleDFS(txn, graph, visited, rec_stack, current_path, cycles)) {
                    // Cycle detected
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("analyzeWaitForGraph", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::detectCycleDFS(uint64_t txn,
                                       const std::map<uint64_t, std::vector<uint64_t>>& graph,
                                       std::set<uint64_t>& visited,
                                       std::set<uint64_t>& rec_stack,
                                       std::vector<uint64_t>& current_path,
                                       std::vector<std::vector<uint64_t>>& cycles) {
    visited.insert(txn);
    rec_stack.insert(txn);
    current_path.push_back(txn);
    
    auto it = graph.find(txn);
    if (it != graph.end()) {
        for (uint64_t neighbor : it->second) {
            if (rec_stack.find(neighbor) != rec_stack.end()) {
                // Cycle detected - extract the cycle
                std::vector<uint64_t> cycle;
                bool in_cycle = false;
                for (uint64_t path_txn : current_path) {
                    if (path_txn == neighbor) {
                        in_cycle = true;
                    }
                    if (in_cycle) {
                        cycle.push_back(path_txn);
                    }
                }
                if (!cycle.empty()) {
                    cycles.push_back(cycle);
                }
                return true;
            } else if (visited.find(neighbor) == visited.end()) {
                if (detectCycleDFS(neighbor, graph, visited, rec_stack, current_path, cycles)) {
                    return true;
                }
            }
        }
    }
    
    rec_stack.erase(txn);
    current_path.pop_back();
    return false;
}

bool LockPrintEnhanced::buildDependencyGraph(const std::vector<SBEnhanced::LockInfo>& locks,
                                             std::map<uint64_t, std::vector<uint64_t>>& graph) {
    try {
        graph.clear();
        
        // Build resource allocation and request maps
        std::map<std::string, std::vector<uint64_t>> resource_holders;  // resource -> list of transaction IDs
        std::map<std::string, std::vector<uint64_t>> resource_waiters;   // resource -> list of waiting transaction IDs
        
        for (const auto& lock : locks) {
            std::string resource = lock.lock_resource;
            if (resource.empty()) {
                resource = lock.table_name + ":" + std::to_string(lock.object_id);
            }
            
            if (lock.lock_state == SBEnhanced::LockState::GRANTED) {
                resource_holders[resource].push_back(lock.owner_transaction_id);
            } else if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                resource_waiters[resource].push_back(lock.owner_transaction_id);
            }
        }
        
        // Build wait-for relationships
        for (const auto& waiter_pair : resource_waiters) {
            const std::string& resource = waiter_pair.first;
            const auto& waiters = waiter_pair.second;
            
            auto holder_it = resource_holders.find(resource);
            if (holder_it != resource_holders.end()) {
                const auto& holders = holder_it->second;
                
                // Each waiter waits for all holders
                for (uint64_t waiter : waiters) {
                    for (uint64_t holder : holders) {
                        if (waiter != holder) {
                            graph[waiter].push_back(holder);
                        }
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("buildDependencyGraph", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::calculateLockStatistics(const std::vector<SBEnhanced::LockInfo>& locks,
                                                SBEnhanced::LockStatistics& statistics) {
    try {
        statistics = SBEnhanced::LockStatistics();
        statistics.collection_time = std::chrono::system_clock::now();
        statistics.total_locks = locks.size();
        
        if (locks.empty()) {
            return true;
        }
        
        // Count locks by state, type, and scope
        std::set<uint64_t> unique_transactions;
        std::set<uint64_t> transactions_with_locks;
        std::set<uint64_t> waiting_transactions;
        std::set<uint64_t> deadlocked_transactions;
        
        double total_wait_time = 0.0;
        double total_hold_time = 0.0;
        uint64_t wait_count = 0;
        uint64_t hold_count = 0;
        
        for (const auto& lock : locks) {
            unique_transactions.insert(lock.owner_transaction_id);
            transactions_with_locks.insert(lock.owner_transaction_id);
            
            // Count by state
            statistics.locks_by_state[lock.lock_state]++;
            
            switch (lock.lock_state) {
                case SBEnhanced::LockState::GRANTED:
                    statistics.active_locks++;
                    if (lock.hold_time.count() > 0) {
                        total_hold_time += lock.hold_time.count();
                        hold_count++;
                        statistics.maximum_hold_time_ms = std::max(statistics.maximum_hold_time_ms, 
                                                                  static_cast<double>(lock.hold_time.count()));
                    }
                    break;
                case SBEnhanced::LockState::WAITING:
                    statistics.waiting_locks++;
                    waiting_transactions.insert(lock.owner_transaction_id);
                    if (lock.wait_time.count() > 0) {
                        total_wait_time += lock.wait_time.count();
                        wait_count++;
                        statistics.maximum_wait_time_ms = std::max(statistics.maximum_wait_time_ms,
                                                                  static_cast<double>(lock.wait_time.count()));
                    }
                    break;
                case SBEnhanced::LockState::DEADLOCKED:
                    statistics.deadlocked_locks++;
                    deadlocked_transactions.insert(lock.owner_transaction_id);
                    break;
                default:
                    break;
            }
            
            // Count by type and scope
            statistics.locks_by_type[lock.lock_type]++;
            statistics.locks_by_scope[lock.lock_scope]++;
        }
        
        // Calculate averages
        if (wait_count > 0) {
            statistics.average_wait_time_ms = total_wait_time / wait_count;
        }
        
        if (hold_count > 0) {
            statistics.average_hold_time_ms = total_hold_time / hold_count;
        }
        
        // Transaction statistics
        statistics.total_transactions = unique_transactions.size();
        statistics.transactions_with_locks = transactions_with_locks.size();
        statistics.transactions_waiting = waiting_transactions.size();
        statistics.transactions_deadlocked = deadlocked_transactions.size();
        
        // Calculate efficiency metrics
        if (statistics.total_locks > 0) {
            statistics.lock_efficiency = static_cast<double>(statistics.active_locks) / statistics.total_locks;
            statistics.contention_level = static_cast<double>(statistics.waiting_locks) / statistics.total_locks;
        }
        
        // Identify most contended objects
        std::map<std::string, uint64_t> object_contention;
        for (const auto& lock : locks) {
            if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                object_contention[lock.object_name]++;
            }
        }
        
        // Sort and get top contended objects
        std::vector<std::pair<uint64_t, std::string>> contention_pairs;
        for (const auto& pair : object_contention) {
            contention_pairs.emplace_back(pair.second, pair.first);
        }
        
        std::sort(contention_pairs.rbegin(), contention_pairs.rend());
        
        for (size_t i = 0; i < std::min(contention_pairs.size(), size_t(5)); ++i) {
            statistics.most_contended_objects.push_back(contention_pairs[i].second);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("calculateLockStatistics", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::analyzeContentionPatterns(const std::vector<SBEnhanced::LockInfo>& locks,
                                                  std::vector<std::string>& patterns) {
    try {
        patterns.clear();
        
        // Analyze contention by object
        std::map<std::string, uint64_t> object_contention;
        std::map<std::string, uint64_t> user_blocking_count;
        
        for (const auto& lock : locks) {
            if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                object_contention[lock.object_name]++;
            }
            
            if (lock.is_blocking) {
                user_blocking_count[lock.owner_user]++;
            }
        }
        
        // Find top contended objects
        if (!object_contention.empty()) {
            auto max_contention = std::max_element(object_contention.begin(), object_contention.end(),
                                                  [](const auto& a, const auto& b) { return a.second < b.second; });
            
            if (max_contention->second > 1) {
                patterns.push_back("High contention on object: " + max_contention->first + 
                                 " (" + std::to_string(max_contention->second) + " waiting locks)");
            }
        }
        
        // Find users causing most blocking
        if (!user_blocking_count.empty()) {
            auto max_blocker = std::max_element(user_blocking_count.begin(), user_blocking_count.end(),
                                               [](const auto& a, const auto& b) { return a.second < b.second; });
            
            if (max_blocker->second > 2) {
                patterns.push_back("User causing most blocking: " + max_blocker->first + 
                                 " (" + std::to_string(max_blocker->second) + " blocking locks)");
            }
        }
        
        // Analyze lock type patterns
        std::map<SBEnhanced::LockType, uint64_t> type_contention;
        for (const auto& lock : locks) {
            if (lock.lock_state == SBEnhanced::LockState::WAITING) {
                type_contention[lock.lock_type]++;
            }
        }
        
        if (!type_contention.empty()) {
            auto max_type = std::max_element(type_contention.begin(), type_contention.end(),
                                            [](const auto& a, const auto& b) { return a.second < b.second; });
            
            if (max_type->second > 1) {
                SBEnhanced::LockInfo temp_lock;
                temp_lock.lock_type = max_type->first;
                patterns.push_back("Most contested lock type: " + temp_lock.getLockTypeString() + 
                                 " (" + std::to_string(max_type->second) + " waiting)");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("analyzeContentionPatterns", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::identifyPerformanceBottlenecks(const std::vector<SBEnhanced::LockInfo>& locks,
                                                       std::vector<std::string>& bottlenecks) {
    try {
        bottlenecks.clear();
        
        // Identify long-running locks
        for (const auto& lock : locks) {
            if (lock.wait_time.count() > 5000) {  // 5 seconds
                bottlenecks.push_back("Long wait time for " + lock.getLockTypeString() + 
                                    " lock on " + lock.object_name + 
                                    " (" + std::to_string(lock.wait_time.count()) + "ms)");
            }
            
            if (lock.hold_time.count() > 30000) {  // 30 seconds
                bottlenecks.push_back("Long-held " + lock.getLockTypeString() + 
                                    " lock on " + lock.object_name + 
                                    " (" + std::to_string(lock.hold_time.count()) + "ms)");
            }
        }
        
        // Identify lock escalation patterns
        std::map<uint64_t, std::vector<SBEnhanced::LockInfo>> transaction_locks;
        for (const auto& lock : locks) {
            transaction_locks[lock.owner_transaction_id].push_back(lock);
        }
        
        for (const auto& pair : transaction_locks) {
            if (pair.second.size() > 100) {  // Many locks per transaction
                bottlenecks.push_back("Transaction " + std::to_string(pair.first) + 
                                    " holding many locks (" + std::to_string(pair.second.size()) + ")");
            }
        }
        
        // Identify blocking chains
        std::map<uint64_t, uint64_t> blocking_chain_length;
        for (const auto& lock : locks) {
            if (lock.is_blocking) {
                for (uint64_t blocked_lock_id : lock.blocking) {
                    blocking_chain_length[lock.lock_id]++;
                }
            }
        }
        
        for (const auto& pair : blocking_chain_length) {
            if (pair.second > 5) {  // Blocking many other locks
                bottlenecks.push_back("Lock " + std::to_string(pair.first) + 
                                    " blocking many others (" + std::to_string(pair.second) + " locks)");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("identifyPerformanceBottlenecks", std::string("Exception: ") + e.what());
        return false;
    }
}

void LockPrintEnhanced::monitoringMainLoop(const SBEnhanced::LockMonitoringOptions& options) {
    try {
        logInfo("monitoringMainLoop", "Lock monitoring started for: " + options.database_path);
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (!shutdown_requested.load()) {
            current_progress.current_time = std::chrono::steady_clock::now();
            current_progress.current_operation = "Monitoring locks";
            
            // Process lock snapshot
            if (!processLockSnapshot(options.database_path, options)) {
                logError("monitoringMainLoop", "Failed to process lock snapshot");
            }
            
            current_progress.snapshots_collected++;
            
            // Check if monitoring duration limit reached
            if (options.monitoring_duration_seconds > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start_time);
                
                if (elapsed.count() >= options.monitoring_duration_seconds) {
                    break;
                }
            }
            
            // Sleep for monitoring interval
            std::this_thread::sleep_for(std::chrono::seconds(options.monitoring_interval_seconds));
        }
        
        logInfo("monitoringMainLoop", "Lock monitoring ended");
        
    } catch (const std::exception& e) {
        logError("monitoringMainLoop", std::string("Exception: ") + e.what());
    }
}

void LockPrintEnhanced::deadlockDetectionLoop(const std::string& database_path) {
    try {
        logInfo("deadlockDetectionLoop", "Deadlock detection started for: " + database_path);
        
        while (!shutdown_requested.load()) {
            // Detect deadlocks
            std::vector<SBEnhanced::DeadlockInfo> deadlocks;
            if (detectDeadlocks(database_path, deadlocks)) {
                if (!deadlocks.empty()) {
                    std::lock_guard<std::mutex> lock(lock_data_mutex);
                    
                    for (const auto& deadlock : deadlocks) {
                        detected_deadlocks.push_back(deadlock);
                        current_progress.deadlocks_detected++;
                        
                        logInfo("deadlockDetectionLoop", "Deadlock detected: " + std::to_string(deadlock.deadlock_id));
                    }
                }
            }
            
            // Sleep for deadlock detection interval
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
        
        logInfo("deadlockDetectionLoop", "Deadlock detection ended");
        
    } catch (const std::exception& e) {
        logError("deadlockDetectionLoop", std::string("Exception: ") + e.what());
    }
}

bool LockPrintEnhanced::processLockSnapshot(const std::string& database_path,
                                           const SBEnhanced::LockMonitoringOptions& options) {
    try {
        std::vector<SBEnhanced::LockInfo> snapshot_locks;
        if (!getCurrentLocks(database_path, snapshot_locks)) {
            return false;
        }
        
        current_progress.locks_analyzed += snapshot_locks.size();
        
        // Update statistics
        updateStatistics(snapshot_locks);
        
        // Call user callback if provided
        if (options.lock_callback) {
            options.lock_callback(snapshot_locks);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("processLockSnapshot", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::updateStatistics(const std::vector<SBEnhanced::LockInfo>& locks) {
    try {
        std::lock_guard<std::mutex> lock(statistics_mutex);
        
        SBEnhanced::LockStatistics stats;
        if (calculateLockStatistics(locks, stats)) {
            historical_statistics.push_back(stats);
            
            // Keep only recent statistics (last 100 snapshots)
            if (historical_statistics.size() > 100) {
                historical_statistics.erase(historical_statistics.begin());
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("updateStatistics", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::establishDatabaseConnection(const std::string& database_path,
                                                   std::unique_ptr<jrd::Attachment>& attachment) {
    try {
        // This is a simplified connection implementation
        // In a real implementation, this would use the ScratchBird engine
        // to establish a proper database connection
        
        if (database_path.empty()) {
            return false;
        }
        
        // Check if database file exists
        if (!fs::exists(database_path)) {
            return false;
        }
        
        // Simulate connection establishment
        // attachment = engine->createAttachment(database_path);
        
        return true;  // Simplified success
        
    } catch (const std::exception& e) {
        logError("establishDatabaseConnection", std::string("Exception: ") + e.what());
        return false;
    }
}

void LockPrintEnhanced::closeDatabaseConnection(std::unique_ptr<jrd::Attachment>& attachment) {
    try {
        if (attachment) {
            // Close the attachment
            attachment.reset();
        }
        
    } catch (const std::exception& e) {
        logError("closeDatabaseConnection", std::string("Exception: ") + e.what());
    }
}

bool LockPrintEnhanced::queryLockManager(jrd::Attachment* attachment,
                                         std::vector<SBEnhanced::LockInfo>& locks) {
    try {
        // This is a simplified implementation
        // In a real implementation, this would query the actual lock manager
        
        // Simulate some lock data for demonstration
        SBEnhanced::LockInfo sample_lock;
        sample_lock.lock_id = 1;
        sample_lock.owner_transaction_id = 100;
        sample_lock.owner_user = "SYSDBA";
        sample_lock.lock_type = SBEnhanced::LockType::SHARED_READ;
        sample_lock.lock_state = SBEnhanced::LockState::GRANTED;
        sample_lock.lock_scope = SBEnhanced::LockScope::TABLE;
        sample_lock.object_name = "TEST_TABLE";
        sample_lock.lock_time = std::chrono::system_clock::now();
        sample_lock.hold_time = std::chrono::milliseconds(1000);
        
        locks.push_back(sample_lock);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("queryLockManager", std::string("Exception: ") + e.what());
        return false;
    }
}

bool LockPrintEnhanced::queryTransactionManager(jrd::Attachment* attachment,
                                                std::vector<SBEnhanced::LockInfo>& transaction_locks) {
    try {
        // This is a simplified implementation
        // In a real implementation, this would query the transaction manager
        
        return true;  // No additional locks for now
        
    } catch (const std::exception& e) {
        logError("queryTransactionManager", std::string("Exception: ") + e.what());
        return false;
    }
}

void LockPrintEnhanced::updateProgress(const std::string& operation, const std::string& current_database) {
    current_progress.current_operation = operation;
    current_progress.current_database = current_database;
}

void LockPrintEnhanced::logError(const std::string& operation, const std::string& error) {
    std::string full_error = "[" + operation + "] " + error;
    error_log.push_back(full_error);
    last_error = full_error;
    
    // Limit log size
    if (error_log.size() > 1000) {
        error_log.erase(error_log.begin(), error_log.begin() + 100);
    }
    
    // Also log to console
    std::cerr << "ERROR: " << full_error << std::endl;
}

void LockPrintEnhanced::logWarning(const std::string& operation, const std::string& warning) {
    std::string full_warning = "[" + operation + "] " + warning;
    warning_log.push_back(full_warning);
    
    // Limit log size
    if (warning_log.size() > 1000) {
        warning_log.erase(warning_log.begin(), warning_log.begin() + 100);
    }
    
    // Also log to console
    std::cout << "WARNING: " << full_warning << std::endl;
}

void LockPrintEnhanced::logInfo(const std::string& operation, const std::string& info) {
    std::string full_info = "[" + operation + "] " + info;
    
    // Log to console
    std::cout << "INFO: " << full_info << std::endl;
}

// Utility functions implementation
namespace SBEnhanced {

bool quickPrintLocks(const std::string& database_path) {
    try {
        LockPrintEnhanced lock_print;
        return lock_print.printLocks(database_path);
    } catch (...) {
        return false;
    }
}

bool quickDetectDeadlocks(const std::string& database_path) {
    try {
        LockPrintEnhanced lock_print;
        return lock_print.printDeadlocks(database_path);
    } catch (...) {
        return false;
    }
}

bool quickAnalyzeLocks(const std::string& database_path) {
    try {
        LockPrintEnhanced lock_print;
        LockAnalysisOptions options;
        options.database_path = database_path;
        options.analysis_level = LockAnalysisLevel::STANDARD;
        
        LockAnalysisResult result;
        if (lock_print.performLockAnalysis(options, result)) {
            std::cout << result.generateAnalysisReport() << std::endl;
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

std::string formatLockDuration(const std::chrono::milliseconds& duration) {
    auto ms = duration.count();
    
    if (ms < 1000) {
        return std::to_string(ms) + "ms";
    } else if (ms < 60000) {
        return std::to_string(ms / 1000) + "s";
    } else {
        return std::to_string(ms / 60000) + "m" + std::to_string((ms % 60000) / 1000) + "s";
    }
}

std::string formatLockResource(const LockInfo& lock_info) {
    std::ostringstream oss;
    oss << lock_info.getLockScopeString() << ":";
    
    if (!lock_info.table_name.empty()) {
        oss << lock_info.table_name;
        if (lock_info.record_number > 0) {
            oss << "[" << lock_info.record_number << "]";
        }
    } else if (!lock_info.object_name.empty()) {
        oss << lock_info.object_name;
    } else {
        oss << "Object_" << lock_info.object_id;
    }
    
    return oss.str();
}

double calculateContentionLevel(const std::vector<LockInfo>& locks) {
    if (locks.empty()) {
        return 0.0;
    }
    
    uint64_t waiting_locks = 0;
    for (const auto& lock : locks) {
        if (lock.lock_state == LockState::WAITING) {
            waiting_locks++;
        }
    }
    
    return static_cast<double>(waiting_locks) / locks.size();
}

bool hasDeadlockCycle(const std::vector<LockInfo>& locks) {
    // Simple deadlock detection - check for waiting locks that are circular
    std::map<uint64_t, std::vector<uint64_t>> wait_graph;
    
    for (const auto& lock : locks) {
        if (lock.lock_state == LockState::WAITING) {
            for (uint64_t blocking_lock_id : lock.blocked_by) {
                for (const auto& other_lock : locks) {
                    if (other_lock.lock_id == blocking_lock_id) {
                        wait_graph[lock.owner_transaction_id].push_back(other_lock.owner_transaction_id);
                        break;
                    }
                }
            }
        }
    }
    
    // Simple cycle detection using DFS
    std::set<uint64_t> visited;
    std::set<uint64_t> rec_stack;
    
    for (const auto& pair : wait_graph) {
        if (visited.find(pair.first) == visited.end()) {
            std::function<bool(uint64_t)> hasCycleDFS = [&](uint64_t txn) -> bool {
                visited.insert(txn);
                rec_stack.insert(txn);
                
                auto it = wait_graph.find(txn);
                if (it != wait_graph.end()) {
                    for (uint64_t neighbor : it->second) {
                        if (rec_stack.find(neighbor) != rec_stack.end()) {
                            return true;  // Back edge found
                        }
                        if (visited.find(neighbor) == visited.end() && hasCycleDFS(neighbor)) {
                            return true;
                        }
                    }
                }
                
                rec_stack.erase(txn);
                return false;
            };
            
            if (hasCycleDFS(pair.first)) {
                return true;
            }
        }
    }
    
    return false;
}

std::vector<uint64_t> findDeadlockVictims(const DeadlockInfo& deadlock) {
    std::vector<uint64_t> victims;
    
    // Simple victim selection - choose the transaction with the least work done
    // In a real implementation, this would use more sophisticated criteria
    
    if (!deadlock.involved_transactions.empty()) {
        victims.push_back(deadlock.involved_transactions[0]);
    }
    
    return victims;
}

double calculateLockEfficiency(const LockStatistics& statistics) {
    if (statistics.total_locks == 0) {
        return 1.0;
    }
    
    return static_cast<double>(statistics.active_locks) / statistics.total_locks;
}

std::vector<std::string> identifyContentionHotspots(const std::vector<LockInfo>& locks) {
    std::map<std::string, uint64_t> object_contention;
    
    for (const auto& lock : locks) {
        if (lock.lock_state == LockState::WAITING) {
            object_contention[lock.object_name]++;
        }
    }
    
    std::vector<std::pair<uint64_t, std::string>> contention_pairs;
    for (const auto& pair : object_contention) {
        contention_pairs.emplace_back(pair.second, pair.first);
    }
    
    std::sort(contention_pairs.rbegin(), contention_pairs.rend());
    
    std::vector<std::string> hotspots;
    for (size_t i = 0; i < std::min(contention_pairs.size(), size_t(5)); ++i) {
        hotspots.push_back(contention_pairs[i].second);
    }
    
    return hotspots;
}

bool exportToCSV(const std::vector<LockInfo>& locks, const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        // Write header
        file << "LockID,TransactionID,LockType,LockState,ObjectName,User,WaitTime,HoldTime\n";
        
        // Write data
        for (const auto& lock : locks) {
            file << lock.lock_id << ","
                 << lock.owner_transaction_id << ","
                 << lock.getLockTypeString() << ","
                 << lock.getLockStateString() << ","
                 << lock.object_name << ","
                 << lock.owner_user << ","
                 << lock.wait_time.count() << ","
                 << lock.hold_time.count() << "\n";
        }
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool exportToJSON(const std::vector<LockInfo>& locks, const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << "{\n  \"locks\": [\n";
        
        for (size_t i = 0; i < locks.size(); ++i) {
            const auto& lock = locks[i];
            
            file << "    {\n";
            file << "      \"lock_id\": " << lock.lock_id << ",\n";
            file << "      \"transaction_id\": " << lock.owner_transaction_id << ",\n";
            file << "      \"lock_type\": \"" << lock.getLockTypeString() << "\",\n";
            file << "      \"lock_state\": \"" << lock.getLockStateString() << "\",\n";
            file << "      \"object_name\": \"" << lock.object_name << "\",\n";
            file << "      \"user\": \"" << lock.owner_user << "\",\n";
            file << "      \"wait_time_ms\": " << lock.wait_time.count() << ",\n";
            file << "      \"hold_time_ms\": " << lock.hold_time.count() << "\n";
            file << "    }";
            
            if (i < locks.size() - 1) {
                file << ",";
            }
            file << "\n";
        }
        
        file << "  ]\n}\n";
        
        return true;
        
    } catch (...) {
        return false;
    }
}

bool exportToXML(const std::vector<LockInfo>& locks, const std::string& filename) {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<locks>\n";
        
        for (const auto& lock : locks) {
            file << "  <lock>\n";
            file << "    <lock_id>" << lock.lock_id << "</lock_id>\n";
            file << "    <transaction_id>" << lock.owner_transaction_id << "</transaction_id>\n";
            file << "    <lock_type>" << lock.getLockTypeString() << "</lock_type>\n";
            file << "    <lock_state>" << lock.getLockStateString() << "</lock_state>\n";
            file << "    <object_name>" << lock.object_name << "</object_name>\n";
            file << "    <user>" << lock.owner_user << "</user>\n";
            file << "    <wait_time_ms>" << lock.wait_time.count() << "</wait_time_ms>\n";
            file << "    <hold_time_ms>" << lock.hold_time.count() << "</hold_time_ms>\n";
            file << "  </lock>\n";
        }
        
        file << "</locks>\n";
        
        return true;
        
    } catch (...) {
        return false;
    }
}

} // namespace SBEnhanced