/**
 * TCP Localhost IPC Implementation
 *
 * ScratchBird Local Server Architecture - Phase 1
 *
 * This file implements IPC using TCP/IP localhost connections.
 * This is a cross-platform fallback that works on all systems.
 *
 * Features:
 * - Cross-platform identical code (Unix/Windows)
 * - Standard debugging tools support (tcpdump, wireshark, netstat)
 * - Natural upgrade path to network protocols (Alpha 3)
 * - Well-understood semantics
 *
 * Note: TCP is ~25-50% slower than Unix sockets for local connections,
 * so it's primarily intended as a fallback or for debugging.
 */

#include "scratchbird/server/ipc_server.h"

#include <atomic>
#include <cstring>
#include <string>

// Cross-platform socket headers
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")

    // Windows socket types
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define closesocket_impl closesocket

    // Initialize Winsock
    static class WinsockInitializer {
    public:
        WinsockInitializer() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinsockInitializer() {
            WSACleanup();
        }
    } g_winsock_init;

#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <errno.h>

    // Unix socket types
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE (-1)
    #define SOCKET_ERROR_VALUE (-1)

    // Use inline function instead of macro to avoid naming conflicts with close() methods
    inline int closesocket_unix(int sock) { return ::close(sock); }
    #define closesocket_impl closesocket_unix
#endif

namespace scratchbird {
namespace server {

// Helper: Get last socket error as string
static std::string getSocketErrorString() {
#ifdef _WIN32
    int error = WSAGetLastError();
    char* msg = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, error, 0, reinterpret_cast<LPSTR>(&msg), 0, NULL);
    std::string result = msg ? msg : "Unknown error";
    LocalFree(msg);
    return result + " (error " + std::to_string(error) + ")";
#else
    return std::string(strerror(errno)) + " (errno " + std::to_string(errno) + ")";
#endif
}

// Helper: Set socket non-blocking
static bool setNonBlocking(socket_t sock, bool non_blocking) {
#ifdef _WIN32
    u_long mode = non_blocking ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(sock, F_SETFL, flags) == 0;
#endif
}

// Helper: Set socket timeouts
static void setSocketTimeout(socket_t sock, uint32_t timeout_ms, bool for_recv) {
#ifdef _WIN32
    DWORD tv = timeout_ms;
    setsockopt(sock, SOL_SOCKET, for_recv ? SO_RCVTIMEO : SO_SNDTIMEO,
               reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, for_recv ? SO_RCVTIMEO : SO_SNDTIMEO,
               &tv, sizeof(tv));
#endif
}

// ============================================================================
// TCP Connection Implementation
// ============================================================================

class TCPConnection : public IPCConnection {
public:
    explicit TCPConnection(socket_t sock, const std::string& remote_addr)
        : sock_(sock)
        , remote_address_(remote_addr)
        , state_(ConnectionState::CONNECTED)
    {
        stats_.connected_at = std::chrono::steady_clock::now();
        stats_.last_activity = stats_.connected_at;

        // Enable TCP_NODELAY for lower latency
        int flag = 1;
        setsockopt(sock_, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&flag), sizeof(flag));
    }

    ~TCPConnection() override {
        close();
    }

    core::Status read(void* buffer, size_t size, size_t* bytes_read,
                      core::ErrorContext* ctx) override {
        if (sock_ == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection is closed");
            return core::Status::CONNECTION_FAILURE;
        }

#ifdef _WIN32
        int result = recv(sock_, static_cast<char*>(buffer),
                           static_cast<int>(size), 0);
#else
        ssize_t result = recv(sock_, buffer, size, 0);
#endif

        if (result < 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                // Timeout
                *bytes_read = 0;
                return core::Status::OK;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("recv() failed: " + getSocketErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        if (result == 0) {
            // Connection closed by peer
            state_ = ConnectionState::DISCONNECTED;
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection closed by peer");
            return core::Status::CONNECTION_FAILURE;
        }

        *bytes_read = static_cast<size_t>(result);
        stats_.bytes_received += *bytes_read;
        stats_.last_activity = std::chrono::steady_clock::now();
        return core::Status::OK;
    }

    core::Status readExact(void* buffer, size_t size,
                           core::ErrorContext* ctx) override {
        uint8_t* ptr = static_cast<uint8_t*>(buffer);
        size_t remaining = size;

        while (remaining > 0) {
            size_t bytes_read = 0;
            core::Status status = read(ptr, remaining, &bytes_read, ctx);

            if (status != core::Status::OK) {
                return status;
            }

            if (bytes_read == 0) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                  "Read timeout: incomplete message");
                return core::Status::IO_ERROR;
            }

            ptr += bytes_read;
            remaining -= bytes_read;
        }

        stats_.messages_received++;
        return core::Status::OK;
    }

    core::Status write(const void* buffer, size_t size, size_t* bytes_written,
                       core::ErrorContext* ctx) override {
        if (sock_ == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection is closed");
            return core::Status::CONNECTION_FAILURE;
        }

#ifdef _WIN32
        int flags = 0;
        int result = send(sock_, static_cast<const char*>(buffer),
                           static_cast<int>(size), flags);
#else
        int flags = MSG_NOSIGNAL;
        ssize_t result = send(sock_, buffer, size, flags);
#endif

        if (result < 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                *bytes_written = 0;
                return core::Status::OK;
            }
#ifdef _WIN32
            if (error == WSAECONNRESET || error == WSAESHUTDOWN) {
#else
            if (errno == EPIPE || errno == ECONNRESET) {
#endif
                state_ = ConnectionState::DISCONNECTED;
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  "Connection reset by peer");
                return core::Status::CONNECTION_FAILURE;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("send() failed: " + getSocketErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        *bytes_written = static_cast<size_t>(result);
        stats_.bytes_sent += *bytes_written;
        stats_.last_activity = std::chrono::steady_clock::now();
        return core::Status::OK;
    }

    core::Status writeExact(const void* buffer, size_t size,
                            core::ErrorContext* ctx) override {
        const uint8_t* ptr = static_cast<const uint8_t*>(buffer);
        size_t remaining = size;

        while (remaining > 0) {
            size_t bytes_written = 0;
            core::Status status = write(ptr, remaining, &bytes_written, ctx);

            if (status != core::Status::OK) {
                return status;
            }

            if (bytes_written == 0) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                  "Write timeout: unable to send data");
                return core::Status::IO_ERROR;
            }

            ptr += bytes_written;
            remaining -= bytes_written;
        }

        stats_.messages_sent++;
        return core::Status::OK;
    }

    void close() override {
        if (sock_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            shutdown(sock_, SD_BOTH);
#else
            shutdown(sock_, SHUT_RDWR);
#endif
            closesocket_impl(sock_);
            sock_ = INVALID_SOCKET_VALUE;
        }
        state_ = ConnectionState::DISCONNECTED;
    }

    bool isOpen() const override {
        return sock_ != INVALID_SOCKET_VALUE && state_ != ConnectionState::DISCONNECTED;
    }

    ConnectionState getState() const override {
        return state_;
    }

    void setState(ConnectionState state) override {
        state_ = state;
    }

    PeerCredentials getPeerCredentials() const override {
        // TCP does not support peer credentials like Unix sockets
        PeerCredentials creds;
        creds.available = false;
        return creds;
    }

    std::string getRemoteAddress() const override {
        return remote_address_;
    }

    IPCMethod getMethod() const override {
        return IPCMethod::TCP_LOCALHOST;
    }

    ConnectionStats getStats() const override {
        return stats_;
    }

    void setReadTimeout(uint32_t timeout_ms) override {
        if (sock_ != INVALID_SOCKET_VALUE) {
            setSocketTimeout(sock_, timeout_ms, true);
        }
    }

    void setWriteTimeout(uint32_t timeout_ms) override {
        if (sock_ != INVALID_SOCKET_VALUE) {
            setSocketTimeout(sock_, timeout_ms, false);
        }
    }

private:
    socket_t sock_ = INVALID_SOCKET_VALUE;
    std::string remote_address_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    ConnectionStats stats_;
};

// ============================================================================
// TCP Server Implementation
// ============================================================================

class TCPServer : public IPCServer {
public:
    explicit TCPServer(const IPCServerConfig& config)
        : config_(config)
        , listen_sock_(INVALID_SOCKET_VALUE)
        , listening_(false)
        , connection_count_(0)
    {
    }

    ~TCPServer() override {
        close();
    }

    core::Status listen(core::ErrorContext* ctx) override {
        if (listening_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Server is already listening");
            return core::Status::INVALID_ARGUMENT;
        }

        // Create socket
        listen_sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_sock_ == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("socket() failed: " + getSocketErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        // Enable SO_REUSEADDR to allow quick restart
        int reuse = 1;
        setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        // Bind to localhost only
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Localhost only
        addr.sin_port = htons(config_.tcp_port);

        if (bind(listen_sock_, reinterpret_cast<struct sockaddr*>(&addr),
                 sizeof(addr)) < 0) {
            closesocket_impl(listen_sock_);
            listen_sock_ = INVALID_SOCKET_VALUE;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("bind() failed: " + getSocketErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        // Start listening
        if (::listen(listen_sock_, SOMAXCONN) < 0) {
            closesocket_impl(listen_sock_);
            listen_sock_ = INVALID_SOCKET_VALUE;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("listen() failed: " + getSocketErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        // Set non-blocking for accept timeout support
        if (config_.accept_timeout_ms > 0) {
            setNonBlocking(listen_sock_, true);
        }

        // Build address string
        address_ = "127.0.0.1:" + std::to_string(config_.tcp_port);

        listening_ = true;
        return core::Status::OK;
    }

    std::unique_ptr<IPCConnection> accept(core::ErrorContext* ctx) override {
        if (!listening_ || listen_sock_ == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Server is not listening");
            return nullptr;
        }

        // Wait for connection with timeout using poll/select
        if (config_.accept_timeout_ms > 0) {
#ifdef _WIN32
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(listen_sock_, &read_set);

            struct timeval tv;
            tv.tv_sec = config_.accept_timeout_ms / 1000;
            tv.tv_usec = (config_.accept_timeout_ms % 1000) * 1000;

            int result = select(0, &read_set, NULL, NULL, &tv);
#else
            struct pollfd pfd;
            pfd.fd = listen_sock_;
            pfd.events = POLLIN;
            pfd.revents = 0;

            int result = poll(&pfd, 1, static_cast<int>(config_.accept_timeout_ms));
#endif

            if (result == 0) {
                // Timeout - not an error
                return nullptr;
            }

            if (result < 0) {
#ifndef _WIN32
                if (errno == EINTR) {
                    return nullptr;
                }
#endif
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                  ("poll/select() failed: " + getSocketErrorString()).c_str());
                return nullptr;
            }
        }

        // Accept connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        socket_t client_sock = ::accept(listen_sock_,
                                         reinterpret_cast<struct sockaddr*>(&client_addr),
                                         &client_len);

        if (client_sock == INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                return nullptr;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("accept() failed: " + getSocketErrorString()).c_str());
            return nullptr;
        }

        // Check connection limit
        if (connection_count_ >= config_.max_connections) {
            closesocket_impl(client_sock);
            SET_ERROR_CONTEXT(ctx, core::Status::TOO_MANY_CONNECTIONS,
                              "Maximum connections exceeded");
            return nullptr;
        }

        // Build remote address string
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        std::string remote_addr = "tcp:" + std::string(ip_str) + ":" +
                                   std::to_string(ntohs(client_addr.sin_port));

        // Set client socket to blocking mode
        setNonBlocking(client_sock, false);

        // Create connection object
        auto conn = std::make_unique<TCPConnection>(client_sock, remote_addr);

        // Set timeouts
        conn->setReadTimeout(config_.read_timeout_ms);
        conn->setWriteTimeout(config_.write_timeout_ms);

        connection_count_++;
        return conn;
    }

    void close() override {
        if (listen_sock_ != INVALID_SOCKET_VALUE) {
            closesocket_impl(listen_sock_);
            listen_sock_ = INVALID_SOCKET_VALUE;
        }
        listening_ = false;
    }

    bool isListening() const override {
        return listening_;
    }

    IPCMethod getMethod() const override {
        return IPCMethod::TCP_LOCALHOST;
    }

    std::string getAddress() const override {
        return address_;
    }

    uint32_t getConnectionCount() const override {
        return connection_count_;
    }

    const IPCServerConfig& getConfig() const override {
        return config_;
    }

private:
    IPCServerConfig config_;
    socket_t listen_sock_;
    std::string address_;
    std::atomic<bool> listening_;
    std::atomic<uint32_t> connection_count_;
};

// ============================================================================
// TCP Client Implementation
// ============================================================================

class TCPClient : public IPCClient {
public:
    explicit TCPClient(const IPCClientConfig& config)
        : config_(config)
    {
    }

    ~TCPClient() override {
        disconnect();
    }

    core::Status connect(core::ErrorContext* ctx) override {
        if (connection_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Already connected");
            return core::Status::INVALID_ARGUMENT;
        }

        // Create socket
        socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("socket() failed: " + getSocketErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        // Build server address
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(config_.tcp_port);

        // Set non-blocking for timeout support
        if (config_.connect_timeout_ms > 0) {
            setNonBlocking(sock, true);
        }

        // Connect
        int result = ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                                sizeof(addr));

        if (result < 0) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK && config_.connect_timeout_ms > 0) {
#else
            if (errno == EINPROGRESS && config_.connect_timeout_ms > 0) {
#endif
                // Non-blocking connect in progress
#ifdef _WIN32
                fd_set write_set, except_set;
                FD_ZERO(&write_set);
                FD_ZERO(&except_set);
                FD_SET(sock, &write_set);
                FD_SET(sock, &except_set);

                struct timeval tv;
                tv.tv_sec = config_.connect_timeout_ms / 1000;
                tv.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;

                int select_result = select(0, NULL, &write_set, &except_set, &tv);
#else
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLOUT;
                pfd.revents = 0;

                int select_result = poll(&pfd, 1,
                                          static_cast<int>(config_.connect_timeout_ms));
#endif

                if (select_result == 0) {
                    closesocket_impl(sock);
                    SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                      "Connection timeout");
                    return core::Status::CONNECTION_FAILURE;
                }

                if (select_result < 0) {
                    closesocket_impl(sock);
                    SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                      ("poll/select() failed: " + getSocketErrorString()).c_str());
                    return core::Status::IO_ERROR;
                }

                // Check if connect succeeded
                int sock_error = 0;
                socklen_t error_len = sizeof(sock_error);
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR,
                               reinterpret_cast<char*>(&sock_error), &error_len) < 0 ||
                    sock_error != 0) {
                    closesocket_impl(sock);
                    SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                      "Connection failed: server not available");
                    return core::Status::CONNECTION_FAILURE;
                }
            } else {
                closesocket_impl(sock);
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  ("connect() failed: " + getSocketErrorString()).c_str());
                return core::Status::CONNECTION_FAILURE;
            }
        }

        // Set back to blocking mode
        setNonBlocking(sock, false);

        // Create connection
        std::string remote_addr = "tcp:127.0.0.1:" + std::to_string(config_.tcp_port);
        connection_ = std::make_unique<TCPConnection>(sock, remote_addr);

        // Set timeouts
        connection_->setReadTimeout(config_.read_timeout_ms);
        connection_->setWriteTimeout(config_.write_timeout_ms);

        return core::Status::OK;
    }

    IPCConnection* getConnection() override {
        return connection_.get();
    }

    bool isConnected() const override {
        return connection_ && connection_->isOpen();
    }

    void disconnect() override {
        if (connection_) {
            connection_->close();
            connection_.reset();
        }
    }

    const IPCClientConfig& getConfig() const override {
        return config_;
    }

private:
    IPCClientConfig config_;
    std::unique_ptr<TCPConnection> connection_;
};

// ============================================================================
// Factory Function Implementations (TCP)
// ============================================================================

std::unique_ptr<IPCServer> createTCPServer(const IPCServerConfig& config,
                                            core::ErrorContext* ctx) {
    return std::make_unique<TCPServer>(config);
}

std::unique_ptr<IPCClient> createTCPClient(const IPCClientConfig& config,
                                            core::ErrorContext* ctx) {
    return std::make_unique<TCPClient>(config);
}

} // namespace server
} // namespace scratchbird
