#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include "sb_database.h"

// Version information
static const char* VERSION = "sb_gstat version SB-T0.5.0.1 ScratchBird 0.5 f90eae0";

// Analysis options
struct GStatOptions {
    std::string database_name;
    std::string username;
    std::string password;
    std::string role;
    std::string table_name;
    std::string schema_name;
    
    // Database connection
    std::unique_ptr<SBDatabase> database;
    
    bool analyze_all = false;
    bool analyze_data = false;
    bool analyze_index = false;
    bool analyze_header = false;
    bool analyze_system = false;
    bool analyze_record = false;
    bool analyze_encryption = false;
    bool no_creation = false;
    bool trusted_auth = false;
    bool fetch_password = false;
    bool version = false;
    bool help = false;
};

static void showUsage() {
    std::cout << "sb_gstat - ScratchBird database analysis tool" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: sb_gstat [options] database" << std::endl;
    std::cout << std::endl;
    std::cout << "Analysis Options:" << std::endl;
    std::cout << "  -a[ll]               analyze data and index pages" << std::endl;
    std::cout << "  -d[ata]              analyze data pages" << std::endl;
    std::cout << "  -i[ndex]             analyze index leaf pages" << std::endl;
    std::cout << "  -h[eader]            analyze header page" << std::endl;
    std::cout << "  -s[ystem]            analyze system relations" << std::endl;
    std::cout << "  -r[ecord]            analyze record versions" << std::endl;
    std::cout << "  -e[ncryption]        analyze database encryption" << std::endl;
    std::cout << std::endl;
    std::cout << "Filter Options:" << std::endl;
    std::cout << "  -t[able] <table>     analyze specific table" << std::endl;
    std::cout << "  -sch[ema] <schema>   analyze specific schema" << std::endl;
    std::cout << std::endl;
    std::cout << "Connection Options:" << std::endl;
    std::cout << "  -u[ser] <username>   database username" << std::endl;
    std::cout << "  -p[assword] <pass>   database password" << std::endl;
    std::cout << "  -role <role>         SQL role name" << std::endl;
    std::cout << "  -trusted             use trusted authentication" << std::endl;
    std::cout << "  -fetch_password      fetch password from file" << std::endl;
    std::cout << std::endl;
    std::cout << "Other Options:" << std::endl;
    std::cout << "  -nocreation          don't show creation date" << std::endl;
    std::cout << "  -z                   show version" << std::endl;
    std::cout << "  -?                   show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  sb_gstat -h mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -a -user SYSDBA -password masterkey mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -t CUSTOMERS mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -schema FINANCE mydb.fdb" << std::endl;
}

static void showVersion() {
    std::cout << VERSION << std::endl;
}

static bool parseCommandLine(int argc, char* argv[], GStatOptions& options) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-z") {
            options.version = true;
        } else if (arg == "-?" || arg == "-help") {
            options.help = true;
        } else if (arg == "-a" || arg == "-all") {
            options.analyze_all = true;
        } else if (arg == "-d" || arg == "-data") {
            options.analyze_data = true;
        } else if (arg == "-i" || arg == "-index") {
            options.analyze_index = true;
        } else if (arg == "-h" || arg == "-header") {
            options.analyze_header = true;
        } else if (arg == "-s" || arg == "-system") {
            options.analyze_system = true;
        } else if (arg == "-r" || arg == "-record") {
            options.analyze_record = true;
        } else if (arg == "-e" || arg == "-encryption") {
            options.analyze_encryption = true;
        } else if (arg == "-t" || arg == "-table") {
            if (i + 1 < argc) {
                options.table_name = argv[++i];
            }
        } else if (arg == "-sch" || arg == "-schema") {
            if (i + 1 < argc) {
                options.schema_name = argv[++i];
            }
        } else if (arg == "-u" || arg == "-user") {
            if (i + 1 < argc) {
                options.username = argv[++i];
            }
        } else if (arg == "-p" || arg == "-password") {
            if (i + 1 < argc) {
                options.password = argv[++i];
            }
        } else if (arg == "-role") {
            if (i + 1 < argc) {
                options.role = argv[++i];
            }
        } else if (arg == "-trusted") {
            options.trusted_auth = true;
        } else if (arg == "-fetch_password") {
            options.fetch_password = true;
        } else if (arg == "-nocreation") {
            options.no_creation = true;
        } else if (arg[0] != '-') {
            options.database_name = arg;
        }
    }
    
    return true;
}

static void showDatabaseHeader(GStatOptions& options) {
    std::cout << "Database header page information:" << std::endl;
    std::cout << "  Database name: " << options.database_name << std::endl;
    
    if (!options.database || !options.database->isConnected()) {
        std::cerr << "Not connected to database" << std::endl;
        return;
    }
    
    SBDatabase::DatabaseStats stats;
    if (options.database->getDatabaseStats(stats)) {
        std::cout << "  Page size: " << stats.page_size << std::endl;
        std::cout << "  Page count: " << stats.page_count << std::endl;
        std::cout << "  Database size: " << std::fixed << std::setprecision(1) 
                  << (stats.page_count * stats.page_size) / (1024.0 * 1024.0) << " MB" << std::endl;
        if (!options.no_creation) {
            std::cout << "  Creation date: " << stats.creation_date << std::endl;
        }
        std::cout << "  Database version: " << stats.database_version << std::endl;
        std::cout << "  Forced writes: " << (stats.force_writes ? "Yes" : "No") << std::endl;
        std::cout << "  Read-only: " << (stats.read_only ? "Yes" : "No") << std::endl;
        std::cout << "  Allocated pages: " << stats.allocated_pages << std::endl;
        std::cout << "  Free pages: " << stats.free_pages << std::endl;
    } else {
        std::cerr << "Failed to get database statistics: " << options.database->getLastError() << std::endl;
    }
    
    std::cout << std::endl;
}

static void showDataAnalysis(GStatOptions& options) {
    std::cout << "Data page analysis:" << std::endl;
    
    if (!options.database || !options.database->isConnected()) {
        std::cerr << "Not connected to database" << std::endl;
        return;
    }
    
    // Get table statistics
    std::string sql = "SELECT COUNT(*) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0";
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> columns;
    
    if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
        std::cout << "  User tables: " << results[0][0] << std::endl;
    }
    
    // Get total record count across all tables
    sql = "SELECT SUM(RDB$RECORD_COUNT) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0";
    if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
        std::cout << "  Total records: " << (results[0][0].empty() ? "0" : results[0][0]) << std::endl;
    }
    
    std::cout << std::endl;
}

static void showIndexAnalysis(GStatOptions& options) {
    std::cout << "Index page analysis:" << std::endl;
    
    if (!options.database || !options.database->isConnected()) {
        std::cerr << "Not connected to database" << std::endl;
        return;
    }
    
    // Get index statistics
    std::string sql = "SELECT COUNT(*) FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0";
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> columns;
    
    if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
        std::cout << "  User indexes: " << results[0][0] << std::endl;
    }
    
    // Get unique vs non-unique indexes
    sql = "SELECT COUNT(*) FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0 AND RDB$UNIQUE_FLAG = 1";
    if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
        std::cout << "  Unique indexes: " << results[0][0] << std::endl;
    }
    
    std::cout << std::endl;
}

static void showSystemAnalysis(GStatOptions& options) {
    std::cout << "System relations analysis:" << std::endl;
    
    if (!options.database || !options.database->isConnected()) {
        std::cerr << "Not connected to database" << std::endl;
        return;
    }
    
    // Get system table counts
    std::vector<std::pair<std::string, std::string>> system_tables = {
        {"RDB$RELATIONS", "Tables and views"},
        {"RDB$RELATION_FIELDS", "Table fields"},
        {"RDB$FIELDS", "Field definitions"},
        {"RDB$INDICES", "Index definitions"},
        {"RDB$INDEX_SEGMENTS", "Index segments"},
        {"RDB$PROCEDURES", "Stored procedures"},
        {"RDB$TRIGGERS", "Triggers"},
        {"RDB$SCHEMAS", "Schemas"}
    };
    
    for (const auto& table : system_tables) {
        std::string sql = "SELECT COUNT(*) FROM " + table.first;
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
            std::cout << "  " << table.first << ": " << results[0][0] << " records (" << table.second << ")" << std::endl;
        }
    }
    
    std::cout << std::endl;
}

static void showTableAnalysis(GStatOptions& options) {
    if (!options.table_name.empty()) {
        std::cout << "Table analysis for: " << options.table_name << std::endl;
        
        if (!options.database || !options.database->isConnected()) {
            std::cerr << "Not connected to database" << std::endl;
            return;
        }
        
        // Check if table exists
        if (!options.database->tableExists(options.table_name, options.schema_name)) {
            std::cerr << "Table not found: " << options.table_name << std::endl;
            return;
        }
        
        // Get table statistics
        std::string sql = "SELECT COUNT(*) FROM " + options.table_name;
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
            std::cout << "  Total records: " << results[0][0] << std::endl;
        }
        
        // Get table structure info
        sql = "SELECT rf.RDB$FIELD_NAME, f.RDB$FIELD_TYPE, f.RDB$FIELD_LENGTH ";
        sql += "FROM RDB$RELATION_FIELDS rf ";
        sql += "JOIN RDB$FIELDS f ON rf.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME ";
        sql += "WHERE rf.RDB$RELATION_NAME = '" + options.table_name + "' ";
        sql += "ORDER BY rf.RDB$FIELD_POSITION";
        
        if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
            std::cout << "  Field count: " << results.size() << std::endl;
            std::cout << "  Fields:" << std::endl;
            for (const auto& row : results) {
                if (row.size() >= 3) {
                    std::string field_name = row[0];
                    field_name.erase(field_name.find_last_not_of(" \t\n\r\f\v") + 1);
                    std::cout << "    " << field_name << " (Type: " << row[1] << ", Length: " << row[2] << ")" << std::endl;
                }
            }
        }
        
        // Get indexes for this table
        sql = "SELECT RDB$INDEX_NAME, RDB$UNIQUE_FLAG FROM RDB$INDICES WHERE RDB$RELATION_NAME = '" + options.table_name + "'";
        if (options.database->executeSelect(sql, results, columns) && !results.empty()) {
            std::cout << "  Indexes:" << std::endl;
            for (const auto& row : results) {
                if (row.size() >= 2) {
                    std::string index_name = row[0];
                    index_name.erase(index_name.find_last_not_of(" \t\n\r\f\v") + 1);
                    bool unique = (row[1] == "1");
                    std::cout << "    " << index_name << (unique ? " (UNIQUE)" : "") << std::endl;
                }
            }
        }
        
        std::cout << std::endl;
    }
}

static void showSchemaAnalysis(GStatOptions& options) {
    if (!options.schema_name.empty()) {
        std::cout << "Schema analysis for: " << options.schema_name << std::endl;
        
        if (!options.database || !options.database->isConnected()) {
            std::cerr << "Not connected to database" << std::endl;
            return;
        }
        
        // Check if schema exists
        if (!options.database->schemaExists(options.schema_name)) {
            std::cerr << "Schema not found: " << options.schema_name << std::endl;
            return;
        }
        
        // Get tables in schema
        std::vector<std::string> tables = options.database->getTableNames(options.schema_name);
        std::cout << "  Tables in schema: " << tables.size() << std::endl;
        
        if (!tables.empty()) {
            std::cout << "  Table list:" << std::endl;
            for (const auto& table : tables) {
                std::cout << "    " << table << std::endl;
            }
        }
        
        std::cout << std::endl;
    }
}

static void showEncryptionAnalysis(GStatOptions& options) {
    std::cout << "Database encryption analysis:" << std::endl;
    
    if (!options.database || !options.database->isConnected()) {
        std::cerr << "Not connected to database" << std::endl;
        return;
    }
    
    // Query encryption status (simplified)
    std::cout << "  Encryption status: Not implemented in this version" << std::endl;
    std::cout << "  Encryption plugin: N/A" << std::endl;
    std::cout << "  Key name: N/A" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    GStatOptions options;
    
    if (!parseCommandLine(argc, argv, options)) {
        return 1;
    }
    
    if (options.version) {
        showVersion();
        return 0;
    }
    
    if (options.help) {
        showUsage();
        return 0;
    }
    
    if (options.database_name.empty()) {
        std::cerr << "sb_gstat: Database name is required" << std::endl;
        showUsage();
        return 1;
    }
    
    // Connect to database
    options.database = std::make_unique<SBDatabase>();
    
    if (!options.database->connect(options.database_name, options.username, 
                                  options.password, options.role, options.trusted_auth)) {
        std::cerr << "Failed to connect to database: " << options.database->getLastError() << std::endl;
        return 1;
    }
    
    // Show connection information
    if (!options.username.empty()) {
        std::cout << "Username: " << options.username << std::endl;
    }
    
    if (!options.role.empty()) {
        std::cout << "Role: " << options.role << std::endl;
    }
    
    if (options.trusted_auth) {
        std::cout << "Using trusted authentication" << std::endl;
    }
    
    std::cout << "Connected to database successfully" << std::endl;
    std::cout << std::endl;
    
    // Perform analysis based on options
    if (options.analyze_all) {
        showDatabaseHeader(options);
        showDataAnalysis(options);
        showIndexAnalysis(options);
        showSystemAnalysis(options);
    } else {
        if (options.analyze_header) {
            showDatabaseHeader(options);
        }
        
        if (options.analyze_data) {
            showDataAnalysis(options);
        }
        
        if (options.analyze_index) {
            showIndexAnalysis(options);
        }
        
        if (options.analyze_system) {
            showSystemAnalysis(options);
        }
        
        if (options.analyze_encryption) {
            showEncryptionAnalysis(options);
        }
    }
    
    // Specific table or schema analysis
    if (!options.table_name.empty()) {
        showTableAnalysis(options);
    }
    
    if (!options.schema_name.empty()) {
        showSchemaAnalysis(options);
    }
    
    // Default to header analysis if no specific analysis requested
    if (!options.analyze_all && !options.analyze_header && !options.analyze_data && 
        !options.analyze_index && !options.analyze_system && !options.analyze_record &&
        !options.analyze_encryption && options.table_name.empty() && options.schema_name.empty()) {
        showDatabaseHeader(options);
    }
    
    std::cout << "Analysis completed successfully" << std::endl;
    return 0;
}