#include "scratchbird/engine/executor.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct Options {
        std::string database_path;
        std::optional<std::string> command;
        std::optional<std::string> file;
        bool json{false};
        bool csv{false};
        bool timing{false};
        bool help{false};
        bool version{false};
    };

    void print_help()
    {
        std::cout << "isql - ScratchBird interactive SQL (script mode)\n"
                  << "Usage: isql [--database <path>] [-c <sql>] [-f <file.sql>] [--json|--csv] [--timing]\n"
                  << "       isql --help | --version\n";
    }

    void print_version()
    {
        std::cout << "isql 0.1 (Phase 19)" << std::endl;
    }

    bool parse_args(int argc, char** argv, Options& opts)
    {
        for (int i = 1; i < argc; ++i) {
            const char* a = argv[i];
            if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
                opts.help = true;
                return true;
            } else if (std::strcmp(a, "--version") == 0) {
                opts.version = true;
                return true;
            } else if (std::strcmp(a, "--database") == 0 || std::strcmp(a, "-d") == 0) {
                if (i + 1 >= argc) return false;
                opts.database_path = argv[++i];
            } else if (std::strcmp(a, "-c") == 0) {
                if (i + 1 >= argc) return false;
                opts.command = std::string(argv[++i]);
            } else if (std::strcmp(a, "-f") == 0) {
                if (i + 1 >= argc) return false;
                opts.file = std::string(argv[++i]);
            } else if (std::strcmp(a, "--json") == 0) {
                opts.json = true;
            } else if (std::strcmp(a, "--csv") == 0) {
                opts.csv = true;
            } else if (std::strcmp(a, "--timing") == 0) {
                opts.timing = true;
            } else {
                // If it's the first non-flag, treat as database path for convenience
                if (opts.database_path.empty() && a[0] != '-') {
                    opts.database_path = a;
                } else {
                    std::cerr << "Unknown argument: " << a << std::endl;
                    return false;
                }
            }
        }
        return true;
    }

    int run_sql(const Options& opts, const std::string& sql)
    {
        // Configure database path if provided
        if (!opts.database_path.empty()) {
            scratchbird::engine::set_executor_db_path(opts.database_path);
        }
        
        auto t0 = std::chrono::steady_clock::now();
        auto r = scratchbird::engine::execute_select_sql(sql);
        auto t1 = std::chrono::steady_clock::now();
        if (!r.success) {
            std::cerr << "error: " << r.error_message << std::endl;
            return 1;
        }
        if (opts.timing) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            std::cerr << "Time: " << ms << " ms" << std::endl;
        }
        return 0;
    }
}

int main(int argc, char** argv)
{
    Options opts;
    if (!parse_args(argc, argv, opts)) {
        print_help();
        return 2;
    }
    if (opts.help) {
        print_help();
        return 0;
    }
    if (opts.version) {
        print_version();
        return 0;
    }

    // Script/non-interactive mode only for initial milestone
    if (opts.command.has_value()) {
        return run_sql(opts, *opts.command);
    }
    if (opts.file.has_value()) {
        std::ifstream in(opts.file.value());
        if (!in.is_open()) {
            std::cerr << "error: cannot open file: " << opts.file.value() << std::endl;
            return 2;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return run_sql(opts, ss.str());
    }

    std::cerr << "Interactive shell not yet implemented. Use -c or -f." << std::endl;
    print_help();
    return 2;
}

