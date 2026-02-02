/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/structured_logger.h"
#include "scratchbird/core/logger.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include <openssl/sha.h>

namespace scratchbird {
namespace core {

namespace {

void appendBytes(std::vector<uint8_t>& out, const void* data, size_t len)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + len);
}

void appendUint64(std::vector<uint8_t>& out, uint64_t value)
{
    for (int i = 7; i >= 0; --i)
    {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void appendUint32(std::vector<uint8_t>& out, uint32_t value)
{
    for (int i = 3; i >= 0; --i)
    {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void appendUint16(std::vector<uint8_t>& out, uint16_t value)
{
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void appendUint8(std::vector<uint8_t>& out, uint8_t value)
{
    out.push_back(value);
}

void appendString(std::vector<uint8_t>& out, const std::string& value)
{
    appendUint32(out, static_cast<uint32_t>(value.size()));
    if (!value.empty())
    {
        appendBytes(out, value.data(), value.size());
    }
}

void appendId(std::vector<uint8_t>& out, const ID& id)
{
    appendBytes(out, id.bytes.data(), id.bytes.size());
}

std::string hashToHex(const std::array<uint8_t, 32>& hash)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (uint8_t byte : hash)
    {
        ss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return ss.str();
}

bool matchesQuery(const AuditEvent& event, const AuditQuery& query)
{
    if (query.start_time > 0 && event.timestamp < query.start_time)
    {
        return false;
    }
    if (query.end_time > 0 && event.timestamp > query.end_time)
    {
        return false;
    }
    if (query.user_id.has_value())
    {
        if (std::memcmp(&event.user_id, &query.user_id.value(), sizeof(ID)) != 0)
        {
            return false;
        }
    }
    if (query.session_id.has_value())
    {
        if (std::memcmp(&event.session_id, &query.session_id.value(), sizeof(ID)) != 0)
        {
            return false;
        }
    }
    if (query.authkey_id.has_value())
    {
        if (std::memcmp(&event.authkey_id, &query.authkey_id.value(), sizeof(ID)) != 0)
        {
            return false;
        }
    }
    if (query.username.has_value() && event.username != query.username.value())
    {
        return false;
    }
    if (query.event_type.has_value() && event.event_type != query.event_type.value())
    {
        return false;
    }
    if (!query.object_name.empty() && event.object_name != query.object_name)
    {
        return false;
    }
    if (query.success.has_value() && event.success != query.success.value())
    {
        return false;
    }

    return true;
}

} // namespace

AuditLogger::AuditLogger(CatalogManager* catalog)
    : catalog_(catalog),
      next_event_id_(1)
{
    buffer_.reserve(MAX_BUFFER_SIZE);
    last_hash_.fill(0);

    if (catalog_)
    {
        ErrorContext ctx;
        uint64_t last_event_id = 0;
        std::array<uint8_t, 32> last_hash;
        Status status = catalog_->getAuditLogTail(last_event_id, last_hash, &ctx);
        if (status == Status::OK)
        {
            if (last_event_id >= next_event_id_)
            {
                next_event_id_ = last_event_id + 1;
            }
            last_hash_ = last_hash;
            tail_loaded_ = true;
        }
        else
        {
            LOG_WARNING(GENERAL, "Audit logger failed to load audit log tail: %s",
                        ctx.message.c_str());
        }
    }
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

void AuditLogger::configureSinks(const AuditSinkConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    bool was_catalog_enabled = sink_config_.enable_catalog;
    sink_config_ = config;
    if (!was_catalog_enabled && sink_config_.enable_catalog)
    {
        tail_loaded_ = false;
    }
}

void AuditLogger::addBroadcastSink(const std::function<void(const AuditEvent&)>& sink)
{
    std::lock_guard<std::mutex> lock(mutex_);
    broadcast_sinks_.push_back(sink);
}

Status AuditLogger::logEvent(AuditEvent& event, ErrorContext* ctx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!tail_loaded_ && catalog_ && sink_config_.enable_catalog)
    {
        uint64_t last_event_id = 0;
        std::array<uint8_t, 32> last_hash;
        Status status = catalog_->getAuditLogTail(last_event_id, last_hash, ctx);
        if (status == Status::OK)
        {
            if (last_event_id >= next_event_id_)
            {
                next_event_id_ = last_event_id + 1;
            }
            last_hash_ = last_hash;
            tail_loaded_ = true;
        }
    }

    // Fill in automatic fields
    event.event_id = next_event_id_++;
    event.timestamp = getCurrentTimeMs();

    AuditBufferEntry entry;
    entry.event = event;
    entry.hash_prev = last_hash_;
    entry.hash_curr = computeHash(event, entry.hash_prev);
    last_hash_ = entry.hash_curr;

    // Add to buffer
    buffer_.push_back(entry);

    // Flush if buffer is full (use unlocked version since we hold mutex)
    if ((buffer_.size() - flush_cursor_) >= MAX_BUFFER_SIZE)
    {
        return flushUnlocked(ctx);
    }

    return Status::OK;
}

Status AuditLogger::writeEventToCatalog(const AuditBufferEntry& entry, ErrorContext* ctx)
{
    if (!catalog_ || !sink_config_.enable_catalog)
    {
        // No catalog available - events only in memory
        return Status::OK;
    }

    return catalog_->appendAuditLog(entry.event, entry.hash_prev, entry.hash_curr, ctx);
}

Status AuditLogger::writeEventToFile(const AuditBufferEntry& entry, ErrorContext* ctx)
{
    if (!sink_config_.enable_file)
    {
        return Status::OK;
    }

    if (sink_config_.file_path.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Audit file sink enabled but file path is empty");
        return Status::INVALID_ARGUMENT;
    }

    std::ofstream out(sink_config_.file_path, std::ios::app);
    if (!out.is_open())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open audit log file sink");
        return Status::IO_ERROR;
    }

    const AuditEvent& event = entry.event;
    std::string event_type = getEventTypeName(event.event_type);

    out << "{"
        << "\"event_id\":" << event.event_id << ","
        << "\"timestamp\":" << event.timestamp << ","
        << "\"event_type\":\"" << StructuredLogEntry::escapeJson(event_type) << "\","
        << "\"success\":" << (event.success ? "true" : "false") << ","
        << "\"user_id\":\"" << event.user_id.toString() << "\","
        << "\"role_id\":\"" << event.role_id.toString() << "\","
        << "\"session_id\":\"" << event.session_id.toString() << "\","
        << "\"authkey_id\":\"" << event.authkey_id.toString() << "\","
        << "\"username\":\"" << StructuredLogEntry::escapeJson(event.username) << "\","
        << "\"target_username\":\"" << StructuredLogEntry::escapeJson(event.target_username) << "\","
        << "\"object_type\":\"" << StructuredLogEntry::escapeJson(event.object_type) << "\","
        << "\"object_name\":\"" << StructuredLogEntry::escapeJson(event.object_name) << "\","
        << "\"object_id\":\"" << event.object_id.toString() << "\","
        << "\"details\":\"" << StructuredLogEntry::escapeJson(event.details) << "\","
        << "\"ip_address\":\"" << StructuredLogEntry::escapeJson(event.ip_address) << "\","
        << "\"application_name\":\"" << StructuredLogEntry::escapeJson(event.application_name) << "\","
        << "\"hash_prev\":\"" << hashToHex(entry.hash_prev) << "\","
        << "\"hash_curr\":\"" << hashToHex(entry.hash_curr) << "\""
        << "}\n";

    if (!out.good())
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write audit log file sink");
        return Status::IO_ERROR;
    }

    return Status::OK;
}

void AuditLogger::broadcastEvent(const AuditEvent& event)
{
    if (!sink_config_.enable_broadcast)
    {
        return;
    }

    for (const auto& sink : broadcast_sinks_)
    {
        if (sink)
        {
            try
            {
                sink(event);
            }
            catch (const std::exception& ex)
            {
                LOG_WARNING(GENERAL, "Audit broadcast sink failed: %s", ex.what());
            }
        }
    }
}

std::array<uint8_t, 32> AuditLogger::computeHash(const AuditEvent& event,
                                                 const std::array<uint8_t, 32>& prev_hash) const
{
    std::vector<uint8_t> data;
    data.reserve(256 + event.username.size() + event.target_username.size() +
                 event.object_type.size() + event.object_name.size() +
                 event.details.size() + event.ip_address.size() +
                 event.application_name.size());

    appendBytes(data, prev_hash.data(), prev_hash.size());
    appendUint64(data, event.event_id);
    appendUint64(data, event.timestamp);
    appendUint16(data, static_cast<uint16_t>(event.event_type));
    appendUint8(data, event.success ? 1 : 0);
    appendId(data, event.user_id);
    appendId(data, event.role_id);
    appendId(data, event.target_user_id);
    appendId(data, event.object_id);
    appendId(data, event.session_id);
    appendId(data, event.authkey_id);
    appendString(data, event.username);
    appendString(data, event.target_username);
    appendString(data, event.object_type);
    appendString(data, event.object_name);
    appendString(data, event.details);
    appendString(data, event.ip_address);
    appendString(data, event.application_name);

    std::array<uint8_t, 32> hash{};
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

Status AuditLogger::flushUnlocked(ErrorContext* ctx)
{
    // Internal flush method - caller must hold mutex_
    size_t start_index = flush_cursor_;

    for (size_t i = start_index; i < buffer_.size(); ++i)
    {
        const auto& entry = buffer_[i];
        Status status = writeEventToCatalog(entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        status = writeEventToFile(entry, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        broadcastEvent(entry.event);
    }

    if (sink_config_.keep_in_memory)
    {
        flush_cursor_ = buffer_.size();
    }
    else
    {
        buffer_.clear();
        flush_cursor_ = 0;
    }

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

    std::vector<AuditEvent> matching_events;
    bool use_catalog = sink_config_.enable_catalog && catalog_;

    if (use_catalog)
    {
        AuditQuery catalog_query = query;
        catalog_query.offset = 0;
        catalog_query.limit = std::numeric_limits<uint32_t>::max();

        std::vector<AuditEvent> catalog_events;
        Status status = catalog_->queryAuditLog(catalog_query, catalog_events, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        matching_events.insert(matching_events.end(), catalog_events.begin(),
                               catalog_events.end());
    }

    size_t start_index = use_catalog ? flush_cursor_ : 0;
    for (size_t i = start_index; i < buffer_.size(); ++i)
    {
        const auto& event = buffer_[i].event;
        if (matchesQuery(event, query))
        {
            matching_events.push_back(event);
        }
    }

    if (query.descending)
    {
        std::sort(matching_events.begin(), matching_events.end(),
                  [](const AuditEvent& a, const AuditEvent& b) {
                      if (a.timestamp != b.timestamp)
                      {
                          return a.timestamp > b.timestamp;
                      }
                      return a.event_id > b.event_id;
                  });
    }
    else
    {
        std::sort(matching_events.begin(), matching_events.end(),
                  [](const AuditEvent& a, const AuditEvent& b) {
                      if (a.timestamp != b.timestamp)
                      {
                          return a.timestamp < b.timestamp;
                      }
                      return a.event_id < b.event_id;
                  });
    }

    size_t start_idx = std::min(static_cast<size_t>(query.offset), matching_events.size());
    size_t end_idx = std::min(start_idx + query.limit, matching_events.size());
    events_out.reserve(end_idx - start_idx);
    for (size_t i = start_idx; i < end_idx; ++i)
    {
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
        case AuditEventType::DOMAIN_ACCESS:        return "DOMAIN_ACCESS";

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

        // Job Scheduler
        case AuditEventType::JOB_CREATED:         return "JOB_CREATED";
        case AuditEventType::JOB_MODIFIED:        return "JOB_MODIFIED";
        case AuditEventType::JOB_DELETED:         return "JOB_DELETED";
        case AuditEventType::JOB_EXECUTED:        return "JOB_EXECUTED";
        case AuditEventType::JOB_FAILED:          return "JOB_FAILED";
        case AuditEventType::JOB_CANCELLED:       return "JOB_CANCELLED";

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
