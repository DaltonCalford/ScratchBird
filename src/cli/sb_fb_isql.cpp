/**
 * sb_fb_isql - ScratchBird Firebird Interactive SQL Shell
 *
 * CLI Tools - Interactive SQL utility using Firebird SQL syntax.
 * Uses FirebirdQueryCompiler to parse Firebird SQL and compile to SBLR bytecode.
 *
 * Usage:
 *   sb_fb_isql <database_path> [options]
 *
 * Options:
 *   -c, --command=<sql>      Execute single command and exit
 *   -f, --file=<file>        Execute commands from file and exit
 *   -q, --quiet              Quiet mode (no welcome message)
 *   -s, --dialect=<n>        SQL dialect (1, 2, or 3; default: 3)
 *   --stats                  Show compilation/execution statistics
 *   -h, --help               Show this help
 *   --version                Show version
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <string_view>
#include <signal.h>

#include "scratchbird/core/database.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/firebird_query_compiler.h"
#include "scratchbird/sblr/executor.h"
// Note: EmulationViewGenerator has API mismatches - views created manually for now

using namespace scratchbird;

// =============================================================================
// Configuration
// =============================================================================

struct FbIsqlConfig {
    std::string database_path;     // ScratchBird database file path
    std::string command;           // -c: single command
    std::string input_file;        // -f: input file
    bool quiet = false;            // -q: no welcome
    bool show_stats = false;       // --stats: show statistics
    int sql_dialect = 3;           // -s: SQL dialect (1, 2, or 3)
    std::string term = ";";        // Statement terminator
    bool heading = true;           // Show column headings
    bool count = true;             // Show row counts
};

// Firebird emulation session state
struct FbEmulationState {
    std::string server_name = "localhost";  // Current Firebird server (emulation)
    std::string database_name;               // Current Firebird database name
    core::ID database_schema_id;             // Schema ID for current database
    bool connected = false;                  // True if connected to a Firebird database
};

// =============================================================================
// Global state
// =============================================================================

static bool g_running = true;
static FbIsqlConfig g_config;
static FbEmulationState g_fb_state;
static std::unique_ptr<core::ConnectionContext> g_conn_ctx;

void updateConnectionContext(const std::string& schema_path, const core::ID& schema_id) {
    if (!g_conn_ctx) {
        return;
    }

    g_conn_ctx->set_dialect_tag("FIREBIRD");
    g_conn_ctx->setCurrentSchemaId(schema_id);
    g_conn_ctx->set_current_schema(schema_path);
    g_conn_ctx->set_search_path({schema_path});
}

// =============================================================================
// Signal handling
// =============================================================================

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n^C\n";
        g_running = false;
    }
}

// =============================================================================
// Output helpers
// =============================================================================

void printRow(const std::vector<std::string>& values, const std::vector<size_t>& widths) {
    std::cout << "|";
    for (size_t i = 0; i < values.size(); ++i) {
        std::cout << " " << std::left << std::setw(static_cast<int>(widths[i])) << values[i] << " |";
    }
    std::cout << "\n";
}

void printSeparator(const std::vector<size_t>& widths) {
    std::cout << "+";
    for (size_t w : widths) {
        for (size_t i = 0; i < w + 2; ++i) std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";
}

void printResultSet(const sblr::ResultSet& results) {
    size_t col_count = results.columnCount();
    size_t row_count = results.rowCount();

    if (col_count == 0) {
        return;
    }

    // Build column names
    std::vector<std::string> columns;
    for (size_t i = 0; i < col_count; ++i) {
        columns.push_back(results.columnName(i));
    }

    // Calculate column widths
    std::vector<size_t> widths(col_count);
    for (size_t i = 0; i < col_count; ++i) {
        widths[i] = columns[i].length();
    }

    for (size_t r = 0; r < row_count; ++r) {
        for (size_t c = 0; c < col_count; ++c) {
            std::string val = results.getValue(r, c).toString();
            widths[c] = std::max(widths[c], val.length());
        }
    }

    // Print header
    if (g_config.heading) {
        printSeparator(widths);
        printRow(columns, widths);
        printSeparator(widths);
    }

    // Print rows
    for (size_t r = 0; r < row_count; ++r) {
        std::vector<std::string> values;
        for (size_t c = 0; c < col_count; ++c) {
            values.push_back(results.getValue(r, c).toString());
        }
        printRow(values, widths);
    }

    if (g_config.heading) {
        printSeparator(widths);
    }

    // Print count
    if (g_config.count) {
        std::cout << "(" << row_count << " row" << (row_count != 1 ? "s" : "") << ")\n";
    }
}

// =============================================================================
// SQL Execution
// =============================================================================

// Forward declaration for handleFirebirdDatabaseCommand
bool handleFirebirdDatabaseCommand(const std::string& sql, core::Database& db);

bool executeSQL(const std::string& sql, sblr::FirebirdQueryCompiler& compiler, sblr::Executor& executor, core::Database& db) {
    // Skip empty or whitespace-only SQL
    std::string trimmed = sql;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t' || trimmed.front() == '\n')) {
        trimmed.erase(0, 1);
    }
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\n')) {
        trimmed.pop_back();
    }
    if (trimmed.empty()) {
        return true;
    }

    // Check for Firebird database commands (CREATE DATABASE, CONNECT, DROP DATABASE)
    if (handleFirebirdDatabaseCommand(trimmed, db)) {
        // Update compiler's schema context if connected
        if (g_fb_state.connected) {
            compiler.setCurrentSchema(g_fb_state.database_schema_id);
        }
        return true;
    }

    // Ensure we're connected to a Firebird database for DDL/DML
    if (!g_fb_state.connected) {
        std::cerr << "Warning: No Firebird database connected. Use CREATE DATABASE or CONNECT first.\n";
        // Allow query execution anyway for basic queries
        executor.clearCurrentSchema();
    } else {
        // Set compiler's and executor's schema context
        compiler.setCurrentSchema(g_fb_state.database_schema_id);
        executor.setCurrentSchema(g_fb_state.database_schema_id);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Compile SQL to bytecode using Firebird parser
    sblr::FirebirdCompilationResult comp_result = compiler.compile(trimmed);

    auto compile_end = std::chrono::high_resolution_clock::now();

    if (!comp_result.success()) {
        for (const auto& err : comp_result.errors()) {
            std::cerr << "Error: " << err << "\n";
        }
        return false;
    }

    // Print warnings
    for (const auto& warn : comp_result.warnings()) {
        std::cerr << "Warning: " << warn << "\n";
    }

    // Execute bytecode
    sblr::ExecutionResult exec_result = executor.execute(comp_result.bytecode());

    auto exec_end = std::chrono::high_resolution_clock::now();

    if (!exec_result.success()) {
        std::cerr << "Error: " << exec_result.error() << "\n";
        return false;
    }

    // Display results
    if (exec_result.hasResultSet() && exec_result.resultSet()) {
        printResultSet(*exec_result.resultSet());
    } else if (exec_result.affectedCount() > 0) {
        std::cout << exec_result.affectedCount() << " row" << (exec_result.affectedCount() != 1 ? "s" : "") << " affected\n";
    } else {
        std::cout << "Statement executed successfully.\n";
    }

    // Print statistics if enabled
    if (g_config.show_stats) {
        auto compile_time = std::chrono::duration_cast<std::chrono::microseconds>(compile_end - start);
        auto exec_time = std::chrono::duration_cast<std::chrono::microseconds>(exec_end - compile_end);
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(exec_end - start);

        std::cout << "Stats: compile=" << compile_time.count() << "us"
                  << " exec=" << exec_time.count() << "us"
                  << " total=" << total_time.count() << "us\n";

        const auto& stats = comp_result.stats();
        if (stats.bytecode_size > 0) {
            std::cout << "Bytecode: " << stats.bytecode_size << " bytes\n";
        }
    }

    return true;
}

// =============================================================================
// Firebird Database Emulation Commands
// =============================================================================

struct FirebirdDatabaseSpec {
    std::string server;
    std::string file_path;
};

FirebirdDatabaseSpec parseFirebirdDatabaseSpec(std::string_view spec) {
    FirebirdDatabaseSpec result;
    result.file_path = std::string(spec);

    size_t colon = result.file_path.find(':');
    if (colon != std::string::npos) {
        bool is_drive = (colon == 1 &&
                         std::isalpha(static_cast<unsigned char>(result.file_path[0])) &&
                         result.file_path.size() > 2 &&
                         (result.file_path[2] == '\\' || result.file_path[2] == '/'));
        if (!is_drive) {
            result.server = result.file_path.substr(0, colon);
            result.file_path.erase(0, colon + 1);
        }
    }

    return result;
}

std::vector<std::string> splitFirebirdPathComponents(std::string_view path) {
    std::string working(path);
    std::vector<std::string> components;

    if (working.size() >= 2 && std::isalpha(static_cast<unsigned char>(working[0])) &&
        working[1] == ':') {
        std::string drive(1, static_cast<char>(std::tolower(static_cast<unsigned char>(working[0]))));
        components.push_back(drive);
        working.erase(0, 2);
    }

    while (!working.empty() && (working.front() == '/' || working.front() == '\\')) {
        working.erase(working.begin());
    }

    std::string current;
    for (char ch : working) {
        if (ch == '/' || ch == '\\') {
            if (!current.empty()) {
                components.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        components.push_back(current);
    }

    if (!components.empty()) {
        components.pop_back();
    }

    return components;
}

std::string deriveFirebirdDatabaseName(std::string_view file_path) {
    size_t last_sep = file_path.find_last_of("/\\");
    std::string base = (last_sep == std::string_view::npos)
        ? std::string(file_path)
        : std::string(file_path.substr(last_sep + 1));

    if (base.empty()) {
        return base;
    }

    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < base.size()) {
        std::string ext = base.substr(dot + 1);
        for (char& ch : ext) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (ext == "fdb" || ext == "gdb") {
            base = base.substr(0, dot);
        }
    }

    return base;
}

bool buildFirebirdSchemaPath(const std::string& db_path,
                             const std::string& default_server,
                             std::string& schema_path_out,
                             std::string& db_name_out,
                             std::string& server_out) {
    std::string normalized = db_path;
    if (normalized.size() >= 2 && normalized.front() == '\'' && normalized.back() == '\'') {
        normalized = normalized.substr(1, normalized.size() - 2);
    }

    FirebirdDatabaseSpec spec = parseFirebirdDatabaseSpec(normalized);
    server_out = spec.server.empty() ? default_server : spec.server;
    db_name_out = deriveFirebirdDatabaseName(spec.file_path);
    if (db_name_out.empty()) {
        return false;
    }

    auto path_components = splitFirebirdPathComponents(spec.file_path);
    std::string schema = "remote.emulation.firebird." + server_out;
    for (const auto& comp : path_components) {
        if (!comp.empty()) {
            schema.push_back('.');
            schema += comp;
        }
    }
    schema.push_back('.');
    schema += db_name_out;
    schema_path_out = schema;
    return true;
}

/**
 * Ensure the Firebird emulation schema hierarchy exists
 * Creates: remote.emulation.firebird, remote.emulation.firebird.{server}
 */
bool ensureFirebirdSchemaHierarchy(core::CatalogManager* catalog, const std::string& server) {
    if (!catalog) return false;

    std::string path = "remote.emulation.firebird." + server;
    core::ErrorContext ctx;
    core::ID schema_id;
    core::Status status = catalog->createSchemaPath(
        path, core::CatalogManager::SchemaType::REMOTE_EMULATED, schema_id, &ctx);
    if (status != core::Status::OK) {
        std::cerr << "Error creating schema hierarchy (" << path << "): " << ctx.message << "\n";
        return false;
    }
    return true;
}

/**
 * Handle CREATE DATABASE command
 * Creates schema at remote.emulation.firebird.{server}.{path...}.{database}
 * and generates RDB$ system views
 */
bool handleCreateDatabase(const std::string& dbPath, core::Database& db) {
    std::string dbName;
    std::string schemaPath;
    std::string serverName;
    if (!buildFirebirdSchemaPath(dbPath, g_fb_state.server_name, schemaPath, dbName, serverName)) {
        std::cerr << "Error: Invalid database name\n";
        return false;
    }
    if (!serverName.empty()) {
        g_fb_state.server_name = serverName;
    }

    core::CatalogManager* catalog = db.catalog_manager();
    if (!catalog) {
        std::cerr << "Error: Catalog manager not available\n";
        return false;
    }

    // Ensure the Firebird emulation schema hierarchy exists
    if (!ensureFirebirdSchemaHierarchy(catalog, g_fb_state.server_name)) {
        return false;
    }

    core::ErrorContext ctx;

    // Create schema using hierarchical path API
    core::ID schemaId;
    core::CatalogManager::SchemaInfo schemaInfo;
    core::Status status = catalog->getSchema(schemaPath, schemaInfo, &ctx);
    if (status == core::Status::OK) {
        schemaId = schemaInfo.schema_id;
    } else if (status == core::Status::NOT_FOUND || status == core::Status::INVALID_ARGUMENT) {
        status = catalog->createSchemaPath(schemaPath,
                                           core::CatalogManager::SchemaType::REMOTE_EMULATED,
                                           schemaId,
                                           &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Error creating database schema: " << ctx.message << "\n";
            return false;
        }
    } else {
        std::cerr << "Error resolving database schema: " << ctx.message << "\n";
        return false;
    }

    // Ensure emulation metadata records exist for this database
    core::CatalogManager::EmulationTypeInfo type_info;
    status = catalog->getEmulationTypeByName("firebird", type_info, &ctx);
    if (status != core::Status::OK && status != core::Status::NOT_FOUND) {
        std::cerr << "Error resolving emulation type: " << ctx.message << "\n";
        return false;
    }
    if (status == core::Status::NOT_FOUND) {
        status = catalog->createEmulationType("firebird",
                                              1, 0,  // version 1.0
                                              "",    // empty mapping rules
                                              type_info.emulation_type_id,
                                              &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Error creating emulation type: " << ctx.message << "\n";
            return false;
        }
    }

    core::CatalogManager::EmulationServerInfo server_info;
    status = catalog->getEmulationServerByName(g_fb_state.server_name, server_info, &ctx);
    if (status != core::Status::OK && status != core::Status::NOT_FOUND) {
        std::cerr << "Error resolving emulation server: " << ctx.message << "\n";
        return false;
    }
    if (status == core::Status::NOT_FOUND) {
        core::ID server_id;
        status = catalog->createEmulationServer(g_fb_state.server_name,
                                                type_info.emulation_type_id,
                                                std::string(),
                                                server_id,
                                                &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Error creating emulation server: " << ctx.message << "\n";
            return false;
        }
        server_info.server_id = server_id;
        server_info.server_name = g_fb_state.server_name;
        server_info.emulation_type_id = type_info.emulation_type_id;
    } else if (server_info.emulation_type_id != type_info.emulation_type_id) {
        std::cerr << "Error: Emulation server type mismatch\n";
        return false;
    }

    core::CatalogManager::EmulatedDatabaseInfo db_info;
    status = catalog->getEmulatedDatabaseByName(server_info.server_id, dbName, db_info, &ctx);
    if (status != core::Status::OK && status != core::Status::NOT_FOUND) {
        std::cerr << "Error resolving emulated database: " << ctx.message << "\n";
        return false;
    }
    if (status == core::Status::NOT_FOUND) {
        core::ID emulated_db_id;
        status = catalog->createEmulatedDatabase(dbName,
                                                 server_info.server_id,
                                                 schemaId,
                                                 std::string(),
                                                 emulated_db_id,
                                                 &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Error creating emulated database record: " << ctx.message << "\n";
            return false;
        }
    }

    // Note: RDB$ system views would be created here by EmulationViewGenerator.
    // Integration is pending; views are still created in adapter bootstrap paths.

    // Auto-connect to the newly created database
    g_fb_state.database_name = dbName;
    g_fb_state.database_schema_id = schemaId;
    g_fb_state.connected = true;
    updateConnectionContext(schemaPath, schemaId);

    std::cout << "Database '" << dbName << "' created.\n";
    std::cout << "Connected to database: " << schemaPath << "\n";

    return true;
}

/**
 * Handle CONNECT command
 * Switches to schema at remote.emulation.firebird.{server}.{path...}.{database}
 */
bool handleConnect(const std::string& dbPath, core::Database& db) {
    std::string dbName;
    std::string schemaPath;
    std::string serverName;
    if (!buildFirebirdSchemaPath(dbPath, g_fb_state.server_name, schemaPath, dbName, serverName)) {
        std::cerr << "Error: Invalid database name\n";
        return false;
    }
    if (!serverName.empty()) {
        g_fb_state.server_name = serverName;
    }

    core::CatalogManager* catalog = db.catalog_manager();
    if (!catalog) {
        std::cerr << "Error: Catalog manager not available\n";
        return false;
    }

    core::ErrorContext ctx;

    // Look up the schema
    core::CatalogManager::SchemaInfo schemaInfo;
    core::Status status = catalog->getSchema(schemaPath, schemaInfo, &ctx);

    if (status != core::Status::OK) {
        std::cerr << "Error: Database '" << dbName << "' does not exist.\n";
        std::cerr << "Use CREATE DATABASE '" << dbName << "' to create it.\n";
        return false;
    }

    g_fb_state.database_name = dbName;
    g_fb_state.database_schema_id = schemaInfo.schema_id;
    g_fb_state.connected = true;
    updateConnectionContext(schemaPath, schemaInfo.schema_id);

    std::cout << "Connected to database: " << schemaPath << "\n";
    return true;
}

/**
 * Handle DROP DATABASE command
 * Drops schema at remote.emulation.firebird.{server}.{path...}.{database}
 */
bool handleDropDatabase(const std::string& dbPath, core::Database& db) {
    std::string dbName;
    std::string schemaPath;
    std::string serverName;
    if (!buildFirebirdSchemaPath(dbPath, g_fb_state.server_name, schemaPath, dbName, serverName)) {
        std::cerr << "Error: Invalid database name\n";
        return false;
    }
    if (!serverName.empty()) {
        g_fb_state.server_name = serverName;
    }

    core::CatalogManager* catalog = db.catalog_manager();
    if (!catalog) {
        std::cerr << "Error: Catalog manager not available\n";
        return false;
    }

    core::ErrorContext ctx;

    // Look up the schema
    core::CatalogManager::SchemaInfo schemaInfo;
    core::Status status = catalog->getSchema(schemaPath, schemaInfo, &ctx);

    if (status != core::Status::OK) {
        std::cerr << "Error: Database '" << dbName << "' does not exist.\n";
        return false;
    }

    // Drop the schema (cascade to drop all objects)
    status = catalog->dropSchema(schemaInfo.schema_id, true, &ctx);

    if (status != core::Status::OK) {
        std::cerr << "Error dropping database: " << ctx.message << "\n";
        return false;
    }

    // Drop emulated database record if present
    core::CatalogManager::EmulationServerInfo server_info;
    status = catalog->getEmulationServerByName(g_fb_state.server_name, server_info, &ctx);
    if (status == core::Status::OK) {
        core::CatalogManager::EmulatedDatabaseInfo db_info;
        status = catalog->getEmulatedDatabaseByName(server_info.server_id, dbName, db_info, &ctx);
        if (status == core::Status::OK) {
            status = catalog->dropEmulatedDatabase(db_info.emulated_db_id, &ctx);
            if (status != core::Status::OK) {
                std::cerr << "Error dropping emulated database record: " << ctx.message << "\n";
                return false;
            }
        } else if (status != core::Status::NOT_FOUND) {
            std::cerr << "Error resolving emulated database record: " << ctx.message << "\n";
            return false;
        }
    } else if (status != core::Status::NOT_FOUND) {
        std::cerr << "Error resolving emulation server: " << ctx.message << "\n";
        return false;
    }

    // If we were connected to this database, disconnect
    if (g_fb_state.connected && g_fb_state.database_name == dbName) {
        g_fb_state.connected = false;
        g_fb_state.database_name.clear();
    }

    std::cout << "Database '" << dbName << "' dropped.\n";
    return true;
}

/**
 * Check if SQL is a Firebird database command and handle it
 * Returns true if handled, false if should be passed to normal SQL execution
 */
bool handleFirebirdDatabaseCommand(const std::string& sql, core::Database& db) {
    std::string upper_sql = sql;
    std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);

    // CREATE DATABASE 'path'
    if (upper_sql.rfind("CREATE DATABASE ", 0) == 0) {
        std::string rest = sql.substr(16);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(0, 1);
        }
        return handleCreateDatabase(rest, db);
    }

    // CONNECT 'path' or CONNECT TO 'path'
    if (upper_sql.rfind("CONNECT ", 0) == 0) {
        std::string rest = sql.substr(8);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(0, 1);
        }
        // Handle CONNECT TO syntax
        std::string upper_rest = rest;
        std::transform(upper_rest.begin(), upper_rest.end(), upper_rest.begin(), ::toupper);
        if (upper_rest.rfind("TO ", 0) == 0) {
            rest = rest.substr(3);
            while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
                rest.erase(0, 1);
            }
        }
        return handleConnect(rest, db);
    }

    // DROP DATABASE 'path' or DROP DATABASE
    if (upper_sql.rfind("DROP DATABASE", 0) == 0) {
        std::string rest = sql.substr(13);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(0, 1);
        }
        // If no path given, use current database
        if (rest.empty()) {
            if (!g_fb_state.connected) {
                std::cerr << "Error: No database connected.\n";
                return true;
            }
            rest = g_fb_state.database_name;
        }
        return handleDropDatabase(rest, db);
    }

    return false;  // Not a database command
}

// =============================================================================
// SET Command Handler
// =============================================================================

bool handleSetCommand(const std::string& sql) {
    std::string upper_sql = sql;
    std::transform(upper_sql.begin(), upper_sql.end(), upper_sql.begin(), ::toupper);

    // SET TERM - change statement terminator
    if (upper_sql.rfind("SET TERM ", 0) == 0 || upper_sql.rfind("SET TERM\t", 0) == 0) {
        std::string rest = sql.substr(9);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(0, 1);
        }
        // Extract new terminator (first word)
        size_t pos = rest.find(' ');
        if (pos != std::string::npos) {
            rest = rest.substr(0, pos);
        }
        pos = rest.find('\t');
        if (pos != std::string::npos) {
            rest = rest.substr(0, pos);
        }
        if (!rest.empty()) {
            g_config.term = rest;
            std::cout << "Statement terminator set to: " << g_config.term << "\n";
            return true;
        }
    }

    // SET SQL DIALECT
    if (upper_sql.rfind("SET SQL DIALECT ", 0) == 0 || upper_sql.rfind("SET SQL DIALECT\t", 0) == 0) {
        std::string rest = sql.substr(16);
        while (!rest.empty() && (rest.front() == ' ' || rest.front() == '\t')) {
            rest.erase(0, 1);
        }
        try {
            int dialect = std::stoi(rest);
            if (dialect >= 1 && dialect <= 3) {
                g_config.sql_dialect = dialect;
                std::cout << "SQL Dialect set to: " << dialect << "\n";
                return true;
            }
        } catch (...) {}
        std::cerr << "Invalid dialect. Use 1, 2, or 3.\n";
        return true;
    }

    // SET COUNT ON/OFF
    if (upper_sql == "SET COUNT" || upper_sql == "SET COUNT ON") {
        g_config.count = true;
        std::cout << "COUNT is ON\n";
        return true;
    }
    if (upper_sql == "SET COUNT OFF") {
        g_config.count = false;
        std::cout << "COUNT is OFF\n";
        return true;
    }

    // SET HEADING ON/OFF
    if (upper_sql == "SET HEADING" || upper_sql == "SET HEADING ON") {
        g_config.heading = true;
        std::cout << "HEADING is ON\n";
        return true;
    }
    if (upper_sql == "SET HEADING OFF") {
        g_config.heading = false;
        std::cout << "HEADING is OFF\n";
        return true;
    }

    // SET STATS ON/OFF
    if (upper_sql == "SET STATS" || upper_sql == "SET STATS ON") {
        g_config.show_stats = true;
        std::cout << "STATS is ON\n";
        return true;
    }
    if (upper_sql == "SET STATS OFF") {
        g_config.show_stats = false;
        std::cout << "STATS is OFF\n";
        return true;
    }

    return false;  // Not a SET command
}

// =============================================================================
// Meta-command Handler
// =============================================================================

bool handleMetaCommand(const std::string& cmd, sblr::FirebirdQueryCompiler& compiler, sblr::Executor& executor, core::Database& db) {
    if (cmd.empty() || cmd[0] != '\\') {
        return false;
    }

    std::string command = cmd.substr(1);
    std::string arg;
    size_t space = command.find(' ');
    if (space != std::string::npos) {
        arg = command.substr(space + 1);
        command = command.substr(0, space);
    }

    // Convert command to lowercase
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    if (command == "q" || command == "quit" || command == "exit") {
        g_running = false;
        return true;
    }

    if (command == "?" || command == "help") {
        std::cout << "Meta-commands:\n"
                  << "  \\q, \\quit, \\exit    Quit\n"
                  << "  \\?                  Show this help\n"
                  << "  \\d                  List tables\n"
                  << "  \\d <table>          Describe table\n"
                  << "  \\di                 List indexes\n"
                  << "  \\dg                 List generators (sequences)\n"
                  << "\nSET commands (Firebird ISQL compatible):\n"
                  << "  SET TERM <char>         Change statement terminator\n"
                  << "  SET SQL DIALECT <n>     Set SQL dialect (1, 2, or 3)\n"
                  << "  SET COUNT [ON|OFF]      Show row counts\n"
                  << "  SET HEADING [ON|OFF]    Show column headings\n"
                  << "  SET STATS [ON|OFF]      Show execution statistics\n";
        return true;
    }

    if (command == "d") {
        if (arg.empty()) {
            return executeSQL("SELECT table_name FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0", compiler, executor, db);
        } else {
            return executeSQL("SELECT RDB$FIELD_NAME, RDB$FIELD_SOURCE FROM RDB$RELATION_FIELDS WHERE RDB$RELATION_NAME = '" + arg + "'", compiler, executor, db);
        }
    }

    if (command == "di") {
        return executeSQL("SELECT index_name FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0", compiler, executor, db);
    }

    if (command == "dg") {
        return executeSQL("SELECT generator_name, gen_id FROM RDB$GENERATORS WHERE RDB$SYSTEM_FLAG = 0", compiler, executor, db);
    }

    std::cerr << "Unknown command: \\" << command << "\n";
    std::cerr << "Type \\? for help\n";
    return true;
}

// =============================================================================
// File execution
// =============================================================================

bool executeFile(const std::string& filename, sblr::FirebirdQueryCompiler& compiler, sblr::Executor& executor, core::Database& db) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error: Cannot open file: " << filename << "\n";
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    bool has_script_marker = false;
    for (const auto& ln : lines) {
        if (ln.rfind("-- ===", 0) == 0) {
            has_script_marker = true;
            break;
        }
    }

    std::string sql_buffer;
    bool success = true;
    bool in_script = !has_script_marker;

    for (const auto& current_line : lines) {
        if (has_script_marker && current_line.rfind("-- ===", 0) == 0) {
            in_script = true;
            continue;
        }

        if (!in_script) {
            continue;
        }

        line = current_line;
        // Skip comments
        if (line.rfind("--", 0) == 0) {
            continue;
        }

        sql_buffer += line + "\n";

        // Check if ends with terminator
        std::string trimmed = sql_buffer;
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\n' || trimmed.back() == '\t')) {
            trimmed.pop_back();
        }

        if (trimmed.size() >= g_config.term.size() &&
            trimmed.substr(trimmed.size() - g_config.term.size()) == g_config.term) {
            // Remove terminator
            std::string sql = trimmed.substr(0, trimmed.size() - g_config.term.size());

            // Check for SET commands first
            if (!handleSetCommand(sql)) {
                if (!executeSQL(sql, compiler, executor, db)) {
                    success = false;
                }
            }
            sql_buffer.clear();
        }
    }

    // Execute any remaining SQL
    if (!sql_buffer.empty()) {
        std::string trimmed = sql_buffer;
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\n' || trimmed.back() == '\t')) {
            trimmed.pop_back();
        }
        if (!trimmed.empty() && !handleSetCommand(trimmed)) {
            if (!executeSQL(trimmed, compiler, executor, db)) {
                success = false;
            }
        }
    }

    return success;
}

// =============================================================================
// Usage and version
// =============================================================================

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " <database_path> [options]\n"
              << "\n"
              << "Options:\n"
              << "  -c, --command=<sql>      Execute single command and exit\n"
              << "  -f, --file=<file>        Execute commands from file and exit\n"
              << "  -q, --quiet              Quiet mode (no welcome message)\n"
              << "  -s, --dialect=<n>        SQL dialect (1, 2, or 3; default: 3)\n"
              << "  --stats                  Show compilation/execution statistics\n"
              << "  -h, --help               Show this help\n"
              << "  --version                Show version\n"
              << "\n"
              << "This tool uses Firebird SQL syntax via the FirebirdQueryCompiler.\n"
              << "Firebird-specific syntax like SELECT FIRST n SKIP m is supported.\n";
}

void printVersion() {
    std::cout << "sb_fb_isql - ScratchBird Firebird Interactive SQL Shell\n"
              << "Version: 1.0.0\n"
              << "Firebird SQL Dialect: 1, 2, 3 supported (default: 3)\n";
}

// =============================================================================
// Argument parsing
// =============================================================================

bool parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        }

        if (arg == "--version") {
            printVersion();
            exit(0);
        }

        if (arg == "-q" || arg == "--quiet") {
            g_config.quiet = true;
            continue;
        }

        if (arg == "--stats") {
            g_config.show_stats = true;
            continue;
        }

        if (arg == "-c" && i + 1 < argc) {
            g_config.command = argv[++i];
            continue;
        }
        if (arg.rfind("-c=", 0) == 0) {
            g_config.command = arg.substr(3);
            continue;
        }
        if (arg.rfind("--command=", 0) == 0) {
            g_config.command = arg.substr(10);
            continue;
        }

        if (arg == "-f" && i + 1 < argc) {
            g_config.input_file = argv[++i];
            continue;
        }
        if (arg.rfind("-f=", 0) == 0) {
            g_config.input_file = arg.substr(3);
            continue;
        }
        if (arg.rfind("--file=", 0) == 0) {
            g_config.input_file = arg.substr(7);
            continue;
        }

        if (arg == "-s" && i + 1 < argc) {
            g_config.sql_dialect = std::stoi(argv[++i]);
            continue;
        }
        if (arg.rfind("-s=", 0) == 0) {
            g_config.sql_dialect = std::stoi(arg.substr(3));
            continue;
        }
        if (arg.rfind("--dialect=", 0) == 0) {
            g_config.sql_dialect = std::stoi(arg.substr(10));
            continue;
        }

        // Positional argument = database path
        if (arg[0] != '-') {
            g_config.database_path = arg;
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        printUsage(argv[0]);
        return false;
    }

    if (g_config.database_path.empty()) {
        std::cerr << "Error: Database path required\n";
        printUsage(argv[0]);
        return false;
    }

    return true;
}

// =============================================================================
// Interactive REPL
// =============================================================================

void runInteractive(sblr::FirebirdQueryCompiler& compiler, sblr::Executor& executor, core::Database& db) {
    if (!g_config.quiet) {
        std::cout << "ScratchBird Firebird Interactive SQL Shell\n"
                  << "ScratchBird Database: " << g_config.database_path << "\n"
                  << "SQL Dialect: " << g_config.sql_dialect << "\n"
                  << "Type \\? for help, \\q to quit.\n"
                  << "\nUse CREATE DATABASE 'name' to create a Firebird database.\n"
                  << "Use CONNECT 'name' to switch to an existing database.\n\n";
    }

    std::string sql_buffer;
    std::string prompt = "SQL> ";
    std::string cont_prompt = "CON> ";

    while (g_running) {
        // Show prompt with current database
        std::string current_prompt = prompt;
        if (g_fb_state.connected) {
            current_prompt = g_fb_state.database_name + ":" + prompt;
        }
        std::cout << (sql_buffer.empty() ? current_prompt : cont_prompt);
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line)) {
            break;  // EOF
        }

        // Handle empty line
        if (line.empty()) {
            continue;
        }

        // Handle meta-commands (start with \)
        if (line[0] == '\\' && sql_buffer.empty()) {
            handleMetaCommand(line, compiler, executor, db);
            continue;
        }

        // Append line to buffer
        sql_buffer += line + "\n";

        // Check if ends with terminator
        std::string trimmed = sql_buffer;
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\n' || trimmed.back() == '\t')) {
            trimmed.pop_back();
        }

        if (trimmed.size() >= g_config.term.size() &&
            trimmed.substr(trimmed.size() - g_config.term.size()) == g_config.term) {
            // Remove terminator
            std::string sql = trimmed.substr(0, trimmed.size() - g_config.term.size());

            // Check for SET commands first
            if (!handleSetCommand(sql)) {
                executeSQL(sql, compiler, executor, db);
            }
            sql_buffer.clear();
        }
    }

    std::cout << "Goodbye.\n";
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (!parseArgs(argc, argv)) {
        return 1;
    }

    // Set up signal handler
    signal(SIGINT, signalHandler);

    // Open database (auto-create if doesn't exist)
    core::Database db;
    core::ErrorContext ctx;

    // Try to open existing database first
    core::Status status = db.open(g_config.database_path, &ctx);
    if (status != core::Status::OK) {
        // Database doesn't exist - create it
        // (Firebird emulation operates on schemas inside the database, not the database file itself)
        core::ErrorContext create_ctx;
        status = core::Database::create(g_config.database_path, 16384, &create_ctx);
        if (status != core::Status::OK) {
            std::cerr << "Error creating database: " << create_ctx.message << "\n";
            return 1;
        }
        // Now open the newly created database
        core::ErrorContext open_ctx;
        status = db.open(g_config.database_path, &open_ctx);
        if (status != core::Status::OK) {
            std::cerr << "Error opening database after create: " << open_ctx.message << "\n";
            return 1;
        }
    if (!g_config.quiet) {
        std::cout << "Created ScratchBird database: " << g_config.database_path << "\n";
    }
}

    // Create connection context for permissions/search_path
    core::Status conn_status = db.connect(g_conn_ctx, &ctx);
    if (conn_status != core::Status::OK || !g_conn_ctx) {
        std::cerr << "Error: Failed to initialize connection context: " << ctx.message << "\n";
        return 1;
    }

    core::ID user_id = core::generateUuidV7();
    g_conn_ctx->setCurrentUser(user_id, true /*superuser*/);
    g_conn_ctx->set_dialect_tag("FIREBIRD");
    g_conn_ctx->set_current_schema("public");
    g_conn_ctx->set_search_path({"public"});
    core::ConnectionContext::setCurrent(g_conn_ctx.get());

    // Create compiler and executor
    sblr::FirebirdQueryCompiler compiler(&db);

    // Set dialect
    parser::firebird::SQLDialect dialect = parser::firebird::SQLDialect::DIALECT_3;
    if (g_config.sql_dialect == 1) {
        dialect = parser::firebird::SQLDialect::DIALECT_1;
    } else if (g_config.sql_dialect == 2) {
        dialect = parser::firebird::SQLDialect::DIALECT_2;
    }
    compiler.setDialect(dialect);
    compiler.setStatsEnabled(g_config.show_stats);

    sblr::Executor executor(&db);
    executor.setConnectionContext(g_conn_ctx.get());

    int result = 0;

    // Execute file if specified
    if (!g_config.input_file.empty()) {
        if (!executeFile(g_config.input_file, compiler, executor, db)) {
            result = 1;
        }
    }
    // Execute single command if specified
    else if (!g_config.command.empty()) {
        if (!executeSQL(g_config.command, compiler, executor, db)) {
            result = 1;
        }
    }
    // Interactive mode
    else {
        runInteractive(compiler, executor, db);
    }

    if (g_conn_ctx) {
        core::ErrorContext shutdown_ctx;
        g_conn_ctx->shutdownTransaction(&shutdown_ctx);
        core::ConnectionContext::setCurrent(nullptr);
        g_conn_ctx.reset();
    } else {
        core::ConnectionContext::setCurrent(nullptr);
    }
    db.close();
    return result;
}
