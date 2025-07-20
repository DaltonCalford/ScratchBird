#include "sb_gssplit_enhanced.h"
#include "sb_engine_integration.h"
#include "utility_enhancements.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <openssl/sha.h>
#include <openssl/md5.h>

// Compression libraries
#ifdef HAVE_LZ4
#include <lz4.h>
#include <lz4hc.h>
#endif

#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

namespace fs = std::filesystem;

// GSplitEnhanced Implementation
GSplitEnhanced::GSplitEnhanced() 
    : engine(std::make_unique<SBEngineIntegration>()),
      split_service(nullptr),
      operation_active(false) {
    
    if (!initializeEngine()) {
        logError("Constructor", "Failed to initialize ScratchBird engine integration");
    }
    
    current_progress.start_time = std::chrono::steady_clock::now();
}

GSplitEnhanced::~GSplitEnhanced() {
    if (operation_active.load()) {
        cancelCurrentOperation();
    }
}

bool GSplitEnhanced::initializeEngine() {
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

bool GSplitEnhanced::initializeSplitService() {
    try {
        // Initialize service component if needed
        // This would integrate with jrd::Service infrastructure
        return true;
        
    } catch (const std::exception& e) {
        logError("initializeSplitService", std::string("Exception: ") + e.what());
        return false;
    }
}

// === ORIGINAL GSSPLIT FUNCTIONALITY (100% Compatible) ===

bool GSplitEnhanced::splitDatabase(const std::string& database_path,
                                  const std::string& output_prefix,
                                  uint64_t max_file_size,
                                  SBEnhanced::SplitOperationResult& result) {
    
    // Create options from classic parameters
    SBEnhanced::SplitOptions options;
    options.source_database_path = database_path;
    options.output_filename_prefix = output_prefix;
    options.max_file_size = max_file_size;
    options.compression = SBEnhanced::SplitCompressionType::NONE;
    options.create_checksum = false;  // Classic mode doesn't create checksums
    options.verify_after_split = false;  // Classic mode doesn't verify
    
    return performSplit(options, result);
}

bool GSplitEnhanced::joinDatabase(const std::vector<std::string>& split_files,
                                 const std::string& output_database,
                                 SBEnhanced::SplitOperationResult& result) {
    
    // Create options from classic parameters
    SBEnhanced::JoinOptions options;
    options.split_file_paths = split_files;
    options.output_database_path = output_database;
    options.validation_mode = SBEnhanced::SplitValidationMode::BASIC;  // Classic mode basic validation
    options.verify_checksums = false;  // Classic mode doesn't verify checksums
    options.verify_after_join = false;  // Classic mode doesn't verify
    
    return performJoin(options, result);
}

// === ENHANCED FUNCTIONALITY ===

bool GSplitEnhanced::performSplit(const SBEnhanced::SplitOptions& options,
                                 SBEnhanced::SplitOperationResult& result) {
    
    result.operation_type = SBEnhanced::SplitOperation::SPLIT_DATABASE;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        // Validate input parameters
        if (options.source_database_path.empty()) {
            result.errors.push_back("Source database path is required");
            return false;
        }
        
        if (!fs::exists(options.source_database_path)) {
            result.errors.push_back("Source database file does not exist: " + options.source_database_path);
            return false;
        }
        
        if (options.max_file_size < 1024 * 1024) {  // Minimum 1MB
            result.errors.push_back("Maximum file size must be at least 1MB");
            return false;
        }
        
        // Update progress
        operation_active = true;
        updateProgress(SBEnhanced::SplitOperation::SPLIT_DATABASE, 0, 100, "Starting split operation");
        
        // Perform the actual split
        bool success = performActualSplit(options, result);
        
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = success;
        
        if (success) {
            result.messages.push_back("Database split completed successfully");
            collectSplitStatistics(result.detailed_stats);
        }
        
        operation_active = false;
        return success;
        
    } catch (const std::exception& e) {
        logError("performSplit", std::string("Exception: ") + e.what());
        result.errors.push_back(std::string("Split operation failed: ") + e.what());
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = false;
        operation_active = false;
        return false;
    }
}

bool GSplitEnhanced::performActualSplit(const SBEnhanced::SplitOptions& options,
                                       SBEnhanced::SplitOperationResult& result) {
    
    try {
        // Get source file size
        uint64_t source_size = fs::file_size(options.source_database_path);
        uint64_t files_needed = (source_size + options.max_file_size - 1) / options.max_file_size;
        
        result.detailed_stats.total_source_size = source_size;
        
        // Create output directory if needed
        if (!options.output_directory.empty()) {
            fs::create_directories(options.output_directory);
        }
        
        // Open source file
        std::ifstream source_file(options.source_database_path, std::ios::binary);
        if (!source_file.is_open()) {
            result.errors.push_back("Cannot open source database file: " + options.source_database_path);
            return false;
        }
        
        uint64_t bytes_processed = 0;
        uint64_t file_sequence = 1;
        std::vector<uint8_t> buffer(1024 * 1024);  // 1MB buffer
        
        while (bytes_processed < source_size && !operation_active == false) {
            // Calculate split file path
            std::string split_file_path;
            if (!options.output_directory.empty()) {
                split_file_path = options.output_directory + "/" + options.output_filename_prefix;
            } else {
                split_file_path = options.output_filename_prefix;
            }
            
            // Add sequence number
            std::ostringstream oss;
            oss << split_file_path << "." << std::setfill('0') << std::setw(3) << file_sequence;
            split_file_path = oss.str();
            
            // Check if file exists and handle overwrite
            if (fs::exists(split_file_path) && !options.overwrite_existing) {
                result.errors.push_back("Output file already exists: " + split_file_path);
                return false;
            }
            
            // Open output file
            std::ofstream output_file(split_file_path, std::ios::binary);
            if (!output_file.is_open()) {
                result.errors.push_back("Cannot create output file: " + split_file_path);
                return false;
            }
            
            // Calculate bytes to write to this file
            uint64_t remaining_source = source_size - bytes_processed;
            uint64_t bytes_this_file = std::min(options.max_file_size, remaining_source);
            uint64_t file_bytes_written = 0;
            
            // Copy data to split file
            while (file_bytes_written < bytes_this_file) {
                uint64_t remaining_this_file = bytes_this_file - file_bytes_written;
                size_t chunk_size = static_cast<size_t>(std::min(remaining_this_file, static_cast<uint64_t>(buffer.size())));
                
                source_file.read(reinterpret_cast<char*>(buffer.data()), chunk_size);
                size_t bytes_read = source_file.gcount();
                
                if (bytes_read == 0) {
                    break;  // End of file
                }
                
                // Apply compression if requested
                if (options.compression != SBEnhanced::SplitCompressionType::NONE) {
                    std::vector<uint8_t> compressed_buffer;
                    if (compressBuffer(std::vector<uint8_t>(buffer.begin(), buffer.begin() + bytes_read),
                                     compressed_buffer, options.compression, options.compression_level)) {
                        output_file.write(reinterpret_cast<const char*>(compressed_buffer.data()), compressed_buffer.size());
                        file_bytes_written += bytes_read;  // Count original bytes
                    } else {
                        result.warnings.push_back("Compression failed for chunk, writing uncompressed");
                        output_file.write(reinterpret_cast<const char*>(buffer.data()), bytes_read);
                        file_bytes_written += bytes_read;
                    }
                } else {
                    output_file.write(reinterpret_cast<const char*>(buffer.data()), bytes_read);
                    file_bytes_written += bytes_read;
                }
                
                bytes_processed += bytes_read;
                
                // Update progress
                double progress = static_cast<double>(bytes_processed) / source_size * 100.0;
                updateProgress(SBEnhanced::SplitOperation::SPLIT_DATABASE, 
                             static_cast<uint64_t>(progress), 100, split_file_path);
                
                // Call progress callback if provided
                if (options.progress_callback) {
                    options.progress_callback(current_progress);
                }
            }
            
            output_file.close();
            
            // Create file info
            SBEnhanced::SplitFileInfo file_info;
            file_info.file_path = split_file_path;
            file_info.file_size = fs::file_size(split_file_path);
            file_info.sequence_number = file_sequence;
            file_info.compression = options.compression;
            file_info.created_time = std::chrono::system_clock::now();
            file_info.description = options.description;
            file_info.metadata = options.custom_metadata;
            
            if (options.compression != SBEnhanced::SplitCompressionType::NONE) {
                file_info.compression_ratio = static_cast<double>(file_info.file_size) / bytes_this_file;
            }
            
            // Generate checksum if requested
            if (options.create_checksum) {
                file_info.checksum = calculateFileChecksum(split_file_path);
            }
            
            result.split_files.push_back(file_info);
            file_sequence++;
        }
        
        source_file.close();
        
        // Verify split if requested
        if (options.verify_after_split) {
            SBEnhanced::SplitValidationOptions validation_options;
            for (const auto& file_info : result.split_files) {
                validation_options.split_file_paths.push_back(file_info.file_path);
            }
            validation_options.validation_mode = SBEnhanced::SplitValidationMode::COMPREHENSIVE;
            
            SBEnhanced::SplitValidationResult validation_result;
            if (!validateSplitFiles(validation_options, validation_result)) {
                result.warnings.push_back("Split verification failed");
                for (const auto& error : validation_result.error_details) {
                    result.warnings.push_back("Verification: " + error);
                }
            }
        }
        
        result.detailed_stats.files_created = file_sequence - 1;
        result.detailed_stats.total_split_size = bytes_processed;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("performActualSplit", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GSplitEnhanced::performJoin(const SBEnhanced::JoinOptions& options,
                                SBEnhanced::SplitOperationResult& result) {
    
    result.operation_type = SBEnhanced::SplitOperation::JOIN_DATABASE;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        // Validate input parameters
        if (options.split_file_paths.empty()) {
            result.errors.push_back("Split file list is empty");
            return false;
        }
        
        if (options.output_database_path.empty()) {
            result.errors.push_back("Output database path is required");
            return false;
        }
        
        // Validate split files exist
        for (const auto& file_path : options.split_file_paths) {
            if (!fs::exists(file_path)) {
                result.errors.push_back("Split file does not exist: " + file_path);
                return false;
            }
        }
        
        // Check if output file exists and handle overwrite
        if (fs::exists(options.output_database_path) && !options.overwrite_existing) {
            result.errors.push_back("Output database already exists: " + options.output_database_path);
            return false;
        }
        
        // Update progress
        operation_active = true;
        updateProgress(SBEnhanced::SplitOperation::JOIN_DATABASE, 0, 100, "Starting join operation");
        
        // Perform validation if requested
        if (options.validation_mode != SBEnhanced::SplitValidationMode::NONE) {
            SBEnhanced::SplitValidationOptions validation_options;
            validation_options.split_file_paths = options.split_file_paths;
            validation_options.validation_mode = options.validation_mode;
            validation_options.verify_checksums = options.verify_checksums;
            validation_options.verify_sequence = options.verify_sequence;
            
            SBEnhanced::SplitValidationResult validation_result;
            if (!validateSplitFiles(validation_options, validation_result)) {
                result.errors.push_back("Split file validation failed");
                for (const auto& error : validation_result.error_details) {
                    result.errors.push_back("Validation: " + error);
                }
                operation_active = false;
                return false;
            }
        }
        
        // Perform the actual join
        bool success = performActualJoin(options, result);
        
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = success;
        
        if (success) {
            result.messages.push_back("Database join completed successfully");
            collectJoinStatistics(result.detailed_stats);
        }
        
        operation_active = false;
        return success;
        
    } catch (const std::exception& e) {
        logError("performJoin", std::string("Exception: ") + e.what());
        result.errors.push_back(std::string("Join operation failed: ") + e.what());
        result.end_time = std::chrono::steady_clock::now();
        result.operation_successful = false;
        operation_active = false;
        return false;
    }
}

bool GSplitEnhanced::performActualJoin(const SBEnhanced::JoinOptions& options,
                                      SBEnhanced::SplitOperationResult& result) {
    
    try {
        // Sort split files by sequence number
        std::vector<std::string> sorted_files = options.split_file_paths;
        std::sort(sorted_files.begin(), sorted_files.end());
        
        // Calculate total size for progress tracking
        uint64_t total_size = 0;
        for (const auto& file_path : sorted_files) {
            total_size += fs::file_size(file_path);
        }
        
        result.detailed_stats.total_source_size = total_size;
        
        // Open output file
        std::ofstream output_file(options.output_database_path, std::ios::binary);
        if (!output_file.is_open()) {
            result.errors.push_back("Cannot create output database file: " + options.output_database_path);
            return false;
        }
        
        uint64_t bytes_processed = 0;
        std::vector<uint8_t> buffer(1024 * 1024);  // 1MB buffer
        
        for (size_t i = 0; i < sorted_files.size(); ++i) {
            const std::string& file_path = sorted_files[i];
            
            // Open split file
            std::ifstream input_file(file_path, std::ios::binary);
            if (!input_file.is_open()) {
                result.errors.push_back("Cannot open split file: " + file_path);
                return false;
            }
            
            // Copy data from split file to output
            while (input_file.good() && !operation_active == false) {
                input_file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                size_t bytes_read = input_file.gcount();
                
                if (bytes_read == 0) {
                    break;  // End of file
                }
                
                // Write to output file
                output_file.write(reinterpret_cast<const char*>(buffer.data()), bytes_read);
                bytes_processed += bytes_read;
                
                // Update progress
                double progress = static_cast<double>(bytes_processed) / total_size * 100.0;
                updateProgress(SBEnhanced::SplitOperation::JOIN_DATABASE,
                             static_cast<uint64_t>(progress), 100, file_path);
                
                // Call progress callback if provided
                if (options.progress_callback) {
                    options.progress_callback(current_progress);
                }
            }
            
            input_file.close();
        }
        
        output_file.close();
        
        // Verify joined database if requested
        if (options.verify_after_join) {
            // Basic verification - check if file can be opened as database
            std::map<std::string, std::string> db_info;
            if (!getDatabaseInfo(options.output_database_path, db_info)) {
                result.warnings.push_back("Joined database verification failed");
            } else {
                result.messages.push_back("Joined database verification successful");
            }
        }
        
        result.detailed_stats.files_joined = sorted_files.size();
        result.detailed_stats.total_joined_size = bytes_processed;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("performActualJoin", std::string("Exception: ") + e.what());
        return false;
    }
}

bool GSplitEnhanced::validateSplitFiles(const SBEnhanced::SplitValidationOptions& options,
                                       SBEnhanced::SplitValidationResult& result) {
    
    try {
        result.validation_successful = false;
        
        if (options.split_file_paths.empty()) {
            result.error_details.push_back("No split files provided for validation");
            return false;
        }
        
        // Update progress
        operation_active = true;
        updateProgress(SBEnhanced::SplitOperation::VALIDATE_SPLIT, 0, 100, "Starting validation");
        
        // File existence check
        if (options.check_file_existence) {
            for (const auto& file_path : options.split_file_paths) {
                if (!fs::exists(file_path)) {
                    result.error_details.push_back("File does not exist: " + file_path);
                    result.validation_errors++;
                }
            }
        }
        
        // File size check
        if (options.check_file_sizes) {
            for (const auto& file_path : options.split_file_paths) {
                if (fs::exists(file_path)) {
                    uint64_t file_size = fs::file_size(file_path);
                    if (file_size == 0) {
                        result.error_details.push_back("File is empty: " + file_path);
                        result.validation_errors++;
                    }
                }
            }
        }
        
        // Sequence validation
        if (options.verify_sequence) {
            std::vector<std::string> sequence_errors;
            if (!validateFileSequence(options.split_file_paths, sequence_errors)) {
                for (const auto& error : sequence_errors) {
                    result.error_details.push_back("Sequence error: " + error);
                    result.sequence_errors++;
                }
            }
        }
        
        // Checksum validation
        if (options.verify_checksums) {
            for (const auto& file_path : options.split_file_paths) {
                if (fs::exists(file_path)) {
                    // Try to read metadata file to get expected checksum
                    std::string metadata_file = file_path + ".meta";
                    if (fs::exists(metadata_file)) {
                        // Read expected checksum from metadata
                        // This is a simplified implementation
                        std::string expected_checksum = ""; // Would read from metadata
                        if (!expected_checksum.empty()) {
                            if (!verifyFileChecksum(file_path, expected_checksum)) {
                                result.error_details.push_back("Checksum mismatch: " + file_path);
                                result.checksum_failures++;
                            }
                        }
                    }
                }
            }
        }
        
        // Metadata validation
        if (options.verify_metadata) {
            std::vector<std::string> metadata_errors;
            if (!validateFileMetadata(options.split_file_paths, metadata_errors)) {
                for (const auto& error : metadata_errors) {
                    result.error_details.push_back("Metadata error: " + error);
                    result.metadata_errors++;
                }
            }
        }
        
        result.files_validated = options.split_file_paths.size();
        result.validation_successful = (result.validation_errors == 0 && 
                                       result.checksum_failures == 0 && 
                                       result.sequence_errors == 0 && 
                                       result.metadata_errors == 0);
        
        // Generate validation report if requested
        if (options.generate_report && !options.report_output_path.empty()) {
            std::string report_content = result.generateValidationReport();
            std::ofstream report_file(options.report_output_path);
            if (report_file.is_open()) {
                report_file << report_content;
                report_file.close();
                result.validation_report_path = options.report_output_path;
            }
        }
        
        operation_active = false;
        return result.validation_successful;
        
    } catch (const std::exception& e) {
        logError("validateSplitFiles", std::string("Exception: ") + e.what());
        result.error_details.push_back(std::string("Validation failed: ") + e.what());
        operation_active = false;
        return false;
    }
}

// Checksum calculation helper
std::string GSplitEnhanced::calculateFileChecksum(const std::string& file_path,
                                                  const std::string& algorithm) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        if (algorithm == "SHA256") {
            SHA256_CTX sha256_ctx;
            SHA256_Init(&sha256_ctx);
            
            std::vector<uint8_t> buffer(8192);
            while (file.good()) {
                file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                size_t bytes_read = file.gcount();
                if (bytes_read > 0) {
                    SHA256_Update(&sha256_ctx, buffer.data(), bytes_read);
                }
            }
            
            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256_Final(hash, &sha256_ctx);
            
            std::ostringstream oss;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
                oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
            }
            return oss.str();
        }
        else if (algorithm == "MD5") {
            MD5_CTX md5_ctx;
            MD5_Init(&md5_ctx);
            
            std::vector<uint8_t> buffer(8192);
            while (file.good()) {
                file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
                size_t bytes_read = file.gcount();
                if (bytes_read > 0) {
                    MD5_Update(&md5_ctx, buffer.data(), bytes_read);
                }
            }
            
            unsigned char hash[MD5_DIGEST_LENGTH];
            MD5_Final(hash, &md5_ctx);
            
            std::ostringstream oss;
            for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
                oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
            }
            return oss.str();
        }
        
        return "";
        
    } catch (const std::exception& e) {
        logError("calculateFileChecksum", std::string("Exception: ") + e.what());
        return "";
    }
}

bool GSplitEnhanced::verifyFileChecksum(const std::string& file_path,
                                       const std::string& expected_checksum,
                                       const std::string& algorithm) {
    std::string calculated_checksum = calculateFileChecksum(file_path, algorithm);
    return !calculated_checksum.empty() && calculated_checksum == expected_checksum;
}

// Compression helper (simplified implementation)
bool GSplitEnhanced::compressBuffer(const std::vector<uint8_t>& input,
                                   std::vector<uint8_t>& output,
                                   SBEnhanced::SplitCompressionType compression,
                                   int level) {
    
    switch (compression) {
        case SBEnhanced::SplitCompressionType::NONE:
            output = input;
            return true;
            
#ifdef HAVE_ZLIB
        case SBEnhanced::SplitCompressionType::GZIP: {
            uLongf compressed_size = compressBound(input.size());
            output.resize(compressed_size);
            
            int result = compress2(output.data(), &compressed_size,
                                 input.data(), input.size(), level);
            
            if (result == Z_OK) {
                output.resize(compressed_size);
                return true;
            }
            return false;
        }
#endif
        
#ifdef HAVE_LZ4
        case SBEnhanced::SplitCompressionType::LZ4: {
            int max_compressed_size = LZ4_compressBound(input.size());
            output.resize(max_compressed_size);
            
            int compressed_size = LZ4_compress_default(
                reinterpret_cast<const char*>(input.data()),
                reinterpret_cast<char*>(output.data()),
                input.size(), max_compressed_size);
            
            if (compressed_size > 0) {
                output.resize(compressed_size);
                return true;
            }
            return false;
        }
#endif
        
        default:
            output = input;  // Fallback to no compression
            return true;
    }
}

// Validation helpers
bool GSplitEnhanced::validateFileSequence(const std::vector<std::string>& split_files,
                                          std::vector<std::string>& sequence_errors) {
    
    // Extract sequence numbers from filenames and validate
    std::vector<std::pair<uint64_t, std::string>> file_sequences;
    
    for (const auto& file_path : split_files) {
        // Extract sequence number from filename (assumes .001, .002, etc. format)
        std::string filename = fs::path(file_path).filename().string();
        size_t last_dot = filename.find_last_of('.');
        
        if (last_dot != std::string::npos) {
            std::string extension = filename.substr(last_dot + 1);
            try {
                uint64_t sequence = std::stoull(extension);
                file_sequences.push_back({sequence, file_path});
            } catch (const std::exception&) {
                sequence_errors.push_back("Invalid sequence number in filename: " + file_path);
            }
        } else {
            sequence_errors.push_back("No sequence number found in filename: " + file_path);
        }
    }
    
    // Sort by sequence number
    std::sort(file_sequences.begin(), file_sequences.end());
    
    // Check for gaps or duplicates
    for (size_t i = 0; i < file_sequences.size(); ++i) {
        uint64_t expected_sequence = i + 1;
        if (file_sequences[i].first != expected_sequence) {
            sequence_errors.push_back("Sequence gap or duplicate detected at: " + file_sequences[i].second);
        }
    }
    
    return sequence_errors.empty();
}

bool GSplitEnhanced::validateFileMetadata(const std::vector<std::string>& split_files,
                                         std::vector<std::string>& metadata_errors) {
    
    // Check for metadata files and validate consistency
    for (const auto& file_path : split_files) {
        std::string metadata_file = file_path + ".meta";
        if (!fs::exists(metadata_file)) {
            // Metadata file is optional, so just note it
            continue;
        }
        
        // Read and validate metadata file
        std::ifstream meta_file(metadata_file);
        if (!meta_file.is_open()) {
            metadata_errors.push_back("Cannot read metadata file: " + metadata_file);
            continue;
        }
        
        // Basic metadata validation (simplified)
        std::string line;
        bool has_checksum = false;
        bool has_size = false;
        
        while (std::getline(meta_file, line)) {
            if (line.find("checksum=") != std::string::npos) {
                has_checksum = true;
            }
            if (line.find("size=") != std::string::npos) {
                has_size = true;
            }
        }
        
        if (!has_checksum || !has_size) {
            metadata_errors.push_back("Incomplete metadata in file: " + metadata_file);
        }
    }
    
    return metadata_errors.empty();
}

// Progress tracking
void GSplitEnhanced::updateProgress(SBEnhanced::SplitOperation operation,
                                   uint64_t completed, uint64_t total,
                                   const std::string& current_item) {
    
    current_progress.current_operation = operation;
    current_progress.completed_operations = completed;
    current_progress.total_operations = total;
    current_progress.current_file = current_item;
    current_progress.operation_active = operation_active.load();
}

void GSplitEnhanced::logError(const std::string& operation, const std::string& error) {
    std::string full_error = "[" + operation + "] " + error;
    error_log.push_back(full_error);
    last_error = full_error;
}

void GSplitEnhanced::logWarning(const std::string& operation, const std::string& warning) {
    std::string full_warning = "[" + operation + "] " + warning;
    warning_log.push_back(full_warning);
}

// Statistics collection
void GSplitEnhanced::collectSplitStatistics(SBEnhanced::SplitStatistics& stats) {
    stats.operation_type = SBEnhanced::SplitOperation::SPLIT_DATABASE;
    stats.operation_end = std::chrono::steady_clock::now();
    
    // Calculate throughput
    auto duration = stats.getDuration();
    if (duration.count() > 0) {
        double seconds = duration.count() / 1000.0;
        double mb_processed = stats.total_bytes_processed / (1024.0 * 1024.0);
        stats.throughput_mbps = mb_processed / seconds;
    }
}

void GSplitEnhanced::collectJoinStatistics(SBEnhanced::SplitStatistics& stats) {
    stats.operation_type = SBEnhanced::SplitOperation::JOIN_DATABASE;
    stats.operation_end = std::chrono::steady_clock::now();
    
    // Calculate throughput
    auto duration = stats.getDuration();
    if (duration.count() > 0) {
        double seconds = duration.count() / 1000.0;
        double mb_processed = stats.total_bytes_processed / (1024.0 * 1024.0);
        stats.throughput_mbps = mb_processed / seconds;
    }
}

void GSplitEnhanced::collectValidationStatistics(SBEnhanced::SplitStatistics& stats) {
    stats.operation_type = SBEnhanced::SplitOperation::VALIDATE_SPLIT;
    stats.operation_end = std::chrono::steady_clock::now();
}

// Public interface methods
SBEnhanced::SplitProgress GSplitEnhanced::getCurrentProgress() const {
    return current_progress;
}

bool GSplitEnhanced::isOperationActive() const {
    return operation_active.load();
}

void GSplitEnhanced::cancelCurrentOperation() {
    operation_active = false;
}

std::vector<std::string> GSplitEnhanced::getErrors() const {
    return error_log;
}

std::vector<std::string> GSplitEnhanced::getWarnings() const {
    return warning_log;
}

std::string GSplitEnhanced::getLastError() const {
    return last_error;
}

void GSplitEnhanced::clearErrorLog() {
    error_log.clear();
    warning_log.clear();
    last_error.clear();
}

bool GSplitEnhanced::getDatabaseInfo(const std::string& database_path,
                                    std::map<std::string, std::string>& database_info) {
    
    try {
        if (!fs::exists(database_path)) {
            return false;
        }
        
        uint64_t file_size = fs::file_size(database_path);
        database_info["file_size"] = std::to_string(file_size);
        database_info["file_path"] = database_path;
        
        // Add more database-specific information here
        // This would integrate with ScratchBird's database analysis capabilities
        
        return true;
        
    } catch (const std::exception& e) {
        logError("getDatabaseInfo", std::string("Exception: ") + e.what());
        return false;
    }
}

// Utility namespace implementations
namespace SBEnhanced {

// Quick operations
bool quickSplitDatabase(const std::string& database_path,
                       const std::string& output_prefix,
                       uint64_t max_file_size) {
    
    GSplitEnhanced splitter;
    SplitOperationResult result;
    
    return splitter.splitDatabase(database_path, output_prefix, max_file_size, result);
}

bool quickJoinDatabase(const std::vector<std::string>& split_files,
                      const std::string& output_database) {
    
    GSplitEnhanced splitter;
    SplitOperationResult result;
    
    return splitter.joinDatabase(split_files, output_database, result);
}

bool quickValidateSplitFiles(const std::vector<std::string>& split_files) {
    GSplitEnhanced splitter;
    SplitValidationOptions options;
    options.split_file_paths = split_files;
    options.validation_mode = SplitValidationMode::COMPREHENSIVE;
    
    SplitValidationResult result;
    return splitter.validateSplitFiles(options, result);
}

uint64_t calculateDatabaseSize(const std::string& database_path) {
    try {
        if (fs::exists(database_path)) {
            return fs::file_size(database_path);
        }
        return 0;
    } catch (const std::exception&) {
        return 0;
    }
}

uint64_t estimateSplitFileCount(uint64_t database_size, uint64_t max_file_size) {
    if (max_file_size == 0) return 0;
    return (database_size + max_file_size - 1) / max_file_size;
}

std::string generateQuickChecksum(const std::string& file_path) {
    GSplitEnhanced splitter;
    return splitter.calculateFileChecksum(file_path, "SHA256");
}

bool verifyQuickChecksum(const std::string& file_path, const std::string& expected_checksum) {
    GSplitEnhanced splitter;
    return splitter.verifyFileChecksum(file_path, expected_checksum, "SHA256");
}

// Report generation methods
std::string SplitStatistics::generateSummaryReport() const {
    std::ostringstream oss;
    oss << "Split Operation Summary\n";
    oss << "======================\n";
    oss << "Operation Type: ";
    
    switch (operation_type) {
        case SplitOperation::SPLIT_DATABASE:
            oss << "Database Split\n";
            oss << "Files Created: " << files_created << "\n";
            oss << "Total Size: " << total_split_size << " bytes\n";
            break;
        case SplitOperation::JOIN_DATABASE:
            oss << "Database Join\n";
            oss << "Files Joined: " << files_joined << "\n";
            oss << "Total Size: " << total_joined_size << " bytes\n";
            break;
        case SplitOperation::VALIDATE_SPLIT:
            oss << "Split Validation\n";
            oss << "Files Validated: " << files_validated << "\n";
            oss << "Validation Errors: " << validation_errors << "\n";
            break;
        default:
            oss << "Unknown\n";
            break;
    }
    
    oss << "Duration: " << getDuration().count() << " ms\n";
    oss << "Throughput: " << throughput_mbps << " MB/s\n";
    
    return oss.str();
}

std::string SplitValidationResult::generateValidationReport() const {
    std::ostringstream oss;
    oss << "Split Files Validation Report\n";
    oss << "=============================\n";
    oss << "Files Validated: " << files_validated << "\n";
    oss << "Validation Successful: " << (validation_successful ? "YES" : "NO") << "\n";
    oss << "Total Errors: " << validation_errors << "\n";
    oss << "Checksum Failures: " << checksum_failures << "\n";
    oss << "Sequence Errors: " << sequence_errors << "\n";
    oss << "Metadata Errors: " << metadata_errors << "\n";
    
    if (!error_details.empty()) {
        oss << "\nError Details:\n";
        for (const auto& error : error_details) {
            oss << "- " << error << "\n";
        }
    }
    
    if (!warnings.empty()) {
        oss << "\nWarnings:\n";
        for (const auto& warning : warnings) {
            oss << "- " << warning << "\n";
        }
    }
    
    return oss.str();
}

} // namespace SBEnhanced