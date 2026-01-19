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

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace scratchbird {
namespace server {

namespace {

std::string protocolName(network::ProtocolType type) {
    switch (type) {
        case network::ProtocolType::NATIVE: return "scratchbird";
        case network::ProtocolType::POSTGRESQL: return "postgresql";
        case network::ProtocolType::MYSQL: return "mysql";
        case network::ProtocolType::FIREBIRD: return "firebird";
        default: return "unknown";
    }
}

std::string listenerBinary(network::ProtocolType type) {
    switch (type) {
        case network::ProtocolType::NATIVE: return "sb_listener_native";
        case network::ProtocolType::POSTGRESQL: return "sb_listener_pg";
        case network::ProtocolType::MYSQL: return "sb_listener_mysql";
        case network::ProtocolType::FIREBIRD: return "sb_listener_fb";
        default: return "sb_listener_native";
    }
}

std::string logLevelString(ServiceConfig::LogLevel level) {
    switch (level) {
        case ServiceConfig::LogLevel::DEBUG: return "debug";
        case ServiceConfig::LogLevel::INFO: return "info";
        case ServiceConfig::LogLevel::NOTICE: return "info";
        case ServiceConfig::LogLevel::WARNING: return "warn";
        case ServiceConfig::LogLevel::ERROR: return "error";
        default: return "info";
    }
}

}  // namespace

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
        control_socket_dir = network->getString("control_socket_dir", control_socket_dir);
        spawn_strategy = network->getString("spawn_strategy", spawn_strategy);
        parser_max_requests = static_cast<uint32_t>(network->getInt("parser_max_requests", parser_max_requests));
        parser_max_age_seconds = static_cast<uint32_t>(network->getInt("parser_max_age_seconds", parser_max_age_seconds));
        unix_socket = network->getString("unix_socket", unix_socket);
        unix_socket_permissions = static_cast<mode_t>(network->getInt("unix_socket_permissions", unix_socket_permissions));
        unix_socket_group = network->getString("unix_socket_group", unix_socket_group);

        // Protocol ports
        protocols.clear();

        uint16_t native_port = static_cast<uint16_t>(network->getInt("native_port", 3092));
        uint32_t native_pool_min = static_cast<uint32_t>(network->getInt("native_pool_min", 4));
        uint32_t native_pool_max = static_cast<uint32_t>(network->getInt("native_pool_max", 64));
        if (native_port > 0) {
            protocols.push_back({network::ProtocolType::NATIVE, bind_address, native_port, true, false,
                                 native_pool_min, native_pool_max});
        }

        uint16_t pg_port = static_cast<uint16_t>(network->getInt("pg_port", 5432));
        uint32_t pg_pool_min = static_cast<uint32_t>(network->getInt("pg_pool_min", 4));
        uint32_t pg_pool_max = static_cast<uint32_t>(network->getInt("pg_pool_max", 64));
        if (pg_port > 0) {
            protocols.push_back({network::ProtocolType::POSTGRESQL, bind_address, pg_port, true, false,
                                 pg_pool_min, pg_pool_max});
        }

        uint16_t mysql_port = static_cast<uint16_t>(network->getInt("mysql_port", 3306));
        uint32_t mysql_pool_min = static_cast<uint32_t>(network->getInt("mysql_pool_min", 4));
        uint32_t mysql_pool_max = static_cast<uint32_t>(network->getInt("mysql_pool_max", 64));
        if (mysql_port > 0) {
            protocols.push_back({network::ProtocolType::MYSQL, bind_address, mysql_port, true, false,
                                 mysql_pool_min, mysql_pool_max});
        }

        uint16_t fb_port = static_cast<uint16_t>(network->getInt("fb_port", 3050));
        uint32_t fb_pool_min = static_cast<uint32_t>(network->getInt("fb_pool_min", 4));
        uint32_t fb_pool_max = static_cast<uint32_t>(network->getInt("fb_pool_max", 64));
        if (fb_port > 0) {
            protocols.push_back({network::ProtocolType::FIREBIRD, bind_address, fb_port, true, false,
                                 fb_pool_min, fb_pool_max});
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
        {network::ProtocolType::NATIVE, "0.0.0.0", 3092, true, false, 4, 64},
        {network::ProtocolType::POSTGRESQL, "0.0.0.0", 5432, false, false, 4, 64},
        {network::ProtocolType::MYSQL, "0.0.0.0", 3306, false, false, 4, 64},
        {network::ProtocolType::FIREBIRD, "0.0.0.0", 3050, false, false, 4, 64}
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
        exit_after_parse_ = true;
        return core::Status::OK;
    }

    if (args.version) {
        printVersion();
        exit_after_parse_ = true;
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

    if (!args.control_socket_dir.empty()) {
        config_.control_socket_dir = args.control_socket_dir;
    }

    for (auto& proto : config_.protocols) {
        switch (proto.type) {
            case network::ProtocolType::NATIVE:
                if (!args.native_bind.empty()) proto.bind_address = args.native_bind;
                if (args.enable_native) proto.enabled = true;
                if (args.disable_native) proto.enabled = false;
                if (args.native_pool_min > 0) proto.pool_min = args.native_pool_min;
                if (args.native_pool_max > 0) proto.pool_max = args.native_pool_max;
                break;
            case network::ProtocolType::POSTGRESQL:
                if (!args.pg_bind.empty()) proto.bind_address = args.pg_bind;
                if (args.enable_pg) proto.enabled = true;
                if (args.disable_pg) proto.enabled = false;
                if (args.pg_pool_min > 0) proto.pool_min = args.pg_pool_min;
                if (args.pg_pool_max > 0) proto.pool_max = args.pg_pool_max;
                break;
            case network::ProtocolType::MYSQL:
                if (!args.mysql_bind.empty()) proto.bind_address = args.mysql_bind;
                if (args.enable_mysql) proto.enabled = true;
                if (args.disable_mysql) proto.enabled = false;
                if (args.mysql_pool_min > 0) proto.pool_min = args.mysql_pool_min;
                if (args.mysql_pool_max > 0) proto.pool_max = args.mysql_pool_max;
                break;
            case network::ProtocolType::FIREBIRD:
                if (!args.fb_bind.empty()) proto.bind_address = args.fb_bind;
                if (args.enable_fb) proto.enabled = true;
                if (args.disable_fb) proto.enabled = false;
                if (args.fb_pool_min > 0) proto.pool_min = args.fb_pool_min;
                if (args.fb_pool_max > 0) proto.pool_max = args.fb_pool_max;
                break;
            default:
                break;
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
        exit_after_parse_ = true;
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
    log(ServiceConfig::LogLevel::INFO, "Starting protocol listeners...");

    std::lock_guard<std::mutex> lock(listeners_mutex_);
    listeners_.clear();

    for (const auto& proto : config_.protocols) {
        if (!proto.enabled || proto.port == 0) {
            continue;
        }

        ListenerProcess listener;
        listener.config = proto;
        listener.name = protocolName(proto.type);
        listener.binary = listenerBinary(proto.type);

        if (!launchListenerProcess(listener, ctx)) {
            continue;
        }

        log(ServiceConfig::LogLevel::INFO,
            "Started " + listener.name + " listener on " + proto.bind_address + ":" +
            std::to_string(proto.port));
        listeners_.push_back(listener);
    }

    if (!config_.unix_socket.empty()) {
        log(ServiceConfig::LogLevel::INFO, "Unix socket: " + config_.unix_socket);
    }

    return core::Status::OK;
}

core::Status ServiceController::stopListeners(core::ErrorContext* ctx) {
    log(ServiceConfig::LogLevel::INFO, "Stopping listeners...");

    std::lock_guard<std::mutex> lock(listeners_mutex_);
    for (auto& listener : listeners_) {
        if (!listener.running) {
            continue;
        }
#ifdef _WIN32
        if (listener.process_handle) {
            TerminateProcess(listener.process_handle, 0);
            CloseHandle(listener.process_handle);
            listener.process_handle = nullptr;
        }
        listener.running = false;
#else
        if (listener.pid > 0) {
            kill(listener.pid, SIGTERM);
            int status = 0;
            waitpid(listener.pid, &status, 0);
            listener.pid = 0;
        }
        listener.running = false;
#endif
    }

    listeners_.clear();
    return core::Status::OK;
}

bool ServiceController::launchListenerProcess(ListenerProcess& listener, core::ErrorContext* ctx) {
    const auto& proto = listener.config;
    std::vector<std::string> args;
    args.push_back(listener.binary);
    args.push_back("--bind");
    args.push_back(proto.bind_address);
    args.push_back("--port");
    args.push_back(std::to_string(proto.port));
    args.push_back("--pool-min");
    args.push_back(std::to_string(proto.pool_min));
    args.push_back("--pool-max");
    args.push_back(std::to_string(proto.pool_max));
    args.push_back("--spawn-strategy");
    args.push_back(config_.spawn_strategy);

    if (config_.parser_max_requests > 0) {
        args.push_back("--max-requests");
        args.push_back(std::to_string(config_.parser_max_requests));
    }
    if (config_.parser_max_age_seconds > 0) {
        args.push_back("--max-age-seconds");
        args.push_back(std::to_string(config_.parser_max_age_seconds));
    }
    if (!config_.control_socket_dir.empty()) {
        args.push_back("--control-socket-dir");
        args.push_back(config_.control_socket_dir);
    }
    std::string engine_endpoint;
    if (config_.mode == ServiceConfig::Mode::SINGLE_DATABASE && !config_.database_path.empty()) {
        engine_endpoint = getIPCPath(config_.database_path, IPCMethod::AUTO);
    } else if (config_.mode == ServiceConfig::Mode::MULTI_DATABASE && !config_.data_dir.empty()) {
        std::string main_path = config_.data_dir + "/main.sbdb";
        engine_endpoint = getIPCPath(main_path, IPCMethod::AUTO);
    }
    if (!engine_endpoint.empty()) {
        args.push_back("--engine-endpoint");
        args.push_back(engine_endpoint);
    }
    if (!config_.config_file.empty()) {
        args.push_back("--config");
        args.push_back(config_.config_file);
    }
    args.push_back("--log-level");
    args.push_back(logLevelString(config_.log_level));

#ifdef _WIN32
    std::string command_line;
    for (const auto& item : args) {
        if (!command_line.empty()) command_line += " ";
        command_line += item;
    }

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    BOOL ok = CreateProcessA(nullptr, command_line.data(), nullptr, nullptr, FALSE,
                             CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi);
    if (!ok) {
        if (ctx) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              "Failed to spawn listener process");
        }
        log(ServiceConfig::LogLevel::ERROR,
            "Failed to spawn listener: " + listener.binary);
        return false;
    }
    listener.process_handle = pi.hProcess;
    listener.process_id = pi.dwProcessId;
    listener.running = true;
    listener.start_count++;
    CloseHandle(pi.hThread);
#else
    pid_t pid = fork();
    if (pid < 0) {
        if (ctx) {
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR,
                              "Failed to fork listener process");
        }
        log(ServiceConfig::LogLevel::ERROR,
            "Failed to fork listener: " + listener.binary);
        return false;
    }

    if (pid == 0) {
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (auto& item : args) {
            argv.push_back(const_cast<char*>(item.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    listener.pid = pid;
    listener.running = true;
    listener.start_count++;
#endif

    return true;
}

void ServiceController::checkListeners() {
    std::lock_guard<std::mutex> lock(listeners_mutex_);

    for (auto& listener : listeners_) {
        if (!listener.running) {
            continue;
        }
#ifdef _WIN32
        if (!listener.process_handle) {
            continue;
        }
        DWORD exit_code = 0;
        if (GetExitCodeProcess(listener.process_handle, &exit_code) && exit_code != STILL_ACTIVE) {
            CloseHandle(listener.process_handle);
            listener.process_handle = nullptr;
            listener.running = false;
            listener.restart_count++;
            log(ServiceConfig::LogLevel::WARNING,
                "Listener exited (" + listener.name + "), restarting");
            if (!shutdown_requested_) {
                launchListenerProcess(listener, nullptr);
            }
        }
#else
        int status = 0;
        pid_t result = waitpid(listener.pid, &status, WNOHANG);
        if (result == listener.pid) {
            listener.running = false;
            listener.pid = 0;
            listener.restart_count++;
            log(ServiceConfig::LogLevel::WARNING,
                "Listener exited (" + listener.name + "), restarting");
            if (!shutdown_requested_) {
                launchListenerProcess(listener, nullptr);
            }
        }
#endif
    }
}

void ServiceController::mainLoop() {
    while (!shutdown_requested_) {
        // Check for signals
        if (daemon_) {
            daemon_->checkSignals();
        }

        checkListeners();

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
        {"control-socket-dir", required_argument, nullptr, 1004},
        {"enable-native", no_argument, nullptr, 1100},
        {"enable-postgres", no_argument, nullptr, 1101},
        {"enable-mysql", no_argument, nullptr, 1102},
        {"enable-firebird", no_argument, nullptr, 1103},
        {"disable-native", no_argument, nullptr, 1110},
        {"disable-postgres", no_argument, nullptr, 1111},
        {"disable-mysql", no_argument, nullptr, 1112},
        {"disable-firebird", no_argument, nullptr, 1113},
        {"native-bind", required_argument, nullptr, 1120},
        {"postgres-bind", required_argument, nullptr, 1121},
        {"mysql-bind", required_argument, nullptr, 1122},
        {"firebird-bind", required_argument, nullptr, 1123},
        {"native-pool-min", required_argument, nullptr, 1200},
        {"native-pool-max", required_argument, nullptr, 1201},
        {"postgres-pool-min", required_argument, nullptr, 1202},
        {"postgres-pool-max", required_argument, nullptr, 1203},
        {"mysql-pool-min", required_argument, nullptr, 1204},
        {"mysql-pool-max", required_argument, nullptr, 1205},
        {"firebird-pool-min", required_argument, nullptr, 1206},
        {"firebird-pool-max", required_argument, nullptr, 1207},
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
            case 1004:
                args.control_socket_dir = optarg;
                break;
            case 1100:
                args.enable_native = true;
                break;
            case 1101:
                args.enable_pg = true;
                break;
            case 1102:
                args.enable_mysql = true;
                break;
            case 1103:
                args.enable_fb = true;
                break;
            case 1110:
                args.disable_native = true;
                break;
            case 1111:
                args.disable_pg = true;
                break;
            case 1112:
                args.disable_mysql = true;
                break;
            case 1113:
                args.disable_fb = true;
                break;
            case 1120:
                args.native_bind = optarg;
                break;
            case 1121:
                args.pg_bind = optarg;
                break;
            case 1122:
                args.mysql_bind = optarg;
                break;
            case 1123:
                args.fb_bind = optarg;
                break;
            case 1200:
                args.native_pool_min = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1201:
                args.native_pool_max = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1202:
                args.pg_pool_min = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1203:
                args.pg_pool_max = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1204:
                args.mysql_pool_min = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1205:
                args.mysql_pool_max = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1206:
                args.fb_pool_min = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 1207:
                args.fb_pool_max = static_cast<uint32_t>(std::stoul(optarg));
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
              << "    --control-socket-dir <DIR>  Control socket directory\n"
              << "                                Default: /var/run/scratchbird\n\n"
              << "    --enable-native             Enable native listener\n"
              << "    --enable-postgres           Enable PostgreSQL listener\n"
              << "    --enable-mysql              Enable MySQL listener\n"
              << "    --enable-firebird           Enable Firebird listener\n\n"
              << "    --disable-native            Disable native listener\n"
              << "    --disable-postgres          Disable PostgreSQL listener\n"
              << "    --disable-mysql             Disable MySQL listener\n"
              << "    --disable-firebird          Disable Firebird listener\n\n"
              << "    --native-bind <ADDR>        Native listener bind address\n"
              << "    --postgres-bind <ADDR>      PostgreSQL listener bind address\n"
              << "    --mysql-bind <ADDR>         MySQL listener bind address\n"
              << "    --firebird-bind <ADDR>      Firebird listener bind address\n\n"
              << "    --native-pool-min <N>        Native parser pool min\n"
              << "    --native-pool-max <N>        Native parser pool max\n"
              << "    --postgres-pool-min <N>      PostgreSQL parser pool min\n"
              << "    --postgres-pool-max <N>      PostgreSQL parser pool max\n"
              << "    --mysql-pool-min <N>         MySQL parser pool min\n"
              << "    --mysql-pool-max <N>         MySQL parser pool max\n"
              << "    --firebird-pool-min <N>      Firebird parser pool min\n"
              << "    --firebird-pool-max <N>      Firebird parser pool max\n\n"
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
