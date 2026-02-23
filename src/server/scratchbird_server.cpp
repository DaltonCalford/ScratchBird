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
 * ScratchBird Server Implementation
 *
 * Local Server Architecture - Phase 3
 */

#include "scratchbird/server/scratchbird_server.h"
#include "scratchbird/server/daemon.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace server {

// ============================================================================
// ScratchBirdServer Implementation
// ============================================================================

ScratchBirdServer::ScratchBirdServer(const ServerConfig& config)
    : config_(config)
    , state_(ServerState::CREATED)
    , shutdown_requested_(false)
    , signal_control_(core::createDefaultSignalControl())
{
    stats_.started_at = std::chrono::steady_clock::now();
}

ScratchBirdServer::~ScratchBirdServer() {
    shutdown();
    waitForShutdown(5000);  // Wait up to 5 seconds

    if (signal_control_) {
        (void)signal_control_->uninstall(nullptr);
    }

    removePIDFile();
}

core::Status ScratchBirdServer::start(core::ErrorContext* ctx) {
    state_ = ServerState::STARTING;
    log("Starting ScratchBird server...");

    // Open database
    core::Status status = openDatabase(ctx);
    if (status != core::Status::OK) {
        if (ctx && !ctx->message.empty()) {
            log("Failed to open database: " + ctx->message);
        } else {
            log("Failed to open database");
        }
        state_ = ServerState::STOPPED;
        return status;
    }

    // Start IPC listener
    status = startListener(ctx);
    if (status != core::Status::OK) {
        if (ctx && !ctx->message.empty()) {
            log("Failed to start IPC listener: " + ctx->message);
        } else {
            log("Failed to start IPC listener");
        }
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

    // Set up runtime control signal handling via platform adapter
    if (signal_control_) {
        core::SignalInstallSpec signal_spec;
        signal_spec.enable_shutdown_signal = true;
        signal_spec.enable_reload_signal = true;
        signal_spec.enable_rotate_logs_signal = false;
        signal_spec.enable_dump_stats_signal = false;
        signal_spec.enable_immediate_stop_signal = true;
        signal_spec.ignore_broken_pipe = true;
        (void)signal_control_->install(signal_spec, nullptr);
    }

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
    ProcessId pid = getServerPID(database_path);
    return pid > 0 && isProcessRunning(pid);
}

ProcessId ScratchBirdServer::getServerPID(const std::string& database_path) {
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
    const LocalIPCPolicy ipc_policy = resolveLocalIPCPolicy(config_.ipc_method);
    auto buildIpcConfig = [this](IPCMethod method) {
        IPCServerConfig ipc_config;
        ipc_config.method = method;
        ipc_config.database_name = config_.database_path;
        ipc_config.tcp_port = config_.tcp_port;
        ipc_config.max_connections = config_.max_connections;
        ipc_config.accept_timeout_ms = config_.accept_timeout_ms;
        ipc_config.socket_path = config_.ipc_path;
        return ipc_config;
    };

    auto startWithMethod = [this, &buildIpcConfig](IPCMethod method,
                                                   core::ErrorContext* local_ctx) -> core::Status {
        listener_ = IPCServer::create(buildIpcConfig(method), local_ctx);
        if (!listener_) {
            SET_ERROR_CONTEXT(local_ctx, core::Status::IO_ERROR, "Failed to create IPC server");
            return core::Status::IO_ERROR;
        }

        core::Status status = listener_->listen(local_ctx);
        if (status != core::Status::OK) {
            listener_.reset();
            return status;
        }
        config_.ipc_method = method;
        return core::Status::OK;
    };

    core::ErrorContext primary_ctx;
    core::Status status = startWithMethod(ipc_policy.preferred_method, &primary_ctx);
    if (status == core::Status::OK) {
        return core::Status::OK;
    }

    if (ipc_policy.fallback_enabled && ipc_policy.fallback_method != ipc_policy.preferred_method) {
        log("Primary IPC method failed (" +
            std::string(ipcMethodToString(ipc_policy.preferred_method)) +
            "), attempting fallback " + ipcMethodToString(ipc_policy.fallback_method));

        core::ErrorContext fallback_ctx;
        status = startWithMethod(ipc_policy.fallback_method, &fallback_ctx);
        if (status == core::Status::OK) {
            return core::Status::OK;
        }

        if (ctx) {
            std::string message = "IPC listener startup failed for primary method (" +
                                  std::string(ipcMethodToString(ipc_policy.preferred_method)) +
                                  "): " + primary_ctx.message +
                                  "; fallback (" +
                                  std::string(ipcMethodToString(ipc_policy.fallback_method)) +
                                  "): " + fallback_ctx.message;
            SET_ERROR_CONTEXT(ctx, status, message.c_str());
        }
        return status;
    }

    if (ctx) {
        SET_ERROR_CONTEXT(ctx, status, primary_ctx.message.c_str());
    }
    return status;
}

void ScratchBirdServer::acceptLoop() {
    core::ErrorContext ctx;

    while (!shutdown_requested_ && listener_ && listener_->isListening()) {
        checkControlSignals();
        if (shutdown_requested_) {
            break;
        }

        // Accept connection (uses timeout from config)
        auto connection = listener_->accept(&ctx);

        if (!connection) {
            checkControlSignals();
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

void ScratchBirdServer::checkControlSignals() {
    if (!signal_control_) {
        return;
    }

    core::ControlSignal signal = core::ControlSignal::NONE;
    if (signal_control_->poll(&signal, nullptr) != core::Status::OK) {
        return;
    }

    switch (signal) {
        case core::ControlSignal::SHUTDOWN:
        case core::ControlSignal::IMMEDIATE_STOP:
            log("Shutdown requested by runtime control signal");
            shutdown();
            break;
        case core::ControlSignal::RELOAD:
            // Dedicated runtime reload path is not implemented for this server mode yet.
            // Keep behavior deterministic by requesting graceful restart semantics.
            log("Reload signal received; requesting graceful shutdown for restart");
            shutdown();
            break;
        case core::ControlSignal::ROTATE_LOGS:
        case core::ControlSignal::DUMP_STATS:
        case core::ControlSignal::NONE:
        default:
            break;
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

    file << getCurrentPid() << std::endl;
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

ProcessId readPIDFile(const std::string& pid_path) {
    std::ifstream file(pid_path);
    if (!file) {
        return 0;
    }

    ProcessId pid = 0;
    file >> pid;
    return pid;
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
