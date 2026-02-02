/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * ScratchBird Service Controller
 *
 * Alpha 3 Phase 3.3: Service Mode & systemd Integration
 *
 * High-level service orchestration combining:
 * - Configuration management (config_parser.h)
 * - Daemon lifecycle (daemon.h)
 * - Protocol listeners (network/)
 * - Database management
 * - Health monitoring
 * - Hot reload support
 */

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>

#ifndef _WIN32
#include <sys/types.h>
#endif

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/security_quorum.h"
#include "scratchbird/server/config_parser.h"
#include "scratchbird/server/daemon.h"
#include "scratchbird/network/socket_types.h"

namespace scratchbird {
namespace server {

// Forward declarations
class ScratchBirdServer;

// ============================================================================
// Service Configuration
// ============================================================================

/**
 * Protocol listener configuration
 */
struct ProtocolConfig {
    network::ProtocolType type;
    std::string bind_address;
    uint16_t port;
    bool enabled;
    bool ssl_required = false;
    uint32_t pool_min = 4;
    uint32_t pool_max = 64;
};

/**
 * Database configuration
 */
struct DatabaseConfig {
    std::string name;               // Logical name
    std::string path;               // File path
    bool auto_create = false;       // Create if not exists
    uint32_t page_size = 16384;     // Page size for new databases
};

/**
 * Complete service configuration
 */
struct ServiceConfig {
    struct DriverDefaults {
        std::string host = "localhost";
        uint16_t port = 3092;
        std::string sslmode = "prefer";  // disable|prefer|require
        uint32_t connect_timeout_ms = 5000;
        std::string default_database;
        std::string default_role;
        bool allow_cleartext = false;
        std::string application_name = "scratchbird_driver";
    };
    // Server mode
    enum class Mode {
        SINGLE_DATABASE,    // One database
        MULTI_DATABASE      // Multiple databases
    };
    Mode mode = Mode::MULTI_DATABASE;

    // Paths
    std::string data_dir = "/var/lib/scratchbird";
    std::string config_file;
    std::string pid_file = "/var/run/scratchbird/sb_server.pid";
    std::string log_file = "/var/log/scratchbird/sb_server.log";

    // Single database mode
    std::string database_path;

    // Network
    std::string bind_address = "0.0.0.0";
    std::vector<ProtocolConfig> protocols;
    std::string control_socket_dir = "/var/run/scratchbird";
    std::string spawn_strategy = "hybrid";
    uint32_t parser_max_requests = 0;
    uint32_t parser_max_age_seconds = 0;

    // Driver bootstrap defaults (native protocol)
    DriverDefaults driver_defaults;

    // Unix socket
    std::string unix_socket = "/var/run/scratchbird/sb.sock";
    mode_t unix_socket_permissions = 0770;
    std::string unix_socket_group;

    // Connections
    uint32_t max_connections = 100;
    uint32_t max_connections_per_user = 0;
    uint32_t max_connections_per_database = 0;
    uint32_t idle_timeout_sec = 3600;
    uint32_t statement_timeout_ms = 0;

    // Threading
    uint32_t worker_threads = 0;    // 0 = auto (CPU cores * 2)

    // Daemon
    DaemonOptions daemon_options;

    // Memory
    uint64_t shared_buffers = 128 * 1024 * 1024;  // 128MB
    uint64_t work_mem = 4 * 1024 * 1024;           // 4MB

    // Shutdown
    uint32_t shutdown_timeout_sec = 30;

    // Logging
    enum class LogLevel {
        DEBUG,
        INFO,
        NOTICE,
        WARNING,
        ERROR
    };
    LogLevel log_level = LogLevel::INFO;
    bool log_connections = true;
    bool log_disconnections = true;
    uint32_t log_slow_queries_ms = 0;  // 0 = disabled

    // Statistics
    bool enable_statistics = true;
    uint16_t prometheus_port = 0;   // 0 = disabled

    // Audit logging
    core::AuditSinkConfig audit_sinks;

    // Security
    core::SecurityQuorumConfig security_quorum;
    core::ConnectionContext::RoleSwitchPolicy role_switch_policy =
        core::ConnectionContext::RoleSwitchPolicy::ERROR;

    // Behavior
    bool foreground = false;        // Run in foreground (don't daemonize)
    bool auto_create_databases = false;

    /**
     * Load configuration from ConfigParser
     */
    void loadFromParser(const ConfigParser& parser);

    /**
     * Get default protocol configurations
     */
    static std::vector<ProtocolConfig> getDefaultProtocols();
};

// ============================================================================
// Service State
// ============================================================================

/**
 * Service lifecycle state
 */
enum class ServiceState : uint8_t {
    UNINITIALIZED = 0,
    INITIALIZING,
    STARTING,
    RUNNING,
    RELOADING,
    STOPPING,
    STOPPED,
    FAILED
};

/**
 * Convert service state to string
 */
const char* serviceStateToString(ServiceState state);

// ============================================================================
// Service Statistics
// ============================================================================

/**
 * Service-wide statistics
 */
struct ServiceStats {
    // Uptime
    std::chrono::steady_clock::time_point started_at;
    std::chrono::steady_clock::time_point last_reload;

    // Connections
    uint64_t total_connections = 0;
    uint32_t active_connections = 0;
    uint32_t peak_connections = 0;

    // Queries
    uint64_t total_queries = 0;
    uint64_t failed_queries = 0;
    uint64_t slow_queries = 0;

    // Databases
    uint32_t active_databases = 0;

    // Protocol breakdown
    std::map<network::ProtocolType, uint64_t> connections_by_protocol;

    /**
     * Get uptime in seconds
     */
    double uptimeSeconds() const;
};

// ============================================================================
// Service Controller
// ============================================================================

/**
 * ServiceController - Main service orchestration class
 *
 * This is the primary entry point for running ScratchBird as a service.
 * It manages:
 * - Configuration loading and hot-reload
 * - Daemonization
 * - Protocol listeners
 * - Database instances
 * - Health monitoring
 * - Graceful shutdown
 *
 * Usage:
 *   ServiceController service;
 *
 *   // Load configuration
 *   service.loadConfig("/etc/scratchbird/sb_server.conf");
 *
 *   // Start service (blocks until shutdown)
 *   service.run();
 */
class ServiceController {
public:
    ServiceController();
    ~ServiceController();

    // Disable copy
    ServiceController(const ServiceController&) = delete;
    ServiceController& operator=(const ServiceController&) = delete;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Load configuration from file
     *
     * @param path Configuration file path
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status loadConfig(const std::string& path, core::ErrorContext* ctx = nullptr);

    /**
     * Load configuration from command-line arguments
     *
     * @param argc Argument count
     * @param argv Argument values
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status parseCommandLine(int argc, char* argv[], core::ErrorContext* ctx = nullptr);

    /**
     * Apply configuration
     *
     * This is called automatically by run(), but can be called
     * separately to validate configuration.
     */
    core::Status applyConfig(core::ErrorContext* ctx = nullptr);

    /**
     * Get current configuration
     */
    const ServiceConfig& config() const { return config_; }

    /**
     * Get mutable configuration (for programmatic setup)
     */
    ServiceConfig& config() { return config_; }

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * Initialize the service
     *
     * Sets up daemon, creates PID file, initializes logging.
     * Call this before run() if you need to do additional setup.
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status initialize(core::ErrorContext* ctx = nullptr);

    /**
     * Run the service
     *
     * This is the main entry point. It:
     * 1. Applies configuration
     * 2. Daemonizes (if configured)
     * 3. Opens databases
     * 4. Starts listeners
     * 5. Enters main event loop
     * 6. Handles shutdown
     *
     * This method blocks until shutdown is requested.
     *
     * @param ctx Error context
     * @return Status::OK on clean shutdown
     */
    core::Status run(core::ErrorContext* ctx = nullptr);

    /**
     * Request graceful shutdown
     */
    void shutdown();

    /**
     * Request immediate shutdown
     */
    void shutdownNow();

    /**
     * Reload configuration
     *
     * Reloads configuration file and applies reloadable settings.
     * Non-reloadable settings (ports, buffer sizes) are logged but not changed.
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status reload(core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // State
    // ========================================================================

    /**
     * Get current service state
     */
    ServiceState state() const { return state_.load(); }

    /**
     * Check if service is running
     */
    bool isRunning() const {
        auto s = state_.load();
        return s == ServiceState::RUNNING || s == ServiceState::RELOADING;
    }

    /**
     * Get service statistics
     */
    ServiceStats getStats() const;

    /**
     * Get daemon instance
     */
    Daemon* daemon() { return daemon_.get(); }

    /**
     * Check if parse requested early exit (help/version/check)
     */
    bool exitRequested() const { return exit_after_parse_; }

    // ========================================================================
    // Databases
    // ========================================================================

    /**
     * Get database by name
     *
     * @param name Database name
     * @return Database pointer, or nullptr if not found
     */
    core::Database* getDatabase(const std::string& name);

    /**
     * Get all database names
     */
    std::vector<std::string> getDatabaseNames() const;

    /**
     * Open a database
     *
     * In multi-database mode, this opens an additional database.
     *
     * @param name Database name
     * @param path Database file path
     * @param create Create if not exists
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status openDatabase(const std::string& name, const std::string& path,
                              bool create = false, core::ErrorContext* ctx = nullptr);

    /**
     * Close a database
     *
     * @param name Database name
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status closeDatabase(const std::string& name, core::ErrorContext* ctx = nullptr);

    // ========================================================================
    // Health
    // ========================================================================

    /**
     * Health check result
     */
    struct HealthStatus {
        bool healthy = false;
        std::string status;
        std::map<std::string, bool> components;
        std::map<std::string, std::string> details;
    };

    /**
     * Get health status
     */
    HealthStatus getHealth() const;

    /**
     * Set health check callback
     */
    using HealthCallback = std::function<void(const HealthStatus&)>;
    void setHealthCallback(HealthCallback callback);

    // ========================================================================
    // Callbacks
    // ========================================================================

    /**
     * Set shutdown callback
     *
     * Called when shutdown is requested (before actual shutdown).
     */
    using ShutdownCallback = std::function<void()>;
    void setShutdownCallback(ShutdownCallback callback);

    /**
     * Set reload callback
     *
     * Called when configuration reload is complete.
     */
    using ReloadCallback = std::function<void()>;
    void setReloadCallback(ReloadCallback callback);

private:
    struct ListenerProcess;

    // Internal methods
    core::Status daemonize(core::ErrorContext* ctx);
    core::Status openDatabases(core::ErrorContext* ctx);
    core::Status startListeners(core::ErrorContext* ctx);
    core::Status stopListeners(core::ErrorContext* ctx);
    void mainLoop();
    void handleSignal(DaemonSignal signal);
    void doShutdown();
    void updateStats();
    void checkListeners();
    bool launchListenerProcess(ListenerProcess& listener, core::ErrorContext* ctx);

    // Log helper
    void log(ServiceConfig::LogLevel level, const std::string& message);

    // Configuration
    ServiceConfig config_;
    std::unique_ptr<ConfigParser> config_parser_;

    // Daemon
    std::unique_ptr<Daemon> daemon_;

    // State
    std::atomic<ServiceState> state_{ServiceState::UNINITIALIZED};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> immediate_shutdown_{false};
    bool exit_after_parse_{false};

    // Databases
    struct DatabaseInstance {
        std::string name;
        std::string path;
        std::unique_ptr<core::Database> owned_database;
        core::Database* database = nullptr;
    };
    std::vector<DatabaseInstance> databases_;
    mutable std::mutex databases_mutex_;

    struct EngineInstance {
        std::string name;
        std::unique_ptr<ScratchBirdServer> server;
    };
    std::vector<EngineInstance> engine_servers_;

    // Statistics
    mutable ServiceStats stats_;
    mutable std::mutex stats_mutex_;

    // Callbacks
    ShutdownCallback shutdown_callback_;
    ReloadCallback reload_callback_;
    HealthCallback health_callback_;

    // Watchdog
    std::thread watchdog_thread_;
    std::atomic<bool> watchdog_running_{false};

    struct ListenerProcess {
        ProtocolConfig config;
        std::string name;
        std::string binary;
        uint64_t start_count = 0;
        uint64_t restart_count = 0;
#ifdef _WIN32
        void* process_handle = nullptr;
        uint32_t process_id = 0;
#else
        pid_t pid = 0;
#endif
        bool running = false;
    };

    std::vector<ListenerProcess> listeners_;
    mutable std::mutex listeners_mutex_;
};

// ============================================================================
// Command-Line Parsing
// ============================================================================

/**
 * Command-line argument parsing result
 */
struct CommandLineArgs {
    // Config file
    std::string config_file;

    // Database
    std::string database_path;
    std::string data_dir;
    bool auto_create = false;

    // Network
    std::string host;
    uint16_t native_port = 0;
    uint16_t pg_port = 0;
    uint16_t mysql_port = 0;
    uint16_t fb_port = 0;
    std::string unix_socket;
    std::string control_socket_dir;
    std::string native_bind;
    std::string pg_bind;
    std::string mysql_bind;
    std::string fb_bind;
    bool enable_native = false;
    bool enable_pg = false;
    bool enable_mysql = false;
    bool enable_fb = false;
    bool disable_native = false;
    bool disable_pg = false;
    bool disable_mysql = false;
    bool disable_fb = false;
    uint32_t native_pool_min = 0;
    uint32_t native_pool_max = 0;
    uint32_t pg_pool_min = 0;
    uint32_t pg_pool_max = 0;
    uint32_t mysql_pool_min = 0;
    uint32_t mysql_pool_max = 0;
    uint32_t fb_pool_min = 0;
    uint32_t fb_pool_max = 0;

    // Server
    uint32_t max_connections = 0;
    uint64_t shared_buffers = 0;

    // Behavior
    bool foreground = false;
    bool single_user = false;
    bool verbose = false;
    bool help = false;
    bool version = false;

    // Check config only
    bool check_config = false;
};

/**
 * Parse command-line arguments
 *
 * @param argc Argument count
 * @param argv Argument values
 * @param args Output parsed arguments
 * @param error Output error message if parsing fails
 * @return true on success
 */
bool parseCommandLineArgs(int argc, char* argv[], CommandLineArgs& args, std::string& error);

/**
 * Print help message
 */
void printHelp(const char* program_name);

/**
 * Print version information
 */
void printVersion();

}  // namespace server
}  // namespace scratchbird
