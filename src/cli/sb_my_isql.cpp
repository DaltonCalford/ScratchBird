/**
 * sb_my_isql - ScratchBird MySQL Interactive SQL Shell
 *
 * Minimal mysql-compatible client for MySQL wire protocol testing.
 */

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "scratchbird/fdw/mysql_adapter.h"
#include "scratchbird/fdw/fdw_types.h"

#include "isql_common.h"

using scratchbird::cli::OutputConfig;
using scratchbird::cli::printResultSet;
using scratchbird::cli::promptHidden;
using scratchbird::cli::splitStatements;
using scratchbird::cli::trim;

namespace {

struct MyIsqlConfig {
    std::string host = "localhost";
    uint16_t port = 3306;
    std::string user = "root";
    std::string password;
    std::string database;
    std::string input_file;
    std::string command;
    std::string output_file;
    bool quiet = false;
    bool prompt_password = false;
    OutputConfig output{};
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] [database]\n"
              << "\n"
              << "Options:\n"
              << "  -h, --host <host>        Server host (default: localhost)\n"
              << "  -P, --port <port>        Server port (default: 3306)\n"
              << "  -u, --user <user>        Username (default: root)\n"
              << "  -p, --password[=pass]    Password (prompt if not provided)\n"
              << "  -D, --database <db>      Database name\n"
              << "  -f, --file <file>        Execute commands from file\n"
              << "  -e, --execute <sql>      Execute a single command\n"
              << "  -o, --output <file>      Write output to file\n"
              << "  -t, --tuples-only        Print rows only (no headers)\n"
              << "  -A, --no-align           Unaligned output mode\n"
              << "  -F, --field-separator <s> Field separator for unaligned output\n"
              << "  -q, --silent             Quiet mode\n"
              << "  --help                   Show this help\n";
}

bool parseArgs(int argc, char* argv[], MyIsqlConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            printUsage(argv[0]);
            return false;
        }

        if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
            continue;
        }
        if ((arg == "-P" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            continue;
        }
        if ((arg == "-u" || arg == "--user") && i + 1 < argc) {
            config.user = argv[++i];
            continue;
        }
        if ((arg == "-D" || arg == "--database") && i + 1 < argc) {
            config.database = argv[++i];
            continue;
        }
        if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            config.input_file = argv[++i];
            continue;
        }
        if ((arg == "-e" || arg == "--execute") && i + 1 < argc) {
            config.command = argv[++i];
            continue;
        }
        if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            config.output_file = argv[++i];
            continue;
        }
        if (arg == "-t" || arg == "--tuples-only") {
            config.output.tuples_only = true;
            config.output.show_row_count = false;
            continue;
        }
        if (arg == "-A" || arg == "--no-align") {
            config.output.no_align = true;
            continue;
        }
        if ((arg == "-F" || arg == "--field-separator") && i + 1 < argc) {
            config.output.field_separator = argv[++i];
            continue;
        }
        if (arg == "-q" || arg == "--silent") {
            config.quiet = true;
            continue;
        }

        if (arg.rfind("--password=", 0) == 0) {
            config.password = arg.substr(std::strlen("--password="));
            continue;
        }

        if (arg.rfind("-p", 0) == 0) {
            if (arg.size() > 2) {
                config.password = arg.substr(2);
            } else {
                config.prompt_password = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    config.password = argv[++i];
                    config.prompt_password = false;
                }
            }
            continue;
        }

        if (!arg.empty() && arg[0] != '-') {
            if (config.database.empty()) {
                config.database = arg;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }

    return true;
}

bool executeStatement(scratchbird::fdw::MySQLAdapter& adapter,
                      const std::string& sql,
                      std::ostream& out,
                      const OutputConfig& output) {
    auto result = adapter.executeQuery(sql);
    if (!result) {
        std::cerr << "Error: " << result.errorMessage() << "\n";
        return false;
    }

    if (!result->success) {
        std::cerr << "Error: " << result->error_message;
        if (!result->sql_state.empty()) {
            std::cerr << " (SQLSTATE " << result->sql_state << ")";
        }
        std::cerr << "\n";
        return false;
    }

    if (!result->columns.empty()) {
        printResultSet(out, *result, output);
    } else {
        out << "Query OK";
        if (result->rows_affected > 0) {
            out << ", " << result->rows_affected << " rows affected";
        }
        out << "\n";
    }

    return true;
}

bool runInput(scratchbird::fdw::MySQLAdapter& adapter,
              const std::string& input,
              std::ostream& out,
              const OutputConfig& output) {
    std::string remainder;
    auto statements = splitStatements(input, &remainder, true);
    std::string tail = trim(remainder);
    if (!tail.empty()) {
        statements.push_back(tail);
    }

    for (const auto& stmt : statements) {
        if (!executeStatement(adapter, stmt, out, output)) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    MyIsqlConfig config;
    if (!parseArgs(argc, argv, config)) {
        return 1;
    }

    if (config.database.empty()) {
        config.database = "default";
    }
    if (config.prompt_password && config.password.empty()) {
        config.password = promptHidden("Enter password: ");
    }

    scratchbird::fdw::ServerDefinition server;
    server.db_type = scratchbird::fdw::RemoteDatabaseType::MYSQL;
    server.host = config.host;
    server.port = config.port;
    server.database = config.database;

    scratchbird::fdw::UserMapping mapping;
    mapping.remote_user = config.user;
    mapping.remote_password = config.password;

    scratchbird::fdw::MySQLAdapter adapter;
    auto connect_result = adapter.connect(server, mapping);
    if (!connect_result) {
        std::cerr << "Connection failed: " << connect_result.errorMessage() << "\n";
        return 1;
    }

    std::ofstream output_file;
    std::ostream* out = &std::cout;
    if (!config.output_file.empty()) {
        output_file.open(config.output_file);
        if (!output_file) {
            std::cerr << "Error: Cannot open output file: " << config.output_file << "\n";
            return 1;
        }
        out = &output_file;
    }

    if (!config.quiet) {
        *out << "sb_my_isql - ScratchBird MySQL SQL Shell\n";
    }

    if (!config.command.empty()) {
        if (!runInput(adapter, config.command, *out, config.output)) {
            return 1;
        }
        return 0;
    }

    if (!config.input_file.empty()) {
        std::ifstream file(config.input_file);
        if (!file) {
            std::cerr << "Error: Cannot open file: " << config.input_file << "\n";
            return 1;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        if (!runInput(adapter, buffer.str(), *out, config.output)) {
            return 1;
        }
        return 0;
    }

    std::ostringstream buffer;
    buffer << std::cin.rdbuf();
    std::string input = buffer.str();
    if (input.empty()) {
        return 0;
    }
    if (!runInput(adapter, input, *out, config.output)) {
        return 1;
    }

    return 0;
}
