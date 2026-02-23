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
 * ScratchBird Parser Agent - Protocol-Specific Entry Point
 *
 * Scaffolding for parser agent binaries per dialect. The control-plane
 * socket and engine IPC wiring are tracked separately.
 */

#include <atomic>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <cctype>

#include "scratchbird/core/error_context.h"
#include "scratchbird/security/tls_config.h"
#include "scratchbird/network/control_plane.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/connection_handler.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"
#include "scratchbird/server/config_parser.h"
#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/version.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include "scratchbird/core/posix_compat.h"
#endif

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
    std::string default_database;
    std::string tls_config;
    scratchbird::security::TLSConfig tls_settings;
    std::shared_ptr<scratchbird::security::TLSContext> tls_context;
    std::string log_level = "info";
    uint32_t protocol_version = 1;
    uint32_t max_requests = 0;
    uint32_t max_age_seconds = 0;
    bool show_help = false;
    bool show_version = false;
};

struct HandoffInfo {
    uint64_t connection_id = 0;
    std::string protocol;
    std::string client_addr;
    uint16_t client_port = 0;
    bool tls_active = false;
    std::vector<uint8_t> initial_bytes;
    std::array<uint8_t, 16> db_uuid{};
    std::array<uint8_t, 16> dbbt_id{};
    std::array<uint8_t, 16> manager_session_id{};
    uint32_t listener_id = 0;
    bool has_binding_context = false;
};

bool tryParseProtocolType(const std::string& protocol,
                          scratchbird::network::ProtocolType& out);

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
              << "  --default-database <name> Bound/default database for engine attach\n"
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
        } else if (arg == "--default-database" && i + 1 < argc) {
            config.default_database = argv[++i];
        } else if (arg.rfind("--default-database=", 0) == 0) {
            config.default_database = arg.substr(19);
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
    scratchbird::network::ProtocolType protocol_type =
        scratchbird::network::ProtocolType::AUTO_DETECT;
    if (!tryParseProtocolType(config.protocol, protocol_type)) {
        std::cerr << "Unsupported parser protocol: " << config.protocol << "\n";
        return false;
    }
    return true;
}

bool loadTLSSettings(const std::string& path,
                     scratchbird::security::TLSConfig& out,
                     std::string& error) {
    scratchbird::core::ErrorContext ctx;
    scratchbird::server::ConfigParser parser;
    if (parser.parseFile(path, &ctx) != scratchbird::core::Status::OK) {
        error = ctx.message.empty() ? "Failed to parse TLS config" : ctx.message;
        return false;
    }

    const auto* ssl = parser.section("ssl");
    if (!ssl) {
        return true;
    }

    out.enabled = ssl->getBool("enabled", out.enabled);
    out.cert_file = ssl->getString("cert_file", out.cert_file);
    out.key_file = ssl->getString("key_file", out.key_file);
    out.key_password = ssl->getString("key_password", out.key_password);
    out.ca_file = ssl->getString("ca_file", out.ca_file);
    out.ca_path = ssl->getString("ca_path", out.ca_path);
    out.ciphers = ssl->getString("ciphers", out.ciphers);
    out.ciphersuites = ssl->getString("ciphersuites", out.ciphersuites);
    out.ecdh_curve = ssl->getString("ecdh_curve", out.ecdh_curve);
    out.dh_params_file = ssl->getString("dh_params_file", out.dh_params_file);
    out.verify_depth = static_cast<int>(ssl->getInt("verify_depth", out.verify_depth));

    if (ssl->getBool("require_client_cert", false)) {
        out.verify_mode = scratchbird::security::VerifyMode::REQUIRE_MODE;
    }

    std::string min_proto = ssl->getString("min_protocol", "");
    if (!min_proto.empty()) {
        scratchbird::security::TLSVersion version;
        if (scratchbird::security::parseTLSVersion(min_proto, version)) {
            out.min_version = version;
        }
    }
    std::string max_proto = ssl->getString("max_protocol", "");
    if (max_proto.empty()) {
        max_proto = ssl->getString("preferred_protocol", "");
    }
    if (!max_proto.empty()) {
        scratchbird::security::TLSVersion version;
        if (scratchbird::security::parseTLSVersion(max_proto, version)) {
            out.max_version = version;
        }
    }

    return true;
}

uint64_t makeWorkerId() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t ticks = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
#ifdef _WIN32
    uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
    uint32_t pid = static_cast<uint32_t>(getpid());
#endif
    return (ticks << 16) ^ pid;
}

void appendU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void appendU64(std::vector<uint8_t>& out, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

uint16_t readU16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

uint32_t readU32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0])
        | (static_cast<uint32_t>(data[1]) << 8)
        | (static_cast<uint32_t>(data[2]) << 16)
        | (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t readU64(const uint8_t* data) {
    uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value = (value << 8) | data[i];
    }
    return value;
}

std::vector<uint8_t> buildHelloPayload(const ParserConfig& config,
                                       uint64_t worker_id,
                                       uint32_t pid) {
    std::vector<uint8_t> payload;
    payload.resize(16);
    std::memset(payload.data(), 0, payload.size());
    std::memcpy(payload.data(), config.protocol.c_str(),
                std::min<size_t>(config.protocol.size(), 15));
    appendU32(payload, pid);
    appendU64(payload, worker_id);
    appendU32(payload, config.protocol_version);
    return payload;
}

bool parseHelloAck(const scratchbird::network::ControlPlaneMessage& msg, std::string& reason) {
    if (msg.payload.empty()) {
        reason = "Empty HELLO_ACK";
        return false;
    }
    uint8_t accepted = msg.payload[0];
    if (accepted != 1) {
        if (msg.payload.size() >= 3) {
            uint16_t len = readU16(msg.payload.data() + 1);
            if (msg.payload.size() >= 3 + len) {
                reason.assign(reinterpret_cast<const char*>(msg.payload.data() + 3), len);
            }
        }
        return false;
    }
    return true;
}

std::vector<uint8_t> buildHealthReport(uint64_t worker_id, uint8_t state,
                                       uint16_t active_sessions, uint32_t last_error) {
    std::vector<uint8_t> payload;
    appendU64(payload, worker_id);
    payload.push_back(state);
    appendU16(payload, active_sessions);
    appendU32(payload, last_error);
    return payload;
}

std::vector<uint8_t> buildHandoffAck(uint64_t connection_id, uint8_t status) {
    std::vector<uint8_t> payload;
    appendU64(payload, connection_id);
    payload.push_back(status);
    appendU16(payload, 0);
    return payload;
}

bool parseHandoffPayload(const scratchbird::network::ControlPlaneMessage& msg,
                         HandoffInfo& info) {
    constexpr size_t kHeaderSize = 8 + 16 + 48 + 2 + 1 + 2;
    constexpr size_t kLegacyReservedTailBytes = 64;
    constexpr size_t kBindingTailBytes = 16 + 16 + 16 + 4;
    if (msg.payload.size() < kHeaderSize) {
        return false;
    }
    size_t offset = 0;
    info.connection_id = readU64(msg.payload.data());
    offset += 8;
    size_t proto_len = 0;
    while (proto_len < 16 && msg.payload[offset + proto_len] != 0) {
        ++proto_len;
    }
    info.protocol.assign(reinterpret_cast<const char*>(msg.payload.data() + offset), proto_len);
    if (info.protocol.empty()) {
        return false;
    }
    offset += 16;
    size_t addr_len = 0;
    while (addr_len < 48 && msg.payload[offset + addr_len] != 0) {
        ++addr_len;
    }
    info.client_addr.assign(reinterpret_cast<const char*>(msg.payload.data() + offset), addr_len);
    offset += 48;
    info.client_port = readU16(msg.payload.data() + offset);
    offset += 2;
    info.tls_active = (msg.payload[offset] != 0);
    offset += 1;
    uint16_t initial_len = readU16(msg.payload.data() + offset);
    offset += 2;
    if (msg.payload.size() < offset + initial_len) {
        return false;
    }
    if (initial_len > 0) {
        info.initial_bytes.assign(msg.payload.begin() + offset,
                                  msg.payload.begin() + offset + initial_len);
    }
    offset += initial_len;

    // Legacy listeners append a 64-byte reserved tail. New listeners may append
    // immutable manager binding context after the reserved block.
    if (msg.payload.size() == offset) {
        return true;
    }
    if (msg.payload.size() < offset + kLegacyReservedTailBytes) {
        return false;
    }
    offset += kLegacyReservedTailBytes;

    if (msg.payload.size() == offset) {
        return true;
    }
    if (msg.payload.size() < offset + kBindingTailBytes) {
        return false;
    }

    std::memcpy(info.db_uuid.data(), msg.payload.data() + offset, info.db_uuid.size());
    offset += info.db_uuid.size();
    std::memcpy(info.dbbt_id.data(), msg.payload.data() + offset, info.dbbt_id.size());
    offset += info.dbbt_id.size();
    std::memcpy(info.manager_session_id.data(),
                msg.payload.data() + offset,
                info.manager_session_id.size());
    offset += info.manager_session_id.size();
    info.listener_id = readU32(msg.payload.data() + offset);
    info.has_binding_context = true;
    return true;
}

void closeSocketFd(scratchbird::network::socket_t fd) {
#ifdef _WIN32
    if (fd != scratchbird::network::INVALID_SOCKET_VALUE) {
        closesocket(fd);
    }
#else
    if (fd >= 0) {
        ::close(fd);
    }
#endif
}

scratchbird::network::AddressFamily detectAddressFamily(scratchbird::network::socket_t fd) {
    sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&ss), &len) != 0) {
        return scratchbird::network::AddressFamily::IPV4;
    }
    switch (ss.ss_family) {
        case AF_INET:
            return scratchbird::network::AddressFamily::IPV4;
        case AF_INET6:
            return scratchbird::network::AddressFamily::IPV6;
#ifndef _WIN32
        case AF_UNIX:
            return scratchbird::network::AddressFamily::UNIX;
#endif
        default:
            return scratchbird::network::AddressFamily::IPV4;
    }
}

std::string normalizeProtocolString(const std::string& protocol) {
    std::string value = protocol;
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

bool tryParseProtocolType(const std::string& protocol,
                          scratchbird::network::ProtocolType& out) {
    const std::string value = normalizeProtocolString(protocol);
    if (value == "postgresql" || value == "postgres" || value == "pg") {
        out = scratchbird::network::ProtocolType::POSTGRESQL;
        return true;
    }
    if (value == "mysql") {
        out = scratchbird::network::ProtocolType::MYSQL;
        return true;
    }
    if (value == "firebird" || value == "firebirdsql") {
        out = scratchbird::network::ProtocolType::FIREBIRD;
        return true;
    }
    if (value == "native" || value == "scratchbird" || value == "sbwp") {
        out = scratchbird::network::ProtocolType::NATIVE;
        return true;
    }
    return false;
}

bool flushWrites(scratchbird::network::Connection& conn) {
    while (conn.hasPendingWrites()) {
        auto bytes = conn.writeFromBuffer();
        if (bytes < 0) {
            return false;
        }
        if (bytes == 0) {
            auto* socket = conn.getSocket();
            if (socket && !socket->waitWritable(1000)) {
                return false;
            }
        }
    }
    return true;
}

uint32_t runSession(const ParserConfig& config,
                    const HandoffInfo& info,
                    scratchbird::network::socket_t client_fd) {
    auto family = detectAddressFamily(client_fd);
    auto socket = scratchbird::network::Socket::fromFd(
        client_fd, family, scratchbird::network::SocketType::STREAM);
    if (!socket) {
        closeSocketFd(client_fd);
        return static_cast<uint32_t>(scratchbird::core::Status::INTERNAL_ERROR);
    }

    if (config.tls_context && config.tls_settings.enabled) {
        scratchbird::core::ErrorContext tls_ctx;
        auto status = socket->startTLS(*config.tls_context, &tls_ctx);
        if (status != scratchbird::core::Status::OK) {
            std::cerr << "[parser_debug] TLS handshake failed: " << tls_ctx.message << "\n";
            return static_cast<uint32_t>(status);
        }
    }

    scratchbird::network::Connection conn(std::move(socket),
                                          static_cast<scratchbird::network::ConnectionId>(
                                              info.connection_id));

    auto hasNonZeroBytes = [](const auto& bytes) {
        return std::any_of(bytes.begin(), bytes.end(), [](uint8_t value) {
            return value != 0;
        });
    };

    scratchbird::network::ProtocolType protocol_type =
        scratchbird::network::ProtocolType::AUTO_DETECT;
    if (!tryParseProtocolType(config.protocol, protocol_type)) {
        conn.close(scratchbird::network::CloseReason::PROTOCOL_ERROR);
        return static_cast<uint32_t>(scratchbird::core::Status::NOT_SUPPORTED);
    }
    conn.setProtocol(protocol_type);

    scratchbird::protocol::ProtocolAdapterConfig adapter_config;
    adapter_config.engine_endpoint = config.engine_endpoint;
    adapter_config.default_database = config.default_database;
    const bool has_db_uuid_binding =
        info.has_binding_context && hasNonZeroBytes(info.db_uuid);
    const bool manager_bound =
        info.has_binding_context &&
        (hasNonZeroBytes(info.dbbt_id) || hasNonZeroBytes(info.manager_session_id));
    if (has_db_uuid_binding) {
        adapter_config.enforce_bound_database = true;
        adapter_config.connect_client_flags |=
            scratchbird::protocol::CONNECT_FLAG_BOUND_DB_UUID;
        adapter_config.has_bound_db_uuid = true;
        adapter_config.bound_db_uuid = info.db_uuid;
    }
    if (manager_bound) {
        adapter_config.connect_client_flags |= scratchbird::protocol::CONNECT_FLAG_MANAGER_DBBT;
    }
    auto adapter = scratchbird::protocol::createProtocolAdapter(protocol_type, adapter_config);
    if (!adapter) {
        conn.close(scratchbird::network::CloseReason::PROTOCOL_ERROR);
        return static_cast<uint32_t>(scratchbird::core::Status::NOT_SUPPORTED);
    }

    auto status = adapter->initializeConnection(&conn);
    if (status != scratchbird::core::Status::OK) {
        conn.close(scratchbird::network::CloseReason::PROTOCOL_ERROR);
        return static_cast<uint32_t>(status);
    }
    if (!flushWrites(conn)) {
        conn.close(scratchbird::network::CloseReason::IO_ERROR);
        return static_cast<uint32_t>(scratchbird::core::Status::IO_ERROR);
    }

    if (!info.initial_bytes.empty()) {
        auto& buffer = conn.getReadBuffer();
        buffer.insert(buffer.end(), info.initial_bytes.begin(), info.initial_bytes.end());
    }

    bool need_read = info.initial_bytes.empty();
    while (!g_shutdown.load(std::memory_order_acquire) && conn.isOpen()) {
        if (need_read) {
            auto bytes = conn.readIntoBuffer();
            if (bytes < 0) {
                conn.close(scratchbird::network::CloseReason::IO_ERROR);
                return static_cast<uint32_t>(scratchbird::core::Status::IO_ERROR);
            }
            if (bytes == 0) {
                if (conn.isClosing()) {
                    break;
                }
                continue;
            }
        }

        status = adapter->handleData(&conn);
        if (!flushWrites(conn)) {
            conn.close(scratchbird::network::CloseReason::IO_ERROR);
            return static_cast<uint32_t>(scratchbird::core::Status::IO_ERROR);
        }

        if (status == scratchbird::core::Status::OK) {
            need_read = true;
            continue;
        }
        if (status == scratchbird::core::Status::IO_ERROR) {
            need_read = true;
            continue;
        }
        std::cerr << "[parser_debug] handleData status=" << static_cast<int>(status) << "\n";

        adapter->sendError(&conn, "Protocol error");
        flushWrites(conn);
        conn.close(scratchbird::network::CloseReason::PROTOCOL_ERROR);
        return static_cast<uint32_t>(status);
    }

    return 0;
}

int runParser(ParserConfig& config) {
    std::cout << SB_PARSER_NAME << " starting\n"
              << "Protocol: " << config.protocol << "\n"
              << "Control socket: " << config.control_socket << "\n"
              << "Engine endpoint: " << config.engine_endpoint << "\n";
    if (!config.default_database.empty()) {
        std::cout << "Default database: " << config.default_database << "\n";
    }

    if (!config.tls_config.empty()) {
        std::cout << "TLS config: " << config.tls_config << "\n";
    }

    if (!scratchbird::network::initNetwork()) {
        std::cerr << "Failed to initialize network subsystem\n";
        return 2;
    }

    if (!config.tls_config.empty()) {
        std::string error;
        if (!loadTLSSettings(config.tls_config, config.tls_settings, error)) {
            std::cerr << "Failed to load TLS settings: " << error << "\n";
            return 2;
        }
        if (config.tls_settings.enabled) {
            scratchbird::core::ErrorContext tls_ctx;
            auto tls_context = scratchbird::security::TLSContext::createServer(
                config.tls_settings, &tls_ctx);
            if (!tls_context || !tls_context->isValid()) {
                std::cerr << "Failed to initialize TLS context: " << tls_ctx.message << "\n";
                return 2;
            }
            config.tls_context = std::move(tls_context);
        }
    }

    scratchbird::core::ErrorContext ctx;
    auto control = scratchbird::network::Socket::connectUnix(config.control_socket, {}, &ctx);
    if (!control) {
        std::cerr << "Failed to connect control socket: " << ctx.message << "\n";
        return 2;
    }

    uint64_t worker_id = makeWorkerId();
#ifdef _WIN32
    uint32_t pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
    uint32_t pid = static_cast<uint32_t>(getpid());
#endif

    scratchbird::network::ControlPlaneMessage hello;
    hello.header.message_type = static_cast<uint16_t>(
        scratchbird::network::ControlPlaneMessageType::HELLO);
    hello.header.request_id = 1;
    hello.payload = buildHelloPayload(config, worker_id, pid);
    hello.header.payload_len = hello.payload.size();

    if (scratchbird::network::sendControlPlaneMessage(*control, hello,
                                                      scratchbird::network::INVALID_SOCKET_VALUE,
                                                      0, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Failed to send HELLO: " << ctx.message << "\n";
        return 2;
    }

    scratchbird::network::ControlPlaneMessage response;
    if (scratchbird::network::receiveControlPlaneMessage(*control, response,
                                                         nullptr, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Failed to read HELLO_ACK: " << ctx.message << "\n";
        return 2;
    }

    if (response.header.message_type != static_cast<uint16_t>(
            scratchbird::network::ControlPlaneMessageType::HELLO_ACK)) {
        std::cerr << "Expected HELLO_ACK\n";
        return 2;
    }
    std::string reason;
    if (!parseHelloAck(response, reason)) {
        std::cerr << "HELLO rejected: " << reason << "\n";
        return 2;
    }
    std::cerr << "[parser_debug] HELLO_ACK accepted worker_id=" << worker_id << "\n";

    bool busy = false;
    while (!g_shutdown.load(std::memory_order_acquire)) {
        scratchbird::network::ControlPlaneMessage msg;
        scratchbird::network::socket_t recv_fd = scratchbird::network::INVALID_SOCKET_VALUE;
        auto status = scratchbird::network::receiveControlPlaneMessage(*control, msg,
                                                                       &recv_fd, &ctx);
        if (status != scratchbird::core::Status::OK) {
            std::cerr << "Control receive failed: " << ctx.message << "\n";
            break;
        }

        auto type = static_cast<scratchbird::network::ControlPlaneMessageType>(
            msg.header.message_type);
        std::cerr << "[parser_debug] control msg type=" << static_cast<int>(type)
                  << " req=" << msg.header.request_id
                  << " recv_fd=" << recv_fd << "\n";

        if (type == scratchbird::network::ControlPlaneMessageType::HEALTH_CHECK) {
            scratchbird::network::ControlPlaneMessage report;
            report.header.message_type = static_cast<uint16_t>(
                scratchbird::network::ControlPlaneMessageType::HEALTH_REPORT);
            report.header.request_id = msg.header.request_id;
            uint8_t state = busy ? 1 : 0;
            report.payload = buildHealthReport(worker_id, state, busy ? 1 : 0, 0);
            report.header.payload_len = report.payload.size();
            scratchbird::network::sendControlPlaneMessage(*control, report,
                                                          scratchbird::network::INVALID_SOCKET_VALUE,
                                                          0, nullptr);
            continue;
        }

        if (type == scratchbird::network::ControlPlaneMessageType::HANDOFF_SOCKET) {
            HandoffInfo info;
            if (!parseHandoffPayload(msg, info)) {
                if (recv_fd != scratchbird::network::INVALID_SOCKET_VALUE) {
                    closeSocketFd(recv_fd);
                }
                scratchbird::network::ControlPlaneMessage nack;
                nack.header.message_type = static_cast<uint16_t>(
                    scratchbird::network::ControlPlaneMessageType::HANDOFF_ACK);
                nack.header.request_id = msg.header.request_id;
                nack.payload = buildHandoffAck(0, 1);
                nack.header.payload_len = nack.payload.size();
                scratchbird::network::sendControlPlaneMessage(*control, nack,
                                                              scratchbird::network::INVALID_SOCKET_VALUE,
                                                              0, nullptr);
                continue;
            }

            if (recv_fd == scratchbird::network::INVALID_SOCKET_VALUE) {
                scratchbird::network::ControlPlaneMessage nack;
                nack.header.message_type = static_cast<uint16_t>(
                    scratchbird::network::ControlPlaneMessageType::HANDOFF_ACK);
                nack.header.request_id = msg.header.request_id;
                nack.payload = buildHandoffAck(info.connection_id, 1);
                nack.header.payload_len = nack.payload.size();
                scratchbird::network::sendControlPlaneMessage(*control, nack,
                                                              scratchbird::network::INVALID_SOCKET_VALUE,
                                                              0, nullptr);
                continue;
            }

            scratchbird::network::ProtocolType expected_type =
                scratchbird::network::ProtocolType::AUTO_DETECT;
            scratchbird::network::ProtocolType actual_type =
                scratchbird::network::ProtocolType::AUTO_DETECT;
            const bool expected_ok = tryParseProtocolType(config.protocol, expected_type);
            const bool actual_ok = tryParseProtocolType(info.protocol, actual_type);
            if (!expected_ok || !actual_ok || expected_type != actual_type) {
                scratchbird::network::ControlPlaneMessage nack;
                nack.header.message_type = static_cast<uint16_t>(
                    scratchbird::network::ControlPlaneMessageType::HANDOFF_ACK);
                nack.header.request_id = msg.header.request_id;
                nack.payload = buildHandoffAck(info.connection_id, 1);
                nack.header.payload_len = nack.payload.size();
                scratchbird::network::sendControlPlaneMessage(*control, nack,
                                                              scratchbird::network::INVALID_SOCKET_VALUE,
                                                              0, nullptr);
                closeSocketFd(recv_fd);
                continue;
            }

            busy = true;
            scratchbird::network::ControlPlaneMessage ack;
            ack.header.message_type = static_cast<uint16_t>(
                scratchbird::network::ControlPlaneMessageType::HANDOFF_ACK);
            ack.header.request_id = msg.header.request_id;
            ack.payload = buildHandoffAck(info.connection_id, 0);
            ack.header.payload_len = ack.payload.size();
            scratchbird::network::sendControlPlaneMessage(*control, ack,
                                                          scratchbird::network::INVALID_SOCKET_VALUE,
                                                          0, nullptr);
            std::cerr << "[parser_debug] handoff ack sent req=" << msg.header.request_id
                      << " conn_id=" << info.connection_id << "\n";

            std::cerr << "[parser_debug] session start conn_id=" << info.connection_id
                      << " fd=" << recv_fd << "\n";
            uint32_t last_error = runSession(config, info, recv_fd);
            std::cerr << "[parser_debug] session end conn_id=" << info.connection_id
                      << " last_error=" << last_error << "\n";
            busy = false;
            scratchbird::network::ControlPlaneMessage report;
            report.header.message_type = static_cast<uint16_t>(
                scratchbird::network::ControlPlaneMessageType::HEALTH_REPORT);
            report.header.request_id = msg.header.request_id;
            report.payload = buildHealthReport(worker_id, 0, 0, last_error);
            report.header.payload_len = report.payload.size();
            scratchbird::network::sendControlPlaneMessage(*control, report,
                                                          scratchbird::network::INVALID_SOCKET_VALUE,
                                                          0, nullptr);
            continue;
        }

        if (type == scratchbird::network::ControlPlaneMessageType::RECYCLE ||
            type == scratchbird::network::ControlPlaneMessageType::SHUTDOWN) {
            break;
        }
    }

    control->close();
    scratchbird::network::cleanupNetwork();
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
