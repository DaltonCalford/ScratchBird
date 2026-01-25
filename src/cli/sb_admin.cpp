/**
 * sb_admin - ScratchBird Administration Tool
 *
 * CLI Tools - Scheduler administration and metrics access.
 *
 * Usage:
 *   sb_admin <database> job list [--like <pattern>]
 *   sb_admin <database> job runs <job_name>
 *   sb_admin <database> metrics
 *
 * Options:
 *   -U, --user=<username>    Admin username
 *   -P, --password=<pass>    Admin password
 *   -p, --port=<n>           TCP port (default: 3092)
 *   --database=<name>        Database name (if not supplied positionally)
 *   -q, --quiet              Only show errors
 *   -h, --help               Show this help
 *   --version                Show version
 */

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <termios.h>
#include <unistd.h>

#include "scratchbird/client/connection.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird;
using namespace scratchbird::client;

enum class AdminCommand {
    NONE,
    JOB_LIST,
    JOB_RUNS,
    METRICS
};

struct AdminConfig {
    AdminCommand command = AdminCommand::NONE;
    std::string database_path;
    std::string admin_user;
    std::string admin_password;
    uint16_t port = 3092;
    bool quiet = false;
    std::string job_name;
    std::string like_pattern;
};

static AdminConfig g_config;
static Connection* g_connection = nullptr;

void printUsage(const char* program) {
    std::cout << "sb_admin - ScratchBird Administration Tool\n\n"
              << "Usage:\n"
              << "  " << program << " <database> job list [--like <pattern>]\n"
              << "  " << program << " <database> job runs <job_name>\n"
              << "  " << program << " <database> metrics\n\n"
              << "Options:\n"
              << "  -U, --user=<username>    Admin username\n"
              << "  -P, --password=<pass>    Admin password\n"
              << "  -p, --port=<n>           TCP port (default: 3092)\n"
              << "  --database=<name>        Database name (if not supplied positionally)\n"
              << "  -q, --quiet              Only show errors\n"
              << "  -h, --help               Show this help\n"
              << "  --version                Show version\n";
}

void printVersion() {
    std::cout << "sb_admin (ScratchBird)\n";
}

void log(const std::string& msg) {
    if (!g_config.quiet) {
        std::cout << msg << "\n";
    }
}

void printError(const std::string& msg) {
    std::cerr << "Error: " << msg << "\n";
}

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

bool connectToDatabase() {
    g_connection = new Connection();

    ConnectionConfig config;
    config.database_name = g_config.database_path;
    config.username = g_config.admin_user;
    config.password = g_config.admin_password;
    config.tcp_port = g_config.port;
    config.ipc_method = server::IPCMethod::TCP_LOCALHOST;

    core::ErrorContext ctx;
    core::Status status = g_connection->connect(config, &ctx);
    if (status != core::Status::OK) {
        printError("Connection failed: " + ctx.message);
        delete g_connection;
        g_connection = nullptr;
        return false;
    }
    return true;
}

void disconnectFromDatabase() {
    if (g_connection) {
        g_connection->disconnect();
        delete g_connection;
        g_connection = nullptr;
    }
}

std::string escapeSqlLiteral(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char ch : value) {
        if (ch == '\'') {
            out.push_back('\'');
        }
        out.push_back(ch);
    }
    return out;
}

bool executeSQL(const std::string& sql, ResultSet* results = nullptr) {
    core::ErrorContext ctx;

    if (results) {
        core::Status status = g_connection->executeQuery(sql, results, &ctx);
        if (status != core::Status::OK) {
            printError(ctx.message);
            return false;
        }
    } else {
        int64_t affected;
        core::Status status = g_connection->execute(sql, &affected, &ctx);
        if (status != core::Status::OK) {
            printError(ctx.message);
            return false;
        }
    }
    return true;
}

void printResultSet(ResultSet& rs, bool include_header) {
    size_t cols = rs.getColumnCount();
    if (include_header) {
        for (size_t i = 0; i < cols; ++i) {
            if (i > 0) std::cout << "\t";
            std::cout << rs.getColumnName(i);
        }
        std::cout << "\n";
    }

    while (rs.next()) {
        for (size_t i = 0; i < cols; ++i) {
            if (i > 0) std::cout << "\t";
            if (rs.isNull(i)) {
                std::cout << "NULL";
            } else {
                std::cout << rs.getString(i);
            }
        }
        std::cout << "\n";
    }
}

bool jobList() {
    std::string sql = "SHOW JOBS";
    if (!g_config.like_pattern.empty()) {
        sql += " LIKE '" + escapeSqlLiteral(g_config.like_pattern) + "'";
    }

    ResultSet rs;
    if (!executeSQL(sql, &rs)) {
        return false;
    }
    printResultSet(rs, true);
    return true;
}

bool jobRuns() {
    if (g_config.job_name.empty()) {
        printError("Job name is required for job runs");
        return false;
    }
    std::string sql = "SHOW JOB RUNS FOR '" + escapeSqlLiteral(g_config.job_name) + "'";
    ResultSet rs;
    if (!executeSQL(sql, &rs)) {
        return false;
    }
    printResultSet(rs, true);
    return true;
}

bool metrics() {
    ResultSet rs;
    if (!executeSQL("SHOW METRICS", &rs)) {
        return false;
    }
    printResultSet(rs, false);
    return true;
}

bool parseArgs(int argc, char* argv[]) {
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            exit(0);
        } else if (arg == "--version") {
            printVersion();
            exit(0);
        } else if ((arg == "-U" || arg == "--user") && i + 1 < argc) {
            g_config.admin_user = argv[++i];
        } else if (arg.rfind("--user=", 0) == 0) {
            g_config.admin_user = arg.substr(7);
        } else if ((arg == "-P" || arg == "--password") && i + 1 < argc) {
            g_config.admin_password = argv[++i];
        } else if (arg.rfind("--password=", 0) == 0) {
            g_config.admin_password = arg.substr(11);
        } else if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            g_config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg.rfind("--port=", 0) == 0) {
            g_config.port = static_cast<uint16_t>(std::stoi(arg.substr(7)));
        } else if (arg == "--database" && i + 1 < argc) {
            g_config.database_path = argv[++i];
        } else if (arg.rfind("--database=", 0) == 0) {
            g_config.database_path = arg.substr(11);
        } else if (arg == "-q" || arg == "--quiet") {
            g_config.quiet = true;
        } else if (arg == "--like" && i + 1 < argc) {
            g_config.like_pattern = argv[++i];
        } else if (arg.rfind("--like=", 0) == 0) {
            g_config.like_pattern = arg.substr(7);
        } else if (!arg.empty() && arg[0] == '-') {
            printError("Unknown option: " + arg);
            return false;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        printUsage(argv[0]);
        return false;
    }

    size_t idx = 0;
    if (positional[0] != "job" && positional[0] != "metrics") {
        if (g_config.database_path.empty()) {
            g_config.database_path = positional[0];
        }
        idx = 1;
    }

    if (idx >= positional.size()) {
        printUsage(argv[0]);
        return false;
    }

    std::string command = positional[idx++];
    if (command == "job") {
        if (idx >= positional.size()) {
            printError("Missing job subcommand (list or runs)");
            return false;
        }
        std::string sub = positional[idx++];
        if (sub == "list") {
            g_config.command = AdminCommand::JOB_LIST;
        } else if (sub == "runs") {
            g_config.command = AdminCommand::JOB_RUNS;
            if (idx < positional.size()) {
                g_config.job_name = positional[idx++];
            }
        } else {
            printError("Unknown job subcommand: " + sub);
            return false;
        }
    } else if (command == "metrics") {
        g_config.command = AdminCommand::METRICS;
    } else {
        printError("Unknown command: " + command);
        return false;
    }

    if (g_config.database_path.empty()) {
        printError("Database name is required");
        return false;
    }

    if (g_config.admin_user.empty()) {
        g_config.admin_user = "SYSARCH";
    }
    if (g_config.admin_password.empty()) {
        g_config.admin_password = readPassword("Password: ");
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (!parseArgs(argc, argv)) {
        return 1;
    }

    if (!connectToDatabase()) {
        return 2;
    }

    bool ok = false;
    switch (g_config.command) {
        case AdminCommand::JOB_LIST:
            ok = jobList();
            break;
        case AdminCommand::JOB_RUNS:
            ok = jobRuns();
            break;
        case AdminCommand::METRICS:
            ok = metrics();
            break;
        default:
            printError("No command specified");
            ok = false;
            break;
    }

    disconnectFromDatabase();
    return ok ? 0 : 1;
}
