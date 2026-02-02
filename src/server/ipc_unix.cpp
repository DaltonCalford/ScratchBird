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
 * Unix Domain Socket IPC Implementation
 *
 * ScratchBird Local Server Architecture - Phase 1
 *
 * This file implements IPC using Unix domain sockets for Linux and macOS.
 * Unix sockets provide 25-50% better performance than TCP for local connections.
 *
 * Features:
 * - Stream-oriented reliable communication
 * - Peer credential passing for authentication
 * - Filesystem-based permissions (chmod 0600)
 * - Automatic socket file cleanup
 */

#include "scratchbird/server/ipc_server.h"

// Unix socket implementation only for Unix-like systems
#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <atomic>
#include <algorithm>

// Linux-specific: SO_PEERCRED
#ifdef __linux__
#include <linux/socket.h>
#endif

// macOS/BSD: LOCAL_PEERCRED
#ifdef __APPLE__
#include <sys/ucred.h>
#endif

namespace scratchbird {
namespace server {

// ============================================================================
// Unix Socket Connection Implementation
// ============================================================================

class UnixSocketConnection final : public IPCConnection {
public:
    explicit UnixSocketConnection(int fd, const std::string& remote_addr)
        : fd_(fd)
        , remote_address_(remote_addr)
        , state_(ConnectionState::CONNECTED)
    {
        stats_.connected_at = std::chrono::steady_clock::now();
        stats_.last_activity = stats_.connected_at;
    }

    ~UnixSocketConnection() override {
        close();
    }

    core::Status read(void* buffer, size_t size, size_t* bytes_read,
                      core::ErrorContext* ctx) override {
        if (fd_ < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection is closed");
            return core::Status::CONNECTION_FAILURE;
        }

        ssize_t result = ::recv(fd_, buffer, size, 0);

        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Timeout
                *bytes_read = 0;
                return core::Status::OK;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("recv() failed: " + std::string(strerror(errno))).c_str());
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
                // Timeout with no data - check if we should continue
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
        if (fd_ < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection is closed");
            return core::Status::CONNECTION_FAILURE;
        }

        ssize_t result = ::send(fd_, buffer, size, MSG_NOSIGNAL);

        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *bytes_written = 0;
                return core::Status::OK;
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                state_ = ConnectionState::DISCONNECTED;
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  "Connection reset by peer");
                return core::Status::CONNECTION_FAILURE;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("send() failed: " + std::string(strerror(errno))).c_str());
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
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
        state_ = ConnectionState::DISCONNECTED;
    }

    bool isOpen() const override {
        return fd_ >= 0 && state_ != ConnectionState::DISCONNECTED;
    }

    ConnectionState getState() const override {
        return state_;
    }

    void setState(ConnectionState state) override {
        state_ = state;
    }

    PeerCredentials getPeerCredentials() const override {
        PeerCredentials creds;

        if (fd_ < 0) {
            return creds;
        }

#ifdef __linux__
        // Linux: SO_PEERCRED
        struct ucred linux_cred;
        socklen_t len = sizeof(linux_cred);

        if (getsockopt(fd_, SOL_SOCKET, SO_PEERCRED, &linux_cred, &len) == 0) {
            creds.uid = linux_cred.uid;
            creds.gid = linux_cred.gid;
            creds.pid = linux_cred.pid;
            creds.available = true;
        }
#elif defined(__APPLE__)
        // macOS: LOCAL_PEERCRED
        struct xucred mac_cred;
        socklen_t len = sizeof(mac_cred);

        if (getsockopt(fd_, SOL_LOCAL, LOCAL_PEERCRED, &mac_cred, &len) == 0) {
            creds.uid = mac_cred.cr_uid;
            creds.gid = mac_cred.cr_groups[0];  // Primary group
            creds.available = true;
            // macOS doesn't provide PID via LOCAL_PEERCRED
            // Would need LOCAL_PEERPID separately
        }

        // Try to get PID on macOS
        pid_t peer_pid = 0;
        len = sizeof(peer_pid);
        if (getsockopt(fd_, SOL_LOCAL, LOCAL_PEERPID, &peer_pid, &len) == 0) {
            creds.pid = static_cast<uint32_t>(peer_pid);
        }
#endif

        return creds;
    }

    std::string getRemoteAddress() const override {
        return remote_address_;
    }

    IPCMethod getMethod() const override {
        return IPCMethod::UNIX_SOCKET;
    }

    ConnectionStats getStats() const override {
        return stats_;
    }

    void setReadTimeout(uint32_t timeout_ms) override {
        if (fd_ < 0) return;

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    void setWriteTimeout(uint32_t timeout_ms) override {
        if (fd_ < 0) return;

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }

private:
    int fd_ = -1;
    std::string remote_address_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    ConnectionStats stats_;
};

// ============================================================================
// Unix Socket Server Implementation
// ============================================================================

class UnixSocketServer final : public IPCServer {
public:
    explicit UnixSocketServer(const IPCServerConfig& config)
        : config_(config)
        , listen_fd_(-1)
        , listening_(false)
        , connection_count_(0)
    {
        // Resolve socket path
        if (config_.socket_path.empty()) {
            socket_path_ = getIPCPath(config_.database_name, IPCMethod::UNIX_SOCKET);
        } else {
            socket_path_ = config_.socket_path;
        }
    }

    ~UnixSocketServer() override {
        close();
    }

    core::Status listen(core::ErrorContext* ctx) override {
        if (listening_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Server is already listening");
            return core::Status::INVALID_ARGUMENT;
        }

        // Create socket
        listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd_ < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("socket() failed: " + std::string(strerror(errno))).c_str());
            return core::Status::IO_ERROR;
        }

        // Remove existing socket file
        ::unlink(socket_path_.c_str());

        // Bind to socket path
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        if (socket_path_.length() >= sizeof(addr.sun_path)) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Socket path too long");
            return core::Status::INVALID_ARGUMENT;
        }

        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        if (::bind(listen_fd_, reinterpret_cast<struct sockaddr*>(&addr),
                   sizeof(addr)) < 0) {
            int bind_errno = errno;
            ::close(listen_fd_);
            listen_fd_ = -1;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("bind() failed: " + std::string(strerror(bind_errno))).c_str());
            return core::Status::IO_ERROR;
        }

        // Set socket file permissions (0600 = user read/write only)
        if (::chmod(socket_path_.c_str(), 0600) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            ::unlink(socket_path_.c_str());
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("chmod() failed: " + std::string(strerror(errno))).c_str());
            return core::Status::IO_ERROR;
        }

        // Start listening
        if (::listen(listen_fd_, SOMAXCONN) < 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
            ::unlink(socket_path_.c_str());
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("listen() failed: " + std::string(strerror(errno))).c_str());
            return core::Status::IO_ERROR;
        }

        // Set non-blocking for accept timeout support
        if (config_.accept_timeout_ms > 0) {
            int flags = fcntl(listen_fd_, F_GETFL, 0);
            fcntl(listen_fd_, F_SETFL, flags | O_NONBLOCK);
        }

        listening_ = true;
        std::fprintf(stderr, "[ipc_debug] unix socket listening path=%s\n",
                     socket_path_.c_str());
        return core::Status::OK;
    }

    std::unique_ptr<IPCConnection> accept(core::ErrorContext* ctx) override {
        if (!listening_ || listen_fd_ < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Server is not listening");
            return nullptr;
        }

        // Wait for connection with timeout using poll()
        if (config_.accept_timeout_ms > 0) {
            struct pollfd pfd;
            pfd.fd = listen_fd_;
            pfd.events = POLLIN;
            pfd.revents = 0;

            int poll_result = ::poll(&pfd, 1, static_cast<int>(config_.accept_timeout_ms));

            if (poll_result == 0) {
                // Timeout - not an error, just no connection
                return nullptr;
            }

            if (poll_result < 0) {
                if (errno == EINTR) {
                    // Interrupted by signal
                    return nullptr;
                }
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                  ("poll() failed: " + std::string(strerror(errno))).c_str());
                return nullptr;
            }
        }

        // Accept connection
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = ::accept(listen_fd_,
                                  reinterpret_cast<struct sockaddr*>(&client_addr),
                                  &client_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No pending connections (non-blocking mode)
                return nullptr;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("accept() failed: " + std::string(strerror(errno))).c_str());
            return nullptr;
        }

        // Check connection limit
        if (connection_count_ >= config_.max_connections) {
            ::close(client_fd);
            SET_ERROR_CONTEXT(ctx, core::Status::TOO_MANY_CONNECTIONS,
                              "Maximum connections exceeded");
            return nullptr;
        }

        // Build remote address string
        std::string remote_addr = "unix:";

        // Try to get peer credentials immediately for the address string
        PeerCredentials creds;
#ifdef __linux__
        struct ucred linux_cred;
        socklen_t len = sizeof(linux_cred);
        if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &linux_cred, &len) == 0) {
            remote_addr += "pid=" + std::to_string(linux_cred.pid);
            remote_addr += ",uid=" + std::to_string(linux_cred.uid);
        }
#elif defined(__APPLE__)
        pid_t peer_pid = 0;
        socklen_t len = sizeof(peer_pid);
        if (getsockopt(client_fd, SOL_LOCAL, LOCAL_PEERPID, &peer_pid, &len) == 0) {
            remote_addr += "pid=" + std::to_string(peer_pid);
        }
#endif

        // Create connection object
        auto conn = std::make_unique<UnixSocketConnection>(client_fd, remote_addr);

        // Set timeouts
        conn->setReadTimeout(config_.read_timeout_ms);
        conn->setWriteTimeout(config_.write_timeout_ms);

        connection_count_++;
        return conn;
    }

    void close() override {
        if (listen_fd_ >= 0) {
            ::close(listen_fd_);
            listen_fd_ = -1;
        }

        // Remove socket file
        if (!socket_path_.empty()) {
            ::unlink(socket_path_.c_str());
        }

        listening_ = false;
        std::fprintf(stderr, "[ipc_debug] unix socket closed path=%s\n",
                     socket_path_.c_str());
    }

    bool isListening() const override {
        return listening_;
    }

    IPCMethod getMethod() const override {
        return IPCMethod::UNIX_SOCKET;
    }

    std::string getAddress() const override {
        return socket_path_;
    }

    uint32_t getConnectionCount() const override {
        return connection_count_;
    }

    const IPCServerConfig& getConfig() const override {
        return config_;
    }

    void decrementConnectionCount() {
        if (connection_count_ > 0) {
            connection_count_--;
        }
    }

private:
    IPCServerConfig config_;
    std::string socket_path_;
    int listen_fd_;
    std::atomic<bool> listening_;
    std::atomic<uint32_t> connection_count_;
};

// ============================================================================
// Unix Socket Client Implementation
// ============================================================================

class UnixSocketClient final : public IPCClient {
public:
    explicit UnixSocketClient(const IPCClientConfig& config)
        : config_(config)
    {
        // Resolve socket path
        if (config_.socket_path.empty()) {
            socket_path_ = getIPCPath(config_.database_name, IPCMethod::UNIX_SOCKET);
        } else {
            socket_path_ = config_.socket_path;
        }
    }

    ~UnixSocketClient() override {
        disconnect();
    }

    core::Status connect(core::ErrorContext* ctx) override {
        if (connection_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Already connected");
            return core::Status::INVALID_ARGUMENT;
        }

        std::fprintf(stderr, "[ipc_debug] unix client connect path=%s\n",
                     socket_path_.c_str());

        // Create socket
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("socket() failed: " + std::string(strerror(errno))).c_str());
            return core::Status::IO_ERROR;
        }

        // Set connect timeout
        if (config_.connect_timeout_ms > 0) {
            // Set non-blocking for timeout support
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        // Build address
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        if (socket_path_.length() >= sizeof(addr.sun_path)) {
            ::close(fd);
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Socket path too long");
            return core::Status::INVALID_ARGUMENT;
        }

        std::strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);

        // Connect
        int result = ::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                                sizeof(addr));

        if (result < 0) {
            if (errno == EINPROGRESS && config_.connect_timeout_ms > 0) {
                // Non-blocking connect in progress, wait for completion
                struct pollfd pfd;
                pfd.fd = fd;
                pfd.events = POLLOUT;
                pfd.revents = 0;

                int poll_result = ::poll(&pfd, 1,
                                          static_cast<int>(config_.connect_timeout_ms));

                if (poll_result == 0) {
                    ::close(fd);
                    SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                      "Connection timeout");
                    return core::Status::CONNECTION_FAILURE;
                }

                if (poll_result < 0) {
                    ::close(fd);
                    SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                      ("poll() failed: " + std::string(strerror(errno))).c_str());
                    return core::Status::IO_ERROR;
                }

                // Check if connect succeeded
                int error = 0;
                socklen_t error_len = sizeof(error);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0 ||
                    error != 0) {
                    ::close(fd);
                    std::fprintf(stderr,
                                 "[ipc_debug] unix client connect failed path=%s err=%s\n",
                                 socket_path_.c_str(),
                                 std::string(strerror(error ? error : errno)).c_str());
                    SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                      ("connect() failed: " +
                                       std::string(strerror(error ? error : errno))).c_str());
                    return core::Status::CONNECTION_FAILURE;
                }
            } else {
                ::close(fd);
                std::fprintf(stderr,
                             "[ipc_debug] unix client connect failed path=%s err=%s\n",
                             socket_path_.c_str(),
                             std::string(strerror(errno)).c_str());
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  ("connect() failed: " + std::string(strerror(errno))).c_str());
                return core::Status::CONNECTION_FAILURE;
            }
        }

        // Set back to blocking mode
        if (config_.connect_timeout_ms > 0) {
            int flags = fcntl(fd, F_GETFL, 0);
            fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
        }

        // Create connection
        std::string remote_addr = "unix:" + socket_path_;
        connection_ = std::make_unique<UnixSocketConnection>(fd, remote_addr);

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
    std::string socket_path_;
    std::unique_ptr<UnixSocketConnection> connection_;
};

// ============================================================================
// Factory Function Implementations (Unix-specific)
// ============================================================================

std::unique_ptr<IPCServer> createUnixSocketServer(const IPCServerConfig& config,
                                                   core::ErrorContext* ctx) {
    return std::make_unique<UnixSocketServer>(config);
}

std::unique_ptr<IPCClient> createUnixSocketClient(const IPCClientConfig& config,
                                                   core::ErrorContext* ctx) {
    return std::make_unique<UnixSocketClient>(config);
}

} // namespace server
} // namespace scratchbird

#endif // Unix-like systems only
