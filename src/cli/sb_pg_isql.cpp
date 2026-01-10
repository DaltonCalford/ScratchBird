/**
 * sb_pg_isql - ScratchBird PostgreSQL Interactive SQL Shell
 *
 * Minimal psql-compatible client for PostgreSQL wire protocol testing.
 */

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "scratchbird/fdw/postgresql_adapter.h"
#include "scratchbird/fdw/fdw_types.h"

#include "isql_common.h"

using scratchbird::cli::OutputConfig;
using scratchbird::cli::printResultSet;
using scratchbird::cli::promptHidden;
using scratchbird::cli::splitStatements;
using scratchbird::cli::trim;

namespace {

struct PgIsqlConfig {
    std::string host = "localhost";
    uint16_t port = 5432;
    std::string user;
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
    std::cout << "Usage: " << program << " [OPTION]... [DBNAME [USERNAME]]\n"
              << "\n"
              << "Options:\n"
              << "  -h, --host <host>        Server host (default: localhost)\n"
              << "  -p, --port <port>        Server port (default: 5432)\n"
              << "  -U, --user <user>        Username\n"
              << "  -d, --dbname <db>        Database name\n"
              << "  -f, --file <file>        Execute commands from file\n"
              << "  -c, --command <sql>      Execute a single command\n"
              << "  -o, --output <file>      Write output to file\n"
              << "  -t, --tuples-only        Print rows only (no headers)\n"
              << "  -A, --no-align           Unaligned output mode\n"
              << "  -F, --field-separator <s> Field separator for unaligned output\n"
              << "  -q, --quiet              Quiet mode\n"
              << "  -W, --password           Force password prompt\n"
              << "  -?, --help               Show this help\n";
}

bool parseArgs(int argc, char* argv[], PgIsqlConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-?" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }

        if ((arg == "-h" || arg == "--host") && i + 1 < argc) {
            config.host = argv[++i];
            continue;
        }
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            continue;
        }
        if ((arg == "-U" || arg == "--user") && i + 1 < argc) {
            config.user = argv[++i];
            continue;
        }
        if ((arg == "-d" || arg == "--dbname") && i + 1 < argc) {
            config.database = argv[++i];
            continue;
        }
        if ((arg == "-f" || arg == "--file") && i + 1 < argc) {
            config.input_file = argv[++i];
            continue;
        }
        if ((arg == "-c" || arg == "--command") && i + 1 < argc) {
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
        if (arg == "-q" || arg == "--quiet") {
            config.quiet = true;
            continue;
        }
        if (arg == "-W" || arg == "--password") {
            config.prompt_password = true;
            continue;
        }

        if (!arg.empty() && arg[0] != '-') {
            if (config.database.empty()) {
                config.database = arg;
            } else if (config.user.empty()) {
                config.user = arg;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }

    return true;
}

bool executeStatement(scratchbird::fdw::PostgreSQLAdapter& adapter,
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
        out << "OK";
        if (result->rows_affected > 0) {
            out << " (" << result->rows_affected << " rows)";
        }
        out << "\n";
    }

    return true;
}

bool runInput(scratchbird::fdw::PostgreSQLAdapter& adapter,
              const std::string& input,
              std::ostream& out,
              const OutputConfig& output) {
    std::string remainder;
    auto statements = splitStatements(input, &remainder, false);
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
    PgIsqlConfig config;
    if (!parseArgs(argc, argv, config)) {
        return 1;
    }

    if (config.user.empty()) {
        const char* user_env = std::getenv("USER");
        config.user = user_env ? user_env : "postgres";
    }
    if (config.database.empty()) {
        config.database = "default";
    }
    if (config.password.empty()) {
        const char* env_pass = std::getenv("PGPASSWORD");
        if (env_pass) {
            config.password = env_pass;
        }
    }
    if (config.prompt_password && config.password.empty()) {
        config.password = promptHidden("Password: ");
    }

    scratchbird::fdw::ServerDefinition server;
    server.db_type = scratchbird::fdw::RemoteDatabaseType::POSTGRESQL;
    server.host = config.host;
    server.port = config.port;
    server.database = config.database;

    scratchbird::fdw::UserMapping mapping;
    mapping.remote_user = config.user;
    mapping.remote_password = config.password;

    scratchbird::fdw::PostgreSQLAdapter adapter;
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
        *out << "sb_pg_isql - ScratchBird PostgreSQL SQL Shell\n";
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
