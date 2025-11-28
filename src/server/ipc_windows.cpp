/**
 * Windows Named Pipe IPC Implementation
 *
 * ScratchBird Local Server Architecture - Phase 1
 *
 * This file implements IPC using Windows Named Pipes.
 * Named pipes are the standard Windows IPC mechanism for local database connections.
 *
 * Features:
 * - Bi-directional byte-mode communication
 * - Built-in security via Windows ACLs
 * - Efficient for local connections
 * - Native Windows API (no third-party dependencies)
 */

#include "scratchbird/server/ipc_server.h"

// Windows named pipe implementation only for Windows
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sddl.h>  // For security descriptors
#include <string>
#include <atomic>
#include <cstring>

namespace scratchbird {
namespace server {

// Helper: Convert std::string to std::wstring for Windows APIs
static std::wstring stringToWide(const std::string& str) {
    if (str.empty()) return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(),
                                           static_cast<int>(str.length()), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()),
                        &result[0], size_needed);
    return result;
}

// Helper: Convert std::wstring to std::string
static std::string wideToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                           static_cast<int>(wstr.length()),
                                           NULL, 0, NULL, NULL);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.length()),
                        &result[0], size_needed, NULL, NULL);
    return result;
}

// Helper: Get last Windows error as string
static std::string getLastErrorString() {
    DWORD error = GetLastError();
    if (error == 0) return "No error";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&messageBuffer), 0, NULL);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    // Remove trailing newline
    while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) {
        message.pop_back();
    }

    return message + " (error " + std::to_string(error) + ")";
}

// ============================================================================
// Named Pipe Connection Implementation
// ============================================================================

class NamedPipeConnection final : public IPCConnection {
public:
    explicit NamedPipeConnection(HANDLE pipe, const std::string& remote_addr)
        : pipe_(pipe)
        , remote_address_(remote_addr)
        , state_(ConnectionState::CONNECTED)
    {
        stats_.connected_at = std::chrono::steady_clock::now();
        stats_.last_activity = stats_.connected_at;
    }

    ~NamedPipeConnection() override {
        close();
    }

    core::Status read(void* buffer, size_t size, size_t* bytes_read,
                      core::ErrorContext* ctx) override {
        if (pipe_ == INVALID_HANDLE_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection is closed");
            return core::Status::CONNECTION_FAILURE;
        }

        DWORD read_bytes = 0;
        BOOL result = ReadFile(pipe_, buffer, static_cast<DWORD>(size),
                                &read_bytes, NULL);

        if (!result) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                state_ = ConnectionState::DISCONNECTED;
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  "Connection closed by peer");
                return core::Status::CONNECTION_FAILURE;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("ReadFile() failed: " + getLastErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        if (read_bytes == 0) {
            state_ = ConnectionState::DISCONNECTED;
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection closed by peer");
            return core::Status::CONNECTION_FAILURE;
        }

        *bytes_read = static_cast<size_t>(read_bytes);
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
        if (pipe_ == INVALID_HANDLE_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              "Connection is closed");
            return core::Status::CONNECTION_FAILURE;
        }

        DWORD written_bytes = 0;
        BOOL result = WriteFile(pipe_, buffer, static_cast<DWORD>(size),
                                 &written_bytes, NULL);

        if (!result) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
                state_ = ConnectionState::DISCONNECTED;
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  "Connection reset by peer");
                return core::Status::CONNECTION_FAILURE;
            }
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("WriteFile() failed: " + getLastErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        *bytes_written = static_cast<size_t>(written_bytes);
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
        if (pipe_ != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(pipe_);
            DisconnectNamedPipe(pipe_);
            CloseHandle(pipe_);
            pipe_ = INVALID_HANDLE_VALUE;
        }
        state_ = ConnectionState::DISCONNECTED;
    }

    bool isOpen() const override {
        return pipe_ != INVALID_HANDLE_VALUE && state_ != ConnectionState::DISCONNECTED;
    }

    ConnectionState getState() const override {
        return state_;
    }

    void setState(ConnectionState state) override {
        state_ = state;
    }

    PeerCredentials getPeerCredentials() const override {
        PeerCredentials creds;

        if (pipe_ == INVALID_HANDLE_VALUE) {
            return creds;
        }

        // Get client process ID (Windows Vista+)
        ULONG client_pid = 0;
        if (GetNamedPipeClientProcessId(pipe_, &client_pid)) {
            creds.pid = static_cast<uint32_t>(client_pid);
            creds.available = true;
        }

        // Note: Getting UID/GID equivalent on Windows requires impersonation
        // and token inspection, which is more complex. For now, we just get PID.

        return creds;
    }

    std::string getRemoteAddress() const override {
        return remote_address_;
    }

    IPCMethod getMethod() const override {
        return IPCMethod::NAMED_PIPE;
    }

    ConnectionStats getStats() const override {
        return stats_;
    }

    void setReadTimeout(uint32_t timeout_ms) override {
        // Named pipes use COMMTIMEOUTS for timeout control
        COMMTIMEOUTS timeouts;
        if (GetCommTimeouts(pipe_, &timeouts)) {
            timeouts.ReadIntervalTimeout = timeout_ms;
            timeouts.ReadTotalTimeoutConstant = timeout_ms;
            timeouts.ReadTotalTimeoutMultiplier = 0;
            SetCommTimeouts(pipe_, &timeouts);
        }
    }

    void setWriteTimeout(uint32_t timeout_ms) override {
        COMMTIMEOUTS timeouts;
        if (GetCommTimeouts(pipe_, &timeouts)) {
            timeouts.WriteTotalTimeoutConstant = timeout_ms;
            timeouts.WriteTotalTimeoutMultiplier = 0;
            SetCommTimeouts(pipe_, &timeouts);
        }
    }

private:
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::string remote_address_;
    ConnectionState state_ = ConnectionState::DISCONNECTED;
    ConnectionStats stats_;
};

// ============================================================================
// Named Pipe Server Implementation
// ============================================================================

class NamedPipeServer final : public IPCServer {
public:
    explicit NamedPipeServer(const IPCServerConfig& config)
        : config_(config)
        , listening_(false)
        , connection_count_(0)
        , pending_pipe_(INVALID_HANDLE_VALUE)
    {
        // Build pipe name
        if (config_.socket_path.empty()) {
            pipe_name_ = getIPCPath(config_.database_name, IPCMethod::NAMED_PIPE);
        } else {
            pipe_name_ = config_.socket_path;
        }
    }

    ~NamedPipeServer() override {
        close();
    }

    core::Status listen(core::ErrorContext* ctx) override {
        if (listening_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Server is already listening");
            return core::Status::INVALID_ARGUMENT;
        }

        // Create security attributes that allow only the current user
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;

        // Create a security descriptor that grants access only to the current user
        // SDDL: D:(A;;GRGW;;;AU) - Allow Authenticated Users Read/Write
        // For more restrictive: D:(A;;GRGW;;;WD) grants Everyone
        // Use: D:(A;;GA;;;BA)(A;;GA;;;CO) for Admin + Creator Owner
        PSECURITY_DESCRIPTOR pSD = NULL;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
                "D:(A;;GA;;;CO)",  // Creator Owner only (current user)
                SDDL_REVISION_1,
                &pSD,
                NULL)) {
            // Fallback to default security (less restrictive)
            sa.lpSecurityDescriptor = NULL;
        } else {
            sa.lpSecurityDescriptor = pSD;
        }

        // Create the first pipe instance for listening
        std::wstring wide_name = stringToWide(pipe_name_);

        pending_pipe_ = CreateNamedPipeW(
            wide_name.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            8192,   // Output buffer size
            8192,   // Input buffer size
            static_cast<DWORD>(config_.accept_timeout_ms),
            &sa
        );

        if (pSD) {
            LocalFree(pSD);
        }

        if (pending_pipe_ == INVALID_HANDLE_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("CreateNamedPipe() failed: " + getLastErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        listening_ = true;
        return core::Status::OK;
    }

    std::unique_ptr<IPCConnection> accept(core::ErrorContext* ctx) override {
        if (!listening_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Server is not listening");
            return nullptr;
        }

        // Create a new pipe instance if we don't have one pending
        if (pending_pipe_ == INVALID_HANDLE_VALUE) {
            std::wstring wide_name = stringToWide(pipe_name_);

            pending_pipe_ = CreateNamedPipeW(
                wide_name.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                8192,
                8192,
                static_cast<DWORD>(config_.accept_timeout_ms),
                NULL  // Default security for subsequent instances
            );

            if (pending_pipe_ == INVALID_HANDLE_VALUE) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                                  ("CreateNamedPipe() failed: " + getLastErrorString()).c_str());
                return nullptr;
            }
        }

        // Wait for client connection
        BOOL connected = ConnectNamedPipe(pending_pipe_, NULL);
        DWORD error = GetLastError();

        if (!connected && error != ERROR_PIPE_CONNECTED) {
            if (error == ERROR_PIPE_LISTENING) {
                // No client yet - timeout
                return nullptr;
            }

            // Real error
            CloseHandle(pending_pipe_);
            pending_pipe_ = INVALID_HANDLE_VALUE;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("ConnectNamedPipe() failed: " + getLastErrorString()).c_str());
            return nullptr;
        }

        // Check connection limit
        if (connection_count_ >= config_.max_connections) {
            DisconnectNamedPipe(pending_pipe_);
            CloseHandle(pending_pipe_);
            pending_pipe_ = INVALID_HANDLE_VALUE;
            SET_ERROR_CONTEXT(ctx, core::Status::TOO_MANY_CONNECTIONS,
                              "Maximum connections exceeded");
            return nullptr;
        }

        // Connection established - transfer pipe handle to connection
        HANDLE client_pipe = pending_pipe_;
        pending_pipe_ = INVALID_HANDLE_VALUE;

        // Build remote address string
        std::string remote_addr = "pipe:";

        ULONG client_pid = 0;
        if (GetNamedPipeClientProcessId(client_pipe, &client_pid)) {
            remote_addr += "pid=" + std::to_string(client_pid);
        } else {
            remote_addr += pipe_name_;
        }

        // Create connection object
        auto conn = std::make_unique<NamedPipeConnection>(client_pipe, remote_addr);

        // Set timeouts
        conn->setReadTimeout(config_.read_timeout_ms);
        conn->setWriteTimeout(config_.write_timeout_ms);

        connection_count_++;
        return conn;
    }

    void close() override {
        listening_ = false;

        if (pending_pipe_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pending_pipe_);
            pending_pipe_ = INVALID_HANDLE_VALUE;
        }
    }

    bool isListening() const override {
        return listening_;
    }

    IPCMethod getMethod() const override {
        return IPCMethod::NAMED_PIPE;
    }

    std::string getAddress() const override {
        return pipe_name_;
    }

    uint32_t getConnectionCount() const override {
        return connection_count_;
    }

    const IPCServerConfig& getConfig() const override {
        return config_;
    }

private:
    IPCServerConfig config_;
    std::string pipe_name_;
    std::atomic<bool> listening_;
    std::atomic<uint32_t> connection_count_;
    HANDLE pending_pipe_;
};

// ============================================================================
// Named Pipe Client Implementation
// ============================================================================

class NamedPipeClient final : public IPCClient {
public:
    explicit NamedPipeClient(const IPCClientConfig& config)
        : config_(config)
    {
        // Build pipe name
        if (config_.socket_path.empty()) {
            pipe_name_ = getIPCPath(config_.database_name, IPCMethod::NAMED_PIPE);
        } else {
            pipe_name_ = config_.socket_path;
        }
    }

    ~NamedPipeClient() override {
        disconnect();
    }

    core::Status connect(core::ErrorContext* ctx) override {
        if (connection_) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                              "Already connected");
            return core::Status::INVALID_ARGUMENT;
        }

        std::wstring wide_name = stringToWide(pipe_name_);

        // Wait for pipe to become available (if server is starting up)
        DWORD timeout = config_.connect_timeout_ms;
        if (!WaitNamedPipeW(wide_name.c_str(), timeout)) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) {
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  "Server not running: pipe not found");
            } else if (error == ERROR_SEM_TIMEOUT) {
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  "Connection timeout: server busy");
            } else {
                SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                                  ("WaitNamedPipe() failed: " + getLastErrorString()).c_str());
            }
            return core::Status::CONNECTION_FAILURE;
        }

        // Open the pipe
        HANDLE pipe = CreateFileW(
            wide_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,      // No sharing
            NULL,   // Default security
            OPEN_EXISTING,
            0,      // Default attributes
            NULL    // No template
        );

        if (pipe == INVALID_HANDLE_VALUE) {
            SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE,
                              ("CreateFile() failed: " + getLastErrorString()).c_str());
            return core::Status::CONNECTION_FAILURE;
        }

        // Set pipe to byte-read mode (matching server)
        DWORD mode = PIPE_READMODE_BYTE;
        if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
            CloseHandle(pipe);
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              ("SetNamedPipeHandleState() failed: " + getLastErrorString()).c_str());
            return core::Status::IO_ERROR;
        }

        // Create connection
        std::string remote_addr = "pipe:" + pipe_name_;
        connection_ = std::make_unique<NamedPipeConnection>(pipe, remote_addr);

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
    std::string pipe_name_;
    std::unique_ptr<NamedPipeConnection> connection_;
};

// ============================================================================
// Factory Function Implementations (Windows-specific)
// ============================================================================

std::unique_ptr<IPCServer> createNamedPipeServer(const IPCServerConfig& config,
                                                  core::ErrorContext* ctx) {
    return std::make_unique<NamedPipeServer>(config);
}

std::unique_ptr<IPCClient> createNamedPipeClient(const IPCClientConfig& config,
                                                  core::ErrorContext* ctx) {
    return std::make_unique<NamedPipeClient>(config);
}

} // namespace server
} // namespace scratchbird

#endif // _WIN32
