/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/*
 * ScratchBird Database Engine
 * DDL Tracker Implementation
 * Copyright (c) 2025 ScratchBird Project
 */

#include "scratchbird/git/DDLTracker.h"
#include <sstream>
#include <random>
#include <iomanip>

namespace scratchbird {
namespace git {

//=============================================================================
// Implementation Details
//=============================================================================

struct DDLTracker::Impl {
    std::vector<DDLEvent> events;
    std::map<std::string, DDLEvent> events_by_id;
};

//=============================================================================
// Constructor/Destructor
//=============================================================================

DDLTracker::DDLTracker(core::Catalog& catalog)
    : impl_(std::make_unique<Impl>())
    , catalog_(catalog) {
}

DDLTracker::~DDLTracker() = default;

//=============================================================================
// Initialization
//=============================================================================

bool DDLTracker::initialize() {
    // Create DDL history table
    std::string create_ddl = R"(
CREATE TABLE IF NOT EXISTS SYS$DDL_HISTORY (
    event_id VARCHAR(36) PRIMARY KEY,
    event_time TIMESTAMP NOT NULL,
    user_name VARCHAR(128) NOT NULL,
    session_id VARCHAR(36),
    ddl_type VARCHAR(32) NOT NULL,
    object_type VARCHAR(32) NOT NULL,
    schema_name VARCHAR(128),
    object_name VARCHAR(128),
    ddl_statement TEXT NOT NULL,
    old_definition TEXT,
    new_definition TEXT,
    git_commit VARCHAR(40),
    application_name VARCHAR(128),
    client_ip VARCHAR(45)
);

CREATE INDEX IF NOT EXISTS idx_ddl_history_time ON SYS$DDL_HISTORY(event_time);
CREATE INDEX IF NOT EXISTS idx_ddl_history_object ON SYS$DDL_HISTORY(schema_name, object_name);
)";

    // In production, execute via catalog
    log("INFO", "DDL tracking initialized");
    return true;
}

bool DDLTracker::isInitialized() const {
    // In production, check if table exists
    return true;
}

void DDLTracker::enable() {
    enabled_ = true;
    log("INFO", "DDL tracking enabled");
}

void DDLTracker::disable() {
    enabled_ = false;
    log("INFO", "DDL tracking disabled");
}

bool DDLTracker::isEnabled() const {
    return enabled_;
}

//=============================================================================
// Event Capture
//=============================================================================

std::string DDLTracker::recordEvent(const DDLEvent& event) {
    if (!enabled_) {
        return "";
    }

    DDLEvent evt = event;
    if (evt.event_id.empty()) {
        evt.event_id = generateEventId();
    }
    if (evt.event_time == std::chrono::system_clock::time_point{}) {
        evt.event_time = std::chrono::system_clock::now();
    }

    impl_->events.push_back(evt);
    impl_->events_by_id[evt.event_id] = evt;

    if (event_callback_) {
        event_callback_(evt);
    }

    log("INFO", "Recorded DDL event: " + toString(evt.ddl_type) + " " +
        evt.schema_name + "." + evt.object_name);

    return evt.event_id;
}

std::string DDLTracker::recordDDL(core::Session& session,
                                   DDLType ddl_type,
                                   SchemaObjectType object_type,
                                   const std::string& schema_name,
                                   const std::string& object_name,
                                   const std::string& ddl_statement,
                                   const std::string& old_definition,
                                   const std::string& new_definition) {
    if (!enabled_) {
        return "";
    }

    if (!shouldTrack(schema_name, object_name, object_type)) {
        return "";
    }

    DDLEvent event;
    event.event_id = generateEventId();
    event.event_time = std::chrono::system_clock::now();
    event.user_name = "system";  // In production, get from session
    // event.session_id = session.getId();
    event.ddl_type = ddl_type;
    event.object_type = object_type;
    event.schema_name = schema_name;
    event.object_name = object_name;
    event.ddl_statement = ddl_statement;

    if (!old_definition.empty()) {
        event.old_definition = old_definition;
    }
    if (!new_definition.empty()) {
        event.new_definition = new_definition;
    }

    // event.application_name = session.getApplicationName();
    // event.client_ip = session.getClientIP();

    return recordEvent(event);
}

bool DDLTracker::linkToCommit(const std::string& event_id,
                               const std::string& commit_sha) {
    auto it = impl_->events_by_id.find(event_id);
    if (it == impl_->events_by_id.end()) {
        setError("Event not found: " + event_id);
        return false;
    }

    it->second.git_commit = commit_sha;

    // Update in events vector as well
    for (auto& evt : impl_->events) {
        if (evt.event_id == event_id) {
            evt.git_commit = commit_sha;
            break;
        }
    }

    log("INFO", "Linked event " + event_id + " to commit " + commit_sha);
    return true;
}

//=============================================================================
// Event Query
//=============================================================================

std::vector<DDLEvent> DDLTracker::getUncommittedEvents() {
    std::vector<DDLEvent> uncommitted;

    for (const auto& event : impl_->events) {
        if (!event.git_commit.has_value()) {
            uncommitted.push_back(event);
        }
    }

    return uncommitted;
}

std::vector<DDLEvent> DDLTracker::getEventsForObject(
    const std::string& schema_name,
    const std::string& object_name,
    int limit) {

    std::vector<DDLEvent> results;

    for (auto it = impl_->events.rbegin(); it != impl_->events.rend(); ++it) {
        if (it->schema_name == schema_name && it->object_name == object_name) {
            results.push_back(*it);
            if (limit > 0 && static_cast<int>(results.size()) >= limit) {
                break;
            }
        }
    }

    return results;
}

std::vector<DDLEvent> DDLTracker::getEventsByType(
    DDLType ddl_type,
    std::chrono::system_clock::time_point since,
    int limit) {

    std::vector<DDLEvent> results;

    for (auto it = impl_->events.rbegin(); it != impl_->events.rend(); ++it) {
        if (it->ddl_type == ddl_type) {
            if (since != std::chrono::system_clock::time_point{} &&
                it->event_time < since) {
                continue;
            }
            results.push_back(*it);
            if (limit > 0 && static_cast<int>(results.size()) >= limit) {
                break;
            }
        }
    }

    return results;
}

std::vector<DDLEvent> DDLTracker::getEventsInRange(
    std::chrono::system_clock::time_point from,
    std::chrono::system_clock::time_point to,
    int limit) {

    std::vector<DDLEvent> results;

    for (const auto& event : impl_->events) {
        if (event.event_time >= from && event.event_time <= to) {
            results.push_back(event);
            if (limit > 0 && static_cast<int>(results.size()) >= limit) {
                break;
            }
        }
    }

    return results;
}

std::optional<DDLEvent> DDLTracker::getEvent(const std::string& event_id) {
    auto it = impl_->events_by_id.find(event_id);
    if (it != impl_->events_by_id.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<DDLEvent> DDLTracker::getLatestEvent(const std::string& schema_name,
                                                    const std::string& object_name) {
    for (auto it = impl_->events.rbegin(); it != impl_->events.rend(); ++it) {
        if (it->schema_name == schema_name && it->object_name == object_name) {
            return *it;
        }
    }
    return std::nullopt;
}

int DDLTracker::getUncommittedCount() {
    int count = 0;
    for (const auto& event : impl_->events) {
        if (!event.git_commit.has_value()) {
            count++;
        }
    }
    return count;
}

//=============================================================================
// Event Management
//=============================================================================

bool DDLTracker::deleteEvent(const std::string& event_id) {
    auto it = impl_->events_by_id.find(event_id);
    if (it == impl_->events_by_id.end()) {
        setError("Event not found: " + event_id);
        return false;
    }

    impl_->events_by_id.erase(it);

    // Remove from vector
    impl_->events.erase(
        std::remove_if(impl_->events.begin(), impl_->events.end(),
                      [&event_id](const DDLEvent& e) {
                          return e.event_id == event_id;
                      }),
        impl_->events.end());

    log("INFO", "Deleted event: " + event_id);
    return true;
}

int DDLTracker::discardUncommitted() {
    int count = 0;

    std::vector<std::string> to_delete;
    for (const auto& event : impl_->events) {
        if (!event.git_commit.has_value()) {
            to_delete.push_back(event.event_id);
        }
    }

    for (const auto& id : to_delete) {
        if (deleteEvent(id)) {
            count++;
        }
    }

    log("INFO", "Discarded " + std::to_string(count) + " uncommitted events");
    return count;
}

int DDLTracker::discardEventsForObject(const std::string& schema_name,
                                        const std::string& object_name) {
    int count = 0;

    std::vector<std::string> to_delete;
    for (const auto& event : impl_->events) {
        if (event.schema_name == schema_name && event.object_name == object_name &&
            !event.git_commit.has_value()) {
            to_delete.push_back(event.event_id);
        }
    }

    for (const auto& id : to_delete) {
        if (deleteEvent(id)) {
            count++;
        }
    }

    return count;
}

int DDLTracker::purgeOldEvents(std::chrono::system_clock::time_point older_than) {
    int count = 0;

    std::vector<std::string> to_delete;
    for (const auto& event : impl_->events) {
        if (event.event_time < older_than) {
            to_delete.push_back(event.event_id);
        }
    }

    for (const auto& id : to_delete) {
        if (deleteEvent(id)) {
            count++;
        }
    }

    log("INFO", "Purged " + std::to_string(count) + " old events");
    return count;
}

int DDLTracker::purgeAll() {
    int count = static_cast<int>(impl_->events.size());
    impl_->events.clear();
    impl_->events_by_id.clear();
    log("WARN", "Purged all " + std::to_string(count) + " events");
    return count;
}

//=============================================================================
// Filtering
//=============================================================================

void DDLTracker::setTrackedSchemas(const std::vector<std::string>& schemas) {
    tracked_schemas_ = schemas;
}

void DDLTracker::setExcludedSchemas(const std::vector<std::string>& schemas) {
    excluded_schemas_ = schemas;
}

void DDLTracker::setTrackedTypes(const std::vector<SchemaObjectType>& types) {
    tracked_types_ = types;
}

bool DDLTracker::shouldTrack(const std::string& schema_name,
                              const std::string& object_name,
                              SchemaObjectType type) const {
    // Check include schemas
    if (!tracked_schemas_.empty()) {
        bool found = false;
        for (const auto& s : tracked_schemas_) {
            if (s == schema_name) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check exclude schemas
    for (const auto& s : excluded_schemas_) {
        if (s == schema_name) return false;
    }

    // Check tracked types
    if (!tracked_types_.empty()) {
        bool found = false;
        for (auto t : tracked_types_) {
            if (t == type) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    return true;
}

//=============================================================================
// Callbacks
//=============================================================================

void DDLTracker::setEventCallback(std::function<void(const DDLEvent&)> callback) {
    event_callback_ = callback;
}

void DDLTracker::setLogCallback(LogCallback callback) {
    log_callback_ = callback;
}

//=============================================================================
// Statistics
//=============================================================================

DDLTracker::Statistics DDLTracker::getStatistics() const {
    Statistics stats;
    stats.total_events = static_cast<int>(impl_->events.size());

    auto now = std::chrono::system_clock::now();
    auto today_start = now - std::chrono::hours(24);
    auto week_start = now - std::chrono::hours(24 * 7);

    for (const auto& event : impl_->events) {
        if (!event.git_commit.has_value()) {
            stats.uncommitted_events++;
        }
        if (event.event_time >= today_start) {
            stats.events_today++;
        }
        if (event.event_time >= week_start) {
            stats.events_this_week++;
        }
        stats.events_by_type[event.ddl_type]++;
        stats.events_by_object_type[event.object_type]++;
    }

    return stats;
}

std::string DDLTracker::getLastError() const {
    return last_error_;
}

//=============================================================================
// Private Methods
//=============================================================================

void DDLTracker::setError(const std::string& error) {
    last_error_ = error;
    log("ERROR", error);
}

void DDLTracker::log(const std::string& level, const std::string& message) {
    if (log_callback_) {
        log_callback_(level, "[DDLTracker] " + message);
    }
}

std::string DDLTracker::generateEventId() {
    // Generate UUID-like string
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    const char* hex = "0123456789abcdef";
    std::string uuid;
    uuid.reserve(36);

    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else if (i == 14) {
            uuid += '4';  // Version 4
        } else if (i == 19) {
            uuid += hex[(dis(gen) & 0x3) | 0x8];  // Variant
        } else {
            uuid += hex[dis(gen)];
        }
    }

    return uuid;
}

} // namespace git
} // namespace scratchbird
