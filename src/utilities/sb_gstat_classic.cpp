#include "sb_gstat_classic.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <getopt.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>

using namespace SBEnhanced;

// Static member definitions
const std::string GSTATClassic::VERSION = "sb_gstat version SB-T0.5.0.1 ScratchBird 0.5 f90eae0";
const std::string GSTATClassic::BUILD_DATE = __DATE__;

// Static members for signal handling
GSTATClassic* GSTATClassicMain::gstat_instance = nullptr;
bool GSTATClassicMain::interrupted = false;

// Constructor
GSTATClassic::GSTATClassic() : output_stream(&std::cout) {
    file_reader = std::make_unique<DatabaseFileReader>();
}

// Destructor
GSTATClassic::~GSTATClassic() {
    closeDatabase();
}

// Parse command line arguments
bool GSTATClassic::parseCommandLine(int argc, char* argv[]) {
    try {
        // Reset options
        options = ClassicGStatOptions();
        
        static struct option long_options[] = {
            {"all", no_argument, 0, 'a'},
            {"data", no_argument, 0, 'd'},
            {"index", no_argument, 0, 'i'},
            {"header", no_argument, 0, 'h'},
            {"encryption", no_argument, 0, 'e'},
            {"system", no_argument, 0, 's'},
            {"record", no_argument, 0, 'r'},
            {"table", required_argument, 0, 't'},
            {"schema", required_argument, 0, 'S'},
            {"user", required_argument, 0, 'u'},
            {"password", required_argument, 0, 'p'},
            {"role", required_argument, 0, 'R'},
            {"trusted", no_argument, 0, 'T'},
            {"fetch", required_argument, 0, 'f'},
            {"nocreation", no_argument, 0, 'n'},
            {"version", no_argument, 0, 'z'},
            {"help", no_argument, 0, '?'},
            {"verbose", no_argument, 0, 'v'},
            {0, 0, 0, 0}
        };
        
        int option_index = 0;
        int c;
        
        while ((c = getopt_long(argc, argv, "adihersnt:S:u:p:R:Tf:zv?", 
                               long_options, &option_index)) != -1) {
            switch (c) {
                case 'a':
                    options.analyze_all = true;
                    options.analyze_data = true;
                    options.analyze_index = true;
                    break;
                case 'd':
                    options.analyze_data = true;
                    break;
                case 'i':
                    options.analyze_index = true;
                    break;
                case 'h':
                    options.analyze_header = true;
                    break;
                case 'e':
                    options.analyze_encryption = true;
                    break;
                case 's':
                    options.analyze_system = true;
                    break;
                case 'r':
                    options.analyze_record = true;
                    break;
                case 't':
                    options.table_names.push_back(optarg);
                    break;
                case 'S': // schema (using -S to avoid conflict with -s for system)
                    options.schema_names.push_back(optarg);
                    break;
                case 'u':
                    options.username = optarg;
                    break;
                case 'p':
                    options.password = optarg;
                    break;
                case 'R':
                    options.role = optarg;
                    break;
                case 'T':
                    options.trusted_auth = true;
                    break;
                case 'f':
                    options.password_file = optarg;
                    break;
                case 'n':
                    options.suppress_creation = true;
                    break;
                case 'z':
                    options.show_version = true;
                    break;
                case 'v':
                    options.verbose = true;
                    break;
                case '?':
                default:
                    options.show_help = true;
                    break;
            }
        }
        
        // Get database path
        if (optind < argc) {
            options.database_path = argv[optind];
        }
        
        return validateOptions();
        
    } catch (const std::exception& e) {
        logError("Error parsing command line: " + std::string(e.what()));
        return false;
    }
}

// Validate options
bool GSTATClassic::validateOptions() {
    if (options.show_help || options.show_version) {
        return true; // These don't need validation
    }
    
    if (options.database_path.empty()) {
        logError("Database path is required");
        return false;
    }
    
    // If no analysis type specified, default to all
    if (!options.analyze_all && !options.analyze_data && !options.analyze_index && 
        !options.analyze_header && !options.analyze_encryption) {
        options.analyze_all = true;
        options.analyze_data = true;
        options.analyze_index = true;
    }
    
    // Set up file reader options
    file_reader->setAnalyzeDataPages(shouldAnalyzeData());
    file_reader->setAnalyzeIndexPages(shouldAnalyzeIndexes());
    file_reader->setAnalyzeBlobPages(shouldAnalyzeBlobs());
    file_reader->setAnalyzeSystemTables(shouldAnalyzeSystem());
    file_reader->setSuppressCreationDate(options.suppress_creation);
    file_reader->setTableFilters(options.table_names);
    file_reader->setSchemaFilters(options.schema_names);
    
    formatter.suppress_creation_date = options.suppress_creation;
    formatter.system_tables_only = options.analyze_system;
    
    return true;
}

// Show usage
void GSTATClassic::showUsage() {
    *output_stream << "sb_gstat - ScratchBird Database Statistics Utility (Classic Mode)" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Usage: sb_gstat [options] database" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Analysis Options:" << std::endl;
    *output_stream << "  -a, --all           analyze data and index pages" << std::endl;
    *output_stream << "  -d, --data          analyze data pages only" << std::endl;
    *output_stream << "  -i, --index         analyze index leaf pages only" << std::endl;
    *output_stream << "  -h, --header        analyze header page ONLY" << std::endl;
    *output_stream << "  -e, --encryption    analyze database encryption status" << std::endl;
    *output_stream << "  -s, --system        analyze system relations in addition to user tables" << std::endl;
    *output_stream << "  -r, --record        analyze average record and version length" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Filter Options:" << std::endl;
    *output_stream << "  -t, --table <name>  analyze specific tables (case sensitive, multiple allowed)" << std::endl;
    *output_stream << "  -S, --schema <name> analyze specific schemas (case sensitive, multiple allowed)" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Authentication Options:" << std::endl;
    *output_stream << "  -u, --user <user>   database username (default: SYSDBA)" << std::endl;
    *output_stream << "  -p, --password <pw> database password" << std::endl;
    *output_stream << "  -R, --role <role>   SQL role name" << std::endl;
    *output_stream << "  -T, --trusted       use trusted authentication" << std::endl;
    *output_stream << "  -f, --fetch <file>  fetch password from file" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Output Options:" << std::endl;
    *output_stream << "  -n, --nocreation    suppress creation date (for testing)" << std::endl;
    *output_stream << "  -v, --verbose       verbose output" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Other Options:" << std::endl;
    *output_stream << "  -z, --version       display version number" << std::endl;
    *output_stream << "  -?, --help          show this help" << std::endl;
    *output_stream << std::endl;
    *output_stream << "Examples:" << std::endl;
    *output_stream << "  sb_gstat -a mydb.fdb" << std::endl;
    *output_stream << "  sb_gstat -d -s mydb.fdb" << std::endl;
    *output_stream << "  sb_gstat -i -t EMPLOYEES mydb.fdb" << std::endl;
    *output_stream << "  sb_gstat -h -n mydb.fdb" << std::endl;
    *output_stream << "  sb_gstat -e mydb.fdb" << std::endl;
}

// Show version
void GSTATClassic::showVersion() {
    *output_stream << VERSION << std::endl;
}

// Main execution
int GSTATClassic::execute() {
    try {
        if (options.show_help) {
            showUsage();
            return 0;
        }
        
        if (options.show_version) {
            showVersion();
            return 0;
        }
        
        if (!openDatabase()) {
            return 1;
        }
        
        // Perform analysis based on options
        bool success = true;
        
        if (options.analyze_header) {
            success &= analyzeHeaderOnly();
        } else {
            success &= analyzeDatabase();
        }
        
        closeDatabase();
        
        return success ? 0 : 1;
        
    } catch (const std::exception& e) {
        logError("Exception during execution: " + std::string(e.what()));
        return 1;
    }
}

// Open database
bool GSTATClassic::openDatabase() {
    if (!file_reader->openDatabase(options.database_path)) {
        logError("Failed to open database: " + options.database_path);
        return false;
    }
    
    if (options.verbose) {
        *output_stream << "Opened database: " << options.database_path << std::endl;
    }
    
    return true;
}

// Close database
void GSTATClassic::closeDatabase() {
    if (file_reader) {
        file_reader->closeDatabase();
    }
}

// Analyze database (full analysis)
bool GSTATClassic::analyzeDatabase() {
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        reportAnalysisStart("Database");
        
        // Perform analysis based on options
        if (options.analyze_all || options.analyze_header) {
            analysis_result = file_reader->performCompleteAnalysis();
        } else if (options.analyze_data) {
            analysis_result = file_reader->performDataAnalysis();
        } else if (options.analyze_index) {
            analysis_result = file_reader->performIndexAnalysis();
        } else if (options.analyze_encryption) {
            analysis_result = file_reader->performEncryptionAnalysis();
        } else {
            analysis_result = file_reader->performHeaderAnalysis();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        reportAnalysisComplete("Database", duration);
        
        // Print results in classic format
        printDatabaseHeader();
        
        if (shouldAnalyzeData()) {
            printTableStatistics();
        }
        
        if (shouldAnalyzeIndexes()) {
            printIndexStatistics();
        }
        
        if (options.analyze_encryption) {
            printEncryptionInfo();
        }
        
        if (options.analyze_system) {
            printSystemTableInfo();
        }
        
        printSpaceDistribution();
        printSummary();
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error during database analysis: " + std::string(e.what()));
        return false;
    }
}

// Analyze header only
bool GSTATClassic::analyzeHeaderOnly() {
    try {
        reportAnalysisStart("Header");
        
        analysis_result = file_reader->performHeaderAnalysis();
        
        reportAnalysisComplete("Header", std::chrono::microseconds(1000));
        
        // Print only header information
        printDatabaseHeader();
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error during header analysis: " + std::string(e.what()));
        return false;
    }
}

// Print database header in classic format
void GSTATClassic::printDatabaseHeader() {
    *output_stream << std::endl;
    *output_stream << "Database \"" << options.database_path << "\"" << std::endl;
    *output_stream << formatClassicHeader(analysis_result.database_header) << std::endl;
}

// Format classic header output
std::string GSTATClassic::formatClassicHeader(const DatabaseHeader& header) {
    std::ostringstream output;
    
    output << "Database header page information:" << std::endl;
    output << "\tFlags\t\t\t0" << std::endl;
    output << "\tGeneration\t\t" << header.sequence_number << std::endl;
    output << "\tSystem Change Number\t" << header.sequence_number << std::endl;
    output << "\tPage size\t\t" << header.page_size << std::endl;
    output << "\tODS version\t\t" << header.ods_version << "." << (header.ods_version % 10) << std::endl;
    output << "\tOldest transaction\t" << header.oldest_transaction << std::endl;
    output << "\tOldest active\t\t" << header.oldest_active << std::endl;
    output << "\tOldest snapshot\t\t" << header.oldest_snapshot << std::endl;
    output << "\tNext transaction\t" << header.next_transaction << std::endl;
    output << "\tBumped transaction\t1" << std::endl;
    output << "\tSequence number\t\t" << header.sequence_number << std::endl;
    output << "\tNext attachment ID\t" << header.next_attachment << std::endl;
    output << "\tImplementation ID\t" << header.implementation << std::endl;
    output << "\tShadow count\t\t" << header.shadow_count << std::endl;
    output << "\tPage buffers\t\t" << header.page_buffers << std::endl;
    output << "\tNext header page\t" << header.next_header_page << std::endl;
    output << "\tDatabase dialect\t" << header.database_dialect << std::endl;
    
    if (!formatter.suppress_creation_date) {
        auto time_t = std::chrono::system_clock::to_time_t(header.creation_time);
        output << "\tCreation date\t\t" << std::put_time(std::localtime(&time_t), "%b %d, %Y %H:%M:%S") << std::endl;
    }
    
    output << "\tAttributes\t\t";
    if (header.force_write) output << "force write";
    if (header.no_reserve) {
        if (header.force_write) output << ", ";
        output << "no reserve";
    }
    if (header.read_only) {
        if (header.force_write || header.no_reserve) output << ", ";
        output << "read only";
    }
    if (!header.force_write && !header.no_reserve && !header.read_only) {
        output << "none";
    }
    output << std::endl;
    
    output << std::endl;
    output << "\tVariable header data:" << std::endl;
    
    if (header.encrypted) {
        output << "\tDatabase encrypted" << std::endl;
        output << "\tCrypt page\t\t" << header.crypt_page << std::endl;
    }
    
    if (header.nbackup_level > 0) {
        output << "\tNBackup level\t\t" << header.nbackup_level << std::endl;
        output << "\tBackup difference file\t" << header.backup_diff_file << std::endl;
    }
    
    output << "\tSweep interval:\t\t" << (header.next_sweep_transaction - header.oldest_transaction) << std::endl;
    
    output << std::endl;
    output << "\t*END*" << std::endl;
    
    return output.str();
}

// Print table statistics in classic format
void GSTATClassic::printTableStatistics() {
    if (analysis_result.table_statistics.empty()) {
        *output_stream << "\nNo table statistics available." << std::endl;
        return;
    }
    
    *output_stream << std::endl;
    for (const auto& table : analysis_result.table_statistics) {
        *output_stream << formatClassicTableStats(table) << std::endl;
    }
}

// Format classic table statistics
std::string GSTATClassic::formatClassicTableStats(const FileTableStats& stats) {
    std::ostringstream output;
    
    output << "Table \"" << stats.table_name << "\" (" << stats.relation_id << ")" << std::endl;
    output << "\tPrimary pointer page: " << stats.pointer_page << ", Index root page: " << stats.index_root << std::endl;
    output << "\tTotal formats: " << stats.total_formats << ", used formats: " << stats.used_formats << std::endl;
    output << "\tAverage record length: " << formatBytes(stats.average_record_length) 
            << ", total records: " << stats.total_records << std::endl;
    output << "\tAverage version length: " << formatBytes(stats.average_version_length)
            << ", total versions: " << stats.total_versions << ", max versions: " << stats.backversions << std::endl;
    output << "\tAverage fragment length: " << formatBytes(stats.average_fragment_length)
            << ", total fragments: " << stats.fragments << std::endl;
    
    if (stats.blob_pages > 0) {
        output << "\tBlob pages: " << stats.blob_pages << std::endl;
    }
    
    output << "\tData pages: " << stats.data_pages << ", data page slots: " << stats.data_pages << std::endl;
    output << "\tPointer pages: " << stats.pointer_pages << ", data page slots: " << stats.data_pages << std::endl;
    
    // Fill distribution
    output << formatClassicSpaceDistribution(stats.fill_distribution);
    
    return output.str();
}

// Format classic space distribution
std::string GSTATClassic::formatClassicSpaceDistribution(const SpaceDistribution& dist) {
    std::ostringstream output;
    
    if (dist.total_pages == 0) {
        return "\tNo pages to analyze\n";
    }
    
    output << "\tFill distribution:" << std::endl;
    output << "\t\t 0 - 19% = " << std::setw(8) << dist.empty_pages << std::endl;
    output << "\t\t20 - 39% = " << std::setw(8) << dist.nearly_empty << std::endl;
    output << "\t\t40 - 59% = " << std::setw(8) << dist.somewhat_full << std::endl;
    output << "\t\t60 - 79% = " << std::setw(8) << dist.nearly_full << std::endl;
    output << "\t\t80 - 99% = " << std::setw(8) << dist.full_pages << std::endl;
    output << "\t\t   100% = " << std::setw(8) << dist.completely_full << std::endl;
    
    return output.str();
}

// Print index statistics
void GSTATClassic::printIndexStatistics() {
    if (analysis_result.index_statistics.empty()) {
        *output_stream << "\nNo index statistics available." << std::endl;
        return;
    }
    
    *output_stream << std::endl;
    for (const auto& index : analysis_result.index_statistics) {
        *output_stream << formatClassicIndexStats(index) << std::endl;
    }
}

// Format classic index statistics
std::string GSTATClassic::formatClassicIndexStats(const FileIndexStats& stats) {
    std::ostringstream output;
    
    output << "Index \"" << stats.index_name << "\" (" << stats.index_id << ")" << std::endl;
    output << "\tRoot page: " << stats.root_page << ", depth: " << stats.depth 
            << ", leaf buckets: " << stats.leaf_buckets << ", nodes: " << stats.nodes << std::endl;
    output << "\tAverage node length: " << formatBytes(stats.average_node_length)
            << ", total dup: " << stats.total_dup_count << ", max dup: " << stats.max_dup_count << std::endl;
    output << "\tAverage key length: " << formatBytes(stats.average_key_length) << std::endl;
    output << "\tCompression ratio: " << formatPercentage(stats.compression_ratio) << std::endl;
    
    if (!stats.field_names.empty()) {
        output << "\tFields: ";
        for (size_t i = 0; i < stats.field_names.size(); ++i) {
            if (i > 0) output << ", ";
            output << stats.field_names[i];
        }
        output << std::endl;
    }
    
    // Fill distribution
    output << formatClassicSpaceDistribution(stats.fill_distribution);
    
    return output.str();
}

// Print encryption info
void GSTATClassic::printEncryptionInfo() {
    const auto& enc = analysis_result.encryption_analysis;
    
    *output_stream << std::endl;
    *output_stream << "Encryption analysis:" << std::endl;
    
    if (!enc.database_encrypted) {
        *output_stream << "\tDatabase is not encrypted" << std::endl;
        return;
    }
    
    *output_stream << "\tDatabase is encrypted" << std::endl;
    *output_stream << "\tTotal pages: " << enc.total_pages << std::endl;
    *output_stream << "\tEncrypted pages: " << enc.encrypted_pages 
                   << " (" << formatPercentage(enc.encryption_percentage) << ")" << std::endl;
    
    if (enc.data_pages_encrypted > 0) {
        *output_stream << "\tData pages encrypted: " << enc.data_pages_encrypted << std::endl;
    }
    if (enc.index_pages_encrypted > 0) {
        *output_stream << "\tIndex pages encrypted: " << enc.index_pages_encrypted << std::endl;
    }
    if (enc.blob_pages_encrypted > 0) {
        *output_stream << "\tBlob pages encrypted: " << enc.blob_pages_encrypted << std::endl;
    }
    
    if (!enc.encryption_plugin.empty()) {
        *output_stream << "\tEncryption plugin: " << enc.encryption_plugin << std::endl;
    }
    if (!enc.key_name.empty()) {
        *output_stream << "\tKey name: " << enc.key_name << std::endl;
    }
}

// Print system table info
void GSTATClassic::printSystemTableInfo() {
    *output_stream << std::endl;
    *output_stream << "System table analysis:" << std::endl;
    *output_stream << "\tSystem tables included in analysis" << std::endl;
    
    // Filter and show only system tables
    for (const auto& table : analysis_result.table_statistics) {
        if (table.table_name.substr(0, 4) == "RDB$") {
            *output_stream << "\t" << table.table_name << ": " 
                          << table.total_records << " records, "
                          << table.data_pages << " pages" << std::endl;
        }
    }
}

// Print space distribution
void GSTATClassic::printSpaceDistribution() {
    *output_stream << std::endl;
    *output_stream << "Overall space distribution:" << std::endl;
    *output_stream << formatClassicSpaceDistribution(analysis_result.overall_space_distribution);
}

// Print summary
void GSTATClassic::printSummary() {
    *output_stream << std::endl;
    *output_stream << "Analysis summary:" << std::endl;
    *output_stream << "\tTotal pages analyzed: " << analysis_result.total_pages_analyzed << std::endl;
    
    if (analysis_result.corrupted_pages > 0) {
        *output_stream << "\tCorrupted pages: " << analysis_result.corrupted_pages << std::endl;
    }
    
    if (analysis_result.orphaned_pages > 0) {
        *output_stream << "\tOrphaned pages: " << analysis_result.orphaned_pages << std::endl;
    }
    
    *output_stream << "\tAnalysis duration: " 
                   << (analysis_result.analysis_duration.count() / 1000.0) << "ms" << std::endl;
    
    if (!analysis_result.errors.empty()) {
        *output_stream << "\tErrors encountered: " << analysis_result.errors.size() << std::endl;
    }
    
    if (!analysis_result.warnings.empty()) {
        *output_stream << "\tWarnings: " << analysis_result.warnings.size() << std::endl;
    }
}

// Helper methods for analysis decisions
bool GSTATClassic::shouldAnalyzeData() {
    return options.analyze_all || options.analyze_data;
}

bool GSTATClassic::shouldAnalyzeIndexes() {
    return options.analyze_all || options.analyze_index;
}

bool GSTATClassic::shouldAnalyzeBlobs() {
    return options.analyze_all || options.analyze_data;
}

bool GSTATClassic::shouldAnalyzeSystem() {
    return options.analyze_system;
}

// Format helper methods
std::string GSTATClassic::formatBytes(uint64_t bytes) {
    if (bytes == 0) return "0";
    
    const char* units[] = {"", "K", "M", "G", "T"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    std::ostringstream oss;
    if (unit == 0) {
        oss << static_cast<uint64_t>(size);
    } else {
        oss << std::fixed << std::setprecision(1) << size << units[unit];
    }
    
    return oss.str();
}

std::string GSTATClassic::formatPercentage(double percentage, int precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << percentage << "%";
    return oss.str();
}

// Progress reporting
void GSTATClassic::reportProgress(const std::string& operation, uint32_t current, uint32_t total) {
    if (options.verbose && total > 0) {
        double percentage = (static_cast<double>(current) / total) * 100.0;
        *output_stream << "\r" << operation << ": " << formatPercentage(percentage, 0) 
                       << " (" << current << "/" << total << ")";
        output_stream->flush();
    }
}

void GSTATClassic::reportAnalysisStart(const std::string& analysis_type) {
    if (options.verbose) {
        *output_stream << "Starting " << analysis_type << " analysis..." << std::endl;
    }
}

void GSTATClassic::reportAnalysisComplete(const std::string& analysis_type, 
                                        std::chrono::microseconds duration) {
    if (options.verbose) {
        *output_stream << analysis_type << " analysis completed in " 
                       << (duration.count() / 1000.0) << "ms" << std::endl;
    }
}

// Error handling
void GSTATClassic::logError(const std::string& error) {
    std::cerr << "ERROR: " << error << std::endl;
}

void GSTATClassic::logWarning(const std::string& warning) {
    if (options.verbose) {
        std::cerr << "WARNING: " << warning << std::endl;
    }
}

// Getters
ClassicGStatOptions GSTATClassic::getOptions() const {
    return options;
}

FileAnalysisResult GSTATClassic::getAnalysisResult() const {
    return analysis_result;
}

std::vector<std::string> GSTATClassic::getErrors() const {
    return file_reader ? file_reader->getErrors() : std::vector<std::string>();
}

std::vector<std::string> GSTATClassic::getWarnings() const {
    return file_reader ? file_reader->getWarnings() : std::vector<std::string>();
}

std::string GSTATClassic::getLastError() const {
    return file_reader ? file_reader->getLastError() : "";
}

// Setters
void GSTATClassic::setOutputStream(std::ostream* stream) {
    output_stream = stream;
}

void GSTATClassic::setQuietMode(bool quiet) {
    quiet_mode = quiet;
}

// Static methods for main class
void GSTATClassicMain::printBanner() {
    std::cout << "ScratchBird GSTAT - Classic Mode" << std::endl;
    std::cout << "Database Statistics Utility" << std::endl;
    std::cout << std::endl;
}

void GSTATClassicMain::handleSignal(int signal) {
    interrupted = true;
    if (gstat_instance) {
        std::cout << "\nInterrupted by signal " << signal << std::endl;
        exit(1);
    }
}

int GSTATClassicMain::main(int argc, char* argv[]) {
    try {
        // Setup signal handling
        signal(SIGINT, handleSignal);
        signal(SIGTERM, handleSignal);
        
        // Create GSTAT instance
        GSTATClassic gstat;
        gstat_instance = &gstat;
        
        // Parse command line
        if (!gstat.parseCommandLine(argc, argv)) {
            gstat.showUsage();
            return 1;
        }
        
        // Execute
        int result = gstat.execute();
        
        gstat_instance = nullptr;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error" << std::endl;
        return 1;
    }
}