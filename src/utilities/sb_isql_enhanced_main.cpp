#include "sb_isql_enhanced.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

// Global instance for signal handling
ISQLEnhanced* g_isql_instance = nullptr;

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    switch (signal) {
        case SIGINT:
        case SIGTERM:
            std::cout << "\nReceived interrupt signal. Shutting down gracefully..." << std::endl;
            if (g_isql_instance) {
                g_isql_instance->stopInteractiveMode();
            }
            break;
        default:
            break;
    }
}

// Print usage information
void printUsage(const char* program_name)
{
    std::cout << "ScratchBird Enhanced ISQL v0.5.0" << std::endl;
    std::cout << "Usage: " << program_name << " [OPTIONS] [database_path]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -h, --help                Show this help message" << std::endl;
    std::cout << "  -v, --version             Show version information" << std::endl;
    std::cout << "  -u, --user USERNAME       Username for database connection" << std::endl;
    std::cout << "  -p, --password PASSWORD   Password for database connection" << std::endl;
    std::cout << "  -r, --role ROLE           Role for database connection" << std::endl;
    std::cout << "  -c, --config CONFIG_FILE  Configuration file path" << std::endl;
    std::cout << "  -s, --script SCRIPT_FILE  Execute script file and exit" << std::endl;
    std::cout << "  -o, --output OUTPUT_FILE  Output file for results" << std::endl;
    std::cout << "  -l, --log LOG_FILE        Log file path" << std::endl;
    std::cout << "  -e, --echo                Echo commands to output" << std::endl;
    std::cout << "  -q, --quiet               Suppress informational messages" << std::endl;
    std::cout << "  -n, --no-headers          Don't show column headers" << std::endl;
    std::cout << "  -t, --timing              Show query execution timing" << std::endl;
    std::cout << "  -f, --format FORMAT       Output format (table, csv, json, xml, html, markdown)" << std::endl;
    std::cout << "  -a, --analyze             Enable query analysis and optimization hints" << std::endl;
    std::cout << "  -m, --monitor             Enable performance monitoring" << std::endl;
    std::cout << "  -T, --trace               Enable query tracing" << std::endl;
    std::cout << "  -S, --statistics          Show query statistics" << std::endl;
    std::cout << "  -P, --profile             Enable query profiling" << std::endl;
    std::cout << "  -C, --continue-on-error   Continue script execution on errors" << std::endl;
    std::cout << "  -V, --validate            Validate SQL syntax before execution" << std::endl;
    std::cout << "  -d, --dry-run             Parse and analyze queries without executing" << std::endl;
    std::cout << "  -i, --interactive         Force interactive mode even with script" << std::endl;
    std::cout << "  -b, --batch               Force batch mode (non-interactive)" << std::endl;
    std::cout << "  -E, --extract-ddl         Extract database DDL and exit" << std::endl;
    std::cout << "  -X, --extract-options     DDL extraction options (data,metadata,schemas,etc.)" << std::endl;
    std::cout << "  --page-size SIZE          Page size for result paging" << std::endl;
    std::cout << "  --max-width WIDTH         Maximum column width for output" << std::endl;
    std::cout << "  --timeout SECONDS         Query timeout in seconds" << std::endl;
    std::cout << "  --charset CHARSET         Character set for connection" << std::endl;
    std::cout << "  --read-only               Connect in read-only mode" << std::endl;
    std::cout << "  --no-gc                   Disable garbage collection" << std::endl;
    std::cout << "  --enable-ssl              Enable SSL connection" << std::endl;
    std::cout << "  --ssl-cert FILE           SSL certificate file" << std::endl;
    std::cout << "  --ssl-key FILE            SSL private key file" << std::endl;
    std::cout << "  --ssl-ca FILE             SSL CA certificate file" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program_name << " /path/to/database.fdb" << std::endl;
    std::cout << "  " << program_name << " -u sysdba -p masterkey /path/to/database.fdb" << std::endl;
    std::cout << "  " << program_name << " -s script.sql -o results.txt database.fdb" << std::endl;
    std::cout << "  " << program_name << " -f json -a -m database.fdb" << std::endl;
    std::cout << "  " << program_name << " -E -X metadata,schemas database.fdb" << std::endl;
    std::cout << std::endl;
    std::cout << "Interactive Commands:" << std::endl;
    std::cout << "  CONNECT database_path [user [password [role]]]" << std::endl;
    std::cout << "  DISCONNECT" << std::endl;
    std::cout << "  SHOW {TABLES|VIEWS|PROCEDURES|FUNCTIONS|SCHEMAS|DATABASE|VERSION}" << std::endl;
    std::cout << "  EXTRACT {DATABASE|SCHEMA schema_name|TABLE table_name}" << std::endl;
    std::cout << "  DESCRIBE table_name" << std::endl;
    std::cout << "  SET {ECHO|HEADERS|TIMING|STATISTICS|FORMAT|SCHEMA} {ON|OFF|value}" << std::endl;
    std::cout << "  HELP [command]" << std::endl;
    std::cout << "  EXIT or QUIT" << std::endl;
    std::cout << std::endl;
}

// Print version information
void printVersion()
{
    std::cout << "ScratchBird Enhanced ISQL v0.5.0" << std::endl;
    std::cout << "Built on: " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "Leverages existing ScratchBird infrastructure" << std::endl;
    std::cout << "Features: Advanced DDL extraction, Query analysis, Multiple output formats" << std::endl;
    std::cout << "Copyright (c) 2025 ScratchBird Project" << std::endl;
}

// Parse output format from string
SBEnhanced::OutputFormat parseOutputFormat(const std::string& format_str)
{
    std::string format = format_str;
    std::transform(format.begin(), format.end(), format.begin(), ::tolower);
    
    if (format == "table") return SBEnhanced::OutputFormat::TABLE;
    if (format == "csv") return SBEnhanced::OutputFormat::CSV;
    if (format == "json") return SBEnhanced::OutputFormat::JSON;
    if (format == "xml") return SBEnhanced::OutputFormat::XML;
    if (format == "html") return SBEnhanced::OutputFormat::HTML;
    if (format == "markdown") return SBEnhanced::OutputFormat::MARKDOWN;
    if (format == "fixed") return SBEnhanced::OutputFormat::FIXED_WIDTH;
    if (format == "delimited") return SBEnhanced::OutputFormat::DELIMITED;
    if (format == "yaml") return SBEnhanced::OutputFormat::YAML;
    if (format == "excel") return SBEnhanced::OutputFormat::EXCEL;
    if (format == "sql") return SBEnhanced::OutputFormat::SQL_INSERT;
    
    return SBEnhanced::OutputFormat::TABLE; // default
}

// Parse extract options from string
SBEnhanced::ExtractOptions parseExtractOptions(const std::string& options_str)
{
    SBEnhanced::ExtractOptions options;
    
    std::istringstream iss(options_str);
    std::string option;
    
    while (std::getline(iss, option, ',')) {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        
        std::transform(option.begin(), option.end(), option.begin(), ::tolower);
        
        if (option == "data") options.include_data = true;
        else if (option == "metadata") options.include_metadata = true;
        else if (option == "system") options.include_system_tables = true;
        else if (option == "indexes") options.include_indexes = true;
        else if (option == "constraints") options.include_constraints = true;
        else if (option == "triggers") options.include_triggers = true;
        else if (option == "procedures") options.include_procedures = true;
        else if (option == "functions") options.include_functions = true;
        else if (option == "views") options.include_views = true;
        else if (option == "domains") options.include_domains = true;
        else if (option == "generators") options.include_generators = true;
        else if (option == "roles") options.include_roles = true;
        else if (option == "schemas") options.include_schemas = true;
        else if (option == "comments") options.include_comments = true;
        else if (option == "grants") options.include_grants = true;
        else if (option == "format") options.format_output = true;
        else if (option == "drops") options.add_drop_statements = true;
        else if (option == "create_db") options.add_create_database = true;
        else if (option == "verbose") options.verbose = true;
    }
    
    return options;
}

// Main function
int main(int argc, char* argv[])
{
    // Command line options
    std::string database_path;
    std::string username = "SYSDBA";
    std::string password = "masterkey";
    std::string role;
    std::string config_file;
    std::string script_file;
    std::string output_file;
    std::string log_file;
    std::string charset = "UTF8";
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_ca_file;
    std::string extract_options_str;
    
    SBEnhanced::OutputFormat output_format = SBEnhanced::OutputFormat::TABLE;
    
    bool show_help = false;
    bool show_version = false;
    bool echo_commands = false;
    bool quiet_mode = false;
    bool show_headers = true;
    bool show_timing = false;
    bool show_statistics = false;
    bool enable_analysis = false;
    bool enable_monitoring = false;
    bool enable_tracing = false;
    bool enable_profiling = false;
    bool continue_on_error = false;
    bool validate_syntax = false;
    bool dry_run = false;
    bool force_interactive = false;
    bool force_batch = false;
    bool extract_ddl = false;
    bool read_only = false;
    bool no_gc = false;
    bool enable_ssl = false;
    
    int page_size = 20;
    int max_width = 50;
    int timeout_seconds = 300;
    
    // Long options
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"user", required_argument, 0, 'u'},
        {"password", required_argument, 0, 'p'},
        {"role", required_argument, 0, 'r'},
        {"config", required_argument, 0, 'c'},
        {"script", required_argument, 0, 's'},
        {"output", required_argument, 0, 'o'},
        {"log", required_argument, 0, 'l'},
        {"echo", no_argument, 0, 'e'},
        {"quiet", no_argument, 0, 'q'},
        {"no-headers", no_argument, 0, 'n'},
        {"timing", no_argument, 0, 't'},
        {"format", required_argument, 0, 'f'},
        {"analyze", no_argument, 0, 'a'},
        {"monitor", no_argument, 0, 'm'},
        {"trace", no_argument, 0, 'T'},
        {"statistics", no_argument, 0, 'S'},
        {"profile", no_argument, 0, 'P'},
        {"continue-on-error", no_argument, 0, 'C'},
        {"validate", no_argument, 0, 'V'},
        {"dry-run", no_argument, 0, 'd'},
        {"interactive", no_argument, 0, 'i'},
        {"batch", no_argument, 0, 'b'},
        {"extract-ddl", no_argument, 0, 'E'},
        {"extract-options", required_argument, 0, 'X'},
        {"page-size", required_argument, 0, 1001},
        {"max-width", required_argument, 0, 1002},
        {"timeout", required_argument, 0, 1003},
        {"charset", required_argument, 0, 1004},
        {"read-only", no_argument, 0, 1005},
        {"no-gc", no_argument, 0, 1006},
        {"enable-ssl", no_argument, 0, 1007},
        {"ssl-cert", required_argument, 0, 1008},
        {"ssl-key", required_argument, 0, 1009},
        {"ssl-ca", required_argument, 0, 1010},
        {0, 0, 0, 0}
    };
    
    // Parse command line arguments
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "hvu:p:r:c:s:o:l:eqntf:amTSPCVdibEX:", long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                show_help = true;
                break;
            case 'v':
                show_version = true;
                break;
            case 'u':
                username = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            case 'r':
                role = optarg;
                break;
            case 'c':
                config_file = optarg;
                break;
            case 's':
                script_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            case 'l':
                log_file = optarg;
                break;
            case 'e':
                echo_commands = true;
                break;
            case 'q':
                quiet_mode = true;
                break;
            case 'n':
                show_headers = false;
                break;
            case 't':
                show_timing = true;
                break;
            case 'f':
                output_format = parseOutputFormat(optarg);
                break;
            case 'a':
                enable_analysis = true;
                break;
            case 'm':
                enable_monitoring = true;
                break;
            case 'T':
                enable_tracing = true;
                break;
            case 'S':
                show_statistics = true;
                break;
            case 'P':
                enable_profiling = true;
                break;
            case 'C':
                continue_on_error = true;
                break;
            case 'V':
                validate_syntax = true;
                break;
            case 'd':
                dry_run = true;
                break;
            case 'i':
                force_interactive = true;
                break;
            case 'b':
                force_batch = true;
                break;
            case 'E':
                extract_ddl = true;
                break;
            case 'X':
                extract_options_str = optarg;
                break;
            case 1001:
                page_size = std::atoi(optarg);
                break;
            case 1002:
                max_width = std::atoi(optarg);
                break;
            case 1003:
                timeout_seconds = std::atoi(optarg);
                break;
            case 1004:
                charset = optarg;
                break;
            case 1005:
                read_only = true;
                break;
            case 1006:
                no_gc = true;
                break;
            case 1007:
                enable_ssl = true;
                break;
            case 1008:
                ssl_cert_file = optarg;
                break;
            case 1009:
                ssl_key_file = optarg;
                break;
            case 1010:
                ssl_ca_file = optarg;
                break;
            case '?':
                std::cerr << "Unknown option. Use --help for usage information." << std::endl;
                return 1;
            default:
                break;
        }
    }
    
    // Handle remaining arguments (database path)
    if (optind < argc) {
        database_path = argv[optind];
    }
    
    // Show help or version if requested
    if (show_help) {
        printUsage(argv[0]);
        return 0;
    }
    
    if (show_version) {
        printVersion();
        return 0;
    }
    
    // Validate arguments
    if (database_path.empty() && !script_file.empty()) {
        std::cerr << "Error: Database path is required when using script file." << std::endl;
        return 1;
    }
    
    if (force_interactive && force_batch) {
        std::cerr << "Error: Cannot specify both --interactive and --batch options." << std::endl;
        return 1;
    }
    
    try {
        // Create enhanced ISQL instance
        ISQLEnhanced isql;
        g_isql_instance = &isql;
        
        // Set up signal handlers
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        
        // Configure connection options
        SBEnhanced::ConnectionOptions connection_options;
        connection_options.database_path = database_path;
        connection_options.username = username;
        connection_options.password = password;
        connection_options.role = role;
        connection_options.charset = charset;
        connection_options.read_only = read_only;
        connection_options.no_garbage_collect = no_gc;
        connection_options.enable_monitoring = enable_monitoring;
        connection_options.enable_tracing = enable_tracing;
        connection_options.connect_timeout = std::chrono::seconds(timeout_seconds);
        connection_options.query_timeout = std::chrono::seconds(timeout_seconds);
        
        // Initialize enhanced ISQL
        if (!isql.initialize(connection_options)) {
            std::cerr << "Error: Failed to initialize enhanced ISQL." << std::endl;
            return 1;
        }
        
        // Load configuration file if specified
        if (!config_file.empty()) {
            if (!isql.loadConfiguration(config_file)) {
                std::cerr << "Warning: Failed to load configuration file: " << config_file << std::endl;
            }
        }
        
        // Configure output options
        isql.setOutputFormat(output_format);
        isql.setShowHeaders(show_headers);
        isql.setShowTiming(show_timing);
        isql.setShowStatistics(show_statistics);
        isql.setPageSize(page_size);
        isql.setMaxColumnWidth(max_width);
        
        if (!output_file.empty()) {
            isql.setOutputFile(output_file);
        }
        
        // Enable features based on command line options
        if (enable_analysis) {
            isql.enableQueryProfiling(true);
        }
        
        if (enable_monitoring) {
            isql.enablePerformanceMonitoring(true);
        }
        
        if (enable_tracing) {
            isql.enableTracing(true);
        }
        
        // Connect to database if specified
        if (!database_path.empty()) {
            if (!quiet_mode) {
                std::cout << "Connecting to database: " << database_path << std::endl;
            }
            
            if (!isql.connect(database_path, username, password, role)) {
                std::cerr << "Error: Failed to connect to database: " << database_path << std::endl;
                return 1;
            }
            
            if (!quiet_mode) {
                std::cout << "Connected successfully." << std::endl;
            }
        }
        
        int exit_code = 0;
        
        // Handle DDL extraction
        if (extract_ddl) {
            if (database_path.empty()) {
                std::cerr << "Error: Database path is required for DDL extraction." << std::endl;
                return 1;
            }
            
            SBEnhanced::ExtractOptions extract_options;
            if (!extract_options_str.empty()) {
                extract_options = parseExtractOptions(extract_options_str);
            }
            
            if (!output_file.empty()) {
                extract_options.output_file = output_file;
            }
            
            if (!isql.extractDatabaseDDL(extract_options)) {
                std::cerr << "Error: DDL extraction failed." << std::endl;
                exit_code = 1;
            }
        }
        // Handle script execution
        else if (!script_file.empty()) {
            if (!quiet_mode) {
                std::cout << "Executing script: " << script_file << std::endl;
            }
            
            SBEnhanced::ScriptOptions script_options;
            script_options.continue_on_error = continue_on_error;
            script_options.echo_commands = echo_commands;
            script_options.show_timing = show_timing;
            script_options.validate_syntax = validate_syntax;
            script_options.dry_run = dry_run;
            script_options.verbose = !quiet_mode;
            script_options.log_file = log_file;
            
            if (!isql.executeScript(script_file, script_options)) {
                std::cerr << "Error: Script execution failed." << std::endl;
                exit_code = 1;
            }
        }
        // Handle interactive mode
        else {
            bool interactive_mode = (!force_batch && (force_interactive || isatty(STDIN_FILENO)));
            
            if (interactive_mode) {
                if (!quiet_mode) {
                    std::cout << "Starting interactive mode..." << std::endl;
                }
                
                if (!isql.startInteractiveMode()) {
                    std::cerr << "Error: Interactive mode failed." << std::endl;
                    exit_code = 1;
                }
            } else {
                // Read from stdin in batch mode
                std::string command;
                std::string accumulated_command;
                
                while (std::getline(std::cin, command)) {
                    if (!accumulated_command.empty()) {
                        accumulated_command += "\n";
                    }
                    accumulated_command += command;
                    
                    // Check if command is complete (simplified check)
                    if (!accumulated_command.empty() && accumulated_command.back() == ';') {
                        if (!isql.executeCommand(accumulated_command)) {
                            if (!continue_on_error) {
                                exit_code = 1;
                                break;
                            }
                        }
                        accumulated_command.clear();
                    }
                }
                
                // Execute any remaining command
                if (!accumulated_command.empty()) {
                    if (!isql.executeCommand(accumulated_command)) {
                        exit_code = 1;
                    }
                }
            }
        }
        
        // Shutdown gracefully
        if (!quiet_mode && enable_monitoring) {
            auto metrics = isql.getPerformanceMetrics();
            std::cout << "\nPerformance Summary:" << std::endl;
            std::cout << "Total execution time: " << metrics.total_execution_time.count() / 1000.0 << " ms" << std::endl;
            std::cout << "Queries executed: " << metrics.queries_executed << std::endl;
            std::cout << "Cache hit ratio: " << (metrics.getCacheHitRatio() * 100.0) << "%" << std::endl;
        }
        
        isql.shutdown();
        
        if (!quiet_mode) {
            std::cout << "Session ended." << std::endl;
        }
        
        return exit_code;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown fatal error occurred." << std::endl;
        return 1;
    }
}