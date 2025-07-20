#include "sb_gssplit_enhanced.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

// Command-line argument parser
class GSplitCommandParser {
private:
    struct CommandOptions {
        // Operation mode
        bool split_mode = false;
        bool join_mode = false;
        bool validate_mode = false;
        bool analyze_mode = false;
        bool help_mode = false;
        bool version_mode = false;
        
        // Split options
        std::string source_database;
        std::string output_prefix = "split_db";
        std::string output_directory;
        uint64_t max_file_size = 2147483648ULL;  // 2GB default
        SBEnhanced::SplitCompressionType compression = SBEnhanced::SplitCompressionType::NONE;
        int compression_level = 6;
        bool create_checksum = false;
        bool verify_after_split = false;
        bool overwrite_existing = false;
        
        // Join options
        std::vector<std::string> split_files;
        std::string output_database;
        SBEnhanced::SplitValidationMode validation_mode = SBEnhanced::SplitValidationMode::BASIC;
        bool verify_checksums = false;
        bool verify_after_join = false;
        
        // Validation options
        bool comprehensive_validation = false;
        bool generate_report = false;
        std::string report_path;
        
        // General options
        bool verbose = false;
        bool quiet = false;
        std::string description;
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
            else if (arg == "-split" || arg == "--split") {
                options.split_mode = true;
            }
            else if (arg == "-join" || arg == "--join") {
                options.join_mode = true;
            }
            else if (arg == "-validate" || arg == "--validate") {
                options.validate_mode = true;
            }
            else if (arg == "-analyze" || arg == "--analyze") {
                options.analyze_mode = true;
            }
            else if (arg == "-db" || arg == "--database") {
                if (i + 1 < argc) {
                    options.source_database = argv[++i];
                } else {
                    errors.push_back("Database path required after " + arg);
                }
            }
            else if (arg == "-o" || arg == "--output") {
                if (i + 1 < argc) {
                    options.output_prefix = argv[++i];
                } else {
                    errors.push_back("Output prefix required after " + arg);
                }
            }
            else if (arg == "-od" || arg == "--output-dir") {
                if (i + 1 < argc) {
                    options.output_directory = argv[++i];
                } else {
                    errors.push_back("Output directory required after " + arg);
                }
            }
            else if (arg == "-s" || arg == "--size") {
                if (i + 1 < argc) {
                    try {
                        options.max_file_size = parseFileSize(argv[++i]);
                    } catch (const std::exception& e) {
                        errors.push_back("Invalid file size: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("File size required after " + arg);
                }
            }
            else if (arg == "-c" || arg == "--compress") {
                if (i + 1 < argc) {
                    std::string compression_str = argv[++i];
                    options.compression = parseCompression(compression_str);
                    if (options.compression == SBEnhanced::SplitCompressionType::NONE && compression_str != "none") {
                        errors.push_back("Invalid compression type: " + compression_str);
                    }
                } else {
                    errors.push_back("Compression type required after " + arg);
                }
            }
            else if (arg == "-cl" || arg == "--compress-level") {
                if (i + 1 < argc) {
                    try {
                        options.compression_level = std::stoi(argv[++i]);
                        if (options.compression_level < 1 || options.compression_level > 9) {
                            errors.push_back("Compression level must be between 1-9");
                        }
                    } catch (const std::exception&) {
                        errors.push_back("Invalid compression level: " + std::string(argv[i]));
                    }
                } else {
                    errors.push_back("Compression level required after " + arg);
                }
            }
            else if (arg == "-f" || arg == "--files") {
                // Collect remaining arguments as split files
                for (int j = i + 1; j < argc && argv[j][0] != '-'; ++j) {
                    options.split_files.push_back(argv[j]);
                    i = j;
                }
                if (options.split_files.empty()) {
                    errors.push_back("Split file list required after " + arg);
                }
            }
            else if (arg == "-out" || arg == "--output-db") {
                if (i + 1 < argc) {
                    options.output_database = argv[++i];
                } else {
                    errors.push_back("Output database path required after " + arg);
                }
            }
            else if (arg == "-checksum" || arg == "--create-checksum") {
                options.create_checksum = true;
            }
            else if (arg == "-verify" || arg == "--verify") {
                options.verify_after_split = true;
                options.verify_checksums = true;
            }
            else if (arg == "-verify-join" || arg == "--verify-join") {
                options.verify_after_join = true;
            }
            else if (arg == "-overwrite" || arg == "--overwrite") {
                options.overwrite_existing = true;
            }
            else if (arg == "-comprehensive" || arg == "--comprehensive") {
                options.comprehensive_validation = true;
                options.validation_mode = SBEnhanced::SplitValidationMode::COMPREHENSIVE;
            }
            else if (arg == "-report" || arg == "--report") {
                options.generate_report = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    options.report_path = argv[++i];
                }
            }
            else if (arg == "-desc" || arg == "--description") {
                if (i + 1 < argc) {
                    options.description = argv[++i];
                } else {
                    errors.push_back("Description required after " + arg);
                }
            }
            else if (arg == "-verbose" || arg == "--verbose") {
                options.verbose = true;
            }
            else if (arg == "-quiet" || arg == "--quiet") {
                options.quiet = true;
            }
            else if (arg[0] != '-') {
                // Positional argument - assume it's a database file
                if (options.source_database.empty()) {
                    options.source_database = arg;
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
        
        if (options.split_mode) {
            return executeSplit();
        }
        else if (options.join_mode) {
            return executeJoin();
        }
        else if (options.validate_mode) {
            return executeValidate();
        }
        else if (options.analyze_mode) {
            return executeAnalyze();
        }
        
        std::cerr << "Error: No operation mode specified. Use -h for help." << std::endl;
        return false;
    }
    
private:
    uint64_t parseFileSize(const std::string& size_str) {
        std::string size = size_str;
        std::transform(size.begin(), size.end(), size.begin(), ::tolower);
        
        uint64_t multiplier = 1;
        if (size.back() == 'k') {
            multiplier = 1024;
            size.pop_back();
        } else if (size.back() == 'm') {
            multiplier = 1024 * 1024;
            size.pop_back();
        } else if (size.back() == 'g') {
            multiplier = 1024 * 1024 * 1024;
            size.pop_back();
        }
        
        uint64_t value = std::stoull(size);
        return value * multiplier;
    }
    
    SBEnhanced::SplitCompressionType parseCompression(const std::string& compression_str) {
        std::string comp = compression_str;
        std::transform(comp.begin(), comp.end(), comp.begin(), ::tolower);
        
        if (comp == "none") return SBEnhanced::SplitCompressionType::NONE;
        if (comp == "gzip" || comp == "gz") return SBEnhanced::SplitCompressionType::GZIP;
        if (comp == "lz4") return SBEnhanced::SplitCompressionType::LZ4;
        if (comp == "zstd") return SBEnhanced::SplitCompressionType::ZSTD;
        if (comp == "bzip2" || comp == "bz2") return SBEnhanced::SplitCompressionType::BZIP2;
        
        return SBEnhanced::SplitCompressionType::NONE;
    }
    
    bool validateOptions() {
        // Check for conflicting modes
        int mode_count = 0;
        if (options.split_mode) mode_count++;
        if (options.join_mode) mode_count++;
        if (options.validate_mode) mode_count++;
        if (options.analyze_mode) mode_count++;
        
        if (mode_count == 0 && !options.help_mode && !options.version_mode) {
            errors.push_back("No operation mode specified");
        } else if (mode_count > 1) {
            errors.push_back("Only one operation mode can be specified");
        }
        
        // Validate split mode options
        if (options.split_mode) {
            if (options.source_database.empty()) {
                errors.push_back("Source database required for split operation");
            }
            if (options.max_file_size < 1024 * 1024) {
                errors.push_back("Maximum file size must be at least 1MB");
            }
        }
        
        // Validate join mode options
        if (options.join_mode) {
            if (options.split_files.empty()) {
                errors.push_back("Split files required for join operation");
            }
            if (options.output_database.empty()) {
                errors.push_back("Output database required for join operation");
            }
        }
        
        // Validate validation mode options
        if (options.validate_mode) {
            if (options.split_files.empty()) {
                errors.push_back("Split files required for validation");
            }
        }
        
        // Validate analyze mode options
        if (options.analyze_mode) {
            if (options.split_files.empty()) {
                errors.push_back("Split files required for analysis");
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
    
    bool executeSplit() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced GSSPLIT - Database Split Operation" << std::endl;
            std::cout << "=======================================================" << std::endl;
            std::cout << "Source database: " << options.source_database << std::endl;
            std::cout << "Output prefix: " << options.output_prefix << std::endl;
            std::cout << "Maximum file size: " << formatFileSize(options.max_file_size) << std::endl;
            if (!options.output_directory.empty()) {
                std::cout << "Output directory: " << options.output_directory << std::endl;
            }
            std::cout << std::endl;
        }
        
        GSplitEnhanced splitter;
        
        // Create split options
        SBEnhanced::SplitOptions split_options;
        split_options.source_database_path = options.source_database;
        split_options.output_directory = options.output_directory;
        split_options.output_filename_prefix = options.output_prefix;
        split_options.max_file_size = options.max_file_size;
        split_options.compression = options.compression;
        split_options.compression_level = options.compression_level;
        split_options.create_checksum = options.create_checksum;
        split_options.verify_after_split = options.verify_after_split;
        split_options.overwrite_existing = options.overwrite_existing;
        split_options.description = options.description;
        
        // Set progress callback if verbose
        if (options.verbose) {
            split_options.progress_callback = [](const SBEnhanced::SplitProgress& progress) {
                std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                         << progress.getProgressPercentage() << "% - " << progress.current_file 
                         << " " << std::flush;
            };
        }
        
        SBEnhanced::SplitOperationResult result;
        bool success = splitter.performSplit(split_options, result);
        
        if (options.verbose) {
            std::cout << std::endl;  // New line after progress
        }
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Split operation completed successfully!" << std::endl;
                std::cout << "Files created: " << result.split_files.size() << std::endl;
                std::cout << "Total size: " << formatFileSize(result.detailed_stats.total_split_size) << std::endl;
                std::cout << "Duration: " << result.getDuration().count() << " ms" << std::endl;
                
                if (options.verbose) {
                    std::cout << "\nCreated files:" << std::endl;
                    for (const auto& file_info : result.split_files) {
                        std::cout << "  " << file_info.file_path 
                                 << " (" << formatFileSize(file_info.file_size) << ")";
                        if (!file_info.checksum.empty()) {
                            std::cout << " [" << file_info.checksum.substr(0, 8) << "...]";
                        }
                        std::cout << std::endl;
                    }
                }
            }
        } else {
            std::cerr << "Split operation failed!" << std::endl;
            for (const auto& error : result.errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        // Display warnings
        if (!result.warnings.empty() && !options.quiet) {
            std::cout << "\nWarnings:" << std::endl;
            for (const auto& warning : result.warnings) {
                std::cout << "Warning: " << warning << std::endl;
            }
        }
        
        return success;
    }
    
    bool executeJoin() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced GSSPLIT - Database Join Operation" << std::endl;
            std::cout << "======================================================" << std::endl;
            std::cout << "Split files: " << options.split_files.size() << " files" << std::endl;
            std::cout << "Output database: " << options.output_database << std::endl;
            std::cout << std::endl;
        }
        
        GSplitEnhanced splitter;
        
        // Create join options
        SBEnhanced::JoinOptions join_options;
        join_options.split_file_paths = options.split_files;
        join_options.output_database_path = options.output_database;
        join_options.validation_mode = options.validation_mode;
        join_options.verify_checksums = options.verify_checksums;
        join_options.verify_after_join = options.verify_after_join;
        join_options.overwrite_existing = options.overwrite_existing;
        
        // Set progress callback if verbose
        if (options.verbose) {
            join_options.progress_callback = [](const SBEnhanced::SplitProgress& progress) {
                std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                         << progress.getProgressPercentage() << "% - " << progress.current_file 
                         << " " << std::flush;
            };
        }
        
        SBEnhanced::SplitOperationResult result;
        bool success = splitter.performJoin(join_options, result);
        
        if (options.verbose) {
            std::cout << std::endl;  // New line after progress
        }
        
        if (success) {
            if (!options.quiet) {
                std::cout << "Join operation completed successfully!" << std::endl;
                std::cout << "Output database: " << options.output_database << std::endl;
                std::cout << "Files joined: " << options.split_files.size() << std::endl;
                std::cout << "Total size: " << formatFileSize(result.detailed_stats.total_joined_size) << std::endl;
                std::cout << "Duration: " << result.getDuration().count() << " ms" << std::endl;
            }
        } else {
            std::cerr << "Join operation failed!" << std::endl;
            for (const auto& error : result.errors) {
                std::cerr << "Error: " << error << std::endl;
            }
        }
        
        // Display warnings
        if (!result.warnings.empty() && !options.quiet) {
            std::cout << "\nWarnings:" << std::endl;
            for (const auto& warning : result.warnings) {
                std::cout << "Warning: " << warning << std::endl;
            }
        }
        
        return success;
    }
    
    bool executeValidate() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced GSSPLIT - Split Files Validation" << std::endl;
            std::cout << "=====================================================" << std::endl;
            std::cout << "Split files: " << options.split_files.size() << " files" << std::endl;
            std::cout << "Validation mode: " << (options.comprehensive_validation ? "Comprehensive" : "Standard") << std::endl;
            std::cout << std::endl;
        }
        
        GSplitEnhanced splitter;
        
        // Create validation options
        SBEnhanced::SplitValidationOptions validation_options;
        validation_options.split_file_paths = options.split_files;
        validation_options.validation_mode = options.validation_mode;
        validation_options.verify_checksums = options.verify_checksums;
        validation_options.generate_report = options.generate_report;
        validation_options.report_output_path = options.report_path;
        
        SBEnhanced::SplitValidationResult result;
        bool success = splitter.validateSplitFiles(validation_options, result);
        
        if (!options.quiet) {
            std::cout << "Validation Results:" << std::endl;
            std::cout << "==================" << std::endl;
            std::cout << "Files validated: " << result.files_validated << std::endl;
            std::cout << "Validation successful: " << (result.validation_successful ? "YES" : "NO") << std::endl;
            std::cout << "Total errors: " << result.validation_errors << std::endl;
            std::cout << "Checksum failures: " << result.checksum_failures << std::endl;
            std::cout << "Sequence errors: " << result.sequence_errors << std::endl;
            std::cout << "Metadata errors: " << result.metadata_errors << std::endl;
            
            if (options.verbose && !result.error_details.empty()) {
                std::cout << "\nError Details:" << std::endl;
                for (const auto& error : result.error_details) {
                    std::cout << "  - " << error << std::endl;
                }
            }
            
            if (!result.warnings.empty()) {
                std::cout << "\nWarnings:" << std::endl;
                for (const auto& warning : result.warnings) {
                    std::cout << "  - " << warning << std::endl;
                }
            }
            
            if (options.generate_report && !result.validation_report_path.empty()) {
                std::cout << "\nValidation report written to: " << result.validation_report_path << std::endl;
            }
        }
        
        return success;
    }
    
    bool executeAnalyze() {
        if (!options.quiet) {
            std::cout << "ScratchBird Enhanced GSSPLIT - Split Files Analysis" << std::endl;
            std::cout << "===================================================" << std::endl;
            std::cout << "Split files: " << options.split_files.size() << " files" << std::endl;
            std::cout << std::endl;
        }
        
        GSplitEnhanced splitter;
        
        // Create analysis options
        SBEnhanced::SplitAnalysisOptions analysis_options;
        analysis_options.split_file_paths = options.split_files;
        analysis_options.analyze_compression_efficiency = true;
        analysis_options.analyze_file_distribution = true;
        analysis_options.analyze_storage_efficiency = true;
        analysis_options.generate_detailed_report = options.generate_report;
        analysis_options.analysis_output_path = options.report_path;
        
        SBEnhanced::SplitAnalysisResult result;
        bool success = splitter.analyzeSplitFiles(analysis_options, result);
        
        if (!options.quiet) {
            std::cout << "Analysis Results:" << std::endl;
            std::cout << "=================" << std::endl;
            std::cout << "Files analyzed: " << result.files_analyzed << std::endl;
            std::cout << "Overall compression ratio: " << std::fixed << std::setprecision(2) 
                     << result.overall_compression_ratio << std::endl;
            std::cout << "Storage efficiency: " << std::fixed << std::setprecision(1) 
                     << result.storage_efficiency << "%" << std::endl;
            std::cout << "Total wasted space: " << formatFileSize(result.total_wasted_space) << std::endl;
            
            if (!result.optimization_recommendations.empty()) {
                std::cout << "\nOptimization Recommendations:" << std::endl;
                for (const auto& recommendation : result.optimization_recommendations) {
                    std::cout << "  - " << recommendation << std::endl;
                }
            }
            
            if (options.generate_report && !result.analysis_report_path.empty()) {
                std::cout << "\nAnalysis report written to: " << result.analysis_report_path << std::endl;
            }
        }
        
        return success;
    }
    
    std::string formatFileSize(uint64_t size) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index = 0;
        double size_double = static_cast<double>(size);
        
        while (size_double >= 1024.0 && unit_index < 4) {
            size_double /= 1024.0;
            unit_index++;
        }
        
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << size_double << " " << units[unit_index];
        return oss.str();
    }
    
    void showUsage(const char* program_name) {
        std::cout << "Usage: " << program_name << " [OPTIONS] OPERATION" << std::endl;
        std::cout << "Try '" << program_name << " --help' for more information." << std::endl;
    }
    
    void showVersion() {
        std::cout << "sb_gssplit version SB-T0.5.0.1 ScratchBird 0.5 f90eae0" << std::endl;
        std::cout << "ScratchBird Enhanced Database Splitting Utility" << std::endl;
        std::cout << "Copyright (C) 2025 ScratchBird Project" << std::endl;
    }
    
    void showHelp() {
        std::cout << "ScratchBird Enhanced GSSPLIT - Database Splitting Utility" << std::endl;
        std::cout << "==========================================================" << std::endl;
        std::cout << std::endl;
        std::cout << "OPERATIONS:" << std::endl;
        std::cout << "  -split              Split a database into multiple files" << std::endl;
        std::cout << "  -join               Join split files back into a database" << std::endl;
        std::cout << "  -validate           Validate split files integrity" << std::endl;
        std::cout << "  -analyze            Analyze split files for optimization" << std::endl;
        std::cout << std::endl;
        std::cout << "SPLIT OPTIONS:" << std::endl;
        std::cout << "  -db PATH            Source database file path" << std::endl;
        std::cout << "  -o PREFIX           Output filename prefix (default: split_db)" << std::endl;
        std::cout << "  -od DIR             Output directory" << std::endl;
        std::cout << "  -s SIZE             Maximum file size (default: 2G)" << std::endl;
        std::cout << "                      Supports K, M, G suffixes (e.g., 100M, 2G)" << std::endl;
        std::cout << "  -c TYPE             Compression type: none, gzip, lz4, zstd, bzip2" << std::endl;
        std::cout << "  -cl LEVEL           Compression level (1-9, default: 6)" << std::endl;
        std::cout << "  -checksum           Create checksum files for validation" << std::endl;
        std::cout << "  -verify             Verify split files after creation" << std::endl;
        std::cout << "  -overwrite          Overwrite existing files" << std::endl;
        std::cout << std::endl;
        std::cout << "JOIN OPTIONS:" << std::endl;
        std::cout << "  -f FILES            Split files to join (space-separated)" << std::endl;
        std::cout << "  -out PATH           Output database file path" << std::endl;
        std::cout << "  -verify-join        Verify joined database integrity" << std::endl;
        std::cout << "  -overwrite          Overwrite existing output file" << std::endl;
        std::cout << std::endl;
        std::cout << "VALIDATION OPTIONS:" << std::endl;
        std::cout << "  -f FILES            Split files to validate (space-separated)" << std::endl;
        std::cout << "  -comprehensive      Perform comprehensive validation" << std::endl;
        std::cout << "  -report [PATH]      Generate validation report" << std::endl;
        std::cout << std::endl;
        std::cout << "ANALYSIS OPTIONS:" << std::endl;
        std::cout << "  -f FILES            Split files to analyze (space-separated)" << std::endl;
        std::cout << "  -report [PATH]      Generate analysis report" << std::endl;
        std::cout << std::endl;
        std::cout << "GENERAL OPTIONS:" << std::endl;
        std::cout << "  -desc TEXT          Description for split operation" << std::endl;
        std::cout << "  -verbose            Verbose output with progress information" << std::endl;
        std::cout << "  -quiet              Suppress non-error output" << std::endl;
        std::cout << "  -h, --help          Show this help message" << std::endl;
        std::cout << "  -v, --version       Show version information" << std::endl;
        std::cout << std::endl;
        std::cout << "EXAMPLES:" << std::endl;
        std::cout << "  # Split database into 100MB files with compression" << std::endl;
        std::cout << "  sb_gssplit -split -db mydb.fdb -s 100M -c gzip -checksum -verify" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Join split files back to database" << std::endl;
        std::cout << "  sb_gssplit -join -f split_db.001 split_db.002 split_db.003 -out mydb_restored.fdb" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Validate split files with comprehensive checking" << std::endl;
        std::cout << "  sb_gssplit -validate -f split_db.* -comprehensive -report validation.txt" << std::endl;
        std::cout << std::endl;
        std::cout << "  # Analyze split files for optimization opportunities" << std::endl;
        std::cout << "  sb_gssplit -analyze -f split_db.* -report analysis.txt" << std::endl;
        std::cout << std::endl;
        std::cout << "ORIGINAL GSSPLIT COMPATIBILITY:" << std::endl;
        std::cout << "  sb_gssplit -split database.fdb prefix 2147483648" << std::endl;
        std::cout << "  sb_gssplit -join file1 file2 file3 output.fdb" << std::endl;
        std::cout << std::endl;
        std::cout << "For more information, see the ScratchBird documentation." << std::endl;
    }
};

// Progress display helper
class ProgressDisplay {
private:
    bool enabled;
    std::chrono::steady_clock::time_point last_update;
    
public:
    ProgressDisplay(bool enable) : enabled(enable) {
        last_update = std::chrono::steady_clock::now();
    }
    
    void update(const SBEnhanced::SplitProgress& progress) {
        if (!enabled) return;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update);
        
        // Update every 100ms to avoid too frequent updates
        if (elapsed.count() < 100) return;
        
        last_update = now;
        
        std::cout << "\r[" << std::fixed << std::setprecision(1) 
                 << progress.getProgressPercentage() << "%] ";
        
        // Progress bar
        int bar_width = 30;
        int progress_chars = static_cast<int>(progress.getProgressPercentage() / 100.0 * bar_width);
        
        std::cout << "[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < progress_chars) {
                std::cout << "=";
            } else if (i == progress_chars) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] ";
        
        // Current file
        if (!progress.current_file.empty()) {
            std::string filename = fs::path(progress.current_file).filename().string();
            if (filename.length() > 20) {
                filename = "..." + filename.substr(filename.length() - 17);
            }
            std::cout << filename;
        }
        
        // ETA
        auto eta = progress.getEstimatedTimeRemaining();
        if (eta.count() > 0) {
            std::cout << " (ETA: " << eta.count() << "s)";
        }
        
        std::cout << std::flush;
    }
    
    void finish() {
        if (enabled) {
            std::cout << std::endl;
        }
    }
};

// Main function
int main(int argc, char* argv[]) {
    try {
        GSplitCommandParser parser;
        
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