/**
 * Socket Abstraction
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * Platform-independent socket wrapper providing both blocking and non-blocking I/O.
 */

#pragma once

#include "scratchbird/network/socket_types.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace scratchbird {
namespace network {

/**
 * Socket class - wrapper around OS socket
 *
 * Thread safety: Individual Socket instances are NOT thread-safe.
 * Each connection should be handled by a single thread.
 */
class Socket {
public:
    /**
     * Create a socket with specified family and type
     */
    static std::unique_ptr<Socket> create(AddressFamily family,
                                          SocketType type = SocketType::STREAM,
                                          core::ErrorContext* ctx = nullptr);

    /**
     * Create a socket from existing file descriptor (takes ownership)
     */
    static std::unique_ptr<Socket> fromFd(socket_t fd, AddressFamily family,
                                          SocketType type = SocketType::STREAM);

    /**
     * Create a connected TCP socket
     */
    static std::unique_ptr<Socket> connect(const NetworkAddress& address,
                                           const SocketOptions& options = SocketOptions(),
                                           core::ErrorContext* ctx = nullptr);

    /**
     * Create a connected Unix domain socket
     */
    static std::unique_ptr<Socket> connectUnix(const std::string& path,
                                               const SocketOptions& options = SocketOptions(),
                                               core::ErrorContext* ctx = nullptr);

    virtual ~Socket();

    // Non-copyable, movable
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // ========================================================================
    // Connection
    // ========================================================================

    /**
     * Connect to remote address (for client sockets)
     */
    core::Status connect(const NetworkAddress& address, core::ErrorContext* ctx = nullptr);

    /**
     * Bind to local address (for server sockets)
     */
    core::Status bind(const NetworkAddress& address, core::ErrorContext* ctx = nullptr);

    /**
     * Start listening for connections (for server sockets)
     */
    core::Status listen(int backlog = DEFAULT_LISTEN_BACKLOG, core::ErrorContext* ctx = nullptr);

    /**
     * Accept a new connection (for server sockets)
     *
     * @param client_address Output: address of connected client
     * @param ctx Error context
     * @return New Socket for the connection, or nullptr on error
     */
    std::unique_ptr<Socket> accept(NetworkAddress* client_address = nullptr,
                                   core::ErrorContext* ctx = nullptr);

    /**
     * Shutdown socket (half-close)
     *
     * @param read Shutdown read side
     * @param write Shutdown write side
     */
    core::Status shutdown(bool read, bool write, core::ErrorContext* ctx = nullptr);

    /**
     * Close the socket
     */
    void close();

    // ========================================================================
    // I/O Operations
    // ========================================================================

    /**
     * Read data from socket
     *
     * @param buffer Buffer to read into
     * @param size Maximum bytes to read
     * @param bytes_read Output: actual bytes read
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status read(void* buffer, size_t size, size_t* bytes_read,
                      core::ErrorContext* ctx = nullptr);

    /**
     * Read exactly 'size' bytes from socket
     *
     * Blocks until all bytes are read or error occurs.
     */
    core::Status readExact(void* buffer, size_t size, core::ErrorContext* ctx = nullptr);

    /**
     * Read with timeout
     *
     * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
     * @return IO_ERROR on timeout
     */
    core::Status readWithTimeout(void* buffer, size_t size, size_t* bytes_read,
                                 uint32_t timeout_ms, core::ErrorContext* ctx = nullptr);

    /**
     * Write data to socket
     *
     * @param buffer Buffer to write from
     * @param size Bytes to write
     * @param bytes_written Output: actual bytes written
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status write(const void* buffer, size_t size, size_t* bytes_written,
                       core::ErrorContext* ctx = nullptr);

    /**
     * Write exactly 'size' bytes to socket
     *
     * Blocks until all bytes are written or error occurs.
     */
    core::Status writeExact(const void* buffer, size_t size, core::ErrorContext* ctx = nullptr);

    /**
     * Write with timeout
     *
     * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
     * @return IO_ERROR on timeout
     */
    core::Status writeWithTimeout(const void* buffer, size_t size, size_t* bytes_written,
                                  uint32_t timeout_ms, core::ErrorContext* ctx = nullptr);

    /**
     * Scatter-gather read (readv)
     */
    core::Status readv(const std::vector<std::pair<void*, size_t>>& buffers,
                       size_t* total_read, core::ErrorContext* ctx = nullptr);

    /**
     * Scatter-gather write (writev)
     */
    core::Status writev(const std::vector<std::pair<const void*, size_t>>& buffers,
                        size_t* total_written, core::ErrorContext* ctx = nullptr);

    /**
     * Peek at data without removing from buffer
     */
    core::Status peek(void* buffer, size_t size, size_t* bytes_peeked,
                      core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Poll/Select
    // ========================================================================

    /**
     * Wait for socket to become readable
     *
     * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 = poll)
     * @return true if readable, false on timeout
     */
    bool waitReadable(int timeout_ms = -1);

    /**
     * Wait for socket to become writable
     *
     * @param timeout_ms Timeout in milliseconds (-1 = infinite, 0 = poll)
     * @return true if writable, false on timeout
     */
    bool waitWritable(int timeout_ms = -1);

    /**
     * Check if data available to read
     */
    bool hasDataAvailable() const;

    /**
     * Get number of bytes available to read
     */
    size_t bytesAvailable() const;

    // ========================================================================
    // Options
    // ========================================================================

    /**
     * Apply socket options
     */
    core::Status applyOptions(const SocketOptions& options, core::ErrorContext* ctx = nullptr);

    /**
     * Set non-blocking mode
     */
    core::Status setNonBlocking(bool enabled, core::ErrorContext* ctx = nullptr);

    /**
     * Set TCP_NODELAY (disable Nagle's algorithm)
     */
    core::Status setTcpNoDelay(bool enabled, core::ErrorContext* ctx = nullptr);

    /**
     * Set SO_KEEPALIVE
     */
    core::Status setKeepAlive(bool enabled, uint32_t idle_sec = 60,
                              uint32_t interval_sec = 10, uint32_t count = 5,
                              core::ErrorContext* ctx = nullptr);

    /**
     * Set SO_REUSEADDR
     */
    core::Status setReuseAddress(bool enabled, core::ErrorContext* ctx = nullptr);

    /**
     * Set SO_REUSEPORT
     */
    core::Status setReusePort(bool enabled, core::ErrorContext* ctx = nullptr);

    /**
     * Set receive buffer size
     */
    core::Status setRecvBufferSize(size_t size, core::ErrorContext* ctx = nullptr);

    /**
     * Set send buffer size
     */
    core::Status setSendBufferSize(size_t size, core::ErrorContext* ctx = nullptr);

    /**
     * Set read timeout
     */
    core::Status setReadTimeout(uint32_t timeout_ms, core::ErrorContext* ctx = nullptr);

    /**
     * Set write timeout
     */
    core::Status setWriteTimeout(uint32_t timeout_ms, core::ErrorContext* ctx = nullptr);

    /**
     * Set SO_LINGER
     */
    core::Status setLinger(bool enabled, int timeout_sec = 0, core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // State & Info
    // ========================================================================

    /**
     * Get raw socket file descriptor
     */
    socket_t getFd() const { return fd_; }

    /**
     * Check if socket is valid (not closed)
     */
    bool isValid() const { return fd_ != INVALID_SOCKET_VALUE; }

    /**
     * Check if socket is open
     */
    bool isOpen() const { return state_ != SocketState::CLOSED; }

    /**
     * Check if socket is connected
     */
    bool isConnected() const {
        return state_ == SocketState::CONNECTED || state_ == SocketState::READY;
    }

    /**
     * Get socket state
     */
    SocketState getState() const { return state_; }

    /**
     * Get address family
     */
    AddressFamily getFamily() const { return family_; }

    /**
     * Get socket type
     */
    SocketType getType() const { return type_; }

    /**
     * Get local address
     */
    std::optional<NetworkAddress> getLocalAddress() const;

    /**
     * Get remote address (peer address)
     */
    std::optional<NetworkAddress> getRemoteAddress() const;

    /**
     * Get connection statistics
     */
    const NetworkStats& getStats() const { return stats_; }

    /**
     * Check if socket is in non-blocking mode
     */
    bool isNonBlocking() const { return non_blocking_; }

    /**
     * Get last error code
     */
    int getLastError() const { return last_error_; }

    /**
     * Get peer credentials (Unix sockets only)
     */
    struct PeerCredentials {
        uint32_t uid = 0;
        uint32_t gid = 0;
        uint32_t pid = 0;
        bool available = false;
    };
    PeerCredentials getPeerCredentials() const;

    /**
     * Release ownership of the file descriptor
     * After this call, the Socket no longer owns the fd and won't close it.
     */
    socket_t release();

protected:
    Socket(socket_t fd, AddressFamily family, SocketType type);

private:
    socket_t fd_ = INVALID_SOCKET_VALUE;
    AddressFamily family_ = AddressFamily::UNSPEC;
    SocketType type_ = SocketType::STREAM;
    SocketState state_ = SocketState::CLOSED;
    bool non_blocking_ = false;
    int last_error_ = 0;
    NetworkStats stats_;

    // Helper to poll socket
    bool pollSocket(short events, int timeout_ms) const;

    // Update state after successful operation
    void updateLastRead() { stats_.last_read = stats_.last_activity = std::chrono::steady_clock::now(); }
    void updateLastWrite() { stats_.last_write = stats_.last_activity = std::chrono::steady_clock::now(); }
};

} // namespace network
} // namespace scratchbird
