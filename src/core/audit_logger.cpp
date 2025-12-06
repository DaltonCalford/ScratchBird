#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/catalog_manager.h"
#include <chrono>
#include <algorithm>

namespace scratchbird {
namespace core {

AuditLogger::AuditLogger(CatalogManager* catalog)
    : catalog_(catalog),
      next_event_id_(1)
{
    buffer_.reserve(MAX_BUFFER_SIZE);
}

AuditLogger::~AuditLogger()
{
    // Flush any remaining events
    ErrorContext ctx;
    flush(&ctx);
}

uint64_t AuditLogger::getCurrentTimeMs() const
{
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

Status AuditLogger::logEvent(AuditEvent& event, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Fill in automatic fields
    event.event_id = next_event_id_++;
    event.timestamp = getCurrentTimeMs();

    // Add to buffer
    buffer_.push_back(event);

    // Flush if buffer is full (use unlocked version since we hold mutex)
    if (buffer_.size() >= MAX_BUFFER_SIZE) {
        return flushUnlocked(ctx);
    }

    return Status::OK;
}

Status AuditLogger::writeEventToCatalog(const AuditEvent& event, ErrorContext* ctx)
{
    if (!catalog_) {
        // No catalog available - events only in memory
        return Status::OK;
    }

    // Phase 4 Enhancement (P0-3 Phase 2): Implement catalog table write
    // For now, just store in memory buffer (events not persisted)
    // Future: Write to sb_audit_log catalog table

    return Status::OK;
}

Status AuditLogger::flushUnlocked(ErrorContext* ctx)
{
    // Internal flush method - caller must hold mutex_
    for (const auto& event : buffer_) {
        Status status = writeEventToCatalog(event, ctx);
        if (status != Status::OK) {
            return status;
        }
    }

    // Keep events in buffer for queries (in-memory for Alpha)
    // In production, this would be cleared after writing to catalog

    return Status::OK;
}

Status AuditLogger::flush(ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return flushUnlocked(ctx);
}

Status AuditLogger::queryAuditLog(
    const AuditQuery& query,
    std::vector<AuditEvent>& events_out,
    ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    events_out.clear();

    // Filter events from buffer (in-memory for Alpha)
    std::vector<AuditEvent> matching_events;

    for (const auto& event : buffer_) {
        // Time range filter
        if (query.start_time > 0 && event.timestamp < query.start_time) {
            continue;
        }
        if (query.end_time > 0 && event.timestamp > query.end_time) {
            continue;
        }

        // User ID filter
        if (query.user_id.has_value()) {
            if (std::memcmp(&event.user_id, &query.user_id.value(), sizeof(ID)) != 0) {
                continue;
            }
        }

        // Username filter
        if (query.username.has_value() && event.username != query.username.value()) {
            continue;
        }

        // Event type filter
        if (query.event_type.has_value() && event.event_type != query.event_type.value()) {
            continue;
        }

        // Object name filter
        if (!query.object_name.empty() && event.object_name != query.object_name) {
            continue;
        }

        // Success filter
        if (query.success.has_value() && event.success != query.success.value()) {
            continue;
        }

        matching_events.push_back(event);
    }

    // Sort by timestamp, with event_id as secondary key for consistent ordering
    // when timestamps are equal (events logged within same millisecond)
    if (query.descending) {
        std::sort(matching_events.begin(), matching_events.end(),
                 [](const AuditEvent& a, const AuditEvent& b) {
                     if (a.timestamp != b.timestamp)
                         return a.timestamp > b.timestamp;
                     return a.event_id > b.event_id;  // Higher ID = more recent
                 });
    } else {
        std::sort(matching_events.begin(), matching_events.end(),
                 [](const AuditEvent& a, const AuditEvent& b) {
                     if (a.timestamp != b.timestamp)
                         return a.timestamp < b.timestamp;
                     return a.event_id < b.event_id;  // Lower ID = older
                 });
    }

    // Apply pagination
    size_t start_idx = std::min(static_cast<size_t>(query.offset), matching_events.size());
    size_t end_idx = std::min(start_idx + query.limit, matching_events.size());

    for (size_t i = start_idx; i < end_idx; i++) {
        events_out.push_back(matching_events[i]);
    }

    return Status::OK;
}

uint64_t AuditLogger::getTotalEventCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return next_event_id_ - 1;
}

std::string AuditLogger::getEventTypeName(AuditEventType type)
{
    switch (type) {
        // Authentication
        case AuditEventType::LOGIN_SUCCESS:        return "LOGIN_SUCCESS";
        case AuditEventType::LOGIN_FAILURE:        return "LOGIN_FAILURE";
        case AuditEventType::LOGOUT:               return "LOGOUT";
        case AuditEventType::PASSWORD_CHANGE:      return "PASSWORD_CHANGE";
        case AuditEventType::PASSWORD_RESET:       return "PASSWORD_RESET";
        case AuditEventType::ACCOUNT_LOCKED:       return "ACCOUNT_LOCKED";
        case AuditEventType::ACCOUNT_UNLOCKED:     return "ACCOUNT_UNLOCKED";

        // Authorization
        case AuditEventType::PERMISSION_GRANTED:   return "PERMISSION_GRANTED";
        case AuditEventType::PERMISSION_REVOKED:   return "PERMISSION_REVOKED";
        case AuditEventType::PERMISSION_DENIED:    return "PERMISSION_DENIED";
        case AuditEventType::ROLE_GRANTED:         return "ROLE_GRANTED";
        case AuditEventType::ROLE_REVOKED:         return "ROLE_REVOKED";

        // User Management
        case AuditEventType::USER_CREATED:         return "USER_CREATED";
        case AuditEventType::USER_DELETED:         return "USER_DELETED";
        case AuditEventType::USER_MODIFIED:        return "USER_MODIFIED";
        case AuditEventType::USER_ENABLED:         return "USER_ENABLED";
        case AuditEventType::USER_DISABLED:        return "USER_DISABLED";
        case AuditEventType::ROLE_CREATED:         return "ROLE_CREATED";
        case AuditEventType::ROLE_DELETED:         return "ROLE_DELETED";
        case AuditEventType::ROLE_MODIFIED:        return "ROLE_MODIFIED";

        // Data Access
        case AuditEventType::RLS_VIOLATION:        return "RLS_VIOLATION";
        case AuditEventType::COLUMN_ACCESS_DENIED: return "COLUMN_ACCESS_DENIED";
        case AuditEventType::TABLE_ACCESS_DENIED:  return "TABLE_ACCESS_DENIED";
        case AuditEventType::SCHEMA_ACCESS_DENIED: return "SCHEMA_ACCESS_DENIED";

        // Privilege Escalation
        case AuditEventType::SUPERUSER_ACCESS:     return "SUPERUSER_ACCESS";
        case AuditEventType::SET_ROLE:             return "SET_ROLE";
        case AuditEventType::IMPERSONATION:        return "IMPERSONATION";

        // DDL Operations
        case AuditEventType::DDL_CREATE:           return "DDL_CREATE";
        case AuditEventType::DDL_ALTER:            return "DDL_ALTER";
        case AuditEventType::DDL_DROP:             return "DDL_DROP";
        case AuditEventType::DDL_TRUNCATE:         return "DDL_TRUNCATE";

        // System Events
        case AuditEventType::DATABASE_STARTUP:     return "DATABASE_STARTUP";
        case AuditEventType::DATABASE_SHUTDOWN:    return "DATABASE_SHUTDOWN";
        case AuditEventType::BACKUP_STARTED:       return "BACKUP_STARTED";
        case AuditEventType::BACKUP_COMPLETED:     return "BACKUP_COMPLETED";
        case AuditEventType::BACKUP_FAILED:        return "BACKUP_FAILED";
        case AuditEventType::RESTORE_STARTED:      return "RESTORE_STARTED";
        case AuditEventType::RESTORE_COMPLETED:    return "RESTORE_COMPLETED";
        case AuditEventType::RESTORE_FAILED:       return "RESTORE_FAILED";

        // Security Configuration
        case AuditEventType::SECURITY_POLICY_CHANGED: return "SECURITY_POLICY_CHANGED";
        case AuditEventType::AUDIT_LOG_ACCESSED:      return "AUDIT_LOG_ACCESSED";
        case AuditEventType::AUDIT_LOG_MODIFIED:      return "AUDIT_LOG_MODIFIED";
        case AuditEventType::ENCRYPTION_KEY_CHANGED:  return "ENCRYPTION_KEY_CHANGED";

        default:                                   return "UNKNOWN";
    }
}

// ===== Helper Functions =====

AuditEvent AuditLogger::createLoginSuccessEvent(
    const ID& user_id,
    const std::string& username)
{
    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_SUCCESS;
    event.user_id = user_id;
    event.username = username;
    event.success = true;
    event.details = "{}";
    return event;
}

AuditEvent AuditLogger::createLoginFailureEvent(
    const std::string& username,
    const std::string& reason)
{
    AuditEvent event;
    event.event_type = AuditEventType::LOGIN_FAILURE;
    event.username = username;
    event.success = false;
    event.details = "{\"reason\":\"" + reason + "\"}";
    return event;
}

AuditEvent AuditLogger::createPermissionDeniedEvent(
    const ID& user_id,
    const std::string& username,
    const std::string& object_type,
    const std::string& object_name,
    const std::string& permission)
{
    AuditEvent event;
    event.event_type = AuditEventType::PERMISSION_DENIED;
    event.user_id = user_id;
    event.username = username;
    event.object_type = object_type;
    event.object_name = object_name;
    event.success = false;
    event.details = "{\"permission\":\"" + permission + "\"}";
    return event;
}

AuditEvent AuditLogger::createUserCreatedEvent(
    const ID& creator_user_id,
    const std::string& creator_username,
    const ID& new_user_id,
    const std::string& new_username)
{
    AuditEvent event;
    event.event_type = AuditEventType::USER_CREATED;
    event.user_id = creator_user_id;
    event.username = creator_username;
    event.target_user_id = new_user_id;
    event.target_username = new_username;
    event.object_type = "USER";
    event.object_name = new_username;
    event.success = true;
    event.details = "{}";
    return event;
}

}  // namespace core
}  // namespace scratchbird
