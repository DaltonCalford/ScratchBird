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

// Split operation types
enum class SplitOperation {
    SPLIT_DATABASE = 0,
    JOIN_DATABASE = 1,
    VALIDATE_SPLIT = 2,
    ANALYZE_SPLIT = 3,
    TEST_SPLIT = 4
};

// Split file validation modes
enum class SplitValidationMode {
    NONE = 0,               // No validation
    BASIC = 1,              // Basic file existence and size checks
    CHECKSUM = 2,           // Checksum validation
    COMPREHENSIVE = 3,      // Comprehensive validation
    FORENSIC = 4            // Forensic-level validation
};

// Split compression types
enum class SplitCompressionType {
    NONE = 0,               // No compression
    GZIP = 1,               // GZIP compression
    LZ4 = 2,                // LZ4 compression
    ZSTD = 3,               // ZSTD compression
    BZIP2 = 4               // BZIP2 compression
};

// Split progress tracking
struct SplitProgress {
    SplitOperation current_operation = SplitOperation::SPLIT_DATABASE;
    uint64_t total_bytes = 0;
    uint64_t processed_bytes = 0;
    uint64_t files_processed = 0;
    uint64_t total_files = 0;
    std::string current_file;
    std::chrono::steady_clock::time_point start_time;
    bool operation_active = false;
    
    double getProgressPercentage() const {
        if (total_bytes == 0) return 0.0;
        return (static_cast<double>(processed_bytes) / total_bytes) * 100.0;
    }
    
    std::chrono::seconds getElapsedTime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    }
    
    std::chrono::seconds getEstimatedTimeRemaining() const {
        if (processed_bytes == 0) return std::chrono::seconds(0);
        auto elapsed = getElapsedTime();
        double progress = getProgressPercentage() / 100.0;
        if (progress <= 0.0) return std::chrono::seconds(0);
        double total_time = elapsed.count() / progress;
        return std::chrono::seconds(static_cast<long>(total_time - elapsed.count()));
    }
};

// Split file information
struct SplitFileInfo {
    std::string file_path;
    uint64_t file_size = 0;
    uint64_t sequence_number = 0;
    std::string checksum;
    SplitCompressionType compression = SplitCompressionType::NONE;
    double compression_ratio = 1.0;
    std::chrono::system_clock::time_point created_time;
    std::string description;
    std::map<std::string, std::string> metadata;
    
    bool isValid() const {
        return !file_path.empty() && file_size > 0;
    }
    
    bool isCompressed() const {
        return compression != SplitCompressionType::NONE;
    }
};

// Split operation options
struct SplitOptions {
    std::string source_database_path;
    std::string output_directory;
    std::string output_filename_prefix = "split_db";
    uint64_t max_file_size = 2147483648ULL;  // 2GB default
    SplitCompressionType compression = SplitCompressionType::NONE;
    int compression_level = 6;
    bool create_checksum = true;
    bool verify_after_split = true;
    bool preserve_original = true;
    bool overwrite_existing = false;
    std::string description;
    std::map<std::string, std::string> custom_metadata;
    std::function<void(const SplitProgress&)> progress_callback;
};

// Join operation options
struct JoinOptions {
    std::vector<std::string> split_file_paths;
    std::string output_database_path;
    SplitValidationMode validation_mode = SplitValidationMode::COMPREHENSIVE;
    bool verify_checksums = true;
    bool verify_sequence = true;
    bool verify_after_join = true;
    bool overwrite_existing = false;
    bool preserve_split_files = true;
    std::function<void(const SplitProgress&)> progress_callback;
};

// Split validation options
struct SplitValidationOptions {
    std::vector<std::string> split_file_paths;
    SplitValidationMode validation_mode = SplitValidationMode::COMPREHENSIVE;
    bool check_file_existence = true;
    bool check_file_sizes = true;
    bool verify_checksums = true;
    bool verify_sequence = true;
    bool verify_metadata = true;
    bool generate_report = true;
    std::string report_output_path;
    std::function<void(const SplitProgress&)> progress_callback;
};

// Split analysis options
struct SplitAnalysisOptions {
    std::vector<std::string> split_file_paths;
    bool analyze_compression_efficiency = true;
    bool analyze_file_distribution = true;
    bool analyze_storage_efficiency = true;
    bool generate_detailed_report = true;
    bool include_performance_metrics = true;
    std::string analysis_output_path;
    std::function<void(const SplitProgress&)> progress_callback;
};

// Split statistics
struct SplitStatistics {
    std::chrono::steady_clock::time_point operation_start;
    std::chrono::steady_clock::time_point operation_end;
    SplitOperation operation_type;
    
    // Split statistics
    uint64_t total_source_size = 0;
    uint64_t total_split_size = 0;
    uint64_t files_created = 0;
    double average_file_size = 0.0;
    double compression_ratio = 1.0;
    uint64_t total_bytes_processed = 0;
    
    // Join statistics
    uint64_t files_joined = 0;
    uint64_t total_joined_size = 0;
    double decompression_ratio = 1.0;
    
    // Validation statistics
    uint64_t files_validated = 0;
    uint64_t validation_errors = 0;
    uint64_t checksum_mismatches = 0;
    uint64_t sequence_errors = 0;
    uint64_t metadata_errors = 0;
    
    // Performance statistics
    double throughput_mbps = 0.0;
    double compression_time_ms = 0.0;
    double validation_time_ms = 0.0;
    
    // Error tracking
    std::vector<std::string> errors_encountered;
    std::vector<std::string> warnings_generated;
    std::vector<std::string> actions_performed;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(operation_end - operation_start);
    }
    
    std::string generateSummaryReport() const;
    std::string generateDetailedReport() const;
};

// Split operation result
struct SplitOperationResult {
    SplitOperation operation_type;
    bool operation_successful = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::vector<std::string> messages;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<SplitFileInfo> split_files;
    SplitStatistics detailed_stats;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
    
    std::string generateOperationReport() const;
    bool hasErrors() const { return !errors.empty(); }
    bool hasWarnings() const { return !warnings.empty(); }
};

// Split validation result
struct SplitValidationResult {
    bool validation_successful = false;
    uint64_t files_validated = 0;
    uint64_t validation_errors = 0;
    uint64_t checksum_failures = 0;
    uint64_t sequence_errors = 0;
    uint64_t metadata_errors = 0;
    std::vector<std::string> error_details;
    std::vector<std::string> warnings;
    std::vector<SplitFileInfo> validated_files;
    SplitStatistics validation_stats;
    std::string validation_report_path;
    
    bool hasValidationErrors() const {
        return validation_errors > 0 || checksum_failures > 0 || sequence_errors > 0;
    }
    
    bool requiresAttention() const {
        return validation_errors > 0 || checksum_failures > 0;
    }
    
    std::string generateValidationReport() const;
};

// Split analysis result
struct SplitAnalysisResult {
    bool analysis_successful = false;
    uint64_t files_analyzed = 0;
    double overall_compression_ratio = 1.0;
    double storage_efficiency = 0.0;
    uint64_t total_wasted_space = 0;
    std::vector<std::string> optimization_recommendations;
    std::vector<std::string> analysis_details;
    std::vector<SplitFileInfo> analyzed_files;
    SplitStatistics analysis_stats;
    std::string analysis_report_path;
    
    std::string generateAnalysisReport() const;
};

} // namespace SBEnhanced

// Main enhanced GSSPLIT utility class
class GSplitEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> split_service;
    std::atomic<bool> operation_active{false};
    SBEnhanced::SplitProgress current_progress;
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    GSplitEnhanced();
    ~GSplitEnhanced();
    
    // === ORIGINAL GSSPLIT FUNCTIONALITY (100% Compatible) ===
    
    // Database splitting operations
    bool splitDatabase(const std::string& database_path,
                      const std::string& output_prefix,
                      uint64_t max_file_size,
                      SBEnhanced::SplitOperationResult& result);
    
    bool joinDatabase(const std::vector<std::string>& split_files,
                     const std::string& output_database,
                     SBEnhanced::SplitOperationResult& result);
    
    // === ENHANCED FUNCTIONALITY ===
    
    // Advanced split operations
    bool performSplit(const SBEnhanced::SplitOptions& options,
                     SBEnhanced::SplitOperationResult& result);
    
    bool performJoin(const SBEnhanced::JoinOptions& options,
                    SBEnhanced::SplitOperationResult& result);
    
    // Split validation and verification
    bool validateSplitFiles(const SBEnhanced::SplitValidationOptions& options,
                           SBEnhanced::SplitValidationResult& result);
    
    bool verifySplitIntegrity(const std::vector<std::string>& split_files,
                             SBEnhanced::SplitValidationResult& result);
    
    // Split analysis and optimization
    bool analyzeSplitFiles(const SBEnhanced::SplitAnalysisOptions& options,
                          SBEnhanced::SplitAnalysisResult& result);
    
    bool optimizeSplitConfiguration(const std::string& database_path,
                                   uint64_t target_file_size,
                                   std::vector<std::string>& recommendations);
    
    // Compression management
    bool compressSplitFiles(const std::vector<std::string>& split_files,
                           SBEnhanced::SplitCompressionType compression,
                           int compression_level = 6);
    
    bool decompressSplitFiles(const std::vector<std::string>& compressed_files,
                             const std::string& output_directory);
    
    // Metadata and information
    bool getSplitFileInfo(const std::string& split_file_path,
                         SBEnhanced::SplitFileInfo& file_info);
    
    bool getSplitSetInfo(const std::vector<std::string>& split_files,
                        std::vector<SBEnhanced::SplitFileInfo>& files_info);
    
    // Checksum operations
    bool generateChecksums(const std::vector<std::string>& split_files,
                          std::map<std::string, std::string>& checksums);
    
    bool verifyChecksums(const std::vector<std::string>& split_files,
                        const std::map<std::string, std::string>& expected_checksums,
                        std::vector<std::string>& checksum_failures);
    
    // Test operations
    bool testSplitOperation(const std::string& database_path,
                           const SBEnhanced::SplitOptions& options,
                           bool perform_actual_split = false);
    
    bool testJoinOperation(const std::vector<std::string>& split_files,
                          const SBEnhanced::JoinOptions& options,
                          bool perform_actual_join = false);
    
    // Progress monitoring
    SBEnhanced::SplitProgress getCurrentProgress() const;
    bool isOperationActive() const;
    void cancelCurrentOperation();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Statistics and reporting
    std::string generateSplitReport(const SBEnhanced::SplitStatistics& stats) const;
    std::string generateValidationReport(const SBEnhanced::SplitValidationResult& result) const;
    std::string generateAnalysisReport(const SBEnhanced::SplitAnalysisResult& result) const;
    
    // Utility functions
    bool estimateSplitSize(const std::string& database_path,
                          uint64_t max_file_size,
                          uint64_t& estimated_files,
                          uint64_t& estimated_total_size);
    
    bool getDatabaseInfo(const std::string& database_path,
                        std::map<std::string, std::string>& database_info);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeSplitService();
    
    // Internal split operations
    bool performActualSplit(const SBEnhanced::SplitOptions& options,
                           SBEnhanced::SplitOperationResult& result);
    
    bool performActualJoin(const SBEnhanced::JoinOptions& options,
                          SBEnhanced::SplitOperationResult& result);
    
    // File handling helpers
    bool readFileChunk(const std::string& file_path,
                      uint64_t offset,
                      uint64_t size,
                      std::vector<uint8_t>& buffer);
    
    bool writeFileChunk(const std::string& file_path,
                       const std::vector<uint8_t>& buffer,
                       bool append = false);
    
    // Compression helpers
    bool compressBuffer(const std::vector<uint8_t>& input,
                       std::vector<uint8_t>& output,
                       SBEnhanced::SplitCompressionType compression,
                       int level = 6);
    
    bool decompressBuffer(const std::vector<uint8_t>& input,
                         std::vector<uint8_t>& output,
                         SBEnhanced::SplitCompressionType compression);
    
    // Checksum helpers
    std::string calculateFileChecksum(const std::string& file_path,
                                     const std::string& algorithm = "SHA256");
    
    bool verifyFileChecksum(const std::string& file_path,
                           const std::string& expected_checksum,
                           const std::string& algorithm = "SHA256");
    
    // Validation helpers
    bool validateFileSequence(const std::vector<std::string>& split_files,
                             std::vector<std::string>& sequence_errors);
    
    bool validateFileMetadata(const std::vector<std::string>& split_files,
                             std::vector<std::string>& metadata_errors);
    
    // Metadata helpers
    bool writeSplitMetadata(const std::string& metadata_file_path,
                           const std::vector<SBEnhanced::SplitFileInfo>& files_info);
    
    bool readSplitMetadata(const std::string& metadata_file_path,
                          std::vector<SBEnhanced::SplitFileInfo>& files_info);
    
    // Progress tracking helpers
    void updateProgress(SBEnhanced::SplitOperation operation,
                       uint64_t processed, uint64_t total,
                       const std::string& current_item);
    
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    
    // Database file analysis
    bool analyzeDatabaseFile(const std::string& database_path,
                            uint64_t& file_size,
                            std::map<std::string, std::string>& properties);
    
    // Split strategy optimization
    uint64_t calculateOptimalSplitSize(uint64_t database_size,
                                      uint64_t max_file_size,
                                      uint64_t& optimal_files);
    
    // Progress callback management
    void notifyProgress(const std::function<void(const SBEnhanced::SplitProgress&)>& callback);
    
    // Statistics collection helpers
    void collectSplitStatistics(SBEnhanced::SplitStatistics& stats);
    void collectJoinStatistics(SBEnhanced::SplitStatistics& stats);
    void collectValidationStatistics(SBEnhanced::SplitStatistics& stats);
};

// Utility functions for enhanced GSSPLIT
namespace SBEnhanced {

// Quick split operations
bool quickSplitDatabase(const std::string& database_path,
                       const std::string& output_prefix,
                       uint64_t max_file_size = 2147483648ULL);

bool quickJoinDatabase(const std::vector<std::string>& split_files,
                      const std::string& output_database);

// Validation helpers
bool quickValidateSplitFiles(const std::vector<std::string>& split_files);
bool validateSplitFileSequence(const std::vector<std::string>& split_files);

// File size and estimation helpers
uint64_t calculateDatabaseSize(const std::string& database_path);
uint64_t estimateSplitFileCount(uint64_t database_size, uint64_t max_file_size);

// Checksum utilities
std::string generateQuickChecksum(const std::string& file_path);
bool verifyQuickChecksum(const std::string& file_path, const std::string& expected_checksum);

// Compression utilities
double estimateCompressionRatio(const std::string& file_path, SplitCompressionType compression);
uint64_t estimateCompressedSize(uint64_t original_size, SplitCompressionType compression);

// Compatibility helpers for command-line usage
int parseGSplitCommandLine(int argc, char* argv[],
                          std::string& database_path,
                          SplitOptions& split_opts,
                          JoinOptions& join_opts);

bool executeClassicGSplitCommand(const std::string& command_line);

} // namespace SBEnhanced