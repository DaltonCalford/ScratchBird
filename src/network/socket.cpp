/**
 * Socket Implementation
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * Platform-independent socket wrapper providing both blocking and non-blocking I/O.
 */

#include "scratchbird/network/socket.h"

#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <mswsock.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <sys/un.h>
    #include <sys/uio.h>      // for readv/writev
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <poll.h>
    #include <errno.h>
    #ifdef __linux__
        #include <linux/sockios.h>
    #endif
#endif

namespace scratchbird {
namespace network {

// ============================================================================
// Helper Functions
// ============================================================================

namespace {

// Convert AddressFamily to native AF_* constant
int toNativeFamily(AddressFamily family) {
    switch (family) {
        case AddressFamily::IPV4: return AF_INET;
        case AddressFamily::IPV6: return AF_INET6;
        case AddressFamily::UNIX: return AF_UNIX;
        default: return AF_UNSPEC;
    }
}

// Convert SocketType to native SOCK_* constant
int toNativeType(SocketType type) {
    switch (type) {
        case SocketType::STREAM: return SOCK_STREAM;
        case SocketType::DATAGRAM: return SOCK_DGRAM;
        case SocketType::RAW: return SOCK_RAW;
        default: return SOCK_STREAM;
    }
}

// Set socket to non-blocking mode
bool setNonBlockingInternal(socket_t fd, bool enabled) {
#ifdef _WIN32
    u_long mode = enabled ? 1 : 0;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    return fcntl(fd, F_SETFL, flags) == 0;
#endif
}

// Fill sockaddr from NetworkAddress
bool fillSockaddr(const NetworkAddress& addr, struct sockaddr_storage* ss, socklen_t* len) {
    std::memset(ss, 0, sizeof(*ss));

    if (addr.family == AddressFamily::UNIX) {
#ifndef _WIN32
        struct sockaddr_un* sun = reinterpret_cast<struct sockaddr_un*>(ss);
        sun->sun_family = AF_UNIX;
        if (addr.path.length() >= sizeof(sun->sun_path)) {
            return false;  // Path too long
        }
        std::strncpy(sun->sun_path, addr.path.c_str(), sizeof(sun->sun_path) - 1);
        *len = sizeof(struct sockaddr_un);
        return true;
#else
        return false;  // Unix sockets not supported on Windows
#endif
    }

    if (addr.family == AddressFamily::IPV4) {
        struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(ss);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(addr.port);
        if (addr.host.empty() || addr.host == "0.0.0.0") {
            sin->sin_addr.s_addr = INADDR_ANY;
        } else if (inet_pton(AF_INET, addr.host.c_str(), &sin->sin_addr) != 1) {
            // Try DNS resolution
            struct addrinfo hints{}, *result;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(addr.host.c_str(), nullptr, &hints, &result) != 0) {
                return false;
            }
            std::memcpy(&sin->sin_addr,
                       &reinterpret_cast<struct sockaddr_in*>(result->ai_addr)->sin_addr,
                       sizeof(sin->sin_addr));
            freeaddrinfo(result);
        }
        *len = sizeof(struct sockaddr_in);
        return true;
    }

    if (addr.family == AddressFamily::IPV6) {
        struct sockaddr_in6* sin6 = reinterpret_cast<struct sockaddr_in6*>(ss);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(addr.port);
        if (addr.host.empty() || addr.host == "::") {
            sin6->sin6_addr = in6addr_any;
        } else if (inet_pton(AF_INET6, addr.host.c_str(), &sin6->sin6_addr) != 1) {
            // Try DNS resolution
            struct addrinfo hints{}, *result;
            hints.ai_family = AF_INET6;
            hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(addr.host.c_str(), nullptr, &hints, &result) != 0) {
                return false;
            }
            std::memcpy(&sin6->sin6_addr,
                       &reinterpret_cast<struct sockaddr_in6*>(result->ai_addr)->sin6_addr,
                       sizeof(sin6->sin6_addr));
            freeaddrinfo(result);
        }
        *len = sizeof(struct sockaddr_in6);
        return true;
    }

    return false;
}

// Extract NetworkAddress from sockaddr
NetworkAddress fromSockaddr(const struct sockaddr* sa, socklen_t /*len*/) {
    NetworkAddress addr;

    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in* sin = reinterpret_cast<const struct sockaddr_in*>(sa);
        addr.family = AddressFamily::IPV4;
        addr.port = ntohs(sin->sin_port);
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        addr.host = buf;
    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6* sin6 = reinterpret_cast<const struct sockaddr_in6*>(sa);
        addr.family = AddressFamily::IPV6;
        addr.port = ntohs(sin6->sin6_port);
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
        addr.host = buf;
    }
#ifndef _WIN32
    else if (sa->sa_family == AF_UNIX) {
        const struct sockaddr_un* sun = reinterpret_cast<const struct sockaddr_un*>(sa);
        addr.family = AddressFamily::UNIX;
        addr.path = sun->sun_path;
    }
#endif

    return addr;
}

} // anonymous namespace

// ============================================================================
// Socket Implementation
// ============================================================================

Socket::Socket(socket_t fd, AddressFamily family, SocketType type)
    : fd_(fd), family_(family), type_(type), state_(SocketState::CREATED) {
    stats_.connected_at = std::chrono::steady_clock::now();
    stats_.last_activity = stats_.connected_at;
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept
    : fd_(other.fd_), family_(other.family_), type_(other.type_),
      state_(other.state_), non_blocking_(other.non_blocking_),
      last_error_(other.last_error_) {
    other.fd_ = INVALID_SOCKET_VALUE;
    other.state_ = SocketState::CLOSED;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        family_ = other.family_;
        type_ = other.type_;
        state_ = other.state_;
        non_blocking_ = other.non_blocking_;
        last_error_ = other.last_error_;
        other.fd_ = INVALID_SOCKET_VALUE;
        other.state_ = SocketState::CLOSED;
    }
    return *this;
}

// ============================================================================
// Factory Methods
// ============================================================================

std::unique_ptr<Socket> Socket::create(AddressFamily family, SocketType type,
                                        core::ErrorContext* ctx) {
    int native_family = toNativeFamily(family);
    int native_type = toNativeType(type);

    socket_t fd = ::socket(native_family, native_type, 0);
    if (fd == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("socket() failed: " + getSocketErrorString(getLastSocketError())).c_str());
        return nullptr;
    }

    return std::unique_ptr<Socket>(new Socket(fd, family, type));
}

std::unique_ptr<Socket> Socket::fromFd(socket_t fd, AddressFamily family, SocketType type) {
    return std::unique_ptr<Socket>(new Socket(fd, family, type));
}

std::unique_ptr<Socket> Socket::connect(const NetworkAddress& address,
                                         const SocketOptions& options,
                                         core::ErrorContext* ctx) {
    auto sock = create(address.family, SocketType::STREAM, ctx);
    if (!sock) return nullptr;

    if (sock->applyOptions(options, ctx) != core::Status::OK) {
        return nullptr;
    }

    if (sock->connect(address, ctx) != core::Status::OK) {
        return nullptr;
    }

    return sock;
}

std::unique_ptr<Socket> Socket::connectUnix(const std::string& path,
                                             const SocketOptions& options,
                                             core::ErrorContext* ctx) {
#ifndef _WIN32
    NetworkAddress addr(path);
    return connect(addr, options, ctx);
#else
    SET_ERROR_CONTEXT(ctx, core::Status::NOT_SUPPORTED, "Unix sockets not supported on Windows");
    return nullptr;
#endif
}

// ============================================================================
// Connection
// ============================================================================

core::Status Socket::connect(const NetworkAddress& address, core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not created");
        return core::Status::INTERNAL_ERROR;
    }

    struct sockaddr_storage ss;
    socklen_t len;
    if (!fillSockaddr(address, &ss, &len)) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Invalid address");
        return core::Status::INVALID_ARGUMENT;
    }

    state_ = SocketState::CONNECTING;

    int result = ::connect(fd_, reinterpret_cast<struct sockaddr*>(&ss), len);
    if (result == SOCKET_ERROR_VALUE) {
        int err = getLastSocketError();
#ifdef _WIN32
        if (err != WSAEWOULDBLOCK) {
#else
        if (err != EINPROGRESS && err != EWOULDBLOCK) {
#endif
            last_error_ = err;
            state_ = SocketState::ERROR_STATE;
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              ("connect() failed: " + getSocketErrorString(err)).c_str());
            return core::Status::CONNECTION_FAILURE;
        }
        // Non-blocking connect in progress
    }

    state_ = SocketState::CONNECTED;
    stats_.connected_at = std::chrono::steady_clock::now();
    return core::Status::OK;
}

core::Status Socket::bind(const NetworkAddress& address, core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not created");
        return core::Status::INTERNAL_ERROR;
    }

    struct sockaddr_storage ss;
    socklen_t len;
    if (!fillSockaddr(address, &ss, &len)) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Invalid address");
        return core::Status::INVALID_ARGUMENT;
    }

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&ss), len) == SOCKET_ERROR_VALUE) {
        last_error_ = getLastSocketError();
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                          ("bind() failed: " + getSocketErrorString(last_error_)).c_str());
        return core::Status::CONNECTION_FAILURE;
    }

    state_ = SocketState::BOUND;
    return core::Status::OK;
}

core::Status Socket::listen(int backlog, core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not created");
        return core::Status::INTERNAL_ERROR;
    }

    if (::listen(fd_, backlog) == SOCKET_ERROR_VALUE) {
        last_error_ = getLastSocketError();
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                          ("listen() failed: " + getSocketErrorString(last_error_)).c_str());
        return core::Status::CONNECTION_FAILURE;
    }

    state_ = SocketState::LISTENING;
    return core::Status::OK;
}

std::unique_ptr<Socket> Socket::accept(NetworkAddress* client_address, core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not created");
        return nullptr;
    }

    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);

    socket_t client_fd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&ss), &len);
    if (client_fd == INVALID_SOCKET_VALUE) {
        int err = getLastSocketError();
#ifdef _WIN32
        if (err == WSAEWOULDBLOCK) {
#else
        if (err == EAGAIN || err == EWOULDBLOCK) {
#endif
            return nullptr;  // No connection available (non-blocking)
        }
        last_error_ = err;
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                          ("accept() failed: " + getSocketErrorString(err)).c_str());
        return nullptr;
    }

    if (client_address) {
        *client_address = fromSockaddr(reinterpret_cast<struct sockaddr*>(&ss), len);
    }

    auto client = std::unique_ptr<Socket>(new Socket(client_fd, family_, type_));
    client->state_ = SocketState::CONNECTED;
    return client;
}

core::Status Socket::shutdown(bool read, bool write, core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        return core::Status::OK;  // Already closed
    }

    int how;
    if (read && write) {
#ifdef _WIN32
        how = SD_BOTH;
#else
        how = SHUT_RDWR;
#endif
    } else if (read) {
#ifdef _WIN32
        how = SD_RECEIVE;
#else
        how = SHUT_RD;
#endif
    } else if (write) {
#ifdef _WIN32
        how = SD_SEND;
#else
        how = SHUT_WR;
#endif
    } else {
        return core::Status::OK;  // Nothing to do
    }

    if (::shutdown(fd_, how) == SOCKET_ERROR_VALUE) {
        last_error_ = getLastSocketError();
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("shutdown() failed: " + getSocketErrorString(last_error_)).c_str());
        return core::Status::IO_ERROR;
    }

    return core::Status::OK;
}

void Socket::close() {
    if (fd_ != INVALID_SOCKET_VALUE) {
        state_ = SocketState::CLOSING;
#ifdef _WIN32
        ::closesocket(fd_);
#else
        ::close(fd_);
#endif
        fd_ = INVALID_SOCKET_VALUE;
        state_ = SocketState::CLOSED;
    }
}

// ============================================================================
// I/O Operations
// ============================================================================

core::Status Socket::read(void* buffer, size_t size, size_t* bytes_read,
                          core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not open");
        return core::Status::INTERNAL_ERROR;
    }

    ssize_t result;
#ifdef _WIN32
    result = ::recv(fd_, static_cast<char*>(buffer), static_cast<int>(size), 0);
#else
    result = ::recv(fd_, buffer, size, 0);
#endif

    if (result < 0) {
        int err = getLastSocketError();
#ifdef _WIN32
        if (err == WSAEWOULDBLOCK) {
#else
        if (err == EAGAIN || err == EWOULDBLOCK) {
#endif
            *bytes_read = 0;
            return core::Status::OK;
        }
        last_error_ = err;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("recv() failed: " + getSocketErrorString(err)).c_str());
        return core::Status::IO_ERROR;
    }

    if (result == 0) {
        *bytes_read = 0;
        state_ = SocketState::CLOSING;
        return core::Status::CONNECTION_FAILURE;  // EOF - connection closed
    }

    *bytes_read = static_cast<size_t>(result);
    stats_.bytes_received += result;
    updateLastRead();
    return core::Status::OK;
}

core::Status Socket::readExact(void* buffer, size_t size, core::ErrorContext* ctx) {
    size_t total_read = 0;
    uint8_t* ptr = static_cast<uint8_t*>(buffer);

    while (total_read < size) {
        size_t bytes_read;
        auto status = read(ptr + total_read, size - total_read, &bytes_read, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (bytes_read == 0) {
            if (!waitReadable(1000)) {  // Wait up to 1 second
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Timeout waiting for data");
                return core::Status::IO_ERROR;
            }
            continue;
        }
        total_read += bytes_read;
    }

    return core::Status::OK;
}

core::Status Socket::readWithTimeout(void* buffer, size_t size, size_t* bytes_read,
                                     uint32_t timeout_ms, core::ErrorContext* ctx) {
    if (!waitReadable(static_cast<int>(timeout_ms))) {
        *bytes_read = 0;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Read timeout");
        return core::Status::IO_ERROR;
    }
    return read(buffer, size, bytes_read, ctx);
}

core::Status Socket::write(const void* buffer, size_t size, size_t* bytes_written,
                           core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not open");
        return core::Status::INTERNAL_ERROR;
    }

    ssize_t result;
#ifdef _WIN32
    result = ::send(fd_, static_cast<const char*>(buffer), static_cast<int>(size), 0);
#else
    result = ::send(fd_, buffer, size, MSG_NOSIGNAL);
#endif

    if (result < 0) {
        int err = getLastSocketError();
#ifdef _WIN32
        if (err == WSAEWOULDBLOCK) {
#else
        if (err == EAGAIN || err == EWOULDBLOCK) {
#endif
            *bytes_written = 0;
            return core::Status::OK;
        }
        last_error_ = err;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("send() failed: " + getSocketErrorString(err)).c_str());
        return core::Status::IO_ERROR;
    }

    *bytes_written = static_cast<size_t>(result);
    stats_.bytes_sent += result;
    updateLastWrite();
    return core::Status::OK;
}

core::Status Socket::writeExact(const void* buffer, size_t size, core::ErrorContext* ctx) {
    size_t total_written = 0;
    const uint8_t* ptr = static_cast<const uint8_t*>(buffer);

    while (total_written < size) {
        size_t bytes_written;
        auto status = write(ptr + total_written, size - total_written, &bytes_written, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (bytes_written == 0) {
            if (!waitWritable(1000)) {  // Wait up to 1 second
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Timeout waiting to write");
                return core::Status::IO_ERROR;
            }
            continue;
        }
        total_written += bytes_written;
    }

    return core::Status::OK;
}

core::Status Socket::writeWithTimeout(const void* buffer, size_t size, size_t* bytes_written,
                                      uint32_t timeout_ms, core::ErrorContext* ctx) {
    if (!waitWritable(static_cast<int>(timeout_ms))) {
        *bytes_written = 0;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Write timeout");
        return core::Status::IO_ERROR;
    }
    return write(buffer, size, bytes_written, ctx);
}

core::Status Socket::readv(const std::vector<std::pair<void*, size_t>>& buffers,
                           size_t* total_read, core::ErrorContext* ctx) {
#ifdef _WIN32
    // Windows doesn't have native readv, emulate with multiple reads
    *total_read = 0;
    for (const auto& [buf, len] : buffers) {
        size_t bytes_read;
        auto status = read(buf, len, &bytes_read, ctx);
        if (status != core::Status::OK) return status;
        *total_read += bytes_read;
        if (bytes_read < len) break;  // Partial read
    }
    return core::Status::OK;
#else
    std::vector<struct iovec> iov(buffers.size());
    for (size_t i = 0; i < buffers.size(); ++i) {
        iov[i].iov_base = buffers[i].first;
        iov[i].iov_len = buffers[i].second;
    }

    ssize_t result = ::readv(fd_, iov.data(), static_cast<int>(iov.size()));
    if (result < 0) {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            *total_read = 0;
            return core::Status::OK;
        }
        last_error_ = err;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("readv() failed: " + getSocketErrorString(err)).c_str());
        return core::Status::IO_ERROR;
    }

    if (result == 0) {
        *total_read = 0;
        return core::Status::CONNECTION_FAILURE;
    }

    *total_read = static_cast<size_t>(result);
    stats_.bytes_received += result;
    updateLastRead();
    return core::Status::OK;
#endif
}

core::Status Socket::writev(const std::vector<std::pair<const void*, size_t>>& buffers,
                            size_t* total_written, core::ErrorContext* ctx) {
#ifdef _WIN32
    // Windows doesn't have native writev, emulate with multiple writes
    *total_written = 0;
    for (const auto& [buf, len] : buffers) {
        size_t bytes_written;
        auto status = write(buf, len, &bytes_written, ctx);
        if (status != core::Status::OK) return status;
        *total_written += bytes_written;
        if (bytes_written < len) break;  // Partial write
    }
    return core::Status::OK;
#else
    std::vector<struct iovec> iov(buffers.size());
    for (size_t i = 0; i < buffers.size(); ++i) {
        iov[i].iov_base = const_cast<void*>(buffers[i].first);
        iov[i].iov_len = buffers[i].second;
    }

    ssize_t result = ::writev(fd_, iov.data(), static_cast<int>(iov.size()));
    if (result < 0) {
        int err = errno;
        if (err == EAGAIN || err == EWOULDBLOCK) {
            *total_written = 0;
            return core::Status::OK;
        }
        last_error_ = err;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("writev() failed: " + getSocketErrorString(err)).c_str());
        return core::Status::IO_ERROR;
    }

    *total_written = static_cast<size_t>(result);
    stats_.bytes_sent += result;
    updateLastWrite();
    return core::Status::OK;
#endif
}

core::Status Socket::peek(void* buffer, size_t size, size_t* bytes_peeked,
                          core::ErrorContext* ctx) {
    if (fd_ == INVALID_SOCKET_VALUE) {
        SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, "Socket not open");
        return core::Status::INTERNAL_ERROR;
    }

    ssize_t result;
#ifdef _WIN32
    result = ::recv(fd_, static_cast<char*>(buffer), static_cast<int>(size), MSG_PEEK);
#else
    result = ::recv(fd_, buffer, size, MSG_PEEK);
#endif

    if (result < 0) {
        int err = getLastSocketError();
#ifdef _WIN32
        if (err == WSAEWOULDBLOCK) {
#else
        if (err == EAGAIN || err == EWOULDBLOCK) {
#endif
            *bytes_peeked = 0;
            return core::Status::OK;
        }
        last_error_ = err;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                          ("peek() failed: " + getSocketErrorString(err)).c_str());
        return core::Status::IO_ERROR;
    }

    *bytes_peeked = static_cast<size_t>(result);
    return core::Status::OK;
}

// ============================================================================
// Poll/Select
// ============================================================================

bool Socket::pollSocket(short events, int timeout_ms) const {
    if (fd_ == INVALID_SOCKET_VALUE) return false;

#ifdef _WIN32
    fd_set read_fds, write_fds, except_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&except_fds);

    if (events & POLLIN) FD_SET(fd_, &read_fds);
    if (events & POLLOUT) FD_SET(fd_, &write_fds);
    FD_SET(fd_, &except_fds);

    struct timeval tv;
    struct timeval* tv_ptr = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tv_ptr = &tv;
    }

    int result = select(static_cast<int>(fd_) + 1, &read_fds, &write_fds, &except_fds, tv_ptr);
    if (result <= 0) return false;

    if (FD_ISSET(fd_, &except_fds)) return false;
    if ((events & POLLIN) && FD_ISSET(fd_, &read_fds)) return true;
    if ((events & POLLOUT) && FD_ISSET(fd_, &write_fds)) return true;
    return false;
#else
    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = events;
    pfd.revents = 0;

    int result = poll(&pfd, 1, timeout_ms);
    if (result <= 0) return false;

    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return false;
    return (pfd.revents & events) != 0;
#endif
}

bool Socket::waitReadable(int timeout_ms) {
    return pollSocket(POLLIN, timeout_ms);
}

bool Socket::waitWritable(int timeout_ms) {
    return pollSocket(POLLOUT, timeout_ms);
}

bool Socket::hasDataAvailable() const {
    return pollSocket(POLLIN, 0);
}

size_t Socket::bytesAvailable() const {
    if (fd_ == INVALID_SOCKET_VALUE) return 0;

#ifdef _WIN32
    u_long bytes;
    if (ioctlsocket(fd_, FIONREAD, &bytes) == 0) {
        return static_cast<size_t>(bytes);
    }
#else
    int bytes;
    if (ioctl(fd_, FIONREAD, &bytes) == 0) {
        return static_cast<size_t>(bytes);
    }
#endif
    return 0;
}

// ============================================================================
// Options
// ============================================================================

core::Status Socket::applyOptions(const SocketOptions& options, core::ErrorContext* ctx) {
    if (options.reuse_address) {
        auto s = setReuseAddress(true, ctx);
        if (s != core::Status::OK) return s;
    }
    if (options.reuse_port) {
        auto s = setReusePort(true, ctx);
        if (s != core::Status::OK) return s;
    }
    if (options.tcp_nodelay && type_ == SocketType::STREAM &&
        family_ != AddressFamily::UNIX) {
        auto s = setTcpNoDelay(true, ctx);
        if (s != core::Status::OK) return s;
    }
    if (options.tcp_keepalive && type_ == SocketType::STREAM) {
        auto s = setKeepAlive(true, options.keepalive_idle_sec,
                              options.keepalive_interval_sec,
                              options.keepalive_count, ctx);
        if (s != core::Status::OK) return s;
    }
    if (options.recv_buffer_size != DEFAULT_RECV_BUFFER_SIZE) {
        auto s = setRecvBufferSize(options.recv_buffer_size, ctx);
        if (s != core::Status::OK) return s;
    }
    if (options.send_buffer_size != DEFAULT_SEND_BUFFER_SIZE) {
        auto s = setSendBufferSize(options.send_buffer_size, ctx);
        if (s != core::Status::OK) return s;
    }
    if (options.non_blocking) {
        auto s = setNonBlocking(true, ctx);
        if (s != core::Status::OK) return s;
    }

    return core::Status::OK;
}

core::Status Socket::setNonBlocking(bool enabled, core::ErrorContext* ctx) {
    if (!setNonBlockingInternal(fd_, enabled)) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Failed to set non-blocking mode");
        return core::Status::IO_ERROR;
    }
    non_blocking_ = enabled;
    return core::Status::OK;
}

core::Status Socket::setTcpNoDelay(bool enabled, core::ErrorContext* ctx) {
    int flag = enabled ? 1 : 0;
    if (setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(TCP_NODELAY) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status Socket::setKeepAlive(bool enabled, uint32_t idle_sec,
                                  uint32_t interval_sec, uint32_t count,
                                  core::ErrorContext* ctx) {
    int flag = enabled ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE,
                   reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_KEEPALIVE) failed");
        return core::Status::IO_ERROR;
    }

    if (!enabled) return core::Status::OK;

#ifdef __linux__
    int idle = static_cast<int>(idle_sec);
    int interval = static_cast<int>(interval_sec);
    int cnt = static_cast<int>(count);

    setsockopt(fd_, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle));
    setsockopt(fd_, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    setsockopt(fd_, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#elif defined(__APPLE__)
    int idle = static_cast<int>(idle_sec);
    (void)interval_sec;
    (void)count;
    setsockopt(fd_, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
#else
    (void)idle_sec;
    (void)interval_sec;
    (void)count;
#endif

    return core::Status::OK;
}

core::Status Socket::setReuseAddress(bool enabled, core::ErrorContext* ctx) {
    int flag = enabled ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_REUSEADDR) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status Socket::setReusePort(bool enabled, core::ErrorContext* ctx) {
#ifdef SO_REUSEPORT
    int flag = enabled ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT,
                   reinterpret_cast<const char*>(&flag), sizeof(flag)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_REUSEPORT) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
#else
    (void)enabled;
    (void)ctx;
    return core::Status::OK;  // Not supported, ignore
#endif
}

core::Status Socket::setRecvBufferSize(size_t size, core::ErrorContext* ctx) {
    int sz = static_cast<int>(size);
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&sz), sizeof(sz)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_RCVBUF) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status Socket::setSendBufferSize(size_t size, core::ErrorContext* ctx) {
    int sz = static_cast<int>(size);
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<const char*>(&sz), sizeof(sz)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_SNDBUF) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status Socket::setReadTimeout(uint32_t timeout_ms, core::ErrorContext* ctx) {
#ifdef _WIN32
    DWORD tv = timeout_ms;
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) != 0) {
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
#endif
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_RCVTIMEO) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status Socket::setWriteTimeout(uint32_t timeout_ms, core::ErrorContext* ctx) {
#ifdef _WIN32
    DWORD tv = timeout_ms;
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) != 0) {
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
#endif
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_SNDTIMEO) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

core::Status Socket::setLinger(bool enabled, int timeout_sec, core::ErrorContext* ctx) {
    struct linger ling;
    ling.l_onoff = enabled ? 1 : 0;
    ling.l_linger = static_cast<u_short>(timeout_sec);
    if (setsockopt(fd_, SOL_SOCKET, SO_LINGER,
                   reinterpret_cast<const char*>(&ling), sizeof(ling)) != 0) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "setsockopt(SO_LINGER) failed");
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

// ============================================================================
// State & Info
// ============================================================================

std::optional<NetworkAddress> Socket::getLocalAddress() const {
    if (fd_ == INVALID_SOCKET_VALUE) return std::nullopt;

    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if (getsockname(fd_, reinterpret_cast<struct sockaddr*>(&ss), &len) != 0) {
        return std::nullopt;
    }
    return fromSockaddr(reinterpret_cast<struct sockaddr*>(&ss), len);
}

std::optional<NetworkAddress> Socket::getRemoteAddress() const {
    if (fd_ == INVALID_SOCKET_VALUE) return std::nullopt;

    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);
    if (getpeername(fd_, reinterpret_cast<struct sockaddr*>(&ss), &len) != 0) {
        return std::nullopt;
    }
    return fromSockaddr(reinterpret_cast<struct sockaddr*>(&ss), len);
}

Socket::PeerCredentials Socket::getPeerCredentials() const {
    PeerCredentials creds;

#if defined(__linux__)
    if (family_ == AddressFamily::UNIX) {
        struct ucred ucred;
        socklen_t len = sizeof(ucred);
        if (getsockopt(fd_, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == 0) {
            creds.uid = ucred.uid;
            creds.gid = ucred.gid;
            creds.pid = ucred.pid;
            creds.available = true;
        }
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    if (family_ == AddressFamily::UNIX) {
        uid_t euid;
        gid_t egid;
        if (getpeereid(fd_, &euid, &egid) == 0) {
            creds.uid = euid;
            creds.gid = egid;
            creds.available = true;
        }
    }
#endif

    return creds;
}

socket_t Socket::release() {
    socket_t fd = fd_;
    fd_ = INVALID_SOCKET_VALUE;
    state_ = SocketState::CLOSED;
    return fd;
}

} // namespace network
} // namespace scratchbird
