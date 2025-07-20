#pragma once

#include "sb_engine_integration.h"
#include "utility_enhancements.h"
#include "utility_config.h"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace SBEnhanced {

// Command types
enum class CommandType {
    SQL_STATEMENT,
    DDL_STATEMENT,
    SHOW_COMMAND,
    SET_COMMAND,
    CONNECT_COMMAND,
    DISCONNECT_COMMAND,
    EXIT_COMMAND,
    HELP_COMMAND,
    DESCRIBE_COMMAND,
    EXTRACT_COMMAND,
    SCRIPT_COMMAND,
    TRANSACTION_COMMAND,
    INPUT_COMMAND,
    OUTPUT_COMMAND,
    EDIT_COMMAND,
    SHELL_COMMAND,
    EXPLAIN_COMMAND,
    ADD_COMMAND,
    COPY_COMMAND,
    BLOBDUMP_COMMAND,
    BLOBVIEW_COMMAND,
    COMMENT,
    EMPTY,
    UNKNOWN
};

// SET command options
enum class SetOption {
    AUTODDL,
    ECHO,
    HEADING,
    LIST,
    PAGESIZE,
    ROWCOUNT,
    SQLDA_DISPLAY,
    STATS,
    TERM,
    TIME,
    WARNINGS,
    NAMES,
    CHARSET,
    BAIL,
    BULK_INSERT,
    COUNT,
    PLAN,
    PLANONLY,
    MAXROWS,
    SQLDIALECT,
    TRANSACTION,
    WIDTH,
    CLIENTLIB,
    DECFLOAT,
    UNKNOWN_SET
};

// SHOW command types
enum class ShowType {
    DATABASE,
    TABLES,
    VIEWS,
    PROCEDURES,
    FUNCTIONS,
    TRIGGERS,
    DOMAINS,
    INDICES,
    GENERATORS,
    GRANTS,
    ROLES,
    USERS,
    SCHEMAS,
    SYSTEM_TABLES,
    COLLATIONS,
    EXCEPTIONS,
    FILTERS,
    PACKAGES,
    PUBLICATIONS,
    SUBSCRIPTIONS,
    DEPENDENCIES,
    PRIVILEGES,
    MAPPING,
    SECCLASSES,
    VERSION,
    UNKNOWN_SHOW
};

// Extract options
struct ExtractOptions {
    bool include_data = false;
    bool include_metadata = true;
    bool include_system_tables = false;
    bool include_indexes = true;
    bool include_constraints = true;
    bool include_triggers = true;
    bool include_procedures = true;
    bool include_functions = true;
    bool include_views = true;
    bool include_domains = true;
    bool include_generators = true;
    bool include_roles = true;
    bool include_schemas = true;
    bool include_comments = true;
    bool include_grants = true;
    bool format_output = true;
    bool add_drop_statements = false;
    bool add_create_database = false;
    std::string schema_filter;
    std::string table_filter;
    std::string object_filter;
    std::vector<std::string> exclude_objects;
    std::vector<std::string> include_objects;
    SBEnhanced::DDLFormat ddl_format = SBEnhanced::DDLFormat::FORMATTED;
    SBEnhanced::ExportFormat export_format = SBEnhanced::ExportFormat::TEXT;
    std::string output_file;
    bool verbose = false;
};

// Show options
struct ShowOptions {
    bool include_system_objects = false;
    bool include_detailed_info = false;
    bool include_statistics = false;
    bool include_dependencies = false;
    bool include_permissions = false;
    bool include_comments = false;
    bool sort_results = true;
    std::string schema_filter;
    std::string name_filter;
    std::string type_filter;
    SBEnhanced::OutputFormat output_format = SBEnhanced::OutputFormat::TABLE;
    int max_results = 1000;
    bool case_sensitive = false;
    bool use_regex = false;
};

// Script processing options
struct ScriptOptions {
    bool continue_on_error = false;
    bool echo_commands = false;
    bool show_timing = true;
    bool show_row_counts = true;
    bool auto_commit = true;
    bool validate_syntax = true;
    bool dry_run = false;
    bool verbose = false;
    std::string log_file;
    std::string error_file;
    std::function<void(const std::string&)> progress_callback;
    std::function<bool(const std::string&)> confirmation_callback;
    std::map<std::string, std::string> variables;
    std::vector<std::string> include_paths;
};

// SET command state
struct SetState {
    bool autoddl = true;              // Auto-commit DDL statements
    bool echo = false;                // Echo commands to output
    bool heading = true;              // Show column headers
    bool list = false;                // List format output
    int pagesize = 20;                // Page size for output
    bool rowcount = true;             // Show row counts
    bool sqlda_display = false;       // Show SQLDA information
    bool stats = false;               // Show statement statistics
    std::string term = ";";           // Statement terminator
    bool time = false;                // Show execution timing
    bool warnings = true;             // Show warnings
    std::string names = "SQL";        // Character set for names
    std::string charset = "UTF8";     // Connection character set
    bool bail = false;                // Exit on first error
    bool bulk_insert = false;         // Bulk insert mode
    bool count = false;               // Count affected rows
    bool plan = false;                // Show execution plan
    bool planonly = false;            // Show plan only (don't execute)
    int maxrows = 0;                  // Maximum rows to fetch (0 = unlimited)
    int sqldialect = 3;               // SQL dialect
    std::string transaction = "READ_COMMITTED"; // Transaction isolation
    int width = 80;                   // Output width
    std::string clientlib = "libsbclient.so"; // Client library
    std::string decfloat = "ROUND_HALF_UP";   // DECFLOAT rounding
};

// Command execution context
struct ExecutionContext {
    std::string current_schema;
    std::string current_role;
    bool auto_commit = true;
    bool echo_commands = false;
    bool show_timing = true;
    bool show_row_counts = true;
    bool show_headers = true;
    bool show_statistics = false;
    bool case_sensitive = false;
    SBEnhanced::OutputFormat output_format = SBEnhanced::OutputFormat::TABLE;
    std::string output_file;
    std::string log_file;
    std::string error_file;
    std::map<std::string, std::string> session_variables;
    std::vector<std::string> command_history;
    int max_history_size = 1000;
    bool enable_paging = true;
    int page_size = 20;
    std::string prompt = "SB> ";
    std::string continuation_prompt = "CON> ";
    std::chrono::seconds query_timeout{300};
    bool enable_trace = false;
    bool enable_explain = false;
    bool enable_profile = false;
    
    // SET command state
    SetState set_state;
    
    // File handling
    std::vector<std::string> input_stack;     // Nested INPUT files
    std::string output_redirect;              // OUTPUT redirection
    std::string editor_command = "vi";        // Editor for EDIT command
    std::string shell_command = "/bin/bash";  // Shell for SHELL command
    
    // BLOB handling
    std::string blob_dump_directory = "./blobs";
    std::string blob_view_command = "hexdump -C";
    
    // Performance monitoring
    std::map<std::string, std::chrono::microseconds> statement_times;
    std::map<std::string, uint64_t> statement_counts;
    std::map<std::string, uint64_t> statement_rows;
};

// Command result
struct CommandResult {
    bool success = false;
    std::string message;
    std::string error_message;
    SBEnhanced::QueryResults query_results;
    std::chrono::microseconds execution_time{0};
    uint64_t rows_affected = 0;
    uint64_t rows_fetched = 0;
    std::string ddl_output;
    std::vector<std::string> output_lines;
    std::map<std::string, std::string> metadata;
    bool has_more_results = false;
};

// Interactive session state
struct SessionState {
    bool interactive_mode = false;
    bool connected = false;
    std::string current_database;
    std::string current_user;
    std::string current_role;
    std::string current_schema;
    std::atomic<bool> should_exit{false};
    std::atomic<bool> should_disconnect{false};
    std::mutex output_mutex;
    std::mutex input_mutex;
    std::queue<std::string> command_queue;
    std::condition_variable command_available;
    std::thread input_thread;
    std::thread output_thread;
    std::atomic<bool> threads_running{false};
    std::vector<std::string> startup_commands;
    std::vector<std::string> shutdown_commands;
    std::map<std::string, std::string> aliases;
    std::map<std::string, std::function<CommandResult(const std::vector<std::string>&)>> custom_commands;
};

} // namespace SBEnhanced

// Enhanced ISQL class
class ISQLEnhanced {
private:
    // Core components
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<OutputFormatter> formatter;
    std::unique_ptr<QueryAnalyzer> analyzer;
    std::unique_ptr<UtilityConfiguration> config;
    
    // Session state
    SBEnhanced::ExecutionContext execution_context;
    SBEnhanced::SessionState session_state;
    
    // Command processors
    std::map<std::string, std::function<SBEnhanced::CommandResult(const std::vector<std::string>&)>> command_processors;
    
    // Input/output streams
    std::istream* input_stream;
    std::ostream* output_stream;
    std::ostream* error_stream;
    
    // File handling
    std::unique_ptr<std::ifstream> script_file;
    std::unique_ptr<std::ofstream> output_file;
    std::unique_ptr<std::ofstream> log_file;
    std::unique_ptr<std::ofstream> error_file;
    
    // Performance tracking
    std::atomic<uint64_t> total_commands_executed{0};
    std::atomic<uint64_t> successful_commands{0};
    std::atomic<uint64_t> failed_commands{0};
    std::chrono::steady_clock::time_point session_start_time;
    
    // Error handling
    std::vector<std::string> error_log;
    std::mutex error_mutex;
    
public:
    ISQLEnhanced();
    ~ISQLEnhanced();
    
    // Initialization and configuration
    bool initialize(const SBEnhanced::ConnectionOptions& options);
    bool loadConfiguration(const std::string& config_file);
    bool shutdown();
    
    // Connection management
    bool connect(const std::string& database_path, const std::string& username, 
                const std::string& password, const std::string& role = "");
    bool disconnect();
    bool isConnected() const;
    bool reconnect();
    
    // Interactive mode
    bool startInteractiveMode();
    bool processInteractiveCommand(const std::string& command);
    void stopInteractiveMode();
    
    // Script processing
    bool executeScript(const std::string& script_path, const SBEnhanced::ScriptOptions& options);
    bool executeScriptFromString(const std::string& script_content, const SBEnhanced::ScriptOptions& options);
    bool executeCommand(const std::string& command);
    
    // Command execution
    SBEnhanced::CommandResult executeSQLStatement(const std::string& sql);
    SBEnhanced::CommandResult executeShowCommand(const std::string& command, const SBEnhanced::ShowOptions& options);
    SBEnhanced::CommandResult executeSetCommand(const std::string& command);
    SBEnhanced::CommandResult executeExtractCommand(const std::string& command, const SBEnhanced::ExtractOptions& options);
    SBEnhanced::CommandResult executeDescribeCommand(const std::string& object_name);
    SBEnhanced::CommandResult executeHelpCommand(const std::string& topic = "");
    SBEnhanced::CommandResult executeInputCommand(const std::string& filename);
    SBEnhanced::CommandResult executeOutputCommand(const std::string& filename);
    SBEnhanced::CommandResult executeEditCommand(const std::string& filename = "");
    SBEnhanced::CommandResult executeShellCommand(const std::string& command);
    SBEnhanced::CommandResult executeExplainCommand(const std::string& sql);
    SBEnhanced::CommandResult executeAddCommand(const std::string& table_name);
    SBEnhanced::CommandResult executeCopyCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult executeBlobDumpCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult executeBlobViewCommand(const std::vector<std::string>& args);
    
    // SET command implementations
    SBEnhanced::CommandResult executeSetAutoddl(bool value);
    SBEnhanced::CommandResult executeSetEcho(bool value);
    SBEnhanced::CommandResult executeSetHeading(bool value);
    SBEnhanced::CommandResult executeSetList(bool value);
    SBEnhanced::CommandResult executeSetPagesize(int value);
    SBEnhanced::CommandResult executeSetRowcount(bool value);
    SBEnhanced::CommandResult executeSetSqldaDisplay(bool value);
    SBEnhanced::CommandResult executeSetStats(bool value);
    SBEnhanced::CommandResult executeSetTerm(const std::string& value);
    SBEnhanced::CommandResult executeSetTime(bool value);
    SBEnhanced::CommandResult executeSetWarnings(bool value);
    SBEnhanced::CommandResult executeSetNames(const std::string& value);
    SBEnhanced::CommandResult executeSetCharset(const std::string& value);
    SBEnhanced::CommandResult executeSetBail(bool value);
    SBEnhanced::CommandResult executeSetBulkInsert(bool value);
    SBEnhanced::CommandResult executeSetCount(bool value);
    SBEnhanced::CommandResult executeSetPlan(bool value);
    SBEnhanced::CommandResult executeSetPlanonly(bool value);
    SBEnhanced::CommandResult executeSetMaxrows(int value);
    SBEnhanced::CommandResult executeSetSqldialect(int value);
    SBEnhanced::CommandResult executeSetTransaction(const std::string& value);
    SBEnhanced::CommandResult executeSetWidth(int value);
    SBEnhanced::CommandResult executeSetClientlib(const std::string& value);
    SBEnhanced::CommandResult executeSetDecfloat(const std::string& value);
    
    // Advanced DDL extraction (using existing infrastructure)
    bool extractDatabaseDDL(const SBEnhanced::ExtractOptions& options);
    bool extractSchemaDDL(const std::string& schema_name, const SBEnhanced::ExtractOptions& options);
    bool extractTableDDL(const std::string& table_name, const SBEnhanced::ExtractOptions& options);
    bool extractViewDDL(const std::string& view_name, const SBEnhanced::ExtractOptions& options);
    bool extractProcedureDDL(const std::string& procedure_name, const SBEnhanced::ExtractOptions& options);
    bool extractFunctionDDL(const std::string& function_name, const SBEnhanced::ExtractOptions& options);
    bool extractTriggerDDL(const std::string& trigger_name, const SBEnhanced::ExtractOptions& options);
    bool extractIndexDDL(const std::string& index_name, const SBEnhanced::ExtractOptions& options);
    bool extractConstraintDDL(const std::string& constraint_name, const SBEnhanced::ExtractOptions& options);
    bool extractDomainDDL(const std::string& domain_name, const SBEnhanced::ExtractOptions& options);
    bool extractGeneratorDDL(const std::string& generator_name, const SBEnhanced::ExtractOptions& options);
    bool extractRoleDDL(const std::string& role_name, const SBEnhanced::ExtractOptions& options);
    bool extractGrantsDDL(const std::string& object_name, const SBEnhanced::ExtractOptions& options);
    
    // Enhanced SHOW commands (using existing metadata)
    bool showTables(const SBEnhanced::ShowOptions& options);
    bool showViews(const SBEnhanced::ShowOptions& options);
    bool showProcedures(const SBEnhanced::ShowOptions& options);
    bool showFunctions(const SBEnhanced::ShowOptions& options);
    bool showTriggers(const SBEnhanced::ShowOptions& options);
    bool showIndexes(const SBEnhanced::ShowOptions& options);
    bool showConstraints(const SBEnhanced::ShowOptions& options);
    bool showDomains(const SBEnhanced::ShowOptions& options);
    bool showGenerators(const SBEnhanced::ShowOptions& options);
    bool showRoles(const SBEnhanced::ShowOptions& options);
    bool showUsers(const SBEnhanced::ShowOptions& options);
    bool showSchemas(const SBEnhanced::ShowOptions& options);
    bool showGrants(const SBEnhanced::ShowOptions& options);
    bool showDependencies(const std::string& object_name, const SBEnhanced::ShowOptions& options);
    bool showSystemTables(const SBEnhanced::ShowOptions& options);
    bool showConnections(const SBEnhanced::ShowOptions& options);
    bool showTransactions(const SBEnhanced::ShowOptions& options);
    bool showStatistics(const SBEnhanced::ShowOptions& options);
    bool showVersion();
    bool showDatabase();
    bool showCurrentSchema();
    bool showCurrentRole();
    bool showCurrentUser();
    bool showTime();
    bool showSession();
    
    // Advanced query execution with analysis
    bool executeQueryWithAnalysis(const std::string& sql);
    bool executeQueryWithPlan(const std::string& sql);
    bool executeQueryWithProfile(const std::string& sql);
    bool executeQueryWithTrace(const std::string& sql);
    bool executeQueryWithOptimization(const std::string& sql);
    
    // Enhanced output formatting
    void setOutputFormat(SBEnhanced::OutputFormat format);
    void setOutputFile(const std::string& filename);
    void setShowHeaders(bool show);
    void setShowRowNumbers(bool show);
    void setShowStatistics(bool show);
    void setShowTiming(bool show);
    void setPageSize(int size);
    void setMaxColumnWidth(int width);
    
    // Transaction management
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool setAutoCommit(bool enabled);
    bool isInTransaction() const;
    
    // Schema management (leveraging existing hierarchical schema support)
    bool setCurrentSchema(const std::string& schema_name);
    std::string getCurrentSchema() const;
    bool createSchema(const std::string& schema_name, const std::string& parent_schema = "");
    bool dropSchema(const std::string& schema_name, bool cascade = false);
    std::vector<std::string> listSchemas(const std::string& pattern = "");
    bool validateSchemaPath(const std::string& schema_path);
    
    // Session management
    bool setSessionVariable(const std::string& name, const std::string& value);
    std::string getSessionVariable(const std::string& name) const;
    bool clearSessionVariables();
    bool saveSession(const std::string& filename);
    bool loadSession(const std::string& filename);
    
    // Command history
    void addToHistory(const std::string& command);
    std::vector<std::string> getHistory() const;
    bool saveHistory(const std::string& filename);
    bool loadHistory(const std::string& filename);
    void clearHistory();
    
    // Performance and monitoring
    SBEnhanced::PerformanceMetrics getPerformanceMetrics() const;
    std::vector<std::string> getOptimizationRecommendations() const;
    void enablePerformanceMonitoring(bool enabled);
    void enableQueryProfiling(bool enabled);
    void enableTracing(bool enabled);
    
    // Error handling
    std::string getLastError() const;
    std::vector<std::string> getErrorLog() const;
    void clearErrorLog();
    
    // Utility methods
    std::string formatOutput(const SBEnhanced::QueryResults& results) const;
    std::string formatDDL(const std::string& ddl) const;
    std::string formatError(const std::string& error) const;
    std::string formatMessage(const std::string& message) const;
    
    // Configuration accessors
    const SBEnhanced::ExecutionContext& getExecutionContext() const { return execution_context; }
    const SBEnhanced::SessionState& getSessionState() const { return session_state; }
    
private:
    // Command parsing and processing
    SBEnhanced::CommandType parseCommandType(const std::string& command);
    std::vector<std::string> parseCommandArgs(const std::string& command);
    std::string normalizeCommand(const std::string& command);
    bool isCompleteStatement(const std::string& statement);
    std::string assembleMultilineStatement(const std::string& partial_statement);
    
    // Command processors
    void initializeCommandProcessors();
    SBEnhanced::CommandResult processConnectCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processDisconnectCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processSetCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processShowCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processExtractCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processDescribeCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processHelpCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processTransactionCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processScriptCommand(const std::vector<std::string>& args);
    SBEnhanced::CommandResult processExitCommand(const std::vector<std::string>& args);
    
    // DDL extraction helpers
    std::string buildTableDDL(const std::string& table_name, const SBEnhanced::ExtractOptions& options);
    std::string buildViewDDL(const std::string& view_name, const SBEnhanced::ExtractOptions& options);
    std::string buildProcedureDDL(const std::string& procedure_name, const SBEnhanced::ExtractOptions& options);
    std::string buildFunctionDDL(const std::string& function_name, const SBEnhanced::ExtractOptions& options);
    std::string buildTriggerDDL(const std::string& trigger_name, const SBEnhanced::ExtractOptions& options);
    std::string buildIndexDDL(const std::string& index_name, const SBEnhanced::ExtractOptions& options);
    std::string buildConstraintDDL(const std::string& constraint_name, const SBEnhanced::ExtractOptions& options);
    std::string buildDomainDDL(const std::string& domain_name, const SBEnhanced::ExtractOptions& options);
    std::string buildGeneratorDDL(const std::string& generator_name, const SBEnhanced::ExtractOptions& options);
    std::string buildRoleDDL(const std::string& role_name, const SBEnhanced::ExtractOptions& options);
    std::string buildGrantsDDL(const std::string& object_name, const SBEnhanced::ExtractOptions& options);
    
    // SHOW command helpers
    std::string buildShowTablesQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowViewsQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowProceduresQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowFunctionsQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowTriggersQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowIndexesQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowConstraintsQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowDomainsQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowGeneratorsQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowRolesQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowUsersQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowSchemasQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowGrantsQuery(const SBEnhanced::ShowOptions& options);
    std::string buildShowDependenciesQuery(const std::string& object_name, const SBEnhanced::ShowOptions& options);
    
    // Interactive mode helpers
    void startInputThread();
    void startOutputThread();
    void stopThreads();
    void processInputLoop();
    void processOutputLoop();
    std::string readMultilineInput(const std::string& prompt);
    void displayPrompt();
    void displayResult(const SBEnhanced::CommandResult& result);
    void displayError(const std::string& error);
    void displayMessage(const std::string& message);
    
    // File I/O helpers
    bool openOutputFile(const std::string& filename);
    bool openLogFile(const std::string& filename);
    bool openErrorFile(const std::string& filename);
    void closeFiles();
    
    // Variable substitution
    std::string substituteVariables(const std::string& text);
    bool setVariable(const std::string& name, const std::string& value);
    std::string getVariable(const std::string& name) const;
    
    // Query analysis helpers
    bool analyzeQuery(const std::string& sql, SBEnhanced::QueryPlan& plan);
    bool profileQuery(const std::string& sql, SBEnhanced::PerformanceProfile& profile);
    std::vector<std::string> getQueryOptimizationHints(const std::string& sql);
    
    // Utility helpers
    void logError(const std::string& error);
    void logMessage(const std::string& message);
    void updatePerformanceMetrics(const SBEnhanced::CommandResult& result);
    std::string formatElapsedTime(const std::chrono::microseconds& duration);
    std::string formatRowCount(uint64_t count);
    std::string formatTimestamp();
    bool validateObjectName(const std::string& name);
    bool validateSchemaName(const std::string& name);
    std::string escapeObjectName(const std::string& name);
    std::string quoteString(const std::string& str);
    
    // Configuration helpers
    void loadDefaultConfiguration();
    void applyConfiguration();
    void updateExecutionContext();
    void updateSessionState();
};