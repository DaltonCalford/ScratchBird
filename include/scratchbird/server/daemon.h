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
 * ScratchBird Daemon Support
 *
 * Alpha 3 Phase 3.3: Service Mode & systemd Integration
 *
 * Provides Unix-style daemonization and systemd integration:
 * - Process forking and session creation
 * - PID file management with locking
 * - Signal handling (SIGTERM, SIGHUP, SIGUSR1, etc.)
 * - systemd notification (sd_notify)
 * - Privilege dropping
 * - File descriptor management
 */

#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <memory>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace server {

// ============================================================================
// Signal Definitions
// ============================================================================

/**
 * Signal types for daemon operations
 */
enum class DaemonSignal : int {
    NONE = 0,
    SHUTDOWN = 1,       // SIGTERM/SIGINT - graceful shutdown
    RELOAD = 2,         // SIGHUP - reload configuration
    ROTATE_LOGS = 3,    // SIGUSR1 - rotate log files
    DUMP_STATS = 4,     // SIGUSR2 - dump statistics
    IMMEDIATE_STOP = 5  // SIGQUIT - immediate shutdown
};

/**
 * Signal handler callback type
 */
using SignalHandler = std::function<void(DaemonSignal)>;

// ============================================================================
// Daemon Options
// ============================================================================

/**
 * Daemon configuration options
 */
struct DaemonOptions {
    // Process identity
    std::string pid_file;               // PID file path (e.g., /var/run/scratchbird/sb_server.pid)
    std::string working_dir = "/";      // Working directory after daemonization
    mode_t umask = 0027;                // File creation mask

    // User/group switching
    std::string run_as_user;            // User to switch to (empty = don't switch)
    std::string run_as_group;           // Group to switch to (empty = don't switch)

    // Standard I/O redirection
    std::string stdout_file;            // stdout redirect (empty = /dev/null)
    std::string stderr_file;            // stderr redirect (empty = /dev/null)
    std::string stdin_file = "/dev/null"; // stdin redirect

    // Behavior
    bool daemonize = true;              // Actually fork and daemonize
    bool close_fds = true;              // Close all file descriptors
    int max_fd = 1024;                  // Maximum FD to close (when close_fds=true)
    bool enable_systemd = true;         // Use systemd notification if available
    bool create_pid_dir = true;         // Create PID file directory if needed

    // Timeouts
    uint32_t shutdown_timeout_sec = 30; // Max time for graceful shutdown
    uint32_t watchdog_interval_sec = 0; // systemd watchdog interval (0 = use systemd default)
};

// ============================================================================
// Daemon State
// ============================================================================

/**
 * Current daemon state
 */
enum class DaemonState : uint8_t {
    INIT = 0,           // Initial state
    STARTING,           // Daemonizing
    RUNNING,            // Running normally
    RELOADING,          // Reloading configuration
    STOPPING,           // Graceful shutdown in progress
    STOPPED             // Stopped
};

/**
 * Convert daemon state to string
 */
const char* daemonStateToString(DaemonState state);

// ============================================================================
// PID File Manager
// ============================================================================

/**
 * PID file manager with file locking
 */
class PIDFile {
public:
    PIDFile();
    ~PIDFile();

    // Disable copy
    PIDFile(const PIDFile&) = delete;
    PIDFile& operator=(const PIDFile&) = delete;

    /**
     * Create and lock PID file
     *
     * @param path PID file path
     * @param create_dir Create parent directory if needed
     * @param ctx Error context
     * @return Status::OK on success, error if file exists and is locked
     */
    core::Status create(const std::string& path, bool create_dir = true,
                        core::ErrorContext* ctx = nullptr);

    /**
     * Remove PID file
     */
    void remove();

    /**
     * Check if PID file exists and is valid (process running)
     *
     * @param path PID file path
     * @param pid Output: PID from file if valid
     * @return true if file exists and process is running
     */
    static bool isLocked(const std::string& path, pid_t* pid = nullptr);

    /**
     * Read PID from file (does not check if running)
     */
    static pid_t read(const std::string& path);

    /**
     * Get our PID file path
     */
    const std::string& path() const { return path_; }

    /**
     * Check if we hold the lock
     */
    bool isHeld() const { return fd_ >= 0; }

private:
    std::string path_;
    int fd_ = -1;
};

// ============================================================================
// systemd Integration
// ============================================================================

/**
 * systemd notification helper
 *
 * Provides integration with systemd's notification protocol via sd_notify().
 * Safe to call even when not running under systemd.
 */
class SystemdNotify {
public:
    /**
     * Check if running under systemd
     */
    static bool isSystemd();

    /**
     * Notify systemd that startup is complete
     */
    static void ready();

    /**
     * Notify systemd that we're reloading configuration
     */
    static void reloading();

    /**
     * Notify systemd that we're stopping
     */
    static void stopping();

    /**
     * Send watchdog keepalive
     */
    static void watchdog();

    /**
     * Set status message
     *
     * @param status Status string (shown in "systemctl status")
     */
    static void status(const std::string& status);

    /**
     * Request extended timeout for an operation
     *
     * @param microseconds Additional time needed
     */
    static void extendTimeout(uint64_t microseconds);

    /**
     * Report main PID (if forked after exec)
     */
    static void mainPid(pid_t pid);

    /**
     * Get watchdog interval from systemd (0 if not set)
     */
    static uint64_t getWatchdogUsec();

private:
    static void notify(const std::string& state);
};

// ============================================================================
// Daemon Class
// ============================================================================

/**
 * Daemon - Process daemonization and lifecycle management
 *
 * Usage:
 *   Daemon daemon(options);
 *   daemon.setSignalHandler([](DaemonSignal sig) { ... });
 *
 *   if (daemon.daemonize() != Status::OK) {
 *       // Handle error
 *   }
 *
 *   // Main loop
 *   while (daemon.isRunning()) {
 *       // Do work
 *       daemon.checkSignals();
 *   }
 *
 *   daemon.cleanup();
 */
class Daemon {
public:
    explicit Daemon(const DaemonOptions& options);
    ~Daemon();

    // Disable copy
    Daemon(const Daemon&) = delete;
    Daemon& operator=(const Daemon&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    /**
     * Daemonize the process
     *
     * If options.daemonize is true:
     * - Forks twice to become a daemon
     * - Creates new session
     * - Changes working directory
     * - Redirects standard I/O
     * - Closes file descriptors
     * - Creates PID file
     * - Drops privileges if configured
     *
     * If options.daemonize is false:
     * - Just creates PID file and sets up signals
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    core::Status daemonize(core::ErrorContext* ctx = nullptr);

    /**
     * Signal handler setup
     *
     * Call this AFTER daemonize() to set up custom signal handling.
     */
    void setSignalHandler(SignalHandler handler);

    /**
     * Process pending signals
     *
     * Call this periodically (e.g., in main loop) to dispatch
     * signals to the registered handler.
     */
    void checkSignals();

    /**
     * Request shutdown
     */
    void requestShutdown();

    /**
     * Cleanup before exit
     */
    void cleanup();

    // ========================================================================
    // State
    // ========================================================================

    /**
     * Get current state
     */
    DaemonState state() const { return state_.load(); }

    /**
     * Check if daemon is running
     */
    bool isRunning() const {
        auto s = state_.load();
        return s == DaemonState::RUNNING || s == DaemonState::RELOADING;
    }

    /**
     * Check if shutdown was requested
     */
    bool shutdownRequested() const { return shutdown_requested_.load(); }

    /**
     * Get pending signal (if any)
     */
    DaemonSignal pendingSignal() const { return pending_signal_.load(); }

    /**
     * Clear pending signal
     */
    void clearPendingSignal() { pending_signal_.store(DaemonSignal::NONE); }

    // ========================================================================
    // systemd
    // ========================================================================

    /**
     * Notify systemd of status change
     */
    void notifyReady();
    void notifyReloading();
    void notifyStopping();
    void notifyStatus(const std::string& status);
    void notifyWatchdog();

    // ========================================================================
    // Utilities
    // ========================================================================

    /**
     * Get the PID file manager
     */
    PIDFile& pidFile() { return pid_file_; }

    /**
     * Get options
     */
    const DaemonOptions& options() const { return options_; }

    /**
     * Check if we're the parent process (pre-fork)
     */
    bool isParent() const { return is_parent_; }

private:
    // Internal setup methods
    core::Status doFork(core::ErrorContext* ctx);
    core::Status setupSession(core::ErrorContext* ctx);
    core::Status redirectIO(core::ErrorContext* ctx);
    core::Status closeFDs(core::ErrorContext* ctx);
    core::Status dropPrivileges(core::ErrorContext* ctx);
    void setupSignals();

    // Static signal handler (routes to instance)
    static void staticSignalHandler(int sig);

    DaemonOptions options_;
    PIDFile pid_file_;

    std::atomic<DaemonState> state_{DaemonState::INIT};
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<DaemonSignal> pending_signal_{DaemonSignal::NONE};

    SignalHandler signal_handler_;
    bool is_parent_ = false;
    bool daemonized_ = false;

    // Global instance for signal routing
    static Daemon* g_instance_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get current process ID
 */
pid_t getCurrentPid();

/**
 * Check if a process is running
 *
 * @param pid Process ID to check
 * @return true if process exists
 */
bool isProcessRunning(pid_t pid);

/**
 * Send signal to a process
 *
 * @param pid Process ID
 * @param signal Signal number
 * @return true if signal was sent successfully
 */
bool sendSignal(pid_t pid, int signal);

/**
 * Get user ID from username
 *
 * @param username Username
 * @param uid Output: User ID
 * @return true if user found
 */
bool getUserId(const std::string& username, uid_t& uid);

/**
 * Get group ID from group name
 *
 * @param groupname Group name
 * @param gid Output: Group ID
 * @return true if group found
 */
bool getGroupId(const std::string& groupname, gid_t& gid);

/**
 * Create directory recursively
 *
 * @param path Directory path
 * @param mode Permissions
 * @return true on success
 */
bool createDirectory(const std::string& path, mode_t mode = 0755);

/**
 * Get default PID file path
 */
std::string getDefaultPidFilePath();

/**
 * Get default run directory
 */
std::string getDefaultRunDirectory();

}  // namespace server
}  // namespace scratchbird
