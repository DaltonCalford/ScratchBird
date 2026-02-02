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
 * @file mysql_adapter.h
 * @brief MySQL Protocol Adapter for Foreign Data Wrapper
 *
 * Implements the MySQL wire protocol as a client to connect
 * to remote MySQL/MariaDB databases.
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#ifndef SCRATCHBIRD_FDW_MYSQL_ADAPTER_H
#define SCRATCHBIRD_FDW_MYSQL_ADAPTER_H

#include "scratchbird/fdw/protocol_adapter.h"
#include "scratchbird/fdw/fdw_types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird {
namespace fdw {

/**
 * @brief MySQL wire protocol adapter
 *
 * Implements the MySQL protocol to connect to remote MySQL/MariaDB
 * servers (5.7+/10+) and execute queries.
 */
class MySQLAdapter : public ProtocolAdapterBase {
public:
    MySQLAdapter();
    ~MySQLAdapter() override;

    // =========================================================================
    // Connection Lifecycle
    // =========================================================================

    Result<void> connect(const ServerDefinition& server,
                         const UserMapping& mapping) override;
    Result<void> disconnect() override;

    // =========================================================================
    // Health Check
    // =========================================================================

    Result<bool> ping() override;
    Result<void> reset() override;

    // =========================================================================
    // Query Execution
    // =========================================================================

    Result<RemoteQueryResult> executeQuery(const std::string& sql) override;
    Result<RemoteQueryResult> executeQueryWithParams(
        const std::string& sql,
        const std::vector<RemoteValue>& params) override;

    // =========================================================================
    // Prepared Statements
    // =========================================================================

    Result<uint64_t> prepare(const std::string& sql) override;
    Result<RemoteQueryResult> executePrepared(
        uint64_t stmt_id,
        const std::vector<RemoteValue>& params) override;
    Result<void> deallocatePrepared(uint64_t stmt_id) override;

    // =========================================================================
    // Transaction Control
    // =========================================================================

    Result<void> beginTransaction() override;
    Result<void> commit() override;
    Result<void> rollback() override;
    Result<void> setSavepoint(const std::string& name) override;
    Result<void> rollbackToSavepoint(const std::string& name) override;

    // =========================================================================
    // Cursor Operations
    // =========================================================================

    Result<std::string> declareCursor(const std::string& name,
                                       const std::string& sql) override;
    Result<RemoteQueryResult> fetchFromCursor(const std::string& name,
                                               uint32_t count) override;
    Result<void> closeCursor(const std::string& name) override;

    // =========================================================================
    // Schema Introspection
    // =========================================================================

    Result<std::vector<std::string>> listSchemas() override;
    Result<std::vector<std::string>> listTables(const std::string& schema) override;
    Result<std::vector<RemoteColumnDesc>> describeTable(
        const std::string& schema,
        const std::string& table) override;
    Result<std::vector<RemoteIndexDesc>> describeIndexes(
        const std::string& schema,
        const std::string& table) override;
    Result<std::vector<RemoteForeignKey>> describeForeignKeys(
        const std::string& schema,
        const std::string& table) override;

    // =========================================================================
    // Metadata
    // =========================================================================

    RemoteDatabaseType getDatabaseType() const override {
        return RemoteDatabaseType::MYSQL;
    }
    PushdownCapability getCapabilities() const override;

    // =========================================================================
    // Type Conversion
    // =========================================================================

    uint32_t mapRemoteType(uint32_t remote_oid, int32_t modifier) const override;
    RemoteValue convertToLocal(const void* data, size_t len,
                               uint32_t remote_oid) const override;
    std::vector<uint8_t> convertToRemote(const RemoteValue& value,
                                          uint32_t remote_oid) const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Protocol helpers
    Result<void> sendHandshakeResponse(const ServerDefinition& server,
                                        const UserMapping& mapping,
                                        const std::vector<uint8_t>& auth_data);
    Result<void> sendCommand(uint8_t command, const std::vector<uint8_t>& data);
    Result<void> sendQuery(const std::string& sql);
    Result<RemoteQueryResult> readQueryResult();
    Result<std::vector<uint8_t>> readPacket();
    Result<void> writePacket(const std::vector<uint8_t>& data);

    // Authentication helpers
    std::vector<uint8_t> scramblePassword(const std::string& password,
                                           const std::vector<uint8_t>& auth_data);

    // Type mapping
    static const std::unordered_map<uint8_t, uint32_t>& getTypeMap();
};

/**
 * @brief MySQL FDW registration
 */
void registerMySQLFDW();

}  // namespace fdw
}  // namespace scratchbird

#endif  // SCRATCHBIRD_FDW_MYSQL_ADAPTER_H
