#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <readline/readline.h>
#include <readline/history.h>
#include <fstream>
#include <iomanip>
#include <chrono>
#include "sb_database.h"

// Version information
static const char* VERSION = "sb_isql version SB-T0.5.0.1 ScratchBird 0.5 f90eae0";

// ISQL session state
struct ISQLSession {
    std::string database_name;
    std::string username;
    std::string password;
    std::string role;
    std::string current_schema;
    std::string home_schema;
    std::string terminator = ";";
    
    // Database connection
    std::unique_ptr<SBDatabase> database;
    
    bool connected = false;
    bool show_stats = false;
    bool show_plan = false;
    bool echo_commands = false;
    bool headers = true;
    bool autocommit = true;
    bool bail_on_error = false;
    bool quiet = false;
    bool trusted_auth = false;
    bool version = false;
    bool help = false;
    bool interactive = true;
    
    std::string input_file;
    std::string output_file;
    std::string merge_file;
    
    int page_size = 4096;
    int list_size = 0;
    
    std::string current_command;
    std::vector<std::string> command_history;
    
    // Schema context
    std::map<std::string, std::string> schema_mappings;
};

// Command processing results
enum CommandResult {
    CONTINUE = 0,
    EXIT_OK = 1,
    EXIT_ERR = 2,
    CMD_COMMIT = 3,
    CMD_ROLLBACK = 4
};

static void showUsage() {
    std::cout << "sb_isql - ScratchBird Interactive SQL utility" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: sb_isql [options] [database]" << std::endl;
    std::cout << std::endl;
    std::cout << "Connection Options:" << std::endl;
    std::cout << "  -user <username>     database username" << std::endl;
    std::cout << "  -password <password> database password" << std::endl;
    std::cout << "  -role <role>         SQL role name" << std::endl;
    std::cout << "  -trusted             use trusted authentication" << std::endl;
    std::cout << "  -fetch_password      fetch password from file" << std::endl;
    std::cout << std::endl;
    std::cout << "Input/Output Options:" << std::endl;
    std::cout << "  -input <file>        read commands from file" << std::endl;
    std::cout << "  -output <file>       write output to file" << std::endl;
    std::cout << "  -merge <file>        merge stderr and stdout to file" << std::endl;
    std::cout << "  -echo                echo commands" << std::endl;
    std::cout << "  -noautocommit        disable autocommit" << std::endl;
    std::cout << "  -bail                bail on first error" << std::endl;
    std::cout << "  -quiet               quiet mode (minimal output)" << std::endl;
    std::cout << std::endl;
    std::cout << "Schema Options:" << std::endl;
    std::cout << "  -schema <name>       set current schema" << std::endl;
    std::cout << "  -home_schema <name>  set home schema" << std::endl;
    std::cout << std::endl;
    std::cout << "Display Options:" << std::endl;
    std::cout << "  -stats               show performance statistics" << std::endl;
    std::cout << "  -plan                show execution plan" << std::endl;
    std::cout << "  -noheaders           don't show column headers" << std::endl;
    std::cout << "  -list                list format output" << std::endl;
    std::cout << "  -pagesize <size>     set page size for output" << std::endl;
    std::cout << std::endl;
    std::cout << "Other Options:" << std::endl;
    std::cout << "  -term <char>         set statement terminator" << std::endl;
    std::cout << "  -x                   extract DDL for database" << std::endl;
    std::cout << "  -a                   extract DDL for all objects" << std::endl;
    std::cout << "  -z                   show version" << std::endl;
    std::cout << "  -?                   show this help" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  sb_isql mydb.fdb" << std::endl;
    std::cout << "  sb_isql -user SYSDBA -password masterkey mydb.fdb" << std::endl;
    std::cout << "  sb_isql -input commands.sql -output results.txt mydb.fdb" << std::endl;
    std::cout << "  sb_isql -schema finance.accounting mydb.fdb" << std::endl;
}

static void showVersion() {
    std::cout << VERSION << std::endl;
}

static void showBanner() {
    std::cout << "ScratchBird Interactive SQL Utility" << std::endl;
    std::cout << "SB-T0.6.0.1 ScratchBird 0.6 f90eae0" << std::endl;
    std::cout << "Use CONNECT or CREATE DATABASE to specify a database" << std::endl;
    std::cout << std::endl;
}

static bool parseCommandLine(int argc, char* argv[], ISQLSession& session) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-z") {
            session.version = true;
        } else if (arg == "-?" || arg == "-help") {
            session.help = true;
        } else if (arg == "-user") {
            if (i + 1 < argc) {
                session.username = argv[++i];
            }
        } else if (arg == "-password") {
            if (i + 1 < argc) {
                session.password = argv[++i];
            }
        } else if (arg == "-role") {
            if (i + 1 < argc) {
                session.role = argv[++i];
            }
        } else if (arg == "-trusted") {
            session.trusted_auth = true;
        } else if (arg == "-input") {
            if (i + 1 < argc) {
                session.input_file = argv[++i];
                session.interactive = false;
            }
        } else if (arg == "-output") {
            if (i + 1 < argc) {
                session.output_file = argv[++i];
            }
        } else if (arg == "-merge") {
            if (i + 1 < argc) {
                session.merge_file = argv[++i];
            }
        } else if (arg == "-echo") {
            session.echo_commands = true;
        } else if (arg == "-noautocommit") {
            session.autocommit = false;
        } else if (arg == "-bail") {
            session.bail_on_error = true;
        } else if (arg == "-quiet") {
            session.quiet = true;
        } else if (arg == "-schema") {
            if (i + 1 < argc) {
                session.current_schema = argv[++i];
            }
        } else if (arg == "-home_schema") {
            if (i + 1 < argc) {
                session.home_schema = argv[++i];
            }
        } else if (arg == "-stats") {
            session.show_stats = true;
        } else if (arg == "-plan") {
            session.show_plan = true;
        } else if (arg == "-noheaders") {
            session.headers = false;
        } else if (arg == "-list") {
            session.list_size = 1;
        } else if (arg == "-pagesize") {
            if (i + 1 < argc) {
                session.page_size = std::atoi(argv[++i]);
            }
        } else if (arg == "-term") {
            if (i + 1 < argc) {
                session.terminator = argv[++i];
            }
        } else if (arg[0] != '-') {
            session.database_name = arg;
        }
    }
    
    return true;
}

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(' ');
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

static std::string toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

static bool connectToDatabase(ISQLSession& session) {
    if (!session.quiet) {
        std::cout << "Connecting to database: " << session.database_name << std::endl;
        
        if (!session.username.empty()) {
            std::cout << "Username: " << session.username << std::endl;
        }
        
        if (!session.role.empty()) {
            std::cout << "Role: " << session.role << std::endl;
        }
        
        if (session.trusted_auth) {
            std::cout << "Using trusted authentication" << std::endl;
        }
    }
    
    // Create database connection
    session.database = std::make_unique<SBDatabase>();
    
    if (!session.database->connect(session.database_name, session.username, 
                                  session.password, session.role, session.trusted_auth)) {
        std::cerr << "Failed to connect to database: " << session.database->getLastError() << std::endl;
        return false;
    }
    
    session.connected = true;
    
    // Set schema context if specified
    if (!session.current_schema.empty()) {
        std::string sql = "SET SCHEMA '" + session.current_schema + "'";
        if (!session.database->executeQuery(sql)) {
            std::cerr << "Warning: Failed to set current schema: " << session.database->getLastError() << std::endl;
        } else if (!session.quiet) {
            std::cout << "Current schema: " << session.current_schema << std::endl;
        }
    }
    
    if (!session.home_schema.empty() && !session.quiet) {
        std::cout << "Home schema: " << session.home_schema << std::endl;
    }
    
    if (!session.quiet) {
        std::cout << "Connected successfully" << std::endl;
    }
    
    return true;
}

static bool executeQuery(const std::string& query, ISQLSession& session) {
    if (session.echo_commands) {
        std::cout << query << std::endl;
    }
    
    if (!session.database || !session.database->isConnected()) {
        std::cerr << "Not connected to database" << std::endl;
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Determine if this is a SELECT statement
    std::string upper_query = toUpper(query);
    bool is_select = (upper_query.find("SELECT") == 0);
    
    if (is_select) {
        // Execute SELECT query
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> column_names;
        
        if (!session.database->executeSelect(query, results, column_names)) {
            std::cerr << "Query failed: " << session.database->getLastError() << std::endl;
            return false;
        }
        
        // Display results
        if (!results.empty()) {
            // Show column headers
            if (session.headers) {
                for (size_t i = 0; i < column_names.size(); i++) {
                    std::cout << std::left << std::setw(15) << column_names[i];
                }
                std::cout << std::endl;
                
                for (size_t i = 0; i < column_names.size(); i++) {
                    std::cout << std::string(15, '=');
                }
                std::cout << std::endl;
            }
            
            // Show data rows
            for (const auto& row : results) {
                for (size_t i = 0; i < row.size(); i++) {
                    std::cout << std::left << std::setw(15) << row[i];
                }
                std::cout << std::endl;
            }
            
            std::cout << std::endl;
            std::cout << "Records fetched: " << results.size() << std::endl;
        } else {
            std::cout << "No records found" << std::endl;
        }
    } else {
        // Execute DML/DDL statement
        int affected_rows = 0;
        if (!session.database->executeUpdate(query, affected_rows)) {
            std::cerr << "Statement failed: " << session.database->getLastError() << std::endl;
            return false;
        }
        
        if (affected_rows > 0) {
            std::cout << "Records affected: " << affected_rows << std::endl;
        } else {
            std::cout << "Statement executed successfully" << std::endl;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    if (session.show_stats) {
        std::cout << "Elapsed time: " << std::fixed << std::setprecision(3) 
                  << duration.count() / 1000.0 << " ms" << std::endl;
        std::cout << std::endl;
    }
    
    return true;
}

static CommandResult processCommand(const std::string& command, ISQLSession& session) {
    std::string cmd = trim(command);
    if (cmd.empty()) return CONTINUE;
    
    std::string upper_cmd = toUpper(cmd);
    
    // Handle ISQL-specific commands
    if (upper_cmd == "QUIT" || upper_cmd == "EXIT") {
        return EXIT_OK;
    }
    
    if (upper_cmd == "COMMIT") {
        if (session.database && session.database->isConnected() && session.database->isInTransaction()) {
            if (session.database->commitTransaction()) {
                std::cout << "Transaction committed" << std::endl;
            } else {
                std::cerr << "Failed to commit transaction: " << session.database->getLastError() << std::endl;
            }
        } else {
            std::cout << "No active transaction" << std::endl;
        }
        return CMD_COMMIT;
    }
    
    if (upper_cmd == "ROLLBACK") {
        if (session.database && session.database->isConnected() && session.database->isInTransaction()) {
            if (session.database->rollbackTransaction()) {
                std::cout << "Transaction rolled back" << std::endl;
            } else {
                std::cerr << "Failed to rollback transaction: " << session.database->getLastError() << std::endl;
            }
        } else {
            std::cout << "No active transaction" << std::endl;
        }
        return CMD_ROLLBACK;
    }
    
    if (upper_cmd.find("CONNECT") == 0) {
        // Extract database name from CONNECT command
        size_t pos = cmd.find_first_of(' ');
        if (pos != std::string::npos) {
            session.database_name = trim(cmd.substr(pos));
            if (!connectToDatabase(session)) {
                return EXIT_ERR;
            }
        }
        return CONTINUE;
    }
    
    if (upper_cmd.find("SET") == 0) {
        // Handle SET commands
        if (upper_cmd.find("SET SCHEMA") == 0) {
            size_t pos = cmd.find("SCHEMA") + 6;
            if (pos < cmd.length()) {
                session.current_schema = trim(cmd.substr(pos));
                std::cout << "Schema set to: " << session.current_schema << std::endl;
            }
        } else if (upper_cmd.find("SET HOME SCHEMA") == 0) {
            size_t pos = cmd.find("HOME SCHEMA") + 11;
            if (pos < cmd.length()) {
                session.home_schema = trim(cmd.substr(pos));
                std::cout << "Home schema set to: " << session.home_schema << std::endl;
            }
        } else if (upper_cmd.find("SET AUTOCOMMIT") == 0) {
            if (upper_cmd.find("ON") != std::string::npos) {
                session.autocommit = true;
                std::cout << "Autocommit enabled" << std::endl;
            } else if (upper_cmd.find("OFF") != std::string::npos) {
                session.autocommit = false;
                std::cout << "Autocommit disabled" << std::endl;
            }
        } else if (upper_cmd.find("SET STATS") == 0) {
            if (upper_cmd.find("ON") != std::string::npos) {
                session.show_stats = true;
                std::cout << "Statistics enabled" << std::endl;
            } else if (upper_cmd.find("OFF") != std::string::npos) {
                session.show_stats = false;
                std::cout << "Statistics disabled" << std::endl;
            }
        } else if (upper_cmd.find("SET PLAN") == 0) {
            if (upper_cmd.find("ON") != std::string::npos) {
                session.show_plan = true;
                std::cout << "Plan display enabled" << std::endl;
            } else if (upper_cmd.find("OFF") != std::string::npos) {
                session.show_plan = false;
                std::cout << "Plan display disabled" << std::endl;
            }
        } else if (upper_cmd.find("SET TERM") == 0) {
            size_t pos = cmd.find("TERM") + 4;
            if (pos < cmd.length()) {
                session.terminator = trim(cmd.substr(pos));
                std::cout << "Terminator set to: " << session.terminator << std::endl;
            }
        }
        return CONTINUE;
    }
    
    if (upper_cmd.find("SHOW") == 0) {
        if (upper_cmd.find("SHOW SCHEMA") == 0) {
            std::cout << "Current schema: " << (session.current_schema.empty() ? "None" : session.current_schema) << std::endl;
        } else if (upper_cmd.find("SHOW HOME SCHEMA") == 0) {
            std::cout << "Home schema: " << (session.home_schema.empty() ? "None" : session.home_schema) << std::endl;
        } else if (upper_cmd.find("SHOW TABLES") == 0) {
            if (session.database && session.database->isConnected()) {
                std::vector<std::string> tables = session.database->getTableNames(session.current_schema);
                if (!tables.empty()) {
                    std::cout << "Tables in database:" << std::endl;
                    for (const auto& table : tables) {
                        std::cout << "  " << table << std::endl;
                    }
                } else {
                    std::cout << "No tables found" << std::endl;
                }
            } else {
                std::cout << "Not connected to database" << std::endl;
            }
        } else if (upper_cmd.find("SHOW SCHEMAS") == 0) {
            if (session.database && session.database->isConnected()) {
                std::vector<std::string> schemas = session.database->getSchemaNames();
                if (!schemas.empty()) {
                    std::cout << "Schemas in database:" << std::endl;
                    for (const auto& schema : schemas) {
                        std::cout << "  " << schema << std::endl;
                    }
                } else {
                    std::cout << "No schemas found" << std::endl;
                }
            } else {
                std::cout << "Not connected to database" << std::endl;
            }
        }
        return CONTINUE;
    }
    
    if (upper_cmd.find("HELP") == 0) {
        std::cout << "Available commands:" << std::endl;
        std::cout << "  CONNECT <database>     - Connect to database" << std::endl;
        std::cout << "  SET SCHEMA <name>      - Set current schema" << std::endl;
        std::cout << "  SET HOME SCHEMA <name> - Set home schema" << std::endl;
        std::cout << "  SET AUTOCOMMIT ON|OFF  - Enable/disable autocommit" << std::endl;
        std::cout << "  SET STATS ON|OFF       - Enable/disable statistics" << std::endl;
        std::cout << "  SET PLAN ON|OFF        - Enable/disable plan display" << std::endl;
        std::cout << "  SET TERM <char>        - Set statement terminator" << std::endl;
        std::cout << "  SHOW SCHEMA            - Show current schema" << std::endl;
        std::cout << "  SHOW HOME SCHEMA       - Show home schema" << std::endl;
        std::cout << "  SHOW TABLES            - Show tables in database" << std::endl;
        std::cout << "  SHOW SCHEMAS           - Show schemas in database" << std::endl;
        std::cout << "  COMMIT                 - Commit transaction" << std::endl;
        std::cout << "  ROLLBACK               - Rollback transaction" << std::endl;
        std::cout << "  QUIT/EXIT              - Exit ISQL" << std::endl;
        return CONTINUE;
    }
    
    // Handle SQL queries
    if (upper_cmd.find("SELECT") == 0 || upper_cmd.find("INSERT") == 0 || 
        upper_cmd.find("UPDATE") == 0 || upper_cmd.find("DELETE") == 0 ||
        upper_cmd.find("CREATE") == 0 || upper_cmd.find("ALTER") == 0 ||
        upper_cmd.find("DROP") == 0) {
        
        if (!session.connected) {
            std::cout << "Not connected to database" << std::endl;
            return CONTINUE;
        }
        
        if (!executeQuery(cmd, session)) {
            if (session.bail_on_error) {
                return EXIT_ERR;
            }
        }
        
        if (session.autocommit) {
            std::cout << "Transaction committed" << std::endl;
        }
        
        return CONTINUE;
    }
    
    std::cout << "Unknown command: " << cmd << std::endl;
    return CONTINUE;
}

static void interactiveMode(ISQLSession& session) {
    if (!session.quiet) {
        showBanner();
    }
    
    // Auto-connect if database specified
    if (!session.database_name.empty()) {
        if (!connectToDatabase(session)) {
            return;
        }
    }
    
    std::string prompt = "SQL> ";
    char* line;
    
    while ((line = readline(prompt.c_str())) != nullptr) {
        if (strlen(line) > 0) {
            add_history(line);
        }
        
        std::string input(line);
        free(line);
        
        // Handle multi-line input
        if (!input.empty() && input.back() != session.terminator[0]) {
            session.current_command += input + " ";
            prompt = "CON> ";
            continue;
        }
        
        // Remove terminator
        if (!input.empty() && input.back() == session.terminator[0]) {
            input.pop_back();
        }
        
        session.current_command += input;
        
        CommandResult result = processCommand(session.current_command, session);
        
        session.current_command.clear();
        prompt = "SQL> ";
        
        if (result == EXIT_OK) {
            break;
        }
        
        if (result == EXIT_ERR && session.bail_on_error) {
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    ISQLSession session;
    
    if (!parseCommandLine(argc, argv, session)) {
        return 1;
    }
    
    if (session.version) {
        showVersion();
        return 0;
    }
    
    if (session.help) {
        showUsage();
        return 0;
    }
    
    if (session.interactive) {
        interactiveMode(session);
    } else {
        // Batch mode processing would go here
        std::cout << "Batch mode not implemented in this demo" << std::endl;
        return 1;
    }
    
    return 0;
}