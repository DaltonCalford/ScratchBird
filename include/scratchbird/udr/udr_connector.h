/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */
#pragma once

/**
 * UDR (User Defined Routine) Connector Framework
 * 
 * Section D: Remote Engine UDR Connectors
 * 
 * Provides infrastructure for connecting to remote databases via native
 * wire protocols (PostgreSQL, MySQL, Firebird, ScratchBird).
 */

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/types.h"
#include "scratchbird/udr/connection_pool.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace scratchbird {
namespace udr {

// ============================================================================
// UDR Connector Types
// ============================================================================

enum class ConnectorType : uint8_t {
    POSTGRESQL = 1,
    MYSQL = 2,
    FIREBIRD = 3,
    FIREBIRDSQL = FIREBIRD,  // Canonical alias used by vNext docs
    SCRATCHBIRD = 4,
    ODBC = 5,
    CASSANDRA = 6,
    MILVUS = 7,
    MONGODB = 8,
    NEO4J = 9,
    REDIS = 10,
    MARIADB = 11,
    INFLUXDB = 12,
    CLICKHOUSE = 13,
    OPENSEARCH = 14,
    DUCKDB = 15
};

enum class RemoteObjectType : uint8_t {
    TABLE = 1,
    VIEW = 2,
    PROCEDURE = 3,
    FUNCTION = 4,
    SEQUENCE = 5
};

// ============================================================================
// UDR Configuration
// ============================================================================

struct UDRServerConfig {
    std::string host = "localhost";
    uint16_t port = 0;  // 0 = use default for connector type
    std::string database;
    std::string user;
    std::string password;
    std::string ssl_mode = "prefer";  // disable, prefer, require, verify-ca, verify-full
    std::string ssl_cert;
    std::string ssl_key;
    std::string ssl_root_cert;
    
    // Firebird-specific options
    std::string role;           // SQL role for Firebird
    std::string charset = "UTF8";  // Character set
    
    // Connection pool settings
    uint32_t pool_min_size = 1;
    uint32_t pool_max_size = 10;
    uint32_t pool_max_idle_ms = 300000;  // 5 minutes
    uint32_t pool_connection_timeout_ms = 10000;
    uint32_t pool_health_check_interval_ms = 30000;
    
    // Type mapping
    bool map_arrays = true;
    bool map_json = true;
    bool map_geometric = false;
    
    // Performance
    bool use_prepared_statements = true;
    uint32_t fetch_size = 1000;
    bool prefetch = true;
};

struct UDRMappingConfig {
    // Local to remote user mapping
    std::unordered_map<std::string, std::string> user_map;
    
    // Default remote user for local users not in map
    std::string default_remote_user;
    
    // Whether to use current session user
    bool use_session_user = true;
    
    // Password callback for dynamic password retrieval
    std::function<std::string(const std::string& local_user)> password_callback;
};

// ============================================================================
// Remote Object Metadata
// ============================================================================

struct RemoteColumn {
    std::string name;
    uint32_t type_oid = 0;
    std::string type_name;
    int32_t type_size = -1;
    int32_t type_modifier = -1;
    bool nullable = true;
    std::string default_value;
};

struct RemoteTableInfo {
    std::string remote_schema;
    std::string remote_name;
    std::vector<RemoteColumn> columns;
    std::vector<std::string> primary_key;
    std::vector<std::string> unique_keys;
    bool has_oids = false;
};

struct RemoteProcedureInfo {
    std::string remote_schema;
    std::string remote_name;
    std::vector<RemoteColumn> input_params;
    std::vector<RemoteColumn> output_params;
    bool returns_set = false;
    std::string return_type;
};

// ============================================================================
// Query Result
// ============================================================================

struct RemoteValue {
    std::vector<uint8_t> data;
    bool is_null = false;
    uint32_t type_oid = 0;
    
    std::string toString() const;
    int64_t toInt64() const;
    double toDouble() const;
    bool toBool() const;
};

struct RemoteRow {
    std::vector<RemoteValue> values;
    
    const RemoteValue& getValue(size_t index) const {
        static RemoteValue null_value;
        if (index < values.size()) return values[index];
        return null_value;
    }
};

struct RemoteResultSet {
    std::vector<RemoteColumn> columns;
    std::vector<RemoteRow> rows;
    uint64_t rows_affected = 0;
    std::string command_tag;
    bool has_more = false;
    std::string cursor_name;
    
    void clear() {
        columns.clear();
        rows.clear();
        rows_affected = 0;
        command_tag.clear();
        has_more = false;
        cursor_name.clear();
    }
};

// ============================================================================
// UDR Connector Interface
// ============================================================================

class UDRConnector {
public:
    virtual ~UDRConnector() = default;

    // Non-copyable, non-movable
    UDRConnector(const UDRConnector&) = delete;
    UDRConnector& operator=(const UDRConnector&) = delete;
    UDRConnector(UDRConnector&&) = delete;
    UDRConnector& operator=(UDRConnector&&) = delete;

    // ========================================================================
    // Lifecycle
    // ========================================================================

    virtual core::Status initialize(const UDRServerConfig& config,
                                    core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status shutdown(core::ErrorContext* ctx = nullptr) = 0;
    virtual bool isConnected() const = 0;

    // ========================================================================
    // Connection Management
    // ========================================================================

    virtual core::Status ping(core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status reconnect(core::ErrorContext* ctx = nullptr) = 0;

    // ========================================================================
    // Query Execution
    // ========================================================================

    virtual core::Status executeQuery(const std::string& sql,
                                      RemoteResultSet& result,
                                      core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status executeCommand(const std::string& sql,
                                        uint64_t& rows_affected,
                                        core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status prepareStatement(const std::string& name,
                                          const std::string& sql,
                                          std::vector<uint32_t>& param_types,
                                          core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status executePrepared(const std::string& name,
                                         const std::vector<RemoteValue>& params,
                                         RemoteResultSet& result,
                                         core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status closeStatement(const std::string& name,
                                        core::ErrorContext* ctx = nullptr) = 0;

    // ========================================================================
    // Cursor/Portal Operations
    // ========================================================================

    virtual core::Status declareCursor(const std::string& cursor_name,
                                       const std::string& query,
                                       bool scrollable,
                                       core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status fetchCursor(const std::string& cursor_name,
                                     uint32_t count,
                                     RemoteResultSet& result,
                                     core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status closeCursor(const std::string& cursor_name,
                                     core::ErrorContext* ctx = nullptr) = 0;

    // ========================================================================
    // Transaction Support
    // ========================================================================

    virtual core::Status beginTransaction(core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status commitTransaction(core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status rollbackTransaction(core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status savepoint(const std::string& name,
                                   core::ErrorContext* ctx = nullptr) = 0;
    virtual core::Status rollbackToSavepoint(const std::string& name,
                                            core::ErrorContext* ctx = nullptr) = 0;

    // ========================================================================
    // Schema Introspection
    // ========================================================================

    virtual core::Status getTableInfo(const std::string& schema,
                                      const std::string& table,
                                      RemoteTableInfo& info,
                                      core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status listTables(const std::string& schema,
                                    std::vector<std::string>& tables,
                                    core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status getProcedureInfo(const std::string& schema,
                                          const std::string& procedure,
                                          RemoteProcedureInfo& info,
                                          core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status listProcedures(const std::string& schema,
                                        std::vector<std::string>& procedures,
                                        core::ErrorContext* ctx = nullptr) = 0;

    // ========================================================================
    // COPY/Streaming Support
    // ========================================================================

    virtual core::Status startCopyIn(const std::string& table,
                                     const std::vector<std::string>& columns,
                                     core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status sendCopyData(const uint8_t* data, size_t len,
                                      core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status endCopyIn(uint64_t& rows_inserted,
                                   core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status startCopyOut(const std::string& query,
                                      core::ErrorContext* ctx = nullptr) = 0;
    
    virtual core::Status receiveCopyData(std::vector<uint8_t>& data,
                                         bool& done,
                                         core::ErrorContext* ctx = nullptr) = 0;

    // ========================================================================
    // Connector Information
    // ========================================================================

    virtual ConnectorType getType() const = 0;
    virtual std::string getVersion() const = 0;
    virtual std::string getRemoteVersion() const = 0;
    virtual std::vector<std::string> getSupportedFeatures() const = 0;

protected:
    UDRConnector() = default;
};

// ============================================================================
// UDR Connector Factory
// ============================================================================

class UDRConnectorFactory {
public:
    static std::unique_ptr<UDRConnector> create(ConnectorType type);
    static std::unique_ptr<UDRConnector> create(const std::string& connection_string);
    
    static bool isSupported(ConnectorType type);
    static std::vector<ConnectorType> getSupportedTypes();
    static const char* typeToString(ConnectorType type);
    static ConnectorType stringToType(const std::string& str);
};

// ============================================================================
// sys.remote_* Runtime Binding
// ============================================================================

struct SysRemoteRuntimeBinding {
    ConnectorType connector_type = ConnectorType::POSTGRESQL;
    UDRServerConfig config;
};

// ============================================================================
// sys.remote_* Procedure Implementations
// ============================================================================

core::Status sys_remote_exec(const std::string& server_name,
                             const std::string& sql,
                             uint64_t& rows_affected,
                             core::ErrorContext* ctx = nullptr);

core::Status sys_remote_query(const std::string& server_name,
                              const std::string& sql,
                              RemoteResultSet& result,
                              core::ErrorContext* ctx = nullptr);

core::Status sys_remote_call(const std::string& server_name,
                             const std::string& procedure_name,
                             const std::vector<RemoteValue>& params,
                             RemoteResultSet& result,
                             core::ErrorContext* ctx = nullptr);

core::Status sys_remote_exec_bound(const SysRemoteRuntimeBinding& binding,
                                   const std::string& sql,
                                   uint64_t& rows_affected,
                                   core::ErrorContext* ctx = nullptr);

core::Status sys_remote_query_bound(const SysRemoteRuntimeBinding& binding,
                                    const std::string& sql,
                                    RemoteResultSet& result,
                                    core::ErrorContext* ctx = nullptr);

core::Status sys_remote_call_bound(const SysRemoteRuntimeBinding& binding,
                                   const std::string& procedure_name,
                                   const std::vector<RemoteValue>& params,
                                   RemoteResultSet& result,
                                   core::ErrorContext* ctx = nullptr);

} // namespace udr
} // namespace scratchbird
