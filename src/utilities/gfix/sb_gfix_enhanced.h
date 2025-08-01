#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>

// Forward declarations for ScratchBird engine components
namespace jrd {
    class Attachment;
    class Database;
    class Transaction;
    class Service;
}

class SBEngineIntegration;

namespace SBEnhanced {

// Database maintenance operation types
enum class MaintenanceOperation {
    VALIDATION = 0,
    REPAIR = 1,
    SWEEP = 2,
    MEND = 3,
    LIMBO_RESOLUTION = 4,
    INDEX_REBUILD = 5,
    STATISTICS_UPDATE = 6,
    SPACE_RECLAIM = 7,
    CHECKSUM_VERIFICATION = 8,
    DATABASE_SHUTDOWN = 9,
    DATABASE_ONLINE = 10,
    SHADOW_MANAGEMENT = 11,
    CONFIGURATION_CHANGE = 12,
    TRANSACTION_MANAGEMENT = 13,
    ACCESS_MODE_CHANGE = 14,
    WRITE_MODE_CHANGE = 15,
    BUFFER_MANAGEMENT = 16
};

// Database shutdown modes (original GFIX compatibility)
enum class ShutdownMode {
    NORMAL = 0,          // Normal shutdown - wait for current users
    MULTI = 1,           // Multi-user shutdown
    SINGLE = 2,          // Single-user shutdown - deny new connections
    FULL = 3,            // Full shutdown - no connections allowed
    FORCE = 4,           // Force shutdown immediately
    ATTACHMENT = 5,      // Shutdown new attachments only
    TRANSACTION = 6      // Shutdown transaction startup
};

// Database access modes
enum class DatabaseAccessMode {
    READ_WRITE = 0,      // Normal read-write access
    READ_ONLY = 1        // Read-only access mode
};

// Database write modes
enum class DatabaseWriteMode {
    SYNC = 0,            // Synchronous writes (safer)
    ASYNC = 1            // Asynchronous writes (faster)
};

// Space usage modes
enum class SpaceUsageMode {
    RESERVE = 0,         // Use reserve space for record versions
    FULL = 1             // Use full space for record versions
};

// Shadow file operations
enum class ShadowOperation {
    ACTIVATE = 0,        // Activate shadow file for database usage
    KILL = 1             // Kill all unavailable shadow files
};

// Transaction resolution modes
enum class TransactionResolution {
    COMMIT = 0,          // Commit limbo transaction
    ROLLBACK = 1,        // Rollback limbo transaction
    PROMPT = 2,          // Prompt for commit/rollback decision
    AUTO_TWO_PHASE = 3   // Automatic two-phase recovery
};

// Replica modes (modern Firebird feature)
enum class ReplicaMode {
    NONE = 0,            // No replica mode
    READ_ONLY = 1,       // Read-only replica
    READ_WRITE = 2       // Read-write replica
};

// Repair strategy types
enum class RepairStrategy {
    CONSERVATIVE = 0,     // Minimal repairs, preserve data integrity
    AGGRESSIVE = 1,       // More extensive repairs, may lose some data
    SALVAGE = 2,         // Maximum recovery effort, expect data loss
    VALIDATE_ONLY = 3    // Only validate, do not repair
};

// Validation severity levels
enum class ValidationSeverity {
    BASIC = 0,           // Basic structural validation
    NORMAL = 1,          // Standard validation with record checks
    FULL = 2,            // Comprehensive validation including indexes
    DEEP = 3,            // Deep analysis with detailed reporting
    FORENSIC = 4         // Forensic-level analysis for corruption investigation
};

// Progress tracking for maintenance operations
struct MaintenanceProgress {
    MaintenanceOperation current_operation = MaintenanceOperation::VALIDATION;
    uint64_t total_pages = 0;
    uint64_t processed_pages = 0;
    uint64_t total_records = 0;
    uint64_t processed_records = 0;
    uint64_t errors_found = 0;
    uint64_t errors_fixed = 0;
    std::string current_object;
    std::chrono::steady_clock::time_point start_time;
    bool operation_active = false;
    
    double getProgressPercentage() const {
        if (total_pages == 0) return 0.0;
        return (static_cast<double>(processed_pages) / total_pages) * 100.0;
    }
    
    std::chrono::seconds getElapsedTime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    }
    
    std::chrono::seconds getEstimatedTimeRemaining() const {
        if (processed_pages == 0) return std::chrono::seconds(0);
        auto elapsed = getElapsedTime();
        double progress = getProgressPercentage() / 100.0;
        if (progress <= 0.0) return std::chrono::seconds(0);
        double total_time = elapsed.count() / progress;
        return std::chrono::seconds(static_cast<long>(total_time - elapsed.count()));
    }
};

// Validation options configuration
struct ValidationOptions {
    ValidationSeverity severity = ValidationSeverity::NORMAL;
    bool check_record_fragments = true;
    bool check_blob_integrity = true;
    bool check_index_consistency = true;
    bool check_referential_integrity = false;  // Can be expensive
    bool check_generator_values = true;
    bool check_trigger_validity = true;
    bool check_procedure_validity = true;
    bool include_system_tables = false;
    bool generate_detailed_report = true;
    bool continue_on_errors = true;
    uint32_t max_errors_to_report = 1000;
    std::string output_file_path;
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Repair options configuration
struct RepairOptions {
    RepairStrategy strategy = RepairStrategy::CONSERVATIVE;
    bool create_backup_before_repair = true;
    std::string backup_path;
    bool fix_record_fragments = true;
    bool fix_blob_corruption = true;
    bool rebuild_corrupt_indexes = true;
    bool resolve_limbo_transactions = true;
    bool update_damaged_generators = false;  // Dangerous operation
    bool reclaim_unused_space = true;
    bool validate_after_repair = true;
    uint32_t max_repair_attempts = 3;
    bool continue_on_critical_errors = false;
    std::string repair_log_path;
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Sweep options configuration
struct SweepOptions {
    bool force_sweep = false;
    bool cooperative_sweep = true;  // Allow other connections during sweep
    uint32_t sweep_interval_override = 0;  // 0 = use database default
    bool update_statistics_during_sweep = true;
    bool optimize_record_versions = true;
    bool reclaim_blob_space = true;
    uint64_t max_sweep_duration_minutes = 0;  // 0 = no limit
    std::string sweep_log_path;
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Database shutdown options (original GFIX compatibility)
struct ShutdownOptions {
    ShutdownMode mode = ShutdownMode::NORMAL;
    uint32_t timeout_seconds = 0;  // 0 = no timeout
    bool force_shutdown = false;
    bool deny_new_attachments = false;
    bool deny_new_transactions = false;
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Database configuration options
struct DatabaseConfigOptions {
    // Page buffer configuration
    bool set_page_buffers = false;
    uint32_t page_buffers = 0;
    
    // Sweep interval configuration
    bool set_sweep_interval = false;
    uint32_t sweep_interval = 20000;
    
    // Access mode configuration
    bool set_access_mode = false;
    DatabaseAccessMode access_mode = DatabaseAccessMode::READ_WRITE;
    
    // Write mode configuration
    bool set_write_mode = false;
    DatabaseWriteMode write_mode = DatabaseWriteMode::SYNC;
    
    // Space usage configuration
    bool set_space_usage = false;
    SpaceUsageMode space_usage = SpaceUsageMode::RESERVE;
    
    // SQL dialect configuration
    bool set_sql_dialect = false;
    uint32_t sql_dialect = 3;
    
    // Replica mode configuration
    bool set_replica_mode = false;
    ReplicaMode replica_mode = ReplicaMode::NONE;
    
    // ICU and upgrade options
    bool fix_icu = false;
    bool upgrade_ods = false;
    bool disable_linger = false;
    
    // Parallel processing
    bool set_parallel_workers = false;
    uint32_t parallel_workers = 1;
    
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Transaction management options
struct TransactionOptions {
    TransactionResolution resolution = TransactionResolution::PROMPT;
    uint64_t specific_transaction_id = 0;  // 0 = all limbo transactions
    bool list_only = false;
    bool auto_commit_prepared = true;
    bool show_transaction_details = false;
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Shadow file management options
struct ShadowOptions {
    ShadowOperation operation = ShadowOperation::ACTIVATE;
    bool force_operation = false;
    std::vector<std::string> specific_shadow_files;  // Empty = all shadows
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Extended validation options (original GFIX modes)
struct ExtendedValidationOptions {
    bool full_validation = false;        // -full: validate record fragments
    bool read_only_validation = false;   // -no_update: read-only validation
    bool ignore_checksums = false;       // -ignore: ignore checksum errors
    bool mend_database = false;          // -mend: prepare corrupt database for backup
    bool check_database_pages = true;
    bool check_data_pages = true;
    bool check_index_pages = true;
    bool check_blob_pages = true;
    bool validate_system_tables = false;
    uint32_t max_validation_errors = 0;  // 0 = no limit
    std::function<void(const MaintenanceProgress&)> progress_callback;
};

// Authentication and connection options
struct ConnectionOptions {
    std::string username;
    std::string password;
    std::string role;
    std::string password_file;          // -fetch_password
    bool use_trusted_auth = false;      // -trusted
    bool use_embedded = false;
    uint32_t connection_timeout = 0;
    std::string character_set;
};

// Complete GFIX operation result
struct GFixOperationResult {
    MaintenanceOperation operation_type;
    bool operation_successful = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::vector<std::string> messages;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    MaintenanceStatistics detailed_stats;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
    
    std::string generateOperationReport() const;
};

// Database maintenance statistics
struct MaintenanceStatistics {
    std::chrono::steady_clock::time_point operation_start;
    std::chrono::steady_clock::time_point operation_end;
    MaintenanceOperation operation_type;
    
    // Page-level statistics
    uint64_t total_pages_processed = 0;
    uint64_t data_pages_processed = 0;
    uint64_t index_pages_processed = 0;
    uint64_t blob_pages_processed = 0;
    uint64_t corrupt_pages_found = 0;
    uint64_t corrupt_pages_repaired = 0;
    
    // Record-level statistics
    uint64_t total_records_processed = 0;
    uint64_t fragmented_records_found = 0;
    uint64_t fragmented_records_fixed = 0;
    uint64_t orphaned_records_found = 0;
    uint64_t orphaned_records_removed = 0;
    
    // Index statistics
    uint64_t indexes_checked = 0;
    uint64_t indexes_rebuilt = 0;
    uint64_t index_errors_found = 0;
    uint64_t index_errors_fixed = 0;
    
    // Blob statistics
    uint64_t blobs_checked = 0;
    uint64_t blob_corruption_found = 0;
    uint64_t blob_corruption_fixed = 0;
    
    // Transaction statistics
    uint64_t limbo_transactions_found = 0;
    uint64_t limbo_transactions_resolved = 0;
    
    // Space reclamation
    uint64_t space_reclaimed_bytes = 0;
    uint64_t pages_released = 0;
    
    // Error tracking
    std::vector<std::string> errors_encountered;
    std::vector<std::string> warnings_generated;
    std::vector<std::string> repairs_performed;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(operation_end - operation_start);
    }
    
    std::string generateSummaryReport() const;
    std::string generateDetailedReport() const;
};

// Database validation result
struct ValidationResult {
    bool database_structurally_sound = true;
    bool data_integrity_intact = true;
    bool indexes_consistent = true;
    bool referential_integrity_valid = true;
    uint32_t total_errors_found = 0;
    uint32_t critical_errors_found = 0;
    uint32_t warnings_generated = 0;
    std::vector<std::string> error_details;
    std::vector<std::string> warning_details;
    std::vector<std::string> recommendations;
    MaintenanceStatistics detailed_stats;
    
    bool isDatabaseHealthy() const {
        return database_structurally_sound && 
               data_integrity_intact && 
               critical_errors_found == 0;
    }
    
    bool requiresImmediateAttention() const {
        return !database_structurally_sound || critical_errors_found > 0;
    }
};

// Database repair result
struct RepairResult {
    bool repair_successful = false;
    bool database_accessible_after_repair = false;
    uint32_t issues_found = 0;
    uint32_t issues_repaired = 0;
    uint32_t issues_unresolved = 0;
    std::vector<std::string> repair_actions_taken;
    std::vector<std::string> unresolved_issues;
    std::string backup_path_created;
    ValidationResult post_repair_validation;
    MaintenanceStatistics detailed_stats;
    
    double getRepairSuccessRate() const {
        if (issues_found == 0) return 100.0;
        return (static_cast<double>(issues_repaired) / issues_found) * 100.0;
    }
    
    bool isDatabaseUsable() const {
        return repair_successful && database_accessible_after_repair;
    }
};

} // namespace SBEnhanced

// Main enhanced GFIX utility class
class GFixEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> maintenance_service;
    std::atomic<bool> operation_active{false};
    SBEnhanced::MaintenanceProgress current_progress;
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    GFixEnhanced();
    ~GFixEnhanced();
    
    // Core maintenance operations
    bool performDatabaseValidation(const std::string& database_path,
                                  const SBEnhanced::ValidationOptions& options,
                                  SBEnhanced::ValidationResult& result);
    
    bool performDatabaseRepair(const std::string& database_path,
                              const SBEnhanced::RepairOptions& options,
                              SBEnhanced::RepairResult& result);
    
    bool performDatabaseSweep(const std::string& database_path,
                             const SBEnhanced::SweepOptions& options,
                             SBEnhanced::MaintenanceStatistics& stats);
    
    // Specialized maintenance operations
    bool rebuildIndexes(const std::string& database_path,
                       const std::vector<std::string>& index_names = {},
                       SBEnhanced::MaintenanceStatistics& stats);
    
    bool resolveLimboTransactions(const std::string& database_path,
                                 bool commit_limbo_transactions = false,
                                 SBEnhanced::MaintenanceStatistics& stats);
    
    bool updateDatabaseStatistics(const std::string& database_path,
                                 bool force_statistics_update = false,
                                 SBEnhanced::MaintenanceStatistics& stats);
    
    bool reclaimUnusedSpace(const std::string& database_path,
                           SBEnhanced::MaintenanceStatistics& stats);
    
    // Database analysis and diagnostics
    bool analyzeDatabaseHealth(const std::string& database_path,
                              SBEnhanced::ValidationResult& health_report);
    
    bool generatePerformanceRecommendations(const std::string& database_path,
                                           std::vector<std::string>& recommendations);
    
    bool estimateRepairTime(const std::string& database_path,
                           const SBEnhanced::RepairOptions& options,
                           std::chrono::minutes& estimated_duration);
    
    // Backup integration
    bool createPreRepairBackup(const std::string& database_path,
                              const std::string& backup_path);
    
    bool verifyBackupIntegrity(const std::string& backup_path);
    
    // === ORIGINAL GFIX FUNCTIONALITY (100% Compatible) ===
    
    // Database state management
    bool shutdownDatabase(const std::string& database_path,
                         const SBEnhanced::ShutdownOptions& options,
                         SBEnhanced::GFixOperationResult& result);
    
    bool bringDatabaseOnline(const std::string& database_path,
                            SBEnhanced::GFixOperationResult& result);
    
    bool setDatabaseAccessMode(const std::string& database_path,
                              SBEnhanced::DatabaseAccessMode mode,
                              SBEnhanced::GFixOperationResult& result);
    
    // Database configuration
    bool configureDatabaseSettings(const std::string& database_path,
                                  const SBEnhanced::DatabaseConfigOptions& options,
                                  SBEnhanced::GFixOperationResult& result);
    
    bool setPageBuffers(const std::string& database_path,
                       uint32_t buffer_count,
                       SBEnhanced::GFixOperationResult& result);
    
    bool setSweepInterval(const std::string& database_path,
                         uint32_t interval,
                         SBEnhanced::GFixOperationResult& result);
    
    bool setWriteMode(const std::string& database_path,
                     SBEnhanced::DatabaseWriteMode mode,
                     SBEnhanced::GFixOperationResult& result);
    
    bool setSpaceUsage(const std::string& database_path,
                      SBEnhanced::SpaceUsageMode mode,
                      SBEnhanced::GFixOperationResult& result);
    
    bool setSQLDialect(const std::string& database_path,
                      uint32_t dialect,
                      SBEnhanced::GFixOperationResult& result);
    
    bool setReplicaMode(const std::string& database_path,
                       SBEnhanced::ReplicaMode mode,
                       SBEnhanced::GFixOperationResult& result);
    
    // Transaction management (original GFIX)
    bool listLimboTransactions(const std::string& database_path,
                              std::vector<uint64_t>& transaction_ids,
                              SBEnhanced::GFixOperationResult& result);
    
    bool commitLimboTransaction(const std::string& database_path,
                               uint64_t transaction_id,
                               SBEnhanced::GFixOperationResult& result);
    
    bool rollbackLimboTransaction(const std::string& database_path,
                                 uint64_t transaction_id,
                                 SBEnhanced::GFixOperationResult& result);
    
    bool performTwoPhaseRecovery(const std::string& database_path,
                                const SBEnhanced::TransactionOptions& options,
                                SBEnhanced::GFixOperationResult& result);
    
    // Shadow file management
    bool manageShadowFiles(const std::string& database_path,
                          const SBEnhanced::ShadowOptions& options,
                          SBEnhanced::GFixOperationResult& result);
    
    bool activateShadowFile(const std::string& database_path,
                           SBEnhanced::GFixOperationResult& result);
    
    bool killShadowFiles(const std::string& database_path,
                        SBEnhanced::GFixOperationResult& result);
    
    // Extended validation (original GFIX modes)
    bool performExtendedValidation(const std::string& database_path,
                                  const SBEnhanced::ExtendedValidationOptions& options,
                                  SBEnhanced::ValidationResult& result);
    
    bool mendCorruptDatabase(const std::string& database_path,
                            SBEnhanced::GFixOperationResult& result);
    
    bool validateWithIgnoreChecksums(const std::string& database_path,
                                    SBEnhanced::ValidationResult& result);
    
    bool validateReadOnly(const std::string& database_path,
                         SBEnhanced::ValidationResult& result);
    
    bool validateRecordFragments(const std::string& database_path,
                                SBEnhanced::ValidationResult& result);
    
    // ICU and upgrade operations
    bool fixICUVersion(const std::string& database_path,
                      SBEnhanced::GFixOperationResult& result);
    
    bool upgradeDatabaseODS(const std::string& database_path,
                           SBEnhanced::GFixOperationResult& result);
    
    bool disableDatabaseLinger(const std::string& database_path,
                              SBEnhanced::GFixOperationResult& result);
    
    // Parallel processing support
    bool setParallelWorkers(const std::string& database_path,
                           uint32_t worker_count,
                           SBEnhanced::GFixOperationResult& result);
    
    // Authentication and connection management
    bool authenticateConnection(const std::string& database_path,
                               const SBEnhanced::ConnectionOptions& options);
    
    bool loadPasswordFromFile(const std::string& password_file,
                             std::string& password);
    
    bool testDatabaseConnection(const std::string& database_path,
                               const SBEnhanced::ConnectionOptions& options,
                               SBEnhanced::GFixOperationResult& result);
    
    // Progress monitoring
    SBEnhanced::MaintenanceProgress getCurrentProgress() const;
    bool isOperationActive() const;
    void cancelCurrentOperation();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Statistics and reporting
    std::string generateMaintenanceReport(const SBEnhanced::MaintenanceStatistics& stats) const;
    std::string generateValidationReport(const SBEnhanced::ValidationResult& result) const;
    std::string generateRepairReport(const SBEnhanced::RepairResult& result) const;
    
    // Database information
    bool getDatabaseInfo(const std::string& database_path,
                        std::map<std::string, std::string>& database_info);
    
    bool getDatabaseStatistics(const std::string& database_path,
                              std::map<std::string, uint64_t>& statistics);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeMaintenanceService(const std::string& database_path);
    
    // Internal validation helpers
    bool validateDatabaseStructure(const std::string& database_path,
                                  const SBEnhanced::ValidationOptions& options,
                                  SBEnhanced::ValidationResult& result);
    
    bool validateDataIntegrity(const std::string& database_path,
                              const SBEnhanced::ValidationOptions& options,
                              SBEnhanced::ValidationResult& result);
    
    bool validateIndexConsistency(const std::string& database_path,
                                 const SBEnhanced::ValidationOptions& options,
                                 SBEnhanced::ValidationResult& result);
    
    // Internal repair helpers
    bool repairDatabaseStructure(const std::string& database_path,
                                const SBEnhanced::RepairOptions& options,
                                SBEnhanced::RepairResult& result);
    
    bool repairDataCorruption(const std::string& database_path,
                             const SBEnhanced::RepairOptions& options,
                             SBEnhanced::RepairResult& result);
    
    bool repairIndexCorruption(const std::string& database_path,
                              const SBEnhanced::RepairOptions& options,
                              SBEnhanced::RepairResult& result);
    
    // Internal utility helpers
    void updateProgress(SBEnhanced::MaintenanceOperation operation,
                       uint64_t processed, uint64_t total,
                       const std::string& current_object);
    
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    
    // Database connection management
    bool connectToDatabase(const std::string& database_path, bool exclusive_access = false);
    void disconnectFromDatabase();
    
    // Service integration helpers
    bool startMaintenanceService(SBEnhanced::MaintenanceOperation operation);
    void stopMaintenanceService();
    bool isMaintenanceServiceActive() const;
    
    // Progress callback management
    void notifyProgress(const std::function<void(const SBEnhanced::MaintenanceProgress&)>& callback);
    
    // Statistics collection helpers
    void collectPageStatistics(SBEnhanced::MaintenanceStatistics& stats);
    void collectRecordStatistics(SBEnhanced::MaintenanceStatistics& stats);
    void collectIndexStatistics(SBEnhanced::MaintenanceStatistics& stats);
    void collectBlobStatistics(SBEnhanced::MaintenanceStatistics& stats);
    void collectTransactionStatistics(SBEnhanced::MaintenanceStatistics& stats);
};

// Utility functions for enhanced GFIX
namespace SBEnhanced {

// Quick validation function
ValidationResult quickValidation(const std::string& database_path);

// Quick repair function with conservative settings
RepairResult quickRepair(const std::string& database_path);

// Database health check
bool isDatabaseHealthy(const std::string& database_path);

// Maintenance scheduling helpers
std::chrono::system_clock::time_point getOptimalMaintenanceTime();
bool shouldPerformMaintenance(const std::string& database_path);
uint32_t estimateMaintenanceDuration(const std::string& database_path, 
                                    MaintenanceOperation operation);

// Compatibility helpers for command-line usage
int parseGFixCommandLine(int argc, char* argv[], 
                        std::string& database_path,
                        ValidationOptions& validation_opts,
                        RepairOptions& repair_opts,
                        SweepOptions& sweep_opts);

bool executeClassicGFixCommand(const std::string& command_line);

} // namespace SBEnhanced