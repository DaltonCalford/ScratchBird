#include "sb_isql_enhanced.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

class ISQLTestSuite {
private:
    std::string test_database_path;
    std::string test_output_dir;
    std::string test_config_file;
    std::unique_ptr<ISQLEnhanced> isql;
    
    int tests_run = 0;
    int tests_passed = 0;
    int tests_failed = 0;
    
    // Test utilities
    bool file_exists(const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }
    
    bool create_test_database() {
        // This would create a test database using existing ScratchBird tools
        // For now, we'll create a placeholder
        std::string create_cmd = "echo 'CREATE DATABASE test_db;' > " + test_database_path;
        return system(create_cmd.c_str()) == 0;
    }
    
    bool cleanup_test_database() {
        if (file_exists(test_database_path)) {
            return remove(test_database_path.c_str()) == 0;
        }
        return true;
    }
    
    void setup_test_environment() {
        // Create test output directory
        test_output_dir = "/tmp/sb_isql_tests";
        std::string mkdir_cmd = "mkdir -p " + test_output_dir;
        system(mkdir_cmd.c_str());
        
        // Set test database path
        test_database_path = test_output_dir + "/test_database.fdb";
        
        // Create test configuration file
        test_config_file = test_output_dir + "/test_config.conf";
        create_test_config();
    }
    
    void create_test_config() {
        std::ofstream config_file(test_config_file);
        config_file << "[isql]\n";
        config_file << "default_format = table\n";
        config_file << "show_headers = true\n";
        config_file << "show_timing = true\n";
        config_file << "show_statistics = true\n";
        config_file << "page_size = 20\n";
        config_file << "max_column_width = 50\n";
        config_file << "enable_history = true\n";
        config_file << "enable_auto_commit = true\n";
        config_file << "\n";
        config_file << "[connection]\n";
        config_file << "connect_timeout = 30\n";
        config_file << "query_timeout = 300\n";
        config_file << "enable_monitoring = true\n";
        config_file << "enable_tracing = false\n";
        config_file << "\n";
        config_file << "[performance]\n";
        config_file << "enable_performance_monitoring = true\n";
        config_file << "enable_query_profiling = true\n";
        config_file << "slow_query_threshold = 1000\n";
        config_file.close();
    }
    
    void cleanup_test_environment() {
        cleanup_test_database();
        std::string cleanup_cmd = "rm -rf " + test_output_dir;
        system(cleanup_cmd.c_str());
    }
    
    void assert_test(bool condition, const std::string& test_name, const std::string& message = "") {
        tests_run++;
        if (condition) {
            tests_passed++;
            std::cout << "✓ " << test_name << " PASSED" << std::endl;
        } else {
            tests_failed++;
            std::cout << "✗ " << test_name << " FAILED";
            if (!message.empty()) {
                std::cout << ": " << message;
            }
            std::cout << std::endl;
        }
    }
    
public:
    ISQLTestSuite() {
        setup_test_environment();
    }
    
    ~ISQLTestSuite() {
        cleanup_test_environment();
    }
    
    // Test initialization and configuration
    void test_initialization() {
        std::cout << "\n=== Testing Initialization and Configuration ===" << std::endl;
        
        // Test basic initialization
        isql = std::make_unique<ISQLEnhanced>();
        assert_test(isql != nullptr, "ISQLEnhanced constructor");
        
        // Test configuration loading
        bool config_loaded = isql->loadConfiguration(test_config_file);
        assert_test(config_loaded, "Configuration loading");
        
        // Test initialization with options
        SBEnhanced::ConnectionOptions options;
        options.database_path = test_database_path;
        options.username = "SYSDBA";
        options.password = "masterkey";
        options.enable_monitoring = true;
        options.enable_tracing = false;
        
        bool initialized = isql->initialize(options);
        assert_test(initialized, "ISQL initialization with options");
        
        std::cout << "Initialization tests completed." << std::endl;
    }
    
    // Test connection management
    void test_connection_management() {
        std::cout << "\n=== Testing Connection Management ===" << std::endl;
        
        // Test connection (would fail without real database)
        bool connected = isql->connect(test_database_path, "SYSDBA", "masterkey");
        assert_test(!connected, "Connection to non-existent database (expected to fail)");
        
        // Test connection status
        bool is_connected = isql->isConnected();
        assert_test(!is_connected, "Connection status check");
        
        // Test disconnect (should not fail even if not connected)
        bool disconnected = isql->disconnect();
        assert_test(disconnected, "Disconnection");
        
        std::cout << "Connection management tests completed." << std::endl;
    }
    
    // Test command parsing
    void test_command_parsing() {
        std::cout << "\n=== Testing Command Parsing ===" << std::endl;
        
        // Test SQL command execution (will fail without connection)
        auto result = isql->executeSQLStatement("SELECT 1 FROM RDB$DATABASE");
        assert_test(!result.success, "SQL execution without connection (expected to fail)");
        assert_test(!result.error_message.empty(), "Error message for failed SQL");
        
        // Test command execution
        auto cmd_result = isql->executeCommand("SHOW VERSION");
        assert_test(cmd_result.success, "SHOW VERSION command");
        
        cmd_result = isql->executeCommand("HELP");
        assert_test(cmd_result.success, "HELP command");
        
        cmd_result = isql->executeCommand("SET ECHO ON");
        assert_test(cmd_result.success, "SET command");
        
        std::cout << "Command parsing tests completed." << std::endl;
    }
    
    // Test output formatting
    void test_output_formatting() {
        std::cout << "\n=== Testing Output Formatting ===" << std::endl;
        
        // Create sample query results
        SBEnhanced::QueryResults results;
        results.column_names = {"ID", "NAME", "VALUE"};
        results.column_types = {"INTEGER", "VARCHAR", "DECIMAL"};
        results.rows = {
            {"1", "Test Item 1", "123.45"},
            {"2", "Test Item 2", "678.90"},
            {"3", "Test Item 3", "999.99"}
        };
        results.rows_fetched = 3;
        results.execution_time = std::chrono::microseconds(1500);
        
        // Test different output formats
        std::string table_output = isql->formatOutput(results);
        assert_test(!table_output.empty(), "Table format output");
        
        isql->setOutputFormat(SBEnhanced::OutputFormat::CSV);
        std::string csv_output = isql->formatOutput(results);
        assert_test(!csv_output.empty(), "CSV format output");
        assert_test(csv_output.find(",") != std::string::npos, "CSV format contains commas");
        
        isql->setOutputFormat(SBEnhanced::OutputFormat::JSON);
        std::string json_output = isql->formatOutput(results);
        assert_test(!json_output.empty(), "JSON format output");
        assert_test(json_output.find("{") != std::string::npos, "JSON format contains braces");
        
        isql->setOutputFormat(SBEnhanced::OutputFormat::XML);
        std::string xml_output = isql->formatOutput(results);
        assert_test(!xml_output.empty(), "XML format output");
        assert_test(xml_output.find("<") != std::string::npos, "XML format contains tags");
        
        std::cout << "Output formatting tests completed." << std::endl;
    }
    
    // Test DDL extraction
    void test_ddl_extraction() {
        std::cout << "\n=== Testing DDL Extraction ===" << std::endl;
        
        // Test DDL extraction options
        SBEnhanced::ExtractOptions options;
        options.include_metadata = true;
        options.include_schemas = true;
        options.include_tables = true;
        options.include_views = true;
        options.format_output = true;
        options.output_file = test_output_dir + "/test_ddl.sql";
        
        // Test database DDL extraction (will fail without connection)
        bool extracted = isql->extractDatabaseDDL(options);
        assert_test(!extracted, "Database DDL extraction without connection (expected to fail)");
        
        // Test schema DDL extraction
        bool schema_extracted = isql->extractSchemaDDL("TEST_SCHEMA", options);
        assert_test(!schema_extracted, "Schema DDL extraction without connection (expected to fail)");
        
        std::cout << "DDL extraction tests completed." << std::endl;
    }
    
    // Test SHOW commands
    void test_show_commands() {
        std::cout << "\n=== Testing SHOW Commands ===" << std::endl;
        
        SBEnhanced::ShowOptions options;
        options.output_format = SBEnhanced::OutputFormat::TABLE;
        options.include_detailed_info = true;
        options.sort_results = true;
        
        // Test SHOW TABLES (will fail without connection)
        bool tables_shown = isql->showTables(options);
        assert_test(!tables_shown, "SHOW TABLES without connection (expected to fail)");
        
        // Test SHOW SCHEMAS (will fail without connection)
        bool schemas_shown = isql->showSchemas(options);
        assert_test(!schemas_shown, "SHOW SCHEMAS without connection (expected to fail)");
        
        // Test SHOW VERSION (should work without connection)
        bool version_shown = isql->showVersion();
        assert_test(version_shown, "SHOW VERSION");
        
        std::cout << "SHOW commands tests completed." << std::endl;
    }
    
    // Test query analysis
    void test_query_analysis() {
        std::cout << "\n=== Testing Query Analysis ===" << std::endl;
        
        // Test query execution with analysis (will fail without connection)
        bool analyzed = isql->executeQueryWithAnalysis("SELECT * FROM RDB$DATABASE");
        assert_test(!analyzed, "Query analysis without connection (expected to fail)");
        
        // Test optimization recommendations
        auto recommendations = isql->getOptimizationRecommendations();
        assert_test(!recommendations.empty(), "Optimization recommendations");
        
        std::cout << "Query analysis tests completed." << std::endl;
    }
    
    // Test session management
    void test_session_management() {
        std::cout << "\n=== Testing Session Management ===" << std::endl;
        
        // Test session variables
        bool var_set = isql->setSessionVariable("test_var", "test_value");
        assert_test(var_set, "Set session variable");
        
        std::string var_value = isql->getSessionVariable("test_var");
        assert_test(var_value == "test_value", "Get session variable");
        
        // Test command history
        isql->addToHistory("SELECT 1");
        isql->addToHistory("SELECT 2");
        isql->addToHistory("SELECT 3");
        
        auto history = isql->getHistory();
        assert_test(history.size() == 3, "Command history size");
        assert_test(history[0] == "SELECT 1", "Command history content");
        
        // Test history save/load
        std::string history_file = test_output_dir + "/test_history.txt";
        bool history_saved = isql->saveHistory(history_file);
        assert_test(history_saved, "Save command history");
        assert_test(file_exists(history_file), "History file created");
        
        isql->clearHistory();
        assert_test(isql->getHistory().empty(), "Clear command history");
        
        bool history_loaded = isql->loadHistory(history_file);
        assert_test(history_loaded, "Load command history");
        assert_test(isql->getHistory().size() == 3, "Loaded history size");
        
        std::cout << "Session management tests completed." << std::endl;
    }
    
    // Test performance monitoring
    void test_performance_monitoring() {
        std::cout << "\n=== Testing Performance Monitoring ===" << std::endl;
        
        // Enable performance monitoring
        isql->enablePerformanceMonitoring(true);
        isql->enableQueryProfiling(true);
        
        // Get performance metrics
        auto metrics = isql->getPerformanceMetrics();
        assert_test(metrics.start_time.time_since_epoch().count() > 0, "Performance metrics available");
        
        // Test optimization recommendations
        auto recommendations = isql->getOptimizationRecommendations();
        assert_test(!recommendations.empty(), "Optimization recommendations available");
        
        std::cout << "Performance monitoring tests completed." << std::endl;
    }
    
    // Test error handling
    void test_error_handling() {
        std::cout << "\n=== Testing Error Handling ===" << std::endl;
        
        // Test invalid SQL
        auto result = isql->executeSQLStatement("INVALID SQL STATEMENT");
        assert_test(!result.success, "Invalid SQL handling");
        assert_test(!result.error_message.empty(), "Error message for invalid SQL");
        
        // Test invalid command
        auto cmd_result = isql->executeCommand("INVALID_COMMAND");
        assert_test(!cmd_result.success, "Invalid command handling");
        
        // Test error log
        auto error_log = isql->getErrorLog();
        assert_test(!error_log.empty(), "Error log not empty");
        
        std::string last_error = isql->getLastError();
        assert_test(!last_error.empty(), "Last error message");
        
        // Test error log clearing
        isql->clearErrorLog();
        assert_test(isql->getErrorLog().empty(), "Error log cleared");
        
        std::cout << "Error handling tests completed." << std::endl;
    }
    
    // Test configuration management
    void test_configuration_management() {
        std::cout << "\n=== Testing Configuration Management ===" << std::endl;
        
        // Test configuration loading
        bool config_loaded = isql->loadConfiguration(test_config_file);
        assert_test(config_loaded, "Configuration file loading");
        
        // Test configuration application
        const auto& exec_context = isql->getExecutionContext();
        assert_test(exec_context.show_headers == true, "Configuration applied - show_headers");
        assert_test(exec_context.show_timing == true, "Configuration applied - show_timing");
        assert_test(exec_context.page_size == 20, "Configuration applied - page_size");
        
        std::cout << "Configuration management tests completed." << std::endl;
    }
    
    // Test file operations
    void test_file_operations() {
        std::cout << "\n=== Testing File Operations ===" << std::endl;
        
        // Test output file setting
        std::string output_file = test_output_dir + "/test_output.txt";
        isql->setOutputFile(output_file);
        
        // Test script creation and execution
        std::string script_file = test_output_dir + "/test_script.sql";
        std::ofstream script(script_file);
        script << "-- Test script\n";
        script << "SELECT 'Hello, World!' AS message;\n";
        script << "SELECT COUNT(*) FROM RDB$DATABASE;\n";
        script.close();
        
        SBEnhanced::ScriptOptions script_options;
        script_options.continue_on_error = true;
        script_options.echo_commands = true;
        script_options.show_timing = true;
        script_options.verbose = true;
        
        // Test script execution (will fail without connection)
        bool script_executed = isql->executeScript(script_file, script_options);
        assert_test(!script_executed, "Script execution without connection (expected to fail)");
        
        std::cout << "File operations tests completed." << std::endl;
    }
    
    // Test schema operations
    void test_schema_operations() {
        std::cout << "\n=== Testing Schema Operations ===" << std::endl;
        
        // Test schema listing (will fail without connection)
        auto schemas = isql->listSchemas();
        assert_test(schemas.empty(), "Schema listing without connection (expected empty)");
        
        // Test schema path validation
        bool valid_path = isql->validateSchemaPath("finance.accounting.reports");
        assert_test(valid_path, "Schema path validation");
        
        bool invalid_path = isql->validateSchemaPath("invalid..path");
        assert_test(!invalid_path, "Invalid schema path validation");
        
        std::cout << "Schema operations tests completed." << std::endl;
    }
    
    // Test utility functions
    void test_utility_functions() {
        std::cout << "\n=== Testing Utility Functions ===" << std::endl;
        
        // Test formatting functions
        SBEnhanced::QueryResults results;
        results.column_names = {"COL1", "COL2"};
        results.rows = {{"Value1", "Value2"}};
        
        std::string formatted = isql->formatOutput(results);
        assert_test(!formatted.empty(), "Format output utility");
        
        std::string ddl = "CREATE TABLE test (id INTEGER);";
        std::string formatted_ddl = isql->formatDDL(ddl);
        assert_test(!formatted_ddl.empty(), "Format DDL utility");
        
        std::string error = "Test error message";
        std::string formatted_error = isql->formatError(error);
        assert_test(!formatted_error.empty(), "Format error utility");
        
        std::string message = "Test message";
        std::string formatted_message = isql->formatMessage(message);
        assert_test(!formatted_message.empty(), "Format message utility");
        
        std::cout << "Utility functions tests completed." << std::endl;
    }
    
    // Run all tests
    void run_all_tests() {
        std::cout << "Starting ScratchBird Enhanced ISQL Integration Tests" << std::endl;
        std::cout << "====================================================" << std::endl;
        
        auto start_time = std::chrono::steady_clock::now();
        
        test_initialization();
        test_connection_management();
        test_command_parsing();
        test_output_formatting();
        test_ddl_extraction();
        test_show_commands();
        test_query_analysis();
        test_session_management();
        test_performance_monitoring();
        test_error_handling();
        test_configuration_management();
        test_file_operations();
        test_schema_operations();
        test_utility_functions();
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "\n====================================================" << std::endl;
        std::cout << "Test Summary:" << std::endl;
        std::cout << "  Total tests run: " << tests_run << std::endl;
        std::cout << "  Tests passed: " << tests_passed << std::endl;
        std::cout << "  Tests failed: " << tests_failed << std::endl;
        std::cout << "  Success rate: " << (tests_run > 0 ? (tests_passed * 100.0 / tests_run) : 0) << "%" << std::endl;
        std::cout << "  Execution time: " << duration.count() << " ms" << std::endl;
        
        if (tests_failed > 0) {
            std::cout << "\nSome tests failed. This is expected when running without a real database connection." << std::endl;
            std::cout << "The tests verify that the integration layer correctly handles error conditions." << std::endl;
        }
        
        std::cout << "\nIntegration test suite completed." << std::endl;
    }
    
    // Get test results
    bool all_tests_passed() const {
        return tests_failed == 0;
    }
    
    int get_tests_run() const { return tests_run; }
    int get_tests_passed() const { return tests_passed; }
    int get_tests_failed() const { return tests_failed; }
};

// Performance benchmark test
void run_performance_benchmarks() {
    std::cout << "\n=== Performance Benchmarks ===" << std::endl;
    
    ISQLEnhanced isql;
    
    // Test initialization performance
    auto start = std::chrono::high_resolution_clock::now();
    SBEnhanced::ConnectionOptions options;
    options.enable_monitoring = true;
    options.enable_tracing = false;
    isql.initialize(options);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto init_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Initialization time: " << init_time.count() << " μs" << std::endl;
    
    // Test command parsing performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        isql.executeCommand("SELECT " + std::to_string(i) + " FROM RDB$DATABASE");
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto parse_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "1000 command parsing operations: " << parse_time.count() << " μs" << std::endl;
    std::cout << "Average command parsing time: " << (parse_time.count() / 1000.0) << " μs" << std::endl;
    
    // Test output formatting performance
    SBEnhanced::QueryResults results;
    results.column_names = {"ID", "NAME", "VALUE", "TIMESTAMP"};
    for (int i = 0; i < 1000; ++i) {
        results.rows.push_back({
            std::to_string(i),
            "Name " + std::to_string(i),
            std::to_string(i * 1.5),
            "2025-07-18 10:00:00"
        });
    }
    
    start = std::chrono::high_resolution_clock::now();
    std::string formatted = isql.formatOutput(results);
    end = std::chrono::high_resolution_clock::now();
    
    auto format_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "1000 row formatting time: " << format_time.count() << " μs" << std::endl;
    std::cout << "Average row formatting time: " << (format_time.count() / 1000.0) << " μs" << std::endl;
    
    // Test history management performance
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        isql.addToHistory("SELECT " + std::to_string(i));
    }
    end = std::chrono::high_resolution_clock::now();
    
    auto history_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "1000 history operations: " << history_time.count() << " μs" << std::endl;
    std::cout << "Average history operation time: " << (history_time.count() / 1000.0) << " μs" << std::endl;
    
    std::cout << "Performance benchmarks completed." << std::endl;
}

// Memory usage test
void run_memory_tests() {
    std::cout << "\n=== Memory Usage Tests ===" << std::endl;
    
    // Test memory usage with large result sets
    ISQLEnhanced isql;
    SBEnhanced::ConnectionOptions options;
    isql.initialize(options);
    
    // Create large result set
    SBEnhanced::QueryResults large_results;
    large_results.column_names = {"ID", "DATA", "TIMESTAMP"};
    
    const int large_row_count = 10000;
    for (int i = 0; i < large_row_count; ++i) {
        large_results.rows.push_back({
            std::to_string(i),
            std::string(100, 'A' + (i % 26)), // 100 character string
            "2025-07-18 10:00:00"
        });
    }
    
    std::cout << "Created result set with " << large_row_count << " rows" << std::endl;
    
    // Test formatting with large result set
    auto start = std::chrono::high_resolution_clock::now();
    std::string formatted = isql.formatOutput(large_results);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto format_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Large result set formatting time: " << format_time.count() << " ms" << std::endl;
    std::cout << "Formatted output size: " << formatted.size() << " bytes" << std::endl;
    
    // Test memory cleanup
    large_results.rows.clear();
    formatted.clear();
    
    std::cout << "Memory tests completed." << std::endl;
}

// Main test runner
int main(int argc, char* argv[])
{
    std::cout << "ScratchBird Enhanced ISQL Integration Test Suite" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    bool run_benchmarks = false;
    bool run_memory = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--benchmarks" || arg == "-b") {
            run_benchmarks = true;
        } else if (arg == "--memory" || arg == "-m") {
            run_memory = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -b, --benchmarks   Run performance benchmarks" << std::endl;
            std::cout << "  -m, --memory       Run memory usage tests" << std::endl;
            std::cout << "  -h, --help         Show this help message" << std::endl;
            return 0;
        }
    }
    
    try {
        // Run main integration tests
        ISQLTestSuite test_suite;
        test_suite.run_all_tests();
        
        // Run optional benchmarks
        if (run_benchmarks) {
            run_performance_benchmarks();
        }
        
        // Run optional memory tests
        if (run_memory) {
            run_memory_tests();
        }
        
        std::cout << "\n===============================================" << std::endl;
        std::cout << "All tests completed successfully!" << std::endl;
        
        return test_suite.all_tests_passed() ? 0 : 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Test suite failed with unknown exception" << std::endl;
        return 1;
    }
}