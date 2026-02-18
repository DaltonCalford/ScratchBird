/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/udr/udr_connector.h"

#include "scratchbird/udr/firebird_udr.h"
#include "scratchbird/udr/mysql_udr.h"
#include "scratchbird/udr/postgresql_udr.h"
#include "scratchbird/udr/scratchbird_udr.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace scratchbird {
namespace udr {

namespace {

constexpr ConnectorType kUnknownConnectorType = static_cast<ConnectorType>(0);

auto normalizeToken(const std::string& value) -> std::string {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    const auto first = out.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string{};
    }
    const auto last = out.find_last_not_of(" \t\r\n");
    return out.substr(first, (last - first) + 1);
}

auto supportedConnectorTypes() -> const std::vector<ConnectorType>& {
    static const std::vector<ConnectorType> kSupported{
        ConnectorType::POSTGRESQL,
        ConnectorType::MYSQL,
        ConnectorType::FIREBIRD,
        ConnectorType::SCRATCHBIRD,
    };
    return kSupported;
}

auto extractConnectorScheme(const std::string& connection_string) -> std::string {
    const std::string normalized = normalizeToken(connection_string);
    if (normalized.empty()) {
        return normalized;
    }

    const size_t scheme_sep = normalized.find("://");
    if (scheme_sep != std::string::npos) {
        return normalized.substr(0, scheme_sep);
    }

    const size_t colon = normalized.find(':');
    const size_t slash = normalized.find('/');
    if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
        return normalized.substr(0, colon);
    }

    return normalized;
}

} // namespace

auto RemoteValue::toString() const -> std::string {
    if (is_null || data.empty()) {
        return std::string{};
    }
    return std::string(data.begin(), data.end());
}

auto RemoteValue::toInt64() const -> int64_t {
    if (is_null || data.empty()) {
        return 0;
    }

    const std::string text = toString();
    if (text.empty()) {
        return 0;
    }

    try {
        return std::stoll(text);
    } catch (...) {
        return 0;
    }
}

auto RemoteValue::toDouble() const -> double {
    if (is_null || data.empty()) {
        return 0.0;
    }

    const std::string text = toString();
    if (text.empty()) {
        return 0.0;
    }

    try {
        return std::stod(text);
    } catch (...) {
        return 0.0;
    }
}

auto RemoteValue::toBool() const -> bool {
    if (is_null || data.empty()) {
        return false;
    }

    std::string text = normalizeToken(toString());
    if (text.empty()) {
        return false;
    }

    return text == "1" || text == "t" || text == "true" || text == "y" || text == "yes";
}

auto UDRConnectorFactory::create(ConnectorType type) -> std::unique_ptr<UDRConnector> {
    switch (type) {
        case ConnectorType::POSTGRESQL:
            return std::make_unique<PostgreSQLUDRConnector>();
        case ConnectorType::MYSQL:
            return std::make_unique<MySQLUDRConnector>();
        case ConnectorType::FIREBIRD:
            return std::make_unique<FirebirdUDRConnector>();
        case ConnectorType::SCRATCHBIRD:
            return std::make_unique<ScratchBirdUDRConnector>();
        case ConnectorType::CASSANDRA:
        case ConnectorType::MILVUS:
        case ConnectorType::MONGODB:
        case ConnectorType::NEO4J:
        case ConnectorType::REDIS:
        case ConnectorType::MARIADB:
        case ConnectorType::INFLUXDB:
        case ConnectorType::CLICKHOUSE:
        case ConnectorType::OPENSEARCH:
        case ConnectorType::DUCKDB:
        case ConnectorType::ODBC:
            return nullptr;
        default:
            return nullptr;
    }
}

auto UDRConnectorFactory::create(const std::string& connection_string) -> std::unique_ptr<UDRConnector> {
    const ConnectorType type = stringToType(extractConnectorScheme(connection_string));
    if (!isSupported(type)) {
        return nullptr;
    }
    return create(type);
}

auto UDRConnectorFactory::isSupported(ConnectorType type) -> bool {
    const auto& supported = supportedConnectorTypes();
    return std::find(supported.begin(), supported.end(), type) != supported.end();
}

auto UDRConnectorFactory::getSupportedTypes() -> std::vector<ConnectorType> {
    return supportedConnectorTypes();
}

auto UDRConnectorFactory::typeToString(ConnectorType type) -> const char* {
    switch (type) {
        case ConnectorType::POSTGRESQL:
            return "postgresql";
        case ConnectorType::MYSQL:
            return "mysql";
        case ConnectorType::FIREBIRD:
            return "firebird";
        case ConnectorType::SCRATCHBIRD:
            return "scratchbird";
        case ConnectorType::ODBC:
            return "odbc";
        case ConnectorType::CASSANDRA:
            return "cassandra";
        case ConnectorType::MILVUS:
            return "milvus";
        case ConnectorType::MONGODB:
            return "mongodb";
        case ConnectorType::NEO4J:
            return "neo4j";
        case ConnectorType::REDIS:
            return "redis";
        case ConnectorType::MARIADB:
            return "mariadb";
        case ConnectorType::INFLUXDB:
            return "influxdb";
        case ConnectorType::CLICKHOUSE:
            return "clickhouse";
        case ConnectorType::OPENSEARCH:
            return "opensearch";
        case ConnectorType::DUCKDB:
            return "duckdb";
        default:
            return "unknown";
    }
}

auto UDRConnectorFactory::stringToType(const std::string& str) -> ConnectorType {
    const std::string normalized = normalizeToken(str);
    if (normalized == "postgresql" || normalized == "postgres" || normalized == "pg") {
        return ConnectorType::POSTGRESQL;
    }
    if (normalized == "mysql") {
        return ConnectorType::MYSQL;
    }
    if (normalized == "mariadb") {
        return ConnectorType::MARIADB;
    }
    if (normalized == "firebird" || normalized == "firebirdsql") {
        return ConnectorType::FIREBIRD;
    }
    if (normalized == "cassandra" || normalized == "cql") {
        return ConnectorType::CASSANDRA;
    }
    if (normalized == "milvus") {
        return ConnectorType::MILVUS;
    }
    if (normalized == "mongodb" || normalized == "mongo" || normalized == "bson") {
        return ConnectorType::MONGODB;
    }
    if (normalized == "neo4j" || normalized == "cypher") {
        return ConnectorType::NEO4J;
    }
    if (normalized == "redis" || normalized == "resp") {
        return ConnectorType::REDIS;
    }
    if (normalized == "influxdb" || normalized == "influx" || normalized == "influxql") {
        return ConnectorType::INFLUXDB;
    }
    if (normalized == "clickhouse") {
        return ConnectorType::CLICKHOUSE;
    }
    if (normalized == "opensearch") {
        return ConnectorType::OPENSEARCH;
    }
    if (normalized == "duckdb") {
        return ConnectorType::DUCKDB;
    }
    if (normalized == "scratchbird" || normalized == "scratch_bird" || normalized == "sb") {
        return ConnectorType::SCRATCHBIRD;
    }
    if (normalized == "odbc") {
        return ConnectorType::ODBC;
    }
    return kUnknownConnectorType;
}

auto sys_remote_exec(const std::string&,
                     const std::string&,
                     uint64_t& rows_affected,
                     core::ErrorContext* ctx) -> core::Status {
    rows_affected = 0;
    if (ctx) {
        ctx->set(core::Status::NOT_IMPLEMENTED,
                 "sys.remote_exec is not implemented in this build",
                 __FILE__,
                 __LINE__,
                 __func__);
    }
    return core::Status::NOT_IMPLEMENTED;
}

auto sys_remote_query(const std::string&,
                      const std::string&,
                      RemoteResultSet& result,
                      core::ErrorContext* ctx) -> core::Status {
    result.clear();
    if (ctx) {
        ctx->set(core::Status::NOT_IMPLEMENTED,
                 "sys.remote_query is not implemented in this build",
                 __FILE__,
                 __LINE__,
                 __func__);
    }
    return core::Status::NOT_IMPLEMENTED;
}

auto sys_remote_call(const std::string&,
                     const std::string&,
                     const std::vector<RemoteValue>&,
                     RemoteResultSet& result,
                     core::ErrorContext* ctx) -> core::Status {
    result.clear();
    if (ctx) {
        ctx->set(core::Status::NOT_IMPLEMENTED,
                 "sys.remote_call is not implemented in this build",
                 __FILE__,
                 __LINE__,
                 __func__);
    }
    return core::Status::NOT_IMPLEMENTED;
}

} // namespace udr
} // namespace scratchbird
