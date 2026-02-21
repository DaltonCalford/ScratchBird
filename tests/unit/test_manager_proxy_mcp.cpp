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

#include <chrono>
#include <cstring>
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
using scratchbird::protocol::StatusRequestType;

namespace {

constexpr char kManagerAuthSecret[] = "manager-token-secret";

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

class ScopedManagerProcess {
public:
    ~ScopedManagerProcess() {
        stop();
    }

    bool start(const std::filesystem::path& binary,
               uint16_t front_port,
               uint16_t native_port,
               const std::string& owner_database = "main",
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
            "--database-owner", owner_database,
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

void runMcpAuthHandshake(Socket& manager_socket,
                         const std::string& mcp_auth_secret = kManagerAuthSecret) {
    ErrorContext ctx;
    Message auth_start = ProtocolCodec::buildMcpAuthStart(
        "admin", AuthMethod::TOKEN, {'t', 'o', 'k', 'e', 'n'});
    ASSERT_EQ(sendWireMessage(manager_socket, auth_start, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::AUTH_CHALLENGE);

    uint8_t challenge_session_id[16] = {0};
    std::string challenge_username;
    std::vector<AuthMethod> allowed_methods;
    bool has_required_method = false;
    AuthMethod required_method = AuthMethod::PASSWORD;
    uint8_t allowed_transport_mask = 0;
    std::vector<uint8_t> challenge_nonce;
    ASSERT_EQ(
        ProtocolCodec::parseAuthChallenge(response,
                                          challenge_session_id,
                                          challenge_username,
                                          allowed_methods,
                                          has_required_method,
                                          required_method,
                                          allowed_transport_mask,
                                          challenge_nonce,
                                          &ctx),
        Status::OK) << ctx.message;
    EXPECT_EQ(challenge_username, "admin");
    ASSERT_EQ(allowed_methods.size(), 1U);
    EXPECT_EQ(allowed_methods[0], AuthMethod::TOKEN);
    EXPECT_TRUE(has_required_method);
    EXPECT_EQ(required_method, AuthMethod::TOKEN);
    EXPECT_FALSE(challenge_nonce.empty());

    const std::vector<uint8_t> secret_payload(mcp_auth_secret.begin(), mcp_auth_secret.end());
    Message auth_continue = ProtocolCodec::buildMcpAuthContinue(secret_payload);
    ASSERT_EQ(sendWireMessage(manager_socket, auth_continue, &ctx), Status::OK) << ctx.message;

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

}  // namespace

TEST(ManagerProxyMcpTest, HelloAuthListConnectFlowWithReadyInternalEndpoint) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const auto manager_binary = managerBinaryPath();
    if (manager_binary.empty()) {
        GTEST_SKIP() << "sb_manager binary not found in build tree";
    }

    const uint16_t manager_port = reserveEphemeralPort();
    const uint16_t native_port = reserveEphemeralPort();
    ASSERT_GT(manager_port, 0);
    ASSERT_GT(native_port, 0);

    ErrorContext listener_ctx;
    auto internal_listener = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &listener_ctx);
    ASSERT_NE(internal_listener, nullptr) << listener_ctx.message;
    ASSERT_EQ(internal_listener->bind(NetworkAddress("127.0.0.1", native_port, AddressFamily::IPV4),
                                      &listener_ctx),
              Status::OK) << listener_ctx.message;
    ASSERT_EQ(internal_listener->listen(8, &listener_ctx), Status::OK) << listener_ctx.message;

    ScopedManagerProcess manager;
    ASSERT_TRUE(manager.start(manager_binary, manager_port, native_port, "main"));

    auto manager_socket = connectWithRetry("127.0.0.1", manager_port, std::chrono::milliseconds(2000));
    ASSERT_NE(manager_socket, nullptr);

    ErrorContext ctx;
    Message hello = ProtocolCodec::buildMcpHello(scratchbird::protocol::MCP_PROTOCOL_VERSION, 0x01);
    ASSERT_EQ(sendWireMessage(*manager_socket, hello, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::STATUS_RESPONSE);

    StatusRequestType request_type = StatusRequestType::CONNECTION_INFO;
    std::vector<ProtocolCodec::StatusEntry> entries;
    ASSERT_EQ(ProtocolCodec::parseStatusResponse(response, request_type, entries, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(request_type, StatusRequestType::SERVER_INFO);

    runMcpAuthHandshake(*manager_socket);

    Message db_list = ProtocolCodec::buildMcpDbList();
    ASSERT_EQ(sendWireMessage(*manager_socket, db_list, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::STATUS_RESPONSE);

    entries.clear();
    request_type = StatusRequestType::SERVER_INFO;
    ASSERT_EQ(ProtocolCodec::parseStatusResponse(response, request_type, entries, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(request_type, StatusRequestType::DATABASE_INFO);

    Message db_info = ProtocolCodec::buildMcpDbInfo("main");
    ASSERT_EQ(sendWireMessage(*manager_socket, db_info, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::STATUS_RESPONSE);
    entries.clear();
    request_type = StatusRequestType::SERVER_INFO;
    ASSERT_EQ(ProtocolCodec::parseStatusResponse(response, request_type, entries, &ctx), Status::OK)
        << ctx.message;
    ASSERT_EQ(request_type, StatusRequestType::DATABASE_INFO);
    bool saw_db_main = false;
    for (const auto& entry : entries) {
        if (entry.key == "db" && entry.value == "main") {
            saw_db_main = true;
            break;
        }
    }
    EXPECT_TRUE(saw_db_main);

    Message db_connect = ProtocolCodec::buildMcpDbConnect(
        "main", "native_v3", "native_v3", std::vector<uint8_t>(16, 0xAB));
    ASSERT_EQ(sendWireMessage(*manager_socket, db_connect, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::CONNECT_RESPONSE);

    bool success = false;
    uint8_t session_id[16] = {0};
    std::string error_message;
    ASSERT_EQ(ProtocolCodec::parseConnectResponse(
                  response, success, session_id, error_message, nullptr, &ctx),
              Status::OK) << ctx.message;
    EXPECT_TRUE(success) << error_message;
#endif
}

TEST(ManagerProxyMcpTest, DbConnectTransitionsToByteProxyAndRelaysNativeAuthAndQueryFrames) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const auto manager_binary = managerBinaryPath();
    if (manager_binary.empty()) {
        GTEST_SKIP() << "sb_manager binary not found in build tree";
    }

    const uint16_t manager_port = reserveEphemeralPort();
    const uint16_t native_port = reserveEphemeralPort();
    ASSERT_GT(manager_port, 0);
    ASSERT_GT(native_port, 0);

    ErrorContext listener_ctx;
    auto internal_listener = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &listener_ctx);
    ASSERT_NE(internal_listener, nullptr) << listener_ctx.message;
    ASSERT_EQ(internal_listener->bind(NetworkAddress("127.0.0.1", native_port, AddressFamily::IPV4),
                                      &listener_ctx),
              Status::OK) << listener_ctx.message;
    ASSERT_EQ(internal_listener->listen(8, &listener_ctx), Status::OK) << listener_ctx.message;
    internal_listener->setNonBlocking(true, nullptr);

    std::atomic<bool> forwarded_flow_complete{false};
    std::thread upstream_thread([&]() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!internal_listener->waitReadable(50)) {
                continue;
            }
            ErrorContext accept_ctx;
            auto upstream_conn = internal_listener->accept(nullptr, &accept_ctx);
            if (!upstream_conn) {
                continue;
            }

            ErrorContext recv_ctx;
            Message forwarded;
            if (receiveWireMessage(*upstream_conn, forwarded, &recv_ctx) != Status::OK) {
                upstream_conn->close();
                continue;
            }
            if (forwarded.getType() != MessageType::CONNECT_REQUEST) {
                upstream_conn->close();
                continue;
            }

            std::string db_name;
            std::string client_name;
            uint32_t client_pid = 0;
            EXPECT_EQ(ProtocolCodec::parseConnectRequest(
                          forwarded, db_name, client_name, client_pid, nullptr, &recv_ctx),
                      Status::OK) << recv_ctx.message;
            EXPECT_EQ(db_name, "main");
            EXPECT_EQ(client_name, "proxy_client");
            EXPECT_EQ(client_pid, 4321u);

            uint8_t upstream_session_id[16] = {0};
            scratchbird::protocol::generateSessionId(upstream_session_id);
            Message connect_ok = ProtocolCodec::buildConnectResponse(true, upstream_session_id);
            EXPECT_EQ(sendWireMessage(*upstream_conn, connect_ok, &recv_ctx), Status::OK)
                << recv_ctx.message;

            Message auth_request;
            EXPECT_EQ(receiveWireMessage(*upstream_conn, auth_request, &recv_ctx), Status::OK)
                << recv_ctx.message;
            EXPECT_EQ(auth_request.getType(), MessageType::AUTH_REQUEST);

            uint8_t parsed_auth_session_id[16] = {0};
            std::string parsed_username;
            AuthMethod parsed_method = AuthMethod::PASSWORD;
            std::vector<uint8_t> parsed_payload;
            EXPECT_EQ(ProtocolCodec::parseAuthRequest(auth_request,
                                                      parsed_auth_session_id,
                                                      parsed_username,
                                                      parsed_method,
                                                      parsed_payload,
                                                      &recv_ctx),
                      Status::OK) << recv_ctx.message;
            EXPECT_EQ(std::memcmp(parsed_auth_session_id, upstream_session_id, 16), 0);
            EXPECT_EQ(parsed_username, "admin");
            EXPECT_EQ(parsed_method, AuthMethod::SCRAM_SHA_256);

            Message auth_ok = ProtocolCodec::buildAuthResponse(AuthStatus::OK, 42, "");
            EXPECT_EQ(sendWireMessage(*upstream_conn, auth_ok, &recv_ctx), Status::OK)
                << recv_ctx.message;

            Message query_request;
            EXPECT_EQ(receiveWireMessage(*upstream_conn, query_request, &recv_ctx), Status::OK)
                << recv_ctx.message;
            EXPECT_EQ(query_request.getType(), MessageType::QUERY);

            std::string parsed_query;
            uint8_t query_session_id[16] = {0};
            uint8_t query_flags = 0;
            EXPECT_EQ(ProtocolCodec::parseQuery(query_request,
                                                query_session_id,
                                                parsed_query,
                                                query_flags,
                                                &recv_ctx),
                      Status::OK) << recv_ctx.message;
            EXPECT_EQ(std::memcmp(query_session_id, upstream_session_id, 16), 0);
            EXPECT_EQ(parsed_query, "SELECT 1");

            Message query_error = ProtocolCodec::buildQueryError(
                static_cast<uint32_t>(Status::NOT_SUPPORTED),
                "0A000",
                "stub_backend_query_rejected");
            EXPECT_EQ(sendWireMessage(*upstream_conn, query_error, &recv_ctx), Status::OK)
                << recv_ctx.message;

            forwarded_flow_complete.store(true, std::memory_order_release);
            upstream_conn->close();
            return;
        }
    });

    ScopedManagerProcess manager;
    ASSERT_TRUE(manager.start(manager_binary, manager_port, native_port, "main"));

    auto manager_socket =
        connectWithRetry("127.0.0.1", manager_port, std::chrono::milliseconds(2000));
    ASSERT_NE(manager_socket, nullptr);

    runMcpAuthHandshake(*manager_socket);

    ErrorContext ctx;
    Message db_connect = ProtocolCodec::buildMcpDbConnect("main");
    ASSERT_EQ(sendWireMessage(*manager_socket, db_connect, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::CONNECT_RESPONSE);

    bool mcp_connect_success = false;
    uint8_t mcp_session_id[16] = {0};
    std::string mcp_error;
    ASSERT_EQ(ProtocolCodec::parseConnectResponse(
                  response, mcp_connect_success, mcp_session_id, mcp_error, nullptr, &ctx),
              Status::OK) << ctx.message;
    ASSERT_TRUE(mcp_connect_success) << mcp_error;

    Message proxied_connect = ProtocolCodec::buildConnectRequest("main", "proxy_client", 4321);
    ASSERT_EQ(sendWireMessage(*manager_socket, proxied_connect, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::CONNECT_RESPONSE);

    bool proxied_success = false;
    uint8_t proxied_session_id[16] = {0};
    std::string proxied_error;
    ASSERT_EQ(ProtocolCodec::parseConnectResponse(
                  response, proxied_success, proxied_session_id, proxied_error, nullptr, &ctx),
              Status::OK) << ctx.message;
    EXPECT_TRUE(proxied_success) << proxied_error;

    Message proxied_auth = ProtocolCodec::buildAuthRequest(
        proxied_session_id,
        "admin",
        AuthMethod::SCRAM_SHA_256,
        {'c', '=', 'b', 'i', 'w', 's'});
    ASSERT_EQ(sendWireMessage(*manager_socket, proxied_auth, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::AUTH_RESPONSE);

    AuthStatus proxied_auth_status = AuthStatus::ERROR;
    uint32_t proxied_user_id = 0;
    std::string proxied_auth_error;
    std::vector<uint8_t> proxied_auth_data;
    ASSERT_EQ(ProtocolCodec::parseAuthResponse(response,
                                               proxied_auth_status,
                                               proxied_user_id,
                                               proxied_auth_error,
                                               &proxied_auth_data,
                                               &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(proxied_auth_status, AuthStatus::OK);
    EXPECT_EQ(proxied_user_id, 42u);

    Message proxied_query = ProtocolCodec::buildQuery(proxied_session_id, "SELECT 1");
    ASSERT_EQ(sendWireMessage(*manager_socket, proxied_query, &ctx), Status::OK) << ctx.message;

    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::QUERY_ERROR);

    uint32_t proxied_query_error_code = 0;
    std::string proxied_sqlstate;
    std::string proxied_query_error;
    std::string proxied_query_detail;
    std::string proxied_query_hint;
    ASSERT_EQ(ProtocolCodec::parseQueryError(response,
                                             proxied_query_error_code,
                                             proxied_sqlstate,
                                             proxied_query_error,
                                             proxied_query_detail,
                                             proxied_query_hint,
                                             &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(proxied_sqlstate, "0A000");
    EXPECT_NE(proxied_query_error.find("stub_backend_query_rejected"), std::string::npos);

    // Manager should close proxied session promptly once upstream has closed.
    manager_socket->setReadTimeout(250, nullptr);
    Message trailing;
    const Status trailing_status = receiveWireMessage(*manager_socket, trailing, &ctx);
    EXPECT_EQ(trailing_status, Status::CONNECTION_FAILURE);

    manager_socket->close();
    upstream_thread.join();
    EXPECT_TRUE(forwarded_flow_complete.load(std::memory_order_acquire));
#endif
}

TEST(ManagerProxyMcpTest, DbConnectFailsWhenInternalEndpointNotReady) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const auto manager_binary = managerBinaryPath();
    if (manager_binary.empty()) {
        GTEST_SKIP() << "sb_manager binary not found in build tree";
    }

    const uint16_t manager_port = reserveEphemeralPort();
    const uint16_t native_port = reserveEphemeralPort();
    ASSERT_GT(manager_port, 0);
    ASSERT_GT(native_port, 0);

    ScopedManagerProcess manager;
    ASSERT_TRUE(manager.start(manager_binary, manager_port, native_port, "main"));

    auto manager_socket = connectWithRetry("127.0.0.1", manager_port, std::chrono::milliseconds(2000));
    ASSERT_NE(manager_socket, nullptr);

    runMcpAuthHandshake(*manager_socket);

    ErrorContext ctx;
    Message db_connect = ProtocolCodec::buildMcpDbConnect("main");
    ASSERT_EQ(sendWireMessage(*manager_socket, db_connect, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::CONNECT_RESPONSE);

    bool success = true;
    uint8_t session_id[16] = {0};
    std::string error_message;
    ASSERT_EQ(ProtocolCodec::parseConnectResponse(
                  response, success, session_id, error_message, nullptr, &ctx),
              Status::OK) << ctx.message;
    EXPECT_FALSE(success);
    EXPECT_NE(error_message.find("not ready"), std::string::npos);
#endif
}

TEST(ManagerProxyMcpTest, AuthFailsWithWrongManagerToken) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const auto manager_binary = managerBinaryPath();
    if (manager_binary.empty()) {
        GTEST_SKIP() << "sb_manager binary not found in build tree";
    }

    const uint16_t manager_port = reserveEphemeralPort();
    const uint16_t native_port = reserveEphemeralPort();
    ASSERT_GT(manager_port, 0);
    ASSERT_GT(native_port, 0);

    ErrorContext listener_ctx;
    auto internal_listener = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &listener_ctx);
    ASSERT_NE(internal_listener, nullptr) << listener_ctx.message;
    ASSERT_EQ(internal_listener->bind(NetworkAddress("127.0.0.1", native_port, AddressFamily::IPV4),
                                      &listener_ctx),
              Status::OK) << listener_ctx.message;
    ASSERT_EQ(internal_listener->listen(8, &listener_ctx), Status::OK) << listener_ctx.message;

    ScopedManagerProcess manager;
    ASSERT_TRUE(manager.start(manager_binary, manager_port, native_port, "main", kManagerAuthSecret));

    auto manager_socket = connectWithRetry("127.0.0.1", manager_port, std::chrono::milliseconds(2000));
    ASSERT_NE(manager_socket, nullptr);

    ErrorContext ctx;
    Message auth_start = ProtocolCodec::buildMcpAuthStart(
        "admin", AuthMethod::TOKEN, {'t', 'o', 'k', 'e', 'n'});
    ASSERT_EQ(sendWireMessage(*manager_socket, auth_start, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::AUTH_CHALLENGE);

    Message auth_continue = ProtocolCodec::buildMcpAuthContinue(
        {'w', 'r', 'o', 'n', 'g', '-', 's', 'e', 'c', 'r', 'e', 't'});
    ASSERT_EQ(sendWireMessage(*manager_socket, auth_continue, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::AUTH_RESPONSE);

    AuthStatus auth_status = AuthStatus::OK;
    uint32_t user_id = 0;
    std::string error_message;
    std::vector<uint8_t> auth_data;
    ASSERT_EQ(ProtocolCodec::parseAuthResponse(
                  response, auth_status, user_id, error_message, &auth_data, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(auth_status, AuthStatus::ERROR);
    EXPECT_NE(error_message.find("failed"), std::string::npos);
#endif
}

TEST(ManagerProxyMcpTest, AuthStartFastPathAcceptsTokenPayload) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const auto manager_binary = managerBinaryPath();
    if (manager_binary.empty()) {
        GTEST_SKIP() << "sb_manager binary not found in build tree";
    }

    const uint16_t manager_port = reserveEphemeralPort();
    const uint16_t native_port = reserveEphemeralPort();
    ASSERT_GT(manager_port, 0);
    ASSERT_GT(native_port, 0);

    ErrorContext listener_ctx;
    auto internal_listener = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &listener_ctx);
    ASSERT_NE(internal_listener, nullptr) << listener_ctx.message;
    ASSERT_EQ(internal_listener->bind(NetworkAddress("127.0.0.1", native_port, AddressFamily::IPV4),
                                      &listener_ctx),
              Status::OK) << listener_ctx.message;
    ASSERT_EQ(internal_listener->listen(8, &listener_ctx), Status::OK) << listener_ctx.message;

    ScopedManagerProcess manager;
    ASSERT_TRUE(manager.start(manager_binary, manager_port, native_port, "main", kManagerAuthSecret));

    auto manager_socket = connectWithRetry("127.0.0.1", manager_port, std::chrono::milliseconds(2000));
    ASSERT_NE(manager_socket, nullptr);

    ErrorContext ctx;
    const std::vector<uint8_t> secret_payload(
        kManagerAuthSecret, kManagerAuthSecret + std::strlen(kManagerAuthSecret));
    Message auth_start = ProtocolCodec::buildMcpAuthStart(
        "admin", AuthMethod::TOKEN, secret_payload);
    ASSERT_EQ(sendWireMessage(*manager_socket, auth_start, &ctx), Status::OK) << ctx.message;

    Message response;
    ASSERT_EQ(receiveWireMessage(*manager_socket, response, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(response.getType(), MessageType::AUTH_RESPONSE);

    AuthStatus auth_status = AuthStatus::ERROR;
    uint32_t user_id = 0;
    std::string error_message;
    std::vector<uint8_t> auth_data;
    ASSERT_EQ(ProtocolCodec::parseAuthResponse(
                  response, auth_status, user_id, error_message, &auth_data, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(auth_status, AuthStatus::OK);
#endif
}
