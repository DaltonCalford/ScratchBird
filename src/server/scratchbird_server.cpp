/**
 * ScratchBird Server Implementation
 *
 * Local Server Architecture - Phase 3
 */

#include "scratchbird/server/scratchbird_server.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>
#endif

namespace scratchbird {
namespace server {

// Global server pointer for signal handling
static ScratchBirdServer* g_server_instance = nullptr;

// Signal handler
static void signalHandler(int signal) {
    if (g_server_instance) {
        g_server_instance->shutdown();
    }
}

// ============================================================================
// ScratchBirdServer Implementation
// ============================================================================

ScratchBirdServer::ScratchBirdServer(const ServerConfig& config)
    : config_(config)
    , state_(ServerState::CREATED)
    , shutdown_requested_(false)
{
    stats_.started_at = std::chrono::steady_clock::now();
}

ScratchBirdServer::~ScratchBirdServer() {
    shutdown();
    waitForShutdown(5000);  // Wait up to 5 seconds

    // Clean up global pointer
    if (g_server_instance == this) {
        g_server_instance = nullptr;
    }

    removePIDFile();
}

core::Status ScratchBirdServer::start(core::ErrorContext* ctx) {
    state_ = ServerState::STARTING;
    log("Starting ScratchBird server...");

    // Open database
    core::Status status = openDatabase(ctx);
    if (status != core::Status::OK) {
        state_ = ServerState::STOPPED;
        return status;
    }

    // Start IPC listener
    status = startListener(ctx);
    if (status != core::Status::OK) {
        database_->close();
        state_ = ServerState::STOPPED;
        return status;
    }

    // Write PID file
    status = writePIDFile(ctx);
    if (status != core::Status::OK) {
        log("Warning: Failed to write PID file");
        // Continue anyway - PID file is optional
    }

    // Set up signal handlers
    g_server_instance = this;
#ifndef _WIN32
    signal(SIGTERM, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGHUP, signalHandler);
#endif

    state_ = ServerState::RUNNING;
    stats_.started_at = std::chrono::steady_clock::now();

    log("Server started on " + getIPCPath());
    log("Database: " + config_.database_path);

    // Run accept loop (blocks until shutdown)
    acceptLoop();

    // Cleanup
    state_ = ServerState::STOPPING;
    log("Server shutting down...");

    // Close all sessions
    session_manager_.shutdownAll();
    session_manager_.waitForShutdown(5000);

    // Wait for client threads
    {
        std::lock_guard<std::mutex> lock(client_threads_mutex_);
        for (auto& thread : client_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        client_threads_.clear();
    }

    // Stop listener
    if (listener_) {
        listener_->close();
    }

    // Close database
    if (database_) {
        database_->close();
    }

    removePIDFile();

    state_ = ServerState::STOPPED;
    log("Server stopped.");

    return core::Status::OK;
}

core::Status ScratchBirdServer::startAsync(core::ErrorContext* ctx) {
    // Start in background thread
    accept_thread_ = std::thread([this, ctx]() {
        start(ctx);
    });

    // Wait for server to actually start
    while (state_ == ServerState::CREATED || state_ == ServerState::STARTING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (state_ == ServerState::STOPPED) {
        return core::Status::IO_ERROR;
    }

    return core::Status::OK;
}

void ScratchBirdServer::shutdown() {
    shutdown_requested_ = true;
    state_ = ServerState::STOPPING;

    // Wake up accept loop by closing listener
    if (listener_) {
        listener_->close();
    }
}

bool ScratchBirdServer::waitForShutdown(uint32_t timeout_ms) {
    auto start = std::chrono::steady_clock::now();

    while (state_ != ServerState::STOPPED) {
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Join accept thread if needed
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    return true;
}

ServerStats ScratchBirdServer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ServerStats s = stats_;
    s.active_connections = session_manager_.sessionCount();
    return s;
}

std::string ScratchBirdServer::getIPCPath() const {
    if (!config_.ipc_path.empty()) {
        return config_.ipc_path;
    }
    return server::getIPCPath(config_.database_path, config_.ipc_method);
}

bool ScratchBirdServer::isServerRunning(const std::string& database_path) {
    pid_t pid = getServerPID(database_path);
    return pid > 0 && isProcessRunning(pid);
}

pid_t ScratchBirdServer::getServerPID(const std::string& database_path) {
    std::string pid_path = getPIDFilePath(database_path);
    return readPIDFile(pid_path);
}

core::Status ScratchBirdServer::openDatabase(core::ErrorContext* ctx) {
    database_ = std::make_unique<core::Database>();

    // Try to open existing database
    core::Status status = database_->open(config_.database_path, ctx);

    if (status == core::Status::FILE_NOT_FOUND && config_.auto_create_db) {
        // Create new database
        log("Creating new database: " + config_.database_path);
        status = core::Database::create(config_.database_path, config_.page_size, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        // Open the newly created database
        status = database_->open(config_.database_path, ctx);
    }

    if (status != core::Status::OK) {
        std::string err_msg = "Failed to open database: " + config_.database_path;
        SET_ERROR_CONTEXT(ctx, status, err_msg.c_str());
    } else {
        log("Database opened: " + config_.database_path);
    }

    return status;
}

core::Status ScratchBirdServer::startListener(core::ErrorContext* ctx) {
    // Build IPC server config
    IPCServerConfig ipc_config;
    ipc_config.method = config_.ipc_method;
    ipc_config.database_name = config_.database_path;
    ipc_config.tcp_port = config_.tcp_port;
    ipc_config.max_connections = config_.max_connections;
    ipc_config.accept_timeout_ms = config_.accept_timeout_ms;
    ipc_config.socket_path = config_.ipc_path;

    // Create server
    listener_ = IPCServer::create(ipc_config, ctx);
    if (!listener_) {
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Failed to create IPC server");
        return core::Status::IO_ERROR;
    }

    // Start listening
    core::Status status = listener_->listen(ctx);
    if (status != core::Status::OK) {
        listener_.reset();
        return status;
    }

    return core::Status::OK;
}

void ScratchBirdServer::acceptLoop() {
    core::ErrorContext ctx;

    while (!shutdown_requested_ && listener_ && listener_->isListening()) {
        // Accept connection (uses timeout from config)
        auto connection = listener_->accept(&ctx);

        if (!connection) {
            // Timeout or error - check if we should continue
            if (shutdown_requested_) {
                break;
            }
            continue;
        }

        // Check connection limit
        if (session_manager_.sessionCount() >= config_.max_connections) {
            log("Connection rejected: max connections reached");
            connection->close();
            continue;
        }

        // Update stats
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.total_connections++;
            stats_.last_connection = std::chrono::steady_clock::now();
        }

        // Handle client in a new thread
        std::lock_guard<std::mutex> lock(client_threads_mutex_);

        // Clean up finished threads - remove non-joinable threads
        auto it = client_threads_.begin();
        while (it != client_threads_.end()) {
            if (!it->joinable()) {
                it = client_threads_.erase(it);
            } else {
                ++it;
            }
        }

        // Capture connection for thread
        auto conn_ptr = connection.release();

        // Start new thread
        client_threads_.emplace_back([this, conn_ptr]() {
            std::unique_ptr<IPCConnection> conn(conn_ptr);
            handleClient(std::move(conn));
        });
    }
}

void ScratchBirdServer::handleClient(std::unique_ptr<IPCConnection> connection) {
    // Create session
    ServerSession* session = session_manager_.createSession(connection.get(), database_.get());
    if (!session) {
        log("Failed to create session");
        return;
    }

    log("Client connected: " + session->sessionIdString());

    // Run session (blocks until client disconnects)
    core::ErrorContext ctx;
    session->run();

    log("Client disconnected: " + session->sessionIdString());

    // Update stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_queries += session->stats().queries_executed;
        stats_.failed_queries += session->stats().queries_failed;
    }

    // Remove session
    session_manager_.removeSession(session->sessionId());
}

core::Status ScratchBirdServer::writePIDFile(core::ErrorContext* ctx) {
    std::string pid_path = config_.pid_file;
    if (pid_path.empty()) {
        pid_path = getPIDFilePath(config_.database_path);
    }

    std::ofstream file(pid_path);
    if (!file) {
        std::string err_msg = "Cannot write PID file: " + pid_path;
        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, err_msg.c_str());
        return core::Status::IO_ERROR;
    }

    file << getpid() << std::endl;
    file.close();

    log("PID file written: " + pid_path);
    return core::Status::OK;
}

void ScratchBirdServer::removePIDFile() {
    std::string pid_path = config_.pid_file;
    if (pid_path.empty()) {
        pid_path = getPIDFilePath(config_.database_path);
    }

    std::remove(pid_path.c_str());
}

void ScratchBirdServer::log(const std::string& message) {
    if (config_.verbose) {
        std::cout << "[sb_server] " << message << std::endl;
    }
}

// ============================================================================
// PID File Management
// ============================================================================

std::string getDefaultPIDPath(const std::string& database_path) {
    return getPIDFilePath(database_path);
}

pid_t readPIDFile(const std::string& pid_path) {
    std::ifstream file(pid_path);
    if (!file) {
        return 0;
    }

    pid_t pid = 0;
    file >> pid;
    return pid;
}

bool isProcessRunning(pid_t pid) {
    if (pid <= 0) {
        return false;
    }

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (process) {
        CloseHandle(process);
        return true;
    }
    return false;
#else
    // Use kill with signal 0 to check if process exists
    return kill(pid, 0) == 0;
#endif
}

// ============================================================================
// Server State Utilities
// ============================================================================

const char* serverStateToString(ServerState state) {
    switch (state) {
        case ServerState::CREATED:  return "CREATED";
        case ServerState::STARTING: return "STARTING";
        case ServerState::RUNNING:  return "RUNNING";
        case ServerState::STOPPING: return "STOPPING";
        case ServerState::STOPPED:  return "STOPPED";
        default:                    return "UNKNOWN";
    }
}

}  // namespace server
}  // namespace scratchbird
