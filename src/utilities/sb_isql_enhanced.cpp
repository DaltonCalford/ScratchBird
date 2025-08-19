#include "sb_isql_enhanced.h"
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
#include <sys/wait.h>

using namespace SBEnhanced;

// Constructor
ISQLEnhanced::ISQLEnhanced()
    : input_stream(&std::cin),
      output_stream(&std::cout),
      error_stream(&std::cerr),
      total_commands_executed(0),
      successful_commands(0),
      failed_commands(0)
{
    session_start_time = std::chrono::steady_clock::now();
    
    // Initialize components
    engine = std::make_unique<SBEngineIntegration>();
    formatter = std::make_unique<OutputFormatter>();
    analyzer = std::make_unique<QueryAnalyzer>();
    config = std::make_unique<UtilityConfiguration>();
    
    // Initialize command processors
    initializeCommandProcessors();
    
    // Load default configuration
    loadDefaultConfiguration();
}

// Destructor
ISQLEnhanced::~ISQLEnhanced()
{
    stopInteractiveMode();
    closeFiles();
    
    if (engine && engine->isConnected()) {
        disconnect();
    }
}

// Initialize enhanced ISQL
bool ISQLEnhanced::initialize(const ConnectionOptions& options)
{
    try {
        // Initialize the engine integration layer
        if (!engine->initialize(options)) {
            logError("Failed to initialize engine integration layer");
            return false;
        }
        
        // Apply configuration
        applyConfiguration();
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Initialization failed: ") + e.what());
        return false;
    }
}

// Load configuration
bool ISQLEnhanced::loadConfiguration(const std::string& config_file)
{
    try {
        if (!config->loadConfiguration(config_file)) {
            logError("Failed to load configuration file: " + config_file);
            return false;
        }
        
        applyConfiguration();
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Configuration loading failed: ") + e.what());
        return false;
    }
}

// Connect to database
bool ISQLEnhanced::connect(const std::string& database_path, const std::string& username, 
                          const std::string& password, const std::string& role)
{
    try {
        ConnectionOptions options;
        options.database_path = database_path;
        options.username = username;
        options.password = password;
        options.role = role;
        options.enable_monitoring = true;
        options.enable_tracing = execution_context.enable_trace;
        
        if (!engine->connectToDatabase(database_path, options)) {
            logError("Failed to connect to database: " + database_path);
            return false;
        }
        
        // Update session state
        session_state.connected = true;
        session_state.current_database = database_path;
        session_state.current_user = username;
        session_state.current_role = role;
        
        // Set current schema if role specified
        if (!role.empty()) {
            execution_context.current_role = role;
        }
        
        // Log successful connection
        logMessage("Connected to database: " + database_path);
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Connection failed: ") + e.what());
        return false;
    }
}

// Disconnect from database
bool ISQLEnhanced::disconnect()
{
    try {
        if (!engine->isConnected()) {
            logMessage("Already disconnected from database");
            return true;
        }
        
        // Rollback any active transaction
        if (engine->isInTransaction()) {
            engine->rollbackTransaction();
        }
        
        if (!engine->disconnectFromDatabase()) {
            logError("Failed to disconnect from database");
            return false;
        }
        
        // Update session state
        session_state.connected = false;
        session_state.current_database.clear();
        session_state.current_user.clear();
        session_state.current_role.clear();
        
        logMessage("Disconnected from database");
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Disconnection failed: ") + e.what());
        return false;
    }
}

// Check if connected
bool ISQLEnhanced::isConnected() const
{
    return engine && engine->isConnected();
}

// Execute SQL statement
CommandResult ISQLEnhanced::executeSQLStatement(const std::string& sql)
{
    CommandResult result;
    
    try {
        if (!isConnected()) {
            result.error_message = "Not connected to database";
            return result;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        // Execute query using engine integration layer
        QueryResults query_results;
        if (!engine->executeQuery(sql, query_results)) {
            result.error_message = query_results.error_message;
            return result;
        }
        
        // Update result
        result.success = true;
        result.query_results = query_results;
        result.execution_time = query_results.execution_time;
        result.rows_affected = query_results.rows_affected;
        result.rows_fetched = query_results.rows_fetched;
        
        // Format output
        result.output_lines.push_back(formatOutput(query_results));
        
        // Add timing information if enabled
        if (execution_context.show_timing) {
            result.output_lines.push_back("Execution time: " + formatElapsedTime(result.execution_time));
        }
        
        // Add row count if enabled
        if (execution_context.show_row_counts) {
            if (result.rows_fetched > 0) {
                result.output_lines.push_back(formatRowCount(result.rows_fetched) + " rows fetched");
            }
            if (result.rows_affected > 0) {
                result.output_lines.push_back(formatRowCount(result.rows_affected) + " rows affected");
            }
        }
        
        // Update performance metrics
        updatePerformanceMetrics(result);
        
        return result;
    }
    catch (const std::exception& e) {
        result.error_message = std::string("SQL execution failed: ") + e.what();
        logError(result.error_message);
        return result;
    }
}

// Execute advanced DDL extraction
bool ISQLEnhanced::extractDatabaseDDL(const ExtractOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::ostringstream ddl_stream;
        
        // Add database creation statement if requested
        if (options.add_create_database) {
            ddl_stream << "-- Database creation statement\n";
            ddl_stream << "CREATE DATABASE '" << session_state.current_database << "';\n\n";
        }
        
        // Extract schemas if requested
        if (options.include_schemas) {
            ddl_stream << "-- Schemas\n";
            std::vector<std::string> schemas = listSchemas();
            for (const auto& schema : schemas) {
                if (options.schema_filter.empty() || schema.find(options.schema_filter) != std::string::npos) {
                    std::string schema_ddl;
                    if (engine->extractDDL(schema, DDLType::SCHEMA, schema_ddl)) {
                        ddl_stream << schema_ddl << "\n";
                    }
                }
            }
            ddl_stream << "\n";
        }
        
        // Extract domains if requested
        if (options.include_domains) {
            ddl_stream << "-- Domains\n";
            // This would use existing RDB$FIELDS queries
            ddl_stream << "-- Domain extractions would go here\n\n";
        }
        
        // Extract tables if requested
        if (options.include_metadata) {
            ddl_stream << "-- Tables\n";
            ShowOptions show_options;
            show_options.schema_filter = options.schema_filter;
            show_options.name_filter = options.table_filter;
            
            // This would use existing RDB$RELATIONS queries
            ddl_stream << "-- Table extractions would go here\n\n";
        }
        
        // Extract views if requested
        if (options.include_views) {
            ddl_stream << "-- Views\n";
            // This would use existing RDB$VIEW_RELATIONS queries
            ddl_stream << "-- View extractions would go here\n\n";
        }
        
        // Extract procedures if requested
        if (options.include_procedures) {
            ddl_stream << "-- Procedures\n";
            // This would use existing RDB$PROCEDURES queries
            ddl_stream << "-- Procedure extractions would go here\n\n";
        }
        
        // Extract functions if requested
        if (options.include_functions) {
            ddl_stream << "-- Functions\n";
            // This would use existing RDB$FUNCTIONS queries
            ddl_stream << "-- Function extractions would go here\n\n";
        }
        
        // Extract triggers if requested
        if (options.include_triggers) {
            ddl_stream << "-- Triggers\n";
            // This would use existing RDB$TRIGGERS queries
            ddl_stream << "-- Trigger extractions would go here\n\n";
        }
        
        // Extract indexes if requested
        if (options.include_indexes) {
            ddl_stream << "-- Indexes\n";
            // This would use existing RDB$INDICES queries
            ddl_stream << "-- Index extractions would go here\n\n";
        }
        
        // Extract constraints if requested
        if (options.include_constraints) {
            ddl_stream << "-- Constraints\n";
            // This would use existing RDB$RELATION_CONSTRAINTS queries
            ddl_stream << "-- Constraint extractions would go here\n\n";
        }
        
        // Extract generators if requested
        if (options.include_generators) {
            ddl_stream << "-- Generators\n";
            // This would use existing RDB$GENERATORS queries
            ddl_stream << "-- Generator extractions would go here\n\n";
        }
        
        // Extract roles if requested
        if (options.include_roles) {
            ddl_stream << "-- Roles\n";
            // This would use existing RDB$ROLES queries
            ddl_stream << "-- Role extractions would go here\n\n";
        }
        
        // Extract grants if requested
        if (options.include_grants) {
            ddl_stream << "-- Grants\n";
            // This would use existing RDB$USER_PRIVILEGES queries
            ddl_stream << "-- Grant extractions would go here\n\n";
        }
        
        std::string ddl_output = ddl_stream.str();
        
        // Format DDL if requested
        if (options.format_output) {
            ddl_output = formatter->formatDDL(ddl_output, options.ddl_format);
        }
        
        // Output to file or stream
        if (!options.output_file.empty()) {
            std::ofstream output_file(options.output_file);
            if (output_file.is_open()) {
                output_file << ddl_output;
                output_file.close();
                logMessage("DDL extracted to file: " + options.output_file);
            } else {
                logError("Failed to open output file: " + options.output_file);
                return false;
            }
        } else {
            *output_stream << ddl_output;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("DDL extraction failed: ") + e.what());
        return false;
    }
}

// Enhanced SHOW commands
bool ISQLEnhanced::showTables(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        // Build query using existing RDB$RELATIONS table
        std::string query = buildShowTablesQuery(options);
        
        // Execute query
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW TABLES query");
            return false;
        }
        
        // Format and display results
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        // Show statistics if requested
        if (options.include_statistics) {
            *output_stream << "Total tables: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW TABLES failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showSchemas(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        // Build query using existing RDB$SCHEMAS table with hierarchical support
        std::string query = buildShowSchemasQuery(options);
        
        // Execute query
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW SCHEMAS query");
            return false;
        }
        
        // Format and display results
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        // Show hierarchical information if available
        if (options.include_detailed_info) {
            *output_stream << "\nSchema Hierarchy:" << std::endl;
            for (const auto& row : results.rows) {
                if (row.size() > 3) { // Assuming schema path is in column 3
                    *output_stream << "  " << row[0] << " -> " << row[3] << std::endl;
                }
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW SCHEMAS failed: ") + e.what());
        return false;
    }
}

// Advanced query execution with analysis
bool ISQLEnhanced::executeQueryWithAnalysis(const std::string& sql)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        // Get query plan
        QueryPlan plan = analyzer->getExecutionPlan(sql);
        
        // Execute query
        CommandResult result = executeSQLStatement(sql);
        
        if (!result.success) {
            *error_stream << "Query execution failed: " << result.error_message << std::endl;
            return false;
        }
        
        // Display results
        for (const auto& line : result.output_lines) {
            *output_stream << line << std::endl;
        }
        
        // Display query plan
        *output_stream << "\nQuery Plan:" << std::endl;
        *output_stream << formatter->formatQueryPlan(plan) << std::endl;
        
        // Display optimization hints
        auto hints = analyzer->getOptimizationHints(sql);
        if (!hints.empty()) {
            *output_stream << "\nOptimization Hints:" << std::endl;
            for (const auto& hint : hints) {
                *output_stream << "  - " << hint << std::endl;
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Query analysis failed: ") + e.what());
        return false;
    }
}

// Enhanced output formatting
std::string ISQLEnhanced::formatOutput(const QueryResults& results) const
{
    return formatter->formatTable(results, execution_context.output_format);
}

// Start interactive mode
bool ISQLEnhanced::startInteractiveMode()
{
    try {
        session_state.interactive_mode = true;
        session_state.should_exit = false;
        
        // Display welcome message
        *output_stream << "ScratchBird Enhanced ISQL v0.6.0" << std::endl;
        *output_stream << "Type 'help' for command help, 'exit' to quit." << std::endl;
        
        if (isConnected()) {
            *output_stream << "Connected to: " << session_state.current_database << std::endl;
        }
        
        *output_stream << std::endl;
        
        // Execute startup commands
        for (const auto& command : session_state.startup_commands) {
            executeCommand(command);
        }
        
        // Main interactive loop
        std::string command;
        std::string accumulated_command;
        
        while (!session_state.should_exit) {
            // Display prompt
            std::string prompt = accumulated_command.empty() ? 
                                execution_context.prompt : 
                                execution_context.continuation_prompt;
            
            *output_stream << prompt;
            
            // Read command
            if (!std::getline(*input_stream, command)) {
                break; // EOF
            }
            
            // Skip empty lines
            if (command.empty()) {
                continue;
            }
            
            // Add to accumulated command
            if (!accumulated_command.empty()) {
                accumulated_command += "\n";
            }
            accumulated_command += command;
            
            // Check if command is complete
            if (isCompleteStatement(accumulated_command)) {
                // Process complete command
                processInteractiveCommand(accumulated_command);
                accumulated_command.clear();
            }
        }
        
        // Execute shutdown commands
        for (const auto& command : session_state.shutdown_commands) {
            executeCommand(command);
        }
        
        session_state.interactive_mode = false;
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Interactive mode failed: ") + e.what());
        return false;
    }
}

// Process interactive command
bool ISQLEnhanced::processInteractiveCommand(const std::string& command)
{
    try {
        // Add to history
        addToHistory(command);
        
        // Echo command if enabled
        if (execution_context.echo_commands) {
            *output_stream << command << std::endl;
        }
        
        // Execute command
        CommandResult result = executeCommand(command);
        
        // Display result
        if (result.success) {
            for (const auto& line : result.output_lines) {
                *output_stream << line << std::endl;
            }
            
            if (!result.message.empty()) {
                *output_stream << result.message << std::endl;
            }
        } else {
            *error_stream << "Error: " << result.error_message << std::endl;
        }
        
        return result.success;
    }
    catch (const std::exception& e) {
        logError(std::string("Command processing failed: ") + e.what());
        return false;
    }
}

// Execute command
CommandResult ISQLEnhanced::executeCommand(const std::string& command)
{
    CommandResult result;
    
    try {
        // Normalize command
        std::string normalized_command = normalizeCommand(command);
        
        // Skip empty commands and comments
        if (normalized_command.empty() || normalized_command[0] == '#' || 
            normalized_command.substr(0, 2) == "--") {
            result.success = true;
            return result;
        }
        
        // Parse command type
        CommandType cmd_type = parseCommandType(normalized_command);
        std::vector<std::string> args = parseCommandArgs(normalized_command);
        
        total_commands_executed++;
        
        // Execute based on command type
        switch (cmd_type) {
            case CommandType::CONNECT_COMMAND:
                result = processConnectCommand(args);
                break;
                
            case CommandType::DISCONNECT_COMMAND:
                result = processDisconnectCommand(args);
                break;
                
            case CommandType::SET_COMMAND:
                result = processSetCommand(args);
                break;
                
            case CommandType::SHOW_COMMAND:
                result = processShowCommand(args);
                break;
                
            case CommandType::EXTRACT_COMMAND:
                result = processExtractCommand(args);
                break;
                
            case CommandType::DESCRIBE_COMMAND:
                result = processDescribeCommand(args);
                break;
                
            case CommandType::HELP_COMMAND:
                result = processHelpCommand(args);
                break;
                
            case CommandType::INPUT_COMMAND:
                result = executeInputCommand(args.size() > 1 ? args[1] : "");
                break;
                
            case CommandType::OUTPUT_COMMAND:
                result = executeOutputCommand(args.size() > 1 ? args[1] : "");
                break;
                
            case CommandType::TRANSACTION_COMMAND:
                result = processTransactionCommand(args);
                break;
                
            case CommandType::SCRIPT_COMMAND:
                result = processScriptCommand(args);
                break;
                
            case CommandType::EXIT_COMMAND:
                result = processExitCommand(args);
                break;
                
            case CommandType::SQL_STATEMENT:
            case CommandType::DDL_STATEMENT:
                result = executeSQLStatement(normalized_command);
                break;
                
            default:
                result.error_message = "Unknown command: " + normalized_command;
                break;
        }
        
        // Update statistics
        if (result.success) {
            successful_commands++;
        } else {
            failed_commands++;
        }
        
        return result;
    }
    catch (const std::exception& e) {
        result.error_message = std::string("Command execution failed: ") + e.what();
        logError(result.error_message);
        return result;
    }
}

// Private helper methods

// Initialize command processors
void ISQLEnhanced::initializeCommandProcessors()
{
    // This would initialize all command processors
    // For now, we'll use the individual process methods
}

// Parse command type
CommandType ISQLEnhanced::parseCommandType(const std::string& command)
{
    std::string upper_command = command;
    std::transform(upper_command.begin(), upper_command.end(), upper_command.begin(), ::toupper);
    
    if (upper_command.substr(0, 7) == "CONNECT") return CommandType::CONNECT_COMMAND;
    if (upper_command.substr(0, 10) == "DISCONNECT") return CommandType::DISCONNECT_COMMAND;
    if (upper_command.substr(0, 3) == "SET") return CommandType::SET_COMMAND;
    if (upper_command.substr(0, 4) == "SHOW") return CommandType::SHOW_COMMAND;
    if (upper_command.substr(0, 7) == "EXTRACT") return CommandType::EXTRACT_COMMAND;
    if (upper_command.substr(0, 8) == "DESCRIBE" || upper_command.substr(0, 4) == "DESC") return CommandType::DESCRIBE_COMMAND;
    if (upper_command.substr(0, 4) == "HELP") return CommandType::HELP_COMMAND;
    if (upper_command.substr(0, 5) == "INPUT") return CommandType::INPUT_COMMAND;
    if (upper_command.substr(0, 6) == "OUTPUT") return CommandType::OUTPUT_COMMAND;
    if (upper_command.substr(0, 4) == "EXIT" || upper_command.substr(0, 4) == "QUIT") return CommandType::EXIT_COMMAND;
    if (upper_command.substr(0, 6) == "COMMIT" || upper_command.substr(0, 8) == "ROLLBACK" || 
        upper_command.substr(0, 5) == "BEGIN") return CommandType::TRANSACTION_COMMAND;
    if (upper_command.substr(0, 1) == "@" || upper_command.substr(0, 6) == "SCRIPT") return CommandType::SCRIPT_COMMAND;
    if (upper_command.substr(0, 6) == "SELECT" || upper_command.substr(0, 6) == "INSERT" || 
        upper_command.substr(0, 6) == "UPDATE" || upper_command.substr(0, 6) == "DELETE") return CommandType::SQL_STATEMENT;
    if (upper_command.substr(0, 6) == "CREATE" || upper_command.substr(0, 5) == "ALTER" || 
        upper_command.substr(0, 4) == "DROP") return CommandType::DDL_STATEMENT;
    if (command.empty()) return CommandType::EMPTY;
    if (command[0] == '#' || command.substr(0, 2) == "--") return CommandType::COMMENT;
    
    return CommandType::UNKNOWN;
}

// Parse command arguments
std::vector<std::string> ISQLEnhanced::parseCommandArgs(const std::string& command)
{
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    
    while (iss >> arg) {
        args.push_back(arg);
    }
    
    return args;
}

// Normalize command
std::string ISQLEnhanced::normalizeCommand(const std::string& command)
{
    std::string normalized = command;
    
    // Trim whitespace
    size_t start = normalized.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return "";
    }
    
    size_t end = normalized.find_last_not_of(" \t\n\r");
    normalized = normalized.substr(start, end - start + 1);
    
    // Remove trailing semicolon if present
    if (!normalized.empty() && normalized.back() == ';') {
        normalized.pop_back();
    }
    
    return normalized;
}

// Check if statement is complete
bool ISQLEnhanced::isCompleteStatement(const std::string& statement)
{
    // Simple implementation - check for semicolon at end
    // In a real implementation, this would be more sophisticated
    std::string trimmed = statement;
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) {
        trimmed = trimmed.substr(0, end + 1);
    }
    
    return !trimmed.empty() && trimmed.back() == ';';
}

// Build show tables query
std::string ISQLEnhanced::buildShowTablesQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$RELATION_NAME AS TABLE_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$RELATION_TYPE AS TYPE";
        query << ", RDB$OWNER_NAME AS OWNER";
        query << ", RDB$RELATION_ID AS ID";
    }
    
    query << " FROM RDB$RELATIONS";
    query << " WHERE RDB$RELATION_TYPE = 0"; // User tables only
    
    if (!options.include_system_objects) {
        query << " AND RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0";
    }
    
    if (!options.name_filter.empty()) {
        if (options.use_regex) {
            query << " AND RDB$RELATION_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << " AND RDB$RELATION_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$RELATION_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show schemas query
std::string ISQLEnhanced::buildShowSchemasQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$SCHEMA_NAME AS SCHEMA_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$PARENT_SCHEMA_NAME AS PARENT_SCHEMA";
        query << ", RDB$SCHEMA_PATH AS SCHEMA_PATH";
        query << ", RDB$SCHEMA_LEVEL AS LEVEL";
        query << ", RDB$OWNER_NAME AS OWNER";
    }
    
    query << " FROM RDB$SCHEMAS";
    
    if (!options.include_system_objects) {
        query << " WHERE RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0";
    }
    
    if (!options.name_filter.empty()) {
        std::string where_clause = options.include_system_objects ? " WHERE " : " AND ";
        if (options.use_regex) {
            query << where_clause << "RDB$SCHEMA_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << where_clause << "RDB$SCHEMA_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$SCHEMA_PATH, RDB$SCHEMA_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show views query
std::string ISQLEnhanced::buildShowViewsQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$RELATION_NAME AS VIEW_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$VIEW_SOURCE AS VIEW_SOURCE";
        query << ", RDB$OWNER_NAME AS OWNER";
        query << ", RDB$RELATION_ID AS ID";
    }
    
    query << " FROM RDB$RELATIONS";
    query << " WHERE RDB$RELATION_TYPE = 1"; // Views only
    
    if (!options.include_system_objects) {
        query << " AND (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        if (options.use_regex) {
            query << " AND RDB$RELATION_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << " AND RDB$RELATION_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$RELATION_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show procedures query
std::string ISQLEnhanced::buildShowProceduresQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$PROCEDURE_NAME AS PROCEDURE_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$PROCEDURE_INPUTS AS INPUTS";
        query << ", RDB$PROCEDURE_OUTPUTS AS OUTPUTS";
        query << ", RDB$OWNER_NAME AS OWNER";
        query << ", RDB$PROCEDURE_TYPE AS TYPE";
    }
    
    query << " FROM RDB$PROCEDURES";
    
    if (!options.include_system_objects) {
        query << " WHERE (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        std::string where_clause = options.include_system_objects ? " WHERE " : " AND ";
        if (options.use_regex) {
            query << where_clause << "RDB$PROCEDURE_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << where_clause << "RDB$PROCEDURE_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$PROCEDURE_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show functions query
std::string ISQLEnhanced::buildShowFunctionsQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$FUNCTION_NAME AS FUNCTION_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$FUNCTION_TYPE AS TYPE";
        query << ", RDB$MODULE_NAME AS MODULE";
        query << ", RDB$ENTRYPOINT AS ENTRYPOINT";
        query << ", RDB$RETURN_ARGUMENT AS RETURN_ARG";
    }
    
    query << " FROM RDB$FUNCTIONS";
    
    if (!options.include_system_objects) {
        query << " WHERE (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        std::string where_clause = options.include_system_objects ? " WHERE " : " AND ";
        if (options.use_regex) {
            query << where_clause << "RDB$FUNCTION_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << where_clause << "RDB$FUNCTION_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$FUNCTION_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show triggers query
std::string ISQLEnhanced::buildShowTriggersQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$TRIGGER_NAME AS TRIGGER_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$RELATION_NAME AS TABLE_NAME";
        query << ", RDB$TRIGGER_TYPE AS TYPE";
        query << ", RDB$TRIGGER_SEQUENCE AS SEQUENCE";
        query << ", RDB$TRIGGER_INACTIVE AS INACTIVE";
    }
    
    query << " FROM RDB$TRIGGERS";
    
    if (!options.include_system_objects) {
        query << " WHERE (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        std::string where_clause = options.include_system_objects ? " WHERE " : " AND ";
        if (options.use_regex) {
            query << where_clause << "RDB$TRIGGER_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << where_clause << "RDB$TRIGGER_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$RELATION_NAME, RDB$TRIGGER_SEQUENCE, RDB$TRIGGER_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show indexes query
std::string ISQLEnhanced::buildShowIndexesQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$INDEX_NAME AS INDEX_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$RELATION_NAME AS TABLE_NAME";
        query << ", RDB$UNIQUE_FLAG AS UNIQUE_FLAG";
        query << ", RDB$INDEX_INACTIVE AS INACTIVE";
        query << ", RDB$INDEX_TYPE AS TYPE";
    }
    
    query << " FROM RDB$INDICES";
    
    if (!options.include_system_objects) {
        query << " WHERE (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        std::string where_clause = options.include_system_objects ? " WHERE " : " AND ";
        if (options.use_regex) {
            query << where_clause << "RDB$INDEX_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << where_clause << "RDB$INDEX_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$RELATION_NAME, RDB$INDEX_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show domains query
std::string ISQLEnhanced::buildShowDomainsQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$FIELD_NAME AS DOMAIN_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$FIELD_TYPE AS TYPE";
        query << ", RDB$FIELD_LENGTH AS LENGTH";
        query << ", RDB$FIELD_SCALE AS SCALE";
        query << ", RDB$FIELD_SUB_TYPE AS SUB_TYPE";
        query << ", RDB$DEFAULT_SOURCE AS DEFAULT_VALUE";
        query << ", RDB$VALIDATION_SOURCE AS CHECK_CONSTRAINT";
    }
    
    query << " FROM RDB$FIELDS";
    query << " WHERE RDB$FIELD_NAME NOT STARTING WITH 'RDB$'";
    
    if (!options.include_system_objects) {
        query << " AND (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        if (options.use_regex) {
            query << " AND RDB$FIELD_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << " AND RDB$FIELD_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$FIELD_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show generators query
std::string ISQLEnhanced::buildShowGeneratorsQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$GENERATOR_NAME AS GENERATOR_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$GENERATOR_ID AS ID";
        query << ", GEN_ID(RDB$GENERATOR_NAME, 0) AS CURRENT_VALUE";
    }
    
    query << " FROM RDB$GENERATORS";
    
    if (!options.include_system_objects) {
        query << " WHERE (RDB$SYSTEM_FLAG IS NULL OR RDB$SYSTEM_FLAG = 0)";
    }
    
    if (!options.name_filter.empty()) {
        std::string where_clause = options.include_system_objects ? " WHERE " : " AND ";
        if (options.use_regex) {
            query << where_clause << "RDB$GENERATOR_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << where_clause << "RDB$GENERATOR_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$GENERATOR_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show roles query
std::string ISQLEnhanced::buildShowRolesQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$ROLE_NAME AS ROLE_NAME";
    
    if (options.include_detailed_info) {
        query << ", RDB$OWNER_NAME AS OWNER";
    }
    
    query << " FROM RDB$ROLES";
    
    if (!options.name_filter.empty()) {
        if (options.use_regex) {
            query << " WHERE RDB$ROLE_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << " WHERE RDB$ROLE_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$ROLE_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show users query
std::string ISQLEnhanced::buildShowUsersQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT SEC$USER_NAME AS USER_NAME";
    
    if (options.include_detailed_info) {
        query << ", SEC$FIRST_NAME AS FIRST_NAME";
        query << ", SEC$MIDDLE_NAME AS MIDDLE_NAME";
        query << ", SEC$LAST_NAME AS LAST_NAME";
        query << ", SEC$ACTIVE AS ACTIVE";
        query << ", SEC$ADMIN AS ADMIN";
    }
    
    query << " FROM SEC$USERS";
    
    if (!options.name_filter.empty()) {
        if (options.use_regex) {
            query << " WHERE SEC$USER_NAME SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << " WHERE SEC$USER_NAME LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY SEC$USER_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show grants query
std::string ISQLEnhanced::buildShowGrantsQuery(const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$USER AS GRANTEE";
    
    if (options.include_detailed_info) {
        query << ", RDB$GRANTOR AS GRANTOR";
        query << ", RDB$PRIVILEGE AS PRIVILEGE";
        query << ", RDB$RELATION_NAME AS OBJECT_NAME";
        query << ", RDB$OBJECT_TYPE AS OBJECT_TYPE";
        query << ", RDB$FIELD_NAME AS FIELD_NAME";
        query << ", RDB$GRANT_OPTION AS GRANT_OPTION";
    }
    
    query << " FROM RDB$USER_PRIVILEGES";
    
    if (!options.name_filter.empty()) {
        if (options.use_regex) {
            query << " WHERE RDB$USER SIMILAR TO '" << options.name_filter << "'";
        } else {
            query << " WHERE RDB$USER LIKE '%" << options.name_filter << "%'";
        }
    }
    
    if (options.sort_results) {
        query << " ORDER BY RDB$USER, RDB$RELATION_NAME, RDB$PRIVILEGE";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Build show dependencies query
std::string ISQLEnhanced::buildShowDependenciesQuery(const std::string& object_name, const ShowOptions& options)
{
    std::ostringstream query;
    
    query << "SELECT RDB$DEPENDENT_NAME AS DEPENDENT_OBJECT";
    
    if (options.include_detailed_info) {
        query << ", RDB$DEPENDENT_TYPE AS DEPENDENT_TYPE";
        query << ", RDB$DEPENDED_ON_NAME AS DEPENDS_ON";
        query << ", RDB$DEPENDED_ON_TYPE AS DEPENDS_ON_TYPE";
        query << ", RDB$FIELD_NAME AS FIELD_NAME";
    }
    
    query << " FROM RDB$DEPENDENCIES";
    query << " WHERE RDB$DEPENDED_ON_NAME = '" << object_name << "'";
    
    if (options.sort_results) {
        query << " ORDER BY RDB$DEPENDENT_NAME";
    }
    
    if (options.max_results > 0) {
        query << " FIRST " << options.max_results;
    }
    
    return query.str();
}

// Process connect command
CommandResult ISQLEnhanced::processConnectCommand(const std::vector<std::string>& args)
{
    CommandResult result;
    
    if (args.size() < 2) {
        result.error_message = "Usage: CONNECT database_path [username [password [role]]]";
        return result;
    }
    
    std::string database_path = args[1];
    std::string username = args.size() > 2 ? args[2] : "SYSDBA";
    std::string password = args.size() > 3 ? args[3] : "masterkey";
    std::string role = args.size() > 4 ? args[4] : "";
    
    if (connect(database_path, username, password, role)) {
        result.success = true;
        result.message = "Connected to database: " + database_path;
    } else {
        result.error_message = "Failed to connect to database: " + database_path;
    }
    
    return result;
}

// Process disconnect command
CommandResult ISQLEnhanced::processDisconnectCommand(const std::vector<std::string>& args)
{
    CommandResult result;
    
    if (disconnect()) {
        result.success = true;
        result.message = "Disconnected from database";
    } else {
        result.error_message = "Failed to disconnect from database";
    }
    
    return result;
}

// Process show command
CommandResult ISQLEnhanced::processShowCommand(const std::vector<std::string>& args)
{
    CommandResult result;
    
    if (args.size() < 2) {
        result.error_message = "Usage: SHOW {TABLES|VIEWS|PROCEDURES|FUNCTIONS|SCHEMAS|DATABASE|VERSION|USERS|ROLES|GENERATORS|DOMAINS|INDICES|TRIGGERS|GRANTS|COLLATIONS|EXCEPTIONS|FILTERS|PACKAGES|SYSTEM|DEPENDENCIES|PRIVILEGES|MAPPING|SECCLASSES|CURRENT|HOME|TIME|SESSION|CONNECTIONS|TRANSACTIONS|STATISTICS|WIRE_STATISTICS|...}";
        return result;
    }
    
    std::string show_type = args[1];
    std::transform(show_type.begin(), show_type.end(), show_type.begin(), ::toupper);
    
    ShowOptions options;
    options.output_format = execution_context.output_format;
    
    // Parse optional object name or filter
    if (args.size() > 2) {
        options.name_filter = args[2];
    }
    
    if (show_type == "TABLES") {
        result.success = showTables(options);
    } else if (show_type == "VIEWS") {
        result.success = showViews(options);
    } else if (show_type == "PROCEDURES") {
        result.success = showProcedures(options);
    } else if (show_type == "FUNCTIONS") {
        result.success = showFunctions(options);
    } else if (show_type == "TRIGGERS") {
        result.success = showTriggers(options);
    } else if (show_type == "INDICES" || show_type == "INDEXES") {
        result.success = showIndexes(options);
    } else if (show_type == "DOMAINS") {
        result.success = showDomains(options);
    } else if (show_type == "GENERATORS") {
        result.success = showGenerators(options);
    } else if (show_type == "ROLES") {
        result.success = showRoles(options);
    } else if (show_type == "USERS") {
        result.success = showUsers(options);
    } else if (show_type == "SCHEMAS") {
        result.success = showSchemas(options);
    } else if (show_type == "GRANTS") {
        result.success = showGrants(options);
    } else if (show_type == "COLLATIONS") {
        result.success = showCollations(options);
    } else if (show_type == "EXCEPTIONS") {
        result.success = showExceptions(options);
    } else if (show_type == "FILTERS") {
        result.success = showFilters(options);
    } else if (show_type == "PACKAGES") {
        result.success = showPackages(options);
    } else if (show_type == "SYSTEM") {
        result.success = showSystemTables(options);
    } else if (show_type == "VERSION") {
        result.success = showVersion();
    } else if (show_type == "DATABASE") {
        result.success = showDatabase();
    } else if (show_type == "SCHEMA") {
        result.success = showCurrentSchema();
    } else if (show_type == "HOME") {
        std::string home_type = args.size() > 2 ? args[2] : "";
        std::transform(home_type.begin(), home_type.end(), home_type.begin(), ::toupper);
        if (home_type == "SCHEMA") {
            result.success = showHomeSchema();
        } else {
            result.error_message = "Usage: SHOW HOME SCHEMA";
        }
    } else if (show_type == "CURRENT") {
        std::string current_type = args.size() > 2 ? args[2] : "";
        std::transform(current_type.begin(), current_type.end(), current_type.begin(), ::toupper);
        if (current_type == "SCHEMA") {
            result.success = showCurrentSchema();
        } else if (current_type == "ROLE") {
            result.success = showCurrentRole();
        } else if (current_type == "USER") {
            result.success = showCurrentUser();
        } else {
            result.error_message = "Usage: SHOW CURRENT {SCHEMA|ROLE|USER}";
        }
    } else if (show_type == "TIME") {
        result.success = showTime();
    } else if (show_type == "SESSION") {
        result.success = showSession();
    } else if (show_type == "CONNECTIONS") {
        result.success = showConnections(options);
    } else if (show_type == "TRANSACTIONS") {
        result.success = showTransactions(options);
    } else if (show_type == "STATISTICS") {
        result.success = showStatistics(options);
    } else if (show_type == "WIRE_STATISTICS" || show_type == "WIRE_STATS") {
        result.success = showWireStatistics();
    } else if (show_type == "DEPENDENCIES") {
        if (args.size() > 2) {
            result.success = showDependencies(args[2], options);
        } else {
            result.error_message = "Usage: SHOW DEPENDENCIES object_name";
        }
    } else if (show_type == "PRIVILEGES") {
        result.success = showPrivileges(options);
    } else if (show_type == "MAPPING") {
        result.success = showMapping(options);
    } else if (show_type == "SECCLASSES") {
        result.success = showSecClasses(options);
    } else {
        result.error_message = "Unknown SHOW command: " + show_type;
        result.error_message += "\nAvailable: TABLES, VIEWS, PROCEDURES, FUNCTIONS, TRIGGERS, INDICES, DOMAINS, GENERATORS, ROLES, USERS, SCHEMAS, GRANTS, COLLATIONS, EXCEPTIONS, FILTERS, PACKAGES, SYSTEM, VERSION, DATABASE, SCHEMA, HOME SCHEMA, CURRENT {SCHEMA|ROLE|USER}, TIME, SESSION, CONNECTIONS, TRANSACTIONS, STATISTICS, WIRE_STATISTICS, DEPENDENCIES, PRIVILEGES, MAPPING, SECCLASSES";
    }
    
    return result;
}

// Process exit command
CommandResult ISQLEnhanced::processExitCommand(const std::vector<std::string>& args)
{
    CommandResult result;
    
    session_state.should_exit = true;
    result.success = true;
    result.message = "Goodbye!";
    
    return result;
}

// Show version
bool ISQLEnhanced::showVersion()
{
    *output_stream << "ScratchBird Enhanced ISQL v0.6.0" << std::endl;
    *output_stream << "Built on: " << __DATE__ << " " << __TIME__ << std::endl;
    
    if (isConnected()) {
        *output_stream << "Database version: " << engine->getDatabaseVersion() << std::endl;
        *output_stream << "Engine version: " << engine->getEngineVersion() << std::endl;
    }
    
    return true;
}

// Show database
bool ISQLEnhanced::showDatabase()
{
    if (!isConnected()) {
        *error_stream << "Not connected to database" << std::endl;
        return false;
    }
    
    *output_stream << "Database: " << session_state.current_database << std::endl;
    *output_stream << "User: " << session_state.current_user << std::endl;
    
    if (!session_state.current_role.empty()) {
        *output_stream << "Role: " << session_state.current_role << std::endl;
    }
    
    if (!execution_context.current_schema.empty()) {
        *output_stream << "Current schema: " << execution_context.current_schema << std::endl;
    }
    
    return true;
}

// Additional SHOW command implementations

bool ISQLEnhanced::showViews(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowViewsQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW VIEWS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total views: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW VIEWS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showProcedures(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowProceduresQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW PROCEDURES query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total procedures: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW PROCEDURES failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showFunctions(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowFunctionsQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW FUNCTIONS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total functions: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW FUNCTIONS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showTriggers(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowTriggersQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW TRIGGERS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total triggers: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW TRIGGERS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showIndexes(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowIndexesQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW INDEXES query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total indexes: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW INDEXES failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showDomains(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowDomainsQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW DOMAINS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total domains: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW DOMAINS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showGenerators(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowGeneratorsQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW GENERATORS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total generators: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW GENERATORS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showRoles(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowRolesQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW ROLES query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total roles: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW ROLES failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showUsers(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowUsersQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW USERS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total users: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW USERS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showGrants(const ShowOptions& options)
{
    try {
        if (!isConnected()) {
            logError("Not connected to database");
            return false;
        }
        
        std::string query = buildShowGrantsQuery(options);
        QueryResults results;
        if (!engine->executeQuery(query, results)) {
            logError("Failed to execute SHOW GRANTS query");
            return false;
        }
        
        std::string formatted_output = formatter->formatTable(results, options.output_format);
        *output_stream << formatted_output << std::endl;
        
        if (options.include_statistics) {
            *output_stream << "Total grants: " << results.rows.size() << std::endl;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("SHOW GRANTS failed: ") + e.what());
        return false;
    }
}

bool ISQLEnhanced::showCurrentSchema()
{
    *output_stream << "Current schema: " << 
        (execution_context.current_schema.empty() ? "None" : execution_context.current_schema) << std::endl;
    return true;
}

bool ISQLEnhanced::showCurrentRole()
{
    *output_stream << "Current role: " << 
        (execution_context.current_role.empty() ? "None" : execution_context.current_role) << std::endl;
    return true;
}

bool ISQLEnhanced::showCurrentUser()
{
    *output_stream << "Current user: " << 
        (session_state.current_user.empty() ? "None" : session_state.current_user) << std::endl;
    return true;
}

bool ISQLEnhanced::showHomeSchema()
{
    *output_stream << "Home schema: " << 
        (execution_context.session_variables.count("HOME_SCHEMA") > 0 ? 
         execution_context.session_variables.at("HOME_SCHEMA") : "None") << std::endl;
    return true;
}

bool ISQLEnhanced::showTime()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    *output_stream << "Current time: " << std::ctime(&time_t) << std::endl;
    return true;
}

bool ISQLEnhanced::showSession()
{
    *output_stream << "Session Information:" << std::endl;
    *output_stream << "  Database: " << session_state.current_database << std::endl;
    *output_stream << "  User: " << session_state.current_user << std::endl;
    *output_stream << "  Role: " << session_state.current_role << std::endl;
    *output_stream << "  Schema: " << execution_context.current_schema << std::endl;
    *output_stream << "  Connected: " << (session_state.connected ? "Yes" : "No") << std::endl;
    *output_stream << "  Interactive: " << (session_state.interactive_mode ? "Yes" : "No") << std::endl;
    *output_stream << "  Commands executed: " << total_commands_executed << std::endl;
    *output_stream << "  Successful: " << successful_commands << std::endl;
    *output_stream << "  Failed: " << failed_commands << std::endl;
    
    auto session_duration = std::chrono::steady_clock::now() - session_start_time;
    auto duration_minutes = std::chrono::duration_cast<std::chrono::minutes>(session_duration);
    *output_stream << "  Session duration: " << duration_minutes.count() << " minutes" << std::endl;
    
    return true;
}

// Placeholder implementations for remaining show commands that need specialized queries
bool ISQLEnhanced::showCollations(const ShowOptions& options) { *output_stream << "SHOW COLLATIONS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showExceptions(const ShowOptions& options) { *output_stream << "SHOW EXCEPTIONS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showFilters(const ShowOptions& options) { *output_stream << "SHOW FILTERS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showPackages(const ShowOptions& options) { *output_stream << "SHOW PACKAGES not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showDependencies(const std::string& object_name, const ShowOptions& options) { *output_stream << "SHOW DEPENDENCIES not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showConnections(const ShowOptions& options) { *output_stream << "SHOW CONNECTIONS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showTransactions(const ShowOptions& options) { *output_stream << "SHOW TRANSACTIONS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showStatistics(const ShowOptions& options) { *output_stream << "SHOW STATISTICS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showWireStatistics() { *output_stream << "SHOW WIRE_STATISTICS not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showPrivileges(const ShowOptions& options) { *output_stream << "SHOW PRIVILEGES not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showMapping(const ShowOptions& options) { *output_stream << "SHOW MAPPING not yet implemented" << std::endl; return true; }
bool ISQLEnhanced::showSecClasses(const ShowOptions& options) { *output_stream << "SHOW SECCLASSES not yet implemented" << std::endl; return true; }

// List schemas
std::vector<std::string> ISQLEnhanced::listSchemas(const std::string& pattern)
{
    std::vector<std::string> schemas;
    
    if (!isConnected()) {
        return schemas;
    }
    
    try {
        ShowOptions options;
        options.name_filter = pattern;
        
        std::string query = buildShowSchemasQuery(options);
        QueryResults results;
        
        if (engine->executeQuery(query, results)) {
            for (const auto& row : results.rows) {
                if (!row.empty()) {
                    schemas.push_back(row[0]);
                }
            }
        }
    }
    catch (const std::exception& e) {
        logError(std::string("List schemas failed: ") + e.what());
    }
    
    return schemas;
}

// Add to history
void ISQLEnhanced::addToHistory(const std::string& command)
{
    execution_context.command_history.push_back(command);
    
    // Limit history size
    if (execution_context.command_history.size() > static_cast<size_t>(execution_context.max_history_size)) {
        execution_context.command_history.erase(execution_context.command_history.begin());
    }
}

// Load default configuration
void ISQLEnhanced::loadDefaultConfiguration()
{
    config->loadDefaultConfiguration();
}

// Apply configuration
void ISQLEnhanced::applyConfiguration()
{
    const auto& isql_config = config->getISQLConfig();
    
    execution_context.output_format = isql_config.default_format;
    execution_context.page_size = isql_config.page_size;
    execution_context.show_headers = isql_config.show_headers;
    execution_context.show_statistics = isql_config.show_statistics;
    execution_context.show_timing = isql_config.show_query_time;
    execution_context.show_row_counts = isql_config.show_row_count;
    execution_context.auto_commit = isql_config.enable_auto_commit;
    execution_context.echo_commands = isql_config.enable_echo_commands;
    execution_context.enable_paging = isql_config.enable_result_paging;
    execution_context.prompt = isql_config.prompt;
    execution_context.continuation_prompt = isql_config.continuation_prompt;
    execution_context.max_history_size = isql_config.max_history_size;
    
    // Configure formatter
    formatter->setDefaultFormat(isql_config.default_format);
    formatter->setShowHeaders(isql_config.show_headers);
    formatter->setShowStatistics(isql_config.show_statistics);
    formatter->setPageSize(isql_config.page_size);
    formatter->setMaxColumnWidth(isql_config.max_column_width);
    formatter->setNullDisplay(isql_config.null_display);
    formatter->setDateFormat(isql_config.date_format);
}

// Update performance metrics
void ISQLEnhanced::updatePerformanceMetrics(const CommandResult& result)
{
    // This would update internal performance tracking
    // For now, this is a placeholder
}

// Format elapsed time
std::string ISQLEnhanced::formatElapsedTime(const std::chrono::microseconds& duration)
{
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return std::to_string(ms.count()) + " ms";
}

// Format row count
std::string ISQLEnhanced::formatRowCount(uint64_t count)
{
    return std::to_string(count);
}

// Log error
void ISQLEnhanced::logError(const std::string& error)
{
    std::lock_guard<std::mutex> lock(error_mutex);
    error_log.push_back(error);
}

// Log message
void ISQLEnhanced::logMessage(const std::string& message)
{
    // This would log to the appropriate output stream
    // For now, we'll just output to the console
    if (execution_context.echo_commands) {
        *output_stream << "INFO: " << message << std::endl;
    }
}

// Close files
void ISQLEnhanced::closeFiles()
{
    if (output_file) {
        output_file->close();
        output_file.reset();
    }
    
    if (log_file) {
        log_file->close();
        log_file.reset();
    }
    
    if (error_file) {
        error_file->close();
        error_file.reset();
    }
    
    if (script_file) {
        script_file->close();
        script_file.reset();
    }
}

// Shutdown
bool ISQLEnhanced::shutdown()
{
    try {
        stopInteractiveMode();
        
        if (isConnected()) {
            disconnect();
        }
        
        if (engine) {
            engine->shutdown();
        }
        
        closeFiles();
        
        return true;
    }
    catch (const std::exception& e) {
        logError(std::string("Shutdown failed: ") + e.what());
        return false;
    }
}

// Stop interactive mode
void ISQLEnhanced::stopInteractiveMode()
{
    session_state.should_exit = true;
    session_state.interactive_mode = false;
}

// Process SET command
CommandResult ISQLEnhanced::processSetCommand(const std::vector<std::string>& args)
{
    CommandResult result;
    
    if (args.size() < 2) {
        // Display all SET options
        result.success = true;
        result.output_lines.push_back("SET Command Status:");
        result.output_lines.push_back("  AUTODDL: " + std::string(execution_context.set_state.autoddl ? "ON" : "OFF"));
        result.output_lines.push_back("  ECHO: " + std::string(execution_context.set_state.echo ? "ON" : "OFF"));
        result.output_lines.push_back("  HEADING: " + std::string(execution_context.set_state.heading ? "ON" : "OFF"));
        result.output_lines.push_back("  LIST: " + std::string(execution_context.set_state.list ? "ON" : "OFF"));
        result.output_lines.push_back("  PAGESIZE: " + std::to_string(execution_context.set_state.pagesize));
        result.output_lines.push_back("  ROWCOUNT: " + std::string(execution_context.set_state.rowcount ? "ON" : "OFF"));
        result.output_lines.push_back("  SQLDA_DISPLAY: " + std::string(execution_context.set_state.sqlda_display ? "ON" : "OFF"));
        result.output_lines.push_back("  STATS: " + std::string(execution_context.set_state.stats ? "ON" : "OFF"));
        result.output_lines.push_back("  TERM: '" + execution_context.set_state.term + "'");
        result.output_lines.push_back("  TIME: " + std::string(execution_context.set_state.time ? "ON" : "OFF"));
        result.output_lines.push_back("  WARNINGS: " + std::string(execution_context.set_state.warnings ? "ON" : "OFF"));
        result.output_lines.push_back("  NAMES: '" + execution_context.set_state.names + "'");
        result.output_lines.push_back("  CHARSET: '" + execution_context.set_state.charset + "'");
        result.output_lines.push_back("  BAIL: " + std::string(execution_context.set_state.bail ? "ON" : "OFF"));
        result.output_lines.push_back("  BULK_INSERT: " + std::string(execution_context.set_state.bulk_insert ? "ON" : "OFF"));
        result.output_lines.push_back("  COUNT: " + std::string(execution_context.set_state.count ? "ON" : "OFF"));
        result.output_lines.push_back("  PLAN: " + std::string(execution_context.set_state.plan ? "ON" : "OFF"));
        result.output_lines.push_back("  PLANONLY: " + std::string(execution_context.set_state.planonly ? "ON" : "OFF"));
        result.output_lines.push_back("  MAXROWS: " + std::to_string(execution_context.set_state.maxrows));
        result.output_lines.push_back("  SQLDIALECT: " + std::to_string(execution_context.set_state.sqldialect));
        result.output_lines.push_back("  TRANSACTION: '" + execution_context.set_state.transaction + "'");
        result.output_lines.push_back("  WIDTH: " + std::to_string(execution_context.set_state.width));
        result.output_lines.push_back("  CLIENTLIB: '" + execution_context.set_state.clientlib + "'");
        result.output_lines.push_back("  DECFLOAT: '" + execution_context.set_state.decfloat + "'");
        return result;
    }
    
    std::string set_option = args[1];
    std::transform(set_option.begin(), set_option.end(), set_option.begin(), ::toupper);
    
    // Parse value if provided
    std::string value_str;
    bool bool_value = false;
    int int_value = 0;
    
    if (args.size() > 2) {
        value_str = args[2];
        std::transform(value_str.begin(), value_str.end(), value_str.begin(), ::toupper);
        bool_value = (value_str == "ON" || value_str == "TRUE" || value_str == "1");
        int_value = std::atoi(value_str.c_str());
    } else {
        // Toggle boolean options when no value provided
        bool_value = true;
    }
    
    // Process SET command based on option
    if (set_option == "AUTODDL") {
        if (args.size() > 2) {
            return executeSetAutoddl(bool_value);
        } else {
            return executeSetAutoddl(!execution_context.set_state.autoddl);
        }
    } else if (set_option == "ECHO") {
        if (args.size() > 2) {
            return executeSetEcho(bool_value);
        } else {
            return executeSetEcho(!execution_context.set_state.echo);
        }
    } else if (set_option == "HEADING") {
        if (args.size() > 2) {
            return executeSetHeading(bool_value);
        } else {
            return executeSetHeading(!execution_context.set_state.heading);
        }
    } else if (set_option == "LIST") {
        if (args.size() > 2) {
            return executeSetList(bool_value);
        } else {
            return executeSetList(!execution_context.set_state.list);
        }
    } else if (set_option == "PAGESIZE") {
        if (args.size() > 2) {
            return executeSetPagesize(int_value);
        } else {
            result.error_message = "PAGESIZE requires a numeric value";
            return result;
        }
    } else if (set_option == "ROWCOUNT") {
        if (args.size() > 2) {
            return executeSetRowcount(bool_value);
        } else {
            return executeSetRowcount(!execution_context.set_state.rowcount);
        }
    } else if (set_option == "SQLDA_DISPLAY") {
        if (args.size() > 2) {
            return executeSetSqldaDisplay(bool_value);
        } else {
            return executeSetSqldaDisplay(!execution_context.set_state.sqlda_display);
        }
    } else if (set_option == "STATS") {
        if (args.size() > 2) {
            return executeSetStats(bool_value);
        } else {
            return executeSetStats(!execution_context.set_state.stats);
        }
    } else if (set_option == "TERM") {
        if (args.size() > 2) {
            return executeSetTerm(args[2]); // Use original case
        } else {
            result.error_message = "TERM requires a terminator value";
            return result;
        }
    } else if (set_option == "TIME") {
        if (args.size() > 2) {
            return executeSetTime(bool_value);
        } else {
            return executeSetTime(!execution_context.set_state.time);
        }
    } else if (set_option == "WARNINGS") {
        if (args.size() > 2) {
            return executeSetWarnings(bool_value);
        } else {
            return executeSetWarnings(!execution_context.set_state.warnings);
        }
    } else if (set_option == "NAMES") {
        if (args.size() > 2) {
            return executeSetNames(args[2]); // Use original case
        } else {
            result.error_message = "NAMES requires a character set name";
            return result;
        }
    } else if (set_option == "CHARSET") {
        if (args.size() > 2) {
            return executeSetCharset(args[2]); // Use original case
        } else {
            result.error_message = "CHARSET requires a character set name";
            return result;
        }
    } else if (set_option == "BAIL") {
        if (args.size() > 2) {
            return executeSetBail(bool_value);
        } else {
            return executeSetBail(!execution_context.set_state.bail);
        }
    } else if (set_option == "BULK_INSERT") {
        if (args.size() > 2) {
            return executeSetBulkInsert(bool_value);
        } else {
            return executeSetBulkInsert(!execution_context.set_state.bulk_insert);
        }
    } else if (set_option == "COUNT") {
        if (args.size() > 2) {
            return executeSetCount(bool_value);
        } else {
            return executeSetCount(!execution_context.set_state.count);
        }
    } else if (set_option == "PLAN") {
        if (args.size() > 2) {
            return executeSetPlan(bool_value);
        } else {
            return executeSetPlan(!execution_context.set_state.plan);
        }
    } else if (set_option == "PLANONLY") {
        if (args.size() > 2) {
            return executeSetPlanonly(bool_value);
        } else {
            return executeSetPlanonly(!execution_context.set_state.planonly);
        }
    } else if (set_option == "MAXROWS") {
        if (args.size() > 2) {
            return executeSetMaxrows(int_value);
        } else {
            result.error_message = "MAXROWS requires a numeric value";
            return result;
        }
    } else if (set_option == "SQLDIALECT") {
        if (args.size() > 2) {
            return executeSetSqldialect(int_value);
        } else {
            result.error_message = "SQLDIALECT requires a numeric value (1-3)";
            return result;
        }
    } else if (set_option == "TRANSACTION") {
        if (args.size() > 2) {
            return executeSetTransaction(args[2]); // Use original case
        } else {
            result.error_message = "TRANSACTION requires a transaction mode";
            return result;
        }
    } else if (set_option == "WIDTH") {
        if (args.size() > 2) {
            return executeSetWidth(int_value);
        } else {
            result.error_message = "WIDTH requires a numeric value";
            return result;
        }
    } else if (set_option == "CLIENTLIB") {
        if (args.size() > 2) {
            return executeSetClientlib(args[2]); // Use original case
        } else {
            result.error_message = "CLIENTLIB requires a library name";
            return result;
        }
    } else if (set_option == "DECFLOAT") {
        if (args.size() > 2) {
            return executeSetDecfloat(args[2]); // Use original case
        } else {
            result.error_message = "DECFLOAT requires a rounding mode";
            return result;
        }
    } else {
        result.error_message = "Unknown SET option: " + set_option;
        result.error_message += "\nAvailable options: AUTODDL, ECHO, HEADING, LIST, PAGESIZE, ROWCOUNT, SQLDA_DISPLAY, STATS, TERM, TIME, WARNINGS, NAMES, CHARSET, BAIL, BULK_INSERT, COUNT, PLAN, PLANONLY, MAXROWS, SQLDIALECT, TRANSACTION, WIDTH, CLIENTLIB, DECFLOAT";
        return result;
    }
}
CommandResult ISQLEnhanced::processExtractCommand(const std::vector<std::string>& args) { return CommandResult(); }
CommandResult ISQLEnhanced::processDescribeCommand(const std::vector<std::string>& args) { return CommandResult(); }
CommandResult ISQLEnhanced::processHelpCommand(const std::vector<std::string>& args) {
    return executeHelpCommand(args.size() > 1 ? args[1] : "");
}
CommandResult ISQLEnhanced::processTransactionCommand(const std::vector<std::string>& args) { return CommandResult(); }
CommandResult ISQLEnhanced::processScriptCommand(const std::vector<std::string>& args) { return CommandResult(); }

// SET command implementations
SBEnhanced::CommandResult ISQLEnhanced::executeSetAutoddl(bool value) {
    execution_context.set_state.autoddl = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "AUTODDL set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetEcho(bool value) {
    execution_context.set_state.echo = value;
    execution_context.echo_commands = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "ECHO set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetHeading(bool value) {
    execution_context.set_state.heading = value;
    execution_context.show_headers = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "HEADING set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetList(bool value) {
    execution_context.set_state.list = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "LIST set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetPagesize(int value) {
    if (value < 0 || value > 32767) {
        SBEnhanced::CommandResult result;
        result.success = false;
        result.error_message = "PAGESIZE must be between 0 and 32767";
        return result;
    }
    execution_context.set_state.pagesize = value;
    execution_context.page_size = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "PAGESIZE set to " + std::to_string(value);
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetRowcount(bool value) {
    execution_context.set_state.rowcount = value;
    execution_context.show_row_counts = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "ROWCOUNT set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetSqldaDisplay(bool value) {
    execution_context.set_state.sqlda_display = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "SQLDA_DISPLAY set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetStats(bool value) {
    execution_context.set_state.stats = value;
    execution_context.show_statistics = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "STATS set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetTerm(const std::string& value) {
    if (value.empty()) {
        SBEnhanced::CommandResult result;
        result.success = false;
        result.error_message = "TERM requires a terminator character";
        return result;
    }
    execution_context.set_state.term = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "TERM set to '" + value + "'";
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetTime(bool value) {
    execution_context.set_state.time = value;
    execution_context.show_timing = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "TIME set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetWarnings(bool value) {
    execution_context.set_state.warnings = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "WARNINGS set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetNames(const std::string& value) {
    execution_context.set_state.names = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "NAMES set to '" + value + "'";
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetCharset(const std::string& value) {
    execution_context.set_state.charset = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "CHARSET set to '" + value + "'";
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetBail(bool value) {
    execution_context.set_state.bail = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "BAIL set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetBulkInsert(bool value) {
    execution_context.set_state.bulk_insert = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "BULK_INSERT set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetCount(bool value) {
    execution_context.set_state.count = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "COUNT set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetPlan(bool value) {
    execution_context.set_state.plan = value;
    execution_context.enable_explain = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "PLAN set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetPlanonly(bool value) {
    execution_context.set_state.planonly = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "PLANONLY set to " + std::string(value ? "ON" : "OFF");
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetMaxrows(int value) {
    if (value < 0) {
        SBEnhanced::CommandResult result;
        result.success = false;
        result.error_message = "MAXROWS must be non-negative";
        return result;
    }
    execution_context.set_state.maxrows = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "MAXROWS set to " + std::to_string(value);
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetSqldialect(int value) {
    if (value < 1 || value > 3) {
        SBEnhanced::CommandResult result;
        result.success = false;
        result.error_message = "SQL_DIALECT must be between 1 and 3";
        return result;
    }
    execution_context.set_state.sqldialect = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "SQL_DIALECT set to " + std::to_string(value);
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetTransaction(const std::string& value) {
    execution_context.set_state.transaction = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "TRANSACTION set to '" + value + "'";
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetWidth(int value) {
    if (value < 10 || value > 32767) {
        SBEnhanced::CommandResult result;
        result.success = false;
        result.error_message = "WIDTH must be between 10 and 32767";
        return result;
    }
    execution_context.set_state.width = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "WIDTH set to " + std::to_string(value);
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetClientlib(const std::string& value) {
    execution_context.set_state.clientlib = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "CLIENTLIB set to '" + value + "'";
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeSetDecfloat(const std::string& value) {
    execution_context.set_state.decfloat = value;
    SBEnhanced::CommandResult result;
    result.success = true;
    result.message = "DECFLOAT set to '" + value + "'";
    return result;
}

// INPUT/OUTPUT command implementations
SBEnhanced::CommandResult ISQLEnhanced::executeInputCommand(const std::string& filename)
{
    SBEnhanced::CommandResult result;
    
    try {
        if (filename.empty()) {
            result.error_message = "INPUT command requires a filename";
            return result;
        }
        
        // Check if we're already processing nested INPUT files
        if (execution_context.input_stack.size() >= 10) {
            result.error_message = "Maximum INPUT nesting level (10) exceeded";
            return result;
        }
        
        // Open the input file
        std::ifstream input_file(filename);
        if (!input_file.is_open()) {
            result.error_message = "Cannot open input file: " + filename;
            return result;
        }
        
        // Add current file to input stack
        execution_context.input_stack.push_back(filename);
        
        // Process the file line by line
        std::string line;
        std::string accumulated_command;
        int line_number = 0;
        int successful_commands = 0;
        int failed_commands = 0;
        
        while (std::getline(input_file, line)) {
            line_number++;
            
            // Skip empty lines and comments
            std::string trimmed_line = line;
            size_t start = trimmed_line.find_first_not_of(" \t");
            if (start != std::string::npos) {
                trimmed_line = trimmed_line.substr(start);
            }
            
            if (trimmed_line.empty() || trimmed_line[0] == '#' || 
                trimmed_line.substr(0, 2) == "--") {
                continue;
            }
            
            // Add line to accumulated command
            if (!accumulated_command.empty()) {
                accumulated_command += "\n";
            }
            accumulated_command += line;
            
            // Check if command is complete (ends with terminator)
            if (isCompleteStatement(accumulated_command)) {
                // Remove the terminator
                if (!accumulated_command.empty() && 
                    accumulated_command.back() == execution_context.set_state.term[0]) {
                    accumulated_command.pop_back();
                }
                
                // Echo command if enabled
                if (execution_context.echo_commands) {
                    *output_stream << accumulated_command << std::endl;
                }
                
                // Execute the command
                CommandResult cmd_result = executeCommand(accumulated_command);
                
                if (cmd_result.success) {
                    successful_commands++;
                    
                    // Display result if not empty
                    if (!cmd_result.output_lines.empty()) {
                        for (const auto& output_line : cmd_result.output_lines) {
                            *output_stream << output_line << std::endl;
                        }
                    }
                    
                    if (!cmd_result.message.empty()) {
                        *output_stream << cmd_result.message << std::endl;
                    }
                } else {
                    failed_commands++;
                    *error_stream << "Error at line " << line_number << " in file " << filename 
                                  << ": " << cmd_result.error_message << std::endl;
                    
                    // Check if we should bail on error
                    if (execution_context.set_state.bail && !session_state.interactive_mode) {
                        input_file.close();
                        execution_context.input_stack.pop_back();
                        result.error_message = "Execution stopped due to error (BAIL is ON)";
                        return result;
                    }
                }
                
                // Clear accumulated command for next statement
                accumulated_command.clear();
            }
        }
        
        // Handle incomplete command at end of file
        if (!accumulated_command.empty()) {
            *error_stream << "Warning: Incomplete command at end of file " << filename << std::endl;
        }
        
        input_file.close();
        execution_context.input_stack.pop_back();
        
        result.success = true;
        result.message = "INPUT file processed: " + filename + 
                        " (Commands: " + std::to_string(successful_commands + failed_commands) + 
                        ", Success: " + std::to_string(successful_commands) + 
                        ", Failed: " + std::to_string(failed_commands) + ")";
        
        return result;
    }
    catch (const std::exception& e) {
        // Clean up input stack on error
        if (!execution_context.input_stack.empty()) {
            execution_context.input_stack.pop_back();
        }
        
        result.error_message = std::string("INPUT command failed: ") + e.what();
        return result;
    }
}

SBEnhanced::CommandResult ISQLEnhanced::executeOutputCommand(const std::string& filename)
{
    SBEnhanced::CommandResult result;
    
    try {
        if (filename.empty()) {
            // Reset output to stdout
            if (output_file && output_file->is_open()) {
                output_file->close();
                output_file.reset();
            }
            
            output_stream = &std::cout;
            execution_context.output_redirect.clear();
            
            result.success = true;
            result.message = "Output redirected to console";
            return result;
        }
        
        // Close current output file if open
        if (output_file && output_file->is_open()) {
            output_file->close();
        }
        
        // Open new output file
        output_file = std::make_unique<std::ofstream>(filename, std::ios::out | std::ios::trunc);
        if (!output_file->is_open()) {
            result.error_message = "Cannot open output file: " + filename;
            return result;
        }
        
        // Redirect output stream
        output_stream = output_file.get();
        execution_context.output_redirect = filename;
        
        result.success = true;
        result.message = "Output redirected to file: " + filename;
        
        // Print this message to stderr so user sees it even with output redirected
        *error_stream << result.message << std::endl;
        
        return result;
    }
    catch (const std::exception& e) {
        result.error_message = std::string("OUTPUT command failed: ") + e.what();
        return result;
    }
}

// HELP command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeHelpCommand(const std::string& topic)
{
    SBEnhanced::CommandResult result;
    result.success = true;
    
    std::string help_topic = topic;
    std::transform(help_topic.begin(), help_topic.end(), help_topic.begin(), ::toupper);
    
    if (help_topic.empty()) {
        // General help
        result.output_lines.push_back("ScratchBird Enhanced ISQL v0.6.0 - Command Help");
        result.output_lines.push_back("=================================================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Available Commands:");
        result.output_lines.push_back("");
        result.output_lines.push_back("Connection Commands:");
        result.output_lines.push_back("  CONNECT database [username [password [role]]]");
        result.output_lines.push_back("  DISCONNECT");
        result.output_lines.push_back("");
        result.output_lines.push_back("Query Commands:");
        result.output_lines.push_back("  SELECT, INSERT, UPDATE, DELETE, CREATE, ALTER, DROP");
        result.output_lines.push_back("  EXPLAIN statement");
        result.output_lines.push_back("");
        result.output_lines.push_back("Information Commands:");
        result.output_lines.push_back("  SHOW {TABLES|VIEWS|PROCEDURES|FUNCTIONS|SCHEMAS|...}");
        result.output_lines.push_back("  DESCRIBE table_name");
        result.output_lines.push_back("  EXTRACT [options]");
        result.output_lines.push_back("");
        result.output_lines.push_back("Configuration Commands:");
        result.output_lines.push_back("  SET option [value]");
        result.output_lines.push_back("  SET (shows all settings)");
        result.output_lines.push_back("");
        result.output_lines.push_back("File Commands:");
        result.output_lines.push_back("  INPUT filename");
        result.output_lines.push_back("  OUTPUT [filename]");
        result.output_lines.push_back("  EDIT [filename]");
        result.output_lines.push_back("");
        result.output_lines.push_back("System Commands:");
        result.output_lines.push_back("  SHELL [command]");
        result.output_lines.push_back("  EXIT, QUIT");
        result.output_lines.push_back("");
        result.output_lines.push_back("Transaction Commands:");
        result.output_lines.push_back("  BEGIN, COMMIT, ROLLBACK");
        result.output_lines.push_back("");
        result.output_lines.push_back("Data Entry Commands:");
        result.output_lines.push_back("  ADD table_name");
        result.output_lines.push_back("  COPY source_table destination_table");
        result.output_lines.push_back("");
        result.output_lines.push_back("BLOB Commands:");
        result.output_lines.push_back("  BLOBDUMP blob_id [filename]");
        result.output_lines.push_back("  BLOBVIEW blob_id");
        result.output_lines.push_back("");
        result.output_lines.push_back("Type 'HELP command_name' for detailed help on a specific command.");
        result.output_lines.push_back("Type 'HELP SET' for SET command options.");
        result.output_lines.push_back("Type 'HELP SHOW' for SHOW command options.");
        
    } else if (help_topic == "SET") {
        result.output_lines.push_back("SET Command Help");
        result.output_lines.push_back("================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage: SET [option [value]]");
        result.output_lines.push_back("");
        result.output_lines.push_back("Available SET options:");
        result.output_lines.push_back("");
        result.output_lines.push_back("  AUTODDL {ON|OFF}        - Auto-commit DDL statements");
        result.output_lines.push_back("  ECHO {ON|OFF}           - Echo commands to output");
        result.output_lines.push_back("  HEADING {ON|OFF}        - Show column headers");
        result.output_lines.push_back("  LIST {ON|OFF}           - List format output");
        result.output_lines.push_back("  PAGESIZE number         - Page size for output (0-32767)");
        result.output_lines.push_back("  ROWCOUNT {ON|OFF}       - Show row counts");
        result.output_lines.push_back("  SQLDA_DISPLAY {ON|OFF}  - Show SQLDA information");
        result.output_lines.push_back("  STATS {ON|OFF}          - Show statement statistics");
        result.output_lines.push_back("  TERM character          - Statement terminator");
        result.output_lines.push_back("  TIME {ON|OFF}           - Show execution timing");
        result.output_lines.push_back("  WARNINGS {ON|OFF}       - Show warnings");
        result.output_lines.push_back("  NAMES charset           - Character set for names");
        result.output_lines.push_back("  CHARSET charset         - Connection character set");
        result.output_lines.push_back("  BAIL {ON|OFF}           - Exit on first error");
        result.output_lines.push_back("  BULK_INSERT {ON|OFF}    - Bulk insert mode");
        result.output_lines.push_back("  COUNT {ON|OFF}          - Count affected rows");
        result.output_lines.push_back("  PLAN {ON|OFF}           - Show execution plan");
        result.output_lines.push_back("  PLANONLY {ON|OFF}       - Show plan only (don't execute)");
        result.output_lines.push_back("  MAXROWS number          - Maximum rows to fetch (0 = unlimited)");
        result.output_lines.push_back("  SQLDIALECT {1|2|3}      - SQL dialect");
        result.output_lines.push_back("  TRANSACTION mode        - Transaction isolation level");
        result.output_lines.push_back("  WIDTH number            - Output width (10-32767)");
        result.output_lines.push_back("  CLIENTLIB library       - Client library name");
        result.output_lines.push_back("  DECFLOAT mode          - DECFLOAT rounding mode");
        result.output_lines.push_back("");
        result.output_lines.push_back("Examples:");
        result.output_lines.push_back("  SET ECHO ON");
        result.output_lines.push_back("  SET PAGESIZE 50");
        result.output_lines.push_back("  SET TERM ^");
        result.output_lines.push_back("  SET                     (shows all current settings)");
        
    } else if (help_topic == "SHOW") {
        result.output_lines.push_back("SHOW Command Help");
        result.output_lines.push_back("=================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage: SHOW object_type [object_name]");
        result.output_lines.push_back("");
        result.output_lines.push_back("Available SHOW commands:");
        result.output_lines.push_back("");
        result.output_lines.push_back("Database Objects:");
        result.output_lines.push_back("  SHOW TABLES [pattern]        - Show tables");
        result.output_lines.push_back("  SHOW VIEWS [pattern]         - Show views");
        result.output_lines.push_back("  SHOW PROCEDURES [pattern]    - Show procedures");
        result.output_lines.push_back("  SHOW FUNCTIONS [pattern]     - Show functions");
        result.output_lines.push_back("  SHOW TRIGGERS [pattern]      - Show triggers");
        result.output_lines.push_back("  SHOW INDICES [pattern]       - Show indexes");
        result.output_lines.push_back("  SHOW DOMAINS [pattern]       - Show domains");
        result.output_lines.push_back("  SHOW GENERATORS [pattern]    - Show generators");
        result.output_lines.push_back("  SHOW SCHEMAS [pattern]       - Show schemas");
        result.output_lines.push_back("  SHOW PACKAGES [pattern]      - Show packages");
        result.output_lines.push_back("");
        result.output_lines.push_back("Security:");
        result.output_lines.push_back("  SHOW ROLES [pattern]         - Show roles");
        result.output_lines.push_back("  SHOW USERS [pattern]         - Show users");
        result.output_lines.push_back("  SHOW GRANTS [pattern]        - Show grants");
        result.output_lines.push_back("  SHOW PRIVILEGES [pattern]    - Show privileges");
        result.output_lines.push_back("");
        result.output_lines.push_back("System Information:");
        result.output_lines.push_back("  SHOW DATABASE                - Show database info");
        result.output_lines.push_back("  SHOW VERSION                 - Show version");
        result.output_lines.push_back("  SHOW SYSTEM                  - Show system objects");
        result.output_lines.push_back("  SHOW COLLATIONS [pattern]    - Show collations");
        result.output_lines.push_back("  SHOW EXCEPTIONS [pattern]    - Show exceptions");
        result.output_lines.push_back("  SHOW FILTERS [pattern]       - Show filters");
        result.output_lines.push_back("");
        result.output_lines.push_back("Current State:");
        result.output_lines.push_back("  SHOW SCHEMA                  - Show current schema");
        result.output_lines.push_back("  SHOW HOME SCHEMA             - Show home schema");
        result.output_lines.push_back("  SHOW CURRENT SCHEMA          - Show current schema");
        result.output_lines.push_back("  SHOW CURRENT ROLE            - Show current role");
        result.output_lines.push_back("  SHOW CURRENT USER            - Show current user");
        result.output_lines.push_back("  SHOW TIME                    - Show current time");
        result.output_lines.push_back("  SHOW SESSION                 - Show session info");
        result.output_lines.push_back("");
        result.output_lines.push_back("Monitoring:");
        result.output_lines.push_back("  SHOW CONNECTIONS             - Show connections");
        result.output_lines.push_back("  SHOW TRANSACTIONS            - Show transactions");
        result.output_lines.push_back("  SHOW STATISTICS              - Show statistics");
        result.output_lines.push_back("  SHOW WIRE_STATISTICS          - Show wire statistics");
        result.output_lines.push_back("  SHOW DEPENDENCIES object     - Show dependencies");
        
    } else if (help_topic == "INPUT") {
        result.output_lines.push_back("INPUT Command Help");
        result.output_lines.push_back("==================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage: INPUT filename");
        result.output_lines.push_back("");
        result.output_lines.push_back("The INPUT command reads and executes commands from a file.");
        result.output_lines.push_back("");
        result.output_lines.push_back("Features:");
        result.output_lines.push_back("  - Supports nested INPUT files (up to 10 levels)");
        result.output_lines.push_back("  - Respects SET ECHO setting for command echoing");
        result.output_lines.push_back("  - Respects SET BAIL setting for error handling");
        result.output_lines.push_back("  - Provides execution statistics");
        result.output_lines.push_back("  - Supports multi-line statements");
        result.output_lines.push_back("  - Skips empty lines and comments");
        result.output_lines.push_back("");
        result.output_lines.push_back("Examples:");
        result.output_lines.push_back("  INPUT schema.sql");
        result.output_lines.push_back("  INPUT /path/to/script.sql");
        result.output_lines.push_back("  INPUT \"file with spaces.sql\"");
        
    } else if (help_topic == "OUTPUT") {
        result.output_lines.push_back("OUTPUT Command Help");
        result.output_lines.push_back("===================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage: OUTPUT [filename]");
        result.output_lines.push_back("");
        result.output_lines.push_back("The OUTPUT command redirects output to a file or console.");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage:");
        result.output_lines.push_back("  OUTPUT filename          - Redirect output to file");
        result.output_lines.push_back("  OUTPUT                   - Reset output to console");
        result.output_lines.push_back("");
        result.output_lines.push_back("Features:");
        result.output_lines.push_back("  - Overwrites existing files");
        result.output_lines.push_back("  - Only redirects query results and command output");
        result.output_lines.push_back("  - Error messages still go to stderr");
        result.output_lines.push_back("  - Status messages shown on stderr when redirecting");
        result.output_lines.push_back("");
        result.output_lines.push_back("Examples:");
        result.output_lines.push_back("  OUTPUT results.txt");
        result.output_lines.push_back("  OUTPUT /tmp/query_output.log");
        result.output_lines.push_back("  OUTPUT                   (reset to console)");
        
    } else if (help_topic == "CONNECT") {
        result.output_lines.push_back("CONNECT Command Help");
        result.output_lines.push_back("====================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage: CONNECT database [username [password [role]]]");
        result.output_lines.push_back("");
        result.output_lines.push_back("The CONNECT command establishes a connection to a database.");
        result.output_lines.push_back("");
        result.output_lines.push_back("Parameters:");
        result.output_lines.push_back("  database                 - Database path or connection string");
        result.output_lines.push_back("  username                 - Username (defaults to SYSDBA)");
        result.output_lines.push_back("  password                 - Password (defaults to masterkey)");
        result.output_lines.push_back("  role                     - SQL role name (optional)");
        result.output_lines.push_back("");
        result.output_lines.push_back("Examples:");
        result.output_lines.push_back("  CONNECT mydb.fdb");
        result.output_lines.push_back("  CONNECT mydb.fdb SYSDBA masterkey");
        result.output_lines.push_back("  CONNECT mydb.fdb SYSDBA masterkey ADMIN");
        result.output_lines.push_back("  CONNECT localhost:mydb.fdb");
        result.output_lines.push_back("  CONNECT \"C:\\Database\\mydb.fdb\"");
        
    } else if (help_topic == "EXPLAIN") {
        result.output_lines.push_back("EXPLAIN Command Help");
        result.output_lines.push_back("====================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Usage: EXPLAIN statement");
        result.output_lines.push_back("");
        result.output_lines.push_back("The EXPLAIN command shows the execution plan for a statement");
        result.output_lines.push_back("without executing it.");
        result.output_lines.push_back("");
        result.output_lines.push_back("Features:");
        result.output_lines.push_back("  - Shows query execution plan");
        result.output_lines.push_back("  - Displays optimization information");
        result.output_lines.push_back("  - Provides performance hints");
        result.output_lines.push_back("  - Works with SELECT, INSERT, UPDATE, DELETE");
        result.output_lines.push_back("");
        result.output_lines.push_back("Examples:");
        result.output_lines.push_back("  EXPLAIN SELECT * FROM employees WHERE id = 1");
        result.output_lines.push_back("  EXPLAIN UPDATE employees SET salary = salary * 1.1");
        result.output_lines.push_back("  EXPLAIN DELETE FROM temp_table WHERE date < '2023-01-01'");
        
    } else {
        result.output_lines.push_back("Help topic '" + topic + "' not found.");
        result.output_lines.push_back("");
        result.output_lines.push_back("Available help topics:");
        result.output_lines.push_back("  SET, SHOW, INPUT, OUTPUT, CONNECT, EXPLAIN");
        result.output_lines.push_back("  DESCRIBE, EXTRACT, SHELL, EDIT, ADD, COPY");
        result.output_lines.push_back("  BLOBDUMP, BLOBVIEW");
        result.output_lines.push_back("");
        result.output_lines.push_back("Type 'HELP' for general command help.");
    }
    
    return result;
}

// EXPLAIN command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeExplainCommand(const std::string& sql) {
    SBEnhanced::CommandResult result;
    
    if (sql.empty()) {
        result.success = false;
        result.error_message = "EXPLAIN requires a SQL statement";
        return result;
    }
    
    if (!engine || !engine->isConnected()) {
        result.success = false;
        result.error_message = "Not connected to database";
        return result;
    }
    
    try {
        auto start_time = std::chrono::steady_clock::now();
        
        // Use the engine's query analysis capability
        SBEnhanced::QueryPlan plan;
        if (analyzer && analyzer->generateExecutionPlan(sql, plan)) {
            result.success = true;
            
            // Format the execution plan
            result.output_lines.push_back("EXECUTION PLAN");
            result.output_lines.push_back("==============");
            result.output_lines.push_back("");
            
            // Show plan details
            if (!plan.plan_text.empty()) {
                result.output_lines.push_back("Plan:");
                std::istringstream plan_stream(plan.plan_text);
                std::string line;
                while (std::getline(plan_stream, line)) {
                    result.output_lines.push_back("  " + line);
                }
                result.output_lines.push_back("");
            }
            
            // Show optimization info
            if (!plan.optimization_info.empty()) {
                result.output_lines.push_back("Optimization Information:");
                for (const auto& info : plan.optimization_info) {
                    result.output_lines.push_back("  " + info);
                }
                result.output_lines.push_back("");
            }
            
            // Show table access info
            if (!plan.table_access.empty()) {
                result.output_lines.push_back("Table Access:");
                for (const auto& access : plan.table_access) {
                    result.output_lines.push_back("  " + access.table_name + " (" + access.access_method + ")");
                    if (access.estimated_rows > 0) {
                        result.output_lines.push_back("    Estimated rows: " + std::to_string(access.estimated_rows));
                    }
                    if (access.estimated_cost > 0.0) {
                        result.output_lines.push_back("    Estimated cost: " + std::to_string(access.estimated_cost));
                    }
                }
                result.output_lines.push_back("");
            }
            
            // Show index usage
            if (!plan.index_usage.empty()) {
                result.output_lines.push_back("Index Usage:");
                for (const auto& index : plan.index_usage) {
                    result.output_lines.push_back("  " + index.index_name + " on " + index.table_name);
                    result.output_lines.push_back("    Selectivity: " + std::to_string(index.selectivity));
                }
                result.output_lines.push_back("");
            }
            
            // Show join information
            if (!plan.join_info.empty()) {
                result.output_lines.push_back("Join Information:");
                for (const auto& join : plan.join_info) {
                    result.output_lines.push_back("  " + join.join_type + " join between " + 
                                                join.left_table + " and " + join.right_table);
                    if (!join.join_condition.empty()) {
                        result.output_lines.push_back("    Condition: " + join.join_condition);
                    }
                }
                result.output_lines.push_back("");
            }
            
            // Show performance estimates
            if (plan.estimated_execution_time > 0.0) {
                result.output_lines.push_back("Performance Estimates:");
                result.output_lines.push_back("  Estimated execution time: " + 
                                            std::to_string(plan.estimated_execution_time) + " ms");
                result.output_lines.push_back("  Estimated memory usage: " + 
                                            std::to_string(plan.estimated_memory_usage) + " bytes");
                result.output_lines.push_back("");
            }
            
            // Show optimization hints
            if (!plan.optimization_hints.empty()) {
                result.output_lines.push_back("Optimization Hints:");
                for (const auto& hint : plan.optimization_hints) {
                    result.output_lines.push_back("  " + hint);
                }
                result.output_lines.push_back("");
            }
            
            auto end_time = std::chrono::steady_clock::now();
            result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            
            if (execution_context.set_state.time) {
                result.output_lines.push_back("Plan generation time: " + 
                                            formatElapsedTime(result.execution_time));
            }
            
        } else {
            // Fallback to basic plan generation if analyzer is not available
            result.success = true;
            result.output_lines.push_back("EXECUTION PLAN");
            result.output_lines.push_back("==============");
            result.output_lines.push_back("");
            
            // Try to get plan using SET PLAN ON approach
            if (engine->executeQuery("SET PLAN ON")) {
                SBEnhanced::QueryResults query_results;
                if (engine->executeQuery(sql, query_results)) {
                    result.output_lines.push_back("Plan extracted from query execution:");
                    result.output_lines.push_back("  " + query_results.plan_text);
                } else {
                    result.output_lines.push_back("Unable to generate execution plan:");
                    result.output_lines.push_back("  " + engine->getLastError());
                }
                engine->executeQuery("SET PLAN OFF");  // Reset
            } else {
                result.output_lines.push_back("Plan generation not available");
                result.output_lines.push_back("Check that the SQL statement is valid and the database is accessible.");
            }
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error generating execution plan: " + std::string(e.what());
    }
    
    return result;
}

// EDIT command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeEditCommand(const std::string& filename) {
    SBEnhanced::CommandResult result;
    
    try {
        std::string file_to_edit = filename;
        
        // If no filename provided, use a temporary file
        if (file_to_edit.empty()) {
            file_to_edit = "/tmp/isql_edit_" + std::to_string(getpid()) + ".sql";
            
            // Create temporary file with current command history or last query
            std::ofstream temp_file(file_to_edit);
            if (temp_file.is_open()) {
                if (!execution_context.command_history.empty()) {
                    // Add last few commands to temporary file
                    temp_file << "-- Last commands from session:\n";
                    int count = 0;
                    for (auto it = execution_context.command_history.rbegin(); 
                         it != execution_context.command_history.rend() && count < 5; 
                         ++it, ++count) {
                        temp_file << "-- " << *it << "\n";
                    }
                    temp_file << "\n-- Enter your SQL commands below:\n";
                }
                temp_file.close();
            }
        }
        
        // Check if file exists or can be created
        if (!filename.empty()) {
            std::ifstream check_file(file_to_edit);
            if (!check_file.good()) {
                // Try to create the file
                std::ofstream create_file(file_to_edit);
                if (!create_file.is_open()) {
                    result.success = false;
                    result.error_message = "Cannot create or access file: " + file_to_edit;
                    return result;
                }
                create_file.close();
            }
        }
        
        // Get editor command from context
        std::string editor_cmd = execution_context.editor_command;
        
        // Check if editor is available
        std::string check_editor = "which " + editor_cmd + " > /dev/null 2>&1";
        if (system(check_editor.c_str()) != 0) {
            // Try fallback editors
            std::vector<std::string> fallback_editors = {"nano", "vim", "emacs", "gedit"};
            bool found_editor = false;
            
            for (const auto& editor : fallback_editors) {
                std::string check_cmd = "which " + editor + " > /dev/null 2>&1";
                if (system(check_cmd.c_str()) == 0) {
                    editor_cmd = editor;
                    found_editor = true;
                    break;
                }
            }
            
            if (!found_editor) {
                result.success = false;
                result.error_message = "No suitable editor found. Please set editor with SET EDITOR command.";
                return result;
            }
        }
        
        // Store original modification time
        struct stat file_stat_before;
        bool file_existed = (stat(file_to_edit.c_str(), &file_stat_before) == 0);
        time_t mod_time_before = file_existed ? file_stat_before.st_mtime : 0;
        
        // Launch editor
        std::string edit_command = editor_cmd + " " + file_to_edit;
        
        result.output_lines.push_back("Launching editor: " + editor_cmd);
        result.output_lines.push_back("File: " + file_to_edit);
        result.output_lines.push_back("");
        
        // Execute the editor
        int edit_result = system(edit_command.c_str());
        
        if (edit_result == 0) {
            // Check if file was modified
            struct stat file_stat_after;
            if (stat(file_to_edit.c_str(), &file_stat_after) == 0) {
                time_t mod_time_after = file_stat_after.st_mtime;
                
                if (mod_time_after > mod_time_before) {
                    result.output_lines.push_back("File was modified.");
                    
                    // Ask user if they want to execute the file
                    std::cout << "Execute the edited file? (y/n): ";
                    std::string response;
                    std::getline(std::cin, response);
                    
                    if (response == "y" || response == "Y" || response == "yes") {
                        // Execute the file using INPUT command
                        SBEnhanced::CommandResult input_result = executeInputCommand(file_to_edit);
                        
                        // Copy results to main result
                        result.output_lines.insert(result.output_lines.end(), 
                                                  input_result.output_lines.begin(), 
                                                  input_result.output_lines.end());
                        
                        if (input_result.success) {
                            result.output_lines.push_back("File executed successfully.");
                        } else {
                            result.output_lines.push_back("File execution failed: " + input_result.error_message);
                        }
                    }
                } else {
                    result.output_lines.push_back("File was not modified.");
                }
            }
            
            // Clean up temporary file
            if (filename.empty()) {
                if (remove(file_to_edit.c_str()) == 0) {
                    result.output_lines.push_back("Temporary file cleaned up.");
                }
            }
            
            result.success = true;
            
        } else {
            result.success = false;
            result.error_message = "Editor exited with error code: " + std::to_string(edit_result);
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error in EDIT command: " + std::string(e.what());
    }
    
    return result;
}

// SHELL command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeShellCommand(const std::string& command) {
    SBEnhanced::CommandResult result;
    
    try {
        std::string shell_cmd = command;
        
        // If no command provided, start interactive shell
        if (shell_cmd.empty()) {
            shell_cmd = execution_context.shell_command;
            
            result.output_lines.push_back("Starting interactive shell...");
            result.output_lines.push_back("Type 'exit' to return to ISQL");
            result.output_lines.push_back("");
        }
        
        // Check if shell is available
        std::string check_shell = "which " + (shell_cmd.empty() ? "sh" : shell_cmd) + " > /dev/null 2>&1";
        if (system(check_shell.c_str()) != 0) {
            // Try fallback shells
            std::vector<std::string> fallback_shells = {"bash", "sh", "zsh", "dash"};
            bool found_shell = false;
            
            for (const auto& shell : fallback_shells) {
                std::string check_cmd = "which " + shell + " > /dev/null 2>&1";
                if (system(check_cmd.c_str()) == 0) {
                    shell_cmd = shell;
                    found_shell = true;
                    break;
                }
            }
            
            if (!found_shell) {
                result.success = false;
                result.error_message = "No suitable shell found.";
                return result;
            }
        }
        
        // Execute the shell command
        auto start_time = std::chrono::steady_clock::now();
        
        int shell_result;
        if (command.empty()) {
            // Interactive shell
            shell_result = system(shell_cmd.c_str());
        } else {
            // Single command execution
            shell_result = system(shell_cmd.c_str());
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (shell_result == 0) {
            result.success = true;
            
            if (command.empty()) {
                result.output_lines.push_back("Shell session completed.");
            } else {
                result.output_lines.push_back("Command executed successfully.");
            }
            
            if (execution_context.set_state.time) {
                result.output_lines.push_back("Execution time: " + formatElapsedTime(result.execution_time));
            }
            
        } else {
            result.success = false;
            result.error_message = "Shell command failed with exit code: " + std::to_string(shell_result);
            
            // Provide more detailed error information
            if (WIFEXITED(shell_result)) {
                int exit_code = WEXITSTATUS(shell_result);
                result.error_message += " (exit code: " + std::to_string(exit_code) + ")";
            } else if (WIFSIGNALED(shell_result)) {
                int signal = WTERMSIG(shell_result);
                result.error_message += " (terminated by signal: " + std::to_string(signal) + ")";
            }
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error in SHELL command: " + std::string(e.what());
    }
    
    return result;
}

// BLOBDUMP command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeBlobDumpCommand(const std::vector<std::string>& args) {
    SBEnhanced::CommandResult result;
    
    if (args.size() < 2) {
        result.success = false;
        result.error_message = "BLOBDUMP requires blob_id and optional filename";
        result.output_lines.push_back("Usage: BLOBDUMP blob_id [filename]");
        return result;
    }
    
    if (!engine || !engine->isConnected()) {
        result.success = false;
        result.error_message = "Not connected to database";
        return result;
    }
    
    try {
        std::string blob_id_str = args[1];
        std::string filename = args.size() > 2 ? args[2] : "";
        
        // Parse blob ID (format: segment:offset or just integer)
        uint64_t blob_id = 0;
        
        if (blob_id_str.find(':') != std::string::npos) {
            // Format: segment:offset
            size_t colon_pos = blob_id_str.find(':');
            uint32_t segment = std::stoul(blob_id_str.substr(0, colon_pos));
            uint32_t offset = std::stoul(blob_id_str.substr(colon_pos + 1));
            blob_id = (static_cast<uint64_t>(segment) << 32) | offset;
        } else {
            // Simple integer format
            blob_id = std::stoull(blob_id_str);
        }
        
        // Generate filename if not provided
        if (filename.empty()) {
            filename = execution_context.blob_dump_directory + "/blob_" + 
                      std::to_string(blob_id) + ".bin";
        }
        
        // Ensure dump directory exists
        std::string dump_dir = execution_context.blob_dump_directory;
        if (system(("mkdir -p " + dump_dir).c_str()) != 0) {
            result.success = false;
            result.error_message = "Failed to create dump directory: " + dump_dir;
            return result;
        }
        
        // Use engine to dump blob
        std::vector<unsigned char> blob_data;
        if (engine->readBlobData(blob_id, blob_data)) {
            // Write blob data to file
            std::ofstream blob_file(filename, std::ios::binary);
            if (blob_file.is_open()) {
                blob_file.write(reinterpret_cast<const char*>(blob_data.data()), blob_data.size());
                blob_file.close();
                
                result.success = true;
                result.output_lines.push_back("BLOB " + blob_id_str + " dumped to: " + filename);
                result.output_lines.push_back("Size: " + std::to_string(blob_data.size()) + " bytes");
                
                // Show file information
                struct stat file_stat;
                if (stat(filename.c_str(), &file_stat) == 0) {
                    result.output_lines.push_back("File size: " + std::to_string(file_stat.st_size) + " bytes");
                    result.output_lines.push_back("File permissions: " + std::to_string(file_stat.st_mode & 0777));
                }
                
            } else {
                result.success = false;
                result.error_message = "Failed to open output file: " + filename;
            }
        } else {
            result.success = false;
            result.error_message = "Failed to read BLOB data: " + engine->getLastError();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error in BLOBDUMP command: " + std::string(e.what());
    }
    
    return result;
}

// BLOBVIEW command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeBlobViewCommand(const std::vector<std::string>& args) {
    SBEnhanced::CommandResult result;
    
    if (args.size() < 2) {
        result.success = false;
        result.error_message = "BLOBVIEW requires blob_id";
        result.output_lines.push_back("Usage: BLOBVIEW blob_id [format]");
        result.output_lines.push_back("Formats: hex, text, binary (default: hex)");
        return result;
    }
    
    if (!engine || !engine->isConnected()) {
        result.success = false;
        result.error_message = "Not connected to database";
        return result;
    }
    
    try {
        std::string blob_id_str = args[1];
        std::string format = args.size() > 2 ? args[2] : "hex";
        
        // Parse blob ID
        uint64_t blob_id = 0;
        if (blob_id_str.find(':') != std::string::npos) {
            size_t colon_pos = blob_id_str.find(':');
            uint32_t segment = std::stoul(blob_id_str.substr(0, colon_pos));
            uint32_t offset = std::stoul(blob_id_str.substr(colon_pos + 1));
            blob_id = (static_cast<uint64_t>(segment) << 32) | offset;
        } else {
            blob_id = std::stoull(blob_id_str);
        }
        
        // Read blob data
        std::vector<unsigned char> blob_data;
        if (engine->readBlobData(blob_id, blob_data)) {
            result.success = true;
            
            result.output_lines.push_back("BLOB " + blob_id_str + " Contents");
            result.output_lines.push_back("========================");
            result.output_lines.push_back("Size: " + std::to_string(blob_data.size()) + " bytes");
            result.output_lines.push_back("Format: " + format);
            result.output_lines.push_back("");
            
            if (format == "hex") {
                // Hexadecimal format with ASCII representation
                for (size_t i = 0; i < blob_data.size(); i += 16) {
                    std::ostringstream hex_line, ascii_line;
                    
                    // Offset
                    hex_line << std::hex << std::setfill('0') << std::setw(8) << i << ": ";
                    
                    // Hex bytes
                    for (size_t j = 0; j < 16 && i + j < blob_data.size(); j++) {
                        hex_line << std::hex << std::setfill('0') << std::setw(2) 
                                << static_cast<int>(blob_data[i + j]) << " ";
                        
                        // ASCII representation
                        char c = static_cast<char>(blob_data[i + j]);
                        ascii_line << (std::isprint(c) ? c : '.');
                    }
                    
                    // Padding for incomplete lines
                    for (size_t j = blob_data.size() - i; j < 16; j++) {
                        hex_line << "   ";
                    }
                    
                    result.output_lines.push_back(hex_line.str() + " |" + ascii_line.str() + "|");
                }
                
            } else if (format == "text") {
                // Text format (with control character handling)
                std::ostringstream text_stream;
                for (unsigned char byte : blob_data) {
                    if (std::isprint(byte)) {
                        text_stream << static_cast<char>(byte);
                    } else if (byte == '\n') {
                        text_stream << '\n';
                    } else if (byte == '\t') {
                        text_stream << '\t';
                    } else if (byte == '\r') {
                        text_stream << '\r';
                    } else {
                        text_stream << "\\x" << std::hex << std::setfill('0') << std::setw(2) 
                                   << static_cast<int>(byte);
                    }
                }
                
                std::string text_content = text_stream.str();
                std::istringstream text_lines(text_content);
                std::string line;
                while (std::getline(text_lines, line)) {
                    result.output_lines.push_back(line);
                }
                
            } else if (format == "binary") {
                // Binary format (show byte values)
                for (size_t i = 0; i < blob_data.size(); i += 32) {
                    std::ostringstream binary_line;
                    binary_line << std::setfill('0') << std::setw(8) << i << ": ";
                    
                    for (size_t j = 0; j < 32 && i + j < blob_data.size(); j++) {
                        binary_line << std::setfill('0') << std::setw(3) 
                                   << static_cast<int>(blob_data[i + j]) << " ";
                    }
                    
                    result.output_lines.push_back(binary_line.str());
                }
                
            } else {
                result.success = false;
                result.error_message = "Unknown format: " + format;
                result.output_lines.push_back("Available formats: hex, text, binary");
            }
            
        } else {
            result.success = false;
            result.error_message = "Failed to read BLOB data: " + engine->getLastError();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error in BLOBVIEW command: " + std::string(e.what());
    }
    
    return result;
}

// ADD command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeAddCommand(const std::string& table_name) {
    SBEnhanced::CommandResult result;
    
    if (table_name.empty()) {
        result.success = false;
        result.error_message = "ADD requires a table name";
        result.output_lines.push_back("Usage: ADD table_name");
        return result;
    }
    
    if (!engine || !engine->isConnected()) {
        result.success = false;
        result.error_message = "Not connected to database";
        return result;
    }
    
    try {
        // Get table metadata
        std::vector<SBEnhanced::ColumnInfo> columns;
        if (!engine->getTableColumns(table_name, columns)) {
            result.success = false;
            result.error_message = "Failed to get table information: " + engine->getLastError();
            return result;
        }
        
        if (columns.empty()) {
            result.success = false;
            result.error_message = "Table not found or has no columns: " + table_name;
            return result;
        }
        
        result.output_lines.push_back("Interactive data entry for table: " + table_name);
        result.output_lines.push_back("===================================");
        result.output_lines.push_back("");
        result.output_lines.push_back("Enter data for each column (press Enter to use NULL):");
        result.output_lines.push_back("");
        
        // Collect values for each column
        std::vector<std::string> column_values;
        std::vector<std::string> column_names;
        
        for (const auto& column : columns) {
            column_names.push_back(column.name);
            
            std::string prompt = column.name + " (" + column.type_name;
            if (column.size > 0) {
                prompt += "(" + std::to_string(column.size) + ")";
            }
            if (!column.nullable) {
                prompt += " NOT NULL";
            }
            if (!column.default_value.empty()) {
                prompt += " DEFAULT " + column.default_value;
            }
            prompt += "): ";
            
            std::cout << prompt;
            std::string input;
            std::getline(std::cin, input);
            
            // Handle special values
            if (input.empty()) {
                if (column.nullable) {
                    column_values.push_back("NULL");
                } else if (!column.default_value.empty()) {
                    column_values.push_back(column.default_value);
                } else {
                    std::cout << "This column is NOT NULL and has no default value. Please enter a value: ";
                    std::getline(std::cin, input);
                    if (input.empty()) {
                        result.success = false;
                        result.error_message = "Cannot insert NULL into NOT NULL column: " + column.name;
                        return result;
                    }
                    column_values.push_back(input);
                }
            } else {
                column_values.push_back(input);
            }
        }
        
        // Construct INSERT statement
        std::ostringstream insert_sql;
        insert_sql << "INSERT INTO " << table_name << " (";
        
        // Column names
        for (size_t i = 0; i < column_names.size(); i++) {
            if (i > 0) insert_sql << ", ";
            insert_sql << column_names[i];
        }
        
        insert_sql << ") VALUES (";
        
        // Column values
        for (size_t i = 0; i < column_values.size(); i++) {
            if (i > 0) insert_sql << ", ";
            
            if (column_values[i] == "NULL") {
                insert_sql << "NULL";
            } else {
                // Determine if we need quotes based on column type
                const auto& column = columns[i];
                bool needs_quotes = (column.type_name.find("VARCHAR") != std::string::npos ||
                                   column.type_name.find("CHAR") != std::string::npos ||
                                   column.type_name.find("TEXT") != std::string::npos ||
                                   column.type_name.find("BLOB") != std::string::npos ||
                                   column.type_name.find("DATE") != std::string::npos ||
                                   column.type_name.find("TIME") != std::string::npos);
                
                if (needs_quotes) {
                    insert_sql << "'" << column_values[i] << "'";
                } else {
                    insert_sql << column_values[i];
                }
            }
        }
        
        insert_sql << ")";
        
        // Show the constructed SQL
        result.output_lines.push_back("Generated SQL:");
        result.output_lines.push_back(insert_sql.str());
        result.output_lines.push_back("");
        
        // Ask for confirmation
        std::cout << "Execute this INSERT statement? (y/n): ";
        std::string confirmation;
        std::getline(std::cin, confirmation);
        
        if (confirmation == "y" || confirmation == "Y" || confirmation == "yes") {
            // Execute the INSERT
            int rows_affected = 0;
            if (engine->executeUpdate(insert_sql.str(), rows_affected)) {
                result.success = true;
                result.output_lines.push_back("INSERT successful. Rows affected: " + std::to_string(rows_affected));
                result.rows_affected = rows_affected;
                
                // Ask if user wants to add more rows
                std::cout << "Add another row? (y/n): ";
                std::string more_rows;
                std::getline(std::cin, more_rows);
                
                if (more_rows == "y" || more_rows == "Y" || more_rows == "yes") {
                    result.output_lines.push_back("To add more rows, use the ADD command again.");
                }
                
            } else {
                result.success = false;
                result.error_message = "INSERT failed: " + engine->getLastError();
            }
        } else {
            result.success = true;
            result.output_lines.push_back("INSERT cancelled by user.");
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error in ADD command: " + std::string(e.what());
    }
    
    return result;
}

// COPY command implementation
SBEnhanced::CommandResult ISQLEnhanced::executeCopyCommand(const std::vector<std::string>& args) {
    SBEnhanced::CommandResult result;
    
    if (args.size() < 4) {
        result.success = false;
        result.error_message = "COPY requires source and destination";
        result.output_lines.push_back("Usage: COPY source_table TO destination_table [database]");
        result.output_lines.push_back("       COPY source_table TO destination_table FROM database");
        result.output_lines.push_back("       COPY source_table TO database:destination_table");
        result.output_lines.push_back("");
        result.output_lines.push_back("Examples:");
        result.output_lines.push_back("  COPY employees TO employees_backup");
        result.output_lines.push_back("  COPY employees TO employees_backup FROM other_db.fdb");
        result.output_lines.push_back("  COPY employees TO backup_db.fdb:employees_backup");
        return result;
    }
    
    if (!engine || !engine->isConnected()) {
        result.success = false;
        result.error_message = "Not connected to database";
        return result;
    }
    
    try {
        std::string source_table = args[1];
        std::string destination_table;
        std::string destination_database;
        
        // Parse arguments
        if (args.size() >= 4 && args[2] == "TO") {
            destination_table = args[3];
            
            // Check if destination includes database
            if (destination_table.find(':') != std::string::npos) {
                size_t colon_pos = destination_table.find(':');
                destination_database = destination_table.substr(0, colon_pos);
                destination_table = destination_table.substr(colon_pos + 1);
            }
            
            // Check for FROM clause
            if (args.size() >= 6 && args[4] == "FROM") {
                destination_database = args[5];
            }
        } else {
            result.success = false;
            result.error_message = "Invalid COPY syntax. Use: COPY source_table TO destination_table";
            return result;
        }
        
        result.output_lines.push_back("Copy Operation Details:");
        result.output_lines.push_back("======================");
        result.output_lines.push_back("Source table: " + source_table);
        result.output_lines.push_back("Destination table: " + destination_table);
        if (!destination_database.empty()) {
            result.output_lines.push_back("Destination database: " + destination_database);
        }
        result.output_lines.push_back("");
        
        // Get source table metadata
        std::vector<SBEnhanced::ColumnInfo> source_columns;
        if (!engine->getTableColumns(source_table, source_columns)) {
            result.success = false;
            result.error_message = "Failed to get source table information: " + engine->getLastError();
            return result;
        }
        
        if (source_columns.empty()) {
            result.success = false;
            result.error_message = "Source table not found or has no columns: " + source_table;
            return result;
        }
        
        // Get row count from source table
        SBEnhanced::QueryResults count_results;
        std::string count_sql = "SELECT COUNT(*) FROM " + source_table;
        if (!engine->executeQuery(count_sql, count_results)) {
            result.success = false;
            result.error_message = "Failed to count source table rows: " + engine->getLastError();
            return result;
        }
        
        int source_row_count = 0;
        if (!count_results.rows.empty() && !count_results.rows[0].empty()) {
            source_row_count = std::stoi(count_results.rows[0][0]);
        }
        
        result.output_lines.push_back("Source table contains " + std::to_string(source_row_count) + " rows");
        result.output_lines.push_back("Source table has " + std::to_string(source_columns.size()) + " columns");
        result.output_lines.push_back("");
        
        // Ask for confirmation
        std::cout << "Proceed with copy operation? (y/n): ";
        std::string confirmation;
        std::getline(std::cin, confirmation);
        
        if (confirmation != "y" && confirmation != "Y" && confirmation != "yes") {
            result.success = true;
            result.output_lines.push_back("Copy operation cancelled by user.");
            return result;
        }
        
        auto start_time = std::chrono::steady_clock::now();
        
        if (destination_database.empty()) {
            // Copy within same database
            
            // Check if destination table exists
            std::vector<SBEnhanced::ColumnInfo> dest_columns;
            bool dest_exists = engine->getTableColumns(destination_table, dest_columns);
            
            if (!dest_exists) {
                // Create destination table with same structure
                std::string create_sql = "CREATE TABLE " + destination_table + " (";
                
                for (size_t i = 0; i < source_columns.size(); i++) {
                    if (i > 0) create_sql += ", ";
                    
                    create_sql += source_columns[i].name + " " + source_columns[i].type_name;
                    
                    if (source_columns[i].size > 0) {
                        create_sql += "(" + std::to_string(source_columns[i].size) + ")";
                    }
                    
                    if (!source_columns[i].nullable) {
                        create_sql += " NOT NULL";
                    }
                    
                    if (!source_columns[i].default_value.empty()) {
                        create_sql += " DEFAULT " + source_columns[i].default_value;
                    }
                }
                
                create_sql += ")";
                
                result.output_lines.push_back("Creating destination table...");
                result.output_lines.push_back("SQL: " + create_sql);
                
                int create_rows = 0;
                if (!engine->executeUpdate(create_sql, create_rows)) {
                    result.success = false;
                    result.error_message = "Failed to create destination table: " + engine->getLastError();
                    return result;
                }
                
                result.output_lines.push_back("Destination table created successfully.");
            } else {
                result.output_lines.push_back("Destination table already exists.");
            }
            
            // Copy data
            std::string copy_sql = "INSERT INTO " + destination_table + " SELECT * FROM " + source_table;
            
            result.output_lines.push_back("Copying data...");
            result.output_lines.push_back("SQL: " + copy_sql);
            
            int copied_rows = 0;
            if (engine->executeUpdate(copy_sql, copied_rows)) {
                result.success = true;
                result.rows_affected = copied_rows;
                result.output_lines.push_back("Data copied successfully.");
                result.output_lines.push_back("Rows copied: " + std::to_string(copied_rows));
            } else {
                result.success = false;
                result.error_message = "Failed to copy data: " + engine->getLastError();
            }
            
        } else {
            // Copy to different database (requires database link or separate connection)
            result.output_lines.push_back("Cross-database copy not yet implemented.");
            result.output_lines.push_back("This feature requires database links or external tool support.");
            result.success = false;
            result.error_message = "Cross-database copy not implemented";
        }
        
        auto end_time = std::chrono::steady_clock::now();
        result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        if (execution_context.set_state.time) {
            result.output_lines.push_back("Copy operation time: " + formatElapsedTime(result.execution_time));
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error in COPY command: " + std::string(e.what());
    }
    
    return result;
}

//----------------------------
// Partial Hash Index Commands Implementation
//----------------------------

SBEnhanced::CommandResult ISQLEnhanced::executeCreatePartialHashIndex(const std::vector<std::string>& args)
{
    SBEnhanced::CommandResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (args.size() < 5) {
            result.success = false;
            result.error_message = "Usage: CREATE PARTIAL HASH INDEX index_name ON table_name (column_list) WHERE condition [OPTIONS]";
            return result;
        }
        
        // Parse command arguments
        std::string index_name = args[0];
        std::string table_name = args[2]; // Skip "ON"
        std::string columns_part;
        std::string where_condition;
        
        // Find column list (between parentheses)
        size_t start_paren = 0, end_paren = 0;
        for (size_t i = 3; i < args.size(); i++) {
            if (args[i].front() == '(') {
                start_paren = i;
            }
            if (args[i].back() == ')') {
                end_paren = i;
                break;
            }
            if (start_paren > 0 && end_paren == 0) {
                if (!columns_part.empty()) columns_part += " ";
                columns_part += args[i];
            }
        }
        
        // Extract WHERE condition
        bool found_where = false;
        for (size_t i = end_paren + 1; i < args.size(); i++) {
            if (args[i] == "WHERE") {
                found_where = true;
                continue;
            }
            if (found_where && args[i] != "OPTIONS") {
                if (!where_condition.empty()) where_condition += " ";
                where_condition += args[i];
            }
        }
        
        if (where_condition.empty()) {
            result.success = false;
            result.error_message = "Partial hash indexes require a WHERE condition";
            return result;
        }
        
        // Build the DDL statement
        std::string create_sql = "CREATE PARTIAL HASH INDEX " + index_name + 
                                " ON " + table_name + " (" + columns_part + ") WHERE " + where_condition;
        
        result.output_lines.push_back("Creating partial hash index...");
        result.output_lines.push_back("SQL: " + create_sql);
        
        // Execute the DDL
        int affected_rows = 0;
        if (engine->executeUpdate(create_sql, affected_rows)) {
            result.success = true;
            result.output_lines.push_back("Partial hash index '" + index_name + "' created successfully.");
            
            // Add index information to result metadata
            result.metadata["index_name"] = index_name;
            result.metadata["table_name"] = table_name;
            result.metadata["columns"] = columns_part;
            result.metadata["where_condition"] = where_condition;
        } else {
            result.success = false;
            result.error_message = "Failed to create partial hash index: " + engine->getLastError();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error creating partial hash index: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeShowPartialHashIndexes(const SBEnhanced::ShowOptions& options)
{
    SBEnhanced::CommandResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // Build query to show partial hash indexes
        std::string show_sql = 
            "SELECT RDB$INDEX_NAME, RDB$RELATION_NAME, RDB$EXPRESSION_SOURCE, "
            "       RDB$INDEX_TYPE, RDB$UNIQUE_FLAG, RDB$INDEX_INACTIVE "
            "FROM RDB$INDICES "
            "WHERE RDB$INDEX_TYPE = " + std::to_string(IDX_TYPE_PARTIAL_HASH);
        
        if (!options.schema_filter.empty()) {
            show_sql += " AND RDB$RELATION_NAME LIKE '" + options.schema_filter + "%'";
        }
        
        if (!options.name_filter.empty()) {
            show_sql += " AND RDB$INDEX_NAME LIKE '" + options.name_filter + "%'";
        }
        
        if (options.sort_results) {
            show_sql += " ORDER BY RDB$RELATION_NAME, RDB$INDEX_NAME";
        }
        
        result.output_lines.push_back("Querying partial hash indexes...");
        result.output_lines.push_back("SQL: " + show_sql);
        
        // Execute the query
        if (engine->executeQuery(show_sql, result.query_results)) {
            result.success = true;
            
            if (result.query_results.rows.empty()) {
                result.output_lines.push_back("No partial hash indexes found.");
            } else {
                result.output_lines.push_back("Found " + std::to_string(result.query_results.rows.size()) + " partial hash indexes:");
                
                // Format the results
                for (const auto& row : result.query_results.rows) {
                    std::string index_info = "  " + row[0] + " on " + row[1];
                    if (!row[2].empty()) {
                        index_info += " WHERE " + row[2];
                    }
                    if (row[4] == "1") {
                        index_info += " (UNIQUE)";
                    }
                    if (row[5] == "1") {
                        index_info += " (INACTIVE)";
                    }
                    result.output_lines.push_back(index_info);
                }
            }
        } else {
            result.success = false;
            result.error_message = "Failed to query partial hash indexes: " + engine->getLastError();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error showing partial hash indexes: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeAnalyzePartialHashIndex(const std::string& index_name)
{
    SBEnhanced::CommandResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (index_name.empty()) {
            result.success = false;
            result.error_message = "Usage: ANALYZE PARTIAL HASH INDEX index_name";
            return result;
        }
        
        result.output_lines.push_back("Analyzing partial hash index: " + index_name);
        
        // Get basic index information
        std::string info_sql = 
            "SELECT RDB$INDEX_NAME, RDB$RELATION_NAME, RDB$EXPRESSION_SOURCE, "
            "       RDB$STATISTICS, RDB$INDEX_TYPE "
            "FROM RDB$INDICES "
            "WHERE RDB$INDEX_NAME = '" + index_name + "' "
            "  AND RDB$INDEX_TYPE = " + std::to_string(IDX_TYPE_PARTIAL_HASH);
        
        SBEnhanced::QueryResults index_info;
        if (!engine->executeQuery(info_sql, index_info) || index_info.rows.empty()) {
            result.success = false;
            result.error_message = "Partial hash index '" + index_name + "' not found";
            return result;
        }
        
        const auto& index_row = index_info.rows[0];
        std::string table_name = index_row[1];
        std::string where_clause = index_row[2];
        std::string statistics = index_row[3];
        
        result.output_lines.push_back("Index: " + index_name);
        result.output_lines.push_back("Table: " + table_name);
        result.output_lines.push_back("WHERE condition: " + where_clause);
        result.output_lines.push_back("Current statistics: " + statistics);
        
        // Get table statistics for comparison
        std::string table_stats_sql = "SELECT COUNT(*) FROM " + table_name;
        SBEnhanced::QueryResults table_stats;
        if (engine->executeQuery(table_stats_sql, table_stats)) {
            uint64_t total_rows = std::stoull(table_stats.rows[0][0]);
            result.output_lines.push_back("Total rows in table: " + std::to_string(total_rows));
        }
        
        // Get index-qualified row count
        std::string qualified_stats_sql = "SELECT COUNT(*) FROM " + table_name + " WHERE " + where_clause;
        SBEnhanced::QueryResults qualified_stats;
        if (engine->executeQuery(qualified_stats_sql, qualified_stats)) {
            uint64_t qualified_rows = std::stoull(qualified_stats.rows[0][0]);
            result.output_lines.push_back("Rows matching index condition: " + std::to_string(qualified_rows));
            
            if (qualified_rows > 0) {
                double selectivity = static_cast<double>(qualified_rows) / static_cast<double>(std::stoull(table_stats.rows[0][0]));
                result.output_lines.push_back("Index selectivity: " + std::to_string(selectivity * 100.0) + "%");
            }
        }
        
        // Performance analysis
        result.output_lines.push_back("");
        result.output_lines.push_back("Performance Analysis:");
        result.output_lines.push_back("- Hash indexes provide O(1) average lookup time");
        result.output_lines.push_back("- Partial indexes reduce storage overhead");
        result.output_lines.push_back("- Best for equality searches on filtered data");
        
        result.success = true;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error analyzing partial hash index: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeDropPartialHashIndex(const std::string& index_name)
{
    SBEnhanced::CommandResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (index_name.empty()) {
            result.success = false;
            result.error_message = "Usage: DROP PARTIAL HASH INDEX index_name";
            return result;
        }
        
        // Verify the index exists and is a partial hash index
        std::string check_sql = 
            "SELECT RDB$INDEX_NAME FROM RDB$INDICES "
            "WHERE RDB$INDEX_NAME = '" + index_name + "' "
            "  AND RDB$INDEX_TYPE = " + std::to_string(IDX_TYPE_PARTIAL_HASH);
        
        SBEnhanced::QueryResults check_results;
        if (!engine->executeQuery(check_sql, check_results) || check_results.rows.empty()) {
            result.success = false;
            result.error_message = "Partial hash index '" + index_name + "' not found";
            return result;
        }
        
        // Drop the index
        std::string drop_sql = "DROP INDEX " + index_name;
        
        result.output_lines.push_back("Dropping partial hash index...");
        result.output_lines.push_back("SQL: " + drop_sql);
        
        int affected_rows = 0;
        if (engine->executeUpdate(drop_sql, affected_rows)) {
            result.success = true;
            result.output_lines.push_back("Partial hash index '" + index_name + "' dropped successfully.");
        } else {
            result.success = false;
            result.error_message = "Failed to drop partial hash index: " + engine->getLastError();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error dropping partial hash index: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return result;
}

SBEnhanced::CommandResult ISQLEnhanced::executeRecomputePartialHashIndex(const std::string& index_name)
{
    SBEnhanced::CommandResult result;
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (index_name.empty()) {
            result.success = false;
            result.error_message = "Usage: RECOMPUTE PARTIAL HASH INDEX index_name";
            return result;
        }
        
        // Verify the index exists and is a partial hash index
        std::string check_sql = 
            "SELECT RDB$INDEX_NAME, RDB$RELATION_NAME FROM RDB$INDICES "
            "WHERE RDB$INDEX_NAME = '" + index_name + "' "
            "  AND RDB$INDEX_TYPE = " + std::to_string(IDX_TYPE_PARTIAL_HASH);
        
        SBEnhanced::QueryResults check_results;
        if (!engine->executeQuery(check_sql, check_results) || check_results.rows.empty()) {
            result.success = false;
            result.error_message = "Partial hash index '" + index_name + "' not found";
            return result;
        }
        
        std::string table_name = check_results.rows[0][1];
        
        result.output_lines.push_back("Recomputing partial hash index: " + index_name);
        result.output_lines.push_back("Table: " + table_name);
        
        // Recompute index statistics
        std::string recompute_sql = "SET STATISTICS INDEX " + index_name;
        
        result.output_lines.push_back("SQL: " + recompute_sql);
        
        int affected_rows = 0;
        if (engine->executeUpdate(recompute_sql, affected_rows)) {
            result.success = true;
            result.output_lines.push_back("Statistics recomputed successfully for partial hash index '" + index_name + "'.");
            
            // Get updated statistics
            std::string stats_sql = 
                "SELECT RDB$STATISTICS FROM RDB$INDICES "
                "WHERE RDB$INDEX_NAME = '" + index_name + "'";
            
            SBEnhanced::QueryResults stats_results;
            if (engine->executeQuery(stats_sql, stats_results) && !stats_results.rows.empty()) {
                result.output_lines.push_back("Updated statistics: " + stats_results.rows[0][0]);
            }
        } else {
            result.success = false;
            result.error_message = "Failed to recompute partial hash index statistics: " + engine->getLastError();
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = "Error recomputing partial hash index: " + std::string(e.what());
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.execution_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    return result;
}