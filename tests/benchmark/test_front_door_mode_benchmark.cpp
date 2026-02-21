/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/protocol/wire_protocol.h"
#include "test_helpers.h"

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::network::AddressFamily;
using scratchbird::network::NetworkAddress;
using scratchbird::network::Socket;
using scratchbird::network::SocketType;
using scratchbird::protocol::AuthMethod;
using scratchbird::protocol::AuthStatus;
using scratchbird::protocol::Message;
using scratchbird::protocol::MessageHeader;
using scratchbird::protocol::MessageType;
using scratchbird::protocol::ProtocolCodec;

namespace {

constexpr char kManagerAuthSecret[] = "manager-token-secret";
constexpr double kDefaultOverheadRatioMeanMax = 6.0;
constexpr double kDefaultOverheadRatioP95Max = 10.0;

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

uint16_t reserveEphemeralPort() {
    ErrorContext ctx;
    auto socket = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    if (!socket) {
        return 0;
    }
    NetworkAddress bind_addr("127.0.0.1", 0, AddressFamily::IPV4);
    if (socket->bind(bind_addr, &ctx) != Status::OK) {
        return 0;
    }
    auto local = socket->getLocalAddress();
    if (!local.has_value()) {
        return 0;
    }
    const uint16_t port = local->port;
    socket->close();
    return port;
}

double readPositiveEnvDoubleOrDefault(const char* name, double fallback) {
    const char* raw = std::getenv(name);
    if (!raw || raw[0] == '\0') {
        return fallback;
    }

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(raw, &end);
    if (errno == ERANGE || end == raw || (end && *end != '\0') || !std::isfinite(parsed) ||
        parsed <= 0.0) {
        return fallback;
    }
    return parsed;
}

std::unique_ptr<Socket> connectWithRetry(const std::string& host,
                                         uint16_t port,
                                         std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        NetworkAddress address(host, port, AddressFamily::IPV4);
        ErrorContext ctx;
        auto socket = Socket::connect(address, scratchbird::network::SocketOptions{}, &ctx);
        if (socket) {
            return socket;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return nullptr;
}

std::filesystem::path managerBinaryPath() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "src" / "sb_manager",
        std::filesystem::current_path() / ".." / "src" / "sb_manager",
        std::filesystem::current_path() / "build" / "src" / "sb_manager"
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return std::filesystem::canonical(candidate, ec);
        }
    }
    return {};
}

#ifndef _WIN32
class ScopedManagerProcess {
public:
    ~ScopedManagerProcess() {
        stop();
    }

    bool start(const std::filesystem::path& binary,
               uint16_t front_port,
               uint16_t native_port,
               const std::string& mcp_auth_secret = kManagerAuthSecret) {
        stop();
        if (binary.empty()) {
            return false;
        }

        std::vector<std::string> args = {
            binary.string(),
            "--bind", "127.0.0.1",
            "--port", std::to_string(front_port),
            "--native-bind", "127.0.0.1",
            "--native-port", std::to_string(native_port),
            "--database-owner", "main",
            "--mcp-auth-secret", mcp_auth_secret,
            "--log-level", "error"
        };

        pid_ = fork();
        if (pid_ < 0) {
            pid_ = 0;
            return false;
        }
        if (pid_ == 0) {
            std::vector<char*> argv;
            argv.reserve(args.size() + 1);
            for (auto& arg : args) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);
            execv(argv[0], argv.data());
            _exit(127);
        }
        return true;
    }

    void stop() {
        if (pid_ <= 0) {
            return;
        }
        ::kill(pid_, SIGTERM);
        int status = 0;
        for (int i = 0; i < 50; ++i) {
            pid_t result = waitpid(pid_, &status, WNOHANG);
            if (result == pid_) {
                pid_ = 0;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        ::kill(pid_, SIGKILL);
        waitpid(pid_, &status, 0);
        pid_ = 0;
    }

private:
    pid_t pid_ = 0;
};
#endif

Status runNativeConnectAuthQueryFlow(Socket& socket) {
    ErrorContext ctx;
    Message connect = ProtocolCodec::buildConnectRequest("main", "bench_client", 7777);
    Status status = sendWireMessage(socket, connect, &ctx);
    if (status != Status::OK) {
        return status;
    }

    Message response;
    status = receiveWireMessage(socket, response, &ctx);
    if (status != Status::OK || response.getType() != MessageType::CONNECT_RESPONSE) {
        return status == Status::OK ? Status::INVALID_ARGUMENT : status;
    }

    bool connected = false;
    std::array<uint8_t, 16> session_id{};
    std::string error;
    status = ProtocolCodec::parseConnectResponse(
        response, connected, session_id.data(), error, nullptr, &ctx);
    if (status != Status::OK || !connected) {
        return status == Status::OK ? Status::INVALID_ARGUMENT : status;
    }

    Message auth = ProtocolCodec::buildAuthRequest(
        session_id.data(), "admin", AuthMethod::SCRAM_SHA_256, {'c', '=', 'b', 'i', 'w', 's'});
    status = sendWireMessage(socket, auth, &ctx);
    if (status != Status::OK) {
        return status;
    }

    status = receiveWireMessage(socket, response, &ctx);
    if (status != Status::OK || response.getType() != MessageType::AUTH_RESPONSE) {
        return status == Status::OK ? Status::INVALID_ARGUMENT : status;
    }

    AuthStatus auth_status = AuthStatus::ERROR;
    uint32_t user_id = 0;
    std::string auth_error;
    std::vector<uint8_t> auth_data;
    status = ProtocolCodec::parseAuthResponse(
        response, auth_status, user_id, auth_error, &auth_data, &ctx);
    if (status != Status::OK || auth_status != AuthStatus::OK) {
        return status == Status::OK ? Status::INVALID_AUTHORIZATION : status;
    }

    Message query = ProtocolCodec::buildQuery(session_id.data(), "SELECT 1");
    status = sendWireMessage(socket, query, &ctx);
    if (status != Status::OK) {
        return status;
    }

    status = receiveWireMessage(socket, response, &ctx);
    if (status != Status::OK || response.getType() != MessageType::QUERY_ERROR) {
        return status == Status::OK ? Status::INVALID_ARGUMENT : status;
    }
    return Status::OK;
}

void runMcpAuthHandshake(Socket& manager_socket,
                         const std::string& mcp_auth_secret = kManagerAuthSecret) {
    ErrorContext ctx;
    const std::vector<uint8_t> secret_payload(mcp_auth_secret.begin(), mcp_auth_secret.end());
    Message auth_start = ProtocolCodec::buildMcpAuthStart(
        "admin", AuthMethod::TOKEN, secret_payload);
    ASSERT_EQ(sendWireMessage(manager_socket, auth_start, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::AUTH_RESPONSE);
    AuthStatus auth_status = AuthStatus::ERROR;
    uint32_t user_id = 0;
    std::string error_message;
    std::vector<uint8_t> auth_data;
    ASSERT_EQ(ProtocolCodec::parseAuthResponse(
                  response, auth_status, user_id, error_message, &auth_data, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(auth_status, AuthStatus::OK);
}

bool runStubNativeServer(uint16_t port, size_t expected_connections) {
    ErrorContext ctx;
    auto listener = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    if (!listener) {
        return false;
    }
    if (listener->bind(NetworkAddress("127.0.0.1", port, AddressFamily::IPV4), &ctx) != Status::OK) {
        return false;
    }
    if (listener->listen(8, &ctx) != Status::OK) {
        return false;
    }

    size_t handled = 0;
    size_t attempts = 0;
    while (handled < expected_connections && attempts < expected_connections * 8) {
        ++attempts;
        auto conn = listener->accept(nullptr, &ctx);
        if (!conn) {
            continue;
        }

        Message request;
        if (receiveWireMessage(*conn, request, &ctx) != Status::OK ||
            request.getType() != MessageType::CONNECT_REQUEST) {
            conn->close();
            continue;
        }

        std::string db_name;
        std::string client_name;
        uint32_t client_pid = 0;
        if (ProtocolCodec::parseConnectRequest(
                request, db_name, client_name, client_pid, nullptr, &ctx) != Status::OK) {
            conn->close();
            continue;
        }

        uint8_t session_id[16] = {0};
        scratchbird::protocol::generateSessionId(session_id);
        Message connect_ok = ProtocolCodec::buildConnectResponse(true, session_id);
        if (sendWireMessage(*conn, connect_ok, &ctx) != Status::OK) {
            conn->close();
            continue;
        }

        if (receiveWireMessage(*conn, request, &ctx) != Status::OK ||
            request.getType() != MessageType::AUTH_REQUEST) {
            conn->close();
            continue;
        }
        Message auth_ok = ProtocolCodec::buildAuthResponse(AuthStatus::OK, 42, "");
        if (sendWireMessage(*conn, auth_ok, &ctx) != Status::OK) {
            conn->close();
            continue;
        }

        if (receiveWireMessage(*conn, request, &ctx) != Status::OK ||
            request.getType() != MessageType::QUERY) {
            conn->close();
            continue;
        }
        Message query_error = ProtocolCodec::buildQueryError(
            static_cast<uint32_t>(Status::NOT_SUPPORTED), "0A000", "bench_stub_query_rejected");
        if (sendWireMessage(*conn, query_error, &ctx) != Status::OK) {
            conn->close();
            continue;
        }
        conn->close();
        ++handled;
    }
    listener->close();
    return handled == expected_connections;
}

double p95Micros(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const size_t idx = static_cast<size_t>(std::max<size_t>(0, (values.size() * 95) / 100));
    return values[std::min(values.size() - 1, idx)];
}

double meanMicros(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    return sum / static_cast<double>(values.size());
}

}  // namespace

TEST(FrontDoorModeBenchmarkTest, DirectVsManagerProxyConnectAuthQueryLatency) {
#ifdef _WIN32
    GTEST_SKIP() << "Benchmark relies on POSIX fork/exec behavior.";
#else
    if (!scratchbird::testing::networkTestsEnabled()) {
        GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
    }

    const auto manager_binary = managerBinaryPath();
    if (manager_binary.empty()) {
        GTEST_SKIP() << "sb_manager binary not found in build tree";
    }

    constexpr size_t kIterations = 40;
    std::vector<double> direct_lat_us;
    std::vector<double> manager_lat_us;
    std::vector<double> manager_mcp_auth_us;
    std::vector<double> manager_db_connect_us;
    std::vector<double> manager_proxy_native_us;
    direct_lat_us.reserve(kIterations);
    manager_lat_us.reserve(kIterations);
    manager_mcp_auth_us.reserve(kIterations);
    manager_db_connect_us.reserve(kIterations);
    manager_proxy_native_us.reserve(kIterations);

    const uint16_t direct_upstream_port = reserveEphemeralPort();
    ASSERT_GT(direct_upstream_port, 0);
    bool direct_stub_ok = false;
    std::thread direct_stub([&]() {
        direct_stub_ok = runStubNativeServer(direct_upstream_port, kIterations);
    });

    for (size_t i = 0; i < kIterations; ++i) {
        auto socket = connectWithRetry("127.0.0.1",
                                       direct_upstream_port,
                                       std::chrono::milliseconds(2000));
        ASSERT_NE(socket, nullptr);
        const auto start = std::chrono::steady_clock::now();
        ASSERT_EQ(runNativeConnectAuthQueryFlow(*socket), Status::OK);
        const auto stop = std::chrono::steady_clock::now();
        direct_lat_us.push_back(
            std::chrono::duration<double, std::micro>(stop - start).count());
        socket->close();
    }
    direct_stub.join();
    ASSERT_TRUE(direct_stub_ok);

    const uint16_t manager_port = reserveEphemeralPort();
    const uint16_t manager_upstream_port = reserveEphemeralPort();
    ASSERT_GT(manager_port, 0);
    ASSERT_GT(manager_upstream_port, 0);

    bool manager_stub_ok = false;
    std::thread manager_stub([&]() {
        manager_stub_ok = runStubNativeServer(manager_upstream_port, kIterations);
    });

    ScopedManagerProcess manager;
    ASSERT_TRUE(manager.start(manager_binary, manager_port, manager_upstream_port));

    for (size_t i = 0; i < kIterations; ++i) {
        auto socket = connectWithRetry("127.0.0.1", manager_port, std::chrono::milliseconds(2000));
        ASSERT_NE(socket, nullptr);

        const auto start = std::chrono::steady_clock::now();
        const auto mcp_auth_start = std::chrono::steady_clock::now();
        runMcpAuthHandshake(*socket);
        const auto mcp_auth_stop = std::chrono::steady_clock::now();
        manager_mcp_auth_us.push_back(
            std::chrono::duration<double, std::micro>(mcp_auth_stop - mcp_auth_start).count());

        ErrorContext ctx;
        const auto db_connect_start = std::chrono::steady_clock::now();
        Message db_connect = ProtocolCodec::buildMcpDbConnect("main");
        ASSERT_EQ(sendWireMessage(*socket, db_connect, &ctx), Status::OK) << ctx.message;

        Message response;
        ASSERT_EQ(receiveWireMessage(*socket, response, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(response.getType(), MessageType::CONNECT_RESPONSE);
        bool connected = false;
        uint8_t session_id[16] = {0};
        std::string connect_error;
        ASSERT_EQ(ProtocolCodec::parseConnectResponse(
                      response, connected, session_id, connect_error, nullptr, &ctx),
                  Status::OK)
            << ctx.message;
        ASSERT_TRUE(connected) << connect_error;
        const auto db_connect_stop = std::chrono::steady_clock::now();
        manager_db_connect_us.push_back(
            std::chrono::duration<double, std::micro>(db_connect_stop - db_connect_start).count());

        const auto proxy_native_start = std::chrono::steady_clock::now();
        ASSERT_EQ(runNativeConnectAuthQueryFlow(*socket), Status::OK);
        const auto proxy_native_stop = std::chrono::steady_clock::now();
        manager_proxy_native_us.push_back(
            std::chrono::duration<double, std::micro>(proxy_native_stop - proxy_native_start).count());

        const auto stop = std::chrono::steady_clock::now();
        manager_lat_us.push_back(
            std::chrono::duration<double, std::micro>(stop - start).count());
        socket->close();
    }

    manager.stop();
    manager_stub.join();
    ASSERT_TRUE(manager_stub_ok);

    const double direct_mean = meanMicros(direct_lat_us);
    const double direct_p95 = p95Micros(direct_lat_us);
    const double manager_mean = meanMicros(manager_lat_us);
    const double manager_p95 = p95Micros(manager_lat_us);
    const double manager_mcp_auth_mean = meanMicros(manager_mcp_auth_us);
    const double manager_mcp_auth_p95 = p95Micros(manager_mcp_auth_us);
    const double manager_db_connect_mean = meanMicros(manager_db_connect_us);
    const double manager_db_connect_p95 = p95Micros(manager_db_connect_us);
    const double manager_proxy_native_mean = meanMicros(manager_proxy_native_us);
    const double manager_proxy_native_p95 = p95Micros(manager_proxy_native_us);
    const double overhead_ratio_mean = direct_mean > 0.0 ? (manager_mean / direct_mean) : 0.0;
    const double overhead_ratio_p95 = direct_p95 > 0.0 ? (manager_p95 / direct_p95) : 0.0;
    const double overhead_ratio_mean_max = readPositiveEnvDoubleOrDefault(
        "SCRATCHBIRD_FRONT_DOOR_OVERHEAD_RATIO_MEAN_MAX",
        kDefaultOverheadRatioMeanMax);
    const double overhead_ratio_p95_max = readPositiveEnvDoubleOrDefault(
        "SCRATCHBIRD_FRONT_DOOR_OVERHEAD_RATIO_P95_MAX",
        kDefaultOverheadRatioP95Max);

    std::cout << "[Benchmark][FrontDoorMode] direct_mean_us=" << direct_mean
              << " direct_p95_us=" << direct_p95
              << " manager_mean_us=" << manager_mean
              << " manager_p95_us=" << manager_p95
              << " manager_mcp_auth_mean_us=" << manager_mcp_auth_mean
              << " manager_mcp_auth_p95_us=" << manager_mcp_auth_p95
              << " manager_db_connect_mean_us=" << manager_db_connect_mean
              << " manager_db_connect_p95_us=" << manager_db_connect_p95
              << " manager_proxy_native_mean_us=" << manager_proxy_native_mean
              << " manager_proxy_native_p95_us=" << manager_proxy_native_p95
              << " overhead_ratio_mean=" << overhead_ratio_mean
              << " overhead_ratio_p95=" << overhead_ratio_p95
              << " overhead_ratio_mean_max=" << overhead_ratio_mean_max
              << " overhead_ratio_p95_max=" << overhead_ratio_p95_max
              << std::endl;

    ASSERT_GT(direct_mean, 0.0);
    ASSERT_GT(manager_mean, 0.0);
    ASSERT_GT(direct_p95, 0.0);
    ASSERT_GT(manager_p95, 0.0);
    ASSERT_LE(overhead_ratio_mean, overhead_ratio_mean_max)
        << "manager_proxy mean overhead budget exceeded";
    ASSERT_LE(overhead_ratio_p95, overhead_ratio_p95_max)
        << "manager_proxy p95 overhead budget exceeded";
#endif
}
