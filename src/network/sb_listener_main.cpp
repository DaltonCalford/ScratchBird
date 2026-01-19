/**
 * ScratchBird Listener - Protocol-Specific Entry Point
 *
 * Scaffolding for listener binaries per protocol. Accepts connections
 * and prepares for parser handoff (control-plane wired separately).
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/network/control_plane.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/network/socket_types.h"
#include "scratchbird/server/config_parser.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/version.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
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

struct ListenerConfig {
    std::string protocol = SB_LISTENER_PROTOCOL;
    std::string bind_address = "0.0.0.0";
    uint16_t port = 0;
    std::string control_socket_dir;
    std::string engine_endpoint;
    std::string config_path;
    std::string log_level = "info";
    uint32_t pool_min = 4;
    uint32_t pool_max = 64;
    std::string spawn_strategy = "hybrid";
    uint32_t max_requests = 0;
    uint32_t max_age_seconds = 0;
    uint32_t health_check_interval_ms = 5000;
    bool show_help = false;
    bool show_version = false;
};

void handleSignal(int signal) {
#ifndef _WIN32
    if (signal == SIGUSR2) {
        g_dump_stats.store(true, std::memory_order_release);
        return;
    }
#endif
    g_shutdown.store(true, std::memory_order_release);
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
    return "\\\\.\\pipe\\scratchbird\\" + protocol + "\\listener";
#else
    if (config.control_socket_dir.empty()) {
        return std::string();
    }
    std::string path = config.control_socket_dir;
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    return path + "sb_listener." + config.protocol + ".sock";
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
          queue_wait_histogram_(queue_wait_histogram) {}

    bool start() {
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
            socket->close();
            return;
        }

        if (msg.header.message_type != static_cast<uint16_t>(
                scratchbird::network::ControlPlaneMessageType::HELLO)) {
            socket->close();
            return;
        }

        std::string proto;
        uint32_t pid = 0;
        uint64_t worker_id = 0;
        if (!parseHello(msg.payload, proto, pid, worker_id)) {
            socket->close();
            return;
        }

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
        scratchbird::network::sendControlPlaneMessage(*socket, ack,
                                                      scratchbird::network::INVALID_SOCKET_VALUE,
                                                      0, nullptr);

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

        worker->reader_thread = std::thread([this, worker]() {
            readerLoop(worker);
        });
        worker->reader_thread.detach();

        cv_.notify_all();
    }

    std::shared_ptr<ParserWorker> acquireWorker(std::chrono::milliseconds timeout) {
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
                    return worker;
                }
            }

            if (config_.spawn_strategy != "prefork" && runningCountLocked() < config_.pool_max) {
                spawnWorkerLocked();
            }

            if (timeout.count() == 0) {
                return nullptr;
            }
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
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
        msg.payload = buildHandoffPayload(msg.header.request_id, client_addr, tls_active);
        msg.header.payload_len = msg.payload.size();

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
            markWorkerFault(worker, "error");
            return false;
        }

        std::unique_lock<std::mutex> lock(worker->mutex);
        bool acked = worker->cv.wait_for(lock, std::chrono::seconds(2), [&worker]() {
            return worker->last_ack_ready;
        });

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

        {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->session_active = true;
            worker->session_start = std::chrono::steady_clock::now();
        }
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

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::shared_ptr<ParserWorker>> workers_;
    std::atomic<uint64_t> request_id_{1};
    std::atomic<bool> healthcheck_running_{false};
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
        args.push_back("--log-level");
        args.push_back(config_.log_level);

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
                break;
            }
            auto type = static_cast<scratchbird::network::ControlPlaneMessageType>(
                msg.header.message_type);
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
                                             bool tls_active) const {
        std::vector<uint8_t> payload;
        payload.reserve(8 + 16 + 48 + 2 + 1 + 2 + 64);
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
              << "  --bind <addr>               Bind address\n"
              << "  --port <port>               Listen port\n"
              << "  --control-socket-dir <dir>  Control socket directory\n"
              << "  --engine-endpoint <path>    Engine IPC endpoint\n"
              << "  --pool-min <n>              Minimum parser pool size\n"
              << "  --pool-max <n>              Maximum parser pool size\n"
              << "  --spawn-strategy <mode>     prefork|on_demand|hybrid\n"
              << "  --max-requests <n>          Recycle parser after N sessions\n"
              << "  --max-age-seconds <n>       Recycle parser after seconds\n"
              << "  --health-check-interval-ms <n>  Health check interval (0 disables)\n"
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

    return true;
}

bool applyArgOverrides(int argc, char* argv[], ListenerConfig& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--bind" && i + 1 < argc) {
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
        }
    }
    return true;
}

int runListener(const ListenerConfig& config) {
    using scratchbird::network::AddressFamily;
    using scratchbird::network::NetworkAddress;
    using scratchbird::network::Socket;
    using scratchbird::core::MetricsRegistry;

    if (!scratchbird::network::initNetwork()) {
        std::cerr << "Failed to initialize network subsystem\n";
        return 2;
    }

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

    scratchbird::core::ErrorContext ctx;
    auto server_socket = Socket::create(AddressFamily::IPV4, scratchbird::network::SocketType::STREAM, &ctx);
    if (!server_socket) {
        std::cerr << "Failed to create socket: " << ctx.message << "\n";
        return 2;
    }

    NetworkAddress addr;
    addr.family = AddressFamily::IPV4;
    addr.host = config.bind_address;
    addr.port = config.port;

    if (server_socket->bind(addr, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Bind failed: " << ctx.message << "\n";
        return 2;
    }
    if (server_socket->listen(scratchbird::network::DEFAULT_LISTEN_BACKLOG, &ctx) != scratchbird::core::Status::OK) {
        std::cerr << "Listen failed: " << ctx.message << "\n";
        return 2;
    }

    std::cout << SB_LISTENER_NAME << " listening on "
              << config.bind_address << ":" << config.port << "\n";
    std::cout << "Parser pool: min=" << config.pool_min
              << " max=" << config.pool_max
              << " strategy=" << config.spawn_strategy << "\n";
    if (config.engine_endpoint.empty()) {
        std::cout << "Engine endpoint: (unset)\n";
    } else {
        std::cout << "Engine endpoint: " << config.engine_endpoint << "\n";
    }

    std::string control_socket = controlSocketPath(config);
    scratchbird::network::ControlPlaneServer control_plane;
    std::thread control_thread;
    bool pool_enabled = false;
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

    if (!control_socket.empty()) {
        if (control_plane.start(control_socket, &ctx) == scratchbird::core::Status::OK) {
            std::cout << "Control socket: " << control_socket << "\n";
            pool_enabled = pool.start();
            control_thread = std::thread([&]() {
                scratchbird::core::ErrorContext local_ctx;
                while (!g_shutdown.load(std::memory_order_acquire)) {
                    auto control_conn = control_plane.accept(&local_ctx);
                    if (!control_conn) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        continue;
                    }
                    pool.handleControlConnection(std::move(control_conn));
                }
            });
        } else {
            std::cerr << "Failed to start control socket: " << ctx.message << "\n";
        }
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
        if (g_dump_stats.exchange(false, std::memory_order_acq_rel)) {
            dumpMetrics(metrics);
        }
        NetworkAddress client_addr;
        auto client = server_socket->accept(&client_addr, &ctx);
        if (!client) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        connections_total->inc(1.0, label);
        accept_total->inc(1.0, label);
        open_connections->inc(1.0, label);

        std::cout << "Accepted connection from " << client_addr.host
                  << ":" << client_addr.port << "\n";
        if (!pool_enabled) {
            reject_total->inc(1.0, {config.protocol, SB_LISTENER_NAME, "error"});
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

        bool handed_off = pool.handoff(worker, client->getFd(), client_addr, false);
        client->close();
        open_connections->dec(1.0, label);

        if (!handed_off) {
            reject_total->inc(1.0, {config.protocol, SB_LISTENER_NAME, "error"});
        }
    }

    dumpMetrics(metrics);
    server_socket->close();
    pool.stop();
    control_plane.stop();
    if (control_thread.joinable()) {
        control_thread.join();
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

    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
#ifndef _WIN32
    std::signal(SIGUSR2, handleSignal);
#endif

    return runListener(config);
}
