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
 * ScratchBird Listener - Protocol-Specific Entry Point
 *
 * Scaffolding for listener binaries per protocol. Accepts connections
 * and prepares for parser handoff (control-plane wired separately).
 */

#include <atomic>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/portable_file_io.h"
#include "scratchbird/core/signal_control.h"
#include "scratchbird/network/control_plane.h"
#include "scratchbird/network/listener_ipc_adapter.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"
#include "scratchbird/server/config_parser.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/security/tls_config.h"
#include "scratchbird/version.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#endif

#ifndef SB_LISTENER_PROTOCOL
#define SB_LISTENER_PROTOCOL "scratchbird"
#endif

#ifndef SB_LISTENER_NAME
#define SB_LISTENER_NAME "sb_listener"
#endif

namespace {

std::atomic<bool> g_shutdown{false};
std::atomic<bool> g_dump_stats{false};
std::atomic<bool> g_draining{false};
std::atomic<bool> g_force_shutdown{false};
std::unique_ptr<scratchbird::core::SignalControl> g_signal_control;

struct ListenerConfig {
    std::string protocol = SB_LISTENER_PROTOCOL;
    std::string listener_mode = "direct";  // direct | managed
    std::string database_owner = "main";
    std::string bind_address = "0.0.0.0";
    uint16_t port = 0;
    std::string control_socket_dir;
    std::string engine_endpoint;
    std::string config_path;
    std::string tls_config;
    std::string log_level = "info";
    uint32_t pool_min = 4;
    uint32_t pool_max = 64;
    std::string spawn_strategy = "hybrid";
    uint32_t max_requests = 0;
    uint32_t max_age_seconds = 0;
    uint32_t health_check_interval_ms = 5000;
    bool show_help = false;
    bool show_version = false;
    bool tls_enabled = false;  // Parsed from TLS config
    uint32_t listener_id = 0;
    std::string dbbt_keyring_path;
    uint32_t dbbt_clock_skew_ms = 2000;
    uint32_t dbbt_replay_cache_size = 4096;
    bool require_proxy_binding = false;
};

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string normalizeListenerModeToken(std::string mode) {
    auto is_space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!mode.empty() && is_space(static_cast<unsigned char>(mode.front()))) {
        mode.erase(mode.begin());
    }
    while (!mode.empty() && is_space(static_cast<unsigned char>(mode.back()))) {
        mode.pop_back();
    }
    mode = toUpperAscii(std::move(mode));
    if (mode == "MANAGER_PROXY") {
        return "MANAGED";
    }
    return mode;
}

bool isLoopbackBindAddress(std::string bind_address) {
    auto is_space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!bind_address.empty() && is_space(static_cast<unsigned char>(bind_address.front()))) {
        bind_address.erase(bind_address.begin());
    }
    while (!bind_address.empty() && is_space(static_cast<unsigned char>(bind_address.back()))) {
        bind_address.pop_back();
    }
    bind_address = toUpperAscii(std::move(bind_address));
    return bind_address == "127.0.0.1" ||
           bind_address == "::1" ||
           bind_address == "[::1]" ||
           bind_address == "LOCALHOST";
}

bool normalizeAndValidateListenerMode(ListenerConfig& config, std::string& error_out) {
    error_out.clear();

    std::string mode = normalizeListenerModeToken(config.listener_mode);
    if (mode.empty()) {
        mode = "DIRECT";
    }
    if (mode != "DIRECT" && mode != "MANAGED") {
        error_out = "listener mode must be 'direct' or 'managed'";
        return false;
    }

    const bool managed = (mode == "MANAGED");
    if (managed) {
        config.require_proxy_binding = true;
        if (!isLoopbackBindAddress(config.bind_address)) {
            error_out = "managed mode requires loopback bind address (127.0.0.1/::1/localhost)";
            return false;
        }
        config.listener_mode = "managed";
        return true;
    }

    // DIRECT mode must not silently enforce managed-only preface behavior.
    if (config.require_proxy_binding) {
        error_out = "direct mode cannot require proxy binding; set listener_mode=managed";
        return false;
    }
    config.listener_mode = "direct";
    return true;
}

struct ListenerHandoffBindingContext {
    std::array<uint8_t, 16> db_uuid{};
    std::array<uint8_t, 16> dbbt_id{};
    std::array<uint8_t, 16> manager_session_id{};
    uint32_t listener_id = 0;
};

void pollRuntimeSignals() {
    if (!g_signal_control) {
        return;
    }

    scratchbird::core::ControlSignal signal = scratchbird::core::ControlSignal::NONE;
    if (g_signal_control->poll(&signal, nullptr) != scratchbird::core::Status::OK) {
        return;
    }

    switch (signal) {
        case scratchbird::core::ControlSignal::SHUTDOWN:
        case scratchbird::core::ControlSignal::IMMEDIATE_STOP:
            g_shutdown.store(true, std::memory_order_release);
            break;
        case scratchbird::core::ControlSignal::RELOAD:
            g_draining.store(true, std::memory_order_release);
            break;
        case scratchbird::core::ControlSignal::DUMP_STATS:
            g_dump_stats.store(true, std::memory_order_release);
            break;
        case scratchbird::core::ControlSignal::ROTATE_LOGS:
        case scratchbird::core::ControlSignal::NONE:
        default:
            break;
    }
}

uint16_t defaultPortForProtocol(const std::string& protocol) {
    using scratchbird::network::DEFAULT_FIREBIRD_PORT;
    using scratchbird::network::DEFAULT_MYSQL_PORT;
    using scratchbird::network::DEFAULT_NATIVE_PORT;
    using scratchbird::network::DEFAULT_POSTGRESQL_PORT;

    if (protocol == "postgresql") {
        return DEFAULT_POSTGRESQL_PORT;
    }
    if (protocol == "mysql") {
        return DEFAULT_MYSQL_PORT;
    }
    if (protocol == "firebird") {
        return DEFAULT_FIREBIRD_PORT;
    }
    return DEFAULT_NATIVE_PORT;
}

uint64_t fnv1a64(const std::string& value, uint64_t seed) {
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = seed;
    for (unsigned char c : value) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
    return hash;
}

std::array<uint8_t, 16> deriveDatabaseUuid(const std::string& database_name) {
    std::array<uint8_t, 16> out{};
    const uint64_t hi = fnv1a64(database_name, 1469598103934665603ull);
    const uint64_t lo = fnv1a64(database_name, 1099511628211ull);
    for (int i = 0; i < 8; ++i) {
        out[static_cast<size_t>(i)] = static_cast<uint8_t>((hi >> (i * 8)) & 0xFF);
        out[static_cast<size_t>(8 + i)] = static_cast<uint8_t>((lo >> (i * 8)) & 0xFF);
    }
    out[6] = static_cast<uint8_t>((out[6] & 0x0F) | 0x40);
    out[8] = static_cast<uint8_t>((out[8] & 0x3F) | 0x80);
    return out;
}

std::string protocolKey(const std::string& protocol) {
    if (protocol == "postgresql") {
        return "pg";
    }
    if (protocol == "mysql") {
        return "mysql";
    }
    if (protocol == "firebird") {
        return "fb";
    }
    return "native";
}

std::string parserBinaryForProtocol(const std::string& protocol) {
    if (protocol == "postgresql") {
        return "sb_parser_pg";
    }
    if (protocol == "mysql") {
        return "sb_parser_mysql";
    }
    if (protocol == "firebird") {
        return "sb_parser_fb";
    }
    return "sb_parser_native";
}

std::string controlSocketPath(const ListenerConfig& config) {
#ifdef _WIN32
    std::string protocol = config.protocol;
    if (protocol.empty()) {
        protocol = "scratchbird";
    }
    return "\\\\.\\pipe\\scratchbird\\" + protocol + "_" +
           std::to_string(config.port) + "\\listener";
#else
    if (config.control_socket_dir.empty()) {
        return std::string();
    }
    std::string path = config.control_socket_dir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    return path + "sb_listener." + config.protocol + "." +
           std::to_string(config.port) + ".sock";
#endif
}

std::string managementSocketPath(const ListenerConfig& config) {
#ifdef _WIN32
    std::string protocol = config.protocol;
    if (protocol.empty()) {
        protocol = "scratchbird";
    }
    return "\\\\.\\pipe\\scratchbird\\" + protocol + "_" +
           std::to_string(config.port) + "\\listener_mgmt";
#else
    if (config.control_socket_dir.empty()) {
        return std::string();
    }
    std::string path = config.control_socket_dir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    return path + "sb_listener." + config.protocol + "." +
           std::to_string(config.port) + ".mgmt.sock";
#endif
}

enum class WorkerState : uint8_t {
    IDLE = 0,
    BUSY = 1,
    DRAINING = 2,
    FAULT = 3
};

struct ParserWorker {
    uint64_t worker_id = 0;
    uint32_t worker_pid = 0;
    std::unique_ptr<scratchbird::network::Socket> control;
    std::thread reader_thread;
    std::mutex mutex;
    std::condition_variable cv;
    WorkerState state = WorkerState::IDLE;
    bool running = false;
    bool awaiting_ack = false;
    uint64_t awaiting_request = 0;
    bool last_ack_ok = false;
    bool last_ack_ready = false;
    bool session_active = false;
    std::chrono::steady_clock::time_point session_start;
    uint64_t healthcheck_request = 0;
    std::chrono::steady_clock::time_point healthcheck_start;
    uint32_t sessions_completed = 0;
    std::chrono::steady_clock::time_point started_at;
    bool recycle_requested = false;
    std::string recycle_reason;
    uint64_t active_connection_id = 0;
};

struct PoolMetrics {
    scratchbird::core::Counter* parser_spawn_total = nullptr;
    scratchbird::core::Counter* parser_recycle_total = nullptr;
    scratchbird::core::Counter* parser_errors_total = nullptr;
    scratchbird::core::Gauge* parser_pool_size = nullptr;
    scratchbird::core::Gauge* parser_pool_idle = nullptr;
    scratchbird::core::Gauge* parser_pool_busy = nullptr;
    scratchbird::core::Histogram* parser_session_seconds = nullptr;
    scratchbird::core::Histogram* parser_healthcheck_seconds = nullptr;
};

class ParserPool {
public:
    ParserPool(const ListenerConfig& config,
               PoolMetrics metrics,
               scratchbird::core::Histogram* handoff_histogram,
               scratchbird::core::Histogram* queue_wait_histogram)
        : config_(config),
          metrics_(metrics),
          handoff_histogram_(handoff_histogram),
          queue_wait_histogram_(queue_wait_histogram) {
        db_uuid_template_ = deriveDatabaseUuid(config_.database_owner);
        listener_id_template_ = config_.listener_id;
    }

    bool start() {
        draining_.store(false, std::memory_order_release);
        if (config_.engine_endpoint.empty()) {
            std::cerr << "Missing engine endpoint; parser pool disabled\n";
            return false;
        }

        if (config_.spawn_strategy != "on_demand") {
            for (uint32_t i = 0; i < config_.pool_min; ++i) {
                if (!spawnWorker()) {
                    return false;
                }
            }
        }
        startHealthChecks();
        return true;
    }

    bool waitForWarm(std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(mutex_);
        while (warmWorkerCountLocked() < config_.pool_min) {
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                break;
            }
        }
        return warmWorkerCountLocked() >= config_.pool_min;
    }

    size_t activeSessionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeSessionCountLocked();
    }

    size_t warmWorkerCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return warmWorkerCountLocked();
    }

    void queueBindingContext(const ListenerHandoffBindingContext& binding) {
        std::lock_guard<std::mutex> lock(binding_mutex_);
        constexpr size_t kMaxQueuedBindings = 1024;
        if (pending_bindings_.size() >= kMaxQueuedBindings) {
            pending_bindings_.pop_front();
        }
        pending_bindings_.push_back(binding);
    }

    void setDraining(bool value) {
        draining_.store(value, std::memory_order_release);
        if (value) {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& worker : workers_) {
                if (!worker->running) {
                    continue;
                }
                std::lock_guard<std::mutex> worker_lock(worker->mutex);
                if (worker->state == WorkerState::IDLE) {
                    worker->state = WorkerState::DRAINING;
                }
            }
            updateMetricsLocked();
        } else {
            updateMetrics();
            cv_.notify_all();
        }
    }

    bool isDraining() const {
        return draining_.load(std::memory_order_acquire);
    }

    bool applyRuntimeConfig(uint32_t pool_min,
                            uint32_t pool_max,
                            uint32_t health_check_interval_ms,
                            std::string& error) {
        if (pool_min == 0 || pool_max == 0 || pool_min > pool_max) {
            error = "invalid_pool_bounds";
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            config_.pool_min = pool_min;
            config_.pool_max = pool_max;
            config_.health_check_interval_ms = health_check_interval_ms;
        }
        ensureMinWorkers();
        cv_.notify_all();
        return true;
    }

    bool terminateConnection(uint64_t connection_id, std::string& error) {
        std::shared_ptr<ParserWorker> target;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& worker : workers_) {
                if (!worker->running) {
                    continue;
                }
                std::lock_guard<std::mutex> worker_lock(worker->mutex);
                if (worker->session_active &&
                    worker->active_connection_id == connection_id) {
                    target = worker;
                    break;
                }
            }
        }
        if (!target) {
            error = "connection_not_found";
            return false;
        }

#ifdef _WIN32
        error = "not_implemented_windows";
        return false;
#else
        if (::kill(static_cast<pid_t>(target->worker_pid), SIGTERM) != 0) {
            error = std::string("kill_failed_errno_") + std::to_string(errno);
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(target->mutex);
            target->running = false;
            target->state = WorkerState::FAULT;
            target->session_active = false;
            target->active_connection_id = 0;
        }
        if (metrics_.parser_recycle_total) {
            metrics_.parser_recycle_total->inc(1.0,
                                               {config_.protocol, "default", "admin_kill"});
        }
        updateMetrics();
        ensureMinWorkers();
        return true;
#endif
    }

    void stop() {
        stopHealthChecks();
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& worker : workers_) {
            if (!worker->running) {
                continue;
            }
            worker->running = false;
            if (worker->control) {
                worker->control->close();
            }
        }
    }

    void handleControlConnection(std::unique_ptr<scratchbird::network::Socket> socket) {
        scratchbird::core::ErrorContext ctx;
        scratchbird::network::ControlPlaneMessage msg;
        auto status = scratchbird::network::receiveControlPlaneMessage(*socket, msg, nullptr, &ctx);
        if (status != scratchbird::core::Status::OK) {
            std::cerr << "Control HELLO receive failed: " << ctx.message << "\n";
            socket->close();
            return;
        }

        if (msg.header.message_type != static_cast<uint16_t>(
                scratchbird::network::ControlPlaneMessageType::HELLO)) {
            std::cerr << "Control HELLO rejected: unexpected message type\n";
            socket->close();
            return;
        }

        std::string proto;
        uint32_t pid = 0;
        uint64_t worker_id = 0;
        if (!parseHello(msg.payload, proto, pid, worker_id)) {
            std::cerr << "Control HELLO rejected: malformed payload\n";
            socket->close();
            return;
        }
        std::cerr << "[listener_debug] HELLO from worker pid=" << pid
                  << " id=" << worker_id << " proto=" << proto << "\n";

        scratchbird::network::ControlPlaneMessage ack;
        ack.header.message_type = static_cast<uint16_t>(
            scratchbird::network::ControlPlaneMessageType::HELLO_ACK);
        ack.header.request_id = msg.header.request_id;
        if (proto != config_.protocol) {
            ack.payload = buildHelloAck(false, "Protocol mismatch");
            ack.header.payload_len = ack.payload.size();
            scratchbird::network::sendControlPlaneMessage(*socket, ack,
                                                          scratchbird::network::INVALID_SOCKET_VALUE,
                                                          0, nullptr);
            socket->close();
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (runningCountLocked() >= config_.pool_max) {
                ack.payload = buildHelloAck(false, "Pool full");
                ack.header.payload_len = ack.payload.size();
                scratchbird::network::sendControlPlaneMessage(*socket, ack,
                                                              scratchbird::network::INVALID_SOCKET_VALUE,
                                                              0, nullptr);
                socket->close();
                return;
            }
        }
        ack.payload = buildHelloAck(true, "");
        ack.header.payload_len = ack.payload.size();
        if (scratchbird::network::sendControlPlaneMessage(*socket, ack,
                                                          scratchbird::network::INVALID_SOCKET_VALUE,
                                                          0, &ctx) != scratchbird::core::Status::OK) {
            std::cerr << "Control HELLO_ACK send failed: " << ctx.message << "\n";
            socket->close();
            return;
        }

        auto worker = std::make_shared<ParserWorker>();
        worker->worker_id = worker_id;
        worker->worker_pid = pid;
        worker->control = std::move(socket);
        worker->state = WorkerState::IDLE;
        worker->running = true;
        worker->started_at = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            workers_.push_back(worker);
            updateMetricsLocked();
        }
        std::cerr << "[listener_debug] worker registered id=" << worker->worker_id
                  << " pid=" << worker->worker_pid << " total=" << workers_.size() << "\n";

        worker->reader_thread = std::thread([this, worker]() {
            readerLoop(worker);
        });
        worker->reader_thread.detach();

        cv_.notify_all();
    }

    std::shared_ptr<ParserWorker> acquireWorker(std::chrono::milliseconds timeout) {
        if (draining_.load(std::memory_order_acquire)) {
            return nullptr;
        }
        auto start = std::chrono::steady_clock::now();
        std::unique_lock<std::mutex> lock(mutex_);

        while (true) {
            for (auto& worker : workers_) {
                if (worker->running && worker->state == WorkerState::IDLE && !worker->awaiting_ack) {
                    worker->state = WorkerState::BUSY;
                    updateMetricsLocked();
                    if (queue_wait_histogram_) {
                        auto elapsed = std::chrono::steady_clock::now() - start;
                        queue_wait_histogram_->observe(
                            std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count(),
                            {config_.protocol, SB_LISTENER_NAME});
                    }
                    std::cerr << "[listener_debug] acquire worker id=" << worker->worker_id
                              << " pid=" << worker->worker_pid << "\n";
                    return worker;
                }
            }

            if (draining_.load(std::memory_order_acquire)) {
                return nullptr;
            }

            if (config_.spawn_strategy != "prefork" && runningCountLocked() < config_.pool_max) {
                spawnWorkerLocked();
            }

            if (timeout.count() == 0) {
                std::cerr << "[listener_debug] acquire worker timeout (0ms)\n";
                return nullptr;
            }
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                std::cerr << "[listener_debug] acquire worker timed out\n";
                return nullptr;
            }
            cv_.wait_for(lock, std::chrono::milliseconds(50));
        }
    }

    bool handoff(const std::shared_ptr<ParserWorker>& worker,
                 scratchbird::network::socket_t client_fd,
                 const scratchbird::network::NetworkAddress& client_addr,
                 bool tls_active) {
        scratchbird::network::ControlPlaneMessage msg;
        msg.header.message_type = static_cast<uint16_t>(
            scratchbird::network::ControlPlaneMessageType::HANDOFF_SOCKET);
        msg.header.request_id = nextRequestId();
        ListenerHandoffBindingContext binding;
        bool has_pending_binding = false;
        {
            std::lock_guard<std::mutex> lock(binding_mutex_);
            if (!pending_bindings_.empty()) {
                binding = pending_bindings_.front();
                pending_bindings_.pop_front();
                has_pending_binding = true;
            }
        }
        if (!has_pending_binding) {
            if (config_.require_proxy_binding) {
                std::cerr << "[listener_debug] handoff denied: missing proxy binding context\n";
                return false;
            }
            binding.db_uuid = db_uuid_template_;
            binding.listener_id = listener_id_template_;
        }
        msg.payload = buildHandoffPayload(msg.header.request_id, client_addr, tls_active, binding);
        msg.header.payload_len = msg.payload.size();
        std::cerr << "[listener_debug] handoff req=" << msg.header.request_id
                  << " worker_id=" << worker->worker_id
                  << " pid=" << worker->worker_pid
                  << " fd=" << client_fd << "\n";

        auto start = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->awaiting_ack = true;
            worker->awaiting_request = msg.header.request_id;
            worker->last_ack_ready = false;
        }

        scratchbird::core::ErrorContext ctx;
        auto status = scratchbird::network::sendControlPlaneMessage(
            *worker->control, msg, client_fd, worker->worker_pid, &ctx);

        if (status != scratchbird::core::Status::OK) {
            std::cerr << "[listener_debug] handoff send failed req="
                      << msg.header.request_id << " err=" << ctx.message << "\n";
            markWorkerFault(worker, "error");
            return false;
        }

        std::unique_lock<std::mutex> lock(worker->mutex);
        bool acked = worker->cv.wait_for(lock, std::chrono::seconds(2), [&worker]() {
            return worker->last_ack_ready;
        });
        std::cerr << "[listener_debug] handoff ack req=" << msg.header.request_id
                  << " acked=" << acked << " ok=" << worker->last_ack_ok << "\n";

        auto elapsed = std::chrono::steady_clock::now() - start;
        if (handoff_histogram_) {
            handoff_histogram_->observe(
                std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count(),
                {config_.protocol, SB_LISTENER_NAME});
        }

        if (!acked || !worker->last_ack_ok) {
            markWorkerFault(worker, "error");
            return false;
        }

        worker->session_active = true;
        worker->session_start = std::chrono::steady_clock::now();
        worker->active_connection_id = msg.header.request_id;
        return true;
    }

    void reportIdle(const std::shared_ptr<ParserWorker>& worker) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->state = WorkerState::IDLE;
        updateMetrics();
        cv_.notify_all();
    }

private:
    ListenerConfig config_;
    PoolMetrics metrics_;
    scratchbird::core::Histogram* handoff_histogram_;
    scratchbird::core::Histogram* queue_wait_histogram_;
    std::array<uint8_t, 16> db_uuid_template_{};
    uint32_t listener_id_template_ = 0;
    mutable std::mutex binding_mutex_;
    std::deque<ListenerHandoffBindingContext> pending_bindings_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<ParserWorker>> workers_;
    std::atomic<uint64_t> request_id_{1};
    std::atomic<bool> healthcheck_running_{false};
    std::atomic<bool> draining_{false};
    std::thread healthcheck_thread_;

    uint64_t nextRequestId() {
        return request_id_.fetch_add(1, std::memory_order_relaxed);
    }

    void updateMetrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        updateMetricsLocked();
    }

    size_t runningCountLocked() const {
        size_t running = 0;
        for (const auto& worker : workers_) {
            if (worker->running) {
                running++;
            }
        }
        return running;
    }

    size_t warmWorkerCountLocked() const {
        size_t warm = 0;
        for (const auto& worker : workers_) {
            if (!worker->running) {
                continue;
            }
            if (worker->state == WorkerState::IDLE) {
                warm++;
            }
        }
        return warm;
    }

    size_t activeSessionCountLocked() const {
        size_t active = 0;
        for (const auto& worker : workers_) {
            if (!worker->running) {
                continue;
            }
            if (worker->session_active) {
                active++;
            }
        }
        return active;
    }

    void updateMetricsLocked() {
        if (!metrics_.parser_pool_size) {
            return;
        }
        size_t total = 0;
        size_t idle = 0;
        size_t busy = 0;
        for (const auto& worker : workers_) {
            if (!worker->running) {
                continue;
            }
            total++;
            if (worker->state == WorkerState::IDLE) {
                idle++;
            } else if (worker->state == WorkerState::BUSY ||
                       worker->state == WorkerState::DRAINING) {
                busy++;
            }
        }
        metrics_.parser_pool_size->set(static_cast<double>(total),
                                       {config_.protocol, "default"});
        metrics_.parser_pool_idle->set(static_cast<double>(idle),
                                       {config_.protocol, "default"});
        metrics_.parser_pool_busy->set(static_cast<double>(busy),
                                       {config_.protocol, "default"});
    }

    void markWorkerFault(const std::shared_ptr<ParserWorker>& worker, const std::string& reason) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->state = WorkerState::FAULT;
        worker->running = false;
        worker->session_active = false;
        worker->active_connection_id = 0;
        if (metrics_.parser_recycle_total) {
            metrics_.parser_recycle_total->inc(1.0, {config_.protocol, "default", reason});
        }
        updateMetrics();
        ensureMinWorkers();
    }

    bool spawnWorker() {
        std::lock_guard<std::mutex> lock(mutex_);
        return spawnWorkerLocked();
    }

    bool spawnWorkerLocked() {
        if (runningCountLocked() >= config_.pool_max) {
            return false;
        }
        if (metrics_.parser_spawn_total) {
            metrics_.parser_spawn_total->inc(1.0, {config_.protocol, "default"});
        }

        std::string binary = parserBinaryForProtocol(config_.protocol);
        std::vector<std::string> args;
        args.push_back(binary);
        args.push_back("--control-socket");
        args.push_back(controlSocketPath(config_));
        args.push_back("--engine-endpoint");
        args.push_back(config_.engine_endpoint);
        args.push_back("--default-database");
        args.push_back(config_.database_owner);
        args.push_back("--log-level");
        args.push_back(config_.log_level);
        if (!config_.tls_config.empty()) {
            args.push_back("--tls-config");
            args.push_back(config_.tls_config);
        }

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
            return false;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
#else
        pid_t pid = fork();
        if (pid < 0) {
            return false;
        }
        if (pid == 0) {
            if (!config_.control_socket_dir.empty()) {
                std::string log_path = config_.control_socket_dir;
                if (!log_path.empty() && log_path.back() != '/') {
                    log_path += '/';
                }
                log_path += "parser_";
                log_path += std::to_string(getpid());
                log_path += ".stderr.log";
                int fd = scratchbird::core::platform::openFd(
                    log_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd >= 0) {
                    ::dup2(fd, STDERR_FILENO);
                    ::close(fd);
                }
            }
            std::vector<char*> argv;
            argv.reserve(args.size() + 1);
            for (auto& item : args) {
                argv.push_back(const_cast<char*>(item.c_str()));
            }
            argv.push_back(nullptr);
            execvp(argv[0], argv.data());
            _exit(127);
        }
#endif

        return true;
    }

    void startHealthChecks() {
        if (!metrics_.parser_healthcheck_seconds || config_.health_check_interval_ms == 0) {
            return;
        }
        healthcheck_running_.store(true, std::memory_order_release);
        healthcheck_thread_ = std::thread([this]() {
            while (healthcheck_running_.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config_.health_check_interval_ms));
                sendHealthChecks();
            }
        });
    }

    void stopHealthChecks() {
        healthcheck_running_.store(false, std::memory_order_release);
        if (healthcheck_thread_.joinable()) {
            healthcheck_thread_.join();
        }
    }

    void sendHealthChecks() {
        std::vector<std::shared_ptr<ParserWorker>> workers;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            workers = workers_;
        }

        for (const auto& worker : workers) {
            if (!worker->control) {
                continue;
            }
            std::lock_guard<std::mutex> lock(worker->mutex);
            if (!worker->running) {
                continue;
            }
            if (worker->awaiting_ack) {
                continue;
            }
            scratchbird::network::ControlPlaneMessage msg;
            msg.header.message_type = static_cast<uint16_t>(
                scratchbird::network::ControlPlaneMessageType::HEALTH_CHECK);
            msg.header.request_id = nextRequestId();
            msg.header.payload_len = 0;
            scratchbird::network::sendControlPlaneMessage(
                *worker->control, msg, scratchbird::network::INVALID_SOCKET_VALUE, 0, nullptr);
            worker->healthcheck_request = msg.header.request_id;
            worker->healthcheck_start = std::chrono::steady_clock::now();
        }
    }

    void readerLoop(const std::shared_ptr<ParserWorker>& worker) {
        scratchbird::core::ErrorContext ctx;
        while (worker->running) {
            scratchbird::network::ControlPlaneMessage msg;
            auto status = scratchbird::network::receiveControlPlaneMessage(*worker->control,
                                                                           msg, nullptr, &ctx);
            if (status != scratchbird::core::Status::OK) {
                std::cerr << "[listener_debug] reader error worker_id="
                          << worker->worker_id << " pid=" << worker->worker_pid
                          << " err=" << ctx.message << "\n";
                bool recycle_requested = false;
                {
                    std::lock_guard<std::mutex> lock(worker->mutex);
                    recycle_requested = worker->recycle_requested;
                }
                if (recycle_requested) {
                    std::lock_guard<std::mutex> lock(worker->mutex);
                    worker->running = false;
                    updateMetrics();
                    ensureMinWorkers();
                } else {
                    markWorkerFault(worker, "error");
                }
#ifndef _WIN32
                int exit_status = 0;
                pid_t exited = waitpid(worker->worker_pid, &exit_status, WNOHANG);
                (void)exited;
#endif
                break;
            }
            auto type = static_cast<scratchbird::network::ControlPlaneMessageType>(
                msg.header.message_type);
            std::cerr << "[listener_debug] reader msg type=" << static_cast<int>(type)
                      << " req=" << msg.header.request_id
                      << " worker_id=" << worker->worker_id << "\n";
            if (type == scratchbird::network::ControlPlaneMessageType::HANDOFF_ACK) {
                handleHandoffAck(worker, msg);
            } else if (type == scratchbird::network::ControlPlaneMessageType::HEALTH_REPORT) {
                handleHealthReport(worker, msg);
            } else if (type == scratchbird::network::ControlPlaneMessageType::ERROR) {
                markWorkerFault(worker, "error");
            }
        }
    }

    void handleHandoffAck(const std::shared_ptr<ParserWorker>& worker,
                          const scratchbird::network::ControlPlaneMessage& msg) {
        bool ok = false;
        if (msg.payload.size() >= 9) {
            uint8_t status = msg.payload[8];
            ok = (status == 0);
        }
        std::lock_guard<std::mutex> lock(worker->mutex);
        if (worker->awaiting_ack && worker->awaiting_request == msg.header.request_id) {
            worker->last_ack_ok = ok;
            worker->last_ack_ready = true;
            worker->awaiting_ack = false;
            worker->cv.notify_all();
        }
    }

    void handleHealthReport(const std::shared_ptr<ParserWorker>& worker,
                            const scratchbird::network::ControlPlaneMessage& msg) {
        if (msg.payload.size() < 15) {
            return;
        }
        uint8_t state = msg.payload[8];
        uint32_t last_error = readU32(msg.payload.data() + 11);
        bool record_healthcheck = false;
        std::chrono::steady_clock::time_point health_start;
        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            if (worker->healthcheck_request == msg.header.request_id) {
                record_healthcheck = true;
                health_start = worker->healthcheck_start;
                worker->healthcheck_request = 0;
            }
        }
        if (record_healthcheck && metrics_.parser_healthcheck_seconds) {
            auto elapsed = std::chrono::steady_clock::now() - health_start;
            metrics_.parser_healthcheck_seconds->observe(
                std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count(),
                {config_.protocol, "default"});
        }

        std::lock_guard<std::mutex> lock(worker->mutex);
        if (state == 0) {
            worker->state = WorkerState::IDLE;
        } else if (state == 2) {
            worker->state = WorkerState::DRAINING;
        } else if (state == 3) {
            worker->state = WorkerState::FAULT;
        } else {
            worker->state = WorkerState::BUSY;
        }

        if (worker->session_active && worker->state == WorkerState::IDLE &&
            metrics_.parser_session_seconds) {
            auto elapsed = std::chrono::steady_clock::now() - worker->session_start;
            metrics_.parser_session_seconds->observe(
                std::chrono::duration_cast<std::chrono::duration<double>>(elapsed).count(),
                {config_.protocol, "default"});
            worker->session_active = false;
            worker->active_connection_id = 0;
            worker->sessions_completed += 1;
            maybeRecycleWorker(worker);
        }

        if (last_error != 0 && metrics_.parser_errors_total) {
            metrics_.parser_errors_total->inc(1.0,
                                              {config_.protocol, "default",
                                               errorCategory(last_error)});
        }
        updateMetrics();
        cv_.notify_all();
    }

    static std::vector<uint8_t> buildHelloAck(bool accepted, const std::string& reason) {
        std::vector<uint8_t> payload;
        payload.push_back(accepted ? 1 : 0);
        uint16_t len = static_cast<uint16_t>(reason.size());
        payload.push_back(static_cast<uint8_t>(len & 0xFF));
        payload.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        payload.insert(payload.end(), reason.begin(), reason.end());
        return payload;
    }

    static bool parseHello(const std::vector<uint8_t>& payload,
                           std::string& protocol,
                           uint32_t& pid,
                           uint64_t& worker_id) {
        if (payload.size() < 32) {
            return false;
        }
        size_t len = 0;
        while (len < 16 && payload[len] != 0) {
            ++len;
        }
        protocol.assign(reinterpret_cast<const char*>(payload.data()), len);
        pid = readU32(payload.data() + 16);
        worker_id = readU64(payload.data() + 20);
        return true;
    }

    std::vector<uint8_t> buildHandoffPayload(uint64_t connection_id,
                                             const scratchbird::network::NetworkAddress& addr,
                                             bool tls_active,
                                             const ListenerHandoffBindingContext& binding) const {
        std::vector<uint8_t> payload;
        payload.reserve(8 + 16 + 48 + 2 + 1 + 2 + 64 + 16 + 16 + 16 + 4);
        appendU64(payload, connection_id);
        char protocol[16];
        std::memset(protocol, 0, sizeof(protocol));
        std::memcpy(protocol, config_.protocol.c_str(),
                    std::min<size_t>(config_.protocol.size(), 15));
        payload.insert(payload.end(), protocol, protocol + sizeof(protocol));
        char client_addr[48];
        std::memset(client_addr, 0, sizeof(client_addr));
        std::string addr_str = addr.host;
        std::memcpy(client_addr, addr_str.c_str(),
                    std::min<size_t>(addr_str.size(), sizeof(client_addr) - 1));
        payload.insert(payload.end(), client_addr, client_addr + sizeof(client_addr));
        appendU16(payload, addr.port);
        payload.push_back(tls_active ? 1 : 0);
        appendU16(payload, 0);
        payload.insert(payload.end(), 64, 0);
        payload.insert(payload.end(), binding.db_uuid.begin(), binding.db_uuid.end());
        payload.insert(payload.end(), binding.dbbt_id.begin(), binding.dbbt_id.end());
        payload.insert(payload.end(),
                       binding.manager_session_id.begin(),
                       binding.manager_session_id.end());
        appendU32(payload, binding.listener_id);
        return payload;
    }

    static void appendU16(std::vector<uint8_t>& out, uint16_t value) {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }

    static void appendU64(std::vector<uint8_t>& out, uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }

    static void appendU32(std::vector<uint8_t>& out, uint32_t value) {
        out.push_back(static_cast<uint8_t>(value & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    }

    static uint32_t readU32(const uint8_t* data) {
        return static_cast<uint32_t>(data[0])
            | (static_cast<uint32_t>(data[1]) << 8)
            | (static_cast<uint32_t>(data[2]) << 16)
            | (static_cast<uint32_t>(data[3]) << 24);
    }

    static uint64_t readU64(const uint8_t* data) {
        uint64_t value = 0;
        for (int i = 7; i >= 0; --i) {
            value = (value << 8) | data[i];
        }
        return value;
    }

    void ensureMinWorkers() {
        while (true) {
            size_t running = 0;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                running = runningCountLocked();
            }
            if (running >= config_.pool_min) {
                break;
            }
            if (!spawnWorker()) {
                break;
            }
        }
    }

    void maybeRecycleWorker(const std::shared_ptr<ParserWorker>& worker) {
        if (worker->recycle_requested) {
            return;
        }
        if (config_.max_requests > 0 &&
            worker->sessions_completed >= config_.max_requests) {
            requestRecycle(worker, "max_requests");
            return;
        }
        if (config_.max_age_seconds > 0) {
            auto age = std::chrono::steady_clock::now() - worker->started_at;
            if (std::chrono::duration_cast<std::chrono::seconds>(age).count() >=
                config_.max_age_seconds) {
                requestRecycle(worker, "max_age");
            }
        }
    }

    void requestRecycle(const std::shared_ptr<ParserWorker>& worker, const std::string& reason) {
        uint16_t reason_code = 0;
        if (reason == "max_requests") {
            reason_code = 1;
        } else if (reason == "max_age") {
            reason_code = 2;
        } else if (reason == "error") {
            reason_code = 3;
        } else {
            reason_code = 4;
        }

        scratchbird::network::ControlPlaneMessage msg;
        msg.header.message_type = static_cast<uint16_t>(
            scratchbird::network::ControlPlaneMessageType::RECYCLE);
        msg.header.request_id = nextRequestId();
        msg.payload.push_back(static_cast<uint8_t>(reason_code & 0xFF));
        msg.payload.push_back(static_cast<uint8_t>((reason_code >> 8) & 0xFF));
        msg.header.payload_len = msg.payload.size();

        scratchbird::network::sendControlPlaneMessage(
            *worker->control, msg, scratchbird::network::INVALID_SOCKET_VALUE, 0, nullptr);

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->recycle_requested = true;
            worker->recycle_reason = reason;
            worker->state = WorkerState::DRAINING;
        }

        if (metrics_.parser_recycle_total) {
            metrics_.parser_recycle_total->inc(1.0, {config_.protocol, "default", reason});
        }
        updateMetrics();
    }

    static std::string errorCategory(uint32_t error_code) {
        if (error_code >= 1000 && error_code < 1100) {
            return "io";
        }
        if (error_code >= 2000 && error_code < 2100) {
            return "corruption";
        }
        if (error_code >= 3000 && error_code < 3100) {
            return "transaction";
        }
        if (error_code >= 4000 && error_code < 4100) {
            return "data";
        }
        if (error_code >= 4100 && error_code < 4200) {
            return "constraint";
        }
        if (error_code >= 4200 && error_code < 4300) {
            return "syntax";
        }
        if (error_code >= 4300 && error_code < 4400) {
            return "cursor";
        }
        if (error_code >= 4400 && error_code < 4500) {
            return "plpgsql";
        }
        if (error_code >= 5000 && error_code < 5100) {
            return "resource";
        }
        if (error_code >= 6000 && error_code < 6100) {
            return "connection";
        }
        if (error_code >= 7000 && error_code < 7100) {
            return "operational";
        }
        return "unknown";
    }
};

void dumpMetrics(const scratchbird::core::MetricsRegistry& registry) {
    auto now = std::chrono::system_clock::now();
    std::time_t ts = std::chrono::system_clock::to_time_t(now);
    std::cout << "\n--- Listener Metrics Dump (" << std::ctime(&ts) << ")---\n";
    std::cout << registry.exportPrometheus() << std::flush;
}

void printUsage(const char* program) {
    std::cout << SB_LISTENER_NAME << " (" << SB_LISTENER_PROTOCOL << ")\n\n"
              << "Usage:\n"
              << "  " << program << " [options]\n\n"
              << "Options:\n"
              << "  --config <file>             Config file path\n"
              << "  --listener-mode <mode>      direct|managed\n"
              << "  --bind <addr>               Bind address\n"
              << "  --port <port>               Listen port\n"
              << "  --control-socket-dir <dir>  Control socket directory\n"
              << "  --engine-endpoint <path>    Engine IPC endpoint\n"
              << "  --database-owner <name>     Owning database name\n"
              << "  --pool-min <n>              Minimum parser pool size\n"
              << "  --pool-max <n>              Maximum parser pool size\n"
              << "  --spawn-strategy <mode>     prefork|on_demand|hybrid\n"
              << "  --max-requests <n>          Recycle parser after N sessions\n"
              << "  --max-age-seconds <n>       Recycle parser after seconds\n"
              << "  --health-check-interval-ms <n>  Health check interval (0 disables)\n"
              << "  --listener-id <n>           DBBT listener id (default: port)\n"
              << "  --dbbt-keyring <file>       DBBT keyring file\n"
              << "  --dbbt-clock-skew-ms <n>    DBBT clock skew tolerance\n"
              << "  --dbbt-replay-cache-size <n> DBBT replay cache entries\n"
              << "  --require-proxy-binding     Legacy alias for managed listener mode\n"
              << "  --log-level <level>         info|debug|warn|error\n"
              << "  --help, -h                  Show this help\n"
              << "  --version                   Show version\n";
}

void printVersion() {
    std::cout << SB_LISTENER_NAME << " (" << SCRATCHBIRD_VERSION_STRING << ")\n";
}

bool parseArgsForConfig(int argc, char* argv[], ListenerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.show_help = true;
        } else if (arg == "--version") {
            config.show_version = true;
        } else if (arg == "--config" && i + 1 < argc) {
            config.config_path = argv[++i];
        } else if (arg.rfind("--config=", 0) == 0) {
            config.config_path = arg.substr(9);
        }
    }
    return true;
}

/**
 * Load TLS settings from config and determine if TLS is enabled.
 * This function uses the already parsed config to check if TLS is enabled.
 */
void loadTLSConfigFromParser(const scratchbird::server::ConfigParser& parser,
                              ListenerConfig& config) {
    const auto* ssl_section = parser.section("ssl");
    if (!ssl_section) {
        config.tls_enabled = false;
        return;  // No SSL section, TLS not enabled
    }

    // Check if TLS is explicitly enabled
    config.tls_enabled = ssl_section->getBool("enabled", false);
    
    if (config.tls_enabled) {
        std::cerr << "[listener] TLS enabled for " << config.protocol << " protocol\n";
    }
}

bool applyConfigFile(ListenerConfig& config) {
    if (config.config_path.empty()) {
        return true;
    }

    scratchbird::server::ConfigParser parser;
    scratchbird::core::ErrorContext ctx;
    if (parser.parseFile(config.config_path, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Failed to parse config: " << ctx.message << "\n";
        return false;
    }

    const auto* network = parser.section("network");
    if (network) {
        config.bind_address = network->getString("bind_address", config.bind_address);
        std::string key = protocolKey(config.protocol) + "_port";
        config.port = static_cast<uint16_t>(network->getInt(key, config.port));
        std::string min_key = protocolKey(config.protocol) + "_pool_min";
        std::string max_key = protocolKey(config.protocol) + "_pool_max";
        std::string health_key = protocolKey(config.protocol) + "_health_check_interval_ms";
        config.pool_min = static_cast<uint32_t>(network->getInt(min_key, config.pool_min));
        config.pool_max = static_cast<uint32_t>(network->getInt(max_key, config.pool_max));
        config.health_check_interval_ms = static_cast<uint32_t>(
            network->getInt(health_key, config.health_check_interval_ms));
        if (network->has("control_socket_dir")) {
            config.control_socket_dir = network->getString("control_socket_dir",
                                                           config.control_socket_dir);
        }
        if (network->has("spawn_strategy")) {
            config.spawn_strategy = network->getString("spawn_strategy", config.spawn_strategy);
        }
    }

    const auto* server = parser.section("server");
    if (server) {
        if (server->has("control_socket_dir")) {
            config.control_socket_dir = server->getString("control_socket_dir",
                                                          config.control_socket_dir);
        }
        if (server->has("health_check_interval_ms")) {
            config.health_check_interval_ms = static_cast<uint32_t>(
                server->getInt("health_check_interval_ms", config.health_check_interval_ms));
        }
        if (config.engine_endpoint.empty() && server->has("database")) {
            auto db_path = server->getString("database", "");
            if (!db_path.empty()) {
                config.engine_endpoint = scratchbird::server::getIPCPath(db_path,
                                                                         scratchbird::server::IPCMethod::AUTO);
            }
        }
    }

    if (!config.config_path.empty() && parser.hasSection("ssl")) {
        config.tls_config = config.config_path;
    }

    const auto* manager = parser.section("manager");
    if (manager) {
        config.listener_mode = manager->getString("listener_mode", config.listener_mode);
        if (manager->has("mode")) {
            config.listener_mode = manager->getString("mode", config.listener_mode);
        }
        config.listener_id = static_cast<uint32_t>(
            manager->getInt("listener_id", config.listener_id));
        config.dbbt_keyring_path = manager->getString(
            "dbbt_keyring", config.dbbt_keyring_path);
        config.dbbt_clock_skew_ms = static_cast<uint32_t>(
            manager->getInt("dbbt_clock_skew_ms", config.dbbt_clock_skew_ms));
        config.dbbt_replay_cache_size = static_cast<uint32_t>(
            manager->getInt("dbbt_replay_cache_size", config.dbbt_replay_cache_size));
        config.require_proxy_binding = manager->getBool(
            "require_proxy_binding", config.require_proxy_binding);
        if (config.require_proxy_binding &&
            normalizeListenerModeToken(config.listener_mode) == "DIRECT") {
            config.listener_mode = "managed";
        }
    }

    // Load TLS settings from the already parsed config
    loadTLSConfigFromParser(parser, config);

    return true;
}

bool applyArgOverrides(int argc, char* argv[], ListenerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--listener-mode" && i + 1 < argc) {
            config.listener_mode = argv[++i];
        } else if (arg.rfind("--listener-mode=", 0) == 0) {
            config.listener_mode = arg.substr(16);
        } else if (arg == "--mode" && i + 1 < argc) {
            config.listener_mode = argv[++i];
        } else if (arg.rfind("--mode=", 0) == 0) {
            config.listener_mode = arg.substr(7);
        } else if (arg == "--managed-mode") {
            config.listener_mode = "managed";
        } else if (arg == "--direct-mode") {
            config.listener_mode = "direct";
            config.require_proxy_binding = false;
        } else if (arg == "--bind" && i + 1 < argc) {
            config.bind_address = argv[++i];
        } else if (arg.rfind("--bind=", 0) == 0) {
            config.bind_address = arg.substr(7);
        } else if (arg == "--port" && i + 1 < argc) {
            try {
                config.port = static_cast<uint16_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid port value\n";
                return false;
            }
        } else if (arg.rfind("--port=", 0) == 0) {
            try {
                config.port = static_cast<uint16_t>(std::stoul(arg.substr(7)));
            } catch (...) {
                std::cerr << "Invalid port value\n";
                return false;
            }
        } else if (arg == "--control-socket-dir" && i + 1 < argc) {
            config.control_socket_dir = argv[++i];
        } else if (arg.rfind("--control-socket-dir=", 0) == 0) {
            config.control_socket_dir = arg.substr(21);
        } else if (arg == "--engine-endpoint" && i + 1 < argc) {
            config.engine_endpoint = argv[++i];
        } else if (arg.rfind("--engine-endpoint=", 0) == 0) {
            config.engine_endpoint = arg.substr(18);
        } else if (arg == "--database-owner" && i + 1 < argc) {
            config.database_owner = argv[++i];
        } else if (arg.rfind("--database-owner=", 0) == 0) {
            config.database_owner = arg.substr(17);
        } else if (arg == "--pool-min" && i + 1 < argc) {
            try {
                config.pool_min = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid pool-min value\n";
                return false;
            }
        } else if (arg.rfind("--pool-min=", 0) == 0) {
            try {
                config.pool_min = static_cast<uint32_t>(std::stoul(arg.substr(11)));
            } catch (...) {
                std::cerr << "Invalid pool-min value\n";
                return false;
            }
        } else if (arg == "--pool-max" && i + 1 < argc) {
            try {
                config.pool_max = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid pool-max value\n";
                return false;
            }
        } else if (arg.rfind("--pool-max=", 0) == 0) {
            try {
                config.pool_max = static_cast<uint32_t>(std::stoul(arg.substr(11)));
            } catch (...) {
                std::cerr << "Invalid pool-max value\n";
                return false;
            }
        } else if (arg == "--spawn-strategy" && i + 1 < argc) {
            config.spawn_strategy = argv[++i];
        } else if (arg.rfind("--spawn-strategy=", 0) == 0) {
            config.spawn_strategy = arg.substr(17);
        } else if (arg == "--max-requests" && i + 1 < argc) {
            try {
                config.max_requests = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid max-requests value\n";
                return false;
            }
        } else if (arg.rfind("--max-requests=", 0) == 0) {
            try {
                config.max_requests = static_cast<uint32_t>(std::stoul(arg.substr(15)));
            } catch (...) {
                std::cerr << "Invalid max-requests value\n";
                return false;
            }
        } else if (arg == "--max-age-seconds" && i + 1 < argc) {
            try {
                config.max_age_seconds = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid max-age-seconds value\n";
                return false;
            }
        } else if (arg.rfind("--max-age-seconds=", 0) == 0) {
            try {
                config.max_age_seconds = static_cast<uint32_t>(std::stoul(arg.substr(18)));
            } catch (...) {
                std::cerr << "Invalid max-age-seconds value\n";
                return false;
            }
        } else if (arg == "--health-check-interval-ms" && i + 1 < argc) {
            try {
                config.health_check_interval_ms = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid health-check-interval-ms value\n";
                return false;
            }
        } else if (arg.rfind("--health-check-interval-ms=", 0) == 0) {
            try {
                config.health_check_interval_ms = static_cast<uint32_t>(std::stoul(arg.substr(27)));
            } catch (...) {
                std::cerr << "Invalid health-check-interval-ms value\n";
                return false;
            }
        } else if (arg == "--log-level" && i + 1 < argc) {
            config.log_level = argv[++i];
        } else if (arg.rfind("--log-level=", 0) == 0) {
            config.log_level = arg.substr(12);
        } else if (arg == "--listener-id" && i + 1 < argc) {
            try {
                config.listener_id = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid listener-id value\n";
                return false;
            }
        } else if (arg.rfind("--listener-id=", 0) == 0) {
            try {
                config.listener_id = static_cast<uint32_t>(std::stoul(arg.substr(14)));
            } catch (...) {
                std::cerr << "Invalid listener-id value\n";
                return false;
            }
        } else if (arg == "--dbbt-keyring" && i + 1 < argc) {
            config.dbbt_keyring_path = argv[++i];
        } else if (arg.rfind("--dbbt-keyring=", 0) == 0) {
            config.dbbt_keyring_path = arg.substr(15);
        } else if (arg == "--dbbt-clock-skew-ms" && i + 1 < argc) {
            try {
                config.dbbt_clock_skew_ms = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid dbbt-clock-skew-ms value\n";
                return false;
            }
        } else if (arg.rfind("--dbbt-clock-skew-ms=", 0) == 0) {
            try {
                config.dbbt_clock_skew_ms = static_cast<uint32_t>(std::stoul(arg.substr(21)));
            } catch (...) {
                std::cerr << "Invalid dbbt-clock-skew-ms value\n";
                return false;
            }
        } else if (arg == "--dbbt-replay-cache-size" && i + 1 < argc) {
            try {
                config.dbbt_replay_cache_size = static_cast<uint32_t>(std::stoul(argv[++i]));
            } catch (...) {
                std::cerr << "Invalid dbbt-replay-cache-size value\n";
                return false;
            }
        } else if (arg.rfind("--dbbt-replay-cache-size=", 0) == 0) {
            try {
                config.dbbt_replay_cache_size = static_cast<uint32_t>(std::stoul(arg.substr(25)));
            } catch (...) {
                std::cerr << "Invalid dbbt-replay-cache-size value\n";
                return false;
            }
        } else if (arg == "--require-proxy-binding") {
            config.require_proxy_binding = true;
            if (normalizeListenerModeToken(config.listener_mode) == "DIRECT") {
                config.listener_mode = "managed";
            }
        }
    }
    return true;
}

static std::vector<uint8_t> buildManagementResponsePayload(bool ok, const std::string& text) {
    std::vector<uint8_t> payload;
    payload.reserve(1 + text.size());
    payload.push_back(ok ? 0 : 1);
    payload.insert(payload.end(), text.begin(), text.end());
    return payload;
}

static std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

static void logManagedAuditEvent(const char* event_name,
                                 bool success,
                                 const std::string& reason,
                                 const std::vector<uint8_t>& dbbt_id = {}) {
    std::cerr << "[audit] event="
              << (event_name ? event_name : "UNKNOWN")
              << " success=" << (success ? 1 : 0);
    if (!reason.empty()) {
        std::cerr << " reason=" << reason;
    }
    if (!dbbt_id.empty()) {
        std::cerr << " dbbt_id=" << scratchbird::network::bytesToHex(dbbt_id);
    }
    std::cerr << "\n";
}

static bool loadListenerDbbtKeyRing(const ListenerConfig& config,
                                    scratchbird::network::DatabaseBindingKeyRing& key_ring,
                                    std::string& source,
                                    scratchbird::core::ErrorContext* ctx) {
    using scratchbird::network::DatabaseBindingKeyRing;
    using scratchbird::core::Status;

    if (!config.dbbt_keyring_path.empty()) {
        auto status = DatabaseBindingKeyRing::loadFromTextFile(
            config.dbbt_keyring_path, key_ring, ctx);
        if (status != Status::OK) {
            return false;
        }
        source = config.dbbt_keyring_path;
        return true;
    }

    const char* shared_hex = std::getenv("SCRATCHBIRD_DBBT_SHARED_KEY_HEX");
    if (shared_hex != nullptr && shared_hex[0] != '\0') {
        std::vector<uint8_t> key_bytes;
        if (!scratchbird::network::hexToBytes(shared_hex, key_bytes) ||
            !key_ring.addKey("env", key_bytes, true, ctx)) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid SCRATCHBIRD_DBBT_SHARED_KEY_HEX");
            return false;
        }
        source = "env:SCRATCHBIRD_DBBT_SHARED_KEY_HEX";
        return true;
    }

    constexpr const char* kDevFallbackHex =
        "73637261746368626972645f6465765f646262745f7368617265645f6b65795f7631";
    std::vector<uint8_t> fallback_key;
    if (!scratchbird::network::hexToBytes(kDevFallbackHex, fallback_key) ||
        !key_ring.addKey("default", fallback_key, true, ctx)) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to initialize default DBBT key");
        return false;
    }
    source = "builtin:default";
    return true;
}

static bool reloadListenerRuntimeConfig(ListenerConfig& config,
                                        ParserPool& pool,
                                        std::string& out_message) {
    if (config.config_path.empty()) {
        out_message = "reload_failed:no_config_path";
        return false;
    }

    ListenerConfig refreshed = config;
    if (!applyConfigFile(refreshed)) {
        out_message = "reload_failed:config_parse";
        return false;
    }
    refreshed.port = config.port;
    refreshed.bind_address = config.bind_address;
    refreshed.control_socket_dir = config.control_socket_dir;
    refreshed.engine_endpoint = config.engine_endpoint;

    std::string error;
    if (!pool.applyRuntimeConfig(refreshed.pool_min,
                                 refreshed.pool_max,
                                 refreshed.health_check_interval_ms,
                                 error)) {
        out_message = "reload_failed:" + error;
        return false;
    }

    config.pool_min = refreshed.pool_min;
    config.pool_max = refreshed.pool_max;
    config.health_check_interval_ms = refreshed.health_check_interval_ms;
    config.max_requests = refreshed.max_requests;
    config.max_age_seconds = refreshed.max_age_seconds;
    config.spawn_strategy = refreshed.spawn_strategy;
    config.log_level = refreshed.log_level;
    out_message = "reloaded";
    return true;
}

static void handleManagementConnection(std::unique_ptr<scratchbird::network::Socket> socket,
                                       ListenerConfig& config,
                                       ParserPool& pool,
                                       scratchbird::network::DatabaseBindingKeyRing& dbbt_key_ring,
                                       scratchbird::network::DatabaseBindingReplayCache& dbbt_replay_cache) {
    scratchbird::core::ErrorContext ctx;
    scratchbird::network::ControlPlaneMessage request;
    auto status = scratchbird::network::receiveControlPlaneMessage(*socket, request, nullptr, &ctx);
    if (status != scratchbird::core::Status::OK) {
        socket->close();
        return;
    }

    scratchbird::network::ControlPlaneMessage response;
    response.header.message_type = static_cast<uint16_t>(
        scratchbird::network::ControlPlaneMessageType::MANAGEMENT_RESPONSE);
    response.header.request_id = request.header.request_id;

    if (request.header.message_type != static_cast<uint16_t>(
            scratchbird::network::ControlPlaneMessageType::MANAGEMENT_COMMAND)) {
        response.payload = buildManagementResponsePayload(false, "invalid_message_type");
        response.header.payload_len = response.payload.size();
        scratchbird::network::sendControlPlaneMessage(*socket, response,
                                                      scratchbird::network::INVALID_SOCKET_VALUE,
                                                      0, nullptr);
        socket->close();
        return;
    }

    std::string command(reinterpret_cast<const char*>(request.payload.data()),
                        request.payload.size());
    command = trimAscii(command);

    bool ok = false;
    std::string message;
    if (command == "PING") {
        ok = true;
        message = "PONG";
    } else if (command == "STATUS") {
        std::ostringstream oss;
        oss << "draining=" << (g_draining.load(std::memory_order_acquire) ? 1 : 0)
            << ";owner_database=" << config.database_owner
            << ";active_sessions=" << pool.activeSessionCount()
            << ";warm_workers=" << pool.warmWorkerCount()
            << ";pool_min=" << config.pool_min
            << ";pool_max=" << config.pool_max;
        ok = true;
        message = oss.str();
    } else if (command == "STOP graceful") {
        g_draining.store(true, std::memory_order_release);
        pool.setDraining(true);
        ok = true;
        message = "draining";
    } else if (command == "STOP force") {
        g_force_shutdown.store(true, std::memory_order_release);
        g_shutdown.store(true, std::memory_order_release);
        ok = true;
        message = "force_shutdown";
    } else if (command == "RELOAD") {
        ok = reloadListenerRuntimeConfig(config, pool, message);
    } else if (command.rfind("POOL SET ", 0) == 0) {
        std::istringstream iss(command.substr(9));
        uint32_t min_value = 0;
        uint32_t max_value = 0;
        if (iss >> min_value >> max_value) {
            std::string error;
            ok = pool.applyRuntimeConfig(min_value, max_value,
                                         config.health_check_interval_ms,
                                         error);
            if (ok) {
                config.pool_min = min_value;
                config.pool_max = max_value;
                message = "pool_updated";
            } else {
                message = "pool_update_failed:" + error;
            }
        } else {
            ok = false;
            message = "pool_update_failed:invalid_args";
        }
    } else if (command.rfind("KILL ", 0) == 0) {
        uint64_t connection_id = 0;
        try {
            connection_id = static_cast<uint64_t>(std::stoull(command.substr(5)));
        } catch (...) {
            connection_id = 0;
        }
        if (connection_id == 0) {
            ok = false;
            message = "kill_failed:invalid_connection_id";
        } else {
            std::string error;
            ok = pool.terminateConnection(connection_id, error);
            message = ok ? "killed" : ("kill_failed:" + error);
        }
    } else if (command.rfind("DBBT_VALIDATE ", 0) == 0) {
        std::string hex_token = trimAscii(command.substr(14));
        std::vector<uint8_t> encoded_token;
        if (!scratchbird::network::hexToBytes(hex_token, encoded_token)) {
            ok = false;
            message = "dbbt_invalid_hex";
        } else {
            scratchbird::network::DatabaseBindingValidationOptions opts;
            opts.expected_listener_id = config.listener_id;
            opts.now_ms = scratchbird::network::currentEpochMillis();
            opts.clock_skew_ms = config.dbbt_clock_skew_ms;
            opts.enforce_replay = true;

            scratchbird::network::DatabaseBindingToken token;
            scratchbird::core::ErrorContext validate_ctx;
            auto validate_status = scratchbird::network::validateDatabaseBindingToken(
                encoded_token, dbbt_key_ring, opts, &dbbt_replay_cache, &token, &validate_ctx);
            if (validate_status == scratchbird::core::Status::OK) {
                ok = true;
                const auto token_id = scratchbird::network::databaseBindingTokenId(token);
                message = std::string("dbbt_valid:") +
                          scratchbird::network::bytesToHex(token_id);
            } else {
                ok = false;
                if (validate_ctx.message.empty()) {
                    message = "dbbt_invalid";
                } else {
                    message = "dbbt_invalid:" + validate_ctx.message;
                }
            }
        }
    } else if (command.rfind("LPREFACE_VALIDATE ", 0) == 0) {
        std::string hex_preface = trimAscii(command.substr(17));
        std::vector<uint8_t> encoded_preface;
        if (!scratchbird::network::hexToBytes(hex_preface, encoded_preface)) {
            ok = false;
            logManagedAuditEvent("MANAGED_PREFACE_DECISION", false, "invalid_hex");
            scratchbird::network::ListenerPrefaceAck nack;
            nack.accepted = false;
            nack.nack_code = scratchbird::network::ListenerPrefaceNackCode::INVALID_FORMAT;
            nack.message = "invalid_hex";
            std::vector<uint8_t> ack_payload;
            scratchbird::network::encodeListenerPrefaceAck(nack, ack_payload, nullptr);
            message = std::string("lpreface_nack:") + scratchbird::network::bytesToHex(ack_payload);
        } else {
            scratchbird::network::DatabaseBindingValidationOptions opts;
            opts.expected_listener_id = config.listener_id;
            opts.now_ms = scratchbird::network::currentEpochMillis();
            opts.clock_skew_ms = config.dbbt_clock_skew_ms;
            opts.enforce_replay = true;

            scratchbird::network::ListenerPrefaceV1 preface;
            scratchbird::network::DatabaseBindingToken token;
            scratchbird::core::ErrorContext validate_ctx;
            const auto validate_status = scratchbird::network::validateListenerPrefaceV1(
                encoded_preface,
                dbbt_key_ring,
                opts,
                &dbbt_replay_cache,
                &preface,
                &token,
                &validate_ctx);

            if (validate_status == scratchbird::core::Status::OK) {
                ok = true;
                ListenerHandoffBindingContext binding;
                binding.db_uuid = token.db_uuid;
                binding.manager_session_id = token.manager_session_id;
                const auto token_id = scratchbird::network::databaseBindingTokenId(token);
                if (token_id.size() == binding.dbbt_id.size()) {
                    std::copy(token_id.begin(), token_id.end(), binding.dbbt_id.begin());
                }
                binding.listener_id =
                    preface.listener_id != 0 ? preface.listener_id : token.listener_id;
                if (binding.listener_id == 0) {
                    binding.listener_id = config.listener_id;
                }
                pool.queueBindingContext(binding);

                scratchbird::network::ListenerPrefaceAck ack;
                ack.accepted = true;
                ack.nack_code = scratchbird::network::ListenerPrefaceNackCode::NONE;
                ack.message = "ok";
                std::vector<uint8_t> ack_payload;
                scratchbird::network::encodeListenerPrefaceAck(ack, ack_payload, nullptr);
                message = std::string("lpreface_ack:") + scratchbird::network::bytesToHex(ack_payload);
                logManagedAuditEvent("MANAGED_PREFACE_DECISION", true, "accepted", token_id);
            } else {
                ok = false;
                scratchbird::network::ListenerPrefaceNackCode nack_code =
                    scratchbird::network::ListenerPrefaceNackCode::INVALID_DBBT;
                if (validate_status == scratchbird::core::Status::PROTOCOL_VIOLATION) {
                    nack_code = scratchbird::network::ListenerPrefaceNackCode::INVALID_FORMAT;
                } else if (validate_ctx.message.find("listener") != std::string::npos) {
                    nack_code = scratchbird::network::ListenerPrefaceNackCode::LISTENER_MISMATCH;
                }
                scratchbird::network::ListenerPrefaceAck nack;
                nack.accepted = false;
                nack.nack_code = nack_code;
                nack.message = validate_ctx.message.empty() ? "invalid_lpreface" : validate_ctx.message;
                std::vector<uint8_t> ack_payload;
                scratchbird::network::encodeListenerPrefaceAck(nack, ack_payload, nullptr);
                message = std::string("lpreface_nack:") +
                          scratchbird::network::bytesToHex(ack_payload);
                logManagedAuditEvent("MANAGED_PREFACE_DECISION",
                                     false,
                                     validate_ctx.message.empty() ? "invalid_lpreface"
                                                                  : validate_ctx.message);
            }
        }
    } else {
        ok = false;
        message = "unknown_command";
    }

    response.payload = buildManagementResponsePayload(ok, message);
    response.header.payload_len = response.payload.size();
    scratchbird::network::sendControlPlaneMessage(*socket, response,
                                                  scratchbird::network::INVALID_SOCKET_VALUE,
                                                  0, nullptr);
    socket->close();
}

int runListener(ListenerConfig config) {
    using scratchbird::network::AddressFamily;
    using scratchbird::network::NetworkAddress;
    using scratchbird::network::Socket;
    using scratchbird::core::MetricsRegistry;

    if (!scratchbird::network::initNetwork()) {
        std::cerr << "Failed to initialize network subsystem\n";
        return 2;
    }

    if (config.listener_id == 0) {
        config.listener_id = config.port;
    }

    scratchbird::network::DatabaseBindingKeyRing dbbt_key_ring;
    std::string dbbt_keyring_source;
    scratchbird::core::ErrorContext key_ctx;
    if (!loadListenerDbbtKeyRing(config, dbbt_key_ring, dbbt_keyring_source, &key_ctx)) {
        std::cerr << "Failed to initialize DBBT keyring: " << key_ctx.message << "\n";
        return 2;
    }
    (void)dbbt_keyring_source;
    scratchbird::network::DatabaseBindingReplayCache dbbt_replay_cache(
        std::max<uint32_t>(1u, config.dbbt_replay_cache_size));

    auto& metrics = MetricsRegistry::getInstance();
    auto* connections_total = metrics.registerCounter(
        "scratchbird_listener_connections_total",
        "Total connections observed by listener",
        {"protocol", "listener"});
    auto* accept_total = metrics.registerCounter(
        "scratchbird_listener_accept_total",
        "Total accepted connections",
        {"protocol", "listener"});
    auto* reject_total = metrics.registerCounter(
        "scratchbird_listener_reject_total",
        "Total rejected connections",
        {"protocol", "listener", "reason"});
    auto* open_connections = metrics.registerGauge(
        "scratchbird_listener_open_connections",
        "Open connections currently tracked by listener",
        {"protocol", "listener"});
    auto* queue_depth = metrics.registerGauge(
        "scratchbird_listener_queue_depth",
        "Listener accept queue depth",
        {"protocol", "listener"});
    auto* handoff_seconds = metrics.registerHistogram(
        "scratchbird_listener_handoff_seconds",
        "Listener handoff latency",
        scratchbird::core::Histogram::DEFAULT_LATENCY_BUCKETS,
        {"protocol", "listener"});
    auto* queue_wait_seconds = metrics.registerHistogram(
        "scratchbird_listener_queue_wait_seconds",
        "Listener queue wait time",
        scratchbird::core::Histogram::DEFAULT_LATENCY_BUCKETS,
        {"protocol", "listener"});
    auto* parser_spawn_total = metrics.registerCounter(
        "scratchbird_parser_spawn_total",
        "Parser spawn count",
        {"protocol", "pool"});
    auto* parser_recycle_total = metrics.registerCounter(
        "scratchbird_parser_recycle_total",
        "Parser recycle count",
        {"protocol", "pool", "reason"});
    auto* parser_errors_total = metrics.registerCounter(
        "scratchbird_parser_errors_total",
        "Parser errors",
        {"protocol", "pool", "category"});
    auto* parser_pool_size = metrics.registerGauge(
        "scratchbird_parser_pool_size",
        "Parser pool size",
        {"protocol", "pool"});
    auto* parser_pool_idle = metrics.registerGauge(
        "scratchbird_parser_pool_idle",
        "Parser pool idle count",
        {"protocol", "pool"});
    auto* parser_pool_busy = metrics.registerGauge(
        "scratchbird_parser_pool_busy",
        "Parser pool busy count",
        {"protocol", "pool"});
    auto* parser_session_seconds = metrics.registerHistogram(
        "scratchbird_parser_session_seconds",
        "Parser session duration",
        scratchbird::core::Histogram::DEFAULT_LATENCY_BUCKETS,
        {"protocol", "pool"});
    auto* parser_healthcheck_seconds = metrics.registerHistogram(
        "scratchbird_parser_healthcheck_seconds",
        "Parser healthcheck duration",
        scratchbird::core::Histogram::DEFAULT_LATENCY_BUCKETS,
        {"protocol", "pool"});

    std::string control_socket = controlSocketPath(config);
    std::string management_socket = managementSocketPath(config);
    scratchbird::core::ErrorContext ctx;
    auto control_plane = scratchbird::network::createDefaultLocalControlChannel();
    auto management_plane = scratchbird::network::createDefaultLocalControlChannel();
    auto front_door = scratchbird::network::createDefaultListenerSocketAcceptor();
    std::thread control_thread;
    std::thread management_thread;
    PoolMetrics pool_metrics;
    pool_metrics.parser_spawn_total = parser_spawn_total;
    pool_metrics.parser_recycle_total = parser_recycle_total;
    pool_metrics.parser_errors_total = parser_errors_total;
    pool_metrics.parser_pool_size = parser_pool_size;
    pool_metrics.parser_pool_idle = parser_pool_idle;
    pool_metrics.parser_pool_busy = parser_pool_busy;
    pool_metrics.parser_session_seconds = parser_session_seconds;
    pool_metrics.parser_healthcheck_seconds = parser_healthcheck_seconds;

    ParserPool pool(config, pool_metrics, handoff_seconds, queue_wait_seconds);
    if (control_socket.empty()) {
        std::cerr << "Control socket path not configured\n";
        return 2;
    }
    if (!control_plane || !management_plane || !front_door) {
        std::cerr << "Failed to initialize listener IPC adapters\n";
        return 2;
    }

    if (control_plane->start(control_socket, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Failed to start control socket: " << ctx.message << "\n";
        return 2;
    }
    std::cout << "Control socket: " << control_socket << "\n";
    control_thread = std::thread([&]() {
        scratchbird::core::ErrorContext local_ctx;
        while (!g_shutdown.load(std::memory_order_acquire)) {
            pollRuntimeSignals();
            auto control_conn = control_plane->accept(&local_ctx);
            if (!control_conn) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }
            pool.handleControlConnection(std::move(control_conn));
        }
    });
    if (!pool.start()) {
        std::cerr << "Failed to start parser pool\n";
        g_shutdown.store(true, std::memory_order_release);
        control_plane->stop();
        if (control_thread.joinable()) {
            control_thread.join();
        }
        g_shutdown.store(false, std::memory_order_release);
        return 2;
    }
    if (!pool.waitForWarm(std::chrono::seconds(10))) {
        std::cerr << "Failed to reach warm pool minimum before listener startup\n";
        g_shutdown.store(true, std::memory_order_release);
        pool.stop();
        control_plane->stop();
        if (control_thread.joinable()) {
            control_thread.join();
        }
        g_shutdown.store(false, std::memory_order_release);
        return 2;
    }

    if (!management_socket.empty()) {
        if (management_plane->start(management_socket, &ctx) == scratchbird::core::Status::OK) {
            std::cout << "Management socket: " << management_socket << "\n";
            management_thread = std::thread([&]() {
                scratchbird::core::ErrorContext local_ctx;
                while (!g_shutdown.load(std::memory_order_acquire)) {
                    pollRuntimeSignals();
                    auto mgmt_conn = management_plane->accept(&local_ctx);
                    if (!mgmt_conn) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        continue;
                    }
                    handleManagementConnection(std::move(mgmt_conn), config, pool,
                                               dbbt_key_ring, dbbt_replay_cache);
                }
            });
        } else {
            std::cerr << "Failed to start management socket: " << ctx.message << "\n";
            g_shutdown.store(true, std::memory_order_release);
            pool.stop();
            control_plane->stop();
            if (control_thread.joinable()) {
                control_thread.join();
            }
            g_shutdown.store(false, std::memory_order_release);
            return 2;
        }
    }

    scratchbird::network::ListenerSocketConfig front_door_config;
    front_door_config.family = AddressFamily::IPV4;
    front_door_config.bind_address = config.bind_address;
    front_door_config.port = config.port;
    front_door_config.backlog = static_cast<int>(scratchbird::network::DEFAULT_LISTEN_BACKLOG);
    front_door_config.non_blocking = true;

    if (front_door->start(front_door_config, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Failed to start listener front-door socket: " << ctx.message << "\n";
        g_shutdown.store(true, std::memory_order_release);
        pool.stop();
        control_plane->stop();
        management_plane->stop();
        if (control_thread.joinable()) {
            control_thread.join();
        }
        if (management_thread.joinable()) {
            management_thread.join();
        }
        return 2;
    }

    std::cout << SB_LISTENER_NAME << " listening on "
              << config.bind_address << ":" << config.port << "\n";
    std::cout << "Listener mode: " << config.listener_mode
              << " (proxy_binding=" << (config.require_proxy_binding ? "required" : "off")
              << ")\n";
    std::cout << "Owner database: " << config.database_owner << "\n";
    std::cout << "Parser pool: min=" << config.pool_min
              << " max=" << config.pool_max
              << " strategy=" << config.spawn_strategy << "\n";
    if (config.engine_endpoint.empty()) {
        std::cout << "Engine endpoint: (unset)\n";
    } else {
        std::cout << "Engine endpoint: " << config.engine_endpoint << "\n";
    }

    std::vector<std::string> label = {config.protocol, SB_LISTENER_NAME};
    connections_total->inc(0.0, label);
    accept_total->inc(0.0, label);
    reject_total->inc(0.0, {config.protocol, SB_LISTENER_NAME, "error"});
    open_connections->set(0.0, label);
    queue_depth->set(0.0, label);
    handoff_seconds->observe(0.0, label);
    queue_wait_seconds->observe(0.0, label);
    parser_spawn_total->inc(0.0, {config.protocol, "default"});
    parser_recycle_total->inc(0.0, {config.protocol, "default", "manual"});
    parser_errors_total->inc(0.0, {config.protocol, "default", "none"});
    parser_pool_size->set(0.0, {config.protocol, "default"});
    parser_pool_idle->set(0.0, {config.protocol, "default"});
    parser_pool_busy->set(0.0, {config.protocol, "default"});
    parser_session_seconds->observe(0.0, {config.protocol, "default"});
    parser_healthcheck_seconds->observe(0.0, {config.protocol, "default"});

    // Accept loop with parser handoff.
    while (!g_shutdown.load(std::memory_order_acquire)) {
        pollRuntimeSignals();
        if (g_draining.load(std::memory_order_acquire) &&
            pool.activeSessionCount() == 0) {
            g_shutdown.store(true, std::memory_order_release);
            break;
        }
        if (g_dump_stats.exchange(false, std::memory_order_acq_rel)) {
            dumpMetrics(metrics);
        }
        NetworkAddress client_addr;
        auto client = front_door->accept(&client_addr, &ctx);
        if (!client) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        connections_total->inc(1.0, label);
        accept_total->inc(1.0, label);
        open_connections->inc(1.0, label);

        std::cerr << "[listener_debug] accepted client "
                  << client_addr.host << ":" << client_addr.port
                  << " fd=" << client->getFd() << "\n";
        if (g_draining.load(std::memory_order_acquire) || pool.isDraining()) {
            reject_total->inc(1.0, {config.protocol, SB_LISTENER_NAME, "draining"});
            client->close();
            open_connections->dec(1.0, label);
            continue;
        }
        auto worker = pool.acquireWorker(std::chrono::milliseconds(1000));
        if (!worker) {
            reject_total->inc(1.0, {config.protocol, SB_LISTENER_NAME, "queue_full"});
            client->close();
            open_connections->dec(1.0, label);
            continue;
        }

        bool handed_off = pool.handoff(worker, client->getFd(), client_addr, config.tls_enabled);
        client->close();
        open_connections->dec(1.0, label);

        if (!handed_off) {
            reject_total->inc(1.0, {config.protocol, SB_LISTENER_NAME, "error"});
        }
    }

    dumpMetrics(metrics);
    front_door->close();
    pool.stop();
    control_plane->stop();
    management_plane->stop();
    if (control_thread.joinable()) {
        control_thread.join();
    }
    if (management_thread.joinable()) {
        management_thread.join();
    }
    scratchbird::network::cleanupNetwork();
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    ListenerConfig config;
    config.port = defaultPortForProtocol(config.protocol);

    parseArgsForConfig(argc, argv, config);
    if (config.show_help) {
        printUsage(argv[0]);
        return 0;
    }
    if (config.show_version) {
        printVersion();
        return 0;
    }

    if (!applyConfigFile(config)) {
        return 1;
    }
    if (!applyArgOverrides(argc, argv, config)) {
        return 1;
    }

    if (config.port == 0) {
        std::cerr << "Invalid port\n";
        return 1;
    }
    if (config.listener_id == 0) {
        config.listener_id = config.port;
    }
    if (config.dbbt_clock_skew_ms == 0) {
        config.dbbt_clock_skew_ms = 2000;
    }
    if (config.dbbt_replay_cache_size == 0) {
        config.dbbt_replay_cache_size = 4096;
    }
    std::string mode_error;
    if (!normalizeAndValidateListenerMode(config, mode_error)) {
        std::cerr << "Invalid listener mode configuration: " << mode_error << "\n";
        return 1;
    }

    g_signal_control = scratchbird::core::createDefaultSignalControl();
    if (g_signal_control) {
        scratchbird::core::SignalInstallSpec signal_spec;
        signal_spec.enable_shutdown_signal = true;
        signal_spec.enable_reload_signal = true;
        signal_spec.enable_rotate_logs_signal = false;
        signal_spec.enable_dump_stats_signal = true;
        signal_spec.enable_immediate_stop_signal = true;
        signal_spec.ignore_broken_pipe = true;
        (void)g_signal_control->install(signal_spec, nullptr);
    }

    g_draining.store(false, std::memory_order_release);
    g_force_shutdown.store(false, std::memory_order_release);
    g_shutdown.store(false, std::memory_order_release);
    const int exit_code = runListener(config);
    if (g_signal_control) {
        (void)g_signal_control->uninstall(nullptr);
        g_signal_control.reset();
    }
    return exit_code;
}
