/**
 * ScratchBird Server - Main Entry Point
 *
 * Service controller runner (listener + parser pools).
 */

#include <iostream>
#include <string>
#include <vector>

#include "scratchbird/server/service_controller.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird;
using namespace scratchbird::server;

namespace {

std::vector<std::string> normalizeArgs(int argc, char* argv[]) {
    std::vector<std::string> normalized;
    normalized.reserve(static_cast<size_t>(argc) + 2);
    normalized.push_back(argv[0]);

    bool has_database_flag = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--database" || arg.rfind("--database=", 0) == 0) {
            has_database_flag = true;
        }
    }

    bool inserted_db = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (!has_database_flag && !inserted_db && !arg.empty() && arg[0] != '-') {
            normalized.push_back("-d");
            normalized.push_back(arg);
            inserted_db = true;
            continue;
        }
        normalized.push_back(arg);
    }

    return normalized;
}

}  // namespace

int main(int argc, char* argv[]) {
    auto normalized = normalizeArgs(argc, argv);
    std::vector<char*> argv_buf;
    argv_buf.reserve(normalized.size() + 1);
    for (auto& arg : normalized) {
        argv_buf.push_back(const_cast<char*>(arg.c_str()));
    }
    argv_buf.push_back(nullptr);

    ServiceController service;
    core::ErrorContext ctx;
    core::Status status = service.parseCommandLine(static_cast<int>(normalized.size()),
                                                   argv_buf.data(), &ctx);
    if (status != core::Status::OK) {
        std::cerr << "Command-line error: " << ctx.message << "\n";
        return 1;
    }

    if (service.exitRequested()) {
        return 0;
    }

    status = service.run(&ctx);
    if (status != core::Status::OK) {
        std::cerr << "Server error: " << ctx.message << "\n";
        return 1;
    }

    return 0;
}
