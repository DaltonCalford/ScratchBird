/**
 * ScratchBird Listener - Protocol-Specific Entry Point
 *
 * Scaffolding for listener binaries per protocol. Accepts connections
 * and prepares for parser handoff (control-plane wired separately).
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "scratchbird/core/error_context.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"
#include "scratchbird/server/config_parser.h"
#include "scratchbird/version.h"

#ifndef SB_LISTENER_PROTOCOL
#define SB_LISTENER_PROTOCOL "scratchbird"
#endif

#ifndef SB_LISTENER_NAME
#define SB_LISTENER_NAME "sb_listener"
#endif

namespace {

std::atomic<bool> g_shutdown{false};

struct ListenerConfig {
    std::string protocol = SB_LISTENER_PROTOCOL;
    std::string bind_address = "0.0.0.0";
    uint16_t port = 0;
    std::string control_socket_dir;
    std::string config_path;
    std::string log_level = "info";
    bool show_help = false;
    bool show_version = false;
};

void handleSignal(int) {
    g_shutdown.store(true, std::memory_order_release);
}

uint16_t defaultPortForProtocol(const std::string& protocol) {
    using scratchbird::network::DEFAULT_FIREBIRD_PORT;
    using scratchbird::network::DEFAULT_MYSQL_PORT;
    using scratchbird::network::DEFAULT_NATIVE_PORT;
    using scratchbird::network::DEFAULT_POSTGRESQL_PORT;

    if (protocol == "postgresql") {
        return DEFAULT_POSTGRESQL_PORT;
    }
    if (protocol == "mysql") {
        return DEFAULT_MYSQL_PORT;
    }
    if (protocol == "firebird") {
        return DEFAULT_FIREBIRD_PORT;
    }
    return DEFAULT_NATIVE_PORT;
}

std::string protocolKey(const std::string& protocol) {
    if (protocol == "postgresql") {
        return "pg";
    }
    if (protocol == "mysql") {
        return "mysql";
    }
    if (protocol == "firebird") {
        return "fb";
    }
    return "native";
}

std::string controlSocketPath(const ListenerConfig& config) {
#ifdef _WIN32
    std::string protocol = config.protocol;
    if (protocol.empty()) {
        protocol = "scratchbird";
    }
    return "\\\\.\\pipe\\scratchbird\\" + protocol + "\\listener";
#else
    if (config.control_socket_dir.empty()) {
        return std::string();
    }
    std::string path = config.control_socket_dir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    return path + "sb_listener." + config.protocol + ".sock";
#endif
}

void printUsage(const char* program) {
    std::cout << SB_LISTENER_NAME << " (" << SB_LISTENER_PROTOCOL << ")\n\n"
              << "Usage:\n"
              << "  " << program << " [options]\n\n"
              << "Options:\n"
              << "  --config <file>             Config file path\n"
              << "  --bind <addr>               Bind address\n"
              << "  --port <port>               Listen port\n"
              << "  --control-socket-dir <dir>  Control socket directory\n"
              << "  --log-level <level>         info|debug|warn|error\n"
              << "  --help, -h                  Show this help\n"
              << "  --version                   Show version\n";
}

void printVersion() {
    std::cout << SB_LISTENER_NAME << " (" << SCRATCHBIRD_VERSION_STRING << ")\n";
}

bool parseArgsForConfig(int argc, char* argv[], ListenerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.show_help = true;
        } else if (arg == "--version") {
            config.show_version = true;
        } else if (arg == "--config" && i + 1 < argc) {
            config.config_path = argv[++i];
        } else if (arg.rfind("--config=", 0) == 0) {
            config.config_path = arg.substr(9);
        }
    }
    return true;
}

bool applyConfigFile(ListenerConfig& config) {
    if (config.config_path.empty()) {
        return true;
    }

    scratchbird::server::ConfigParser parser;
    scratchbird::core::ErrorContext ctx;
    if (parser.parseFile(config.config_path, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Failed to parse config: " << ctx.message << "\n";
        return false;
    }

    const auto* network = parser.section("network");
    if (network) {
        config.bind_address = network->getString("bind_address", config.bind_address);
        std::string key = protocolKey(config.protocol) + "_port";
        config.port = static_cast<uint16_t>(network->getInt(key, config.port));
        if (network->has("control_socket_dir")) {
            config.control_socket_dir = network->getString("control_socket_dir",
                                                           config.control_socket_dir);
        }
    }

    const auto* server = parser.section("server");
    if (server && server->has("control_socket_dir")) {
        config.control_socket_dir = server->getString("control_socket_dir",
                                                     config.control_socket_dir);
    }

    return true;
}

bool applyArgOverrides(int argc, char* argv[], ListenerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--bind" && i + 1 < argc) {
            config.bind_address = argv[++i];
        } else if (arg.rfind("--bind=", 0) == 0) {
            config.bind_address = arg.substr(7);
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                config.port = static_cast<uint16_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid port value\n";
                return false;
            }
        } else if (arg.rfind("--port=", 0) == 0) {
            try {
                config.port = static_cast<uint16_t>(std::stoul(arg.substr(7)));
            } catch (...) {
                std::cerr << "Invalid port value\n";
                return false;
            }
        } else if (arg == "--control-socket-dir" && i + 1 < argc) {
            config.control_socket_dir = argv[++i];
        } else if (arg.rfind("--control-socket-dir=", 0) == 0) {
            config.control_socket_dir = arg.substr(21);
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = argv[++i];
        } else if (arg.rfind("--log-level=", 0) == 0) {
            config.log_level = arg.substr(12);
        }
    }
    return true;
}

int runListener(const ListenerConfig& config) {
    using scratchbird::network::AddressFamily;
    using scratchbird::network::NetworkAddress;
    using scratchbird::network::Socket;

    if (!scratchbird::network::initNetwork()) {
        std::cerr << "Failed to initialize network subsystem\n";
        return 2;
    }

    scratchbird::core::ErrorContext ctx;
    auto server_socket = Socket::create(AddressFamily::IPV4, scratchbird::network::SocketType::STREAM, &ctx);
    if (!server_socket) {
        std::cerr << "Failed to create socket: " << ctx.message << "\n";
        return 2;
    }

    NetworkAddress addr;
    addr.family = AddressFamily::IPV4;
    addr.host = config.bind_address;
    addr.port = config.port;

    if (server_socket->bind(addr, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Bind failed: " << ctx.message << "\n";
        return 2;
    }
    if (server_socket->listen(scratchbird::network::DEFAULT_LISTEN_BACKLOG, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Listen failed: " << ctx.message << "\n";
        return 2;
    }

    std::cout << SB_LISTENER_NAME << " listening on "
              << config.bind_address << ":" << config.port << "\n";

    std::string control_socket = controlSocketPath(config);
    if (!control_socket.empty()) {
        std::cout << "Control socket: " << control_socket << "\n";
    }

    // Accept loop (connections are closed until parser handoff is wired).
    while (!g_shutdown.load(std::memory_order_acquire)) {
        NetworkAddress client_addr;
        auto client = server_socket->accept(&client_addr, &ctx);
        if (!client) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::cout << "Accepted connection from " << client_addr.host
                  << ":" << client_addr.port << "\n";
        client->close();
    }

    server_socket->close();
    scratchbird::network::cleanupNetwork();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    ListenerConfig config;
    config.port = defaultPortForProtocol(config.protocol);

    parseArgsForConfig(argc, argv, config);
    if (config.show_help) {
        printUsage(argv[0]);
        return 0;
    }
    if (config.show_version) {
        printVersion();
        return 0;
    }

    if (!applyConfigFile(config)) {
        return 1;
    }
    if (!applyArgOverrides(argc, argv, config)) {
        return 1;
    }

    if (config.port == 0) {
        std::cerr << "Invalid port\n";
        return 1;
    }

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    return runListener(config);
}
