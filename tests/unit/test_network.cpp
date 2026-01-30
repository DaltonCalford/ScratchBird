/**
 * Network Infrastructure Unit Tests
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * Tests for Socket, EventLoop, ThreadPool, and ConnectionManager.
 */

#include <gtest/gtest.h>

#include "scratchbird/network/network.h"
#include "scratchbird/core/error_context.h"
#include "test_helpers.h"

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <future>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

using namespace scratchbird::network;
using namespace scratchbird::core;
using scratchbird::testing::uniqueTestSocketPath;

namespace {
bool isNetworkRestrictedError(const ErrorContext& ctx) {
    return ctx.message.find("Operation not permitted") != std::string::npos ||
           ctx.message.find("Permission denied") != std::string::npos;
}
} // namespace

// ============================================================================
// Test Fixtures
// ============================================================================

class NetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!scratchbird::testing::networkTestsEnabled()) {
            GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
        }
        ASSERT_TRUE(initNetwork());
        ErrorContext ctx;
        auto probe = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
        if (!probe && isNetworkRestrictedError(ctx)) {
            GTEST_SKIP() << "Sockets not permitted in this environment: " << ctx.message;
        }
    }

    void TearDown() override {
        // Don't cleanup - let other tests use the initialized network
    }
};

// ============================================================================
// Socket Tests
// ============================================================================

TEST_F(NetworkTest, SocketCreate) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->isValid());
    EXPECT_EQ(sock->getFamily(), AddressFamily::IPV4);
    EXPECT_EQ(sock->getType(), SocketType::STREAM);
}

TEST_F(NetworkTest, SocketCreateIPv6) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV6, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->isValid());
    EXPECT_EQ(sock->getFamily(), AddressFamily::IPV6);
}

#ifndef _WIN32
TEST_F(NetworkTest, SocketCreateUnix) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::UNIX, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);
    EXPECT_TRUE(sock->isValid());
    EXPECT_EQ(sock->getFamily(), AddressFamily::UNIX);
}
#endif

TEST_F(NetworkTest, SocketOptions) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    // Test setting options
    EXPECT_EQ(sock->setReuseAddress(true), Status::OK);
    EXPECT_EQ(sock->setTcpNoDelay(true), Status::OK);
    EXPECT_EQ(sock->setKeepAlive(true), Status::OK);
    EXPECT_EQ(sock->setNonBlocking(true), Status::OK);
    EXPECT_TRUE(sock->isNonBlocking());
}

TEST_F(NetworkTest, SocketBindListen) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    ASSERT_EQ(sock->setReuseAddress(true), Status::OK);

    // Bind to any available port
    NetworkAddress addr("127.0.0.1", 0, AddressFamily::IPV4);
    ASSERT_EQ(sock->bind(addr, &ctx), Status::OK);
    EXPECT_EQ(sock->getState(), SocketState::BOUND);

    // Start listening
    ASSERT_EQ(sock->listen(10, &ctx), Status::OK);
    EXPECT_EQ(sock->getState(), SocketState::LISTENING);

    // Get actual bound port
    auto local = sock->getLocalAddress();
    ASSERT_TRUE(local.has_value());
    EXPECT_GT(local->port, 0);
}

TEST_F(NetworkTest, SocketConnectAccept) {
    ErrorContext ctx;

    // Create server socket
    auto server = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(server->setReuseAddress(true), Status::OK);

    NetworkAddress server_addr("127.0.0.1", 0, AddressFamily::IPV4);
    ASSERT_EQ(server->bind(server_addr, &ctx), Status::OK);
    ASSERT_EQ(server->listen(10, &ctx), Status::OK);

    auto local = server->getLocalAddress();
    ASSERT_TRUE(local.has_value());
    uint16_t port = local->port;

    // Set server to non-blocking for accept
    ASSERT_EQ(server->setNonBlocking(true), Status::OK);

    // Create client socket
    auto client = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(client, nullptr);

    // Connect client
    NetworkAddress connect_addr("127.0.0.1", port, AddressFamily::IPV4);
    ASSERT_EQ(client->connect(connect_addr, &ctx), Status::OK);

    // Accept connection on server
    // May need to wait a bit for connection to establish
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    NetworkAddress client_addr;
    auto accepted = server->accept(&client_addr, &ctx);
    ASSERT_NE(accepted, nullptr);
    EXPECT_TRUE(accepted->isConnected());
}

TEST_F(NetworkTest, SocketReadWrite) {
    ErrorContext ctx;

    // Create server
    auto server = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(server->setReuseAddress(true), Status::OK);

    NetworkAddress server_addr("127.0.0.1", 0, AddressFamily::IPV4);
    ASSERT_EQ(server->bind(server_addr, &ctx), Status::OK);
    ASSERT_EQ(server->listen(10, &ctx), Status::OK);

    auto local = server->getLocalAddress();
    uint16_t port = local->port;

    // Connect client
    auto client = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(client, nullptr);
    NetworkAddress connect_addr("127.0.0.1", port, AddressFamily::IPV4);
    ASSERT_EQ(client->connect(connect_addr, &ctx), Status::OK);

    // Accept
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto accepted = server->accept(nullptr, &ctx);
    ASSERT_NE(accepted, nullptr);

    // Write from client
    const char* msg = "Hello, World!";
    size_t bytes_written;
    ASSERT_EQ(client->write(msg, strlen(msg), &bytes_written), Status::OK);
    EXPECT_EQ(bytes_written, strlen(msg));

    // Read on server side
    char buf[64];
    size_t bytes_read;

    // Wait for data
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    ASSERT_EQ(accepted->read(buf, sizeof(buf), &bytes_read), Status::OK);
    EXPECT_EQ(bytes_read, strlen(msg));
    EXPECT_EQ(std::string(buf, bytes_read), msg);

    // Check stats
    EXPECT_GT(client->getStats().bytes_sent, 0u);
    EXPECT_GT(accepted->getStats().bytes_received, 0u);
}

TEST_F(NetworkTest, SocketMove) {
    ErrorContext ctx;
    auto sock1 = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock1, nullptr);

    socket_t fd = sock1->getFd();
    EXPECT_NE(fd, INVALID_SOCKET_VALUE);

    // Move construct
    Socket sock2(std::move(*sock1));
    EXPECT_EQ(sock2.getFd(), fd);
    EXPECT_EQ(sock1->getFd(), INVALID_SOCKET_VALUE);

    // Move assign
    auto sock3 = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    *sock3 = std::move(sock2);
    EXPECT_EQ(sock3->getFd(), fd);
    EXPECT_EQ(sock2.getFd(), INVALID_SOCKET_VALUE);
}

// ============================================================================
// Event Loop Tests
// ============================================================================

TEST_F(NetworkTest, EventLoopCreate) {
    ErrorContext ctx;
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);
    EXPECT_FALSE(loop->isRunning());
    EXPECT_EQ(loop->size(), 0u);
}

TEST_F(NetworkTest, EventLoopAddRemove) {
    ErrorContext ctx;
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    // Create a socket to register
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    socket_t fd = sock->getFd();
    bool event_fired = false;

    // Add to event loop
    ASSERT_EQ(loop->add(fd, EventType::READ,
        [&event_fired](const EventData& /*event*/) {
            event_fired = true;
        }), Status::OK);

    EXPECT_TRUE(loop->contains(fd));
    EXPECT_EQ(loop->size(), 1u);

    // Remove from event loop
    ASSERT_EQ(loop->remove(fd), Status::OK);
    EXPECT_FALSE(loop->contains(fd));
    EXPECT_EQ(loop->size(), 0u);
}

TEST_F(NetworkTest, EventLoopTimer) {
    ErrorContext ctx;
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    std::atomic<int> timer_count{0};

    // Add one-shot timer
    auto timer_id = loop->addTimer(std::chrono::milliseconds(50), [&timer_count](TimerId /*id*/) {
        timer_count++;
    });
    EXPECT_NE(timer_id, INVALID_TIMER_ID);
    EXPECT_EQ(loop->timerCount(), 1u);

    // Run event loop for a bit
    auto start = std::chrono::steady_clock::now();
    while (timer_count.load() == 0) {
        loop->runOnce(10);
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
            FAIL() << "Timer didn't fire within timeout";
        }
    }

    EXPECT_EQ(timer_count.load(), 1);
}

TEST_F(NetworkTest, EventLoopRepeatingTimer) {
    ErrorContext ctx;
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    std::atomic<int> timer_count{0};

    // Add repeating timer
    auto timer_id = loop->addRepeatingTimer(std::chrono::milliseconds(20),
        [&timer_count](TimerId /*id*/) {
            timer_count++;
        });
    EXPECT_NE(timer_id, INVALID_TIMER_ID);

    // Run for ~100ms
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        loop->runOnce(5);
    }

    // Should have fired multiple times
    EXPECT_GE(timer_count.load(), 3);

    // Cancel timer
    EXPECT_TRUE(loop->cancelTimer(timer_id));
}

TEST_F(NetworkTest, EventLoopReadEvent) {
    ErrorContext ctx;
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    // Create server socket
    auto server = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(server->setReuseAddress(true), Status::OK);
    ASSERT_EQ(server->setNonBlocking(true), Status::OK);

    NetworkAddress server_addr("127.0.0.1", 0, AddressFamily::IPV4);
    ASSERT_EQ(server->bind(server_addr, &ctx), Status::OK);
    ASSERT_EQ(server->listen(10, &ctx), Status::OK);

    auto local = server->getLocalAddress();
    uint16_t port = local->port;

    std::atomic<bool> accept_ready{false};
    socket_t server_fd = server->getFd();

    // Register server socket for read (accept)
    ASSERT_EQ(loop->add(server_fd, EventType::READ,
        [&accept_ready](const EventData& /*event*/) {
            accept_ready = true;
        }), Status::OK);

    // Connect client in background
    std::thread client_thread([port, &ctx]() {
        auto client = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
        NetworkAddress addr("127.0.0.1", port, AddressFamily::IPV4);
        client->connect(addr, &ctx);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    // Poll for events
    auto start = std::chrono::steady_clock::now();
    while (!accept_ready.load()) {
        loop->runOnce(10);
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
            client_thread.join();
            FAIL() << "Accept event didn't fire";
        }
    }

    EXPECT_TRUE(accept_ready.load());
    client_thread.join();
}

// ============================================================================
// Thread Pool Tests
// ============================================================================

TEST_F(NetworkTest, ThreadPoolCreate) {
    ErrorContext ctx;
    auto pool = ThreadPool::create(&ctx);
    ASSERT_NE(pool, nullptr);
    EXPECT_FALSE(pool->isRunning());
}

TEST_F(NetworkTest, ThreadPoolStartStop) {
    ErrorContext ctx;
    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);

    ASSERT_EQ(pool->start(), Status::OK);
    EXPECT_TRUE(pool->isRunning());
    EXPECT_GT(pool->getThreadCount(), 0u);

    pool->stop();
    EXPECT_FALSE(pool->isRunning());
}

TEST_F(NetworkTest, ThreadPoolSubmit) {
    ErrorContext ctx;
    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);
    ASSERT_EQ(pool->start(), Status::OK);

    std::atomic<int> counter{0};

    // Submit tasks
    for (int i = 0; i < 100; ++i) {
        auto id = pool->submit([&counter]() {
            counter++;
        });
        EXPECT_NE(id, INVALID_TASK_ID);
    }

    // Wait for completion
    pool->waitAll();

    EXPECT_EQ(counter.load(), 100);
    EXPECT_EQ(pool->getStats().completed_tasks.load(), 100u);

    pool->stop();
}

TEST_F(NetworkTest, ThreadPoolSubmitWithFuture) {
    ErrorContext ctx;
    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);
    ASSERT_EQ(pool->start(), Status::OK);

    // Submit task with return value
    auto future = pool->submitWithFuture([]() -> int {
        return 42;
    });

    int result = future.get();
    EXPECT_EQ(result, 42);

    pool->stop();
}

TEST_F(NetworkTest, ThreadPoolSchedule) {
    ErrorContext ctx;
    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);
    ASSERT_EQ(pool->start(), Status::OK);

    std::atomic<bool> fired{false};
    auto start = std::chrono::steady_clock::now();

    // Schedule delayed task
    pool->schedule([&fired]() {
        fired = true;
    }, std::chrono::milliseconds(50));

    // Wait for task
    while (!fired.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) {
            FAIL() << "Scheduled task didn't fire";
        }
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 40);

    pool->stop();
}

TEST_F(NetworkTest, ThreadPoolPriority) {
    ErrorContext ctx;
    auto pool = ThreadPool::create(1, &ctx);  // Single thread to ensure ordering
    ASSERT_NE(pool, nullptr);

    // Pause to queue up tasks
    pool->pause();
    ASSERT_EQ(pool->start(), Status::OK);

    std::vector<int> order;
    std::mutex order_mutex;

    // Submit low priority
    pool->submit([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(1);
    }, TaskPriority::LOW);

    // Submit high priority
    pool->submit([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(3);
    }, TaskPriority::HIGH);

    // Submit normal priority
    pool->submit([&]() {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(2);
    }, TaskPriority::NORMAL);

    // Resume and wait
    pool->resume();
    pool->waitAll();

    // High priority should execute first
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 3);  // HIGH
    EXPECT_EQ(order[1], 2);  // NORMAL
    EXPECT_EQ(order[2], 1);  // LOW

    pool->stop();
}

// ============================================================================
// Connection Tests
// ============================================================================

TEST_F(NetworkTest, ConnectionCreate) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    Connection conn(std::move(sock), 1);

    EXPECT_EQ(conn.getId(), 1u);
    EXPECT_EQ(conn.getState(), ConnectionState::NEW);
    EXPECT_FALSE(conn.isReady());
    EXPECT_TRUE(conn.isOpen());
}

TEST_F(NetworkTest, ConnectionBuffers) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    Connection conn(std::move(sock), 1);

    // Test write buffer
    const char* data = "test data";
    conn.appendToWriteBuffer(data, strlen(data));
    EXPECT_TRUE(conn.hasPendingWrites());
    EXPECT_EQ(conn.getWriteBuffer().size(), strlen(data));

    // Clear write buffer
    conn.clearWriteBuffer();
    EXPECT_FALSE(conn.hasPendingWrites());
    EXPECT_TRUE(conn.getWriteBuffer().empty());
}

TEST_F(NetworkTest, ConnectionStateTransitions) {
    ErrorContext ctx;
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    Connection conn(std::move(sock), 1);

    // Initial state
    EXPECT_EQ(conn.getState(), ConnectionState::NEW);

    // State transitions
    conn.setState(ConnectionState::PROTOCOL_DETECTION);
    EXPECT_EQ(conn.getState(), ConnectionState::PROTOCOL_DETECTION);

    conn.setState(ConnectionState::AUTHENTICATING);
    EXPECT_EQ(conn.getState(), ConnectionState::AUTHENTICATING);

    conn.setState(ConnectionState::READY);
    EXPECT_EQ(conn.getState(), ConnectionState::READY);
    EXPECT_TRUE(conn.isReady());
}

// ============================================================================
// Connection Manager Tests
// ============================================================================

TEST_F(NetworkTest, ConnectionManagerCreate) {
    ErrorContext ctx;
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);

    auto manager = ConnectionManager::create(loop.get(), pool.get(), {}, &ctx);
    ASSERT_NE(manager, nullptr);

    EXPECT_EQ(manager->getConnectionCount(), 0u);
}

TEST_F(NetworkTest, ConnectionManagerAccept) {
    ErrorContext ctx;

    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);
    ASSERT_EQ(pool->start(), Status::OK);

    auto manager = ConnectionManager::create(loop.get(), pool.get(), {}, &ctx);
    ASSERT_NE(manager, nullptr);

    // Create a socket to accept
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(sock, nullptr);

    // Accept connection (doesn't need actual connection for basic test)
    auto id = manager->acceptConnection(std::move(sock));
    EXPECT_NE(id, INVALID_CONNECTION_ID);
    EXPECT_EQ(manager->getConnectionCount(), 1u);

    // Get connection
    auto* conn = manager->getConnection(id);
    ASSERT_NE(conn, nullptr);
    EXPECT_EQ(conn->getId(), id);

    // Close connection
    manager->closeConnection(id);
    EXPECT_EQ(manager->getConnectionCount(), 0u);
    EXPECT_EQ(manager->getConnection(id), nullptr);

    pool->stop();
}

TEST_F(NetworkTest, ConnectionManagerEvents) {
    ErrorContext ctx;

    auto loop = EventLoop::create(&ctx);
    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_EQ(pool->start(), Status::OK);

    auto manager = ConnectionManager::create(loop.get(), pool.get(), {}, &ctx);

    std::vector<ConnectionEventType> events;
    std::mutex events_mutex;

    manager->setEventCallback([&](const ConnectionEvent& event) {
        std::lock_guard<std::mutex> lock(events_mutex);
        events.push_back(event.type);
    });

    // Accept connection
    auto sock = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    auto id = manager->acceptConnection(std::move(sock));

    // Should have CONNECTED event
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        ASSERT_GE(events.size(), 1u);
        EXPECT_EQ(events[0], ConnectionEventType::CONNECTED);
    }

    // Close connection
    manager->closeConnection(id);

    // Should have DISCONNECTED event
    {
        std::lock_guard<std::mutex> lock(events_mutex);
        ASSERT_GE(events.size(), 2u);
        EXPECT_EQ(events[1], ConnectionEventType::DISCONNECTED);
    }

    pool->stop();
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(NetworkTest, FullServerClientIntegration) {
    ErrorContext ctx;

    // Create server components
    auto loop = EventLoop::create(&ctx);
    ASSERT_NE(loop, nullptr);

    auto pool = ThreadPool::create(4, &ctx);
    ASSERT_NE(pool, nullptr);
    ASSERT_EQ(pool->start(), Status::OK);

    auto manager = ConnectionManager::create(loop.get(), pool.get(), {}, &ctx);
    ASSERT_NE(manager, nullptr);

    // Create server socket
    auto server = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &ctx);
    ASSERT_NE(server, nullptr);
    ASSERT_EQ(server->setReuseAddress(true), Status::OK);
    ASSERT_EQ(server->setNonBlocking(true), Status::OK);

    NetworkAddress addr("127.0.0.1", 0, AddressFamily::IPV4);
    ASSERT_EQ(server->bind(addr, &ctx), Status::OK);
    ASSERT_EQ(server->listen(10, &ctx), Status::OK);

    auto local = server->getLocalAddress();
    uint16_t port = local->port;

    // Register server for accept
    std::atomic<bool> client_connected{false};
    loop->add(server->getFd(), EventType::READ, [&](const EventData& /*event*/) {
        auto client_sock = server->accept();
        if (client_sock) {
            manager->acceptConnection(std::move(client_sock));
            client_connected = true;
        }
    });

    // Start client in background
    std::thread client_thread([port, &ctx]() {
        auto client = Socket::connect(NetworkAddress("127.0.0.1", port, AddressFamily::IPV4),
                                      {}, &ctx);
        ASSERT_NE(client, nullptr);

        // Send data
        const char* msg = "Hello Server";
        size_t written;
        client->write(msg, strlen(msg), &written);

        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });

    // Run event loop
    auto start = std::chrono::steady_clock::now();
    while (!client_connected.load()) {
        loop->runOnce(10);
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(2)) {
            client_thread.join();
            FAIL() << "Client didn't connect within timeout";
        }
    }

    EXPECT_TRUE(client_connected.load());
    EXPECT_EQ(manager->getConnectionCount(), 1u);

    client_thread.join();
    pool->stop();
}

// ============================================================================
// Network Address Tests
// ============================================================================

TEST(NetworkAddressTest, IPv4ToString) {
    NetworkAddress addr("192.168.1.1", 8080, AddressFamily::IPV4);
    EXPECT_EQ(addr.toString(), "192.168.1.1:8080");
    EXPECT_TRUE(addr.isValid());
}

TEST(NetworkAddressTest, IPv6ToString) {
    NetworkAddress addr("::1", 8080, AddressFamily::IPV6);
    EXPECT_EQ(addr.toString(), "[::1]:8080");
    EXPECT_TRUE(addr.isValid());
}

TEST(NetworkAddressTest, UnixToString) {
    std::string socket_path = uniqueTestSocketPath("test_network_addr");
    NetworkAddress addr(socket_path);
    EXPECT_EQ(addr.toString(), "unix:" + socket_path);
    EXPECT_TRUE(addr.isValid());
}

TEST(NetworkAddressTest, InvalidAddress) {
    NetworkAddress addr;
    EXPECT_FALSE(addr.isValid());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
