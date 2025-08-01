#pragma once

#include "sb_database_file_reader.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>

namespace SBEnhanced {

// Classic GSTAT analysis options
struct ClassicGStatOptions {
    bool analyze_all = false;           // -a flag
    bool analyze_data = false;          // -d flag  
    bool analyze_index = false;         // -i flag
    bool analyze_header = false;        // -h flag
    bool analyze_encryption = false;    // -e flag
    bool analyze_system = false;        // -s flag
    bool analyze_record = false;        // -r flag
    bool suppress_creation = false;     // -n flag
    bool show_version = false;          // -z flag
    bool show_help = false;             // -? flag
    
    std::string username = "SYSDBA";    // -u flag
    std::string password = "masterkey"; // -p flag
    std::string role;                   // -role flag
    bool trusted_auth = false;          // -tr flag
    std::string password_file;          // -fetch flag
    
    std::vector<std::string> table_names;  // -t flag (can be multiple)
    std::vector<std::string> schema_names; // -sch flag (can be multiple)
    
    std::string database_path;
    bool verbose = false;
};

// Classic output formatter for GSTAT compatibility
struct ClassicOutputFormatter {
    bool suppress_creation_date = false;
    bool system_tables_only = false;
    std::string current_table_filter;
    std::string current_schema_filter;
};

} // namespace SBEnhanced

// Classic GSTAT implementation for backward compatibility
class GSTATClassic {
private:
    std::unique_ptr<DatabaseFileReader> file_reader;
    SBEnhanced::ClassicGStatOptions options;
    SBEnhanced::ClassicOutputFormatter formatter;
    
    // Analysis results
    SBEnhanced::FileAnalysisResult analysis_result;
    
    // Output control
    std::ostream* output_stream;
    bool quiet_mode = false;
    
    // Version information
    static const std::string VERSION;
    static const std::string BUILD_DATE;

public:
    GSTATClassic();
    ~GSTATClassic();
    
    // Command line parsing
    bool parseCommandLine(int argc, char* argv[]);
    void showUsage();
    void showVersion();
    
    // Main execution
    int execute();
    
    // Analysis methods (classic interface)
    bool analyzeDatabase();
    bool analyzeHeaderOnly();
    bool analyzeDataPages();
    bool analyzeIndexPages();  
    bool analyzeEncryption();
    bool analyzeSystemTables();
    
    // Output formatting (classic format)
    void printDatabaseHeader();
    void printTableStatistics();
    void printIndexStatistics();
    void printEncryptionInfo();
    void printSystemTableInfo();
    void printSpaceDistribution();
    void printSummary();
    
    // Classic GSTAT output format compatibility
    std::string formatClassicHeader(const SBEnhanced::DatabaseHeader& header);
    std::string formatClassicTableStats(const SBEnhanced::FileTableStats& stats);
    std::string formatClassicIndexStats(const SBEnhanced::FileIndexStats& stats);
    std::string formatClassicSpaceDistribution(const SBEnhanced::SpaceDistribution& dist);
    
    // Utility methods
    void setOutputStream(std::ostream* stream);
    void setQuietMode(bool quiet);
    SBEnhanced::ClassicGStatOptions getOptions() const;
    SBEnhanced::FileAnalysisResult getAnalysisResult() const;
    
    // Error handling
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;

private:
    // Internal helper methods
    bool validateOptions();
    bool openDatabase();
    void closeDatabase();
    
    // Option processing helpers
    bool processTableFilter(const std::string& table_name);
    bool processSchemaFilter(const std::string& schema_name);
    bool shouldAnalyzeTable(const std::string& table_name);
    bool shouldAnalyzeSchema(const std::string& schema_name);
    
    // Classic output helpers
    void printClassicDatabaseInfo();
    void printClassicPageAnalysis();
    void printClassicTableAnalysis();
    void printClassicIndexAnalysis();
    void printClassicBlobAnalysis();
    void printClassicFragmentationInfo();
    void printClassicFillDistribution();
    
    // Format helpers for classic output
    std::string formatPageFlags(uint8_t flags);
    std::string formatPageType(SBEnhanced::PageType type);
    std::string formatImplementation(uint32_t implementation);
    std::string formatAttributes(uint32_t attributes);
    std::string formatTimestamp(const std::chrono::system_clock::time_point& time);
    std::string formatBytes(uint64_t bytes);
    std::string formatPercentage(double percentage, int precision = 1);
    
    // Analysis flow control
    bool shouldAnalyzeData();
    bool shouldAnalyzeIndexes();
    bool shouldAnalyzeBlobs();
    bool shouldAnalyzeSystem();
    
    // Table and schema filtering
    bool applyTableFilters();
    bool applySchemaFilters();
    
    // Classic statistics aggregation
    void aggregateTableStatistics();
    void aggregateIndexStatistics();
    void aggregateSpaceStatistics();
    
    // Error and warning handling
    void logError(const std::string& error);
    void logWarning(const std::string& warning);
    
    // Progress reporting (classic style)
    void reportProgress(const std::string& operation, uint32_t current, uint32_t total);
    void reportAnalysisStart(const std::string& analysis_type);
    void reportAnalysisComplete(const std::string& analysis_type, 
                               std::chrono::microseconds duration);
};

// Classic GSTAT main function for compatibility
class GSTATClassicMain {
public:
    static int main(int argc, char* argv[]);
    static void printBanner();
    static void handleSignal(int signal);
    
private:
    static GSTATClassic* gstat_instance;
    static bool interrupted;
};