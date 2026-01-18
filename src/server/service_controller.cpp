/**
 * ScratchBird Service Controller Implementation
 *
 * Alpha 3 Phase 3.3: Service Mode & systemd Integration
 */

#include "scratchbird/server/service_controller.h"
#include "scratchbird/version.h"
#include "scratchbird/core/permission_cache.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <getopt.h>

namespace scratchbird {
namespace server {

// ============================================================================
// Service State String Conversion
// ============================================================================

const char* serviceStateToString(ServiceState state) {
    switch (state) {
        case ServiceState::UNINITIALIZED: return "UNINITIALIZED";
        case ServiceState::INITIALIZING: return "INITIALIZING";
        case ServiceState::STARTING: return "STARTING";
        case ServiceState::RUNNING: return "RUNNING";
        case ServiceState::RELOADING: return "RELOADING";
        case ServiceState::STOPPING: return "STOPPING";
        case ServiceState::STOPPED: return "STOPPED";
        case ServiceState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// ServiceConfig Implementation
// ============================================================================

void ServiceConfig::loadFromParser(const ConfigParser& parser) {
    // Server section
    const ConfigSection* server = parser.section("server");
    if (server) {
        std::string mode_str = server->getString("mode", "multi-database");
        if (mode_str == "single-database") {
            mode = Mode::SINGLE_DATABASE;
        } else {
            mode = Mode::MULTI_DATABASE;
        }

        data_dir = server->getString("data_dir", data_dir);
        database_path = server->getString("database", database_path);
        pid_file = server->getString("pid_file", pid_file);
        shutdown_timeout_sec = static_cast<uint32_t>(server->getInt("shutdown_timeout", shutdown_timeout_sec));
        worker_threads = static_cast<uint32_t>(server->getInt("worker_threads", worker_threads));
        max_connections = static_cast<uint32_t>(server->getInt("max_connections", max_connections));
        max_connections_per_user = static_cast<uint32_t>(server->getInt("max_connections_per_user", max_connections_per_user));
        max_connections_per_database = static_cast<uint32_t>(server->getInt("max_connections_per_database", max_connections_per_database));
        idle_timeout_sec = static_cast<uint32_t>(server->getDuration("idle_timeout", idle_timeout_sec * 1000) / 1000);
        statement_timeout_ms = static_cast<uint32_t>(server->getDuration("statement_timeout", statement_timeout_ms));
        auto_create_databases = server->getBool("auto_create", auto_create_databases);
    }

    // Network section
    const ConfigSection* network = parser.section("network");
    if (network) {
        bind_address = network->getString("bind_address", bind_address);
        unix_socket = network->getString("unix_socket", unix_socket);
        unix_socket_permissions = static_cast<mode_t>(network->getInt("unix_socket_permissions", unix_socket_permissions));
        unix_socket_group = network->getString("unix_socket_group", unix_socket_group);

        // Protocol ports
        protocols.clear();

        uint16_t native_port = static_cast<uint16_t>(network->getInt("native_port", 3092));
        if (native_port > 0) {
            protocols.push_back({network::ProtocolType::NATIVE, bind_address, native_port, true});
        }

        uint16_t pg_port = static_cast<uint16_t>(network->getInt("pg_port", 5432));
        if (pg_port > 0) {
            protocols.push_back({network::ProtocolType::POSTGRESQL, bind_address, pg_port, true});
        }

        uint16_t mysql_port = static_cast<uint16_t>(network->getInt("mysql_port", 3306));
        if (mysql_port > 0) {
            protocols.push_back({network::ProtocolType::MYSQL, bind_address, mysql_port, true});
        }

        uint16_t fb_port = static_cast<uint16_t>(network->getInt("fb_port", 3050));
        if (fb_port > 0) {
            protocols.push_back({network::ProtocolType::FIREBIRD, bind_address, fb_port, true});
        }
    } else {
        // Use defaults
        protocols = getDefaultProtocols();
    }

    // Memory section
    const ConfigSection* memory = parser.section("memory");
    if (memory) {
        shared_buffers = memory->getSize("shared_buffers", shared_buffers);
        work_mem = memory->getSize("work_mem", work_mem);
    }

    // Logging section
    const ConfigSection* logging = parser.section("logging");
    if (logging) {
        std::string level_str = logging->getString("level", "info");
        if (level_str == "debug") log_level = LogLevel::DEBUG;
        else if (level_str == "info") log_level = LogLevel::INFO;
        else if (level_str == "notice") log_level = LogLevel::NOTICE;
        else if (level_str == "warning") log_level = LogLevel::WARNING;
        else if (level_str == "error") log_level = LogLevel::ERROR;

        log_file = logging->getString("file", log_file);
        log_connections = logging->getBool("log_connections", log_connections);
        log_disconnections = logging->getBool("log_disconnections", log_disconnections);
        log_slow_queries_ms = static_cast<uint32_t>(logging->getDuration("log_slow_queries", log_slow_queries_ms));
    }

    // Statistics section
    const ConfigSection* stats = parser.section("statistics");
    if (stats) {
        enable_statistics = stats->getBool("enabled", enable_statistics);
        prometheus_port = static_cast<uint16_t>(stats->getInt("prometheus_port", prometheus_port));
    }

    // Audit section
    const ConfigSection* audit = parser.section("audit");
    if (audit) {
        audit_sinks.enable_catalog = audit->getBool("sink_catalog", audit_sinks.enable_catalog);
        audit_sinks.enable_file = audit->getBool("sink_file", audit_sinks.enable_file);
        audit_sinks.enable_broadcast = audit->getBool("sink_broadcast",
                                                     audit_sinks.enable_broadcast);
        if (audit->has("sink_kafka")) {
            audit_sinks.enable_broadcast = audit->getBool("sink_kafka",
                                                         audit_sinks.enable_broadcast);
        }
        audit_sinks.keep_in_memory = audit->getBool("keep_in_memory",
                                                    audit_sinks.keep_in_memory);
        audit_sinks.file_path = audit->getString("file_path", audit_sinks.file_path);
    }

    // Security section
    const ConfigSection* security = parser.section("security");
    if (security) {
        if (security->has("security_quorum_n")) {
            security_quorum.required = static_cast<uint32_t>(
                security->getInt("security_quorum_n", security_quorum.required));
        } else {
            security_quorum.required = static_cast<uint32_t>(
                security->getInt("quorum_n", security_quorum.required));
        }

        if (security->has("security_quorum_m")) {
            security_quorum.total = static_cast<uint32_t>(
                security->getInt("security_quorum_m", security_quorum.total));
        } else {
            security_quorum.total = static_cast<uint32_t>(
                security->getInt("quorum_m", security_quorum.total));
        }

        std::string failure_mode = security->getString("quorum_failure_mode", "fail_open");
        if (security->has("security_quorum_failure_mode")) {
            failure_mode = security->getString("security_quorum_failure_mode", failure_mode);
        }
        if (failure_mode == "fail_closed") {
            security_quorum.failure_mode = core::QuorumFailureMode::FAIL_CLOSED;
        } else if (failure_mode == "require_remote") {
            security_quorum.failure_mode = core::QuorumFailureMode::REQUIRE_REMOTE;
        } else {
            security_quorum.failure_mode = core::QuorumFailureMode::FAIL_OPEN;
        }

        std::string role_action = security->getString("role_switch_default_action", "error");
        if (role_action == "commit") {
            role_switch_policy = core::ConnectionContext::RoleSwitchPolicy::COMMIT;
        } else if (role_action == "rollback") {
            role_switch_policy = core::ConnectionContext::RoleSwitchPolicy::ROLLBACK;
        } else if (role_action == "defer") {
            role_switch_policy = core::ConnectionContext::RoleSwitchPolicy::DEFER;
        } else {
            role_switch_policy = core::ConnectionContext::RoleSwitchPolicy::ERROR;
        }
    }

    // Update daemon options
    daemon_options.pid_file = pid_file;
    daemon_options.shutdown_timeout_sec = shutdown_timeout_sec;
    daemon_options.daemonize = !foreground;
}

std::vector<ProtocolConfig> ServiceConfig::getDefaultProtocols() {
    return {
        {network::ProtocolType::NATIVE, "0.0.0.0", 3092, true},
        {network::ProtocolType::POSTGRESQL, "0.0.0.0", 5432, true},
        {network::ProtocolType::MYSQL, "0.0.0.0", 3306, true},
        {network::ProtocolType::FIREBIRD, "0.0.0.0", 3050, true}
    };
}

// ============================================================================
// ServiceStats Implementation
// ============================================================================

double ServiceStats::uptimeSeconds() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - started_at).count();
}

// ============================================================================
// ServiceController Implementation
// ============================================================================

ServiceController::ServiceController()
    : config_parser_(std::make_unique<ConfigParser>()) {}

ServiceController::~ServiceController() {
    if (state_ != ServiceState::STOPPED && state_ != ServiceState::UNINITIALIZED) {
        shutdown();
        // Wait briefly for shutdown
        for (int i = 0; i < 50 && state_ != ServiceState::STOPPED; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    watchdog_running_ = false;
    if (watchdog_thread_.joinable()) {
        watchdog_thread_.join();
    }
}

core::Status ServiceController::loadConfig(const std::string& path, core::ErrorContext* ctx) {
    config_.config_file = path;

    core::Status status = config_parser_->parseFile(path, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    config_.loadFromParser(*config_parser_);
    return core::Status::OK;
}

core::Status ServiceController::parseCommandLine(int argc, char* argv[], core::ErrorContext* ctx) {
    CommandLineArgs args;
    std::string error;

    if (!parseCommandLineArgs(argc, argv, args, error)) {
        if (ctx) {
            SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, error.c_str());
        }
        return core::Status::INVALID_ARGUMENT;
    }

    // Handle help/version
    if (args.help) {
        printHelp(argv[0]);
        return core::Status::OK;
    }

    if (args.version) {
        printVersion();
        return core::Status::OK;
    }

    // Load config file
    std::string config_path = args.config_file;
    if (config_path.empty()) {
        config_path = findConfigFile();
    }

    if (!config_path.empty() && std::filesystem::exists(config_path)) {
        core::Status status = loadConfig(config_path, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Command-line overrides
    if (!args.database_path.empty()) {
        config_.mode = ServiceConfig::Mode::SINGLE_DATABASE;
        config_.database_path = args.database_path;
    }

    if (!args.data_dir.empty()) {
        config_.mode = ServiceConfig::Mode::MULTI_DATABASE;
        config_.data_dir = args.data_dir;
    }

    if (!args.host.empty()) {
        config_.bind_address = args.host;
        for (auto& proto : config_.protocols) {
            proto.bind_address = args.host;
        }
    }

    if (args.native_port > 0) {
        for (auto& proto : config_.protocols) {
            if (proto.type == network::ProtocolType::NATIVE) {
                proto.port = args.native_port;
            }
        }
    }

    if (args.pg_port != 0) {  // 0 means use default, explicit 0 disables
        for (auto& proto : config_.protocols) {
            if (proto.type == network::ProtocolType::POSTGRESQL) {
                proto.port = args.pg_port;
                proto.enabled = (args.pg_port > 0);
            }
        }
    }

    if (args.mysql_port != 0) {
        for (auto& proto : config_.protocols) {
            if (proto.type == network::ProtocolType::MYSQL) {
                proto.port = args.mysql_port;
                proto.enabled = (args.mysql_port > 0);
            }
        }
    }

    if (args.fb_port != 0) {
        for (auto& proto : config_.protocols) {
            if (proto.type == network::ProtocolType::FIREBIRD) {
                proto.port = args.fb_port;
                proto.enabled = (args.fb_port > 0);
            }
        }
    }

    if (!args.unix_socket.empty()) {
        config_.unix_socket = args.unix_socket;
    }

    if (args.max_connections > 0) {
        config_.max_connections = args.max_connections;
    }

    if (args.shared_buffers > 0) {
        config_.shared_buffers = args.shared_buffers;
    }

    if (args.auto_create) {
        config_.auto_create_databases = true;
    }

    if (args.foreground) {
        config_.foreground = true;
        config_.daemon_options.daemonize = false;
    }

    if (args.verbose) {
        config_.log_level = ServiceConfig::LogLevel::DEBUG;
    }

    // Config check mode
    if (args.check_config) {
        std::cout << "Configuration OK\n";
        return core::Status::OK;
    }

    return core::Status::OK;
}

core::Status ServiceController::applyConfig(core::ErrorContext* ctx) {
    // Update daemon options from config
    config_.daemon_options.pid_file = config_.pid_file;
    config_.daemon_options.shutdown_timeout_sec = config_.shutdown_timeout_sec;
    config_.daemon_options.daemonize = !config_.foreground;

    return core::Status::OK;
}

core::Status ServiceController::initialize(core::ErrorContext* ctx) {
    state_ = ServiceState::INITIALIZING;

    // Apply configuration
    core::Status status = applyConfig(ctx);
    if (status != core::Status::OK) {
        state_ = ServiceState::FAILED;
        return status;
    }

    // Create daemon
    daemon_ = std::make_unique<Daemon>(config_.daemon_options);

    return core::Status::OK;
}

core::Status ServiceController::run(core::ErrorContext* ctx) {
    // Initialize if not done
    if (state_ == ServiceState::UNINITIALIZED) {
        core::Status status = initialize(ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    state_ = ServiceState::STARTING;

    // Daemonize
    core::Status status = daemonize(ctx);
    if (status != core::Status::OK) {
        state_ = ServiceState::FAILED;
        return status;
    }

    // Parent process exits here during daemonization
    if (daemon_ && daemon_->isParent()) {
        return core::Status::OK;
    }

    // Open databases
    status = openDatabases(ctx);
    if (status != core::Status::OK) {
        state_ = ServiceState::FAILED;
        doShutdown();
        return status;
    }

    // Start listeners
    status = startListeners(ctx);
    if (status != core::Status::OK) {
        state_ = ServiceState::FAILED;
        doShutdown();
        return status;
    }

    // Record start time
    stats_.started_at = std::chrono::steady_clock::now();

    // Notify systemd we're ready
    if (daemon_) {
        daemon_->notifyReady();
        daemon_->notifyStatus("Running with " + std::to_string(stats_.active_databases) + " database(s)");
    }

    state_ = ServiceState::RUNNING;
    log(ServiceConfig::LogLevel::INFO, "Service started successfully");

    // Start watchdog thread (for systemd watchdog)
    uint64_t watchdog_usec = SystemdNotify::getWatchdogUsec();
    if (watchdog_usec > 0) {
        watchdog_running_ = true;
        watchdog_thread_ = std::thread([this, watchdog_usec]() {
            uint64_t interval = watchdog_usec / 2;  // Ping at half the interval
            while (watchdog_running_) {
                std::this_thread::sleep_for(std::chrono::microseconds(interval));
                if (watchdog_running_ && daemon_) {
                    daemon_->notifyWatchdog();
                }
            }
        });
    }

    // Main loop
    mainLoop();

    // Shutdown
    doShutdown();

    state_ = ServiceState::STOPPED;
    log(ServiceConfig::LogLevel::INFO, "Service stopped");

    return core::Status::OK;
}

void ServiceController::shutdown() {
    shutdown_requested_ = true;
    if (daemon_) {
        daemon_->requestShutdown();
    }
}

void ServiceController::shutdownNow() {
    immediate_shutdown_ = true;
    shutdown();
}

core::Status ServiceController::reload(core::ErrorContext* ctx) {
    if (state_ != ServiceState::RUNNING) {
        return core::Status::INTERNAL_ERROR;
    }

    state_ = ServiceState::RELOADING;
    if (daemon_) {
        daemon_->notifyReloading();
    }

    log(ServiceConfig::LogLevel::INFO, "Reloading configuration...");

    // Reload config file
    if (!config_.config_file.empty()) {
        ConfigParser new_parser;
        core::Status status = new_parser.parseFile(config_.config_file, ctx);
        if (status != core::Status::OK) {
            log(ServiceConfig::LogLevel::ERROR, "Failed to reload configuration");
            state_ = ServiceState::RUNNING;
            return status;
        }

        // Apply reloadable settings
        ServiceConfig new_config = config_;
        new_config.loadFromParser(new_parser);

        // Update reloadable settings
        config_.max_connections = new_config.max_connections;
        config_.max_connections_per_user = new_config.max_connections_per_user;
        config_.max_connections_per_database = new_config.max_connections_per_database;
        config_.idle_timeout_sec = new_config.idle_timeout_sec;
        config_.statement_timeout_ms = new_config.statement_timeout_ms;
        config_.log_level = new_config.log_level;
        config_.log_connections = new_config.log_connections;
        config_.log_disconnections = new_config.log_disconnections;
        config_.log_slow_queries_ms = new_config.log_slow_queries_ms;

        // Note: ports, bind address, buffer sizes are NOT reloadable
    }

    stats_.last_reload = std::chrono::steady_clock::now();

    if (reload_callback_) {
        reload_callback_();
    }

    if (daemon_) {
        daemon_->notifyReady();
    }

    state_ = ServiceState::RUNNING;
    log(ServiceConfig::LogLevel::INFO, "Configuration reloaded");

    return core::Status::OK;
}

ServiceStats ServiceController::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

core::Database* ServiceController::getDatabase(const std::string& name) {
    std::lock_guard<std::mutex> lock(databases_mutex_);
    for (auto& db : databases_) {
        if (db.name == name) {
            return db.database.get();
        }
    }
    return nullptr;
}

std::vector<std::string> ServiceController::getDatabaseNames() const {
    std::lock_guard<std::mutex> lock(databases_mutex_);
    std::vector<std::string> names;
    names.reserve(databases_.size());
    for (const auto& db : databases_) {
        names.push_back(db.name);
    }
    return names;
}

core::Status ServiceController::openDatabase(const std::string& name, const std::string& path,
                                             bool create, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(databases_mutex_);

    // Check if already open
    for (const auto& db : databases_) {
        if (db.name == name) {
            return core::Status::FILE_EXISTS;
        }
    }

    // Open database
    auto database = std::make_unique<core::Database>();
    core::Status status;

    if (create && !std::filesystem::exists(path)) {
        status = database->create(path, 16384, ctx);
    } else {
        status = database->open(path, ctx);
    }

    if (status != core::Status::OK) {
        return status;
    }

    if (database->audit_logger()) {
        database->audit_logger()->configureSinks(config_.audit_sinks);
    }
    if (database->permission_cache()) {
        database->permission_cache()->configureQuorum(config_.security_quorum);
    }
    database->setRoleSwitchPolicy(config_.role_switch_policy);

    databases_.push_back({name, path, std::move(database)});
    stats_.active_databases = static_cast<uint32_t>(databases_.size());

    return core::Status::OK;
}

core::Status ServiceController::closeDatabase(const std::string& name, core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(databases_mutex_);

    auto it = std::find_if(databases_.begin(), databases_.end(),
                           [&name](const DatabaseInstance& db) { return db.name == name; });

    if (it == databases_.end()) {
        return core::Status::NOT_FOUND;
    }

    it->database->close();
    databases_.erase(it);
    stats_.active_databases = static_cast<uint32_t>(databases_.size());

    return core::Status::OK;
}

ServiceController::HealthStatus ServiceController::getHealth() const {
    HealthStatus health;
    health.healthy = (state_ == ServiceState::RUNNING);
    health.status = serviceStateToString(state_);

    // Check databases
    {
        std::lock_guard<std::mutex> lock(databases_mutex_);
        for (const auto& db : databases_) {
            health.components["db:" + db.name] = (db.database != nullptr);
        }
    }

    // Add stats
    auto stats = getStats();
    health.details["uptime"] = std::to_string(static_cast<uint64_t>(stats.uptimeSeconds())) + "s";
    health.details["connections"] = std::to_string(stats.active_connections);
    health.details["queries"] = std::to_string(stats.total_queries);

    return health;
}

void ServiceController::setHealthCallback(HealthCallback callback) {
    health_callback_ = std::move(callback);
}

void ServiceController::setShutdownCallback(ShutdownCallback callback) {
    shutdown_callback_ = std::move(callback);
}

void ServiceController::setReloadCallback(ReloadCallback callback) {
    reload_callback_ = std::move(callback);
}

core::Status ServiceController::daemonize(core::ErrorContext* ctx) {
    if (!daemon_) {
        return core::Status::INTERNAL_ERROR;
    }

    // Set up signal handler
    daemon_->setSignalHandler([this](DaemonSignal sig) {
        handleSignal(sig);
    });

    return daemon_->daemonize(ctx);
}

core::Status ServiceController::openDatabases(core::ErrorContext* ctx) {
    if (config_.mode == ServiceConfig::Mode::SINGLE_DATABASE) {
        // Single database mode
        if (config_.database_path.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                                 "Database path not specified");
            }
            return core::Status::INVALID_ARGUMENT;
        }

        return openDatabase("main", config_.database_path, config_.auto_create_databases, ctx);
    } else {
        // Multi-database mode
        if (config_.data_dir.empty()) {
            if (ctx) {
                SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT,
                                 "Data directory not specified");
            }
            return core::Status::INVALID_ARGUMENT;
        }

        // Create data directory if needed
        if (!std::filesystem::exists(config_.data_dir)) {
            if (config_.auto_create_databases) {
                std::error_code ec;
                std::filesystem::create_directories(config_.data_dir, ec);
                if (ec) {
                    if (ctx) {
                        std::string msg = "Failed to create data directory: " + ec.message();
                        SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
                    }
                    return core::Status::IO_ERROR;
                }
            } else {
                if (ctx) {
                    std::string msg = "Data directory not found: " + config_.data_dir;
                    SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, msg.c_str());
                }
                return core::Status::NOT_FOUND;
            }
        }

        // Open all .sbdb files in data directory
        for (const auto& entry : std::filesystem::directory_iterator(config_.data_dir)) {
            if (entry.path().extension() == ".sbdb") {
                std::string name = entry.path().stem().string();
                core::Status status = openDatabase(name, entry.path().string(), false, ctx);
                if (status != core::Status::OK) {
                    log(ServiceConfig::LogLevel::WARNING,
                        "Failed to open database: " + name);
                }
            }
        }

        // Create main database if no databases found and auto_create is enabled
        if (databases_.empty() && config_.auto_create_databases) {
            std::string main_path = config_.data_dir + "/main.sbdb";
            return openDatabase("main", main_path, true, ctx);
        }
    }

    return core::Status::OK;
}

core::Status ServiceController::startListeners(core::ErrorContext* ctx) {
    // Note: Actual listener implementation would use the network layer
    // For now, this is a placeholder that will be connected to the network infrastructure
    log(ServiceConfig::LogLevel::INFO, "Starting protocol listeners...");

    for (const auto& proto : config_.protocols) {
        if (!proto.enabled) continue;

        std::string proto_name;
        switch (proto.type) {
            case network::ProtocolType::NATIVE: proto_name = "Native"; break;
            case network::ProtocolType::POSTGRESQL: proto_name = "PostgreSQL"; break;
            case network::ProtocolType::MYSQL: proto_name = "MySQL"; break;
            case network::ProtocolType::FIREBIRD: proto_name = "Firebird"; break;
            default: proto_name = "Unknown"; break;
        }

        log(ServiceConfig::LogLevel::INFO,
            proto_name + " listener on " + proto.bind_address + ":" + std::to_string(proto.port));
    }

    if (!config_.unix_socket.empty()) {
        log(ServiceConfig::LogLevel::INFO, "Unix socket: " + config_.unix_socket);
    }

    return core::Status::OK;
}

core::Status ServiceController::stopListeners(core::ErrorContext* ctx) {
    log(ServiceConfig::LogLevel::INFO, "Stopping listeners...");
    return core::Status::OK;
}

void ServiceController::mainLoop() {
    while (!shutdown_requested_) {
        // Check for signals
        if (daemon_) {
            daemon_->checkSignals();
        }

        // Update stats periodically
        updateStats();

        // Sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ServiceController::handleSignal(DaemonSignal signal) {
    switch (signal) {
        case DaemonSignal::SHUTDOWN:
            log(ServiceConfig::LogLevel::INFO, "Received shutdown signal");
            if (shutdown_callback_) {
                shutdown_callback_();
            }
            shutdown_requested_ = true;
            break;

        case DaemonSignal::IMMEDIATE_STOP:
            log(ServiceConfig::LogLevel::WARNING, "Received immediate shutdown signal");
            immediate_shutdown_ = true;
            shutdown_requested_ = true;
            break;

        case DaemonSignal::RELOAD:
            log(ServiceConfig::LogLevel::INFO, "Received reload signal");
            reload();
            break;

        case DaemonSignal::ROTATE_LOGS:
            log(ServiceConfig::LogLevel::INFO, "Received log rotation signal");
            // TODO: Implement log rotation
            break;

        case DaemonSignal::DUMP_STATS:
            log(ServiceConfig::LogLevel::INFO, "Received stats dump signal");
            // TODO: Dump stats to log
            break;

        default:
            break;
    }
}

void ServiceController::doShutdown() {
    state_ = ServiceState::STOPPING;

    if (daemon_) {
        daemon_->notifyStopping();
        daemon_->notifyStatus("Shutting down...");
    }

    // Stop watchdog
    watchdog_running_ = false;

    // Stop listeners
    stopListeners(nullptr);

    // Close databases
    {
        std::lock_guard<std::mutex> lock(databases_mutex_);
        for (auto& db : databases_) {
            if (db.database) {
                db.database->close();
            }
        }
        databases_.clear();
    }

    // Cleanup daemon
    if (daemon_) {
        daemon_->cleanup();
    }
}

void ServiceController::updateStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.active_databases = static_cast<uint32_t>(databases_.size());
}

void ServiceController::log(ServiceConfig::LogLevel level, const std::string& message) {
    if (level < config_.log_level) return;

    const char* level_str;
    switch (level) {
        case ServiceConfig::LogLevel::DEBUG: level_str = "DEBUG"; break;
        case ServiceConfig::LogLevel::INFO: level_str = "INFO"; break;
        case ServiceConfig::LogLevel::NOTICE: level_str = "NOTICE"; break;
        case ServiceConfig::LogLevel::WARNING: level_str = "WARNING"; break;
        case ServiceConfig::LogLevel::ERROR: level_str = "ERROR"; break;
        default: level_str = "UNKNOWN"; break;
    }

    // Simple logging to stderr for now
    // In production, this would go to the configured log destination
    std::cerr << "[" << level_str << "] " << message << std::endl;
}

// ============================================================================
// Command-Line Parsing
// ============================================================================

bool parseCommandLineArgs(int argc, char* argv[], CommandLineArgs& args, std::string& error) {
    static const struct option long_options[] = {
        {"config", required_argument, nullptr, 'c'},
        {"data-dir", required_argument, nullptr, 'D'},
        {"database", required_argument, nullptr, 'd'},
        {"create", no_argument, nullptr, 'C'},
        {"host", required_argument, nullptr, 'h'},
        {"port", required_argument, nullptr, 'p'},
        {"pg-port", required_argument, nullptr, 1001},
        {"mysql-port", required_argument, nullptr, 1002},
        {"fb-port", required_argument, nullptr, 1003},
        {"unix-socket", required_argument, nullptr, 'k'},
        {"max-connections", required_argument, nullptr, 'N'},
        {"shared-buffers", required_argument, nullptr, 'B'},
        {"foreground", no_argument, nullptr, 'F'},
        {"single-user", no_argument, nullptr, 's'},
        {"verbose", no_argument, nullptr, 'v'},
        {"check", no_argument, nullptr, 1010},
        {"version", no_argument, nullptr, 'V'},
        {"help", no_argument, nullptr, '?'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "c:D:d:Ch:p:k:N:B:FsvV?",
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 'c':
                args.config_file = optarg;
                break;
            case 'D':
                args.data_dir = optarg;
                break;
            case 'd':
                args.database_path = optarg;
                break;
            case 'C':
                args.auto_create = true;
                break;
            case 'h':
                args.host = optarg;
                break;
            case 'p':
                args.native_port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 1001:
                args.pg_port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 1002:
                args.mysql_port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 1003:
                args.fb_port = static_cast<uint16_t>(std::stoi(optarg));
                break;
            case 'k':
                args.unix_socket = optarg;
                break;
            case 'N':
                args.max_connections = static_cast<uint32_t>(std::stoi(optarg));
                break;
            case 'B': {
                uint64_t size;
                if (parseSize(optarg, size)) {
                    args.shared_buffers = size;
                } else {
                    error = "Invalid size value: " + std::string(optarg);
                    return false;
                }
                break;
            }
            case 'F':
                args.foreground = true;
                break;
            case 's':
                args.single_user = true;
                break;
            case 'v':
                args.verbose = true;
                break;
            case 1010:
                args.check_config = true;
                break;
            case 'V':
                args.version = true;
                break;
            case '?':
                args.help = true;
                break;
            default:
                error = "Unknown option";
                return false;
        }
    }

    return true;
}

void printHelp(const char* program_name) {
    std::cout << "sb_server - ScratchBird Database Server\n\n"
              << "USAGE:\n"
              << "    " << program_name << " [OPTIONS]\n\n"
              << "OPTIONS:\n"
              << "    -c, --config <FILE>         Configuration file path\n"
              << "                                Default: /etc/scratchbird/sb_server.conf\n\n"
              << "    -D, --data-dir <DIR>        Data directory (multi-database mode)\n"
              << "                                Default: /var/lib/scratchbird\n\n"
              << "    -d, --database <FILE>       Single database file path\n"
              << "                                Mutually exclusive with --data-dir\n\n"
              << "    -C, --create                Create database(s) if they don't exist\n\n"
              << "    -h, --host <ADDR>           Bind address for all protocols\n"
              << "                                Default: 0.0.0.0\n\n"
              << "    -p, --port <PORT>           ScratchBird native protocol port\n"
              << "                                Default: 3092\n\n"
              << "    --pg-port <PORT>            PostgreSQL protocol port (0 to disable)\n"
              << "                                Default: 5432\n\n"
              << "    --mysql-port <PORT>         MySQL protocol port (0 to disable)\n"
              << "                                Default: 3306\n\n"
              << "    --fb-port <PORT>            Firebird protocol port (0 to disable)\n"
              << "                                Default: 3050\n\n"
              << "    -k, --unix-socket <PATH>    Unix domain socket path\n"
              << "                                Default: /var/run/scratchbird/sb.sock\n\n"
              << "    -N, --max-connections <N>   Maximum concurrent connections\n"
              << "                                Default: 100\n\n"
              << "    -B, --shared-buffers <SIZE> Shared buffer pool size\n"
              << "                                Default: 128MB\n\n"
              << "    -F, --foreground            Run in foreground (don't daemonize)\n\n"
              << "    -s, --single-user           Single-user mode (maintenance)\n\n"
              << "    -v, --verbose               Verbose logging\n\n"
              << "    --check                     Check configuration and exit\n\n"
              << "    -V, --version               Print version and exit\n\n"
              << "    -?, --help                  Show this help message\n\n"
              << "ENVIRONMENT VARIABLES:\n"
              << "    SCRATCHBIRD_CONFIG          Configuration file path\n"
              << "    SCRATCHBIRD_DATA_DIR        Data directory\n"
              << "    SCRATCHBIRD_LOG_LEVEL       Log level (debug, info, warning, error)\n\n"
              << "EXAMPLES:\n"
              << "    # Start with default configuration\n"
              << "    " << program_name << "\n\n"
              << "    # Start in foreground with custom config\n"
              << "    " << program_name << " -F -c /path/to/custom.conf\n\n"
              << "    # Single database mode\n"
              << "    " << program_name << " -d /path/to/mydb.sbdb --create\n\n"
              << "    # Multi-database mode with custom ports\n"
              << "    " << program_name << " -D /var/lib/scratchbird --pg-port 5434\n";
}

void printVersion() {
    std::cout << "sb_server (ScratchBird) " << SCRATCHBIRD_VERSION_STRING << "\n"
              << "Protocol versions:\n"
              << "  - ScratchBird Native: 1.0\n"
              << "  - PostgreSQL: 3.0\n"
              << "  - MySQL: 10\n"
              << "  - Firebird: 18\n"
              << "Build: Release\n";
}

}  // namespace server
}  // namespace scratchbird
