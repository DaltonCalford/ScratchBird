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
 * ScratchBird Daemon Implementation
 *
 * Daemon/service-process support implementation.
 */

#include "scratchbird/server/daemon.h"
#include "scratchbird/core/storage_lock_provider.h"

#include <fstream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#ifdef ALREADY_EXISTS
#undef ALREADY_EXISTS
#endif
#define getpid _getpid
#else
#include "scratchbird/core/posix_compat.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <dlfcn.h>
#endif

namespace scratchbird {
namespace server {

namespace {

DaemonSignal mapControlSignal(core::ControlSignal signal) {
    switch (signal) {
        case core::ControlSignal::SHUTDOWN:
            return DaemonSignal::SHUTDOWN;
        case core::ControlSignal::RELOAD:
            return DaemonSignal::RELOAD;
        case core::ControlSignal::ROTATE_LOGS:
            return DaemonSignal::ROTATE_LOGS;
        case core::ControlSignal::DUMP_STATS:
            return DaemonSignal::DUMP_STATS;
        case core::ControlSignal::IMMEDIATE_STOP:
            return DaemonSignal::IMMEDIATE_STOP;
        case core::ControlSignal::NONE:
        default:
            return DaemonSignal::NONE;
    }
}

}  // namespace

// ============================================================================
// Daemon State String Conversion
// ============================================================================

const char* daemonStateToString(DaemonState state) {
    switch (state) {
        case DaemonState::INIT: return "INIT";
        case DaemonState::STARTING: return "STARTING";
        case DaemonState::RUNNING: return "RUNNING";
        case DaemonState::RELOADING: return "RELOADING";
        case DaemonState::STOPPING: return "STOPPING";
        case DaemonState::STOPPED: return "STOPPED";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// PIDFile Implementation
// ============================================================================

PIDFile::PIDFile() = default;

PIDFile::~PIDFile() {
    remove();
}

core::Status PIDFile::create(const std::string& path, bool create_dir,
                             core::ErrorContext* ctx) {
#ifdef _WIN32
    // Windows: Simple file-based locking
    path_ = path;

    // Create directory if needed
    if (create_dir) {
        std::filesystem::path p(path);
        std::filesystem::create_directories(p.parent_path());
    }

    // Check if already locked
    ProcessId existing_pid = 0;
    if (isLocked(path, &existing_pid)) {
        if (ctx) {
            std::string msg = "Server already running (PID " + std::to_string(existing_pid) + ")";
            SET_ERROR_CONTEXT(ctx, core::Status::FILE_EXISTS, msg.c_str());
        }
        return core::Status::FILE_EXISTS;
    }

    // Write PID file
    std::ofstream file(path);
    if (!file) {
        if (ctx) {
            std::string msg = "Failed to create PID file: " + path;
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        return core::Status::IO_ERROR;
    }

    file << getCurrentPid() << std::endl;
    file.close();
    fd_ = 0;  // Mark as held

    return core::Status::OK;
#else
    path_ = path;

    // Create directory if needed
    if (create_dir) {
        std::filesystem::path p(path);
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) {
            if (ctx) {
                std::string msg = "Failed to create PID directory: " + ec.message();
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
            }
            return core::Status::IO_ERROR;
        }
    }

    // Open or create PID file
    fd_ = open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) {
        if (ctx) {
            std::string msg = "Failed to open PID file: " + std::string(strerror(errno));
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        return core::Status::IO_ERROR;
    }

    // Try to acquire exclusive lock
    int lock_errno = 0;
    const core::StorageLockResult lock_result =
        core::getStorageLockProvider().tryLockExclusive(fd_, &lock_errno);
    if (lock_result != core::StorageLockResult::LOCKED) {
        if (lock_result == core::StorageLockResult::LOCK_CONFLICT) {
            // Already locked - read existing PID
            char buf[32];
            ssize_t n = ::read(fd_, buf, sizeof(buf) - 1);
            close(fd_);
            fd_ = -1;

            ProcessId existing_pid = 0;
            if (n > 0) {
                buf[n] = '\0';
                existing_pid = std::stoi(buf);
            }

            if (ctx) {
                std::string msg = "Server already running (PID " + std::to_string(existing_pid) + ")";
                SET_ERROR_CONTEXT(ctx, core::Status::FILE_EXISTS, msg.c_str());
            }
            return core::Status::FILE_EXISTS;
        }

        if (ctx) {
            std::string msg = "Failed to lock PID file: " +
                              std::string(strerror(lock_errno != 0 ? lock_errno : errno));
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        close(fd_);
        fd_ = -1;
        return core::Status::IO_ERROR;
    }

    // Truncate and write PID
    if (ftruncate(fd_, 0) < 0) {
        if (ctx) {
            std::string msg = "Failed to truncate PID file: " + std::string(strerror(errno));
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        close(fd_);
        fd_ = -1;
        return core::Status::IO_ERROR;
    }

    std::string pid_str = std::to_string(getCurrentPid()) + "\n";
    if (write(fd_, pid_str.c_str(), pid_str.size()) != static_cast<ssize_t>(pid_str.size())) {
        if (ctx) {
            std::string msg = "Failed to write PID file: " + std::string(strerror(errno));
            SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, msg.c_str());
        }
        close(fd_);
        fd_ = -1;
        return core::Status::IO_ERROR;
    }

    // Keep file open (holds lock)
    return core::Status::OK;
#endif
}

void PIDFile::remove() {
#ifndef _WIN32
    if (fd_ >= 0) {
        int lock_errno = 0;
        (void)core::getStorageLockProvider().unlock(fd_, &lock_errno);
        close(fd_);
        fd_ = -1;
    }
#else
    fd_ = -1;
#endif

    if (!path_.empty()) {
        std::filesystem::remove(path_);
        path_.clear();
    }
}

bool PIDFile::isLocked(const std::string& path, ProcessId* pid) {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    // Read PID from file
    ProcessId file_pid = read(path);
    if (file_pid <= 0) {
        return false;
    }

    // Check if process is running
    if (!isProcessRunning(file_pid)) {
        return false;
    }

    if (pid) {
        *pid = file_pid;
    }
    return true;
}

ProcessId PIDFile::read(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return 0;
    }

    ProcessId pid = 0;
    file >> pid;
    return pid;
}

// ============================================================================
// systemd Integration
// ============================================================================

// Function pointer types for sd_notify
#ifndef _WIN32
typedef int (*sd_notify_func)(int unset_environment, const char* state);
typedef int (*sd_watchdog_enabled_func)(int unset_environment, uint64_t* usec);

static sd_notify_func get_sd_notify() {
    static sd_notify_func func = nullptr;
    static bool tried = false;

    if (!tried) {
        tried = true;
        void* handle = dlopen("libsystemd.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            func = reinterpret_cast<sd_notify_func>(dlsym(handle, "sd_notify"));
        }
    }
    return func;
}

static sd_watchdog_enabled_func get_sd_watchdog_enabled() {
    static sd_watchdog_enabled_func func = nullptr;
    static bool tried = false;

    if (!tried) {
        tried = true;
        void* handle = dlopen("libsystemd.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (handle) {
            func = reinterpret_cast<sd_watchdog_enabled_func>(dlsym(handle, "sd_watchdog_enabled"));
        }
    }
    return func;
}
#endif

bool SystemdNotify::isSystemd() {
#ifdef _WIN32
    return false;
#else
    return std::getenv("NOTIFY_SOCKET") != nullptr;
#endif
}

void SystemdNotify::notify(const std::string& state) {
#ifndef _WIN32
    auto func = get_sd_notify();
    if (func && isSystemd()) {
        func(0, state.c_str());
    }
#endif
}

void SystemdNotify::ready() {
    notify("READY=1");
}

void SystemdNotify::reloading() {
    notify("RELOADING=1");
}

void SystemdNotify::stopping() {
    notify("STOPPING=1");
}

void SystemdNotify::watchdog() {
    notify("WATCHDOG=1");
}

void SystemdNotify::status(const std::string& status) {
    notify("STATUS=" + status);
}

void SystemdNotify::extendTimeout(uint64_t microseconds) {
    notify("EXTEND_TIMEOUT_USEC=" + std::to_string(microseconds));
}

void SystemdNotify::mainPid(ProcessId pid) {
    notify("MAINPID=" + std::to_string(pid));
}

uint64_t SystemdNotify::getWatchdogUsec() {
#ifdef _WIN32
    return 0;
#else
    auto func = get_sd_watchdog_enabled();
    if (func) {
        uint64_t usec = 0;
        if (func(0, &usec) > 0) {
            return usec;
        }
    }
    return 0;
#endif
}

// ============================================================================
// Daemon Implementation
// ============================================================================

Daemon::Daemon(const DaemonOptions& options)
    : options_(options),
      process_control_(core::createDefaultProcessControl()),
      signal_control_(core::createDefaultSignalControl()) {}

Daemon::~Daemon() {
    cleanup();
}

core::Status Daemon::daemonize(core::ErrorContext* ctx) {
    state_ = DaemonState::STARTING;

#ifdef _WIN32
    // Windows doesn't support Unix-style daemonization
    // Just create PID file and continue
    core::Status status = pid_file_.create(options_.pid_file, options_.create_pid_dir, ctx);
    if (status != core::Status::OK) {
        state_ = DaemonState::STOPPED;
        return status;
    }

    setupSignals();
    daemonized_ = true;
    state_ = DaemonState::RUNNING;
    return core::Status::OK;
#else
    // Create PID file first to check for existing instance
    // (We'll update it after forking)
    if (!options_.pid_file.empty()) {
        ProcessId existing_pid = 0;
        if (PIDFile::isLocked(options_.pid_file, &existing_pid)) {
            if (ctx) {
                std::string msg = "Server already running (PID " + std::to_string(existing_pid) + ")";
                SET_ERROR_CONTEXT(ctx, core::Status::FILE_EXISTS, msg.c_str());
            }
            state_ = DaemonState::STOPPED;
            return core::Status::FILE_EXISTS;
        }
    }

    if (options_.daemonize) {
        // First fork
        core::Status status = doFork(ctx);
        if (status != core::Status::OK) {
            state_ = DaemonState::STOPPED;
            return status;
        }

        // Parent exits here
        if (is_parent_) {
            // Wait briefly for child to set up
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            _exit(0);
        }

        // Create new session
        status = setupSession(ctx);
        if (status != core::Status::OK) {
            state_ = DaemonState::STOPPED;
            return status;
        }

        // Second fork (prevents acquiring controlling terminal)
        status = doFork(ctx);
        if (status != core::Status::OK) {
            state_ = DaemonState::STOPPED;
            return status;
        }

        if (is_parent_) {
            _exit(0);
        }

        // Close file descriptors
        if (options_.close_fds) {
            status = closeFDs(ctx);
            if (status != core::Status::OK) {
                // Non-fatal, continue
            }
        }

        // Redirect standard I/O
        status = redirectIO(ctx);
        if (status != core::Status::OK) {
            state_ = DaemonState::STOPPED;
            return status;
        }

        // Change working directory
        if (!options_.working_dir.empty()) {
            if (chdir(options_.working_dir.c_str()) < 0) {
                // Non-fatal warning
            }
        }

        // Set umask
        umask(static_cast<mode_t>(options_.umask & 0777U));
    }

    // Create PID file (now with actual daemon PID)
    if (!options_.pid_file.empty()) {
        core::Status status = pid_file_.create(options_.pid_file, options_.create_pid_dir, ctx);
        if (status != core::Status::OK) {
            state_ = DaemonState::STOPPED;
            return status;
        }
    }

    // Drop privileges
    if (!options_.run_as_user.empty() || !options_.run_as_group.empty()) {
        core::Status status = dropPrivileges(ctx);
        if (status != core::Status::OK) {
            state_ = DaemonState::STOPPED;
            return status;
        }
    }

    // Set up signal handlers
    setupSignals();

    daemonized_ = true;
    state_ = DaemonState::RUNNING;

    return core::Status::OK;
#endif
}

#ifndef _WIN32
core::Status Daemon::doFork(core::ErrorContext* ctx) {
    if (!process_control_) {
        process_control_ = core::createDefaultProcessControl();
    }
    uint64_t child_pid = 0;
    core::Status status = process_control_->forkSelf(&is_parent_, &child_pid, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    return core::Status::OK;
}

core::Status Daemon::setupSession(core::ErrorContext* ctx) {
    // Create new session
    if (setsid() < 0) {
        if (ctx) {
            std::string msg = "setsid failed: " + std::string(strerror(errno));
            SET_ERROR_CONTEXT(ctx, core::Status::INTERNAL_ERROR, msg.c_str());
        }
        return core::Status::INTERNAL_ERROR;
    }

    return core::Status::OK;
}

core::Status Daemon::redirectIO(core::ErrorContext* ctx) {
    // Redirect stdin
    int fd_in = open(options_.stdin_file.c_str(), O_RDONLY);
    if (fd_in >= 0) {
        dup2(fd_in, STDIN_FILENO);
        if (fd_in > STDERR_FILENO) close(fd_in);
    }

    // Redirect stdout
    std::string stdout_path = options_.stdout_file.empty() ? "/dev/null" : options_.stdout_file;
    int fd_out = open(stdout_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_out >= 0) {
        dup2(fd_out, STDOUT_FILENO);
        if (fd_out > STDERR_FILENO) close(fd_out);
    }

    // Redirect stderr
    std::string stderr_path = options_.stderr_file.empty() ? "/dev/null" : options_.stderr_file;
    int fd_err = open(stderr_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_err >= 0) {
        dup2(fd_err, STDERR_FILENO);
        if (fd_err > STDERR_FILENO) close(fd_err);
    }

    return core::Status::OK;
}

core::Status Daemon::closeFDs(core::ErrorContext* ctx) {
    // Close all file descriptors except stdin/stdout/stderr and PID file
    int max_fd = options_.max_fd;
    if (max_fd <= 0) {
        max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd < 0) max_fd = 1024;
    }

    for (int fd = STDERR_FILENO + 1; fd < max_fd; fd++) {
        // Don't close PID file descriptor
        if (fd != pid_file_.isHeld()) {
            close(fd);
        }
    }

    return core::Status::OK;
}

core::Status Daemon::dropPrivileges(core::ErrorContext* ctx) {
    // Must drop group first, then user

    // Drop supplementary groups
    if (!options_.run_as_user.empty()) {
        setgroups(0, nullptr);
    }

    // Change group
    if (!options_.run_as_group.empty()) {
        GroupId gid = 0;
        if (!getGroupId(options_.run_as_group, gid)) {
            if (ctx) {
                std::string msg = "Group not found: " + options_.run_as_group;
                SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, msg.c_str());
            }
            return core::Status::NOT_FOUND;
        }

        if (setgid(static_cast<gid_t>(gid)) < 0) {
            if (ctx) {
                std::string msg = "Failed to set GID: " + std::string(strerror(errno));
                SET_ERROR_CONTEXT(ctx, core::Status::PERMISSION_DENIED, msg.c_str());
            }
            return core::Status::PERMISSION_DENIED;
        }
    }

    // Change user
    if (!options_.run_as_user.empty()) {
        UserId uid = 0;
        if (!getUserId(options_.run_as_user, uid)) {
            if (ctx) {
                std::string msg = "User not found: " + options_.run_as_user;
                SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, msg.c_str());
            }
            return core::Status::NOT_FOUND;
        }

        if (setuid(static_cast<uid_t>(uid)) < 0) {
            if (ctx) {
                std::string msg = "Failed to set UID: " + std::string(strerror(errno));
                SET_ERROR_CONTEXT(ctx, core::Status::PERMISSION_DENIED, msg.c_str());
            }
            return core::Status::PERMISSION_DENIED;
        }
    }

    return core::Status::OK;
}

#else
// Windows stubs
core::Status Daemon::doFork(core::ErrorContext* ctx) { return core::Status::OK; }
core::Status Daemon::setupSession(core::ErrorContext* ctx) { return core::Status::OK; }
core::Status Daemon::redirectIO(core::ErrorContext* ctx) { return core::Status::OK; }
core::Status Daemon::closeFDs(core::ErrorContext* ctx) { return core::Status::OK; }
core::Status Daemon::dropPrivileges(core::ErrorContext* ctx) { return core::Status::OK; }
#endif

void Daemon::setupSignals() {
    if (!signal_control_) {
        signal_control_ = core::createDefaultSignalControl();
    }
    if (!signal_control_) {
        return;
    }

    core::SignalInstallSpec spec;
    spec.enable_shutdown_signal = true;
    spec.enable_reload_signal = true;
    spec.enable_rotate_logs_signal = true;
    spec.enable_dump_stats_signal = true;
    spec.enable_immediate_stop_signal = true;
    spec.ignore_broken_pipe = true;

    (void)signal_control_->install(spec, nullptr);
}

void Daemon::setSignalHandler(SignalHandler handler) {
    signal_handler_ = std::move(handler);
}

void Daemon::checkSignals() {
    if (signal_control_) {
        core::ControlSignal control_signal = core::ControlSignal::NONE;
        if (signal_control_->poll(&control_signal, nullptr) == core::Status::OK) {
            DaemonSignal signal = mapControlSignal(control_signal);
            if (signal != DaemonSignal::NONE) {
                pending_signal_.store(signal);
                if (signal == DaemonSignal::SHUTDOWN || signal == DaemonSignal::IMMEDIATE_STOP) {
                    shutdown_requested_.store(true);
                    state_.store(DaemonState::STOPPING);
                } else if (signal == DaemonSignal::RELOAD) {
                    state_.store(DaemonState::RELOADING);
                }
            }
        }
    }

    DaemonSignal sig = pending_signal_.exchange(DaemonSignal::NONE);
    if (sig != DaemonSignal::NONE && signal_handler_) {
        signal_handler_(sig);

        // After reload, go back to running state
        if (sig == DaemonSignal::RELOAD) {
            state_.store(DaemonState::RUNNING);
        }
    }
}

void Daemon::requestShutdown() {
    shutdown_requested_.store(true);
    state_.store(DaemonState::STOPPING);
}

void Daemon::cleanup() {
    if (signal_control_) {
        (void)signal_control_->uninstall(nullptr);
    }

    pid_file_.remove();
    state_.store(DaemonState::STOPPED);
}

void Daemon::notifyReady() {
    if (options_.enable_systemd) {
        SystemdNotify::ready();
    }
}

void Daemon::notifyReloading() {
    if (options_.enable_systemd) {
        SystemdNotify::reloading();
    }
}

void Daemon::notifyStopping() {
    if (options_.enable_systemd) {
        SystemdNotify::stopping();
    }
}

void Daemon::notifyStatus(const std::string& status) {
    if (options_.enable_systemd) {
        SystemdNotify::status(status);
    }
}

void Daemon::notifyWatchdog() {
    if (options_.enable_systemd) {
        SystemdNotify::watchdog();
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

ProcessId getCurrentPid() {
    return static_cast<ProcessId>(getpid());
}

bool isProcessRunning(ProcessId pid) {
    if (pid <= 0) return false;

#ifdef _WIN32
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                 FALSE,
                                 static_cast<DWORD>(pid));
    if (process) {
        DWORD exit_code;
        bool running = GetExitCodeProcess(process, &exit_code) && exit_code == STILL_ACTIVE;
        CloseHandle(process);
        return running;
    }
    return false;
#else
    // kill with signal 0 checks if process exists
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

bool sendSignal(ProcessId pid, int signal) {
#ifdef _WIN32
    return false;  // Not supported
#else
    return kill(static_cast<pid_t>(pid), signal) == 0;
#endif
}

bool getUserId(const std::string& username, UserId& uid) {
#ifdef _WIN32
    return false;
#else
    struct passwd* pw = getpwnam(username.c_str());
    if (pw) {
        uid = static_cast<UserId>(pw->pw_uid);
        return true;
    }
    return false;
#endif
}

bool getGroupId(const std::string& groupname, GroupId& gid) {
#ifdef _WIN32
    return false;
#else
    struct group* gr = getgrnam(groupname.c_str());
    if (gr) {
        gid = static_cast<GroupId>(gr->gr_gid);
        return true;
    }
    return false;
#endif
}

bool createDirectory(const std::string& path, FileModeBits mode) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) return false;

#ifndef _WIN32
    chmod(path.c_str(), static_cast<mode_t>(mode & 0777U));
#endif
    return true;
}

std::string getDefaultPidFilePath() {
#ifdef _WIN32
    return "";
#else
    return "/var/run/scratchbird/sb_server.pid";
#endif
}

std::string getDefaultRunDirectory() {
#ifdef _WIN32
    return "";
#else
    return "/var/run/scratchbird";
#endif
}

}  // namespace server
}  // namespace scratchbird
