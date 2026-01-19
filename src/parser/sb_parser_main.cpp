/**
 * ScratchBird Parser Agent - Protocol-Specific Entry Point
 *
 * Scaffolding for parser agent binaries per dialect. The control-plane
 * socket and engine IPC wiring are tracked separately.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "scratchbird/version.h"

#ifndef SB_PARSER_PROTOCOL
#define SB_PARSER_PROTOCOL "scratchbird"
#endif

#ifndef SB_PARSER_NAME
#define SB_PARSER_NAME "sb_parser"
#endif

namespace {

std::atomic<bool> g_shutdown{false};

struct ParserConfig {
    std::string protocol = SB_PARSER_PROTOCOL;
    std::string control_socket;
    std::string engine_endpoint;
    std::string tls_config;
    std::string log_level = "info";
    uint32_t protocol_version = 1;
    uint32_t max_requests = 0;
    uint32_t max_age_seconds = 0;
    bool show_help = false;
    bool show_version = false;
};

void handleSignal(int) {
    g_shutdown.store(true, std::memory_order_release);
}

void printUsage(const char* program) {
    std::cout << SB_PARSER_NAME << " (" << SB_PARSER_PROTOCOL << ")\n\n"
              << "Usage:\n"
              << "  " << program << " --control-socket <path> --engine-endpoint <path> [options]\n\n"
              << "Options:\n"
              << "  --control-socket <path>   Control-plane socket path\n"
              << "  --engine-endpoint <path>  Engine IPC endpoint\n"
              << "  --tls-config <file>       TLS configuration file\n"
              << "  --protocol-version <n>    Dialect protocol version\n"
              << "  --max-requests <n>        Max sessions before recycle (0 = unlimited)\n"
              << "  --max-age-seconds <n>     Max lifetime before recycle (0 = unlimited)\n"
              << "  --log-level <level>       info|debug|warn|error\n"
              << "  --help, -h                Show this help\n"
              << "  --version                 Show version\n";
}

void printVersion() {
    std::cout << SB_PARSER_NAME << " (" << SCRATCHBIRD_VERSION_STRING << ")\n";
}

bool parseArgs(int argc, char* argv[], ParserConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.show_help = true;
        } else if (arg == "--version") {
            config.show_version = true;
        } else if (arg == "--control-socket" && i + 1 < argc) {
            config.control_socket = argv[++i];
        } else if (arg.rfind("--control-socket=", 0) == 0) {
            config.control_socket = arg.substr(17);
        } else if (arg == "--engine-endpoint" && i + 1 < argc) {
            config.engine_endpoint = argv[++i];
        } else if (arg.rfind("--engine-endpoint=", 0) == 0) {
            config.engine_endpoint = arg.substr(18);
        } else if (arg == "--tls-config" && i + 1 < argc) {
            config.tls_config = argv[++i];
        } else if (arg.rfind("--tls-config=", 0) == 0) {
            config.tls_config = arg.substr(13);
        } else if (arg == "--protocol-version" && i + 1 < argc) {
            try {
                config.protocol_version = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid protocol version\n";
                return false;
            }
        } else if (arg.rfind("--protocol-version=", 0) == 0) {
            try {
                config.protocol_version = static_cast<uint32_t>(std::stoul(arg.substr(19)));
            } catch (...) {
                std::cerr << "Invalid protocol version\n";
                return false;
            }
        } else if (arg == "--max-requests" && i + 1 < argc) {
            try {
                config.max_requests = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid max-requests value\n";
                return false;
            }
        } else if (arg.rfind("--max-requests=", 0) == 0) {
            try {
                config.max_requests = static_cast<uint32_t>(std::stoul(arg.substr(15)));
            } catch (...) {
                std::cerr << "Invalid max-requests value\n";
                return false;
            }
        } else if (arg == "--max-age-seconds" && i + 1 < argc) {
            try {
                config.max_age_seconds = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid max-age-seconds value\n";
                return false;
            }
        } else if (arg.rfind("--max-age-seconds=", 0) == 0) {
            try {
                config.max_age_seconds = static_cast<uint32_t>(std::stoul(arg.substr(18)));
            } catch (...) {
                std::cerr << "Invalid max-age-seconds value\n";
                return false;
            }
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = argv[++i];
        } else if (arg.rfind("--log-level=", 0) == 0) {
            config.log_level = arg.substr(12);
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

bool validateConfig(const ParserConfig& config) {
    if (config.control_socket.empty()) {
        std::cerr << "Missing --control-socket\n";
        return false;
    }
    if (config.engine_endpoint.empty()) {
        std::cerr << "Missing --engine-endpoint\n";
        return false;
    }
    if (!config.tls_config.empty()) {
        std::ifstream in(config.tls_config);
        if (!in.good()) {
            std::cerr << "TLS config not readable: " << config.tls_config << "\n";
            return false;
        }
    }
    return true;
}

int runParser(const ParserConfig& config) {
    std::cout << SB_PARSER_NAME << " starting\n"
              << "Protocol: " << config.protocol << "\n"
              << "Control socket: " << config.control_socket << "\n"
              << "Engine endpoint: " << config.engine_endpoint << "\n";

    if (!config.tls_config.empty()) {
        std::cout << "TLS config: " << config.tls_config << "\n";
    }

    // Idle loop until control-plane wiring is implemented.
    while (!g_shutdown.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    ParserConfig config;
    if (!parseArgs(argc, argv, config)) {
        printUsage(argv[0]);
        return 1;
    }
    if (config.show_help) {
        printUsage(argv[0]);
        return 0;
    }
    if (config.show_version) {
        printVersion();
        return 0;
    }
    if (!validateConfig(config)) {
        return 1;
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    return runParser(config);
}
