#include "sb_gstat_enhanced.h"
#include "sb_gstat_web_interface.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <ctime>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cmath>
#include <numeric>

using namespace SBEnhanced;

// Constructor
GSTATEnhanced::GSTATEnhanced()
    : total_collections(0),
      successful_collections(0),
      failed_collections(0),
      web_interface(nullptr)
{
    session_start_time = std::chrono::steady_clock::now();
    
    // Initialize components
    engine = std::make_unique<SBEngineIntegration>();
    formatter = std::make_unique<OutputFormatter>();
    analyzer = std::make_unique<QueryAnalyzer>();
    config = std::make_unique<UtilityConfiguration>();
    
    // Initialize web interface
    web_interface = new GSTATWebInterface(this);
    
    // Initialize analyzers
    initializeAnalyzers();
    
    // Load default configuration
    loadDefaultConfiguration();
}

// Destructor
GSTATEnhanced::~GSTATEnhanced()
{
    stopMonitoring();
    
    // Stop web interface
    if (web_interface) {
        web_interface->stop();
        delete web_interface;
        web_interface = nullptr;
    }
    
    if (output_file && output_file->is_open()) {
        output_file->close();
    }
    
    if (log_file && log_file->is_open()) {
        log_file->close();
    }
    
    if (engine && engine->isConnected()) {
        disconnect();
    }
}

// Initialize enhanced GSTAT
bool GSTATEnhanced::initialize(const ConnectionOptions& options)
{
    try {
        // Initialize engine
        if (!engine->initialize(options)) {
            logError("Failed to initialize database engine");
            return false;
        }
        
        // Initialize formatter
        if (!formatter->initialize()) {
            logError("Failed to initialize output formatter");
            return false;
        }
        
        // Initialize analyzer
        if (!analyzer->initialize()) {
            logError("Failed to initialize query analyzer");
            return false;
        }
        
        // Initialize configuration
        if (!config->initialize(options.config_file)) {
            logError("Failed to initialize configuration");
            return false;
        }
        
        // Initialize default alerts and reports
        initializeDefaultAlerts();
        initializeDefaultReports();
        
        logMessage("GSTAT Enhanced initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error initializing GSTAT Enhanced: " + std::string(e.what()));
        return false;
    }
}

// Load configuration
bool GSTATEnhanced::loadConfiguration(const std::string& config_file)
{
    try {
        if (!config->loadFromFile(config_file)) {
            logError("Failed to load configuration from: " + config_file);
            return false;
        }
        
        // Apply configuration settings
        monitoring_config.collection_interval = std::chrono::seconds(
            config->getIntValue("monitoring.collection_interval", 60));
        monitoring_config.retention_period = std::chrono::seconds(
            config->getIntValue("monitoring.retention_period", 86400 * 30));
        monitoring_config.enable_real_time = config->getBoolValue("monitoring.enable_real_time", false);
        monitoring_config.enable_alerts = config->getBoolValue("monitoring.enable_alerts", false);
        monitoring_config.enable_web_interface = config->getBoolValue("monitoring.enable_web_interface", false);
        monitoring_config.web_port = config->getIntValue("monitoring.web_port", 8080);
        monitoring_config.web_bind_address = config->getStringValue("monitoring.web_bind_address", "127.0.0.1");
        monitoring_config.storage_path = config->getStringValue("monitoring.storage_path", "./gstat_data");
        
        logMessage("Configuration loaded successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error loading configuration: " + std::string(e.what()));
        return false;
    }
}

// Connect to database
bool GSTATEnhanced::connect(const std::string& database_path, const std::string& username, 
                           const std::string& password, const std::string& role)
{
    try {
        if (!engine->connect(database_path, username, password, role)) {
            logError("Failed to connect to database: " + engine->getLastError());
            return false;
        }
        
        current_database = database_path;
        
        // Validate database
        if (!validateDatabase()) {
            logError("Database validation failed");
            return false;
        }
        
        logMessage("Connected to database: " + database_path);
        return true;
        
    } catch (const std::exception& e) {
        logError("Error connecting to database: " + std::string(e.what()));
        return false;
    }
}

// Disconnect from database
bool GSTATEnhanced::disconnect()
{
    try {
        if (engine && engine->isConnected()) {
            engine->disconnect();
        }
        
        current_database.clear();
        logMessage("Disconnected from database");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error disconnecting from database: " + std::string(e.what()));
        return false;
    }
}

// Check if connected
bool GSTATEnhanced::isConnected() const
{
    return engine && engine->isConnected();
}

// Collect statistics
bool GSTATEnhanced::collectStatistics(const StatisticsOptions& options)
{
    if (!validateStatisticsOptions(options)) {
        return false;
    }
    
    if (!isConnected()) {
        logError("Not connected to database");
        return false;
    }
    
    try {
        auto start_time = std::chrono::steady_clock::now();
        total_collections++;
        
        bool success = true;
        
        // Collect database statistics
        if (options.categories.empty() || options.categories.count(StatCategory::DATABASE_OVERVIEW)) {
            DatabaseStatistics db_stats;
            if (collectDatabaseStatistics(db_stats)) {
                database_stats[current_database] = db_stats;
            } else {
                success = false;
            }
        }
        
        // Collect table statistics
        if (options.categories.empty() || options.categories.count(StatCategory::TABLE_STATISTICS)) {
            std::vector<TableStatistics> tables;
            
            // Get table list
            std::vector<std::string> table_names = engine->getTableNames(options.schema_filter);
            for (const auto& table_name : table_names) {
                if (!options.table_filter.empty() && 
                    table_name.find(options.table_filter) == std::string::npos) {
                    continue;
                }
                
                TableStatistics table_stat;
                if (collectTableStatistics(table_name, table_stat)) {
                    tables.push_back(table_stat);
                }
            }
            
            if (!tables.empty()) {
                table_stats[current_database] = tables;
            }
        }
        
        // Collect index statistics
        if (options.categories.empty() || options.categories.count(StatCategory::INDEX_STATISTICS)) {
            std::vector<IndexStatistics> indexes;
            
            // Get index list
            std::vector<std::string> index_names = engine->getIndexNames(options.schema_filter);
            for (const auto& index_name : index_names) {
                IndexStatistics index_stat;
                if (collectIndexStatistics(index_name, index_stat)) {
                    indexes.push_back(index_stat);
                }
            }
            
            if (!indexes.empty()) {
                index_stats[current_database] = indexes;
            }
        }
        
        // Collect transaction statistics
        if (options.categories.empty() || options.categories.count(StatCategory::TRANSACTION_STATISTICS)) {
            TransactionStatistics trans_stats;
            if (collectTransactionStatistics(trans_stats)) {
                transaction_stats[current_database] = trans_stats;
            } else {
                success = false;
            }
        }
        
        // Collect connection statistics
        if (options.categories.empty() || options.categories.count(StatCategory::CONNECTION_STATISTICS)) {
            ConnectionStatistics conn_stats;
            if (collectConnectionStatistics(conn_stats)) {
                connection_stats[current_database] = conn_stats;
            } else {
                success = false;
            }
        }
        
        // Collect performance metrics
        if (options.categories.empty() || options.categories.count(StatCategory::PERFORMANCE_COUNTERS)) {
            PerformanceMetrics perf_metrics;
            if (collectPerformanceMetrics(perf_metrics)) {
                database_stats[current_database].performance = perf_metrics;
            } else {
                success = false;
            }
        }
        
        // Collect storage statistics
        if (options.categories.empty() || options.categories.count(StatCategory::STORAGE_STATISTICS)) {
            StorageStatistics storage_stat;
            if (collectStorageStatistics(storage_stat)) {
                storage_stats[current_database] = storage_stat;
            } else {
                success = false;
            }
        }
        
        // Collect cache statistics
        if (options.categories.empty() || options.categories.count(StatCategory::CACHE_STATISTICS)) {
            CacheStatistics cache_stat;
            if (collectCacheStatistics(cache_stat)) {
                cache_stats[current_database] = cache_stat;
            } else {
                success = false;
            }
        }
        
        // Collect lock statistics
        if (options.categories.empty() || options.categories.count(StatCategory::LOCK_STATISTICS)) {
            LockStatistics lock_stat;
            if (collectLockStatistics(lock_stat)) {
                lock_stats[current_database] = lock_stat;
            } else {
                success = false;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (success) {
            successful_collections++;
            logMessage("Statistics collection completed in " + formatDuration(duration));
        } else {
            failed_collections++;
            logError("Statistics collection completed with errors");
        }
        
        return success;
        
    } catch (const std::exception& e) {
        failed_collections++;
        logError("Error collecting statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect database statistics
bool GSTATEnhanced::collectDatabaseStatistics(DatabaseStatistics& stats)
{
    try {
        stats.database_name = current_database;
        stats.database_path = current_database;
        stats.collection_time = std::chrono::system_clock::now();
        
        // Get basic database information
        QueryResults results;
        
        // Database size and pages
        std::string query = "SELECT MON$DATABASE_NAME, MON$PAGE_SIZE, MON$PAGES FROM MON$DATABASE";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.page_size = std::stoull(results.rows[0][1]);
                stats.pages_allocated = std::stoull(results.rows[0][2]);
                stats.database_size_bytes = stats.page_size * stats.pages_allocated;
            }
        }
        
        // Object counts
        query = "SELECT 'TABLES' AS OBJECT_TYPE, COUNT(*) AS COUNT FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 "
                "UNION ALL SELECT 'VIEWS', COUNT(*) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0 AND RDB$VIEW_BLR IS NOT NULL "
                "UNION ALL SELECT 'PROCEDURES', COUNT(*) FROM RDB$PROCEDURES WHERE RDB$SYSTEM_FLAG = 0 "
                "UNION ALL SELECT 'FUNCTIONS', COUNT(*) FROM RDB$FUNCTIONS WHERE RDB$SYSTEM_FLAG = 0 "
                "UNION ALL SELECT 'TRIGGERS', COUNT(*) FROM RDB$TRIGGERS WHERE RDB$SYSTEM_FLAG = 0 "
                "UNION ALL SELECT 'DOMAINS', COUNT(*) FROM RDB$FIELDS WHERE RDB$SYSTEM_FLAG = 0 "
                "UNION ALL SELECT 'GENERATORS', COUNT(*) FROM RDB$GENERATORS WHERE RDB$SYSTEM_FLAG = 0 "
                "UNION ALL SELECT 'ROLES', COUNT(*) FROM RDB$ROLES "
                "UNION ALL SELECT 'INDEXES', COUNT(*) FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0";
        
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                std::string object_type = row[0];
                uint64_t count = std::stoull(row[1]);
                
                if (object_type == "TABLES") stats.table_count = count;
                else if (object_type == "VIEWS") stats.view_count = count;
                else if (object_type == "PROCEDURES") stats.procedure_count = count;
                else if (object_type == "FUNCTIONS") stats.function_count = count;
                else if (object_type == "TRIGGERS") stats.trigger_count = count;
                else if (object_type == "DOMAINS") stats.domain_count = count;
                else if (object_type == "GENERATORS") stats.generator_count = count;
                else if (object_type == "ROLES") stats.role_count = count;
                else if (object_type == "INDEXES") stats.index_count = count;
            }
        }
        
        // Database version and ODS
        query = "SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') AS ENGINE_VERSION FROM RDB$DATABASE";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.database_version = results.rows[0][0];
            }
        }
        
        // Get attached users
        query = "SELECT DISTINCT MON$USER FROM MON$ATTACHMENTS WHERE MON$ATTACHMENT_ID <> CURRENT_CONNECTION";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.attached_users.push_back(row[0]);
            }
        }
        
        // Get active transactions
        query = "SELECT MON$TRANSACTION_ID, MON$STATE FROM MON$TRANSACTIONS WHERE MON$STATE = 1";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.active_transactions.push_back(row[0]);
            }
        }
        
        // Schema count (if hierarchical schema support is available)
        query = "SELECT COUNT(*) FROM RDB$SCHEMAS";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.schema_count = std::stoull(results.rows[0][0]);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting database statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect table statistics
bool GSTATEnhanced::collectTableStatistics(const std::string& table_name, TableStatistics& stats)
{
    try {
        stats.table_name = table_name;
        stats.last_analyzed = std::chrono::system_clock::now();
        
        // Get table record count
        std::string query = "SELECT COUNT(*) FROM " + table_name;
        QueryResults results;
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.record_count = std::stoull(results.rows[0][0]);
            }
        }
        
        // Get table statistics from system tables
        query = "SELECT RDB$RELATION_NAME, RDB$SYSTEM_FLAG FROM RDB$RELATIONS WHERE RDB$RELATION_NAME = '" + table_name + "'";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                // Table exists, get more detailed statistics
                
                // Get indexes for this table
                query = "SELECT RDB$INDEX_NAME FROM RDB$INDICES WHERE RDB$RELATION_NAME = '" + table_name + "'";
                if (engine->executeQuery(query, results)) {
                    for (const auto& row : results.rows) {
                        stats.indexes.push_back(row[0]);
                    }
                }
                
                // Get constraints for this table
                query = "SELECT RDB$CONSTRAINT_NAME, RDB$CONSTRAINT_TYPE FROM RDB$RELATION_CONSTRAINTS WHERE RDB$RELATION_NAME = '" + table_name + "'";
                if (engine->executeQuery(query, results)) {
                    for (const auto& row : results.rows) {
                        stats.constraints.push_back(row[0] + " (" + row[1] + ")");
                    }
                }
                
                // Get triggers for this table
                query = "SELECT RDB$TRIGGER_NAME FROM RDB$TRIGGERS WHERE RDB$RELATION_NAME = '" + table_name + "'";
                if (engine->executeQuery(query, results)) {
                    for (const auto& row : results.rows) {
                        stats.triggers.push_back(row[0]);
                    }
                }
                
                // Estimate average record size
                if (stats.record_count > 0) {
                    // Get column information to estimate record size
                    query = "SELECT RDB$FIELD_NAME, RDB$FIELD_TYPE, RDB$FIELD_LENGTH FROM RDB$RELATION_FIELDS "
                            "WHERE RDB$RELATION_NAME = '" + table_name + "'";
                    if (engine->executeQuery(query, results)) {
                        uint64_t estimated_size = 0;
                        for (const auto& row : results.rows) {
                            int field_type = std::stoi(row[1]);
                            int field_length = std::stoi(row[2]);
                            
                            // Estimate size based on field type
                            switch (field_type) {
                                case 7: // SMALLINT
                                    estimated_size += 2;
                                    break;
                                case 8: // INTEGER
                                    estimated_size += 4;
                                    break;
                                case 10: // FLOAT
                                    estimated_size += 4;
                                    break;
                                case 27: // DOUBLE
                                    estimated_size += 8;
                                    break;
                                case 12: // DATE
                                    estimated_size += 4;
                                    break;
                                case 13: // TIME
                                    estimated_size += 4;
                                    break;
                                case 35: // TIMESTAMP
                                    estimated_size += 8;
                                    break;
                                case 37: // VARCHAR
                                case 14: // CHAR
                                    estimated_size += field_length;
                                    break;
                                case 261: // BLOB
                                    estimated_size += 8; // BLOB ID
                                    break;
                                default:
                                    estimated_size += field_length;
                            }
                        }
                        stats.average_record_size = estimated_size;
                    }
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting table statistics for " + table_name + ": " + std::string(e.what()));
        return false;
    }
}

// Collect index statistics
bool GSTATEnhanced::collectIndexStatistics(const std::string& index_name, IndexStatistics& stats)
{
    try {
        stats.index_name = index_name;
        stats.last_analyzed = std::chrono::system_clock::now();
        
        // Get index information
        std::string query = "SELECT I.RDB$INDEX_NAME, I.RDB$RELATION_NAME, I.RDB$UNIQUE_FLAG, "
                           "I.RDB$INDEX_TYPE, I.RDB$STATISTICS "
                           "FROM RDB$INDICES I WHERE I.RDB$INDEX_NAME = '" + index_name + "'";
        
        QueryResults results;
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.table_name = results.rows[0][1];
                stats.is_unique = (results.rows[0][2] == "1");
                
                // Get selectivity (stored as RDB$STATISTICS)
                if (!results.rows[0][4].empty()) {
                    stats.selectivity = std::stod(results.rows[0][4]);
                }
                
                // Get index segments (columns)
                query = "SELECT RDB$FIELD_NAME FROM RDB$INDEX_SEGMENTS "
                        "WHERE RDB$INDEX_NAME = '" + index_name + "' ORDER BY RDB$FIELD_POSITION";
                if (engine->executeQuery(query, results)) {
                    for (const auto& row : results.rows) {
                        stats.columns.push_back(row[0]);
                    }
                }
                
                // Check if this is a primary key index
                query = "SELECT RDB$CONSTRAINT_NAME FROM RDB$RELATION_CONSTRAINTS "
                        "WHERE RDB$INDEX_NAME = '" + index_name + "' AND RDB$CONSTRAINT_TYPE = 'PRIMARY KEY'";
                if (engine->executeQuery(query, results)) {
                    stats.is_primary = !results.rows.empty();
                }
                
                // Check if this is a foreign key index
                query = "SELECT RDB$CONSTRAINT_NAME FROM RDB$RELATION_CONSTRAINTS "
                        "WHERE RDB$INDEX_NAME = '" + index_name + "' AND RDB$CONSTRAINT_TYPE = 'FOREIGN KEY'";
                if (engine->executeQuery(query, results)) {
                    stats.is_foreign = !results.rows.empty();
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting index statistics for " + index_name + ": " + std::string(e.what()));
        return false;
    }
}

// Collect transaction statistics
bool GSTATEnhanced::collectTransactionStatistics(TransactionStatistics& stats)
{
    try {
        stats.collection_time = std::chrono::system_clock::now();
        
        // Get transaction information from monitoring tables
        std::string query = "SELECT MON$STATE, COUNT(*) FROM MON$TRANSACTIONS GROUP BY MON$STATE";
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                int state = std::stoi(row[0]);
                uint64_t count = std::stoull(row[1]);
                
                switch (state) {
                    case 0: // idle
                        // Add to total
                        break;
                    case 1: // active
                        stats.active_transactions += count;
                        break;
                    case 2: // prepared
                        // Add to total
                        break;
                    case 3: // committed
                        stats.committed_transactions += count;
                        break;
                    case 4: // rolled back
                        stats.rolled_back_transactions += count;
                        break;
                }
            }
        }
        
        // Get read-only vs read-write transactions
        query = "SELECT MON$READ_ONLY, COUNT(*) FROM MON$TRANSACTIONS GROUP BY MON$READ_ONLY";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                bool read_only = (row[0] == "1");
                uint64_t count = std::stoull(row[1]);
                
                if (read_only) {
                    stats.read_only_transactions += count;
                } else {
                    stats.read_write_transactions += count;
                }
            }
        }
        
        // Get isolation level statistics
        query = "SELECT MON$ISOLATION_MODE, COUNT(*) FROM MON$TRANSACTIONS GROUP BY MON$ISOLATION_MODE";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                int isolation_mode = std::stoi(row[0]);
                uint64_t count = std::stoull(row[1]);
                
                switch (isolation_mode) {
                    case 0:
                        stats.isolation_levels["READ_COMMITTED"] = count;
                        break;
                    case 1:
                        stats.isolation_levels["SNAPSHOT"] = count;
                        break;
                    case 2:
                        stats.isolation_levels["SNAPSHOT_TABLE_STABILITY"] = count;
                        break;
                }
            }
        }
        
        // Get oldest active transactions
        query = "SELECT MON$TRANSACTION_ID, MON$TIMESTAMP FROM MON$TRANSACTIONS "
                "WHERE MON$STATE = 1 ORDER BY MON$TIMESTAMP LIMIT 10";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.oldest_active_transactions.push_back(row[0] + " (started: " + row[1] + ")");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting transaction statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect connection statistics
bool GSTATEnhanced::collectConnectionStatistics(ConnectionStatistics& stats)
{
    try {
        stats.collection_time = std::chrono::system_clock::now();
        
        // Get connection information from monitoring tables
        std::string query = "SELECT MON$STATE, COUNT(*) FROM MON$ATTACHMENTS GROUP BY MON$STATE";
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                int state = std::stoi(row[0]);
                uint64_t count = std::stoull(row[1]);
                
                stats.total_connections += count;
                
                switch (state) {
                    case 0: // idle
                        stats.idle_connections += count;
                        break;
                    case 1: // active
                        stats.active_connections += count;
                        break;
                }
            }
        }
        
        // Get connections by user
        query = "SELECT MON$USER, COUNT(*) FROM MON$ATTACHMENTS GROUP BY MON$USER";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.connections_by_user[row[0]] = std::stoull(row[1]);
            }
        }
        
        // Get connections by application
        query = "SELECT MON$REMOTE_PROCESS, COUNT(*) FROM MON$ATTACHMENTS "
                "WHERE MON$REMOTE_PROCESS IS NOT NULL GROUP BY MON$REMOTE_PROCESS";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.connections_by_application[row[0]] = std::stoull(row[1]);
            }
        }
        
        // Get connections by protocol
        query = "SELECT MON$REMOTE_PROTOCOL, COUNT(*) FROM MON$ATTACHMENTS "
                "WHERE MON$REMOTE_PROTOCOL IS NOT NULL GROUP BY MON$REMOTE_PROTOCOL";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.connections_by_protocol[row[0]] = std::stoull(row[1]);
            }
        }
        
        // Get long-running connections
        query = "SELECT MON$ATTACHMENT_ID, MON$USER, MON$TIMESTAMP FROM MON$ATTACHMENTS "
                "WHERE MON$TIMESTAMP < DATEADD(-1 HOUR TO CURRENT_TIMESTAMP) ORDER BY MON$TIMESTAMP";
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                stats.long_running_connections.push_back(
                    row[0] + " (" + row[1] + ", started: " + row[2] + ")");
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting connection statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect storage statistics
bool GSTATEnhanced::collectStorageStatistics(StorageStatistics& stats)
{
    try {
        stats.collection_time = std::chrono::system_clock::now();
        
        // Get database file information
        std::string query = "SELECT MON$DATABASE_NAME, MON$PAGE_SIZE, MON$PAGES FROM MON$DATABASE";
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                uint64_t page_size = std::stoull(results.rows[0][1]);
                uint64_t pages = std::stoull(results.rows[0][2]);
                stats.total_database_size = page_size * pages;
                
                // Get file size information
                struct stat file_stat;
                if (stat(current_database.c_str(), &file_stat) == 0) {
                    stats.file_sizes[current_database] = file_stat.st_size;
                }
            }
        }
        
        // Estimate data vs index size
        // This is a rough estimation based on table and index statistics
        uint64_t estimated_data_size = 0;
        uint64_t estimated_index_size = 0;
        
        // Get table data estimates
        for (const auto& table_pair : table_stats) {
            for (const auto& table : table_pair.second) {
                estimated_data_size += table.record_count * table.average_record_size;
            }
        }
        
        // Get index estimates (rough calculation)
        for (const auto& index_pair : index_stats) {
            for (const auto& index : index_pair.second) {
                // Estimate index size based on columns and selectivity
                estimated_index_size += index.key_count * index.columns.size() * 20; // rough estimate
            }
        }
        
        stats.data_size = estimated_data_size;
        stats.index_size = estimated_index_size;
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting storage statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect cache statistics
bool GSTATEnhanced::collectCacheStatistics(CacheStatistics& stats)
{
    try {
        stats.collection_time = std::chrono::system_clock::now();
        
        // Get cache information from monitoring tables
        std::string query = "SELECT MON$PAGE_BUFFERS, MON$PAGE_SIZE FROM MON$DATABASE";
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                uint64_t page_buffers = std::stoull(results.rows[0][0]);
                uint64_t page_size = std::stoull(results.rows[0][1]);
                stats.cache_size_bytes = page_buffers * page_size;
            }
        }
        
        // Get I/O statistics
        query = "SELECT MON$PAGE_READS, MON$PAGE_WRITES, MON$PAGE_FETCHES, MON$PAGE_MARKS FROM MON$IO_STATS";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.page_buffer_reads = std::stoull(results.rows[0][0]);
                stats.page_buffer_writes = std::stoull(results.rows[0][1]);
                stats.cache_reads = std::stoull(results.rows[0][2]);
                stats.cache_writes = std::stoull(results.rows[0][3]);
                
                // Calculate cache hit ratio
                if (stats.cache_reads > 0) {
                    stats.cache_hit_ratio = ((stats.cache_reads - stats.page_buffer_reads) * 100) / stats.cache_reads;
                }
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting cache statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect lock statistics
bool GSTATEnhanced::collectLockStatistics(LockStatistics& stats)
{
    try {
        stats.collection_time = std::chrono::system_clock::now();
        
        // Get lock information from monitoring tables
        std::string query = "SELECT COUNT(*) FROM MON$RECORD_STATS WHERE MON$RECORD_LOCKS > 0";
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.total_locks = std::stoull(results.rows[0][0]);
            }
        }
        
        // Get blocked transactions
        query = "SELECT COUNT(*) FROM MON$TRANSACTIONS WHERE MON$STATE = 1 AND MON$TIMESTAMP < DATEADD(-10 SECOND TO CURRENT_TIMESTAMP)";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                stats.lock_waits = std::stoull(results.rows[0][0]);
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting lock statistics: " + std::string(e.what()));
        return false;
    }
}

// Collect performance metrics
bool GSTATEnhanced::collectPerformanceMetrics(PerformanceMetrics& metrics)
{
    try {
        metrics.timestamp = std::chrono::system_clock::now();
        
        // Get active connections
        std::string query = "SELECT COUNT(*) FROM MON$ATTACHMENTS WHERE MON$STATE = 1";
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                metrics.active_connections = std::stoull(results.rows[0][0]);
            }
        }
        
        // Get total connections
        query = "SELECT COUNT(*) FROM MON$ATTACHMENTS";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                metrics.total_connections = std::stoull(results.rows[0][0]);
            }
        }
        
        // Get I/O statistics
        query = "SELECT SUM(MON$PAGE_READS), SUM(MON$PAGE_WRITES), SUM(MON$PAGE_FETCHES), SUM(MON$PAGE_MARKS) FROM MON$IO_STATS";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                metrics.page_reads = std::stoull(results.rows[0][0]);
                metrics.page_writes = std::stoull(results.rows[0][1]);
                metrics.page_fetches = std::stoull(results.rows[0][2]);
                metrics.page_marks = std::stoull(results.rows[0][3]);
                
                // Calculate cache hit ratio
                if (metrics.page_fetches > 0) {
                    metrics.cache_hit_ratio = ((metrics.page_fetches - metrics.page_reads) * 100) / metrics.page_fetches;
                }
            }
        }
        
        // Get memory usage information
        query = "SELECT MON$PAGE_BUFFERS, MON$PAGE_SIZE FROM MON$DATABASE";
        if (engine->executeQuery(query, results)) {
            if (!results.rows.empty()) {
                uint64_t page_buffers = std::stoull(results.rows[0][0]);
                uint64_t page_size = std::stoull(results.rows[0][1]);
                metrics.memory_usage_bytes = page_buffers * page_size;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error collecting performance metrics: " + std::string(e.what()));
        return false;
    }
}

// Load default configuration
bool GSTATEnhanced::loadDefaultConfiguration()
{
    try {
        // Set default monitoring configuration
        monitoring_config.collection_interval = std::chrono::seconds(60);
        monitoring_config.retention_period = std::chrono::seconds(86400 * 30); // 30 days
        monitoring_config.storage_path = "./gstat_data";
        monitoring_config.enable_real_time = false;
        monitoring_config.enable_alerts = false;
        monitoring_config.enable_web_interface = false;
        monitoring_config.web_port = 8080;
        monitoring_config.web_bind_address = "127.0.0.1";
        
        // Enable all categories by default
        monitoring_config.enabled_categories = {
            StatCategory::DATABASE_OVERVIEW,
            StatCategory::TABLE_STATISTICS,
            StatCategory::INDEX_STATISTICS,
            StatCategory::TRANSACTION_STATISTICS,
            StatCategory::CONNECTION_STATISTICS,
            StatCategory::PERFORMANCE_COUNTERS,
            StatCategory::STORAGE_STATISTICS,
            StatCategory::CACHE_STATISTICS,
            StatCategory::LOCK_STATISTICS
        };
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error loading default configuration: " + std::string(e.what()));
        return false;
    }
}

// Initialize analyzers
void GSTATEnhanced::initializeAnalyzers()
{
    // Basic analysis
    analyzers[AnalysisType::BASIC] = [this](const StatisticsOptions& options) -> AnalysisResult {
        AnalysisResult result;
        result.type = AnalysisType::BASIC;
        result.analysis_time = std::chrono::system_clock::now();
        
        if (database_stats.find(current_database) != database_stats.end()) {
            const auto& db_stats = database_stats[current_database];
            
            result.summary = "Basic database analysis for " + db_stats.database_name;
            result.findings.push_back("Database size: " + formatBytes(db_stats.database_size_bytes));
            result.findings.push_back("Page size: " + formatBytes(db_stats.page_size));
            result.findings.push_back("Total pages: " + std::to_string(db_stats.pages_allocated));
            result.findings.push_back("Tables: " + std::to_string(db_stats.table_count));
            result.findings.push_back("Indexes: " + std::to_string(db_stats.index_count));
            result.findings.push_back("Active connections: " + std::to_string(db_stats.performance.active_connections));
            
            result.health_status = determineHealthStatus(db_stats.performance);
            result.confidence_score = 0.8;
            result.success = true;
        }
        
        return result;
    };
    
    // Performance analysis
    analyzers[AnalysisType::PERFORMANCE] = [this](const StatisticsOptions& options) -> AnalysisResult {
        return analyzePerformance(options);
    };
    
    // Health check analysis
    analyzers[AnalysisType::HEALTH_CHECK] = [this](const StatisticsOptions& options) -> AnalysisResult {
        return analyzeHealth(options);
    };
}

// Initialize default alerts
void GSTATEnhanced::initializeDefaultAlerts()
{
    // High CPU usage alert
    AlertConfig cpu_alert;
    cpu_alert.alert_name = "high_cpu_usage";
    cpu_alert.metric_name = "cpu_usage_percent";
    cpu_alert.condition = "greater_than";
    cpu_alert.threshold_value = 80.0;
    cpu_alert.message_template = "High CPU usage detected: {value}%";
    cpu_alert.enabled = true;
    alert_configs.push_back(cpu_alert);
    
    // Low cache hit ratio alert
    AlertConfig cache_alert;
    cache_alert.alert_name = "low_cache_hit_ratio";
    cache_alert.metric_name = "cache_hit_ratio";
    cache_alert.condition = "less_than";
    cache_alert.threshold_value = 90.0;
    cache_alert.message_template = "Low cache hit ratio: {value}%";
    cache_alert.enabled = true;
    alert_configs.push_back(cache_alert);
    
    // High deadlock rate alert
    AlertConfig deadlock_alert;
    deadlock_alert.alert_name = "high_deadlock_rate";
    deadlock_alert.metric_name = "deadlocks";
    deadlock_alert.condition = "greater_than";
    deadlock_alert.threshold_value = 10.0;
    deadlock_alert.message_template = "High deadlock rate detected: {value} deadlocks";
    deadlock_alert.enabled = true;
    alert_configs.push_back(deadlock_alert);
}

// Initialize default reports
void GSTATEnhanced::initializeDefaultReports()
{
    // Daily summary report
    ReportConfig daily_report;
    daily_report.report_name = "daily_summary";
    daily_report.format = StatOutputFormat::HTML;
    daily_report.output_path = "./reports/daily_summary.html";
    daily_report.generation_interval = std::chrono::seconds(86400); // 24 hours
    daily_report.included_categories = {
        StatCategory::DATABASE_OVERVIEW,
        StatCategory::PERFORMANCE_COUNTERS,
        StatCategory::STORAGE_STATISTICS
    };
    daily_report.auto_generate = true;
    report_configs.push_back(daily_report);
    
    // Weekly detailed report
    ReportConfig weekly_report;
    weekly_report.report_name = "weekly_detailed";
    weekly_report.format = StatOutputFormat::HTML;
    weekly_report.output_path = "./reports/weekly_detailed.html";
    weekly_report.generation_interval = std::chrono::seconds(86400 * 7); // 7 days
    weekly_report.included_categories = {
        StatCategory::DATABASE_OVERVIEW,
        StatCategory::TABLE_STATISTICS,
        StatCategory::INDEX_STATISTICS,
        StatCategory::TRANSACTION_STATISTICS,
        StatCategory::CONNECTION_STATISTICS,
        StatCategory::PERFORMANCE_COUNTERS,
        StatCategory::STORAGE_STATISTICS,
        StatCategory::CACHE_STATISTICS,
        StatCategory::LOCK_STATISTICS
    };
    weekly_report.auto_generate = true;
    report_configs.push_back(weekly_report);
}

// Validate database
bool GSTATEnhanced::validateDatabase()
{
    try {
        // Check if we can access basic system tables
        std::string query = "SELECT COUNT(*) FROM RDB$RELATIONS";
        QueryResults results;
        
        if (!engine->executeQuery(query, results)) {
            logError("Cannot access system tables");
            return false;
        }
        
        // Check if monitoring tables are available
        query = "SELECT COUNT(*) FROM MON$DATABASE";
        if (!engine->executeQuery(query, results)) {
            logError("Warning: Monitoring tables not available, some statistics may be limited");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Database validation failed: " + std::string(e.what()));
        return false;
    }
}

// Validate statistics options
bool GSTATEnhanced::validateStatisticsOptions(const StatisticsOptions& options)
{
    // Check output file path if specified
    if (!options.output_file.empty()) {
        std::string dir = options.output_file.substr(0, options.output_file.find_last_of('/'));
        if (!dir.empty() && !createDirectory(dir)) {
            logError("Cannot create output directory: " + dir);
            return false;
        }
    }
    
    // Validate time range format if specified
    if (!options.time_range.empty()) {
        // Simple validation for time range format
        if (options.time_range.find("hour") == std::string::npos &&
            options.time_range.find("day") == std::string::npos &&
            options.time_range.find("week") == std::string::npos &&
            options.time_range.find("month") == std::string::npos) {
            logError("Invalid time range format: " + options.time_range);
            return false;
        }
    }
    
    return true;
}

// Determine health status
HealthStatus GSTATEnhanced::determineHealthStatus(const PerformanceMetrics& metrics)
{
    int health_score = 0;
    int total_checks = 0;
    
    // Check cache hit ratio
    if (metrics.cache_hit_ratio >= 95) health_score += 2;
    else if (metrics.cache_hit_ratio >= 90) health_score += 1;
    else if (metrics.cache_hit_ratio < 80) health_score -= 1;
    total_checks++;
    
    // Check CPU usage
    if (metrics.cpu_usage_percent <= 70) health_score += 2;
    else if (metrics.cpu_usage_percent <= 85) health_score += 1;
    else if (metrics.cpu_usage_percent > 95) health_score -= 1;
    total_checks++;
    
    // Check deadlock rate
    if (metrics.deadlocks == 0) health_score += 1;
    else if (metrics.deadlocks > 10) health_score -= 1;
    total_checks++;
    
    // Check lock waits
    if (metrics.lock_waits == 0) health_score += 1;
    else if (metrics.lock_waits > 50) health_score -= 1;
    total_checks++;
    
    // Calculate overall health
    double health_percentage = (double)health_score / (total_checks * 2) * 100;
    
    if (health_percentage >= 80) return HealthStatus::EXCELLENT;
    else if (health_percentage >= 60) return HealthStatus::GOOD;
    else if (health_percentage >= 40) return HealthStatus::WARNING;
    else return HealthStatus::CRITICAL;
}

// Analyze performance
AnalysisResult GSTATEnhanced::analyzePerformance(const StatisticsOptions& options)
{
    AnalysisResult result;
    result.type = AnalysisType::PERFORMANCE;
    result.analysis_time = std::chrono::system_clock::now();
    
    if (database_stats.find(current_database) != database_stats.end()) {
        const auto& db_stats = database_stats[current_database];
        const auto& perf_metrics = db_stats.performance;
        
        result.summary = "Performance analysis for " + db_stats.database_name;
        
        // Analyze cache performance
        if (perf_metrics.cache_hit_ratio < 90) {
            result.warnings.push_back("Cache hit ratio is low: " + formatPercentage(perf_metrics.cache_hit_ratio));
            result.recommendations.push_back("Consider increasing page buffer size to improve cache hit ratio");
        } else {
            result.findings.push_back("Cache hit ratio is good: " + formatPercentage(perf_metrics.cache_hit_ratio));
        }
        
        // Analyze connection usage
        if (perf_metrics.active_connections > 100) {
            result.warnings.push_back("High number of active connections: " + std::to_string(perf_metrics.active_connections));
            result.recommendations.push_back("Monitor connection pooling and consider reducing concurrent connections");
        }
        
        // Analyze deadlocks
        if (perf_metrics.deadlocks > 0) {
            result.warnings.push_back("Deadlocks detected: " + std::to_string(perf_metrics.deadlocks));
            result.recommendations.push_back("Review transaction logic to minimize deadlock potential");
        }
        
        // Analyze lock waits
        if (perf_metrics.lock_waits > 10) {
            result.warnings.push_back("High lock wait count: " + std::to_string(perf_metrics.lock_waits));
            result.recommendations.push_back("Review queries and transaction duration to reduce lock contention");
        }
        
        result.health_status = determineHealthStatus(perf_metrics);
        result.confidence_score = 0.9;
        result.success = true;
    }
    
    return result;
}

// Analyze health
AnalysisResult GSTATEnhanced::analyzeHealth(const StatisticsOptions& options)
{
    AnalysisResult result;
    result.type = AnalysisType::HEALTH_CHECK;
    result.analysis_time = std::chrono::system_clock::now();
    
    if (database_stats.find(current_database) != database_stats.end()) {
        const auto& db_stats = database_stats[current_database];
        
        result.summary = "Health check for " + db_stats.database_name;
        
        // Check database size growth
        if (db_stats.database_size_bytes > 10ULL * 1024 * 1024 * 1024) { // 10GB
            result.findings.push_back("Large database size: " + formatBytes(db_stats.database_size_bytes));
            result.recommendations.push_back("Consider archiving old data or implementing data retention policies");
        }
        
        // Check table count
        if (db_stats.table_count > 1000) {
            result.findings.push_back("High table count: " + std::to_string(db_stats.table_count));
            result.recommendations.push_back("Consider database normalization or partitioning strategies");
        }
        
        // Check index count
        if (db_stats.index_count > db_stats.table_count * 5) {
            result.warnings.push_back("High index to table ratio: " + std::to_string(db_stats.index_count) + " indexes for " + std::to_string(db_stats.table_count) + " tables");
            result.recommendations.push_back("Review index usage and consider removing unused indexes");
        }
        
        // Check active transactions
        if (db_stats.active_transactions.size() > 100) {
            result.warnings.push_back("High number of active transactions: " + std::to_string(db_stats.active_transactions.size()));
            result.recommendations.push_back("Monitor long-running transactions and consider reducing transaction scope");
        }
        
        result.health_status = determineHealthStatus(db_stats.performance);
        result.confidence_score = 0.85;
        result.success = true;
    }
    
    return result;
}

// Format bytes
std::string GSTATEnhanced::formatBytes(uint64_t bytes)
{
    const std::vector<std::string> units = {"B", "KB", "MB", "GB", "TB"};
    double size = static_cast<double>(bytes);
    size_t unit_index = 0;
    
    while (size >= 1024.0 && unit_index < units.size() - 1) {
        size /= 1024.0;
        unit_index++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return oss.str();
}

// Format duration
std::string GSTATEnhanced::formatDuration(const std::chrono::microseconds& duration)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    
    if (hours.count() > 0) {
        return std::to_string(hours.count()) + "h " + std::to_string(minutes.count() % 60) + "m";
    } else if (minutes.count() > 0) {
        return std::to_string(minutes.count()) + "m " + std::to_string(seconds.count() % 60) + "s";
    } else if (seconds.count() > 0) {
        return std::to_string(seconds.count()) + "s";
    } else {
        return std::to_string(ms.count()) + "ms";
    }
}

// Format percentage
std::string GSTATEnhanced::formatPercentage(double percentage)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << percentage << "%";
    return oss.str();
}

// Create directory
bool GSTATEnhanced::createDirectory(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
    return mkdir(path.c_str(), 0755) == 0;
}

// Log error
void GSTATEnhanced::logError(const std::string& error)
{
    std::lock_guard<std::mutex> lock(error_mutex);
    error_log.push_back(error);
    std::cerr << "ERROR: " << error << std::endl;
}

// Log message
void GSTATEnhanced::logMessage(const std::string& message)
{
    if (log_file && log_file->is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        *log_file << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] " << message << std::endl;
    }
}

// Get last error
std::string GSTATEnhanced::getLastError() const
{
    std::lock_guard<std::mutex> lock(error_mutex);
    return error_log.empty() ? "" : error_log.back();
}

// Get error log
std::vector<std::string> GSTATEnhanced::getErrorLog() const
{
    std::lock_guard<std::mutex> lock(error_mutex);
    return error_log;
}

// Clear error log
void GSTATEnhanced::clearErrorLog()
{
    std::lock_guard<std::mutex> lock(error_mutex);
    error_log.clear();
}

// Analyze database
AnalysisResult GSTATEnhanced::analyzeDatabase(AnalysisType type, const StatisticsOptions& options)
{
    auto analyzer_it = analyzers.find(type);
    if (analyzer_it != analyzers.end()) {
        return analyzer_it->second(options);
    }
    
    AnalysisResult result;
    result.type = type;
    result.analysis_time = std::chrono::system_clock::now();
    result.success = false;
    result.errors.push_back("Analysis type not supported: " + std::to_string(static_cast<int>(type)));
    return result;
}

// Analyze capacity
AnalysisResult GSTATEnhanced::analyzeCapacity(const StatisticsOptions& options)
{
    AnalysisResult result;
    result.type = AnalysisType::CAPACITY_PLANNING;
    result.analysis_time = std::chrono::system_clock::now();
    
    if (database_stats.find(current_database) != database_stats.end()) {
        const auto& db_stats = database_stats[current_database];
        
        result.summary = "Capacity planning analysis for " + db_stats.database_name;
        
        // Analyze current database size
        uint64_t current_size = db_stats.database_size_bytes;
        result.findings.push_back("Current database size: " + formatBytes(current_size));
        
        // Estimate growth based on table statistics
        double estimated_growth_rate = 0.0;
        if (!table_stats.empty() && table_stats.find(current_database) != table_stats.end()) {
            const auto& tables = table_stats[current_database];
            uint64_t total_records = 0;
            uint64_t total_size = 0;
            
            for (const auto& table : tables) {
                total_records += table.record_count;
                total_size += table.record_count * table.average_record_size;
            }
            
            if (total_records > 0) {
                double avg_record_size = static_cast<double>(total_size) / total_records;
                result.findings.push_back("Average record size: " + formatBytes(static_cast<uint64_t>(avg_record_size)));
                
                // Assume 10% growth per month (configurable)
                estimated_growth_rate = 0.10;
                uint64_t projected_size_3m = current_size * (1.0 + estimated_growth_rate * 3);
                uint64_t projected_size_6m = current_size * (1.0 + estimated_growth_rate * 6);
                uint64_t projected_size_12m = current_size * (1.0 + estimated_growth_rate * 12);
                
                result.findings.push_back("Projected size (3 months): " + formatBytes(projected_size_3m));
                result.findings.push_back("Projected size (6 months): " + formatBytes(projected_size_6m));
                result.findings.push_back("Projected size (12 months): " + formatBytes(projected_size_12m));
                
                // Storage recommendations
                if (projected_size_12m > 100ULL * 1024 * 1024 * 1024) { // 100GB
                    result.recommendations.push_back("Consider implementing data archiving strategy");
                    result.recommendations.push_back("Monitor disk space allocation for projected growth");
                }
                
                if (projected_size_12m > current_size * 2) {
                    result.warnings.push_back("Database size expected to double within 12 months");
                    result.recommendations.push_back("Plan for storage expansion and backup strategy updates");
                }
            }
        }
        
        // Analyze index overhead
        if (db_stats.index_count > 0) {
            double index_ratio = static_cast<double>(db_stats.index_count) / db_stats.table_count;
            result.findings.push_back("Index to table ratio: " + std::to_string(index_ratio));
            
            if (index_ratio > 10) {
                result.warnings.push_back("High index overhead detected");
                result.recommendations.push_back("Review index usage and remove unused indexes");
            }
        }
        
        result.health_status = HealthStatus::GOOD;
        result.confidence_score = 0.75;
        result.success = true;
    }
    
    return result;
}

// Analyze trends
AnalysisResult GSTATEnhanced::analyzeTrends(const StatisticsOptions& options)
{
    AnalysisResult result;
    result.type = AnalysisType::TREND_ANALYSIS;
    result.analysis_time = std::chrono::system_clock::now();
    
    result.summary = "Trend analysis for " + current_database;
    
    // Analyze historical data if available
    if (!historical_data.empty()) {
        // Group data by metric
        std::map<std::string, std::vector<HistoricalDataPoint>> metrics_data;
        
        for (const auto& data_point : historical_data) {
            for (const auto& metric : data_point.metrics) {
                metrics_data[metric.first].push_back(data_point);
            }
        }
        
        // Analyze each metric trend
        for (const auto& metric_pair : metrics_data) {
            const std::string& metric_name = metric_pair.first;
            const auto& data_points = metric_pair.second;
            
            if (data_points.size() >= 2) {
                TrendAnalysis trend = analyzeTrend(metric_name, options.time_range);
                
                if (trend.trend_direction == "increasing") {
                    if (metric_name.find("error") != std::string::npos || 
                        metric_name.find("deadlock") != std::string::npos) {
                        result.warnings.push_back("Increasing trend in " + metric_name);
                    } else {
                        result.findings.push_back("Increasing trend in " + metric_name);
                    }
                } else if (trend.trend_direction == "decreasing") {
                    if (metric_name.find("performance") != std::string::npos ||
                        metric_name.find("cache_hit") != std::string::npos) {
                        result.warnings.push_back("Decreasing trend in " + metric_name);
                    } else {
                        result.findings.push_back("Decreasing trend in " + metric_name);
                    }
                }
                
                result.metrics[metric_name + "_trend_slope"] = trend.trend_slope;
                result.metrics[metric_name + "_correlation"] = trend.correlation_coefficient;
            }
        }
        
        result.confidence_score = 0.8;
        result.success = true;
    } else {
        result.warnings.push_back("No historical data available for trend analysis");
        result.recommendations.push_back("Enable historical data collection for trend analysis");
        result.confidence_score = 0.0;
        result.success = false;
    }
    
    return result;
}

// Analyze trend for specific metric
TrendAnalysis GSTATEnhanced::analyzeTrend(const std::string& metric_name, const std::string& time_range)
{
    TrendAnalysis trend;
    trend.metric_name = metric_name;
    trend.analysis_time = std::chrono::system_clock::now();
    
    // Get historical data for the metric
    trend.data_points = getHistoricalData(metric_name, time_range);
    
    if (trend.data_points.size() >= 2) {
        // Calculate trend slope using linear regression
        std::vector<double> x_values, y_values;
        
        auto start_time = trend.data_points.front().timestamp;
        for (const auto& point : trend.data_points) {
            auto duration = std::chrono::duration_cast<std::chrono::hours>(point.timestamp - start_time);
            x_values.push_back(static_cast<double>(duration.count()));
            
            auto metric_it = point.metrics.find(metric_name);
            if (metric_it != point.metrics.end()) {
                y_values.push_back(metric_it->second);
            }
        }
        
        if (x_values.size() == y_values.size() && x_values.size() >= 2) {
            // Calculate linear regression
            double n = static_cast<double>(x_values.size());
            double sum_x = std::accumulate(x_values.begin(), x_values.end(), 0.0);
            double sum_y = std::accumulate(y_values.begin(), y_values.end(), 0.0);
            double sum_xy = 0.0, sum_xx = 0.0;
            
            for (size_t i = 0; i < x_values.size(); i++) {
                sum_xy += x_values[i] * y_values[i];
                sum_xx += x_values[i] * x_values[i];
            }
            
            trend.trend_slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
            
            // Calculate correlation coefficient
            double sum_yy = 0.0;
            for (double y : y_values) {
                sum_yy += y * y;
            }
            
            double numerator = n * sum_xy - sum_x * sum_y;
            double denominator = std::sqrt((n * sum_xx - sum_x * sum_x) * (n * sum_yy - sum_y * sum_y));
            
            if (denominator != 0.0) {
                trend.correlation_coefficient = numerator / denominator;
            }
            
            // Determine trend direction
            if (std::abs(trend.trend_slope) < 0.001) {
                trend.trend_direction = "stable";
            } else if (trend.trend_slope > 0) {
                trend.trend_direction = "increasing";
            } else {
                trend.trend_direction = "decreasing";
            }
            
            // Calculate confidence level
            trend.confidence_level = std::abs(trend.correlation_coefficient);
            
            // Generate prediction
            if (trend.confidence_level > 0.7) {
                double future_hours = 24.0; // Predict 24 hours ahead
                double predicted_value = y_values.back() + trend.trend_slope * future_hours;
                trend.prediction = "Predicted value in 24 hours: " + std::to_string(predicted_value);
            } else {
                trend.prediction = "Insufficient data for reliable prediction";
            }
        }
    }
    
    return trend;
}

// Generate recommendations
std::vector<std::string> GSTATEnhanced::generateRecommendations(const DatabaseStatistics& stats)
{
    std::vector<std::string> recommendations;
    
    // Database size recommendations
    if (stats.database_size_bytes > 50ULL * 1024 * 1024 * 1024) { // 50GB
        recommendations.push_back("Large database detected. Consider implementing data archiving strategies.");
    }
    
    // Table count recommendations
    if (stats.table_count > 500) {
        recommendations.push_back("High table count. Consider database normalization or schema organization.");
    }
    
    // Index recommendations
    if (stats.index_count > stats.table_count * 8) {
        recommendations.push_back("High index-to-table ratio. Review index usage and remove unused indexes.");
    }
    
    // Performance recommendations
    if (stats.performance.cache_hit_ratio < 95) {
        recommendations.push_back("Cache hit ratio below optimal. Consider increasing page buffer size.");
    }
    
    if (stats.performance.active_connections > 200) {
        recommendations.push_back("High connection count. Implement connection pooling and review application architecture.");
    }
    
    if (stats.performance.deadlocks > 0) {
        recommendations.push_back("Deadlocks detected. Review transaction design and locking strategies.");
    }
    
    // Connection recommendations
    if (stats.attached_users.size() > 50) {
        recommendations.push_back("Many concurrent users. Monitor resource usage and consider load balancing.");
    }
    
    return recommendations;
}

// Identify performance issues
std::vector<std::string> GSTATEnhanced::identifyPerformanceIssues(const PerformanceMetrics& metrics)
{
    std::vector<std::string> issues;
    
    // CPU usage issues
    if (metrics.cpu_usage_percent > 90) {
        issues.push_back("Critical: CPU usage is very high (" + formatPercentage(metrics.cpu_usage_percent) + ")");
    } else if (metrics.cpu_usage_percent > 80) {
        issues.push_back("Warning: CPU usage is high (" + formatPercentage(metrics.cpu_usage_percent) + ")");
    }
    
    // Memory usage issues
    if (metrics.memory_usage_bytes > 8ULL * 1024 * 1024 * 1024) { // 8GB
        issues.push_back("High memory usage: " + formatBytes(metrics.memory_usage_bytes));
    }
    
    // Cache performance issues
    if (metrics.cache_hit_ratio < 85) {
        issues.push_back("Critical: Low cache hit ratio (" + formatPercentage(metrics.cache_hit_ratio) + ")");
    } else if (metrics.cache_hit_ratio < 95) {
        issues.push_back("Warning: Suboptimal cache hit ratio (" + formatPercentage(metrics.cache_hit_ratio) + ")");
    }
    
    // I/O issues
    if (metrics.disk_io_reads > 10000 || metrics.disk_io_writes > 10000) {
        issues.push_back("High disk I/O detected (reads: " + std::to_string(metrics.disk_io_reads) + 
                        ", writes: " + std::to_string(metrics.disk_io_writes) + ")");
    }
    
    // Lock contention issues
    if (metrics.deadlocks > 0) {
        issues.push_back("Deadlocks detected: " + std::to_string(metrics.deadlocks));
    }
    
    if (metrics.lock_waits > 100) {
        issues.push_back("High lock wait count: " + std::to_string(metrics.lock_waits));
    }
    
    // Connection issues
    if (metrics.active_connections > 500) {
        issues.push_back("Very high connection count: " + std::to_string(metrics.active_connections));
    }
    
    // Transaction rate issues
    if (metrics.transactions_per_second > 1000) {
        issues.push_back("High transaction rate: " + std::to_string(metrics.transactions_per_second) + " TPS");
    }
    
    return issues;
}

// Start monitoring
bool GSTATEnhanced::startMonitoring(const MonitoringConfig& config)
{
    if (monitoring_active) {
        logError("Monitoring is already active");
        return false;
    }
    
    monitoring_config = config;
    monitoring_active = true;
    
    // Start monitoring thread
    monitoring_thread = std::thread(&GSTATEnhanced::monitoringLoop, this);
    
    logMessage("Monitoring started with " + std::to_string(config.collection_interval.count()) + "s interval");
    return true;
}

// Stop monitoring
bool GSTATEnhanced::stopMonitoring()
{
    if (!monitoring_active) {
        return true;
    }
    
    monitoring_active = false;
    
    // Notify monitoring thread to stop
    data_available.notify_all();
    
    // Wait for monitoring thread to finish
    if (monitoring_thread.joinable()) {
        monitoring_thread.join();
    }
    
    logMessage("Monitoring stopped");
    return true;
}

// Check if monitoring is active
bool GSTATEnhanced::isMonitoring() const
{
    return monitoring_active;
}

// Monitoring loop
void GSTATEnhanced::monitoringLoop()
{
    while (monitoring_active) {
        try {
            // Collect performance metrics
            PerformanceMetrics metrics;
            if (collectPerformanceMetrics(metrics)) {
                // Store metrics in queue for processing
                {
                    std::lock_guard<std::mutex> lock(data_mutex);
                    metrics_queue.push(metrics);
                }
                data_available.notify_one();
                
                // Store historical data
                HistoricalDataPoint data_point;
                data_point.timestamp = metrics.timestamp;
                data_point.category = "performance";
                data_point.metrics["cpu_usage_percent"] = metrics.cpu_usage_percent;
                data_point.metrics["memory_usage_bytes"] = static_cast<double>(metrics.memory_usage_bytes);
                data_point.metrics["cache_hit_ratio"] = static_cast<double>(metrics.cache_hit_ratio);
                data_point.metrics["active_connections"] = static_cast<double>(metrics.active_connections);
                data_point.metrics["deadlocks"] = static_cast<double>(metrics.deadlocks);
                data_point.metrics["lock_waits"] = static_cast<double>(metrics.lock_waits);
                
                storeHistoricalData(data_point);
                
                // Check alerts
                if (monitoring_config.enable_alerts) {
                    checkAlerts();
                }
            }
            
            // Process metrics queue
            processMetricsQueue();
            
            // Sleep until next collection
            std::this_thread::sleep_for(monitoring_config.collection_interval);
            
        } catch (const std::exception& e) {
            logError("Error in monitoring loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10)); // Wait before retrying
        }
    }
}

// Process metrics queue
void GSTATEnhanced::processMetricsQueue()
{
    std::lock_guard<std::mutex> lock(data_mutex);
    
    while (!metrics_queue.empty()) {
        PerformanceMetrics metrics = metrics_queue.front();
        metrics_queue.pop();
        
        // Update database statistics with latest performance metrics
        if (database_stats.find(current_database) != database_stats.end()) {
            database_stats[current_database].performance = metrics;
        }
        
        // Identify performance issues
        auto issues = identifyPerformanceIssues(metrics);
        for (const auto& issue : issues) {
            logMessage("Performance issue detected: " + issue);
        }
    }
}

// Store historical data
bool GSTATEnhanced::storeHistoricalData(const HistoricalDataPoint& data_point)
{
    try {
        std::lock_guard<std::mutex> lock(data_mutex);
        historical_data.push_back(data_point);
        
        // Limit historical data size to prevent memory issues
        const size_t max_historical_points = 10000;
        if (historical_data.size() > max_historical_points) {
            historical_data.erase(historical_data.begin(), 
                                historical_data.begin() + (historical_data.size() - max_historical_points));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error storing historical data: " + std::string(e.what()));
        return false;
    }
}

// Get historical data
std::vector<HistoricalDataPoint> GSTATEnhanced::getHistoricalData(const std::string& metric_name, 
                                                                 const std::string& time_range)
{
    std::vector<HistoricalDataPoint> filtered_data;
    
    try {
        std::lock_guard<std::mutex> lock(data_mutex);
        
        // Parse time range
        auto cutoff_time = std::chrono::system_clock::now();
        if (time_range.find("hour") != std::string::npos) {
            int hours = std::stoi(time_range.substr(0, time_range.find("hour")));
            cutoff_time -= std::chrono::hours(hours);
        } else if (time_range.find("day") != std::string::npos) {
            int days = std::stoi(time_range.substr(0, time_range.find("day")));
            cutoff_time -= std::chrono::hours(days * 24);
        } else if (time_range.find("week") != std::string::npos) {
            int weeks = std::stoi(time_range.substr(0, time_range.find("week")));
            cutoff_time -= std::chrono::hours(weeks * 24 * 7);
        }
        
        // Filter data by time range and metric
        for (const auto& data_point : historical_data) {
            if (data_point.timestamp >= cutoff_time) {
                if (metric_name.empty() || data_point.metrics.find(metric_name) != data_point.metrics.end()) {
                    filtered_data.push_back(data_point);
                }
            }
        }
        
    } catch (const std::exception& e) {
        logError("Error retrieving historical data: " + std::string(e.what()));
    }
    
    return filtered_data;
}

// Check alerts
bool GSTATEnhanced::checkAlerts()
{
    try {
        if (database_stats.find(current_database) == database_stats.end()) {
            return false;
        }
        
        const auto& metrics = database_stats[current_database].performance;
        auto current_time = std::chrono::system_clock::now();
        
        for (auto& alert : alert_configs) {
            if (!alert.enabled) {
                continue;
            }
            
            // Check cooldown period
            if (current_time - alert.last_fired < alert.cooldown_period) {
                continue;
            }
            
            double current_value = 0.0;
            
            // Get current value for the metric
            if (alert.metric_name == "cpu_usage_percent") {
                current_value = metrics.cpu_usage_percent;
            } else if (alert.metric_name == "memory_usage_bytes") {
                current_value = static_cast<double>(metrics.memory_usage_bytes);
            } else if (alert.metric_name == "cache_hit_ratio") {
                current_value = static_cast<double>(metrics.cache_hit_ratio);
            } else if (alert.metric_name == "active_connections") {
                current_value = static_cast<double>(metrics.active_connections);
            } else if (alert.metric_name == "deadlocks") {
                current_value = static_cast<double>(metrics.deadlocks);
            } else if (alert.metric_name == "lock_waits") {
                current_value = static_cast<double>(metrics.lock_waits);
            }
            
            // Evaluate alert condition
            if (evaluateAlertCondition(alert, current_value)) {
                // Format alert message
                std::string message = alert.message_template;
                size_t pos = message.find("{value}");
                if (pos != std::string::npos) {
                    message.replace(pos, 7, std::to_string(current_value));
                }
                
                sendAlert(alert, message);
                alert.last_fired = current_time;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logError("Error checking alerts: " + std::string(e.what()));
        return false;
    }
}

// Evaluate alert condition
bool GSTATEnhanced::evaluateAlertCondition(const AlertConfig& alert, double current_value)
{
    if (alert.condition == "greater_than") {
        return current_value > alert.threshold_value;
    } else if (alert.condition == "less_than") {
        return current_value < alert.threshold_value;
    } else if (alert.condition == "equals") {
        return std::abs(current_value - alert.threshold_value) < 0.001;
    } else if (alert.condition == "not_equals") {
        return std::abs(current_value - alert.threshold_value) >= 0.001;
    }
    
    return false;
}

// Send alert
void GSTATEnhanced::sendAlert(const AlertConfig& alert, const std::string& message)
{
    logMessage("ALERT [" + alert.alert_name + "]: " + message);
    
    // In a full implementation, this would send emails, SMS, webhook notifications, etc.
    // For now, we just log the alert
    std::cerr << "ALERT: " << message << std::endl;
}

// Add alert
bool GSTATEnhanced::addAlert(const AlertConfig& alert)
{
    try {
        alert_configs.push_back(alert);
        logMessage("Alert added: " + alert.alert_name);
        return true;
        
    } catch (const std::exception& e) {
        logError("Error adding alert: " + std::string(e.what()));
        return false;
    }
}

// Remove alert
bool GSTATEnhanced::removeAlert(const std::string& alert_name)
{
    try {
        auto it = std::remove_if(alert_configs.begin(), alert_configs.end(),
                                [&alert_name](const AlertConfig& alert) {
                                    return alert.alert_name == alert_name;
                                });
        
        if (it != alert_configs.end()) {
            alert_configs.erase(it, alert_configs.end());
            logMessage("Alert removed: " + alert_name);
            return true;
        }
        
        logError("Alert not found: " + alert_name);
        return false;
        
    } catch (const std::exception& e) {
        logError("Error removing alert: " + std::string(e.what()));
        return false;
    }
}

// Format timestamp
std::string GSTATEnhanced::formatTimestamp(const std::chrono::system_clock::time_point& time)
{
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// Get performance metrics
uint64_t GSTATEnhanced::getTotalCollections() const
{
    return total_collections;
}

uint64_t GSTATEnhanced::getSuccessfulCollections() const
{
    return successful_collections;
}

uint64_t GSTATEnhanced::getFailedCollections() const
{
    return failed_collections;
}

std::chrono::microseconds GSTATEnhanced::getAverageCollectionTime() const
{
    // This would need to be tracked during collections
    // For now, return a placeholder
    return std::chrono::microseconds(1000);
}

// Web interface management methods

// Start web interface
bool GSTATEnhanced::startWebInterface(const WebServerConfig& config)
{
    try {
        if (!web_interface) {
            logError("Web interface not initialized");
            return false;
        }
        
        if (web_interface->isRunning()) {
            logMessage("Web interface is already running");
            return true;
        }
        
        if (!web_interface->initialize(config)) {
            logError("Failed to initialize web interface");
            return false;
        }
        
        if (!web_interface->start()) {
            logError("Failed to start web interface");
            return false;
        }
        
        logMessage("Web interface started successfully on " + config.bind_address + ":" + std::to_string(config.port));
        return true;
        
    } catch (const std::exception& e) {
        logError("Error starting web interface: " + std::string(e.what()));
        return false;
    }
}

// Stop web interface
bool GSTATEnhanced::stopWebInterface()
{
    try {
        if (!web_interface) {
            return true;
        }
        
        if (!web_interface->isRunning()) {
            logMessage("Web interface is not running");
            return true;
        }
        
        if (!web_interface->stop()) {
            logError("Failed to stop web interface");
            return false;
        }
        
        logMessage("Web interface stopped successfully");
        return true;
        
    } catch (const std::exception& e) {
        logError("Error stopping web interface: " + std::string(e.what()));
        return false;
    }
}

// Check if web interface is running
bool GSTATEnhanced::isWebInterfaceRunning() const
{
    return web_interface && web_interface->isRunning();
}

// Restart web interface
bool GSTATEnhanced::restartWebInterface()
{
    try {
        if (!web_interface) {
            logError("Web interface not initialized");
            return false;
        }
        
        return web_interface->restart();
        
    } catch (const std::exception& e) {
        logError("Error restarting web interface: " + std::string(e.what()));
        return false;
    }
}

// Get web interface URL
std::string GSTATEnhanced::getWebInterfaceUrl() const
{
    if (!web_interface || !web_interface->isRunning()) {
        return "";
    }
    
    try {
        auto config = web_interface->getConfig();
        std::string protocol = config.enable_ssl ? "https" : "http";
        return protocol + "://" + config.bind_address + ":" + std::to_string(config.port);
        
    } catch (const std::exception& e) {
        return "";
    }
}

// Get web interface requests count
uint64_t GSTATEnhanced::getWebInterfaceRequests() const
{
    if (!web_interface) {
        return 0;
    }
    
    try {
        return web_interface->getTotalRequests();
    } catch (const std::exception& e) {
        return 0;
    }
}

// Get web interface status
std::string GSTATEnhanced::getWebInterfaceStatus() const
{
    if (!web_interface) {
        return "Not Initialized";
    }
    
    try {
        if (web_interface->isRunning()) {
            auto config = web_interface->getConfig();
            std::ostringstream status;
            status << "Running on " << config.bind_address << ":" << config.port;
            status << " (Requests: " << web_interface->getTotalRequests();
            status << ", Success: " << web_interface->getSuccessfulRequests();
            status << ", Failed: " << web_interface->getFailedRequests();
            status << ", Active Connections: " << web_interface->getActiveConnections() << ")";
            return status.str();
        } else {
            return "Stopped";
        }
        
    } catch (const std::exception& e) {
        return "Error: " + std::string(e.what());
    }
}