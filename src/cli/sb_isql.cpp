/**
 * sb_isql - ScratchBird Interactive SQL Shell
 *
 * CLI Tools - Interactive SQL utility for querying ScratchBird databases.
 *
 * Usage:
 *   sb_isql <database_path> [options]
 *
 * Options:
 *   -U, --user=<username>    Username for authentication
 *   -P, --password=<pass>    Password (prompted if not given)
 *   -p, --port=<n>           TCP port (default: 5433)
 *   -H, --host=<host>        Host (default: localhost)
 *   -c, --command=<sql>      Execute single command and exit
 *   -f, --file=<file>        Execute commands from file and exit
 *   -o, --output=<file>      Write output to file
 *   -t, --tuples-only        Print tuples only (no headers/footers)
 *   -A, --no-align           Unaligned output mode
 *   -F, --field-separator=<s> Field separator (default: |)
 *   -q, --quiet              Quiet mode (no welcome message)
 *   -e, --echo               Echo commands before execution
 *   -v, --verbose            Verbose mode
 *   -h, --help               Show this help
 *   --version                Show version
 *
 * Meta-commands (start with \):
 *   \?                       Show help for meta-commands
 *   \q                       Quit
 *   \d                       List tables
 *   \d <table>               Describe table
 *   \dt                      List tables
 *   \di                      List indexes
 *   \du                      List users
 *   \l                       List databases
 *   \c <database>            Connect to database
 *   \i <file>                Execute commands from file
 *   \o <file>                Write output to file (or \o to stop)
 *   \timing [on|off]         Toggle timing display
 *   \pset <option> <value>   Set output formatting
 *   \x [on|off]              Toggle expanded display
 *   \! <command>             Execute shell command
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <termios.h>
#include <unistd.h>
#include <signal.h>

#include "scratchbird/client/connection.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird;
using namespace scratchbird::client;

// =============================================================================
// Configuration
// =============================================================================

struct IsqlConfig {
    std::string database_path;
    std::string username;
    std::string password;
    std::string host = "localhost";
    uint16_t port = 5433;

    std::string command;           // -c: single command
    std::string input_file;        // -f: input file
    std::string output_file;       // -o: output file

    bool tuples_only = false;      // -t: no headers/footers
    bool no_align = false;         // -A: unaligned output
    std::string field_separator = "|";  // -F
    bool quiet = false;            // -q: no welcome
    bool echo = false;             // -e: echo commands
    bool verbose = false;          // -v: verbose

    // Runtime settings
    bool timing = false;           // \timing
    bool expanded = false;         // \x expanded display
    std::string format = "aligned"; // Output format
};

// =============================================================================
// Global state
// =============================================================================

static Connection* g_connection = nullptr;
static bool g_running = true;
static IsqlConfig g_config;
static std::ofstream* g_output_file = nullptr;

// =============================================================================
// Signal handling
// =============================================================================

void signalHandler(int sig) {
    if (sig == SIGINT) {
        std::cout << "\n^C\n";
        // Don't exit, just interrupt current input
    }
}

// =============================================================================
// Output helpers
// =============================================================================

std::ostream& getOutput() {
    return g_output_file && g_output_file->is_open() ? *g_output_file : std::cout;
}

void printSeparator(const std::vector<size_t>& widths) {
    auto& out = getOutput();
    out << "+";
    for (size_t w : widths) {
        for (size_t i = 0; i < w + 2; ++i) out << "-";
        out << "+";
    }
    out << "\n";
}

void printRow(const std::vector<std::string>& values, const std::vector<size_t>& widths) {
    auto& out = getOutput();

    if (g_config.no_align) {
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) out << g_config.field_separator;
            out << values[i];
        }
        out << "\n";
    } else {
        out << "|";
        for (size_t i = 0; i < values.size(); ++i) {
            out << " " << std::left << std::setw(static_cast<int>(widths[i])) << values[i] << " |";
        }
        out << "\n";
    }
}

void printExpandedRow(const std::vector<std::string>& columns,
                      const std::vector<std::string>& values,
                      int row_num) {
    auto& out = getOutput();

    // Find max column name width
    size_t max_name = 0;
    for (const auto& c : columns) {
        max_name = std::max(max_name, c.size());
    }

    out << "-[ RECORD " << row_num << " ]";
    for (size_t i = 0; i < max_name; ++i) out << "-";
    out << "+";
    for (size_t i = 0; i < 40; ++i) out << "-";
    out << "\n";

    for (size_t i = 0; i < columns.size(); ++i) {
        out << std::right << std::setw(static_cast<int>(max_name)) << columns[i] << " | " << values[i] << "\n";
    }
}

// =============================================================================
// Result set display
// =============================================================================

void displayResultSet(ResultSet& results, bool show_timing = false,
                      std::chrono::microseconds exec_time = std::chrono::microseconds(0)) {
    auto& out = getOutput();

    size_t col_count = results.getColumnCount();
    if (col_count == 0) {
        int64_t affected = results.getRowsAffected();
        if (affected >= 0) {
            out << "Rows affected: " << affected << "\n";
        }
        if (show_timing) {
            out << "Time: " << std::fixed << std::setprecision(3)
                << (exec_time.count() / 1000.0) << " ms\n";
        }
        return;
    }

    // Collect column names
    std::vector<std::string> columns(col_count);
    std::vector<size_t> widths(col_count);
    for (size_t i = 0; i < col_count; ++i) {
        columns[i] = results.getColumnName(i);
        widths[i] = columns[i].size();
    }

    // Collect all rows
    std::vector<std::vector<std::string>> rows;
    while (results.next()) {
        std::vector<std::string> row(col_count);
        for (size_t i = 0; i < col_count; ++i) {
            if (results.isNull(i)) {
                row[i] = "(null)";
            } else {
                row[i] = results.getString(i);
            }
            widths[i] = std::max(widths[i], row[i].size());
        }
        rows.push_back(std::move(row));
    }

    // Display
    if (g_config.expanded) {
        // Expanded display mode
        int row_num = 1;
        for (const auto& row : rows) {
            printExpandedRow(columns, row, row_num++);
        }
    } else {
        // Normal table display
        if (!g_config.tuples_only && !g_config.no_align) {
            printSeparator(widths);
            printRow(columns, widths);
            printSeparator(widths);
        }

        for (const auto& row : rows) {
            printRow(row, widths);
        }

        if (!g_config.tuples_only && !g_config.no_align) {
            printSeparator(widths);
        }
    }

    // Footer
    if (!g_config.tuples_only) {
        out << "(" << rows.size() << " row" << (rows.size() == 1 ? "" : "s") << ")\n";
    }

    if (show_timing) {
        out << "Time: " << std::fixed << std::setprecision(3)
            << (exec_time.count() / 1000.0) << " ms\n";
    }
}

// =============================================================================
// SQL Execution
// =============================================================================

bool executeSQL(const std::string& sql) {
    if (!g_connection || !g_connection->isConnected()) {
        std::cerr << "Error: Not connected to database\n";
        return false;
    }

    if (g_config.echo) {
        getOutput() << sql << "\n";
    }

    auto start = std::chrono::high_resolution_clock::now();

    ResultSet results;
    core::ErrorContext ctx;
    core::Status status = g_connection->executeQuery(sql, &results, &ctx);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    if (status != core::Status::OK) {
        std::cerr << "Error: " << ctx.message << "\n";
        return false;
    }

    displayResultSet(results, g_config.timing, duration);
    return true;
}

// =============================================================================
// Meta-commands
// =============================================================================

void showMetaHelp() {
    auto& out = getOutput();
    out << R"(
Meta-commands:
  \?                Show this help
  \q                Quit
  \d                List tables
  \d <table>        Describe table
  \dt               List tables
  \di               List indexes
  \du               List users
  \l                List databases
  \c <database>     Connect to database
  \i <file>         Execute commands from file
  \o [file]         Write output to file (no arg = stop)
  \timing [on|off]  Toggle/set timing display
  \x [on|off]       Toggle/set expanded display
  \! <command>      Execute shell command
  \echo <text>      Print text
  \set              Show all variables
  \pset <opt> <val> Set output formatting option
    format          Output format (aligned, unaligned)
    tuples_only     Only show data rows (on/off)
    expanded        Expanded display mode (on/off)

)";
}

bool handleMetaCommand(const std::string& cmd) {
    if (cmd.empty() || cmd[0] != '\\') return false;

    std::string meta = cmd.substr(1);

    // Parse command and arguments
    std::string command, arg;
    size_t space = meta.find(' ');
    if (space != std::string::npos) {
        command = meta.substr(0, space);
        arg = meta.substr(space + 1);
        // Trim leading spaces from arg
        while (!arg.empty() && arg[0] == ' ') arg = arg.substr(1);
    } else {
        command = meta;
    }

    if (command == "?" || command == "help") {
        showMetaHelp();
        return true;
    }

    if (command == "q" || command == "quit" || command == "exit") {
        g_running = false;
        return true;
    }

    if (command == "d") {
        if (arg.empty()) {
            // List tables
            return executeSQL("SHOW TABLES");
        } else {
            // Describe table
            return executeSQL("DESCRIBE " + arg);
        }
    }

    if (command == "dt") {
        return executeSQL("SHOW TABLES");
    }

    if (command == "di") {
        return executeSQL("SHOW INDEXES");
    }

    if (command == "du") {
        return executeSQL("SELECT name, is_admin FROM sys_users ORDER BY name");
    }

    if (command == "l") {
        return executeSQL("SHOW DATABASES");
    }

    if (command == "c" || command == "connect") {
        if (arg.empty()) {
            std::cerr << "Usage: \\c <database>\n";
            return true;
        }

        // Disconnect and reconnect
        if (g_connection) {
            g_connection->disconnect();
        }

        g_config.database_path = arg;
        core::ErrorContext ctx;
        core::Status status = g_connection->connect(arg, g_config.username, g_config.password, &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Connection failed: " << ctx.message << "\n";
        } else {
            std::cout << "Connected to " << arg << "\n";
        }
        return true;
    }

    if (command == "i" || command == "include") {
        if (arg.empty()) {
            std::cerr << "Usage: \\i <filename>\n";
            return true;
        }

        std::ifstream file(arg);
        if (!file) {
            std::cerr << "Error: Cannot open file: " << arg << "\n";
            return true;
        }

        std::string line;
        std::string sql;
        while (std::getline(file, line)) {
            // Skip comments
            if (line.empty() || line[0] == '#' || line.substr(0, 2) == "--") {
                continue;
            }

            sql += line;
            // Check for statement terminator
            if (!line.empty() && line.back() == ';') {
                executeSQL(sql);
                sql.clear();
            } else {
                sql += " ";
            }
        }

        if (!sql.empty()) {
            executeSQL(sql);
        }
        return true;
    }

    if (command == "o" || command == "output") {
        if (arg.empty()) {
            // Stop output to file
            if (g_output_file) {
                g_output_file->close();
                delete g_output_file;
                g_output_file = nullptr;
                std::cout << "Output redirected to stdout\n";
            }
        } else {
            // Start output to file
            if (g_output_file) {
                g_output_file->close();
                delete g_output_file;
            }
            g_output_file = new std::ofstream(arg);
            if (!g_output_file->is_open()) {
                std::cerr << "Error: Cannot open file: " << arg << "\n";
                delete g_output_file;
                g_output_file = nullptr;
            } else {
                std::cout << "Output redirected to " << arg << "\n";
            }
        }
        return true;
    }

    if (command == "timing") {
        if (arg.empty()) {
            g_config.timing = !g_config.timing;
        } else if (arg == "on") {
            g_config.timing = true;
        } else if (arg == "off") {
            g_config.timing = false;
        }
        std::cout << "Timing is " << (g_config.timing ? "on" : "off") << "\n";
        return true;
    }

    if (command == "x" || command == "expanded") {
        if (arg.empty()) {
            g_config.expanded = !g_config.expanded;
        } else if (arg == "on") {
            g_config.expanded = true;
        } else if (arg == "off") {
            g_config.expanded = false;
        }
        std::cout << "Expanded display is " << (g_config.expanded ? "on" : "off") << "\n";
        return true;
    }

    if (command == "!" || command.substr(0, 1) == "!") {
        std::string shell_cmd = (command == "!") ? arg : (command.substr(1) + " " + arg);
        if (shell_cmd.empty()) {
            std::cerr << "Usage: \\! <command>\n";
        } else {
            int ret = system(shell_cmd.c_str());
            if (ret != 0 && g_config.verbose) {
                std::cerr << "Command exited with status " << ret << "\n";
            }
        }
        return true;
    }

    if (command == "echo") {
        getOutput() << arg << "\n";
        return true;
    }

    if (command == "set") {
        auto& out = getOutput();
        out << "Variables:\n";
        out << "  database = " << g_config.database_path << "\n";
        out << "  user = " << g_config.username << "\n";
        out << "  port = " << g_config.port << "\n";
        out << "  timing = " << (g_config.timing ? "on" : "off") << "\n";
        out << "  expanded = " << (g_config.expanded ? "on" : "off") << "\n";
        out << "  format = " << g_config.format << "\n";
        out << "  tuples_only = " << (g_config.tuples_only ? "on" : "off") << "\n";
        return true;
    }

    if (command == "pset") {
        size_t sep = arg.find(' ');
        if (sep == std::string::npos) {
            std::cerr << "Usage: \\pset <option> <value>\n";
            return true;
        }
        std::string opt = arg.substr(0, sep);
        std::string val = arg.substr(sep + 1);
        while (!val.empty() && val[0] == ' ') val = val.substr(1);

        if (opt == "format") {
            g_config.format = val;
            g_config.no_align = (val == "unaligned");
        } else if (opt == "tuples_only") {
            g_config.tuples_only = (val == "on" || val == "true" || val == "1");
        } else if (opt == "expanded") {
            g_config.expanded = (val == "on" || val == "true" || val == "1");
        } else {
            std::cerr << "Unknown option: " << opt << "\n";
        }
        return true;
    }

    std::cerr << "Unknown command: \\" << command << "\nType \\? for help.\n";
    return true;
}

// =============================================================================
// Password input (no echo)
// =============================================================================

std::string readPassword(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    struct termios old_term, new_term;
    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    std::string password;
    std::getline(std::cin, password);

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    std::cout << "\n";

    return password;
}

// =============================================================================
// Simple line editing (no readline dependency)
// =============================================================================

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::cout.flush();

    std::string line;
    if (!std::getline(std::cin, line)) {
        g_running = false;
        return "";
    }
    return line;
}

// =============================================================================
// Main REPL
// =============================================================================

void runInteractive() {
    if (!g_config.quiet) {
        std::cout << "sb_isql (ScratchBird " << "0.1.0" << ")\n";
        std::cout << "Type \\? for help, \\q to quit.\n\n";
    }

    std::string sql_buffer;
    bool in_string = false;
    char string_char = 0;
    bool in_multiline = false;

    while (g_running) {
        std::string prompt;
        if (in_multiline) {
            prompt = g_config.database_path + "-# ";
        } else {
            prompt = g_config.database_path + "=> ";
        }

        std::string line = readLine(prompt);
        if (!g_running) break;

        // Trim whitespace
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line = line.substr(1);
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }

        // Empty line
        if (line.empty()) {
            continue;
        }

        // Meta-command
        if (line[0] == '\\' && !in_multiline) {
            handleMetaCommand(line);
            continue;
        }

        // Append to buffer
        if (!sql_buffer.empty()) {
            sql_buffer += " ";
        }
        sql_buffer += line;

        // Check for statement terminator (simple check, ignoring strings)
        // This is a simplified version - a full parser would track quotes properly
        bool ends_with_semicolon = false;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (in_string) {
                if (c == string_char && (i == 0 || line[i-1] != '\\')) {
                    in_string = false;
                }
            } else {
                if (c == '\'' || c == '"') {
                    in_string = true;
                    string_char = c;
                } else if (c == ';' && i == line.size() - 1) {
                    ends_with_semicolon = true;
                }
            }
        }

        if (ends_with_semicolon && !in_string) {
            executeSQL(sql_buffer);
            sql_buffer.clear();
            in_multiline = false;
        } else {
            in_multiline = true;
        }
    }
}

// =============================================================================
// Argument parsing
// =============================================================================

void printUsage(const char* program) {
    std::cout << "ScratchBird Interactive SQL Shell\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program << " <database_path> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -U, --user=<username>     Username for authentication\n";
    std::cout << "  -P, --password=<pass>     Password (prompted if not given)\n";
    std::cout << "  -p, --port=<n>            TCP port (default: 5433)\n";
    std::cout << "  -H, --host=<host>         Host (default: localhost)\n";
    std::cout << "  -c, --command=<sql>       Execute single command and exit\n";
    std::cout << "  -f, --file=<file>         Execute commands from file and exit\n";
    std::cout << "  -o, --output=<file>       Write output to file\n";
    std::cout << "  -t, --tuples-only         Print tuples only (no headers/footers)\n";
    std::cout << "  -A, --no-align            Unaligned output mode\n";
    std::cout << "  -F, --field-separator=<s> Field separator (default: |)\n";
    std::cout << "  -q, --quiet               Quiet mode (no welcome message)\n";
    std::cout << "  -e, --echo                Echo commands before execution\n";
    std::cout << "  -v, --verbose             Verbose mode\n";
    std::cout << "  -h, --help                Show this help\n";
    std::cout << "      --version             Show version\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " /path/to/mydb.sbdb\n";
    std::cout << "  " << program << " mydb.sbdb -U admin -c \"SELECT * FROM users\"\n";
    std::cout << "  " << program << " mydb.sbdb -f queries.sql -o results.txt\n";
}

void printVersion() {
    std::cout << "sb_isql (ScratchBird Interactive SQL) 0.1.0\n";
}

bool parseArgs(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--version") {
            printVersion();
            std::exit(0);
        }
        if (arg == "-U" && i + 1 < argc) {
            g_config.username = argv[++i];
        } else if (arg.find("--user=") == 0) {
            g_config.username = arg.substr(7);
        } else if (arg == "-P" && i + 1 < argc) {
            g_config.password = argv[++i];
        } else if (arg.find("--password=") == 0) {
            g_config.password = arg.substr(11);
        } else if (arg == "-p" && i + 1 < argc) {
            g_config.port = static_cast<uint16_t>(std::stoul(argv[++i]));
        } else if (arg.find("--port=") == 0) {
            g_config.port = static_cast<uint16_t>(std::stoul(arg.substr(7)));
        } else if (arg == "-H" && i + 1 < argc) {
            g_config.host = argv[++i];
        } else if (arg.find("--host=") == 0) {
            g_config.host = arg.substr(7);
        } else if (arg == "-c" && i + 1 < argc) {
            g_config.command = argv[++i];
        } else if (arg.find("--command=") == 0) {
            g_config.command = arg.substr(10);
        } else if (arg == "-f" && i + 1 < argc) {
            g_config.input_file = argv[++i];
        } else if (arg.find("--file=") == 0) {
            g_config.input_file = arg.substr(7);
        } else if (arg == "-o" && i + 1 < argc) {
            g_config.output_file = argv[++i];
        } else if (arg.find("--output=") == 0) {
            g_config.output_file = arg.substr(9);
        } else if (arg == "-t" || arg == "--tuples-only") {
            g_config.tuples_only = true;
        } else if (arg == "-A" || arg == "--no-align") {
            g_config.no_align = true;
        } else if (arg == "-F" && i + 1 < argc) {
            g_config.field_separator = argv[++i];
        } else if (arg.find("--field-separator=") == 0) {
            g_config.field_separator = arg.substr(18);
        } else if (arg == "-q" || arg == "--quiet") {
            g_config.quiet = true;
        } else if (arg == "-e" || arg == "--echo") {
            g_config.echo = true;
        } else if (arg == "-v" || arg == "--verbose") {
            g_config.verbose = true;
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        } else if (g_config.database_path.empty()) {
            g_config.database_path = arg;
        } else {
            std::cerr << "Error: Multiple database paths specified\n";
            return false;
        }
    }

    return true;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    if (!parseArgs(argc, argv)) {
        return 1;
    }

    if (g_config.database_path.empty()) {
        std::cerr << "Error: No database specified\n";
        printUsage(argv[0]);
        return 1;
    }

    // Setup signal handler
    signal(SIGINT, signalHandler);

    // Setup output file if specified
    if (!g_config.output_file.empty()) {
        g_output_file = new std::ofstream(g_config.output_file);
        if (!g_output_file->is_open()) {
            std::cerr << "Error: Cannot open output file: " << g_config.output_file << "\n";
            return 1;
        }
    }

    // Prompt for password if username given but no password
    if (!g_config.username.empty() && g_config.password.empty()) {
        g_config.password = readPassword("Password: ");
    }

    // Connect to database
    Connection conn;
    g_connection = &conn;

    ConnectionConfig conn_config;
    conn_config.database_name = g_config.database_path;
    conn_config.username = g_config.username;
    conn_config.password = g_config.password;
    conn_config.tcp_port = g_config.port;
    conn_config.ipc_method = server::IPCMethod::TCP_LOCALHOST;

    core::ErrorContext ctx;
    core::Status status = conn.connect(conn_config, &ctx);

    if (status != core::Status::OK) {
        std::cerr << "Connection failed: " << ctx.message << "\n";
        return 1;
    }

    if (g_config.verbose) {
        std::cout << "Connected to " << g_config.database_path << "\n";
    }

    int result = 0;

    // Execute single command if given
    if (!g_config.command.empty()) {
        if (!executeSQL(g_config.command)) {
            result = 1;
        }
    }
    // Execute file if given
    else if (!g_config.input_file.empty()) {
        // Use \i meta-command
        if (!handleMetaCommand("\\i " + g_config.input_file)) {
            result = 1;
        }
    }
    // Interactive mode
    else {
        runInteractive();
    }

    // Cleanup
    conn.disconnect();
    g_connection = nullptr;

    if (g_output_file) {
        g_output_file->close();
        delete g_output_file;
    }

    return result;
}
