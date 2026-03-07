/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Server - Main Entry Point
 *
 * Service controller runner for the engine runtime plus the protocol listener
 * and parser-agent process sets.
 */

#include <iostream>
#include <string>
#include <vector>

#include "scratchbird/server/service_controller.h"
#include "scratchbird/server/windows_service.h"
#include "scratchbird/core/error_context.h"

using namespace scratchbird;
using namespace scratchbird::server;

namespace {

struct LauncherOptions {
    bool windows_service = false;
    std::string windows_service_name = "ScratchBirdServer";
    std::vector<std::string> service_args;
};

std::vector<std::string> normalizeArgs(int argc, char* argv[]) {
    std::vector<std::string> normalized;
    normalized.reserve(static_cast<size_t>(argc) + 2);
    normalized.push_back(argv[0]);

    auto needs_value = [](const std::string& opt) -> bool {
        return opt == "-c" || opt == "--config" ||
               opt == "-D" || opt == "--data-dir" ||
               opt == "-d" || opt == "--database" ||
               opt == "-h" || opt == "--host" ||
               opt == "-p" || opt == "--port" ||
               opt == "--pg-port" || opt == "--mysql-port" || opt == "--fb-port" ||
               opt == "--control-socket-dir" ||
               opt == "--native-bind" || opt == "--postgres-bind" ||
               opt == "--mysql-bind" || opt == "--firebird-bind" ||
               opt == "--native-pool-min" || opt == "--native-pool-max" ||
               opt == "--postgres-pool-min" || opt == "--postgres-pool-max" ||
               opt == "--mysql-pool-min" || opt == "--mysql-pool-max" ||
               opt == "--firebird-pool-min" || opt == "--firebird-pool-max" ||
               opt == "--windows-service-name" ||
               opt == "-k" || opt == "--unix-socket" ||
               opt == "-N" || opt == "--max-connections" ||
               opt == "-B" || opt == "--shared-buffers";
    };

    bool has_database_flag = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--database" || arg.rfind("--database=", 0) == 0) {
            has_database_flag = true;
        }
    }

    bool inserted_db = false;
    bool skip_next_value = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (skip_next_value) {
            normalized.push_back(arg);
            skip_next_value = false;
            continue;
        }

        if (arg.rfind("--", 0) == 0) {
            auto eq = arg.find('=');
            std::string opt = (eq == std::string::npos) ? arg : arg.substr(0, eq);
            if (eq == std::string::npos && needs_value(opt)) {
                normalized.push_back(arg);
                skip_next_value = true;
                continue;
            }
        } else if (arg.size() == 2 && arg[0] == '-' && needs_value(arg)) {
            normalized.push_back(arg);
            skip_next_value = true;
            continue;
        }

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

LauncherOptions extractLauncherOptions(const std::vector<std::string>& normalized) {
    LauncherOptions options;
    if (normalized.empty()) {
        return options;
    }

    options.service_args.push_back(normalized.front());
    for (size_t i = 1; i < normalized.size(); ++i) {
        const std::string& arg = normalized[i];
        if (arg == "--windows-service") {
            options.windows_service = true;
            continue;
        }
        if (arg.rfind("--windows-service-name=", 0) == 0) {
            options.windows_service = true;
            options.windows_service_name = arg.substr(std::string("--windows-service-name=").size());
            continue;
        }
        if (arg == "--windows-service-name") {
            options.windows_service = true;
            if (i + 1 < normalized.size()) {
                options.windows_service_name = normalized[++i];
            }
            continue;
        }

        options.service_args.push_back(arg);
    }

    if (options.windows_service_name.empty()) {
        options.windows_service_name = "ScratchBirdServer";
    }
    return options;
}

}  // namespace

int main(int argc, char* argv[]) {
    auto normalized = normalizeArgs(argc, argv);
    auto launcher_options = extractLauncherOptions(normalized);
    if (launcher_options.service_args.empty()) {
        launcher_options.service_args.push_back(argv[0]);
    }

    ServiceController service;
    auto run_controller = [&service, &launcher_options]() -> int {
        std::vector<char*> argv_buf;
        argv_buf.reserve(launcher_options.service_args.size() + 1);
        for (auto& arg : launcher_options.service_args) {
            argv_buf.push_back(const_cast<char*>(arg.c_str()));
        }
        argv_buf.push_back(nullptr);

        core::ErrorContext ctx;
        core::Status status = service.parseCommandLine(
            static_cast<int>(launcher_options.service_args.size()),
            argv_buf.data(), &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Command-line error: " << ctx.message << "\n";
            return 1;
        }

        if (service.exitRequested()) {
            return 0;
        }

        ServiceController::StartupMode startup_mode = ServiceController::StartupMode::AUTO;
        if (launcher_options.windows_service) {
            startup_mode = ServiceController::StartupMode::FORCE_FOREGROUND;
        }

        status = service.runWithStartupMode(startup_mode, &ctx);
        if (status != core::Status::OK) {
            std::cerr << "Server error: " << ctx.message << "\n";
            return 1;
        }

        return 0;
    };

    auto service_host = createDefaultWindowsServiceHost();
    if (launcher_options.windows_service) {
        WindowsServiceOptions options;
        options.service_name = launcher_options.windows_service_name;

        core::ErrorContext service_ctx;
        core::Status service_status = service_host->runAsService(
            options,
            run_controller,
            [&service]() { service.shutdown(); },
            &service_ctx);
        if (service_status != core::Status::OK) {
            std::cerr << "Service mode error: " << service_ctx.message << "\n";
            return 1;
        }
        return 0;
    }

    core::ErrorContext launch_ctx;
    core::Status launch_status = service_host->runConsole(run_controller, &launch_ctx);
    if (launch_status != core::Status::OK) {
        if (!launch_ctx.message.empty()) {
            std::cerr << "Launcher error: " << launch_ctx.message << "\n";
        }
        return 1;
    }
    return 0;
}
