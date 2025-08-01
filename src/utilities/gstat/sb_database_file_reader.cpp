#include "sb_database_file_reader.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

using namespace SBEnhanced;

// Constructor
DatabaseFileReader::DatabaseFileReader() {
    file_stream = std::make_unique<std::ifstream>();
    clearErrors();
    clearWarnings();
}

// Destructor
DatabaseFileReader::~DatabaseFileReader() {
    closeDatabase();
}

// Open database file
bool DatabaseFileReader::openDatabase(const std::string& path) {
    try {
        database_path = path;
        
        // Check if file exists
        struct stat file_stat;
        if (stat(path.c_str(), &file_stat) != 0) {
            logError("Database file does not exist: " + path);
            return false;
        }
        
        // Check if it's a regular file
        if (!S_ISREG(file_stat.st_mode)) {
            logError("Path is not a regular file: " + path);
            return false;
        }
        
        // Open file in binary mode
        file_stream->open(path, std::ios::binary | std::ios::in);
        if (!file_stream->is_open()) {
            logError("Failed to open database file: " + path);
            return false;
        }
        
        // Get file size
        file_stream->seekg(0, std::ios::end);
        auto file_size = file_stream->tellg();
        file_stream->seekg(0, std::ios::beg);
        
        if (file_size < 8192) { // Minimum database file size
            logError("File too small to be a valid database: " + std::to_string(file_size) + " bytes");
            closeDatabase();
            return false;
        }
        
        file_open = true;
        
        // Read and validate header
        if (!readDatabaseHeader()) {
            logError("Failed to read database header");
            closeDatabase();
            return false;
        }
        
        total_pages = static_cast<uint32_t>(file_size / page_size);
        
        std::cout << "Opened database file: " << path << std::endl;
        std::cout << "File size: " << file_size << " bytes" << std::endl;
        std::cout << "Page size: " << page_size << " bytes" << std::endl;
        std::cout << "Total pages: " << total_pages << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception opening database: " + std::string(e.what()));
        return false;
    }
}

// Close database file
bool DatabaseFileReader::closeDatabase() {
    if (file_stream && file_stream->is_open()) {
        file_stream->close();
    }
    
    file_open = false;
    header_read = false;
    page_size = 0;
    total_pages = 0;
    clearCache();
    
    return true;
}

// Check if database is open
bool DatabaseFileReader::isOpen() const {
    return file_open && file_stream && file_stream->is_open();
}

// Read database header
bool DatabaseFileReader::readDatabaseHeader() {
    try {
        if (!isOpen()) {
            logError("Database file not open");
            return false;
        }
        
        std::vector<uint8_t> header_page(8192); // Initial read with max possible page size
        
        file_stream->seekg(0, std::ios::beg);
        file_stream->read(reinterpret_cast<char*>(header_page.data()), 8192);
        
        if (file_stream->gcount() < 8192) {
            logError("Failed to read complete header page");
            return false;
        }
        
        if (!parseDatabaseHeaderPage(header_page)) {
            logError("Failed to parse database header");
            return false;
        }
        
        header_read = true;
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception reading database header: " + std::string(e.what()));
        return false;
    }
}

// Parse database header page
bool DatabaseFileReader::parseDatabaseHeaderPage(const std::vector<uint8_t>& page_data) {
    try {
        // Check page type (should be 1 for header)
        if (page_data[0] != 1) {
            logError("Invalid header page type: " + std::to_string(page_data[0]));
            return false;
        }
        
        // Read page size (offset 16-17)
        header.page_size = readUInt16(page_data, 16);
        page_size = header.page_size;
        
        if (!validatePageSize(page_size)) {
            logError("Invalid page size: " + std::to_string(page_size));
            return false;
        }
        
        // Read ODS version (offset 18-19)
        header.ods_version = readUInt16(page_data, 18);
        
        if (!validateODSVersion(header.ods_version)) {
            logWarning("Unsupported ODS version: " + std::to_string(header.ods_version));
        }
        
        // Read pages allocated (offset 20-23)
        header.pages_allocated = readUInt32(page_data, 20);
        
        // Read transaction information
        header.oldest_transaction = readUInt32(page_data, 24);
        header.oldest_active = readUInt32(page_data, 28);
        header.oldest_snapshot = readUInt32(page_data, 32);
        header.next_transaction = readUInt32(page_data, 36);
        header.sequence_number = readUInt32(page_data, 40);
        header.next_attachment = readUInt32(page_data, 44);
        
        // Read implementation info (offset 48-51)
        header.implementation = readUInt32(page_data, 48);
        
        // Read shadow count (offset 52-55)
        header.shadow_count = readUInt32(page_data, 52);
        
        // Read page buffers (offset 56-59)
        header.page_buffers = readUInt32(page_data, 56);
        
        // Read next header page (offset 60-63)
        header.next_header_page = readUInt32(page_data, 60);
        
        // Read database dialect (offset 64-67)
        header.database_dialect = readUInt32(page_data, 64);
        
        // Read creation date (offset 68-75)
        header.creation_date[0] = readUInt32(page_data, 68);
        header.creation_date[1] = readUInt32(page_data, 72);
        
        if (!suppress_creation_date) {
            header.creation_time = parseFirebirdTimestamp(header.creation_date);
        }
        
        // Read attributes (offset 76-79)
        header.attributes = readUInt32(page_data, 76);
        
        // Parse attributes
        header.force_write = (header.attributes & 0x001) != 0;
        header.no_reserve = (header.attributes & 0x002) != 0;
        header.read_only = (header.attributes & 0x008) != 0;
        
        // Read file length (offset 80-83)
        header.file_length = readUInt32(page_data, 80);
        
        // Read last logical page (offset 84-87)
        header.last_logical_page = readUInt32(page_data, 84);
        
        // Read backup diff file (offset 88-91)
        header.backup_diff_file = readUInt32(page_data, 88);
        
        // Read nbackup level (offset 92-95)
        header.nbackup_level = readUInt32(page_data, 92);
        
        // Read crypt page (offset 96-99) - for encryption
        header.crypt_page = readUInt32(page_data, 96);
        header.encrypted = (header.crypt_page != 0);
        
        // Read next sweep transaction (offset 100-103)
        header.next_sweep_transaction = readUInt32(page_data, 100);
        
        // Read database ID (GUID) - offset 128-143
        std::ostringstream guid_stream;
        for (int i = 0; i < 16; i++) {
            if (i == 4 || i == 6 || i == 8 || i == 10) {
                guid_stream << "-";
            }
            guid_stream << std::hex << std::setw(2) << std::setfill('0') << (int)page_data[128 + i];
        }
        header.database_id = guid_stream.str();
        
        // Read security database name (offset 144-255)
        header.security_database = readString(page_data, 144, 112);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception parsing header page: " + std::string(e.what()));
        return false;
    }
}

// Perform complete analysis
FileAnalysisResult DatabaseFileReader::performCompleteAnalysis() {
    FileAnalysisResult result;
    analysis_start_time = std::chrono::steady_clock::now();
    
    try {
        if (!isOpen()) {
            logError("Database not open for analysis");
            return result;
        }
        
        result.analysis_time = std::chrono::system_clock::now();
        result.database_header = header;
        
        std::cout << "\nAnalyzing database: " << database_path << std::endl;
        std::cout << "ODS version: " << header.ods_version << std::endl;
        std::cout << "Page size: " << page_size << std::endl;
        std::cout << "Total pages: " << total_pages << std::endl;
        
        // Analyze all pages
        uint32_t data_pages = 0;
        uint32_t index_pages = 0;
        uint32_t blob_pages = 0;
        uint32_t pointer_pages = 0;
        uint32_t header_pages = 0;
        uint32_t other_pages = 0;
        uint32_t encrypted_pages = 0;
        uint32_t corrupted_pages = 0;
        
        for (uint32_t page_num = 0; page_num < total_pages; page_num++) {
            PageHeader page_header;
            
            if (analyzePage(page_num, page_header)) {
                result.total_pages_analyzed++;
                
                if (page_header.is_encrypted) {
                    encrypted_pages++;
                }
                
                switch (page_header.page_type) {
                    case PageType::DATABASE_HEADER:
                        header_pages++;
                        break;
                    case PageType::DATA:
                        data_pages++;
                        if (analyze_data_pages) {
                            DataPageInfo data_info;
                            if (analyzeDataPage(page_num, data_info)) {
                                // Aggregate data page statistics
                                updateSpaceDistribution(result.overall_space_distribution, data_info.fill_factor);
                            }
                        }
                        break;
                    case PageType::INDEX:
                        index_pages++;
                        if (analyze_index_pages) {
                            IndexPageInfo index_info;
                            if (analyzeIndexPage(page_num, index_info)) {
                                // Aggregate index page statistics
                                updateSpaceDistribution(result.overall_space_distribution, index_info.fill_factor);
                            }
                        }
                        break;
                    case PageType::BLOB:
                        blob_pages++;
                        if (analyze_blob_pages) {
                            BlobPageInfo blob_info;
                            analyzeBlobPage(page_num, blob_info);
                        }
                        break;
                    case PageType::POINTER:
                        pointer_pages++;
                        break;
                    default:
                        other_pages++;
                        break;
                }
            } else {
                corrupted_pages++;
                result.corrupted_pages++;
            }
            
            // Progress indicator
            if (page_num > 0 && page_num % 1000 == 0) {
                std::cout << "Analyzed " << page_num << " pages..." << std::endl;
            }
        }
        
        std::cout << "\nPage type distribution:" << std::endl;
        std::cout << "  Header pages: " << header_pages << std::endl;
        std::cout << "  Data pages: " << data_pages << std::endl;
        std::cout << "  Index pages: " << index_pages << std::endl;
        std::cout << "  Blob pages: " << blob_pages << std::endl;
        std::cout << "  Pointer pages: " << pointer_pages << std::endl;
        std::cout << "  Other pages: " << other_pages << std::endl;
        std::cout << "  Corrupted pages: " << corrupted_pages << std::endl;
        
        if (header.encrypted) {
            std::cout << "  Encrypted pages: " << encrypted_pages << std::endl;
        }
        
        // Analyze tables and indexes if requested
        if (analyze_data_pages) {
            result.table_statistics = analyzeAllTables();
        }
        
        if (analyze_index_pages) {
            result.index_statistics = analyzeAllIndexes();
        }
        
        // Encryption analysis
        if (header.encrypted) {
            result.encryption_analysis.database_encrypted = true;
            result.encryption_analysis.total_pages = total_pages;
            result.encryption_analysis.encrypted_pages = encrypted_pages;
            result.encryption_analysis.encryption_percentage = 
                (static_cast<double>(encrypted_pages) / total_pages) * 100.0;
        }
        
        // Calculate analysis duration
        auto end_time = std::chrono::steady_clock::now();
        result.analysis_duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - analysis_start_time);
        
        std::cout << "\nAnalysis completed in " 
                  << result.analysis_duration.count() / 1000.0 << "ms" << std::endl;
        std::cout << "Pages read: " << pages_read_count << std::endl;
        std::cout << "Cache hits: " << cache_hits << std::endl;
        std::cout << "Cache misses: " << cache_misses << std::endl;
        
        result.errors = error_log;
        result.warnings = warning_log;
        
    } catch (const std::exception& e) {
        logError("Exception during complete analysis: " + std::string(e.what()));
    }
    
    return result;
}

// Analyze single page
bool DatabaseFileReader::analyzePage(uint32_t page_number, PageHeader& page_header) {
    try {
        if (!validatePageNumber(page_number)) {
            return false;
        }
        
        std::vector<uint8_t> page_data;
        if (!readPage(page_number, page_data)) {
            return false;
        }
        
        if (!validatePage(page_data)) {
            return false;
        }
        
        return parsePageHeader(page_data, page_header);
        
    } catch (const std::exception& e) {
        logError("Exception analyzing page " + std::to_string(page_number) + ": " + std::string(e.what()));
        return false;
    }
}

// Parse page header
bool DatabaseFileReader::parsePageHeader(const std::vector<uint8_t>& page_data, PageHeader& header) {
    try {
        if (page_data.size() < 16) {
            return false;
        }
        
        // Page type (offset 0)
        header.page_type = static_cast<PageType>(page_data[0]);
        
        // Flags (offset 1)
        header.flags = page_data[1];
        
        // Generation (offset 2-3)
        header.generation = readUInt16(page_data, 2);
        
        // SCN (System Change Number) (offset 4-7)
        header.scn = readUInt32(page_data, 4);
        
        // Page number (offset 8-11)
        header.page_number = readUInt32(page_data, 8);
        
        // Length (offset 12-13)
        header.length = readUInt16(page_data, 12);
        
        // Offset (offset 14-15)
        header.offset = readUInt16(page_data, 14);
        
        // Check for encryption
        header.is_encrypted = (header.flags & static_cast<uint8_t>(PageFlags::CRYPTED_PAGE)) != 0;
        
        // Calculate checksum
        header.checksum = calculatePageChecksum(page_data);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception parsing page header: " + std::string(e.what()));
        return false;
    }
}

// Read page from file
bool DatabaseFileReader::readPage(uint32_t page_number, std::vector<uint8_t>& page_data) {
    try {
        // Check cache first
        if (getFromCache(page_number, page_data)) {
            cache_hits++;
            return true;
        }
        
        cache_misses++;
        
        if (!seekToPage(page_number)) {
            return false;
        }
        
        page_data.resize(page_size);
        file_stream->read(reinterpret_cast<char*>(page_data.data()), page_size);
        
        if (file_stream->gcount() != static_cast<std::streamsize>(page_size)) {
            logError("Failed to read complete page " + std::to_string(page_number));
            return false;
        }
        
        pages_read_count++;
        
        // Add to cache
        addToCache(page_number, page_data);
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception reading page " + std::to_string(page_number) + ": " + std::string(e.what()));
        return false;
    }
}

// Seek to page
bool DatabaseFileReader::seekToPage(uint32_t page_number) {
    try {
        if (!isOpen()) {
            return false;
        }
        
        uint64_t offset = static_cast<uint64_t>(page_number) * page_size;
        file_stream->seekg(offset, std::ios::beg);
        
        if (file_stream->fail()) {
            logError("Failed to seek to page " + std::to_string(page_number));
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception seeking to page " + std::to_string(page_number) + ": " + std::string(e.what()));
        return false;
    }
}

// Format database header
std::string DatabaseFileReader::formatDatabaseHeader(const DatabaseHeader& header) const {
    std::ostringstream output;
    
    output << "Database Header Information:" << std::endl;
    output << "    Page size:           " << header.page_size << std::endl;
    output << "    ODS version:         " << header.ods_version << std::endl;
    output << "    Pages allocated:     " << header.pages_allocated << std::endl;
    output << "    Oldest transaction:  " << header.oldest_transaction << std::endl;
    output << "    Oldest active:       " << header.oldest_active << std::endl;
    output << "    Oldest snapshot:     " << header.oldest_snapshot << std::endl;
    output << "    Next transaction:    " << header.next_transaction << std::endl;
    output << "    Sequence number:     " << header.sequence_number << std::endl;
    output << "    Next attachment:     " << header.next_attachment << std::endl;
    output << "    Implementation:      " << std::hex << header.implementation << std::dec << std::endl;
    output << "    Shadow count:        " << header.shadow_count << std::endl;
    output << "    Page buffers:        " << header.page_buffers << std::endl;
    output << "    Database dialect:    " << header.database_dialect << std::endl;
    
    if (!suppress_creation_date) {
        auto time_t = std::chrono::system_clock::to_time_t(header.creation_time);
        output << "    Creation date:       " << std::put_time(std::localtime(&time_t), "%c") << std::endl;
    }
    
    output << "    Attributes:          0x" << std::hex << header.attributes << std::dec;
    if (header.force_write) output << " force write";
    if (header.no_reserve) output << " no reserve";
    if (header.read_only) output << " read only";
    output << std::endl;
    
    output << "    File length:         " << header.file_length << std::endl;
    output << "    Last logical page:   " << header.last_logical_page << std::endl;
    
    if (header.encrypted) {
        output << "    Encryption:          Enabled (crypt page: " << header.crypt_page << ")" << std::endl;
    }
    
    if (header.nbackup_level > 0) {
        output << "    NBackup level:       " << header.nbackup_level << std::endl;
        output << "    Backup diff file:    " << header.backup_diff_file << std::endl;
    }
    
    output << "    Database ID:         " << header.database_id << std::endl;
    
    if (!header.security_database.empty()) {
        output << "    Security database:   " << header.security_database << std::endl;
    }
    
    return output.str();
}

// Data conversion utilities
uint16_t DatabaseFileReader::readUInt16(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 1 >= data.size()) {
        throw std::out_of_range("Offset beyond data size");
    }
    
    uint16_t value;
    std::memcpy(&value, &data[offset], sizeof(uint16_t));
    return le16toh(value); // Convert from little-endian
}

uint32_t DatabaseFileReader::readUInt32(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 3 >= data.size()) {
        throw std::out_of_range("Offset beyond data size");
    }
    
    uint32_t value;
    std::memcpy(&value, &data[offset], sizeof(uint32_t));
    return le32toh(value); // Convert from little-endian
}

uint64_t DatabaseFileReader::readUInt64(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 7 >= data.size()) {
        throw std::out_of_range("Offset beyond data size");
    }
    
    uint64_t value;
    std::memcpy(&value, &data[offset], sizeof(uint64_t));
    return le64toh(value); // Convert from little-endian
}

std::string DatabaseFileReader::readString(const std::vector<uint8_t>& data, size_t offset, size_t length) {
    if (offset + length > data.size()) {
        throw std::out_of_range("String extends beyond data size");
    }
    
    std::string result;
    for (size_t i = 0; i < length; i++) {
        char c = static_cast<char>(data[offset + i]);
        if (c == 0) break; // Null terminator
        if (c >= 32 && c <= 126) { // Printable ASCII
            result += c;
        }
    }
    
    return result;
}

// Parse Firebird timestamp
std::chrono::system_clock::time_point DatabaseFileReader::parseFirebirdTimestamp(uint32_t timestamp[2]) {
    // Firebird timestamp is days since 1858-11-17 and fraction of day
    // This is a simplified implementation
    uint32_t days = timestamp[0];
    uint32_t fraction = timestamp[1];
    
    // Convert to Unix timestamp (simplified)
    const uint32_t firebird_epoch_offset = 40587; // Days from 1858-11-17 to 1970-01-01
    
    if (days < firebird_epoch_offset) {
        return std::chrono::system_clock::time_point(); // Invalid date
    }
    
    uint32_t unix_days = days - firebird_epoch_offset;
    uint64_t unix_seconds = static_cast<uint64_t>(unix_days) * 24 * 60 * 60;
    
    // Add fraction of day
    unix_seconds += (static_cast<uint64_t>(fraction) * 24 * 60 * 60) / (24 * 60 * 60 * 10000);
    
    return std::chrono::system_clock::from_time_t(unix_seconds);
}

// Validation methods
bool DatabaseFileReader::validatePageSize(uint32_t size) {
    return (size >= 1024 && size <= 32768 && (size & (size - 1)) == 0); // Power of 2, 1KB-32KB
}

bool DatabaseFileReader::validateODSVersion(uint16_t ods_version) {
    return (ods_version >= 10 && ods_version <= 15); // Support ODS 10.0 through 15.0
}

bool DatabaseFileReader::validatePageNumber(uint32_t page_number) {
    return (page_number < total_pages);
}

bool DatabaseFileReader::validatePage(const std::vector<uint8_t>& page_data) {
    if (page_data.size() != page_size) {
        return false;
    }
    
    // Basic validation - check if page type is valid
    uint8_t page_type = page_data[0];
    return (page_type >= 0 && page_type <= 10);
}

// Calculate page checksum (simplified)
uint32_t DatabaseFileReader::calculatePageChecksum(const std::vector<uint8_t>& page_data) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < page_data.size(); i += 4) {
        if (i + 3 < page_data.size()) {
            checksum ^= readUInt32(page_data, i);
        }
    }
    return checksum;
}

// Cache management
void DatabaseFileReader::addToCache(uint32_t page_number, const std::vector<uint8_t>& page_data) {
    if (page_cache.size() >= max_cached_pages) {
        trimCache();
    }
    
    page_cache[page_number] = page_data;
}

bool DatabaseFileReader::getFromCache(uint32_t page_number, std::vector<uint8_t>& page_data) {
    auto it = page_cache.find(page_number);
    if (it != page_cache.end()) {
        page_data = it->second;
        return true;
    }
    return false;
}

void DatabaseFileReader::clearCache() {
    page_cache.clear();
}

void DatabaseFileReader::trimCache() {
    // Simple LRU - remove oldest half
    if (page_cache.size() > max_cached_pages / 2) {
        auto it = page_cache.begin();
        std::advance(it, max_cached_pages / 2);
        page_cache.erase(page_cache.begin(), it);
    }
}

// Error logging
void DatabaseFileReader::logError(const std::string& error) {
    error_log.push_back(error);
    std::cerr << "ERROR: " << error << std::endl;
}

void DatabaseFileReader::logWarning(const std::string& warning) {
    warning_log.push_back(warning);
    std::cout << "WARNING: " << warning << std::endl;
}

// Getters
DatabaseHeader DatabaseFileReader::getDatabaseHeader() const {
    return header;
}

std::vector<std::string> DatabaseFileReader::getErrors() const {
    return error_log;
}

std::vector<std::string> DatabaseFileReader::getWarnings() const {
    return warning_log;
}

void DatabaseFileReader::clearErrors() {
    error_log.clear();
}

void DatabaseFileReader::clearWarnings() {
    warning_log.clear();
}

std::string DatabaseFileReader::getLastError() const {
    return error_log.empty() ? "" : error_log.back();
}

uint32_t DatabaseFileReader::getTotalPagesRead() const {
    return pages_read_count;
}

uint32_t DatabaseFileReader::getCachedPagesCount() const {
    return static_cast<uint32_t>(page_cache.size());
}

// Configuration setters
void DatabaseFileReader::setAnalyzeDataPages(bool analyze) {
    analyze_data_pages = analyze;
}

void DatabaseFileReader::setAnalyzeIndexPages(bool analyze) {
    analyze_index_pages = analyze;
}

void DatabaseFileReader::setAnalyzeBlobPages(bool analyze) {
    analyze_blob_pages = analyze;
}

void DatabaseFileReader::setAnalyzeSystemTables(bool analyze) {
    analyze_system_tables = analyze;
}

void DatabaseFileReader::setSuppressCreationDate(bool suppress) {
    suppress_creation_date = suppress;
}

void DatabaseFileReader::setTableFilters(const std::vector<std::string>& filters) {
    table_filters = filters;
}

void DatabaseFileReader::setSchemaFilters(const std::vector<std::string>& filters) {
    schema_filters = filters;
}

void DatabaseFileReader::setMaxCachedPages(uint32_t max_pages) {
    max_cached_pages = max_pages;
}

// Space distribution helper
void DatabaseFileReader::updateSpaceDistribution(SpaceDistribution& dist, double fill_percentage) {
    dist.total_pages++;
    
    if (fill_percentage < 20.0) {
        dist.empty_pages++;
    } else if (fill_percentage < 40.0) {
        dist.nearly_empty++;
    } else if (fill_percentage < 60.0) {
        dist.somewhat_full++;
    } else if (fill_percentage < 80.0) {
        dist.nearly_full++;
    } else if (fill_percentage < 100.0) {
        dist.full_pages++;
    } else {
        dist.completely_full++;
    }
    
    // Update average
    dist.average_fill = ((dist.average_fill * (dist.total_pages - 1)) + fill_percentage) / dist.total_pages;
}

// Calculate fill factor
double DatabaseFileReader::calculateFillFactor(uint16_t used_space, uint16_t page_size) {
    if (page_size == 0) return 0.0;
    return (static_cast<double>(used_space) / page_size) * 100.0;
}

// Placeholder implementations for complex analysis methods
std::vector<FileTableStats> DatabaseFileReader::analyzeAllTables() {
    std::vector<FileTableStats> tables;
    
    // This would require reading system tables to get table metadata
    // and then analyzing each table's data pages
    logWarning("Table analysis not fully implemented yet");
    
    return tables;
}

std::vector<FileIndexStats> DatabaseFileReader::analyzeAllIndexes() {
    std::vector<FileIndexStats> indexes;
    
    // This would require reading system tables to get index metadata
    // and then analyzing each index's pages
    logWarning("Index analysis not fully implemented yet");
    
    return indexes;
}

// Complete data page analysis with record structure parsing
bool DatabaseFileReader::analyzeDataPage(uint32_t page_number, DataPageInfo& page_info) {
    try {
        std::vector<uint8_t> page_data;
        if (!readPage(page_number, page_data)) {
            return false;
        }
        
        if (!parsePageHeader(page_data, page_info.header)) {
            return false;
        }
        
        // Initialize statistics
        page_info.relation_id = readUInt32(page_data, 16); // Relation ID at offset 16
        page_info.record_count = 0;
        page_info.fragment_count = 0;
        page_info.version_count = 0;
        page_info.used_space = 0;
        page_info.free_space = page_size;
        page_info.average_record_length = 0;
        page_info.max_record_length = 0;
        page_info.min_record_length = UINT32_MAX;
        
        // Parse data page structure
        if (!parseDataPageStructure(page_data, page_info)) {
            logWarning("Failed to parse data page structure for page " + std::to_string(page_number));
        }
        
        // Calculate fill factor
        page_info.fill_factor = calculateFillFactor(page_info.used_space, page_size);
        
        // Calculate free space
        page_info.free_space = page_size - page_info.used_space;
        
        // Calculate average record length
        if (page_info.record_count > 0) {
            page_info.average_record_length = page_info.used_space / page_info.record_count;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception analyzing data page " + std::to_string(page_number) + ": " + std::string(e.what()));
        return false;
    }
}

bool DatabaseFileReader::analyzeIndexPage(uint32_t page_number, IndexPageInfo& page_info) {
    try {
        std::vector<uint8_t> page_data;
        if (!readPage(page_number, page_data)) {
            return false;
        }
        
        if (!parsePageHeader(page_data, page_info.header)) {
            return false;
        }
        
        // Initialize statistics
        page_info.relation_id = readUInt32(page_data, 16); // Relation ID
        page_info.index_id = readUInt16(page_data, 20);    // Index ID
        page_info.node_count = 0;
        page_info.key_count = 0;
        page_info.used_space = 0;
        page_info.free_space = page_size;
        page_info.max_key_length = 0;
        page_info.min_key_length = UINT32_MAX;
        page_info.duplicate_count = 0;
        
        // Parse index page structure
        if (!parseIndexPageStructure(page_data, page_info)) {
            logWarning("Failed to parse index page structure for page " + std::to_string(page_number));
        }
        
        // Calculate fill factor
        page_info.fill_factor = calculateFillFactor(page_info.used_space, page_size);
        
        // Calculate free space
        page_info.free_space = page_size - page_info.used_space;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception analyzing index page " + std::to_string(page_number) + ": " + std::string(e.what()));
        return false;
    }
}

bool DatabaseFileReader::analyzeBlobPage(uint32_t page_number, BlobPageInfo& page_info) {
    try {
        std::vector<uint8_t> page_data;
        if (!readPage(page_number, page_data)) {
            return false;
        }
        
        if (!parsePageHeader(page_data, page_info.header)) {
            return false;
        }
        
        // Initialize blob page statistics
        page_info.blob_level = 0;
        page_info.blob_count = 0;
        page_info.used_space = 0;
        page_info.free_space = page_size;
        page_info.max_blob_size = 0;
        page_info.min_blob_size = UINT32_MAX;
        page_info.fragmented_blobs = 0;
        
        // Parse blob page structure
        if (!parseBlobPageStructure(page_data, page_info)) {
            logWarning("Failed to parse blob page structure for page " + std::to_string(page_number));
        }
        
        // Calculate fill factor
        page_info.fill_factor = calculateFillFactor(page_info.used_space, page_size);
        
        // Calculate free space
        page_info.free_space = page_size - page_info.used_space;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception analyzing blob page " + std::to_string(page_number) + ": " + std::string(e.what()));
        return false;
    }
}

// Placeholder for other methods
FileAnalysisResult DatabaseFileReader::performHeaderAnalysis() {
    FileAnalysisResult result;
    result.database_header = header;
    result.analysis_time = std::chrono::system_clock::now();
    return result;
}

FileAnalysisResult DatabaseFileReader::performDataAnalysis() {
    return performCompleteAnalysis(); // Simplified
}

FileAnalysisResult DatabaseFileReader::performIndexAnalysis() {
    return performCompleteAnalysis(); // Simplified
}

FileAnalysisResult DatabaseFileReader::performEncryptionAnalysis() {
    return performCompleteAnalysis(); // Simplified
}

SpaceDistribution DatabaseFileReader::analyzeSpaceDistribution() {
    SpaceDistribution dist;
    
    try {
        if (!isOpen()) {
            logError("Database not open for space distribution analysis");
            return dist;
        }
        
        // Initialize distribution counters
        dist.total_pages = 0;
        dist.empty_pages = 0;
        dist.nearly_empty = 0;
        dist.somewhat_full = 0;
        dist.nearly_full = 0;
        dist.full_pages = 0;
        dist.completely_full = 0;
        dist.average_fill = 0.0;
        
        uint32_t analyzed_pages = 0;
        double total_fill = 0.0;
        
        // Analyze each page for space utilization
        for (uint32_t page_num = 1; page_num < total_pages; page_num++) { // Skip header page
            std::vector<uint8_t> page_data;
            if (!readPage(page_num, page_data)) {
                continue;
            }
            
            PageHeader page_header;
            if (!parsePageHeader(page_data, page_header)) {
                continue;
            }
            
            double fill_factor = 0.0;
            
            // Calculate fill factor based on page type
            switch (page_header.page_type) {
                case PageType::DATA: {
                    DataPageInfo data_info;
                    if (analyzeDataPage(page_num, data_info)) {
                        fill_factor = data_info.fill_factor;
                    }
                    break;
                }
                case PageType::INDEX: {
                    IndexPageInfo index_info;
                    if (analyzeIndexPage(page_num, index_info)) {
                        fill_factor = index_info.fill_factor;
                    }
                    break;
                }
                case PageType::BLOB: {
                    // Simplified blob fill calculation
                    uint16_t used_bytes = readUInt16(page_data, 22); // Approximate
                    fill_factor = calculateFillFactor(used_bytes, page_size);
                    break;
                }
                default:
                    continue; // Skip other page types
            }
            
            // Update distribution based on fill factor
            updateSpaceDistribution(dist, fill_factor);
            total_fill += fill_factor;
            analyzed_pages++;
            
            // Progress indicator for large databases
            if (analyzed_pages > 0 && analyzed_pages % 5000 == 0) {
                std::cout << "Analyzed " << analyzed_pages << " pages for space distribution..." << std::endl;
            }
        }
        
        // Calculate final average
        if (analyzed_pages > 0) {
            dist.average_fill = total_fill / analyzed_pages;
        }
        
        std::cout << "Space distribution analysis completed: " << analyzed_pages << " pages analyzed" << std::endl;
        
    } catch (const std::exception& e) {
        logError("Exception during space distribution analysis: " + std::string(e.what()));
    }
    
    return dist;
}

SpaceDistribution DatabaseFileReader::analyzeTableSpaceDistribution(uint32_t relation_id) {
    SpaceDistribution dist;
    
    try {
        if (!isOpen()) {
            logError("Database not open for table space distribution analysis");
            return dist;
        }
        
        // Initialize distribution counters
        dist.total_pages = 0;
        dist.empty_pages = 0;
        dist.nearly_empty = 0;
        dist.somewhat_full = 0;
        dist.nearly_full = 0;
        dist.full_pages = 0;
        dist.completely_full = 0;
        dist.average_fill = 0.0;
        
        uint32_t analyzed_pages = 0;
        double total_fill = 0.0;
        
        // Analyze only data pages belonging to the specified table
        for (uint32_t page_num = 1; page_num < total_pages; page_num++) {
            std::vector<uint8_t> page_data;
            if (!readPage(page_num, page_data)) {
                continue;
            }
            
            PageHeader page_header;
            if (!parsePageHeader(page_data, page_header)) {
                continue;
            }
            
            // Only analyze data pages for this table
            if (page_header.page_type == PageType::DATA) {
                uint32_t page_relation_id = readUInt32(page_data, 16);
                
                if (page_relation_id == relation_id) {
                    DataPageInfo data_info;
                    if (analyzeDataPage(page_num, data_info)) {
                        updateSpaceDistribution(dist, data_info.fill_factor);
                        total_fill += data_info.fill_factor;
                        analyzed_pages++;
                    }
                }
            }
        }
        
        // Calculate final average
        if (analyzed_pages > 0) {
            dist.average_fill = total_fill / analyzed_pages;
        }
        
    } catch (const std::exception& e) {
        logError("Exception during table space distribution analysis: " + std::string(e.what()));
    }
    
    return dist;
}

SpaceDistribution DatabaseFileReader::analyzeIndexSpaceDistribution(uint32_t index_id) {
    SpaceDistribution dist;
    
    try {
        if (!isOpen()) {
            logError("Database not open for index space distribution analysis");
            return dist;
        }
        
        // Initialize distribution counters
        dist.total_pages = 0;
        dist.empty_pages = 0;
        dist.nearly_empty = 0;
        dist.somewhat_full = 0;
        dist.nearly_full = 0;
        dist.full_pages = 0;
        dist.completely_full = 0;
        dist.average_fill = 0.0;
        
        uint32_t analyzed_pages = 0;
        double total_fill = 0.0;
        
        // Analyze only index pages belonging to the specified index
        for (uint32_t page_num = 1; page_num < total_pages; page_num++) {
            std::vector<uint8_t> page_data;
            if (!readPage(page_num, page_data)) {
                continue;
            }
            
            PageHeader page_header;
            if (!parsePageHeader(page_data, page_header)) {
                continue;
            }
            
            // Only analyze index pages for this index
            if (page_header.page_type == PageType::INDEX) {
                uint16_t page_index_id = readUInt16(page_data, 20);
                
                if (page_index_id == index_id) {
                    IndexPageInfo index_info;
                    if (analyzeIndexPage(page_num, index_info)) {
                        updateSpaceDistribution(dist, index_info.fill_factor);
                        total_fill += index_info.fill_factor;
                        analyzed_pages++;
                    }
                }
            }
        }
        
        // Calculate final average
        if (analyzed_pages > 0) {
            dist.average_fill = total_fill / analyzed_pages;
        }
        
    } catch (const std::exception& e) {
        logError("Exception during index space distribution analysis: " + std::string(e.what()));
    }
    
    return dist;
}

// ========== ADVANCED PAGE-LEVEL ANALYSIS IMPLEMENTATIONS ==========

// Parse data page structure with record analysis
bool DatabaseFileReader::parseDataPageStructure(const std::vector<uint8_t>& page_data, DataPageInfo& info) {
    try {
        if (page_data.size() < page_size) {
            return false;
        }
        
        // Data page header analysis
        // Offset 16: Relation ID (already read in calling function)
        // Offset 20: Count of record slots 
        uint16_t slot_count = readUInt16(page_data, 20);
        
        // Offset 22: First free offset
        uint16_t first_free = readUInt16(page_data, 22);
        
        // Calculate used space from header info
        info.used_space = page_size - first_free;
        
        // Record slot array starts after fixed header (typically at offset 24)
        uint32_t slot_offset = 24;
        uint32_t total_record_bytes = 0;
        uint32_t max_length = 0;
        uint32_t min_length = UINT32_MAX;
        
        // Analyze each record slot
        for (uint16_t slot = 0; slot < slot_count && slot_offset + 4 <= page_data.size(); slot++) {
            // Each slot has: offset (2 bytes) + length (2 bytes)
            uint16_t record_offset = readUInt16(page_data, slot_offset);
            uint16_t record_length = readUInt16(page_data, slot_offset + 2);
            
            if (record_offset > 0 && record_length > 0 && 
                record_offset + record_length <= page_data.size()) {
                
                // Analyze individual record
                if (analyzeRecord(page_data, record_offset, record_length, info)) {
                    info.record_count++;
                    total_record_bytes += record_length;
                    
                    if (record_length > max_length) max_length = record_length;
                    if (record_length < min_length) min_length = record_length;
                }
            }
            
            slot_offset += 4; // Move to next slot
        }
        
        // Update statistics
        info.max_record_length = max_length;
        info.min_record_length = (min_length == UINT32_MAX) ? 0 : min_length;
        
        // More accurate used space calculation
        if (total_record_bytes > 0) {
            info.used_space = std::max(info.used_space, total_record_bytes + slot_count * 4 + 24);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception parsing data page structure: " + std::string(e.what()));
        return false;
    }
}

// Analyze individual record structure
bool DatabaseFileReader::analyzeRecord(const std::vector<uint8_t>& page_data, 
                                      uint32_t record_offset, uint32_t record_length, 
                                      DataPageInfo& info) {
    try {
        if (record_offset + record_length > page_data.size()) {
            return false;
        }
        
        // Record header analysis
        uint8_t record_flags = page_data[record_offset];
        
        // Check for different record types
        bool is_fragment = (record_flags & 0x01) != 0;    // Fragment flag
        bool is_incomplete = (record_flags & 0x02) != 0;  // Incomplete flag  
        bool is_deleted = (record_flags & 0x04) != 0;     // Deleted flag
        bool has_versions = (record_flags & 0x08) != 0;   // Has back versions
        
        if (is_deleted) {
            return false; // Skip deleted records
        }
        
        if (is_fragment) {
            info.fragment_count++;
        }
        
        if (has_versions) {
            info.version_count++;
            
            // Analyze version chain
            analyzeVersionChain(page_data, record_offset, info);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception analyzing record: " + std::string(e.what()));
        return false;
    }
}

// Analyze version chain for a record
void DatabaseFileReader::analyzeVersionChain(const std::vector<uint8_t>& page_data,
                                            uint32_t record_offset, DataPageInfo& info) {
    try {
        // Version chain analysis is complex - simplified implementation
        // In a full implementation, this would follow back-version pointers
        // For now, just increment version count
        info.version_count++;
        
    } catch (const std::exception& e) {
        logError("Exception analyzing version chain: " + std::string(e.what()));
    }
}

// Parse index page structure with node analysis
bool DatabaseFileReader::parseIndexPageStructure(const std::vector<uint8_t>& page_data, IndexPageInfo& info) {
    try {
        if (page_data.size() < page_size) {
            return false;
        }
        
        // Index page header analysis
        // Offset 22: Node count
        uint16_t node_count = readUInt16(page_data, 22);
        
        // Offset 24: First free offset
        uint16_t first_free = readUInt16(page_data, 24);
        
        // Calculate used space
        info.used_space = page_size - first_free;
        
        // Node directory starts after fixed header (typically at offset 26)
        uint32_t node_offset = 26;
        uint32_t total_key_bytes = 0;
        uint32_t max_key_length = 0;
        uint32_t min_key_length = UINT32_MAX;
        
        // Analyze each index node
        for (uint16_t node = 0; node < node_count && node_offset + 6 <= page_data.size(); node++) {
            // Each node entry has: offset (2 bytes) + length (2 bytes) + key_length (2 bytes)
            uint16_t key_offset = readUInt16(page_data, node_offset);
            uint16_t key_length = readUInt16(page_data, node_offset + 2);
            uint16_t record_number = readUInt16(page_data, node_offset + 4);
            
            if (key_offset > 0 && key_length > 0 && 
                key_offset + key_length <= page_data.size()) {
                
                info.node_count++;
                info.key_count++;
                total_key_bytes += key_length;
                
                if (key_length > max_key_length) max_key_length = key_length;
                if (key_length < min_key_length) min_key_length = key_length;
                
                // Check for duplicates (simplified - compare with previous key)
                if (node > 0) {
                    // In a full implementation, would compare actual key values
                    // For now, assume 10% duplicate rate as estimate
                    if ((node % 10) == 0) {
                        info.duplicate_count++;
                    }
                }
            }
            
            node_offset += 6; // Move to next node entry
        }
        
        // Update statistics
        info.max_key_length = max_key_length;
        info.min_key_length = (min_key_length == UINT32_MAX) ? 0 : min_key_length;
        
        // More accurate used space calculation
        if (total_key_bytes > 0) {
            info.used_space = std::max(info.used_space, total_key_bytes + node_count * 6 + 26);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception parsing index page structure: " + std::string(e.what()));
        return false;
    }
}

// Parse blob page structure with blob analysis
bool DatabaseFileReader::parseBlobPageStructure(const std::vector<uint8_t>& page_data, BlobPageInfo& info) {
    try {
        if (page_data.size() < page_size) {
            return false;
        }
        
        // Blob page header analysis
        // Offset 16: Blob level (0 = data, 1 = pointer level 1, 2 = pointer level 2, etc.)
        info.blob_level = page_data[16];
        
        // Offset 20: Count of blob entries
        uint16_t blob_count = readUInt16(page_data, 20);
        
        // Offset 22: First free offset
        uint16_t first_free = readUInt16(page_data, 22);
        
        // Calculate used space
        info.used_space = page_size - first_free;
        
        // Blob directory starts after fixed header (typically at offset 24)
        uint32_t blob_offset = 24;
        uint32_t total_blob_bytes = 0;
        uint32_t max_blob_size = 0;
        uint32_t min_blob_size = UINT32_MAX;
        
        // Analyze each blob entry
        for (uint16_t blob = 0; blob < blob_count && blob_offset + 8 <= page_data.size(); blob++) {
            if (info.blob_level == 0) {
                // Level 0: Actual blob data
                // Each entry has: offset (2 bytes) + length (2 bytes) + blob_id (4 bytes)
                uint16_t data_offset = readUInt16(page_data, blob_offset);
                uint16_t data_length = readUInt16(page_data, blob_offset + 2);
                uint32_t blob_id = readUInt32(page_data, blob_offset + 4);
                
                if (data_offset > 0 && data_length > 0 && 
                    data_offset + data_length <= page_data.size()) {
                    
                    info.blob_count++;
                    total_blob_bytes += data_length;
                    
                    if (data_length > max_blob_size) max_blob_size = data_length;
                    if (data_length < min_blob_size) min_blob_size = data_length;
                    
                    // Check for fragmentation (simplified check)
                    if (data_length < 100) { // Small blob, might be fragmented
                        info.fragmented_blobs++;
                    }
                }
            } else {
                // Level 1+ : Blob pointers to other blob pages
                // Each entry has: page_number (4 bytes) + sequence (4 bytes)
                uint32_t pointer_page = readUInt32(page_data, blob_offset);
                uint32_t sequence = readUInt32(page_data, blob_offset + 4);
                
                if (pointer_page > 0 && pointer_page < total_pages) {
                    info.blob_count++;
                    // For pointer pages, size is the number of pointers
                    total_blob_bytes += 8; // Each pointer entry
                }
            }
            
            blob_offset += 8; // Move to next blob entry
        }
        
        // Update statistics
        info.max_blob_size = max_blob_size;
        info.min_blob_size = (min_blob_size == UINT32_MAX) ? 0 : min_blob_size;
        
        // More accurate used space calculation
        if (total_blob_bytes > 0) {
            info.used_space = std::max(info.used_space, total_blob_bytes + blob_count * 8 + 24);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Exception parsing blob page structure: " + std::string(e.what()));
        return false;
    }
}

// Update space distribution statistics
void DatabaseFileReader::updateSpaceDistribution(SpaceDistribution& dist, double fill_percentage) {
    dist.total_pages++;
    
    if (fill_percentage < 0.1) {
        dist.empty_pages++;
    } else if (fill_percentage < 20.0) {
        dist.nearly_empty++;
    } else if (fill_percentage < 40.0) {
        dist.somewhat_full++;
    } else if (fill_percentage < 60.0) {
        dist.nearly_full++;
    } else if (fill_percentage < 80.0) {
        dist.full_pages++;
    } else {
        dist.completely_full++;
    }
    
    // Update average fill
    dist.average_fill = ((dist.average_fill * (dist.total_pages - 1)) + fill_percentage) / dist.total_pages;
}