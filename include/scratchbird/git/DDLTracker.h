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
 * DDL Change Tracker - Automatic DDL Event Capture
 * Copyright (c) 2025 ScratchBird Project
 */
#pragma once

#include "GitTypes.h"
#include <memory>
#include <functional>

namespace scratchbird {

// Forward declarations
namespace core {
class Catalog;
class Database;
class Session;
}

namespace git {

/**
 * DDLTracker automatically captures DDL changes for Git integration.
 *
 * Features:
 * - Hook into DDL execution to capture all schema changes
 * - Store events in SYS$DDL_HISTORY table
 * - Track before/after definitions
 * - Support filtering by object type or schema
 * - Link events to Git commits
 */
class DDLTracker {
public:
    /**
     * Constructor
     * @param catalog Database catalog
     */
    explicit DDLTracker(core::Catalog& catalog);
    ~DDLTracker();

    // Non-copyable
    DDLTracker(const DDLTracker&) = delete;
    DDLTracker& operator=(const DDLTracker&) = delete;

    //=========================================================================
    // Initialization
    //=========================================================================

    /**
     * Initialize DDL tracking (create SYS$DDL_HISTORY table)
     * @return true on success
     */
    bool initialize();

    /**
     * Check if DDL tracking is initialized
     */
    bool isInitialized() const;

    /**
     * Enable DDL tracking
     */
    void enable();

    /**
     * Disable DDL tracking
     */
    void disable();

    /**
     * Check if DDL tracking is enabled
     */
    bool isEnabled() const;

    //=========================================================================
    // Event Capture
    //=========================================================================

    /**
     * Record a DDL event
     * @param event Event to record
     * @return Event ID
     */
    std::string recordEvent(const DDLEvent& event);

    /**
     * Record DDL execution
     * @param session Session executing the DDL
     * @param ddl_type Type of DDL operation
     * @param object_type Type of object affected
     * @param schema_name Schema name
     * @param object_name Object name
     * @param ddl_statement Full DDL statement
     * @param old_definition Previous definition (for ALTER/DROP)
     * @param new_definition New definition (for CREATE/ALTER)
     * @return Event ID
     */
    std::string recordDDL(core::Session& session,
                          DDLType ddl_type,
                          SchemaObjectType object_type,
                          const std::string& schema_name,
                          const std::string& object_name,
                          const std::string& ddl_statement,
                          const std::string& old_definition = "",
                          const std::string& new_definition = "");

    /**
     * Link event to Git commit
     * @param event_id Event ID
     * @param commit_sha Git commit SHA
     * @return true on success
     */
    bool linkToCommit(const std::string& event_id,
                      const std::string& commit_sha);

    //=========================================================================
    // Event Query
    //=========================================================================

    /**
     * Get uncommitted DDL events
     * @return List of events not linked to a Git commit
     */
    std::vector<DDLEvent> getUncommittedEvents();

    /**
     * Get events for a specific object
     * @param schema_name Schema name
     * @param object_name Object name
     * @param limit Maximum events to return (0 = all)
     * @return List of events
     */
    std::vector<DDLEvent> getEventsForObject(const std::string& schema_name,
                                              const std::string& object_name,
                                              int limit = 0);

    /**
     * Get events by type
     * @param ddl_type DDL operation type
     * @param since Only events after this time
     * @param limit Maximum events to return (0 = all)
     * @return List of events
     */
    std::vector<DDLEvent> getEventsByType(DDLType ddl_type,
                                           std::chrono::system_clock::time_point since = {},
                                           int limit = 0);

    /**
     * Get events in time range
     * @param from Start time
     * @param to End time
     * @param limit Maximum events to return (0 = all)
     * @return List of events
     */
    std::vector<DDLEvent> getEventsInRange(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to,
        int limit = 0);

    /**
     * Get event by ID
     * @param event_id Event ID
     * @return Event or nullopt
     */
    std::optional<DDLEvent> getEvent(const std::string& event_id);

    /**
     * Get latest event for an object
     * @param schema_name Schema name
     * @param object_name Object name
     * @return Latest event or nullopt
     */
    std::optional<DDLEvent> getLatestEvent(const std::string& schema_name,
                                            const std::string& object_name);

    /**
     * Count uncommitted events
     * @return Number of uncommitted events
     */
    int getUncommittedCount();

    //=========================================================================
    // Event Management
    //=========================================================================

    /**
     * Delete an event
     * @param event_id Event ID
     * @return true on success
     */
    bool deleteEvent(const std::string& event_id);

    /**
     * Discard uncommitted events
     * @return Number of events discarded
     */
    int discardUncommitted();

    /**
     * Discard events for a specific object
     * @param schema_name Schema name
     * @param object_name Object name
     * @return Number of events discarded
     */
    int discardEventsForObject(const std::string& schema_name,
                                const std::string& object_name);

    /**
     * Purge old events
     * @param older_than Delete events older than this time
     * @return Number of events purged
     */
    int purgeOldEvents(std::chrono::system_clock::time_point older_than);

    /**
     * Purge all events (admin only)
     * @return Number of events purged
     */
    int purgeAll();

    //=========================================================================
    // Filtering
    //=========================================================================

    /**
     * Set schemas to track (empty = all)
     */
    void setTrackedSchemas(const std::vector<std::string>& schemas);

    /**
     * Set schemas to exclude from tracking
     */
    void setExcludedSchemas(const std::vector<std::string>& schemas);

    /**
     * Set object types to track (empty = all)
     */
    void setTrackedTypes(const std::vector<SchemaObjectType>& types);

    /**
     * Check if an object should be tracked
     * @param schema_name Schema name
     * @param object_name Object name
     * @param type Object type
     * @return true if should be tracked
     */
    bool shouldTrack(const std::string& schema_name,
                     const std::string& object_name,
                     SchemaObjectType type) const;

    //=========================================================================
    // Callbacks
    //=========================================================================

    /**
     * Set callback for DDL events
     * @param callback Function called when DDL is captured
     */
    void setEventCallback(std::function<void(const DDLEvent&)> callback);

    /**
     * Set log callback
     */
    void setLogCallback(LogCallback callback);

    //=========================================================================
    // Statistics
    //=========================================================================

    /**
     * Get tracking statistics
     */
    struct Statistics {
        int total_events;
        int uncommitted_events;
        int events_today;
        int events_this_week;
        std::map<DDLType, int> events_by_type;
        std::map<SchemaObjectType, int> events_by_object_type;
    };

    Statistics getStatistics() const;

    /**
     * Get last error message
     */
    std::string getLastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    core::Catalog& catalog_;
    bool enabled_ = false;
    std::vector<std::string> tracked_schemas_;
    std::vector<std::string> excluded_schemas_;
    std::vector<SchemaObjectType> tracked_types_;
    std::function<void(const DDLEvent&)> event_callback_;
    LogCallback log_callback_;
    std::string last_error_;

    void setError(const std::string& error);
    void log(const std::string& level, const std::string& message);
    std::string generateEventId();
};

} // namespace git
} // namespace scratchbird
