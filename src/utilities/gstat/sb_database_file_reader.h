#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <cstdint>
#include <chrono>

namespace SBEnhanced {

// Database page types (from Firebird ODS)
enum class PageType : uint8_t {
    UNDEFINED = 0,
    DATABASE_HEADER = 1,
    PAGE_INVENTORY = 2,
    TRANSACTION_INVENTORY = 3,
    POINTER = 4,
    DATA = 5,
    ROOT = 6,
    INDEX = 7,
    BLOB = 8,
    GENERATOR = 9,
    SCN_INVENTORY = 10,
    UNKNOWN = 255
};

// Page flags
enum class PageFlags : uint8_t {
    ORPHAN = 1,
    FULL = 2,
    LARGE = 4,
    SWEPT = 8,
    CRYPTED_PAGE = 16
};

// Database header information
struct DatabaseHeader {
    uint32_t page_size = 0;
    uint16_t ods_version = 0;
    uint32_t pages_allocated = 0;
    uint32_t oldest_transaction = 0;
    uint32_t oldest_active = 0;
    uint32_t oldest_snapshot = 0;
    uint32_t next_transaction = 0;
    uint32_t sequence_number = 0;
    uint32_t next_attachment = 0;
    uint32_t implementation = 0;
    uint32_t shadow_count = 0;
    uint32_t page_buffers = 0;
    uint32_t next_header_page = 0;
    uint32_t database_dialect = 0;
    uint32_t creation_date[2] = {0, 0};
    uint32_t attributes = 0;
    uint32_t file_length = 0;
    uint32_t last_logical_page = 0;
    uint32_t backup_diff_file = 0;
    uint32_t nbackup_level = 0;
    uint32_t crypt_page = 0;
    uint32_t next_sweep_transaction = 0;
    std::string database_id;
    std::string security_database;
    bool force_write = false;
    bool no_reserve = false;
    bool encrypted = false;
    bool read_only = false;
    std::chrono::system_clock::time_point creation_time;
};

// Page header structure
struct PageHeader {
    PageType page_type = PageType::UNDEFINED;
    uint8_t flags = 0;
    uint16_t generation = 0;
    uint32_t scn = 0;  // System change number
    uint32_t page_number = 0;
    uint16_t length = 0;
    uint16_t offset = 0;
    uint32_t checksum = 0;
    bool is_encrypted = false;
    bool is_compressed = false;
};

// Data page structure
struct DataPageInfo {
    PageHeader header;
    uint16_t count = 0;          // Number of records
    uint16_t free_space = 0;     // Free space in page
    uint16_t first_free = 0;     // First free slot
    std::vector<uint16_t> slot_offsets;
    uint32_t total_record_length = 0;
    uint32_t total_version_length = 0;
    uint32_t fragment_count = 0;
    uint32_t compressed_length = 0;
    double fill_factor = 0.0;
    double fragmentation_ratio = 0.0;
};

// Index page structure
struct IndexPageInfo {
    PageHeader header;
    uint32_t relation_id = 0;
    uint16_t index_id = 0;
    uint32_t left_sibling = 0;
    uint32_t right_sibling = 0;
    uint16_t prefix_total = 0;
    uint16_t length = 0;
    uint16_t count = 0;
    uint16_t level = 0;
    uint32_t page_number = 0;
    std::vector<uint16_t> node_offsets;
    uint32_t total_key_length = 0;
    uint32_t total_prefix_length = 0;
    uint32_t duplicate_count = 0;
    double compression_ratio = 0.0;
    double fill_factor = 0.0;
};

// Blob page structure
struct BlobPageInfo {
    PageHeader header;
    uint32_t lead_page = 0;
    uint32_t sequence = 0;
    uint16_t length = 0;
    uint16_t flags = 0;
    uint8_t level = 0;           // 0, 1, or 2
    std::vector<uint32_t> blob_pointers;
    uint32_t total_blob_length = 0;
    uint32_t blob_count = 0;
};

// Page space distribution
struct SpaceDistribution {
    uint32_t empty_pages = 0;     // 0-19% full
    uint32_t nearly_empty = 0;    // 20-39% full
    uint32_t somewhat_full = 0;   // 40-59% full
    uint32_t nearly_full = 0;     // 60-79% full
    uint32_t full_pages = 0;      // 80-99% full
    uint32_t completely_full = 0; // 100% full
    double average_fill = 0.0;
    uint32_t total_pages = 0;
};

// Table statistics from direct file analysis
struct FileTableStats {
    std::string table_name;
    uint32_t relation_id = 0;
    uint32_t pointer_page = 0;
    uint32_t index_root = 0;
    uint32_t data_pages = 0;
    uint32_t pointer_pages = 0;
    uint32_t total_records = 0;
    uint32_t total_versions = 0;
    uint32_t fragments = 0;
    uint32_t backversions = 0;
    uint32_t blob_pages = 0;
    uint32_t total_formats = 0;
    uint32_t used_formats = 0;
    uint64_t total_record_length = 0;
    uint64_t total_version_length = 0;
    uint64_t average_record_length = 0;
    uint64_t average_version_length = 0;
    uint64_t average_fragment_length = 0;
    SpaceDistribution fill_distribution;
    double compression_ratio = 0.0;
    bool swept = false;
};

// Index statistics from direct file analysis
struct FileIndexStats {
    std::string index_name;
    uint32_t index_id = 0;
    uint32_t relation_id = 0;
    uint32_t root_page = 0;
    uint32_t depth = 0;
    uint32_t leaf_buckets = 0;
    uint32_t nodes = 0;
    uint64_t total_dup_count = 0;
    uint64_t max_dup_count = 0;
    uint64_t total_key_length = 0;
    uint64_t total_node_length = 0;
    uint64_t average_key_length = 0;
    uint64_t average_node_length = 0;
    uint64_t total_prefix_length = 0;
    uint64_t average_prefix_length = 0;
    double clustering_factor = 0.0;
    double compression_ratio = 0.0;
    SpaceDistribution fill_distribution;
    bool unique_flag = false;
    std::vector<std::string> field_names;
};

// Database encryption analysis
struct EncryptionAnalysis {
    bool database_encrypted = false;
    uint32_t total_pages = 0;
    uint32_t encrypted_pages = 0;
    uint32_t data_pages_encrypted = 0;
    uint32_t index_pages_encrypted = 0;
    uint32_t blob_pages_encrypted = 0;
    uint32_t pointer_pages_encrypted = 0;
    uint32_t header_pages_encrypted = 0;
    uint32_t other_pages_encrypted = 0;
    double encryption_percentage = 0.0;
    std::string encryption_plugin;
    std::string key_name;
};

// Complete file analysis result
struct FileAnalysisResult {
    DatabaseHeader database_header;
    std::vector<FileTableStats> table_statistics;
    std::vector<FileIndexStats> index_statistics;
    EncryptionAnalysis encryption_analysis;
    SpaceDistribution overall_space_distribution;
    uint32_t total_pages_analyzed = 0;
    uint32_t corrupted_pages = 0;
    uint32_t orphaned_pages = 0;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::chrono::system_clock::time_point analysis_time;
    std::chrono::microseconds analysis_duration;
};

} // namespace SBEnhanced

// Database file reader class
class DatabaseFileReader {
private:
    std::string database_path;
    std::unique_ptr<std::ifstream> file_stream;
    SBEnhanced::DatabaseHeader header;
    uint32_t page_size = 0;
    uint32_t total_pages = 0;
    bool file_open = false;
    bool header_read = false;
    
    // Page cache for performance
    std::map<uint32_t, std::vector<uint8_t>> page_cache;
    uint32_t max_cached_pages = 1000;
    
    // Analysis options
    bool analyze_data_pages = true;
    bool analyze_index_pages = true;
    bool analyze_blob_pages = true;
    bool analyze_system_tables = false;
    bool suppress_creation_date = false;
    std::vector<std::string> table_filters;
    std::vector<std::string> schema_filters;
    
    // Error tracking
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;

public:
    DatabaseFileReader();
    ~DatabaseFileReader();
    
    // File operations
    bool openDatabase(const std::string& path);
    bool closeDatabase();
    bool isOpen() const;
    
    // Configuration
    void setAnalyzeDataPages(bool analyze);
    void setAnalyzeIndexPages(bool analyze);
    void setAnalyzeBlobPages(bool analyze);
    void setAnalyzeSystemTables(bool analyze);
    void setSuppressCreationDate(bool suppress);
    void setTableFilters(const std::vector<std::string>& filters);
    void setSchemaFilters(const std::vector<std::string>& filters);
    void setMaxCachedPages(uint32_t max_pages);
    
    // Core analysis methods
    bool readDatabaseHeader();
    SBEnhanced::DatabaseHeader getDatabaseHeader() const;
    bool analyzePage(uint32_t page_number, SBEnhanced::PageHeader& page_header);
    bool analyzeDataPage(uint32_t page_number, SBEnhanced::DataPageInfo& page_info);
    bool analyzeIndexPage(uint32_t page_number, SBEnhanced::IndexPageInfo& page_info);
    bool analyzeBlobPage(uint32_t page_number, SBEnhanced::BlobPageInfo& page_info);
    
    // High-level analysis
    SBEnhanced::FileAnalysisResult performCompleteAnalysis();
    SBEnhanced::FileAnalysisResult performHeaderAnalysis();
    SBEnhanced::FileAnalysisResult performDataAnalysis();
    SBEnhanced::FileAnalysisResult performIndexAnalysis();
    SBEnhanced::FileAnalysisResult performEncryptionAnalysis();
    
    // Table and index analysis
    std::vector<SBEnhanced::FileTableStats> analyzeAllTables();
    std::vector<SBEnhanced::FileIndexStats> analyzeAllIndexes();
    SBEnhanced::FileTableStats analyzeTable(uint32_t relation_id);
    SBEnhanced::FileIndexStats analyzeIndex(uint32_t index_id, uint32_t relation_id);
    
    // Space analysis
    SBEnhanced::SpaceDistribution analyzeSpaceDistribution();
    SBEnhanced::SpaceDistribution analyzeTableSpaceDistribution(uint32_t relation_id);
    SBEnhanced::SpaceDistribution analyzeIndexSpaceDistribution(uint32_t index_id);
    
    // Utility methods
    std::string formatDatabaseHeader(const SBEnhanced::DatabaseHeader& header) const;
    std::string formatTableStats(const SBEnhanced::FileTableStats& stats) const;
    std::string formatIndexStats(const SBEnhanced::FileIndexStats& stats) const;
    std::string formatSpaceDistribution(const SBEnhanced::SpaceDistribution& dist) const;
    std::string formatEncryptionAnalysis(const SBEnhanced::EncryptionAnalysis& analysis) const;
    
    // Page utilities
    bool readPage(uint32_t page_number, std::vector<uint8_t>& page_data);
    bool validatePage(const std::vector<uint8_t>& page_data);
    uint32_t calculatePageChecksum(const std::vector<uint8_t>& page_data);
    SBEnhanced::PageType getPageType(const std::vector<uint8_t>& page_data);
    bool isPageEncrypted(const std::vector<uint8_t>& page_data);
    double calculateFillFactor(uint16_t used_space, uint16_t page_size);
    
    // System metadata reading
    std::map<uint32_t, std::string> getTableNames();
    std::map<uint32_t, std::string> getIndexNames();
    std::map<uint32_t, std::vector<std::string>> getIndexFields();
    std::map<std::string, uint32_t> getSchemaMapping();
    
    // Error handling
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    void clearErrors();
    void clearWarnings();
    std::string getLastError() const;
    
    // Statistics
    uint32_t getTotalPagesRead() const;
    uint32_t getCachedPagesCount() const;
    std::chrono::microseconds getLastAnalysisDuration() const;

private:
    // Internal helper methods
    bool seekToPage(uint32_t page_number);
    bool parsePageHeader(const std::vector<uint8_t>& page_data, SBEnhanced::PageHeader& header);
    bool parseDataPageStructure(const std::vector<uint8_t>& page_data, SBEnhanced::DataPageInfo& info);
    bool parseIndexPageStructure(const std::vector<uint8_t>& page_data, SBEnhanced::IndexPageInfo& info);
    bool parseBlobPageStructure(const std::vector<uint8_t>& page_data, SBEnhanced::BlobPageInfo& info);
    
    // Database header parsing
    bool parseDatabaseHeaderPage(const std::vector<uint8_t>& page_data);
    std::chrono::system_clock::time_point parseFirebirdTimestamp(uint32_t timestamp[2]);
    
    // Record and index parsing
    uint32_t analyzeRecords(const std::vector<uint8_t>& page_data, SBEnhanced::DataPageInfo& info);
    uint32_t analyzeIndexNodes(const std::vector<uint8_t>& page_data, SBEnhanced::IndexPageInfo& info);
    uint32_t analyzeBlobPointers(const std::vector<uint8_t>& page_data, SBEnhanced::BlobPageInfo& info);
    
    // Advanced record analysis
    bool analyzeRecord(const std::vector<uint8_t>& page_data, uint32_t record_offset, 
                      uint32_t record_length, SBEnhanced::DataPageInfo& info);
    void analyzeVersionChain(const std::vector<uint8_t>& page_data, uint32_t record_offset, 
                            SBEnhanced::DataPageInfo& info);
    
    // System table queries (minimal engine interaction for metadata)
    bool readSystemMetadata();
    std::string getTableName(uint32_t relation_id);
    std::string getIndexName(uint32_t index_id, uint32_t relation_id);
    std::vector<std::string> getIndexFields(uint32_t index_id, uint32_t relation_id);
    
    // Space distribution helpers
    void updateSpaceDistribution(SBEnhanced::SpaceDistribution& dist, double fill_percentage);
    uint8_t getFillBucket(double fill_percentage);
    
    // Cache management
    void addToCache(uint32_t page_number, const std::vector<uint8_t>& page_data);
    bool getFromCache(uint32_t page_number, std::vector<uint8_t>& page_data);
    void clearCache();
    void trimCache();
    
    // Error logging
    void logError(const std::string& error);
    void logWarning(const std::string& warning);
    
    // Validation helpers
    bool validateDatabaseFile();
    bool validatePageNumber(uint32_t page_number);
    bool validatePageSize(uint32_t size);
    bool validateODSVersion(uint16_t ods_version);
    
    // Data conversion utilities
    uint16_t readUInt16(const std::vector<uint8_t>& data, size_t offset);
    uint32_t readUInt32(const std::vector<uint8_t>& data, size_t offset);
    uint64_t readUInt64(const std::vector<uint8_t>& data, size_t offset);
    std::string readString(const std::vector<uint8_t>& data, size_t offset, size_t length);
    
    // Performance tracking
    std::chrono::steady_clock::time_point analysis_start_time;
    uint32_t pages_read_count = 0;
    uint32_t cache_hits = 0;
    uint32_t cache_misses = 0;
};