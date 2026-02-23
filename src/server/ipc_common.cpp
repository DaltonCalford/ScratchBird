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
 * IPC Common Implementation
 *
 * ScratchBird Local Server Architecture - Phase 1
 *
 * This file contains the platform-detection logic, factory methods,
 * and utility functions shared across all IPC implementations.
 */

#include "scratchbird/server/ipc_server.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

// Platform-specific headers for isServerRunning()
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <tlhelp32.h>
#else
    #include <sys/types.h>
    #include <signal.h>
    #include <unistd.h>
#endif

namespace scratchbird {
namespace server {

// Forward declarations for platform-specific factory functions
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
std::unique_ptr<IPCServer> createUnixSocketServer(const IPCServerConfig& config,
                                                   core::ErrorContext* ctx);
std::unique_ptr<IPCClient> createUnixSocketClient(const IPCClientConfig& config,
                                                   core::ErrorContext* ctx);
#endif

#ifdef _WIN32
std::unique_ptr<IPCServer> createNamedPipeServer(const IPCServerConfig& config,
                                                  core::ErrorContext* ctx);
std::unique_ptr<IPCClient> createNamedPipeClient(const IPCClientConfig& config,
                                                  core::ErrorContext* ctx);
#endif

std::unique_ptr<IPCServer> createTCPServer(const IPCServerConfig& config,
                                            core::ErrorContext* ctx);
std::unique_ptr<IPCClient> createTCPClient(const IPCClientConfig& config,
                                            core::ErrorContext* ctx);

// ============================================================================
// Platform Detection
// ============================================================================

LocalIPCPolicy getDefaultLocalIPCPolicy() {
    LocalIPCPolicy policy;
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    policy.preferred_method = IPCMethod::UNIX_SOCKET;
    policy.fallback_method = IPCMethod::TCP_LOCALHOST;
    policy.fallback_enabled = true;
    policy.peer_credentials_supported = true;
    policy.peer_credentials_required_for_peer_auth = true;
#elif defined(_WIN32)
    policy.preferred_method = IPCMethod::NAMED_PIPE;
    policy.fallback_method = IPCMethod::TCP_LOCALHOST;
    policy.fallback_enabled = true;
    // Windows named pipes currently expose PID only in this cycle.
    policy.peer_credentials_supported = false;
    policy.peer_credentials_required_for_peer_auth = true;
#else
    policy.preferred_method = IPCMethod::TCP_LOCALHOST;
    policy.fallback_method = IPCMethod::TCP_LOCALHOST;
    policy.fallback_enabled = false;
    policy.peer_credentials_supported = false;
    policy.peer_credentials_required_for_peer_auth = false;
#endif
    return policy;
}

LocalIPCPolicy resolveLocalIPCPolicy(IPCMethod requested_method) {
    LocalIPCPolicy resolved = getDefaultLocalIPCPolicy();
    if (requested_method != IPCMethod::AUTO) {
        resolved.preferred_method = requested_method;
        resolved.fallback_method = requested_method;
        resolved.fallback_enabled = false;
    }
    return resolved;
}

IPCMethod getDefaultIPCMethod() {
    return getDefaultLocalIPCPolicy().preferred_method;
}

// ============================================================================
// Path/Address Generation
// ============================================================================

std::string getIPCPath(const std::string& database_name, IPCMethod method) {
    // Resolve AUTO to platform default
    if (method == IPCMethod::AUTO) {
        method = getDefaultIPCMethod();
    }

    // Sanitize database name (remove special characters)
    std::string safe_name;
    for (char c : database_name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            safe_name += c;
        }
    }
    if (safe_name.empty()) {
        safe_name = "default";
    }

    switch (method) {
        case IPCMethod::UNIX_SOCKET: {
            // Keep socket files inside the build tree (no /tmp usage per project rules)
            std::filesystem::path base = std::filesystem::path("build") / "ipc";
            std::error_code ec;
            std::filesystem::create_directories(base, ec);
#ifndef _WIN32
            if (ec || ::access(base.c_str(), W_OK) != 0) {
                std::filesystem::path fallback = std::filesystem::path("build") / "ipc_user";
                std::error_code fallback_ec;
                std::filesystem::create_directories(fallback, fallback_ec);
                if (!fallback_ec) {
                    base = fallback;
                }
            }
#else
            if (ec) {
                std::filesystem::path fallback = std::filesystem::path("build") / "ipc_user";
                std::error_code fallback_ec;
                std::filesystem::create_directories(fallback, fallback_ec);
                if (!fallback_ec) {
                    base = fallback;
                }
            }
#endif
            auto path = base / ("scratchbird-" + safe_name + ".sock");
            return path.string();
        }

        case IPCMethod::NAMED_PIPE:
            // Windows named pipe: \\.\pipe\scratchbird-{name}
            return "\\\\.\\pipe\\scratchbird-" + safe_name;

        case IPCMethod::TCP_LOCALHOST:
            // TCP doesn't have a path, return address format
            return "127.0.0.1:3092";

        default:
            // Fallback to the build IPC directory
            std::filesystem::path base = std::filesystem::path("build") / "ipc";
            std::error_code ec;
            std::filesystem::create_directories(base, ec);
#ifndef _WIN32
            if (ec || ::access(base.c_str(), W_OK) != 0) {
                std::filesystem::path fallback = std::filesystem::path("build") / "ipc_user";
                std::error_code fallback_ec;
                std::filesystem::create_directories(fallback, fallback_ec);
                if (!fallback_ec) {
                    base = fallback;
                }
            }
#else
            if (ec) {
                std::filesystem::path fallback = std::filesystem::path("build") / "ipc_user";
                std::error_code fallback_ec;
                std::filesystem::create_directories(fallback, fallback_ec);
                if (!fallback_ec) {
                    base = fallback;
                }
            }
#endif
            auto path = base / ("scratchbird-" + safe_name + ".sock");
            return path.string();
    }
}

std::string getPIDFilePath(const std::string& database_name) {
    // Sanitize database name
    std::string safe_name;
    for (char c : database_name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            safe_name += c;
        }
    }
    if (safe_name.empty()) {
        safe_name = "default";
    }

#ifdef _WIN32
    // Windows: use %TEMP% directory
    char temp_path[MAX_PATH];
    if (GetTempPathA(MAX_PATH, temp_path) > 0) {
        return std::string(temp_path) + "scratchbird-" + safe_name + ".pid";
    }
    return "C:\\temp\\scratchbird-" + safe_name + ".pid";
#else
    // Unix: keep PID files inside the build tree (no /tmp usage per project rules)
    std::filesystem::path base = std::filesystem::path("build") / "run";
    std::error_code ec;
    std::filesystem::create_directories(base, ec);
    auto path = base / ("scratchbird-" + safe_name + ".pid");
    return path.string();
#endif
}

bool looksLikeTcpEndpoint(const std::string& endpoint) {
    if (endpoint.empty()) {
        return false;
    }
    if (endpoint.find('/') != std::string::npos || endpoint.find('\\') != std::string::npos) {
        return false;
    }
    auto colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= endpoint.size()) {
        return false;
    }
    for (size_t i = colon + 1; i < endpoint.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(endpoint[i]))) {
            return false;
        }
    }
    return true;
}

bool parseTcpEndpointPort(const std::string& endpoint, uint16_t& port_out) {
    if (!looksLikeTcpEndpoint(endpoint)) {
        return false;
    }
    auto colon = endpoint.rfind(':');
    if (colon == std::string::npos || colon + 1 >= endpoint.size()) {
        return false;
    }
    try {
        unsigned long port = std::stoul(endpoint.substr(colon + 1));
        if (port == 0 || port > 65535) {
            return false;
        }
        port_out = static_cast<uint16_t>(port);
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Server Running Check
// ============================================================================

bool isServerRunning(const std::string& database_name) {
    std::string pid_file = getPIDFilePath(database_name);

    // Read PID from file
    std::ifstream f(pid_file);
    if (!f.is_open()) {
        return false;  // No PID file
    }

    int pid = 0;
    f >> pid;
    f.close();

    if (pid <= 0) {
        return false;  // Invalid PID
    }

#ifdef _WIN32
    // Windows: Open process to check if it exists
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                   static_cast<DWORD>(pid));
    if (hProcess == NULL) {
        return false;  // Process doesn't exist
    }

    DWORD exitCode = 0;
    if (GetExitCodeProcess(hProcess, &exitCode)) {
        CloseHandle(hProcess);
        return (exitCode == STILL_ACTIVE);
    }

    CloseHandle(hProcess);
    return false;
#else
    // Unix: Send signal 0 to check if process exists
    // This doesn't actually send a signal, just checks existence
    if (kill(static_cast<pid_t>(pid), 0) == 0) {
        return true;  // Process exists
    }

    // Check if we got EPERM (process exists but we don't have permission)
    if (errno == EPERM) {
        return true;  // Process exists but owned by another user
    }

    return false;  // Process doesn't exist (ESRCH)
#endif
}

// ============================================================================
// String Conversion Utilities
// ============================================================================

const char* ipcMethodToString(IPCMethod method) {
    switch (method) {
        case IPCMethod::AUTO:
            return "auto";
        case IPCMethod::UNIX_SOCKET:
            return "unix";
        case IPCMethod::NAMED_PIPE:
            return "pipe";
        case IPCMethod::TCP_LOCALHOST:
            return "tcp";
        default:
            return "unknown";
    }
}

IPCMethod stringToIPCMethod(const std::string& str) {
    // Convert to lowercase for comparison
    std::string lower;
    for (char c : str) {
        lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "auto" || lower == "default") {
        return IPCMethod::AUTO;
    }
    if (lower == "unix" || lower == "unix_socket" || lower == "socket") {
        return IPCMethod::UNIX_SOCKET;
    }
    if (lower == "pipe" || lower == "named_pipe" || lower == "namedpipe") {
        return IPCMethod::NAMED_PIPE;
    }
    if (lower == "tcp" || lower == "tcp_localhost" || lower == "localhost") {
        return IPCMethod::TCP_LOCALHOST;
    }

    // Default to auto if not recognized
    return IPCMethod::AUTO;
}

// ============================================================================
// Factory Methods
// ============================================================================

std::unique_ptr<IPCServer> IPCServer::create(const IPCServerConfig& config,
                                              core::ErrorContext* ctx) {
    IPCServerConfig resolved = config;
    IPCMethod method = resolved.method;

    // Resolve AUTO to platform default
    if (method == IPCMethod::AUTO) {
        method = getDefaultIPCMethod();
        if (looksLikeTcpEndpoint(resolved.socket_path)) {
            method = IPCMethod::TCP_LOCALHOST;
        }
    }
    if (method == IPCMethod::TCP_LOCALHOST) {
        uint16_t port = 0;
        if (parseTcpEndpointPort(resolved.socket_path, port)) {
            resolved.tcp_port = port;
        }
    }

    switch (method) {
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
        case IPCMethod::UNIX_SOCKET:
            return createUnixSocketServer(resolved, ctx);
#endif

#ifdef _WIN32
        case IPCMethod::NAMED_PIPE:
            return createNamedPipeServer(resolved, ctx);
#endif

        case IPCMethod::TCP_LOCALHOST:
            return createTCPServer(resolved, ctx);

        default:
            // Unsupported method on this platform
            SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                              ("IPC method not supported on this platform: " +
                               std::string(ipcMethodToString(method))).c_str());

            // Fall back to TCP which works everywhere
            return createTCPServer(config, ctx);
    }
}

std::unique_ptr<IPCClient> IPCClient::create(const IPCClientConfig& config,
                                              core::ErrorContext* ctx) {
    IPCClientConfig resolved = config;
    IPCMethod method = resolved.method;

    // Resolve AUTO to platform default
    if (method == IPCMethod::AUTO) {
        method = getDefaultIPCMethod();
        if (looksLikeTcpEndpoint(resolved.socket_path)) {
            method = IPCMethod::TCP_LOCALHOST;
        }
    }
    if (method == IPCMethod::TCP_LOCALHOST) {
        uint16_t port = 0;
        if (parseTcpEndpointPort(resolved.socket_path, port)) {
            resolved.tcp_port = port;
        }
    }

    switch (method) {
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
        case IPCMethod::UNIX_SOCKET:
            return createUnixSocketClient(resolved, ctx);
#endif

#ifdef _WIN32
        case IPCMethod::NAMED_PIPE:
            return createNamedPipeClient(resolved, ctx);
#endif

        case IPCMethod::TCP_LOCALHOST:
            return createTCPClient(resolved, ctx);

        default:
            SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED,
                              ("IPC method not supported on this platform: " +
                               std::string(ipcMethodToString(method))).c_str());

            // Fall back to TCP
            return createTCPClient(resolved, ctx);
    }
}

} // namespace server
} // namespace scratchbird
