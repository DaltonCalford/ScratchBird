#include "sb_nbackup_enhanced.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <filesystem>
#include <csignal>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// Global nbackup instance for signal handling
static NBackupEnhanced* g_nbackup_instance = nullptr;

// Signal handler for graceful cancellation
void signalHandler(int signal) {
    if (g_nbackup_instance) {
        std::cout << "\nReceived signal " << signal << ", cancelling operation..." << std::endl;
        g_nbackup_instance->requestCancel();
    }
}

// Command-line argument parser
class NBackupCommandParser {
private:
    struct CommandOptions {
        // Operation mode
        bool backup_mode = false;
        bool restore_mode = false;
        bool merge_mode = false;
        bool validate_mode = false;
        bool list_mode = false;
        bool analyze_mode = false;
        bool help_mode = false;
        bool version_mode = false;
        
        // File paths
        std::string database_path;
        std::string backup_path;
        std::vector<std::string> input_files;
        std::string output_file;
        
        // Backup options
        SBEnhanced::BackupLevel backup_level = SBEnhanced::BackupLevel::FULL;
        SBEnhanced::BackupCompression compression = SBEnhanced::BackupCompression::AUTO;
        bool auto_detect_parent = true;
        bool parallel_processing = true;
        uint32_t worker_threads = 4;
        
        // Verification options
        SBEnhanced::VerificationLevel verification = SBEnhanced::VerificationLevel::BASIC;
        bool verify_after_backup = false;
        bool test_restore = false;
        
        // Advanced options
        bool direct_io = false;
        bool lock_database = false;
        bool encrypt_backup = false;
        std::string encryption_key;
        uint32_t buffer_size_mb = 64;
        
        // Filtering options
        std::vector<std::string> include_tables;
        std::vector<std::string> exclude_tables;
        std::vector<std::string> include_schemas;
        std::vector<std::string> exclude_schemas;
        
        // Output options
        std::string report_format = "TEXT";
        std::string report_path;
        bool verbose = false;
        bool quiet = false;
        bool show_progress = true;
        
        // Cleanup options
        uint32_t retention_days = 30;
        uint32_t max_backup_count = 50;
        bool cleanup_temp_files = true;
    };
    
    CommandOptions options;
    std::vector<std::string> errors;
    
public:
    bool parseArguments(int argc, char* argv[]) {
        if (argc < 2) {
            showUsage(argv[0]);
            return false;
        }
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                options.help_mode = true;
                return true;
            }
            else if (arg == "-v" || arg == "--version") {
                options.version_mode = true;
                return true;
            }
            else if (arg == "backup" || arg == "-B") {
                options.backup_mode = true;
            }
            else if (arg == "restore" || arg == "-R") {
                options.restore_mode = true;
            }
            else if (arg == "merge" || arg == "-M") {
                options.merge_mode = true;
            }
            else if (arg == "validate" || arg == "-V") {
                options.validate_mode = true;
            }
            else if (arg == "list" || arg == "-L") {
                options.list_mode = true;
            }
            else if (arg == "analyze" || arg == "-A") {
                options.analyze_mode = true;
            }
            else if (arg == "-db" || arg == "--database") {
                if (i + 1 < argc) {
                    options.database_path = argv[++i];
                } else {
                    errors.push_back("Database path required after " + arg);
                }
            }
            else if (arg == "-backup" || arg == "--backup-file") {
                if (i + 1 < argc) {
                    options.backup_path = argv[++i];
                } else {
                    errors.push_back("Backup file path required after " + arg);
                }
            }
            else if (arg == "-level" || arg == "--backup-level") {
                if (i + 1 < argc) {
                    try {
                        int level = std::stoi(argv[++i]);
                        options.backup_level = static_cast<SBEnhanced::BackupLevel>(level);
                    } catch (const std::exception&) {
                        errors.push_back("Invalid backup level: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Backup level required after " + arg);
                }
            }
            else if (arg == "-compression" || arg == "--compression") {
                if (i + 1 < argc) {
                    std::string comp_str = argv[++i];
                    options.compression = parseCompressionType(comp_str);
                } else {
                    errors.push_back("Compression type required after " + arg);
                }
            }
            else if (arg == "-threads" || arg == "--worker-threads") {
                if (i + 1 < argc) {
                    try {
                        options.worker_threads = static_cast<uint32_t>(std::stoi(argv[++i]));
                    } catch (const std::exception&) {
                        errors.push_back("Invalid thread count: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Thread count required after " + arg);
                }
            }
            else if (arg == "-verification" || arg == "--verification-level") {
                if (i + 1 < argc) {
                    std::string level_str = argv[++i];
                    options.verification = parseVerificationLevel(level_str);
                } else {
                    errors.push_back("Verification level required after " + arg);
                }
            }
            else if (arg == "-buffer" || arg == "--buffer-size") {
                if (i + 1 < argc) {
                    try {
                        options.buffer_size_mb = static_cast<uint32_t>(std::stoi(argv[++i]));
                    } catch (const std::exception&) {
                        errors.push_back("Invalid buffer size: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Buffer size required after " + arg);
                }
            }
            else if (arg == "-include-table" || arg == "--include-table") {
                if (i + 1 < argc) {
                    options.include_tables.push_back(argv[++i]);
                } else {
                    errors.push_back("Table name required after " + arg);
                }
            }
            else if (arg == "-exclude-table" || arg == "--exclude-table") {
                if (i + 1 < argc) {
                    options.exclude_tables.push_back(argv[++i]);
                } else {
                    errors.push_back("Table name required after " + arg);
                }
            }
            else if (arg == "-include-schema" || arg == "--include-schema") {
                if (i + 1 < argc) {
                    options.include_schemas.push_back(argv[++i]);
                } else {
                    errors.push_back("Schema name required after " + arg);
                }
            }
            else if (arg == "-exclude-schema" || arg == "--exclude-schema") {
                if (i + 1 < argc) {
                    options.exclude_schemas.push_back(argv[++i]);
                } else {
                    errors.push_back("Schema name required after " + arg);
                }
            }
            else if (arg == "-encrypt" || arg == "--encrypt") {
                options.encrypt_backup = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options.encryption_key = argv[++i];
                }
            }
            else if (arg == "-verify" || arg == "--verify-after-backup") {
                options.verify_after_backup = true;
            }
            else if (arg == "-test-restore" || arg == "--test-restore") {
                options.test_restore = true;
            }
            else if (arg == "-lock" || arg == "--lock-database") {
                options.lock_database = true;
            }
            else if (arg == "-direct-io" || arg == "--direct-io") {
                options.direct_io = true;
            }
            else if (arg == "-no-parallel" || arg == "--disable-parallel") {
                options.parallel_processing = false;
            }
            else if (arg == "-report" || arg == "--report") {
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options.report_path = argv[++i];
                }
            }
            else if (arg == "-format" || arg == "--report-format") {
                if (i + 1 < argc) {
                    options.report_format = argv[++i];
                    std::transform(options.report_format.begin(), options.report_format.end(),
                                 options.report_format.begin(), ::toupper);
                } else {
                    errors.push_back("Report format required after " + arg);
                }
            }
            else if (arg == "-retention" || arg == "--retention-days") {
                if (i + 1 < argc) {
                    try {
                        options.retention_days = static_cast<uint32_t>(std::stoi(argv[++i]));
                    } catch (const std::exception&) {
                        errors.push_back("Invalid retention days: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Retention days required after " + arg);
                }
            }
            else if (arg == "-max-backups" || arg == "--max-backup-count") {
                if (i + 1 < argc) {
                    try {
                        options.max_backup_count = static_cast<uint32_t>(std::stoi(argv[++i]));
                    } catch (const std::exception&) {
                        errors.push_back("Invalid max backup count: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Max backup count required after " + arg);
                }
            }
            else if (arg == "-verbose" || arg == "--verbose") {
                options.verbose = true;
            }
            else if (arg == "-quiet" || arg == "--quiet") {
                options.quiet = true;
            }
            else if (arg == "-no-progress" || arg == "--no-progress") {
                options.show_progress = false;
            }
            else if (arg[0] != '-') {
                // Positional arguments
                if (options.database_path.empty()) {
                    options.database_path = arg;
                } else if (options.backup_path.empty()) {
                    options.backup_path = arg;
                } else {
                    options.input_files.push_back(arg);
                }
            }
            else {
                errors.push_back("Unknown option: " + arg);
            }
        }
        
        return validateOptions();
    }
    
    bool executeCommand() {
        if (options.help_mode) {
            showHelp();
            return true;
        }
        
        if (options.version_mode) {
            showVersion();
            return true;
        }
        
        if (options.backup_mode) {
            return executeBackup();
        }
        else if (options.restore_mode) {
            return executeRestore();
        }
        else if (options.merge_mode) {
            return executeMerge();
        }
        else if (options.validate_mode) {
            return executeValidate();
        }
        else if (options.list_mode) {
            return executeList();
        }
        else if (options.analyze_mode) {
            return executeAnalyze();
        }
        
        std::cerr << "Error: No operation specified. Use -h for help." << std::endl;
        return false;
    }
    
private:
    SBEnhanced::BackupCompression parseCompressionType(const std::string& comp_str) {
        std::string comp = comp_str;
        std::transform(comp.begin(), comp.end(), comp.begin(), ::tolower);
        
        if (comp == "none" || comp == "0") {
            return SBEnhanced::BackupCompression::NONE;
        } else if (comp == "gzip" || comp == "gz") {
            return SBEnhanced::BackupCompression::GZIP;
        } else if (comp == "lz4") {
            return SBEnhanced::BackupCompression::LZ4;
        } else if (comp == "zstd") {
            return SBEnhanced::BackupCompression::ZSTD;
        } else if (comp == "bzip2" || comp == "bz2") {
            return SBEnhanced::BackupCompression::BZIP2;
        } else if (comp == "auto") {
            return SBEnhanced::BackupCompression::AUTO;
        }
        
        return SBEnhanced::BackupCompression::AUTO;
    }
    
    SBEnhanced::VerificationLevel parseVerificationLevel(const std::string& level_str) {
        std::string level = level_str;
        std::transform(level.begin(), level.end(), level.begin(), ::tolower);
        
        if (level == "none" || level == "0") {
            return SBEnhanced::VerificationLevel::NONE;
        } else if (level == "basic" || level == "1") {
            return SBEnhanced::VerificationLevel::BASIC;
        } else if (level == "checksum" || level == "2") {
            return SBEnhanced::VerificationLevel::CHECKSUM;
        } else if (level == "structural" || level == "3") {
            return SBEnhanced::VerificationLevel::STRUCTURAL;
        } else if (level == "comprehensive" || level == "4") {
            return SBEnhanced::VerificationLevel::COMPREHENSIVE;
        } else if (level == "forensic" || level == "5") {
            return SBEnhanced::VerificationLevel::FORENSIC;
        }
        
        return SBEnhanced::VerificationLevel::BASIC;
    }
    
    bool validateOptions() {
        // Check for conflicting modes
        int mode_count = 0;
        if (options.backup_mode) mode_count++;
        if (options.restore_mode) mode_count++;
        if (options.merge_mode) mode_count++;
        if (options.validate_mode) mode_count++;
        if (options.list_mode) mode_count++;
        if (options.analyze_mode) mode_count++;
        
        if (mode_count == 0 && !options.help_mode && !options.version_mode) {
            errors.push_back("No operation specified");
        } else if (mode_count > 1) {
            errors.push_back("Only one operation can be specified");
        }
        
        // Validate backup mode options
        if (options.backup_mode) {
            if (options.database_path.empty()) {
                errors.push_back("Database path required for backup operation");
            }
            if (options.backup_path.empty()) {
                errors.push_back("Backup file path required for backup operation");
            }
        }
        
        // Validate restore mode options
        if (options.restore_mode) {
            if (options.backup_path.empty()) {
                errors.push_back("Backup file path required for restore operation");
            }
            if (options.database_path.empty()) {
                errors.push_back("Database path required for restore operation");
            }
        }
        
        // Validate merge mode options
        if (options.merge_mode) {
            if (options.input_files.size() < 2) {
                errors.push_back("At least two backup files required for merge operation");
            }
            if (options.output_file.empty() && options.backup_path.empty()) {
                errors.push_back("Output file required for merge operation");
            }
        }
        
        if (!errors.empty()) {
            for (const auto& error : errors) {
                std::cerr << "Error: " << error << std::endl;
            }
            return false;
        }
        
        return true;
    }
    
    bool executeBackup() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced NBackup - Creating Database Backup" << std::endl;
            std::cout << "=======================================================" << std::endl;
            std::cout << "Database: " << options.database_path << std::endl;
            std::cout << "Backup file: " << options.backup_path << std::endl;
            std::cout << "Backup level: " << getBackupLevelString(options.backup_level) << std::endl;
            std::cout << "Compression: " << getCompressionString(options.compression) << std::endl;
            if (options.parallel_processing) {
                std::cout << "Worker threads: " << options.worker_threads << std::endl;
            }
            std::cout << std::endl;
        }
        
        // Setup signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
        std::signal(SIGHUP, signalHandler);
#endif
        
        NBackupEnhanced nbackup;
        g_nbackup_instance = &nbackup;
        
        // Create backup options
        SBEnhanced::BackupOptions backup_options;
        backup_options.database_path = options.database_path;
        backup_options.backup_path = options.backup_path;
        backup_options.level = options.backup_level;
        backup_options.compression = options.compression;
        backup_options.parallel_processing = options.parallel_processing;
        backup_options.worker_threads = options.worker_threads;
        backup_options.buffer_size_mb = options.buffer_size_mb;
        backup_options.verification = options.verification;
        backup_options.verify_after_backup = options.verify_after_backup;
        backup_options.test_restore = options.test_restore;
        backup_options.lock_database = options.lock_database;
        backup_options.direct_io = options.direct_io;
        backup_options.encrypt_backup = options.encrypt_backup;
        backup_options.encryption_key = options.encryption_key;
        backup_options.include_tables = options.include_tables;
        backup_options.exclude_tables = options.exclude_tables;
        backup_options.include_schemas = options.include_schemas;
        backup_options.exclude_schemas = options.exclude_schemas;
        backup_options.verbose_output = options.verbose;
        backup_options.quiet_mode = options.quiet;
        backup_options.cleanup_temp_files = options.cleanup_temp_files;
        
        // Set progress callback if showing progress
        if (options.show_progress && !options.quiet) {
            backup_options.progress_callback = [](const std::string& operation, double percentage) {
                std::cout << "\\r[" << std::fixed << std::setprecision(1) << percentage 
                         << "%] " << operation << std::flush;
            };
        }
        
        SBEnhanced::BackupOperationResult result;
        bool success = nbackup.performBackup(backup_options, result);
        
        if (options.show_progress && !options.quiet) {
            std::cout << std::endl;  // New line after progress
        }
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Backup completed successfully!" << std::endl;
                std::cout << "Duration: " << SBEnhanced::formatBackupDuration(result.total_duration) << std::endl;
                std::cout << "Size: " << SBEnhanced::formatBackupSize(result.total_bytes_written) << std::endl;
                
                if (result.compression_ratio > 0.0) {
                    std::cout << "Compression ratio: " << std::fixed << std::setprecision(2) 
                             << result.compression_ratio << std::endl;
                }
                
                if (options.verbose) {
                    std::cout << "Average throughput: " << std::fixed << std::setprecision(2)
                             << result.average_throughput_mbps << " MB/s" << std::endl;
                    std::cout << "Pages backed up: " << result.pages_backed_up << std::endl;
                }
            }
        } else {
            std::cerr << "Backup failed!" << std::endl;
            for (const auto& error : result.operation_errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        // Display warnings
        if (!result.operation_warnings.empty() && !options.quiet) {
            std::cout << "\\nWarnings:" << std::endl;
            for (const auto& warning : result.operation_warnings) {
                std::cout << "Warning: " << warning << std::endl;
            }
        }
        
        // Generate report if requested
        if (!options.report_path.empty()) {
            std::string report = result.generateOperationReport();
            std::ofstream report_file(options.report_path);
            if (report_file.is_open()) {
                report_file << report;
                report_file.close();
                if (!options.quiet) {
                    std::cout << "Report generated: " << options.report_path << std::endl;
                }
            }
        }
        
        g_nbackup_instance = nullptr;
        return success;
    }
    
    bool executeRestore() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced NBackup - Restoring Database" << std::endl;
            std::cout << "=================================================" << std::endl;
            std::cout << "Backup file: " << options.backup_path << std::endl;
            std::cout << "Database: " << options.database_path << std::endl;
            if (options.parallel_processing) {
                std::cout << "Worker threads: " << options.worker_threads << std::endl;
            }
            std::cout << std::endl;
        }
        
        // Setup signal handlers
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
#ifndef _WIN32
        std::signal(SIGHUP, signalHandler);
#endif
        
        NBackupEnhanced nbackup;
        g_nbackup_instance = &nbackup;
        
        // Create restore options
        SBEnhanced::RestoreOptions restore_options;
        restore_options.backup_path = options.backup_path;
        restore_options.database_path = options.database_path;
        restore_options.parallel_processing = options.parallel_processing;
        restore_options.worker_threads = options.worker_threads;
        restore_options.buffer_size_mb = options.buffer_size_mb;
        restore_options.validate_before_restore = (options.verification != SBEnhanced::VerificationLevel::NONE);
        restore_options.create_database = true;
        restore_options.overwrite_existing = true;
        restore_options.include_tables = options.include_tables;
        restore_options.exclude_tables = options.exclude_tables;
        restore_options.include_schemas = options.include_schemas;
        restore_options.exclude_schemas = options.exclude_schemas;
        restore_options.verbose_output = options.verbose;
        restore_options.quiet_mode = options.quiet;
        
        // Set progress callback if showing progress
        if (options.show_progress && !options.quiet) {
            restore_options.progress_callback = [](const std::string& operation, double percentage) {
                std::cout << "\\r[" << std::fixed << std::setprecision(1) << percentage 
                         << "%] " << operation << std::flush;
            };
        }
        
        SBEnhanced::BackupOperationResult result;
        bool success = nbackup.performRestore(restore_options, result);
        
        if (options.show_progress && !options.quiet) {
            std::cout << std::endl;  // New line after progress
        }
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Restore completed successfully!" << std::endl;
                std::cout << "Duration: " << SBEnhanced::formatBackupDuration(result.total_duration) << std::endl;
                
                if (options.verbose) {
                    std::cout << "Average throughput: " << std::fixed << std::setprecision(2)
                             << result.average_throughput_mbps << " MB/s" << std::endl;
                }
            }
        } else {
            std::cerr << "Restore failed!" << std::endl;
            for (const auto& error : result.operation_errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        g_nbackup_instance = nullptr;
        return success;
    }
    
    bool executeMerge() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced NBackup - Merging Backup Files" << std::endl;
            std::cout << "===================================================" << std::endl;
            std::cout << "Input files: " << options.input_files.size() << std::endl;
            for (const auto& file : options.input_files) {
                std::cout << "  " << file << std::endl;
            }
            std::string output = options.output_file.empty() ? options.backup_path : options.output_file;
            std::cout << "Output file: " << output << std::endl;
            std::cout << std::endl;
        }
        
        NBackupEnhanced nbackup;
        
        // Create merge options
        SBEnhanced::MergeOptions merge_options;
        merge_options.backup_paths = options.input_files;
        merge_options.output_path = options.output_file.empty() ? options.backup_path : options.output_file;
        merge_options.strategy = SBEnhanced::MergeStrategy::OPTIMAL;
        merge_options.output_compression = options.compression;
        merge_options.verify_inputs = true;
        merge_options.verify_output = true;
        merge_options.parallel_processing = options.parallel_processing;
        merge_options.worker_threads = options.worker_threads;
        merge_options.buffer_size_mb = options.buffer_size_mb;
        merge_options.verbose_output = options.verbose;
        merge_options.quiet_mode = options.quiet;
        merge_options.cleanup_temp_files = options.cleanup_temp_files;
        
        SBEnhanced::BackupOperationResult result;
        bool success = nbackup.performMerge(merge_options, result);
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Merge completed successfully!" << std::endl;
                std::cout << "Duration: " << SBEnhanced::formatBackupDuration(result.total_duration) << std::endl;
                std::cout << "Output size: " << SBEnhanced::formatBackupSize(result.total_bytes_written) << std::endl;
            }
        } else {
            std::cerr << "Merge failed!" << std::endl;
            for (const auto& error : result.operation_errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        return success;
    }
    
    bool executeValidate() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced NBackup - Validating Backup Files" << std::endl;
            std::cout << "=======================================================" << std::endl;
            std::cout << "Verification level: " << getVerificationLevelString(options.verification) << std::endl;
            std::cout << std::endl;
        }
        
        NBackupEnhanced nbackup;
        
        // Collect files to validate
        std::vector<std::string> files_to_validate;
        if (!options.backup_path.empty()) {
            files_to_validate.push_back(options.backup_path);
        }
        files_to_validate.insert(files_to_validate.end(), 
                               options.input_files.begin(), options.input_files.end());
        
        if (files_to_validate.empty()) {
            std::cerr << "No backup files specified for validation" << std::endl;
            return false;
        }
        
        // Create validation options
        SBEnhanced::ValidationOptions validation_options;
        validation_options.backup_paths = files_to_validate;
        validation_options.verification_level = options.verification;
        validation_options.parallel_validation = options.parallel_processing;
        validation_options.worker_threads = std::min(options.worker_threads, 2u);  // Limit for validation
        validation_options.generate_report = !options.report_path.empty();
        validation_options.report_path = options.report_path;
        validation_options.report_format = options.report_format;
        validation_options.verbose_output = options.verbose;
        validation_options.quiet_mode = options.quiet;
        
        std::vector<std::string> validation_results;
        bool success = nbackup.validateBackups(validation_options, validation_results);
        
        if (!options.quiet) {
            std::cout << "Validation Results:" << std::endl;
            std::cout << "==================" << std::endl;
            for (const auto& result : validation_results) {
                std::cout << result << std::endl;
            }
        }
        
        return success;
    }
    
    bool executeList() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced NBackup - Listing Backup Information" << std::endl;
            std::cout << "=========================================================" << std::endl;
        }
        
        NBackupEnhanced nbackup;
        
        // Collect files to list
        std::vector<std::string> files_to_list;
        if (!options.backup_path.empty()) {
            if (fs::is_directory(options.backup_path)) {
                // List all backup files in directory
                for (const auto& entry : fs::directory_iterator(options.backup_path)) {
                    if (entry.is_regular_file()) {
                        files_to_list.push_back(entry.path().string());
                    }
                }
            } else {
                files_to_list.push_back(options.backup_path);
            }
        }
        files_to_list.insert(files_to_list.end(), 
                           options.input_files.begin(), options.input_files.end());
        
        if (files_to_list.empty()) {
            std::cerr << "No backup files specified" << std::endl;
            return false;
        }
        
        bool success = true;
        for (const auto& backup_path : files_to_list) {
            SBEnhanced::BackupInfo backup_info;
            if (nbackup.getBackupInfo(backup_path, backup_info)) {
                if (!options.quiet) {
                    displayBackupInfo(backup_info);
                }
            } else {
                std::cerr << "Failed to get info for: " << backup_path << std::endl;
                success = false;
            }
        }
        
        return success;
    }
    
    bool executeAnalyze() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced NBackup - Analyzing Backup Files" << std::endl;
            std::cout << "=====================================================" << std::endl;
        }
        
        NBackupEnhanced nbackup;
        
        // Collect files to analyze
        std::vector<std::string> files_to_analyze;
        if (!options.backup_path.empty()) {
            if (fs::is_directory(options.backup_path)) {
                // Analyze all backup files in directory
                for (const auto& entry : fs::directory_iterator(options.backup_path)) {
                    if (entry.is_regular_file()) {
                        files_to_analyze.push_back(entry.path().string());
                    }
                }
            } else {
                files_to_analyze.push_back(options.backup_path);
            }
        }
        files_to_analyze.insert(files_to_analyze.end(), 
                              options.input_files.begin(), options.input_files.end());
        
        if (files_to_analyze.empty()) {
            std::cerr << "No backup files specified for analysis" << std::endl;
            return false;
        }
        
        SBEnhanced::BackupAnalysisResult analysis_result;
        bool success = nbackup.analyzeBackups(files_to_analyze, analysis_result);
        
        if (success && !options.quiet) {
            std::string report = analysis_result.generateAnalysisReport();
            std::cout << report << std::endl;
            
            if (options.verbose) {
                std::string recommendations = analysis_result.generateRecommendationsReport();
                std::cout << recommendations << std::endl;
            }
        }
        
        // Generate report file if requested
        if (!options.report_path.empty()) {
            std::string report = analysis_result.generateAnalysisReport();
            if (options.verbose) {
                report += "\\n" + analysis_result.generateRecommendationsReport();
            }
            
            std::ofstream report_file(options.report_path);
            if (report_file.is_open()) {
                report_file << report;
                report_file.close();
                if (!options.quiet) {
                    std::cout << "Analysis report generated: " << options.report_path << std::endl;
                }
            }
        }
        
        return success;
    }
    
    void displayBackupInfo(const SBEnhanced::BackupInfo& info) {
        std::cout << "File: " << info.backup_path << std::endl;
        std::cout << "  Type: " << info.getFileTypeString() << std::endl;
        std::cout << "  Level: " << info.getBackupLevelString() << std::endl;
        std::cout << "  Size: " << SBEnhanced::formatBackupSize(info.file_size) << std::endl;
        std::cout << "  Compression: " << info.getCompressionString() << std::endl;
        
        if (info.compression_ratio > 0.0) {
            std::cout << "  Compression Ratio: " << std::fixed << std::setprecision(2) 
                     << info.compression_ratio << std::endl;
        }
        
        std::cout << "  Valid: " << (info.is_valid ? "Yes" : "No") << std::endl;
        
        if (info.is_encrypted) {
            std::cout << "  Encrypted: Yes" << std::endl;
        }
        
        if (!info.checksum_md5.empty()) {
            std::cout << "  MD5: " << info.checksum_md5 << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    std::string getBackupLevelString(SBEnhanced::BackupLevel level) {
        switch (level) {
            case SBEnhanced::BackupLevel::FULL: return "Full (0)";
            case SBEnhanced::BackupLevel::INCREMENTAL_1: return "Incremental-1";
            case SBEnhanced::BackupLevel::INCREMENTAL_2: return "Incremental-2";
            case SBEnhanced::BackupLevel::INCREMENTAL_3: return "Incremental-3";
            case SBEnhanced::BackupLevel::INCREMENTAL_4: return "Incremental-4";
            case SBEnhanced::BackupLevel::INCREMENTAL_5: return "Incremental-5";
            case SBEnhanced::BackupLevel::INCREMENTAL_6: return "Incremental-6";
            case SBEnhanced::BackupLevel::INCREMENTAL_7: return "Incremental-7";
            case SBEnhanced::BackupLevel::INCREMENTAL_8: return "Incremental-8";
            case SBEnhanced::BackupLevel::INCREMENTAL_9: return "Incremental-9";
            case SBEnhanced::BackupLevel::AUTO: return "Auto";
            default: return "Unknown";
        }
    }
    
    std::string getCompressionString(SBEnhanced::BackupCompression compression) {
        switch (compression) {
            case SBEnhanced::BackupCompression::NONE: return "None";
            case SBEnhanced::BackupCompression::GZIP: return "GZIP";
            case SBEnhanced::BackupCompression::LZ4: return "LZ4";
            case SBEnhanced::BackupCompression::ZSTD: return "ZSTD";
            case SBEnhanced::BackupCompression::BZIP2: return "BZIP2";
            case SBEnhanced::BackupCompression::AUTO: return "Auto";
            default: return "Unknown";
        }
    }
    
    std::string getVerificationLevelString(SBEnhanced::VerificationLevel level) {
        switch (level) {
            case SBEnhanced::VerificationLevel::NONE: return "None";
            case SBEnhanced::VerificationLevel::BASIC: return "Basic";
            case SBEnhanced::VerificationLevel::CHECKSUM: return "Checksum";
            case SBEnhanced::VerificationLevel::STRUCTURAL: return "Structural";
            case SBEnhanced::VerificationLevel::COMPREHENSIVE: return "Comprehensive";
            case SBEnhanced::VerificationLevel::FORENSIC: return "Forensic";
            default: return "Unknown";
        }
    }
    
    void showUsage(const char* program_name) {
        std::cout << "Usage: " << program_name << " [OPTIONS] OPERATION" << std::endl;
        std::cout << "Try '" << program_name << " --help' for more information." << std::endl;
    }
    
    void showVersion() {
        std::cout << "sb_nbackup version SB-T0.5.0.1 ScratchBird 0.5 f90eae0" << std::endl;
        std::cout << "ScratchBird Enhanced Incremental Backup Utility" << std::endl;
        std::cout << "Copyright (C) 2025 ScratchBird Project" << std::endl;
    }
    
    void showHelp() {
        std::cout << "ScratchBird Enhanced NBackup - Incremental Backup Utility" << std::endl;
        std::cout << "==========================================================" << std::endl;
        std::cout << std::endl;
        std::cout << "OPERATIONS:" << std::endl;
        std::cout << "  backup              Create incremental backup" << std::endl;
        std::cout << "  restore             Restore from backup" << std::endl;
        std::cout << "  merge               Merge backup levels" << std::endl;
        std::cout << "  validate            Validate backup files" << std::endl;
        std::cout << "  list                List backup information" << std::endl;
        std::cout << "  analyze             Analyze backup files and chains" << std::endl;
        std::cout << std::endl;
        std::cout << "FILE OPTIONS:" << std::endl;
        std::cout << "  -db, --database PATH        Database file path" << std::endl;
        std::cout << "  -backup, --backup-file PATH Backup file path" << std::endl;
        std::cout << std::endl;
        std::cout << "BACKUP OPTIONS:" << std::endl;
        std::cout << "  -level LEVEL                Backup level (0-9, default: 0)" << std::endl;
        std::cout << "  -compression TYPE           Compression: none, gzip, lz4, zstd, bzip2, auto" << std::endl;
        std::cout << "  -threads N                  Number of worker threads (default: 4)" << std::endl;
        std::cout << "  -buffer SIZE                Buffer size in MB (default: 64)" << std::endl;
        std::cout << "  -verify                     Verify backup after creation" << std::endl;
        std::cout << "  -test-restore               Test restore after backup" << std::endl;
        std::cout << "  -lock                       Lock database during backup" << std::endl;
        std::cout << "  -direct-io                  Use direct I/O" << std::endl;
        std::cout << "  -encrypt [KEY]              Encrypt backup with optional key" << std::endl;
        std::cout << std::endl;
        std::cout << "FILTERING OPTIONS:" << std::endl;
        std::cout << "  -include-table TABLE        Include specific table" << std::endl;
        std::cout << "  -exclude-table TABLE        Exclude specific table" << std::endl;
        std::cout << "  -include-schema SCHEMA      Include specific schema" << std::endl;
        std::cout << "  -exclude-schema SCHEMA      Exclude specific schema" << std::endl;
        std::cout << std::endl;
        std::cout << "VERIFICATION OPTIONS:" << std::endl;
        std::cout << "  -verification LEVEL         Verification level: none, basic, checksum," << std::endl;
        std::cout << "                              structural, comprehensive, forensic" << std::endl;
        std::cout << std::endl;
        std::cout << "REPORTING OPTIONS:" << std::endl;
        std::cout << "  -report [PATH]              Generate operation report" << std::endl;
        std::cout << "  -format FORMAT              Report format: TEXT, JSON, XML, HTML" << std::endl;
        std::cout << std::endl;
        std::cout << "CLEANUP OPTIONS:" << std::endl;
        std::cout << "  -retention DAYS             Backup retention in days (default: 30)" << std::endl;
        std::cout << "  -max-backups N              Maximum backup count (default: 50)" << std::endl;
        std::cout << std::endl;
        std::cout << "GENERAL OPTIONS:" << std::endl;
        std::cout << "  -no-parallel                Disable parallel processing" << std::endl;
        std::cout << "  -verbose                    Verbose output with detailed information" << std::endl;
        std::cout << "  -quiet                      Suppress non-error output" << std::endl;
        std::cout << "  -no-progress                Disable progress display" << std::endl;
        std::cout << "  -h, --help                  Show this help message" << std::endl;
        std::cout << "  -v, --version               Show version information" << std::endl;
        std::cout << std::endl;
        std::cout << "EXAMPLES:" << std::endl;
        std::cout << "  # Create full backup with compression" << std::endl;
        std::cout << "  sb_nbackup backup -db mydb.fdb -backup mydb_full.nbk -compression zstd" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Create incremental level 1 backup" << std::endl;
        std::cout << "  sb_nbackup backup -db mydb.fdb -backup mydb_inc1.nbk -level 1" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Restore from backup with verification" << std::endl;
        std::cout << "  sb_nbackup restore -backup mydb_full.nbk -db restored.fdb -verification comprehensive" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Merge backup levels" << std::endl;
        std::cout << "  sb_nbackup merge mydb_full.nbk mydb_inc1.nbk mydb_inc2.nbk -backup merged.nbk" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Validate backup chain" << std::endl;
        std::cout << "  sb_nbackup validate *.nbk -verification structural -report validation.txt" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Analyze backup directory" << std::endl;
        std::cout << "  sb_nbackup analyze -backup /backups/ -report analysis.html -format HTML" << std::endl;
        std::cout << std::endl;
        std::cout << "ORIGINAL NBACKUP COMPATIBILITY:" << std::endl;
        std::cout << "  sb_nbackup -B 0 database.fdb backup.nbk    # Create full backup" << std::endl;
        std::cout << "  sb_nbackup -B 1 database.fdb backup.nbk    # Create level 1 backup" << std::endl;
        std::cout << "  sb_nbackup -R backup.nbk database.fdb      # Restore backup" << std::endl;
        std::cout << "  sb_nbackup -M backup.nbk output.nbk        # Merge backup" << std::endl;
        std::cout << std::endl;
        std::cout << "For more information, see the ScratchBird documentation." << std::endl;
    }
};

// Main function
int main(int argc, char* argv[]) {
    try {
        NBackupCommandParser parser;
        
        if (!parser.parseArguments(argc, argv)) {
            return 1;
        }
        
        bool success = parser.executeCommand();
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: Unknown exception" << std::endl;
        return 1;
    }
}