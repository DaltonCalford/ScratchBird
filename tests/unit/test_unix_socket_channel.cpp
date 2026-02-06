/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include <gtest/gtest.h>
#include "scratchbird/ipc/unix_socket_channel.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <filesystem>

using namespace scratchbird::ipc;

// ============================================================================
// UnixSocketIPCChannel Tests
// ============================================================================

class UnixSocketIPCChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a unique temporary socket path
        socket_path_ = "/tmp/test_scratchbird_" + std::to_string(getpid()) + "_" + 
                      std::to_string(reinterpret_cast<uintptr_t>(this)) + ".sock";
        
        // Clean up any existing socket file
        std::filesystem::remove(socket_path_);
        
        // Create server socket
        server_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        ASSERT_GE(server_fd_, 0);
        
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
        
        int result = bind(server_fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        ASSERT_EQ(result, 0);
        
        result = listen(server_fd_, 5);
        ASSERT_EQ(result, 0);
    }
    
    void TearDown() override {
        if (server_fd_ >= 0) {
            close(server_fd_);
        }
        std::filesystem::remove(socket_path_);
    }
    
    std::string socket_path_;
    int server_fd_ = -1;
};

TEST_F(UnixSocketIPCChannelTest, ConstructorInitializesCorrectly) {
    UnixSocketIPCChannel channel;
    EXPECT_FALSE(channel.isConnected());
    EXPECT_EQ(channel.getSessionId(), 0);
}

TEST_F(UnixSocketIPCChannelTest, ConnectSuccess) {
    UnixSocketIPCChannel channel;
    
    // Accept connection in background
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        EXPECT_GE(client, 0);
        if (client >= 0) {
            // Read session ID
            uint32_t session_id;
            recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
            close(client);
        }
    });
    
    auto status = channel.connect(socket_path_);
    EXPECT_EQ(status.code(), core::Status::OK);
    EXPECT_TRUE(channel.isConnected());
    
    server_thread.join();
}

TEST_F(UnixSocketIPCChannelTest, ConnectFailureNoServer) {
    UnixSocketIPCChannel channel;
    
    // Use non-existent socket path
    auto status = channel.connect("/tmp/nonexistent_socket_12345.sock");
    EXPECT_EQ(status.code(), core::Status::CONNECTION_REFUSED);
    EXPECT_FALSE(channel.isConnected());
}

TEST_F(UnixSocketIPCChannelTest, ConnectWhenAlreadyConnected) {
    UnixSocketIPCChannel channel;
    
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            uint32_t session_id;
            recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
            close(client);
        }
    });
    
    channel.connect(socket_path_);
    server_thread.join();
    
    // Try to connect again
    auto status = channel.connect(socket_path_);
    EXPECT_EQ(status.code(), core::Status::ALREADY_EXISTS);
}

TEST_F(UnixSocketIPCChannelTest, Disconnect) {
    UnixSocketIPCChannel channel;
    
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            uint32_t session_id;
            recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
            close(client);
        }
    });
    
    channel.connect(socket_path_);
    server_thread.join();
    
    EXPECT_TRUE(channel.isConnected());
    
    auto status = channel.disconnect();
    EXPECT_EQ(status.code(), core::Status::OK);
    EXPECT_FALSE(channel.isConnected());
}

TEST_F(UnixSocketIPCChannelTest, SendReceive) {
    UnixSocketIPCChannel client_channel;
    int server_client_fd = -1;
    
    std::thread server_thread([this, &server_client_fd]() {
        server_client_fd = accept(server_fd_, nullptr, nullptr);
        ASSERT_GE(server_client_fd, 0);
        
        // Read session ID
        uint32_t session_id;
        recv(server_client_fd, &session_id, sizeof(session_id), MSG_WAITALL);
        
        // Read message
        uint32_t len;
        recv(server_client_fd, &len, sizeof(len), MSG_WAITALL);
        len = ntohl(len);
        
        std::vector<uint8_t> msg(len);
        recv(server_client_fd, msg.data(), len, MSG_WAITALL);
        
        // Echo back
        send(server_client_fd, &len, sizeof(len), 0);
        send(server_client_fd, msg.data(), len, 0);
    });
    
    client_channel.connect(socket_path_);
    
    // Create and send a message
    IPCMessage msg(IPCMessageType::SIMPLE_QUERY, 1);
    msg.payload = std::vector<uint8_t>(100, 0xAB);
    
    auto status = client_channel.send(msg);
    EXPECT_EQ(status.code(), core::Status::OK);
    
    // Receive response
    IPCMessage received;
    // Note: receive would need actual implementation testing
    
    server_thread.join();
    if (server_client_fd >= 0) {
        close(server_client_fd);
    }
}

TEST_F(UnixSocketIPCChannelTest, SendWhenNotConnected) {
    UnixSocketIPCChannel channel;
    
    IPCMessage msg(IPCMessageType::SIMPLE_QUERY, 1);
    auto status = channel.send(msg);
    
    EXPECT_EQ(status.code(), core::Status::NOT_CONNECTED);
}

TEST_F(UnixSocketIPCChannelTest, ReceiveWhenNotConnected) {
    UnixSocketIPCChannel channel;
    
    IPCMessage msg;
    auto status = channel.receive(msg);
    
    EXPECT_EQ(status.code(), core::Status::NOT_CONNECTED);
}

TEST_F(UnixSocketIPCChannelTest, GetEndpoint) {
    UnixSocketIPCChannel channel;
    
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            uint32_t session_id;
            recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
            close(client);
        }
    });
    
    channel.connect(socket_path_);
    server_thread.join();
    
    EXPECT_EQ(channel.getEndpoint(), socket_path_);
}

TEST_F(UnixSocketIPCChannelTest, GetSessionId) {
    UnixSocketIPCChannel channel;
    
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            // Send session ID
            uint32_t session_id = 42;
            send(client, &session_id, sizeof(session_id), 0);
            close(client);
        }
    });
    
    channel.connect(socket_path_);
    server_thread.join();
    
    // Session ID should be set after connect
    EXPECT_EQ(channel.getSessionId(), 42);
}

TEST_F(UnixSocketIPCChannelTest, SetNonBlocking) {
    UnixSocketIPCChannel channel;
    
    // Should fail when not connected
    auto status = channel.setNonBlocking(true);
    EXPECT_EQ(status.code(), core::Status::NOT_CONNECTED);
    
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            uint32_t session_id;
            recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
            close(client);
        }
    });
    
    channel.connect(socket_path_);
    server_thread.join();
    
    // Should succeed when connected
    status = channel.setNonBlocking(true);
    EXPECT_EQ(status.code(), core::Status::OK);
}

TEST_F(UnixSocketIPCChannelTest, TryReceiveTimeout) {
    UnixSocketIPCChannel channel;
    
    std::thread server_thread([this]() {
        int client = accept(server_fd_, nullptr, nullptr);
        if (client >= 0) {
            uint32_t session_id;
            recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
            // Don't send anything - let timeout occur
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            close(client);
        }
    });
    
    channel.connect(socket_path_);
    
    IPCMessage msg;
    auto status = channel.tryReceive(msg, 50); // 50ms timeout
    
    EXPECT_EQ(status.code(), core::Status::DEADLINE_EXCEEDED);
    
    server_thread.join();
}

// ============================================================================
// IPCChannelFactory Tests
// ============================================================================

TEST(IPCChannelFactoryTest, CreateUnixSocket) {
    auto channel = IPCChannelFactory::create(IPCChannelType::UNIX_SOCKET);
    EXPECT_NE(channel, nullptr);
}

TEST(IPCChannelFactoryTest, CreateDefault) {
    auto channel = IPCChannelFactory::createDefault();
    EXPECT_NE(channel, nullptr);
}

TEST(IPCChannelFactoryTest, GetDefaultType) {
    auto type = IPCChannelFactory::getDefaultType();
    #if defined(__linux__) || defined(__APPLE__)
        EXPECT_EQ(type, IPCChannelType::UNIX_SOCKET);
    #endif
}

TEST(IPCChannelFactoryTest, IsSupportedUnixSocket) {
    #if defined(__linux__) || defined(__APPLE__)
        EXPECT_TRUE(IPCChannelFactory::isSupported(IPCChannelType::UNIX_SOCKET));
    #else
        EXPECT_FALSE(IPCChannelFactory::isSupported(IPCChannelType::UNIX_SOCKET));
    #endif
}

TEST(IPCChannelFactoryTest, IsSupportedTcpLoopback) {
    EXPECT_TRUE(IPCChannelFactory::isSupported(IPCChannelType::TCP_LOOPBACK));
}

TEST(IPCChannelFactoryTest, IsSupportedSharedMemory) {
    EXPECT_FALSE(IPCChannelFactory::isSupported(IPCChannelType::SHARED_MEMORY));
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(UnixSocketStressTest, MultipleConnections) {
    const int num_connections = 10;
    std::string socket_path = "/tmp/test_scratchbird_stress.sock";
    std::filesystem::remove(socket_path);
    
    // Create server
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(server_fd, 0);
    
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    listen(server_fd, num_connections);
    
    // Accept connections in background
    std::atomic<int> accepted{0};
    std::thread server_thread([&]() {
        for (int i = 0; i < num_connections; i++) {
            int client = accept(server_fd, nullptr, nullptr);
            if (client >= 0) {
                accepted++;
                uint32_t session_id;
                recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
                close(client);
            }
        }
    });
    
    // Create multiple clients
    std::vector<std::thread> client_threads;
    for (int i = 0; i < num_connections; i++) {
        client_threads.emplace_back([&]() {
            UnixSocketIPCChannel channel;
            channel.connect(socket_path);
        });
    }
    
    for (auto& t : client_threads) {
        t.join();
    }
    server_thread.join();
    
    EXPECT_EQ(accepted, num_connections);
    
    close(server_fd);
    std::filesystem::remove(socket_path);
}

TEST(UnixSocketStressTest, RapidConnectDisconnect) {
    std::string socket_path = "/tmp/test_scratchbird_rapid.sock";
    std::filesystem::remove(socket_path);
    
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    listen(server_fd, 10);
    
    std::atomic<bool> running{true};
    std::thread server_thread([&]() {
        while (running) {
            int client = accept(server_fd, nullptr, nullptr);
            if (client >= 0) {
                uint32_t session_id;
                recv(client, &session_id, sizeof(session_id), MSG_WAITALL);
                close(client);
            }
        }
    });
    
    // Rapid connect/disconnect cycles
    for (int i = 0; i < 100; i++) {
        UnixSocketIPCChannel channel;
        channel.connect(socket_path);
        channel.disconnect();
    }
    
    running = false;
    close(server_fd);
    server_thread.join();
    std::filesystem::remove(socket_path);
}
