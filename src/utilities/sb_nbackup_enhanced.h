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

// Backup levels for incremental backups
enum class BackupLevel {
    FULL = 0,           // Full backup (level 0)
    INCREMENTAL_1 = 1,  // First incremental level
    INCREMENTAL_2 = 2,  // Second incremental level
    INCREMENTAL_3 = 3,  // Third incremental level
    INCREMENTAL_4 = 4,  // Fourth incremental level
    INCREMENTAL_5 = 5,  // Fifth incremental level
    INCREMENTAL_6 = 6,  // Sixth incremental level
    INCREMENTAL_7 = 7,  // Seventh incremental level
    INCREMENTAL_8 = 8,  // Eighth incremental level
    INCREMENTAL_9 = 9,  // Ninth incremental level (maximum)
    AUTO = 255          // Automatically determine level
};

// Backup operation types
enum class BackupOperation {
    BACKUP = 0,         // Create backup
    RESTORE = 1,        // Restore from backup
    MERGE = 2,          // Merge backup levels
    VALIDATE = 3,       // Validate backup chain
    ANALYZE = 4,        // Analyze backup files
    CLEANUP = 5,        // Clean up old backups
    LIST = 6,           // List backup information
    COPY = 7            // Copy backup files
};

// Backup file types
enum class BackupFileType {
    UNKNOWN = 0,
    FULL_BACKUP = 1,        // Full database backup
    INCREMENTAL_BACKUP = 2, // Incremental backup
    DELTA_FILE = 3,         // Delta changes file
    MERGED_BACKUP = 4,      // Merged backup file
    TEMPORARY_FILE = 5,     // Temporary work file
    METADATA_FILE = 6       // Backup metadata file
};

// Backup compression algorithms
enum class BackupCompression {
    NONE = 0,           // No compression
    GZIP = 1,           // GZIP compression
    LZ4 = 2,            // LZ4 fast compression
    ZSTD = 3,           // ZSTD balanced compression
    BZIP2 = 4,          // BZIP2 high compression
    AUTO = 255          // Automatically select best algorithm
};

// Backup verification levels
enum class VerificationLevel {
    NONE = 0,           // No verification
    BASIC = 1,          // Basic file integrity check
    CHECKSUM = 2,       // Checksum verification
    STRUCTURAL = 3,     // Database structure verification
    COMPREHENSIVE = 4,  // Full comprehensive verification
    FORENSIC = 5        // Forensic-level verification
};

// Backup merge strategies
enum class MergeStrategy {
    CONSERVATIVE = 0,   // Conservative merge (safe)
    AGGRESSIVE = 1,     // Aggressive merge (fast)
    OPTIMAL = 2,        // Optimal merge (balanced)
    CUSTOM = 3          // Custom merge parameters
};

// Backup information structure
struct BackupInfo {
    std::string backup_path;
    std::string database_path;
    BackupLevel level = BackupLevel::FULL;
    BackupFileType file_type = BackupFileType::UNKNOWN;
    BackupCompression compression = BackupCompression::NONE;
    
    std::chrono::system_clock::time_point creation_time;
    std::chrono::system_clock::time_point modification_time;
    uint64_t file_size = 0;
    uint64_t compressed_size = 0;
    uint64_t uncompressed_size = 0;
    
    std::string checksum_md5;
    std::string checksum_sha256;
    std::string backup_guid;
    std::string database_guid;
    std::string parent_backup_guid;
    
    uint32_t backup_version = 0;
    uint32_t database_ods_version = 0;
    uint32_t pages_backed_up = 0;
    uint32_t pages_changed = 0;
    double compression_ratio = 0.0;
    
    bool is_valid = false;
    bool is_encrypted = false;
    bool has_metadata = false;
    bool requires_parent = false;
    
    std::vector<std::string> dependencies;  // Required parent backups
    std::vector<std::string> children;      // Dependent child backups
    std::map<std::string, std::string> metadata;
    
    std::string getBackupLevelString() const;
    std::string getCompressionString() const;
    std::string getFileTypeString() const;
    bool isIncremental() const { return level != BackupLevel::FULL; }
    uint64_t getCompressionSavings() const;
};

// Backup chain information
struct BackupChain {
    std::string chain_id;
    std::string database_path;
    std::vector<BackupInfo> backups;
    
    std::chrono::system_clock::time_point chain_start_time;
    std::chrono::system_clock::time_point chain_end_time;
    uint64_t total_size = 0;
    uint64_t total_compressed_size = 0;
    uint32_t total_levels = 0;
    
    bool is_complete = false;
    bool is_valid = false;
    std::vector<std::string> missing_levels;
    std::vector<std::string> validation_errors;
    
    BackupInfo* getFullBackup();
    std::vector<BackupInfo*> getIncrementalBackups();
    BackupInfo* getBackupAtLevel(BackupLevel level);
    bool validateChainIntegrity();
    std::chrono::milliseconds getChainDuration() const;
};

// Backup options
struct BackupOptions {
    std::string database_path;
    std::string backup_path;
    BackupLevel level = BackupLevel::FULL;
    BackupCompression compression = BackupCompression::AUTO;
    
    // Incremental backup options
    std::string parent_backup_path;
    bool auto_detect_parent = true;
    bool create_backup_chain = false;
    uint32_t max_chain_length = 10;
    
    // Performance options
    bool parallel_processing = true;
    uint32_t worker_threads = 4;
    uint32_t buffer_size_mb = 64;
    uint32_t checkpoint_interval = 1000;  // Pages
    
    // Verification options
    VerificationLevel verification = VerificationLevel::BASIC;
    bool generate_checksum = true;
    bool verify_after_backup = true;
    bool test_restore = false;
    
    // Advanced options
    bool lock_database = false;
    bool direct_io = false;
    bool preserve_timestamps = true;
    bool include_metadata = true;
    bool encrypt_backup = false;
    std::string encryption_key;
    
    // Filtering options
    std::vector<std::string> include_tables;
    std::vector<std::string> exclude_tables;
    std::vector<std::string> include_schemas;
    std::vector<std::string> exclude_schemas;
    
    // Progress and logging
    std::function<void(const std::string&, double)> progress_callback;
    std::function<void(const std::string&)> log_callback;
    bool verbose_output = false;
    bool quiet_mode = false;
    
    // Cleanup options
    bool cleanup_on_error = true;
    bool cleanup_temp_files = true;
    uint32_t retention_days = 30;
    uint32_t max_backup_count = 50;
};

// Restore options
struct RestoreOptions {
    std::string backup_path;
    std::string database_path;
    BackupLevel restore_to_level = BackupLevel::FULL;
    
    // Restore strategy
    bool restore_full_chain = true;
    bool auto_merge_levels = false;
    bool validate_before_restore = true;
    bool create_database = true;
    bool overwrite_existing = false;
    
    // Performance options
    bool parallel_processing = true;
    uint32_t worker_threads = 4;
    uint32_t buffer_size_mb = 64;
    bool direct_io = false;
    
    // Advanced options
    bool restore_metadata_only = false;
    bool restore_data_only = false;
    bool skip_corrupted_pages = false;
    bool fix_minor_corruption = false;
    
    // Filtering options
    std::vector<std::string> include_tables;
    std::vector<std::string> exclude_tables;
    std::vector<std::string> include_schemas;
    std::vector<std::string> exclude_schemas;
    
    // Progress and logging
    std::function<void(const std::string&, double)> progress_callback;
    std::function<void(const std::string&)> log_callback;
    bool verbose_output = false;
    bool quiet_mode = false;
    
    // Recovery options
    std::string target_time;  // Point-in-time recovery
    std::string target_transaction;  // Transaction-level recovery
    bool allow_partial_restore = false;
};

// Merge options
struct MergeOptions {
    std::vector<std::string> backup_paths;
    std::string output_path;
    MergeStrategy strategy = MergeStrategy::OPTIMAL;
    
    // Merge parameters
    BackupLevel target_level = BackupLevel::FULL;
    BackupCompression output_compression = BackupCompression::AUTO;
    bool verify_inputs = true;
    bool verify_output = true;
    bool preserve_metadata = true;
    
    // Performance options
    bool parallel_processing = true;
    uint32_t worker_threads = 4;
    uint32_t buffer_size_mb = 128;
    bool use_temp_space = true;
    std::string temp_directory;
    
    // Advanced options
    bool optimize_output = true;
    bool remove_redundancy = true;
    bool compact_output = false;
    double compression_threshold = 0.1;  // 10% minimum compression
    
    // Progress and logging
    std::function<void(const std::string&, double)> progress_callback;
    std::function<void(const std::string&)> log_callback;
    bool verbose_output = false;
    bool quiet_mode = false;
    
    // Cleanup options
    bool cleanup_inputs = false;
    bool cleanup_on_error = true;
    bool cleanup_temp_files = true;
};

// Validation options
struct ValidationOptions {
    std::vector<std::string> backup_paths;
    VerificationLevel verification_level = VerificationLevel::COMPREHENSIVE;
    
    // Validation scope
    bool validate_file_integrity = true;
    bool validate_backup_chain = true;
    bool validate_database_structure = true;
    bool validate_data_consistency = false;
    bool check_dependencies = true;
    
    // Performance options
    bool parallel_validation = true;
    uint32_t worker_threads = 2;
    bool quick_validation = false;
    bool deep_validation = false;
    
    // Reporting options
    bool generate_report = true;
    std::string report_path;
    std::string report_format = "TEXT";
    bool detailed_errors = true;
    
    // Progress and logging
    std::function<void(const std::string&, double)> progress_callback;
    std::function<void(const std::string&)> log_callback;
    bool verbose_output = false;
    bool quiet_mode = false;
};

// Backup analysis result
struct BackupAnalysisResult {
    std::chrono::system_clock::time_point analysis_time;
    bool analysis_successful = false;
    
    // File analysis
    std::vector<BackupInfo> analyzed_backups;
    std::vector<BackupChain> backup_chains;
    uint64_t total_backup_size = 0;
    uint64_t total_compressed_size = 0;
    uint32_t total_backup_count = 0;
    
    // Chain analysis
    uint32_t complete_chains = 0;
    uint32_t incomplete_chains = 0;
    uint32_t broken_chains = 0;
    std::vector<std::string> orphaned_backups;
    
    // Space analysis
    double average_compression_ratio = 0.0;
    uint64_t potential_space_savings = 0;
    std::vector<std::string> optimization_opportunities;
    
    // Performance analysis
    std::vector<std::string> performance_recommendations;
    std::vector<std::string> maintenance_recommendations;
    
    // Issues and errors
    std::vector<std::string> critical_issues;
    std::vector<std::string> warnings;
    std::vector<std::string> analysis_errors;
    
    std::string analysis_report_path;
    
    std::string generateAnalysisReport() const;
    std::string generateRecommendationsReport() const;
};

// Backup operation progress
struct BackupProgress {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point current_time;
    
    BackupOperation operation = BackupOperation::BACKUP;
    std::string current_file;
    std::string current_operation;
    
    uint64_t total_bytes = 0;
    uint64_t processed_bytes = 0;
    uint32_t total_pages = 0;
    uint32_t processed_pages = 0;
    uint32_t total_files = 0;
    uint32_t processed_files = 0;
    
    double completion_percentage = 0.0;
    std::chrono::milliseconds estimated_remaining{0};
    double processing_rate_mbps = 0.0;
    
    bool operation_active = false;
    std::string current_phase;
    
    std::chrono::seconds getElapsedTime() const;
    std::chrono::milliseconds getEstimatedTimeRemaining() const;
    double getProcessingRateMBps() const;
    std::string getProgressSummary() const;
};

// Backup operation result
struct BackupOperationResult {
    BackupOptions backup_options;
    BackupProgress operation_progress;
    
    bool operation_successful = false;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Operation results
    std::vector<std::string> created_files;
    std::vector<std::string> processed_files;
    uint64_t total_bytes_processed = 0;
    uint64_t total_bytes_written = 0;
    uint32_t pages_backed_up = 0;
    double compression_ratio = 0.0;
    
    // Performance metrics
    double average_throughput_mbps = 0.0;
    double peak_throughput_mbps = 0.0;
    std::chrono::milliseconds total_duration{0};
    std::chrono::milliseconds io_time{0};
    std::chrono::milliseconds compression_time{0};
    
    // Quality metrics
    bool backup_verified = false;
    bool checksum_validated = false;
    VerificationLevel verification_performed = VerificationLevel::NONE;
    
    // Issues and warnings
    std::vector<std::string> operation_warnings;
    std::vector<std::string> operation_errors;
    std::vector<std::string> performance_issues;
    
    std::string operation_report_path;
    
    std::chrono::milliseconds getOperationDuration() const;
    std::string generateOperationReport() const;
};

} // namespace SBEnhanced

// Main enhanced NBackup utility class
class NBackupEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> backup_service;
    std::atomic<bool> operation_active{false};
    std::atomic<bool> cancel_requested{false};
    
    // Operation thread management
    std::unique_ptr<std::thread> operation_thread;
    std::mutex progress_mutex;
    std::mutex backup_data_mutex;
    
    // Data storage
    std::vector<SBEnhanced::BackupInfo> known_backups;
    std::vector<SBEnhanced::BackupChain> backup_chains;
    SBEnhanced::BackupProgress current_progress;
    
    // Internal state
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    NBackupEnhanced();
    ~NBackupEnhanced();
    
    // === ORIGINAL NBACKUP FUNCTIONALITY (100% Compatible) ===
    
    // Basic incremental backup operations
    bool createBackup(const std::string& database_path, 
                     const std::string& backup_path,
                     int level = 0);
    
    bool restoreBackup(const std::string& backup_path,
                      const std::string& database_path);
    
    bool mergeBackups(const std::vector<std::string>& backup_files,
                     const std::string& output_file);
    
    // === ENHANCED FUNCTIONALITY ===
    
    // Advanced backup operations
    bool performBackup(const SBEnhanced::BackupOptions& options,
                      SBEnhanced::BackupOperationResult& result);
    
    bool performRestore(const SBEnhanced::RestoreOptions& options,
                       SBEnhanced::BackupOperationResult& result);
    
    bool performMerge(const SBEnhanced::MergeOptions& options,
                     SBEnhanced::BackupOperationResult& result);
    
    // Backup analysis and validation
    bool validateBackups(const SBEnhanced::ValidationOptions& options,
                        std::vector<std::string>& validation_results);
    
    bool analyzeBackups(const std::vector<std::string>& backup_paths,
                       SBEnhanced::BackupAnalysisResult& analysis_result);
    
    bool validateBackupChain(const std::vector<std::string>& backup_files,
                            std::vector<std::string>& chain_errors);
    
    // Backup information queries
    bool getBackupInfo(const std::string& backup_path,
                      SBEnhanced::BackupInfo& backup_info);
    
    bool listBackups(const std::string& directory_path,
                    std::vector<SBEnhanced::BackupInfo>& backup_list);
    
    bool findBackupChains(const std::string& directory_path,
                         std::vector<SBEnhanced::BackupChain>& chains);
    
    bool getBackupDependencies(const std::string& backup_path,
                              std::vector<std::string>& dependencies);
    
    // Advanced backup management
    bool createBackupChain(const std::string& database_path,
                          const std::string& backup_directory,
                          const SBEnhanced::BackupOptions& options);
    
    bool optimizeBackupChain(const std::vector<std::string>& backup_files,
                            const std::string& output_directory);
    
    bool cleanupOldBackups(const std::string& backup_directory,
                          uint32_t retention_days,
                          uint32_t max_backup_count);
    
    bool repairBackupChain(const std::vector<std::string>& backup_files,
                          const std::string& repair_directory);
    
    // Compression and encryption
    bool compressBackup(const std::string& backup_path,
                       SBEnhanced::BackupCompression algorithm,
                       const std::string& output_path);
    
    bool decompressBackup(const std::string& compressed_backup_path,
                         const std::string& output_path);
    
    bool encryptBackup(const std::string& backup_path,
                      const std::string& encryption_key,
                      const std::string& output_path);
    
    bool decryptBackup(const std::string& encrypted_backup_path,
                      const std::string& encryption_key,
                      const std::string& output_path);
    
    // Backup verification and testing
    bool verifyBackup(const std::string& backup_path,
                     SBEnhanced::VerificationLevel level);
    
    bool testRestore(const std::string& backup_path,
                    const std::string& test_directory);
    
    bool compareBackups(const std::string& backup1_path,
                       const std::string& backup2_path,
                       std::vector<std::string>& differences);
    
    // Performance monitoring and optimization
    bool enablePerformanceMonitoring(bool enable);
    
    bool getPerformanceMetrics(std::map<std::string, double>& metrics);
    
    bool generatePerformanceReport(const std::string& output_path);
    
    bool getOptimizationRecommendations(const std::string& backup_directory,
                                       std::vector<std::string>& recommendations);
    
    // Export and reporting
    bool exportBackupInventory(const std::string& backup_directory,
                              const std::string& export_format,
                              const std::string& output_path);
    
    bool generateBackupReport(const std::vector<std::string>& backup_paths,
                             const std::string& report_format,
                             const std::string& output_path);
    
    bool createBackupDashboard(const std::string& backup_directory,
                              const std::string& output_path);
    
    // Progress monitoring and control
    SBEnhanced::BackupProgress getCurrentProgress() const;
    bool isOperationActive() const;
    void requestCancel();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Configuration and validation
    bool validateDatabaseAccess(const std::string& database_path);
    bool testBackupService();

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeBackupService();
    
    // Core backup operations
    bool performIncrementalBackup(const std::string& database_path,
                                 const std::string& backup_path,
                                 SBEnhanced::BackupLevel level,
                                 const SBEnhanced::BackupOptions& options);
    
    bool performFullBackup(const std::string& database_path,
                          const std::string& backup_path,
                          const SBEnhanced::BackupOptions& options);
    
    bool restoreFromBackupChain(const std::vector<std::string>& backup_files,
                               const std::string& database_path,
                               const SBEnhanced::RestoreOptions& options);
    
    // Backup analysis algorithms
    bool analyzeBackupFile(const std::string& backup_path,
                          SBEnhanced::BackupInfo& backup_info);
    
    bool buildBackupChains(const std::vector<SBEnhanced::BackupInfo>& backups,
                          std::vector<SBEnhanced::BackupChain>& chains);
    
    bool validateFileIntegrity(const std::string& backup_path,
                              SBEnhanced::VerificationLevel level);
    
    // Compression and optimization
    bool compressBackupData(const std::vector<uint8_t>& input_data,
                           std::vector<uint8_t>& compressed_data,
                           SBEnhanced::BackupCompression algorithm);
    
    bool decompressBackupData(const std::vector<uint8_t>& compressed_data,
                             std::vector<uint8_t>& output_data,
                             SBEnhanced::BackupCompression algorithm);
    
    bool optimizeBackupLayout(const std::string& backup_path,
                             const std::string& optimized_path);
    
    // Operation management
    void backupOperationLoop(const SBEnhanced::BackupOptions& options,
                            SBEnhanced::BackupOperationResult& result);
    
    void restoreOperationLoop(const SBEnhanced::RestoreOptions& options,
                             SBEnhanced::BackupOperationResult& result);
    
    void mergeOperationLoop(const SBEnhanced::MergeOptions& options,
                           SBEnhanced::BackupOperationResult& result);
    
    // Progress tracking helpers
    void updateProgress(const std::string& operation, 
                       const std::string& current_file,
                       double completion_percentage);
    
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    void logInfo(const std::string& operation, const std::string& info);
    
    // File and path management
    bool createBackupDirectory(const std::string& directory_path);
    bool validateBackupPath(const std::string& backup_path);
    std::string generateBackupFilename(const std::string& database_path,
                                      SBEnhanced::BackupLevel level,
                                      const std::chrono::system_clock::time_point& timestamp);
    
    // Database connection helpers
    bool establishDatabaseConnection(const std::string& database_path,
                                   std::unique_ptr<jrd::Attachment>& attachment);
    
    void closeDatabaseConnection(std::unique_ptr<jrd::Attachment>& attachment);
    
    // Backup service interaction
    bool startBackupService(const SBEnhanced::BackupOptions& options,
                           std::unique_ptr<jrd::Service>& service);
    
    bool monitorBackupProgress(jrd::Service* service,
                              SBEnhanced::BackupProgress& progress);
};

// Utility functions for enhanced NBackup
namespace SBEnhanced {

// Quick backup operations
bool quickCreateBackup(const std::string& database_path,
                      const std::string& backup_path,
                      BackupLevel level = BackupLevel::FULL);

bool quickRestoreBackup(const std::string& backup_path,
                       const std::string& database_path);

bool quickValidateBackup(const std::string& backup_path);

// Backup information helpers
std::string formatBackupSize(uint64_t size_bytes);
std::string formatBackupDuration(const std::chrono::milliseconds& duration);
double calculateCompressionRatio(uint64_t original_size, uint64_t compressed_size);

// Backup chain utilities
bool isValidBackupChain(const std::vector<BackupInfo>& backups);
std::vector<std::string> getRequiredBackups(const BackupInfo& target_backup);
BackupLevel getNextIncrementalLevel(const std::vector<BackupInfo>& existing_backups);

// File utilities
bool copyBackupFile(const std::string& source_path, const std::string& dest_path);
std::string calculateFileChecksum(const std::string& file_path, const std::string& algorithm);
bool validateFileChecksum(const std::string& file_path, const std::string& expected_checksum);

// Export utilities
bool exportToCSV(const std::vector<BackupInfo>& backups, const std::string& filename);
bool exportToJSON(const std::vector<BackupInfo>& backups, const std::string& filename);
bool exportToXML(const std::vector<BackupInfo>& backups, const std::string& filename);

// Compatibility helpers for command-line usage
int parseNBackupCommandLine(int argc, char* argv[],
                           std::string& database_path,
                           std::string& backup_path,
                           BackupOptions& backup_opts);

bool executeClassicNBackupCommand(const std::string& command_line);

} // namespace SBEnhanced