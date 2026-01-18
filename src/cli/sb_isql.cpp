/**
 * sb_isql - ScratchBird Interactive SQL Shell
 *
 * CLI Tools - Interactive SQL utility for querying ScratchBird databases.
 * Firebird ISQL compatible with PostgreSQL psql extensions.
 *
 * Usage:
 *   sb_isql <database_path> [options]
 *
 * Options:
 *   -U, --user=<username>    Username for authentication
 *   -P, --password=<pass>    Password (prompted if not given)
 *   -p, --port=<n>           TCP port (default: 3092)
 *   -H, --host=<host>        Host (default: localhost)
 *   -c, --command=<sql>      Execute single command and exit
 *   -f, --file=<file>        Execute commands from file and exit
 *   -o, --output=<file>      Write output to file
 *   -t, --tuples-only        Print tuples only (no headers/footers)
 *   -A, --no-align           Unaligned output mode
 *   -F, --field-separator=<s> Field separator (default: |)
 *   -q, --quiet              Quiet mode (no welcome message)
 *   -e, --echo               Echo commands before execution
 *   -b, --bail               Stop on first error (bail out)
 *   -v, --verbose            Verbose mode
 *   -h, --help               Show this help
 *   --version                Show version
 *   --schema-tree            Print schema tree and exit
 *
 * SET Commands (Firebird ISQL compatible):
 *   SET BAIL [ON|OFF]        Stop on first error
 *   SET TERM <char>          Change statement terminator
 *   SET COUNT [ON|OFF]       Display row counts
 *   SET HEADING [ON|OFF]     Show column headings
 *   SET ECHO [ON|OFF]        Echo commands before execution
 *   SET LIST [ON|OFF]        Vertical display mode
 *   SET NULL <string>        String to display for NULL values
 *   SET WIDTH <col> <n>      Set column display width
 *   SET STATS [ON|OFF]       Show timing statistics
 *   SET PLAN [ON|OFF]        Show query execution plan
 *   SET PLANONLY [ON|OFF]    Show plan only, don't execute
 *   SET EXPLAIN [ON|OFF]     Show detailed plan analysis
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
#include <map>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <sys/stat.h>

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
    uint16_t port = 3092;

    std::string command;           // -c: single command
    std::string input_file;        // -f: input file
    std::string output_file;       // -o: output file
    std::string parser_name;       // -par/--parser: select SQL parser

    bool tuples_only = false;      // -t: no headers/footers
    bool no_align = false;         // -A: unaligned output
    std::string field_separator = "|";  // -F
    bool quiet = false;            // -q: no welcome
    bool echo = false;             // -e: echo commands
    bool verbose = false;          // -v: verbose
    bool schema_tree = false;      // --schema-tree: print schema tree and exit

    // Runtime settings
    bool timing = false;           // \timing
    bool expanded = false;         // \x expanded display
    std::string format = "aligned"; // Output format

    // Firebird ISQL compatible settings
    bool bail = false;             // -b: stop on first error
    std::string term = ";";        // SET TERM: statement terminator
    bool count = true;             // SET COUNT: show row counts
    bool heading = true;           // SET HEADING: show column headers
    bool list = false;             // SET LIST: vertical display mode
    std::string null_display = "(null)";  // SET NULL: NULL representation
    bool stats = false;            // SET STATS: show execution statistics
    bool plan = false;             // SET PLAN: show query plan before execution
    bool plan_only = false;        // SET PLANONLY: show plan without executing
    bool explain = false;          // SET EXPLAIN: show detailed plan analysis
    std::string names = "UTF8";    // SET NAMES: client character set
    bool warnings = true;          // SET WARNINGS: display warnings
    std::map<std::string, size_t> column_widths;  // SET WIDTH: per-column widths

    // Variable substitution
    std::map<std::string, std::string> variables;  // User-defined variables

    // Prompt customization
    std::string prompt;            // SET PROMPT: custom prompt (empty = default)
    bool show_time = false;        // SET TIME: show time in prompt

    // Last query for \watch
    std::string last_query;        // Last executed query

    // History
    std::string history_file;      // Path to history file
    std::vector<std::string> history;  // Command history
    static constexpr size_t MAX_HISTORY = 1000;

    // DDL Extraction modes (Firebird isql compatible)
    enum class DDLMode {
        NONE,           // Normal operation
        EXTRACT_ALL,    // -a: Extract DDL for all objects
        EXTRACT_X,      // -x: Extract DDL (no data)
        EXTRACT_EX      // -ex: Extract DDL with CREATE DATABASE
    };
    DDLMode ddl_mode = DDLMode::NONE;

    // SQL Dialect (Firebird compatibility)
    int sql_dialect = 3;           // -s: SQL dialect (1, 2, or 3), default 3 (modern)

    // Phase 5 settings
    bool autoddl = true;           // SET AUTODDL: auto-commit DDL statements
    int maxrows = 0;               // SET MAXROWS: limit rows returned (0 = unlimited)
    int local_timeout = 0;         // SET LOCAL_TIMEOUT: statement timeout in seconds (0 = unlimited)

    // Error handling
    enum class ErrorAction {
        CONTINUE,       // Continue on error (default)
        STOP,           // Stop on error
        EXIT            // Exit on error
    };
    ErrorAction on_error = ErrorAction::CONTINUE;
    int exit_code = 0;             // Exit code for EXIT command
};

// =============================================================================
// Global state
// =============================================================================

static Connection* g_connection = nullptr;
static bool g_running = true;
static IsqlConfig g_config;
static std::ofstream* g_output_file = nullptr;
static std::ofstream* g_error_file = nullptr;
static std::streambuf* g_original_cerr = nullptr;  // To restore stderr

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
// History management
// =============================================================================

std::string getHistoryFilePath() {
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (home) {
        return std::string(home) + "/.sb_isql_history";
    }
    return "";
}

void loadHistory() {
    if (g_config.history_file.empty()) {
        g_config.history_file = getHistoryFilePath();
    }
    if (g_config.history_file.empty()) return;

    std::ifstream file(g_config.history_file);
    if (!file) return;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            g_config.history.push_back(line);
        }
    }

    // Trim to max size
    while (g_config.history.size() > IsqlConfig::MAX_HISTORY) {
        g_config.history.erase(g_config.history.begin());
    }
}

void saveHistory() {
    if (g_config.history_file.empty()) return;

    std::ofstream file(g_config.history_file);
    if (!file) return;

    // Only save last MAX_HISTORY entries
    size_t start = 0;
    if (g_config.history.size() > IsqlConfig::MAX_HISTORY) {
        start = g_config.history.size() - IsqlConfig::MAX_HISTORY;
    }

    for (size_t i = start; i < g_config.history.size(); ++i) {
        file << g_config.history[i] << "\n";
    }
}

void addToHistory(const std::string& cmd) {
    // Don't add empty commands or duplicates of the last command
    if (cmd.empty()) return;
    if (!g_config.history.empty() && g_config.history.back() == cmd) return;

    g_config.history.push_back(cmd);
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
        if (affected >= 0 && g_config.count) {
            out << "Rows affected: " << affected << "\n";
        }
        if (show_timing || g_config.stats) {
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

        // Apply configured column width if set
        auto it = g_config.column_widths.find(columns[i]);
        if (it != g_config.column_widths.end()) {
            widths[i] = std::max(widths[i], it->second);
        }
    }

    // Collect all rows
    std::vector<std::vector<std::string>> rows;
    while (results.next()) {
        std::vector<std::string> row(col_count);
        for (size_t i = 0; i < col_count; ++i) {
            if (results.isNull(i)) {
                row[i] = g_config.null_display;  // Use configured NULL display
            } else {
                row[i] = results.getString(i);
            }
            // Apply configured column width limit
            auto it = g_config.column_widths.find(columns[i]);
            if (it != g_config.column_widths.end() && row[i].size() > it->second) {
                row[i] = row[i].substr(0, it->second);
            }
            widths[i] = std::max(widths[i], row[i].size());
        }
        rows.push_back(std::move(row));
    }

    // Display - check format setting
    bool use_expanded = g_config.expanded || g_config.list;

    if (g_config.format == "csv") {
        // CSV format output
        if (g_config.heading && !g_config.tuples_only) {
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) out << ",";
                // Quote if contains comma, quote, or newline
                if (columns[i].find_first_of(",\"\n") != std::string::npos) {
                    out << "\"";
                    for (char c : columns[i]) {
                        if (c == '"') out << "\"\"";
                        else out << c;
                    }
                    out << "\"";
                } else {
                    out << columns[i];
                }
            }
            out << "\n";
        }
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) out << ",";
                if (row[i].find_first_of(",\"\n") != std::string::npos) {
                    out << "\"";
                    for (char c : row[i]) {
                        if (c == '"') out << "\"\"";
                        else out << c;
                    }
                    out << "\"";
                } else {
                    out << row[i];
                }
            }
            out << "\n";
        }
    } else if (g_config.format == "html") {
        // HTML table format
        out << "<table>\n";
        if (g_config.heading && !g_config.tuples_only) {
            out << "  <thead>\n    <tr>\n";
            for (const auto& col : columns) {
                out << "      <th>" << col << "</th>\n";
            }
            out << "    </tr>\n  </thead>\n";
        }
        out << "  <tbody>\n";
        for (const auto& row : rows) {
            out << "    <tr>\n";
            for (const auto& val : row) {
                out << "      <td>" << val << "</td>\n";
            }
            out << "    </tr>\n";
        }
        out << "  </tbody>\n</table>\n";
    } else if (g_config.format == "json") {
        // JSON array format
        out << "[\n";
        for (size_t r = 0; r < rows.size(); ++r) {
            out << "  {";
            for (size_t c = 0; c < columns.size(); ++c) {
                if (c > 0) out << ", ";
                out << "\"" << columns[c] << "\": ";
                // Quote strings, but try to detect numbers
                const std::string& val = rows[r][c];
                bool is_number = !val.empty() && val != g_config.null_display;
                if (is_number) {
                    for (char ch : val) {
                        if (!std::isdigit(ch) && ch != '.' && ch != '-' && ch != '+') {
                            is_number = false;
                            break;
                        }
                    }
                }
                if (val == g_config.null_display) {
                    out << "null";
                } else if (is_number) {
                    out << val;
                } else {
                    out << "\"";
                    for (char ch : val) {
                        if (ch == '"') out << "\\\"";
                        else if (ch == '\\') out << "\\\\";
                        else if (ch == '\n') out << "\\n";
                        else if (ch == '\r') out << "\\r";
                        else if (ch == '\t') out << "\\t";
                        else out << ch;
                    }
                    out << "\"";
                }
            }
            out << "}" << (r < rows.size() - 1 ? "," : "") << "\n";
        }
        out << "]\n";
    } else if (use_expanded) {
        // Expanded/list display mode (vertical)
        int row_num = 1;
        for (const auto& row : rows) {
            printExpandedRow(columns, row, row_num++);
        }
    } else {
        // Normal table display (aligned or unaligned)
        // Show headers only if heading is on and not tuples_only
        if (g_config.heading && !g_config.tuples_only && !g_config.no_align) {
            printSeparator(widths);
            printRow(columns, widths);
            printSeparator(widths);
        }

        for (const auto& row : rows) {
            printRow(row, widths);
        }

        if (g_config.heading && !g_config.tuples_only && !g_config.no_align) {
            printSeparator(widths);
        }
    }

    // Footer - row count (controlled by SET COUNT)
    if (!g_config.tuples_only && g_config.count) {
        out << "(" << rows.size() << " row" << (rows.size() == 1 ? "" : "s") << ")\n";
    }

    // Timing/statistics
    if (show_timing || g_config.stats) {
        out << "Time: " << std::fixed << std::setprecision(3)
            << (exec_time.count() / 1000.0) << " ms\n";
    }
}

// =============================================================================
// SET Command Handling (Firebird ISQL compatible)
// =============================================================================

// Helper to parse ON/OFF value
bool parseOnOff(const std::string& val, bool& result) {
    std::string upper = val;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
    if (upper == "ON" || upper == "TRUE" || upper == "1") {
        result = true;
        return true;
    } else if (upper == "OFF" || upper == "FALSE" || upper == "0") {
        result = false;
        return true;
    }
    return false;
}

// Forward declaration for meta command handler (used by INPUT/OUTPUT commands)
bool handleMetaCommand(const std::string& cmd);

// Handle client-side SET commands
// Returns true if command was handled locally, false if should be sent to server
bool handleSetCommand(const std::string& sql) {
    // Parse SET command
    std::string upper = sql;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Skip leading whitespace
    size_t pos = 0;
    while (pos < upper.size() && (upper[pos] == ' ' || upper[pos] == '\t')) ++pos;

    // Check for SET keyword
    if (upper.substr(pos, 3) != "SET") return false;
    pos += 3;
    while (pos < upper.size() && (upper[pos] == ' ' || upper[pos] == '\t')) ++pos;

    // Get the setting name
    size_t name_start = pos;
    while (pos < upper.size() && upper[pos] != ' ' && upper[pos] != '\t' && upper[pos] != ';') ++pos;
    std::string name = upper.substr(name_start, pos - name_start);

    // Get value (rest after whitespace)
    while (pos < upper.size() && (upper[pos] == ' ' || upper[pos] == '\t')) ++pos;
    std::string value = sql.substr(pos);  // Use original case for value
    // Trim trailing semicolon and whitespace
    while (!value.empty() && (value.back() == ';' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }

    auto& out = getOutput();

    // SET BAIL [ON|OFF]
    if (name == "BAIL") {
        if (value.empty()) {
            out << "BAIL is " << (g_config.bail ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.bail = val;
                out << "BAIL set to " << (g_config.bail ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET BAIL: " << value << "\n";
            }
        }
        return true;
    }

    // SET TERM <terminator>
    if (name == "TERM") {
        if (value.empty()) {
            out << "TERM is '" << g_config.term << "'\n";
        } else {
            g_config.term = value;
            out << "TERM set to '" << g_config.term << "'\n";
        }
        return true;
    }

    // SET COUNT [ON|OFF]
    if (name == "COUNT") {
        if (value.empty()) {
            out << "COUNT is " << (g_config.count ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.count = val;
                out << "COUNT set to " << (g_config.count ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET COUNT: " << value << "\n";
            }
        }
        return true;
    }

    // SET HEADING [ON|OFF]
    if (name == "HEADING") {
        if (value.empty()) {
            out << "HEADING is " << (g_config.heading ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.heading = val;
                out << "HEADING set to " << (g_config.heading ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET HEADING: " << value << "\n";
            }
        }
        return true;
    }

    // SET ECHO [ON|OFF]
    if (name == "ECHO") {
        if (value.empty()) {
            out << "ECHO is " << (g_config.echo ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.echo = val;
                out << "ECHO set to " << (g_config.echo ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET ECHO: " << value << "\n";
            }
        }
        return true;
    }

    // SET LIST [ON|OFF]
    if (name == "LIST") {
        if (value.empty()) {
            out << "LIST is " << (g_config.list ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.list = val;
                out << "LIST set to " << (g_config.list ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET LIST: " << value << "\n";
            }
        }
        return true;
    }

    // SET NULL <string>
    if (name == "NULL") {
        if (value.empty()) {
            out << "NULL display is '" << g_config.null_display << "'\n";
        } else {
            g_config.null_display = value;
            out << "NULL display set to '" << g_config.null_display << "'\n";
        }
        return true;
    }

    // SET WIDTH <column> <n>
    if (name == "WIDTH") {
        // Parse column name and width
        std::istringstream iss(value);
        std::string col_name;
        size_t width;
        if (iss >> col_name >> width) {
            g_config.column_widths[col_name] = width;
            out << "WIDTH for '" << col_name << "' set to " << width << "\n";
        } else if (!col_name.empty()) {
            // Show width for specific column
            auto it = g_config.column_widths.find(col_name);
            if (it != g_config.column_widths.end()) {
                out << "WIDTH for '" << col_name << "' is " << it->second << "\n";
            } else {
                out << "No WIDTH set for '" << col_name << "'\n";
            }
        } else {
            // Show all column widths
            if (g_config.column_widths.empty()) {
                out << "No column widths set\n";
            } else {
                out << "Column widths:\n";
                for (const auto& p : g_config.column_widths) {
                    out << "  " << p.first << " = " << p.second << "\n";
                }
            }
        }
        return true;
    }

    // SET STATS [ON|OFF]
    if (name == "STATS") {
        if (value.empty()) {
            out << "STATS is " << (g_config.stats ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.stats = val;
                out << "STATS set to " << (g_config.stats ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET STATS: " << value << "\n";
            }
        }
        return true;
    }

    // SET PLAN [ON|OFF] - Show query plan before execution
    if (name == "PLAN") {
        if (value.empty()) {
            out << "PLAN is " << (g_config.plan ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.plan = val;
                out << "PLAN set to " << (g_config.plan ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET PLAN: " << value << "\n";
            }
        }
        return true;
    }

    // SET PLANONLY [ON|OFF] - Show plan without executing
    if (name == "PLANONLY") {
        if (value.empty()) {
            out << "PLANONLY is " << (g_config.plan_only ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.plan_only = val;
                // If PLANONLY is ON, also enable PLAN
                if (val) g_config.plan = true;
                out << "PLANONLY set to " << (g_config.plan_only ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET PLANONLY: " << value << "\n";
            }
        }
        return true;
    }

    // SET EXPLAIN [ON|OFF] - Show detailed plan analysis
    if (name == "EXPLAIN") {
        if (value.empty()) {
            out << "EXPLAIN is " << (g_config.explain ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.explain = val;
                // If EXPLAIN is ON, also enable PLAN
                if (val) g_config.plan = true;
                out << "EXPLAIN set to " << (g_config.explain ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET EXPLAIN: " << value << "\n";
            }
        }
        return true;
    }

    // SET NAMES <charset> - Set client character set
    if (name == "NAMES") {
        if (value.empty()) {
            out << "NAMES is '" << g_config.names << "'\n";
        } else {
            g_config.names = value;
            out << "NAMES set to '" << g_config.names << "'\n";
            // TODO: Actually set connection charset when supported
        }
        return true;
    }

    // SET WARNINGS [ON|OFF] - Display warnings
    if (name == "WARNINGS" || name == "WNG") {
        if (value.empty()) {
            out << "WARNINGS is " << (g_config.warnings ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.warnings = val;
                out << "WARNINGS set to " << (g_config.warnings ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET WARNINGS: " << value << "\n";
            }
        }
        return true;
    }

    // DEFINE var=value - Define a variable (for substitution)
    if (name == "DEFINE" || name == "DEF") {
        if (value.empty()) {
            // Show all defined variables
            if (g_config.variables.empty()) {
                out << "No variables defined\n";
            } else {
                for (const auto& [var, val] : g_config.variables) {
                    out << var << " = '" << val << "'\n";
                }
            }
        } else {
            // Parse var=value or var value
            size_t eq_pos = value.find('=');
            if (eq_pos != std::string::npos) {
                std::string var_name = value.substr(0, eq_pos);
                std::string var_value = value.substr(eq_pos + 1);
                // Trim quotes if present
                if (var_value.size() >= 2 &&
                    ((var_value.front() == '\'' && var_value.back() == '\'') ||
                     (var_value.front() == '"' && var_value.back() == '"'))) {
                    var_value = var_value.substr(1, var_value.size() - 2);
                }
                g_config.variables[var_name] = var_value;
                out << "Variable " << var_name << " defined\n";
            } else {
                std::cerr << "Usage: SET DEFINE var=value\n";
            }
        }
        return true;
    }

    // UNDEFINE var - Remove a variable
    if (name == "UNDEFINE" || name == "UNDEF") {
        if (value.empty()) {
            std::cerr << "Usage: SET UNDEFINE varname\n";
        } else {
            if (g_config.variables.erase(value) > 0) {
                out << "Variable " << value << " undefined\n";
            } else {
                out << "Variable " << value << " was not defined\n";
            }
        }
        return true;
    }

    // SET PROMPT <string> - Custom prompt
    if (name == "PROMPT") {
        if (value.empty()) {
            if (g_config.prompt.empty()) {
                out << "PROMPT is using default\n";
            } else {
                out << "PROMPT is '" << g_config.prompt << "'\n";
            }
        } else {
            // Allow special tokens: %d = database, %u = user, %t = time
            g_config.prompt = value;
            out << "PROMPT set to '" << g_config.prompt << "'\n";
        }
        return true;
    }

    // SET TIME [ON|OFF] - Show time in prompt
    if (name == "TIME") {
        if (value.empty()) {
            out << "TIME is " << (g_config.show_time ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.show_time = val;
                out << "TIME set to " << (g_config.show_time ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET TIME: " << value << "\n";
            }
        }
        return true;
    }

    // SET SQL DIALECT N - Set SQL dialect (1, 2, or 3)
    if (name == "SQL") {
        // Check if next word is DIALECT
        std::string upper_value = value;
        std::transform(upper_value.begin(), upper_value.end(), upper_value.begin(), ::toupper);
        if (upper_value.substr(0, 7) == "DIALECT") {
            std::string dialect_str = value.substr(7);
            while (!dialect_str.empty() && (dialect_str[0] == ' ' || dialect_str[0] == '\t')) {
                dialect_str = dialect_str.substr(1);
            }
            if (dialect_str.empty()) {
                out << "SQL DIALECT is " << g_config.sql_dialect << "\n";
            } else {
                try {
                    int dialect = std::stoi(dialect_str);
                    if (dialect < 1 || dialect > 3) {
                        std::cerr << "Error: SQL dialect must be 1, 2, or 3\n";
                    } else {
                        g_config.sql_dialect = dialect;
                        out << "SQL DIALECT set to " << g_config.sql_dialect << "\n";
                    }
                } catch (...) {
                    std::cerr << "Error: Invalid dialect number\n";
                }
            }
            return true;
        }
    }

    // SET AUTODDL [ON|OFF] - Auto-commit DDL statements
    if (name == "AUTODDL") {
        if (value.empty()) {
            out << "AUTODDL is " << (g_config.autoddl ? "ON" : "OFF") << "\n";
        } else {
            bool val;
            if (parseOnOff(value, val)) {
                g_config.autoddl = val;
                out << "AUTODDL set to " << (g_config.autoddl ? "ON" : "OFF") << "\n";
            } else {
                std::cerr << "Invalid value for SET AUTODDL: " << value << "\n";
            }
        }
        return true;
    }

    // SET MAXROWS N - Limit rows returned (0 = unlimited)
    if (name == "MAXROWS") {
        if (value.empty()) {
            out << "MAXROWS is " << g_config.maxrows << (g_config.maxrows == 0 ? " (unlimited)" : "") << "\n";
        } else {
            try {
                int rows = std::stoi(value);
                if (rows < 0) rows = 0;
                g_config.maxrows = rows;
                out << "MAXROWS set to " << g_config.maxrows << (g_config.maxrows == 0 ? " (unlimited)" : "") << "\n";
            } catch (...) {
                std::cerr << "Invalid value for SET MAXROWS: " << value << "\n";
            }
        }
        return true;
    }

    // SET LOCAL_TIMEOUT N - Statement timeout in seconds (0 = unlimited)
    if (name == "LOCAL_TIMEOUT") {
        if (value.empty()) {
            out << "LOCAL_TIMEOUT is " << g_config.local_timeout << " seconds" << (g_config.local_timeout == 0 ? " (unlimited)" : "") << "\n";
        } else {
            try {
                int timeout = std::stoi(value);
                if (timeout < 0) timeout = 0;
                g_config.local_timeout = timeout;
                out << "LOCAL_TIMEOUT set to " << g_config.local_timeout << " seconds" << (g_config.local_timeout == 0 ? " (unlimited)" : "") << "\n";
            } catch (...) {
                std::cerr << "Invalid value for SET LOCAL_TIMEOUT: " << value << "\n";
            }
        }
        return true;
    }

    // Not a client-side SET command, let server handle it
    return false;
}

/**
 * Substitute variables with concatenation support for command arguments.
 * Handles patterns like: :varname||'literal' or :var1||:var2||'suffix'
 *
 * Examples:
 *   :basepath||'file.csv'  -> /home/user/reports/file.csv
 *   :dir||:filename        -> /data/output.txt
 *   'prefix_'||:name||'.sql' -> prefix_report.sql
 */
static std::string substituteVariablesWithConcat(const std::string& expr) {
    std::string result;
    size_t i = 0;

    while (i < expr.size()) {
        // Skip whitespace around ||
        while (i < expr.size() && (expr[i] == ' ' || expr[i] == '\t')) ++i;

        // Check for || concatenation operator
        if (i + 1 < expr.size() && expr[i] == '|' && expr[i+1] == '|') {
            i += 2;
            // Skip whitespace after ||
            while (i < expr.size() && (expr[i] == ' ' || expr[i] == '\t')) ++i;
            continue;
        }

        // Check for quoted string literal
        if (expr[i] == '\'' || expr[i] == '"') {
            char quote = expr[i];
            ++i;
            std::string literal;
            while (i < expr.size() && expr[i] != quote) {
                // Handle escaped quotes
                if (expr[i] == '\\' && i + 1 < expr.size()) {
                    ++i;
                    literal += expr[i];
                } else {
                    literal += expr[i];
                }
                ++i;
            }
            if (i < expr.size()) ++i;  // Skip closing quote
            result += literal;
            continue;
        }

        // Check for variable reference (:varname or &varname)
        if (expr[i] == ':' || expr[i] == '&') {
            ++i;
            std::string varname;
            while (i < expr.size() && (std::isalnum(expr[i]) || expr[i] == '_')) {
                varname += expr[i];
                ++i;
            }
            if (!varname.empty()) {
                auto it = g_config.variables.find(varname);
                if (it != g_config.variables.end()) {
                    result += it->second;
                } else {
                    // Variable not found, keep original
                    result += (expr[i-varname.size()-1] == ':' ? ":" : "&") + varname;
                }
            }
            continue;
        }

        // Regular character (part of unquoted literal)
        std::string literal;
        while (i < expr.size() && expr[i] != '|' && expr[i] != ':' && expr[i] != '&' &&
               expr[i] != '\'' && expr[i] != '"' && expr[i] != ' ' && expr[i] != '\t') {
            literal += expr[i];
            ++i;
        }
        result += literal;
    }

    return result;
}

/**
 * Substitute variables in SQL string.
 * Replaces :varname and &varname with their values.
 */
static std::string substituteVariables(const std::string& sql) {
    if (g_config.variables.empty()) {
        return sql;
    }

    std::string result = sql;

    // Substitute :varname and &varname patterns
    for (const auto& [var, val] : g_config.variables) {
        // Replace :varname (Firebird style)
        std::string pattern1 = ":" + var;
        size_t pos = 0;
        while ((pos = result.find(pattern1, pos)) != std::string::npos) {
            // Make sure it's not part of a longer identifier
            size_t end_pos = pos + pattern1.size();
            if (end_pos >= result.size() || !std::isalnum(result[end_pos])) {
                result.replace(pos, pattern1.size(), val);
                pos += val.size();
            } else {
                pos += pattern1.size();
            }
        }

        // Replace &varname (Oracle/Firebird interactive style)
        std::string pattern2 = "&" + var;
        pos = 0;
        while ((pos = result.find(pattern2, pos)) != std::string::npos) {
            size_t end_pos = pos + pattern2.size();
            if (end_pos >= result.size() || !std::isalnum(result[end_pos])) {
                result.replace(pos, pattern2.size(), val);
                pos += val.size();
            } else {
                pos += pattern2.size();
            }
        }
    }

    return result;
}

// =============================================================================
// SQL Execution
// =============================================================================

/**
 * Check if a SQL statement can have an execution plan (is explainable).
 * Only DML statements (SELECT, INSERT, UPDATE, DELETE) can be explained.
 */
static bool isExplainableStatement(const std::string& sql) {
    std::string upper = sql;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Skip leading whitespace
    size_t pos = upper.find_first_not_of(" \t\n\r");
    if (pos == std::string::npos) return false;

    std::string start = upper.substr(pos, 10);
    return start.find("SELECT") == 0 ||
           start.find("INSERT") == 0 ||
           start.find("UPDATE") == 0 ||
           start.find("DELETE") == 0 ||
           start.find("WITH") == 0;  // CTEs are also explainable
}

/**
 * Display query execution plan using EXPLAIN.
 * Returns true on success, false on error.
 */
static bool displayQueryPlan(const std::string& sql) {
    auto& out = getOutput();

    // Build EXPLAIN query based on settings
    std::string explain_sql = "EXPLAIN ";
    if (g_config.explain) {
        // SET EXPLAIN shows detailed analysis with costs
        explain_sql = "EXPLAIN (VERBOSE, COSTS) ";
    }
    explain_sql += sql;

    ResultSet plan_results;
    core::ErrorContext ctx;
    core::Status status = g_connection->executeQuery(explain_sql, &plan_results, &ctx);

    if (status != core::Status::OK) {
        std::cerr << "Error getting execution plan: " << ctx.message << "\n";
        return false;
    }

    // Display plan
    out << "PLAN:\n";
    while (plan_results.next()) {
        // Plan output is typically in the first column
        std::string plan_line = plan_results.getString(0);
        out << "  " << plan_line << "\n";
    }
    out << "\n";

    return true;
}

bool executeSQL(const std::string& sql) {
    // Check for client-side SET commands first
    if (handleSetCommand(sql)) {
        return true;  // Handled locally
    }

    // Check for client-side SHOW commands (SQL DIALECT)
    {
        std::string upper = sql;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        // Trim leading whitespace
        size_t pos = 0;
        while (pos < upper.size() && (upper[pos] == ' ' || upper[pos] == '\t')) ++pos;
        // Remove trailing semicolon for comparison
        std::string trimmed = upper.substr(pos);
        while (!trimmed.empty() && (trimmed.back() == ';' || trimmed.back() == ' ' || trimmed.back() == '\t')) {
            trimmed.pop_back();
        }
        if (trimmed == "SHOW SQL DIALECT") {
            auto& out = getOutput();
            out << "SQL DIALECT is " << g_config.sql_dialect << "\n";
            return true;
        }
    }

    // Handle transaction commands (Firebird compatible)
    {
        std::string upper = sql;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
        // Trim leading whitespace
        size_t pos = 0;
        while (pos < upper.size() && (upper[pos] == ' ' || upper[pos] == '\t')) ++pos;
        // Remove trailing semicolon/whitespace for comparison
        std::string trimmed = upper.substr(pos);
        while (!trimmed.empty() && (trimmed.back() == ';' || trimmed.back() == ' ' || trimmed.back() == '\t')) {
            trimmed.pop_back();
        }

        // COMMIT [WORK] [RETAIN]
        if (trimmed == "COMMIT" || trimmed == "COMMIT WORK" ||
            trimmed == "COMMIT RETAIN" || trimmed == "COMMIT WORK RETAIN") {
            if (!g_connection || !g_connection->isConnected()) {
                std::cerr << "Error: Not connected to database\n";
                return false;
            }
            bool retain = (trimmed.find("RETAIN") != std::string::npos);
            core::ErrorContext ctx;
            core::Status status = g_connection->commit(&ctx);
            if (status != core::Status::OK) {
                std::cerr << "Error: " << ctx.message << "\n";
                return false;
            }
            auto& out = getOutput();
            if (retain) {
                // Start a new transaction immediately (COMMIT RETAIN behavior)
                status = g_connection->beginTransaction(&ctx);
                if (status != core::Status::OK) {
                    std::cerr << "Warning: Failed to start new transaction after COMMIT RETAIN: " << ctx.message << "\n";
                }
                out << "Transaction committed and retained.\n";
            } else {
                out << "Transaction committed.\n";
            }
            return true;
        }

        // ROLLBACK [WORK] [RETAIN] or ROLLBACK [WORK] [TO [SAVEPOINT] name]
        if (trimmed.substr(0, 8) == "ROLLBACK") {
            if (!g_connection || !g_connection->isConnected()) {
                std::cerr << "Error: Not connected to database\n";
                return false;
            }
            std::string rest = trimmed.substr(8);
            // Trim leading spaces
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);
            // Skip optional WORK keyword
            if (rest.substr(0, 4) == "WORK") {
                rest = rest.substr(4);
                while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);
            }

            core::ErrorContext ctx;
            auto& out = getOutput();

            // Check for ROLLBACK TO [SAVEPOINT] name
            if (rest.substr(0, 2) == "TO") {
                rest = rest.substr(2);
                while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);
                // Skip optional SAVEPOINT keyword
                if (rest.substr(0, 9) == "SAVEPOINT") {
                    rest = rest.substr(9);
                    while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);
                }
                // rest is now the savepoint name
                if (rest.empty()) {
                    std::cerr << "Error: ROLLBACK TO requires a savepoint name\n";
                    return false;
                }
                core::Status status = g_connection->rollbackTo(rest, &ctx);
                if (status != core::Status::OK) {
                    std::cerr << "Error: " << ctx.message << "\n";
                    return false;
                }
                out << "Rolled back to savepoint '" << rest << "'.\n";
                return true;
            }

            // Regular ROLLBACK [RETAIN]
            bool retain = (rest == "RETAIN");
            core::Status status = g_connection->rollback(&ctx);
            if (status != core::Status::OK) {
                std::cerr << "Error: " << ctx.message << "\n";
                return false;
            }
            if (retain) {
                // Start a new transaction immediately (ROLLBACK RETAIN behavior)
                status = g_connection->beginTransaction(&ctx);
                if (status != core::Status::OK) {
                    std::cerr << "Warning: Failed to start new transaction after ROLLBACK RETAIN: " << ctx.message << "\n";
                }
                out << "Transaction rolled back and retained.\n";
            } else {
                out << "Transaction rolled back.\n";
            }
            return true;
        }

        // SAVEPOINT name
        if (trimmed.substr(0, 9) == "SAVEPOINT") {
            if (!g_connection || !g_connection->isConnected()) {
                std::cerr << "Error: Not connected to database\n";
                return false;
            }
            std::string name = trimmed.substr(9);
            while (!name.empty() && (name[0] == ' ' || name[0] == '\t')) name = name.substr(1);
            if (name.empty()) {
                std::cerr << "Error: SAVEPOINT requires a name\n";
                return false;
            }
            core::ErrorContext ctx;
            core::Status status = g_connection->savepoint(name, &ctx);
            if (status != core::Status::OK) {
                std::cerr << "Error: " << ctx.message << "\n";
                return false;
            }
            auto& out = getOutput();
            out << "Savepoint '" << name << "' created.\n";
            return true;
        }

        // RELEASE SAVEPOINT name
        if (trimmed.substr(0, 17) == "RELEASE SAVEPOINT") {
            if (!g_connection || !g_connection->isConnected()) {
                std::cerr << "Error: Not connected to database\n";
                return false;
            }
            std::string name = trimmed.substr(17);
            while (!name.empty() && (name[0] == ' ' || name[0] == '\t')) name = name.substr(1);
            if (name.empty()) {
                std::cerr << "Error: RELEASE SAVEPOINT requires a name\n";
                return false;
            }
            core::ErrorContext ctx;
            core::Status status = g_connection->releaseSavepoint(name, &ctx);
            if (status != core::Status::OK) {
                std::cerr << "Error: " << ctx.message << "\n";
                return false;
            }
            auto& out = getOutput();
            out << "Savepoint '" << name << "' released.\n";
            return true;
        }

        // SET TRANSACTION - start explicit transaction
        if (trimmed.substr(0, 15) == "SET TRANSACTION") {
            if (!g_connection || !g_connection->isConnected()) {
                std::cerr << "Error: Not connected to database\n";
                return false;
            }
            // For now, just start a basic transaction
            // TODO: Parse transaction parameters (READ ONLY, READ WRITE, isolation levels, etc.)
            core::ErrorContext ctx;
            core::Status status = g_connection->beginTransaction(&ctx);
            if (status != core::Status::OK) {
                std::cerr << "Error: " << ctx.message << "\n";
                return false;
            }
            auto& out = getOutput();
            out << "Transaction started.\n";
            return true;
        }

        // WHENEVER ERROR|SQLERROR CONTINUE|STOP|EXIT [N]
        if (trimmed.substr(0, 8) == "WHENEVER") {
            std::string rest = trimmed.substr(8);
            while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);

            // Check for ERROR or SQLERROR
            if (rest.substr(0, 5) == "ERROR" || rest.substr(0, 8) == "SQLERROR") {
                size_t skip = (rest.substr(0, 8) == "SQLERROR") ? 8 : 5;
                rest = rest.substr(skip);
                while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);

                auto& out = getOutput();
                if (rest == "CONTINUE") {
                    g_config.on_error = IsqlConfig::ErrorAction::CONTINUE;
                    out << "Error handling set to CONTINUE.\n";
                    return true;
                } else if (rest == "STOP") {
                    g_config.on_error = IsqlConfig::ErrorAction::STOP;
                    out << "Error handling set to STOP.\n";
                    return true;
                } else if (rest.substr(0, 4) == "EXIT") {
                    g_config.on_error = IsqlConfig::ErrorAction::EXIT;
                    rest = rest.substr(4);
                    while (!rest.empty() && (rest[0] == ' ' || rest[0] == '\t')) rest = rest.substr(1);
                    if (!rest.empty()) {
                        try {
                            g_config.exit_code = std::stoi(rest);
                        } catch (...) {
                            g_config.exit_code = 1;
                        }
                    } else {
                        g_config.exit_code = 1;
                    }
                    out << "Error handling set to EXIT with code " << g_config.exit_code << ".\n";
                    return true;
                } else {
                    std::cerr << "Error: WHENEVER ERROR expects CONTINUE, STOP, or EXIT [code]\n";
                    return false;
                }
            } else {
                std::cerr << "Error: WHENEVER expects ERROR or SQLERROR\n";
                return false;
            }
        }

        // EXIT [N] - Exit with optional code
        if (trimmed == "EXIT" || trimmed.substr(0, 5) == "EXIT ") {
            std::string code_str = (trimmed.length() > 4) ? trimmed.substr(5) : "";
            while (!code_str.empty() && (code_str[0] == ' ' || code_str[0] == '\t')) code_str = code_str.substr(1);

            int exit_code = 0;
            if (!code_str.empty()) {
                try {
                    exit_code = std::stoi(code_str);
                } catch (...) {
                    std::cerr << "Error: Invalid exit code: " << code_str << "\n";
                    return false;
                }
            }
            g_config.exit_code = exit_code;
            g_running = false;
            return true;
        }

        // QUIT - Quit immediately (Firebird compatible)
        if (trimmed == "QUIT") {
            g_running = false;
            return true;
        }

        // INPUT [filename] - Include/execute a script file (Firebird compatible)
        // Can also use SET INPUT <filename>
        if (trimmed.substr(0, 5) == "INPUT" || trimmed.substr(0, 9) == "SET INPUT") {
            std::string filename;
            if (trimmed.substr(0, 9) == "SET INPUT") {
                filename = trimmed.substr(9);
            } else {
                filename = trimmed.substr(5);
            }
            // Trim whitespace
            while (!filename.empty() && (filename[0] == ' ' || filename[0] == '\t')) filename = filename.substr(1);
            while (!filename.empty() && (filename.back() == ' ' || filename.back() == '\t')) filename.pop_back();

            if (filename.empty()) {
                std::cerr << "Usage: INPUT <filename> or SET INPUT <filename>\n";
                return false;
            }

            // Perform variable substitution and concatenation on filename
            filename = substituteVariablesWithConcat(filename);

            // Use existing \i implementation
            return handleMetaCommand("\\i " + filename);
        }

        // OUTPUT [filename] - Redirect output to file (Firebird compatible)
        // No filename means redirect back to stdout
        if (trimmed.substr(0, 6) == "OUTPUT" || trimmed.substr(0, 10) == "SET OUTPUT") {
            std::string filename;
            if (trimmed.substr(0, 10) == "SET OUTPUT") {
                filename = trimmed.substr(10);
            } else {
                filename = trimmed.substr(6);
            }
            // Trim whitespace
            while (!filename.empty() && (filename[0] == ' ' || filename[0] == '\t')) filename = filename.substr(1);
            while (!filename.empty() && (filename.back() == ' ' || filename.back() == '\t')) filename.pop_back();

            // Perform variable substitution and concatenation on filename
            if (!filename.empty()) {
                filename = substituteVariablesWithConcat(filename);
            }

            // Use existing \o implementation
            if (filename.empty()) {
                return handleMetaCommand("\\o");
            } else {
                return handleMetaCommand("\\o " + filename);
            }
        }

        // ERROR [filename] or SET ERROR [filename] - Redirect errors to file
        // No filename means redirect back to stderr
        if (trimmed.substr(0, 5) == "ERROR" || trimmed.substr(0, 9) == "SET ERROR") {
            std::string filename;
            if (trimmed.substr(0, 9) == "SET ERROR") {
                filename = trimmed.substr(9);
            } else {
                filename = trimmed.substr(5);
            }
            // Trim whitespace
            while (!filename.empty() && (filename[0] == ' ' || filename[0] == '\t')) filename = filename.substr(1);
            while (!filename.empty() && (filename.back() == ' ' || filename.back() == '\t')) filename.pop_back();

            auto& out = getOutput();

            if (filename.empty()) {
                // Redirect back to original stderr
                if (g_error_file) {
                    // Restore original cerr buffer
                    if (g_original_cerr) {
                        std::cerr.rdbuf(g_original_cerr);
                        g_original_cerr = nullptr;
                    }
                    g_error_file->close();
                    delete g_error_file;
                    g_error_file = nullptr;
                    out << "Errors redirected to stderr\n";
                } else {
                    out << "Errors are already going to stderr\n";
                }
            } else {
                // Perform variable substitution and concatenation on filename
                filename = substituteVariablesWithConcat(filename);

                // Close existing error file if open
                if (g_error_file) {
                    if (g_original_cerr) {
                        std::cerr.rdbuf(g_original_cerr);
                    }
                    g_error_file->close();
                    delete g_error_file;
                }

                // Open new error file
                g_error_file = new std::ofstream(filename);
                if (!g_error_file->is_open()) {
                    std::cerr << "Error: Cannot open error file: " << filename << "\n";
                    delete g_error_file;
                    g_error_file = nullptr;
                    return false;
                }

                // Redirect cerr to the file
                g_original_cerr = std::cerr.rdbuf();
                std::cerr.rdbuf(g_error_file->rdbuf());
                out << "Errors redirected to " << filename << "\n";
            }
            return true;
        }
    }

    if (!g_connection || !g_connection->isConnected()) {
        std::cerr << "Error: Not connected to database\n";
        return false;
    }

    // Perform variable substitution
    std::string processed_sql = substituteVariables(sql);

    if (g_config.echo) {
        getOutput() << processed_sql << "\n";
    }

    // If SET PLAN is ON and this is an explainable statement, show the plan first
    if (g_config.plan && isExplainableStatement(processed_sql)) {
        if (!displayQueryPlan(processed_sql)) {
            // Plan display failed, but we can still try to execute
            // (unless plan_only is set)
            if (g_config.plan_only) {
                return false;
            }
        }

        // If SET PLANONLY is ON, don't execute the actual query
        if (g_config.plan_only) {
            return true;
        }
    }

    auto start = std::chrono::high_resolution_clock::now();

    ResultSet results;
    core::ErrorContext ctx;
    core::Status status = g_connection->executeQuery(processed_sql, &results, &ctx);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    if (status != core::Status::OK) {
        std::cerr << "Error: " << ctx.message << "\n";
        return false;
    }

    // Store as last query for \watch
    g_config.last_query = processed_sql;

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
  \dr               List roles
  \ds               List schemas
  \dS               List system tables/views
  \dc [table]       List check constraints
  \dg [object]      Show grants/privileges
  \dv [name]        List views (or show view definition)
  \df [name]        List functions (or show function)
  \l                List databases
  \c <database>     Connect to database
  \i <file>         Execute commands from file
  \o [file]         Write output to file (no arg = stop)
  \e [file]         Edit SQL in $EDITOR (or file if given)
  \watch [N]        Repeat last query every N seconds (default: 2)
  \copy table TO 'file'    Export table to CSV file
  \copy table FROM 'file'  Import CSV file into table
  \copy (SELECT...) TO 'file'  Export query result to CSV
  \timing [on|off]  Toggle/set timing display
  \x [on|off]       Toggle/set expanded display
  \! <command>      Execute shell command
  \echo <text>      Print text
  \set              Show all settings and variables
  \pset <opt> <val> Set output formatting option
    format          Output format (aligned, unaligned, csv, html, json)
    tuples_only     Only show data rows (on/off)
    expanded        Expanded display mode (on/off)

SET Commands (Firebird ISQL compatible):
  SET SQL DIALECT N       Set SQL dialect (1, 2, or 3)
  SET BAIL [ON|OFF]       Stop on first error
  SET TERM <char>         Change statement terminator
  SET COUNT [ON|OFF]      Display row counts
  SET HEADING [ON|OFF]    Show column headings
  SET ECHO [ON|OFF]       Echo commands before execution
  SET LIST [ON|OFF]       Vertical display mode
  SET NULL <string>       String to display for NULL values
  SET WIDTH <col> <n>     Set column display width
  SET STATS [ON|OFF]      Show timing statistics
  SET PLAN [ON|OFF]       Show query execution plan
  SET PLANONLY [ON|OFF]   Show plan only, don't execute
  SET EXPLAIN [ON|OFF]    Show detailed plan analysis
  SET NAMES <charset>     Set client character set
  SET WARNINGS [ON|OFF]   Display warnings
  SET AUTODDL [ON|OFF]    Auto-commit DDL statements
  SET MAXROWS N           Limit rows returned (0 = unlimited)
  SET LOCAL_TIMEOUT N     Statement timeout in seconds (0 = unlimited)
  SET TIME [ON|OFF]       Show time in prompt
  SET PROMPT <string>     Custom prompt (%d=database, %u=user, %t=time)
  SET DEFINE var=value    Define a variable for substitution
  SET UNDEFINE var        Remove a defined variable

SHOW Commands:
  SHOW SQL DIALECT        Show current SQL dialect

Transaction Commands (Firebird compatible):
  SET TRANSACTION         Start explicit transaction
  COMMIT [WORK] [RETAIN]  Commit transaction (RETAIN keeps transaction open)
  ROLLBACK [WORK] [RETAIN] Rollback transaction
  ROLLBACK TO [SAVEPOINT] name  Rollback to savepoint
  SAVEPOINT name          Create a savepoint
  RELEASE SAVEPOINT name  Release a savepoint

Error Handling Commands (Firebird compatible):
  WHENEVER ERROR CONTINUE   Continue on error (default)
  WHENEVER ERROR STOP       Stop processing on error
  WHENEVER ERROR EXIT [N]   Exit with code N on error
  WHENEVER SQLERROR ...     Same as WHENEVER ERROR
  EXIT [N]                  Exit with optional code
  QUIT                      Quit immediately

Scripting Commands (Firebird compatible):
  INPUT <filename>          Execute script file, return to caller when done
  SET INPUT <filename>      Same as INPUT
  OUTPUT [filename]         Redirect output to file (no arg = restore stdout)
  SET OUTPUT [filename]     Same as OUTPUT
  ERROR [filename]          Redirect errors to file (no arg = restore stderr)
  SET ERROR [filename]      Same as ERROR

Variable substitution and concatenation:
  :varname and &varname are replaced with defined variable values.
  In scripting commands, use || to concatenate:
    DEFINE basepath=/home/user/reports/
    OUTPUT :basepath||'report.csv'    -> /home/user/reports/report.csv
    OUTPUT :dir||:filename            -> concatenates two variables

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

    if (command == "dr") {
        // List roles
        return executeSQL("SELECT name, is_system FROM sb_catalog.sb_roles ORDER BY name");
    }

    if (command == "ds") {
        // List schemas
        return executeSQL("SELECT schema_name FROM sb_catalog.sb_schemas ORDER BY schema_name");
    }

    if (command == "dS") {
        // List system tables
        return executeSQL("SELECT name, 'TABLE' as type FROM sb_catalog.sb_tables WHERE name LIKE 'sb_%' "
                          "UNION ALL SELECT name, 'VIEW' FROM sb_catalog.sb_views WHERE name LIKE 'sb_%' "
                          "ORDER BY name");
    }

    if (command == "dc") {
        // Show check constraints
        if (arg.empty()) {
            return executeSQL("SELECT c.name, c.table_name, c.check_expression "
                              "FROM sb_catalog.sb_check_constraints c "
                              "ORDER BY c.table_name, c.name");
        } else {
            return executeSQL("SELECT c.name, c.check_expression "
                              "FROM sb_catalog.sb_check_constraints c "
                              "WHERE c.table_name = '" + arg + "' "
                              "ORDER BY c.name");
        }
    }

    if (command == "dg") {
        // Show grants/privileges
        if (arg.empty()) {
            return executeSQL("SELECT grantee, object_name, privilege_type, is_grantable "
                              "FROM sb_catalog.sb_privileges "
                              "ORDER BY object_name, grantee, privilege_type");
        } else {
            return executeSQL("SELECT grantee, privilege_type, is_grantable "
                              "FROM sb_catalog.sb_privileges "
                              "WHERE object_name = '" + arg + "' "
                              "ORDER BY grantee, privilege_type");
        }
    }

    if (command == "dv") {
        // List views
        if (arg.empty()) {
            return executeSQL("SELECT name FROM sb_catalog.sb_views WHERE name NOT LIKE 'sb_%' ORDER BY name");
        } else {
            return executeSQL("SELECT name, definition FROM sb_catalog.sb_views WHERE name = '" + arg + "'");
        }
    }

    if (command == "df") {
        // List functions
        if (arg.empty()) {
            return executeSQL("SELECT name, return_type FROM sb_catalog.sb_functions "
                              "WHERE name NOT LIKE 'sb_%' ORDER BY name");
        } else {
            return executeSQL("SELECT name, return_type, definition FROM sb_catalog.sb_functions "
                              "WHERE name = '" + arg + "'");
        }
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
        bool had_error = false;
        while (std::getline(file, line)) {
            // Skip comments
            if (line.empty() || line[0] == '#' || line.substr(0, 2) == "--") {
                continue;
            }

            sql += line;
            // Check for statement terminator (supports custom terminator)
            const std::string& term = g_config.term;
            if (sql.size() >= term.size() && sql.substr(sql.size() - term.size()) == term) {
                // Remove terminator before executing
                std::string sql_to_exec = sql.substr(0, sql.size() - term.size());
                while (!sql_to_exec.empty() && (sql_to_exec.back() == ' ' || sql_to_exec.back() == '\t')) {
                    sql_to_exec.pop_back();
                }
                bool success = executeSQL(sql_to_exec);
                if (!success) {
                    had_error = true;
                    if (g_config.bail) {
                        std::cerr << "Stopping due to error (SET BAIL is ON)\n";
                        break;
                    }
                }
                sql.clear();
            } else {
                sql += " ";
            }
        }

        if (!sql.empty() && !(had_error && g_config.bail)) {
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

    // \e [file] - Edit SQL in $EDITOR
    if (command == "e" || command == "edit") {
        const char* editor = getenv("VISUAL");
        if (!editor) editor = getenv("EDITOR");
        if (!editor) editor = "vi";

        std::string tmpfile;
        if (arg.empty()) {
            // Create temp file with current query buffer or last command
            tmpfile = "/tmp/sb_isql_edit.sql";
            // For now, create empty file; could save last query here
            std::ofstream tmp(tmpfile);
            tmp << "-- Enter SQL here\n";
            tmp.close();
        } else {
            tmpfile = arg;
        }

        std::string cmd = std::string(editor) + " \"" + tmpfile + "\"";
        int ret = system(cmd.c_str());

        if (ret == 0) {
            // Read the edited file and execute
            if (arg.empty()) {
                std::ifstream edited(tmpfile);
                if (edited) {
                    std::stringstream buffer;
                    buffer << edited.rdbuf();
                    std::string sql = buffer.str();
                    // Remove trailing whitespace and comments-only lines
                    while (!sql.empty() && (sql.back() == '\n' || sql.back() == ' ')) {
                        sql.pop_back();
                    }
                    if (!sql.empty() && sql.substr(0, 2) != "--") {
                        // Check if it ends with terminator
                        if (sql.size() >= g_config.term.size() &&
                            sql.substr(sql.size() - g_config.term.size()) == g_config.term) {
                            sql = sql.substr(0, sql.size() - g_config.term.size());
                        }
                        if (!sql.empty()) {
                            executeSQL(sql);
                        }
                    }
                }
                // Clean up temp file
                std::remove(tmpfile.c_str());
            }
            // If editing a named file, just open it (user can use \i to execute)
        } else {
            std::cerr << "Editor exited with status " << ret << "\n";
        }
        return true;
    }

    // \watch N - Repeat last query every N seconds
    if (command == "watch") {
        if (g_config.last_query.empty()) {
            std::cerr << "No query to repeat. Execute a query first.\n";
            return true;
        }

        int interval = 2;  // default 2 seconds
        if (!arg.empty()) {
            try {
                interval = std::stoi(arg);
                if (interval < 1) interval = 1;
            } catch (...) {
                std::cerr << "Invalid interval. Usage: \\watch [seconds]\n";
                return true;
            }
        }

        std::cout << "Repeating query every " << interval << " seconds. Press Ctrl+C to stop.\n\n";

        // Use volatile flag for signal handling
        volatile bool watch_running = true;

        // Temporarily override signal handler to stop watch
        auto old_handler = signal(SIGINT, [](int) {
            std::cout << "\nWatch stopped.\n";
        });

        while (g_running) {
            // Show timestamp
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::cout << "-- " << std::ctime(&time);

            // Execute the query
            if (!executeSQL(g_config.last_query)) {
                break;
            }

            std::cout << "\n";

            // Sleep with interrupt check
            for (int i = 0; i < interval * 10 && g_running; ++i) {
                usleep(100000);  // 100ms intervals to check for interrupt
            }

            // Check if interrupted
            if (!g_running) {
                g_running = true;  // Reset for continued operation
                break;
            }
        }

        // Restore signal handler
        signal(SIGINT, old_handler);
        return true;
    }

    // \copy table TO|FROM 'file' - Copy data to/from file
    if (command == "copy") {
        // Parse: \copy table TO|FROM 'file' [OPTIONS]
        // Formats: \copy table TO 'file.csv'
        //          \copy table FROM 'file.csv'
        //          \copy (SELECT ...) TO 'file.csv'

        std::string rest = arg;

        // Extract table name or query
        std::string table_or_query;
        bool is_query = false;

        if (!rest.empty() && rest[0] == '(') {
            // It's a query: \copy (SELECT ...) TO 'file'
            size_t paren_depth = 1;
            size_t pos = 1;
            while (pos < rest.size() && paren_depth > 0) {
                if (rest[pos] == '(') paren_depth++;
                else if (rest[pos] == ')') paren_depth--;
                pos++;
            }
            if (paren_depth != 0) {
                std::cerr << "Error: Unbalanced parentheses in query.\n";
                return true;
            }
            table_or_query = rest.substr(1, pos - 2);  // Extract query without parens
            rest = rest.substr(pos);
            is_query = true;
        } else {
            // It's a table name
            size_t space = rest.find(' ');
            if (space == std::string::npos) {
                std::cerr << "Usage: \\copy table TO|FROM 'file'\n";
                return true;
            }
            table_or_query = rest.substr(0, space);
            rest = rest.substr(space);
        }

        // Trim leading spaces
        while (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);

        // Parse TO or FROM
        bool is_export = false;
        if (rest.size() >= 2 && (rest.substr(0, 2) == "TO" || rest.substr(0, 2) == "to")) {
            is_export = true;
            rest = rest.substr(2);
        } else if (rest.size() >= 4 && (rest.substr(0, 4) == "FROM" || rest.substr(0, 4) == "from")) {
            is_export = false;
            rest = rest.substr(4);
        } else {
            std::cerr << "Usage: \\copy table TO|FROM 'file'\n";
            return true;
        }

        // Trim leading spaces
        while (!rest.empty() && rest[0] == ' ') rest = rest.substr(1);

        // Parse filename (quoted or unquoted)
        std::string filename;
        if (!rest.empty() && (rest[0] == '\'' || rest[0] == '"')) {
            char quote = rest[0];
            size_t end = rest.find(quote, 1);
            if (end == std::string::npos) {
                std::cerr << "Error: Unterminated filename string.\n";
                return true;
            }
            filename = rest.substr(1, end - 1);
        } else {
            size_t space = rest.find(' ');
            filename = (space == std::string::npos) ? rest : rest.substr(0, space);
        }

        if (filename.empty()) {
            std::cerr << "Error: No filename specified.\n";
            return true;
        }

        if (is_export) {
            // EXPORT: \copy table TO 'file' or \copy (query) TO 'file'
            std::ofstream outfile(filename);
            if (!outfile) {
                std::cerr << "Error: Cannot open file '" << filename << "' for writing.\n";
                return true;
            }

            std::string query = is_query ? table_or_query : ("SELECT * FROM " + table_or_query);

            // Execute query and write CSV
            ResultSet results;
            core::ErrorContext ctx;
            core::Status status = g_connection->executeQuery(query, &results, &ctx);

            if (status != core::Status::OK) {
                std::cerr << "Error: " << ctx.message << "\n";
                outfile.close();
                return true;
            }

            // Write header
            size_t col_count = results.getColumnCount();
            for (size_t i = 0; i < col_count; ++i) {
                if (i > 0) outfile << ",";
                std::string col = results.getColumnName(i);
                if (col.find_first_of(",\"\n") != std::string::npos) {
                    outfile << "\"";
                    for (char c : col) {
                        if (c == '"') outfile << "\"\"";
                        else outfile << c;
                    }
                    outfile << "\"";
                } else {
                    outfile << col;
                }
            }
            outfile << "\n";

            // Write data rows
            size_t row_count = 0;
            while (results.next()) {
                for (size_t i = 0; i < col_count; ++i) {
                    if (i > 0) outfile << ",";
                    std::string val = results.isNull(i) ? "" : results.getString(i);
                    if (val.find_first_of(",\"\n") != std::string::npos) {
                        outfile << "\"";
                        for (char c : val) {
                            if (c == '"') outfile << "\"\"";
                            else outfile << c;
                        }
                        outfile << "\"";
                    } else {
                        outfile << val;
                    }
                }
                outfile << "\n";
                row_count++;
            }

            outfile.close();
            std::cout << "COPY " << row_count << " rows to '" << filename << "'\n";
        } else {
            // IMPORT: \copy table FROM 'file'
            if (is_query) {
                std::cerr << "Error: Cannot use query with FROM, use table name.\n";
                return true;
            }

            std::ifstream infile(filename);
            if (!infile) {
                std::cerr << "Error: Cannot open file '" << filename << "' for reading.\n";
                return true;
            }

            // Read header line to get column names
            std::string header_line;
            if (!std::getline(infile, header_line)) {
                std::cerr << "Error: Empty file.\n";
                return true;
            }

            // Parse CSV header
            std::vector<std::string> columns;
            std::string field;
            bool in_quote = false;
            for (size_t i = 0; i <= header_line.size(); ++i) {
                char c = (i < header_line.size()) ? header_line[i] : ',';
                if (c == '"') {
                    in_quote = !in_quote;
                } else if (c == ',' && !in_quote) {
                    columns.push_back(field);
                    field.clear();
                } else {
                    field += c;
                }
            }
            if (!field.empty() || columns.empty()) {
                columns.push_back(field);
            }

            // Build INSERT statement
            std::string insert_base = "INSERT INTO " + table_or_query + " (";
            for (size_t i = 0; i < columns.size(); ++i) {
                if (i > 0) insert_base += ", ";
                insert_base += columns[i];
            }
            insert_base += ") VALUES (";

            // Read and insert data rows
            size_t row_count = 0;
            size_t error_count = 0;
            std::string data_line;
            while (std::getline(infile, data_line)) {
                // Parse CSV row
                std::vector<std::string> values;
                field.clear();
                in_quote = false;
                for (size_t i = 0; i <= data_line.size(); ++i) {
                    char c = (i < data_line.size()) ? data_line[i] : ',';
                    if (c == '"') {
                        if (in_quote && i + 1 < data_line.size() && data_line[i + 1] == '"') {
                            field += '"';
                            i++;  // Skip escaped quote
                        } else {
                            in_quote = !in_quote;
                        }
                    } else if (c == ',' && !in_quote) {
                        values.push_back(field);
                        field.clear();
                    } else {
                        field += c;
                    }
                }
                if (!field.empty() || values.empty()) {
                    values.push_back(field);
                }

                // Build and execute INSERT
                std::string insert_sql = insert_base;
                for (size_t i = 0; i < values.size(); ++i) {
                    if (i > 0) insert_sql += ", ";
                    // Quote strings, detect NULLs
                    if (values[i].empty()) {
                        insert_sql += "NULL";
                    } else {
                        // Escape single quotes
                        std::string escaped;
                        for (char ch : values[i]) {
                            if (ch == '\'') escaped += "''";
                            else escaped += ch;
                        }
                        insert_sql += "'" + escaped + "'";
                    }
                }
                insert_sql += ")";

                core::ErrorContext insert_ctx;
                core::Status insert_status = g_connection->execute(insert_sql, nullptr, &insert_ctx);
                if (insert_status == core::Status::OK) {
                    row_count++;
                } else {
                    error_count++;
                    if (g_config.verbose) {
                        std::cerr << "Error inserting row: " << insert_ctx.message << "\n";
                    }
                }
            }

            infile.close();
            std::cout << "COPY " << row_count << " rows from '" << filename << "'";
            if (error_count > 0) {
                std::cout << " (" << error_count << " errors)";
            }
            std::cout << "\n";
        }
        return true;
    }

    if (command == "set") {
        auto& out = getOutput();
        out << "Connection:\n";
        out << "  database = " << g_config.database_path << "\n";
        out << "  user = " << g_config.username << "\n";
        out << "  port = " << g_config.port << "\n";
        out << "\nDisplay settings:\n";
        out << "  timing = " << (g_config.timing ? "on" : "off") << "\n";
        out << "  expanded = " << (g_config.expanded ? "on" : "off") << "\n";
        out << "  format = " << g_config.format << "\n";
        out << "  tuples_only = " << (g_config.tuples_only ? "on" : "off") << "\n";
        out << "\nFirebird ISQL settings:\n";
        out << "  sql_dialect = " << g_config.sql_dialect << "\n";
        out << "  bail = " << (g_config.bail ? "on" : "off") << "\n";
        out << "  on_error = " << (g_config.on_error == IsqlConfig::ErrorAction::CONTINUE ? "CONTINUE" :
                                   (g_config.on_error == IsqlConfig::ErrorAction::STOP ? "STOP" :
                                   ("EXIT " + std::to_string(g_config.exit_code)))) << "\n";
        out << "  term = '" << g_config.term << "'\n";
        out << "  count = " << (g_config.count ? "on" : "off") << "\n";
        out << "  heading = " << (g_config.heading ? "on" : "off") << "\n";
        out << "  echo = " << (g_config.echo ? "on" : "off") << "\n";
        out << "  list = " << (g_config.list ? "on" : "off") << "\n";
        out << "  null = '" << g_config.null_display << "'\n";
        out << "  stats = " << (g_config.stats ? "on" : "off") << "\n";
        out << "  plan = " << (g_config.plan ? "on" : "off") << "\n";
        out << "  planonly = " << (g_config.plan_only ? "on" : "off") << "\n";
        out << "  explain = " << (g_config.explain ? "on" : "off") << "\n";
        out << "  names = '" << g_config.names << "'\n";
        out << "  warnings = " << (g_config.warnings ? "on" : "off") << "\n";
        out << "  autoddl = " << (g_config.autoddl ? "on" : "off") << "\n";
        out << "  maxrows = " << g_config.maxrows << (g_config.maxrows == 0 ? " (unlimited)" : "") << "\n";
        out << "  local_timeout = " << g_config.local_timeout << " seconds" << (g_config.local_timeout == 0 ? " (unlimited)" : "") << "\n";
        out << "  time = " << (g_config.show_time ? "on" : "off") << "\n";
        if (!g_config.prompt.empty()) {
            out << "  prompt = '" << g_config.prompt << "'\n";
        }
        if (!g_config.column_widths.empty()) {
            out << "  column widths:\n";
            for (const auto& p : g_config.column_widths) {
                out << "    " << p.first << " = " << p.second << "\n";
            }
        }
        if (!g_config.variables.empty()) {
            out << "\nUser variables:\n";
            for (const auto& [var, val] : g_config.variables) {
                out << "  " << var << " = '" << val << "'\n";
            }
        }
        if (!g_config.last_query.empty()) {
            std::string preview = g_config.last_query;
            if (preview.size() > 60) preview = preview.substr(0, 57) + "...";
            out << "\nLast query (\\watch): " << preview << "\n";
        }
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
    // Load command history
    loadHistory();

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
        if (!g_config.prompt.empty()) {
            // Custom prompt with token substitution
            prompt = g_config.prompt;
            // Replace %d with database name
            size_t pos;
            while ((pos = prompt.find("%d")) != std::string::npos) {
                prompt.replace(pos, 2, g_config.database_path);
            }
            // Replace %u with username
            while ((pos = prompt.find("%u")) != std::string::npos) {
                prompt.replace(pos, 2, g_config.username.empty() ? "user" : g_config.username);
            }
            // Replace %t with time
            if ((pos = prompt.find("%t")) != std::string::npos) {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                struct tm tm_buf;
                localtime_r(&time, &tm_buf);
                char time_str[16];
                strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);
                while ((pos = prompt.find("%t")) != std::string::npos) {
                    prompt.replace(pos, 2, time_str);
                }
            }
            if (in_multiline && prompt.find("=>") != std::string::npos) {
                // Replace => with -# for multiline
                while ((pos = prompt.find("=>")) != std::string::npos) {
                    prompt.replace(pos, 2, "-#");
                }
            }
        } else if (g_config.show_time) {
            // Time prefix
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            struct tm tm_buf;
            localtime_r(&time, &tm_buf);
            char time_str[16];
            strftime(time_str, sizeof(time_str), "%H:%M:%S", &tm_buf);
            if (in_multiline) {
                prompt = std::string("[") + time_str + "] " + g_config.database_path + "-# ";
            } else {
                prompt = std::string("[") + time_str + "] " + g_config.database_path + "=> ";
            }
        } else {
            // Default prompt
            if (in_multiline) {
                prompt = g_config.database_path + "-# ";
            } else {
                prompt = g_config.database_path + "=> ";
            }
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
            addToHistory(line);
            handleMetaCommand(line);
            continue;
        }

        // Append to buffer
        if (!sql_buffer.empty()) {
            sql_buffer += " ";
        }
        sql_buffer += line;

        // Check for statement terminator (supports custom terminator via SET TERM)
        // This is a simplified version - a full parser would track quotes properly
        bool ends_with_term = false;
        const std::string& term = g_config.term;

        // Check if line ends with terminator (outside of strings)
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
                }
            }
        }

        // After string tracking, check if buffer ends with terminator
        if (!in_string && sql_buffer.size() >= term.size()) {
            if (sql_buffer.substr(sql_buffer.size() - term.size()) == term) {
                ends_with_term = true;
            }
        }

        if (ends_with_term) {
            // Remove terminator from SQL before executing
            std::string sql_to_exec = sql_buffer.substr(0, sql_buffer.size() - term.size());
            // Trim trailing whitespace
            while (!sql_to_exec.empty() && (sql_to_exec.back() == ' ' || sql_to_exec.back() == '\t')) {
                sql_to_exec.pop_back();
            }

            addToHistory(sql_buffer);
            bool success = executeSQL(sql_to_exec);

            // Handle error based on bail or on_error setting
            if (!success) {
                if (g_config.bail || g_config.on_error == IsqlConfig::ErrorAction::STOP) {
                    std::cerr << "Stopping due to error (bail mode)\n";
                    g_running = false;
                } else if (g_config.on_error == IsqlConfig::ErrorAction::EXIT) {
                    std::cerr << "Exiting with code " << g_config.exit_code << " due to error\n";
                    g_running = false;
                }
                // ErrorAction::CONTINUE - just continue to next statement
            }

            sql_buffer.clear();
            in_multiline = false;
        } else {
            in_multiline = true;
        }
    }

    // Save command history
    saveHistory();
}

// =============================================================================
// DDL Extraction (Firebird isql compatible)
// =============================================================================

/**
 * Extract DDL from the database
 *
 * Modes:
 * - EXTRACT_ALL (-a): All objects
 * - EXTRACT_X (-x): All objects without data
 * - EXTRACT_EX (-ex): All objects with CREATE DATABASE header
 *
 * Returns true on success, false on error
 */
bool extractDDL(bool include_create_database) {
    std::ostream& out = g_output_file ? *g_output_file : std::cout;

    // Header
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    out << "/*\n";
    out << " * ScratchBird DDL Extract\n";
    out << " * Database: " << g_config.database_path << "\n";
    out << " * Generated: " << std::ctime(&time);
    out << " */\n\n";

    // SET TERM for stored procedures (Firebird compatible)
    out << "SET TERM ^;\n\n";

    // CREATE DATABASE statement (only for -ex mode)
    if (include_create_database) {
        out << "/* CREATE DATABASE statement */\n";
        out << "CREATE DATABASE '" << g_config.database_path << "';\n\n";
    }

    // =========================================================================
    // Extract Domains/Types
    // =========================================================================
    out << "/* ==================== DOMAINS ==================== */\n\n";

    core::ErrorContext ctx;
    ResultSet results;
    std::string sql = "SELECT name FROM sb_catalog.sb_domains ORDER BY name";
    if (g_connection) {
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string domain_name = results.getString("name");
                // Get domain definition via SHOW CREATE DOMAIN (if available)
                // For now, output a placeholder
                out << "/* Domain: " << domain_name << " */\n";
            }
            out << "\n";
        }
    }

    // =========================================================================
    // Extract Sequences/Generators
    // =========================================================================
    out << "/* ==================== SEQUENCES ==================== */\n\n";

    sql = "SELECT name, current_value, increment FROM sb_catalog.sb_sequences ORDER BY name";
    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string seq_name = results.getString("name");
                int64_t start_value = results.getInt64("current_value");
                int64_t inc = results.getInt64("increment");
                if (inc == 0) inc = 1;

                out << "CREATE SEQUENCE " << seq_name;
                if (start_value != 1) {
                    out << " START WITH " << start_value;
                }
                if (inc != 1) {
                    out << " INCREMENT BY " << inc;
                }
                out << ";\n";
            }
            out << "\n";
        }
    }

    // =========================================================================
    // Extract Tables
    // =========================================================================
    out << "/* ==================== TABLES ==================== */\n\n";

    sql = "SELECT name FROM sb_catalog.sb_tables WHERE name NOT LIKE 'sb_%' ORDER BY name";
    std::vector<std::string> table_names;

    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                table_names.push_back(results.getString("name"));
            }
        }
    }

    // For each table, get CREATE TABLE statement via SHOW CREATE TABLE
    for (const auto& table_name : table_names) {
        sql = "SHOW CREATE TABLE " + table_name;
        if (g_connection) {
            results = ResultSet();
            core::Status status = g_connection->executeQuery(sql, &results, &ctx);
            if (status == core::Status::OK && results.next()) {
                std::string create_stmt = results.getString(0);
                out << create_stmt << ";\n\n";
            } else {
                // Fallback: output placeholder
                out << "/* Unable to extract DDL for table: " << table_name << " */\n\n";
            }
        }
    }

    // =========================================================================
    // Extract Views
    // =========================================================================
    out << "/* ==================== VIEWS ==================== */\n\n";

    sql = "SELECT name, definition FROM sb_catalog.sb_views WHERE name NOT LIKE 'sb_%' ORDER BY name";
    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string view_name = results.getString("name");
                std::string definition = results.getString("definition");

                if (!definition.empty()) {
                    out << "CREATE VIEW " << view_name << " AS\n" << definition << ";\n\n";
                } else {
                    out << "/* View: " << view_name << " (definition not available) */\n\n";
                }
            }
        }
    }

    // =========================================================================
    // Extract Indexes (non-primary, non-unique constraints)
    // =========================================================================
    out << "/* ==================== INDEXES ==================== */\n\n";

    sql = "SELECT i.name, i.table_name, i.is_unique, i.columns "
          "FROM sb_catalog.sb_indexes i "
          "WHERE i.name NOT LIKE 'sb_%' AND i.is_primary = FALSE "
          "ORDER BY i.table_name, i.name";
    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string idx_name = results.getString("name");
                std::string tbl_name = results.getString("table_name");
                bool is_unique = results.getInt64("is_unique") != 0;
                std::string columns = results.getString("columns");

                out << "CREATE ";
                if (is_unique) out << "UNIQUE ";
                out << "INDEX " << idx_name << " ON " << tbl_name;
                if (!columns.empty()) {
                    out << " (" << columns << ")";
                }
                out << ";\n";
            }
            out << "\n";
        }
    }

    // =========================================================================
    // Extract Triggers
    // =========================================================================
    out << "/* ==================== TRIGGERS ==================== */\n\n";

    sql = "SELECT name, table_name, timing, event, definition "
          "FROM sb_catalog.sb_triggers "
          "WHERE name NOT LIKE 'sb_%' "
          "ORDER BY table_name, name";
    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string trig_name = results.getString("name");
                std::string tbl_name = results.getString("table_name");
                std::string timing = results.getString("timing");
                std::string event = results.getString("event");
                std::string definition = results.getString("definition");
                if (timing.empty()) timing = "BEFORE";
                if (event.empty()) event = "INSERT";

                out << "CREATE TRIGGER " << trig_name << "\n";
                out << timing << " " << event << " ON " << tbl_name << "\n";
                if (!definition.empty()) {
                    out << definition;
                } else {
                    out << "AS\nBEGIN\n  /* Trigger body */\nEND";
                }
                out << "^\n\n";
            }
        }
    }

    // =========================================================================
    // Extract Stored Procedures
    // =========================================================================
    out << "/* ==================== PROCEDURES ==================== */\n\n";

    sql = "SELECT name, definition FROM sb_catalog.sb_procedures WHERE name NOT LIKE 'sb_%' ORDER BY name";
    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string proc_name = results.getString("name");
                std::string definition = results.getString("definition");

                if (!definition.empty()) {
                    out << definition << "^\n\n";
                } else {
                    out << "/* Procedure: " << proc_name << " (definition not available) */\n\n";
                }
            }
        }
    }

    // =========================================================================
    // Extract Functions
    // =========================================================================
    out << "/* ==================== FUNCTIONS ==================== */\n\n";

    sql = "SELECT name, definition FROM sb_catalog.sb_functions WHERE name NOT LIKE 'sb_%' ORDER BY name";
    if (g_connection) {
        results = ResultSet();
        core::Status status = g_connection->executeQuery(sql, &results, &ctx);
        if (status == core::Status::OK) {
            while (results.next()) {
                std::string func_name = results.getString("name");
                std::string definition = results.getString("definition");

                if (!definition.empty()) {
                    out << definition << "^\n\n";
                } else {
                    out << "/* Function: " << func_name << " (definition not available) */\n\n";
                }
            }
        }
    }

    // Reset terminator
    out << "SET TERM ;^\n\n";

    // Footer
    out << "/* End of DDL Extract */\n";

    return true;
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
    std::cout << "  -p, --port=<n>            TCP port (default: 3092)\n";
    std::cout << "  -H, --host=<host>         Host (default: localhost)\n";
    std::cout << "  -c, --command=<sql>       Execute single command and exit\n";
    std::cout << "  -f, --file=<file>         Execute commands from file and exit\n";
    std::cout << "  -i, --input=<file>        Alias for -f (Firebird compatible)\n";
    std::cout << "  -o, --output=<file>       Write output to file\n";
    std::cout << "  -t, --tuples-only         Print tuples only (no headers/footers)\n";
    std::cout << "  -A, --no-align            Unaligned output mode\n";
    std::cout << "  -F, --field-separator=<s> Field separator (default: |)\n";
    std::cout << "  -q, --quiet               Quiet mode (no welcome message)\n";
    std::cout << "  -e, --echo                Echo commands before execution\n";
    std::cout << "  -b, --bail                Stop on first error (Firebird compatible)\n";
    std::cout << "  -v, --verbose             Verbose mode\n";
    std::cout << "      --schema-tree         Print schema tree and exit\n";
    std::cout << "  -a, --extract-all         Extract DDL for all objects (Firebird compatible)\n";
    std::cout << "  -x, --extract             Extract DDL (no data)\n";
    std::cout << "  -ex, --extract-db         Extract DDL with CREATE DATABASE\n";
    std::cout << "  -s, --dialect=<n>         SQL dialect (1, 2, or 3, default: 3)\n";
    std::cout << "  -par, --parser=<name>     Parser (scratchbird, firebird, postgresql, mysql)\n";
    std::cout << "  -h, --help                Show this help\n";
    std::cout << "      --version             Show version\n\n";
    std::cout << "SET Commands (Firebird ISQL compatible):\n";
    std::cout << "  SET SQL DIALECT N         Set SQL dialect (1, 2, or 3)\n";
    std::cout << "  SET BAIL [ON|OFF]         Stop on first error\n";
    std::cout << "  SET TERM <char>           Change statement terminator\n";
    std::cout << "  SET COUNT [ON|OFF]        Display row counts\n";
    std::cout << "  SET HEADING [ON|OFF]      Show column headings\n";
    std::cout << "  SET ECHO [ON|OFF]         Echo commands before execution\n";
    std::cout << "  SET LIST [ON|OFF]         Vertical display mode\n";
    std::cout << "  SET NULL <string>         String to display for NULL values\n";
    std::cout << "  SET WIDTH <col> <n>       Set column display width\n";
    std::cout << "  SET STATS [ON|OFF]        Show timing statistics\n";
    std::cout << "  SET PLAN [ON|OFF]         Show query execution plan\n";
    std::cout << "  SET PLANONLY [ON|OFF]     Show plan only, don't execute\n";
    std::cout << "  SET EXPLAIN [ON|OFF]      Show detailed plan analysis\n";
    std::cout << "  SET NAMES <charset>       Set client character set\n";
    std::cout << "  SET WARNINGS [ON|OFF]     Display warnings\n";
    std::cout << "  SET AUTODDL [ON|OFF]      Auto-commit DDL statements\n";
    std::cout << "  SET MAXROWS N             Limit rows returned (0 = unlimited)\n";
    std::cout << "  SET LOCAL_TIMEOUT N       Statement timeout in seconds\n";
    std::cout << "  SET TIME [ON|OFF]         Show time in prompt\n";
    std::cout << "  SET PROMPT <string>       Custom prompt (%d=db, %u=user, %t=time)\n";
    std::cout << "  SET DEFINE var=value      Define a variable\n";
    std::cout << "  SET UNDEFINE var          Remove a defined variable\n\n";
    std::cout << "SHOW Commands:\n";
    std::cout << "  SHOW SQL DIALECT          Show current SQL dialect\n\n";
    std::cout << "Transaction Commands:\n";
    std::cout << "  SET TRANSACTION           Start explicit transaction\n";
    std::cout << "  COMMIT [WORK] [RETAIN]    Commit (RETAIN keeps transaction open)\n";
    std::cout << "  ROLLBACK [WORK] [RETAIN]  Rollback transaction\n";
    std::cout << "  ROLLBACK TO SAVEPOINT name  Rollback to savepoint\n";
    std::cout << "  SAVEPOINT name            Create savepoint\n";
    std::cout << "  RELEASE SAVEPOINT name    Release savepoint\n\n";
    std::cout << "Error Handling Commands:\n";
    std::cout << "  WHENEVER ERROR CONTINUE   Continue on error (default)\n";
    std::cout << "  WHENEVER ERROR STOP       Stop processing on error\n";
    std::cout << "  WHENEVER ERROR EXIT [N]   Exit with code N on error\n";
    std::cout << "  EXIT [N]                  Exit with optional code\n";
    std::cout << "  QUIT                      Quit immediately\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " /path/to/mydb.sbdb\n";
    std::cout << "  " << program << " mydb.sbdb -U admin -c \"SELECT * FROM users\"\n";
    std::cout << "  " << program << " mydb.sbdb -f queries.sql -o results.txt\n";
    std::cout << "  " << program << " mydb.sbdb -b -f script.sql   # Stop on error\n";
    std::cout << "  " << program << " mydb.sbdb -x -o schema.sql   # Extract DDL to file\n";
    std::cout << "  " << program << " mydb.sbdb -ex > backup.sql   # Extract with CREATE DATABASE\n";
}

void printVersion() {
    std::cout << "sb_isql (ScratchBird Interactive SQL) 0.1.0\n";
}

std::string normalizeParserName(const std::string& input) {
    std::string upper = input;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    if (upper == "SCRATCHBIRD" || upper == "V2" || upper == "AUTO") {
        return "SCRATCHBIRD";
    }
    if (upper == "FIREBIRD" || upper == "FIREBIRDSQL" || upper == "FB") {
        return "FIREBIRD";
    }
    if (upper == "POSTGRESQL" || upper == "POSTGRES" || upper == "PG") {
        return "POSTGRESQL";
    }
    if (upper == "MYSQL") {
        return "MYSQL";
    }

    return {};
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
        } else if (arg == "-i" && i + 1 < argc) {
            g_config.input_file = argv[++i];
        } else if (arg.find("--input=") == 0) {
            g_config.input_file = arg.substr(8);
        } else if (arg == "-f" && i + 1 < argc) {
            g_config.input_file = argv[++i];
        } else if (arg.find("--file=") == 0) {
            g_config.input_file = arg.substr(7);
        } else if (arg == "-par" && i + 1 < argc) {
            g_config.parser_name = argv[++i];
        } else if (arg.find("--parser=") == 0) {
            g_config.parser_name = arg.substr(9);
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
        } else if (arg == "-ex" || arg == "--extract-db") {
            // Must be before -e to avoid conflict
            g_config.ddl_mode = IsqlConfig::DDLMode::EXTRACT_EX;
        } else if (arg == "-e" || arg == "--echo") {
            g_config.echo = true;
        } else if (arg == "-b" || arg == "--bail") {
            g_config.bail = true;
        } else if (arg == "-v" || arg == "--verbose") {
            g_config.verbose = true;
        } else if (arg == "--schema-tree") {
            g_config.schema_tree = true;
        } else if (arg == "-a" || arg == "--extract-all") {
            g_config.ddl_mode = IsqlConfig::DDLMode::EXTRACT_ALL;
        } else if (arg == "-x" || arg == "--extract") {
            g_config.ddl_mode = IsqlConfig::DDLMode::EXTRACT_X;
        } else if (arg == "-s" && i + 1 < argc) {
            int dialect = std::stoi(argv[++i]);
            if (dialect < 1 || dialect > 3) {
                std::cerr << "Error: SQL dialect must be 1, 2, or 3\n";
                return false;
            }
            g_config.sql_dialect = dialect;
        } else if (arg.find("--dialect=") == 0) {
            int dialect = std::stoi(arg.substr(10));
            if (dialect < 1 || dialect > 3) {
                std::cerr << "Error: SQL dialect must be 1, 2, or 3\n";
                return false;
            }
            g_config.sql_dialect = dialect;
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
// Schema tree output
// =============================================================================

namespace {
struct SchemaTreeNode {
    std::map<std::string, std::unique_ptr<SchemaTreeNode>> children;
    std::vector<std::pair<std::string, std::string>> objects;  // (type, name)
};

std::vector<std::string> splitSchemaPath(const std::string& path) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : path) {
        if (ch == '.' || ch == '/') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    return parts;
}

std::string formatObjectLabel(const std::string& type) {
    std::string label = type;
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    std::replace(label.begin(), label.end(), '_', ' ');
    return label;
}

void ensureSchemaPath(SchemaTreeNode& root, const std::string& path) {
    SchemaTreeNode* node = &root;
    for (const auto& part : splitSchemaPath(path)) {
        if (part.empty()) {
            continue;
        }
        auto& child = node->children[part];
        if (!child) {
            child = std::make_unique<SchemaTreeNode>();
        }
        node = child.get();
    }
}

void addObjectToTree(SchemaTreeNode& root,
                     const std::string& object_type,
                     const std::string& schema_path,
                     const std::string& object_name) {
    if (object_type == "SCHEMA") {
        std::string full_path = schema_path;
        if (!object_name.empty()) {
            if (!full_path.empty()) {
                full_path.push_back('.');
            }
            full_path += object_name;
        }
        ensureSchemaPath(root, full_path);
        return;
    }
    if (object_type == "COLUMN") {
        return;
    }

    SchemaTreeNode* node = &root;
    for (const auto& part : splitSchemaPath(schema_path)) {
        if (part.empty()) {
            continue;
        }
        auto& child = node->children[part];
        if (!child) {
            child = std::make_unique<SchemaTreeNode>();
        }
        node = child.get();
    }

    node->objects.emplace_back(formatObjectLabel(object_type), object_name);
}

void printSchemaTreeNode(const SchemaTreeNode& node, size_t indent) {
    auto& out = getOutput();
    std::string padding(indent, ' ');

    for (const auto& entry : node.children) {
        out << padding << entry.first << "\n";
        printSchemaTreeNode(*entry.second, indent + 4);
    }

    if (!node.objects.empty()) {
        std::vector<std::pair<std::string, std::string>> objects = node.objects;
        std::sort(objects.begin(), objects.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });
        for (const auto& obj : objects) {
            out << padding << "(" << obj.first << ") " << obj.second << "\n";
        }
    }
}
}  // namespace

static bool outputSchemaTree() {
    if (!g_connection || !g_connection->isConnected()) {
        std::cerr << "Error: Not connected to database\n";
        return false;
    }

    ResultSet results;
    core::ErrorContext ctx;
    core::Status status = g_connection->executeQuery(
        "SELECT object_type, schema_path, object_name FROM sys.catalog.object_resolver",
        &results, &ctx);
    if (status != core::Status::OK) {
        std::cerr << "Error: " << ctx.message << "\n";
        return false;
    }

    SchemaTreeNode root;
    while (results.next()) {
        std::string object_type = results.isNull(0) ? "" : results.getString(0);
        std::string schema_path = results.isNull(1) ? "" : results.getString(1);
        std::string object_name = results.isNull(2) ? "" : results.getString(2);

        addObjectToTree(root, object_type, schema_path, object_name);
    }

    auto& out = getOutput();
    out << "(root/database)\n";
    printSchemaTreeNode(root, 4);
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

    if (!g_config.parser_name.empty()) {
        std::string parser_name = normalizeParserName(g_config.parser_name);
        if (parser_name.empty()) {
            std::cerr << "Error: Unknown parser '" << g_config.parser_name << "'\n";
            return 1;
        }
        std::string set_parser = "SET PARSER TO '" + parser_name + "'";
        if (!executeSQL(set_parser)) {
            return 1;
        }
    }

    int result = 0;

    if (g_config.schema_tree) {
        if (!outputSchemaTree()) {
            result = 1;
        }
    }
    // DDL Extraction mode (-a, -x, -ex)
    else if (g_config.ddl_mode != IsqlConfig::DDLMode::NONE) {
        bool include_create_db = (g_config.ddl_mode == IsqlConfig::DDLMode::EXTRACT_EX);
        if (!extractDDL(include_create_db)) {
            result = 1;
        }
    }
    // Execute single command if given
    else if (!g_config.command.empty()) {
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

    if (g_error_file) {
        // Restore original cerr buffer before closing
        if (g_original_cerr) {
            std::cerr.rdbuf(g_original_cerr);
        }
        g_error_file->close();
        delete g_error_file;
    }

    return result;
}
