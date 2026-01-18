/**
 * ScratchBird Server - Main Entry Point
 *
 * Local Server Architecture - Phase 3
 *
 * Usage:
 *   sb_server <database_path> [options]
 *
 * Options:
 *   --create            Create database if it doesn't exist
 *   --page-size=<n>     Page size for new database (default: 16384)
 *   --port=<n>          TCP port (if using TCP, default: 3092)
 *   --tcp               Force TCP transport
 *   --unix              Force Unix socket transport
 *   --pid-file=<path>   Custom PID file path
 *   --max-connections=<n>  Maximum connections (default: 100)
 *   --verbose           Verbose logging
 *   --help              Show this help
 *   --version           Show version
 */

#include <iostream>
#include <string>
#include <cstring>

#include "scratchbird/server/scratchbird_server.h"
#include "scratchbird/version.h"

using namespace scratchbird;
using namespace scratchbird::server;

void printUsage(const char* program) {
    std::cout << "ScratchBird Database Server\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << program << " <database_path> [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --create              Create database if it doesn't exist\n";
    std::cout << "  --page-size=<n>       Page size for new database (default: 16384)\n";
    std::cout << "  --port=<n>            TCP port (default: 3092)\n";
    std::cout << "  --tcp                 Force TCP transport\n";
    std::cout << "  --unix                Force Unix socket transport\n";
    std::cout << "  --pid-file=<path>     Custom PID file path\n";
    std::cout << "  --max-connections=<n> Maximum connections (default: 100)\n";
    std::cout << "  --verbose, -v         Verbose logging\n";
    std::cout << "  --help, -h            Show this help\n";
    std::cout << "  --version             Show version\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program << " /path/to/mydb.sbdb\n";
    std::cout << "  " << program << " mydb.sbdb --create --verbose\n";
    std::cout << "  " << program << " mydb.sbdb --tcp --port=5555\n";
}

void printVersion() {
    std::cout << "sb_server (ScratchBird Database Server)\n";
    std::cout << "Version: 0.1.0-alpha.1\n";
    std::cout << "Local Server Architecture - Phase 3\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    ServerConfig config;
    bool show_help = false;
    bool show_version = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            show_help = true;
        } else if (arg == "--version") {
            show_version = true;
        } else if (arg == "--verbose" || arg == "-v") {
            config.verbose = true;
        } else if (arg == "--create") {
            config.auto_create_db = true;
        } else if (arg == "--tcp") {
            config.ipc_method = IPCMethod::TCP_LOCALHOST;
        } else if (arg == "--unix") {
            config.ipc_method = IPCMethod::UNIX_SOCKET;
        } else if (arg.find("--port=") == 0) {
            try {
                config.tcp_port = static_cast<uint16_t>(std::stoul(arg.substr(7)));
            } catch (...) {
                std::cerr << "Error: Invalid port number\n";
                return 1;
            }
        } else if (arg.find("--page-size=") == 0) {
            try {
                config.page_size = std::stoul(arg.substr(12));
            } catch (...) {
                std::cerr << "Error: Invalid page size\n";
                return 1;
            }
        } else if (arg.find("--pid-file=") == 0) {
            config.pid_file = arg.substr(11);
        } else if (arg.find("--max-connections=") == 0) {
            try {
                config.max_connections = std::stoul(arg.substr(18));
            } catch (...) {
                std::cerr << "Error: Invalid max connections\n";
                return 1;
            }
        } else if (arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        } else {
            // Assume database path
            if (config.database_path.empty()) {
                config.database_path = arg;
            } else {
                std::cerr << "Error: Multiple database paths specified\n";
                return 1;
            }
        }
    }

    // Handle help/version
    if (show_help) {
        printUsage(argv[0]);
        return 0;
    }

    if (show_version) {
        printVersion();
        return 0;
    }

    // Validate database path
    if (config.database_path.empty()) {
        std::cerr << "Error: No database path specified\n";
        printUsage(argv[0]);
        return 1;
    }

    // Check if server is already running
    if (ScratchBirdServer::isServerRunning(config.database_path)) {
        pid_t pid = ScratchBirdServer::getServerPID(config.database_path);
        std::cerr << "Error: Server already running for this database (PID: " << pid << ")\n";
        return 1;
    }

    // Create and start server
    if (config.verbose) {
        std::cout << "Starting ScratchBird server...\n";
        std::cout << "Database: " << config.database_path << "\n";
        std::cout << "IPC Method: ";
        switch (config.ipc_method) {
            case IPCMethod::AUTO:          std::cout << "automatic\n"; break;
            case IPCMethod::UNIX_SOCKET:   std::cout << "unix socket\n"; break;
            case IPCMethod::NAMED_PIPE:    std::cout << "named pipe\n"; break;
            case IPCMethod::TCP_LOCALHOST: std::cout << "tcp (port " << config.tcp_port << ")\n"; break;
        }
    }

    ScratchBirdServer server(config);
    core::ErrorContext ctx;

    core::Status status = server.start(&ctx);

    if (status != core::Status::OK) {
        std::cerr << "Server error: " << ctx.message << "\n";
        return 1;
    }

    return 0;
}
