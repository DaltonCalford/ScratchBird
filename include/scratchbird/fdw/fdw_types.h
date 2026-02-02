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
 * @file fdw_types.h
 * @brief Foreign Data Wrapper Core Types and Interfaces
 *
 * This file defines the core types, enumerations, and structures for the
 * Foreign Data Wrapper (FDW) system that enables ScratchBird to connect
 * to remote databases (PostgreSQL, MySQL, Firebird, etc.)
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#ifndef SCRATCHBIRD_FDW_TYPES_H
#define SCRATCHBIRD_FDW_TYPES_H

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace fdw {

// =============================================================================
// Enumerations
// =============================================================================

/**
 * @brief Identifies the type of remote database
 */
enum class RemoteDatabaseType : uint8_t {
    POSTGRESQL = 1,    // PostgreSQL 9.6+
    MYSQL      = 2,    // MySQL 5.7+, MariaDB 10+
    MSSQL      = 3,    // Microsoft SQL Server 2016+
    FIREBIRD   = 4,    // Firebird 2.5+
    SCRATCHBIRD = 5,   // ScratchBird (federated)
    ORACLE     = 6,    // Oracle 12c+ (future)
    SQLITE     = 7,    // SQLite 3.x (future)
    ODBC       = 8,    // Generic ODBC
    JDBC       = 9,    // Generic JDBC (future)
};

/**
 * @brief Connection lifecycle states
 */
enum class ConnectionState : uint8_t {
    DISCONNECTED  = 0,   // Not connected
    CONNECTING    = 1,   // Connection in progress
    AUTHENTICATING = 2,  // Authentication in progress
    CONNECTED     = 3,   // Ready for queries
    IN_TRANSACTION = 4,  // Transaction active
    EXECUTING     = 5,   // Query executing
    FETCHING      = 6,   // Fetching results
    CLOSING       = 7,   // Graceful close in progress
    FAILED        = 8,   // Connection failed
};

/**
 * @brief Query pushdown capabilities
 */
enum class PushdownCapability : uint32_t {
    NONE           = 0x0000,
    WHERE_CLAUSE   = 0x0001,   // Filter pushdown
    ORDER_BY       = 0x0002,   // Sorting pushdown
    LIMIT_OFFSET   = 0x0004,   // Pagination pushdown
    AGGREGATE      = 0x0008,   // COUNT, SUM, etc.
    GROUP_BY       = 0x0010,   // Grouping pushdown
    HAVING         = 0x0020,   // HAVING clause
    JOIN           = 0x0040,   // Remote joins
    SUBQUERY       = 0x0080,   // Subquery pushdown
    CTE            = 0x0100,   // WITH clause
    WINDOW_FUNC    = 0x0200,   // Window functions
    DISTINCT       = 0x0400,   // DISTINCT pushdown
    UNION          = 0x0800,   // Set operations

    // Common combinations
    BASIC = WHERE_CLAUSE | ORDER_BY | LIMIT_OFFSET,
    STANDARD = BASIC | AGGREGATE | GROUP_BY | HAVING | DISTINCT,
    FULL = 0xFFFF,
};

inline PushdownCapability operator|(PushdownCapability a, PushdownCapability b) {
    return static_cast<PushdownCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline PushdownCapability operator&(PushdownCapability a, PushdownCapability b) {
    return static_cast<PushdownCapability>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasCapability(PushdownCapability caps, PushdownCapability check) {
    return (caps & check) == check;
}

/**
 * @brief SSL connection modes
 */
enum class SslMode : uint8_t {
    DISABLE    = 0,   // No SSL
    ALLOW      = 1,   // Use SSL if server supports it
    PREFER     = 2,   // Try SSL first, fall back to unencrypted
    REQUIRE    = 3,   // Require SSL
    VERIFY_CA  = 4,   // Require SSL with CA verification
    VERIFY_FULL = 5,  // Require SSL with full verification
};

/**
 * @brief Health check types
 */
enum class HealthCheckType : uint8_t {
    PING,           // Simple connection test
    QUERY,          // Execute test query
    FULL_VALIDATE,  // Reset and validate fully
};

/**
 * @brief Pool exhaustion policy
 */
enum class ExhaustionPolicy : uint8_t {
    WAIT,           // Wait for available connection
    FAIL_FAST,      // Return error immediately
    OVERFLOW,       // Create temporary connection beyond max
};

// =============================================================================
// Type Mappings - PostgreSQL OIDs
// =============================================================================

namespace pg_types {
    constexpr uint32_t BOOL        = 16;
    constexpr uint32_t INT2        = 21;
    constexpr uint32_t INT4        = 23;
    constexpr uint32_t INT8        = 20;
    constexpr uint32_t FLOAT4      = 700;
    constexpr uint32_t FLOAT8      = 701;
    constexpr uint32_t NUMERIC     = 1700;
    constexpr uint32_t VARCHAR     = 1043;
    constexpr uint32_t TEXT        = 25;
    constexpr uint32_t CHAR        = 1042;
    constexpr uint32_t BYTEA       = 17;
    constexpr uint32_t DATE        = 1082;
    constexpr uint32_t TIME        = 1083;
    constexpr uint32_t TIMESTAMP   = 1114;
    constexpr uint32_t TIMESTAMPTZ = 1184;
    constexpr uint32_t INTERVAL    = 1186;
    constexpr uint32_t UUID        = 2950;
    constexpr uint32_t JSON        = 114;
    constexpr uint32_t JSONB       = 3802;
    constexpr uint32_t INET        = 869;
    constexpr uint32_t CIDR        = 650;
    constexpr uint32_t MACADDR     = 829;
    constexpr uint32_t POINT       = 600;
    constexpr uint32_t LSEG        = 601;
    constexpr uint32_t PATH        = 602;
    constexpr uint32_t BOX         = 603;
    constexpr uint32_t POLYGON     = 604;
    constexpr uint32_t CIRCLE      = 718;
    constexpr uint32_t ARRAY_INT4  = 1007;
    constexpr uint32_t ARRAY_TEXT  = 1009;
    constexpr uint32_t INT4RANGE   = 3904;
    constexpr uint32_t NUMRANGE    = 3906;
    constexpr uint32_t TSRANGE     = 3908;
    constexpr uint32_t TSTZRANGE   = 3910;
    constexpr uint32_t DATERANGE   = 3912;
    constexpr uint32_t INT8RANGE   = 3926;
}

// =============================================================================
// Type Mappings - MySQL Type Codes
// =============================================================================

namespace mysql_types {
    constexpr uint8_t DECIMAL    = 0x00;
    constexpr uint8_t TINY       = 0x01;
    constexpr uint8_t SHORT      = 0x02;
    constexpr uint8_t LONG       = 0x03;
    constexpr uint8_t FLOAT      = 0x04;
    constexpr uint8_t DOUBLE     = 0x05;
    constexpr uint8_t NULL_TYPE  = 0x06;
    constexpr uint8_t TIMESTAMP  = 0x07;
    constexpr uint8_t LONGLONG   = 0x08;
    constexpr uint8_t INT24      = 0x09;
    constexpr uint8_t DATE       = 0x0A;
    constexpr uint8_t TIME       = 0x0B;
    constexpr uint8_t DATETIME   = 0x0C;
    constexpr uint8_t YEAR       = 0x0D;
    constexpr uint8_t VARCHAR    = 0x0F;
    constexpr uint8_t BIT        = 0x10;
    constexpr uint8_t JSON       = 0xF5;
    constexpr uint8_t BLOB       = 0xFC;
    constexpr uint8_t VAR_STRING = 0xFD;
    constexpr uint8_t STRING     = 0xFE;
    constexpr uint8_t GEOMETRY   = 0xFF;
}

// =============================================================================
// Type Mappings - Firebird SQL Types
// =============================================================================

namespace firebird_types {
    constexpr int16_t SQL_SHORT       = 500;
    constexpr int16_t SQL_LONG        = 496;
    constexpr int16_t SQL_QUAD        = 550;
    constexpr int16_t SQL_INT64       = 580;
    constexpr int16_t SQL_INT128      = 32752;
    constexpr int16_t SQL_FLOAT       = 482;
    constexpr int16_t SQL_DOUBLE      = 480;
    constexpr int16_t SQL_D_FLOAT     = 530;
    constexpr int16_t SQL_TEXT        = 452;
    constexpr int16_t SQL_VARYING     = 448;
    constexpr int16_t SQL_BLOB        = 520;
    constexpr int16_t SQL_TYPE_DATE   = 570;
    constexpr int16_t SQL_TYPE_TIME   = 560;
    constexpr int16_t SQL_TIMESTAMP   = 510;
    constexpr int16_t SQL_TIME_TZ     = 32756;
    constexpr int16_t SQL_TIMESTAMP_TZ = 32754;
    constexpr int16_t SQL_BOOLEAN     = 32764;
    constexpr int16_t SQL_DEC_FIXED   = 32758;
    constexpr int16_t SQL_DEC64       = 32762;
    constexpr int16_t SQL_DEC128      = 32760;
    constexpr int16_t SQL_NULL        = 32766;
}

// =============================================================================
// Configuration Structures
// =============================================================================

/**
 * @brief Represents a CREATE SERVER definition
 */
struct ServerDefinition {
    // Identity
    uint64_t server_id = 0;
    std::string server_name;
    RemoteDatabaseType db_type = RemoteDatabaseType::POSTGRESQL;
    std::string fdw_name;

    // Connection
    std::string host;
    uint16_t port = 0;
    std::string database;

    // Pool settings
    uint32_t min_connections = 1;
    uint32_t max_connections = 10;
    uint32_t connection_timeout_ms = 5000;
    uint32_t query_timeout_ms = 30000;
    uint32_t idle_timeout_ms = 300000;
    uint32_t max_lifetime_ms = 3600000;

    // SSL/TLS settings
    bool use_ssl = false;
    SslMode ssl_mode = SslMode::PREFER;
    std::string ssl_ca_cert;
    std::string ssl_client_cert;
    std::string ssl_client_key;

    // Additional options
    std::unordered_map<std::string, std::string> options;

    // Metadata
    uint64_t created_at = 0;
    uint64_t modified_at = 0;
    std::string owner;

    // Get default port for database type
    static uint16_t defaultPort(RemoteDatabaseType type) {
        switch (type) {
            case RemoteDatabaseType::POSTGRESQL: return 5432;
            case RemoteDatabaseType::MYSQL: return 3306;
            case RemoteDatabaseType::MSSQL: return 1433;
            case RemoteDatabaseType::FIREBIRD: return 3050;
            case RemoteDatabaseType::SCRATCHBIRD: return 3092;
            default: return 0;
        }
    }
};

/**
 * @brief Maps local users to remote database credentials
 */
struct UserMapping {
    uint64_t mapping_id = 0;
    uint64_t server_id = 0;
    std::string local_user;

    // Remote credentials
    std::string remote_user;
    std::string remote_password;

    // Authentication options
    std::string auth_type = "password";
    std::string kerberos_principal;
    std::string client_certificate;

    // Additional options
    std::unordered_map<std::string, std::string> options;
};

/**
 * @brief Column mapping between local and remote tables
 */
struct ColumnMapping {
    std::string local_name;
    std::string remote_name;
    uint32_t local_type_id = 0;      // ScratchBird type ID
    uint32_t remote_type_oid = 0;    // Remote type identifier
    int32_t type_modifier = -1;      // Type modifier (e.g., varchar length)
    bool nullable = true;
    std::string default_value;
};

/**
 * @brief Represents a foreign table definition
 */
struct ForeignTableDefinition {
    uint64_t table_id = 0;
    uint64_t server_id = 0;
    std::string local_schema;
    std::string local_name;

    std::string remote_schema;
    std::string remote_name;

    // Column mappings
    std::vector<ColumnMapping> columns;

    // Pushdown hints
    PushdownCapability capabilities = PushdownCapability::STANDARD;
    bool updatable = true;

    // Cost hints for query planner
    double estimated_row_count = 1000.0;
    double startup_cost = 10.0;
    double per_row_cost = 0.01;

    // Options
    uint32_t fetch_size = 1000;
    std::unordered_map<std::string, std::string> options;
};

// =============================================================================
// Query Result Structures
// =============================================================================

/**
 * @brief Describes a column in remote query results
 */
struct RemoteColumnDesc {
    std::string name;
    uint32_t type_oid = 0;
    int32_t type_modifier = -1;
    bool nullable = true;
    uint32_t mapped_type_id = 0;  // ScratchBird type
};

/**
 * @brief Describes a remote index
 */
struct RemoteIndexDesc {
    std::string index_name;
    std::string table_name;
    std::vector<std::string> columns;
    bool is_unique = false;
    bool is_primary = false;
    std::string index_type;  // btree, hash, gin, etc.
};

/**
 * @brief Describes a remote foreign key
 */
struct RemoteForeignKey {
    std::string constraint_name;
    std::string table_name;
    std::vector<std::string> columns;
    std::string referenced_table;
    std::vector<std::string> referenced_columns;
    std::string on_delete;
    std::string on_update;
};

/**
 * @brief Value type for remote query results
 * Uses a variant to hold different SQL types
 */
using RemoteValue = std::variant<
    std::monostate,                    // NULL
    bool,                              // BOOLEAN
    int16_t,                           // SMALLINT
    int32_t,                           // INTEGER
    int64_t,                           // BIGINT
    float,                             // REAL
    double,                            // DOUBLE
    std::string,                       // VARCHAR, TEXT, etc.
    std::vector<uint8_t>               // BLOB, BYTEA
>;

/**
 * @brief Row of remote query results
 */
using RemoteRow = std::vector<RemoteValue>;

/**
 * @brief Result set from a remote query
 */
struct RemoteQueryResult {
    // Status
    bool success = false;
    std::string error_message;
    std::string sql_state;

    // Metadata
    std::vector<RemoteColumnDesc> columns;
    uint64_t rows_affected = 0;

    // Data (row-oriented)
    std::vector<RemoteRow> rows;

    // Statistics
    uint64_t execution_time_us = 0;
    uint64_t fetch_time_us = 0;
    uint64_t bytes_transferred = 0;

    // Cursor for large results
    bool has_more = false;
    std::string cursor_name;
};

// =============================================================================
// Statistics Structures
// =============================================================================

/**
 * @brief Connection-level statistics
 */
struct ConnectionStats {
    uint64_t queries_executed = 0;
    uint64_t rows_fetched = 0;
    uint64_t bytes_sent = 0;
    uint64_t bytes_received = 0;
    uint64_t errors = 0;
    uint64_t total_query_time_us = 0;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_used;
};

/**
 * @brief Pool-level statistics
 */
struct PoolStats {
    uint32_t total_connections = 0;
    uint32_t active_connections = 0;
    uint32_t idle_connections = 0;
    uint32_t waiting_requests = 0;
    uint64_t total_acquires = 0;
    uint64_t total_releases = 0;
    uint64_t acquire_timeouts = 0;
    uint64_t connection_errors = 0;
    uint64_t avg_acquire_time_us = 0;
    uint64_t max_acquire_time_us = 0;
};

/**
 * @brief Server pool statistics
 */
struct ServerPoolStats {
    std::string server_name;
    RemoteDatabaseType db_type = RemoteDatabaseType::POSTGRESQL;
    bool healthy = true;
    std::string unhealthy_reason;

    uint32_t total_user_pools = 0;
    uint32_t total_connections = 0;
    uint32_t active_connections = 0;
    uint32_t idle_connections = 0;
    uint32_t pending_requests = 0;

    uint64_t total_acquires = 0;
    uint64_t total_releases = 0;
    uint64_t acquire_timeouts = 0;
    uint64_t connection_errors = 0;
    uint64_t queries_executed = 0;

    double avg_acquire_time_ms = 0.0;
    double avg_query_time_ms = 0.0;

    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;
};

/**
 * @brief Registry-level statistics
 */
struct RegistryStats {
    uint32_t total_servers = 0;
    uint32_t healthy_servers = 0;
    uint32_t unhealthy_servers = 0;
    uint64_t total_connections = 0;
    uint64_t active_connections = 0;
    uint64_t total_acquires = 0;
    uint64_t total_releases = 0;
    uint64_t total_errors = 0;
};

// =============================================================================
// Health Check Configuration
// =============================================================================

/**
 * @brief Health check configuration
 */
struct HealthCheckConfig {
    HealthCheckType type = HealthCheckType::PING;
    std::string test_query = "SELECT 1";
    std::chrono::milliseconds timeout{5000};
    std::chrono::milliseconds interval{30000};
    uint32_t failure_threshold = 3;
    uint32_t recovery_threshold = 2;
};

// =============================================================================
// SQLSTATE Error Codes
// =============================================================================

namespace remote_sqlstate {
    // Connection errors (RD001-RD009)
    constexpr const char* CONNECTION_FAILED     = "RD001";
    constexpr const char* AUTH_FAILED           = "RD002";
    constexpr const char* SSL_ERROR             = "RD003";
    constexpr const char* TIMEOUT               = "RD004";
    constexpr const char* POOL_EXHAUSTED        = "RD005";
    constexpr const char* SERVER_UNAVAILABLE    = "RD006";
    constexpr const char* NETWORK_ERROR         = "RD007";
    constexpr const char* PROTOCOL_ERROR        = "RD008";
    constexpr const char* VERSION_MISMATCH      = "RD009";

    // Query errors (RD010-RD019)
    constexpr const char* REMOTE_SYNTAX_ERROR   = "RD010";
    constexpr const char* REMOTE_PERMISSION     = "RD011";
    constexpr const char* REMOTE_OBJECT_NOT_FOUND = "RD012";
    constexpr const char* TYPE_CONVERSION       = "RD013";
    constexpr const char* PUSHDOWN_FAILED       = "RD014";
    constexpr const char* CURSOR_ERROR          = "RD015";
    constexpr const char* PREPARED_STMT_ERROR   = "RD016";

    // Transaction errors (RD020-RD029)
    constexpr const char* REMOTE_DEADLOCK       = "RD020";
    constexpr const char* REMOTE_SERIALIZATION  = "RD021";
    constexpr const char* REMOTE_CONSTRAINT     = "RD022";
    constexpr const char* TRANSACTION_ABORTED   = "RD023";

    // Configuration errors (RD030-RD039)
    constexpr const char* SERVER_NOT_FOUND      = "RD030";
    constexpr const char* USER_MAPPING_MISSING  = "RD031";
    constexpr const char* INVALID_OPTION        = "RD032";
    constexpr const char* FOREIGN_TABLE_ERROR   = "RD033";
}

// =============================================================================
// Constants and Limits
// =============================================================================

namespace remote_db_limits {
    // Connection pool
    constexpr uint32_t DEFAULT_MIN_CONNECTIONS = 1;
    constexpr uint32_t DEFAULT_MAX_CONNECTIONS = 10;
    constexpr uint32_t MAX_CONNECTIONS_PER_SERVER = 100;

    // Timeouts (milliseconds)
    constexpr uint32_t DEFAULT_CONNECT_TIMEOUT = 5000;
    constexpr uint32_t DEFAULT_QUERY_TIMEOUT = 30000;
    constexpr uint32_t DEFAULT_IDLE_TIMEOUT = 300000;
    constexpr uint32_t MAX_QUERY_TIMEOUT = 3600000;

    // Buffer sizes
    constexpr size_t DEFAULT_RECEIVE_BUFFER = 65536;
    constexpr size_t DEFAULT_SEND_BUFFER = 65536;
    constexpr size_t MAX_PACKET_SIZE = 16777216;

    // Batch sizes
    constexpr uint32_t DEFAULT_FETCH_SIZE = 1000;
    constexpr uint32_t MAX_FETCH_SIZE = 100000;
    constexpr uint32_t DEFAULT_BATCH_INSERT_SIZE = 1000;

    // Query limits
    constexpr size_t MAX_SQL_LENGTH = 16777216;
    constexpr uint32_t MAX_PARAMETERS = 65535;
    constexpr uint32_t MAX_COLUMNS = 1664;

    // Health check
    constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 30000;
    constexpr uint32_t HEALTH_CHECK_TIMEOUT_MS = 5000;
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Convert database type to string
 */
const char* databaseTypeToString(RemoteDatabaseType type);

/**
 * @brief Parse database type from string
 */
bool parseDatabaseType(const std::string& str, RemoteDatabaseType& type);

/**
 * @brief Convert SSL mode to string
 */
const char* sslModeToString(SslMode mode);

/**
 * @brief Parse SSL mode from string
 */
bool parseSslMode(const std::string& str, SslMode& mode);

/**
 * @brief Convert connection state to string
 */
const char* connectionStateToString(ConnectionState state);

/**
 * @brief Get health check query for database type
 */
const char* healthCheckQuery(RemoteDatabaseType type);

/**
 * @brief Check if a value is NULL
 */
inline bool isNull(const RemoteValue& value) {
    return std::holds_alternative<std::monostate>(value);
}

}  // namespace fdw
}  // namespace scratchbird

#endif  // SCRATCHBIRD_FDW_TYPES_H
