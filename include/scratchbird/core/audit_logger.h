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

#include <array>
#include <string>
#include <vector>
#include <mutex>
#include <optional>
#include <functional>
#include <cstring>  // for std::memset, std::memcmp
#include "scratchbird/core/types.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace core {

/**
 * Audit Event Types
 *
 * Categorizes security-relevant events for compliance and forensics.
 *
 * P0-3: Security Audit Logging (CWE-778)
 * Compliance: SOC 2, PCI-DSS, HIPAA, GDPR
 */
enum class AuditEventType {
    // Authentication Events (100-199)
    LOGIN_SUCCESS = 100,
    LOGIN_FAILURE = 101,
    LOGOUT = 102,
    PASSWORD_CHANGE = 103,
    PASSWORD_RESET = 104,
    ACCOUNT_LOCKED = 105,
    ACCOUNT_UNLOCKED = 106,

    // Authorization Events (200-299)
    PERMISSION_GRANTED = 200,
    PERMISSION_REVOKED = 201,
    PERMISSION_DENIED = 202,
    ROLE_GRANTED = 203,
    ROLE_REVOKED = 204,

    // User Management (300-399)
    USER_CREATED = 300,
    USER_DELETED = 301,
    USER_MODIFIED = 302,
    USER_ENABLED = 303,
    USER_DISABLED = 304,
    ROLE_CREATED = 305,
    ROLE_DELETED = 306,
    ROLE_MODIFIED = 307,

    // Data Access (400-499)
    RLS_VIOLATION = 400,
    COLUMN_ACCESS_DENIED = 401,
    TABLE_ACCESS_DENIED = 402,
    SCHEMA_ACCESS_DENIED = 403,
    DOMAIN_ACCESS = 404,

    // Privilege Escalation (500-599)
    SUPERUSER_ACCESS = 500,
    SET_ROLE = 501,
    IMPERSONATION = 502,

    // DDL Operations (600-699)
    DDL_CREATE = 600,
    DDL_ALTER = 601,
    DDL_DROP = 602,
    DDL_TRUNCATE = 603,

    // System Events (700-799)
    DATABASE_STARTUP = 700,
    DATABASE_SHUTDOWN = 701,
    BACKUP_STARTED = 702,
    BACKUP_COMPLETED = 703,
    BACKUP_FAILED = 704,
    RESTORE_STARTED = 705,
    RESTORE_COMPLETED = 706,
    RESTORE_FAILED = 707,

    // Security Configuration (800-899)
    SECURITY_POLICY_CHANGED = 800,
    AUDIT_LOG_ACCESSED = 801,
    AUDIT_LOG_MODIFIED = 802,  // Critical - should never happen
    ENCRYPTION_KEY_CHANGED = 803,

    // Job Scheduler (900-999)
    JOB_CREATED = 900,
    JOB_MODIFIED = 901,
    JOB_DELETED = 902,
    JOB_EXECUTED = 903,
    JOB_FAILED = 904,
    JOB_CANCELLED = 905,
};

/**
 * Audit Event Record
 *
 * Complete record of a security-relevant event.
 */
struct AuditEvent {
    uint64_t event_id = 0;              // Sequential ID
    AuditEventType event_type;          // Event type
    ID user_id;                         // User who performed action
    ID role_id;                         // Effective role (if applicable)
    std::string username;               // Username (denormalized for performance)
    ID target_user_id;                  // User affected by action (if applicable)
    std::string target_username;        // Target username
    std::string object_type;            // Object type (TABLE, ROLE, USER, etc.)
    std::string object_name;            // Name of affected object
    ID object_id;                       // Object UUID (if applicable)
    std::string details;                // JSON format for structured data
    bool success;                       // true = action succeeded
    uint64_t timestamp;                 // Unix timestamp (milliseconds)
    std::string ip_address;             // Client IP (Alpha 3+)
    std::string application_name;       // Application name (Alpha 3+)
    ID session_id;                      // Session identifier (UUID)
    ID authkey_id;                      // AuthKey identifier (UUID)

    AuditEvent()
        : event_id(0),
          event_type(AuditEventType::LOGIN_SUCCESS),
          success(false),
          timestamp(0)
    {
        std::memset(&user_id, 0, sizeof(user_id));
        std::memset(&role_id, 0, sizeof(role_id));
        std::memset(&target_user_id, 0, sizeof(target_user_id));
        std::memset(&object_id, 0, sizeof(object_id));
        std::memset(&session_id, 0, sizeof(session_id));
        std::memset(&authkey_id, 0, sizeof(authkey_id));
    }
};

/**
 * Audit Query Parameters
 *
 * Filter criteria for querying audit log.
 */
struct AuditQuery {
    uint64_t start_time = 0;            // Filter by time range (ms)
    uint64_t end_time = 0;              // 0 = no limit
    std::optional<ID> user_id;          // Filter by user
    std::optional<std::string> username; // Filter by username
    std::optional<AuditEventType> event_type; // Filter by event type
    std::optional<ID> session_id;        // Filter by session
    std::optional<ID> authkey_id;        // Filter by AuthKey
    std::string object_name;            // Filter by object name
    std::optional<bool> success;        // Filter by success/failure
    uint32_t limit = 100;               // Max results
    uint32_t offset = 0;                // Pagination offset
    bool descending = true;             // Sort by timestamp descending
};

/**
 * Audit sink configuration
 */
struct AuditSinkConfig {
    bool enable_catalog = true;
    bool enable_file = false;
    bool enable_broadcast = false;
    bool keep_in_memory = false;
    std::string file_path;
};

/**
 * Audit Logger
 *
 * Thread-safe audit logging system for security events.
 *
 * Features:
 * - Sequential event IDs for tamper detection
 * - Asynchronous buffered writes for performance
 * - Catalog integration for persistence
 * - Query API for forensics and compliance
 * - Tamper-resistant design
 *
 * Example Usage:
 * ```cpp
 * AuditLogger logger;
 *
 * AuditEvent event;
 * event.event_type = AuditEventType::LOGIN_SUCCESS;
 * event.user_id = user_id;
 * event.username = "admin";
 * event.success = true;
 *
 * logger.logEvent(event, &ctx);
 * ```
 */
class AuditLogger {
public:
    /**
     * Constructor
     *
     * @param catalog Catalog manager for persistence (optional for testing)
     */
    explicit AuditLogger(class CatalogManager* catalog = nullptr);

    ~AuditLogger();

    /**
     * Log an audit event
     *
     * Thread-safe. Events are buffered and written asynchronously.
     *
     * @param event Event to log (event_id and timestamp filled automatically)
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status logEvent(AuditEvent& event, ErrorContext* ctx);

    /**
     * Query audit log
     *
     * @param query Query parameters
     * @param events_out [out] Matching events
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status queryAuditLog(
        const AuditQuery& query,
        std::vector<AuditEvent>& events_out,
        ErrorContext* ctx);

    /**
     * Get total event count
     *
     * @return Total number of events logged
     */
    uint64_t getTotalEventCount() const;

    /**
     * Flush buffered events to disk
     *
     * Normally happens automatically, but can be called explicitly
     * for critical events or shutdown.
     *
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status flush(ErrorContext* ctx);

    /**
     * Configure audit sinks
     */
    void configureSinks(const AuditSinkConfig& config);

    /**
     * Register a broadcast sink callback
     */
    void addBroadcastSink(const std::function<void(const AuditEvent&)>& sink);

    /**
     * Get event type name (for display/logging)
     */
    static std::string getEventTypeName(AuditEventType type);

    /**
     * Helper: Create login success event
     */
    static AuditEvent createLoginSuccessEvent(
        const ID& user_id,
        const std::string& username);

    /**
     * Helper: Create login failure event
     */
    static AuditEvent createLoginFailureEvent(
        const std::string& username,
        const std::string& reason);

    /**
     * Helper: Create permission denied event
     */
    static AuditEvent createPermissionDeniedEvent(
        const ID& user_id,
        const std::string& username,
        const std::string& object_type,
        const std::string& object_name,
        const std::string& permission);

    /**
     * Helper: Create user created event
     */
    static AuditEvent createUserCreatedEvent(
        const ID& creator_user_id,
        const std::string& creator_username,
        const ID& new_user_id,
        const std::string& new_username);

private:
    struct AuditBufferEntry
    {
        AuditEvent event;
        std::array<uint8_t, 32> hash_prev{};
        std::array<uint8_t, 32> hash_curr{};
    };

    class CatalogManager* catalog_;
    uint64_t next_event_id_;
    mutable std::mutex mutex_;
    AuditSinkConfig sink_config_;
    std::vector<std::function<void(const AuditEvent&)>> broadcast_sinks_;
    std::array<uint8_t, 32> last_hash_{};
    bool tail_loaded_ = false;

    // Buffer for asynchronous writes (future optimization)
    std::vector<AuditBufferEntry> buffer_;
    static constexpr size_t MAX_BUFFER_SIZE = 100;
    size_t flush_cursor_ = 0;

    /**
     * Get current time in milliseconds
     */
    uint64_t getCurrentTimeMs() const;

    /**
     * Write event to catalog (if catalog available)
     */
    Status writeEventToCatalog(const AuditBufferEntry& entry, ErrorContext* ctx);

    /**
     * Write event to file sink (if configured)
     */
    Status writeEventToFile(const AuditBufferEntry& entry, ErrorContext* ctx);

    /**
     * Broadcast event to registered sinks
     */
    void broadcastEvent(const AuditEvent& event);

    /**
     * Compute hash-chain value for event
     */
    std::array<uint8_t, 32> computeHash(const AuditEvent& event,
                                        const std::array<uint8_t, 32>& prev_hash) const;

    /**
     * Flush buffered events (internal, caller must hold mutex_)
     */
    Status flushUnlocked(ErrorContext* ctx);
};

}  // namespace core
}  // namespace scratchbird
