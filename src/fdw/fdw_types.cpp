/**
 * @file fdw_types.cpp
 * @brief Foreign Data Wrapper Core Types Implementation
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#include "scratchbird/fdw/fdw_types.h"

#include <algorithm>
#include <cctype>

namespace scratchbird {
namespace fdw {

const char* databaseTypeToString(RemoteDatabaseType type) {
    switch (type) {
        case RemoteDatabaseType::POSTGRESQL: return "postgresql";
        case RemoteDatabaseType::MYSQL: return "mysql";
        case RemoteDatabaseType::MSSQL: return "mssql";
        case RemoteDatabaseType::FIREBIRD: return "firebird";
        case RemoteDatabaseType::SCRATCHBIRD: return "scratchbird";
        case RemoteDatabaseType::ORACLE: return "oracle";
        case RemoteDatabaseType::SQLITE: return "sqlite";
        case RemoteDatabaseType::ODBC: return "odbc";
        case RemoteDatabaseType::JDBC: return "jdbc";
        default: return "unknown";
    }
}

bool parseDatabaseType(const std::string& str, RemoteDatabaseType& type) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "postgresql" || lower == "postgres" || lower == "pg") {
        type = RemoteDatabaseType::POSTGRESQL;
        return true;
    }
    if (lower == "mysql" || lower == "mariadb") {
        type = RemoteDatabaseType::MYSQL;
        return true;
    }
    if (lower == "mssql" || lower == "sqlserver" || lower == "sql_server") {
        type = RemoteDatabaseType::MSSQL;
        return true;
    }
    if (lower == "firebird" || lower == "fb" || lower == "interbase") {
        type = RemoteDatabaseType::FIREBIRD;
        return true;
    }
    if (lower == "scratchbird" || lower == "sb") {
        type = RemoteDatabaseType::SCRATCHBIRD;
        return true;
    }
    if (lower == "oracle" || lower == "ora") {
        type = RemoteDatabaseType::ORACLE;
        return true;
    }
    if (lower == "sqlite" || lower == "sqlite3") {
        type = RemoteDatabaseType::SQLITE;
        return true;
    }
    if (lower == "odbc") {
        type = RemoteDatabaseType::ODBC;
        return true;
    }
    if (lower == "jdbc") {
        type = RemoteDatabaseType::JDBC;
        return true;
    }
    return false;
}

const char* sslModeToString(SslMode mode) {
    switch (mode) {
        case SslMode::DISABLE: return "disable";
        case SslMode::ALLOW: return "allow";
        case SslMode::PREFER: return "prefer";
        case SslMode::REQUIRE: return "require";
        case SslMode::VERIFY_CA: return "verify-ca";
        case SslMode::VERIFY_FULL: return "verify-full";
        default: return "unknown";
    }
}

bool parseSslMode(const std::string& str, SslMode& mode) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "disable" || lower == "disabled" || lower == "off" || lower == "false") {
        mode = SslMode::DISABLE;
        return true;
    }
    if (lower == "allow") {
        mode = SslMode::ALLOW;
        return true;
    }
    if (lower == "prefer" || lower == "preferred") {
        mode = SslMode::PREFER;
        return true;
    }
    if (lower == "require" || lower == "required" || lower == "on" || lower == "true") {
        mode = SslMode::REQUIRE;
        return true;
    }
    if (lower == "verify-ca" || lower == "verify_ca" || lower == "verifyca") {
        mode = SslMode::VERIFY_CA;
        return true;
    }
    if (lower == "verify-full" || lower == "verify_full" || lower == "verifyfull") {
        mode = SslMode::VERIFY_FULL;
        return true;
    }
    return false;
}

const char* connectionStateToString(ConnectionState state) {
    switch (state) {
        case ConnectionState::DISCONNECTED: return "disconnected";
        case ConnectionState::CONNECTING: return "connecting";
        case ConnectionState::AUTHENTICATING: return "authenticating";
        case ConnectionState::CONNECTED: return "connected";
        case ConnectionState::IN_TRANSACTION: return "in_transaction";
        case ConnectionState::EXECUTING: return "executing";
        case ConnectionState::FETCHING: return "fetching";
        case ConnectionState::CLOSING: return "closing";
        case ConnectionState::FAILED: return "failed";
        default: return "unknown";
    }
}

const char* healthCheckQuery(RemoteDatabaseType type) {
    switch (type) {
        case RemoteDatabaseType::POSTGRESQL:
        case RemoteDatabaseType::MYSQL:
        case RemoteDatabaseType::MSSQL:
        case RemoteDatabaseType::SCRATCHBIRD:
            return "SELECT 1";
        case RemoteDatabaseType::FIREBIRD:
            return "SELECT 1 FROM RDB$DATABASE";
        case RemoteDatabaseType::ORACLE:
            return "SELECT 1 FROM DUAL";
        case RemoteDatabaseType::SQLITE:
            return "SELECT 1";
        default:
            return "SELECT 1";
    }
}

}  // namespace fdw
}  // namespace scratchbird
