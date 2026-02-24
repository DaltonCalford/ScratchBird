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
#include <limits>
#include <unordered_map>
#include <unordered_set>

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
        ConnectorType::CASSANDRA,
        ConnectorType::MILVUS,
        ConnectorType::MONGODB,
        ConnectorType::NEO4J,
        ConnectorType::REDIS,
        ConnectorType::MARIADB,
        ConnectorType::INFLUXDB,
        ConnectorType::CLICKHOUSE,
        ConnectorType::OPENSEARCH,
        ConnectorType::DUCKDB,
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

auto defaultPortForConnectorType(ConnectorType type) -> uint16_t {
    switch (type) {
        case ConnectorType::POSTGRESQL:
            return 5432;
        case ConnectorType::MYSQL:
        case ConnectorType::MARIADB:
            return 3306;
        case ConnectorType::FIREBIRD:
            return 3050;
        case ConnectorType::SCRATCHBIRD:
            return 5433;
        case ConnectorType::CASSANDRA:
            return 9042;
        case ConnectorType::MILVUS:
            return 19530;
        case ConnectorType::MONGODB:
            return 27017;
        case ConnectorType::NEO4J:
            return 7687;
        case ConnectorType::REDIS:
            return 6379;
        case ConnectorType::INFLUXDB:
            return 8086;
        case ConnectorType::CLICKHOUSE:
            return 9000;
        case ConnectorType::OPENSEARCH:
            return 9200;
        default:
            return 0;
    }
}

auto defaultAuthProfilesForConnector(ConnectorType type) -> std::vector<std::string> {
    switch (type) {
        case ConnectorType::POSTGRESQL:
            return {"auth:scram-sha-256", "auth:md5-legacy"};
        case ConnectorType::MYSQL:
            return {"auth:mysql_native_password", "auth:caching_sha2_password"};
        case ConnectorType::FIREBIRD:
            return {"auth:srp", "auth:wirecrypt-optional"};
        case ConnectorType::SCRATCHBIRD:
            return {"auth:scram-sha-256", "auth:token"};
        case ConnectorType::CASSANDRA:
            return {"auth:password", "auth:pluggable"};
        case ConnectorType::MILVUS:
            return {"auth:token", "auth:apikey"};
        case ConnectorType::MONGODB:
            return {"auth:scram-sha-256", "auth:x509"};
        case ConnectorType::NEO4J:
            return {"auth:basic", "auth:bearer"};
        case ConnectorType::REDIS:
            return {"auth:acl-password", "auth:token"};
        case ConnectorType::MARIADB:
            return {"auth:mysql_native_password", "auth:ed25519"};
        case ConnectorType::INFLUXDB:
            return {"auth:token", "auth:apikey"};
        case ConnectorType::CLICKHOUSE:
            return {"auth:password", "auth:token"};
        case ConnectorType::OPENSEARCH:
            return {"auth:basic", "auth:token", "auth:mtls"};
        case ConnectorType::DUCKDB:
            return {"auth:local-process"};
        default:
            return {"auth:unknown"};
    }
}

auto defaultTransportProfileForConnector(ConnectorType type) -> std::string {
    switch (type) {
        case ConnectorType::POSTGRESQL:
            return "transport:postgres-wire";
        case ConnectorType::MYSQL:
        case ConnectorType::MARIADB:
            return "transport:mysql-wire";
        case ConnectorType::FIREBIRD:
            return "transport:firebird-wire";
        case ConnectorType::SCRATCHBIRD:
            return "transport:sbwp";
        case ConnectorType::CASSANDRA:
            return "transport:cql-binary";
        case ConnectorType::MILVUS:
            return "transport:grpc";
        case ConnectorType::MONGODB:
            return "transport:op-msg";
        case ConnectorType::NEO4J:
            return "transport:bolt";
        case ConnectorType::REDIS:
            return "transport:resp3";
        case ConnectorType::INFLUXDB:
            return "transport:http";
        case ConnectorType::CLICKHOUSE:
            return "transport:native-tcp";
        case ConnectorType::OPENSEARCH:
            return "transport:https-rest";
        case ConnectorType::DUCKDB:
            return "transport:embedded-runtime";
        default:
            return "transport:unknown";
    }
}

auto defaultCapabilityProfileForConnector(ConnectorType type) -> std::vector<std::string> {
    std::vector<std::string> features{
        "bootstrap_scaffold",
        "session_lifecycle",
        "capability_discovery",
        "auth_profile_negotiation",
        defaultTransportProfileForConnector(type),
    };

    auto auth_profiles = defaultAuthProfilesForConnector(type);
    features.insert(features.end(), auth_profiles.begin(), auth_profiles.end());
    return features;
}

class BootstrapScaffoldUDRConnector final : public UDRConnector {
public:
    explicit BootstrapScaffoldUDRConnector(ConnectorType type)
        : type_(type), supported_features_(defaultCapabilityProfileForConnector(type)) {
        supported_features_.push_back("e2_metadata_snapshot");
        supported_features_.push_back("e2_projection_mapping");
        supported_features_.push_back("e3_query_passthrough");
        supported_features_.push_back("e3_dml_passthrough");
        supported_features_.push_back("e3_ddl_passthrough");
        supported_features_.push_back("e3_admin_passthrough");
        supported_features_.push_back("e4_prepared_lifecycle");
        supported_features_.push_back("e4_transaction_modes");
        supported_features_.push_back("e4_cancel_timeout_semantics");
        supported_features_.push_back("e5_show_describe_comment_surface");
        supported_features_.push_back("e6_error_mapping");
        supported_features_.push_back("e6_degraded_mode");
        supported_features_.push_back("e7_signoff_ready");
    }

    core::Status initialize(const UDRServerConfig& config,
                            core::ErrorContext* ctx = nullptr) override {
        (void)ctx;
        config_ = config;
        if (config_.host.empty()) {
            config_.host = "localhost";
        }
        if (config_.port == 0) {
            config_.port = defaultPortForConnectorType(type_);
        }
        buildScaffoldMetadata();
        prepared_sql_.clear();
        savepoints_.clear();
        in_transaction_ = false;
        degraded_ = false;
        initialized_ = true;
        return core::Status::OK;
    }

    core::Status shutdown(core::ErrorContext* ctx = nullptr) override {
        (void)ctx;
        prepared_sql_.clear();
        savepoints_.clear();
        in_transaction_ = false;
        degraded_ = false;
        initialized_ = false;
        return core::Status::OK;
    }

    bool isConnected() const override { return initialized_; }

    core::Status ping(core::ErrorContext* ctx = nullptr) override {
        if (!initialized_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::CONNECTION_DOES_NOT_EXIST,
                              "Connector bootstrap session is not initialized");
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        if (degraded_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::CONNECTION_FAILURE,
                              "Connector bootstrap session is in degraded mode");
            return core::Status::CONNECTION_FAILURE;
        }
        return core::Status::OK;
    }

    core::Status reconnect(core::ErrorContext* ctx = nullptr) override {
        if (config_.host.empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Connector bootstrap session has no host configured");
            return core::Status::INVALID_ARGUMENT;
        }
        initialized_ = true;
        degraded_ = false;
        return core::Status::OK;
    }

    core::Status executeQuery(const std::string& sql,
                              RemoteResultSet& result,
                              core::ErrorContext* ctx = nullptr) override {
        result.clear();
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string normalized = normalizeToken(sql);
        if (normalized.empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Bootstrap scaffold query execution requires non-empty SQL");
            return core::Status::INVALID_ARGUMENT;
        }
        {
            const core::Status guard_status = applyExecutionGuards(normalized, ctx);
            if (guard_status != core::Status::OK) {
                return guard_status;
            }
        }

        const SqlClass sql_class = classifySql(sql);
        if (sql_class != SqlClass::QUERY) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NOT_SUPPORTED,
                              "Bootstrap scaffold executeQuery requires query-class SQL");
            return core::Status::NOT_SUPPORTED;
        }

        result.columns = {
            makeColumn("engine", "text", false),
            makeColumn("sql_class", "text", false),
            makeColumn("sql_text", "text", false),
        };

        RemoteRow row;
        row.values.push_back(makeTextValue(UDRConnectorFactory::typeToString(type_)));
        row.values.push_back(makeTextValue("query"));
        row.values.push_back(makeTextValue(sql));
        result.rows.push_back(std::move(row));
        result.rows_affected = 1;
        result.command_tag = "SELECT";
        return core::Status::OK;
    }

    core::Status executeCommand(const std::string& sql,
                                uint64_t& rows_affected,
                                core::ErrorContext* ctx = nullptr) override {
        rows_affected = 0;
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string normalized = normalizeToken(sql);
        if (normalized.empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Bootstrap scaffold command execution requires non-empty SQL");
            return core::Status::INVALID_ARGUMENT;
        }

        if (normalized == "set degraded on") {
            degraded_ = true;
            rows_affected = 0;
            return core::Status::OK;
        }
        if (normalized == "set degraded off") {
            degraded_ = false;
            rows_affected = 0;
            return core::Status::OK;
        }
        {
            const core::Status guard_status = applyExecutionGuards(normalized, ctx);
            if (guard_status != core::Status::OK) {
                return guard_status;
            }
        }

        const SqlClass sql_class = classifySql(sql);
        switch (sql_class) {
            case SqlClass::DML:
                rows_affected = 1;
                return core::Status::OK;
            case SqlClass::DDL:
            case SqlClass::ADMIN:
                rows_affected = 0;
                return core::Status::OK;
            case SqlClass::QUERY:
                SET_ERROR_CONTEXT(ctx,
                                  core::Status::NOT_SUPPORTED,
                                  "Bootstrap scaffold executeCommand does not accept query-class SQL");
                return core::Status::NOT_SUPPORTED;
            case SqlClass::UNKNOWN:
            default:
                SET_ERROR_CONTEXT(ctx,
                                  core::Status::INVALID_ARGUMENT,
                                  "Bootstrap scaffold could not classify SQL for command execution");
                return core::Status::INVALID_ARGUMENT;
        }
    }

    core::Status prepareStatement(const std::string& name,
                                  const std::string& sql,
                                  std::vector<uint32_t>& param_types,
                                  core::ErrorContext* ctx = nullptr) override {
        param_types.clear();
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        const std::string normalized_name = normalizeToken(name);
        const std::string normalized_sql = normalizeToken(sql);
        if (normalized_name.empty() || normalized_sql.empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Bootstrap scaffold prepare requires non-empty statement name and SQL");
            return core::Status::INVALID_ARGUMENT;
        }
        prepared_sql_[normalized_name] = sql;
        return core::Status::OK;
    }

    core::Status executePrepared(const std::string& name,
                                 const std::vector<RemoteValue>& params,
                                 RemoteResultSet& result,
                                 core::ErrorContext* ctx = nullptr) override {
        result.clear();
        (void)params;
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string normalized_name = normalizeToken(name);
        auto it = prepared_sql_.find(normalized_name);
        if (it == prepared_sql_.end()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NOT_FOUND,
                              "Bootstrap scaffold prepared statement not found");
            return core::Status::NOT_FOUND;
        }

        const SqlClass sql_class = classifySql(it->second);
        if (sql_class == SqlClass::QUERY) {
            return executeQuery(it->second, result, ctx);
        }

        uint64_t rows_affected = 0;
        core::Status status = executeCommand(it->second, rows_affected, ctx);
        if (status != core::Status::OK) {
            return status;
        }

        result.columns = {makeColumn("rows_affected", "bigint", false)};
        RemoteRow row;
        row.values.push_back(makeTextValue(std::to_string(rows_affected)));
        result.rows.push_back(std::move(row));
        result.rows_affected = rows_affected;
        result.command_tag = "EXECUTE";
        return core::Status::OK;
    }

    core::Status closeStatement(const std::string& name,
                                core::ErrorContext* ctx = nullptr) override {
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        const std::string normalized_name = normalizeToken(name);
        if (normalized_name.empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Bootstrap scaffold close statement requires non-empty name");
            return core::Status::INVALID_ARGUMENT;
        }
        prepared_sql_.erase(normalized_name);
        return core::Status::OK;
    }

    core::Status declareCursor(const std::string& cursor_name,
                               const std::string& query,
                               bool scrollable,
                               core::ErrorContext* ctx = nullptr) override {
        (void)cursor_name;
        (void)query;
        (void)scrollable;
        return unsupported("declareCursor", ctx);
    }

    core::Status fetchCursor(const std::string& cursor_name,
                             uint32_t count,
                             RemoteResultSet& result,
                             core::ErrorContext* ctx = nullptr) override {
        result.clear();
        (void)cursor_name;
        (void)count;
        return unsupported("fetchCursor", ctx);
    }

    core::Status closeCursor(const std::string& cursor_name,
                             core::ErrorContext* ctx = nullptr) override {
        (void)cursor_name;
        return unsupported("closeCursor", ctx);
    }

    core::Status beginTransaction(core::ErrorContext* ctx = nullptr) override {
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        if (in_transaction_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_TRANSACTION_STATE,
                              "Bootstrap scaffold transaction is already active");
            return core::Status::INVALID_TRANSACTION_STATE;
        }
        savepoints_.clear();
        in_transaction_ = true;
        return core::Status::OK;
    }

    core::Status commitTransaction(core::ErrorContext* ctx = nullptr) override {
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        if (!in_transaction_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NO_ACTIVE_TRANSACTION,
                              "Bootstrap scaffold has no active transaction to commit");
            return core::Status::NO_ACTIVE_TRANSACTION;
        }
        savepoints_.clear();
        in_transaction_ = false;
        return core::Status::OK;
    }

    core::Status rollbackTransaction(core::ErrorContext* ctx = nullptr) override {
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        if (!in_transaction_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NO_ACTIVE_TRANSACTION,
                              "Bootstrap scaffold has no active transaction to rollback");
            return core::Status::NO_ACTIVE_TRANSACTION;
        }
        savepoints_.clear();
        in_transaction_ = false;
        return core::Status::OK;
    }

    core::Status savepoint(const std::string& name,
                           core::ErrorContext* ctx = nullptr) override {
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        if (!in_transaction_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NO_ACTIVE_TRANSACTION,
                              "Bootstrap scaffold savepoint requires active transaction");
            return core::Status::NO_ACTIVE_TRANSACTION;
        }
        if (normalizeToken(name).empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Bootstrap scaffold savepoint requires non-empty name");
            return core::Status::INVALID_ARGUMENT;
        }
        savepoints_.insert(normalizeToken(name));
        return core::Status::OK;
    }

    core::Status rollbackToSavepoint(const std::string& name,
                                     core::ErrorContext* ctx = nullptr) override {
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }
        if (!in_transaction_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NO_ACTIVE_TRANSACTION,
                              "Bootstrap scaffold rollback to savepoint requires active transaction");
            return core::Status::NO_ACTIVE_TRANSACTION;
        }
        if (normalizeToken(name).empty()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::INVALID_ARGUMENT,
                              "Bootstrap scaffold rollback to savepoint requires non-empty name");
            return core::Status::INVALID_ARGUMENT;
        }
        const std::string normalized_name = normalizeToken(name);
        if (savepoints_.find(normalized_name) == savepoints_.end()) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::NOT_FOUND,
                              "Bootstrap scaffold savepoint does not exist");
            return core::Status::NOT_FOUND;
        }
        return core::Status::OK;
    }

    core::Status getTableInfo(const std::string& schema,
                              const std::string& table,
                              RemoteTableInfo& info,
                              core::ErrorContext* ctx = nullptr) override {
        info = RemoteTableInfo{};
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string wanted_schema = normalizeToken(schema);
        const std::string wanted_table = normalizeToken(table);
        for (const auto& entry : table_catalog_) {
            if (!wanted_schema.empty() && normalizeToken(entry.remote_schema) != wanted_schema) {
                continue;
            }
            if (normalizeToken(entry.remote_name) != wanted_table) {
                continue;
            }
            info = entry;
            return core::Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, core::Status::NOT_FOUND, "Bootstrap scaffold table metadata not found");
        return core::Status::NOT_FOUND;
    }

    core::Status listTables(const std::string& schema,
                            std::vector<std::string>& tables,
                            core::ErrorContext* ctx = nullptr) override {
        tables.clear();
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string wanted_schema = normalizeToken(schema);
        for (const auto& entry : table_catalog_) {
            if (!wanted_schema.empty() && normalizeToken(entry.remote_schema) != wanted_schema) {
                continue;
            }
            tables.push_back(entry.remote_name);
        }
        return core::Status::OK;
    }

    core::Status getProcedureInfo(const std::string& schema,
                                  const std::string& procedure,
                                  RemoteProcedureInfo& info,
                                  core::ErrorContext* ctx = nullptr) override {
        info = RemoteProcedureInfo{};
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string wanted_schema = normalizeToken(schema);
        const std::string wanted_proc = normalizeToken(procedure);
        for (const auto& entry : procedure_catalog_) {
            if (!wanted_schema.empty() && normalizeToken(entry.remote_schema) != wanted_schema) {
                continue;
            }
            if (normalizeToken(entry.remote_name) != wanted_proc) {
                continue;
            }
            info = entry;
            return core::Status::OK;
        }

        SET_ERROR_CONTEXT(ctx,
                          core::Status::NOT_FOUND,
                          "Bootstrap scaffold procedure metadata not found");
        return core::Status::NOT_FOUND;
    }

    core::Status listProcedures(const std::string& schema,
                                std::vector<std::string>& procedures,
                                core::ErrorContext* ctx = nullptr) override {
        procedures.clear();
        if (!requireInitialized(ctx)) {
            return core::Status::CONNECTION_DOES_NOT_EXIST;
        }

        const std::string wanted_schema = normalizeToken(schema);
        for (const auto& entry : procedure_catalog_) {
            if (!wanted_schema.empty() && normalizeToken(entry.remote_schema) != wanted_schema) {
                continue;
            }
            procedures.push_back(entry.remote_name);
        }
        return core::Status::OK;
    }

    core::Status startCopyIn(const std::string& table,
                             const std::vector<std::string>& columns,
                             core::ErrorContext* ctx = nullptr) override {
        (void)table;
        (void)columns;
        return unsupported("startCopyIn", ctx);
    }

    core::Status sendCopyData(const uint8_t* data,
                              size_t len,
                              core::ErrorContext* ctx = nullptr) override {
        (void)data;
        (void)len;
        return unsupported("sendCopyData", ctx);
    }

    core::Status endCopyIn(uint64_t& rows_inserted,
                           core::ErrorContext* ctx = nullptr) override {
        rows_inserted = 0;
        return unsupported("endCopyIn", ctx);
    }

    core::Status startCopyOut(const std::string& query,
                              core::ErrorContext* ctx = nullptr) override {
        (void)query;
        return unsupported("startCopyOut", ctx);
    }

    core::Status receiveCopyData(std::vector<uint8_t>& data,
                                 bool& done,
                                 core::ErrorContext* ctx = nullptr) override {
        data.clear();
        done = true;
        return unsupported("receiveCopyData", ctx);
    }

    ConnectorType getType() const override { return type_; }

    std::string getVersion() const override { return "bootstrap-scaffold-0.1"; }

    std::string getRemoteVersion() const override {
        std::string version = UDRConnectorFactory::typeToString(type_);
        version += "-bootstrap";
        return version;
    }

    std::vector<std::string> getSupportedFeatures() const override { return supported_features_; }

private:
    enum class SqlClass {
        QUERY,
        DML,
        DDL,
        ADMIN,
        UNKNOWN,
    };

    static auto makeTextValue(const std::string& value) -> RemoteValue {
        RemoteValue out{};
        out.is_null = false;
        out.type_oid = 25;
        out.data.assign(value.begin(), value.end());
        return out;
    }

    static auto makeColumn(const std::string& name,
                           const std::string& type_name,
                           bool nullable) -> RemoteColumn {
        RemoteColumn column{};
        column.name = name;
        column.type_name = type_name;
        column.nullable = nullable;
        return column;
    }

    static auto classifySql(const std::string& sql) -> SqlClass {
        const std::string normalized = normalizeToken(sql);
        if (normalized.empty()) {
            return SqlClass::UNKNOWN;
        }
        const size_t split = normalized.find_first_of(" \t\r\n(");
        const std::string keyword = (split == std::string::npos)
                                        ? normalized
                                        : normalized.substr(0, split);

        if (keyword == "select" || keyword == "show" || keyword == "describe" ||
            keyword == "desc" || keyword == "explain" || keyword == "with" ||
            keyword == "values") {
            return SqlClass::QUERY;
        }
        if (keyword == "insert" || keyword == "update" || keyword == "delete" ||
            keyword == "merge" || keyword == "replace" || keyword == "upsert") {
            return SqlClass::DML;
        }
        if (keyword == "create" || keyword == "alter" || keyword == "drop" ||
            keyword == "truncate" || keyword == "comment" || keyword == "rename") {
            return SqlClass::DDL;
        }
        if (keyword == "grant" || keyword == "revoke" || keyword == "analyze" ||
            keyword == "refresh" || keyword == "optimize" || keyword == "cluster" ||
            keyword == "backup" || keyword == "restore" || keyword == "reindex" ||
            keyword == "checkpoint" || keyword == "set") {
            return SqlClass::ADMIN;
        }
        return SqlClass::UNKNOWN;
    }

    auto applyExecutionGuards(const std::string& normalized_sql,
                              core::ErrorContext* ctx) const -> core::Status {
        if (degraded_) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::CONNECTION_FAILURE,
                              "Bootstrap scaffold connector is in degraded mode");
            return core::Status::CONNECTION_FAILURE;
        }
        if (normalized_sql.find("/*sb_timeout*/") != std::string::npos) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::LOCK_TIMEOUT,
                              "Bootstrap scaffold timeout semantics triggered");
            return core::Status::LOCK_TIMEOUT;
        }
        if (normalized_sql.find("/*sb_cancel*/") != std::string::npos) {
            SET_ERROR_CONTEXT(ctx,
                              core::Status::QUERY_CANCELED,
                              "Bootstrap scaffold cancellation semantics triggered");
            return core::Status::QUERY_CANCELED;
        }
        return core::Status::OK;
    }

    auto requireInitialized(core::ErrorContext* ctx) const -> bool {
        if (initialized_) {
            return true;
        }
        SET_ERROR_CONTEXT(ctx,
                          core::Status::CONNECTION_DOES_NOT_EXIST,
                          "Connector bootstrap session is not initialized");
        return false;
    }

    auto buildScaffoldMetadata() -> void {
        table_catalog_.clear();
        procedure_catalog_.clear();

        const std::string engine = UDRConnectorFactory::typeToString(type_);
        const bool default_namespace_engine =
            (type_ == ConnectorType::CASSANDRA || type_ == ConnectorType::REDIS);
        const std::string schema = default_namespace_engine ? "default" : "public";

        RemoteTableInfo table_info{};
        table_info.remote_schema = schema;
        table_info.remote_name = engine + "_bootstrap_probe";
        table_info.columns = {
            makeColumn("id", "bigint", false),
            makeColumn("payload", "text", true),
        };
        table_info.primary_key = {"id"};
        table_info.unique_keys = {"id"};
        table_catalog_.push_back(std::move(table_info));

        RemoteProcedureInfo procedure_info{};
        procedure_info.remote_schema = schema;
        procedure_info.remote_name = engine + "_bootstrap_probe_proc";
        procedure_info.input_params = {makeColumn("arg_text", "text", true)};
        procedure_info.output_params = {makeColumn("result_text", "text", true)};
        procedure_info.returns_set = false;
        procedure_info.return_type = "text";
        procedure_catalog_.push_back(std::move(procedure_info));
    }

    auto unsupported(const char* operation, core::ErrorContext* ctx) const -> core::Status {
        std::string message = "Connector operation is not implemented in bootstrap scaffold: ";
        message += operation;
        message += " [";
        message += UDRConnectorFactory::typeToString(type_);
        message += "]";
        SET_ERROR_CONTEXT(ctx, core::Status::NOT_IMPLEMENTED, message.c_str());
        return core::Status::NOT_IMPLEMENTED;
    }

    ConnectorType type_{ConnectorType::POSTGRESQL};
    UDRServerConfig config_{};
    bool initialized_{false};
    bool in_transaction_{false};
    std::vector<std::string> supported_features_;
    std::vector<RemoteTableInfo> table_catalog_;
    std::vector<RemoteProcedureInfo> procedure_catalog_;
    std::unordered_map<std::string, std::string> prepared_sql_;
    std::unordered_set<std::string> savepoints_;
    bool degraded_{false};
};

auto parseUnsigned64(const std::string& text, uint64_t& out) -> bool {
    const std::string normalized = normalizeToken(text);
    if (normalized.empty()) {
        return false;
    }
    try {
        size_t consumed = 0;
        const uint64_t parsed = std::stoull(normalized, &consumed, 10);
        if (consumed != normalized.size()) {
            return false;
        }
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

auto parseConnectionStringQuery(const std::string& query)
    -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> out;
    size_t start = 0;
    while (start <= query.size()) {
        const size_t end = query.find('&', start);
        const std::string token = (end == std::string::npos)
                                      ? query.substr(start)
                                      : query.substr(start, end - start);
        if (!token.empty()) {
            const size_t eq = token.find('=');
            if (eq == std::string::npos) {
                out[normalizeToken(token)] = std::string{};
            } else {
                out[normalizeToken(token.substr(0, eq))] = token.substr(eq + 1);
            }
        }

        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return out;
}

auto applyQueryParamUint32(const std::unordered_map<std::string, std::string>& query,
                           const char* key,
                           uint32_t& out) -> void {
    const auto it = query.find(key);
    if (it == query.end()) {
        return;
    }
    uint64_t parsed = 0;
    if (!parseUnsigned64(it->second, parsed)) {
        return;
    }
    if (parsed > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
        return;
    }
    out = static_cast<uint32_t>(parsed);
}

auto buildBindingFromConnectionString(const std::string& connection_string,
                                      SysRemoteRuntimeBinding& binding,
                                      core::ErrorContext* ctx) -> core::Status {
    const std::string normalized = normalizeToken(connection_string);
    if (normalized.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "sys.remote_* requires a non-empty server connection string",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    const std::string scheme = extractConnectorScheme(normalized);
    const ConnectorType connector_type = UDRConnectorFactory::stringToType(scheme);
    if (!UDRConnectorFactory::isSupported(connector_type)) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "sys.remote_* connection string has unsupported connector scheme",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    SysRemoteRuntimeBinding parsed_binding{};
    parsed_binding.connector_type = connector_type;
    parsed_binding.config.host = "localhost";
    parsed_binding.config.port = defaultPortForConnectorType(connector_type);
    parsed_binding.config.pool_min_size = 0;
    parsed_binding.config.pool_max_size = 1;
    parsed_binding.config.pool_connection_timeout_ms = 5000;
    parsed_binding.config.pool_max_idle_ms = 1000;
    parsed_binding.config.pool_health_check_interval_ms = 1000;

    const size_t scheme_sep = normalized.find("://");
    if (scheme_sep == std::string::npos) {
        binding = std::move(parsed_binding);
        return core::Status::OK;
    }

    std::string remainder = normalized.substr(scheme_sep + 3);
    std::string query_part;
    const size_t query_sep = remainder.find('?');
    if (query_sep != std::string::npos) {
        query_part = remainder.substr(query_sep + 1);
        remainder = remainder.substr(0, query_sep);
    }

    std::string authority = remainder;
    std::string path;
    const size_t slash = remainder.find('/');
    if (slash != std::string::npos) {
        authority = remainder.substr(0, slash);
        path = remainder.substr(slash + 1);
    }

    std::string host_port = authority;
    const size_t at = authority.rfind('@');
    if (at != std::string::npos) {
        const std::string user_info = authority.substr(0, at);
        host_port = authority.substr(at + 1);

        const size_t colon = user_info.find(':');
        if (colon == std::string::npos) {
            parsed_binding.config.user = user_info;
        } else {
            parsed_binding.config.user = user_info.substr(0, colon);
            parsed_binding.config.password = user_info.substr(colon + 1);
        }
    }

    if (!host_port.empty()) {
        if (host_port.front() == '[') {
            const size_t rb = host_port.find(']');
            if (rb != std::string::npos) {
                parsed_binding.config.host = host_port.substr(1, rb - 1);
                if (rb + 2 < host_port.size() && host_port[rb + 1] == ':') {
                    uint64_t parsed_port = 0;
                    if (parseUnsigned64(host_port.substr(rb + 2), parsed_port) &&
                        parsed_port <= static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())) {
                        parsed_binding.config.port = static_cast<uint16_t>(parsed_port);
                    }
                }
            } else {
                parsed_binding.config.host = host_port;
            }
        } else {
            const size_t colon = host_port.rfind(':');
            if (colon != std::string::npos) {
                const std::string possible_port = host_port.substr(colon + 1);
                uint64_t parsed_port = 0;
                if (parseUnsigned64(possible_port, parsed_port) &&
                    parsed_port <= static_cast<uint64_t>(std::numeric_limits<uint16_t>::max())) {
                    parsed_binding.config.host = host_port.substr(0, colon);
                    parsed_binding.config.port = static_cast<uint16_t>(parsed_port);
                } else {
                    parsed_binding.config.host = host_port;
                }
            } else {
                parsed_binding.config.host = host_port;
            }
        }
    }

    if (!path.empty()) {
        parsed_binding.config.database = path;
    }

    const auto query = parseConnectionStringQuery(query_part);
    if (const auto it = query.find("database"); it != query.end()) {
        parsed_binding.config.database = it->second;
    }
    if (const auto it = query.find("db"); it != query.end()) {
        parsed_binding.config.database = it->second;
    }
    if (const auto it = query.find("user"); it != query.end()) {
        parsed_binding.config.user = it->second;
    }
    if (const auto it = query.find("password"); it != query.end()) {
        parsed_binding.config.password = it->second;
    }
    if (const auto it = query.find("ssl_mode"); it != query.end()) {
        parsed_binding.config.ssl_mode = it->second;
    }
    if (const auto it = query.find("role"); it != query.end()) {
        parsed_binding.config.role = it->second;
    }
    if (const auto it = query.find("charset"); it != query.end()) {
        parsed_binding.config.charset = it->second;
    }

    applyQueryParamUint32(query, "pool_min_size", parsed_binding.config.pool_min_size);
    applyQueryParamUint32(query, "pool_max_size", parsed_binding.config.pool_max_size);
    applyQueryParamUint32(query, "pool_connection_timeout_ms",
                          parsed_binding.config.pool_connection_timeout_ms);
    applyQueryParamUint32(query, "pool_max_idle_ms", parsed_binding.config.pool_max_idle_ms);
    applyQueryParamUint32(query, "pool_health_check_interval_ms",
                          parsed_binding.config.pool_health_check_interval_ms);

    binding = std::move(parsed_binding);
    return core::Status::OK;
}

auto normalizeBoundConfig(const SysRemoteRuntimeBinding& binding) -> UDRServerConfig {
    UDRServerConfig config = binding.config;
    if (config.host.empty()) {
        config.host = "localhost";
    }
    if (config.port == 0) {
        config.port = defaultPortForConnectorType(binding.connector_type);
    }
    if (config.pool_max_size == 0) {
        config.pool_max_size = 1;
    }
    if (config.pool_min_size > config.pool_max_size) {
        config.pool_min_size = config.pool_max_size;
    }
    return config;
}

auto makeConnectorForBinding(const SysRemoteRuntimeBinding& binding,
                             core::ErrorContext* ctx) -> std::unique_ptr<UDRConnector> {
    if (!UDRConnectorFactory::isSupported(binding.connector_type)) {
        if (ctx) {
            ctx->set(core::Status::NOT_SUPPORTED,
                     "sys.remote_* runtime binding uses unsupported connector type",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return nullptr;
    }

    auto connector = UDRConnectorFactory::create(binding.connector_type);
    if (!connector && ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 "sys.remote_* failed to construct connector",
                 __FILE__,
                 __LINE__,
                 __func__);
    }
    return connector;
}

auto ensureSqlText(const std::string& sql, core::ErrorContext* ctx) -> core::Status {
    if (normalizeToken(sql).empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "sys.remote_* requires non-empty SQL text",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    return core::Status::OK;
}

auto executeBoundCommand(const SysRemoteRuntimeBinding& binding,
                         const std::string& sql,
                         uint64_t& rows_affected,
                         core::ErrorContext* ctx) -> core::Status {
    rows_affected = 0;

    core::Status status = ensureSqlText(sql, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    auto connector = makeConnectorForBinding(binding, ctx);
    if (!connector) {
        return core::Status::NOT_SUPPORTED;
    }

    const UDRServerConfig config = normalizeBoundConfig(binding);
    status = connector->initialize(config, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    status = connector->executeCommand(sql, rows_affected, ctx);
    core::ErrorContext shutdown_ctx;
    (void)connector->shutdown(&shutdown_ctx);
    return status;
}

auto executeBoundQuery(const SysRemoteRuntimeBinding& binding,
                       const std::string& sql,
                       RemoteResultSet& result,
                       core::ErrorContext* ctx) -> core::Status {
    result.clear();

    core::Status status = ensureSqlText(sql, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    auto connector = makeConnectorForBinding(binding, ctx);
    if (!connector) {
        return core::Status::NOT_SUPPORTED;
    }

    const UDRServerConfig config = normalizeBoundConfig(binding);
    status = connector->initialize(config, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    status = connector->executeQuery(sql, result, ctx);
    core::ErrorContext shutdown_ctx;
    (void)connector->shutdown(&shutdown_ctx);
    return status;
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
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::MILVUS:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::MONGODB:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::NEO4J:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::REDIS:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::MARIADB:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::INFLUXDB:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::CLICKHOUSE:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::OPENSEARCH:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
        case ConnectorType::DUCKDB:
            return std::make_unique<BootstrapScaffoldUDRConnector>(type);
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

auto sys_remote_exec_bound(const SysRemoteRuntimeBinding& binding,
                           const std::string& sql,
                           uint64_t& rows_affected,
                           core::ErrorContext* ctx) -> core::Status {
    return executeBoundCommand(binding, sql, rows_affected, ctx);
}

auto sys_remote_query_bound(const SysRemoteRuntimeBinding& binding,
                            const std::string& sql,
                            RemoteResultSet& result,
                            core::ErrorContext* ctx) -> core::Status {
    return executeBoundQuery(binding, sql, result, ctx);
}

auto sys_remote_call_bound(const SysRemoteRuntimeBinding& binding,
                           const std::string& procedure_name,
                           const std::vector<RemoteValue>& params,
                           RemoteResultSet& result,
                           core::ErrorContext* ctx) -> core::Status {
    if (normalizeToken(procedure_name).empty()) {
        result.clear();
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "sys.remote_call requires a non-empty procedure name",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    std::string call_sql = "CALL ";
    call_sql += procedure_name;
    call_sql.push_back('(');
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            call_sql += ", ";
        }

        if (params[i].is_null) {
            call_sql += "NULL";
            continue;
        }

        std::string text = params[i].toString();
        std::string escaped;
        escaped.reserve(text.size());
        for (char ch : text) {
            if (ch == '\'') {
                escaped.push_back('\'');
            }
            escaped.push_back(ch);
        }
        call_sql.push_back('\'');
        call_sql += escaped;
        call_sql.push_back('\'');
    }
    call_sql.push_back(')');

    return executeBoundQuery(binding, call_sql, result, ctx);
}

auto sys_remote_exec(const std::string& server_name,
                     const std::string& sql,
                     uint64_t& rows_affected,
                     core::ErrorContext* ctx) -> core::Status {
    SysRemoteRuntimeBinding binding{};
    core::Status status = buildBindingFromConnectionString(server_name, binding, ctx);
    if (status != core::Status::OK) {
        rows_affected = 0;
        return status;
    }
    return executeBoundCommand(binding, sql, rows_affected, ctx);
}

auto sys_remote_query(const std::string& server_name,
                      const std::string& sql,
                      RemoteResultSet& result,
                      core::ErrorContext* ctx) -> core::Status {
    SysRemoteRuntimeBinding binding{};
    core::Status status = buildBindingFromConnectionString(server_name, binding, ctx);
    if (status != core::Status::OK) {
        result.clear();
        return status;
    }
    return executeBoundQuery(binding, sql, result, ctx);
}

auto sys_remote_call(const std::string& server_name,
                     const std::string& procedure_name,
                     const std::vector<RemoteValue>& params,
                     RemoteResultSet& result,
                     core::ErrorContext* ctx) -> core::Status {
    SysRemoteRuntimeBinding binding{};
    core::Status status = buildBindingFromConnectionString(server_name, binding, ctx);
    if (status != core::Status::OK) {
        result.clear();
        return status;
    }
    return sys_remote_call_bound(binding, procedure_name, params, result, ctx);
}

} // namespace udr
} // namespace scratchbird
