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
 * @file postgresql_adapter.h
 * @brief PostgreSQL Protocol Adapter for Foreign Data Wrapper
 *
 * Implements the PostgreSQL wire protocol (v3) as a client to connect
 * to remote PostgreSQL databases.
 *
 * Part of Phase 3.7: UDR Plugin System
 */

#ifndef SCRATCHBIRD_FDW_POSTGRESQL_ADAPTER_H
#define SCRATCHBIRD_FDW_POSTGRESQL_ADAPTER_H

#include "scratchbird/fdw/protocol_adapter.h"
#include "scratchbird/fdw/fdw_types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird {
namespace fdw {

/**
 * @brief PostgreSQL wire protocol adapter
 *
 * Implements the PostgreSQL v3 protocol to connect to remote PostgreSQL
 * servers (9.6+) and execute queries.
 */
class PostgreSQLAdapter : public ProtocolAdapterBase {
public:
    PostgreSQLAdapter();
    ~PostgreSQLAdapter() override;

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
        return RemoteDatabaseType::POSTGRESQL;
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
    // Implementation details
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Protocol helpers
    Result<void> sendStartupMessage(const ServerDefinition& server,
                                     const UserMapping& mapping);
    Result<void> handleAuthentication(const UserMapping& mapping);
    Result<void> sendSimpleQuery(const std::string& sql);
    Result<void> sendExtendedQuery(const std::string& sql,
                                    const std::vector<RemoteValue>& params);
    Result<RemoteQueryResult> readQueryResult();
    Result<void> sendSync();

    // Message reading/writing
    Result<void> writeMessage(char type, const std::vector<uint8_t>& data);
    Result<std::pair<char, std::vector<uint8_t>>> readMessage();

    // Type conversion helpers
    static const std::unordered_map<uint32_t, uint32_t>& getTypeMap();
};

/**
 * @brief PostgreSQL FDW registration
 */
void registerPostgreSQLFDW();

}  // namespace fdw
}  // namespace scratchbird

#endif  // SCRATCHBIRD_FDW_POSTGRESQL_ADAPTER_H
