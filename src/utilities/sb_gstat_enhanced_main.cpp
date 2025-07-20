#include "sb_gstat_enhanced.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <getopt.h>
#include <signal.h>
#include <cstdlib>
#include <thread>
#include <chrono>

using namespace SBEnhanced;

// Global instance for signal handling
static GSTATEnhanced* g_gstat_instance = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    if (g_gstat_instance) {
        std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
        g_gstat_instance->stopMonitoring();
        exit(0);
    }
}

// Version information
static const char* VERSION = "sb_gstat version SB-T0.5.0.1 ScratchBird 0.5 f90eae0";

// Show usage information
void showUsage() {
    std::cout << "sb_gstat - ScratchBird Enhanced Database Statistics Utility" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: sb_gstat [options] database" << std::endl;
    std::cout << std::endl;
    std::cout << "Connection Options:" << std::endl;
    std::cout << "  -user <username>     database username (default: SYSDBA)" << std::endl;
    std::cout << "  -password <password> database password" << std::endl;
    std::cout << "  -role <role>         SQL role name" << std::endl;
    std::cout << "  -trusted             use trusted authentication" << std::endl;
    std::cout << std::endl;
    std::cout << "Statistics Options:" << std::endl;
    std::cout << "  -all                 collect all available statistics" << std::endl;
    std::cout << "  -database            database overview statistics" << std::endl;
    std::cout << "  -tables              table statistics" << std::endl;
    std::cout << "  -indexes             index statistics" << std::endl;
    std::cout << "  -transactions        transaction statistics" << std::endl;
    std::cout << "  -connections         connection statistics" << std::endl;
    std::cout << "  -performance         performance metrics" << std::endl;
    std::cout << "  -storage             storage statistics" << std::endl;
    std::cout << "  -cache               cache statistics" << std::endl;
    std::cout << "  -locks               lock statistics" << std::endl;
    std::cout << std::endl;
    std::cout << "Analysis Options:" << std::endl;
    std::cout << "  -analyze <type>      perform analysis (basic|performance|health|capacity|trends)" << std::endl;
    std::cout << "  -recommendations     generate optimization recommendations" << std::endl;
    std::cout << "  -health              perform health check analysis" << std::endl;
    std::cout << std::endl;
    std::cout << "Output Options:" << std::endl;
    std::cout << "  -format <format>     output format (table|csv|json|xml|html|markdown)" << std::endl;
    std::cout << "  -output <file>       write output to file" << std::endl;
    std::cout << "  -verbose             detailed output" << std::endl;
    std::cout << "  -quiet               minimal output" << std::endl;
    std::cout << std::endl;
    std::cout << "Monitoring Options:" << std::endl;
    std::cout << "  -monitor             start continuous monitoring" << std::endl;
    std::cout << "  -interval <seconds>  monitoring collection interval (default: 60)" << std::endl;
    std::cout << "  -duration <seconds>  monitoring duration (0 = infinite)" << std::endl;
    std::cout << "  -alerts              enable alerts during monitoring" << std::endl;
    std::cout << "  -realtime            enable real-time monitoring" << std::endl;
    std::cout << std::endl;
    std::cout << "Historical Options:" << std::endl;
    std::cout << "  -history             include historical data analysis" << std::endl;
    std::cout << "  -timerange <range>   time range for historical data (1hour|1day|1week|1month)" << std::endl;
    std::cout << "  -trends              analyze trends in historical data" << std::endl;
    std::cout << std::endl;
    std::cout << "Report Options:" << std::endl;
    std::cout << "  -report <name>       generate predefined report (daily|weekly|monthly)" << std::endl;
    std::cout << "  -dashboard           generate HTML dashboard" << std::endl;
    std::cout << "  -export              export all statistics" << std::endl;
    std::cout << std::endl;
    std::cout << "Web Interface Options:" << std::endl;
    std::cout << "  -web                 start web interface" << std::endl;
    std::cout << "  -webport <port>      web interface port (default: 8080)" << std::endl;
    std::cout << "  -webaddr <address>   web interface bind address (default: 127.0.0.1)" << std::endl;
    std::cout << "  -webssl              enable SSL/TLS for web interface" << std::endl;
    std::cout << "  -webcors             enable CORS for web interface" << std::endl;
    std::cout << "  -webauth <token>     authentication token for web interface" << std::endl;
    std::cout << std::endl;
    std::cout << "Filter Options:" << std::endl;
    std::cout << "  -schema <pattern>    filter by schema name pattern" << std::endl;
    std::cout << "  -table <pattern>     filter by table name pattern" << std::endl;
    std::cout << "  -limit <count>       limit number of results" << std::endl;
    std::cout << std::endl;
    std::cout << "Other Options:" << std::endl;
    std::cout << "  -config <file>       load configuration from file" << std::endl;
    std::cout << "  -log <file>          write log to file" << std::endl;
    std::cout << "  -z                   show version" << std::endl;
    std::cout << "  -?                   show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  sb_gstat mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -all -format html -output report.html mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -analyze performance -recommendations mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -monitor -interval 30 -alerts mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -tables -schema finance -format csv mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -trends -timerange 1week mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -web -webport 9090 mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -web -monitor -alerts mydb.fdb" << std::endl;
    std::cout << "  sb_gstat -web -webssl -webauth mytoken123 mydb.fdb" << std::endl;
}

// Show version information
void showVersion() {
    std::cout << VERSION << std::endl;
}

// Parse command line arguments
bool parseCommandLine(int argc, char* argv[], StatisticsOptions& options, 
                     MonitoringConfig& monitoring_config, ConnectionOptions& conn_options,
                     std::string& database_path, bool& show_help, bool& show_version_info,
                     bool& start_monitoring, std::string& config_file, std::string& log_file,
                     bool& start_web_interface, SBEnhanced::WebServerConfig& web_config) {
    
    // Set defaults
    conn_options.username = "SYSDBA";
    conn_options.password = "masterkey";
    options.output_format = StatOutputFormat::TABLE;
    options.verbose = false;
    options.include_detailed_info = true;
    options.include_recommendations = false;
    options.max_results = 1000;
    options.sort_results = true;
    options.ascending = true;
    monitoring_config.collection_interval = std::chrono::seconds(60);
    start_web_interface = false;
    
    // Set web interface defaults
    web_config.bind_address = "127.0.0.1";
    web_config.port = 8080;
    web_config.enable_ssl = false;
    web_config.enable_cors = true;
    web_config.enable_websockets = true;
    
    static struct option long_options[] = {
        {"user", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'p'},
        {"role", required_argument, 0, 'r'},
        {"trusted", no_argument, 0, 't'},
        {"all", no_argument, 0, 'A'},
        {"database", no_argument, 0, 'D'},
        {"tables", no_argument, 0, 'T'},
        {"indexes", no_argument, 0, 'I'},
        {"transactions", no_argument, 0, 'X'},
        {"connections", no_argument, 0, 'C'},
        {"performance", no_argument, 0, 'P'},
        {"storage", no_argument, 0, 'S'},
        {"cache", no_argument, 0, 'H'},
        {"locks", no_argument, 0, 'L'},
        {"analyze", required_argument, 0, 'a'},
        {"recommendations", no_argument, 0, 'R'},
        {"health", no_argument, 0, 'h'},
        {"format", required_argument, 0, 'f'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"quiet", no_argument, 0, 'q'},
        {"monitor", no_argument, 0, 'm'},
        {"interval", required_argument, 0, 'i'},
        {"duration", required_argument, 0, 'd'},
        {"alerts", no_argument, 0, 'e'},
        {"realtime", no_argument, 0, 'M'},
        {"history", no_argument, 0, 'Y'},
        {"timerange", required_argument, 0, 'g'},
        {"trends", no_argument, 0, 'n'},
        {"report", required_argument, 0, 'G'},
        {"dashboard", no_argument, 0, 'B'},
        {"export", no_argument, 0, 'E'},
        {"schema", required_argument, 0, 's'},
        {"table", required_argument, 0, 'b'},
        {"limit", required_argument, 0, 'l'},
        {"config", required_argument, 0, 'c'},
        {"log", required_argument, 0, 'L'},
        {"web", no_argument, 0, 'W'},
        {"webport", required_argument, 0, 'P'},
        {"webaddr", required_argument, 0, 'A'},
        {"webssl", no_argument, 0, 'S'},
        {"webcors", no_argument, 0, 'O'},
        {"webauth", required_argument, 0, 'U'},
        {"version", no_argument, 0, 'z'},
        {"help", no_argument, 0, '?'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "u:p:r:tADTIXCPSHLa:Rhf:o:vqmi:d:eMYg:nG:BEs:b:l:c:L:WP:A:SOU:z?", 
                           long_options, &option_index)) != -1) {
        switch (c) {
            case 'u':
                conn_options.username = optarg;
                break;
            case 'p':
                conn_options.password = optarg;
                break;
            case 'r':
                conn_options.role = optarg;
                break;
            case 't':
                conn_options.trusted_auth = true;
                break;
            case 'A':
                options.categories.clear(); // All categories
                break;
            case 'D':
                options.categories.insert(StatCategory::DATABASE_OVERVIEW);
                break;
            case 'T':
                options.categories.insert(StatCategory::TABLE_STATISTICS);
                break;
            case 'I':
                options.categories.insert(StatCategory::INDEX_STATISTICS);
                break;
            case 'X':
                options.categories.insert(StatCategory::TRANSACTION_STATISTICS);
                break;
            case 'C':
                options.categories.insert(StatCategory::CONNECTION_STATISTICS);
                break;
            case 'P':
                options.categories.insert(StatCategory::PERFORMANCE_COUNTERS);
                break;
            case 'S':
                options.categories.insert(StatCategory::STORAGE_STATISTICS);
                break;
            case 'H':
                options.categories.insert(StatCategory::CACHE_STATISTICS);
                break;
            case 'L':
                options.categories.insert(StatCategory::LOCK_STATISTICS);
                break;
            case 'a':
                // Analysis type will be handled in main
                break;
            case 'R':
                options.include_recommendations = true;
                break;
            case 'h':
                // Health check will be handled in main
                break;
            case 'f':
                if (std::string(optarg) == "table") options.output_format = StatOutputFormat::TABLE;
                else if (std::string(optarg) == "csv") options.output_format = StatOutputFormat::CSV;
                else if (std::string(optarg) == "json") options.output_format = StatOutputFormat::JSON;
                else if (std::string(optarg) == "xml") options.output_format = StatOutputFormat::XML;
                else if (std::string(optarg) == "html") options.output_format = StatOutputFormat::HTML;
                else if (std::string(optarg) == "markdown") options.output_format = StatOutputFormat::MARKDOWN;
                else {
                    std::cerr << "Unknown output format: " << optarg << std::endl;
                    return false;
                }
                break;
            case 'o':
                options.output_file = optarg;
                break;
            case 'v':
                options.verbose = true;
                break;
            case 'q':
                options.verbose = false;
                break;
            case 'm':
                start_monitoring = true;
                break;
            case 'i':
                monitoring_config.collection_interval = std::chrono::seconds(std::atoi(optarg));
                break;
            case 'd':
                // Duration will be handled in main
                break;
            case 'e':
                monitoring_config.enable_alerts = true;
                break;
            case 'M':
                monitoring_config.enable_real_time = true;
                break;
            case 'Y':
                options.include_history = true;
                break;
            case 'g':
                options.time_range = optarg;
                break;
            case 'n':
                // Trends analysis will be handled in main
                break;
            case 'G':
                // Report generation will be handled in main
                break;
            case 'B':
                // Dashboard generation will be handled in main
                break;
            case 'E':
                // Export will be handled in main
                break;
            case 's':
                options.schema_filter = optarg;
                break;
            case 'b':
                options.table_filter = optarg;
                break;
            case 'l':
                options.max_results = std::atoi(optarg);
                break;
            case 'c':
                config_file = optarg;
                break;
            case 'L':
                log_file = optarg;
                break;
            case 'W':
                start_web_interface = true;
                break;
            case 'P':
                web_config.port = std::atoi(optarg);
                break;
            case 'A':
                web_config.bind_address = optarg;
                break;
            case 'S':
                web_config.enable_ssl = true;
                break;
            case 'O':
                web_config.enable_cors = true;
                break;
            case 'U':
                web_config.auth_token = optarg;
                break;
            case 'z':
                show_version_info = true;
                break;
            case '?':
                show_help = true;
                break;
            default:
                return false;
        }
    }
    
    // Get database path
    if (optind < argc) {
        database_path = argv[optind];
    }
    
    return true;
}

// Main function
int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        StatisticsOptions stats_options;
        MonitoringConfig monitoring_config;
        ConnectionOptions conn_options;
        SBEnhanced::WebServerConfig web_config;
        std::string database_path;
        std::string config_file;
        std::string log_file;
        bool show_help = false;
        bool show_version_info = false;
        bool start_monitoring = false;
        bool start_web_interface = false;
        
        // Parse command line
        if (!parseCommandLine(argc, argv, stats_options, monitoring_config, conn_options,
                             database_path, show_help, show_version_info, start_monitoring,
                             config_file, log_file, start_web_interface, web_config)) {
            showUsage();
            return 1;
        }
        
        // Handle help and version
        if (show_help) {
            showUsage();
            return 0;
        }
        
        if (show_version_info) {
            showVersion();
            return 0;
        }
        
        // Check if database path is provided
        if (database_path.empty()) {
            std::cerr << "Error: Database path is required" << std::endl;
            showUsage();
            return 1;
        }
        
        // Create GSTAT Enhanced instance
        GSTATEnhanced gstat;
        g_gstat_instance = &gstat;
        
        // Load configuration if specified
        if (!config_file.empty()) {
            if (!gstat.loadConfiguration(config_file)) {
                std::cerr << "Error: Failed to load configuration from " << config_file << std::endl;
                return 1;
            }
        }
        
        // Initialize
        if (!gstat.initialize(conn_options)) {
            std::cerr << "Error: Failed to initialize GSTAT Enhanced" << std::endl;
            return 1;
        }
        
        // Connect to database
        if (!gstat.connect(database_path, conn_options.username, conn_options.password, conn_options.role)) {
            std::cerr << "Error: Failed to connect to database: " << gstat.getLastError() << std::endl;
            return 1;
        }
        
        if (stats_options.verbose) {
            std::cout << "Connected to database: " << database_path << std::endl;
        }
        
        // Start web interface if requested
        if (start_web_interface) {
            if (stats_options.verbose) {
                std::cout << "Starting web interface on " << web_config.bind_address 
                         << ":" << web_config.port << "..." << std::endl;
            }
            
            if (!gstat.startWebInterface(web_config)) {
                std::cerr << "Error: Failed to start web interface" << std::endl;
                return 1;
            }
            
            std::cout << "Web interface started: " << gstat.getWebInterfaceUrl() << std::endl;
            std::cout << "Dashboard available at: " << gstat.getWebInterfaceUrl() << "/" << std::endl;
            std::cout << "API available at: " << gstat.getWebInterfaceUrl() << "/api/" << std::endl;
            
            // If only web interface is requested (no monitoring), run indefinitely
            if (!start_monitoring) {
                std::cout << "Web interface running. Press Ctrl+C to stop." << std::endl;
                while (gstat.isWebInterfaceRunning()) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                return 0;
            }
        }
        
        // Start monitoring if requested
        if (start_monitoring) {
            if (stats_options.verbose) {
                std::cout << "Starting monitoring with " << monitoring_config.collection_interval.count() 
                         << "s interval..." << std::endl;
            }
            
            if (!gstat.startMonitoring(monitoring_config)) {
                std::cerr << "Error: Failed to start monitoring" << std::endl;
                return 1;
            }
            
            // Keep running until interrupted
            std::cout << "Monitoring active. Press Ctrl+C to stop." << std::endl;
            while (gstat.isMonitoring()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            return 0;
        }
        
        // Collect statistics
        if (stats_options.verbose) {
            std::cout << "Collecting statistics..." << std::endl;
        }
        
        if (!gstat.collectStatistics(stats_options)) {
            std::cerr << "Error: Failed to collect statistics: " << gstat.getLastError() << std::endl;
            return 1;
        }
        
        // Perform analysis if requested
        AnalysisResult analysis_result;
        bool has_analysis = false;
        
        // Check for specific analysis requests from command line
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "-analyze" && i + 1 < argc) {
                std::string analysis_type = argv[i + 1];
                AnalysisType type;
                
                if (analysis_type == "basic") type = AnalysisType::BASIC;
                else if (analysis_type == "performance") type = AnalysisType::PERFORMANCE;
                else if (analysis_type == "health") type = AnalysisType::HEALTH_CHECK;
                else if (analysis_type == "capacity") type = AnalysisType::CAPACITY_PLANNING;
                else if (analysis_type == "trends") type = AnalysisType::TREND_ANALYSIS;
                else {
                    std::cerr << "Unknown analysis type: " << analysis_type << std::endl;
                    continue;
                }
                
                analysis_result = gstat.analyzeDatabase(type, stats_options);
                has_analysis = true;
                break;
            } else if (std::string(argv[i]) == "-health") {
                analysis_result = gstat.analyzeHealth(stats_options);
                has_analysis = true;
                break;
            } else if (std::string(argv[i]) == "-trends") {
                analysis_result = gstat.analyzeTrends(stats_options);
                has_analysis = true;
                break;
            }
        }
        
        // Export statistics or generate output
        if (!gstat.exportStatistics(stats_options)) {
            std::cerr << "Error: Failed to export statistics" << std::endl;
            return 1;
        }
        
        // Display analysis results if available
        if (has_analysis) {
            std::cout << "\n" << gstat.formatAnalysisResult(analysis_result, stats_options.output_format) << std::endl;
        }
        
        if (stats_options.verbose) {
            std::cout << "Statistics collection completed successfully." << std::endl;
            std::cout << "Total collections: " << gstat.getTotalCollections() << std::endl;
            std::cout << "Successful collections: " << gstat.getSuccessfulCollections() << std::endl;
            std::cout << "Failed collections: " << gstat.getFailedCollections() << std::endl;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}