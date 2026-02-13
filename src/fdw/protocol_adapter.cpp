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
 * @file protocol_adapter.cpp
 * @brief Protocol Adapter Base Implementation
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#include "scratchbird/fdw/protocol_adapter.h"
#include "scratchbird/fdw/postgresql_adapter.h"
#include "scratchbird/fdw/mysql_adapter.h"
#include "scratchbird/fdw/firebird_adapter.h"

namespace scratchbird {
namespace fdw {

// =============================================================================
// ProtocolAdapterBase Implementation
// =============================================================================

ProtocolAdapterBase::ProtocolAdapterBase()
    : state_(ConnectionState::DISCONNECTED)
    , stats_()
    , server_version_()
    , query_start_()
{
    stats_.created_at = std::chrono::steady_clock::now();
    stats_.last_used = stats_.created_at;
}

void ProtocolAdapterBase::recordQueryStart() {
    query_start_ = std::chrono::steady_clock::now();
}

void ProtocolAdapterBase::recordQueryEnd(bool success, uint64_t rows) {
    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - query_start_).count();

    stats_.queries_executed++;
    stats_.rows_fetched += rows;
    stats_.total_query_time_us += duration_us;
    stats_.last_used = end;

    if (!success) {
        stats_.errors++;
    }
}

void ProtocolAdapterBase::recordBytesSent(uint64_t bytes) {
    stats_.bytes_sent += bytes;
}

void ProtocolAdapterBase::recordBytesReceived(uint64_t bytes) {
    stats_.bytes_received += bytes;
}

// =============================================================================
// ProtocolAdapterFactory Implementation
// =============================================================================

std::unique_ptr<IProtocolAdapter> ProtocolAdapterFactory::create(RemoteDatabaseType type) {
    switch (type) {
        case RemoteDatabaseType::POSTGRESQL:
            return std::make_unique<PostgreSQLAdapter>();

        case RemoteDatabaseType::MYSQL:
            return std::make_unique<MySQLAdapter>();

        case RemoteDatabaseType::FIREBIRD:
            return std::make_unique<FirebirdAdapter>();

        case RemoteDatabaseType::MSSQL:
        case RemoteDatabaseType::SCRATCHBIRD:
        case RemoteDatabaseType::ORACLE:
        case RemoteDatabaseType::SQLITE:
        case RemoteDatabaseType::ODBC:
        case RemoteDatabaseType::JDBC:
            // Not yet implemented
            return nullptr;

        default:
            return nullptr;
    }
}

bool ProtocolAdapterFactory::isSupported(RemoteDatabaseType type) {
    switch (type) {
        case RemoteDatabaseType::POSTGRESQL:
        case RemoteDatabaseType::MYSQL:
        case RemoteDatabaseType::FIREBIRD:
            return true;
        default:
            return false;
    }
}

std::vector<RemoteDatabaseType> ProtocolAdapterFactory::supportedTypes() {
    return {
        RemoteDatabaseType::POSTGRESQL,
        RemoteDatabaseType::MYSQL,
        RemoteDatabaseType::FIREBIRD
    };
}

}  // namespace fdw
}  // namespace scratchbird
