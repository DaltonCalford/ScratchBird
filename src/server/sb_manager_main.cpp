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
 * ScratchBird Manager Proxy - Main Entry Point
 *
 * Beta MCP skeleton:
 * - Accepts native wire-protocol MCP control verbs
 * - Implements HELLO / AUTH_START / AUTH_CONTINUE / DB_LIST / DB_CONNECT
 * - Performs DB readiness check against internal native listener endpoint
 */

#include "scratchbird/core/error_context.h"
#include "scratchbird/network/control_plane.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/protocol/wire_protocol.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <poll.h>
#endif

namespace {

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::network::AddressFamily;
using scratchbird::network::NetworkAddress;
using scratchbird::network::Socket;
using scratchbird::network::SocketType;
using scratchbird::network::DatabaseBindingKeyRing;
using scratchbird::network::DatabaseBindingToken;
using scratchbird::network::ControlPlaneMessage;
using scratchbird::network::ControlPlaneMessageType;
using scratchbird::protocol::AuthMethod;
using scratchbird::protocol::AuthStatus;
using scratchbird::protocol::Message;
using scratchbird::protocol::MessageHeader;
using scratchbird::protocol::MessageType;
using scratchbird::protocol::ProtocolCodec;
using scratchbird::protocol::StatusRequestType;

std::atomic<bool> g_running{true};

struct ManagerOptions {
    std::string bind_address = "0.0.0.0";
    uint16_t port = 3090;
    std::string native_bind = "127.0.0.1";
    uint16_t native_port = 3392;
    std::string owner_database = "main";
    std::string control_socket_dir;
    std::string config_path;
    std::string log_level = "info";
    std::string dbbt_keyring_path;
    std::string mcp_auth_secret;
    uint32_t listener_id = 1;
    uint32_t dbbt_ttl_ms = 30000;
};

struct ManagerSessionContext {
    uint8_t session_id[16] = {0};
    bool auth_started = false;
    bool authenticated = false;
    AuthMethod auth_method = AuthMethod::PASSWORD;
    std::string username;
    std::string selected_database;
    std::vector<uint8_t> last_dbbt;
    std::vector<uint8_t> last_dbbt_id;
    uint64_t last_dbbt_expires_at_ms = 0;
    std::unique_ptr<Socket> prepared_upstream;
};

void onSignal(int) {
    g_running.store(false, std::memory_order_release);
}

void printUsage(const char* program) {
    std::cerr
        << "Usage: " << program << " [options]\n"
        << "  --bind <address>               Front-door bind address\n"
        << "  --port <port>                  Front-door port\n"
        << "  --native-bind <address>        Internal native listener bind address\n"
        << "  --native-port <port>           Internal native listener port\n"
        << "  --database-owner <name>        Database owner for MCP DB list/connect\n"
        << "  --control-socket-dir <path>    Control socket directory\n"
        << "  --config <path>                Server configuration path\n"
        << "  --log-level <level>            Log level\n"
        << "  --dbbt-keyring <path>          DBBT keyring file\n"
        << "  --mcp-auth-secret <value>      Manager MCP auth secret (token auth)\n"
        << "  --listener-id <id>             Listener id for DBBT binding\n"
        << "  --dbbt-ttl-ms <ms>             DBBT expiration window\n";
}

bool parseArgs(int argc, char* argv[], ManagerOptions& out) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto require_value = [&](const char* opt_name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << opt_name << "\n";
                return nullptr;
            }
            ++i;
            return argv[i];
        };

        if (arg == "--bind") {
            const char* value = require_value("--bind");
            if (!value) return false;
            out.bind_address = value;
            continue;
        }
        if (arg == "--port") {
            const char* value = require_value("--port");
            if (!value) return false;
            out.port = static_cast<uint16_t>(std::strtoul(value, nullptr, 10));
            continue;
        }
        if (arg == "--native-bind") {
            const char* value = require_value("--native-bind");
            if (!value) return false;
            out.native_bind = value;
            continue;
        }
        if (arg == "--native-port") {
            const char* value = require_value("--native-port");
            if (!value) return false;
            out.native_port = static_cast<uint16_t>(std::strtoul(value, nullptr, 10));
            continue;
        }
        if (arg == "--database-owner") {
            const char* value = require_value("--database-owner");
            if (!value) return false;
            out.owner_database = value;
            continue;
        }
        if (arg == "--control-socket-dir") {
            const char* value = require_value("--control-socket-dir");
            if (!value) return false;
            out.control_socket_dir = value;
            continue;
        }
        if (arg == "--config") {
            const char* value = require_value("--config");
            if (!value) return false;
            out.config_path = value;
            continue;
        }
        if (arg == "--log-level") {
            const char* value = require_value("--log-level");
            if (!value) return false;
            out.log_level = value;
            continue;
        }
        if (arg == "--dbbt-keyring") {
            const char* value = require_value("--dbbt-keyring");
            if (!value) return false;
            out.dbbt_keyring_path = value;
            continue;
        }
        if (arg == "--mcp-auth-secret") {
            const char* value = require_value("--mcp-auth-secret");
            if (!value) return false;
            out.mcp_auth_secret = value;
            continue;
        }
        if (arg == "--listener-id") {
            const char* value = require_value("--listener-id");
            if (!value) return false;
            out.listener_id = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
            continue;
        }
        if (arg == "--dbbt-ttl-ms") {
            const char* value = require_value("--dbbt-ttl-ms");
            if (!value) return false;
            out.dbbt_ttl_ms = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
            continue;
        }
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        return false;
    }

    if (out.port == 0 || out.native_port == 0) {
        std::cerr << "Both --port and --native-port must be greater than zero\n";
        return false;
    }
    if (out.owner_database.empty()) {
        out.owner_database = "main";
    }
    if (out.listener_id == 0) {
        out.listener_id = 1;
    }
    if (out.dbbt_ttl_ms == 0) {
        out.dbbt_ttl_ms = 30000;
    }
    if (out.mcp_auth_secret.empty()) {
        const char* env_secret = std::getenv("SCRATCHBIRD_MCP_AUTH_SECRET");
        if (env_secret != nullptr && env_secret[0] != '\0') {
            out.mcp_auth_secret = env_secret;
        }
    }
    if (out.mcp_auth_secret.empty()) {
        std::cerr
            << "Manager MCP auth secret is required (--mcp-auth-secret or "
               "SCRATCHBIRD_MCP_AUTH_SECRET)\n";
        return false;
    }
    return true;
}

AddressFamily addressFamilyForHost(const std::string& host) {
    return host.find(':') != std::string::npos
        ? AddressFamily::IPV6
        : AddressFamily::IPV4;
}

std::string internalEndpointString(const ManagerOptions& options) {
    return options.native_bind + ":" + std::to_string(options.native_port);
}

uint64_t fnv1a64(const std::string& value, uint64_t seed) {
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = seed;
    for (unsigned char c : value) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
    return hash;
}

std::array<uint8_t, 16> deriveDatabaseUuid(const std::string& database_name) {
    std::array<uint8_t, 16> out{};
    const uint64_t hi = fnv1a64(database_name, 1469598103934665603ull);
    const uint64_t lo = fnv1a64(database_name, 1099511628211ull);
    for (int i = 0; i < 8; ++i) {
        out[static_cast<size_t>(i)] = static_cast<uint8_t>((hi >> (i * 8)) & 0xFF);
        out[static_cast<size_t>(8 + i)] = static_cast<uint8_t>((lo >> (i * 8)) & 0xFF);
    }
    // Normalize to UUID v4 layout bits.
    out[6] = static_cast<uint8_t>((out[6] & 0x0F) | 0x40);
    out[8] = static_cast<uint8_t>((out[8] & 0x3F) | 0x80);
    return out;
}

std::vector<uint8_t> randomBytes(size_t count) {
    std::vector<uint8_t> bytes(count, 0);
    std::random_device rd;
    for (size_t i = 0; i < count; ++i) {
        bytes[i] = static_cast<uint8_t>(rd() & 0xFF);
    }
    return bytes;
}

bool timingSafeBytesEqual(const std::vector<uint8_t>& lhs, const std::vector<uint8_t>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < lhs.size(); ++i) {
        diff |= static_cast<uint8_t>(lhs[i] ^ rhs[i]);
    }
    return diff == 0;
}

void logManagedAuditEvent(const char* event_name,
                          bool success,
                          const std::string& database_name,
                          uint32_t listener_id,
                          const std::string& reason,
                          const std::vector<uint8_t>& dbbt_id = {}) {
    std::cerr << "[audit] event="
              << (event_name ? event_name : "UNKNOWN")
              << " success=" << (success ? 1 : 0)
              << " db=" << database_name
              << " listener_id=" << listener_id;
    if (!reason.empty()) {
        std::cerr << " reason=" << reason;
    }
    if (!dbbt_id.empty()) {
        std::cerr << " dbbt_id=" << scratchbird::network::bytesToHex(dbbt_id);
    }
    std::cerr << "\n";
}

std::string listenerManagementSocketPath(const ManagerOptions& options) {
#ifdef _WIN32
    (void)options;
    return std::string();
#else
    if (options.control_socket_dir.empty()) {
        return std::string();
    }
    std::string path = options.control_socket_dir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    path += "sb_listener.scratchbird.";
    path += std::to_string(options.native_port);
    path += ".mgmt.sock";
    return path;
#endif
}

struct ProxyBuffer {
    std::vector<uint8_t> bytes;
    size_t offset = 0;

    bool empty() const {
        return offset >= bytes.size();
    }

    size_t size() const {
        return empty() ? 0 : (bytes.size() - offset);
    }

    const uint8_t* data() const {
        return empty() ? nullptr : (bytes.data() + offset);
    }

    void append(const uint8_t* data_ptr, size_t len) {
        if (len == 0) {
            return;
        }
        bytes.insert(bytes.end(), data_ptr, data_ptr + len);
    }

    void consume(size_t len) {
        if (len == 0 || empty()) {
            return;
        }
        offset = std::min(offset + len, bytes.size());
        if (offset >= bytes.size()) {
            bytes.clear();
            offset = 0;
            return;
        }
        if (offset >= 65536 && offset * 2 >= bytes.size()) {
            bytes.erase(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
            offset = 0;
        }
    }
};

struct ProxyIoReady {
    bool client_readable = false;
    bool client_writable = false;
    bool upstream_readable = false;
    bool upstream_writable = false;

    bool any() const {
        return client_readable || client_writable || upstream_readable || upstream_writable;
    }
};

bool waitProxyIo(Socket& client,
                 Socket& upstream,
                 bool want_client_read,
                 bool want_client_write,
                 bool want_upstream_read,
                 bool want_upstream_write,
                 int timeout_ms,
                 ProxyIoReady& ready) {
    ready = ProxyIoReady{};

#ifdef _WIN32
    fd_set read_fds;
    fd_set write_fds;
    fd_set except_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    socket_t max_fd = 0;
    auto register_fd = [&](socket_t fd, bool want_read, bool want_write) {
        if (fd == INVALID_SOCKET_VALUE) {
            return;
        }
        if (want_read) {
            FD_SET(fd, &read_fds);
        }
        if (want_write) {
            FD_SET(fd, &write_fds);
        }
        FD_SET(fd, &except_fds);
        if (fd > max_fd) {
            max_fd = fd;
        }
    };

    register_fd(client.getFd(), want_client_read, want_client_write);
    register_fd(upstream.getFd(), want_upstream_read, want_upstream_write);

    timeval tv;
    timeval* tv_ptr = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    const int rc = ::select(static_cast<int>(max_fd + 1),
                            &read_fds,
                            &write_fds,
                            &except_fds,
                            tv_ptr);
    if (rc <= 0) {
        return false;
    }

    const socket_t client_fd = client.getFd();
    const socket_t upstream_fd = upstream.getFd();

    if (client_fd != INVALID_SOCKET_VALUE) {
        const bool has_except = FD_ISSET(client_fd, &except_fds);
        ready.client_readable = (want_client_read && FD_ISSET(client_fd, &read_fds)) || has_except;
        ready.client_writable = (want_client_write && FD_ISSET(client_fd, &write_fds)) || has_except;
    }
    if (upstream_fd != INVALID_SOCKET_VALUE) {
        const bool has_except = FD_ISSET(upstream_fd, &except_fds);
        ready.upstream_readable =
            (want_upstream_read && FD_ISSET(upstream_fd, &read_fds)) || has_except;
        ready.upstream_writable =
            (want_upstream_write && FD_ISSET(upstream_fd, &write_fds)) || has_except;
    }

    return ready.any();
#else
    pollfd pfds[2];
    int count = 0;

    auto register_fd = [&](Socket& socket, bool want_read, bool want_write) {
        if (!want_read && !want_write) {
            return;
        }
        pollfd pfd{};
        pfd.fd = socket.getFd();
        pfd.events = 0;
        if (want_read) {
            pfd.events = static_cast<short>(pfd.events | POLLIN);
        }
        if (want_write) {
            pfd.events = static_cast<short>(pfd.events | POLLOUT);
        }
        pfds[count++] = pfd;
    };

    register_fd(client, want_client_read, want_client_write);
    register_fd(upstream, want_upstream_read, want_upstream_write);

    if (count == 0) {
        return false;
    }

    const int rc = ::poll(pfds, static_cast<nfds_t>(count), timeout_ms);
    if (rc <= 0) {
        return false;
    }

    const socket_t client_fd = client.getFd();
    const socket_t upstream_fd = upstream.getFd();

    for (int i = 0; i < count; ++i) {
        const pollfd& pfd = pfds[i];
        const bool errorish = (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;
        const bool readable = (pfd.revents & POLLIN) != 0;
        const bool writable = (pfd.revents & POLLOUT) != 0;

        if (pfd.fd == client_fd) {
            ready.client_readable = (want_client_read && readable) || (want_client_read && errorish);
            ready.client_writable = (want_client_write && writable) || (want_client_write && errorish);
        } else if (pfd.fd == upstream_fd) {
            ready.upstream_readable =
                (want_upstream_read && readable) || (want_upstream_read && errorish);
            ready.upstream_writable =
                (want_upstream_write && writable) || (want_upstream_write && errorish);
        }
    }

    return ready.any();
#endif
}

std::unique_ptr<Socket> connectInternalNative(const ManagerOptions& options,
                                              ErrorContext* ctx) {
    NetworkAddress upstream_address;
    upstream_address.family = addressFamilyForHost(options.native_bind);
    upstream_address.host = options.native_bind;
    upstream_address.port = options.native_port;
    return Socket::connect(upstream_address, scratchbird::network::SocketOptions{}, ctx);
}

bool runProxyTransport(Socket& client,
                       const ManagerOptions& options,
                       std::unique_ptr<Socket> prepared_upstream,
                       ErrorContext* ctx) {
    auto upstream = std::move(prepared_upstream);
    if (!upstream) {
        ErrorContext connect_ctx;
        upstream = connectInternalNative(options, &connect_ctx);
        if (!upstream) {
            SET_ERROR_CONTEXT(ctx, Status::CONNECTION_FAILURE,
                              connect_ctx.message.empty()
                                  ? "Failed to connect internal native listener"
                                  : connect_ctx.message.c_str());
            return false;
        }
    }

    client.setNonBlocking(true, nullptr);
    upstream->setNonBlocking(true, nullptr);
    client.setTcpNoDelay(true, nullptr);
    upstream->setTcpNoDelay(true, nullptr);

    constexpr size_t kMaxBufferedBytes = 256 * 1024;
    constexpr size_t kReadChunkBytes = 32 * 1024;
    constexpr auto kDrainIdleLimit = std::chrono::milliseconds(50);

    ProxyBuffer client_to_upstream;
    ProxyBuffer upstream_to_client;
    std::array<uint8_t, kReadChunkBytes> scratch{};

    bool client_read_eof = false;
    bool upstream_read_eof = false;
    bool client_write_closed = false;
    bool upstream_write_closed = false;
    auto last_progress = std::chrono::steady_clock::now();

    auto readInto = [&](Socket& source, ProxyBuffer& out, bool& eof_flag) -> bool {
        if (eof_flag || out.size() >= kMaxBufferedBytes) {
            return true;
        }

        while (out.size() < kMaxBufferedBytes) {
            const size_t room = kMaxBufferedBytes - out.size();
            const size_t to_read = std::min(room, kReadChunkBytes);
            size_t bytes_read = 0;
            ErrorContext io_ctx;
            const Status status = source.read(scratch.data(), to_read, &bytes_read, &io_ctx);
            if (status == Status::CONNECTION_FAILURE) {
                eof_flag = true;
                return true;
            }
            if (status != Status::OK) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, status,
                                      io_ctx.message.empty() ? "Proxy read failed"
                                                             : io_ctx.message.c_str());
                }
                return false;
            }
            if (bytes_read == 0) {
                break;
            }
            out.append(scratch.data(), bytes_read);
            if (bytes_read < to_read) {
                break;
            }
        }
        return true;
    };

    auto flushOut = [&](Socket& sink, ProxyBuffer& in) -> bool {
        if (in.empty()) {
            return true;
        }

        while (!in.empty()) {
            size_t bytes_written = 0;
            ErrorContext io_ctx;
            const Status status = sink.write(in.data(), in.size(), &bytes_written, &io_ctx);
            if (status != Status::OK) {
                if (ctx) {
                    SET_ERROR_CONTEXT(ctx, status,
                                      io_ctx.message.empty() ? "Proxy write failed"
                                                             : io_ctx.message.c_str());
                }
                return false;
            }
            if (bytes_written == 0) {
                break;
            }
            in.consume(bytes_written);
        }
        return true;
    };

    while (g_running.load(std::memory_order_acquire)) {
        const bool want_client_read =
            !client_read_eof && !upstream_write_closed && client_to_upstream.size() < kMaxBufferedBytes;
        const bool want_upstream_read =
            !upstream_read_eof && !client_write_closed && upstream_to_client.size() < kMaxBufferedBytes;
        const bool want_upstream_write = !upstream_write_closed && !client_to_upstream.empty();
        const bool want_client_write = !client_write_closed && !upstream_to_client.empty();

        const int io_wait_timeout_ms = (want_upstream_write || want_client_write) ? 0 : 1;
        ProxyIoReady ready;
        (void)waitProxyIo(client,
                          *upstream,
                          want_client_read,
                          want_client_write,
                          want_upstream_read,
                          want_upstream_write,
                          io_wait_timeout_ms,
                          ready);

        bool progressed = false;

        if (ready.client_readable && want_client_read) {
            const size_t before = client_to_upstream.size();
            if (!readInto(client, client_to_upstream, client_read_eof)) {
                return false;
            }
            progressed = progressed || (client_to_upstream.size() > before);
        }

        if (ready.upstream_readable && want_upstream_read) {
            const size_t before = upstream_to_client.size();
            if (!readInto(*upstream, upstream_to_client, upstream_read_eof)) {
                return false;
            }
            progressed = progressed || (upstream_to_client.size() > before);
        }

        const size_t before_upstream = client_to_upstream.size();
        if (ready.upstream_writable && want_upstream_write) {
            if (!flushOut(*upstream, client_to_upstream)) {
                return false;
            }
        }
        progressed = progressed || (client_to_upstream.size() < before_upstream);

        const size_t before_client = upstream_to_client.size();
        if (ready.client_writable && want_client_write) {
            if (!flushOut(client, upstream_to_client)) {
                return false;
            }
        }
        progressed = progressed || (upstream_to_client.size() < before_client);

        if (client_read_eof && client_to_upstream.empty() && !upstream_write_closed) {
            upstream->shutdown(false, true, nullptr);
            upstream_write_closed = true;
            progressed = true;
        }

        if (upstream_read_eof && upstream_to_client.empty() && !client_write_closed) {
            client.shutdown(false, true, nullptr);
            client_write_closed = true;
            progressed = true;
        }

        // If upstream has closed and both directions are drained, end proxy session
        // immediately. Waiting for client EOF can stall the manager accept loop.
        if (upstream_read_eof &&
            client_to_upstream.empty() &&
            upstream_to_client.empty()) {
            break;
        }

        if (client_read_eof &&
            upstream_read_eof &&
            client_to_upstream.empty() &&
            upstream_to_client.empty()) {
            break;
        }

        if (progressed) {
            last_progress = std::chrono::steady_clock::now();
            continue;
        }

        const auto now = std::chrono::steady_clock::now();
        if ((client_read_eof || upstream_read_eof) &&
            client_to_upstream.empty() &&
            upstream_to_client.empty() &&
            (now - last_progress) >= kDrainIdleLimit) {
            break;
        }
    }

    upstream->close();
    return true;
}

bool loadDbbtKeyRing(const ManagerOptions& options,
                     DatabaseBindingKeyRing& key_ring,
                     std::string& source,
                     ErrorContext* ctx) {
    if (!options.dbbt_keyring_path.empty()) {
        auto status = DatabaseBindingKeyRing::loadFromTextFile(
            options.dbbt_keyring_path, key_ring, ctx);
        if (status != Status::OK) {
            return false;
        }
        source = options.dbbt_keyring_path;
        return true;
    }

    const char* shared_hex = std::getenv("SCRATCHBIRD_DBBT_SHARED_KEY_HEX");
    if (shared_hex != nullptr && shared_hex[0] != '\0') {
        std::vector<uint8_t> key_bytes;
        if (!scratchbird::network::hexToBytes(shared_hex, key_bytes) ||
            !key_ring.addKey("env", key_bytes, true, ctx)) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid SCRATCHBIRD_DBBT_SHARED_KEY_HEX");
            return false;
        }
        source = "env:SCRATCHBIRD_DBBT_SHARED_KEY_HEX";
        return true;
    }

    constexpr const char* kDevFallbackHex =
        "73637261746368626972645f6465765f646262745f7368617265645f6b65795f7631";
    std::vector<uint8_t> fallback_key;
    if (!scratchbird::network::hexToBytes(kDevFallbackHex, fallback_key) ||
        !key_ring.addKey("default", fallback_key, true, ctx)) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to initialize default DBBT key");
        return false;
    }
    source = "builtin:default";
    return true;
}

bool sendListenerManagementCommand(const ManagerOptions& options,
                                   const std::string& command,
                                   std::string& response,
                                   ErrorContext* ctx) {
    const std::string socket_path = listenerManagementSocketPath(options);
    if (socket_path.empty()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Listener management socket unavailable");
        return false;
    }

    ErrorContext local_ctx;
    auto socket = Socket::connectUnix(socket_path, {}, &local_ctx);
    if (!socket) {
        SET_ERROR_CONTEXT(ctx, Status::CONNECTION_FAILURE,
                          local_ctx.message.empty() ? "Listener management connect failed"
                                                    : local_ctx.message.c_str());
        return false;
    }

    ControlPlaneMessage request;
    request.header.message_type =
        static_cast<uint16_t>(ControlPlaneMessageType::MANAGEMENT_COMMAND);
    request.header.request_id = 1;
    request.payload.assign(command.begin(), command.end());
    request.header.payload_len = request.payload.size();
    if (scratchbird::network::sendControlPlaneMessage(
            *socket, request, scratchbird::network::INVALID_SOCKET_VALUE, 0, &local_ctx) !=
        Status::OK) {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                          local_ctx.message.empty() ? "Listener management send failed"
                                                    : local_ctx.message.c_str());
        return false;
    }

    ControlPlaneMessage reply;
    if (scratchbird::network::receiveControlPlaneMessage(*socket, reply, nullptr, &local_ctx) !=
        Status::OK) {
        SET_ERROR_CONTEXT(ctx, Status::CONNECTION_FAILURE,
                          local_ctx.message.empty() ? "Listener management receive failed"
                                                    : local_ctx.message.c_str());
        return false;
    }
    if (reply.header.message_type !=
        static_cast<uint16_t>(ControlPlaneMessageType::MANAGEMENT_RESPONSE)) {
        SET_ERROR_CONTEXT(ctx, Status::PROTOCOL_VIOLATION,
                          "Unexpected listener management response type");
        return false;
    }

    if (reply.payload.empty()) {
        SET_ERROR_CONTEXT(ctx, Status::PROTOCOL_VIOLATION,
                          "Empty listener management response");
        return false;
    }
    response.assign(reinterpret_cast<const char*>(reply.payload.data() + 1),
                    reply.payload.size() - 1);
    return reply.payload[0] == 0;
}

bool hasSessionId(const ManagerSessionContext& session) {
    for (uint8_t b : session.session_id) {
        if (b != 0) {
            return true;
        }
    }
    return false;
}

Status sendWireMessage(Socket& socket, const Message& message, ErrorContext* ctx) {
    std::vector<uint8_t> wire_bytes;
    Status status = message.serialize(wire_bytes);
    if (status != Status::OK) {
        return status;
    }
    return socket.writeExact(wire_bytes.data(), wire_bytes.size(), ctx);
}

Status receiveWireMessage(Socket& socket, Message& message, ErrorContext* ctx) {
    uint8_t header_buf[sizeof(MessageHeader)];
    Status status = socket.readExact(header_buf, sizeof(header_buf), ctx);
    if (status != Status::OK) {
        return status;
    }

    MessageHeader header{};
    status = Message::parseHeader(header_buf, header, ctx);
    if (status != Status::OK) {
        return status;
    }

    message = Message(static_cast<MessageType>(header.type));
    message.setFlags(header.flags);
    if (header.payload_length > 0) {
        std::vector<uint8_t> payload(header.payload_length);
        status = socket.readExact(payload.data(), payload.size(), ctx);
        if (status != Status::OK) {
            return status;
        }
        message.setPayload(payload.data(), payload.size());
    }
    return Status::OK;
}

bool isInternalNativeReady(const ManagerOptions& options) {
    NetworkAddress address;
    address.family = addressFamilyForHost(options.native_bind);
    address.host = options.native_bind;
    address.port = options.native_port;
    ErrorContext ctx;
    auto probe = Socket::connect(address, scratchbird::network::SocketOptions{}, &ctx);
    if (!probe) {
        return false;
    }
    probe->close();
    return true;
}

Message buildMcpHelloResponse(const ManagerOptions& options,
                              uint16_t requested_version,
                              uint16_t client_flags) {
    std::vector<ProtocolCodec::StatusEntry> entries;
    entries.push_back({"mcp_version", std::to_string(scratchbird::protocol::MCP_PROTOCOL_VERSION)});
    entries.push_back({"requested_version", std::to_string(requested_version)});
    entries.push_back({"client_flags", std::to_string(client_flags)});
    entries.push_back({"auth_flow", "token_auth_start_auth_continue"});
    entries.push_back({"database_owner", options.owner_database});
    entries.push_back({"internal_native_endpoint", internalEndpointString(options)});
    entries.push_back({"ready", isInternalNativeReady(options) ? "true" : "false"});
    return ProtocolCodec::buildStatusResponse(StatusRequestType::SERVER_INFO, entries);
}

Message buildMcpDbListResponse(const ManagerOptions& options) {
    std::vector<ProtocolCodec::StatusEntry> entries;
    entries.push_back({"count", "1"});
    entries.push_back({"db.0", options.owner_database});
    entries.push_back({"default_db", options.owner_database});
    entries.push_back({"internal_native_endpoint", internalEndpointString(options)});
    return ProtocolCodec::buildStatusResponse(StatusRequestType::DATABASE_INFO, entries);
}

Message buildMcpDbInfoResponse(const ManagerOptions& options,
                               const std::string& database_name) {
    const bool ready = isInternalNativeReady(options);
    std::vector<ProtocolCodec::StatusEntry> entries;
    entries.push_back({"db", database_name});
    entries.push_back({"available", (database_name == options.owner_database) ? "true" : "false"});
    entries.push_back({"state", ready ? "OPEN" : "CLOSED"});
    entries.push_back({"ready", ready ? "true" : "false"});
    entries.push_back({"internal_native_endpoint", internalEndpointString(options)});
    return ProtocolCodec::buildStatusResponse(StatusRequestType::DATABASE_INFO, entries);
}

Message buildMcpResponseForRequest(const Message& request,
                                   const ManagerOptions& options,
                                   const DatabaseBindingKeyRing& dbbt_key_ring,
                                   ManagerSessionContext& session_ctx) {
    auto buildNotReadyResponse = [&session_ctx]() -> Message {
        if (!hasSessionId(session_ctx)) {
            scratchbird::protocol::generateSessionId(session_ctx.session_id);
        }
        return ProtocolCodec::buildConnectResponse(
            false, session_ctx.session_id, "Internal native listener is not ready");
    };

    switch (request.getType()) {
        case MessageType::MCP_HELLO: {
            uint16_t requested_version = 0;
            uint16_t client_flags = 0;
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseMcpHello(
                    request, requested_version, client_flags, &parse_ctx) != Status::OK) {
                return ProtocolCodec::buildProtocolError(parse_ctx.message);
            }
            return buildMcpHelloResponse(options, requested_version, client_flags);
        }

        case MessageType::MCP_AUTH_START: {
            std::string username;
            AuthMethod auth_method = AuthMethod::PASSWORD;
            std::vector<uint8_t> initial_payload;
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseMcpAuthStart(
                    request, username, auth_method, initial_payload, &parse_ctx) != Status::OK) {
                return ProtocolCodec::buildProtocolError(parse_ctx.message);
            }
            if (username.empty()) {
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "MCP_AUTH_START requires username");
            }
            if (auth_method != AuthMethod::TOKEN) {
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "MCP_AUTH_START requires auth_method=TOKEN");
            }

            session_ctx.auth_started = true;
            session_ctx.authenticated = false;
            session_ctx.auth_method = auth_method;
            session_ctx.username = username;
            scratchbird::protocol::generateSessionId(session_ctx.session_id);

            // Fast path: if caller already supplied the manager token as initial payload,
            // complete authentication in a single round-trip.
            if (!initial_payload.empty()) {
                const std::vector<uint8_t> expected_secret(
                    options.mcp_auth_secret.begin(), options.mcp_auth_secret.end());
                if (timingSafeBytesEqual(initial_payload, expected_secret)) {
                    session_ctx.authenticated = true;
                    return ProtocolCodec::buildAuthResponse(AuthStatus::OK, 0, "");
                }
            }

            std::vector<AuthMethod> allowed_methods = {
                AuthMethod::TOKEN
            };

            std::vector<uint8_t> nonce(session_ctx.session_id,
                                       session_ctx.session_id + sizeof(session_ctx.session_id));
            return ProtocolCodec::buildAuthChallenge(
                session_ctx.session_id,
                username,
                allowed_methods,
                true,
                AuthMethod::TOKEN,
                0x01,
                nonce);
        }

        case MessageType::MCP_AUTH_CONTINUE: {
            std::vector<uint8_t> continuation_payload;
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseMcpAuthContinue(
                    request, continuation_payload, &parse_ctx) != Status::OK) {
                return ProtocolCodec::buildProtocolError(parse_ctx.message);
            }
            if (!session_ctx.auth_started) {
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "MCP_AUTH_START is required before MCP_AUTH_CONTINUE");
            }
            if (continuation_payload.empty()) {
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "MCP_AUTH_CONTINUE payload must not be empty");
            }
            if (session_ctx.auth_method != AuthMethod::TOKEN) {
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "MCP_AUTH_CONTINUE method mismatch");
            }
            const std::vector<uint8_t> expected_secret(
                options.mcp_auth_secret.begin(), options.mcp_auth_secret.end());
            if (!timingSafeBytesEqual(continuation_payload, expected_secret)) {
                session_ctx.authenticated = false;
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "MCP authentication failed");
            }
            session_ctx.authenticated = true;
            return ProtocolCodec::buildAuthResponse(AuthStatus::OK, 0, "");
        }

        case MessageType::MCP_DB_LIST: {
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseMcpDbList(request, &parse_ctx) != Status::OK) {
                return ProtocolCodec::buildProtocolError(parse_ctx.message);
            }
            return buildMcpDbListResponse(options);
        }

        case MessageType::MCP_DB_INFO: {
            std::string requested_database;
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseMcpDbInfo(request, requested_database, &parse_ctx) !=
                Status::OK) {
                return ProtocolCodec::buildProtocolError(parse_ctx.message);
            }
            if (!session_ctx.authenticated) {
                return ProtocolCodec::buildAuthResponse(
                    AuthStatus::ERROR, 0, "Authenticate before MCP_DB_INFO");
            }
            if (requested_database.empty()) {
                requested_database = options.owner_database;
            }
            return buildMcpDbInfoResponse(options, requested_database);
        }

        case MessageType::MCP_DB_CONNECT: {
            std::string requested_database;
            std::string requested_profile;
            std::string client_intent;
            std::vector<uint8_t> client_nonce;
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseMcpDbConnect(
                    request,
                    requested_database,
                    requested_profile,
                    client_intent,
                    client_nonce,
                    &parse_ctx) !=
                Status::OK) {
                return ProtocolCodec::buildProtocolError(parse_ctx.message);
            }
            if (requested_database.empty()) {
                requested_database = options.owner_database;
            }
            if (!client_intent.empty() && client_intent != "native_v3") {
                if (!hasSessionId(session_ctx)) {
                    scratchbird::protocol::generateSessionId(session_ctx.session_id);
                }
                return ProtocolCodec::buildConnectResponse(
                    false, session_ctx.session_id, "Unsupported MCP client intent: " + client_intent);
            }
            if (!client_nonce.empty() &&
                (client_nonce.size() < 16 || client_nonce.size() > 32)) {
                if (!hasSessionId(session_ctx)) {
                    scratchbird::protocol::generateSessionId(session_ctx.session_id);
                }
                return ProtocolCodec::buildConnectResponse(
                    false,
                    session_ctx.session_id,
                    "MCP_DB_CONNECT client_nonce must be 16..32 bytes");
            }
            if (!session_ctx.authenticated) {
                if (!hasSessionId(session_ctx)) {
                    scratchbird::protocol::generateSessionId(session_ctx.session_id);
                }
                return ProtocolCodec::buildConnectResponse(
                    false, session_ctx.session_id, "Authenticate before MCP_DB_CONNECT");
            }
            if (requested_database != options.owner_database) {
                if (!hasSessionId(session_ctx)) {
                    scratchbird::protocol::generateSessionId(session_ctx.session_id);
                }
                return ProtocolCodec::buildConnectResponse(
                    false, session_ctx.session_id,
                    "Database not available in manager_proxy scope: " + requested_database);
            }

            session_ctx.prepared_upstream.reset();

            // If no listener management channel is configured, route directly to proxy mode
            // without DBBT/LPREFACE validation.
            if (options.control_socket_dir.empty()) {
                ErrorContext connect_ctx;
                session_ctx.prepared_upstream = connectInternalNative(options, &connect_ctx);
                if (!session_ctx.prepared_upstream) {
                    return buildNotReadyResponse();
                }
                session_ctx.last_dbbt.clear();
                session_ctx.last_dbbt_id.clear();
                session_ctx.last_dbbt_expires_at_ms = 0;
                session_ctx.selected_database = requested_database;
                if (!hasSessionId(session_ctx)) {
                    scratchbird::protocol::generateSessionId(session_ctx.session_id);
                }
                return ProtocolCodec::buildConnectResponse(
                    true,
                    session_ctx.session_id,
                    "",
                    static_cast<uint16_t>(scratchbird::protocol::CONNECT_FLAG_BASE_CAPABILITIES));
            }

            DatabaseBindingToken dbbt;
            dbbt.db_uuid = deriveDatabaseUuid(requested_database);
            dbbt.listener_id = options.listener_id;
            dbbt.issued_at_ms = scratchbird::network::currentEpochMillis();
            dbbt.expires_at_ms = dbbt.issued_at_ms + options.dbbt_ttl_ms;
            std::memcpy(dbbt.manager_session_id.data(),
                        session_ctx.session_id,
                        dbbt.manager_session_id.size());
            dbbt.client_nonce = client_nonce.empty() ? randomBytes(16) : client_nonce;
            dbbt.server_nonce = randomBytes(16);
            dbbt.flags = 0;

            ErrorContext dbbt_ctx;
            if (dbbt_key_ring.sign(dbbt, &dbbt_ctx) != Status::OK) {
                logManagedAuditEvent("MANAGED_DBBT_ISSUED",
                                     false,
                                     requested_database,
                                     options.listener_id,
                                     dbbt_ctx.message.empty() ? "dbbt_sign_failed"
                                                              : dbbt_ctx.message);
                return ProtocolCodec::buildConnectResponse(
                    false, session_ctx.session_id,
                    dbbt_ctx.message.empty() ? "Failed to sign DBBT" : dbbt_ctx.message);
            }

            std::vector<uint8_t> encoded_dbbt;
            if (!scratchbird::network::encodeDatabaseBindingToken(dbbt, encoded_dbbt, &dbbt_ctx)) {
                logManagedAuditEvent("MANAGED_DBBT_ISSUED",
                                     false,
                                     requested_database,
                                     options.listener_id,
                                     dbbt_ctx.message.empty() ? "dbbt_encode_failed"
                                                              : dbbt_ctx.message);
                return ProtocolCodec::buildConnectResponse(
                    false, session_ctx.session_id,
                    dbbt_ctx.message.empty() ? "Failed to encode DBBT" : dbbt_ctx.message);
            }
            const std::vector<uint8_t> dbbt_id = scratchbird::network::databaseBindingTokenId(dbbt);
            logManagedAuditEvent("MANAGED_DBBT_ISSUED",
                                 true,
                                 requested_database,
                                 options.listener_id,
                                 "issued",
                                 dbbt_id);

            scratchbird::network::ListenerPrefaceV1 preface;
            preface.listener_id = options.listener_id;
            preface.dbbt = encoded_dbbt;
            preface.db_selector = requested_database;
            preface.requested_profile = requested_profile.empty()
                ? "native_v3"
                : requested_profile;
            preface.flags = 0;

            std::vector<uint8_t> encoded_preface;
            if (!scratchbird::network::encodeListenerPrefaceV1(preface, encoded_preface,
                                                                &dbbt_ctx)) {
                return ProtocolCodec::buildConnectResponse(
                    false, session_ctx.session_id,
                    dbbt_ctx.message.empty() ? "Failed to encode LPREFACE"
                                             : dbbt_ctx.message);
            }

            if (!options.control_socket_dir.empty()) {
                const std::string validate_command =
                    std::string("LPREFACE_VALIDATE ") +
                    scratchbird::network::bytesToHex(encoded_preface);
                std::string validate_response;
                if (!sendListenerManagementCommand(options, validate_command, validate_response,
                                                   &dbbt_ctx)) {
                    const std::string reason = validate_response.empty()
                        ? (dbbt_ctx.message.empty() ? "Listener DBBT validation failed"
                                                    : dbbt_ctx.message)
                        : validate_response;
                    logManagedAuditEvent("MANAGED_PREFACE_DECISION",
                                         false,
                                         requested_database,
                                         options.listener_id,
                                         reason,
                                         dbbt_id);
                    return ProtocolCodec::buildConnectResponse(
                        false, session_ctx.session_id, reason);
                }
                if (validate_response.rfind("lpreface_ack", 0) != 0) {
                    const std::string reason = "Listener LPREFACE rejected: " + validate_response;
                    logManagedAuditEvent("MANAGED_PREFACE_DECISION",
                                         false,
                                         requested_database,
                                         options.listener_id,
                                         reason,
                                         dbbt_id);
                    return ProtocolCodec::buildConnectResponse(
                        false, session_ctx.session_id, reason);
                }
                logManagedAuditEvent("MANAGED_PREFACE_DECISION",
                                     true,
                                     requested_database,
                                     options.listener_id,
                                     "accepted",
                                     dbbt_id);
            }

            session_ctx.last_dbbt = std::move(encoded_dbbt);
            session_ctx.last_dbbt_id = scratchbird::network::databaseBindingTokenId(dbbt);
            session_ctx.last_dbbt_expires_at_ms = dbbt.expires_at_ms;
            session_ctx.selected_database = requested_database;
            ErrorContext connect_ctx;
            session_ctx.prepared_upstream = connectInternalNative(options, &connect_ctx);
            if (!session_ctx.prepared_upstream) {
                session_ctx.last_dbbt.clear();
                session_ctx.last_dbbt_id.clear();
                session_ctx.last_dbbt_expires_at_ms = 0;
                return buildNotReadyResponse();
            }
            if (!hasSessionId(session_ctx)) {
                scratchbird::protocol::generateSessionId(session_ctx.session_id);
            }
            return ProtocolCodec::buildConnectResponse(
                true,
                session_ctx.session_id,
                "",
                static_cast<uint16_t>(
                    scratchbird::protocol::CONNECT_FLAG_BASE_CAPABILITIES |
                    scratchbird::protocol::CONNECT_FLAG_MANAGER_DBBT));
        }

        default:
            return ProtocolCodec::buildProtocolError(
                std::string("Unsupported MCP message type: ") +
                scratchbird::protocol::messageTypeToString(request.getType()));
    }
}

void handleClient(Socket& client,
                  const ManagerOptions& options,
                  const DatabaseBindingKeyRing& dbbt_key_ring) {
    ManagerSessionContext session_ctx;

    while (g_running.load(std::memory_order_acquire) && client.isOpen()) {
        if (!client.waitReadable(200)) {
            continue;
        }

        Message request;
        ErrorContext recv_ctx;
        Status recv_status = receiveWireMessage(client, request, &recv_ctx);
        if (recv_status != Status::OK) {
            break;
        }

        if (request.getType() == MessageType::DISCONNECT) {
            break;
        }
        if (request.getType() == MessageType::SHUTDOWN) {
            g_running.store(false, std::memory_order_release);
            break;
        }

        Message response = buildMcpResponseForRequest(request, options, dbbt_key_ring, session_ctx);
        ErrorContext send_ctx;
        if (sendWireMessage(client, response, &send_ctx) != Status::OK) {
            break;
        }

        if (request.getType() == MessageType::MCP_DB_CONNECT) {
            bool connect_success = false;
            uint8_t upstream_session_id[16] = {0};
            std::string connect_error;
            ErrorContext parse_ctx;
            if (ProtocolCodec::parseConnectResponse(
                    response, connect_success, upstream_session_id, connect_error, nullptr,
                    &parse_ctx) == Status::OK &&
                connect_success) {
                ErrorContext proxy_ctx;
                runProxyTransport(client,
                                  options,
                                  std::move(session_ctx.prepared_upstream),
                                  &proxy_ctx);
                break;
            }
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    ManagerOptions options;
    if (!parseArgs(argc, argv, options)) {
        return 1;
    }

    DatabaseBindingKeyRing dbbt_key_ring;
    ErrorContext keyring_ctx;
    std::string keyring_source;
    if (!loadDbbtKeyRing(options, dbbt_key_ring, keyring_source, &keyring_ctx)) {
        std::cerr << "Manager DBBT keyring init failed: " << keyring_ctx.message << "\n";
        return 1;
    }
    (void)keyring_source;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    AddressFamily family = addressFamilyForHost(options.bind_address);
    ErrorContext socket_ctx;
    auto listener = Socket::create(family, SocketType::STREAM, &socket_ctx);
    if (!listener) {
        std::cerr << "Manager socket create failed: " << socket_ctx.message << "\n";
        return 1;
    }

    listener->setReuseAddress(true, nullptr);
    NetworkAddress listen_address(options.bind_address, options.port, family);
    if (listener->bind(listen_address, &socket_ctx) != Status::OK) {
        std::cerr << "Manager bind failed on " << listen_address.toString()
                  << ": " << socket_ctx.message << "\n";
        return 1;
    }
    if (listener->listen(64, &socket_ctx) != Status::OK) {
        std::cerr << "Manager listen failed on " << listen_address.toString()
                  << ": " << socket_ctx.message << "\n";
        return 1;
    }
    listener->setNonBlocking(true, nullptr);

    while (g_running.load(std::memory_order_acquire)) {
        if (!listener->waitReadable(200)) {
            continue;
        }

        ErrorContext accept_ctx;
        auto client = listener->accept(nullptr, &accept_ctx);
        if (!client) {
            continue;
        }

        client->setReadTimeout(2000, nullptr);
        client->setWriteTimeout(2000, nullptr);
        handleClient(*client, options, dbbt_key_ring);
        client->close();
    }

    listener->close();
    return 0;
}
